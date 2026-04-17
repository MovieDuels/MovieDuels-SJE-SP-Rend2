/*
===========================================================================
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

#include "q_shared.h"
#include "matcomp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <memory.h>	// for memcpy

#define MC_MASK_X ((1<<(MC_BITS_X))-1)
#define MC_MASK_Y ((1<<(MC_BITS_Y))-1)
#define MC_MASK_Z ((1<<(MC_BITS_Z))-1)
#define MC_MASK_VECT ((1<<(MC_BITS_VECT))-1)

#define MC_SCALE_VECT (1.0f/(float)((1<<(MC_BITS_VECT-1))-2))

#define MC_POS_X (0)
#define MC_SHIFT_X (0)

#define MC_POS_Y ((((MC_BITS_X))/8))
#define MC_SHIFT_Y ((((MC_BITS_X)%8)))

#define MC_POS_Z ((((MC_BITS_X+MC_BITS_Y))/8))
#define MC_SHIFT_Z ((((MC_BITS_X+MC_BITS_Y)%8)))

#define MC_POS_V11 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z))/8))
#define MC_SHIFT_V11 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z)%8)))

#define MC_POS_V12 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT))/8))
#define MC_SHIFT_V12 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT)%8)))

#define MC_POS_V13 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*2))/8))
#define MC_SHIFT_V13 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*2)%8)))

#define MC_POS_V21 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*3))/8))
#define MC_SHIFT_V21 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*3)%8)))

#define MC_POS_V22 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*4))/8))
#define MC_SHIFT_V22 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*4)%8)))

#define MC_POS_V23 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*5))/8))
#define MC_SHIFT_V23 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*5)%8)))

#define MC_POS_V31 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*6))/8))
#define MC_SHIFT_V31 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*6)%8)))

#define MC_POS_V32 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*7))/8))
#define MC_SHIFT_V32 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*7)%8)))

#define MC_POS_V33 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*8))/8))
#define MC_SHIFT_V33 ((((MC_BITS_X+MC_BITS_Y+MC_BITS_Z+MC_BITS_VECT*8)%8)))

void MC_Compress(const float mat[3][4], unsigned char* _comp)
{
	char comp[MC_COMP_BYTES * 2];

	for (int i = 0; i < MC_COMP_BYTES; i++)
		comp[i] = 0;

	int val = static_cast<int>(mat[0][3] / MC_SCALE_X);
	val += 1 << (MC_BITS_X - 1);
	if (val >= (1 << MC_BITS_X))
		val = (1 << MC_BITS_X) - 1;
	if (val < 0)
		val = 0;

	auto ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_X]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_X;

	val = static_cast<int>(mat[1][3] / MC_SCALE_Y);
	val += 1 << (MC_BITS_Y - 1);
	if (val >= (1 << MC_BITS_Y))
		val = (1 << MC_BITS_Y) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_Y]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_Y;

	val = static_cast<int>(mat[2][3] / MC_SCALE_Z);
	val += 1 << (MC_BITS_Z - 1);
	if (val >= (1 << MC_BITS_Z))
		val = (1 << MC_BITS_Z) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_Z]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_Z;

	val = static_cast<int>(mat[0][0] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V11]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V11;

	val = static_cast<int>(mat[0][1] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V12]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V12;

	val = static_cast<int>(mat[0][2] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V13]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V13;

	val = static_cast<int>(mat[1][0] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V21]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V21;

	val = static_cast<int>(mat[1][1] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V22]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V22;

	val = static_cast<int>(mat[1][2] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V23]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V23;

	val = static_cast<int>(mat[2][0] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V31]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V31;

	val = static_cast<int>(mat[2][1] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V32]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V32;

	val = static_cast<int>(mat[2][2] / MC_SCALE_VECT);
	val += 1 << (MC_BITS_VECT - 1);
	if (val >= (1 << MC_BITS_VECT))
		val = (1 << MC_BITS_VECT) - 1;
	if (val < 0)
		val = 0;

	ba = reinterpret_cast<byteAlias_t*>(&comp[MC_POS_V33]);
	ba->ui |= static_cast<uint32_t>(val) << MC_SHIFT_V33;

	// I added this because the line above actually ORs data into an int at the 22 byte (from 0), and therefore
	//	technically is writing beyond the 24th byte of the output array. This *should* be harmless if the OR'd-in value
	//	doesn't change those bytes, but BoundsChecker says that it's accessing undefined memory (which it does,
	//	sometimes). This is probably bad, so...
	memcpy(_comp, comp, MC_COMP_BYTES);
}

void MC_UnCompress(float mat[3][4], const unsigned char* comp)
{
	int val = ((unsigned short*)comp)[0];
	val -= 1 << (MC_BITS_X - 1);
	mat[0][3] = static_cast<float>(val) * MC_SCALE_X;

	val = static_cast<int>(((unsigned short*)(comp))[1]);
	val -= 1 << (MC_BITS_Y - 1);
	mat[1][3] = static_cast<float>(val) * MC_SCALE_Y;

	val = static_cast<int>(((unsigned short*)(comp))[2]);
	val -= 1 << (MC_BITS_Z - 1);
	mat[2][3] = static_cast<float>(val) * MC_SCALE_Z;

	val = static_cast<int>(((unsigned short*)(comp))[3]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[0][0] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[4]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[0][1] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[5]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[0][2] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[6]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[1][0] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[7]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[1][1] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[8]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[1][2] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[9]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[2][0] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[10]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[2][1] = static_cast<float>(val) * MC_SCALE_VECT;

	val = static_cast<int>(((unsigned short*)(comp))[11]);
	val -= 1 << (MC_BITS_VECT - 1);
	mat[2][2] = static_cast<float>(val) * MC_SCALE_VECT;
}

void MC_UnCompressQuat(float mat[3][4], const unsigned char* comp)
{
	const unsigned short* pw_in = (unsigned short*)comp;

	float w = *pw_in++;
	w /= 16383.0f;
	w -= 2.0f;
	float x = *pw_in++;
	x /= 16383.0f;
	x -= 2.0f;
	float y = *pw_in++;
	y /= 16383.0f;
	y -= 2.0f;
	float z = *pw_in++;
	z /= 16383.0f;
	z -= 2.0f;

	const float f_tx = 2.0f * x;
	const float f_ty = 2.0f * y;
	const float f_tz = 2.0f * z;
	const float f_twx = f_tx * w;
	const float f_twy = f_ty * w;
	const float f_twz = f_tz * w;
	const float f_txx = f_tx * x;
	const float f_txy = f_ty * x;
	const float f_txz = f_tz * x;
	const float f_tyy = f_ty * y;
	const float f_tyz = f_tz * y;
	const float f_tzz = f_tz * z;

	// rot...
	//
	mat[0][0] = 1.0f - (f_tyy + f_tzz);
	mat[0][1] = f_txy - f_twz;
	mat[0][2] = f_txz + f_twy;
	mat[1][0] = f_txy + f_twz;
	mat[1][1] = 1.0f - (f_txx + f_tzz);
	mat[1][2] = f_tyz - f_twx;
	mat[2][0] = f_txz - f_twy;
	mat[2][1] = f_tyz + f_twx;
	mat[2][2] = 1.0f - (f_txx + f_tyy);

	// xlat...
	//
	float f = *pw_in++;
	f /= 64;
	f -= 512;
	mat[0][3] = f;

	f = *pw_in++;
	f /= 64;
	f -= 512;
	mat[1][3] = f;

	f = *pw_in++;
	f /= 64;
	f -= 512;
	mat[2][3] = f;
}