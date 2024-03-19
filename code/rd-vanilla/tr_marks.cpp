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

// tr_marks.c -- polygon projection on the world polygons

#include "../server/exe_headers.h"

#include "tr_local.h"

#define MAX_VERTS_ON_POLY		64

#define MARKER_OFFSET			0	// 1

/*
=============
R_ChopPolyBehindPlane

Out must have space for two more vertexes than in
=============
*/
#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
static void R_ChopPolyBehindPlane(const int num_in_points, vec3_t in_points[MAX_VERTS_ON_POLY],
	int* num_out_points, vec3_t out_points[MAX_VERTS_ON_POLY],
	vec3_t normal, const vec_t dist, const vec_t epsilon) {
	float		dists[MAX_VERTS_ON_POLY + 4] = { 0 };
	int			sides[MAX_VERTS_ON_POLY + 4] = { 0 };
	int			counts[3]{};
	float		dot;
	int			i;

	// don't clip if it might overflow
	if (num_in_points >= MAX_VERTS_ON_POLY - 2) {
		*num_out_points = 0;
		return;
	}

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < num_in_points; i++) {
		dot = DotProduct(in_points[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon) {
			sides[i] = SIDE_FRONT;
		}
		else if (dot < -epsilon) {
			sides[i] = SIDE_BACK;
		}
		else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*num_out_points = 0;

	if (!counts[0]) {
		return;
	}
	if (!counts[1]) {
		*num_out_points = num_in_points;
		memcpy(out_points, in_points, num_in_points * sizeof(vec3_t));
		return;
	}

	for (i = 0; i < num_in_points; i++) {
		const float* p1 = in_points[i];
		float* clip = out_points[*num_out_points];

		if (sides[i] == SIDE_ON) {
			VectorCopy(p1, clip);
			(*num_out_points)++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, clip);
			(*num_out_points)++;
			clip = out_points[*num_out_points];
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i]) {
			continue;
		}

		// generate a split point
		const float* p2 = in_points[(i + 1) % num_in_points];

		const float d = dists[i] - dists[i + 1];
		if (d == 0) {
			dot = 0;
		}
		else {
			dot = dists[i] / d;
		}

		// clip xyz

		for (int j = 0; j < 3; j++) {
			clip[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		(*num_out_points)++;
	}
}

/*
=================
R_BoxSurfaces_r

=================
*/
void R_BoxSurfaces_r(const mnode_t* node, vec3_t mins, vec3_t maxs, surfaceType_t** list, const int listsize, int* listlength, vec3_t dir) {
	int			s;

	// do the tail recursion in a loop
	while (node->contents == -1) {
		s = BoxOnPlaneSide(mins, maxs, node->plane);
		if (s == 1) {
			node = node->children[0];
		}
		else if (s == 2) {
			node = node->children[1];
		}
		else {
			R_BoxSurfaces_r(node->children[0], mins, maxs, list, listsize, listlength, dir);
			node = node->children[1];
		}
	}

	// add the individual surfaces
	msurface_t** mark = node->firstmarksurface;
	int c = node->nummarksurfaces;
	while (c--) {
		//
		if (*listlength >= listsize) break;
		//
		msurface_t* surf = *mark;

		// check if the surface has NOIMPACT or NOMARKS set
		if (surf->shader->surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS)
			|| surf->shader->contentFlags & CONTENTS_FOG) {
			surf->viewCount = tr.viewCount;
		}
		// extra check for surfaces to avoid list overflows
		else if (*surf->data == SF_FACE) {
			// the face plane should go through the box
			s = BoxOnPlaneSide(mins, maxs, &reinterpret_cast<srfSurfaceFace_t*>(surf->data)->plane);
			if (s == 1 || s == 2) {
				surf->viewCount = tr.viewCount;
			}
			else if (DotProduct(reinterpret_cast<srfSurfaceFace_t*>(surf->data)->plane.normal, dir) > -0.5) {
				// don't add faces that make sharp angles with the projection direction
				surf->viewCount = tr.viewCount;
			}
		}
		else if (*surf->data != SF_GRID
			&& *surf->data != SF_TRIANGLES)
		{
			surf->viewCount = tr.viewCount;
		}
		// check the viewCount because the surface may have
		// already been added if it spans multiple leafs
		if (surf->viewCount != tr.viewCount) {
			surf->viewCount = tr.viewCount;
			list[*listlength] = surf->data;
			(*listlength)++;
		}
		mark++;
	}
}

/*
=================
R_AddMarkFragments

=================
*/
void R_AddMarkFragments(int num_clip_points, vec3_t clip_points[2][MAX_VERTS_ON_POLY],
	const int num_planes, vec3_t* normals, const float* dists,
	const int max_points, vec3_t point_buffer,
	int max_fragments, markFragment_t* fragment_buffer,
	int* returned_points, int* returned_fragments, vec3_t mins, vec3_t maxs)
{
	// chop the surface by all the bounding planes of the to be projected polygon
	int ping_pong = 0;

	for (int i = 0; i < num_planes; i++) {
		R_ChopPolyBehindPlane(num_clip_points, clip_points[ping_pong],
			&num_clip_points, clip_points[!ping_pong],
			normals[i], dists[i], 0.5);
		ping_pong ^= 1;
		if (num_clip_points == 0) {
			break;
		}
	}
	// completely clipped away?
	if (num_clip_points == 0) {
		return;
	}

	// add this fragment to the returned list
	if (num_clip_points + *returned_points > max_points) {
		return;	// not enough space for this polygon
	}

	markFragment_t* mf = fragment_buffer + *returned_fragments;
	mf->firstPoint = *returned_points;
	mf->numPoints = num_clip_points;
	memcpy(point_buffer + *returned_points * 3, clip_points[ping_pong], num_clip_points * sizeof(vec3_t));

	*returned_points += num_clip_points;
	(*returned_fragments)++;
}

/*
=================
R_MarkFragments

=================
*/
int R_MarkFragments(int num_points, const vec3_t* points, const vec3_t projection,
	const int max_points, vec3_t point_buffer, const int max_fragments, markFragment_t* fragment_buffer) {
	int				numsurfaces;
	int				i, k;
	surfaceType_t* surfaces[64];
	vec3_t			mins, maxs;
	int				returned_fragments;
	int				returned_points;
	vec3_t			normals[MAX_VERTS_ON_POLY + 2]{};
	float			dists[MAX_VERTS_ON_POLY + 2]{};
	vec3_t			projection_dir;
	vec3_t			v1, v2;

	//increment view count for double check prevention
	tr.viewCount++;

	//
	VectorNormalize2(projection, projection_dir);
	// find all the brushes that are to be considered
	ClearBounds(mins, maxs);
	for (i = 0; i < num_points; i++) {
		vec3_t	temp;

		AddPointToBounds(points[i], mins, maxs);
		VectorAdd(points[i], projection, temp);
		AddPointToBounds(temp, mins, maxs);
		// make sure we get all the leafs (also the one(s) in front of the hit surface)
		VectorMA(points[i], -20, projection_dir, temp);
		AddPointToBounds(temp, mins, maxs);
	}

	if (num_points > MAX_VERTS_ON_POLY) num_points = MAX_VERTS_ON_POLY;
	// create the bounding planes for the to be projected polygon
	for (i = 0; i < num_points; i++) {
		VectorSubtract(points[(i + 1) % num_points], points[i], v1);
		VectorAdd(points[i], projection, v2);
		VectorSubtract(points[i], v2, v2);
		CrossProduct(v1, v2, normals[i]);
		VectorNormalizeFast(normals[i]);
		dists[i] = DotProduct(normals[i], points[i]);
	}
	// add near and far clipping planes for projection
	VectorCopy(projection_dir, normals[num_points]);
	dists[num_points] = DotProduct(normals[num_points], points[0]) - 32;
	VectorCopy(projection_dir, normals[num_points + 1]);
	VectorInverse(normals[num_points + 1]);
	dists[num_points + 1] = DotProduct(normals[num_points + 1], points[0]) - 20;
	const int num_planes = num_points + 2;

	numsurfaces = 0;
	R_BoxSurfaces_r(tr.world->nodes, mins, maxs, surfaces, 64, &numsurfaces, projection_dir);
	//assert(numsurfaces <= 64);
	//assert(numsurfaces != 64);

	returned_points = 0;
	returned_fragments = 0;

	for (i = 0; i < numsurfaces; i++)
	{
		vec3_t normal;
		vec3_t clip_points[2][MAX_VERTS_ON_POLY]{};
		if (*surfaces[i] == SF_GRID) {
			const srfGridMesh_t* const cv = reinterpret_cast<srfGridMesh_t*>(surfaces[i]);
			for (int m = 0; m < cv->height - 1; m++) {
				for (int n = 0; n < cv->width - 1; n++) {
					// We triangulate the grid and chop all triangles within
					// the bounding planes of the to be projected polygon.
					// LOD is not taken into account, not such a big deal though.
					//
					// It's probably much nicer to chop the grid itself and deal
					// with this grid as a normal SF_GRID surface so LOD will
					// be applied. However the LOD of that chopped grid must
					// be synced with the LOD of the original curve.
					// One way to do this; the chopped grid shares vertices with
					// the original curve. When LOD is applied to the original
					// curve the unused vertices are flagged. Now the chopped curve
					// should skip the flagged vertices. This still leaves the
					// problems with the vertices at the chopped grid edges.
					//
					// To avoid issues when LOD applied to "hollow curves" (like
					// the ones around many jump pads) we now just add a 2 unit
					// offset to the triangle vertices.
					// The offset is added in the vertex normal vector direction
					// so all triangles will still fit together.
					// The 2 unit offset should avoid pretty much all LOD problems.

					constexpr int num_clip_points = 3;

					const drawVert_t* const dv = cv->verts + m * cv->width + n;

					VectorCopy(dv[0].xyz, clip_points[0][0]);
					VectorMA(clip_points[0][0], MARKER_OFFSET, dv[0].normal, clip_points[0][0]);
					VectorCopy(dv[cv->width].xyz, clip_points[0][1]);
					VectorMA(clip_points[0][1], MARKER_OFFSET, dv[cv->width].normal, clip_points[0][1]);
					VectorCopy(dv[1].xyz, clip_points[0][2]);
					VectorMA(clip_points[0][2], MARKER_OFFSET, dv[1].normal, clip_points[0][2]);
					// check the normal of this triangle
					VectorSubtract(clip_points[0][0], clip_points[0][1], v1);
					VectorSubtract(clip_points[0][2], clip_points[0][1], v2);
					CrossProduct(v1, v2, normal);
					VectorNormalizeFast(normal);
					if (DotProduct(normal, projection_dir) < -0.1) {
						// add the fragments of this triangle
						R_AddMarkFragments(num_clip_points, clip_points,
							num_planes, normals, dists,
							max_points, point_buffer,
							max_fragments, fragment_buffer,
							&returned_points, &returned_fragments, mins, maxs);

						if (returned_fragments == max_fragments) {
							return returned_fragments;	// not enough space for more fragments
						}
					}

					VectorCopy(dv[1].xyz, clip_points[0][0]);
					VectorMA(clip_points[0][0], MARKER_OFFSET, dv[1].normal, clip_points[0][0]);
					VectorCopy(dv[cv->width].xyz, clip_points[0][1]);
					VectorMA(clip_points[0][1], MARKER_OFFSET, dv[cv->width].normal, clip_points[0][1]);
					VectorCopy(dv[cv->width + 1].xyz, clip_points[0][2]);
					VectorMA(clip_points[0][2], MARKER_OFFSET, dv[cv->width + 1].normal, clip_points[0][2]);
					// check the normal of this triangle
					VectorSubtract(clip_points[0][0], clip_points[0][1], v1);
					VectorSubtract(clip_points[0][2], clip_points[0][1], v2);
					CrossProduct(v1, v2, normal);
					VectorNormalizeFast(normal);
					if (DotProduct(normal, projection_dir) < -0.05) {
						// add the fragments of this triangle
						R_AddMarkFragments(num_clip_points, clip_points,
							num_planes, normals, dists,
							max_points, point_buffer,
							max_fragments, fragment_buffer,
							&returned_points, &returned_fragments, mins, maxs);

						if (returned_fragments == max_fragments) {
							return returned_fragments;	// not enough space for more fragments
						}
					}
				}
			}
		}
		else if (*surfaces[i] == SF_FACE) {
			const srfSurfaceFace_t* const surf = reinterpret_cast<srfSurfaceFace_t*>(surfaces[i]);
			// check the normal of this face
			if (DotProduct(surf->plane.normal, projection_dir) > -0.5) {
				continue;
			}

			const int* const indexes = reinterpret_cast<int*>((byte*)surf + surf->ofsIndices);

			for (k = 0; k < surf->numIndices; k += 3) {
				for (int j = 0; j < 3; j++) {
					const float* const v = surf->points[0] + VERTEXSIZE * indexes[k + j];
					VectorMA(v, MARKER_OFFSET, surf->plane.normal, clip_points[0][j]);
				}
				// add the fragments of this face
				R_AddMarkFragments(3, clip_points,
					num_planes, normals, dists,
					max_points, point_buffer,
					max_fragments, fragment_buffer,
					&returned_points, &returned_fragments, mins, maxs);
				if (returned_fragments == max_fragments) {
					return returned_fragments;	// not enough space for more fragments
				}
			}
		}
		else if (*surfaces[i] == SF_TRIANGLES)
		{
			const srfTriangles_t* const surf = reinterpret_cast<srfTriangles_t*>(surfaces[i]);

			for (k = 0; k < surf->numIndexes; k += 3)
			{
				const int i1 = surf->indexes[k];
				const int i2 = surf->indexes[k + 1];
				const int i3 = surf->indexes[k + 2];
				VectorSubtract(surf->verts[i1].xyz, surf->verts[i2].xyz, v1);
				VectorSubtract(surf->verts[i3].xyz, surf->verts[i2].xyz, v2);
				CrossProduct(v1, v2, normal);
				VectorNormalizeFast(normal);
				// check the normal of this triangle
				if (DotProduct(normal, projection_dir) < -0.1)
				{
					VectorMA(surf->verts[i1].xyz, MARKER_OFFSET, normal, clip_points[0][0]);
					VectorMA(surf->verts[i2].xyz, MARKER_OFFSET, normal, clip_points[0][1]);
					VectorMA(surf->verts[i3].xyz, MARKER_OFFSET, normal, clip_points[0][2]);

					// add the fragments of this triangle
					R_AddMarkFragments(3, clip_points,
						num_planes, normals, dists,
						max_points, point_buffer,
						max_fragments, fragment_buffer,
						&returned_points, &returned_fragments, mins, maxs);
					if (returned_fragments == max_fragments)
					{
						return returned_fragments;	// not enough space for more fragments
					}
				}
			}
		}
		else {
		}
	}
	return returned_fragments;
}