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

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "tr_common.h"
#include "tr_public.h"
#include "mdx_format.h"
#include "qgl.h"

#define GL_INDEX_TYPE		GL_UNSIGNED_INT
using glIndex_t = unsigned int;

extern refimport_t ri;

// 13 bits
// can't be increased without changing bit packing for drawsurfs
// see QSORT_SHADERNUM_SHIFT
#define SHADERNUM_BITS	13
#define MAX_SHADERS		(1<<SHADERNUM_BITS)

using dlight_t = struct dlight_s {
	vec3_t	origin;
	vec3_t	color;				// range from 0.0 to 1.0, should be color normalized
	float	radius;

	vec3_t	transformed;		// origin in local coordinate system
};

// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
using trRefEntity_t = struct {
	refEntity_t	e;

	float		axisLength;		// compensate for non-normalized axis

	qboolean	needDlights;	// true for bmodels that touch a dlight
	qboolean	lightingCalculated;
	vec3_t		lightDir;		// normalized direction towards light
	vec3_t		ambientLight;	// color normalized to 0-255
	int			ambientLightInt;	// 32 bit rgba packed
	vec3_t		directedLight;
	int			dlightBits;
};

// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
using trRefdef_t = struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	int			time;				// time in milliseconds for shader effects and other time dependent rendering issues
	int			frametime;
	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];
	qboolean	areamaskModified;	// qtrue if areamask changed since last scene

	float		floatTime;			// tr.refdef.time / 1000.0

	// text messages for deform text shaders
//	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	int			num_entities;
	trRefEntity_t* entities;

	int			num_dlights;
	dlight_s* dlights;

	int			numPolys;
	struct srfPoly_s* polys;

	int			numDrawSurfs;
	struct drawSurf_s* drawSurfs;

	qboolean	doLAGoggles;

	int			fogIndex;	//what fog brush the vieworg is in
};

using orientationr_t = struct {
	vec3_t		origin;			// in world coordinates
	vec3_t		axis[3];		// orientation in world
	vec3_t		viewOrigin;		// viewParms->or.origin in local coordinates
	float		model_matrix[16];
};

using image_t = struct image_s {
	char		imgName[MAX_QPATH];		// game path, including extension
	int			frameUsed;			// for texture usage in frame statistics
	word		width, height;				// source image
	//	int			imgfileSize;

	GLuint		texnum;					// gl texture binding
	int			internalFormat;
	int			wrapClampMode;		// GL_CLAMP or GL_REPEAT

	bool		mipmap;

	bool		allowPicmip;
	short		iLastLevelUsedOn;
};

//===============================================================================

using shaderSort_t = enum {
	SS_BAD,
	SS_PORTAL,			// mirrors, portals, viewscreens
	SS_ENVIRONMENT,		// sky box
	SS_OPAQUE,			// opaque

	SS_DECAL,			// scorch marks, etc.
	SS_SEE_THROUGH,		// ladders, grates, grills that may have small blended edges
	// in addition to alpha test
	SS_BANNER,

	SS_INSIDE,			// inside body parts (i.e. heart)
	SS_MID_INSIDE,
	SS_MIDDLE,
	SS_MID_OUTSIDE,
	SS_OUTSIDE,			// outside body parts (i.e. ribs)

	SS_FOG,

	SS_UNDERWATER,		// for items that should be drawn in front of the water plane

	SS_BLEND0,			// regular transparency and filters
	SS_BLEND1,			// generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,	// gun smoke puffs

	SS_NEAREST			// blood blobs
};

#define MAX_SHADER_STAGES 8

using genFunc_t = enum {
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,

	GF_NOISE,
	GF_RAND,
};

using deform_t = enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
};

using alphaGen_t = enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_BLEND,
	AGEN_CONST,
	AGEN_DOT,
	AGEN_ONE_MINUS_DOT
};

using colorGen_t = enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,	// tr.identityLight
	CGEN_IDENTITY,			// always (1,1,1,1)
	CGEN_SKIP,
	CGEN_ENTITY,			// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,		// tess.vertexColors
	CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,			// programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_LIGHTING_DIFFUSE_ENTITY, //diffuse lighting * entity
	CGEN_FOG,				// standard fog
	CGEN_CONST,				// fixed color
	CGEN_LIGHTMAPSTYLE,
};

using texCoordGen_t = enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,			// clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_LIGHTMAP1,
	TCGEN_LIGHTMAP2,
	TCGEN_LIGHTMAP3,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_FOG,
	TCGEN_VECTOR			// S and T from world coordinates
};

using acff_t = enum {
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
};

using EGLFogOverride = enum {
	GLFOGOVERRIDE_NONE = 0,
	GLFOGOVERRIDE_BLACK,
	GLFOGOVERRIDE_WHITE,

	GLFOGOVERRIDE_MAX
};

using waveForm_t = struct {
	genFunc_t	func;

	float base;
	float amplitude;
	float phase;
	float frequency;
};

#define TR_MAX_TEXMODS 4

using texMod_t = enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE
};

#define	MAX_SHADER_DEFORMS	3
using deformStage_t = struct {
	deform_t	deformation;			// vertex coordinate modification type

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
};

using texModInfo_t = struct {
	texMod_t		type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t		wave;

	// used for TMOD_TRANSFORM
	float			matrix[2][2];		// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float			translate[2];		// t' = s * m[0][1] + t * m[0][1] + trans[1]

	// used for TMOD_SCALE
//	float			scale[2];			// s *= scale[0]
										// t *= scale[1]

	// used for TMOD_SCROLL
//	float			scroll[2];			// s' = s + scroll[0] * time
										// t' = t + scroll[1] * time

	// used for TMOD_ROTATE
	// + = clockwise
	// - = counterclockwise
	//float			rotateSpeed;
};

#define SURFSPRITE_NONE			0
#define SURFSPRITE_VERTICAL		1
#define SURFSPRITE_ORIENTED		2
#define SURFSPRITE_EFFECT		3
#define SURFSPRITE_WEATHERFX	4
#define SURFSPRITE_FLATTENED	5

#define SURFSPRITE_FACING_NORMAL	0
#define SURFSPRITE_FACING_UP		1
#define SURFSPRITE_FACING_DOWN		2
#define SURFSPRITE_FACING_ANY		3

using surfaceSprite_t = struct surfaceSprite_s
{
	int				surfaceSpriteType;
	float			width, height, density, wind, windIdle, fadeDist, fadeMax, fadeScale;
	float			fxAlphaStart, fxAlphaEnd, fxDuration, vertSkew;
	vec2_t			variance, fxGrow;
	int				facing;		// Hangdown on vertical sprites, faceup on others.
};

using textureBundle_t = struct {
	image_t* image;

	texCoordGen_t	tcGen;
	vec3_t* tcGenVectors;

	texModInfo_t* texMods;
	short			numTexMods;
	short			numImageAnimations;
	float			imageAnimationSpeed;

	bool			isLightmap;
	bool			oneShotAnimMap;
	bool			vertexLightmap;
	bool			isVideoMap;

	int				videoMapHandle;
};

#define NUM_TEXTURE_BUNDLES 2

using shaderStage_t = struct {
	bool			active;
	bool			isDetail;
	byte			index;						// index of stage
	byte			lightmapStyle;

	textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];

	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
	alphaGen_t		alphaGen;

	byte			constantColor[4];			// for CGEN_CONST and AGEN_CONST

	uint32_t		stateBits;					// GLS_xxxx mask

	acff_t			adjustColorsForFog;

	EGLFogOverride	mGLFogColorOverride;

	surfaceSprite_t* ss;

	// Whether this object emits a glow or not.
	bool			glow;
};

struct shaderCommands_s;

#define LIGHTMAP_2D			-4		// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX	-3		// pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE	-2
#define	LIGHTMAP_NONE		-1

using cullType_t = enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
};

using fogPass_t = enum {
	FP_NONE,		// surface is translucent and will just be adjusted properly
	FP_EQUAL,		// surface is opaque but possibly alpha tested
	FP_LE			// surface is trnaslucent, but still needs a fog pass (fog surface)
};

using skyParms_t = struct {
	float		cloudHeight;
	//	image_t		*outerbox[6], *innerbox[6];
	image_t* outerbox[6];
};

using fogParms_t = struct {
	vec3_t	color;
	float	depthForOpaque;
};

using shader_t = struct shader_s {
	char		name[MAX_QPATH];		// game path, including extension
	int			lightmapIndex[MAXLIGHTMAPS];	// for a shader to match, both name and lightmapIndex must match
	byte		styles[MAXLIGHTMAPS];

	int			index;					// this shader == tr.shaders[index]
	int			sortedIndex;			// this shader == tr.sortedShaders[sortedIndex]

	float		sort;					// lower numbered shaders draw before higher numbered

	int			surfaceFlags;			// if explicitlyDefined, this will have SURF_* flags
	int			contentFlags;

	bool		defaultShader;			// we want to return index 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	bool		explicitlyDefined;		// found in a .shader file
	bool		entityMergable;			// merge across entites optimizable (smoke, blood)

	skyParms_t* sky;
	fogParms_t* fogParms;

	float		portalRange;			// distance to fog out at

	int			multitextureEnv;		// 0, GL_MODULATE, GL_ADD (FIXME: put in stage)

	cullType_t	cullType;				// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
	bool		polygonOffset;			// set for decals and other items that must be offset
	bool		noMipMaps;				// for console fonts, 2D elements, etc.
	bool		noPicMip;				// for images that must always be full resolution
	bool		noTC;					// for images that don't want to be texture compressed (namely skies)

	fogPass_t	fogPass;				// draw a blended pass, possibly with depth test equals

	deformStage_t* deforms[MAX_SHADER_DEFORMS];
	short		numDeforms;

	short		numUnfoggedPasses;
	shaderStage_t* stages;

	float			timeOffset;                                 // current time offset for this shader

	// True if this shader has a stage with glow in it (just an optimization).
	bool hasGlow;

	//	struct shader_s		*remappedShader;                  // current shader this one is remapped too
	shader_s* next;
};

/*
Ghoul2 Insert Start
*/
// bogus little registration system for hit and location based damage files in hunk memory
/*
typedef struct
{
	byte	*loc;
	int		width;
	int		height;
	char	name[MAX_QPATH];
} hitMatReg_t;

#define MAX_HITMAT_ENTRIES 1000

extern hitMatReg_t		hitMatReg[MAX_HITMAT_ENTRIES];
*/
/*
Ghoul2 Insert End
*/

//=================================================================================

// skins allow models to be retextured without modifying the model file
using skinSurface_t = struct {
	char		name[MAX_QPATH];
	shader_t* shader;
};

using skin_t = struct skin_s {
	char		name[MAX_QPATH];		// game path, including extension
	int			numSurfaces;
	skinSurface_t* surfaces[128];
};

using fog_t = struct {
	int			originalBrushNumber;
	vec3_t		bounds[2];

	unsigned	colorInt;				// in packed byte format
	float		tcScale;				// texture coordinate vector scales
	fogParms_t	parms;

	// for clipping distance in fog when outside
	qboolean	hasSurface;
	float		surface[4];
};

using viewParms_t = struct {
	orientationr_t	ori;
	orientationr_t	world;
	vec3_t		pvsOrigin;			// may be different than or.origin for portals
	qboolean	is_portal;			// true if this view is through a portal
	qboolean	isMirror;			// the portal is a mirror, invert the face culling
	int			frameSceneNum;		// copied from tr.frameSceneNum
	int			frameCount;			// copied from tr.frameCount
	cplane_t	portalPlane;		// clip anything behind this if mirroring
	int			viewportX, viewportY, viewportWidth, viewportHeight;
	float		fovX, fovY;
	float		projectionMatrix[16];
	cplane_t	frustum[5];
	vec3_t		visBounds[2];
	float		zFar;
};

/*
==============================================================================

SURFACES

==============================================================================
*/

// any changes in surfaceType must be mirrored in rb_surfaceTable[]
typedef enum {
	SF_BAD,
	SF_SKIP,				// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MD3,
	/*
	Ghoul2 Insert Start
	*/
	SF_MDX,
	/*
	Ghoul2 Insert End
	*/

	SF_FLARE,
	SF_ENTITY,				// beams, rails, lightning, etc that can be determined by entity
	SF_DISPLAY_LIST,

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0xffffffff			// ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

using drawSurf_t = struct drawSurf_s {
	unsigned			sort;			// bit combination for fast compares
	surfaceType_t* surface;		// any of surface*_t
};

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
using srfPoly_t = struct srfPoly_s {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	int				fogIndex;
	int				numVerts;
	polyVert_t* verts;
};

using srfDisplayList_t = struct srfDisplayList_s {
	surfaceType_t	surfaceType;
	int				listNum;
};

using srfFlare_t = struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
};

using srfGridMesh_t = struct srfGridMesh_s {
	surfaceType_t	surfaceType;

	// dynamic lighting information
	int				dlightBits;

	// culling information
	vec3_t			meshBounds[2];
	vec3_t			localOrigin;
	float			meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t			lodOrigin;
	float			lodRadius;

	// vertexes
	int				width, height;
	float* widthLodError;
	float* heightLodError;
	drawVert_t		verts[1];		// variable sized
};

#define	VERTEXSIZE			(6+(MAXLIGHTMAPS*3))
#define VERTEX_LM			5
#define	VERTEX_COLOR		(5+(MAXLIGHTMAPS*2))

#define	VERTEX_FINAL_COLOR	(5+(MAXLIGHTMAPS*3))

using srfSurfaceFace_t = struct {
	surfaceType_t	surfaceType;
	cplane_t	plane;

	// dynamic lighting information
	int			dlightBits;

	// triangle definitions (no normals at points)
	int			numPoints;
	int			numIndices;
	int			ofsIndices;
	float		points[1][VERTEXSIZE];	// variable sized
	// there is a variable length list of indices here also
};

// misc_models in maps are turned into direct geometry by q3map
using srfTriangles_t = struct {
	surfaceType_t	surfaceType;

	// dynamic lighting information
	int				dlightBits;

	// culling information (FIXME: use this!)
	vec3_t			bounds[2];
	//	vec3_t			localOrigin;
	//	float			radius;

		// triangle definitions
	int				numIndexes;
	int* indexes;

	int				numVerts;
	drawVert_t* verts;
};

extern	void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])(void*);

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/

//
// in memory representation
//

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

using msurface_t = struct msurface_s {
	int					viewCount;		// if == tr.viewCount, already added
	shader_s* shader;
	int					fogIndex;

	surfaceType_t* data;			// any of srf*_t
};

#define	CONTENTS_NODE		-1

using mnode_t = struct mnode_s {
	// common with leaf and node
	int			contents;		// -1 for nodes, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	vec3_t		mins, maxs;		// for bounding box culling
	mnode_s* parent;

	// node specific
	cplane_t* plane;
	mnode_s* children[2];

	// leaf specific
	int			cluster;
	int			area;

	msurface_t** firstmarksurface;
	int			nummarksurfaces;
};

using bmodel_t = struct {
	vec3_t		bounds[2];		// for culling
	msurface_t* firstSurface;
	int			numSurfaces;
};

using mgrid_t = struct {
	byte		ambientLight[MAXLIGHTMAPS][3];
	byte		directLight[MAXLIGHTMAPS][3];
	byte		styles[MAXLIGHTMAPS];
	byte		latLong[2];
};

using world_t = struct {
	char		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp
	char		baseName[MAX_QPATH];	// ie: tim_dm2

	int			numShaders;
	dshader_t* shaders;

	bmodel_t* bmodels;

	int			numplanes;
	cplane_t* planes;

	int			numnodes;		// includes leafs
	int			numDecisionNodes;
	mnode_t* nodes;

	int			numsurfaces;
	msurface_t* surfaces;

	int			nummarksurfaces;
	msurface_t** marksurfaces;

	int			numfogs;
	fog_t* fogs;
	int			globalFog;

	int			startLightMapIndex;

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];
	mgrid_t* lightGridData;
	unsigned short* lightGridArray;
	int			numGridArrayElements;

	int			numClusters;
	int			clusterBytes;

	const byte* vis;			// may be passed in by CM_LoadMap to save space

	byte* novis;			// clusterBytes of 0xff
};

//======================================================================

using modtype_t = enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	/*
	Ghoul2 Insert Start
	*/
	MOD_MDXM,
	MOD_MDXA
	/*
	Ghoul2 Insert End
	*/
};

using model_t = struct model_s {
	char		name[MAX_QPATH];
	modtype_t	type;
	int			index;				// model = tr.models[model->mod_index]

	int			dataSize;			// just for listing purposes
	bmodel_t* bmodel;			// only if type == MOD_BRUSH
	md3Header_t* md3[MD3_MAX_LODS];	// only if type == MOD_MESH
	/*
	Ghoul2 Insert Start
	*/
	mdxmHeader_t* mdxm;				// only if type == MOD_GL2M which is a GHOUL II Mesh file NOT a GHOUL II animation file
	mdxaHeader_t* mdxa;				// only if type == MOD_GL2A which is a GHOUL II Animation file
	/*
	Ghoul2 Insert End
	*/
	unsigned char	numLods;
	bool			bspInstance;			// model is a bsp instance
};

#define	MAX_MOD_KNOWN	1024

void		R_ModelInit();
model_t* R_GetModelByHandle(qhandle_t index);

int R_LerpTag(orientation_t* tag, const qhandle_t handle, const int startFrame, const int endFrame, const float frac, const char* tagName);

void		R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs);

void		R_Modellist_f();

//====================================================
constexpr auto MAX_LIGHTMAPS = 256;
constexpr auto MAX_SKINS = 1024;

constexpr auto MAX_DRAWSURFS = 0x10000;
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

/*

the drawsurf sort data is packed into a single 32 bit value so it can be
compared quickly during the qsorting process

the bits are allocated as follows:

22 - 31	: sorted shader index
12 - 21	: entity index
3 - 7	: fog index
2		: used to be clipped flag
0 - 1	: dlightmap index
#define	QSORT_SHADERNUM_SHIFT	22
#define	QSORT_ENTITYNUM_SHIFT	12
#define	QSORT_FOGNUM_SHIFT		3

	TTimo - 1.32
31	:  used for alpha fading models (drawn last)
18-30 : sorted shader index
7-17  : entity index
2-6   : fog index
0-1   : dlightmap index
*/

#define	QSORT_FOGNUM_SHIFT		2
#define	QSORT_REFENTITYNUM_SHIFT	7
#define	QSORT_SHADERNUM_SHIFT	(QSORT_REFENTITYNUM_SHIFT+REFENTITYNUM_BITS)
// Note: 32nd bit is reserved for RF_ALPHA_FADE voodoo magic
// see R_AddEntitySurfaces tr.shiftedEntityNum
#if (QSORT_SHADERNUM_SHIFT+SHADERNUM_BITS) > 31
#error "Need to update sorting, too many bits."
#endif

extern	int			gl_filter_min, gl_filter_max;

/*
** performanceCounters_t
*/
using frontEndCounters_t = struct {
	int		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	int		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	int		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	int		c_leafs;
	int		c_dlightSurfaces;
	int		c_dlightSurfacesCulled;
};

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)

// the renderer front end should never modify glstate_t
using glstate_t = struct {
	int			currenttextures[2];
	int			currenttmu;
	qboolean	finishCalled;
	int			texEnv[2];
	int			faceCulling;
	uint32_t	glStateBits;
};

using backEndCounters_t = struct {
	int		c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
	float	c_overDraw;

	int		c_dlightVertexes;
	int		c_dlightIndexes;

	int		c_flareAdds;
	int		c_flareTests;
	int		c_flareRenders;

	int		msec;			// total msec for backend run
};

// all state modified by the back end is seperated
// from the front end state
using backEndState_t = struct {
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	orientationr_t	ori;
	backEndCounters_t	pc;
	qboolean	isHyperspace;
	trRefEntity_t* currentEntity;
	qboolean	skyRenderedThisView;	// flag for drawing sun

	qboolean	projection2D;	// if qtrue, drawstretchpic doesn't need to change modes
	byte		color2D[4];
	qboolean	vertexes2D;		// shader needs to be finished
	trRefEntity_t	entity2D;	// currentEntity will point at this when doing 2D rendering
};

/*
** trGlobals_t
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
#define NUM_SCRATCH_IMAGES 16

using trGlobals_t = struct {
	qboolean				registered;		// cleared at shutdown, set at beginRegistration

	int						visCount;		// incremented every time a new vis cluster is entered
	int						frameCount;		// incremented every frame
	int						sceneCount;		// incremented every scene
	int						viewCount;		// incremented every view (twice a scene if portaled)
	// and every R_MarkFragments call

	int						frameSceneNum;	// zeroed at RE_BeginFrame

	qboolean				worldMapLoaded;
	world_t* world;
	char					worldDir[MAX_QPATH];		// ie: maps/tim_dm2 (copy of world_t::name sans extension but still includes the path)

	const byte* externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t* defaultImage;
	image_t* scratchImage[NUM_SCRATCH_IMAGES];
	image_t* fogImage;
	image_t* dlightImage;	// inverse-quare highlight for projective adding
	image_t* whiteImage;			// full of 0xff
	image_t* identityLightImage;	// full of tr.identityLightByte

	image_t* screenImage; //reserve us a gl texnum to use with RF_DISTORTION

	// Handle to the Glow Effect Vertex Shader. - AReis
	GLuint					glowVShader;

	// Handle to the Glow Effect Pixel Shader. - AReis
	GLuint					glowPShader;

	// Image the glowing objects are rendered to. - AReis
	GLuint					screenGlow;

	// A rectangular texture representing the normally rendered scene.
	GLuint					sceneImage;

	// Image used to downsample and blur scene to.	- AReis
	GLuint					blurImage;

	shader_t* defaultShader;
	shader_t* shadowShader;
	shader_t* distortionShader;
	shader_t* projectionShadowShader;

	shader_t* sunShader;

	int						numLightmaps;
	image_t* lightmaps[MAX_LIGHTMAPS];

	trRefEntity_t* currentEntity;
	trRefEntity_t			worldEntity;		// point currentEntity at this when rendering world
	int						currentEntityNum;
	unsigned				shiftedEntityNum;	// currentEntityNum << QSORT_REFENTITYNUM_SHIFT (possible with high bit set for RF_ALPHA_FADE)
	model_t* currentModel;

	viewParms_t				viewParms;

	float					identityLight;		// 1.0 / ( 1 << overbrightBits )
	int						identityLightByte;	// identityLight * 255
	int						overbrightBits;		// r_overbrightBits->integer, but set to 0 if no hw gamma

	orientationr_t			ori;					// for current entity

	trRefdef_t				refdef;

	int						viewCluster;

	vec3_t					sunLight;			// from the sky shader for this level
	vec3_t					sunDirection;
	int						sunSurfaceLight;	// from the sky shader for this level
	vec3_t					sunAmbient;			// from the sky shader	(only used for John's terrain system)

	frontEndCounters_t		pc;
	int						frontEndMsec;		// not in pc due to clearing issue

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	model_t* models[MAX_MOD_KNOWN];
	int						numModels;

	world_t					bspModels[MAX_SUB_BSP];
	int						numBSPModels;

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	int						numShaders;
	shader_t* shaders[MAX_SHADERS];
	shader_t* sortedShaders[MAX_SHADERS];
	int						iNumDeniedShaders;	// used for error-messages only

	int						numSkins;
	skin_t* skins[MAX_SKINS];

	float					sinTable[FUNCTABLE_SIZE];

	float					squareTable[FUNCTABLE_SIZE];
	float					triangleTable[FUNCTABLE_SIZE];
	float					sawToothTable[FUNCTABLE_SIZE];
	float					inverseSawToothTable[FUNCTABLE_SIZE];
	float					fogTable[FOG_TABLE_SIZE];

	float					rangedFog;

	float					distanceCull;
};

int		 R_Images_StartIteration();
image_t* R_Images_GetNextIteration();
void	 R_Images_Clear();
void	 R_Images_DeleteLightMaps();
void	 R_Images_DeleteImage(image_t* pImage);

extern backEndState_t	backEnd;
extern trGlobals_t	tr;
extern glconfig_t	glConfig;		// outside of TR since it shouldn't be cleared during ref re-init
extern glstate_t	glState;		// outside of TR since it shouldn't be cleared during ref re-init
extern window_t		window;

//
// cvars
//
extern cvar_t* r_ignore;				// used for debugging anything
extern cvar_t* r_verbose;				// used for verbose debug spew

extern cvar_t* r_znear;				// near Z clip plane

extern cvar_t* r_stencilbits;			// number of desired stencil bits
extern cvar_t* r_depthbits;			// number of desired depth bits
extern cvar_t* r_colorbits;			// number of desired color bits, only relevant for fullscreen
extern cvar_t* r_stereo;				// desired pixelformat stereo flag
extern cvar_t* r_texturebits;			// number of desired texture bits
// 0 = use framebuffer depth
// 16 = use 16-bit textures
// 32 = use 32-bit textures
// all else = error
extern cvar_t* r_texturebitslm;		// number of desired lightmap texture bits

extern cvar_t* r_measureOverdraw;		// enables stencil buffer overdraw measurement

extern cvar_t* r_lodbias;				// push/pull LOD transitions
extern cvar_t* r_lodscale;

extern cvar_t* r_primitives;			// "0" = based on compiled vertex array existance
// "1" = glDrawElemet tristrips
// "2" = glDrawElements triangles
// "-1" = no drawing

extern cvar_t* r_fastsky;				// controls whether sky should be cleared or drawn
extern cvar_t* r_drawSun;				// controls drawing of sun quad
extern cvar_t* r_dynamiclight;		// dynamic lights enabled/disabled
// rjr - removed for hacking extern cvar_t	*r_dlightBacks;			// dlight non-facing surfaces for continuity

extern	cvar_t* r_norefresh;			// bypasses the ref rendering
extern	cvar_t* r_drawentities;		// disable/enable entity rendering
extern	cvar_t* r_drawworld;			// disable/enable world rendering
extern	cvar_t* r_drawfog;				// disable/enable fog rendering
extern	cvar_t* r_speeds;				// various levels of information display
extern  cvar_t* r_detailTextures;		// enables/disables detail texturing stages

extern	cvar_t* r_novis;				// disable/enable usage of PVS
extern	cvar_t* r_nocull;
extern	cvar_t* r_facePlaneCull;		// enables culling of planar surfaces with back side test
extern	cvar_t* r_nocurves;
extern	cvar_t* r_showcluster;

extern cvar_t* r_dlightStyle;
extern cvar_t* r_surfaceSprites;
extern cvar_t* r_surfaceWeather;
extern cvar_t* r_AdvancedsurfaceSprites;

extern cvar_t* r_windSpeed;
extern cvar_t* r_windAngle;
extern cvar_t* r_windGust;
extern cvar_t* r_windDampFactor;
extern cvar_t* r_windPointForce;
extern cvar_t* r_windPointX;
extern cvar_t* r_windPointY;

extern cvar_t* r_mode;				// video mode
extern cvar_t* r_fullscreen;
extern cvar_t* r_noborder;			// disable border in windowed mode
extern cvar_t* r_centerWindow;		// override vid_x/ypos and center the window
extern cvar_t* r_gamma;
extern cvar_t* r_displayRefresh;		// optional display refresh option
extern cvar_t* r_ignorehwgamma;		// overrides hardware gamma capabilities

extern cvar_t* r_allowExtensions;				// global enable/disable of OpenGL extensions
extern cvar_t* r_ext_compressed_textures;		// these control use of specific extensions
extern cvar_t* r_ext_compressed_lightmaps;	// turns on compression of lightmaps, off by default
extern cvar_t* r_ext_preferred_tc_method;
extern cvar_t* r_ext_gamma_control;
extern cvar_t* r_ext_texenv_op;
extern cvar_t* r_ext_multitexture;
extern cvar_t* r_ext_compiled_vertex_array;
extern cvar_t* r_ext_texture_env_add;
extern cvar_t* r_ext_texture_filter_anisotropic;

extern cvar_t* r_DynamicGlow;
extern cvar_t* r_DynamicGlowPasses;
extern cvar_t* r_DynamicGlowDelta;
extern cvar_t* r_DynamicGlowIntensity;
extern cvar_t* r_DynamicGlowSoft;
extern cvar_t* r_DynamicGlowWidth;
extern cvar_t* r_DynamicGlowHeight;
extern cvar_t* r_Dynamic_AMD_Fix;

extern	cvar_t* r_nobind;						// turns off binding to appropriate textures
extern	cvar_t* r_singleShader;				// make most world faces use default shader
extern	cvar_t* r_colorMipLevels;				// development aid to see texture mip usage
extern	cvar_t* r_picmip;						// controls picmip values
extern	cvar_t* r_finish;
extern	cvar_t* r_swapInterval;
extern	cvar_t* r_textureMode;
extern	cvar_t* r_offsetFactor;
extern	cvar_t* r_offsetUnits;

extern	cvar_t* r_fullbright;					// avoid lightmap pass
extern	cvar_t* r_lightmap;					// render lightmaps only
extern	cvar_t* r_vertexLight;					// vertex lighting mode for better performance

extern	cvar_t* r_logFile;						// number of frames to emit GL logs
extern	cvar_t* r_showtris;					// enables wireframe rendering of the world
extern	cvar_t* r_showtriscolor;				// changes wireframe color
extern	cvar_t* r_showsky;						// forces sky in front of all surfaces
extern	cvar_t* r_shownormals;					// draws wireframe normals
extern	cvar_t* r_clear;						// force screen clear every frame

extern	cvar_t* r_shadows;						// controls shadows: 0 = none, 1 = blur, 2 = stencil, 3 = black planar projection
extern	cvar_t* r_flares;						// light flares

extern	cvar_t* r_intensity;

extern	cvar_t* r_lockpvs;
extern	cvar_t* r_noportals;
extern	cvar_t* r_portalOnly;

extern	cvar_t* r_subdivisions;
extern	cvar_t* r_lodCurveError;
extern	cvar_t* r_skipBackEnd;

extern	cvar_t* r_ignoreGLErrors;

extern	cvar_t* r_overBrightBits;
extern	cvar_t* r_mapOverBrightBits;

extern	cvar_t* r_debugSurface;
extern	cvar_t* r_simpleMipMaps;

extern	cvar_t* r_showImages;
extern	cvar_t* r_debugSort;
extern	cvar_t* r_debugStyle;
/*
Ghoul2 Insert Start
*/
extern	cvar_t* r_noGhoul2;
/*
Ghoul2 Insert End
*/

extern	cvar_t* r_environmentMapping;

extern	cvar_t* r_ratiofix;
//====================================================================


void R_RenderView(const viewParms_t* parms);

void R_AddMD3Surfaces(trRefEntity_t* ent);

void R_AddPolygonSurfaces();

void R_DecomposeSort(unsigned sort, int* entityNum, shader_t** shader,
	int* fogNum, int* dlightMap);

void R_AddDrawSurf(surfaceType_t* surface, const shader_t* shader, int fogIndex, const int dlightMap);

#define	CULL_IN		0		// completely unclipped
#define	CULL_CLIP	1		// clipped by one or more planes
#define	CULL_OUT	2		// completely outside the clipping planes
void R_LocalNormalToWorld(const vec3_t local, vec3_t world);
void R_LocalPointToWorld(const vec3_t local, vec3_t world);
void R_WorldNormalToEntity(const vec3_t worldvec, vec3_t entvec);
int R_CullLocalBox(const vec3_t bounds[2]);
int R_CullPointAndRadius(const vec3_t pt, float radius);
int R_CullLocalPointAndRadius(const vec3_t pt, float radius);

void R_RotateForEntity(const trRefEntity_t* ent, const viewParms_t* view_parms, orientationr_t* ori);

/*
** GL wrapper/helper functions
*/
void	GL_Bind(image_t* image);
void	GL_SetDefaultState();
void	GL_SelectTexture(int unit);
void	GL_TextureMode(const char* string);
void	GL_CheckErrors();
void	GL_State(uint32_t stateBits);
void	GL_TexEnv(int env);
void	GL_Cull(int cullType);

#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define		GLS_SRCBLEND_BITS					0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define		GLS_DSTBLEND_BITS					0x000000f0

#define GLS_DEPTHMASK_TRUE						0x00000100

#define GLS_POLYMODE_LINE						0x00001000

#define GLS_DEPTHTEST_DISABLE					0x00010000
#define GLS_DEPTHFUNC_EQUAL						0x00020000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80							0x20000000
#define GLS_ATEST_GE_80							0x40000000
#define GLS_ATEST_GE_C0							0x80000000
#define	GLS_ATEST_BITS							0xF0000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE
#define GLS_ALPHA			(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)

void	RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte* data, int i_client, qboolean b_dirty);
void	RE_UploadCinematic(int cols, int rows, const byte* data, int client, qboolean dirty);
void	RE_GetScreenShot(byte* buffer, int w, int h);
byte* RE_TempRawImage_ReadFromFile(const char* psLocalFilename, int* piWidth, int* piHeight, byte* pb_re_sample_buffer, const qboolean qbVertFlip);
void	RE_TempRawImage_CleanUp();

void		RE_BeginRegistration(glconfig_t* glconfig_out);
void		RE_LoadWorldMap(const char* name);
void		RE_SetWorldVisData(const byte* vis);
qhandle_t	RE_RegisterModel(const char* name);
qhandle_t	RE_RegisterSkin(const char* name);
int			RE_GetAnimationCFG(const char* ps_cfg_filename, char* ps_dest, int i_dest_size);
void		RE_Shutdown(const qboolean destroyWindow, const qboolean restarting);

void		RE_RegisterMedia_LevelLoadBegin(const char* ps_map_name, ForceReload_e e_force_reload, qboolean b_allow_screen_dissolve);
void		RE_RegisterMedia_LevelLoadEnd();
int			RE_RegisterMedia_GetLevel();
qboolean	RE_RegisterModels_LevelLoadEnd(qboolean b_delete_everything_not_used_this_level = qfalse);
void* RE_RegisterModels_Malloc(const int iSize, void* pvDiskBufferIfJustLoaded, const char* psModelFileName, qboolean* pqbAlreadyFound, const memtag_t eTag);
void		RE_RegisterModels_StoreShaderRequest(const char* psModelFileName, const char* ps_shader_name, const int* pi_shader_index_poke);
void		RE_RegisterModels_Info_f();
qboolean	RE_RegisterImages_LevelLoadEnd();
void		RE_RegisterImages_Info_f();

model_t* R_AllocModel();

void    	R_Init();
image_t* R_FindImageFile(const char* name, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, int glWrapClampMode);

image_t* R_CreateImage(const char* name, const byte* pic, int width, int height, GLenum format, qboolean mipmap, qboolean allow_picmip, qboolean allow_tc, int gl_wrap_clamp_mode);

qboolean	R_GetModeInfo(int* width, int* height, int mode);

void		R_SetColorMappings();
void		R_GammaCorrect(byte* buffer, int bufSize);

void	R_ImageList_f();
void	R_SkinList_f();
void	R_FontList_f();
void	R_ScreenShot_f();
void	R_ScreenShotTGA_f();

void	R_InitFogTable();
float	R_FogFactor(float s, float t);
void	R_InitImages();
void	R_DeleteTextures();
float	R_SumOfUsedImages(qboolean bUseFormat);
void	R_InitSkins();
skin_t* R_GetSkinByHandle(qhandle_t h_skin);

//
// tr_shader.c
//
extern	const int	lightmapsNone[MAXLIGHTMAPS];
extern	const int	lightmaps2d[MAXLIGHTMAPS];
extern	const int	lightmapsVertex[MAXLIGHTMAPS];
extern	const int	lightmapsFullBright[MAXLIGHTMAPS];
extern	const byte	stylesDefault[MAXLIGHTMAPS];

qhandle_t		 RE_RegisterShader(const char* name);
qhandle_t		 RE_RegisterShaderNoMip(const char* name);

shader_t* R_FindShader(const char* name, const int* lightmap_index, const byte* styles, qboolean mip_raw_image);
shader_t* R_GetShaderByHandle(qhandle_t hShader);
void		R_InitShaders();
void		R_ShaderList_f();

//
// tr_arb.c
//
void ARB_InitGlowShaders();

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/
static void GLimp_LogComment(char* comment) {}

/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/
using color4ub_t = byte[4];

using stageVars_t = struct stageVars
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
};

#define	NUM_TEX_COORDS		(MAXLIGHTMAPS+1)

struct shaderCommands_s
{
	glIndex_t	indexes[SHADER_MAX_INDEXES] QALIGN(16);
	vec4_t		xyz[SHADER_MAX_VERTEXES] QALIGN(16);
	vec4_t		normal[SHADER_MAX_VERTEXES] QALIGN(16);
	vec2_t		texCoords[SHADER_MAX_VERTEXES][NUM_TEX_COORDS] QALIGN(16);
	color4ub_t	vertexColors[SHADER_MAX_VERTEXES] QALIGN(16);
	byte		vertexAlphas[SHADER_MAX_VERTEXES][4] QALIGN(16);
	int			vertexDlightBits[SHADER_MAX_VERTEXES] QALIGN(16);

	stageVars_t	svars QALIGN(16);

	shader_t* shader;
	int			fogNum;

	int			dlightBits;	// or together of all vertexDlightBits

	int			numIndexes;
	int			numVertexes;

	// info extracted from current shader
	int			numPasses;
	void		(*currentStageIteratorFunc)();
	shaderStage_t* xstages;

	int			registration;

	qboolean	SSInitializedWind;

	//rww - doing a fade, don't compute shader color/alpha overrides
	bool		fading;
};

#ifdef _MSC_VER
using shaderCommands_t = __declspec(align(16)) shaderCommands_s;
#else
typedef shaderCommands_s	shaderCommands_t;
#endif

extern	shaderCommands_t	tess;

extern	color4ub_t	styleColors[MAX_LIGHT_STYLES];
extern	bool		styleUpdated[MAX_LIGHT_STYLES];

void RB_BeginSurface(shader_t* shader, int fogNum);
void RB_EndSurface();
void RB_CheckOverflow(int verts, int indexes);
#define RB_CHECKOVERFLOW(v,i) if (tess.numVertexes + (v) >= SHADER_MAX_VERTEXES || tess.numIndexes + (i) >= SHADER_MAX_INDEXES ) {RB_CheckOverflow(v,i);}

void RB_StageIteratorGeneric();
void RB_StageIteratorSky();

void RB_AddQuadStamp(vec3_t origin, vec3_t left, vec3_t up, byte* color);
void RB_AddQuadStampExt(vec3_t origin, vec3_t left, vec3_t up, byte* color, float s1, float t1, float s2, float t2);

void RB_ShowImages(void);

/*
============================================================

WORLD MAP

============================================================
*/

void R_AddBrushModelSurfaces(trRefEntity_t* ent);
void R_AddWorldSurfaces();

/*
============================================================

LIGHTS

============================================================
*/

void R_DlightBmodel(const bmodel_t* bmodel, qboolean NoLight);
void R_SetupEntityLighting(const trRefdef_t* refdef, trRefEntity_t* ent);
void R_TransformDlights(int count, dlight_t* dl, const orientationr_t* ori);

/*
============================================================

SHADOWS

============================================================
*/

void RB_ShadowTessEnd();
void RB_ShadowFinish();
void RB_ProjectionShadowDeform();

/*
============================================================

SKIES

============================================================
*/

void R_BuildCloudData(const shaderCommands_t* input);
void R_InitSkyTexCoords(float height_cloud);
void R_DrawSkyBox(shaderCommands_t* shader);
void RB_DrawSun();
void RB_ClipSkyPolygons(const shaderCommands_t* input);

/*
============================================================

CURVE TESSELATION

============================================================
*/
srfGridMesh_t* R_SubdividePatchToGrid(int width, int height,
	drawVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE]);
/*
Ghoul2 Insert Start
*/

float ProjectRadius(float r, vec3_t location);
/*
Ghoul2 Insert End
*/

/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/

int R_MarkFragments(int num_points, const vec3_t* points, const vec3_t projection,
	int max_points, vec3_t point_buffer, int max_fragments, markFragment_t* fragment_buffer);

/*
============================================================

SCENE GENERATION

============================================================
*/

void R_InitNextFrame();

void RE_ClearScene();
void RE_AddRefEntityToScene(const refEntity_t* ent);
void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int numPolys);
void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void RE_RenderScene(const refdef_t* fd);

qboolean RE_GetLighting(const vec3_t origin, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);

// Only returns a four sided face and normal of the best face to break ( this is for glass right now )
void RE_GetBModelVerts(int bmodelIndex, vec3_t* verts, vec3_t normal);

/*
=============================================================

ANIMATED MODELS

=============================================================
*/

/*
Ghoul2 Insert Start
*/
class CBoneCache;

class CRenderableSurface
{
public:
#ifdef _G2_GORE
	int				ident;
#else
	const int		ident;			// ident of this surface - required so the materials renderer knows what sort of surface this refers to
#endif
	CBoneCache* boneCache;		// pointer to transformed bone list for this surf
	mdxmSurface_t* surfaceData;	// pointer to surface data loaded into file - only used by client renderer DO NOT USE IN GAME SIDE - if there is a vid restart this will be out of wack on the game
#ifdef _G2_GORE
	float* alternateTex;		// alternate texture coordinates.
	void* goreChain;

	float			scale;
	float			fade;
	float			impactTime; // this is a number between 0 and 1 that dictates the progression of the bullet impact
#endif

#ifdef _G2_GORE
	CRenderableSurface& operator= (const CRenderableSurface& src)
	{
		ident = src.ident;
		boneCache = src.boneCache;
		surfaceData = src.surfaceData;
		alternateTex = src.alternateTex;
		goreChain = src.goreChain;

		return *this;
	}
#endif

	CRenderableSurface() :
		ident(SF_MDX),
		boneCache(nullptr),
#ifdef _G2_GORE
		surfaceData(nullptr),
		alternateTex(nullptr),
		goreChain(nullptr), scale(0), fade(0), impactTime(0)
#else
		surfaceData(0)
#endif
	{
	}

	void Init()
	{
		boneCache = nullptr;
		surfaceData = nullptr;
#ifdef _G2_GORE
		ident = SF_MDX;
		alternateTex = nullptr;
		goreChain = nullptr;
#endif
	}
};

void R_AddGhoulSurfaces(trRefEntity_t* ent);
void RB_SurfaceGhoul(CRenderableSurface* surf);
/*
Ghoul2 Insert End
*/

/*
=============================================================
=============================================================
*/
void	R_TransformModelToClip(const vec3_t src, const float* model_matrix, const float* projection_matrix,
	vec4_t eye, vec4_t dst);
void	R_TransformClipToWindow(const vec4_t clip, const viewParms_t* view, vec4_t normalized, vec4_t window);

void	RB_DeformTessGeometry();

void	RB_CalcScaleTexCoords(const float scale[2], float* dst_tex_coords);
void	RB_CalcScrollTexCoords(const float scroll_speed[2], float* dst_tex_coords);
void	RB_CalcStretchTexCoords(const waveForm_t* wf, float* tex_coords);
void	RB_CalcTransformTexCoords(const texModInfo_t* tmi, float* dst_tex_coords);
void	RB_CalcRotateTexCoords(float degs_per_second, float* dst_tex_coords);

void	RB_CalcEnvironmentTexCoords(float* dst_tex_coords);
void	RB_CalcFogTexCoords(float* dst_tex_coords);
void	RB_CalcTurbulentTexCoords(const waveForm_t* wf, float* dst_tex_coords);

void	RB_CalcWaveColor(const waveForm_t* wf, unsigned char* dst_colors);
void	RB_CalcColorFromEntity(unsigned char* dst_colors);
void	RB_CalcColorFromOneMinusEntity(unsigned char* dst_colors);
void	RB_CalcWaveAlpha(const waveForm_t* wf, unsigned char* dst_colors);
void	RB_CalcSpecularAlpha(unsigned char* alphas);
void	RB_CalcAlphaFromEntity(unsigned char* dst_colors);
void	RB_CalcAlphaFromOneMinusEntity(unsigned char* dst_colors);
void	RB_CalcModulateColorsByFog(unsigned char* dstColors);
void	RB_CalcModulateAlphasByFog(unsigned char* dstColors);
void	RB_CalcModulateRGBAsByFog(unsigned char* dstColors);

void	RB_CalcDiffuseColor(unsigned char* colors);
void	RB_CalcDiffuseEntityColor(unsigned char* colors);
void	RB_CalcDisintegrateColors(unsigned char* colors, colorGen_t rgb_gen);
void	RB_CalcDisintegrateVertDeform();
/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/

void RB_ExecuteRenderCommands(const void* data);

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x80000

using renderCommandList_t = struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
};

using setColorCommand_t = struct {
	int		commandId;
	float	color[4];
};

using drawBufferCommand_t = struct {
	int		commandId;
	int		buffer;
};

using subImageCommand_t = struct {
	int		commandId;
	image_t* image;
	int		width;
	int		height;
	void* data;
};

using swapBuffersCommand_t = struct {
	int		commandId;
};

using endFrameCommand_t = struct {
	int		commandId;
	int		buffer;
};

using stretchPicCommand_t = struct {
	int		commandId;
	shader_t* shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
};

using rotatePicCommand_t = struct {
	int		commandId;
	shader_t* shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
	float	a;
};

using setModeCommand_t = struct
{
	int			commandId;
};

using scissorCommand_t = struct
{
	int		commandId;
	float	x, y;
	float	w, h;
};

using drawSurfsCommand_t = struct {
	int		commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	drawSurf_t* drawSurfs;
	int		numDrawSurfs;
};

using renderCommand_t = enum {
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_SCISSOR,
	RC_ROTATE_PIC,
	RC_ROTATE_PIC2,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_WORLD_EFFECTS,
};

// these are sort of arbitrary limits.
// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	MAX_POLYS		2048
#define	MAX_POLYVERTS	( MAX_POLYS * 4 )

// all of the information needed by the back end must be
// contained in a backEndData_t.
using backEndData_t = struct {
	drawSurf_t	drawSurfs[MAX_DRAWSURFS];
	dlight_t	dlights[MAX_DLIGHTS];
	trRefEntity_t	entities[MAX_REFENTITIES];
	srfPoly_t	polys[MAX_POLYS];
	polyVert_t	polyVerts[MAX_POLYVERTS];
	renderCommandList_t	commands;
};

extern	backEndData_t* backEndData;

void* R_GetCommandBuffer(const unsigned int bytes);
void RB_ExecuteRenderCommands(const void* data);

void R_IssuePendingRenderCommands(void);

void R_AddDrawSurfCmd(drawSurf_t* drawSurfs, const int numDrawSurfs);

void RE_SetColor(const float* rgba);
void RE_StretchPic(const float x, const float y, const float w, const float h, const float s1, const float t1, const float s2, const float t2, const qhandle_t hShader);
void RE_RotatePic(const float x, const float y, const float w, const float h, const float s1, const float t1, const float s2, const float t2, const float a, const qhandle_t hShader);
void RE_RotatePic2(const float x, const float y, const float w, const float h, const float s1, const float t1, const float s2, const float t2, const float a, const qhandle_t hShader);
void RE_RenderWorldEffects();
void RE_LAGoggles(void);
void RE_Scissor(const float x, const float y, const float w, const float h);
void RE_BeginFrame(const stereoFrame_t stereoFrame);
void RE_EndFrame(int* frontEndMsec, int* backEndMsec);
qboolean	RE_ProcessDissolve();
qboolean	RE_InitDissolve(qboolean bForceCircularExtroWipe);

long generateHashValue(const char* fname);
void R_LoadImage(const char* shortname, byte** pic, int* width, int* height);
void		RE_InsertModelIntoHash(const char* name, const model_t* mod);
qboolean R_FogParmsMatch(int fog1, int fog2);

/*
Ghoul2 Insert Start
*/

// tr_ghoul2.cpp
void		Create_Matrix(const float* angle, mdxaBone_t* matrix);
void		Multiply_3x4Matrix(mdxaBone_t* out, const mdxaBone_t* in2, const mdxaBone_t* in);
extern qboolean R_LoadMDXM(model_t* mod, void* buffer, const char* mod_name, qboolean& b_already_cached);
extern qboolean R_LoadMDXA(model_t* mod, void* buffer, const char* mod_name, qboolean& b_already_cached);
/*
Ghoul2 Insert End
*/

// tr_surfacesprites
void RB_DrawSurfaceSprites(shaderStage_t* stage, shaderCommands_t* input);
