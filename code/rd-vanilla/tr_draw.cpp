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

// tr_draw.c
#include "../server/exe_headers.h"
#include "tr_common.h"
#include "tr_local.h"

/*
=============
RE_StretchRaw

Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/

// param 'bDirty' should be true 99% of the time
void RE_StretchRaw(const int x, const int y, const int w, const int h, int cols, int rows, const byte* data, const int client, const qboolean dirty)
{
	if (!tr.registered) {
		return;
	}

	R_IssuePendingRenderCommands();

	if (tess.numIndexes) {
		RB_EndSurface();
	}

	// we definately want to sync every frame for the cinematics
	qglFinish();

#ifdef TIMEBIND
	int start, end;
	start = end = 0;	// only to stop compiler whining, don't need to be initialised
#endif
	// make sure rows and cols are powers of 2
	if (cols & cols - 1 || rows & rows - 1)
	{
		Com_Error(ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture...
	//
	if (cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height)
	{
		tr.scratchImage[client]->width = cols;
		tr.scratchImage[client]->height = rows;
#ifdef TIMEBIND
		if (r_ignore->integer)
		{
			start = ri.Milliseconds();
		}
#endif

		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glConfig.clampToEdgeAvailable ? GL_CLAMP_TO_EDGE : GL_CLAMP);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glConfig.clampToEdgeAvailable ? GL_CLAMP_TO_EDGE : GL_CLAMP);

#ifdef TIMEBIND
		if (r_ignore->integer)
		{
			end = ri.Milliseconds();
			ri.Printf(PRINT_ALL, "qglTexImage2D %i, %i: %i msec\n", cols, rows, end - start);
		}
#endif
	}
	else
	{
		if (dirty)	// FIXME: some TA addition or other, not sure why, yet. Should probably be true 99% of the time?
		{
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression

#ifdef TIMEBIND
			if (r_ignore->integer)
			{
				start = ri.Milliseconds();
			}
#endif

			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);

#ifdef TIMEBIND
			if (r_ignore->integer)
			{
				end = ri.Milliseconds();
				ri.Printf(PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start);
			}
#endif
		}
	}

	extern void	RB_SetGL2D(void);
	if (!backEnd.projection2D)
	{
		RB_SetGL2D();
	}
	qglColor3f(tr.identityLight, tr.identityLight, tr.identityLight);

	qglBegin(GL_QUADS);
	qglTexCoord2f(0.5f / cols, 0.5f / rows);
	qglVertex2f(x, y);
	qglTexCoord2f((cols - 0.5f) / cols, 0.5f / rows);
	qglVertex2f(x + w, y);
	qglTexCoord2f((cols - 0.5f) / cols, (rows - 0.5f) / rows);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(0.5f / cols, (rows - 0.5f) / rows);
	qglVertex2f(x, y + h);
	qglEnd();
}

void RE_UploadCinematic(const int cols, const int rows, const byte* data, const int client, const qboolean dirty)
{
	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if (cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height) {
		tr.scratchImage[client]->width = cols;
		tr.scratchImage[client]->height = rows;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glConfig.clampToEdgeAvailable ? GL_CLAMP_TO_EDGE : GL_CLAMP);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glConfig.clampToEdgeAvailable ? GL_CLAMP_TO_EDGE : GL_CLAMP);
	}
	else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}
}

extern byte* RB_ReadPixels(int x, int y, int width, int height, size_t* offset, int* padlen);
void RE_GetScreenShot(byte* buffer, const int w, const int h)
{
	size_t offset = 0;
	int padlen;

	int g, b;

	byte* source = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
	const size_t memcount = (glConfig.vidWidth * 3 + padlen) * glConfig.vidHeight;

	// gamma correct
	if (glConfig.deviceSupportsGamma)
		R_GammaCorrect(source + offset, memcount);

	// resample from source
	const float x_scale = glConfig.vidWidth / (4.0 * w);
	const float y_scale = glConfig.vidHeight / (3.0 * h);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int r = g = b = 0;
			for (int yy = 0; yy < 3; yy++) {
				for (int xx = 0; xx < 4; xx++) {
					const byte* src = source + offset + 3 * (glConfig.vidWidth * static_cast<int>((y * 3 + yy) * y_scale) + static_cast<int>((x * 4 + xx) * x_scale));
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			byte* dst = buffer + 4 * ((h - y - 1) * w + x);
			dst[0] = r / 12;
			dst[1] = g / 12;
			dst[2] = b / 12;
			dst[3] = 255;
		}
	}

	R_Free(source);
}

// this is just a chunk of code from RE_TempRawImage_ReadFromFile() below, subroutinised so I can call it
//	from the screen dissolve code as well...
//
static byte* RE_ReSample(byte* pbLoadedPic, const int iLoadedWidth, const int iLoadedHeight, byte* pb_re_sample_buffer, int* piWidth, int* piHeight)
{
	byte* pb_return;

	// if not resampling, just return some values and return...
	//
	if (pb_re_sample_buffer == nullptr || iLoadedWidth == *piWidth && iLoadedHeight == *piHeight)
	{
		// if not resampling, we're done, just return the loaded size...
		//
		*piWidth = iLoadedWidth;
		*piHeight = iLoadedHeight;
		pb_return = pbLoadedPic;
	}
	else
	{
		// resample from pbLoadedPic to pbReSampledBuffer...
		//
		const float	f_x_step = static_cast<float>(iLoadedWidth) / static_cast<float>(*piWidth);
		const float	f_y_step = static_cast<float>(iLoadedHeight) / static_cast<float>(*piHeight);
		const int		i_tot_pixels_per_down_sample = static_cast<int>(ceil(f_x_step)) * static_cast<int>(ceil(f_y_step));

		int g, b;

		byte* pb_dst = pb_re_sample_buffer;

		for (int y = 0; y < *piHeight; y++)
		{
			for (int x = 0; x < *piWidth; x++)
			{
				int r = g = b = 0;

				for (float yy = static_cast<float>(y) * f_y_step; yy < static_cast<float>(y + 1) * f_y_step; yy += 1)
				{
					for (float xx = static_cast<float>(x) * f_x_step; xx < static_cast<float>(x + 1) * f_x_step; xx += 1)
					{
						const byte* pbSrc = pbLoadedPic + 4 * (static_cast<int>(yy) * iLoadedWidth + static_cast<int>(xx));

						assert(pbSrc < pbLoadedPic + iLoadedWidth * iLoadedHeight * 4);

						r += pbSrc[0];
						g += pbSrc[1];
						b += pbSrc[2];
					}
				}

				assert(pb_dst < pb_re_sample_buffer + *piWidth * *piHeight * 4);

				pb_dst[0] = r / i_tot_pixels_per_down_sample;
				pb_dst[1] = g / i_tot_pixels_per_down_sample;
				pb_dst[2] = b / i_tot_pixels_per_down_sample;
				pb_dst[3] = 255;
				pb_dst += 4;
			}
		}

		// set return value...
		//
		pb_return = pb_re_sample_buffer;
	}

	return pb_return;
}

// this is so the server (or anyone else) can get access to raw pixels if they really need to,
//	currently it's only used by the server so that savegames can embed a graphic in the auto-save files
//	(which can't do a screenshot since they're saved out before the level is drawn).
//
// by default, the pic will be returned as the original dims, but if pbReSampleBuffer != NULL then it's assumed to
//	be a big enough buffer to hold the resampled image, which also means that the width and height params are read as
//	inputs (as well as still being inherently outputs) and the pic is scaled to that size, and to that buffer.
//
// the return value is either NULL, or a pointer to the pixels to use (which may be either the pbReSampleBuffer param,
//	or the local ptr below).
//
// In either case, you MUST call the free-up function afterwards ( RE_TempRawImage_CleanUp() ) to get rid of any temp
//	memory after you've finished with the pic.
//
// Note: ALWAYS use the return value if != NULL, even if you passed in a declared resample buffer. This is because the
//	resample will get skipped if the values you want are the same size as the pic that it loaded, so it'll return a
//	different buffer.
//
// the vertflip param is used for those functions that expect things in OpenGL's upside-down pixel-read format (sigh)
//
// (not brilliantly fast, but it's only used for weird stuff anyway)
//
byte* pbLoadedPic = nullptr;

byte* RE_TempRawImage_ReadFromFile(const char* psLocalFilename, int* piWidth, int* piHeight, byte* pb_re_sample_buffer, const qboolean qbVertFlip)
{
	RE_TempRawImage_CleanUp();	// jic

	byte* pb_return = nullptr;

	if (psLocalFilename && piWidth && piHeight)
	{
		int	 iLoadedWidth, iLoadedHeight;

		R_LoadImage(psLocalFilename, &pbLoadedPic, &iLoadedWidth, &iLoadedHeight);
		if (pbLoadedPic)
		{
			pb_return = RE_ReSample(pbLoadedPic, iLoadedWidth, iLoadedHeight,
				pb_re_sample_buffer, piWidth, piHeight);
		}
	}

	if (pb_return && qbVertFlip)
	{
		auto p_src_line = reinterpret_cast<unsigned*>(pb_return);
		unsigned int* p_dst_line = reinterpret_cast<unsigned*>(pb_return) + *piHeight * *piWidth;	// *4 done by compiler (longs)
		p_dst_line -= *piWidth;	// point at start of last line, not first after buffer

		for (int i_line_count = 0; i_line_count < *piHeight / 2; i_line_count++)
		{
			for (int x = 0; x < *piWidth; x++)
			{
				const unsigned int l = p_src_line[x];
				p_src_line[x] = p_dst_line[x];
				p_dst_line[x] = l;
			}
			p_src_line += *piWidth;
			p_dst_line -= *piWidth;
		}
	}

	return pb_return;
}

void RE_TempRawImage_CleanUp(void)
{
	if (pbLoadedPic)
	{
		R_Free(pbLoadedPic);
		pbLoadedPic = nullptr;
	}
}

using Dissolve_e = enum
{
	eDISSOLVE_RT_TO_LT = 0,
	eDISSOLVE_LT_TO_RT,
	eDISSOLVE_TP_TO_BT,
	eDISSOLVE_BT_TO_TP,
	eDISSOLVE_CIRCULAR_OUT,	// new image comes out from centre
	//
	eDISSOLVE_RAND_LIMIT,	// label only, not valid to select
	//
	// any others...
	//
	eDISSOLVE_CIRCULAR_IN,	// new image comes in from edges
	//
	eDISSOLVE_NUMBEROF
};

using Dissolve_t = struct
{
	int			iWidth;
	int			iHeight;
	int			iUploadWidth;
	int			iUploadHeight;
	int			iScratchPadNumber;
	image_t* pImage;	// old image screen
	image_t* pDissolve;	// fuzzy thing
	image_t* pBlack;	// small black image for clearing
	int			iStartTime;	// 0 = not processing
	Dissolve_e	eDissolveType;
	qboolean	bTouchNeeded;
};

static int PowerOf2(int i_arg)
{
	if ((i_arg & i_arg - 1) != 0)
	{
		int i_shift = 0;
		while (i_arg)
		{
			i_arg >>= 1;
			i_shift++;
		}

		i_arg = 1 << i_shift;
	}

	return i_arg;
}

Dissolve_t Dissolve = { 0 };
#define fDISSOLVE_SECONDS 0.75f

// leave the UV stuff in for now as comments in case I ever need to do some sneaky stuff, but for now...
//
static void RE_Blit(const float fX0, const float fY0, const float fX1, const float fY1, const float fX2, const float fY2, const float fX3, const float fY3, image_t* pImage, const int iGLState)
{
	//
	// some junk they had at the top of other StretchRaw code...
	//
	R_IssuePendingRenderCommands();
	//	qglFinish();

	GL_Bind(pImage);
	GL_State(iGLState);
	GL_Cull(CT_TWO_SIDED);

	qglColor3f(1.0f, 1.0f, 1.0f);

	qglBegin(GL_QUADS);
	{
		// TL...
		//
//		qglTexCoord2f( fU0 / (float)pImage->width,  fV0 / (float)pImage->height );
		qglTexCoord2f(0, 0);
		qglVertex2f(fX0, fY0);

		// TR...
		//
//		qglTexCoord2f( fU1 / (float)pImage->width,  fV1 / (float)pImage->height );
		qglTexCoord2f(1, 0);
		qglVertex2f(fX1, fY1);

		// BR...
		//
//		qglTexCoord2f( fU2 / (float)pImage->width,  fV2 / (float)pImage->height );
		qglTexCoord2f(1, 1);
		qglVertex2f(fX2, fY2);

		// BL...
		//
//		qglTexCoord2f( fU3 / (float)pImage->width,  fV3 / (float)pImage->height );
		qglTexCoord2f(0, 1);
		qglVertex2f(fX3, fY3);
	}
	qglEnd();
}

static void RE_KillDissolve(void)
{
	Dissolve.iStartTime = 0;

	if (Dissolve.pImage)
	{
		R_Images_DeleteImage(Dissolve.pImage);
		Dissolve.pImage = nullptr;
	}
}
// Draw the dissolve pic to the screen, over the top of what's already been rendered.
//
// return = qtrue while still processing, for those interested...
//
#define iSAFETY_SPRITE_OVERLAP 2	// #pixels to overlap blit region by, in case some drivers leave onscreen seams
qboolean RE_ProcessDissolve(void)
{
	if (Dissolve.iStartTime)
	{
		if (Dissolve.bTouchNeeded)
		{
			// Stuff to avoid music stutter...
			//
			//	The problem is, that if I call RE_InitDissolve() then call RestartMusic, then by the time the music
			//	has loaded in if it took longer than one second the dissolve would think that it had finished,
			//	even if it had never actually drawn up. However, if I called RE_InitDissolve() AFTER the music had
			//	restarted, then the music would stutter on slow video cards or CPUs while I did the binding/resampling.
			//
			// This way, I restart the millisecond counter the first time we actually get as far as rendering, which
			//	should let things work properly...
			//
			Dissolve.bTouchNeeded = qfalse;
			Dissolve.iStartTime = ri.Milliseconds();
		}

		int i_dissolve_percentage = (ri.Milliseconds() - Dissolve.iStartTime) * 100 / (1000.0f * fDISSOLVE_SECONDS);

		//		ri.Printf(PRINT_ALL,"iDissolvePercentage %d\n",iDissolvePercentage);

		if (i_dissolve_percentage <= 100)
		{
			extern void	RB_SetGL2D(void);
			RB_SetGL2D();

			//			GLdouble glD;
			//			qglGetDoublev(GL_DEPTH_CLEAR_VALUE,&glD);
			//			qglClearColor(0,0,0,1);
			qglClearDepth(1.0f);
			qglClear(GL_DEPTH_BUFFER_BIT);

			float f_x_scale_factor = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(Dissolve.iWidth);
			float f_y_scale_factor = static_cast<float>(SCREEN_HEIGHT) / static_cast<float>(Dissolve.iHeight);
			float x0, y0, x1, y1, x2, y2, x3, y3;

			switch (Dissolve.eDissolveType)
			{
			case eDISSOLVE_RT_TO_LT:
			{
				float f_xboundary = static_cast<float>(Dissolve.iWidth) - static_cast<float>(Dissolve.iWidth + Dissolve.pDissolve->width) * static_cast<float>(i_dissolve_percentage) / 100.0f;

				// blit the fuzzy-dissolve sprite...
				//
				x0 = f_x_scale_factor * f_xboundary;
				y0 = 0.0f;
				x1 = f_x_scale_factor * (f_xboundary + Dissolve.pDissolve->width);
				y1 = 0.0f;
				x2 = x1;
				y2 = f_y_scale_factor * Dissolve.iHeight;
				x3 = x0;
				y3 = y2;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pDissolve, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE | GLS_ATEST_LT_80);

				// blit a blank thing over the area the old screen is to be displayed on to enable screen-writing...
				// (to the left of fXboundary)
				//
				x0 = 0.0f;
				y0 = 0.0f;
				x1 = f_x_scale_factor * (f_xboundary + iSAFETY_SPRITE_OVERLAP);
				y1 = 0.0f;
				x2 = x1;
				y2 = f_y_scale_factor * Dissolve.iHeight;
				x3 = x0;
				y3 = y2;
				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE);
			}
			break;

			case eDISSOLVE_LT_TO_RT:
			{
				float f_xboundary = static_cast<float>(Dissolve.iWidth + 2 * Dissolve.pDissolve->width) * static_cast<float>(i_dissolve_percentage) / 100.0f - Dissolve.pDissolve->width;

				// blit the fuzzy-dissolve sprite...
				//
				x0 = f_x_scale_factor * (f_xboundary + Dissolve.pDissolve->width);
				y0 = 0.0f;
				x1 = f_x_scale_factor * f_xboundary;
				y1 = 0.0f;
				x2 = x1;
				y2 = f_y_scale_factor * Dissolve.iHeight;
				x3 = x0;
				y3 = y2;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pDissolve, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE | GLS_ATEST_LT_80);

				// blit a blank thing over the area the old screen is to be displayed on to enable screen-writing...
				// (to the right of fXboundary)
				//
				x0 = f_x_scale_factor * (f_xboundary + Dissolve.pDissolve->width - iSAFETY_SPRITE_OVERLAP);
				y0 = 0.0f;
				x1 = f_x_scale_factor * Dissolve.iWidth;
				y0 = 0.0f;
				x2 = x1;
				y2 = f_y_scale_factor * Dissolve.iHeight;
				x3 = x0;
				y3 = y2;
				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE);
			}
			break;

			case eDISSOLVE_TP_TO_BT:
			{
				float f_yboundary = static_cast<float>(Dissolve.iHeight + 2 * Dissolve.pDissolve->width) * static_cast<float>(i_dissolve_percentage) / 100.0f - Dissolve.pDissolve->width;

				// blit the fuzzy-dissolve sprite...
				//
				x0 = 0.0f;
				y0 = f_y_scale_factor * (f_yboundary + Dissolve.pDissolve->width);
				x1 = x0;
				y1 = f_y_scale_factor * f_yboundary;
				x2 = f_x_scale_factor * Dissolve.iWidth;
				y2 = y1;
				x3 = x2;
				y3 = y0;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pDissolve, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE | GLS_ATEST_LT_80);

				// blit a blank thing over the area the old screen is to be displayed on to enable screen-writing...
				// (underneath fYboundary)
				//
				x0 = 0.0f;
				y0 = f_y_scale_factor * (f_yboundary + Dissolve.pDissolve->width - iSAFETY_SPRITE_OVERLAP);
				x1 = f_x_scale_factor * Dissolve.iWidth;
				y1 = y0;
				x2 = x1;
				y2 = f_y_scale_factor * Dissolve.iHeight;
				x3 = x0;
				y3 = y2;
				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE);
			}
			break;

			case eDISSOLVE_BT_TO_TP:
			{
				float f_yboundary = Dissolve.iHeight - static_cast<float>(Dissolve.iHeight + Dissolve.pDissolve->width) * static_cast<float>(i_dissolve_percentage) / 100.0f;

				// blit the fuzzy-dissolve sprite...
				//
				x0 = 0.0f;
				y0 = f_y_scale_factor * f_yboundary;
				x1 = x0;
				y1 = f_y_scale_factor * (f_yboundary + Dissolve.pDissolve->width);
				x2 = f_x_scale_factor * Dissolve.iWidth;
				y2 = y1;
				x3 = x2;
				y3 = y0;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pDissolve, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE | GLS_ATEST_LT_80);

				// blit a blank thing over the area the old screen is to be displayed on to enable screen-writing...
				// (above fYboundary)
				//
				x0 = 0.0f;
				y0 = 0.0f;
				x1 = f_x_scale_factor * Dissolve.iWidth;
				y1 = y0;
				x2 = x1;
				y2 = f_y_scale_factor * (f_yboundary + iSAFETY_SPRITE_OVERLAP);
				x3 = x0;
				y3 = y2;
				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE);
			}
			break;

			case eDISSOLVE_CIRCULAR_IN:
			{
				float f_diag_zoom = static_cast<float>(Dissolve.iWidth) * 0.8 * (100 - i_dissolve_percentage) / 100.0f;

				//
				// blit circular graphic...
				//
				x0 = f_x_scale_factor * (Dissolve.iWidth / static_cast<float>(2) - f_diag_zoom);
				y0 = f_y_scale_factor * (Dissolve.iHeight / static_cast<float>(2) - f_diag_zoom);
				x1 = f_x_scale_factor * (Dissolve.iWidth / static_cast<float>(2) + f_diag_zoom);
				y1 = y0;
				x2 = x1;
				y2 = f_y_scale_factor * (Dissolve.iHeight / static_cast<float>(2) + f_diag_zoom);
				x3 = x0;
				y3 = y2;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pDissolve, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE | GLS_ATEST_LT_80);
			}
			break;

			case eDISSOLVE_CIRCULAR_OUT:
			{
				float f_diag_zoom = static_cast<float>(Dissolve.iWidth) * 0.8 * i_dissolve_percentage / 100.0f;

				//
				// blit circular graphic...
				//
				x0 = f_x_scale_factor * (Dissolve.iWidth / static_cast<float>(2) - f_diag_zoom);
				y0 = f_y_scale_factor * (Dissolve.iHeight / static_cast<float>(2) - f_diag_zoom);
				x1 = f_x_scale_factor * (Dissolve.iWidth / static_cast<float>(2) + f_diag_zoom);
				y1 = y0;
				x2 = x1;
				y2 = f_y_scale_factor * (Dissolve.iHeight / static_cast<float>(2) + f_diag_zoom);
				x3 = x0;
				y3 = y2;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pDissolve, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE | GLS_ATEST_LT_80);
				// now blit the 4 black squares around it to mask off the rest of the screen...
				//
				// LHS, top to bottom...
				//
				RE_Blit(0, 0,								// x0,y0
					x0 + iSAFETY_SPRITE_OVERLAP, 0,		// x1,y1
					x0 + iSAFETY_SPRITE_OVERLAP, f_y_scale_factor * Dissolve.iHeight,// x2,y2
					0, f_y_scale_factor * Dissolve.iHeight,	// x3,y3,
					Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE
				);

				// RHS top to bottom...
				//
				RE_Blit(x1 - iSAFETY_SPRITE_OVERLAP, 0,		// x0,y0
					f_x_scale_factor * Dissolve.iWidth, 0,	// x1,y1
					f_x_scale_factor * Dissolve.iWidth, f_y_scale_factor * Dissolve.iHeight,// x2,y2
					x1 - iSAFETY_SPRITE_OVERLAP, f_y_scale_factor * Dissolve.iHeight,	// x3,y3,
					Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE
				);

				// top...
				//
				RE_Blit(x0 - iSAFETY_SPRITE_OVERLAP, 0,		// x0,y0
					x1 + iSAFETY_SPRITE_OVERLAP, 0,		// x1,y1
					x1 + iSAFETY_SPRITE_OVERLAP, y0 + iSAFETY_SPRITE_OVERLAP,	// x2,y2
					x0 - iSAFETY_SPRITE_OVERLAP, y0 + iSAFETY_SPRITE_OVERLAP,	// x3,y3
					Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE
				);

				// bottom...
				//
				RE_Blit(x0 - iSAFETY_SPRITE_OVERLAP, y3 - iSAFETY_SPRITE_OVERLAP,	// x0,y0
					x1 + iSAFETY_SPRITE_OVERLAP, y2 - iSAFETY_SPRITE_OVERLAP,		// x1,y1
					x1 + iSAFETY_SPRITE_OVERLAP, f_y_scale_factor * Dissolve.iHeight,	// x2,y2
					x0 - iSAFETY_SPRITE_OVERLAP, f_y_scale_factor * Dissolve.iHeight,	// x3,y3
					Dissolve.pBlack, GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE
				);
			}
			break;

			default:
			{
				assert(0);
				i_dissolve_percentage = 101;	// force a dissolve-kill
				break;
			}
			}

			// re-check in case we hit the default case above...
			//
			if (i_dissolve_percentage <= 100)
			{
				// still dissolving, so now (finally), blit old image over top...
				//
				x0 = 0.0f;
				y0 = 0.0f;
				x1 = f_x_scale_factor * Dissolve.pImage->width;
				y1 = y0;
				x2 = x1;
				y2 = f_y_scale_factor * Dissolve.pImage->height;
				x3 = x0;
				y3 = y2;

				RE_Blit(x0, y0, x1, y1, x2, y2, x3, y3, Dissolve.pImage, GLS_DEPTHFUNC_EQUAL);
			}
		}

		if (i_dissolve_percentage > 100)
		{
			RE_KillDissolve();
		}
	}

	return qfalse;
}

// return = qtrue(success) else fail, for those interested...
//
qboolean RE_InitDissolve(const qboolean bForceCircularExtroWipe)
{
	R_IssuePendingRenderCommands();

	//	ri.Printf( PRINT_ALL, "RE_InitDissolve()\n");
	qboolean bReturn = qfalse;

	if (//Dissolve.iStartTime == 0	// no point in interruping an existing one
		//&&
		tr.registered == qtrue		// ... stops it crashing during first cinematic before the menus... :-)
		)
	{
		RE_KillDissolve();	// kill any that are already running

		const int i_pow2_vid_width = PowerOf2(glConfig.vidWidth);
		const int i_pow2_vid_height = PowerOf2(glConfig.vidHeight);

		const int i_buffer_bytes = i_pow2_vid_width * i_pow2_vid_height * 4;
		byte* p_buffer = static_cast<byte*>(R_Malloc(i_buffer_bytes, TAG_TEMP_WORKSPACE, qfalse));
		if (p_buffer)
		{
			// read current screen image...  (GL_RGBA should work even on 3DFX in that the RGB parts will be valid at least)
			//
			qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_RGBA, GL_UNSIGNED_BYTE, p_buffer);
			//
			// now expand the pic over the top of itself so that it has a stride value of {PowerOf2(glConfig.vidWidth)}
			//	(for GL power-of-2 rules)
			//
			byte* pb_src = &p_buffer[glConfig.vidWidth * glConfig.vidHeight * 4];
			byte* pb_dst = &p_buffer[i_pow2_vid_width * glConfig.vidHeight * 4];
			//
			// ( clear to end, since we've got pbDst nicely setup here)
			//
			int i_clear_bytes = &p_buffer[i_buffer_bytes] - pb_dst;
			memset(pb_dst, 0, i_clear_bytes);
			//
			// work out copy/stride vals...
			//
			i_clear_bytes = (i_pow2_vid_width - glConfig.vidWidth) * 4;
			const int i_copy_bytes = glConfig.vidWidth * 4;
			//
			// do it...
			//
			for (int y = 0; y < glConfig.vidHeight; y++)
			{
				pb_dst -= i_clear_bytes;
				memset(pb_dst, 0, i_clear_bytes);
				pb_dst -= i_copy_bytes;
				pb_src -= i_copy_bytes;
				memmove(pb_dst, pb_src, i_copy_bytes);
			}
			//
			// ok, now we've got the screen image in the top left of the power-of-2 texture square,
			//	but of course the damn thing's upside down (thanks, GL), so invert it, but only within
			//	the picture pixels, NOT the upload texture as a whole...
			//
			byte* pb_swap_line_buffer = static_cast<byte*>(R_Malloc(i_copy_bytes, TAG_TEMP_WORKSPACE, qfalse));
			pb_src = &p_buffer[0];
			pb_dst = &p_buffer[(glConfig.vidHeight - 1) * i_pow2_vid_width * 4];
			for (int y = 0; y < glConfig.vidHeight / 2; y++)
			{
				memcpy(pb_swap_line_buffer, pb_dst, i_copy_bytes);
				memcpy(pb_dst, pb_src, i_copy_bytes);
				memcpy(pb_src, pb_swap_line_buffer, i_copy_bytes);
				pb_dst -= i_pow2_vid_width * 4;
				pb_src += i_pow2_vid_width * 4;
			}
			R_Free(pb_swap_line_buffer);

			//
			// Now, in case of busted drivers, 3DFX cards, etc etc we stomp the alphas to 255...
			//
			byte* p_pix = p_buffer;
			for (int i = 0; i < i_buffer_bytes / 4; i++, p_pix += 4)
			{
				p_pix[3] = 255;
			}

			// work out what res we're capable of storing/xfading this "screen sprite"...
			//
			Dissolve.iWidth = glConfig.vidWidth;
			Dissolve.iHeight = glConfig.vidHeight;
			Dissolve.iUploadWidth = i_pow2_vid_width;
			Dissolve.iUploadHeight = i_pow2_vid_height;
			int	i_tex_size = glConfig.maxTextureSize;

			if (glConfig.maxTextureSize < 256)	// jic the driver sucks
			{
				i_tex_size = 256;
			}

			if (Dissolve.iUploadWidth > i_tex_size) {
				Dissolve.iUploadWidth = i_tex_size;
			}

			if (Dissolve.iUploadHeight > i_tex_size) {
				Dissolve.iUploadHeight = i_tex_size;
			}

			// alloc resample buffer...  (note slight optimisation to avoid spurious alloc)
			//
			byte* pb_re_sample_buffer = i_pow2_vid_width == Dissolve.iUploadWidth &&
				i_pow2_vid_height == Dissolve.iUploadHeight ?
				nullptr :
				static_cast<byte*>(R_Malloc(i_pow2_vid_width * i_pow2_vid_height * 4, TAG_TEMP_WORKSPACE, qfalse));

			// re-sample screen...
			//
			const byte* pb_screen_sprite = RE_ReSample(p_buffer,				// byte *pbLoadedPic
				i_pow2_vid_width,			// int iLoadedWidth
				i_pow2_vid_height,			// int iLoadedHeight
				//
				pb_re_sample_buffer,		// byte *pbReSampleBuffer
				&Dissolve.iUploadWidth,	// int *piWidth
				&Dissolve.iUploadHeight	// int *piHeight
			);

			Dissolve.pImage = R_CreateImage("*DissolveImage",		// const char *name
				pb_screen_sprite,			// const byte *pic
				Dissolve.iUploadWidth,	// int width
				Dissolve.iUploadHeight,	// int height
				GL_RGBA,
				qfalse,					// qboolean mipmap
				qfalse,					// qboolean allowPicmip
				qfalse,					// qboolean allowTC
				GL_CLAMP				// int glWrapClampMode
			);

			static byte b_black[8 * 8 * 4] = { 0 };
			for (int j = 0; j < 8 * 8 * 4; j += 4)	// itu?
				b_black[j + 3] = 255;		//

			Dissolve.pBlack = R_CreateImage("*DissolveBlack",	// const char *name
				b_black,				// const byte *pic
				8,					// int width
				8,					// int height
				GL_RGBA,
				qfalse,				// qboolean mipmap
				qfalse,				// qboolean allowPicmip
				qfalse,				// qboolean allowTC
				GL_CLAMP			// int glWrapClampMode
			);

			if (pb_re_sample_buffer)
			{
				R_Free(pb_re_sample_buffer);
			}
			R_Free(p_buffer);

			// pick dissolve type...
			//
#if 0
			// cycles through every dissolve type, for testing...
			//
			static Dissolve_e eDissolve = (Dissolve_e)0;
			Dissolve.eDissolveType = eDissolve;
			eDissolve = (Dissolve_e)(eDissolve + 1);
			if (eDissolve == eDISSOLVE_RAND_LIMIT)
				eDissolve = (Dissolve_e)(eDissolve + 1);
			if (eDissolve >= eDISSOLVE_NUMBEROF)
				eDissolve = (Dissolve_e)0;
#else
			// final (& random) version...
			//
			Dissolve.eDissolveType = static_cast<Dissolve_e>(Q_irand(0, eDISSOLVE_RAND_LIMIT - 1));
#endif

			if (bForceCircularExtroWipe)
			{
				Dissolve.eDissolveType = eDISSOLVE_CIRCULAR_IN;
			}

			// ... and load appropriate graphics...
			//

			// special tweak, although this code is normally called just before client spawns into world (and
			//	is therefore pretty much immune to precache issues) I also need to make sure that the inverse
			//	iris graphic is loaded so for the special case of doing a circular wipe at the end of the last
			//	level doesn't stall on loading the image. So I'll load it here anyway - to prime the image -
			//	then allow the random wiper to overwrite the ptr if needed. This way the end of level call
			//	will be instant.  Downside: every level has one extra 256x256 texture.
			// Trying to decipher these comments - looks like no problem taking this out. I want the RAM.
			{
				Dissolve.pDissolve = R_FindImageFile("gfx/2d/iris_mono_rev",		// const char *name
					qfalse,						// qboolean mipmap
					qfalse,						// qboolean allowPicmip
					qfalse,						// qboolean allowTC
					GL_CLAMP					// int glWrapClampMode
				);
			}

			extern cvar_t* com_buildScript;
			if (com_buildScript->integer)
			{
				// register any/all of the possible CASE statements below...
				//
				Dissolve.pDissolve = R_FindImageFile("gfx/2d/iris_mono",			// const char *name
					qfalse,						// qboolean mipmap
					qfalse,						// qboolean allowPicmip
					qfalse,						// qboolean allowTC
					GL_CLAMP					// int glWrapClampMode
				);
				Dissolve.pDissolve = R_FindImageFile("textures/common/dissolve",	// const char *name
					qfalse,						// qboolean mipmap
					qfalse,						// qboolean allowPicmip
					qfalse,						// qboolean allowTC
					GL_REPEAT					// int glWrapClampMode
				);
			}

			switch (Dissolve.eDissolveType)
			{
			case eDISSOLVE_CIRCULAR_IN:
			{
				Dissolve.pDissolve = R_FindImageFile("gfx/2d/iris_mono_rev",		// const char *name
					qfalse,						// qboolean mipmap
					qfalse,						// qboolean allowPicmip
					qfalse,						// qboolean allowTC
					GL_CLAMP					// int glWrapClampMode
				);
			}
			break;

			case eDISSOLVE_CIRCULAR_OUT:
			{
				Dissolve.pDissolve = R_FindImageFile("gfx/2d/iris_mono",			// const char *name
					qfalse,						// qboolean mipmap
					qfalse,						// qboolean allowPicmip
					qfalse,						// qboolean allowTC
					GL_CLAMP					// int glWrapClampMode
				);
			}
			break;

			default:
			{
				Dissolve.pDissolve = R_FindImageFile("textures/common/dissolve",	// const char *name
					qfalse,						// qboolean mipmap
					qfalse,						// qboolean allowPicmip
					qfalse,						// qboolean allowTC
					GL_REPEAT					// int glWrapClampMode
				);
			}
			break;
			}

			// all good?...
			//
			if (Dissolve.pDissolve)	// test if image was found, if not, don't do dissolves
			{
				Dissolve.iStartTime = ri.Milliseconds();	// gets overwritten first time, but MUST be set to NZ
				Dissolve.bTouchNeeded = qtrue;
				bReturn = qtrue;
			}
			else
			{
				RE_KillDissolve();
			}
		}
	}

	return bReturn;
}