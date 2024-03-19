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

// tr_map.c
#include "tr_local.h"

/*

Loads and prepares a map file for scene rendering.

A single entry point:

void RE_LoadWorldMap( const char *name );

*/

static	world_t		s_worldData;
static	byte* fileBase;

int			c_subdivisions;
int			c_gridVerts;

//===============================================================================

static void HSVtoRGB(float h, float s, float v, float rgb[3])
{
	h *= 5;

	const int i = floor(h);
	const float f = h - i;

	const float p = v * (1 - s);
	const float q = v * (1 - s * f);
	const float t = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:
		rgb[0] = v;
		rgb[1] = t;
		rgb[2] = p;
		break;
	case 1:
		rgb[0] = q;
		rgb[1] = v;
		rgb[2] = p;
		break;
	case 2:
		rgb[0] = p;
		rgb[1] = v;
		rgb[2] = t;
		break;
	case 3:
		rgb[0] = p;
		rgb[1] = q;
		rgb[2] = v;
		break;
	case 4:
		rgb[0] = t;
		rgb[1] = p;
		rgb[2] = v;
		break;
	case 5:
		rgb[0] = v;
		rgb[1] = p;
		rgb[2] = q;
		break;
	default:;
	}
}

/*
===============
R_ColorShiftLightingBytes

===============
*/
void R_ColorShiftLightingBytes(byte in[4], byte out[4]) {
	// shift the color data based on overbright range
	const int shift = Q_max(0, r_mapOverBrightBits->integer - tr.overbrightBits);

	// shift the data based on overbright range
	int r = in[0] << shift;
	int g = in[1] << shift;
	int b = in[2] << shift;

	// normalize by color instead of saturating to white
	if ((r | g | b) > 255) {
		int max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in[3];
}

/*
===============
R_ColorShiftLightingBytes

===============
*/
void R_ColorShiftLightingBytes(byte in[3]) {
	// shift the color data based on overbright range
	const int shift = Q_max(0, r_mapOverBrightBits->integer - tr.overbrightBits);

	// shift the data based on overbright range
	int r = in[0] << shift;
	int g = in[1] << shift;
	int b = in[2] << shift;

	// normalize by color instead of saturating to white
	if ((r | g | b) > 255) {
		int max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	in[0] = r;
	in[1] = g;
	in[2] = b;
}

/*
===============
R_LoadLightmaps

===============
*/
#define	LIGHTMAP_SIZE	128
static	void R_LoadLightmaps(lump_t* l, const char* psMapName, world_t& world_data) {
	byte		image[LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4];
	int j;
	float maxIntensity = 0;
	double sumIntensity = 0;

	if (&world_data == &s_worldData)
	{
		tr.numLightmaps = 0;
	}

	const int len = l->filelen;
	if (!len) {
		return;
	}
	byte* buf = fileBase + l->fileofs;

	// we are about to upload textures
	R_IssuePendingRenderCommands();

	// create all the lightmaps
	tr.numLightmaps = len / (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
	if (tr.numLightmaps == 1) {
		//FIXME: HACK: maps with only one lightmap turn up fullbright for some reason.
		//this avoids this, but isn't the correct solution.
		tr.numLightmaps++;
	}

	// if we are in r_vertexLight mode, we don't need the lightmaps at all
	if (r_vertexLight->integer) {
		return;
	}

	char sMapName[MAX_QPATH];
	COM_StripExtension(psMapName, sMapName, sizeof(sMapName));

	for (int i = 0; i < tr.numLightmaps; i++) {
		// expand the 24 bit on-disk to 32 bit
		byte* buf_p = buf + i * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3;

		if (r_lightmap->integer == 2)
		{	// color code by intensity as development tool	(FIXME: check range)
			for (j = 0; j < LIGHTMAP_SIZE * LIGHTMAP_SIZE; j++)
			{
				const float r = buf_p[j * 3 + 0];
				const float g = buf_p[j * 3 + 1];
				const float b = buf_p[j * 3 + 2];
				float out[3] = { 0.0f, 0.0f, 0.0f };

				float intensity = 0.33f * r + 0.685f * g + 0.063f * b;

				if (intensity > 255)
					intensity = 1.0f;
				else
					intensity /= 255.0f;

				if (intensity > maxIntensity)
					maxIntensity = intensity;

				HSVtoRGB(intensity, 1.00, 0.50, out);

				image[j * 4 + 0] = out[0] * 255;
				image[j * 4 + 1] = out[1] * 255;
				image[j * 4 + 2] = out[2] * 255;
				image[j * 4 + 3] = 255;

				sumIntensity += intensity;
			}
		}
		else {
			for (j = 0; j < LIGHTMAP_SIZE * LIGHTMAP_SIZE; j++) {
				R_ColorShiftLightingBytes(&buf_p[j * 3], &image[j * 4]);
				image[j * 4 + 3] = 255;
			}
		}
		tr.lightmaps[i] = R_CreateImage(va("*%s/lightmap%d", sMapName, i), image,
			LIGHTMAP_SIZE, LIGHTMAP_SIZE, GL_RGBA, qfalse, qfalse, static_cast<qboolean>(r_ext_compressed_lightmaps->integer), GL_CLAMP);
	}

	if (r_lightmap->integer == 2) {
		ri->Printf(PRINT_ALL, "Brightest lightmap value: %d\n", static_cast<int>(maxIntensity * 255));
	}
}

/*
=================
RE_SetWorldVisData

This is called by the clipmodel subsystem so we can share the 1.8 megs of
space in big maps...
=================
*/
void		RE_SetWorldVisData(const byte* vis) {
	tr.externalVisData = vis;
}

/*
=================
R_LoadVisibility
=================
*/
static	void R_LoadVisibility(lump_t* l, world_t& world_data) {
	int len = (world_data.numClusters + 63) & ~63;
	world_data.novis = static_cast<unsigned char*>(Hunk_Alloc(len, h_low));
	memset(world_data.novis, 0xff, len);

	len = l->filelen;
	if (!len) {
		return;
	}
	byte* buf = fileBase + l->fileofs;

	world_data.numClusters = LittleLong(((int*)buf)[0]);
	world_data.clusterBytes = LittleLong(((int*)buf)[1]);

	// CM_Load should have given us the vis data to share, so
	// we don't need to allocate another copy
	if (tr.externalVisData) {
		world_data.vis = tr.externalVisData;
	}
	else {
		byte* dest = static_cast<unsigned char*>(Hunk_Alloc(len - 8, h_low));
		memcpy(dest, buf + 8, len - 8);
		world_data.vis = dest;
	}
}

//===============================================================================

/*
===============
ShaderForShaderNum
===============
*/
static shader_t* ShaderForShaderNum(int shader_num, const int* lightmap_num, const byte* lightmapStyles, const byte* vertexStyles, world_t& world_data)
{
	const byte* styles = lightmapStyles;

	LL(shader_num);
	if (shader_num < 0 || shader_num >= world_data.numShaders) {
		Com_Error(ERR_DROP, "ShaderForShaderNum: bad num %i", shader_num);
	}
	const dshader_t* dsh = &world_data.shaders[shader_num];

	if (lightmap_num[0] == LIGHTMAP_BY_VERTEX)
	{
		styles = vertexStyles;
	}

	if (r_vertexLight->integer)
	{
		lightmap_num = lightmapsVertex;
		styles = vertexStyles;
	}

	shader_t* shader = R_FindShader(dsh->shader, lightmap_num, styles, qtrue);

	// if the shader had errors, just use default shader
	if (shader->defaultShader) {
		return tr.defaultShader;
	}

	return shader;
}

/*
===============
ParseFace
===============
*/
static void ParseFace(dsurface_t* ds, mapVert_t* verts, msurface_t* surf, int* indexes, world_t& world_data, int index) {
	int					i, j, k;
	int					lightmap_num[MAXLIGHTMAPS];

	for (i = 0; i < MAXLIGHTMAPS; i++)
	{
		lightmap_num[i] = LittleLong(ds->lightmap_num[i]);
	}

	// get fog volume
	surf->fogIndex = LittleLong(ds->fogNum) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = world_data.globalFog;
	}

	// get shader value
	surf->shader = ShaderForShaderNum(ds->shader_num, lightmap_num, ds->lightmapStyles, ds->vertexStyles, world_data);
	if (r_singleShader->integer && !surf->shader->sky) {
		surf->shader = tr.defaultShader;
	}

	int numPoints = (ds->num_verts);
	if (numPoints > MAX_FACE_POINTS) {
		ri->Printf(PRINT_ALL, S_COLOR_YELLOW  "WARNING: MAX_FACE_POINTS exceeded: %i\n", numPoints);
		numPoints = MAX_FACE_POINTS;
		surf->shader = tr.defaultShader;
	}

	const int num_indexes = (ds->num_indexes);

	// create the srfSurfaceFace_t
	int sfaceSize = (size_t) & static_cast<srfSurfaceFace_t*>(nullptr)->points[numPoints];
	const int ofsIndexes = sfaceSize;
	sfaceSize += sizeof(int) * num_indexes;

	srfSurfaceFace_t* cv = static_cast<srfSurfaceFace_t*>(Hunk_Alloc(sfaceSize, h_low));
	cv->surfaceType = SF_FACE;
	cv->numPoints = numPoints;
	cv->numIndices = num_indexes;
	cv->ofsIndices = ofsIndexes;

	verts += LittleLong(ds->firstVert);
	for (i = 0; i < numPoints; i++) {
		for (j = 0; j < 3; j++) {
			cv->points[i][j] = LittleFloat(verts[i].xyz[j]);
		}
		for (j = 0; j < 2; j++) {
			cv->points[i][3 + j] = LittleFloat(verts[i].st[j]);
			for (k = 0; k < MAXLIGHTMAPS; k++)
			{
				cv->points[i][VERTEX_LM + j + (k * 2)] = LittleFloat(verts[i].lightmap[k][j]);
			}
		}
		for (k = 0; k < MAXLIGHTMAPS; k++)
		{
			R_ColorShiftLightingBytes(verts[i].color[k], (byte*)&cv->points[i][VERTEX_COLOR + k]);
		}
	}

	indexes += LittleLong(ds->firstIndex);
	for (i = 0; i < num_indexes; i++) {
		((int*)((byte*)cv + cv->ofsIndices))[i] = LittleLong(indexes[i]);
	}

	// take the plane information from the lightmap vector
	for (i = 0; i < 3; i++) {
		cv->plane.normal[i] = LittleFloat(ds->lightmapVecs[2][i]);
	}
	cv->plane.dist = DotProduct(cv->points[0], cv->plane.normal);
	SetPlaneSignbits(&cv->plane);
	cv->plane.type = PlaneTypeForNormal(cv->plane.normal);

	surf->data = (surfaceType_t*)cv;
}

/*
===============
ParseMesh
===============
*/
static void ParseMesh(dsurface_t* ds, mapVert_t* verts, msurface_t* surf, world_t& world_data, int index) {
	int						i, j, k;
	drawVert_t				points[MAX_PATCH_SIZE * MAX_PATCH_SIZE];
	int						lightmap_num[MAXLIGHTMAPS];
	vec3_t					bounds[2];
	vec3_t					tmpVec;
	static surfaceType_t	skipData = SF_SKIP;

	for (i = 0; i < MAXLIGHTMAPS; i++)
	{
		lightmap_num[i] = LittleLong(ds->lightmap_num[i]);
	}

	// get fog volume
	surf->fogIndex = LittleLong(ds->fogNum) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = world_data.globalFog;
	}

	// get shader value
	surf->shader = ShaderForShaderNum(ds->shader_num, lightmap_num, ds->lightmapStyles, ds->vertexStyles, world_data);
	if (r_singleShader->integer && !surf->shader->sky) {
		surf->shader = tr.defaultShader;
	}

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if (world_data.shaders[LittleLong(ds->shader_num)].surfaceFlags & SURF_NODRAW) {
		surf->data = &skipData;
		return;
	}

	const int width = (ds->patchWidth);
	const int height = (ds->patchHeight);

	verts += LittleLong(ds->firstVert);
	const int numPoints = width * height;
	for (i = 0; i < numPoints; i++) {
		for (j = 0; j < 3; j++) {
			points[i].xyz[j] = LittleFloat(verts[i].xyz[j]);
			points[i].normal[j] = LittleFloat(verts[i].normal[j]);
		}
		for (j = 0; j < 2; j++) {
			points[i].st[j] = LittleFloat(verts[i].st[j]);
			for (k = 0; k < MAXLIGHTMAPS; k++)
			{
				points[i].lightmap[k][j] = LittleFloat(verts[i].lightmap[k][j]);
			}
		}
		for (k = 0; k < MAXLIGHTMAPS; k++)
		{
			R_ColorShiftLightingBytes(verts[i].color[k], points[i].color[k]);
		}
	}

	// pre-tesseleate
	srfGridMesh_t* grid = R_SubdividePatchToGrid(width, height, points);
	surf->data = (surfaceType_t*)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking
	for (i = 0; i < 3; i++) {
		bounds[0][i] = LittleFloat(ds->lightmapVecs[0][i]);
		bounds[1][i] = LittleFloat(ds->lightmapVecs[1][i]);
	}
	VectorAdd(bounds[0], bounds[1], bounds[1]);
	VectorScale(bounds[1], 0.5f, grid->lodOrigin);
	VectorSubtract(bounds[0], grid->lodOrigin, tmpVec);
	grid->lodRadius = VectorLength(tmpVec);
}

/*
===============
ParseTriSurf
===============
*/
static void ParseTriSurf(dsurface_t* ds, mapVert_t* verts, msurface_t* surf, int* indexes, world_t& world_data, int index) {
	int				i, j, k;

	// get fog volume
	surf->fogIndex = LittleLong(ds->fogNum) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = world_data.globalFog;
	}

	// get shader
	surf->shader = ShaderForShaderNum(ds->shader_num, lightmapsVertex, ds->lightmapStyles, ds->vertexStyles, world_data);
	if (r_singleShader->integer && !surf->shader->sky) {
		surf->shader = tr.defaultShader;
	}

	const int num_verts = (ds->num_verts);
	const int num_indexes = (ds->num_indexes);

	if (num_verts >= SHADER_MAX_VERTEXES) {
		Com_Error(ERR_DROP, "ParseTriSurf: verts > MAX (%d > %d) on misc_model %s", num_verts, SHADER_MAX_VERTEXES, surf->shader->name);
	}
	if (num_indexes >= SHADER_MAX_INDEXES) {
		Com_Error(ERR_DROP, "ParseTriSurf: indices > MAX (%d > %d) on misc_model %s", num_indexes, SHADER_MAX_INDEXES, surf->shader->name);
	}

	srfTriangles_t* tri = static_cast<srfTriangles_t*>(Hunk_Alloc(sizeof(*tri) + num_verts * sizeof(tri->verts[0])
		+ num_indexes * sizeof(tri->indexes[0]), h_low));
	tri->surfaceType = SF_TRIANGLES;
	tri->num_verts = num_verts;
	tri->num_indexes = num_indexes;
	tri->verts = (drawVert_t*)(tri + 1);
	tri->indexes = (int*)(tri->verts + tri->num_verts);

	surf->data = (surfaceType_t*)tri;

	// copy vertexes
	ClearBounds(tri->bounds[0], tri->bounds[1]);
	verts += LittleLong(ds->firstVert);
	for (i = 0; i < num_verts; i++) {
		for (j = 0; j < 3; j++) {
			tri->verts[i].xyz[j] = LittleFloat(verts[i].xyz[j]);
			tri->verts[i].normal[j] = LittleFloat(verts[i].normal[j]);
		}
		AddPointToBounds(tri->verts[i].xyz, tri->bounds[0], tri->bounds[1]);
		for (j = 0; j < 2; j++) {
			tri->verts[i].st[j] = LittleFloat(verts[i].st[j]);
			for (k = 0; k < MAXLIGHTMAPS; k++)
			{
				tri->verts[i].lightmap[k][j] = LittleFloat(verts[i].lightmap[k][j]);
			}
		}

		for (k = 0; k < MAXLIGHTMAPS; k++)
		{
			R_ColorShiftLightingBytes(verts[i].color[k], tri->verts[i].color[k]);
		}
	}

	// copy indexes
	indexes += LittleLong(ds->firstIndex);
	for (i = 0; i < num_indexes; i++) {
		tri->indexes[i] = LittleLong(indexes[i]);
		if (tri->indexes[i] < 0 || tri->indexes[i] >= num_verts) {
			Com_Error(ERR_DROP, "Bad index in triangle surface");
		}
	}
}

/*
===============
ParseFlare
===============
*/
static void ParseFlare(dsurface_t* ds, mapVert_t* verts, msurface_t* surf, int* indexes, world_t& world_data, int index) {
	const int				lightmaps[MAXLIGHTMAPS] = { LIGHTMAP_BY_VERTEX };

	// get fog volume
	surf->fogIndex = LittleLong(ds->fogNum) + 1;
	if (index && !surf->fogIndex && tr.world->globalFog != -1)
	{
		surf->fogIndex = world_data.globalFog;
	}

	// get shader
	surf->shader = ShaderForShaderNum(ds->shader_num, lightmaps, ds->lightmapStyles, ds->vertexStyles, world_data);
	if (r_singleShader->integer && !surf->shader->sky) {
		surf->shader = tr.defaultShader;
	}

	srfFlare_t* flare = static_cast<srfFlare_s*>(Hunk_Alloc(sizeof(*flare), h_low));
	flare->surfaceType = SF_FLARE;

	surf->data = (surfaceType_t*)flare;

	for (int i = 0; i < 3; i++) {
		flare->origin[i] = LittleFloat(ds->lightmapOrigin[i]);
		flare->color[i] = LittleFloat(ds->lightmapVecs[0][i]);
		flare->normal[i] = LittleFloat(ds->lightmapVecs[2][i]);
	}
}

/*
=================
R_MergedWidthPoints

returns true if there are grid points merged on a width edge
=================
*/
int R_MergedWidthPoints(srfGridMesh_t* grid, int offset) {
	for (int i = 1; i < grid->width - 1; i++) {
		for (int j = i + 1; j < grid->width - 1; j++) {
			if (fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
			if (fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
			if (fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_MergedHeightPoints

returns true if there are grid points merged on a height edge
=================
*/
int R_MergedHeightPoints(srfGridMesh_t* grid, int offset) {
	for (int i = 1; i < grid->height - 1; i++) {
		for (int j = i + 1; j < grid->height - 1; j++) {
			if (fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
			if (fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
			if (fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_FixSharedVertexLodError_r

NOTE: never sync LoD through grid edges with merged points!

FIXME: write generalized version that also avoids cracks between a patch and one that meets half way?
=================
*/
void R_FixSharedVertexLodError_r(int start, srfGridMesh_t* grid1, world_t& world_data) {
	int k, l, m, n, offset1, offset2;

	for (int j = start; j < world_data.numsurfaces; j++) {
		//
		srfGridMesh_t* grid2 = (srfGridMesh_t*)world_data.surfaces[j].data;
		// if this surface is not a grid
		if (grid2->surfaceType != SF_GRID) continue;
		// if the LOD errors are already fixed for this patch
		if (grid2->lodFixed == 2) continue;
		// grids in the same LOD group should have the exact same lod radius
		if (grid1->lodRadius != grid2->lodRadius) continue;
		// grids in the same LOD group should have the exact same lod origin
		if (grid1->lodOrigin[0] != grid2->lodOrigin[0]) continue;
		if (grid1->lodOrigin[1] != grid2->lodOrigin[1]) continue;
		if (grid1->lodOrigin[2] != grid2->lodOrigin[2]) continue;
		//
		int touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height - 1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width - 1; k++) {
				for (m = 0; m < 2; m++) {
					if (m) offset2 = (grid2->height - 1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for (l = 1; l < grid2->width - 1; l++) {
						//
						if (fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if (fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if (fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {
					if (m) offset2 = grid2->width - 1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for (l = 1; l < grid2->height - 1; l++) {
						//
						if (fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if (fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if (fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width - 1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height - 1; k++) {
				for (m = 0; m < 2; m++) {
					if (m) offset2 = (grid2->height - 1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for (l = 1; l < grid2->width - 1; l++) {
						//
						if (fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if (fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if (fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {
					if (m) offset2 = grid2->width - 1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for (l = 1; l < grid2->height - 1; l++) {
						//
						if (fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if (fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if (fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			grid2->lodFixed = 2;
			R_FixSharedVertexLodError_r(start, grid2, world_data);
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}

/*
=================
R_FixSharedVertexLodError

This function assumes that all patches in one group are nicely stitched together for the highest LoD.
If this is not the case this function will still do its job but won't fix the highest LoD cracks.
=================
*/
void R_FixSharedVertexLodError(world_t& world_data) {
	for (int i = 0; i < world_data.numsurfaces; i++) {
		//
		srfGridMesh_t* grid1 = (srfGridMesh_t*)world_data.surfaces[i].data;
		// if this surface is not a grid
		if (grid1->surfaceType != SF_GRID)
			continue;
		//
		if (grid1->lodFixed)
			continue;
		//
		grid1->lodFixed = 2;
		// recursively fix other patches in the same LOD group
		R_FixSharedVertexLodError_r(i + 1, grid1, world_data);
	}
}

/*
===============
R_StitchPatches
===============
*/
int R_StitchPatches(int grid1num, int grid2num, world_t& world_data) {
	int k, l, m, n, offset1, offset2, row, column;
	srfGridMesh_t* grid1, * grid2;
	float* v1, * v2;

	grid1 = (srfGridMesh_t*)world_data.surfaces[grid1num].data;
	grid2 = (srfGridMesh_t*)world_data.surfaces[grid2num].data;
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height - 1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->width - 2; k += 2) {
			for (m = 0; m < 2; m++) {
				if (grid2->width >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = (grid2->height - 1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->width - 1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height - 1;
					else row = 0;
					grid2 = R_GridInsertColumn(grid2, l + 1, row,
						grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {
				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width - 1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->height - 1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width - 1;
					else column = 0;
					grid2 = R_GridInsertRow(grid2, l + 1, column,
						grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width - 1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->height - 2; k += 2) {
			for (m = 0; m < 2; m++) {
				if (grid2->width >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = (grid2->height - 1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->width - 1; l++) {
					//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height - 1;
					else row = 0;
					grid2 = R_GridInsertColumn(grid2, l + 1, row,
						grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {
				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width - 1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->height - 1; l++) {
					//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width - 1;
					else column = 0;
					grid2 = R_GridInsertRow(grid2, l + 1, column,
						grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height - 1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = grid1->width - 1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {
				if (grid2->width >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = (grid2->height - 1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->width - 1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height - 1;
					else row = 0;
					grid2 = R_GridInsertColumn(grid2, l + 1, row,
						grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {
				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width - 1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->height - 1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width - 1;
					else column = 0;
					grid2 = R_GridInsertRow(grid2, l + 1, column,
						grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k + 1]);
					if (!grid2)
						break;
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width - 1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = grid1->height - 1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {
				if (grid2->width >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = (grid2->height - 1) * grid2->width;
				else offset2 = 0;
				//if (R_MergedWidthPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->width - 1; l++) {
					//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height - 1;
					else row = 0;
					grid2 = R_GridInsertColumn(grid2, l + 1, row,
						grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {
				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width - 1;
				else offset2 = 0;
				//if (R_MergedHeightPoints(grid2, offset2))
				//	continue;
				for (l = 0; l < grid2->height - 1; l++) {
					//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) > .1)
						continue;
					if (fabs(v1[1] - v2[1]) > .1)
						continue;
					if (fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if (fabs(v1[0] - v2[0]) < .01 &&
						fabs(v1[1] - v2[1]) < .01 &&
						fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri->Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width - 1;
					else column = 0;
					grid2 = R_GridInsertRow(grid2, l + 1, column,
						grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k + 1]);
					grid2->lodStitched = qfalse;
					world_data.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
===============
R_TryStitchPatch

This function will try to stitch patches in the same LoD group together for the highest LoD.

Only single missing vertice cracks will be fixed.

Vertices will be joined at the patch side a crack is first found, at the other side
of the patch (on the same row or column) the vertices will not be joined and cracks
might still appear at that side.
===============
*/
int R_TryStitchingPatch(int grid1num, world_t& world_data) {
	int numstitches = 0;
	const srfGridMesh_t* grid1 = (srfGridMesh_t*)world_data.surfaces[grid1num].data;
	for (int j = 0; j < world_data.numsurfaces; j++) {
		//
		const srfGridMesh_t* grid2 = (srfGridMesh_t*)world_data.surfaces[j].data;
		// if this surface is not a grid
		if (grid2->surfaceType != SF_GRID) continue;
		// grids in the same LOD group should have the exact same lod radius
		if (grid1->lodRadius != grid2->lodRadius) continue;
		// grids in the same LOD group should have the exact same lod origin
		if (grid1->lodOrigin[0] != grid2->lodOrigin[0]) continue;
		if (grid1->lodOrigin[1] != grid2->lodOrigin[1]) continue;
		if (grid1->lodOrigin[2] != grid2->lodOrigin[2]) continue;
		//
		while (R_StitchPatches(grid1num, j, world_data))
		{
			numstitches++;
		}
	}
	return numstitches;
}

/*
===============
R_StitchAllPatches
===============
*/
void R_StitchAllPatches(world_t& world_data) {
	int stitched;

	int numstitches = 0;
	do
	{
		stitched = qfalse;
		for (int i = 0; i < world_data.numsurfaces; i++) {
			//
			srfGridMesh_t* grid1 = (srfGridMesh_t*)world_data.surfaces[i].data;
			// if this surface is not a grid
			if (grid1->surfaceType != SF_GRID)
				continue;
			//
			if (grid1->lodStitched)
				continue;
			//
			grid1->lodStitched = qtrue;
			stitched = qtrue;
			//
			numstitches += R_TryStitchingPatch(i, world_data);
		}
	} while (stitched);
	//	ri->Printf( PRINT_ALL, "stitched %d LoD cracks\n", numstitches );
}

/*
===============
R_MovePatchSurfacesToHunk
===============
*/
void R_MovePatchSurfacesToHunk(world_t& world_data) {
	for (int i = 0; i < world_data.numsurfaces; i++) {
		//
		srfGridMesh_t* grid = (srfGridMesh_t*)world_data.surfaces[i].data;
		// if this surface is not a grid
		if (grid->surfaceType != SF_GRID)
			continue;
		//
		const int size = (grid->width * grid->height - 1) * sizeof(drawVert_t) + sizeof(*grid);
		srfGridMesh_t* hunkgrid = static_cast<srfGridMesh_s*>(Hunk_Alloc(size, h_low));
		memcpy(hunkgrid, grid, size);

		hunkgrid->widthLodError = static_cast<float*>(Hunk_Alloc(grid->width * 4, h_low));
		memcpy(hunkgrid->widthLodError, grid->widthLodError, grid->width * 4);

		hunkgrid->heightLodError = static_cast<float*>(Hunk_Alloc(grid->height * 4, h_low));
		memcpy(grid->heightLodError, grid->heightLodError, grid->height * 4);

		R_FreeSurfaceGridMesh(grid);

		world_data.surfaces[i].data = (surfaceType_t*)hunkgrid;
	}
}

/*
===============
R_LoadSurfaces
===============
*/
static	void R_LoadSurfaces(lump_t* surfs, lump_t* verts, lump_t* index_lump, world_t& world_data, int index) {
	int numFaces = 0;
	int numMeshes = 0;
	int numTriSurfs = 0;
	int numFlares = 0;

	dsurface_t* in = (dsurface_t*)(fileBase + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	const int count = surfs->filelen / sizeof(*in);

	mapVert_t* dv = (mapVert_t*)(fileBase + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);

	int* indexes = (int*)(fileBase + index_lump->fileofs);
	if (index_lump->filelen % sizeof(*indexes))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);

	msurface_t* out = static_cast<msurface_s*>(Hunk_Alloc(count * sizeof(*out), h_low));

	world_data.surfaces = out;
	world_data.numsurfaces = count;

	for (int i = 0; i < count; i++, in++, out++) {
		switch (LittleLong(in->surfaceType)) {
		case MST_PATCH:
			ParseMesh(in, dv, out, world_data, index);
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf(in, dv, out, indexes, world_data, index);
			numTriSurfs++;
			break;
		case MST_PLANAR:
			ParseFace(in, dv, out, indexes, world_data, index);
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare(in, dv, out, indexes, world_data, index);
			numFlares++;
			break;
		default:
			Com_Error(ERR_DROP, "Bad surfaceType");
		}
	}

#ifdef PATCH_STITCHING
	R_StitchAllPatches(world_data);
#endif

	R_FixSharedVertexLodError(world_data);

#ifdef PATCH_STITCHING
	R_MovePatchSurfacesToHunk(world_data);
#endif

	ri->Printf(PRINT_ALL, "...loaded %d faces, %i meshes, %i trisurfs, %i flares\n", numFaces, numMeshes, numTriSurfs, numFlares);
}

/*
=================
R_LoadSubmodels
=================
*/
static	void R_LoadSubmodels(lump_t* l, world_t& world_data, int index) {
	bmodel_t* out;

	dmodel_t* in = (dmodel_t*)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	const int count = l->filelen / sizeof(*in);

	world_data.bmodels = out = static_cast<bmodel_t*>(Hunk_Alloc(count * sizeof(*out), h_low));

	for (int i = 0; i < count; i++, in++, out++) {
		model_t* model = R_AllocModel();

		assert(model != NULL);			// this should never happen
		if (model == nullptr) {
			ri->Error(ERR_DROP, "R_LoadSubmodels: R_AllocModel() failed");
		}

		model->type = MOD_BRUSH;
		model->bmodel = out;
		if (index)
		{
			Com_sprintf(model->name, sizeof(model->name), "*%d-%d", index, i);
			model->bspInstance = qtrue;
		}
		else
		{
			Com_sprintf(model->name, sizeof(model->name), "*%d", i);
		}

		for (int j = 0; j < 3; j++) {
			out->bounds[0][j] = LittleFloat(in->mins[j]);
			out->bounds[1][j] = LittleFloat(in->maxs[j]);
		}
		/*
		Ghoul2 Insert Start
		*/

		RE_InsertModelIntoHash(model->name, model);
		/*
		Ghoul2 Insert End
		*/
		out->firstSurface = world_data.surfaces + LittleLong(in->firstSurface);
		out->numSurfaces = LittleLong(in->numSurfaces);
	}
}

//==================================================================

/*
=================
R_SetParent
=================
*/
static	void R_SetParent(mnode_t* node, mnode_t* parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	R_SetParent(node->children[0], node);
	R_SetParent(node->children[1], node);
}

/*
=================
R_LoadNodesAndLeafs
=================
*/
static	void R_LoadNodesAndLeafs(lump_t* nodeLump, lump_t* leafLump, world_t& world_data) {
	int			i, j;

	dnode_t* in = (dnode_t*)(fileBase + nodeLump->fileofs);
	if (nodeLump->filelen % sizeof(dnode_t) ||
		leafLump->filelen % sizeof(dleaf_t)) {
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	}
	const int numNodes = nodeLump->filelen / sizeof(dnode_t);
	const int numLeafs = leafLump->filelen / sizeof(dleaf_t);

	mnode_t* out = static_cast<mnode_s*>(Hunk_Alloc((numNodes + numLeafs) * sizeof(*out), h_low));

	world_data.nodes = out;
	world_data.numnodes = numNodes + numLeafs;
	world_data.numDecisionNodes = numNodes;

	// load nodes
	for (i = 0; i < numNodes; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->mins[j] = LittleLong(in->mins[j]);
			out->maxs[j] = LittleLong(in->maxs[j]);
		}

		int p = (in->planeNum);
		out->plane = world_data.planes + p;

		out->contents = CONTENTS_NODE;	// differentiate from leafs

		for (j = 0; j < 2; j++)
		{
			p = LittleLong(in->children[j]);
			if (p >= 0)
				out->children[j] = world_data.nodes + p;
			else
				out->children[j] = world_data.nodes + numNodes + (-1 - p);
		}
	}

	// load leafs
	dleaf_t* inLeaf = (dleaf_t*)(fileBase + leafLump->fileofs);
	for (i = 0; i < numLeafs; i++, inLeaf++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->mins[j] = LittleLong(inLeaf->mins[j]);
			out->maxs[j] = LittleLong(inLeaf->maxs[j]);
		}

		out->cluster = LittleLong(inLeaf->cluster);
		out->area = LittleLong(inLeaf->area);

		if (out->cluster >= world_data.numClusters) {
			world_data.numClusters = out->cluster + 1;
		}

		out->firstmarksurface = world_data.marksurfaces +
			LittleLong(inLeaf->firstLeafSurface);
		out->nummarksurfaces = LittleLong(inLeaf->numLeafSurfaces);
	}

	// chain decendants
	R_SetParent(world_data.nodes, nullptr);
}

//=============================================================================

/*
=================
R_LoadShaders
=================
*/
static	void R_LoadShaders(lump_t* l, world_t& world_data) {
	const dshader_t* in = (dshader_t*)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	const int count = l->filelen / sizeof(*in);
	dshader_t* out = static_cast<dshader_t*>(Hunk_Alloc(count * sizeof(*out), h_low));

	world_data.shaders = out;
	world_data.numShaders = count;

	memcpy(out, in, count * sizeof(*out));

	for (int i = 0; i < count; i++) {
		out[i].surfaceFlags = LittleLong(out[i].surfaceFlags);
		out[i].contentFlags = LittleLong(out[i].contentFlags);
	}
}

/*
=================
R_LoadMarksurfaces
=================
*/
static	void R_LoadMarksurfaces(lump_t* l, world_t& world_data)
{
	const int* in = (int*)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	const int count = l->filelen / sizeof(*in);
	msurface_t** out = static_cast<msurface_s**>(Hunk_Alloc(count * sizeof(*out), h_low));

	world_data.marksurfaces = out;
	world_data.nummarksurfaces = count;

	for (int i = 0; i < count; i++)
	{
		const int j = (in[i]);
		out[i] = world_data.surfaces + j;
	}
}

/*
=================
R_LoadPlanes
=================
*/
static	void R_LoadPlanes(lump_t* l, world_t& world_data) {
	dplane_t* in = (dplane_t*)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	const int count = l->filelen / sizeof(*in);
	cplane_t* out = static_cast<cplane_s*>(Hunk_Alloc(count * 2 * sizeof(*out), h_low));

	world_data.planes = out;
	world_data.numplanes = count;

	for (int i = 0; i < count; i++, in++, out++) {
		int bits = 0;
		for (int j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0) {
				bits |= 1 << j;
			}
		}

		out->dist = LittleFloat(in->dist);
		out->type = PlaneTypeForNormal(out->normal);
		out->signbits = bits;
	}
}

/*
=================
R_LoadFogs

=================
*/
static	void R_LoadFogs(lump_t* l, lump_t* brushesLump, lump_t* sidesLump, world_t& world_data, int index) {
	fog_t* out;
	int			sideNum;
	int			planeNum;
	int			firstSide = 0;
	const int			lightmaps[MAXLIGHTMAPS] = { LIGHTMAP_NONE };

	dfog_t* fogs = (dfog_t*)(fileBase + l->fileofs);
	if (l->filelen % sizeof(*fogs)) {
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	}
	const int count = l->filelen / sizeof(*fogs);

	// create fog strucutres for them
	world_data.numfogs = count + 1;
	world_data.fogs = static_cast<fog_t*>(Hunk_Alloc(world_data.numfogs * sizeof(*out), h_low));
	world_data.globalFog = -1;
	out = world_data.fogs + 1;

	// Copy the global fog from the main world into the bsp instance
	if (index)
	{
		if (tr.world && (tr.world->globalFog != -1))
		{
			// Use the nightvision fog slot
			world_data.fogs[world_data.numfogs] = tr.world->fogs[tr.world->globalFog];
			world_data.globalFog = world_data.numfogs;
			world_data.numfogs++;
		}
	}

	if (!count) {
		return;
	}

	const dbrush_t* brushes = (dbrush_t*)(fileBase + brushesLump->fileofs);
	if (brushesLump->filelen % sizeof(*brushes)) {
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	}
	const int brushesCount = brushesLump->filelen / sizeof(*brushes);

	const dbrushside_t* sides = (dbrushside_t*)(fileBase + sidesLump->fileofs);
	if (sidesLump->filelen % sizeof(*sides)) {
		Com_Error(ERR_DROP, "LoadMap: funny lump size in %s", world_data.name);
	}
	const int sidesCount = sidesLump->filelen / sizeof(*sides);

	for (int i = 0; i < count; i++, fogs++) {
		out->originalBrushNumber = LittleLong(fogs->brushNum);

		if (out->originalBrushNumber == -1)
		{
			out->bounds[0][0] = out->bounds[0][1] = out->bounds[0][2] = MIN_WORLD_COORD;
			out->bounds[1][0] = out->bounds[1][1] = out->bounds[1][2] = MAX_WORLD_COORD;
			firstSide = -1;
			world_data.globalFog = i + 1;
		}
		else
		{
			if (static_cast<unsigned>(out->originalBrushNumber) >= static_cast<unsigned>(brushesCount)) {
				Com_Error(ERR_DROP, "fog brushNumber out of range");
			}
			const dbrush_t* brush = brushes + out->originalBrushNumber;

			firstSide = LittleLong(brush->firstSide);

			if (static_cast<unsigned>(firstSide) > static_cast<unsigned>(sidesCount - 6)) {
				Com_Error(ERR_DROP, "fog brush sideNumber out of range");
			}

			// brushes are always sorted with the axial sides first
			sideNum = firstSide + 0;
			planeNum = LittleLong(sides[sideNum].planeNum);
			out->bounds[0][0] = -world_data.planes[planeNum].dist;

			sideNum = firstSide + 1;
			planeNum = LittleLong(sides[sideNum].planeNum);
			out->bounds[1][0] = world_data.planes[planeNum].dist;

			sideNum = firstSide + 2;
			planeNum = LittleLong(sides[sideNum].planeNum);
			out->bounds[0][1] = -world_data.planes[planeNum].dist;

			sideNum = firstSide + 3;
			planeNum = LittleLong(sides[sideNum].planeNum);
			out->bounds[1][1] = world_data.planes[planeNum].dist;

			sideNum = firstSide + 4;
			planeNum = LittleLong(sides[sideNum].planeNum);
			out->bounds[0][2] = -world_data.planes[planeNum].dist;

			sideNum = firstSide + 5;
			planeNum = LittleLong(sides[sideNum].planeNum);
			out->bounds[1][2] = world_data.planes[planeNum].dist;
		}

		// get information from the shader for fog parameters
		const shader_t* shader = R_FindShader(fogs->shader, lightmaps, stylesDefault, qtrue);

		if (!shader->fogParms)
		{//bad shader!!
			assert(shader->fogParms);
			out->parms.color[0] = 1.0f;
			out->parms.color[1] = 0.0f;
			out->parms.color[2] = 0.0f;
			out->parms.depthForOpaque = 250.0f;
		}
		else
		{
			out->parms = *shader->fogParms;
		}

		out->colorInt = ColorBytes4(out->parms.color[0] * tr.identityLight,
			out->parms.color[1] * tr.identityLight,
			out->parms.color[2] * tr.identityLight, 1.0);
		const float d = out->parms.depthForOpaque < 1 ? 1 : out->parms.depthForOpaque;
		out->tcScale = 1.0f / (d * 8);

		// set the gradient vector
		sideNum = LittleLong(fogs->visibleSide);

		if (sideNum == -1) {
			//rww - we need to set this to qtrue for global fog as well
			out->hasSurface = qtrue;
		}
		else {
			out->hasSurface = qtrue;
			planeNum = LittleLong(sides[firstSide + sideNum].planeNum);
			VectorSubtract(vec3_origin, world_data.planes[planeNum].normal, out->surface);
			out->surface[3] = -world_data.planes[planeNum].dist;
		}

		out++;
	}
}

/*
================
R_LoadLightGrid

================
*/
void R_LoadLightGrid(lump_t* l, world_t& world_data) {
	int		i;
	vec3_t	maxs;

	world_t* w = &world_data;

	w->lightGridInverseSize[0] = 1.0 / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0 / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0 / w->lightGridSize[2];

	const float* wMins = w->bmodels[0].bounds[0];
	const float* wMaxs = w->bmodels[0].bounds[1];

	for (i = 0; i < 3; i++) {
		w->lightGridOrigin[i] = w->lightGridSize[i] * ceil(wMins[i] / w->lightGridSize[i]);
		maxs[i] = w->lightGridSize[i] * floor(wMaxs[i] / w->lightGridSize[i]);
		w->lightGridBounds[i] = (maxs[i] - w->lightGridOrigin[i]) / w->lightGridSize[i] + 1;
	}

	const int numGridDataElements = l->filelen / sizeof(*w->lightGridData);

	w->lightGridData = static_cast<mgrid_t*>(Hunk_Alloc(l->filelen, h_low));
	memcpy(w->lightGridData, (void*)(fileBase + l->fileofs), l->filelen);

	// deal with overbright bits
	for (i = 0; i < numGridDataElements; i++)
	{
		for (int j = 0; j < MAXLIGHTMAPS; j++)
		{
			R_ColorShiftLightingBytes(w->lightGridData[i].ambientLight[j]);
			R_ColorShiftLightingBytes(w->lightGridData[i].directLight[j]);
		}
	}
}

/*
================
R_LoadLightGridArray

================
*/
void R_LoadLightGridArray(lump_t* l, world_t& world_data) {
	world_t* w;
#ifdef Q3_BIG_ENDIAN
	int		i;
#endif

	w = &world_data;

	w->numGridArrayElements = w->lightGridBounds[0] * w->lightGridBounds[1] * w->lightGridBounds[2];

	if (static_cast<unsigned>(l->filelen) != w->numGridArrayElements * sizeof(*w->lightGridArray)) {
		ri->Printf(PRINT_ALL, S_COLOR_YELLOW  "WARNING: light grid array mismatch\n");
		w->lightGridData = nullptr;
		return;
	}

	w->lightGridArray = static_cast<unsigned short*>(Hunk_Alloc(l->filelen, h_low));
	memcpy(w->lightGridArray, (void*)(fileBase + l->fileofs), l->filelen);
#ifdef Q3_BIG_ENDIAN
	for (i = 0; i < w->numGridArrayElements; i++) {
		w->lightGridArray[i] = LittleShort(w->lightGridArray[i]);
	}
#endif
}

/*
================
R_LoadEntities
================
*/
void R_LoadEntities(lump_t* l, world_t& world_data) {
	const char* p;
	char keyname[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];
	float ambient = 1;

	world_t* w = &world_data;
	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	VectorSet(tr.sunAmbient, 1, 1, 1);
	tr.distanceCull = 6000;//DEFAULT_DISTANCE_CULL;

	p = (char*)(fileBase + l->fileofs);

	// store for reference by the cgame
	w->entityString = static_cast<char*>(Hunk_Alloc(l->filelen + 1, h_low));
	strcpy(w->entityString, p);
	w->entityParsePoint = w->entityString;

	COM_BeginParseSession("R_LoadEntities");

	const char* token = COM_ParseExt(&p, qtrue);
	if (!*token || *token != '{') {
		return;
	}

	// only parse the world spawn
	while (1) {
		// parse key
		token = COM_ParseExt(&p, qtrue);

		if (!*token || *token == '}') {
			break;
		}
		Q_strncpyz(keyname, token, sizeof(keyname));

		// parse value
		token = COM_ParseExt(&p, qtrue);

		if (!*token || *token == '}') {
			break;
		}
		Q_strncpyz(value, token, sizeof(value));

		// check for remapping of shaders for vertex lighting
		char* s = "vertexremapshader";
		if (!Q_strncmp(keyname, s, strlen(s))) {
			s = strchr(value, ';');
			if (!s) {
				ri->Printf(PRINT_ALL, S_COLOR_YELLOW  "WARNING: no semi colon in vertexshaderremap '%s'\n", value);
				break;
			}
			*s++ = 0;
			if (r_vertexLight->integer) {
				R_RemapShader(value, s, "0");
			}
			continue;
		}
		// check for remapping of shaders
		s = "remapshader";
		if (!Q_strncmp(keyname, s, strlen(s))) {
			s = strchr(value, ';');
			if (!s) {
				ri->Printf(PRINT_ALL, S_COLOR_YELLOW  "WARNING: no semi colon in shaderremap '%s'\n", value);
				break;
			}
			*s++ = 0;
			R_RemapShader(value, s, "0");
			continue;
		}
		if (!Q_stricmp(keyname, "distanceCull")) {
			sscanf(value, "%f", &tr.distanceCull);
			continue;
		}
		// check for a different grid size
		if (!Q_stricmp(keyname, "gridsize")) {
			sscanf(value, "%f %f %f", &w->lightGridSize[0], &w->lightGridSize[1], &w->lightGridSize[2]);
			continue;
		}
		// find the optional world ambient for arioche
		if (!Q_stricmp(keyname, "_color")) {
			sscanf(value, "%f %f %f", &tr.sunAmbient[0], &tr.sunAmbient[1], &tr.sunAmbient[2]);
			continue;
		}
		if (!Q_stricmp(keyname, "ambient")) {
			sscanf(value, "%f", &ambient);
		}
	}
	//both default to 1 so no harm if not present.
	VectorScale(tr.sunAmbient, ambient, tr.sunAmbient);
}

/*
=================
R_GetEntityToken
=================
*/
qboolean R_GetEntityToken(char* buffer, int size) {
	if (size == -1)
	{ //force reset
		s_worldData.entityParsePoint = s_worldData.entityString;
		return qtrue;
	}

	const char* s = COM_Parse((const char**)&s_worldData.entityParsePoint);
	Q_strncpyz(buffer, s, size);
	if (!s_worldData.entityParsePoint || !s[0]) {
		return qfalse;
	}
	return qtrue;
}

/*
=================
RE_LoadWorldMap

Called directly from cgame
=================
*/
void RE_LoadWorldMap_Actual(const char* name, world_t& world_data, int index)
{
	byte* buffer;

	if (tr.worldMapLoaded && !index) {
		Com_Error(ERR_DROP, "ERROR: attempted to redundantly load world map\n");
	}

	if (!index)
	{
		skyboxportal = 0;

		// set default sun direction to be used if it isn't
		// overridden by a shader
		tr.sunDirection[0] = 0.45f;
		tr.sunDirection[1] = 0.3f;
		tr.sunDirection[2] = 0.9f;

		VectorNormalize(tr.sunDirection);

		tr.worldMapLoaded = qtrue;

		// clear tr.world so if the level fails to load, the next
		// try will not look at the partially loaded version
		tr.world = nullptr;
	}

	// check for cached disk file from the server first...
	//
	if (ri->CM_GetCachedMapDiskImage())
	{
		buffer = static_cast<byte*>(ri->CM_GetCachedMapDiskImage());
	}
	else
	{
		// still needs loading...
		//
		ri->FS_ReadFile(name, (void**)&buffer);
		if (!buffer) {
			Com_Error(ERR_DROP, "RE_LoadWorldMap: %s not found", name);
		}
	}

	memset(&world_data, 0, sizeof(world_data));
	Q_strncpyz(world_data.name, name, sizeof(world_data.name));
	Q_strncpyz(tr.worldDir, name, sizeof(tr.worldDir));
	Q_strncpyz(world_data.baseName, COM_SkipPath(world_data.name), sizeof(world_data.name));

	COM_StripExtension(world_data.baseName, world_data.baseName, sizeof(world_data.baseName));
	COM_StripExtension(tr.worldDir, tr.worldDir, sizeof(tr.worldDir));

	const byte* startMarker = static_cast<byte*>(Hunk_Alloc(0, h_low));
	c_gridVerts = 0;

	dheader_t* header = (dheader_t*)buffer;
	fileBase = (byte*)header;

	const int i = LittleLong(header->version);
	if (i != BSP_VERSION) {
		Com_Error(ERR_DROP, "RE_LoadWorldMap: %s has wrong version number (%i should be %i)",
			name, i, BSP_VERSION);
	}

	// swap all the lumps
	for (size_t i = 0; i < sizeof(dheader_t) / 4; i++) {
		((int*)header)[i] = LittleLong(((int*)header)[i]);
	}

	// load into heap
	R_LoadShaders(&header->lumps[LUMP_SHADERS], world_data);
	R_LoadLightmaps(&header->lumps[LUMP_LIGHTMAPS], name, world_data);
	R_LoadPlanes(&header->lumps[LUMP_PLANES], world_data);
	R_LoadFogs(&header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES], world_data, index);
	R_LoadSurfaces(&header->lumps[LUMP_SURFACES], &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES], world_data, index);
	R_LoadMarksurfaces(&header->lumps[LUMP_LEAFSURFACES], world_data);
	R_LoadNodesAndLeafs(&header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS], world_data);
	R_LoadSubmodels(&header->lumps[LUMP_MODELS], world_data, index);
	R_LoadVisibility(&header->lumps[LUMP_VISIBILITY], world_data);

	world_data.dataSize = static_cast<byte*>(Hunk_Alloc(0, h_low)) - startMarker;

	if (!index)
	{
		R_LoadEntities(&header->lumps[LUMP_ENTITIES], world_data);
		R_LoadLightGrid(&header->lumps[LUMP_LIGHTGRID], world_data);
		R_LoadLightGridArray(&header->lumps[LUMP_LIGHTARRAY], world_data);

		// only set tr.world now that we know the entire level has loaded properly
		tr.world = &world_data;
	}

	if (ri->CM_GetCachedMapDiskImage())
	{
		Z_Free(ri->CM_GetCachedMapDiskImage());
		ri->CM_SetCachedMapDiskImage(nullptr);
	}
	else
	{
		ri->FS_FreeFile(buffer);
	}
}

// new wrapper used for convenience to tell z_malloc()-fail recovery code whether it's safe to dump the cached-bsp or not.
//
void RE_LoadWorldMap(const char* name)
{
	ri->CM_SetUsingCache(qtrue);
	RE_LoadWorldMap_Actual(name, s_worldData, 0);
	ri->CM_SetUsingCache(qfalse);
}