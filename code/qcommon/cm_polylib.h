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

// this is only used for visualization tools in cm_ debug functions

#ifndef CM_POLYLIB_H
#define CM_POLYLIB_H

using winding_t = struct
{
	int numpoints;
	vec3_t p[4]; // variable sized
};

constexpr auto MAX_POINTS_ON_WINDING = 64;

constexpr auto SIDE_FRONT = 0;
constexpr auto SIDE_BACK = 1;
constexpr auto SIDE_ON = 2;
constexpr auto SIDE_CROSS = 3;

constexpr auto CLIP_EPSILON = 0.1f;

constexpr auto MAX_MAP_BOUNDS = 65535;

// you can define on_epsilon in the makefile as tighter
#ifndef	ON_EPSILON
#define	ON_EPSILON	0.1f
#endif

winding_t* AllocWinding(int points);
winding_t* CopyWinding(const winding_t* w);
winding_t* BaseWindingForPlane(vec3_t normal, vec_t dist);
void FreeWinding(winding_t* w);
void WindingBounds(const winding_t* w, vec3_t mins, vec3_t maxs);

void ChopWindingInPlace(winding_t** inout, vec3_t normal, vec_t dist, vec_t epsilon);
// frees the original if clipped

void pw(const winding_t* w);
#endif
