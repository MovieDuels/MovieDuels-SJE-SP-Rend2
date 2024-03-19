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

#include "tr_local.h"
#include "glext.h"
#include "tr_WorldEffects.h"

backEndData_t* backEndData;
backEndState_t	backEnd;

bool tr_stencilled = false;
extern qboolean tr_distortionPrePost; //tr_shadows.cpp
extern qboolean tr_distortionNegate; //tr_shadows.cpp
extern void RB_CaptureScreenImage(void); //tr_shadows.cpp
extern void RB_DistortionFill(void); //tr_shadows.cpp
static void RB_DrawGlowOverlay();
static void RB_BlurGlowTexture();

// Whether we are currently rendering only glowing objects or not.
bool g_bRenderGlowingObjects = false;

// Whether the current hardware supports dynamic glows/flares.
bool g_bDynamicGlowSupported = false;

extern void R_RotateForViewer(void);
extern void R_SetupFrustum(void);

static const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

/*
** GL_Bind
*/
void GL_Bind(image_t* image) {
	int texnum;

	if (!image) {
		ri->Printf(PRINT_ALL, S_COLOR_YELLOW  "GL_Bind: NULL image\n");
		texnum = tr.defaultImage->texnum;
	}
	else {
		texnum = image->texnum;
	}

	if (r_nobind->integer && tr.dlightImage) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if (glState.currenttextures[glState.currenttmu] != texnum) {
		image->frameUsed = tr.frameCount;
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture(GL_TEXTURE_2D, texnum);
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture(int unit)
{
	if (glState.currenttmu == unit)
	{
		return;
	}

	if (unit == 0)
	{
		qglActiveTextureARB(GL_TEXTURE0_ARB);
		GLimp_LogComment("glActiveTextureARB( GL_TEXTURE0_ARB )\n");
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		GLimp_LogComment("glClientActiveTextureARB( GL_TEXTURE0_ARB )\n");
	}
	else if (unit == 1)
	{
		qglActiveTextureARB(GL_TEXTURE1_ARB);
		GLimp_LogComment("glActiveTextureARB( GL_TEXTURE1_ARB )\n");
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		GLimp_LogComment("glClientActiveTextureARB( GL_TEXTURE1_ARB )\n");
	}
	else if (unit == 2)
	{
		qglActiveTextureARB(GL_TEXTURE2_ARB);
		GLimp_LogComment("glActiveTextureARB( GL_TEXTURE2_ARB )\n");
		qglClientActiveTextureARB(GL_TEXTURE2_ARB);
		GLimp_LogComment("glClientActiveTextureARB( GL_TEXTURE2_ARB )\n");
	}
	else if (unit == 3)
	{
		qglActiveTextureARB(GL_TEXTURE3_ARB);
		GLimp_LogComment("glActiveTextureARB( GL_TEXTURE3_ARB )\n");
		qglClientActiveTextureARB(GL_TEXTURE3_ARB);
		GLimp_LogComment("glClientActiveTextureARB( GL_TEXTURE3_ARB )\n");
	}
	else {
		Com_Error(ERR_DROP, "GL_SelectTexture: unit = %i", unit);
	}

	glState.currenttmu = unit;
}

/*
** GL_Cull
*/
void GL_Cull(int cull_type) {
	if (glState.faceCulling == cull_type) {
		return;
	}
	glState.faceCulling = cull_type;
	if (backEnd.projection2D) {	//don't care, we're in 2d when it's always disabled
		return;
	}

	if (cull_type == CT_TWO_SIDED)
	{
		qglDisable(GL_CULL_FACE);
	}
	else
	{
		qglEnable(GL_CULL_FACE);

		if (cull_type == CT_BACK_SIDED)
		{
			if (backEnd.viewParms.isMirror)
			{
				qglCullFace(GL_FRONT);
			}
			else
			{
				qglCullFace(GL_BACK);
			}
		}
		else
		{
			if (backEnd.viewParms.isMirror)
			{
				qglCullFace(GL_BACK);
			}
			else
			{
				qglCullFace(GL_FRONT);
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv(int env)
{
	if (env == glState.texEnv[glState.currenttmu])
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;

	switch (env)
	{
	case GL_MODULATE:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		break;
	case GL_REPLACE:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		break;
	case GL_DECAL:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		break;
	case GL_ADD:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
		break;
	default:
		Com_Error(ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env);
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State(uint32_t state_bits)
{
	const uint32_t diff = state_bits ^ glState.glStateBits;

	if (!diff)
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if (diff & GLS_DEPTHFUNC_EQUAL)
	{
		if (state_bits & GLS_DEPTHFUNC_EQUAL)
		{
			qglDepthFunc(GL_EQUAL);
		}
		else
		{
			qglDepthFunc(GL_LEQUAL);
		}
	}

	//
	// check blend bits
	//
	if (diff & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
	{
		GLenum srcFactor, dstFactor;

		if (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
		{
			switch (state_bits & GLS_SRCBLEND_BITS)
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				Com_Error(ERR_DROP, "GL_State: invalid src blend state bits\n");
			}

			switch (state_bits & GLS_DSTBLEND_BITS)
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				Com_Error(ERR_DROP, "GL_State: invalid dst blend state bits\n");
			}

			qglEnable(GL_BLEND);
			qglBlendFunc(srcFactor, dstFactor);
		}
		else
		{
			qglDisable(GL_BLEND);
		}
	}

	//
	// check depthmask
	//
	if (diff & GLS_DEPTHMASK_TRUE)
	{
		if (state_bits & GLS_DEPTHMASK_TRUE)
		{
			qglDepthMask(GL_TRUE);
		}
		else
		{
			qglDepthMask(GL_FALSE);
		}
	}

	//
	// fill/line mode
	//
	if (diff & GLS_POLYMODE_LINE)
	{
		if (state_bits & GLS_POLYMODE_LINE)
		{
			qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else
		{
			qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}

	//
	// depthtest
	//
	if (diff & GLS_DEPTHTEST_DISABLE)
	{
		if (state_bits & GLS_DEPTHTEST_DISABLE)
		{
			qglDisable(GL_DEPTH_TEST);
		}
		else
		{
			qglEnable(GL_DEPTH_TEST);
		}
	}

	//
	// alpha test
	//
	if (diff & GLS_ATEST_BITS)
	{
		switch (state_bits & GLS_ATEST_BITS)
		{
		case 0:
			qglDisable(GL_ALPHA_TEST);
			break;
		case GLS_ATEST_GT_0:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GREATER, 0.0f);
			break;
		case GLS_ATEST_LT_80:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.5f);
			break;
		case GLS_ATEST_GE_80:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.5f);
			break;
		case GLS_ATEST_GE_C0:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.75f);
			break;
		default:
			assert(0);
			break;
		}
	}

	glState.glStateBits = state_bits;
}

/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace(void) {
	if (!backEnd.isHyperspace) {
		// do initialization shit
	}

	const float c = (backEnd.refdef.time & 255) / 255.0f;
	qglClearColor(c, c, c, 1);
	qglClear(GL_COLOR_BUFFER_BIT);

	backEnd.isHyperspace = qtrue;
}

void SetViewportAndScissor(void) {
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
	qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView(void) {
	int clearBits = GL_DEPTH_BUFFER_BIT;

	// sync with gl if needed
	if (r_finish->integer == 1 && !glState.finishCalled) {
		qglFinish();
		glState.finishCalled = qtrue;
	}
	if (r_finish->integer == 0) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State(GLS_DEFAULT);

	// clear relevant buffers
	if (r_measureOverdraw->integer || r_shadows->integer == 2 || tr_stencilled)
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
		tr_stencilled = false;
	}

	if (skyboxportal)
	{
		if (backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)
		{	// portal scene, clear whatever is necessary
			if (r_fastsky->integer || (backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
			{	// fastsky: clear color
				// try clearing first with the portal sky fog color, then the world fog color, then finally a default
				clearBits |= GL_COLOR_BUFFER_BIT;
				//rwwFIXMEFIXME: Clear with fog color if there is one
				qglClearColor(0.5, 0.5, 0.5, 1.0);
			}
		}
	}
	else
	{
		if (r_fastsky->integer && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && !g_bRenderGlowingObjects)
		{
			clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
			qglClearColor(0.8f, 0.7f, 0.4f, 1.0f);	// FIXME: get color of sky
#else
			qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// FIXME: get color of sky
#endif
		}
	}

	if (tr.refdef.rdflags & RDF_AUTOMAP || (!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && r_DynamicGlow->integer && !g_bRenderGlowingObjects))
	{
		if (tr.world && tr.world->globalFog != -1)
		{ //this is because of a bug in multiple scenes I think, it needs to clear for the second scene but it doesn't normally.
			const fog_t* fog = &tr.world->fogs[tr.world->globalFog];

			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor(fog->parms.color[0], fog->parms.color[1], fog->parms.color[2], 1.0f);
		}
	}

	// If this pass is to just render the glowing objects, don't clear the depth buffer since
	// we're sharing it with the main scene (since the main scene has already been rendered). -AReis
	if (g_bRenderGlowingObjects)
	{
		clearBits &= ~GL_DEPTH_BUFFER_BIT;
	}

	if (clearBits)
	{
		qglClear(clearBits);
	}

	if ((backEnd.refdef.rdflags & RDF_HYPERSPACE))
	{
		RB_Hyperspace();
		return;
	}
	backEnd.isHyperspace = qfalse;

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if (backEnd.viewParms.is_portal) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct(backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct(backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct(backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct(plane, backEnd.viewParms.ori.origin) - plane[3];

		qglLoadMatrixf(s_flipMatrix);
		qglClipPlane(GL_CLIP_PLANE0, plane2);
		qglEnable(GL_CLIP_PLANE0);
	}
	else {
		qglDisable(GL_CLIP_PLANE0);
	}
}

#define	MAC_EVENT_PUMP_MSEC		5

//used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoordFloat(vec3_t world_coord, float* x, float* y)
{
	vec3_t	local, transformed;
	vec3_t	vfwd;
	vec3_t	vright;
	vec3_t	vup;

	const int xcenter = glConfig.vidWidth / 2;
	const int ycenter = glConfig.vidHeight / 2;

	//AngleVectors (tr.refdef.viewangles, vfwd, vright, vup);
	VectorCopy(tr.refdef.viewaxis[0], vfwd);
	VectorCopy(tr.refdef.viewaxis[1], vright);
	VectorCopy(tr.refdef.viewaxis[2], vup);

	VectorSubtract(world_coord, tr.refdef.vieworg, local);

	transformed[0] = DotProduct(local, vright);
	transformed[1] = DotProduct(local, vup);
	transformed[2] = DotProduct(local, vfwd);

	// Make sure Z is not negative.
	if (transformed[2] < 0.01)
	{
		return false;
	}

	const float xzi = xcenter / transformed[2] * (90.0 / tr.refdef.fov_x);
	const float yzi = ycenter / transformed[2] * (90.0 / tr.refdef.fov_y);

	*x = xcenter + xzi * transformed[0];
	*y = ycenter - yzi * transformed[1];

	return true;
}

//used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoord(vec3_t world_coord, int* x, int* y)
{
	float	xF, yF;
	const bool retVal = R_WorldCoordToScreenCoordFloat(world_coord, &xF, &yF);
	*x = static_cast<int>(xF);
	*y = static_cast<int>(yF);
	return retVal;
}

/*
==================
RB_RenderDrawSurfList
==================
*/
//number of possible surfs we can postrender.
//note that postrenders lack much of the optimization that the standard sort-render crap does,
//so it's slower.
#define MAX_POST_RENDERS	128

typedef struct postRender_s {
	int			fogNum;
	int			ent_num;
	int			dlighted;
	int			depthRange;
	drawSurf_t* draw_surf;
	shader_t* shader;
	qboolean	eValid;
} postRender_t;

static postRender_t g_postRenders[MAX_POST_RENDERS];
static int g_numPostRenders = 0;

#if 0
//get the "average" (ideally center) position of a surface on the tess.
//this is a kind of lame method because I can't think correctly right now.
static inline bool R_AverageTessXYZ(vec3_t dest)
{
	int i = 1;
	float bd = 0.0f;
	float d = 0.0f;
	int b = -1;
	vec3_t v;

	while (i < tess.numVertexes)
	{
		VectorSubtract(tess.xyz[i], tess.xyz[i], v);
		d = VectorLength(v);
		if (b == -1 || d < bd)
		{
			b = i;
			bd = d;
		}
		i++;
	}
	if (b != -1)
	{
		VectorSubtract(tess.xyz[0], tess.xyz[b], v);

		VectorScale(v, 0.5f, dest);
		VectorAdd(dest, tess.xyz[0], dest);

		return true;
	}

	return false;
}
#endif

void RB_RenderDrawSurfList(drawSurf_t* draw_surfs, int num_draw_surfs) {
	shader_t* shader;
	int				fogNum;
	int				entity_num;
	int				dlighted;
	int				i;
	drawSurf_t* draw_surf;
	postRender_t* pRender;
	bool			didShadowPass = false;

	if (g_bRenderGlowingObjects)
	{ //only shadow on initial passes
		didShadowPass = true;
	}

	// save original time for entity shader offsets
	const float originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	// draw everything
	int oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	shader_t* oldShader = nullptr;
	int oldFogNum = -1;
	int oldDepthRange = qfalse;
	int oldDlighted = qfalse;
	unsigned int oldSort = static_cast<unsigned>(-1);
	int depthRange = qfalse;

	backEnd.pc.c_surfaces += num_draw_surfs;

	for (i = 0, draw_surf = draw_surfs; i < num_draw_surfs; i++, draw_surf++)
	{
		if (draw_surf->sort == oldSort)
		{
			// fast path, same as previous sort
			rb_surfaceTable[*draw_surf->surface](draw_surf->surface);
			continue;
		}
		R_DecomposeSort(draw_surf->sort, &entity_num, &shader, &fogNum, &dlighted);

		// If we're rendering glowing objects, but this shader has no stages with glow, skip it!
		if (g_bRenderGlowingObjects && !shader->hasGlow)
		{
			shader = oldShader;
			entity_num = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;
			continue;
		}

		oldSort = draw_surf->sort;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (entity_num != REFENTITYNUM_WORLD &&
			g_numPostRenders < MAX_POST_RENDERS)
		{
			if ((backEnd.refdef.entities[entity_num].e.renderfx & RF_DISTORTION) ||
				(backEnd.refdef.entities[entity_num].e.renderfx & RF_FORCEPOST) ||
				(backEnd.refdef.entities[entity_num].e.renderfx & RF_FORCE_ENT_ALPHA))
			{ //must render last
				const trRefEntity_t* curEnt = &backEnd.refdef.entities[entity_num];
				pRender = &g_postRenders[g_numPostRenders];

				g_numPostRenders++;

				depthRange = 0;
				//figure this stuff out now and store it
				if (curEnt->e.renderfx & RF_NODEPTH)
				{
					depthRange = 2;
				}
				else if (curEnt->e.renderfx & RF_DEPTHHACK)
				{
					depthRange = 1;
				}
				pRender->depthRange = depthRange;

				//It is not necessary to update the old* values because
				//we are not updating now with the current values.
				depthRange = oldDepthRange;

				//store off the ent num
				pRender->ent_num = entity_num;

				//remember the other values necessary for rendering this surf
				pRender->draw_surf = draw_surf;
				pRender->dlighted = dlighted;
				pRender->fogNum = fogNum;
				pRender->shader = shader;

				/*
				if (shader == tr.distortionShader)
				{
					pRender->eValid = qfalse;
				}
				else
				*/
				{
					pRender->eValid = qtrue;
				}

				//assure the info is back to the last set state
				shader = oldShader;
				entity_num = oldEntityNum;
				fogNum = oldFogNum;
				dlighted = oldDlighted;

				oldSort = -20; //invalidate this thing, cause we may want to postrender more surfs of the same sort

				//continue without bothering to begin a draw surf
				continue;
			}
		}
		/*
		else if (shader == tr.distortionShader &&
			g_numPostRenders < MAX_POST_RENDERS)
		{ //not an ent, just a surface that needs this effect
			pRender = &g_postRenders[g_numPostRenders];

			g_numPostRenders++;

			depthRange = 0;
			pRender->depthRange = depthRange;

			//It is not necessary to update the old* values because
			//we are not updating now with the current values.
			depthRange = oldDepthRange;

			//store off the ent num
			pRender->ent_num = entity_num;

			//remember the other values necessary for rendering this surf
			pRender->draw_surf = draw_surf;
			pRender->dlighted = dlighted;
			pRender->fogNum = fogNum;
			pRender->shader = shader;

			pRender->eValid = qfalse;

			//assure the info is back to the last set state
			shader = oldShader;
			entity_num = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;

			oldSort = -20; //invalidate this thing, cause we may want to postrender more surfs of the same sort

			//continue without bothering to begin a draw surf
			continue;
		}
		*/

		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted
			|| (entity_num != oldEntityNum && !shader->entityMergable))
		{
			if (oldShader != nullptr) {
				RB_EndSurface();

				if (!didShadowPass && shader && shader->sort > SS_BANNER)
				{
					RB_ShadowFinish();
					didShadowPass = true;
				}
			}
			RB_BeginSurface(shader, fogNum);
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if (entity_num != oldEntityNum) {
			depthRange = 0;

			if (entity_num != REFENTITYNUM_WORLD) {
				backEnd.currentEntity = &backEnd.refdef.entities[entity_num];

				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori);

				// set up the dynamic lighting if needed
				if (backEnd.currentEntity->needDlights) {
					R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
				}

				if (backEnd.currentEntity->e.renderfx & RF_NODEPTH) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = 2;
				}
				else if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = 1;
				}
			}
			else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
			}

			qglLoadMatrixf(backEnd.ori.model_matrix);

			//
			// change depthrange if needed
			//
			if (oldDepthRange != depthRange) {
				switch (depthRange) {
				default:
				case 0:
					qglDepthRange(0, 1);
					break;

				case 1:
					qglDepthRange(0, .3);
					break;

				case 2:
					qglDepthRange(0, 0);
					break;
				}

				oldDepthRange = depthRange;
			}

			oldEntityNum = entity_num;
		}

		// add the triangles for this surface
		rb_surfaceTable[*draw_surf->surface](draw_surf->surface);
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	//assert(entity_num < MAX_GENTITIES);

	if (oldShader != nullptr) {
		RB_EndSurface();
	}

#ifdef _CRAZY_ATTRIB_DEBUG
	qglPopAttrib();
	glState.glStateBits = -1;
#endif

	if (tr_stencilled && tr_distortionPrePost)
	{ //ok, cap it now
		RB_CaptureScreenImage();
		RB_DistortionFill();
	}

	//render distortion surfs (or anything else that needs to be post-rendered)
	if (g_numPostRenders > 0)
	{
		int lastPostEnt = -1;

		while (g_numPostRenders > 0)
		{
			g_numPostRenders--;
			pRender = &g_postRenders[g_numPostRenders];

			RB_BeginSurface(pRender->shader, pRender->fogNum);

			/*
			if (!pRender->eValid && pRender->ent_num == REFENTITYNUM_WORLD)
			{ //world/other surface
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}
			else
			*/
			{ //ent
				backEnd.currentEntity = &backEnd.refdef.entities[pRender->ent_num];

				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori);

				// set up the dynamic lighting if needed
				if (backEnd.currentEntity->needDlights) {
					R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
				}
			}

			qglLoadMatrixf(backEnd.ori.model_matrix);

			depthRange = pRender->depthRange;
			switch (depthRange)
			{
			default:
			case 0:
				qglDepthRange(0, 1);
				break;

			case 1:
				qglDepthRange(0, .3);
				break;

			case 2:
				qglDepthRange(0, 0);
				break;
			}

			/*
			if (!pRender->eValid)
			{ //special full-screen "distortion" (or refraction or whatever the heck you want to call it)
				if (!tr_stencilled)
				{ //only need to do this once every frame (that a surface using it is around)
					int radX = 2048;
					int radY = 2048;
					int x = glConfig.vidWidth/2;
					int y = glConfig.vidHeight/2;
					int cX, cY;

					GL_Bind( tr.screenImage );
					//using this method, we could pixel-filter the texture and all sorts of crazy stuff.
					//but, it is slow as hell.
#if 0
					static byte *tmp = NULL;
					if (!tmp)
					{
						tmp = (byte *)Z_Malloc((sizeof(byte)*4)*(glConfig.vidWidth*glConfig.vidHeight), TAG_ICARUS, qtrue);
					}
					qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
					qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
#endif

					if (radX > glConfig.maxTextureSize)
					{
						radX = glConfig.maxTextureSize;
					}
					if (radY > glConfig.maxTextureSize)
					{
						radY = glConfig.maxTextureSize;
					}

					while (glConfig.vidWidth < radX)
					{
						radX /= 2;
					}
					while (glConfig.vidHeight < radY)
					{
						radY /= 2;
					}

					cX = x-(radX/2);
					cY = y-(radY/2);

					if (cX+radX > glConfig.vidWidth)
					{ //would it go off screen?
						cX = glConfig.vidWidth-radX;
					}
					else if (cX < 0)
					{ //cap it off at 0
						cX = 0;
					}

					if (cY+radY > glConfig.vidHeight)
					{ //would it go off screen?
						cY = glConfig.vidHeight-radY;
					}
					else if (cY < 0)
					{ //cap it off at 0
						cY = 0;
					}

					qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, cX, cY, radX, radY, 0);
				}
				lastPostEnt = ENTITYNUM_NONE;
			}
			*/
			if (!pRender->eValid)
			{
			}
			else if ((backEnd.refdef.entities[pRender->ent_num].e.renderfx & RF_DISTORTION) &&
				lastPostEnt != pRender->ent_num)
			{ //do the capture now, we only need to do it once per ent
				int x, y;
				int rad;
				bool r;
				//We are going to just bind this, and then the CopyTexImage is going to
				//stomp over this texture num in texture memory.
				GL_Bind(tr.screenImage);

#if 0 //yeah.. this kinda worked but it was stupid
				if (pRender->eValid)
				{
					r = R_WorldCoordToScreenCoord(backEnd.currentEntity->e.origin, &x, &y);
					rad = backEnd.currentEntity->e.radius;
				}
				else
				{
					vec3_t v;
					//probably a little bit expensive.. but we're doing this for looks, not speed!
					if (!R_AverageTessXYZ(v))
					{ //failed, just use first vert I guess
						VectorCopy(tess.xyz[0], v);
					}
					r = R_WorldCoordToScreenCoord(v, &x, &y);
					rad = 256;
				}
#else
				r = R_WorldCoordToScreenCoord(backEnd.currentEntity->e.origin, &x, &y);
				rad = backEnd.currentEntity->e.radius;
#endif

				if (r)
				{
					int cX = glConfig.vidWidth - x - (rad / 2);
					int cY = glConfig.vidHeight - y - (rad / 2);

					if (cX + rad > glConfig.vidWidth)
					{ //would it go off screen?
						cX = glConfig.vidWidth - rad;
					}
					else if (cX < 0)
					{ //cap it off at 0
						cX = 0;
					}

					if (cY + rad > glConfig.vidHeight)
					{ //would it go off screen?
						cY = glConfig.vidHeight - rad;
					}
					else if (cY < 0)
					{ //cap it off at 0
						cY = 0;
					}

					//now copy a portion of the screen to this texture
					qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, cX, cY, rad, rad, 0);

					lastPostEnt = pRender->ent_num;
				}
			}

			rb_surfaceTable[*pRender->draw_surf->surface](pRender->draw_surf->surface);
			RB_EndSurface();
		}
	}

	// go back to the world modelview matrix
	qglLoadMatrixf(backEnd.viewParms.world.model_matrix);
	if (depthRange) {
		qglDepthRange(0, 1);
	}

#if 0
	RB_DrawSun();
#endif
	if (tr_stencilled && !tr_distortionPrePost)
	{ //draw in the stencil buffer's cutout
		RB_DistortionFill();
	}
	if (!didShadowPass)
	{
		// darken down any stencil shadows
		RB_ShadowFinish();
		didShadowPass = true;
	}

	// add light flares on lights that aren't obscured

	// rww - 9-13-01 [1-26-01-sof2]
//	RB_RenderFlares();
}

/*
============================================================================

RENDER BACK END FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D(void) {
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, 640, 480, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	GL_State(GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

	qglDisable(GL_CULL_FACE);
	qglDisable(GL_CLIP_PLANE0);

	// set time for 2D shaders
	backEnd.refdef.time = ri->Milliseconds() * ri->Cvar_VariableValue("timescale");
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}

/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte* data, int client, qboolean dirty)
{
	int end;

	if (!tr.registered) {
		return;
	}
	R_IssuePendingRenderCommands();

	if (tess.num_indexes) {
		RB_EndSurface();
	}

	// we definately want to sync every frame for the cinematics
	qglFinish();

	int start = end = 0;
	if (r_speeds->integer) {
		start = ri->Milliseconds() * ri->Cvar_VariableValue("timescale");
	}

	// make sure rows and cols are powers of 2
	if ((cols & (cols - 1)) || (rows & (rows - 1)))
	{
		Com_Error(ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

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

	if (r_speeds->integer) {
		end = ri->Milliseconds() * ri->Cvar_VariableValue("timescale");
		ri->Printf(PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start);
	}

	RB_SetGL2D();

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

void RE_UploadCinematic(int cols, int rows, const byte* data, int client, qboolean dirty) {
	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if (cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height) {
		// Note: q3 has the commented sections being uploaded width/height
		tr.scratchImage[client]->width = /*tr.scratchImage[client]->width =*/ cols;
		tr.scratchImage[client]->height = /*tr.scratchImage[client]->height =*/ rows;
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

/*
=============
RB_SetColor

=============
*/
const void* RB_SetColor(const void* data) {
	const setColorCommand_t* cmd = static_cast<const setColorCommand_t*>(data);

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void*)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void* RB_StretchPic(const void* data) {
	const stretchPicCommand_t* cmd = static_cast<const stretchPicCommand_t*>(data);

	if (!backEnd.projection2D) {
		RB_SetGL2D();
	}

	shader_t* shader = cmd->shader;
	if (shader != tess.shader) {
		if (tess.num_indexes) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface(shader, 0);
	}

	RB_CHECKOVERFLOW(4, 6);
	const int num_verts = tess.numVertexes;
	const int num_indexes = tess.num_indexes;

	tess.numVertexes += 4;
	tess.num_indexes += 6;

	tess.indexes[num_indexes] = num_verts + 3;
	tess.indexes[num_indexes + 1] = num_verts + 0;
	tess.indexes[num_indexes + 2] = num_verts + 2;
	tess.indexes[num_indexes + 3] = num_verts + 2;
	tess.indexes[num_indexes + 4] = num_verts + 0;
	tess.indexes[num_indexes + 5] = num_verts + 1;

	const byteAlias_t* baSource = (byteAlias_t*)&backEnd.color2D;
	byteAlias_t* baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 0]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 1]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 2]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 3]; baDest->ui = baSource->ui;

	tess.xyz[num_verts][0] = cmd->x;
	tess.xyz[num_verts][1] = cmd->y;
	tess.xyz[num_verts][2] = 0;

	tess.texCoords[num_verts][0][0] = cmd->s1;
	tess.texCoords[num_verts][0][1] = cmd->t1;

	tess.xyz[num_verts + 1][0] = cmd->x + cmd->w;
	tess.xyz[num_verts + 1][1] = cmd->y;
	tess.xyz[num_verts + 1][2] = 0;

	tess.texCoords[num_verts + 1][0][0] = cmd->s2;
	tess.texCoords[num_verts + 1][0][1] = cmd->t1;

	tess.xyz[num_verts + 2][0] = cmd->x + cmd->w;
	tess.xyz[num_verts + 2][1] = cmd->y + cmd->h;
	tess.xyz[num_verts + 2][2] = 0;

	tess.texCoords[num_verts + 2][0][0] = cmd->s2;
	tess.texCoords[num_verts + 2][0][1] = cmd->t2;

	tess.xyz[num_verts + 3][0] = cmd->x;
	tess.xyz[num_verts + 3][1] = cmd->y + cmd->h;
	tess.xyz[num_verts + 3][2] = 0;

	tess.texCoords[num_verts + 3][0][0] = cmd->s1;
	tess.texCoords[num_verts + 3][0][1] = cmd->t2;

	return (const void*)(cmd + 1);
}

/*
=============
RB_DrawRotatePic
=============
*/
const void* RB_RotatePic(const void* data)
{
	const rotatePicCommand_t* cmd = static_cast<const rotatePicCommand_t*>(data);

	shader_t* shader = cmd->shader;
	const image_t* image = &shader->stages[0].bundle[0].image[0];

	if (image) {
		if (!backEnd.projection2D) {
			RB_SetGL2D();
		}

		shader = cmd->shader;
		if (shader != tess.shader) {
			if (tess.num_indexes) {
				RB_EndSurface();
			}
			backEnd.currentEntity = &backEnd.entity2D;
			RB_BeginSurface(shader, 0);
		}

		RB_CHECKOVERFLOW(4, 6);
		const int num_verts = tess.numVertexes;
		const int num_indexes = tess.num_indexes;

		const float angle = DEG2RAD(cmd->a);
		const float s = sinf(angle);
		const float c = cosf(angle);

		const matrix3_t m = {
			{ c, s, 0.0f },
			{ -s, c, 0.0f },
			{ cmd->x + cmd->w, cmd->y, 1.0f }
		};

		tess.numVertexes += 4;
		tess.num_indexes += 6;

		tess.indexes[num_indexes] = num_verts + 3;
		tess.indexes[num_indexes + 1] = num_verts + 0;
		tess.indexes[num_indexes + 2] = num_verts + 2;
		tess.indexes[num_indexes + 3] = num_verts + 2;
		tess.indexes[num_indexes + 4] = num_verts + 0;
		tess.indexes[num_indexes + 5] = num_verts + 1;

		const byteAlias_t* baSource = (byteAlias_t*)&backEnd.color2D;
		byteAlias_t* baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 0]; baDest->ui = baSource->ui;
		baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 1]; baDest->ui = baSource->ui;
		baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 2]; baDest->ui = baSource->ui;
		baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 3]; baDest->ui = baSource->ui;

		tess.xyz[num_verts][0] = m[0][0] * (-cmd->w) + m[2][0];
		tess.xyz[num_verts][1] = m[0][1] * (-cmd->w) + m[2][1];
		tess.xyz[num_verts][2] = 0;

		tess.texCoords[num_verts][0][0] = cmd->s1;
		tess.texCoords[num_verts][0][1] = cmd->t1;

		tess.xyz[num_verts + 1][0] = m[2][0];
		tess.xyz[num_verts + 1][1] = m[2][1];
		tess.xyz[num_verts + 1][2] = 0;

		tess.texCoords[num_verts + 1][0][0] = cmd->s2;
		tess.texCoords[num_verts + 1][0][1] = cmd->t1;

		tess.xyz[num_verts + 2][0] = m[1][0] * (cmd->h) + m[2][0];
		tess.xyz[num_verts + 2][1] = m[1][1] * (cmd->h) + m[2][1];
		tess.xyz[num_verts + 2][2] = 0;

		tess.texCoords[num_verts + 2][0][0] = cmd->s2;
		tess.texCoords[num_verts + 2][0][1] = cmd->t2;

		tess.xyz[num_verts + 3][0] = m[0][0] * (-cmd->w) + m[1][0] * (cmd->h) + m[2][0];
		tess.xyz[num_verts + 3][1] = m[0][1] * (-cmd->w) + m[1][1] * (cmd->h) + m[2][1];
		tess.xyz[num_verts + 3][2] = 0;

		tess.texCoords[num_verts + 3][0][0] = cmd->s1;
		tess.texCoords[num_verts + 3][0][1] = cmd->t2;

		return (const void*)(cmd + 1);
	}

	return (const void*)(cmd + 1);
}

/*
=============
RB_DrawRotatePic2
=============
*/
const void* RB_RotatePic2(const void* data)
{
	const rotatePicCommand_t* cmd = static_cast<const rotatePicCommand_t*>(data);

	shader_t* shader = cmd->shader;

	if (shader->numUnfoggedPasses)
	{
		const image_t* image = &shader->stages[0].bundle[0].image[0];

		if (image)
		{
			if (!backEnd.projection2D) {
				RB_SetGL2D();
			}

			shader = cmd->shader;
			if (shader != tess.shader) {
				if (tess.num_indexes) {
					RB_EndSurface();
				}
				backEnd.currentEntity = &backEnd.entity2D;
				RB_BeginSurface(shader, 0);
			}

			RB_CHECKOVERFLOW(4, 6);
			const int num_verts = tess.numVertexes;
			const int num_indexes = tess.num_indexes;

			const float angle = DEG2RAD(cmd->a);
			const float s = sinf(angle);
			const float c = cosf(angle);

			const matrix3_t m = {
				{ c, s, 0.0f },
				{ -s, c, 0.0f },
				{ cmd->x, cmd->y, 1.0f }
			};

			tess.numVertexes += 4;
			tess.num_indexes += 6;

			tess.indexes[num_indexes] = num_verts + 3;
			tess.indexes[num_indexes + 1] = num_verts + 0;
			tess.indexes[num_indexes + 2] = num_verts + 2;
			tess.indexes[num_indexes + 3] = num_verts + 2;
			tess.indexes[num_indexes + 4] = num_verts + 0;
			tess.indexes[num_indexes + 5] = num_verts + 1;

			const byteAlias_t* baSource = (byteAlias_t*)&backEnd.color2D;
			byteAlias_t* baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 0]; baDest->ui = baSource->ui;
			baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 1]; baDest->ui = baSource->ui;
			baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 2]; baDest->ui = baSource->ui;
			baDest = (byteAlias_t*)&tess.vertexColors[num_verts + 3]; baDest->ui = baSource->ui;

			tess.xyz[num_verts][0] = m[0][0] * (-cmd->w * 0.5f) + m[1][0] * (-cmd->h * 0.5f) + m[2][0];
			tess.xyz[num_verts][1] = m[0][1] * (-cmd->w * 0.5f) + m[1][1] * (-cmd->h * 0.5f) + m[2][1];
			tess.xyz[num_verts][2] = 0;

			tess.texCoords[num_verts][0][0] = cmd->s1;
			tess.texCoords[num_verts][0][1] = cmd->t1;

			tess.xyz[num_verts + 1][0] = m[0][0] * (cmd->w * 0.5f) + m[1][0] * (-cmd->h * 0.5f) + m[2][0];
			tess.xyz[num_verts + 1][1] = m[0][1] * (cmd->w * 0.5f) + m[1][1] * (-cmd->h * 0.5f) + m[2][1];
			tess.xyz[num_verts + 1][2] = 0;

			tess.texCoords[num_verts + 1][0][0] = cmd->s2;
			tess.texCoords[num_verts + 1][0][1] = cmd->t1;

			tess.xyz[num_verts + 2][0] = m[0][0] * (cmd->w * 0.5f) + m[1][0] * (cmd->h * 0.5f) + m[2][0];
			tess.xyz[num_verts + 2][1] = m[0][1] * (cmd->w * 0.5f) + m[1][1] * (cmd->h * 0.5f) + m[2][1];
			tess.xyz[num_verts + 2][2] = 0;

			tess.texCoords[num_verts + 2][0][0] = cmd->s2;
			tess.texCoords[num_verts + 2][0][1] = cmd->t2;

			tess.xyz[num_verts + 3][0] = m[0][0] * (-cmd->w * 0.5f) + m[1][0] * (cmd->h * 0.5f) + m[2][0];
			tess.xyz[num_verts + 3][1] = m[0][1] * (-cmd->w * 0.5f) + m[1][1] * (cmd->h * 0.5f) + m[2][1];
			tess.xyz[num_verts + 3][2] = 0;

			tess.texCoords[num_verts + 3][0][0] = cmd->s1;
			tess.texCoords[num_verts + 3][0][1] = cmd->t2;

			return (const void*)(cmd + 1);

#if 0
			// Hmmm, this is not too cool
			GL_State(GLS_DEPTHTEST_DISABLE |
				GLS_SRCBLEND_SRC_ALPHA |
				GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
#endif
		}
	}

	return (const void*)(cmd + 1);
}

/*
=============
RB_DrawSurfs

=============
*/
const void* RB_DrawSurfs(const void* data) {
	// finish any 2D drawing if needed
	if (tess.num_indexes) {
		RB_EndSurface();
	}

	const drawSurfsCommand_t* cmd = static_cast<const drawSurfsCommand_t*>(data);

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

	// Dynamic Glow/Flares:
	/*
		The basic idea is to render the glowing parts of the scene to an offscreen buffer, then take
		that buffer and blur it. After it is sufficiently blurred, re-apply that image back to
		the normal screen using a additive blending. To blur the scene I use a vertex program to supply
		four texture coordinate offsets that allow 'peeking' into adjacent pixels. In the register
		combiner (pixel shader), I combine the adjacent pixels using a weighting factor. - Aurelio
	*/

	// Render dynamic glowing/flaring objects.
	if (!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && g_bDynamicGlowSupported && r_DynamicGlow->integer)
	{
		// Copy the normal scene to texture.
		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_TEXTURE_RECTANGLE_ARB);
		qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage);
		qglCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);
		qglDisable(GL_TEXTURE_RECTANGLE_ARB);
		qglEnable(GL_TEXTURE_2D);

		// Just clear colors, but leave the depth buffer intact so we can 'share' it.
		qglClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		qglClear(GL_COLOR_BUFFER_BIT);

		// Render the glowing objects.
		g_bRenderGlowingObjects = true;
		RB_RenderDrawSurfList(cmd->drawSurfs, cmd->numDrawSurfs);
		g_bRenderGlowingObjects = false;

		qglFinish();

		// Copy the glow scene to texture.
		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_TEXTURE_RECTANGLE_ARB);
		qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.screenGlow);
		qglCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);
		qglDisable(GL_TEXTURE_RECTANGLE_ARB);
		qglEnable(GL_TEXTURE_2D);

		// Resize the viewport to the blur texture size.
		const int oldViewWidth = backEnd.viewParms.viewportWidth;
		const int oldViewHeight = backEnd.viewParms.viewportHeight;
		backEnd.viewParms.viewportWidth = r_DynamicGlowWidth->integer;
		backEnd.viewParms.viewportHeight = r_DynamicGlowHeight->integer;
		SetViewportAndScissor();

		// Blur the scene.
		RB_BlurGlowTexture();

		// Copy the finished glow scene back to texture.
		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_TEXTURE_RECTANGLE_ARB);
		qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.blurImage);
		qglCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
		qglDisable(GL_TEXTURE_RECTANGLE_ARB);
		qglEnable(GL_TEXTURE_2D);

		// Set the viewport back to normal.
		backEnd.viewParms.viewportWidth = oldViewWidth;
		backEnd.viewParms.viewportHeight = oldViewHeight;
		SetViewportAndScissor();
		qglClear(GL_COLOR_BUFFER_BIT);

		// Draw the glow additively over the screen.
		RB_DrawGlowOverlay();
	}

	return (const void*)(cmd + 1);
}

/*
=============
RB_DrawBuffer

=============
*/
const void* RB_DrawBuffer(const void* data) {
	const drawBufferCommand_t* cmd = static_cast<const drawBufferCommand_t*>(data);

	qglDrawBuffer(cmd->buffer);

	// clear screen for debugging
	if (tr.world && tr.world->globalFog != -1)
	{
		const fog_t* fog = &tr.world->fogs[tr.world->globalFog];

		qglClearColor(fog->parms.color[0], fog->parms.color[1], fog->parms.color[2], 1.0f);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else if (r_clear->integer) {
		int i = r_clear->integer;

		if (i == 42) {
			i = Q_irand(0, 8);
		}
		switch (i)
		{
		default:
			qglClearColor(1, 0, 0.5, 1); // default q3 pink
			break;
		case 1:
			qglClearColor(1.0, 0.0, 0.0, 1.0); //red
			break;
		case 2:
			qglClearColor(0.0, 1.0, 0.0, 1.0); //green
			break;
		case 3:
			qglClearColor(1.0, 1.0, 0.0, 1.0); //yellow
			break;
		case 4:
			qglClearColor(0.0, 0.0, 1.0, 1.0); //blue
			break;
		case 5:
			qglClearColor(0.0, 1.0, 1.0, 1.0); //cyan
			break;
		case 6:
			qglClearColor(1.0, 0.0, 1.0, 1.0); //magenta
			break;
		case 7:
			qglClearColor(1.0, 1.0, 1.0, 1.0); //white
			break;
		case 8:
			qglClearColor(0.0, 0.0, 0.0, 1.0); //black
			break;
		}
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return (const void*)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages(void) {
	image_t* image;
	//	int		start, end;

	if (!backEnd.projection2D) {
		RB_SetGL2D();
	}

	qglClear(GL_COLOR_BUFFER_BIT);

	qglFinish();

	//	start = ri->Milliseconds()*ri->Cvar_VariableValue( "timescale" );

	int i = 0;
	R_Images_StartIteration();
	while ((image = R_Images_GetNextIteration()) != nullptr)
	{
		float w = glConfig.vidWidth / 20;
		float h = glConfig.vidHeight / 15;
		const float x = i % 20 * w;
		const float y = i / 20 * h;

		// show in proportional size in mode 2
		if (r_showImages->integer == 2) {
			w *= image->width / 512.0;
			h *= image->height / 512.0;
		}

		GL_Bind(image);
		qglBegin(GL_QUADS);
		qglTexCoord2f(0, 0);
		qglVertex2f(x, y);
		qglTexCoord2f(1, 0);
		qglVertex2f(x + w, y);
		qglTexCoord2f(1, 1);
		qglVertex2f(x + w, y + h);
		qglTexCoord2f(0, 1);
		qglVertex2f(x, y + h);
		qglEnd();
		i++;
	}

	qglFinish();

	//	end = ri->Milliseconds()*ri->Cvar_VariableValue( "timescale" );
	//	ri->Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}

static void RB_GammaCorrectRender()
{
	qglPushAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	RB_SetGL2D();

	// All this fixed-function texture type enabling/disabling is ludicrous :(
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	GL_SelectTexture(0);
	qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage);
	qglCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 0, 0, glConfig.vidWidth, glConfig.vidHeight, 0);

	qglEnable(GL_TEXTURE_3D);
	GL_SelectTexture(1);
	qglBindTexture(GL_TEXTURE_3D, tr.gammaCorrectLUTImage);

	qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, tr.gammaCorrectVtxShader);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, tr.gammaCorrectPxShader);

	qglEnable(GL_VERTEX_PROGRAM_ARB);
	qglEnable(GL_FRAGMENT_PROGRAM_ARB);

	qglBegin(GL_QUADS);
	qglTexCoord2f(0.0f, 0.0f);
	qglVertex2f(-1.0f, -1.0f);

	qglTexCoord2f(0.0f, static_cast<float>(glConfig.vidHeight));
	qglVertex2f(-1.0f, 1.0f);

	qglTexCoord2f(static_cast<float>(glConfig.vidWidth), static_cast<float>(glConfig.vidHeight));
	qglVertex2f(1.0f, 1.0f);

	qglTexCoord2f(static_cast<float>(glConfig.vidWidth), 0.0f);
	qglVertex2f(1.0f, -1.0f);
	qglEnd();

	qglDisable(GL_VERTEX_PROGRAM_ARB);
	qglDisable(GL_FRAGMENT_PROGRAM_ARB);

	qglDisable(GL_TEXTURE_3D);
	GL_SelectTexture(0);

	qglPopAttrib();
}

/*
=============
RB_SwapBuffers

=============
*/
const void* RB_SwapBuffers(const void* data) {
	// finish any 2D drawing if needed
	if (tess.num_indexes) {
		RB_EndSurface();
	}

	if (glConfigExt.doGammaCorrectionWithShaders)
	{
		RB_GammaCorrectRender();
	}

	// texture swapping test
	if (r_showImages->integer) {
		RB_ShowImages();
	}

	const swapBuffersCommand_t* cmd = static_cast<const swapBuffersCommand_t*>(data);

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if (r_measureOverdraw->integer) {
		long sum = 0;

		unsigned char* stencilReadback = static_cast<unsigned char*>(Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight));
		qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback);

		for (int i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		Hunk_FreeTempMemory(stencilReadback);
	}

	if (!glState.finishCalled) {
		qglFinish();
	}

	GLimp_LogComment("***************** RB_SwapBuffers *****************\n\n\n");

	ri->WIN_Present(&window);

	backEnd.projection2D = qfalse;

	return (const void*)(cmd + 1);
}

const void* RB_WorldEffects(const void* data)
{
	const drawBufferCommand_t* cmd = static_cast<const drawBufferCommand_t*>(data);

	// Always flush the tess buffer
	if (tess.shader && tess.num_indexes)
	{
		RB_EndSurface();
	}
	RB_RenderWorldEffects();

	if (tess.shader)
	{
		RB_BeginSurface(tess.shader, tess.fogNum);
	}

	return (const void*)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands
====================
*/
extern const void* R_DrawWireframeAutomap(const void* data); //tr_world.cpp
void RB_ExecuteRenderCommands(const void* data) {
	const int t1 = ri->Milliseconds() * ri->Cvar_VariableValue("timescale");

	while (1) {
		data = PADP(data, sizeof(void*));

		switch (*static_cast<const int*>(data)) {
		case RC_SET_COLOR:
			data = RB_SetColor(data);
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic(data);
			break;
		case RC_ROTATE_PIC:
			data = RB_RotatePic(data);
			break;
		case RC_ROTATE_PIC2:
			data = RB_RotatePic2(data);
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs(data);
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer(data);
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers(data);
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd(data);
			break;
		case RC_WORLD_EFFECTS:
			data = RB_WorldEffects(data);
			break;
		case RC_AUTO_MAP:
			data = R_DrawWireframeAutomap(data);
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering
			const int t2 = ri->Milliseconds() * ri->Cvar_VariableValue("timescale");
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}
}

// What Pixel Shader type is currently active (regcoms or fragment programs).
GLuint g_uiCurrentPixelShaderType = 0x0;

// Begin using a Pixel Shader.
void BeginPixelShader(GLuint ui_type, GLuint ui_id)
{
	switch (ui_type)
	{
		// Using Register Combiners, so call the Display List that stores it.
	case GL_REGISTER_COMBINERS_NV:
	{
		// Just in case...
		if (!qglCombinerParameterfvNV)
			return;

		// Call the list with the regcom in it.
		qglEnable(GL_REGISTER_COMBINERS_NV);
		qglCallList(ui_id);

		g_uiCurrentPixelShaderType = GL_REGISTER_COMBINERS_NV;
	}
	return;

	// Using Fragment Programs, so call the program.
	case GL_FRAGMENT_PROGRAM_ARB:
	{
		// Just in case...
		if (!qglGenProgramsARB)
			return;

		qglEnable(GL_FRAGMENT_PROGRAM_ARB);
		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ui_id);

		g_uiCurrentPixelShaderType = GL_FRAGMENT_PROGRAM_ARB;
	}
	default:;
	}
}

// Stop using a Pixel Shader and return states to normal.
void EndPixelShader()
{
	if (g_uiCurrentPixelShaderType == 0x0)
		return;

	qglDisable(g_uiCurrentPixelShaderType);
}

// Hack variable for deciding which kind of texture rectangle thing to do (for some
// reason it acts different on radeon! It's against the spec!).
extern bool g_bTextureRectangleHack;

static inline void RB_BlurGlowTexture()
{
	qglDisable(GL_CLIP_PLANE0);
	GL_Cull(CT_TWO_SIDED);
	qglDisable(GL_DEPTH_TEST);

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	/////////////////////////////////////////////////////////
	// Setup vertex and pixel programs.
	/////////////////////////////////////////////////////////

	// NOTE: The 0.25 is because we're blending 4 textures (so = 1.0) and we want a relatively normalized pixel
	// intensity distribution, but this won't happen anyways if intensity is higher than 1.0.
	const float fBlurDistribution = r_DynamicGlowIntensity->value * 0.25f;
	float fBlurWeight[4] = { fBlurDistribution, fBlurDistribution, fBlurDistribution, 1.0f };

	// Enable and set the Vertex Program.
	qglEnable(GL_VERTEX_PROGRAM_ARB);
	qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, tr.glowVShader);

	// Apply Pixel Shaders.
	if (qglCombinerParameterfvNV)
	{
		BeginPixelShader(GL_REGISTER_COMBINERS_NV, tr.glowPShader);

		// Pass the blur weight to the regcom.
		qglCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV, (float*)&fBlurWeight);
	}
	else if (qglProgramEnvParameter4fARB)
	{
		BeginPixelShader(GL_FRAGMENT_PROGRAM_ARB, tr.glowPShader);

		// Pass the blur weight to the Fragment Program.
		qglProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, fBlurWeight[0], fBlurWeight[1], fBlurWeight[2], fBlurWeight[3]);
	}

	/////////////////////////////////////////////////////////
	// Set the blur texture to the 4 texture stages.
	/////////////////////////////////////////////////////////

	// How much to offset each texel by.
	float fTexelWidthOffset = 0.1f, fTexelHeightOffset = 0.1f;

	GLuint uiTex = tr.screenGlow;

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);

	qglActiveTextureARB(GL_TEXTURE2_ARB);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);

	qglActiveTextureARB(GL_TEXTURE0_ARB);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);

	/////////////////////////////////////////////////////////
	// Draw the blur passes (each pass blurs it more, increasing the blur radius ).
	/////////////////////////////////////////////////////////

	//int iTexWidth = backEnd.viewParms.viewportWidth, iTexHeight = backEnd.viewParms.viewportHeight;
	int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;

	for (int iNumBlurPasses = 0; iNumBlurPasses < r_DynamicGlowPasses->integer; iNumBlurPasses++)
	{
		// Load the Texel Offsets into the Vertex Program.
		qglProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 0, -fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f);
		qglProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 1, -fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f);
		qglProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 2, fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f);
		qglProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 3, fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f);

		// After first pass put the tex coords to the viewport size.
		if (iNumBlurPasses == 1)
		{
			// OK, very weird, but dependent on which texture rectangle extension we're using, the
			// texture either needs to be always texure correct or view correct...
			if (!g_bTextureRectangleHack)
			{
				iTexWidth = backEnd.viewParms.viewportWidth;
				iTexHeight = backEnd.viewParms.viewportHeight;
			}

			uiTex = tr.blurImage;
			qglActiveTextureARB(GL_TEXTURE3_ARB);
			qglDisable(GL_TEXTURE_2D);
			qglEnable(GL_TEXTURE_RECTANGLE_ARB);
			qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);
			qglActiveTextureARB(GL_TEXTURE2_ARB);
			qglDisable(GL_TEXTURE_2D);
			qglEnable(GL_TEXTURE_RECTANGLE_ARB);
			qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);
			qglActiveTextureARB(GL_TEXTURE1_ARB);
			qglDisable(GL_TEXTURE_2D);
			qglEnable(GL_TEXTURE_RECTANGLE_ARB);
			qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);
			qglActiveTextureARB(GL_TEXTURE0_ARB);
			qglDisable(GL_TEXTURE_2D);
			qglEnable(GL_TEXTURE_RECTANGLE_ARB);
			qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);

			// Copy the current image over.
			qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, uiTex);
			qglCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
		}

		// Draw the fullscreen quad.
		qglBegin(GL_QUADS);
		qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0, iTexHeight);
		qglVertex2f(0, 0);

		qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0, 0);
		qglVertex2f(0, backEnd.viewParms.viewportHeight);

		qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, iTexWidth, 0);
		qglVertex2f(backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

		qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, iTexWidth, iTexHeight);
		qglVertex2f(backEnd.viewParms.viewportWidth, 0);
		qglEnd();

		qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.blurImage);
		qglCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

		// Increase the texel offsets.
		// NOTE: This is possibly the most important input to the effect. Even by using an exponential function I've been able to
		// make it look better (at a much higher cost of course). This is cheap though and still looks pretty great. In the future
		// I might want to use an actual gaussian equation to correctly calculate the pixel coefficients and attenuates, texel
		// offsets, gaussian amplitude and radius...
		fTexelWidthOffset += r_DynamicGlowDelta->value;
		fTexelHeightOffset += r_DynamicGlowDelta->value;
	}

	// Disable multi-texturing.
	qglActiveTextureARB(GL_TEXTURE3_ARB);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);

	qglActiveTextureARB(GL_TEXTURE2_ARB);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);

	qglActiveTextureARB(GL_TEXTURE0_ARB);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);

	qglDisable(GL_VERTEX_PROGRAM_ARB);
	EndPixelShader();

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglDisable(GL_BLEND);
	qglEnable(GL_DEPTH_TEST);

	glState.currenttmu = 0;	//this matches the last one we activated
}

// Draw the glow blur over the screen additively.
static inline void RB_DrawGlowOverlay()
{
	qglDisable(GL_CLIP_PLANE0);
	GL_Cull(CT_TWO_SIDED);
	qglDisable(GL_DEPTH_TEST);

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);

	// For debug purposes.
	if (r_DynamicGlow->integer != 2)
	{
		// Render the normal scene texture.
		qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage);
		qglBegin(GL_QUADS);
		qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		qglTexCoord2f(0, glConfig.vidHeight);
		qglVertex2f(0, 0);

		qglTexCoord2f(0, 0);
		qglVertex2f(0, glConfig.vidHeight);

		qglTexCoord2f(glConfig.vidWidth, 0);
		qglVertex2f(glConfig.vidWidth, glConfig.vidHeight);

		qglTexCoord2f(glConfig.vidWidth, glConfig.vidHeight);
		qglVertex2f(glConfig.vidWidth, 0);
		qglEnd();
	}

	// One and Inverse Src Color give a very soft addition, while one one is a bit stronger. With one one we can
	// use additive blending through multitexture though.
	if (r_DynamicGlowSoft->integer)
	{
		qglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	}
	else
	{
		qglBlendFunc(GL_ONE, GL_ONE);
	}
	qglEnable(GL_BLEND);

	// Now additively render the glow texture.
	qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.blurImage);
	qglBegin(GL_QUADS);
	qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	qglTexCoord2f(0, r_DynamicGlowHeight->integer);
	qglVertex2f(0, 0);

	qglTexCoord2f(0, 0);
	qglVertex2f(0, glConfig.vidHeight);

	qglTexCoord2f(r_DynamicGlowWidth->integer, 0);
	qglVertex2f(glConfig.vidWidth, glConfig.vidHeight);

	qglTexCoord2f(r_DynamicGlowWidth->integer, r_DynamicGlowHeight->integer);
	qglVertex2f(glConfig.vidWidth, 0);
	qglEnd();

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
	qglBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
	qglDisable(GL_BLEND);

	// NOTE: Multi-texture wasn't that much faster (we're obviously not bottlenecked by transform pipeline),
	// and besides, soft glow looks better anyways.
/*	else
	{
		int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;
		if ( GL_TEXTURE_RECTANGLE_ARB == GL_TEXTURE_RECTANGLE_NV )
		{
			iTexWidth = r_DynamicGlowWidth->integer;
			iTexHeight = r_DynamicGlowHeight->integer;
		}

		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_ARB );
		qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.screenGlow );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_ARB );
		qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

		qglBegin(GL_QUADS);
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, iTexHeight );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, glConfig.vidHeight );
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, 0 );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, 0 );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, 0 );
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, iTexHeight );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 );
		qglEnd();

		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglDisable( GL_TEXTURE_RECTANGLE_ARB );

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglDisable( GL_TEXTURE_RECTANGLE_ARB );
		qglEnable( GL_TEXTURE_2D );
	}*/

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglEnable(GL_DEPTH_TEST);
}