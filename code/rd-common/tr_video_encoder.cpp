/*
===========================================================================
Copyright (C) 2026 MovieDuels / SerenityJediEngine contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.
===========================================================================
*/

// Video recording (AVI) frame encoder on worker threads.
//
// The render thread only reads the frame back (R_VideoEncoderBegin / R_VideoEncoderSubmit); the JPEG compression (or the
// raw BGR conversion) runs on the workers, and the finished frames are handed back in recording order to be written by
// the client (R_VideoEncoderFlush, on the render thread, so the file is only ever written from there).
// Compressing on the render thread cost tens of milliseconds per captured frame - the big frame drop while recording.

#include "../server/exe_headers.h"

#include "tr_common.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	constexpr int VIDEO_WORKERS = 2; // two frames compressed at the same time (high resolutions)
	constexpr size_t VIDEO_MAX_QUEUED = 6; // frames waiting or being compressed; the render thread waits beyond this
	constexpr int VIDEO_AVI_LINE_PADDING = 4; // AVI_LINE_PADDING (qcommon.h): raw AVI rows are padded to 4 bytes

	struct videoJob_t
	{
		std::vector<byte> pixels; // the frame as read back: RGB rows, bottom up, padded to the pack alignment
		int width = 0;
		int height = 0;
		int padding = 0; // bytes after each row of pixels
		int quality = 90;
		qboolean motionJpeg = qtrue;
		std::vector<byte> out;
		size_t outSize = 0;
		bool taken = false; // a worker is compressing it
		bool done = false;
	};

	std::mutex s_lock;
	std::condition_variable s_work; // a job to compress, or quitting
	std::condition_variable s_done; // a job finished
	std::deque<std::unique_ptr<videoJob_t>> s_jobs; // in recording order
	std::unique_ptr<videoJob_t> s_next; // being read back (R_VideoEncoderBegin .. R_VideoEncoderSubmit)
	std::vector<std::thread> s_workers;
	bool s_quit = false;

	void R_VideoEncode(videoJob_t* job)
	{
		const size_t linelen = static_cast<size_t>(job->width) * 3;

		if (job->motionJpeg)
		{
			job->out.resize(linelen * job->height + 4096);
			job->outSize = RE_SaveVideoJPGToBuffer(job->out.data(), job->out.size(), job->quality,
				job->width, job->height, job->pixels.data(), job->padding);
			return;
		}

		// raw: swap R and B and pad the rows as AVI wants them
		const size_t avipadwidth = PAD(linelen, VIDEO_AVI_LINE_PADDING);
		const size_t avipadlen = avipadwidth - linelen;
		const byte* src = job->pixels.data();
		byte* dest;

		job->out.resize(avipadwidth * job->height);
		dest = job->out.data();
		for (int y = 0; y < job->height; y++)
		{
			const byte* lineend = src + linelen;
			while (src < lineend)
			{
				*dest++ = src[2];
				*dest++ = src[1];
				*dest++ = src[0];
				src += 3;
			}
			memset(dest, 0, avipadlen);
			dest += avipadlen;
			src += job->padding;
		}
		job->outSize = avipadwidth * job->height;
	}

	void R_VideoWorker()
	{
		for (;;)
		{
			videoJob_t* job = nullptr;
			{
				std::unique_lock<std::mutex> lock(s_lock);
				s_work.wait(lock, []
				{
					if (s_quit)
					{
						return true;
					}
					for (const auto& j : s_jobs)
					{
						if (!j->taken)
						{
							return true;
						}
					}
					return false;
				});
				if (s_quit)
				{
					return;
				}
				for (const auto& j : s_jobs)
				{
					if (!j->taken)
					{
						job = j.get();
						job->taken = true;
						break;
					}
				}
			}
			R_VideoEncode(job);
			{
				std::lock_guard<std::mutex> lock(s_lock);
				job->done = true;
			}
			s_done.notify_all();
		}
	}

	// the finished frames at the front, in order (all: waits for every queued frame); called with s_lock held
	void R_VideoEncoderWriteDone(std::unique_lock<std::mutex>& lock, void (*write)(const byte*, int), const bool all)
	{
		while (!s_jobs.empty())
		{
			if (!s_jobs.front()->done)
			{
				if (!all)
				{
					return;
				}
				s_done.wait(lock, [] { return s_jobs.front()->done; });
			}
			std::unique_ptr<videoJob_t> job = std::move(s_jobs.front());
			s_jobs.pop_front();
			lock.unlock();
			write(job->out.data(), static_cast<int>(job->outSize));
			lock.lock();
		}
	}
}

// the buffer to read the next frame into (bytes long), queued by R_VideoEncoderSubmit
byte* R_VideoEncoderBegin(const size_t bytes)
{
	if (!s_next)
	{
		s_next = std::make_unique<videoJob_t>();
	}
	s_next->pixels.resize(bytes);
	return s_next->pixels.data();
}

// queues the frame read into R_VideoEncoderBegin's buffer and writes the frames finished so far
void R_VideoEncoderSubmit(const int width, const int height, const int padding, const int quality,
	const qboolean motionJpeg, void (*write)(const byte*, int))
{
	if (!s_next)
	{
		return;
	}
	if (s_workers.empty())
	{
		s_quit = false;
		for (int i = 0; i < VIDEO_WORKERS; i++)
		{
			s_workers.emplace_back(R_VideoWorker);
		}
	}

	s_next->width = width;
	s_next->height = height;
	s_next->padding = padding;
	s_next->quality = quality;
	s_next->motionJpeg = motionJpeg;
	{
		std::unique_lock<std::mutex> lock(s_lock);
		// too far behind (a slow disk, 4K): wait for the oldest instead of using ever more memory
		while (s_jobs.size() >= VIDEO_MAX_QUEUED)
		{
			s_done.wait(lock, [] { return s_jobs.front()->done; });
			R_VideoEncoderWriteDone(lock, write, false);
		}
		s_jobs.push_back(std::move(s_next));
	}
	s_work.notify_one();

	std::unique_lock<std::mutex> lock(s_lock);
	R_VideoEncoderWriteDone(lock, write, false);
}

// writes the finished frames (all: every queued frame, waiting for them - before the AVI file is closed)
void R_VideoEncoderFlush(void (*write)(const byte*, int), const qboolean all)
{
	std::unique_lock<std::mutex> lock(s_lock);
	R_VideoEncoderWriteDone(lock, write, all ? true : false);
}

// stops the workers (renderer shutdown); frames not flushed are dropped
void R_VideoEncoderShutdown(void)
{
	{
		std::lock_guard<std::mutex> lock(s_lock);
		s_quit = true;
	}
	s_work.notify_all();
	for (auto& t : s_workers)
	{
		t.join();
	}
	s_workers.clear();
	std::lock_guard<std::mutex> lock(s_lock);
	s_jobs.clear();
	s_next.reset();
	s_quit = false;
}
