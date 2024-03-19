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

#ifndef __QFILES_H__
#define __QFILES_H__

//
// qfiles.h: quake file formats
// This file must be identical in the quake and utils directories
//

// surface geometry should not exceed these limits
#define	SHADER_MAX_VERTEXES	1000
#define	SHADER_MAX_INDEXES	(6*SHADER_MAX_VERTEXES)

// the maximum size of game reletive pathnames
#define	MAX_QPATH		64

/*
========================================================================

QVM files

========================================================================
*/

constexpr auto VM_MAGIC = 0x12721444;
using vmHeader_t = struct
{
	int vmMagic;

	int instructionCount;

	int codeOffset;
	int codeLength;

	int dataOffset;
	int dataLength;
	int litLength; // ( dataLength - litLength ) should be byteswapped on load
	int bssLength; // zero filled memory appended to datalength
};

/*
========================================================================

PCX files are used for 8 bit images

========================================================================
*/

using pcx_t = struct
{
	char manufacturer;
	char version;
	char encoding;
	char bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hres, vres;
	unsigned char palette[48];
	char reserved;
	char color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char filler[58];
	unsigned char data; // unbounded
};

/*
========================================================================

.MD3 triangle model file format

========================================================================
*/

constexpr auto MD3_IDENT = (('3' << 24) + ('P' << 16) + ('D' << 8) + 'I');
constexpr auto MD3_VERSION = 15;

#define MDR_IDENT	(('5'<<24)+('M'<<16)+('D'<<8)+'R')
#define MDR_VERSION	2
#define	MDR_MAX_BONES	128

// limits
constexpr auto MD3_MAX_LODS = 3;
constexpr auto MD3_MAX_TRIANGLES = 8192; // per surface;
constexpr auto MD3_MAX_VERTS = 4096; // per surface;
constexpr auto MD3_MAX_SHADERS = 256; // per surface;
constexpr auto MD3_MAX_FRAMES = 1024; // per model;
constexpr auto MD3_MAX_SURFACES = 32 + 32; // per model;
constexpr auto MD3_MAX_TAGS = 16; // per frame;

// vertex scales
constexpr auto MD3_XYZ_SCALE = (1.0 / 64);

using md3Frame_t = struct md3Frame_s
{
	vec3_t bounds[2];
	vec3_t localOrigin;
	float radius;
	char name[16];
};

using md3Tag_t = struct md3Tag_s
{
	char name[MAX_QPATH]; // tag name
	vec3_t origin;
	vec3_t axis[3];
};

/*
** md3Surface_t
**
** CHUNK			SIZE
** header			sizeof( md3Surface_t )
** shaders			sizeof( md3Shader_t ) * numShaders
** triangles[0]		sizeof( md3Triangle_t ) * numTriangles
** st				sizeof( md3St_t ) * numVerts
** XyzNormals		sizeof( md3XyzNormal_t ) * numVerts * numFrames
*/
using md3Surface_t = struct
{
	int ident; //

	char name[MAX_QPATH]; // polyset name

	int flags;
	int numFrames; // all surfaces in a model should have the same

	int numShaders; // all surfaces in a model should have the same
	int numVerts;

	int numTriangles;
	int ofsTriangles;

	int ofsShaders; // offset from start of
	int ofsSt; // texture coords are common for all frames
	int ofsXyzNormals; // numVerts * numFrames

	int ofsEnd; // next surface follows
};;

using md3Shader_t = struct
{
	char name[MAX_QPATH];
	int shaderIndex; // for in-game use
};

using md3Triangle_t = struct
{
	int indexes[3];
};

using md3St_t = struct
{
	float st[2];
};

using md3XyzNormal_t = struct
{
	short xyz[3];
	short normal;
};

using md3Header_t = struct
{
	int ident;
	int version;

	char name[MAX_QPATH]; // model name

	int flags;

	int numFrames;
	int numTags;
	int numSurfaces;

	int numSkins;

	int ofsFrames; // offset for first frame
	int ofsTags; // numFrames * numTags
	int ofsSurfaces; // first surface, others follow

	int ofsEnd; // end of file
};

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];	// model name

	// frames and bones are shared by all levels of detail
	int			numFrames;
	int			numBones;
	int			ofsFrames;			// mdrFrame_t[numFrames]

	// each level of detail has completely separate sets of surfaces
	int			numLODs;
	int			ofsLODs;

	int                     numTags;
	int                     ofsTags;

	int			ofsEnd;				// end of file
} mdrHeader_t;

typedef struct {
	float		matrix[3][4];
} mdrBone_t;

typedef struct {
	vec3_t		bounds[2];		// bounds of all surfaces of all LOD's for this frame
	vec3_t		localOrigin;		// midpoint of bounds, used for sphere cull
	float		radius;			// dist from localOrigin to corner
	char		name[16];
	mdrBone_t	bones[1];		// [numBones]
} mdrFrame_t;

typedef struct {
	int			ident;

	char		name[MAX_QPATH];	// polyset name
	char		shader[MAX_QPATH];
	int			shaderIndex;	// for in-game use

	int			ofsHeader;	// this will be a negative number

	int			numVerts;
	int			ofsVerts;

	int			numTriangles;
	int			ofsTriangles;

	// Bone references are a set of ints representing all the bones
	// present in any vertex weights for this surface.  This is
	// needed because a model may have surfaces that need to be
	// drawn at different sort times, and we don't want to have
	// to re-interpolate all the bones for each surface.
	int			numBoneReferences;
	int			ofsBoneReferences;

	int			ofsEnd;		// next surface follows
} mdrSurface_t;

typedef struct {
	int			numSurfaces;
	int			ofsSurfaces;		// first surface, others follow
	int			ofsEnd;				// next lod follows
} mdrLOD_t;

typedef struct {
	int			bone_index;	// these are indexes into the boneReferences,
	float		   boneWeight;		// not the global per-frame bone list
	vec3_t		offset;
} mdrWeight_t;

typedef struct {
	vec3_t		normal;
	vec2_t		texCoords;
	int			numWeights;
	mdrWeight_t	weights[1];		// variable sized
} mdrVertex_t;

typedef struct {
	int			indexes[3];
} mdrTriangle_t;

typedef struct {
	int                     bone_index;
	char            name[32];
} mdrTag_t;

typedef struct {
	unsigned char Comp[24]; // MC_COMP_BYTES is in MatComp.h, but don't want to couple
} mdrCompBone_t;

typedef struct {
	vec3_t          bounds[2];		// bounds of all surfaces of all LOD's for this frame
	vec3_t          localOrigin;		// midpoint of bounds, used for sphere cull
	float           radius;			// dist from localOrigin to corner
	mdrCompBone_t   bones[1];		// [numBones]
} mdrCompFrame_t;

/*
==============================================================================

  .BSP file format

==============================================================================
*/

constexpr auto BSP_IDENT = (('P' << 24) + ('S' << 16) + ('B' << 8) + 'R');
// little-endian "IBSP"

constexpr auto BSP_VERSION = 1;

// there shouldn't be any problem with increasing these values at the
// expense of more memory allocation in the utilities
constexpr auto MAX_MAP_MODELS = 0x400;
constexpr auto MAX_MAP_BRUSHES = 0x8000;
constexpr auto MAX_MAP_ENTITIES = 0x800;
constexpr auto MAX_MAP_ENTSTRING = 0x40000;
constexpr auto MAX_MAP_SHADERS = 0x400;

constexpr auto MAX_MAP_AREAS = 0x100; // MAX_MAP_AREA_BYTES in q_shared must match!;
constexpr auto MAX_MAP_FOGS = 0x100;
constexpr auto MAX_MAP_PLANES = 0x20000;
constexpr auto MAX_MAP_NODES = 0x20000;
constexpr auto MAX_MAP_BRUSHSIDES = 0x20000;
constexpr auto MAX_MAP_LEAFS = 0x20000;
constexpr auto MAX_MAP_LEAFFACES = 0x20000;
constexpr auto MAX_MAP_LEAFBRUSHES = 0x40000;
constexpr auto MAX_MAP_PORTALS = 0x20000;
constexpr auto MAX_MAP_LIGHTING = 0x800000;
constexpr auto MAX_MAP_LIGHTGRID = 65535;
constexpr auto MAX_MAP_LIGHTGRID_ARRAY = 0x100000;

constexpr auto MAX_MAP_VISIBILITY = 0x400000;

constexpr auto MAX_MAP_DRAW_SURFS = 0x20000;
constexpr auto MAX_MAP_DRAW_VERTS = 0x80000;
constexpr auto MAX_MAP_DRAW_INDEXES = 0x80000;

// key / value pair sizes in the entities lump
constexpr auto MAX_KEY = 32;
constexpr auto MAX_VALUE = 1024;

// the editor uses these predefined yaw angles to orient entities up or down
constexpr auto ANGLE_UP = -1;
constexpr auto ANGLE_DOWN = -2;

constexpr auto LIGHTMAP_WIDTH = 128;
constexpr auto LIGHTMAP_HEIGHT = 128;

//=============================================================================

using lump_t = struct
{
	int fileofs, filelen;
};

constexpr auto LUMP_ENTITIES = 0;
constexpr auto LUMP_SHADERS = 1;
constexpr auto LUMP_PLANES = 2;
constexpr auto LUMP_NODES = 3;
constexpr auto LUMP_LEAFS = 4;
constexpr auto LUMP_LEAFSURFACES = 5;
constexpr auto LUMP_LEAFBRUSHES = 6;
constexpr auto LUMP_MODELS = 7;
constexpr auto LUMP_BRUSHES = 8;
constexpr auto LUMP_BRUSHSIDES = 9;
constexpr auto LUMP_DRAWVERTS = 10;
constexpr auto LUMP_DRAWINDEXES = 11;
constexpr auto LUMP_FOGS = 12;
constexpr auto LUMP_SURFACES = 13;
constexpr auto LUMP_LIGHTMAPS = 14;
constexpr auto LUMP_LIGHTGRID = 15;
constexpr auto LUMP_VISIBILITY = 16;
constexpr auto LUMP_LIGHTARRAY = 17;
constexpr auto HEADER_LUMPS = 18;

using dheader_t = struct
{
	int ident;
	int version;

	lump_t lumps[HEADER_LUMPS];
};

using dmodel_t = struct
{
	float mins[3], maxs[3];
	int firstSurface, numSurfaces;
	int firstBrush, numBrushes;
};

using dshader_t = struct dshader_s
{
	char shader[MAX_QPATH];
	int surfaceFlags;
	int contentFlags;
};

// planes x^1 is allways the opposite of plane x

using dplane_t = struct
{
	float normal[3];
	float dist;
};

using dnode_t = struct
{
	int planeNum;
	int children[2]; // negative numbers are -(leafs+1), not nodes
	int mins[3]; // for frustom culling
	int maxs[3];
};

using dleaf_t = struct
{
	int cluster; // -1 = opaque cluster (do I still store these?)
	int area;

	int mins[3]; // for frustum culling
	int maxs[3];

	int firstLeafSurface;
	int numLeafSurfaces;

	int firstLeafBrush;
	int numLeafBrushes;
};

using dbrushside_t = struct
{
	int planeNum; // positive plane side faces out of the leaf
	int shaderNum;
	int drawSurfNum;
};

using dbrush_t = struct
{
	int firstSide;
	int numSides;
	int shaderNum; // the shader that determines the contents flags
};

using dfog_t = struct
{
	char shader[MAX_QPATH];
	int brushNum;
	int visibleSide; // the brush side that ray tests need to clip against (-1 == none)
};

// Light Style Constants
constexpr auto MAXLIGHTMAPS = 4;
constexpr auto LS_NORMAL = 0x00;
constexpr auto LS_UNUSED = 0xfe;
#define	LS_NONE			0xff
#define MAX_LIGHT_STYLES		64

using mapVert_t = struct
{
	vec3_t xyz;
	float st[2];
	float lightmap[MAXLIGHTMAPS][2];
	vec3_t normal;
	byte color[MAXLIGHTMAPS][4];
};

using drawVert_t = struct
{
	vec3_t xyz;
	float st[2];
	float lightmap[MAXLIGHTMAPS][2];
	vec3_t normal;
	byte color[MAXLIGHTMAPS][4];
};

using dgrid_t = struct
{
	byte ambientLight[MAXLIGHTMAPS][3];
	byte directLight[MAXLIGHTMAPS][3];
	byte styles[MAXLIGHTMAPS];
	byte latLong[2];
};

using mapSurfaceType_t = enum
{
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE
};

using dsurface_t = struct
{
	int shaderNum;
	int fogNum;
	int surfaceType;

	int firstVert;
	int numVerts;

	int firstIndex;
	int numIndexes;

	byte lightmapStyles[MAXLIGHTMAPS], vertexStyles[MAXLIGHTMAPS];
	int lightmapNum[MAXLIGHTMAPS];
	int lightmapX[MAXLIGHTMAPS], lightmapY[MAXLIGHTMAPS];
	int lightmapWidth, lightmapHeight;

	vec3_t lightmapOrigin;
	vec3_t lightmapVecs[3]; // for patches, [0] and [1] are lodbounds

	int patchWidth;
	int patchHeight;
};

using hunkAllocType_t = enum //# hunkAllocType_e
{
	HA_MISC,
	HA_MAP,
	HA_SHADERS,
	HA_LIGHTING,
	HA_FOG,
	HA_PATCHES,
	HA_VIS,
	HA_SUBMODELS,
	HA_MODELS,
	MAX_HA_TYPES
};

/////////////////////////////////////////////////////////////
//
// Defines and structures required for fonts

constexpr auto GLYPH_COUNT = 256;

// Must match define in stmparse.h
constexpr auto STYLE_DROPSHADOW = 0x80000000;
constexpr auto STYLE_BLINK = 0x40000000;
constexpr auto SET_MASK = 0x00ffffff;

using glyphInfo_t = struct
{
	short width; // number of pixels wide
	short height; // number of scan lines
	short horizAdvance; // number of pixels to advance to the next char
	short horizOffset; // x offset into space to render glyph
	int baseline; // y offset
	float s; // x start tex coord
	float t; // y start tex coord
	float s2; // x end tex coord
	float t2; // y end tex coord
};

// this file corresponds 1:1 with the "*.fontdat" files, so don't change it unless you're going to
//	recompile the fontgen util and regenerate all the fonts!
//
using dfontdat_t = struct dfontdat_s
{
	glyphInfo_t mGlyphs[GLYPH_COUNT];

	short mPointSize;
	short mHeight; // max height of font
	short mAscender;
	short mDescender;

	short mKoreanHack;
	// unused field, written out by John's fontgen program but we have to leave it there for disk structs <sigh>
};

/////////////////// fonts end ////////////////////////////////////

#endif
