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

// tr_sky.c
#include "tr_local.h"

#define SKY_SUBDIVISIONS		8
#define HALF_SKY_SUBDIVISIONS	(SKY_SUBDIVISIONS/2)

static float s_cloudTexCoords[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];
static float s_cloudTexP[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];

extern bool g_bRenderGlowingObjects;

/*
===================================================================================

POLYGON TO BOX SIDE PROJECTION

===================================================================================
*/

static vec3_t sky_clip[6] =
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};

static float	sky_mins[2][6], sky_maxs[2][6];
static float	sky_min, sky_max;

/*
================
AddSkyPolygon
================
*/
static void AddSkyPolygon(int nump, vec3_t vecs)
{
	int		i;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float* vp;
	// s = [0]/[2], t = [1]/[2]
	static int	floato_st[6][3] =
	{
		{-2,3,1},
		{2,3,-1},

		{1,3,2},
		{-1,3,-2},

		{-2,-1,3},
		{-2,1,-3}

		//	{-1,2,3},
		//	{1,2,-3}
	};

	// decide which face it maps to
	VectorCopy(vec3_origin, v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
	{
		VectorAdd(vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		int j = floato_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001)
			continue;	// don't divide by zero
		j = floato_st[axis][0];
		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;
		j = floato_st[axis][1];
		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if (s < sky_mins[0][axis])
			sky_mins[0][axis] = s;
		if (t < sky_mins[1][axis])
			sky_mins[1][axis] = t;
		if (s > sky_maxs[0][axis])
			sky_maxs[0][axis] = s;
		if (t > sky_maxs[1][axis])
			sky_maxs[1][axis] = t;
	}
}

#define	ON_EPSILON		0.1f			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64
/*
================
ClipSkyPolygon
================
*/
static void ClipSkyPolygon(int nump, vec3_t vecs, int stage)
{
	float* v;
	qboolean back;
	float	d;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i;

	if (nump > MAX_CLIP_VERTS - 2)
		Com_Error(ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	// fully clipped, so draw it
		AddSkyPolygon(nump, vecs);
		return;
	}

	qboolean front = back = qfalse;
	const float* norm = sky_clip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		d = DotProduct(v, norm);
		if (d > ON_EPSILON)
		{
			front = qtrue;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = qtrue;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon(nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i + 1]);
		for (int j = 0; j < 3; j++)
		{
			const float e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon(newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon(newc[1], newv[1][0], stage + 1);
}

/*
==============
ClearSkyBox
==============
*/
static void ClearSkyBox(void) {
	for (int i = 0; i < 6; i++) {
		sky_mins[0][i] = sky_mins[1][i] = 9999;
		sky_maxs[0][i] = sky_maxs[1][i] = -9999;
	}
}

/*
================
RB_ClipSkyPolygons
================
*/
void RB_ClipSkyPolygons(shaderCommands_t* input)
{
	vec3_t		p[5];	// need one extra point for clipping

	ClearSkyBox();

	for (int i = 0; i < input->num_indexes; i += 3)
	{
		for (int j = 0; j < 3; j++)
		{
			VectorSubtract(input->xyz[input->indexes[i + j]],
				backEnd.viewParms.ori.origin,
				p[j]);
		}
		ClipSkyPolygon(3, p[0], 0);
	}
}

/*
===================================================================================

CLOUD VERTEX GENERATION

===================================================================================
*/

/*
** MakeSkyVec
**
** Parms: s, t range from -1 to 1
*/
static void MakeSkyVec(float s, float t, int axis, float out_st[2], vec3_t out_xyz)
{
	// 1 = s, 2 = t, 3 = 2048
	static int	st_to_vec[6][3] =
	{
		{3,-1,2},
		{-3,1,2},

		{1,3,2},
		{-1,-3,2},

		{-2,-1,3},		// 0 degrees yaw, look straight up
		{2,-1,-3}		// look straight down
	};

	vec3_t		b;

	const float boxSize = backEnd.viewParms.zFar / 1.75;		// div sqrt(3)
	b[0] = s * boxSize;
	b[1] = t * boxSize;
	b[2] = boxSize;

	for (int j = 0; j < 3; j++)
	{
		const int k = st_to_vec[axis][j];
		if (k < 0)
		{
			out_xyz[j] = -b[-k - 1];
		}
		else
		{
			out_xyz[j] = b[k - 1];
		}
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;
	if (s < sky_min)
	{
		s = sky_min;
	}
	else if (s > sky_max)
	{
		s = sky_max;
	}

	if (t < sky_min)
	{
		t = sky_min;
	}
	else if (t > sky_max)
	{
		t = sky_max;
	}

	t = 1.0 - t;

	if (out_st)
	{
		out_st[0] = s;
		out_st[1] = t;
	}
}

static vec3_t	s_sky_points[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];
static float	s_sky_tex_coords[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

static void DrawSkySide(image_s* image, const int mins[2], const int maxs[2])
{
	GL_Bind(image);

	for (int t = mins[1] + HALF_SKY_SUBDIVISIONS; t < maxs[1] + HALF_SKY_SUBDIVISIONS; t++)
	{
		qglBegin(GL_TRIANGLE_STRIP);

		for (int s = mins[0] + HALF_SKY_SUBDIVISIONS; s <= maxs[0] + HALF_SKY_SUBDIVISIONS; s++)
		{
			qglTexCoord2fv(s_sky_tex_coords[t][s]);
			qglVertex3fv(s_sky_points[t][s]);

			qglTexCoord2fv(s_sky_tex_coords[t + 1][s]);
			qglVertex3fv(s_sky_points[t + 1][s]);
		}

		qglEnd();
	}
}

static void DrawSkyBox(const shader_t* shader)
{
	sky_min = 0;
	sky_max = 1;

	memset(s_sky_tex_coords, 0, sizeof(s_sky_tex_coords));

	for (int i = 0; i < 6; i++)
	{
		int sky_mins_subd[2], sky_maxs_subd[2];

		sky_mins[0][i] = floor(sky_mins[0][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor(sky_mins[1][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil(sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil(sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;

		if ((sky_mins[0][i] >= sky_maxs[0][i]) ||
			(sky_mins[1][i] >= sky_maxs[1][i]))
		{
			continue;
		}

		sky_mins_subd[0] = sky_mins[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_mins_subd[1] = sky_mins[1][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[0] = sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[1] = sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS;

		if (sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if (sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if (sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if (sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		//
		// iterate through the subdivisions
		//
		for (int t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++)
		{
			for (int s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++)
			{
				MakeSkyVec((s - HALF_SKY_SUBDIVISIONS) / static_cast<float>(HALF_SKY_SUBDIVISIONS),
					(t - HALF_SKY_SUBDIVISIONS) / static_cast<float>(HALF_SKY_SUBDIVISIONS),
					i,
					s_sky_tex_coords[t][s],
					s_sky_points[t][s]);
			}
		}

		DrawSkySide(shader->sky->outerbox[i],
			sky_mins_subd,
			sky_maxs_subd);
	}
}

static void FillCloudySkySide(const int mins[2], const int maxs[2], const qboolean add_indexes)
{
	int s, t;
	const int vertex_start = tess.numVertexes;

	const int t_height = maxs[1] - mins[1] + 1;
	const int s_width = maxs[0] - mins[0] + 1;

	for (t = mins[1] + HALF_SKY_SUBDIVISIONS; t <= maxs[1] + HALF_SKY_SUBDIVISIONS; t++)
	{
		for (s = mins[0] + HALF_SKY_SUBDIVISIONS; s <= maxs[0] + HALF_SKY_SUBDIVISIONS; s++)
		{
			VectorAdd(s_sky_points[t][s], backEnd.viewParms.ori.origin, tess.xyz[tess.numVertexes]);
			tess.texCoords[tess.numVertexes][0][0] = s_sky_tex_coords[t][s][0];
			tess.texCoords[tess.numVertexes][0][1] = s_sky_tex_coords[t][s][1];

			tess.numVertexes++;

			if (tess.numVertexes >= SHADER_MAX_VERTEXES)
			{
				Com_Error(ERR_DROP, "SHADER_MAX_VERTEXES hit in FillCloudySkySide()\n");
			}
		}
	}

	// only add indexes for one pass, otherwise it would draw multiple times for each pass
	if (add_indexes) {
		for (t = 0; t < t_height - 1; t++)
		{
			for (s = 0; s < s_width - 1; s++)
			{
				tess.indexes[tess.num_indexes] = vertex_start + s + t * (s_width);
				tess.num_indexes++;
				tess.indexes[tess.num_indexes] = vertex_start + s + (t + 1) * (s_width);
				tess.num_indexes++;
				tess.indexes[tess.num_indexes] = vertex_start + s + 1 + t * (s_width);
				tess.num_indexes++;

				tess.indexes[tess.num_indexes] = vertex_start + s + (t + 1) * (s_width);
				tess.num_indexes++;
				tess.indexes[tess.num_indexes] = vertex_start + s + 1 + (t + 1) * (s_width);
				tess.num_indexes++;
				tess.indexes[tess.num_indexes] = vertex_start + s + 1 + t * (s_width);
				tess.num_indexes++;
			}
		}
	}
}

static void FillCloudBox(const int stage)
{
	for (int i = 0; i < 6; i++)
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		float min_t;

		if (true)
		{
			min_t = -HALF_SKY_SUBDIVISIONS;

			// still don't want to draw the bottom, even if fullClouds
			if (i == 5)
				continue;
		}

		sky_mins[0][i] = floor(sky_mins[0][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor(sky_mins[1][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil(sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil(sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;

		if ((sky_mins[0][i] >= sky_maxs[0][i]) ||
			(sky_mins[1][i] >= sky_maxs[1][i]))
		{
			continue;
		}

		sky_mins_subd[0] = Q_ftol(sky_mins[0][i] * HALF_SKY_SUBDIVISIONS);
		sky_mins_subd[1] = Q_ftol(sky_mins[1][i] * HALF_SKY_SUBDIVISIONS);
		sky_maxs_subd[0] = Q_ftol(sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS);
		sky_maxs_subd[1] = Q_ftol(sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS);

		if (sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if (sky_mins_subd[1] < min_t)
			sky_mins_subd[1] = min_t;
		else if (sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS)
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if (sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if (sky_maxs_subd[1] < min_t)
			sky_maxs_subd[1] = min_t;
		else if (sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS)
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		//
		// iterate through the subdivisions
		//
		for (int t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++)
		{
			for (int s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++)
			{
				MakeSkyVec((s - HALF_SKY_SUBDIVISIONS) / static_cast<float>(HALF_SKY_SUBDIVISIONS),
					(t - HALF_SKY_SUBDIVISIONS) / static_cast<float>(HALF_SKY_SUBDIVISIONS),
					i,
					nullptr,
					s_sky_points[t][s]);

				s_sky_tex_coords[t][s][0] = s_cloudTexCoords[i][t][s][0];
				s_sky_tex_coords[t][s][1] = s_cloudTexCoords[i][t][s][1];
			}
		}

		// only add indexes for first stage
		FillCloudySkySide(sky_mins_subd, sky_maxs_subd, static_cast<qboolean>(stage == 0));
	}
}

/*
** R_BuildCloudData
*/
void R_BuildCloudData(shaderCommands_t* input) {
	assert(input->shader->sky);

	sky_min = 1.0 / 256.0f;		// FIXME: not correct?
	sky_max = 255.0 / 256.0f;

	// set up for drawing
	tess.num_indexes = 0;
	tess.numVertexes = 0;

	if (input->shader->sky->cloudHeight) {
		for (int i = 0; i < input->shader->numUnfoggedPasses; i++)
			FillCloudBox(i);
	}
}

/*
** R_InitSkyTexCoords
** Called when a sky shader is parsed
*/
#define SQR( a ) ((a)*(a))
void R_InitSkyTexCoords(const float height_cloud)
{
	vec3_t v;

	// init zfar so MakeSkyVec works even though
	// a world hasn't been bounded
	backEnd.viewParms.zFar = 1024;

	for (int i = 0; i < 6; i++)
	{
		for (int t = 0; t <= SKY_SUBDIVISIONS; t++)
		{
			for (int s = 0; s <= SKY_SUBDIVISIONS; s++)
			{
				vec3_t sky_vec;
				constexpr float radius_world = 4096;
				// compute vector from view origin to sky side integral point
				MakeSkyVec((s - HALF_SKY_SUBDIVISIONS) / static_cast<float>(HALF_SKY_SUBDIVISIONS),
					(t - HALF_SKY_SUBDIVISIONS) / static_cast<float>(HALF_SKY_SUBDIVISIONS),
					i,
					nullptr,
					sky_vec);

				// compute parametric value 'p' that intersects with cloud layer
				const float p = (1.0f / (2 * DotProduct(sky_vec, sky_vec))) *
					(-2 * sky_vec[2] * radius_world +
						2 * sqrt(SQR(sky_vec[2]) * SQR(radius_world) +
							2 * SQR(sky_vec[0]) * radius_world * height_cloud +
							SQR(sky_vec[0]) * SQR(height_cloud) +
							2 * SQR(sky_vec[1]) * radius_world * height_cloud +
							SQR(sky_vec[1]) * SQR(height_cloud) +
							2 * SQR(sky_vec[2]) * radius_world * height_cloud +
							SQR(sky_vec[2]) * SQR(height_cloud)));

				s_cloudTexP[i][t][s] = p;

				// compute intersection point based on p
				VectorScale(sky_vec, p, v);
				v[2] += radius_world;

				// compute vector from world origin to intersection point 'v'
				VectorNormalize(v);

				const float s_rad = Q_acos(v[0]);
				const float t_rad = Q_acos(v[1]);

				s_cloudTexCoords[i][t][s][0] = s_rad;
				s_cloudTexCoords[i][t][s][1] = t_rad;
			}
		}
	}
}

//======================================================================================

/*
** RB_DrawSun
*/
void RB_DrawSun(void) {
	vec3_t		origin, vec1, vec2;
	vec3_t		temp;

	if (!backEnd.skyRenderedThisView) {
		return;
	}
	if (!r_drawSun->integer) {
		return;
	}
	qglLoadMatrixf(backEnd.viewParms.world.model_matrix);
	qglTranslatef(backEnd.viewParms.ori.origin[0], backEnd.viewParms.ori.origin[1], backEnd.viewParms.ori.origin[2]);

	const float dist = backEnd.viewParms.zFar / 1.75;		// div sqrt(3)
	const float size = dist * 0.4;

	VectorScale(tr.sunDirection, dist, origin);
	PerpendicularVector(vec1, tr.sunDirection);
	CrossProduct(tr.sunDirection, vec1, vec2);

	VectorScale(vec1, size, vec1);
	VectorScale(vec2, size, vec2);

	// farthest depth range
	qglDepthRange(1.0, 1.0);

	// FIXME: use quad stamp
	RB_BeginSurface(tr.sunShader, tess.fogNum);
	VectorCopy(origin, temp);
	VectorSubtract(temp, vec1, temp);
	VectorSubtract(temp, vec2, temp);
	VectorCopy(temp, tess.xyz[tess.numVertexes]);
	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = 255;
	tess.vertexColors[tess.numVertexes][1] = 255;
	tess.vertexColors[tess.numVertexes][2] = 255;
	tess.numVertexes++;

	VectorCopy(origin, temp);
	VectorAdd(temp, vec1, temp);
	VectorSubtract(temp, vec2, temp);
	VectorCopy(temp, tess.xyz[tess.numVertexes]);
	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = 1;
	tess.vertexColors[tess.numVertexes][0] = 255;
	tess.vertexColors[tess.numVertexes][1] = 255;
	tess.vertexColors[tess.numVertexes][2] = 255;
	tess.numVertexes++;

	VectorCopy(origin, temp);
	VectorAdd(temp, vec1, temp);
	VectorAdd(temp, vec2, temp);
	VectorCopy(temp, tess.xyz[tess.numVertexes]);
	tess.texCoords[tess.numVertexes][0][0] = 1;
	tess.texCoords[tess.numVertexes][0][1] = 1;
	tess.vertexColors[tess.numVertexes][0] = 255;
	tess.vertexColors[tess.numVertexes][1] = 255;
	tess.vertexColors[tess.numVertexes][2] = 255;
	tess.numVertexes++;

	VectorCopy(origin, temp);
	VectorSubtract(temp, vec1, temp);
	VectorAdd(temp, vec2, temp);
	VectorCopy(temp, tess.xyz[tess.numVertexes]);
	tess.texCoords[tess.numVertexes][0][0] = 1;
	tess.texCoords[tess.numVertexes][0][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = 255;
	tess.vertexColors[tess.numVertexes][1] = 255;
	tess.vertexColors[tess.numVertexes][2] = 255;
	tess.numVertexes++;

	tess.indexes[tess.num_indexes++] = 0;
	tess.indexes[tess.num_indexes++] = 1;
	tess.indexes[tess.num_indexes++] = 2;
	tess.indexes[tess.num_indexes++] = 0;
	tess.indexes[tess.num_indexes++] = 2;
	tess.indexes[tess.num_indexes++] = 3;

	RB_EndSurface();

	// back to normal depth range
	qglDepthRange(0.0, 1.0);
}

/*
================
RB_StageIteratorSky

All of the visible sky triangles are in tess

Other things could be stuck in here, like birds in the sky, etc
================
*/
void RB_StageIteratorSky(void)
{
	if (g_bRenderGlowingObjects)
		return;

	if (r_fastsky->integer) {
		return;
	}

	if (skyboxportal && !(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL))
	{
		return;
	}

	// go through all the polygons and project them onto
	// the sky box to see which blocks on each side need
	// to be drawn
	RB_ClipSkyPolygons(&tess);

	// r_showsky will let all the sky blocks be drawn in
	// front of everything to allow developers to see how
	// much sky is getting sucked in
	if (r_showsky->integer) {
		qglDepthRange(0.0, 0.0);
	}
	else {
		qglDepthRange(1.0, 1.0);
	}

	// draw the outer skybox
	if (tess.shader->sky->outerbox[0] && tess.shader->sky->outerbox[0] != tr.defaultImage) {
		qglColor3f(tr.identityLight, tr.identityLight, tr.identityLight);

		qglPushMatrix();
		GL_State(0);
		qglTranslatef(backEnd.viewParms.ori.origin[0], backEnd.viewParms.ori.origin[1], backEnd.viewParms.ori.origin[2]);

		DrawSkyBox(tess.shader);

		qglPopMatrix();
	}

	// generate the vertexes for all the clouds, which will be drawn
	// by the generic shader routine
	R_BuildCloudData(&tess);

	if (tess.num_indexes && tess.numVertexes)
	{
		RB_StageIteratorGeneric();
	}

	// draw the inner skybox

	// back to normal depth range
	qglDepthRange(0.0, 1.0);

	// note that sky was drawn so we will draw a sun later
	backEnd.skyRenderedThisView = qtrue;
}