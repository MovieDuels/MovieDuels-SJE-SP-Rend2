/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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
// tr_marks.c -- polygon projection on the world polygons

#include "tr_local.h"
//#include "assert.h"

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
static void R_ChopPolyBehindPlane(const int num_in_points, vec3_t in_points[MAX_VERTS_ON_POLY], int* num_out_points, vec3_t out_points[MAX_VERTS_ON_POLY], vec3_t normal, const vec_t dist, const vec_t epsilon)
{
	float		dists[MAX_VERTS_ON_POLY + 4] = { 0 };
	int			sides[MAX_VERTS_ON_POLY + 4] = { 0 };
	int			counts[3];
	float		dot;
	int			i, j;
	float* p1, * p2, * clip;
	float		d;

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
		Com_Memcpy(out_points, in_points, num_in_points * sizeof(vec3_t));
		return;
	}

	for (i = 0; i < num_in_points; i++) {
		p1 = in_points[i];
		clip = out_points[*num_out_points];

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
		p2 = in_points[(i + 1) % num_in_points];

		d = dists[i] - dists[i + 1];
		if (d == 0) {
			dot = 0;
		}
		else {
			dot = dists[i] / d;
		}

		// clip xyz

		for (j = 0; j < 3; j++) {
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
void R_BoxSurfaces_r(const mnode_t* node, vec3_t mins, vec3_t maxs, surfaceType_t** list, const int listsize, int* listlength, vec3_t dir)
{
	int			s, c;
	msurface_t* surf;
	int* mark;

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
	mark = tr.world->marksurfaces + node->firstmarksurface;
	c = node->nummarksurfaces;
	while (c--) {
		int* surfViewCount;
		//
		if (*listlength >= listsize) break;
		//
		surfViewCount = &tr.world->surfacesViewCount[*mark];
		surf = tr.world->surfaces + *mark;
		// check if the surface has NOIMPACT or NOMARKS set
		if ((surf->shader->surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS))
			|| (surf->shader->contentFlags & CONTENTS_FOG)) {
			*surfViewCount = tr.viewCount;
		}
		// extra check for surfaces to avoid list overflows
		else if (*(surf->data) == SF_FACE) {
			// the face plane should go through the box
			s = BoxOnPlaneSide(mins, maxs, &surf->cullinfo.plane);
			if (s == 1 || s == 2) {
				*surfViewCount = tr.viewCount;
			}
			else if (DotProduct(surf->cullinfo.plane.normal, dir) > -0.5) {
				// don't add faces that make sharp angles with the projection direction
				*surfViewCount = tr.viewCount;
			}
		}
		else if (*(surf->data) != SF_GRID &&
			*(surf->data) != SF_TRIANGLES)
			*surfViewCount = tr.viewCount;
		// check the viewCount because the surface may have
		// already been added if it spans multiple leafs
		if (*surfViewCount != tr.viewCount) {
			*surfViewCount = tr.viewCount;
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
void R_AddMarkFragments(int num_clip_points, vec3_t clip_points[2][MAX_VERTS_ON_POLY], const int numplanes, vec3_t* normals, const float* dists, const int max_points, vec3_t point_buffer, int max_fragments, markFragment_t* fragment_buffer, int* returned_points, int* returned_fragments, vec3_t mins, vec3_t maxs)
{
	int pingPong, i;
	markFragment_t* mf;

	// chop the surface by all the bounding planes of the to be projected polygon
	pingPong = 0;

	for (i = 0; i < numplanes; i++) {
		R_ChopPolyBehindPlane(num_clip_points, clip_points[pingPong],
			&num_clip_points, clip_points[!pingPong],
			normals[i], dists[i], 0.5);
		pingPong ^= 1;
		if (num_clip_points == 0) {
			break;
		}
	}
	// completely clipped away?
	if (num_clip_points == 0) {
		return;
	}

	// add this fragment to the returned list
	if (num_clip_points + (*returned_points) > max_points) {
		return;	// not enough space for this polygon
	}
	/*
	// all the clip points should be within the bounding box
	for ( i = 0 ; i < num_clip_points ; i++ ) {
		int j;
		for ( j = 0 ; j < 3 ; j++ ) {
			if (clip_points[pingPong][i][j] < mins[j] - 0.5) break;
			if (clip_points[pingPong][i][j] > maxs[j] + 0.5) break;
		}
		if (j < 3) break;
	}
	if (i < num_clip_points) return;
	*/

	mf = fragment_buffer + (*returned_fragments);
	mf->firstPoint = (*returned_points);
	mf->numPoints = num_clip_points;
	Com_Memcpy(point_buffer + (*returned_points) * 3, clip_points[pingPong], num_clip_points * sizeof(vec3_t));

	(*returned_points) += num_clip_points;
	(*returned_fragments)++;
}

/*
=================
R_MarkFragments

=================
*/
int R_MarkFragments(int numPoints, const vec3_t* points, const vec3_t projection, const int max_points, vec3_t point_buffer, const int max_fragments, markFragment_t* fragment_buffer)
{
	int				numsurfaces, numplanes;
	int				i, j, k, m, n;
	surfaceType_t* surfaces[64];
	vec3_t			mins, maxs;
	int				returned_fragments;
	int				returned_points;
	vec3_t			normals[MAX_VERTS_ON_POLY + 2];
	float			dists[MAX_VERTS_ON_POLY + 2];
	vec3_t			clip_points[2][MAX_VERTS_ON_POLY];
	int				num_clip_points;
	float* v;
	srfBspSurface_t* cv;
	glIndex_t* tri;
	srfVert_t* dv;
	vec3_t			normal;
	vec3_t			projectionDir;
	vec3_t			v1, v2;

	if (numPoints <= 0) {
		return 0;
	}

	//increment view count for double check prevention
	tr.viewCount++;

	//
	VectorNormalize2(projection, projectionDir);
	// find all the brushes that are to be considered
	ClearBounds(mins, maxs);
	for (i = 0; i < numPoints; i++) {
		vec3_t	temp;

		AddPointToBounds(points[i], mins, maxs);
		VectorAdd(points[i], projection, temp);
		AddPointToBounds(temp, mins, maxs);
		// make sure we get all the leafs (also the one(s) in front of the hit surface)
		VectorMA(points[i], -20, projectionDir, temp);
		AddPointToBounds(temp, mins, maxs);
	}

	if (numPoints > MAX_VERTS_ON_POLY) numPoints = MAX_VERTS_ON_POLY;
	// create the bounding planes for the to be projected polygon
	for (i = 0; i < numPoints; i++) {
		VectorSubtract(points[(i + 1) % numPoints], points[i], v1);
		VectorAdd(points[i], projection, v2);
		VectorSubtract(points[i], v2, v2);
		CrossProduct(v1, v2, normals[i]);
		VectorNormalizeFast(normals[i]);
		dists[i] = DotProduct(normals[i], points[i]);
	}
	// add near and far clipping planes for projection
	VectorCopy(projectionDir, normals[numPoints]);
	dists[numPoints] = DotProduct(normals[numPoints], points[0]) - 32;
	VectorCopy(projectionDir, normals[numPoints + 1]);
	VectorInverse(normals[numPoints + 1]);
	dists[numPoints + 1] = DotProduct(normals[numPoints + 1], points[0]) - 20;
	numplanes = numPoints + 2;

	numsurfaces = 0;
	R_BoxSurfaces_r(tr.world->nodes, mins, maxs, surfaces, 64, &numsurfaces, projectionDir);
	//assert(numsurfaces <= 64);
	//assert(numsurfaces != 64);

	returned_points = 0;
	returned_fragments = 0;

	for (i = 0; i < numsurfaces; i++) {
		if (*surfaces[i] == SF_GRID) {
			cv = (srfBspSurface_t*)surfaces[i];
			for (m = 0; m < cv->height - 1; m++) {
				for (n = 0; n < cv->width - 1; n++) {
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

					num_clip_points = 3;

					dv = cv->verts + m * cv->width + n;

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
					if (DotProduct(normal, projectionDir) < -0.1) {
						// add the fragments of this triangle
						R_AddMarkFragments(num_clip_points, clip_points,
							numplanes, normals, dists,
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
					if (DotProduct(normal, projectionDir) < -0.05) {
						// add the fragments of this triangle
						R_AddMarkFragments(num_clip_points, clip_points,
							numplanes, normals, dists,
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
			srfBspSurface_t* surf = (srfBspSurface_t*)surfaces[i];

			// check the normal of this face
			if (DotProduct(surf->cullPlane.normal, projectionDir) > -0.5) {
				continue;
			}

			for (k = 0, tri = surf->indexes; k < surf->numIndexes; k += 3, tri += 3)
			{
				for (j = 0; j < 3; j++)
				{
					v = surf->verts[tri[j]].xyz;
					VectorMA(v, MARKER_OFFSET, surf->cullPlane.normal, clip_points[0][j]);
				}

				// add the fragments of this face
				R_AddMarkFragments(3, clip_points,
					numplanes, normals, dists,
					max_points, point_buffer,
					max_fragments, fragment_buffer,
					&returned_points, &returned_fragments, mins, maxs);
				if (returned_fragments == max_fragments) {
					return returned_fragments;	// not enough space for more fragments
				}
			}
		}
		else if (*surfaces[i] == SF_TRIANGLES && r_marksOnTriangleMeshes->integer) {
			srfBspSurface_t* surf = (srfBspSurface_t*)surfaces[i];

			for (k = 0, tri = surf->indexes; k < surf->numIndexes; k += 3, tri += 3)
			{
				for (j = 0; j < 3; j++)
				{
					v = surf->verts[tri[j]].xyz;
					VectorMA(v, MARKER_OFFSET, surf->verts[tri[j]].normal, clip_points[0][j]);
				}

				// add the fragments of this face
				R_AddMarkFragments(3, clip_points,
					numplanes, normals, dists,
					max_points, point_buffer,
					max_fragments, fragment_buffer, &returned_points, &returned_fragments, mins, maxs);
				if (returned_fragments == max_fragments)
				{
					return returned_fragments;	// not enough space for more fragments
				}
			}
		}
	}
	return returned_fragments;
}