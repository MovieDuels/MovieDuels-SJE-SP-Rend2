/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

#include "../server/exe_headers.h"

/*****************************************************************************
 * name:		cl_cin.c
 *
 * desc:		video and cinematic playback
 *
 * $Archive: /MissionPack/code/client/cl_cin.c $
 * $Author: Ttimo $
 * $Revision: 82 $
 * $Modtime: 4/13/01 4:48p $
 * $Date: 4/13/01 4:48p $
 *
 * cl_glconfig.hwtype trtypes 3dfx/ragepro need 256x256
 *
 *****************************************************************************/

#include "client.h"
#include "client_ui.h"	// CHC
#include "snd_local.h"
#include "qcommon/stringed_ingame.h"

constexpr auto MAXSIZE = 8;
constexpr auto MINSIZE = 4;

constexpr auto DEFAULT_CIN_WIDTH = 512;
constexpr auto DEFAULT_CIN_HEIGHT = 512;

constexpr auto ROQ_QUAD = 0x1000;
constexpr auto ROQ_QUAD_INFO = 0x1001;
constexpr auto ROQ_CODEBOOK = 0x1002;
constexpr auto ROQ_QUAD_VQ = 0x1011;
constexpr auto ROQ_QUAD_JPEG = 0x1012;
constexpr auto ROQ_QUAD_HANG = 0x1013;
constexpr auto ROQ_PACKET = 0x1030;
constexpr auto ZA_SOUND_MONO = 0x1020;
constexpr auto ZA_SOUND_STEREO = 0x1021;

constexpr auto MAX_VIDEO_HANDLES = 32;

extern void S_CIN_StopSound(sfxHandle_t sfxHandle);
static void RoQ_init();

/******************************************************************************
*
* Class:		trFMV
*
* Description:	RoQ/RnR manipulation routines
*				not entirely complete for first run
*
******************************************************************************/

static long ROQ_YY_tab[256];
static long ROQ_UB_tab[256];
static long ROQ_UG_tab[256];
static long ROQ_VG_tab[256];
static long ROQ_VR_tab[256];
static unsigned short vq2[256 * 16 * 4];
static unsigned short vq4[256 * 64 * 4];
static unsigned short vq8[256 * 256 * 4];

using cinematics_t = struct
{
	byte linbuf[DEFAULT_CIN_WIDTH * DEFAULT_CIN_HEIGHT * 4 * 2];
	byte file[65536];
	short sqr_table[256];

	int mcomp[256];
	byte* q_status[2][32768];

	long old_x_off, old_y_off, oldysize, oldxsize;

	int current_handle;
};

using cin_cache = struct
{
	char fileName[MAX_OSPATH];
	int CIN_WIDTH, CIN_HEIGHT;
	int xpos, ypos, width, height;
	qboolean looping, holdAtEnd, dirty, alterGameState, silent, shader;
	fileHandle_t iFile; // 0 = none
	e_status status;
	unsigned int startTime;
	unsigned int lastTime;
	long tfps;
	long RoQPlayed;
	long ROQSize;
	unsigned int RoQFrameSize;
	long onQuad;
	long numQuads;
	long samplesPerLine;
	unsigned int roq_id;
	long screenDelta;

	void (*VQ0)(byte* status, void* qdata);
	void (*VQ1)(byte* status, void* qdata);
	void (*VQNormal)(byte* status, void* qdata);
	void (*VQBuffer)(byte* status, void* qdata);

	long samplesPerPixel; // defaults to 2
	byte* gray;
	unsigned int xsize, ysize, maxsize, minsize;

	qboolean half, smootheddouble, inMemory;
	long normalBuffer0;
	long roq_flags;
	long roqF0;
	long roqF1;
	long t[2];
	long roqFPS;
	int playonwalls;
	byte* buf;
	long drawX, drawY;
	sfxHandle_t hSFX; // 0 = none
	qhandle_t hCRAWLTEXT; // 0 = none
};

static cinematics_t cin;
static cin_cache cinTable[MAX_VIDEO_HANDLES];
static int currentHandle = -1;
static int CL_handle = -1;
static int CL_iPlaybackStartTime; // so I can stop users quitting playback <1 second after it starts

extern int s_soundtime; // sample PAIRS
extern int s_paintedtime; // sample PAIRS

void CIN_CloseAllVideos()
{
	for (int i = 0; i < MAX_VIDEO_HANDLES; i++)
	{
		if (cinTable[i].fileName[0] != 0)
		{
			CIN_StopCinematic(i);
		}
	}
}

static int CIN_HandleForVideo()
{
	for (int i = 0; i < MAX_VIDEO_HANDLES; i++)
	{
		if (cinTable[i].fileName[0] == 0)
		{
			return i;
		}
	}
	Com_Error(ERR_DROP, "CIN_HandleForVideo: none free");
}

//-----------------------------------------------------------------------------
// RllSetupTable
//
// Allocates and initializes the square table.
//
// Parameters:	None
//
// Returns:		Nothing
//-----------------------------------------------------------------------------
static void RllSetupTable()
{
	for (int z = 0; z < 128; z++)
	{
		cin.sqr_table[z] = static_cast<short>(z * z);
		cin.sqr_table[z + 128] = static_cast<short>(-cin.sqr_table[z]);
	}
}

//-----------------------------------------------------------------------------
// RllDecodeMonoToMono
//
// Decode mono source data into a mono buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= # of shorts of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
/*
static long RllDecodeMonoToMono(unsigned char *from,short *to,unsigned int size,char signedOutput ,unsigned short flag)
{
	unsigned int z;
	int prev;

	if (signedOutput)
		prev =  flag - 0x8000;
	else
		prev = flag;

	for (z=0;z<size;z++) {
		prev = to[z] = (short)(prev + cin.sqrTable[from[z]]);
	}
	return size;	//*sizeof(short));
}
*/

//-----------------------------------------------------------------------------
// RllDecodeMonoToStereo
//
// Decode mono source data into a stereo buffer. Output is 4 times the number
// of bytes in the input.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= 1/4 # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
static long RllDecodeMonoToStereo(const unsigned char* from, short* to, const unsigned int size, const char signed_output,
	const unsigned short flag)
{
	int prev;

	if (signed_output)
		prev = flag - 0x8000;
	else
		prev = flag;

	for (unsigned int z = 0; z < size; z++)
	{
		prev = static_cast<short>(prev + cin.sqr_table[from[z]]);
		to[z * 2 + 0] = to[z * 2 + 1] = static_cast<short>(prev);
	}

	return size; // * 2 * sizeof(short));
}

//-----------------------------------------------------------------------------
// RllDecodeStereoToStereo
//
// Decode stereo source data into a stereo buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= 1/2 # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
static long RllDecodeStereoToStereo(unsigned char* from, short* to, const unsigned int size, const char signed_output,
	const unsigned short flag)
{
	unsigned char* zz = from;
	int prev_l, prev_r;

	if (signed_output)
	{
		prev_l = (flag & 0xff00) - 0x8000;
		prev_r = ((flag & 0x00ff) << 8) - 0x8000;
	}
	else
	{
		prev_l = flag & 0xff00;
		prev_r = (flag & 0x00ff) << 8;
	}

	for (unsigned int z = 0; z < size; z += 2)
	{
		prev_l = static_cast<short>(prev_l + cin.sqr_table[*zz++]);
		prev_r = static_cast<short>(prev_r + cin.sqr_table[*zz++]);
		to[z + 0] = static_cast<short>(prev_l);
		to[z + 1] = static_cast<short>(prev_r);
	}

	return size >> 1; //*sizeof(short));
}

//-----------------------------------------------------------------------------
// RllDecodeStereoToMono
//
// Decode stereo source data into a mono buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
/*
static long RllDecodeStereoToMono(unsigned char *from,short *to,unsigned int size,char signedOutput, unsigned short flag)
{
	unsigned int z;
	int prevL,prevR;

	if (signedOutput) {
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) -0x8000;
	} else {
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z=0;z<size;z+=1) {
		prevL= prevL + cin.sqrTable[from[z*2]];
		prevR = prevR + cin.sqrTable[from[z*2+1]];
		to[z] = (short)((prevL + prevR)/2);
	}

	return size;
}
*/
/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void move8_32(byte* src, byte* dst, const int spl)
{
	for (int i = 0; i < 8; ++i)
	{
		memcpy(dst, src, 32);
		src += spl;
		dst += spl;
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void move4_32(byte* src, byte* dst, const int spl)
{
	for (int i = 0; i < 4; ++i)
	{
		memcpy(dst, src, 16);
		src += spl;
		dst += spl;
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void blit8_32(byte* src, byte* dst, const int spl)
{
	for (int i = 0; i < 8; ++i)
	{
		memcpy(dst, src, 32);
		src += 32;
		dst += spl;
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/
static void blit4_32(byte* src, byte* dst, const int spl)
{
	for (int i = 0; i < 4; ++i)
	{
		memmove(dst, src, 16);
		src += 16;
		dst += spl;
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void blit2_32(const byte* src, byte* dst, const int spl)
{
	memcpy(dst, src, 8);
	memcpy(dst + spl, src + 8, 8);
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void blitVQQuad32fs(byte** status, unsigned char* data)
{
	unsigned int i;

	unsigned short newd = 0;
	unsigned short celdata = 0;
	unsigned int index = 0;

	const int spl = cinTable[currentHandle].samplesPerLine;

	do
	{
		if (!newd)
		{
			newd = 7;
			celdata = data[0] + data[1] * 256;
			data += 2;
		}
		else
		{
			newd--;
		}

		unsigned short code = static_cast<unsigned short>(celdata & 0xc000);
		celdata <<= 2;

		switch (code)
		{
		case 0x8000: // vq code
			blit8_32(reinterpret_cast<byte*>(&vq8[*data * 128]), status[index], spl);
			data++;
			index += 5;
			break;
		case 0xc000: // drop
			index++; // skip 8x8
			for (i = 0; i < 4; i++)
			{
				if (!newd)
				{
					newd = 7;
					celdata = data[0] + data[1] * 256;
					data += 2;
				}
				else
				{
					newd--;
				}

				code = static_cast<unsigned short>(celdata & 0xc000);
				celdata <<= 2;

				switch (code)
				{
					// code in top two bits of code
				case 0x8000: // 4x4 vq code
					blit4_32(reinterpret_cast<byte*>(&vq4[*data * 32]), status[index], spl);
					data++;
					break;
				case 0xc000: // 2x2 vq code
					blit2_32(reinterpret_cast<byte*>(&vq2[*data * 8]), status[index], spl);
					data++;
					blit2_32(reinterpret_cast<byte*>(&vq2[*data * 8]), status[index] + 8, spl);
					data++;
					blit2_32(reinterpret_cast<byte*>(&vq2[*data * 8]), status[index] + spl * 2, spl);
					data++;
					blit2_32(reinterpret_cast<byte*>(&vq2[*data * 8]), status[index] + spl * 2 + 8, spl);
					data++;
					break;
				case 0x4000: // motion compensation
					move4_32(status[index] + cin.mcomp[(*data)], status[index], spl);
					data++;
					break;
				default:;
				}
				index++;
			}
			break;
		case 0x4000: // motion compensation
			move8_32(status[index] + cin.mcomp[(*data)], status[index], spl);
			data++;
			index += 5;
			break;
		case 0x0000:
			index += 5;
			break;
		default:;
		}
	} while (status[index] != nullptr);
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void ROQ_GenYUVTables()
{
	constexpr float t_ub = 1.77200f / 2.0f * static_cast<float>(1 << 6) + 0.5f;
	constexpr float t_vr = 1.40200f / 2.0f * static_cast<float>(1 << 6) + 0.5f;
	constexpr float t_ug = 0.34414f / 2.0f * static_cast<float>(1 << 6) + 0.5f;
	constexpr float t_vg = 0.71414f / 2.0f * static_cast<float>(1 << 6) + 0.5f;
	for (long i = 0; i < 256; i++)
	{
		const float x = static_cast<float>(2 * i - 255);

		ROQ_UB_tab[i] = static_cast<long>(t_ub * x + (1 << 5));
		ROQ_VR_tab[i] = static_cast<long>(t_vr * x + (1 << 5));
		ROQ_UG_tab[i] = static_cast<long>((-t_ug * x));
		ROQ_VG_tab[i] = static_cast<long>(-t_vg * x + (1 << 5));
		ROQ_YY_tab[i] = i << 6 | i >> 2;
	}
}

#define VQ2TO4(a,b,c,d) { \
    	*c++ = a[0];	\
	*d++ = a[0];	\
	*d++ = a[0];	\
	*c++ = a[1];	\
	*d++ = a[1];	\
	*d++ = a[1];	\
	*c++ = b[0];	\
	*d++ = b[0];	\
	*d++ = b[0];	\
	*c++ = b[1];	\
	*d++ = b[1];	\
	*d++ = b[1];	\
	*d++ = a[0];	\
	*d++ = a[0];	\
	*d++ = a[1];	\
	*d++ = a[1];	\
	*d++ = b[0];	\
	*d++ = b[0];	\
	*d++ = b[1];	\
	*d++ = b[1];	\
	a += 2; b += 2; }

#define VQ2TO2(a,b,c,d) { \
	*c++ = *a;	\
	*d++ = *a;	\
	*d++ = *a;	\
	*c++ = *b;	\
	*d++ = *b;	\
	*d++ = *b;	\
	*d++ = *a;	\
	*d++ = *a;	\
	*d++ = *b;	\
	*d++ = *b;	\
	a++; b++; }

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static unsigned short yuv_to_rgb(const long y, const long u, const long v)
{
	const long yy = ROQ_YY_tab[y];

	long r = (yy + ROQ_VR_tab[v]) >> 9;
	long g = (yy + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 8;
	long b = (yy + ROQ_UB_tab[u]) >> 9;

	if (r < 0)
		r = 0;
	if (g < 0)
		g = 0;
	if (b < 0)
		b = 0;
	if (r > 31)
		r = 31;
	if (g > 63)
		g = 63;
	if (b > 31)
		b = 31;

	return static_cast<unsigned short>((r << 11) + (g << 5) + b);
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static unsigned int yuv_to_rgb24(const long y, const long u, const long v)
{
	const long yy = ROQ_YY_tab[y];

	long r = (yy + ROQ_VR_tab[v]) >> 6;
	long g = (yy + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 6;
	long b = (yy + ROQ_UB_tab[u]) >> 6;

	if (r < 0)
		r = 0;
	if (g < 0)
		g = 0;
	if (b < 0)
		b = 0;
	if (r > 255)
		r = 255;
	if (g > 255)
		g = 255;
	if (b > 255)
		b = 255;

	return LittleLong r | g << 8 | b << 16 | 255 << 24;
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void decodeCodeBook(byte* input, unsigned short roq_flags)
{
	long i, j, two, four;
	unsigned short* aptr, * bptr, * cptr, * dptr;
	long y0, y2, cr, cb;
	byte* bbptr, * baptr, * bcptr, * bdptr;
	union
	{
		unsigned int* i;
		unsigned short* s;
	} iaptr{}, ibptr{}, icptr{}, idptr{};

	if (!roq_flags)
	{
		two = four = 256;
	}
	else
	{
		two = roq_flags >> 8;
		if (!two) two = 256;
		four = roq_flags & 0xff;
	}

	four *= 2;

	bptr = static_cast<unsigned short*>(vq2);

	if (!cinTable[currentHandle].half)
	{
		long y3;
		long y1;
		if (!cinTable[currentHandle].smootheddouble)
		{
			//
			// normal height
			//
			if (cinTable[currentHandle].samplesPerPixel == 2)
			{
				for (i = 0; i < two; i++)
				{
					y0 = static_cast<long>(*input++);
					y1 = static_cast<long>(*input++);
					y2 = static_cast<long>(*input++);
					y3 = static_cast<long>(*input++);
					cr = static_cast<long>(*input++);
					cb = static_cast<long>(*input++);
					*bptr++ = yuv_to_rgb(y0, cr, cb);
					*bptr++ = yuv_to_rgb(y1, cr, cb);
					*bptr++ = yuv_to_rgb(y2, cr, cb);
					*bptr++ = yuv_to_rgb(y3, cr, cb);
				}

				cptr = static_cast<unsigned short*>(vq4);
				dptr = static_cast<unsigned short*>(vq8);

				for (i = 0; i < four; i++)
				{
					aptr = static_cast<unsigned short*>(vq2) + *input++ * 4;
					bptr = static_cast<unsigned short*>(vq2) + *input++ * 4;
					for (j = 0; j < 2; j++)
						VQ2TO4(aptr, bptr, cptr, dptr);
				}
			}
			else if (cinTable[currentHandle].samplesPerPixel == 4)
			{
				ibptr.s = bptr;
				for (i = 0; i < two; i++)
				{
					y0 = static_cast<long>(*input++);
					y1 = static_cast<long>(*input++);
					y2 = static_cast<long>(*input++);
					y3 = static_cast<long>(*input++);
					cr = static_cast<long>(*input++);
					cb = static_cast<long>(*input++);
					*ibptr.i++ = yuv_to_rgb24(y0, cr, cb);
					*ibptr.i++ = yuv_to_rgb24(y1, cr, cb);
					*ibptr.i++ = yuv_to_rgb24(y2, cr, cb);
					*ibptr.i++ = yuv_to_rgb24(y3, cr, cb);
				}

				icptr.s = vq4;
				idptr.s = vq8;

				for (i = 0; i < four; i++)
				{
					iaptr.s = vq2;
					iaptr.i += *input++ * 4;
					ibptr.s = vq2;
					ibptr.i += *input++ * 4;
					for (j = 0; j < 2; j++)
						VQ2TO4(iaptr.i, ibptr.i, icptr.i, idptr.i);
				}
			}
			else if (cinTable[currentHandle].samplesPerPixel == 1)
			{
				bbptr = reinterpret_cast<byte*>(bptr);
				for (i = 0; i < two; i++)
				{
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input];
					input += 3;
				}

				bcptr = reinterpret_cast<byte*>(vq4);
				bdptr = reinterpret_cast<byte*>(vq8);

				for (i = 0; i < four; i++)
				{
					baptr = reinterpret_cast<byte*>(vq2) + *input++ * 4;
					bbptr = reinterpret_cast<byte*>(vq2) + *input++ * 4;
					for (j = 0; j < 2; j++)
						VQ2TO4(baptr, bbptr, bcptr, bdptr);
				}
			}
		}
		else
		{
			//
			// double height, smoothed
			//
			if (cinTable[currentHandle].samplesPerPixel == 2)
			{
				for (i = 0; i < two; i++)
				{
					y0 = static_cast<long>(*input++);
					y1 = static_cast<long>(*input++);
					y2 = static_cast<long>(*input++);
					y3 = static_cast<long>(*input++);
					cr = static_cast<long>(*input++);
					cb = static_cast<long>(*input++);
					*bptr++ = yuv_to_rgb(y0, cr, cb);
					*bptr++ = yuv_to_rgb(y1, cr, cb);
					*bptr++ = yuv_to_rgb((y0 * 3 + y2) / 4, cr, cb);
					*bptr++ = yuv_to_rgb((y1 * 3 + y3) / 4, cr, cb);
					*bptr++ = yuv_to_rgb((y0 + y2 * 3) / 4, cr, cb);
					*bptr++ = yuv_to_rgb((y1 + y3 * 3) / 4, cr, cb);
					*bptr++ = yuv_to_rgb(y2, cr, cb);
					*bptr++ = yuv_to_rgb(y3, cr, cb);
				}

				cptr = static_cast<unsigned short*>(vq4);
				dptr = static_cast<unsigned short*>(vq8);

				for (i = 0; i < four; i++)
				{
					aptr = static_cast<unsigned short*>(vq2) + *input++ * 8;
					bptr = static_cast<unsigned short*>(vq2) + *input++ * 8;
					for (j = 0; j < 2; j++)
					{
						VQ2TO4(aptr, bptr, cptr, dptr);
						VQ2TO4(aptr, bptr, cptr, dptr);
					}
				}
			}
			else if (cinTable[currentHandle].samplesPerPixel == 4)
			{
				ibptr.s = bptr;
				for (i = 0; i < two; i++)
				{
					y0 = static_cast<long>(*input++);
					y1 = static_cast<long>(*input++);
					y2 = static_cast<long>(*input++);
					y3 = static_cast<long>(*input++);
					cr = static_cast<long>(*input++);
					cb = static_cast<long>(*input++);
					*ibptr.i++ = yuv_to_rgb24(y0, cr, cb);
					*ibptr.i++ = yuv_to_rgb24(y1, cr, cb);
					*ibptr.i++ = yuv_to_rgb24((y0 * 3 + y2) / 4, cr, cb);
					*ibptr.i++ = yuv_to_rgb24((y1 * 3 + y3) / 4, cr, cb);
					*ibptr.i++ = yuv_to_rgb24((y0 + y2 * 3) / 4, cr, cb);
					*ibptr.i++ = yuv_to_rgb24((y1 + y3 * 3) / 4, cr, cb);
					*ibptr.i++ = yuv_to_rgb24(y2, cr, cb);
					*ibptr.i++ = yuv_to_rgb24(y3, cr, cb);
				}

				icptr.s = vq4;
				idptr.s = vq8;

				for (i = 0; i < four; i++)
				{
					iaptr.s = vq2;
					iaptr.i += *input++ * 8;
					ibptr.s = vq2;
					ibptr.i += *input++ * 8;
					for (j = 0; j < 2; j++)
					{
						VQ2TO4(iaptr.i, ibptr.i, icptr.i, idptr.i);
						VQ2TO4(iaptr.i, ibptr.i, icptr.i, idptr.i);
					}
				}
			}
			else if (cinTable[currentHandle].samplesPerPixel == 1)
			{
				bbptr = reinterpret_cast<byte*>(bptr);
				for (i = 0; i < two; i++)
				{
					y0 = static_cast<long>(*input++);
					y1 = static_cast<long>(*input++);
					y2 = static_cast<long>(*input++);
					y3 = static_cast<long>(*input);
					input += 3;
					*bbptr++ = cinTable[currentHandle].gray[y0];
					*bbptr++ = cinTable[currentHandle].gray[y1];
					*bbptr++ = cinTable[currentHandle].gray[(y0 * 3 + y2) / 4];
					*bbptr++ = cinTable[currentHandle].gray[(y1 * 3 + y3) / 4];
					*bbptr++ = cinTable[currentHandle].gray[(y0 + y2 * 3) / 4];
					*bbptr++ = cinTable[currentHandle].gray[(y1 + y3 * 3) / 4];
					*bbptr++ = cinTable[currentHandle].gray[y2];
					*bbptr++ = cinTable[currentHandle].gray[y3];
				}

				bcptr = reinterpret_cast<byte*>(vq4);
				bdptr = reinterpret_cast<byte*>(vq8);

				for (i = 0; i < four; i++)
				{
					baptr = reinterpret_cast<byte*>(vq2) + *input++ * 8;
					bbptr = reinterpret_cast<byte*>(vq2) + *input++ * 8;
					for (j = 0; j < 2; j++)
					{
						VQ2TO4(baptr, bbptr, bcptr, bdptr);
						VQ2TO4(baptr, bbptr, bcptr, bdptr);
					}
				}
			}
		}
	}
	else
	{
		//
		// 1/4 screen
		//
		if (cinTable[currentHandle].samplesPerPixel == 2)
		{
			for (i = 0; i < two; i++)
			{
				y0 = static_cast<long>(*input);
				input += 2;
				y2 = static_cast<long>(*input);
				input += 2;
				cr = static_cast<long>(*input++);
				cb = static_cast<long>(*input++);
				*bptr++ = yuv_to_rgb(y0, cr, cb);
				*bptr++ = yuv_to_rgb(y2, cr, cb);
			}

			cptr = static_cast<unsigned short*>(vq4);
			dptr = static_cast<unsigned short*>(vq8);

			for (i = 0; i < four; i++)
			{
				aptr = static_cast<unsigned short*>(vq2) + *input++ * 2;
				bptr = static_cast<unsigned short*>(vq2) + *input++ * 2;
				for (j = 0; j < 2; j++)
				{
					VQ2TO2(aptr, bptr, cptr, dptr);
				}
			}
		}
		else if (cinTable[currentHandle].samplesPerPixel == 1)
		{
			bbptr = reinterpret_cast<byte*>(bptr);

			for (i = 0; i < two; i++)
			{
				*bbptr++ = cinTable[currentHandle].gray[*input];
				input += 2;
				*bbptr++ = cinTable[currentHandle].gray[*input];
				input += 4;
			}

			bcptr = reinterpret_cast<byte*>(vq4);
			bdptr = reinterpret_cast<byte*>(vq8);

			for (i = 0; i < four; i++)
			{
				baptr = reinterpret_cast<byte*>(vq2) + *input++ * 2;
				bbptr = reinterpret_cast<byte*>(vq2) + *input++ * 2;
				for (j = 0; j < 2; j++)
				{
					VQ2TO2(baptr, bbptr, bcptr, bdptr);
				}
			}
		}
		else if (cinTable[currentHandle].samplesPerPixel == 4)
		{
			ibptr.s = bptr;
			for (i = 0; i < two; i++)
			{
				y0 = static_cast<long>(*input);
				input += 2;
				y2 = static_cast<long>(*input);
				input += 2;
				cr = static_cast<long>(*input++);
				cb = static_cast<long>(*input++);
				*ibptr.i++ = yuv_to_rgb24(y0, cr, cb);
				*ibptr.i++ = yuv_to_rgb24(y2, cr, cb);
			}

			icptr.s = vq4;
			idptr.s = vq8;

			for (i = 0; i < four; i++)
			{
				iaptr.s = vq2;
				iaptr.i += *input++ * 2;
				ibptr.s = vq2 + *input++ * 2;
				ibptr.i += *input++ * 2;
				for (j = 0; j < 2; j++)
				{
					VQ2TO2(iaptr.i, ibptr.i, icptr.i, idptr.i);
				}
			}
		}
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void recurseQuad(const long start_x, const long start_y, long quad_size, const long x_off, const long y_off)
{
	long lowy;

	const long offset = cinTable[currentHandle].screenDelta;

	const long lowx = lowy = 0;
	long bigx = cinTable[currentHandle].xsize;
	long bigy = cinTable[currentHandle].ysize;

	if (bigx > cinTable[currentHandle].CIN_WIDTH) bigx = cinTable[currentHandle].CIN_WIDTH;
	if (bigy > cinTable[currentHandle].CIN_HEIGHT) bigy = cinTable[currentHandle].CIN_HEIGHT;

	if (start_x >= lowx && start_x + quad_size <= bigx && start_y + quad_size <= bigy && start_y >= lowy && quad_size <=
		MAXSIZE)
	{
		const long useY = start_y;
		byte* scroff = cin.linbuf + (useY + ((cinTable[currentHandle].CIN_HEIGHT - bigy) >> 1) + y_off) * (cinTable[
			currentHandle].samplesPerLine) + (((start_x + x_off)) * cinTable[currentHandle].samplesPerPixel);

			cin.q_status[0][cinTable[currentHandle].onQuad] = scroff;
			cin.q_status[1][cinTable[currentHandle].onQuad++] = scroff + offset;
	}

	if (quad_size != MINSIZE)
	{
		quad_size >>= 1;
		recurseQuad(start_x, start_y, quad_size, x_off, y_off);
		recurseQuad(start_x + quad_size, start_y, quad_size, x_off, y_off);
		recurseQuad(start_x, start_y + quad_size, quad_size, x_off, y_off);
		recurseQuad(start_x + quad_size, start_y + quad_size, quad_size, x_off, y_off);
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void setupQuad(const long x_off, const long y_off)
{
	if (x_off == cin.old_x_off && y_off == cin.old_y_off && cinTable[currentHandle].ysize == static_cast<unsigned>(cin.
		oldysize) && cinTable[currentHandle].xsize == static_cast<unsigned>(cin.oldxsize))
	{
		return;
	}

	cin.old_x_off = x_off;
	cin.old_y_off = y_off;
	cin.oldysize = cinTable[currentHandle].ysize;
	cin.oldxsize = cinTable[currentHandle].xsize;
	/*	Enisform: Not in q3 source
		numQuadCels  = (cinTable[currentHandle].CIN_WIDTH*cinTable[currentHandle].CIN_HEIGHT) / (16);
		numQuadCels += numQuadCels/4 + numQuadCels/16;
		numQuadCels += 64;							  // for overflow
	*/

	long num_quad_cels = cinTable[currentHandle].xsize * cinTable[currentHandle].ysize / 16;
	num_quad_cels += num_quad_cels / 4;
	num_quad_cels += 64; // for overflow

	cinTable[currentHandle].onQuad = 0;

	for (long y = 0; y < static_cast<long>(cinTable[currentHandle].ysize); y += 16)
		for (long x = 0; x < static_cast<long>(cinTable[currentHandle].xsize); x += 16)
			recurseQuad(x, y, 16, x_off, y_off);

	for (long i = num_quad_cels - 64; i < num_quad_cels; i++)
	{
		byte* temp = nullptr;
		cin.q_status[0][i] = temp; // eoq
		cin.q_status[1][i] = temp; // eoq
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void readQuadInfo(const byte* q_data)
{
	if (currentHandle < 0) return;

	cinTable[currentHandle].xsize = q_data[0] + q_data[1] * 256;
	cinTable[currentHandle].ysize = q_data[2] + q_data[3] * 256;
	cinTable[currentHandle].maxsize = q_data[4] + q_data[5] * 256;
	cinTable[currentHandle].minsize = q_data[6] + q_data[7] * 256;

	cinTable[currentHandle].CIN_HEIGHT = cinTable[currentHandle].ysize;
	cinTable[currentHandle].CIN_WIDTH = cinTable[currentHandle].xsize;

	cinTable[currentHandle].samplesPerLine = cinTable[currentHandle].CIN_WIDTH * cinTable[currentHandle].
		samplesPerPixel;
	cinTable[currentHandle].screenDelta = cinTable[currentHandle].CIN_HEIGHT * cinTable[currentHandle].samplesPerLine;

	cinTable[currentHandle].half = qfalse;
	cinTable[currentHandle].smootheddouble = qfalse;

	cinTable[currentHandle].VQ0 = cinTable[currentHandle].VQNormal;
	cinTable[currentHandle].VQ1 = cinTable[currentHandle].VQBuffer;

	cinTable[currentHandle].t[0] = cinTable[currentHandle].screenDelta;
	cinTable[currentHandle].t[1] = -cinTable[currentHandle].screenDelta;

	cinTable[currentHandle].drawX = cinTable[currentHandle].CIN_WIDTH;
	cinTable[currentHandle].drawY = cinTable[currentHandle].CIN_HEIGHT;
	// jic the card sucks
	if (cls.glconfig.maxTextureSize <= 256)
	{
		if (cinTable[currentHandle].drawX > 256)
		{
			cinTable[currentHandle].drawX = 256;
		}
		if (cinTable[currentHandle].drawY > 256)
		{
			cinTable[currentHandle].drawY = 256;
		}
		if (cinTable[currentHandle].CIN_WIDTH != 256 || cinTable[currentHandle].CIN_HEIGHT != 256)
		{
			Com_Printf("HACK: approxmimating cinematic for Rage Pro or Voodoo\n");
		}
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void RoQPrepMcomp(const long xoff, const long yoff)
{
	long i = cinTable[currentHandle].samplesPerLine;
	long j = cinTable[currentHandle].samplesPerPixel;
	if (cinTable[currentHandle].xsize == cinTable[currentHandle].ysize * 4 && !cinTable[currentHandle].half)
	{
		j = j + j;
		i = i + i;
	}

	for (long y = 0; y < 16; y++)
	{
		const long temp2 = (y + yoff - 8) * i;
		for (long x = 0; x < 16; x++)
		{
			const long temp = (x + xoff - 8) * j;
			cin.mcomp[x * 16 + y] = cinTable[currentHandle].normalBuffer0 - (temp2 + temp);
		}
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void initRoQ()
{
	if (currentHandle < 0) return;

	cinTable[currentHandle].VQNormal = reinterpret_cast<void (*)(byte*, void*)>(blitVQQuad32fs);
	cinTable[currentHandle].VQBuffer = reinterpret_cast<void (*)(byte*, void*)>(blitVQQuad32fs);
	cinTable[currentHandle].samplesPerPixel = 4;
	ROQ_GenYUVTables();
	RllSetupTable();
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/
/*
static byte* RoQFetchInterlaced( byte *source ) {
	int x, *src, *dst;

	if (currentHandle < 0) return NULL;

	src = (int *)source;
	dst = (int *)cinTable[currentHandle].buf2;

	for(x=0;x<256*256;x++) {
		*dst = *src;
		dst++; src += 2;
	}
	return cinTable[currentHandle].buf2;
}
*/
static void RoQReset()
{
	if (currentHandle < 0) return;

	FS_FCloseFile(cinTable[currentHandle].iFile);
	FS_FOpenFileRead(cinTable[currentHandle].fileName, &cinTable[currentHandle].iFile, qtrue);
	// let the background thread start reading ahead
	FS_Read(cin.file, 16, cinTable[currentHandle].iFile);
	RoQ_init();
	cinTable[currentHandle].status = FMV_LOOPED;
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void RoQInterrupt()
{
	short sbuf[32768];
	int ssize;

	if (currentHandle < 0) return;

	FS_Read(cin.file, cinTable[currentHandle].RoQFrameSize + 8, cinTable[currentHandle].iFile);
	if (cinTable[currentHandle].RoQPlayed >= cinTable[currentHandle].ROQSize)
	{
		if (cinTable[currentHandle].holdAtEnd == qfalse)
		{
			if (cinTable[currentHandle].looping)
			{
				RoQReset();
			}
			else
			{
				cinTable[currentHandle].status = FMV_EOF;
			}
		}
		else
		{
			cinTable[currentHandle].status = FMV_IDLE;
		}
		return;
	}

	byte* framedata = cin.file;
	//
	// new frame is ready
	//
redump:
	switch (cinTable[currentHandle].roq_id)
	{
	case ROQ_QUAD_VQ:
		if (cinTable[currentHandle].numQuads & 1)
		{
			cinTable[currentHandle].normalBuffer0 = cinTable[currentHandle].t[1];
			RoQPrepMcomp(cinTable[currentHandle].roqF0, cinTable[currentHandle].roqF1);
			cinTable[currentHandle].VQ1(reinterpret_cast<byte*>(cin.q_status[1]), framedata);
			cinTable[currentHandle].buf = cin.linbuf + cinTable[currentHandle].screenDelta;
		}
		else
		{
			cinTable[currentHandle].normalBuffer0 = cinTable[currentHandle].t[0];
			RoQPrepMcomp(cinTable[currentHandle].roqF0, cinTable[currentHandle].roqF1);
			cinTable[currentHandle].VQ0(reinterpret_cast<byte*>(cin.q_status[0]), framedata);
			cinTable[currentHandle].buf = cin.linbuf;
		}
		if (cinTable[currentHandle].numQuads == 0)
		{
			// first frame
			Com_Memcpy(cin.linbuf + cinTable[currentHandle].screenDelta, cin.linbuf,
				cinTable[currentHandle].samplesPerLine * cinTable[currentHandle].ysize);
		}
		cinTable[currentHandle].numQuads++;
		cinTable[currentHandle].dirty = qtrue;
		break;
	case ROQ_CODEBOOK:
		decodeCodeBook(framedata, static_cast<unsigned short>(cinTable[currentHandle].roq_flags));
		break;
	case ZA_SOUND_MONO:
		if (!cinTable[currentHandle].silent)
		{
			ssize = RllDecodeMonoToStereo(framedata, sbuf, cinTable[currentHandle].RoQFrameSize, 0,
				static_cast<unsigned short>(cinTable[currentHandle].roq_flags));
			S_RawSamples(ssize, 22050, 2, 1, reinterpret_cast<byte*>(sbuf), s_volume->value, qtrue);
		}
		break;
	case ZA_SOUND_STEREO:
		if (!cinTable[currentHandle].silent)
		{
			if (cinTable[currentHandle].numQuads == -1)
			{
				S_Update();
				s_rawend = s_soundtime;
			}
			ssize = RllDecodeStereoToStereo(framedata, sbuf, cinTable[currentHandle].RoQFrameSize, 0,
				static_cast<unsigned short>(cinTable[currentHandle].roq_flags));
			S_RawSamples(ssize, 22050, 2, 2, reinterpret_cast<byte*>(sbuf), s_volume->value, qtrue);
		}
		break;
	case ROQ_QUAD_INFO:
		if (cinTable[currentHandle].numQuads == -1)
		{
			readQuadInfo(framedata);
			setupQuad(0, 0);
			cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = Sys_Milliseconds() * com_timescale->
				value;
		}
		if (cinTable[currentHandle].numQuads != 1) cinTable[currentHandle].numQuads = 0;
		break;
	case ROQ_PACKET:
		cinTable[currentHandle].inMemory = static_cast<qboolean>(cinTable[currentHandle].roq_flags);
		cinTable[currentHandle].RoQFrameSize = 0; // for header
		break;
	case ROQ_QUAD_HANG:
		cinTable[currentHandle].RoQFrameSize = 0;
		break;
	case ROQ_QUAD_JPEG:
		break;
	default:
		cinTable[currentHandle].status = FMV_EOF;
		break;
	}
	//
	// read in next frame data
	//
	if (cinTable[currentHandle].RoQPlayed >= cinTable[currentHandle].ROQSize)
	{
		if (cinTable[currentHandle].holdAtEnd == qfalse)
		{
			if (cinTable[currentHandle].looping)
			{
				RoQReset();
			}
			else
			{
				cinTable[currentHandle].status = FMV_EOF;
			}
		}
		else
		{
			cinTable[currentHandle].status = FMV_IDLE;
		}
		return;
	}

	framedata += cinTable[currentHandle].RoQFrameSize;
	cinTable[currentHandle].roq_id = framedata[0] + framedata[1] * 256;
	cinTable[currentHandle].RoQFrameSize = framedata[2] + framedata[3] * 256 + framedata[4] * 65536;
	cinTable[currentHandle].roq_flags = framedata[6] + framedata[7] * 256;
	cinTable[currentHandle].roqF0 = static_cast<signed char>(framedata[7]);
	cinTable[currentHandle].roqF1 = static_cast<signed char>(framedata[6]);

	if (cinTable[currentHandle].RoQFrameSize > 65536 || cinTable[currentHandle].roq_id == 0x1084)
	{
		Com_DPrintf("roq_size>65536||roq_id==0x1084\n");
		cinTable[currentHandle].status = FMV_EOF;
		if (cinTable[currentHandle].looping)
		{
			RoQReset();
		}
		return;
	}
	if (cinTable[currentHandle].inMemory && cinTable[currentHandle].status != FMV_EOF)
	{
		cinTable[currentHandle].inMemory = static_cast<qboolean>(static_cast<int>(cinTable[currentHandle].inMemory) -
			1);
		framedata += 8;
		goto redump;
	}
	//
	// one more frame hits the dust
	//
	//	assert(cinTable[currentHandle].RoQFrameSize <= 65536);
	//	r = FS_Read( cin.file, cinTable[currentHandle].RoQFrameSize+8, cinTable[currentHandle].iFile );
	cinTable[currentHandle].RoQPlayed += cinTable[currentHandle].RoQFrameSize + 8;
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void RoQ_init()
{
	cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = Sys_Milliseconds() * com_timescale->value;

	cinTable[currentHandle].RoQPlayed = 24;

	/*	get frame rate */
	cinTable[currentHandle].roqFPS = cin.file[6] + cin.file[7] * 256;

	if (!cinTable[currentHandle].roqFPS) cinTable[currentHandle].roqFPS = 30;

	cinTable[currentHandle].numQuads = -1;

	cinTable[currentHandle].roq_id = cin.file[8] + cin.file[9] * 256;
	cinTable[currentHandle].RoQFrameSize = cin.file[10] + cin.file[11] * 256 + cin.file[12] * 65536;
	cinTable[currentHandle].roq_flags = cin.file[14] + cin.file[15] * 256;

	if (cinTable[currentHandle].RoQFrameSize > 65536 || !cinTable[currentHandle].RoQFrameSize)
	{
		return;
	}

	if (cinTable[currentHandle].hSFX)
	{
		S_StartLocalSound(cinTable[currentHandle].hSFX, CHAN_AUTO);
	}
}

/******************************************************************************
*
* Function:
*
* Description:
*
******************************************************************************/

static void RoQShutdown()
{
	if (!cinTable[currentHandle].buf)
	{
		if (cinTable[currentHandle].iFile)
		{
			//			assert( 0 && "ROQ handle leak-prevention WAS needed!");
			FS_FCloseFile(cinTable[currentHandle].iFile);
			cinTable[currentHandle].iFile = 0;
			if (cinTable[currentHandle].hSFX)
			{
				S_CIN_StopSound(cinTable[currentHandle].hSFX);
			}
		}
		return;
	}

	if (cinTable[currentHandle].status == FMV_IDLE)
	{
		return;
	}

	Com_DPrintf("finished cinematic\n");
	cinTable[currentHandle].status = FMV_IDLE;

	if (cinTable[currentHandle].iFile)
	{
		FS_FCloseFile(cinTable[currentHandle].iFile);
		cinTable[currentHandle].iFile = 0;
		if (cinTable[currentHandle].hSFX)
		{
			S_CIN_StopSound(cinTable[currentHandle].hSFX);
		}
	}

	if (cinTable[currentHandle].alterGameState)
	{
		cls.state = CA_DISCONNECTED;
		// we can't just do a vstr nextmap, because
		// if we are aborting the intro cinematic with
		// a devmap command, nextmap would be valid by
		// the time it was referenced
		const char* s = Cvar_VariableString("nextmap");
		if (s[0])
		{
			Cbuf_ExecuteText(EXEC_APPEND, va("%s\n", s));
			Cvar_Set("nextmap", "");
		}
		CL_handle = -1;
	}
	cinTable[currentHandle].fileName[0] = 0;
	currentHandle = -1;
}

/*
==================
CIN_StopCinematic
==================
*/

e_status CIN_StopCinematic(const int handle)
{
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return FMV_EOF;
	currentHandle = handle;

	Com_DPrintf("trFMV::stop(), closing %s\n", cinTable[currentHandle].fileName);

	if (!cinTable[currentHandle].buf)
	{
		if (cinTable[currentHandle].iFile)
		{
			//			assert( 0 && "ROQ handle leak-prevention WAS needed!");
			FS_FCloseFile(cinTable[currentHandle].iFile);
			cinTable[currentHandle].iFile = 0;
			cinTable[currentHandle].fileName[0] = 0;
			if (cinTable[currentHandle].hSFX)
			{
				S_CIN_StopSound(cinTable[currentHandle].hSFX);
			}
		}
		return FMV_EOF;
	}

	if (cinTable[currentHandle].alterGameState)
	{
		if (cls.state != CA_CINEMATIC)
		{
			return cinTable[currentHandle].status;
		}
	}
	cinTable[currentHandle].status = FMV_EOF;
	RoQShutdown();

	return FMV_EOF;
}

/*
==================
SCR_RunCinematic

Fetch and decompress the pending frame
==================
*/

e_status CIN_RunCinematic(const int handle)
{
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return FMV_EOF;

	if (cin.current_handle != handle)
	{
		currentHandle = handle;
		cin.current_handle = currentHandle;
		cinTable[currentHandle].status = FMV_EOF;
		RoQReset();
	}

	if (cinTable[handle].playonwalls < -1)
	{
		return cinTable[handle].status;
	}

	currentHandle = handle;

	if (cinTable[currentHandle].alterGameState)
	{
		if (cls.state != CA_CINEMATIC)
		{
			return cinTable[currentHandle].status;
		}
	}

	if (cinTable[currentHandle].status == FMV_IDLE)
	{
		return cinTable[currentHandle].status;
	}

	const int this_time = Sys_Milliseconds() * com_timescale->value;
	if (cinTable[currentHandle].shader && abs(this_time - static_cast<double>(cinTable[currentHandle].lastTime)) > 100)
	{
		cinTable[currentHandle].startTime += this_time - cinTable[currentHandle].lastTime;
	}
	cinTable[currentHandle].tfps = (Sys_Milliseconds() * com_timescale->value - cinTable[currentHandle].startTime) *
		cinTable[currentHandle].roqFPS / 1000;

	int start = cinTable[currentHandle].startTime;
	while (cinTable[currentHandle].tfps != cinTable[currentHandle].numQuads
		&& cinTable[currentHandle].status == FMV_PLAY)
	{
		RoQInterrupt();
		if (static_cast<unsigned>(start) != cinTable[currentHandle].startTime)
		{
			cinTable[currentHandle].tfps = (Sys_Milliseconds() * com_timescale->value
				- cinTable[currentHandle].startTime) * cinTable[currentHandle].roqFPS / 1000;
			start = cinTable[currentHandle].startTime;
		}
	}

	cinTable[currentHandle].lastTime = this_time;

	if (cinTable[currentHandle].status == FMV_LOOPED)
	{
		cinTable[currentHandle].status = FMV_PLAY;
	}

	if (cinTable[currentHandle].status == FMV_EOF)
	{
		if (cinTable[currentHandle].looping)
		{
			RoQReset();
		}
		else
		{
			RoQShutdown();
		}
	}

	return cinTable[currentHandle].status;
}

void Menus_CloseAll();
void UI_Cursor_Show(qboolean flag);

/*
==================
CL_PlayCinematic

==================
*/
int CIN_PlayCinematic(const char* arg, const int x, const int y, const int w, const int h, const int systemBits, const char* ps_audio_file)
{
	char name[MAX_OSPATH];

	if (strstr(arg, "/") == nullptr && strstr(arg, "\\") == nullptr)
	{
		Com_sprintf(name, sizeof name, "video/%s", arg);
	}
	else
	{
		Com_sprintf(name, sizeof name, "%s", arg);
	}
	COM_DefaultExtension(name, sizeof name, ".roq");

	if (!(systemBits & CIN_system))
	{
		for (int i = 0; i < MAX_VIDEO_HANDLES; i++)
		{
			if (strcmp(cinTable[i].fileName, name) == 0)
			{
				return i;
			}
		}
	}

	Com_DPrintf("CIN_PlayCinematic( %s )\n", arg);

	memset(&cin, 0, sizeof(cinematics_t));
	currentHandle = CIN_HandleForVideo();

	cin.current_handle = currentHandle;

	Q_strncpyz(cinTable[currentHandle].fileName, name, MAX_OSPATH);

	cinTable[currentHandle].ROQSize = 0;
	cinTable[currentHandle].ROQSize = FS_FOpenFileRead(cinTable[currentHandle].fileName, &cinTable[currentHandle].iFile,
		qtrue);

	if (cinTable[currentHandle].ROQSize <= 0)
	{
		Com_Printf(S_COLOR_RED"ERROR: playCinematic: %s not found!\n", arg);
		cinTable[currentHandle].fileName[0] = 0;
		return -1;
	}

	CIN_SetExtents(currentHandle, x, y, w, h);
	CIN_SetLooping(currentHandle, static_cast<qboolean>((systemBits & CIN_loop) != 0));

	cinTable[currentHandle].CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
	cinTable[currentHandle].CIN_WIDTH = DEFAULT_CIN_WIDTH;
	cinTable[currentHandle].holdAtEnd = static_cast<qboolean>((systemBits & CIN_hold) != 0);
	cinTable[currentHandle].alterGameState = static_cast<qboolean>((systemBits & CIN_system) != 0);
	cinTable[currentHandle].playonwalls = 1;
	cinTable[currentHandle].silent = static_cast<qboolean>((systemBits & CIN_silent) != 0);
	cinTable[currentHandle].shader = static_cast<qboolean>((systemBits & CIN_shader) != 0);
	if (ps_audio_file)
	{
		cinTable[currentHandle].hSFX = S_RegisterSound(ps_audio_file);
	}
	else
	{
		cinTable[currentHandle].hSFX = 0;
	}
	cinTable[currentHandle].hCRAWLTEXT = 0;

	if (cinTable[currentHandle].alterGameState)
	{
		// close the menu
		Con_Close();
		if (cls.uiStarted)
		{
			UI_Cursor_Show(qfalse);
			Menus_CloseAll();
		}
	}
	else
	{
		cinTable[currentHandle].playonwalls = cl_inGameVideo->integer;
	}

	initRoQ();

	FS_Read(cin.file, 16, cinTable[currentHandle].iFile);

	const unsigned short ro_qid = static_cast<unsigned short>(cin.file[0]) + static_cast<unsigned short>(cin.file[1]) *
		256;
	if (ro_qid == 0x1084)
	{
		RoQ_init();
		//		FS_Read (cin.file, cinTable[currentHandle].RoQFrameSize+8, cinTable[currentHandle].iFile);

		cinTable[currentHandle].status = FMV_PLAY;
		Com_DPrintf("trFMV::play(), playing %s\n", arg);

		if (cinTable[currentHandle].alterGameState)
		{
			cls.state = CA_CINEMATIC;
		}

		Con_Close();

		if (!cinTable[currentHandle].silent)
			s_rawend = s_soundtime;

		return currentHandle;
	}
	Com_DPrintf("trFMV::play(), invalid RoQ ID\n");

	RoQShutdown();
	return -1;
}

void CIN_SetExtents(const int handle, const int x, const int y, const int w, const int h)
{
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;
	cinTable[handle].xpos = x;
	cinTable[handle].ypos = y;
	cinTable[handle].width = w;
	cinTable[handle].height = h;
	cinTable[handle].dirty = qtrue;
}

void CIN_SetLooping(const int handle, const qboolean loop)
{
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;
	cinTable[handle].looping = loop;
}

// Text crawl defines
constexpr auto TC_PLANE_WIDTH = 250;
constexpr auto TC_PLANE_NEAR = 90;
constexpr auto TC_PLANE_FAR = 715;
constexpr auto TC_PLANE_TOP = 0;
constexpr auto TC_PLANE_BOTTOM = 1100;

constexpr auto TC_DELAY = 9000;
constexpr auto TC_STOPTIME = 81000;

static void CIN_AddTextCrawl()
{
	refdef_t refdef;
	polyVert_t verts[4]{};

	// Set up refdef
	memset(&refdef, 0, sizeof refdef);

	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear(refdef.viewaxis);

	refdef.fov_x = 130;
	refdef.fov_y = 130;

	refdef.x = 0;
	refdef.y = -50;
	refdef.width = cls.glconfig.vidWidth;
	refdef.height = cls.glconfig.vidHeight * 2; // deliberately extend off the bottom of the screen

	// use to set shaderTime for scrolling shaders
	refdef.time = 0;

	// Set up the poly verts
	float fade_down = 1.0;
	if (cls.realtime - CL_iPlaybackStartTime >= TC_STOPTIME - 2500)
	{
		fade_down = (TC_STOPTIME - (cls.realtime - CL_iPlaybackStartTime)) / 2480.0f;
		if (fade_down < 0)
		{
			fade_down = 0;
		}
		if (fade_down > 1)
		{
			fade_down = 1;
		}
	}
	for (auto& vert : verts)
	{
		vert.modulate[0] = 255 * fade_down; // gold color?
		vert.modulate[1] = 235 * fade_down;
		vert.modulate[2] = 127 * fade_down;
		vert.modulate[3] = 255 * fade_down;
	}

	VectorScaleM(verts[2].modulate, 0.1f, verts[2].modulate); // darken at the top??
	VectorScaleM(verts[3].modulate, 0.1f, verts[3].modulate);

#define TIMEOFFSET  +(cls.realtime-CL_iPlaybackStartTime-TC_DELAY)*0.000015f -1
	VectorSet(verts[0].xyz, TC_PLANE_NEAR, -TC_PLANE_WIDTH, TC_PLANE_TOP);
	verts[0].st[0] = 1;
	verts[0].st[1] = 1 TIMEOFFSET;

	VectorSet(verts[1].xyz, TC_PLANE_NEAR, TC_PLANE_WIDTH, TC_PLANE_TOP);
	verts[1].st[0] = 0;
	verts[1].st[1] = 1 TIMEOFFSET;

	VectorSet(verts[2].xyz, TC_PLANE_FAR, TC_PLANE_WIDTH, TC_PLANE_BOTTOM);
	verts[2].st[0] = 0;
	verts[2].st[1] = 0 TIMEOFFSET;

	VectorSet(verts[3].xyz, TC_PLANE_FAR, -TC_PLANE_WIDTH, TC_PLANE_BOTTOM);
	verts[3].st[0] = 1;
	verts[3].st[1] = 0 TIMEOFFSET;

	// render it out
	re.ClearScene();
	re.AddPolyToScene(cinTable[CL_handle].hCRAWLTEXT, 4, verts, 1);
	re.RenderScene(&refdef);

	//time's up
	if (cls.realtime - CL_iPlaybackStartTime >= TC_STOPTIME)
	{
		//		cinTable[currentHandle].holdAtEnd = qfalse;
		cinTable[CL_handle].status = FMV_EOF;
		RoQShutdown();
		SCR_StopCinematic(); // change ROQ from FMV_IDLE to FMV_EOF, and clear some other vars
	}
}

/*
==================
CIN_ResampleCinematic

Resample cinematic to 256x256 and store in buf2
==================
*/
static void CIN_ResampleCinematic(const int handle, int* buf2)
{
	int ix, iy;

	byte* buf = cinTable[handle].buf;

	const int xm = cinTable[handle].CIN_WIDTH / 256;
	const int ym = cinTable[handle].CIN_HEIGHT / 256;
	int ll = 8;
	if (cinTable[handle].CIN_WIDTH == 512)
	{
		ll = 9;
	}

	const auto buf3 = reinterpret_cast<int*>(buf);
	if (xm == 2 && ym == 2)
	{
		auto bc2 = reinterpret_cast<byte*>(buf2);
		const byte* bc3 = reinterpret_cast<byte*>(buf3);
		for (iy = 0; iy < 256; iy++)
		{
			const int iiy = iy << 12;
			for (ix = 0; ix < 2048; ix += 8)
			{
				for (int ic = ix; ic < (ix + 4); ic++)
				{
					*bc2 = (bc3[iiy + ic] + bc3[iiy + 4 + ic] + bc3[iiy + 2048 + ic] + bc3[iiy + 2048 + 4 + ic]) >> 2;
					bc2++;
				}
			}
		}
	}
	else if (xm == 2 && ym == 1)
	{
		auto bc2 = reinterpret_cast<byte*>(buf2);
		const byte* bc3 = reinterpret_cast<byte*>(buf3);
		for (iy = 0; iy < 256; iy++)
		{
			const int iiy = iy << 11;
			for (ix = 0; ix < 2048; ix += 8)
			{
				for (int ic = ix; ic < (ix + 4); ic++)
				{
					*bc2 = (bc3[iiy + ic] + bc3[iiy + 4 + ic]) >> 1;
					bc2++;
				}
			}
		}
	}
	else
	{
		for (iy = 0; iy < 256; iy++)
		{
			for (ix = 0; ix < 256; ix++)
			{
				buf2[(iy << 8) + ix] = buf3[(iy * ym << ll) + ix * xm];
			}
		}
	}
}

/*
==================
CIN_DrawCinematic

==================
*/
void CIN_DrawCinematic(const int handle)
{
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;

	if (!cinTable[handle].buf)
	{
		return;
	}

	const float x = cinTable[handle].xpos;
	const float y = cinTable[handle].ypos;
	const float w = cinTable[handle].width;
	const float h = cinTable[handle].height;
	const byte* buf = cinTable[handle].buf;

	if (cinTable[handle].dirty && (cinTable[handle].CIN_WIDTH != cinTable[handle].drawX || cinTable[handle].CIN_HEIGHT
		!= cinTable[handle].drawY))
	{
		const auto buf2 = static_cast<int*>(Z_Malloc(256 * 256 * 4, TAG_TEMP_WORKSPACE, qfalse));

		CIN_ResampleCinematic(handle, buf2);

		re.DrawStretchRaw(x, y, w, h, 256, 256, reinterpret_cast<byte*>(buf2), handle, qtrue);
		cinTable[handle].dirty = qfalse;
		Z_Free(buf2); //Hunk_FreeTempMemory(buf2);
		return;
	}

	re.DrawStretchRaw(x, y, w, h, cinTable[handle].drawX, cinTable[handle].drawY, buf, handle, cinTable[handle].dirty);
	cinTable[handle].dirty = qfalse;
}

// external vars so I can check if the game is setup enough that I can play the intro video...
//
extern qboolean com_fullyInitialized;
extern qboolean s_soundStarted, s_soundMuted;
//
// ... and if the app isn't ready yet (which should only apply for the intro video), then I use these...
//
static char sPendingCinematic_Arg[256] = { 0 };
static char sPendingCinematic_s[256] = { 0 };
static qboolean gbPendingCinematic = qfalse;
//
// This stuff is for EF1-type ingame cinematics...
//
static qboolean qbPlayingInGameCinematic = qfalse;
static qboolean qbInGameCinematicOnStandBy = qfalse;
static char sInGameCinematicStandingBy[MAX_QPATH];
static char sTextCrawlFixedCinematic[MAX_QPATH];
static qboolean qbTextCrawlFixed = qfalse;
static int stopCinematicCallCount = 0;

static qboolean CIN_HardwareReadyToPlayVideos()
{
	if (com_fullyInitialized && cls.rendererStarted &&
		cls.soundStarted &&
		cls.soundRegistered
		)
	{
		return qtrue;
	}

	return qfalse;
}

static void PlayCinematic(const char* arg, const char* s, const qboolean qb_in_game)
{
	qboolean b_failed = qfalse;

	Cvar_Set("timescale", "1"); // jic we were skipping a scripted cinematic, return to normal after playing video
	Cvar_Set("skippingCinematic", "0"); // ""

	if (qbInGameCinematicOnStandBy == qfalse)
	{
		qbTextCrawlFixed = qfalse;
	}
	else
	{
		qbInGameCinematicOnStandBy = qfalse;
	}

	int bits = qb_in_game ? 0 : CIN_system;

	Com_DPrintf("CL_PlayCinematic_f\n");

	char s_temp[1024];
	if (strstr(arg, "/") == nullptr && strstr(arg, "\\") == nullptr)
	{
		Com_sprintf(s_temp, sizeof s_temp, "video/%s", arg);
	}
	else
	{
		Com_sprintf(s_temp, sizeof s_temp, "%s", arg);
	}
	COM_DefaultExtension(s_temp, sizeof s_temp, ".roq");
	arg = &s_temp[0];

	extern qboolean S_FileExists(const char* ps_filename);
	if (S_FileExists(arg))
	{
		SCR_StopCinematic();
		// command-line hack to avoid problems when playing intro video before app is fully setup...
		//
		if (!CIN_HardwareReadyToPlayVideos())
		{
			Q_strncpyz(sPendingCinematic_Arg, arg, 256);
			Q_strncpyz(sPendingCinematic_s, s && s[0] ? s : "", 256);
			gbPendingCinematic = qtrue;
			return;
		}

		qbPlayingInGameCinematic = qb_in_game;

		if (s && s[0] == '1' || Q_stricmp(arg, "video/end.roq") == 0)
		{
			bits |= CIN_hold;
		}
		if (s && s[0] == '2')
		{
			bits |= CIN_loop;
		}

		S_StopAllSounds();

		////////////////////////////////////////////////////////////////////
		//
		// work out associated audio-overlay file, if any...
		//
		extern cvar_t* s_language;
		const auto b_is_foreign = static_cast<qboolean>(s_language && Q_stricmp(s_language->string, "english") && Q_stricmp(s_language->string, ""));
		const char* ps_audio_file = nullptr;
		qhandle_t hCrawl = 0;
		if (!Q_stricmp(arg, "video/jk0101_sw.roq"))
		{
			ps_audio_file = "music/cinematic_1";

			if (com_outcast->integer == 1) //playing outcast
			{
				hCrawl = re.RegisterShaderNoMip("menu/video/tc_engl"); //outcast version of text crawl
			}
			else
			{
				hCrawl = re.RegisterShaderNoMip(va("menu/video/tc_%s", se_language->string));
				//academy version of text crawl
			}

			if (!hCrawl)
			{
				hCrawl = re.RegisterShaderNoMip("menu/video/tc_english"); //failed, so go back to english
			}
			bits |= CIN_hold;
		}
		else if (!Q_stricmp(arg, "video/openinglogos.roq"))
		{
			ps_audio_file = "music/mainMenu/main/OpeningLogosMusic";
			bits |= CIN_system;
		}
		else if (b_is_foreign)
		{
			if (!Q_stricmp(arg, "video/jk05.roq"))
			{
				ps_audio_file = "sound/chars/video/cinematic_5";
				bits |= CIN_silent; // knock out existing english track
			}
			else if (!Q_stricmp(arg, "video/jk06.roq"))
			{
				ps_audio_file = "sound/chars/video/cinematic_6";
				bits |= CIN_silent; // knock out existing english track
			}
		}

		float ratio = static_cast<float>(SCREEN_WIDTH * cls.glconfig.vidHeight) / static_cast<float>(SCREEN_HEIGHT * cls.glconfig.vidWidth);
		ratio = Com_Clamp(0.75f, 1.0f, ratio);
		const float new_height = SCREEN_HEIGHT / ratio;
		const float offset = (SCREEN_HEIGHT - SCREEN_HEIGHT / ratio) / 2.0f;

		CL_handle = CIN_PlayCinematic(arg, 0, offset, SCREEN_WIDTH, new_height, bits, ps_audio_file);
		if (CL_handle >= 0)
		{
			cinTable[CL_handle].hCRAWLTEXT = hCrawl;
			do
			{
				SCR_RunCinematic();
			} while (cinTable[currentHandle].buf == nullptr && cinTable[currentHandle].status == FMV_PLAY);
			// wait for first frame (load codebook and sound)

			if (qb_in_game)
			{
				Cvar_SetValue("cl_paused", 1);
				// remove-menu call will have unpaused us, so we sometimes need to re-pause
			}

			CL_iPlaybackStartTime = cls.realtime;
			// special use to avoid accidentally skipping ingame videos via fast-firing
		}
		else
		{
			// failed to open video...
			//
			b_failed = qtrue;
		}
	}
	else
	{
		// failed to open video...
		//
		b_failed = qtrue;
	}

	if (b_failed)
	{
		Com_Printf(S_COLOR_RED "PlayCinematic(): Failed to open \"%s\"\n", arg);
		//S_RestartMusic();	//restart the level music
		SCR_StopCinematic(); // I know this seems pointless, but it clears a bunch of vars as well
	}
	else
	{
		// this doesn't work for now...
		//
		//		if (cls.state == CA_ACTIVE){
		//			re.InitDissolve(qfalse);	// so we get a dissolve between previous screen image and cinematic
		//		}
	}
}

qboolean CL_CheckPendingCinematic()
{
	if (gbPendingCinematic && CIN_HardwareReadyToPlayVideos())
	{
		gbPendingCinematic = qfalse; // BEFORE next line, or we get recursion
		PlayCinematic(sPendingCinematic_Arg, sPendingCinematic_s[0] ? sPendingCinematic_s : nullptr, qfalse);
		return qtrue;
	}
	return qfalse;
}

/*
==================
CL_CompleteCinematic
==================
*/
void CL_CompleteCinematic(char* args, const int arg_num)
{
	if (arg_num == 2)
		Field_CompleteFilename("video", "roq", qtrue, qfalse);
}

void CL_PlayCinematic_f()
{
	const char* arg = Cmd_Argv(1);
	const char* s = Cmd_Argv(2);
	PlayCinematic(arg, s, qfalse);
}

void CL_PlayInGameCinematic_f()
{
	const char* arg = Cmd_Argv(1);
	if (cls.state == CA_ACTIVE)
	{
		PlayCinematic(arg, nullptr, qtrue);
	}
	else if (!qbInGameCinematicOnStandBy)
	{
		Q_strncpyz(sInGameCinematicStandingBy, arg, MAX_QPATH);
		qbInGameCinematicOnStandBy = qtrue;
	}
	else
	{
		// hack in order to fix text crawl
		Q_strncpyz(sTextCrawlFixedCinematic, arg, MAX_QPATH);
		qbTextCrawlFixed = qtrue;
	}
}

// Externally-called only, and only if cls.state == CA_CINEMATIC (or CL_IsRunningInGameCinematic() == true now)
//
void SCR_DrawCinematic()
{
	if (CL_InGameCinematicOnStandBy())
	{
		PlayCinematic(sInGameCinematicStandingBy, nullptr, qtrue);
	}
	else if (qbTextCrawlFixed && stopCinematicCallCount > 1)
	{
		PlayCinematic(sTextCrawlFixedCinematic, nullptr, qtrue);
	}

	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES)
	{
		CIN_DrawCinematic(CL_handle);
		if (cinTable[CL_handle].hCRAWLTEXT && cls.realtime - CL_iPlaybackStartTime >= TC_DELAY)
		{
			CIN_AddTextCrawl();
		}
	}
}

void SCR_RunCinematic()
{
	CL_CheckPendingCinematic();

	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES)
	{
		const e_status Status = CIN_RunCinematic(CL_handle);

		if (CL_IsRunningInGameCinematic() && Status == FMV_IDLE && !cinTable[CL_handle].holdAtEnd)
		{
			SCR_StopCinematic(); // change ROQ from FMV_IDLE to FMV_EOF, and clear some other vars
		}
	}
}

void SCR_StopCinematic(const qboolean b_allow_refusal /* = qfalse */)
{
	if (b_allow_refusal)
	{
		if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES
			&&
			cls.realtime < CL_iPlaybackStartTime + 1200 // 1.2 seconds have to have elapsed
			)
		{
			return;
		}
	}

	if (CL_IsRunningInGameCinematic())
	{
		Com_DPrintf("In-game Cinematic Stopped\n");
	}

	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES &&
		stopCinematicCallCount != 1)
	{
		// hello no, don't want this plz
		CIN_StopCinematic(CL_handle);
		S_StopAllSounds();
		CL_handle = -1;
		if (CL_IsRunningInGameCinematic())
		{
			re.InitDissolve(qfalse); // dissolve from cinematic to underlying ingame
		}
	}

	if (cls.state == CA_CINEMATIC)
	{
		Com_DPrintf("Cinematic Stopped\n");
		cls.state = CA_DISCONNECTED;
	}

	if (sInGameCinematicStandingBy[0] &&
		qbTextCrawlFixed)
	{
		// Hacky fix to help deal with broken text crawl..
		// If we are skipping past the one on standby, DO NOT SKIP THE OTHER ONES!
		stopCinematicCallCount++;
	}
	else if (stopCinematicCallCount == 1)
	{
		stopCinematicCallCount++;
	}
	else
	{
		// Skipping the last one in the list, go ahead and kill it.
		qbTextCrawlFixed = qfalse;
		sTextCrawlFixedCinematic[0] = 0;
		stopCinematicCallCount = 0;
	}

	if (stopCinematicCallCount != 2)
	{
		qbPlayingInGameCinematic = qfalse;
		qbInGameCinematicOnStandBy = qfalse;
		sInGameCinematicStandingBy[0] = 0;
		Cvar_SetValue("cl_paused", 0);
	}
	if (cls.state != CA_DISCONNECTED) // cut down on needless calls to music code
	{
		S_RestartMusic(); //restart the level music
	}
}

void CIN_UploadCinematic(const int handle)
{
	if (handle >= 0 && handle < MAX_VIDEO_HANDLES)
	{
		if (!cinTable[handle].buf)
		{
			return;
		}
		if (cinTable[handle].playonwalls <= 0 && cinTable[handle].dirty)
		{
			if (cinTable[handle].playonwalls == 0)
			{
				cinTable[handle].playonwalls = -1;
			}
			else
			{
				if (cinTable[handle].playonwalls == -1)
				{
					cinTable[handle].playonwalls = -2;
				}
				else
				{
					cinTable[handle].dirty = qfalse;
				}
			}
		}

		// Resample the video if needed
		if (cinTable[handle].dirty && (cinTable[handle].CIN_WIDTH != cinTable[handle].drawX || cinTable[handle].
			CIN_HEIGHT != cinTable[handle].drawY))
		{
			const auto buf2 = static_cast<int*>(Z_Malloc(256 * 256 * 4, TAG_TEMP_WORKSPACE, qfalse));

			CIN_ResampleCinematic(handle, buf2);

			re.UploadCinematic(256, 256, reinterpret_cast<byte*>(buf2), handle, qtrue);
			cinTable[handle].dirty = qfalse;
			Z_Free(buf2);
		}
		else
		{
			// Upload video at normal resolution
			re.UploadCinematic(cinTable[handle].drawX, cinTable[handle].drawY,
				cinTable[handle].buf, handle, cinTable[handle].dirty);
			cinTable[handle].dirty = qfalse;
		}

		if (cl_inGameVideo->integer == 0 && cinTable[handle].playonwalls == 1)
		{
			cinTable[handle].playonwalls--;
		}
		else if (cl_inGameVideo->integer != 0 && cinTable[handle].playonwalls != 1)
		{
			cinTable[handle].playonwalls = 1;
		}
	}
}

qboolean CL_IsRunningInGameCinematic()
{
	return qbPlayingInGameCinematic;
}

qboolean CL_InGameCinematicOnStandBy()
{
	return qbInGameCinematicOnStandBy;
}