/*
===========================================================================
Copyright (C) 2005-2006 Tim Angus

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// SP port of MP's AVI recorder (cl_avi.c). SP records live play, not a demo playback, so the game is not locked to
// the video frame rate: frames are captured at real-time pace (repeated or skipped to keep cl_aviFrameRate), and the
// recording shows a timer on screen (not in the video) and where the file went when it stops.
#include "../server/exe_headers.h"
#include "client.h"
#include "snd_local.h"
#include "../../shared/sys/sys_public.h"
#include <ctime>

cvar_t* cl_aviFrameRate;
cvar_t* cl_aviMotionJpeg;
cvar_t* cl_avi2GBLimit;

#define INDEX_FILE_EXTENSION ".index.dat"

constexpr auto MAX_RIFF_CHUNKS = 16;

using audioFormat_t = struct audioFormat_s
{
	int rate;
	int format;
	int channels;
	int bits;

	int sampleSize;
	int totalBytes;
};

using aviFileData_t = struct aviFileData_s
{
	qboolean fileOpen;
	fileHandle_t f;
	char fileName[MAX_QPATH];
	int fileSize;
	int moviOffset;
	int moviSize;

	fileHandle_t idxF;
	int numIndices;

	int frameRate;
	int framePeriod;
	int width, height;
	int numVideoFrames;
	int maxRecordSize;
	qboolean motionJpeg;

	qboolean audio;
	audioFormat_t a;
	int numAudioFrames;

	int chunkStack[MAX_RIFF_CHUNKS];
	int chunkStackTop;

	byte* cBuffer, * eBuffer;

	// SP: live capture pacing
	int startTime; // Sys_Milliseconds when the recording started
	int framesRequested; // video frames asked for so far
	int pendingFrames[32]; // copies to write of each captured frame not written yet (frames owed at its capture)
	int pendingHead, pendingCount; // (the renderer compresses on workers: frames arrive later, in capture order)
};

// SP: on-screen status after a recording stops
static char aviSavedPath[MAX_OSPATH];
static int aviSavedTime;
static int aviSavedFrames;

static aviFileData_t afd;

constexpr auto MAX_AVI_BUFFER = 2048;

static byte buffer[MAX_AVI_BUFFER];
static int bufIndex;

/*
===============
SafeFS_Write
===============
*/
static QINLINE void SafeFS_Write(const void* buffer, const int len, const fileHandle_t f)
{
	if (FS_Write(buffer, len, f) < len)
		Com_Error(ERR_DROP, "Failed to write avi file");
}

/*
===============
WRITE_STRING
===============
*/
static QINLINE void WRITE_STRING(const char* s)
{
	Com_Memcpy(&buffer[bufIndex], s, strlen(s));
	bufIndex += strlen(s);
}

/*
===============
WRITE_4BYTES
===============
*/
static QINLINE void WRITE_4BYTES(const int x)
{
	buffer[bufIndex + 0] = static_cast<byte>(x >> 0 & 0xFF);
	buffer[bufIndex + 1] = static_cast<byte>(x >> 8 & 0xFF);
	buffer[bufIndex + 2] = static_cast<byte>(x >> 16 & 0xFF);
	buffer[bufIndex + 3] = static_cast<byte>(x >> 24 & 0xFF);
	bufIndex += 4;
}

/*
===============
WRITE_2BYTES
===============
*/
static QINLINE void WRITE_2BYTES(const int x)
{
	buffer[bufIndex + 0] = static_cast<byte>(x >> 0 & 0xFF);
	buffer[bufIndex + 1] = static_cast<byte>(x >> 8 & 0xFF);
	bufIndex += 2;
}

/*
===============
START_CHUNK
===============
*/
static QINLINE void START_CHUNK(const char* s)
{
	if (afd.chunkStackTop == MAX_RIFF_CHUNKS)
	{
		Com_Error(ERR_DROP, "ERROR: Top of chunkstack breached");
	}

	afd.chunkStack[afd.chunkStackTop] = bufIndex;
	afd.chunkStackTop++;
	WRITE_STRING(s);
	WRITE_4BYTES(0);
}

/*
===============
END_CHUNK
===============
*/
static QINLINE void END_CHUNK(void)
{
	const int endIndex = bufIndex;

	if (afd.chunkStackTop <= 0)
	{
		Com_Error(ERR_DROP, "ERROR: Bottom of chunkstack breached");
	}

	afd.chunkStackTop--;
	bufIndex = afd.chunkStack[afd.chunkStackTop];
	bufIndex += 4;
	WRITE_4BYTES(endIndex - bufIndex - 4);
	bufIndex = endIndex;
	bufIndex = PAD(bufIndex, 2);
}

/*
===============
CL_WriteAVIHeader
===============
*/
void CL_WriteAVIHeader(void)
{
	bufIndex = 0;
	afd.chunkStackTop = 0;

	START_CHUNK("RIFF");
	{
		WRITE_STRING("AVI ");
		{
			START_CHUNK("LIST");
			{
				WRITE_STRING("hdrl");
				WRITE_STRING("avih");
				WRITE_4BYTES(56); //"avih" "chunk" size
				WRITE_4BYTES(afd.framePeriod); //dwMicroSecPerFrame
				WRITE_4BYTES(afd.maxRecordSize *
					afd.frameRate); //dwMaxBytesPerSec
				WRITE_4BYTES(0); //dwReserved1
				WRITE_4BYTES(0x110); //dwFlags bits HAS_INDEX and IS_INTERLEAVED
				WRITE_4BYTES(afd.numVideoFrames); //dwTotalFrames
				WRITE_4BYTES(0); //dwInitialFrame

				if (afd.audio) //dwStreams
					WRITE_4BYTES(2);
				else
					WRITE_4BYTES(1);

				WRITE_4BYTES(afd.maxRecordSize); //dwSuggestedBufferSize
				WRITE_4BYTES(afd.width); //dwWidth
				WRITE_4BYTES(afd.height); //dwHeight
				WRITE_4BYTES(0); //dwReserved[ 0 ]
				WRITE_4BYTES(0); //dwReserved[ 1 ]
				WRITE_4BYTES(0); //dwReserved[ 2 ]
				WRITE_4BYTES(0); //dwReserved[ 3 ]

				START_CHUNK("LIST");
				{
					WRITE_STRING("strl");
					WRITE_STRING("strh");
					WRITE_4BYTES(56); //"strh" "chunk" size
					WRITE_STRING("vids");

					if (afd.motionJpeg)
						WRITE_STRING("MJPG");
					else
						WRITE_4BYTES(0); // BI_RGB

					WRITE_4BYTES(0); //dwFlags
					WRITE_4BYTES(0); //dwPriority
					WRITE_4BYTES(0); //dwInitialFrame

					WRITE_4BYTES(1); //dwTimescale
					WRITE_4BYTES(afd.frameRate); //dwDataRate
					WRITE_4BYTES(0); //dwStartTime
					WRITE_4BYTES(afd.numVideoFrames); //dwDataLength

					WRITE_4BYTES(afd.maxRecordSize); //dwSuggestedBufferSize
					WRITE_4BYTES(-1); //dwQuality
					WRITE_4BYTES(0); //dwSampleSize
					WRITE_2BYTES(0); //rcFrame
					WRITE_2BYTES(0); //rcFrame
					WRITE_2BYTES(afd.width); //rcFrame
					WRITE_2BYTES(afd.height); //rcFrame

					WRITE_STRING("strf");
					WRITE_4BYTES(40); //"strf" "chunk" size
					WRITE_4BYTES(40); //biSize
					WRITE_4BYTES(afd.width); //biWidth
					WRITE_4BYTES(afd.height); //biHeight
					WRITE_2BYTES(1); //biPlanes
					WRITE_2BYTES(24); //biBitCount

					if (afd.motionJpeg) //biCompression
					{
						WRITE_STRING("MJPG");
						WRITE_4BYTES(afd.width *
							afd.height); //biSizeImage
					}
					else
					{
						WRITE_4BYTES(0); // BI_RGB
						WRITE_4BYTES(afd.width *
							afd.height * 3); //biSizeImage
					}

					WRITE_4BYTES(0); //biXPelsPetMeter
					WRITE_4BYTES(0); //biYPelsPetMeter
					WRITE_4BYTES(0); //biClrUsed
					WRITE_4BYTES(0); //biClrImportant
				}
				END_CHUNK();

				if (afd.audio)
				{
					START_CHUNK("LIST");
					{
						WRITE_STRING("strl");
						WRITE_STRING("strh");
						WRITE_4BYTES(56); //"strh" "chunk" size
						WRITE_STRING("auds");
						WRITE_4BYTES(0); //FCC
						WRITE_4BYTES(0); //dwFlags
						WRITE_4BYTES(0); //dwPriority
						WRITE_4BYTES(0); //dwInitialFrame

						WRITE_4BYTES(afd.a.sampleSize); //dwTimescale
						WRITE_4BYTES(afd.a.sampleSize *
							afd.a.rate); //dwDataRate
						WRITE_4BYTES(0); //dwStartTime
						WRITE_4BYTES(afd.a.totalBytes /
							afd.a.sampleSize); //dwDataLength

						WRITE_4BYTES(0); //dwSuggestedBufferSize
						WRITE_4BYTES(-1); //dwQuality
						WRITE_4BYTES(afd.a.sampleSize); //dwSampleSize
						WRITE_2BYTES(0); //rcFrame
						WRITE_2BYTES(0); //rcFrame
						WRITE_2BYTES(0); //rcFrame
						WRITE_2BYTES(0); //rcFrame

						WRITE_STRING("strf");
						WRITE_4BYTES(18); //"strf" "chunk" size
						WRITE_2BYTES(afd.a.format); //wFormatTag
						WRITE_2BYTES(afd.a.channels); //nChannels
						WRITE_4BYTES(afd.a.rate); //nSamplesPerSec
						WRITE_4BYTES(afd.a.sampleSize *
							afd.a.rate); //nAvgBytesPerSec
						WRITE_2BYTES(afd.a.sampleSize); //nBlockAlign
						WRITE_2BYTES(afd.a.bits); //wBitsPerSample
						WRITE_2BYTES(0); //cbSize
					}
					END_CHUNK();
				}
			}
			END_CHUNK();

			afd.moviOffset = bufIndex;

			START_CHUNK("LIST");
			{
				WRITE_STRING("movi");
			}
		}
	}
}

/*
===============
CL_OpenAVIForWriting

Creates an AVI file and gets it into a state where
writing the actual data can begin
===============
*/
qboolean CL_OpenAVIForWriting(const char* fileName)
{
	if (afd.fileOpen)
		return qfalse;

	Com_Memset(&afd, 0, sizeof(aviFileData_t));

	// Don't start if a framerate has not been chosen
	if (cl_aviFrameRate->integer <= 0)
	{
		Com_Printf(S_COLOR_RED "cl_aviFrameRate must be >= 1\n");
		return qfalse;
	}

	if ((afd.f = FS_FOpenFileWrite(fileName)) <= 0)
		return qfalse;

	if ((afd.idxF = FS_FOpenFileWrite(
		va("%s" INDEX_FILE_EXTENSION, fileName))) <= 0)
	{
		FS_FCloseFile(afd.f);
		return qfalse;
	}

	Q_strncpyz(afd.fileName, fileName, MAX_QPATH);
	afd.startTime = Sys_Milliseconds();
	afd.framesRequested = 0;
	afd.pendingHead = afd.pendingCount = 0;

	afd.frameRate = cl_aviFrameRate->integer;
	afd.framePeriod = static_cast<int>(1000000.0f / afd.frameRate);
	afd.width = cls.glconfig.vidWidth;
	afd.height = cls.glconfig.vidHeight;

	if (cl_aviMotionJpeg->integer)
		afd.motionJpeg = qtrue;
	else
		afd.motionJpeg = qfalse;

	// Buffers only need to store RGB pixels.
	// Allocate a bit more space for the capture buffer to account for possible
	// padding at the end of pixel lines, and padding for alignment
	constexpr auto MAX_PACK_LEN = 16;
	afd.cBuffer = static_cast<byte*>(Z_Malloc((afd.width * 3 + MAX_PACK_LEN - 1) * afd.height + MAX_PACK_LEN - 1,
		TAG_GENERAL, qtrue));
	// raw avi files have pixel lines start on 4-byte boundaries
	afd.eBuffer = static_cast<byte*>(Z_Malloc(PAD(afd.width * 3, AVI_LINE_PADDING) * afd.height, TAG_GENERAL, qtrue));

	afd.a.rate = dma.speed;
	afd.a.format = WAV_FORMAT_PCM;
	afd.a.channels = dma.channels;
	afd.a.bits = dma.samplebits;
	afd.a.sampleSize = afd.a.bits / 8 * afd.a.channels;

	if (afd.a.rate % afd.frameRate)
	{
		int suggestRate = afd.frameRate;

		while (afd.a.rate % suggestRate && suggestRate >= 1)
			suggestRate--;

		Com_Printf(S_COLOR_YELLOW "WARNING: cl_aviFrameRate is not a divisor "
			"of the audio rate, suggest %d\n", suggestRate);
	}

	if (!Cvar_VariableIntegerValue("s_initsound"))
	{
		afd.audio = qfalse;
	}
	else if (Cvar_VariableIntegerValue("s_UseOpenAL") == 0)
		//else if( Q_stricmp( Cvar_VariableString( "s_backend" ), "OpenAL" ) )
	{
		if (afd.a.bits != 16 || afd.a.channels != 2)
		{
			Com_Printf(S_COLOR_YELLOW "WARNING: Audio format of %d bit/%d channels not supported",
				afd.a.bits, afd.a.channels);
			afd.audio = qfalse;
		}
		else
			afd.audio = qtrue;
	}
	else
	{
		afd.audio = qfalse;
		Com_Printf(S_COLOR_YELLOW "WARNING: Audio capture is not supported "
			"with OpenAL. Set s_UseOpenAL to 0 for audio capture\n");
	}

	// This doesn't write a real header, but allocates the
	// correct amount of space at the beginning of the file
	CL_WriteAVIHeader();

	SafeFS_Write(buffer, bufIndex, afd.f);
	afd.fileSize = bufIndex;

	bufIndex = 0;
	START_CHUNK("idx1");
	SafeFS_Write(buffer, bufIndex, afd.idxF);

	afd.moviSize = 4; // For the "movi"
	afd.fileOpen = qtrue;

	return qtrue;
}

/*
===============
CL_CheckFileSize
===============
*/
static qboolean CL_CheckFileSize(const int bytesToAdd)
{
	const unsigned int newFileSize = afd.fileSize + // Current file size
		bytesToAdd + // What we want to add
		afd.numIndices * 16 + // The index
		4; // The index size

	if (cl_avi2GBLimit->integer)
	{
		// I assume all the operating systems
		// we target can handle a 2Gb file
		if (newFileSize > INT_MAX)
		{
			// Close the current file...
			CL_CloseAVI();

			// ...And open a new one
			CL_OpenAVIForWriting(va("%s_", afd.fileName));

			return qtrue;
		}
	}

	return qfalse;
}

/*
===============
CL_WriteAVIVideoFrame
===============
*/
static void CL_WriteAVIVideoFrameOnce(const byte* imageBuffer, const int size)
{
	const int chunkOffset = afd.fileSize - afd.moviOffset - 8;
	const int chunkSize = 8 + size;
	const int paddingSize = PADLEN(size, 2);
	constexpr byte padding[4] = { 0 };

	if (!afd.fileOpen)
		return;

	// Chunk header + contents + padding
	if (CL_CheckFileSize(8 + size + 2))
		return;

	bufIndex = 0;
	WRITE_STRING("00dc");
	WRITE_4BYTES(size);

	SafeFS_Write(buffer, 8, afd.f);
	SafeFS_Write(imageBuffer, size, afd.f);
	SafeFS_Write(padding, paddingSize, afd.f);
	afd.fileSize += chunkSize + paddingSize;

	afd.numVideoFrames++;
	afd.moviSize += chunkSize + paddingSize;

	if (size > afd.maxRecordSize)
		afd.maxRecordSize = size;

	// Index
	bufIndex = 0;
	WRITE_STRING("00dc"); //dwIdentifier
	WRITE_4BYTES(0x00000010); //dwFlags (all frames are KeyFrames)
	WRITE_4BYTES(chunkOffset); //dwOffset
	WRITE_4BYTES(size); //dwLength
	SafeFS_Write(buffer, 16, afd.idxF);

	afd.numIndices++;
}

// the renderer hands back the captured frame: write it as often as frames are owed (SP live pacing)
void CL_WriteAVIVideoFrame(const byte* imageBuffer, const int size)
{
	int copies = 1;

	if (afd.pendingCount > 0)
	{
		copies = afd.pendingFrames[afd.pendingHead];
		afd.pendingHead = (afd.pendingHead + 1) % 32;
		afd.pendingCount--;
	}
	while (copies-- > 0 && afd.fileOpen)
	{
		CL_WriteAVIVideoFrameOnce(imageBuffer, size);
	}
}

constexpr auto PCM_BUFFER_SIZE = 44100;

/*
===============
CL_WriteAVIAudioFrame
===============
*/
void CL_WriteAVIAudioFrame(const byte* pcmBuffer, int size)
{
	static byte pcmCaptureBuffer[PCM_BUFFER_SIZE] = { 0 };
	static int bytesInBuffer = 0;

	if (!afd.audio)
		return;

	if (!afd.fileOpen)
		return;

	// Chunk header + contents + padding
	if (CL_CheckFileSize(8 + bytesInBuffer + size + 2))
		return;

	if (bytesInBuffer + size > PCM_BUFFER_SIZE)
	{
		Com_Printf(S_COLOR_YELLOW
			"WARNING: Audio capture buffer overflow -- truncating\n");
		size = PCM_BUFFER_SIZE - bytesInBuffer;
	}

	Com_Memcpy(&pcmCaptureBuffer[bytesInBuffer], pcmBuffer, size);
	bytesInBuffer += size;

	// Only write if we have a frame's worth of audio
	if (bytesInBuffer >= static_cast<int>(ceil(static_cast<float>(afd.a.rate) / static_cast<float>(afd.frameRate))) *
		afd.a.sampleSize)
	{
		const int chunkOffset = afd.fileSize - afd.moviOffset - 8;
		const int chunkSize = 8 + bytesInBuffer;
		const int paddingSize = PADLEN(bytesInBuffer, 2);
		constexpr byte padding[4] = { 0 };

		bufIndex = 0;
		WRITE_STRING("01wb");
		WRITE_4BYTES(bytesInBuffer);

		SafeFS_Write(buffer, 8, afd.f);
		SafeFS_Write(pcmCaptureBuffer, bytesInBuffer, afd.f);
		SafeFS_Write(padding, paddingSize, afd.f);
		afd.fileSize += chunkSize + paddingSize;

		afd.numAudioFrames++;
		afd.moviSize += chunkSize + paddingSize;
		afd.a.totalBytes += bytesInBuffer;

		// Index
		bufIndex = 0;
		WRITE_STRING("01wb"); //dwIdentifier
		WRITE_4BYTES(0); //dwFlags
		WRITE_4BYTES(chunkOffset); //dwOffset
		WRITE_4BYTES(bytesInBuffer); //dwLength
		SafeFS_Write(buffer, 16, afd.idxF);

		afd.numIndices++;

		bytesInBuffer = 0;
	}
}

/*
===============
CL_TakeVideoFrame
===============
*/
void CL_TakeVideoFrame(void)
{
	// AVI file isn't open
	if (!afd.fileOpen)
		return;

	re.TakeVideoFrame(afd.width, afd.height,
		afd.cBuffer, afd.eBuffer, afd.motionJpeg);
}

/*
===============
CL_VideoRecordingFrame

SP: called once per drawn frame, after the scene and the menus and before the console and the recording status:
captures this frame if the video (at cl_aviFrameRate, real time) is owed one or more frames
===============
*/
void CL_VideoRecordingFrame(void)
{
	if (!afd.fileOpen)
		return;

	const int elapsed = Sys_Milliseconds() - afd.startTime;
	const int due = static_cast<int>(static_cast<long long>(elapsed) * afd.frameRate / 1000) + 1;
	int owed = due - afd.framesRequested;

	if (owed <= 0)
		return;

	// a long hitch (loading a level) doesn't freeze the video for seconds: at most one second is filled in
	if (owed > afd.frameRate)
	{
		afd.framesRequested += owed - afd.frameRate;
		owed = afd.frameRate;
	}

	afd.framesRequested += owed;
	if (afd.pendingCount < 32)
	{
		afd.pendingFrames[(afd.pendingHead + afd.pendingCount) % 32] = owed;
		afd.pendingCount++;
	}
	CL_TakeVideoFrame();
}

/*
===============
CL_CloseAVI

Closes the AVI file and writes an index chunk
===============
*/
qboolean CL_CloseAVI(void)
{
	// the frames still being compressed on the renderer's workers go in first
	if (afd.fileOpen && re.FlushVideoFrames)
	{
		re.FlushVideoFrames();
	}

	int indexSize = afd.numIndices * 16;
	const char* idxFileName = va("%s" INDEX_FILE_EXTENSION, afd.fileName);

	// AVI file isn't open
	if (!afd.fileOpen)
		return qfalse;

	afd.fileOpen = qfalse;

	FS_Seek(afd.idxF, 4, FS_SEEK_SET);
	bufIndex = 0;
	WRITE_4BYTES(indexSize);
	SafeFS_Write(buffer, bufIndex, afd.idxF);
	FS_FCloseFile(afd.idxF);

	// Write index

	// Open the temp index file
	if ((indexSize = FS_FOpenFileRead(idxFileName,
		&afd.idxF, qtrue)) <= 0)
	{
		FS_FCloseFile(afd.f);
		return qfalse;
	}

	int indexRemainder = indexSize;

	// Append index to end of avi file
	while (indexRemainder > MAX_AVI_BUFFER)
	{
		FS_Read(buffer, MAX_AVI_BUFFER, afd.idxF);
		SafeFS_Write(buffer, MAX_AVI_BUFFER, afd.f);
		afd.fileSize += MAX_AVI_BUFFER;
		indexRemainder -= MAX_AVI_BUFFER;
	}
	FS_Read(buffer, indexRemainder, afd.idxF);
	SafeFS_Write(buffer, indexRemainder, afd.f);
	afd.fileSize += indexRemainder;
	FS_FCloseFile(afd.idxF);

	// Remove temp index file
	FS_HomeRemove(idxFileName);

	// Write the real header
	FS_Seek(afd.f, 0, FS_SEEK_SET);
	CL_WriteAVIHeader();

	bufIndex = 4;
	WRITE_4BYTES(afd.fileSize - 8); // "RIFF" size

	bufIndex = afd.moviOffset + 4; // Skip "LIST"
	WRITE_4BYTES(afd.moviSize);

	SafeFS_Write(buffer, bufIndex, afd.f);

	Z_Free(afd.cBuffer);
	Z_Free(afd.eBuffer);
	FS_FCloseFile(afd.f);

	Com_Printf("Wrote %d:%d frames to %s (%.1f s, %d audio bytes)\n", afd.numVideoFrames, afd.numAudioFrames, afd.fileName,
		(Sys_Milliseconds() - afd.startTime) / 1000.0f, afd.a.totalBytes);

	// SP: tell the player where it went (the real path, in the homepath: Documents/My Games/...)
	Q_strncpyz(aviSavedPath, FS_BuildOSPath(Cvar_VariableString("fs_homepath"), nullptr, afd.fileName), sizeof aviSavedPath);
#ifdef _WIN32
	for (char* p = aviSavedPath; *p; p++)
	{
		if (*p == '/')
			*p = '\\';
	}
#endif
	aviSavedTime = Sys_Milliseconds();
	aviSavedFrames = afd.numVideoFrames;
	Com_Printf(S_COLOR_GREEN "Video saved: %s\n", aviSavedPath);

	return qtrue;
}

/*
===============
CL_VideoRecording
===============
*/
qboolean CL_VideoRecording(void)
{
	return afd.fileOpen;
}
/*
===============
CL_VideoFilename

demos/video<date>_<time>.avi, next to the MP demos (homepath: Documents/My Games/MovieDuels/<game>/demos)
===============
*/
static void CL_VideoFilename(char* buf, const int bufSize)
{
	time_t rawtime;
	char timeStr[32] = { 0 };

	time(&rawtime);
	strftime(timeStr, sizeof timeStr, "%Y-%m-%d_%H-%M-%S", localtime(&rawtime));

	Com_sprintf(buf, bufSize, "demos/video%s.avi", timeStr);
}

/*
===============
CL_Video_f

video [filename] (also recorddemo, the Controls bind)
===============
*/
static void CL_Video_f(void)
{
	char filename[MAX_OSPATH];

	if (afd.fileOpen)
	{
		Com_Printf("Already recording a video (%s)\n", afd.fileName);
		return;
	}

	if (Cmd_Argc() == 2)
	{
		// explicit filename
		Com_sprintf(filename, MAX_OSPATH, "demos/%s.avi", Cmd_Argv(1));
	}
	else
	{
		CL_VideoFilename(filename, MAX_OSPATH);

		if (FS_FileExists(filename))
		{
			Com_Printf("Video: Couldn't create a file\n");
			return;
		}
	}

	if (CL_OpenAVIForWriting(filename))
	{
		aviSavedTime = 0;
		Com_Printf("Recording video to %s\n", filename);
	}
	else
	{
		Com_Printf(S_COLOR_RED "Video: couldn't start recording %s\n", filename);
	}
}

/*
===============
CL_StopVideo_f

stopvideo (also stoprecord, the Controls bind)
===============
*/
static void CL_StopVideo_f(void)
{
	if (!afd.fileOpen)
	{
		Com_Printf("Not recording a video\n");
		return;
	}
	CL_CloseAVI();
}

/*
===============
CL_DrawVideoRecordingStatus

SP: drawn after the video frame is captured, so it isn't in the video. While recording: a blinking red dot and
the recording time; after stopping: where the file was saved, for a few seconds.
===============
*/
constexpr int AVI_SAVED_MESSAGE_TIME = 8000;
extern console_t con;

void CL_DrawVideoRecordingStatus(void)
{
	static vec4_t red = { 1.0f, 0.15f, 0.15f, 1.0f };
	static vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	static vec4_t shade = { 0.0f, 0.0f, 0.0f, 0.6f };

	if (afd.fileOpen)
	{
		const int secs = (Sys_Milliseconds() - afd.startTime) / 1000;
		const char* text = va("REC %02d:%02d:%02d", secs / 3600, secs / 60 % 60, secs % 60);
		const int w = static_cast<int>(strlen(text)) * BIGCHAR_WIDTH;
		const int x = SCREEN_WIDTH - w - 12;
		constexpr int y = 10;

		SCR_FillRect(x - 26, y - 4, w + 34, BIGCHAR_HEIGHT + 8, shade);
		if (secs % 2 == 0)
		{
			SCR_FillRect(x - 20, y + 3, 10, 10, red);
		}
		SCR_DrawBigStringColor(x, y, text, red, qtrue);
		re.SetColor(nullptr);
		return;
	}

	if (aviSavedTime && Sys_Milliseconds() - aviSavedTime < AVI_SAVED_MESSAGE_TIME)
	{
		const char* line1 = va("Video saved (%d frames):", aviSavedFrames);
		constexpr int y = 84; // below the console notify lines

		// small chars are drawn at native resolution (scaled by con.yadjust), the box in 640x480
		SCR_FillRect(0, (y - 4) * con.yadjust, SCREEN_WIDTH, (SMALLCHAR_HEIGHT * 2 + 12) * con.yadjust, shade);
		SCR_DrawSmallStringExt(8, y, line1, white, qtrue, qtrue);
		SCR_DrawSmallStringExt(8, y + SMALLCHAR_HEIGHT + 4, aviSavedPath, white, qtrue, qtrue);
		re.SetColor(nullptr);
	}
}

/*
===============
CL_InitVideoRecording
===============
*/
void CL_InitVideoRecording(void)
{
	cl_aviFrameRate = Cvar_Get("cl_aviFrameRate", "25", CVAR_ARCHIVE);
	cl_aviMotionJpeg = Cvar_Get("cl_aviMotionJpeg", "1", CVAR_ARCHIVE);
	cl_avi2GBLimit = Cvar_Get("cl_avi2GBLimit", "1", CVAR_ARCHIVE);

	Cmd_AddCommand("video", CL_Video_f);
	Cmd_AddCommand("stopvideo", CL_StopVideo_f);
	// the Controls > Other binds (MP's names; SP has no demos, so these record a video)
	Cmd_AddCommand("recorddemo", CL_Video_f);
	Cmd_AddCommand("stoprecord", CL_StopVideo_f);
}

void CL_ShutdownVideoRecording(void)
{
	if (afd.fileOpen)
	{
		CL_CloseAVI();
	}
	Cmd_RemoveCommand("video");
	Cmd_RemoveCommand("stopvideo");
	Cmd_RemoveCommand("recorddemo");
	Cmd_RemoveCommand("stoprecord");
}
