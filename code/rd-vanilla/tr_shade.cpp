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

// tr_shade.c

#include "../server/exe_headers.h"

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

shaderCommands_t	tess;
static qboolean	setArraysOnce;

color4ub_t	styleColors[MAX_LIGHT_STYLES];
bool		styleUpdated[MAX_LIGHT_STYLES];

extern bool g_bRenderGlowingObjects;

/*
================
R_ArrayElementDiscrete

This is just for OpenGL conformance testing, it should never be the fastest
================
*/
static void APIENTRY R_ArrayElementDiscrete(const GLint index) {
	qglColor4ubv(tess.svars.colors[index]);
	if (glState.currenttmu) {
		qglMultiTexCoord2fARB(0, tess.svars.texcoords[0][index][0], tess.svars.texcoords[0][index][1]);
		qglMultiTexCoord2fARB(1, tess.svars.texcoords[1][index][0], tess.svars.texcoords[1][index][1]);
	}
	else {
		qglTexCoord2fv(tess.svars.texcoords[0][index]);
	}
	qglVertex3fv(tess.xyz[index]);
}

/*
===================
R_DrawStripElements

===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void R_DrawStripElements(const int numIndexes, const glIndex_t* indexes, void (APIENTRY* element)(GLint))
{
	glIndex_t last[3]{};

	qglBegin(GL_TRIANGLE_STRIP);
	c_begins++;

	if (numIndexes <= 0) {
		return;
	}

	// prime the strip
	element(indexes[0]);
	element(indexes[1]);
	element(indexes[2]);
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	qboolean even = qfalse;

	for (int i = 3; i < numIndexes; i += 3)
	{
		// odd numbered triangle in potential strip
		if (!even)
		{
			// check previous triangle to see if we're continuing a strip
			if (indexes[i + 0] == last[2] && indexes[i + 1] == last[1])
			{
				element(indexes[i + 2]);
				c_vertexes++;
				assert(static_cast<int>(indexes[i + 2]) < tess.numVertexes);
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin(GL_TRIANGLE_STRIP);
				c_begins++;

				element(indexes[i + 0]);
				element(indexes[i + 1]);
				element(indexes[i + 2]);

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if (last[2] == indexes[i + 1] && last[0] == indexes[i + 0])
			{
				element(indexes[i + 2]);
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin(GL_TRIANGLE_STRIP);
				c_begins++;

				element(indexes[i + 0]);
				element(indexes[i + 1]);
				element(indexes[i + 2]);
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i + 0];
		last[1] = indexes[i + 1];
		last[2] = indexes[i + 2];
	}

	qglEnd();
}

/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void R_DrawElements(const int numIndexes, const glIndex_t* indexes)
{
	int primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if (primitives == 0) {
		if (qglLockArraysEXT) {
			primitives = 2;
		}
		else {
			primitives = 1;
		}
	}

	if (primitives == 2) {
		qglDrawElements(GL_TRIANGLES,
			numIndexes,
			GL_INDEX_TYPE,
			indexes);
		return;
	}

	if (primitives == 1) {
		R_DrawStripElements(numIndexes, indexes, qglArrayElement);
		return;
	}

	if (primitives == 3) {
		R_DrawStripElements(numIndexes, indexes, R_ArrayElementDiscrete);
	}

	// anything else will cause no drawing
}

/*
=============================================================

SURFACE SHADERS

=============================================================
*/

/*
=================
R_BindAnimatedImage

=================
*/
void R_BindAnimatedImage(const textureBundle_t* bundle) {
	int		index;

	if (bundle->isVideoMap) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ((r_fullbright->integer || tr.refdef.doLAGoggles || tr.refdef.rdflags & RDF_doFullbright) && bundle->isLightmap)
	{
		GL_Bind(tr.whiteImage);
		return;
	}

	if (bundle->numImageAnimations <= 1) {
		GL_Bind(bundle->image);
		return;
	}

	if (backEnd.currentEntity->e.renderfx & RF_SETANIMINDEX)
	{
		index = backEnd.currentEntity->e.skinNum;
	}
	else
	{
		// it is necessary to do this messy calc to make sure animations line up
		// exactly with waveforms of the same frequency
		index = Q_ftol(backEnd.refdef.floatTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE);
		index >>= FUNCTABLE_SIZE2;

		if (index < 0) {
			index = 0;	// may happen with shader time offsets
		}
	}

	if (bundle->oneShotAnimMap)
	{
		if (index >= bundle->numImageAnimations)
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	GL_Bind(*(reinterpret_cast<image_t**>(bundle->image) + index));
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris(const shaderCommands_t* input)
{
	GL_Bind(tr.whiteImage);

	if (r_showtriscolor->integer)
	{
		int i = r_showtriscolor->integer;
		if (i == 42) {
			i = Q_irand(0, 8);
		}
		switch (i)
		{
		case 1:
			qglColor3f(1.0, 0.0, 0.0); //red
			break;
		case 2:
			qglColor3f(0.0, 1.0, 0.0); //green
			break;
		case 3:
			qglColor3f(1.0, 1.0, 0.0); //yellow
			break;
		case 4:
			qglColor3f(0.0, 0.0, 1.0); //blue
			break;
		case 5:
			qglColor3f(0.0, 1.0, 1.0); //cyan
			break;
		case 6:
			qglColor3f(1.0, 0.0, 1.0); //magenta
			break;
		case 7:
			qglColor3f(0.8f, 0.8f, 0.8f); //white/grey
			break;
		case 8:
			qglColor3f(0.0, 0.0, 0.0); //black
			break;
		default:;
		}
	}
	else
	{
		qglColor3f(1.0, 1.0, 1.0); //white
	}

	if (r_showtris->integer == 2)
	{
		// tries to do non-xray style showtris
		GL_State(GLS_POLYMODE_LINE);

		qglEnable(GL_POLYGON_OFFSET_LINE);
		qglPolygonOffset(-1, -2);

		qglDisableClientState(GL_COLOR_ARRAY);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

		qglVertexPointer(3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment("glLockArraysEXT\n");
		}

		R_DrawElements(input->numIndexes, input->indexes);

		if (qglUnlockArraysEXT)
		{
			qglUnlockArraysEXT();
			GLimp_LogComment("glUnlockArraysEXT\n");
		}

		qglDisable(GL_POLYGON_OFFSET_LINE);
	}
	else
	{
		// same old showtris
		GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);
		qglDepthRange(0, 0);

		qglDisableClientState(GL_COLOR_ARRAY);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

		qglVertexPointer(3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

		if (qglLockArraysEXT) {
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment("glLockArraysEXT\n");
		}

		R_DrawElements(input->numIndexes, input->indexes);

		if (qglUnlockArraysEXT) {
			qglUnlockArraysEXT();
			GLimp_LogComment("glUnlockArraysEXT\n");
		}

		qglDepthRange(0, 1);
	}
}

/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals(const shaderCommands_t* input) {
	GL_Bind(tr.whiteImage);
	qglColor3f(1, 1, 1);
	qglDepthRange(0, 0);	// never occluded
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);

	qglBegin(GL_LINES);
	for (int i = 0; i < input->numVertexes; i++) {
		vec3_t temp;
		qglVertex3fv(input->xyz[i]);
		VectorMA(input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv(temp);
	}
	qglEnd();

	qglDepthRange(0, 1);
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface(shader_t* shader, const int fogNum)
{
	//	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;
	shader_t* state = shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;//shader;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions

	tess.SSInitializedWind = qfalse;	//is this right?

	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;

	tess.currentStageIteratorFunc = shader->sky ? RB_StageIteratorSky : RB_StageIteratorGeneric;

	tess.fading = false;

	tess.registration++;
}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured(const shaderCommands_t* input, const int stage)
{
	const shaderStage_t* p_stage = &tess.xstages[stage];

	GL_State(p_stage->stateBits);

	//
	// base
	//
	GL_SelectTexture(0);
	qglTexCoordPointer(2, GL_FLOAT, 0, input->svars.texcoords[0]);
	R_BindAnimatedImage(&p_stage->bundle[0]);

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture(1);
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	if (r_lightmap->integer) {
		GL_TexEnv(GL_REPLACE);
	}
	else {
		GL_TexEnv(tess.shader->multitextureEnv);
	}

	qglTexCoordPointer(2, GL_FLOAT, 0, input->svars.texcoords[1]);

	R_BindAnimatedImage(&p_stage->bundle[1]);

	R_DrawElements(input->numIndexes, input->indexes);

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(0);
}

//--EF_old dlight code...reverting back to Quake III dlight to see if people like that better
// Lifted the whole function because someone hacked the heck out of this and it doesn't seem to
//	be a case where it's as easy as just changing the blend mode....
/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
/*
static void ProjectDlightTexture( void ) {
	int		l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	unsigned	hitIndexes[SHADER_MAX_INDEXES];

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		int		numIndexes;
		vec3_t	floatColor;
		float	scale;
		float	radius, chord;
		dlight_t	*dl;
		int i;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		chord = radius*radius*0.25f;
		scale = 1.0f / radius;
		floatColor[0] = dl->color[0] * 255f;
		floatColor[1] = dl->color[1] * 255f;
		floatColor[2] = dl->color[2] * 255f;

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	distVec;
			int		clip;
			float	tempColor;
			float	modulate, dist;

//			if ( 0 ) {
//				clipBits[i] = 255;	// definately not dlighted
//				continue;
//			}
//
			backEnd.pc.c_dlightVertexes++;

			VectorSubtract( origin, tess.xyz[i], distVec );
			dist = VectorLengthSquared(distVec);

			texCoords[0] = 0.5 + distVec[0] * scale;	//xy projection
			texCoords[1] = 0.5 + distVec[1] * scale;

			clip = 0;
			if ( texCoords[0] < 0 ) {
				clip |= 1;
			} else if ( texCoords[0] > 1 ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0 ) {
				clip |= 4;
			} else if ( texCoords[1] > 1 ) {
				clip |= 8;
			}
			clipBits[i] = clip;

			// modulate the strength based on the height and color
			if ( dist > chord) {
				clip |= 16;
				modulate = 255*1.0ff;
			} else {
				modulate = 255*2*dist*scale*scale;
			}
			tempColor = floatColor[0] + modulate;
			colors[0] = tempColor > 255 ? 255: Q_ftol(tempColor);

			tempColor = floatColor[1] + modulate;
			colors[1] = tempColor > 255 ? 255: Q_ftol(tempColor);

			tempColor = floatColor[2] + modulate;
			colors[2] = tempColor > 255 ? 255: Q_ftol(tempColor);

//			colors[3] = 255;
			if ( distVec[2] > radius ) {
				colors[3] = 0;
			} else if ( distVec[2] < -radius ) {
				colors[3] = 0;
			} else {
				if ( distVec[2] < 0 ) {
					distVec[2] = -distVec[2];
				}
				if ( distVec[2] < radius * 0.5 ) {
					colors[3] = 255;
				} else {
					colors[3] = Q_ftol(255* (radius - distVec[2]) * scale);
				}
			}
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

		GL_Bind( tr.dlightImage );

		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_SRC_COLOR | GLS_DEPTHFUNC_EQUAL);//our way
//		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );	//Id way
		R_DrawElements( numIndexes, hitIndexes );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}
*/

// Lifted from Quake III to see if people like this kind of dlight better
/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static void ProjectDlightTexture2()
{
	int		i;
	byte	clip_bits[SHADER_MAX_VERTEXES]{};
	float	tex_coords_array[SHADER_MAX_VERTEXES][2]{};
	float	old_tex_coords_array[SHADER_MAX_VERTEXES][2]{};
	unsigned int		color_array[SHADER_MAX_VERTEXES]{};
	glIndex_t	hit_indexes[SHADER_MAX_INDEXES]{};
	int		numIndexes;
	float	radius;
#ifndef JK2_MODE
	int		fogging;
#endif
	shaderStage_t* d_stage;
	vec3_t	float_color{};
	byte color_temp[4]{};

	int		need_reset_verts = 0;

	if (!backEnd.refdef.num_dlights)
	{
		return;
	}

	for (int l = 0; l < backEnd.refdef.num_dlights; l++)
	{
		vec3_t dist;
		float vert_coords_array[SHADER_MAX_VERTEXES][4]{};
		vec3_t origin;
		if (!(tess.dlightBits & 1 << l)) {
			continue;	// this surface definately doesn't have any of this light
		}

		const dlight_t* dl = &backEnd.refdef.dlights[l];
		VectorCopy(dl->transformed, origin);
		radius = dl->radius;

		int		clipall = 63;
		for (i = 0; i < tess.numVertexes; i++)
		{
			VectorSubtract(origin, tess.xyz[i], dist);

			int clip = 0;
			if (dist[0] < -radius)
			{
				clip |= 1;
			}
			else if (dist[0] > radius)
			{
				clip |= 2;
			}
			if (dist[1] < -radius)
			{
				clip |= 4;
			}
			else if (dist[1] > radius)
			{
				clip |= 8;
			}
			if (dist[2] < -radius)
			{
				clip |= 16;
			}
			else if (dist[2] > radius)
			{
				clip |= 32;
			}

			clip_bits[i] = clip;
			clipall &= clip;
		}
		if (clipall)
		{
			continue;	// this surface doesn't have any of this light
		}
		float_color[0] = dl->color[0] * 255.0f;
		float_color[1] = dl->color[1] * 255.0f;
		float_color[2] = dl->color[2] * 255.0f;
		// build a list of triangles that need light
		numIndexes = 0;
		for (i = 0; i < tess.numIndexes; i += 3)
		{
			vec3_t normal;
			vec3_t e2;
			vec3_t e1;
			vec3_t posc;
			vec3_t posb;
			vec3_t posa;
			const int a = tess.indexes[i];
			const int b = tess.indexes[i + 1];
			const int c = tess.indexes[i + 2];
			if (clip_bits[a] & clip_bits[b] & clip_bits[c])
			{
				continue;	// not lighted
			}

			// copy the vertex positions
			VectorCopy(tess.xyz[a], posa);
			VectorCopy(tess.xyz[b], posb);
			VectorCopy(tess.xyz[c], posc);

			VectorSubtract(posa, posb, e1);
			VectorSubtract(posc, posb, e2);
			CrossProduct(e1, e2, normal);
			//			fac=DotProduct(normal,origin)-DotProduct(normal,posa);
			//			if (fac <= 0.0f || // backface
			// rjr - removed for hacking 			if ( (!r_dlightBacks->integer && DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f) || // backface
			if (DotProduct(normal, origin) - DotProduct(normal, posa) <= 0.0f || // backface
				DotProduct(normal, normal) < 1E-8f) // junk triangle
			{
				continue;
			}
			VectorNormalize(normal);
			float fac = DotProduct(normal, origin) - DotProduct(normal, posa);
			if (fac >= radius)  // out of range
			{
				continue;
			}
			const float modulate = 1.0f - fac * fac / (radius * radius);
			fac = 0.5f / sqrtf(radius * radius - fac * fac);

			// save the verts
			VectorCopy(posa, vert_coords_array[numIndexes]);
			VectorCopy(posb, vert_coords_array[numIndexes + 1]);
			VectorCopy(posc, vert_coords_array[numIndexes + 2]);

			// now we need e1 and e2 to be an orthonormal basis
			if (DotProduct(e1, e1) > DotProduct(e2, e2))
			{
				VectorNormalize(e1);
				CrossProduct(e1, normal, e2);
			}
			else
			{
				VectorNormalize(e2);
				CrossProduct(normal, e2, e1);
			}
			VectorScale(e1, fac, e1);
			VectorScale(e2, fac, e2);

			VectorSubtract(posa, origin, dist);
			tex_coords_array[numIndexes][0] = DotProduct(dist, e1) + 0.5f;
			tex_coords_array[numIndexes][1] = DotProduct(dist, e2) + 0.5f;

			VectorSubtract(posb, origin, dist);
			tex_coords_array[numIndexes + 1][0] = DotProduct(dist, e1) + 0.5f;
			tex_coords_array[numIndexes + 1][1] = DotProduct(dist, e2) + 0.5f;

			VectorSubtract(posc, origin, dist);
			tex_coords_array[numIndexes + 2][0] = DotProduct(dist, e1) + 0.5f;
			tex_coords_array[numIndexes + 2][1] = DotProduct(dist, e2) + 0.5f;

			if (tex_coords_array[numIndexes][0] < 0.0f && tex_coords_array[numIndexes + 1][0] < 0.0f && tex_coords_array[numIndexes + 2][0] < 0.0f ||
				tex_coords_array[numIndexes][0] > 1.0f && tex_coords_array[numIndexes + 1][0] > 1.0f && tex_coords_array[numIndexes + 2][0] > 1.0f ||
				tex_coords_array[numIndexes][1] < 0.0f && tex_coords_array[numIndexes + 1][1] < 0.0f && tex_coords_array[numIndexes + 2][1] < 0.0f ||
				tex_coords_array[numIndexes][1] > 1.0f && tex_coords_array[numIndexes + 1][1] > 1.0f && tex_coords_array[numIndexes + 2][1] > 1.0f)
			{
				continue; // didn't end up hitting this tri
			}

			old_tex_coords_array[numIndexes][0] = tess.texCoords[a][0][0];
			old_tex_coords_array[numIndexes][1] = tess.texCoords[a][0][1];
			old_tex_coords_array[numIndexes + 1][0] = tess.texCoords[b][0][0];
			old_tex_coords_array[numIndexes + 1][1] = tess.texCoords[b][0][1];
			old_tex_coords_array[numIndexes + 2][0] = tess.texCoords[c][0][0];
			old_tex_coords_array[numIndexes + 2][1] = tess.texCoords[c][0][1];

			color_temp[0] = Q_ftol(float_color[0] * modulate);
			color_temp[1] = Q_ftol(float_color[1] * modulate);
			color_temp[2] = Q_ftol(float_color[2] * modulate);
			color_temp[3] = 255;

			const byteAlias_t* ba = reinterpret_cast<byteAlias_t*>(&color_temp);
			color_array[numIndexes + 0] = ba->ui;
			color_array[numIndexes + 1] = ba->ui;
			color_array[numIndexes + 2] = ba->ui;

			hit_indexes[numIndexes] = numIndexes;
			hit_indexes[numIndexes + 1] = numIndexes + 1;
			hit_indexes[numIndexes + 2] = numIndexes + 2;
			numIndexes += 3;

			if (numIndexes >= SHADER_MAX_VERTEXES - 3)
			{
				break; // we are out of space, so we are done :)
			}
		}

		if (!numIndexes) {
			continue;
		}
#ifndef JK2_MODE
		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (r_drawfog->value == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}
#endif

		if (!need_reset_verts)
		{
			need_reset_verts = 1;
			if (qglUnlockArraysEXT)
			{
				qglUnlockArraysEXT();
				GLimp_LogComment("glUnlockArraysEXT\n");
			}
		}
		qglVertexPointer(3, GL_FLOAT, 16, vert_coords_array);	// padded for SIMD

		d_stage = nullptr;
		if (tess.shader && qglActiveTextureARB)
		{
			int i1 = 0;
			while (i1 < tess.shader->numUnfoggedPasses)
			{
				constexpr int blend_bits = GLS_SRCBLEND_BITS + GLS_DSTBLEND_BITS;
				if ((tess.shader->stages[i1].bundle[0].image && !tess.shader->stages[i1].bundle[0].isLightmap && !tess.shader->stages[i1].bundle[0].numTexMods && tess.shader->stages[i1].bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i1].bundle[0].tcGen != TCGEN_FOG ||
					tess.shader->stages[i1].bundle[1].image && !tess.shader->stages[i1].bundle[1].isLightmap && !tess.shader->stages[i1].bundle[1].numTexMods && tess.shader->stages[i1].bundle[1].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i1].bundle[1].tcGen != TCGEN_FOG) &&
					(tess.shader->stages[i1].stateBits & blend_bits) == 0)
				{ //only use non-lightmap opaque stages
					d_stage = &tess.shader->stages[i1];
					break;
				}
				i1++;
			}
		}

		if (d_stage)
		{
			GL_SelectTexture(0);
			GL_State(0);
			qglTexCoordPointer(2, GL_FLOAT, 0, old_tex_coords_array[0]);
			if (d_stage->bundle[0].image && !d_stage->bundle[0].isLightmap && !d_stage->bundle[0].numTexMods && d_stage->bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && d_stage->bundle[0].tcGen != TCGEN_FOG)
			{
				R_BindAnimatedImage(&d_stage->bundle[0]);
			}
			else
			{
				R_BindAnimatedImage(&d_stage->bundle[1]);
			}

			GL_SelectTexture(1);
			qglEnable(GL_TEXTURE_2D);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			qglTexCoordPointer(2, GL_FLOAT, 0, tex_coords_array[0]);
			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
			GL_Bind(tr.dlightImage);
			GL_TexEnv(GL_MODULATE);

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

			R_DrawElements(numIndexes, hit_indexes);

			qglDisable(GL_TEXTURE_2D);
			GL_SelectTexture(0);
		}
		else
		{
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			qglTexCoordPointer(2, GL_FLOAT, 0, tex_coords_array[0]);

			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);

			GL_Bind(tr.dlightImage);
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			//if ( dl->additive ) {
			//	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			//}
			//else
			{
				GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);
			}

			R_DrawElements(numIndexes, hit_indexes);
		}

#ifndef JK2_MODE
		if (fogging)
		{
			qglEnable(GL_FOG);
		}
#endif

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
	if (need_reset_verts)
	{
		qglVertexPointer(3, GL_FLOAT, 16, tess.xyz);	// padded for SIMD
		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, tess.numVertexes);
			GLimp_LogComment("glLockArraysEXT\n");
		}
	}
}
static void ProjectDlightTexture()
{
	int		i;
	float* tex_coords;
	byte* colors;
	byte	clip_bits[SHADER_MAX_VERTEXES]{};
	glIndex_t	hit_indexes[SHADER_MAX_INDEXES]{};
	int		numIndexes;
	float	scale;
	float	radius;
#ifndef JK2_MODE
	int		fogging;
#endif
	vec3_t	float_color{};
	shaderStage_t* d_stage;

	if (!backEnd.refdef.num_dlights) {
		return;
	}

	for (int l = 0; l < backEnd.refdef.num_dlights; l++) {
		byte color_array[SHADER_MAX_VERTEXES][4]{};
		float tex_coords_array[SHADER_MAX_VERTEXES][2]{};
		vec3_t origin;
		if (!(tess.dlightBits & 1 << l)) {
			continue;	// this surface definately doesn't have any of this light
		}

		tex_coords = tex_coords_array[0];
		colors = color_array[0];

		const dlight_t* dl = &backEnd.refdef.dlights[l];
		VectorCopy(dl->transformed, origin);
		radius = dl->radius;

		float_color[0] = dl->color[0] * 255.0f;
		float_color[1] = dl->color[1] * 255.0f;
		float_color[2] = dl->color[2] * 255.0f;

		for (i = 0; i < tess.numVertexes; i++, tex_coords += 2, colors += 4) {
			vec3_t	dist;
			float	modulate;

			backEnd.pc.c_dlightVertexes++;

			VectorSubtract(origin, tess.xyz[i], dist);

			int best_index = 0;
			float greatest = tess.normal[i][0];
			if (greatest < 0.0f)
			{
				greatest = -greatest;
			}

			if (VectorCompare(tess.normal[i], vec3_origin))
			{ //damn you terrain!
				best_index = 2;
			}
			else
			{
				int i1 = 1;
				while (i1 < 3)
				{
					if (tess.normal[i][i1] > greatest && tess.normal[i][i1] > 0.0f ||
						tess.normal[i][i1] < -greatest && tess.normal[i][i1] < 0.0f)
					{
						greatest = tess.normal[i][i1];
						if (greatest < 0.0f)
						{
							greatest = -greatest;
						}
						best_index = i1;
					}
					i1++;
				}
			}

			float d_use;
			constexpr float max_scale = 1.5f;
			constexpr float light_scale_tolerance = 0.1f;

			if (best_index == 2)
			{
				constexpr float max_ground_scale = 1.4f;
				d_use = origin[2] - tess.xyz[i][2];
				if (d_use < 0.0f)
				{
					d_use = -d_use;
				}
				d_use = radius * 0.5f / d_use;
				if (d_use > max_ground_scale)
				{
					d_use = max_ground_scale;
				}
				else if (d_use < 0.1f)
				{
					d_use = 0.1f;
				}

				if (VectorCompare(tess.normal[i], vec3_origin) ||
					tess.normal[i][0] > light_scale_tolerance ||
					tess.normal[i][0] < -light_scale_tolerance ||
					tess.normal[i][1] > light_scale_tolerance ||
					tess.normal[i][1] < -light_scale_tolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius * d_use);
				}

				tex_coords[0] = 0.5f + dist[0] * scale;
				tex_coords[1] = 0.5f + dist[1] * scale;
			}
			else if (best_index == 1)
			{
				d_use = origin[1] - tess.xyz[i][1];
				if (d_use < 0.0f)
				{
					d_use = -d_use;
				}
				d_use = radius * 0.5f / d_use;
				if (d_use > max_scale)
				{
					d_use = max_scale;
				}
				else if (d_use < 0.1f)
				{
					d_use = 0.1f;
				}
				if (tess.normal[i][0] > light_scale_tolerance ||
					tess.normal[i][0] < -light_scale_tolerance ||
					tess.normal[i][2] > light_scale_tolerance ||
					tess.normal[i][2] < -light_scale_tolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius * d_use);
				}

				tex_coords[0] = 0.5f + dist[0] * scale;
				tex_coords[1] = 0.5f + dist[2] * scale;
			}
			else
			{
				d_use = origin[0] - tess.xyz[i][0];
				if (d_use < 0.0f)
				{
					d_use = -d_use;
				}
				d_use = radius * 0.5f / d_use;
				if (d_use > max_scale)
				{
					d_use = max_scale;
				}
				else if (d_use < 0.1f)
				{
					d_use = 0.1f;
				}
				if (tess.normal[i][2] > light_scale_tolerance ||
					tess.normal[i][2] < -light_scale_tolerance ||
					tess.normal[i][1] > light_scale_tolerance ||
					tess.normal[i][1] < -light_scale_tolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius * d_use);
				}

				tex_coords[0] = 0.5f + dist[1] * scale;
				tex_coords[1] = 0.5f + dist[2] * scale;
			}

			int clip = 0;
			if (tex_coords[0] < 0.0f) {
				clip |= 1;
			}
			else if (tex_coords[0] > 1.0f) {
				clip |= 2;
			}
			if (tex_coords[1] < 0.0f) {
				clip |= 4;
			}
			else if (tex_coords[1] > 1.0f) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if (dist[best_index] > radius) {
				clip |= 16;
				modulate = 0.0f;
			}
			else if (dist[best_index] < -radius) {
				clip |= 32;
				modulate = 0.0f;
			}
			else {
				dist[best_index] = Q_fabs(dist[best_index]);
				if (dist[best_index] < radius * 0.5f) {
					modulate = 1.0f;
				}
				else {
					modulate = 2.0f * (radius - dist[best_index]) * scale;
				}
			}
			clip_bits[i] = clip;

			colors[0] = Q_ftol(float_color[0] * modulate);
			colors[1] = Q_ftol(float_color[1] * modulate);
			colors[2] = Q_ftol(float_color[2] * modulate);
			colors[3] = 255;
		}
		// build a list of triangles that need light
		numIndexes = 0;
		for (i = 0; i < tess.numIndexes; i += 3) {
			const int a = tess.indexes[i];
			const int b = tess.indexes[i + 1];
			const int c = tess.indexes[i + 2];
			if (clip_bits[a] & clip_bits[b] & clip_bits[c]) {
				continue;	// not lighted
			}
			hit_indexes[numIndexes] = a;
			hit_indexes[numIndexes + 1] = b;
			hit_indexes[numIndexes + 2] = c;
			numIndexes += 3;
		}

		if (!numIndexes) {
			continue;
		}

#ifndef JK2_MODE
		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (r_drawfog->value == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}
#endif

		d_stage = nullptr;
		if (tess.shader && qglActiveTextureARB)
		{
			int i1 = 0;
			while (i1 < tess.shader->numUnfoggedPasses)
			{
				constexpr int blend_bits = GLS_SRCBLEND_BITS + GLS_DSTBLEND_BITS;
				if ((tess.shader->stages[i1].bundle[0].image && !tess.shader->stages[i1].bundle[0].isLightmap && !tess.shader->stages[i1].bundle[0].numTexMods && tess.shader->stages[i1].bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i1].bundle[0].tcGen != TCGEN_FOG ||
					tess.shader->stages[i1].bundle[1].image && !tess.shader->stages[i1].bundle[1].isLightmap && !tess.shader->stages[i1].bundle[1].numTexMods && tess.shader->stages[i1].bundle[1].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i1].bundle[1].tcGen != TCGEN_FOG) &&
					(tess.shader->stages[i1].stateBits & blend_bits) == 0)
				{ //only use non-lightmap opaque stages
					d_stage = &tess.shader->stages[i1];
					break;
				}
				i1++;
			}
		}

		if (d_stage)
		{
			GL_SelectTexture(0);
			GL_State(0);
			qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texcoords[0]);
			if (d_stage->bundle[0].image && !d_stage->bundle[0].isLightmap && !d_stage->bundle[0].numTexMods && d_stage->bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && d_stage->bundle[0].tcGen != TCGEN_FOG)
			{
				R_BindAnimatedImage(&d_stage->bundle[0]);
			}
			else
			{
				R_BindAnimatedImage(&d_stage->bundle[1]);
			}

			GL_SelectTexture(1);
			qglEnable(GL_TEXTURE_2D);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			qglTexCoordPointer(2, GL_FLOAT, 0, tex_coords_array[0]);
			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
			GL_Bind(tr.dlightImage);
			GL_TexEnv(GL_MODULATE);

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

			R_DrawElements(numIndexes, hit_indexes);

			qglDisable(GL_TEXTURE_2D);
			GL_SelectTexture(0);
		}
		else
		{
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			qglTexCoordPointer(2, GL_FLOAT, 0, tex_coords_array[0]);

			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);

			GL_Bind(tr.dlightImage);
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			//if ( dl->additive ) {
			//	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			//}
			//else
			{
				GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);
			}

			R_DrawElements(numIndexes, hit_indexes);
		}

#ifndef JK2_MODE
		if (fogging)
		{
			qglEnable(GL_FOG);
		}
#endif

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass() {
	qglEnableClientState(GL_COLOR_ARRAY);
	qglColorPointer(4, GL_UNSIGNED_BYTE, 0, tess.svars.colors);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texcoords[0]);

	const fog_t* fog = tr.world->fogs + tess.fogNum;

	for (int i = 0; i < tess.numVertexes; i++)
	{
		auto* ba = reinterpret_cast<byteAlias_t*>(&tess.svars.colors[i]);
		ba->i = fog->colorInt;
	}

	RB_CalcFogTexCoords(reinterpret_cast<float*>(tess.svars.texcoords[0]));

	GL_Bind(tr.fogImage);

	if (tess.shader->fogPass == FP_EQUAL) {
		GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL);
	}
	else {
		GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
	}

	R_DrawElements(tess.numIndexes, tess.indexes);
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors(shaderStage_t* p_stage, alphaGen_t force_alpha_gen, colorGen_t force_rgb_gen)
{
	int i;

	if (tess.shader != tr.projectionShadowShader && tess.shader != tr.shadowShader &&
		backEnd.currentEntity->e.renderfx & (RF_DISINTEGRATE1 | RF_DISINTEGRATE2))
	{
		RB_CalcDisintegrateColors(reinterpret_cast<unsigned char*>(tess.svars.colors), p_stage->rgbGen);
		RB_CalcDisintegrateVertDeform();

		// We've done some custom alpha and color stuff, so we can skip the rest.  Let it do fog though
		force_rgb_gen = CGEN_SKIP;
		force_alpha_gen = AGEN_SKIP;
	}

	//
	// rgbGen
	//
	if (!force_rgb_gen)
	{
		force_rgb_gen = p_stage->rgbGen;
	}

	if (backEnd.currentEntity->e.renderfx & RF_VOLUMETRIC) // does not work for rotated models, technically, this should also be a CGEN type, but that would entail adding new shader commands....which is too much work for one thing
	{
		float* normal = tess.normal[0];
		unsigned char* color = tess.svars.colors[0];

		const int num_vertexes = tess.numVertexes;

		for (int i1 = 0; i1 < num_vertexes; i1++, normal += 4, color += 4)
		{
			float dot = DotProduct(normal, backEnd.refdef.viewaxis[0]);

			dot *= dot * dot * dot;

			if (dot < 0.2f) // so low, so just clamp it
			{
				dot = 0.0f;
			}

			color[0] = color[1] = color[2] = color[3] = Q_ftol(backEnd.currentEntity->e.shaderRGBA[0] * (1 - dot));
		}

		force_rgb_gen = CGEN_SKIP;
		force_alpha_gen = AGEN_SKIP;
	}

	if (!force_alpha_gen)	//set this up so we can override below
	{
		force_alpha_gen = p_stage->alphaGen;
	}

	switch (force_rgb_gen)
	{
	case CGEN_SKIP:
		break;
	case CGEN_IDENTITY:
		memset(tess.svars.colors, 0xff, tess.numVertexes * 4);
		break;
	default:
	case CGEN_IDENTITY_LIGHTING:
		memset(tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4);
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor(reinterpret_cast<unsigned char*>(tess.svars.colors));
		break;
	case CGEN_LIGHTING_DIFFUSE_ENTITY:
		RB_CalcDiffuseEntityColor(reinterpret_cast<unsigned char*>(tess.svars.colors));

		if (force_alpha_gen == AGEN_IDENTITY &&
			backEnd.currentEntity->e.shaderRGBA[3] == 0xff)
		{
			force_alpha_gen = AGEN_SKIP;	//already got it in this set since it does all 4 components
		}
		break;
	case CGEN_EXACT_VERTEX:
		memcpy(tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof tess.vertexColors[0]);
		break;
	case CGEN_CONST:
		for (i = 0; i < tess.numVertexes; i++) {
			auto* ba_dest = reinterpret_cast<byteAlias_t*>(&tess.svars.colors[i]);
			const byteAlias_t* ba_source = reinterpret_cast<byteAlias_t*>(&p_stage->constantColor);
			ba_dest->i = ba_source->i;
		}
		break;
	case CGEN_VERTEX:
		if (tr.identityLight == 1)
		{
			memcpy(tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof tess.vertexColors[0]);
		}
		else
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
				tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
				tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if (tr.identityLight == 1)
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				tess.svars.colors[i][0] = 255 - tess.vertexColors[i][0];
				tess.svars.colors[i][1] = 255 - tess.vertexColors[i][1];
				tess.svars.colors[i][2] = 255 - tess.vertexColors[i][2];
			}
		}
		else
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				tess.svars.colors[i][0] = (255 - tess.vertexColors[i][0]) * tr.identityLight;
				tess.svars.colors[i][1] = (255 - tess.vertexColors[i][1]) * tr.identityLight;
				tess.svars.colors[i][2] = (255 - tess.vertexColors[i][2]) * tr.identityLight;
			}
		}
		break;
	case CGEN_FOG:
	{
		const fog_t* fog = tr.world->fogs + tess.fogNum;

		for (i = 0; i < tess.numVertexes; i++) {
			auto* ba = reinterpret_cast<byteAlias_t*>(&tess.svars.colors[i]);
			ba->i = fog->colorInt;
		}
	}
	break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor(&p_stage->rgbWave, reinterpret_cast<unsigned char*>(tess.svars.colors));
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity(reinterpret_cast<unsigned char*>(tess.svars.colors));
		if (force_alpha_gen == AGEN_IDENTITY &&
			backEnd.currentEntity->e.shaderRGBA[3] == 0xff
			)
		{
			force_alpha_gen = AGEN_SKIP;	//already got it in this set since it does all 4 components
		}

		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity(reinterpret_cast<unsigned char*>(tess.svars.colors));
		break;
	case CGEN_LIGHTMAPSTYLE:
		for (i = 0; i < tess.numVertexes; i++)
		{
			auto* ba_dest = reinterpret_cast<byteAlias_t*>(&tess.svars.colors[i]);
			const byteAlias_t* ba_source = reinterpret_cast<byteAlias_t*>(&styleColors[p_stage->lightmapStyle]);
			ba_dest->ui = ba_source->ui;
		}
		break;
	}

	//
	// alphaGen
	//

	switch (force_alpha_gen)
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if (force_rgb_gen != CGEN_IDENTITY && force_rgb_gen != CGEN_LIGHTING_DIFFUSE) {
			if (force_rgb_gen == CGEN_VERTEX && tr.identityLight != 1 ||
				force_rgb_gen != CGEN_VERTEX) {
				for (i = 0; i < tess.numVertexes; i++) {
					tess.svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_CONST:
		if (force_rgb_gen != CGEN_CONST) {
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.colors[i][3] = p_stage->constantColor[3];
			}
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha(&p_stage->alphaWave, reinterpret_cast<unsigned char*>(tess.svars.colors));
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha(reinterpret_cast<unsigned char*>(tess.svars.colors));
		break;
	case AGEN_ENTITY:
		if (force_rgb_gen != CGEN_ENTITY) { //already got it in the CGEN_entity since it does all 4 components
			RB_CalcAlphaFromEntity(reinterpret_cast<unsigned char*>(tess.svars.colors));
		}
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity(reinterpret_cast<unsigned char*>(tess.svars.colors));
		break;
	case AGEN_VERTEX:
		if (force_rgb_gen != CGEN_VERTEX) {
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for (i = 0; i < tess.numVertexes; i++)
		{
			tess.svars.colors[i][3] = 255 - tess.vertexColors[i][3];
		}
		break;
	case AGEN_PORTAL:
	{
		unsigned char alpha;

		for (i = 0; i < tess.numVertexes; i++)
		{
			vec3_t v;

			VectorSubtract(tess.xyz[i], backEnd.viewParms.ori.origin, v);
			float len = VectorLength(v);

			len /= tess.shader->portalRange;

			if (len < 0)
			{
				alpha = 0;
			}
			else if (len > 1)
			{
				alpha = 0xff;
			}
			else
			{
				alpha = len * 0xff;
			}

			tess.svars.colors[i][3] = alpha;
		}
	}
	break;
	case AGEN_BLEND:
		if (force_rgb_gen != CGEN_VERTEX)
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				tess.svars.colors[i][3] = tess.vertexAlphas[i][p_stage->index];
			}
		}
		break;
	default:
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if (tess.fogNum)
	{
		switch (p_stage->adjustColorsForFog)
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog(reinterpret_cast<unsigned char*>(tess.svars.colors));
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog(reinterpret_cast<unsigned char*>(tess.svars.colors));
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog(reinterpret_cast<unsigned char*>(tess.svars.colors));
			break;
		case ACFF_NONE:
			break;
		default:
			break;
		}
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords(shaderStage_t* p_stage)
{
	int		i;

	for (int b = 0; b < NUM_TEXTURE_BUNDLES; b++) {
		//
		// generate the texture coordinates
		//
		switch (p_stage->bundle[b].tcGen)
		{
		case TCGEN_IDENTITY:
			memset(tess.svars.texcoords[b], 0, sizeof(float) * 2 * tess.numVertexes);
			break;
		case TCGEN_TEXTURE:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][0][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][0][1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][1][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][1][1];
			}
			break;
		case TCGEN_LIGHTMAP1:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][2][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][2][1];
			}
			break;
		case TCGEN_LIGHTMAP2:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][3][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][3][1];
			}
			break;
		case TCGEN_LIGHTMAP3:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][4][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][4][1];
			}
			break;
		case TCGEN_VECTOR:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = DotProduct(tess.xyz[i], p_stage->bundle[b].tcGenVectors[0]);
				tess.svars.texcoords[b][i][1] = DotProduct(tess.xyz[i], p_stage->bundle[b].tcGenVectors[1]);
			}
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords(reinterpret_cast<float*>(tess.svars.texcoords[b]));
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			if (r_environmentMapping->integer)
				RB_CalcEnvironmentTexCoords(reinterpret_cast<float*>(tess.svars.texcoords[b]));
			else
				memset(tess.svars.texcoords[b], 0, sizeof(float) * 2 * tess.numVertexes);
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for (int tm = 0; tm < p_stage->bundle[b].numTexMods; tm++) {
			switch (p_stage->bundle[b].texMods[tm].type)
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords(&p_stage->bundle[b].texMods[tm].wave,
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords(backEnd.currentEntity->e.shaderTexCoord,
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords(p_stage->bundle[b].texMods[tm].translate,	//union scroll into translate
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords(p_stage->bundle[b].texMods[tm].translate,		//union scroll into translate
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			case TMOD_STRETCH:
				RB_CalcStretchTexCoords(&p_stage->bundle[b].texMods[tm].wave,
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords(&p_stage->bundle[b].texMods[tm],
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords(p_stage->bundle[b].texMods[tm].translate[0], //union rotateSpeed into translate[0]
					reinterpret_cast<float*>(tess.svars.texcoords[b]));
				break;

			default:
				Com_Error(ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", p_stage->bundle[b].texMods[tm].type, tess.shader->name);
			}
		}
	}
}

void ForceAlpha(unsigned char* dst_colors, const int tr_force_ent_alpha)
{
	dst_colors += 3;

	for (int i = 0; i < tess.numVertexes; i++, dst_colors += 4)
	{
		*dst_colors = tr_force_ent_alpha;
	}
}

/*
** RB_IterateStagesGeneric
*/
#ifndef JK2_MODE
static vec4_t	GLFogOverrideColors[GLFOGOVERRIDE_MAX] =
{
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_NONE
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_BLACK
	{ 1.0, 1.0, 1.0, 1.0 }	// GLFOGOVERRIDE_WHITE
};

static const float logtestExp2 = sqrt(-log(1.0 / 255.0));
#endif
extern bool tr_stencilled; //tr_backend.cpp
static void RB_IterateStagesGeneric(const shaderCommands_t* input)
{
#ifndef JK2_MODE
	bool	use_gl_fog = false;
	bool	fog_color_change = false;
	const fog_t* fog = nullptr;

	if (tess.fogNum && tess.shader->fogPass && (tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs)
		&& r_drawfog->value == 2)
	{	// only gl fog global fog and the "special fog"
		fog = tr.world->fogs + tess.fogNum;

		if (tr.rangedFog)
		{ //ranged fog, used for sniper scope
			float f_start = fog->parms.depthForOpaque;
			float f_end = tr.distanceCull;

			if (tr.rangedFog < 0.0f)
			{ //special designer override
				f_start = -tr.rangedFog;
				f_end = fog->parms.depthForOpaque;

				if (f_start >= f_end)
				{
					f_start = f_end - 1.0f;
				}
			}
			else
			{
				//the greater tr.rangedFog is, the more fog we will get between the view point and cull distance
				if (tr.distanceCull - f_start < tr.rangedFog)
				{ //assure a minimum range between fog beginning and cutoff distance
					f_start = tr.distanceCull - tr.rangedFog;

					if (f_start < 16.0f)
					{
						f_start = 16.0f;
					}
				}
			}

			qglFogi(GL_FOG_MODE, GL_LINEAR);
			qglFogf(GL_FOG_START, f_start);
			qglFogf(GL_FOG_END, f_end);
		}
		else
		{
			qglFogi(GL_FOG_MODE, GL_EXP2);
			qglFogf(GL_FOG_DENSITY, logtestExp2 / fog->parms.depthForOpaque);
		}

		if (g_bRenderGlowingObjects)
		{
			constexpr float fog_color[3] = { 0.0f, 0.0f, 0.0f };
			qglFogfv(GL_FOG_COLOR, fog_color);
		}
		else
		{
			qglFogfv(GL_FOG_COLOR, fog->parms.color);
		}

		qglEnable(GL_FOG);
		use_gl_fog = true;
	}
#endif

	for (int stage = 0; stage < input->shader->numUnfoggedPasses; stage++)
	{
		shaderStage_t* p_stage = &tess.xstages[stage];
		if (!p_stage->active)
		{
			break;
		}

		// Reject this stage if it's not a glow stage but we are doing a glow pass.
		if (g_bRenderGlowingObjects && !p_stage->glow)
		{
			continue;
		}

		int	stateBits = p_stage->stateBits;
		auto	force_alpha_gen = static_cast<alphaGen_t>(0);
		auto	force_rgb_gen = static_cast<colorGen_t>(0);

		// allow skipping out to show just lightmaps during development
		if (stage && r_lightmap->integer)
		{
			if (!(p_stage->bundle[0].isLightmap || p_stage->bundle[1].isLightmap || p_stage->bundle[0].vertexLightmap))
			{
				continue;	// need to keep going in case the LM is in a later stage
			}
			stateBits = GLS_DSTBLEND_ZERO | GLS_SRCBLEND_ONE;	//we want to replace the prior stages with this LM, not blend
		}

		if (backEnd.currentEntity)
		{
			if (backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1)
			{
				// we want to be able to rip a hole in the thing being disintegrated, and by doing the depth-testing it avoids some kinds of artefacts, but will probably introduce others?
				//	NOTE: adjusting the alphaFunc seems to help a bit
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_C0;
			}

			if (backEnd.currentEntity->e.renderfx & RF_ALPHA_FADE)
			{
				if (backEnd.currentEntity->e.shaderRGBA[3] < 255)
				{
					stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
					force_alpha_gen = AGEN_ENTITY;
				}
			}

			if (backEnd.currentEntity->e.renderfx & RF_RGB_TINT)
			{//want to use RGBGen from ent
				force_rgb_gen = CGEN_ENTITY;
			}
		}

		if (p_stage->ss && p_stage->ss->surfaceSpriteType)
		{
			// We check for surfacesprites AFTER drawing everything else
			continue;
		}

#ifndef JK2_MODE
		if (use_gl_fog)
		{
			if (p_stage->mGLFogColorOverride)
			{
				qglFogfv(GL_FOG_COLOR, GLFogOverrideColors[p_stage->mGLFogColorOverride]);
				fog_color_change = true;
			}
			else if (fog_color_change && fog)
			{
				fog_color_change = false;
				qglFogfv(GL_FOG_COLOR, fog->parms.color);
			}
		}
#endif

		if (!input->fading)
		{ //this means ignore this, while we do a fade-out
			ComputeColors(p_stage, force_alpha_gen, force_rgb_gen);
		}
		ComputeTexCoords(p_stage);

		if (!setArraysOnce)
		{
			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, input->svars.colors);
		}

		if (p_stage->bundle[0].isLightmap && r_debugStyle->integer >= 0)
		{
			if (p_stage->lightmapStyle != r_debugStyle->integer)
			{
				if (p_stage->lightmapStyle == 0)
				{
					GL_State(GLS_DSTBLEND_ZERO | GLS_SRCBLEND_ZERO);
					R_DrawElements(input->numIndexes, input->indexes);
				}
				continue;
			}
		}

		//
		// do multitexture
		//
		if (p_stage->bundle[1].image != nullptr)
		{
			DrawMultitextured(input, stage);
		}
		else
		{
			static bool l_stencilled = false;

			if (!setArraysOnce)
			{
				qglTexCoordPointer(2, GL_FLOAT, 0, input->svars.texcoords[0]);
			}

			//
			// set state
			//
			if (tess.shader == tr.distortionShader ||
				backEnd.currentEntity && backEnd.currentEntity->e.renderfx & RF_DISTORTION)
			{ //special distortion effect -rww
				//tr.screenImage should have been set for this specific entity before we got in here.
				GL_Bind(tr.screenImage);
				GL_Cull(CT_TWO_SIDED);
			}
			else if (p_stage->bundle[0].vertexLightmap && r_vertexLight->integer && r_lightmap->integer)
			{
				GL_Bind(tr.whiteImage);
			}
			else
				R_BindAnimatedImage(&p_stage->bundle[0]);

			if (tess.shader == tr.distortionShader &&
				glConfig.stencilBits >= 4)
			{ //draw it to the stencil buffer!
				tr_stencilled = true;
				l_stencilled = true;
				qglEnable(GL_STENCIL_TEST);
				qglStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

				//don't depthmask, don't blend.. don't do anything
				GL_State(0);
			}
			else if (backEnd.currentEntity && backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA)
			{
				ForceAlpha(reinterpret_cast<unsigned char*>(tess.svars.colors), backEnd.currentEntity->e.shaderRGBA[3]);

				if (backEnd.currentEntity->e.renderfx & RF_ALPHA_DEPTH)
				{ //depth write, so faces through the model will be stomped over by nearer ones. this works because
					//we draw RF_FORCE_ENT_ALPHA stuff after everything else, including standard alpha surfs.
					GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE);
				}
				else
				{
					GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
				}
			}
			else
			{
				GL_State(stateBits);
			}

			//
			// draw
			//
			R_DrawElements(input->numIndexes, input->indexes);

			if (l_stencilled)
			{ //re-enable the color buffer, disable stencil test
				l_stencilled = false;
				qglDisable(GL_STENCIL_TEST);
				qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
	}
#ifndef JK2_MODE
	if (fog_color_change)
	{
		qglFogfv(GL_FOG_COLOR, fog->parms.color);
	}
#endif
}

/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric()
{
	shaderCommands_t* input = &tess;

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if (r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name));
	}

	//
	// set face culling appropriately
	//
	GL_Cull(input->shader->cullType);

	// set polygon offset if necessary
	if (input->shader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if (tess.numPasses > 1 || input->shader->multitextureEnv)
	{
		setArraysOnce = qfalse;
		qglDisableClientState(GL_COLOR_ARRAY);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		setArraysOnce = qtrue;

		qglEnableClientState(GL_COLOR_ARRAY);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, tess.svars.colors);

		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texcoords[0]);
	}

	//
	// lock XYZ
	//
	qglVertexPointer(3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment("glLockArraysEXT\n");
	}

	//
	// enable color and texcoord arrays after the lock if necessary
	//
	if (!setArraysOnce)
	{
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglEnableClientState(GL_COLOR_ARRAY);
	}

	//
	// call shader function
	//
	RB_IterateStagesGeneric(input);

	//
	// now do any dynamic lighting needed
	//
	if (tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY))) {
		if (r_dlightStyle->integer > 0)
		{
			ProjectDlightTexture2();
		}
		else
		{
			ProjectDlightTexture();
		}
	}

	//
	// now do fog
	//
#ifdef JK2_MODE
	if (tess.fogNum && tess.shader->fogPass && r_drawfog->value)
#else
	if (tr.world && (tess.fogNum != tr.world->globalFog || r_drawfog->value != 2) && r_drawfog->value && tess.fogNum && tess.shader->fogPass)
#endif
	{
		RB_FogPass();
	}

	//
	// unlock arrays
	//
	if (qglUnlockArraysEXT)
	{
		qglUnlockArraysEXT();
		GLimp_LogComment("glUnlockArraysEXT\n");
	}

	//
	// reset polygon offset
	//
	if (input->shader->polygonOffset)
	{
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for (int stage = 1; stage < tess.shader->numUnfoggedPasses; stage++)
		{
			if (tess.xstages[stage].ss && tess.xstages[stage].ss->surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites(&tess.xstages[stage], input);
			}
		}
	}

#ifndef JK2_MODE
	//don't disable the hardware fog til after we do surface sprites
	if (r_drawfog->value == 2 &&
		tess.fogNum && tess.shader->fogPass &&
		(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
	{
		qglDisable(GL_FOG);
	}
#endif
}

/*
** RB_EndSurface
*/
void RB_EndSurface()
{
	const shaderCommands_t* input = &tess;

	if (input->numIndexes == 0)
	{
		return;
	}

	if (input->indexes[SHADER_MAX_INDEXES - 1] != 0)
	{
		Com_Error(ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES indexes hit");
	}
	if (input->xyz[SHADER_MAX_VERTEXES - 1][0] != 0)
	{
		Com_Error(ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES xyz hit");
	}

	if (tess.shader == tr.shadowShader) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if (r_debugSort->integer && r_debugSort->integer < tess.shader->sort) {
		return;
	}

	if (skyboxportal)
	{
		// world
		if (!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL))
		{
			if (tess.currentStageIteratorFunc == RB_StageIteratorSky)
			{	// don't process these tris at all
				return;
			}
		}
		// portal sky
		else
		{
			if (!drawskyboxportal)
			{
				if (tess.currentStageIteratorFunc != RB_StageIteratorSky)
				{	// /only/ process sky tris
					return;
				}
			}
		}
	}

	//
	// update performance counters
	//
	if (!backEnd.projection2D)
	{
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes;
		backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;
#ifdef JK2_MODE
		if (tess.fogNum && tess.shader->fogPass && r_drawfog->value)
#else
		if (tess.fogNum && tess.shader->fogPass && r_drawfog->value == 1)
#endif
		{	// Fogging adds an additional pass
			backEnd.pc.c_totalIndexes += tess.numIndexes;
		}
	}

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if (r_showtris->integer)
	{
		DrawTris(input);
	}

	if (r_shownormals->integer) {
		DrawNormals(input);
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

	GLimp_LogComment("----------\n");
}