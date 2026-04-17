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

#include "cm_local.h"

// counters are only bumped when running single threaded,
// because they are an awefull coherence problem
int c_active_windings;
int c_peak_windings;
int c_winding_allocs;
int c_winding_points;

void pw(const winding_t* w)
{
	for (int i = 0; i < w->numpoints; i++)
		printf("(%5.1f, %5.1f, %5.1f)\n", w->p[i][0], w->p[i][1], w->p[i][2]);
}

/*
=============
AllocWinding
=============
*/
winding_t* AllocWinding(const int points)
{
	c_winding_allocs++;
	c_winding_points += points;
	c_active_windings++;
	if (c_active_windings > c_peak_windings)
		c_peak_windings = c_active_windings;

	const int s = sizeof(vec_t) * 3 * points + sizeof(int);
	const auto w = static_cast<winding_t*>(Z_Malloc(s, TAG_BSP, qtrue)); //TAG_WINDING);
	//	memset (w, 0, s);	// qtrue above does this
	return w;
}

void FreeWinding(winding_t* w)
{
	if (*reinterpret_cast<unsigned*>(w) == 0xdeaddead)
		Com_Error(ERR_FATAL, "FreeWinding: freed a freed winding");
	*reinterpret_cast<unsigned*>(w) = 0xdeaddead;

	c_active_windings--;
	Z_Free(w);
}

void WindingBounds(const winding_t* w, vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = WORLD_SIZE; // 99999;	// WORLD_SIZE instead of MAX_WORLD_COORD so that...
	maxs[0] = maxs[1] = maxs[2] = -WORLD_SIZE; //-99999;	// ... it's guaranteed to be outide of legal

	for (int i = 0; i < w->numpoints; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			const vec_t v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
}

/*
=================
BaseWindingForPlane
=================
*/
winding_t* BaseWindingForPlane(vec3_t normal, const vec_t dist)
{
	vec_t v;
	vec3_t org, vright, vup;

	// find the major axis

	vec_t max = -MAX_MAP_BOUNDS;
	int x = -1;
	for (int i = 0; i < 3; i++)
	{
		v = fabs(normal[i]);
		if (v > max)
		{
			x = i;
			max = v;
		}
	}
	if (x == -1)
		Com_Error(ERR_DROP, "BaseWindingForPlane: no axis found");

	VectorCopy(vec3_origin, vup);
	switch (x)
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	default:;
	}

	v = DotProduct(vup, normal);
	VectorMA(vup, -v, normal, vup);
	VectorNormalize2(vup, vup);

	VectorScale(normal, dist, org);

	CrossProduct(vup, normal, vright);

	VectorScale(vup, MAX_MAP_BOUNDS, vup);
	VectorScale(vright, MAX_MAP_BOUNDS, vright);

	// project a really big	axis aligned box onto the plane
	winding_t* w = AllocWinding(4);

	VectorSubtract(org, vright, w->p[0]);
	VectorAdd(w->p[0], vup, w->p[0]);

	VectorAdd(org, vright, w->p[1]);
	VectorAdd(w->p[1], vup, w->p[1]);

	VectorAdd(org, vright, w->p[2]);
	VectorSubtract(w->p[2], vup, w->p[2]);

	VectorSubtract(org, vright, w->p[3]);
	VectorSubtract(w->p[3], vup, w->p[3]);

	w->numpoints = 4;

	return w;
}

/*
==================
CopyWinding
==================
*/
winding_t* CopyWinding(const winding_t* w)
{
	winding_t* c = AllocWinding(w->numpoints);
	const intptr_t size = reinterpret_cast<intptr_t>(static_cast<winding_t*>(nullptr)->p[w->numpoints]);
	memcpy(c, w, size);
	return c;
}

/*
=============
ChopWindingInPlace
=============
*/
void ChopWindingInPlace(winding_t** inout, vec3_t normal, const vec_t dist, const vec_t epsilon)
{
	vec_t dists[MAX_POINTS_ON_WINDING + 4]{};
	int sides[MAX_POINTS_ON_WINDING + 4]{};
	int counts[3]{};
	static vec_t dot; // VC 4.2 optimizer bug if not static
	int i;
	vec3_t mid{};

	winding_t* in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++)
	{
		dot = DotProduct(in->p[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0])
	{
		FreeWinding(in);
		*inout = nullptr;
		return;
	}
	if (!counts[1])
		return; // inout stays the same

	const int maxpts = in->numpoints + 4; // cant use counts[0]+2 because
	// of fp grouping errors

	winding_t* f = AllocWinding(maxpts);

	for (i = 0; i < in->numpoints; i++)
	{
		const vec_t* p1 = in->p[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy(p1, f->p[f->numpoints]);
			f->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy(p1, f->p[f->numpoints]);
			f->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		// generate a split point
		const vec_t* p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (int j = 0; j < 3; j++)
		{
			// avoid round off error when possible
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy(mid, f->p[f->numpoints]);
		f->numpoints++;
	}

	if (f->numpoints > maxpts)
		Com_Error(ERR_DROP, "ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Com_Error(ERR_DROP, "ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWinding(in);
	*inout = f;
}