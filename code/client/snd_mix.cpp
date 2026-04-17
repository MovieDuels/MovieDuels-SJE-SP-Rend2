/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "../server/exe_headers.h"

#include "snd_local.h"

portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int* snd_p, snd_linear_count, snd_vol;
short* snd_out;

void S_WriteLinearBlastStereo16()
{
	for (int i = 0; i < snd_linear_count; i += 2)
	{
		int val = snd_p[i] >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < static_cast<short>(0x8000))
			snd_out[i] = static_cast<short>(0x8000);
		else
			snd_out[i] = val;

		val = snd_p[i + 1] >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < static_cast<short>(0x8000))
			snd_out[i + 1] = static_cast<short>(0x8000);
		else
			snd_out[i + 1] = val;
	}
}

void S_TransferStereo16(unsigned long* pbuf, const int endtime)
{
	snd_p = reinterpret_cast<int*>(paintbuffer);
	int ls_paintedtime = s_paintedtime;

	while (ls_paintedtime < endtime)
	{
		// handle recirculating buffer issues
		const int lpos = ls_paintedtime & ((dma.samples >> 1) - 1);

		snd_out = reinterpret_cast<short*>(pbuf) + (lpos << 1);

		snd_linear_count = (dma.samples >> 1) - lpos;
		if (ls_paintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - ls_paintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		ls_paintedtime += (snd_linear_count >> 1);
	}
}

/*
===================
S_TransferPaintBuffer

===================
*/
void S_TransferPaintBuffer(const int endtime)
{
	const auto pbuf = reinterpret_cast<unsigned long*>(dma.buffer);

	if (s_testsound->integer)
	{
		// write a fixed sine wave
		const int count = (endtime - s_paintedtime);
		for (int i = 0; i < count; i++)
			paintbuffer[i].left = paintbuffer[i].right = static_cast<int>(sin((s_paintedtime + i) * 0.1) * 20000 * 256);
	}

	if (dma.samplebits == 16 && dma.channels == 2)
	{
		// optimized case
		S_TransferStereo16(pbuf, endtime);
	}
	else
	{
		int val;
		// general case
		auto p = reinterpret_cast<int*>(paintbuffer);
		int count = (endtime - s_paintedtime) * dma.channels;
		const int out_mask = dma.samples - 1;
		int out_idx = s_paintedtime * dma.channels & out_mask;
		const int step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			const auto out = reinterpret_cast<short*>(pbuf);
			while (count--)
			{
				val = *p >> 8;
				p += step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < static_cast<short>(0x8000))
					val = static_cast<short>(0x8000);
				out[out_idx] = static_cast<short>(val);
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			const auto out = reinterpret_cast<unsigned char*>(pbuf);
			while (count--)
			{
				val = *p >> 8;
				p += step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < static_cast<short>(0x8000))
					val = static_cast<short>(0x8000);
				out[out_idx] = static_cast<short>((val >> 8) + 128);
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/
static void S_PaintChannelFrom16(const channel_t* ch, const sfx_t* sfx, const int count, int sampleOffset, const int bufferOffset)
{
	const int iLeftVol = ch->leftvol * snd_vol;
	const int iRightVol = ch->rightvol * snd_vol;

	portable_samplepair_t* pSamplesDest = &paintbuffer[bufferOffset];

	for (int i = 0; i < count; i++)
	{
		const int iData = sfx->pSoundData[sampleOffset++];

		pSamplesDest[i].left += (iData * iLeftVol) >> 8;
		pSamplesDest[i].right += (iData * iRightVol) >> 8;
	}
}

void S_PaintChannelFromMP3(channel_t* ch, const sfx_t* sc, int count, const int sampleOffset, const int bufferOffset)
{
	int data;
	static short tempMP3Buffer[PAINTBUFFER_SIZE];

	MP3Stream_GetSamples(ch, sampleOffset, count, tempMP3Buffer, qfalse); // qfalse = not stereo

	const int leftvol = ch->leftvol * snd_vol;
	const int rightvol = ch->rightvol * snd_vol;
	signed short* sfx = tempMP3Buffer;

	portable_samplepair_t* samp = &paintbuffer[bufferOffset];

	while (count & 3)
	{
		data = *sfx;
		samp->left += (data * leftvol) >> 8;
		samp->right += (data * rightvol) >> 8;

		sfx++;
		samp++;
		count--;
	}

	for (int i = 0; i < count; i += 4)
	{
		data = sfx[i];
		samp[i].left += (data * leftvol) >> 8;
		samp[i].right += (data * rightvol) >> 8;

		data = sfx[i + 1];
		samp[i + 1].left += (data * leftvol) >> 8;
		samp[i + 1].right += (data * rightvol) >> 8;

		data = sfx[i + 2];
		samp[i + 2].left += (data * leftvol) >> 8;
		samp[i + 2].right += (data * rightvol) >> 8;

		data = sfx[i + 3];
		samp[i + 3].left += (data * leftvol) >> 8;
		samp[i + 3].right += (data * rightvol) >> 8;
	}
}

// subroutinised to save code dup (called twice)	-ste
//
void ChannelPaint(channel_t* ch, const sfx_t* sc, const int count, const int sampleOffset, const int bufferOffset)
{
	switch (sc->eSoundCompressionMethod)
	{
	case ct_16:

		S_PaintChannelFrom16(ch, sc, count, sampleOffset, bufferOffset);
		break;

	case ct_MP3:

		S_PaintChannelFromMP3(ch, sc, count, sampleOffset, bufferOffset);
		break;

	default:

		assert(0); // debug aid, ignored in release. FIXME: Should we ERR_DROP here for badness-catch?
		break;
	}
}

void S_PaintChannels(const int endtime)
{
	int i;
	int sampleOffset;
	int normal_vol;

	snd_vol = normal_vol = s_volume->value * 256.0f;
	const int voice_vol = (s_volumeVoice->value * 256.0f);

	//Com_Printf ("%i to %i\n", s_paintedtime, endtime);
	while (s_paintedtime < endtime)
	{
		// if paintbuffer is smaller than DMA buffer
		// we may need to fill it multiple times
		int end = endtime;
		if (endtime - s_paintedtime > PAINTBUFFER_SIZE)
		{
			end = s_paintedtime + PAINTBUFFER_SIZE;
		}

		// clear the paint buffer to either music or zeros
		if (s_rawend < s_paintedtime)
		{
			if (s_rawend)
			{
				//Com_DPrintf ("background sound underrun\n");
			}
			memset(paintbuffer, 0, (end - s_paintedtime) * sizeof(portable_samplepair_t));
		}
		else
		{
			const int stop = (end < s_rawend) ? end : s_rawend;

			for (i = s_paintedtime; i < stop; i++)
			{
				const int s = i & (MAX_RAW_SAMPLES - 1);
				paintbuffer[i - s_paintedtime] = s_rawsamples[s];
			}
			//		if (i != end)
			//			Com_Printf ("partial stream\n");
			//		else
			//			Com_Printf ("full stream\n");
			for (; i < end; i++)
			{
				paintbuffer[i - s_paintedtime].left =
					paintbuffer[i - s_paintedtime].right = 0;
			}
		}

		// paint in the channels.
		channel_t* ch = s_channels;
		for (i = 0; i < MAX_CHANNELS; i++, ch++)
		{
			if (!ch->thesfx || (ch->leftvol < 0.25 && ch->rightvol < 0.25))
			{
				continue;
			}

			if (ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN || ch->entchannel ==
				CHAN_VOICE_GLOBAL)
				snd_vol = voice_vol;
			else
				snd_vol = normal_vol;

			int ltime = s_paintedtime;
			const sfx_t* sc = ch->thesfx;

			// we might have to make 2 passes if it is
			//	a looping sound effect and the end of
			//	the sameple is hit...
			//
			do
			{
				if (ch->loopSound)
				{
					sampleOffset = ltime % sc->iSoundLengthInSamples;
				}
				else
				{
					sampleOffset = ltime - ch->startSample;
				}

				int count = end - ltime;
				if (sampleOffset + count > sc->iSoundLengthInSamples)
				{
					count = sc->iSoundLengthInSamples - sampleOffset;
				}

				if (count > 0)
				{
					ChannelPaint(ch, sc, count, sampleOffset, ltime - s_paintedtime);
					ltime += count;
				}
			} while (ltime < end && ch->loopSound);
		}
		/* temprem
				// paint in the looped channels.
				ch = loop_channels;
				for ( i = 0; i < numLoopChannels ; i++, ch++ ) {
					if ( !ch->thesfx || (!ch->leftvol && !ch->rightvol )) {
						continue;
					}

					{
						ltime = s_paintedtime;
						sc = ch->thesfx;

						if (sc->soundData==NULL || sc->soundLength==0) {
							continue;
						}
						// we might have to make two passes if it
						// is a looping sound effect and the end of
						// the sample is hit
						do {
							sampleOffset = (ltime % sc->soundLength);

							count = end - ltime;
							if ( sampleOffset + count > sc->soundLength ) {
								count = sc->soundLength - sampleOffset;
							}

							if ( count > 0 )
							{
								ChannelPaint(ch, sc, count, sampleOffset, ltime - s_paintedtime);
								ltime += count;
							}
						} while ( ltime < end);
					}
				}
		*/
		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		s_paintedtime = end;
	}
}