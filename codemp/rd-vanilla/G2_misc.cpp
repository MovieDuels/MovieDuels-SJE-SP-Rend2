/*
===========================================================================
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

#include "qcommon/matcomp.h"
#include "ghoul2/G2.h"
#include "qcommon/MiniHeap.h"
#include "server/server.h"
#include "ghoul2/g2_local.h"

#ifdef _G2_GORE
#include "ghoul2/G2_gore.h"

#include "tr_local.h"

#define GORE_TAG_UPPER (256)
#define GORE_TAG_MASK (~255)

static int CurrentTag = GORE_TAG_UPPER + 1;
static int CurrentTagUpper = GORE_TAG_UPPER;

static std::map<int, GoreTextureCoordinates> GoreRecords;
static std::map<std::pair<int, int>, int> GoreTagsTemp; // this is a surface index to gore tag map used only
// temporarily during the generation phase so we reuse gore tags per LOD
int goreModelIndex;

static cvar_t* cg_g2MarksAllModels = nullptr;

GoreTextureCoordinates* FindGoreRecord(int tag);
static inline void DestroyGoreTexCoordinates(int tag)
{
	GoreTextureCoordinates* gTC = FindGoreRecord(tag);
	if (!gTC)
	{
		return;
	}
	gTC->~GoreTextureCoordinates();
	//I don't know what's going on here, it should call the destructor for
	//this when it erases the record but sometimes it doesn't. -rww
}

//TODO: This needs to be set via a scalability cvar with some reasonable minimum value if pgore is used at all
#define MAX_GORE_RECORDS (500)

int AllocGoreRecord()
{
	while (GoreRecords.size() > MAX_GORE_RECORDS)
	{
		const int tagHigh = (*GoreRecords.begin()).first & GORE_TAG_MASK;
		std::map<int, GoreTextureCoordinates>::iterator it;

		it = GoreRecords.begin();
		GoreTextureCoordinates* gTC = &(*it).second;

		if (gTC)
		{
			gTC->~GoreTextureCoordinates();
		}
		GoreRecords.erase(GoreRecords.begin());
		while (GoreRecords.size())
		{
			if (((*GoreRecords.begin()).first & GORE_TAG_MASK) != tagHigh)
			{
				break;
			}
			it = GoreRecords.begin();
			gTC = &(*it).second;

			if (gTC)
			{
				gTC->~GoreTextureCoordinates();
			}
			GoreRecords.erase(GoreRecords.begin());
		}
	}
	const int ret = CurrentTag;
	GoreRecords[CurrentTag] = GoreTextureCoordinates();
	CurrentTag++;
	return ret;
}

void ResetGoreTag()
{
	GoreTagsTemp.clear();
	CurrentTag = CurrentTagUpper;
	CurrentTagUpper += GORE_TAG_UPPER;
}

GoreTextureCoordinates* FindGoreRecord(int tag)
{
	const std::map<int, GoreTextureCoordinates>::iterator i = GoreRecords.find(tag);
	if (i != GoreRecords.end())
	{
		return &(*i).second;
	}
	return nullptr;
}

void* G2_GetGoreRecord(int tag)
{
	return FindGoreRecord(tag);
}

void DeleteGoreRecord(int tag)
{
	DestroyGoreTexCoordinates(tag);
	GoreRecords.erase(tag);
}

static int CurrentGoreSet = 1; // this is a UUID for gore sets
static std::map<int, CGoreSet*> GoreSets; // map from uuid to goreset

CGoreSet* FindGoreSet(int goreSetTag)
{
	const std::map<int, CGoreSet*>::iterator f = GoreSets.find(goreSetTag);
	if (f != GoreSets.end())
	{
		return (*f).second;
	}
	return nullptr;
}

CGoreSet* NewGoreSet()
{
	CGoreSet* ret = new CGoreSet(CurrentGoreSet++);
	GoreSets[ret->mMyGoreSetTag] = ret;
	ret->mRefCount = 1;
	return ret;
}

void DeleteGoreSet(int goreSetTag)
{
	const std::map<int, CGoreSet*>::iterator f = GoreSets.find(goreSetTag);
	if (f != GoreSets.end())
	{
		if ((*f).second->mRefCount == 0 || (*f).second->mRefCount - 1 == 0)
		{
			delete (*f).second;
			GoreSets.erase(f);
		}
		else
		{
			(*f).second->mRefCount--;
		}
	}
}

CGoreSet::~CGoreSet()
{
	for (std::multimap<int, SGoreSurface>::iterator i = mGoreRecords.begin(); i != mGoreRecords.end(); ++i)
	{
		DeleteGoreRecord((*i).second.mGoreTag);
	}
};
#endif // _SOF2

const mdxaBone_t& EvalBoneCache(int index, CBoneCache* bone_cache);
class CTraceSurface
{
public:
	int					surface_num;
	surfaceInfo_v& rootSList;
	model_t* current_model;
	int					lod;
	vec3_t				rayStart;
	vec3_t				rayEnd;
	CollisionRecord_t* collRecMap;
	int					ent_num;
	int					model_index;
	skin_t* skin;
	shader_t* cust_shader;
	size_t* TransformedVertsArray;
	int					traceFlags;
	bool				hitOne;
	float				m_fRadius;

#ifdef _G2_GORE
	//gore application thing
	float				ssize;
	float				tsize;
	float				theta;
	int					goreShader;
	CGhoul2Info* ghoul2info;

	//	Procedural-gore application things
	SSkinGoreData* gore;
#endif

	CTraceSurface(
		int					initsurfaceNum,
		surfaceInfo_v& initrootSList,
		model_t* initcurrentModel,
		int					initlod,
		vec3_t				initrayStart,
		vec3_t				initrayEnd,
		CollisionRecord_t* initcollRecMap,
		int					initentNum,
		int					initmodelIndex,
		skin_t* initskin,
		shader_t* initcust_shader,
		size_t* initTransformedVertsArray,
		int					inittraceFlags,
#ifdef _G2_GORE
		float				fRadius,
		float				initssize,
		float				inittsize,
		float				inittheta,
		int					initgoreShader,
		CGhoul2Info* initghoul2info,
		SSkinGoreData* initgore):
#else
		float				fRadius):
#endif

	surface_num(initsurfaceNum),
		rootSList(initrootSList),
		current_model(initcurrentModel),
		lod(initlod),
		collRecMap(initcollRecMap),
		ent_num(initentNum),
		model_index(initmodelIndex),
		skin(initskin),
		cust_shader(initcust_shader),
		TransformedVertsArray(initTransformedVertsArray),
		traceFlags(inittraceFlags),
#ifdef _G2_GORE
		m_fRadius(fRadius),
		ssize(initssize),
		tsize(inittsize),
		theta(inittheta),
		goreShader(initgoreShader),
		ghoul2info(initghoul2info),
		gore(initgore)
#else
		m_fRadius(fRadius)
#endif
	{
		VectorCopy(initrayStart, rayStart);
		VectorCopy(initrayEnd, rayEnd);
		hitOne = false;
	}
};

// assorted Ghoul 2 functions.
// list all surfaces associated with a model
void G2_List_Model_Surfaces(const char* fileName)
{
	const model_t* mod_m = R_GetModelByHandle(RE_RegisterModel(fileName));

	mdxmSurfHierarchy_t* surf = (mdxmSurfHierarchy_t*)((byte*)mod_m->mdxm + mod_m->mdxm->ofsSurfHierarchy);
	mdxmSurface_t* surface = (mdxmSurface_t*)((byte*)mod_m->mdxm + mod_m->mdxm->ofsLODs + sizeof(mdxmLOD_t));

	for (int x = 0; x < mod_m->mdxm->numSurfaces; x++)
	{
		ri->Printf(PRINT_ALL, "Surface %i Name %s\n", x, surf->name);
		if (r_verbose->integer)
		{
			ri->Printf(PRINT_ALL, "Num Descendants %i\n", surf->numChildren);
			for (int i = 0; i < surf->numChildren; i++)
			{
				ri->Printf(PRINT_ALL, "Descendant %i\n", surf->childIndexes[i]);
			}
		}
		// find the next surface
		surf = (mdxmSurfHierarchy_t*)((byte*)surf + (size_t)(&static_cast<mdxmSurfHierarchy_t*>(nullptr)->childIndexes[surf->numChildren]));
		surface = (mdxmSurface_t*)((byte*)surface + surface->ofsEnd);
	}
}

// list all bones associated with a model
void G2_List_Model_Bones(const char* file_name)
{
	const model_t* mod_m = R_GetModelByHandle(RE_RegisterModel(file_name));
	const model_t* mod_a = R_GetModelByHandle(mod_m->mdxm->animIndex);
	// 	mdxaFrame_t		*aframe=0;
	//	int				frameSize;
	mdxaHeader_t* header = mod_a->mdxa;

	// figure out where the offset list is
	const mdxaSkelOffsets_t* offsets = (mdxaSkelOffsets_t*)((byte*)header + sizeof(mdxaHeader_t));

	//    frameSize = (size_t)( &((mdxaFrame_t *)0)->boneIndexes[ header->numBones ] );

	//	aframe = (mdxaFrame_t *)((byte *)header + header->ofsFrames + (frame * frameSize));
		// walk each bone and list it's name
	for (int x = 0; x < mod_a->mdxa->numBones; x++)
	{
		mdxaSkel_t* skel = (mdxaSkel_t*)((byte*)header + sizeof(mdxaHeader_t) + offsets->offsets[x]);
		ri->Printf(PRINT_ALL, "Bone %i Name %s\n", x, skel->name);

		ri->Printf(PRINT_ALL, "X pos %f, Y pos %f, Z pos %f\n", skel->BasePoseMat.matrix[0][3], skel->BasePoseMat.matrix[1][3], skel->BasePoseMat.matrix[2][3]);

		// if we are in verbose mode give us more details
		if (r_verbose->integer)
		{
			ri->Printf(PRINT_ALL, "Num Descendants %i\n", skel->numChildren);
			for (int i = 0; i < skel->numChildren; i++)
			{
				ri->Printf(PRINT_ALL, "Num Descendants %i\n", skel->numChildren);
			}
		}
	}
}

/************************************************************************************************
 * G2_GetAnimFileName
 *    obtain the .gla filename for a model
 *
 * Input
 *    filename of model
 *
 * Output
 *    true if we successfully obtained a filename, false otherwise
 *
 ************************************************************************************************/
qboolean G2_GetAnimFileName(const char* file_name, char** filename)
{
	// find the model we want
	const model_t* mod = R_GetModelByHandle(RE_RegisterModel(file_name));

	if (mod && mod->mdxm && (mod->mdxm->animName[0] != 0))
	{
		*filename = mod->mdxm->animName;
		return qtrue;
	}
	return qfalse;
}

/////////////////////////////////////////////////////////////////////
//
//	Code for collision detection for models gameside
//
/////////////////////////////////////////////////////////////////////

int G2_DecideTraceLod(const CGhoul2Info& ghoul2, int use_lod)
{
	int returnLod = use_lod;

	// if we are overriding the LOD at top level, then we can afford to only check this level of model
	if (ghoul2.mLodBias > returnLod)
	{
		returnLod = ghoul2.mLodBias;
	}
	//	assert(G2_MODEL_OK(&ghoul2));

	assert(ghoul2.current_model);
	assert(ghoul2.current_model->mdxm);
	//what about r_lodBias?

	// now ensure that we haven't selected a lod that doesn't exist for this model
	if (returnLod >= ghoul2.current_model->mdxm->numLODs)
	{
		returnLod = ghoul2.current_model->mdxm->numLODs - 1;
	}

	return returnLod;
}

void R_TransformEachSurface(const mdxmSurface_t* surface, vec3_t scale, IHeapAllocator* g2_vert_space, size_t* TransformedVertsArray, CBoneCache* bone_cache)
{
	int				 j, k;

	//
	// deform the vertexes by the lerped bones
	//
	const int* piBoneReferences = (int*)((byte*)surface + surface->ofsBoneReferences);

	// alloc some space for the transformed verts to get put in
	float* TransformedVerts = (float*)g2_vert_space->MiniHeapAlloc(surface->num_verts * 5 * 4);
	TransformedVertsArray[surface->thisSurfaceIndex] = (size_t)TransformedVerts;
	if (!TransformedVerts)
	{
		Com_Error(ERR_DROP, "Ran out of transform space for Ghoul2 Models. Adjust MiniHeapSize in SV_SpawnServer.\n");
	}

	// whip through and actually transform each vertex
	const int num_verts = surface->num_verts;
	mdxmVertex_t* v = (mdxmVertex_t*)((byte*)surface + surface->ofsVerts);
	const mdxmVertexTexCoord_t* pTexCoords = (mdxmVertexTexCoord_t*)&v[num_verts];

	// optimisation issue
	if ((scale[0] != 1.0) || (scale[1] != 1.0) || (scale[2] != 1.0))
	{
		for (j = 0; j < num_verts; j++)
		{
			vec3_t			tempVert, tempNormal;
			//			mdxmWeight_t	*w;

			VectorClear(tempVert);
			VectorClear(tempNormal);
			//			w = v->weights;

			const int iNumWeights = G2_GetVertWeights(v);

			float fTotalWeight = 0.0f;
			for (k = 0; k < iNumWeights; k++)
			{
				const int		i_bone_index = G2_GetVertBoneIndex(v, k);
				const float	fBoneWeight = G2_GetVertBoneWeight(v, k, fTotalWeight, iNumWeights);

				const mdxaBone_t& bone = EvalBoneCache(piBoneReferences[i_bone_index], bone_cache);

				tempVert[0] += fBoneWeight * (DotProduct(bone.matrix[0], v->vertCoords) + bone.matrix[0][3]);
				tempVert[1] += fBoneWeight * (DotProduct(bone.matrix[1], v->vertCoords) + bone.matrix[1][3]);
				tempVert[2] += fBoneWeight * (DotProduct(bone.matrix[2], v->vertCoords) + bone.matrix[2][3]);

				tempNormal[0] += fBoneWeight * DotProduct(bone.matrix[0], v->normal);
				tempNormal[1] += fBoneWeight * DotProduct(bone.matrix[1], v->normal);
				tempNormal[2] += fBoneWeight * DotProduct(bone.matrix[2], v->normal);
			}
			int pos = j * 5;

			// copy tranformed verts into temp space
			TransformedVerts[pos++] = tempVert[0] * scale[0];
			TransformedVerts[pos++] = tempVert[1] * scale[1];
			TransformedVerts[pos++] = tempVert[2] * scale[2];
			// we will need the S & T coors too for hitlocation and hitmaterial stuff
			TransformedVerts[pos++] = pTexCoords[j].texCoords[0];
			TransformedVerts[pos] = pTexCoords[j].texCoords[1];

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}
	}
	else
	{
		int pos = 0;
		for (j = 0; j < num_verts; j++)
		{
			vec3_t			tempVert, tempNormal;
			//			const mdxmWeight_t	*w;

			VectorClear(tempVert);
			VectorClear(tempNormal);
			//			w = v->weights;

			const int iNumWeights = G2_GetVertWeights(v);

			float fTotalWeight = 0.0f;
			for (k = 0; k < iNumWeights; k++)
			{
				const int		i_bone_index = G2_GetVertBoneIndex(v, k);
				const float	fBoneWeight = G2_GetVertBoneWeight(v, k, fTotalWeight, iNumWeights);

				const mdxaBone_t& bone = EvalBoneCache(piBoneReferences[i_bone_index], bone_cache);

				tempVert[0] += fBoneWeight * (DotProduct(bone.matrix[0], v->vertCoords) + bone.matrix[0][3]);
				tempVert[1] += fBoneWeight * (DotProduct(bone.matrix[1], v->vertCoords) + bone.matrix[1][3]);
				tempVert[2] += fBoneWeight * (DotProduct(bone.matrix[2], v->vertCoords) + bone.matrix[2][3]);

				tempNormal[0] += fBoneWeight * DotProduct(bone.matrix[0], v->normal);
				tempNormal[1] += fBoneWeight * DotProduct(bone.matrix[1], v->normal);
				tempNormal[2] += fBoneWeight * DotProduct(bone.matrix[2], v->normal);
			}

			// copy tranformed verts into temp space
			TransformedVerts[pos++] = tempVert[0];
			TransformedVerts[pos++] = tempVert[1];
			TransformedVerts[pos++] = tempVert[2];
			// we will need the S & T coors too for hitlocation and hitmaterial stuff
			TransformedVerts[pos++] = pTexCoords[j].texCoords[0];
			TransformedVerts[pos++] = pTexCoords[j].texCoords[1];

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}
	}
}

void G2_TransformSurfaces(int surface_num, surfaceInfo_v& rootSList,
	CBoneCache* bone_cache, const model_t* current_model, int lod, vec3_t scale, IHeapAllocator* g2_vert_space, size_t* TransformedVertArray, bool secondTimeAround)
{
	assert(current_model);
	assert(current_model->mdxm);
	// back track and get the surfinfo struct for this surface
	const mdxmSurface_t* surface = static_cast<mdxmSurface_t*>(G2_FindSurface((void*)current_model, surface_num, lod));
	const mdxmHierarchyOffsets_t* surf_indexes = (mdxmHierarchyOffsets_t*)((byte*)current_model->mdxm + sizeof(mdxmHeader_t));
	const mdxmSurfHierarchy_t* surfInfo = (mdxmSurfHierarchy_t*)((byte*)surf_indexes + surf_indexes->offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	const surfaceInfo_t* surfOverride = G2_FindOverrideSurface(surface_num, rootSList);

	// really, we should use the default flags for this surface unless it's been overriden
	int off_flags = surfInfo->flags;

	if (surfOverride)
	{
		off_flags = surfOverride->off_flags;
	}
	// if this surface is not off, add it to the shader render list
	if (!off_flags)
	{
		R_TransformEachSurface(surface, scale, g2_vert_space, TransformedVertArray, bone_cache);
	}

	// if we are turning off all descendants, then stop this recursion now
	if (off_flags & G2SURFACEFLAG_NODESCENDANTS)
	{
		return;
	}

	// now recursively call for the children
	for (int i = 0; i < surfInfo->numChildren; i++)
	{
		G2_TransformSurfaces(surfInfo->childIndexes[i], rootSList, bone_cache, current_model, lod, scale, g2_vert_space, TransformedVertArray, secondTimeAround);
	}
}

// main calling point for the model transform for collision detection. At this point all of the skeleton has been transformed.
#ifdef _G2_GORE
void G2_TransformModel(CGhoul2Info_v& ghoul2, const int frame_num, vec3_t scale, IHeapAllocator* g2_vert_space, int use_lod, bool ApplyGore)
#else
void G2_TransformModel(CGhoul2Info_v& ghoul2, const int frame_num, vec3_t scale, IHeapAllocator* g2_vert_space, int use_lod)
#endif
{
	int lod;
	vec3_t			correctScale;
	qboolean		firstModelOnly = qfalse;

	if (cg_g2MarksAllModels == nullptr)
	{
		cg_g2MarksAllModels = ri->Cvar_Get("cg_g2MarksAllModels", "0", 0, "");
	}

	if (cg_g2MarksAllModels == nullptr
		|| !cg_g2MarksAllModels->integer)
	{
		firstModelOnly = qtrue;
	}

	VectorCopy(scale, correctScale);
	// check for scales of 0 - that's the default I believe
	if (!scale[0])
	{
		correctScale[0] = 1.0;
	}
	if (!scale[1])
	{
		correctScale[1] = 1.0;
	}
	if (!scale[2])
	{
		correctScale[2] = 1.0;
	}

	// walk each possible model for this entity and try rendering it out
	for (int i = 0; i < ghoul2.size(); i++)
	{
		CGhoul2Info& g = ghoul2[i];
		// don't bother with models that we don't care about.
		if (!g.mValid)
		{
			continue;
		}
		assert(g.mBoneCache);
		//		assert(G2_MODEL_OK(&g));
				// stop us building this model more than once per frame
		g.mMeshFrameNum = frame_num;

		// decide the LOD
#ifdef _G2_GORE
		if (ApplyGore)
		{
			lod = use_lod;
			assert(g.current_model);
			if (lod >= g.current_model->numLods)
			{
				g.mTransformedVertsArray = nullptr;
				if (firstModelOnly)
				{
					// we don't really need to do multiple models for gore.
					return;
				}
				//do the rest
				continue;
			}
		}
		else
		{
#endif
			lod = G2_DecideTraceLod(g, use_lod);
#ifdef _G2_GORE
		}
#endif

		// give us space for the transformed vertex array to be put in
		if (!(g.mFlags & GHOUL2_ZONETRANSALLOC))
		{ //do not stomp if we're using zone space
			g.mTransformedVertsArray = (size_t*)g2_vert_space->MiniHeapAlloc(g.current_model->mdxm->numSurfaces * sizeof(size_t));
			if (!g.mTransformedVertsArray)
			{
				Com_Error(ERR_DROP, "Ran out of transform space for Ghoul2 Models. Adjust MiniHeapSize in SV_SpawnServer.\n");
			}
		}

		memset(g.mTransformedVertsArray, 0, g.current_model->mdxm->numSurfaces * sizeof(size_t));

		G2_FindOverrideSurface(-1, g.mSlist); //reset the quick surface override lookup;
		// recursively call the model surface transform

		G2_TransformSurfaces(g.mSurfaceRoot, g.mSlist, g.mBoneCache, g.current_model, lod, correctScale, g2_vert_space, g.mTransformedVertsArray, false);

#ifdef _G2_GORE
		if (ApplyGore && firstModelOnly)
		{
			// we don't really need to do multiple models for gore.
			break;
		}
#endif
	}
}

// work out how much space a triangle takes
static float	G2_AreaOfTri(const vec3_t A, const vec3_t B, const vec3_t C)
{
	vec3_t	cross, ab, cb;
	VectorSubtract(A, B, ab);
	VectorSubtract(C, B, cb);

	CrossProduct(ab, cb, cross);

	return VectorLength(cross);
}

// actually determine the S and T of the coordinate we hit in a given poly
static void G2_BuildHitPointST(const vec3_t A, const float SA, const float TA,
	const vec3_t B, const float SB, const float TB,
	const vec3_t C, const float SC, const float TC,
	const vec3_t P, float* s, float* t, float& bary_i, float& bary_j)
{
	const float	areaABC = G2_AreaOfTri(A, B, C);

	const float i = G2_AreaOfTri(P, B, C) / areaABC;
	bary_i = i;
	const float j = G2_AreaOfTri(A, P, C) / areaABC;
	bary_j = j;
	const float k = G2_AreaOfTri(A, B, P) / areaABC;

	*s = SA * i + SB * j + SC * k;
	*t = TA * i + TB * j + TC * k;

	*s = fmod(*s, 1);
	if (*s < 0)
	{
		*s += 1.0;
	}

	*t = fmod(*t, 1);
	if (*t < 0)
	{
		*t += 1.0;
	}
}

// routine that works out given a ray whether or not it hits a poly
qboolean G2_SegmentTriangleTest(const vec3_t start, const vec3_t end,
	const vec3_t A, const vec3_t B, const vec3_t C,
	qboolean back_faces, qboolean front_faces, vec3_t returned_point, vec3_t returned_normal, float* denom)
{
	static const float tiny = 1E-10f;
	vec3_t returnedNormalT;
	vec3_t edgeAC;

	VectorSubtract(C, A, edgeAC);
	VectorSubtract(B, A, returnedNormalT);

	CrossProduct(returnedNormalT, edgeAC, returned_normal);

	vec3_t ray;
	VectorSubtract(end, start, ray);

	*denom = DotProduct(ray, returned_normal);

	if (fabs(*denom) < tiny ||        // triangle parallel to ray
		(!back_faces && *denom > 0) ||		// not accepting back faces
		(!front_faces && *denom < 0))		//not accepting front faces
	{
		return qfalse;
	}

	vec3_t toPlane;
	VectorSubtract(A, start, toPlane);

	const float t = DotProduct(toPlane, returned_normal) / *denom;

	if (t < 0.0f || t>1.0f)
	{
		return qfalse; // off segment
	}

	VectorScale(ray, t, ray);

	VectorAdd(ray, start, returned_point);

	vec3_t edgePA;
	VectorSubtract(A, returned_point, edgePA);

	vec3_t edgePB;
	VectorSubtract(B, returned_point, edgePB);

	vec3_t edgePC;
	VectorSubtract(C, returned_point, edgePC);

	vec3_t temp;

	CrossProduct(edgePA, edgePB, temp);
	if (DotProduct(temp, returned_normal) < 0.0f)
	{
		return qfalse; // off triangle
	}

	CrossProduct(edgePC, edgePA, temp);
	if (DotProduct(temp, returned_normal) < 0.0f)
	{
		return qfalse; // off triangle
	}

	CrossProduct(edgePB, edgePC, temp);
	if (DotProduct(temp, returned_normal) < 0.0f)
	{
		return qfalse; // off triangle
	}
	return qtrue;
}

#ifdef _G2_GORE
struct SVertexTemp
{
	int flags;
	int touch;
	int newindex;
	float tex[2];
	SVertexTemp()
	{
		touch = 0;
	}
};

#define MAX_GORE_VERTS (3000)
static SVertexTemp GoreVerts[MAX_GORE_VERTS];
static int GoreIndexCopy[MAX_GORE_VERTS];
static int GoreTouch = 1;

#define MAX_GORE_INDECIES (6000)
static int GoreIndecies[MAX_GORE_INDECIES];

#define GORE_MARGIN (0.0f)
int	G2API_GetTime(int arg_time);

// now we at poly level, check each model space transformed poly against the model world transfomed ray
void G2_GorePolys(const mdxmSurface_t* surface, CTraceSurface& TS, const mdxmSurfHierarchy_t* surfInfo)
{
	int				j;
	vec3_t basis1;
	vec3_t basis2;
	vec3_t taxis;
	vec3_t saxis;

	basis2[0] = 0.0f;
	basis2[1] = 0.0f;
	basis2[2] = 1.0f;

	CrossProduct(TS.rayEnd, basis2, basis1);

	if (DotProduct(basis1, basis1) < .1f)
	{
		basis2[0] = 0.0f;
		basis2[1] = 1.0f;
		basis2[2] = 0.0f;
		CrossProduct(TS.rayEnd, basis2, basis1);
	}

	CrossProduct(TS.rayEnd, basis1, basis2);
	// Give me a shot direction not a bunch of zeros :) -Gil
	assert(DotProduct(basis1, basis1) > .0001f);
	assert(DotProduct(basis2, basis2) > .0001f);

	VectorNormalize(basis1);
	VectorNormalize(basis2);

	const float c = cos(TS.theta);
	const float s = sin(TS.theta);

	VectorScale(basis1, .5f * c / TS.tsize, taxis);
	VectorMA(taxis, .5f * s / TS.tsize, basis2, taxis);

	VectorScale(basis1, -.5f * s / TS.ssize, saxis);
	VectorMA(saxis, .5f * c / TS.ssize, basis2, saxis);

	const float* verts = (float*)TS.TransformedVertsArray[surface->thisSurfaceIndex];
	const int num_verts = surface->num_verts;
	int flags = 15;
	assert(num_verts < MAX_GORE_VERTS);
	for (j = 0; j < num_verts; j++)
	{
		const int pos = j * 5;
		vec3_t delta;
		delta[0] = verts[pos + 0] - TS.rayStart[0];
		delta[1] = verts[pos + 1] - TS.rayStart[1];
		delta[2] = verts[pos + 2] - TS.rayStart[2];
		const float s = DotProduct(delta, saxis) + 0.5f;
		const float t = DotProduct(delta, taxis) + 0.5f;
		int vflags = 0;
		if (s > GORE_MARGIN)
		{
			vflags |= 1;
		}
		if (s < 1.0f - GORE_MARGIN)
		{
			vflags |= 2;
		}
		if (t > GORE_MARGIN)
		{
			vflags |= 4;
		}
		if (t < 1.0f - GORE_MARGIN)
		{
			vflags |= 8;
		}
		vflags = (~vflags);
		flags &= vflags;
		GoreVerts[j].flags = vflags;
		GoreVerts[j].tex[0] = s;
		GoreVerts[j].tex[1] = t;
	}
	if (flags)
	{
		return; // completely off the gore splotch.
	}
	const int numTris = surface->numTriangles;
	const mdxmTriangle_t* tris = (mdxmTriangle_t*)((byte*)surface + surface->ofsTriangles);
	verts = (float*)TS.TransformedVertsArray[surface->thisSurfaceIndex];
	int newNumTris = 0;
	int newNumVerts = 0;
	GoreTouch++;
	if (!TS.gore)
	{
		return;
	}
	for (j = 0; j < numTris; j++)
	{
		assert(tris[j].indexes[0] >= 0 && tris[j].indexes[0] < num_verts);
		assert(tris[j].indexes[1] >= 0 && tris[j].indexes[1] < num_verts);
		assert(tris[j].indexes[2] >= 0 && tris[j].indexes[2] < num_verts);
		flags = 15 &
			GoreVerts[tris[j].indexes[0]].flags &
			GoreVerts[tris[j].indexes[1]].flags &
			GoreVerts[tris[j].indexes[2]].flags;
		if (flags)
		{
			continue;
		}
		if (!TS.gore->frontFaces || !TS.gore->backFaces)
		{
			// we need to back/front face cull
			vec3_t e1, e2, n;

			VectorSubtract(&verts[tris[j].indexes[1] * 5], &verts[tris[j].indexes[0] * 5], e1);
			VectorSubtract(&verts[tris[j].indexes[2] * 5], &verts[tris[j].indexes[0] * 5], e2);
			CrossProduct(e1, e2, n);
			if (DotProduct(TS.rayEnd, n) > 0.0f)
			{
				if (!TS.gore->frontFaces)
				{
					continue;
				}
			}
			else
			{
				if (!TS.gore->backFaces)
				{
					continue;
				}
			}
		}

		assert(newNumTris * 3 + 3 < MAX_GORE_INDECIES);
		for (int k = 0; k < 3; k++)
		{
			if (GoreVerts[tris[j].indexes[k]].touch == GoreTouch)
			{
				GoreIndecies[newNumTris * 3 + k] = GoreVerts[tris[j].indexes[k]].newindex;
			}
			else
			{
				GoreVerts[tris[j].indexes[k]].touch = GoreTouch;
				GoreVerts[tris[j].indexes[k]].newindex = newNumVerts;
				GoreIndecies[newNumTris * 3 + k] = newNumVerts;
				GoreIndexCopy[newNumVerts] = tris[j].indexes[k];
				newNumVerts++;
			}
		}
		newNumTris++;
	}
	if (!newNumVerts)
	{
		return;
	}

	int newTag;
	const std::map<std::pair<int, int>, int>::iterator f = GoreTagsTemp.find(std::make_pair(goreModelIndex, TS.surface_num));
	if (f == GoreTagsTemp.end()) // need to generate a record
	{
		newTag = AllocGoreRecord();
		CGoreSet* goreSet = nullptr;
		if (TS.ghoul2info->mGoreSetTag)
		{
			goreSet = FindGoreSet(TS.ghoul2info->mGoreSetTag);
		}
		if (!goreSet)
		{
			goreSet = NewGoreSet();
			TS.ghoul2info->mGoreSetTag = goreSet->mMyGoreSetTag;
		}
		assert(goreSet);
		SGoreSurface add;
		add.shader = TS.goreShader;
		add.mDeleteTime = 0;
		if (TS.gore->lifeTime)
		{
			add.mDeleteTime = G2API_GetTime(0) + TS.gore->lifeTime;
		}
		add.mFadeTime = TS.gore->fadeOutTime;
		add.mFadeRGB = (TS.gore->fadeRGB != qfalse);
		add.mGoreTag = newTag;

		add.mGoreGrowStartTime = G2API_GetTime(0);
		if (TS.gore->growDuration == -1)
		{
			add.mGoreGrowEndTime = -1;    // set this to -1 to disable growing
		}
		else
		{
			add.mGoreGrowEndTime = G2API_GetTime(0) + TS.gore->growDuration;
		}

		assert(TS.gore->growDuration != 0);
		add.mGoreGrowFactor = (1.0f - TS.gore->goreScaleStartFraction) / static_cast<float>(TS.gore->growDuration);	//curscale = (curtime-mGoreGrowStartTime)*mGoreGrowFactor;
		add.mGoreGrowOffset = TS.gore->goreScaleStartFraction;

		goreSet->mGoreRecords.insert(std::make_pair(TS.surface_num, add));
		GoreTagsTemp[std::make_pair(goreModelIndex, TS.surface_num)] = newTag;
	}
	else
	{
		newTag = (*f).second;
	}
	GoreTextureCoordinates* gore = FindGoreRecord(newTag);
	if (gore)
	{
		assert(sizeof(float) == sizeof(int));
		// data block format:
		const unsigned int size =
			sizeof(int) + // num verts
			sizeof(int) + // num tris
			sizeof(int) * newNumVerts + // which verts to copy from original surface
			sizeof(float) * 4 * newNumVerts + // storgage for deformed verts
			sizeof(float) * 4 * newNumVerts + // storgage for deformed normal
			sizeof(float) * 2 * newNumVerts + // texture coordinates
			sizeof(int) * newNumTris * 3;  // new indecies

		int* data = static_cast<int*>(Z_Malloc(sizeof(int) * size, TAG_GHOUL2_GORE, qtrue));

		if (gore->tex[TS.lod])
		{
			Z_Free(gore->tex[TS.lod]);
		}

		gore->tex[TS.lod] = (float*)data;
		*data++ = newNumVerts;
		*data++ = newNumTris;

		memcpy(data, GoreIndexCopy, sizeof(int) * newNumVerts);
		data += newNumVerts * 9; // skip verts and normals
		float* fdata = (float*)data;

		for (j = 0; j < newNumVerts; j++)
		{
			*fdata++ = GoreVerts[GoreIndexCopy[j]].tex[0];
			*fdata++ = GoreVerts[GoreIndexCopy[j]].tex[1];
		}
		data = (int*)fdata;
		memcpy(data, GoreIndecies, sizeof(int) * newNumTris * 3);
		data += newNumTris * 3;
		assert((data - (int*)gore->tex[TS.lod]) * sizeof(int) == size);
		fdata = (float*)data;
		// build the entity to gore matrix
		VectorCopy(saxis, fdata + 0);
		VectorCopy(taxis, fdata + 4);
		VectorCopy(TS.rayEnd, fdata + 8);
		VectorNormalize(fdata + 0);
		VectorNormalize(fdata + 4);
		VectorNormalize(fdata + 8);
		fdata[3] = -0.5f; // subtract texture center
		fdata[7] = -0.5f;
		fdata[11] = 0.0f;
		vec3_t shotOriginInCurrentSpace; // unknown space
		TransformPoint(TS.rayStart, shotOriginInCurrentSpace, (mdxaBone_t*)fdata); // dest middle arg
		// this will insure the shot origin in our unknown space is now the shot origin, making it a known space
		fdata[3] -= shotOriginInCurrentSpace[0];
		fdata[7] -= shotOriginInCurrentSpace[1];
		fdata[11] -= shotOriginInCurrentSpace[2];
		Inverse_Matrix((mdxaBone_t*)fdata, (mdxaBone_t*)(fdata + 12));  // dest 2nd arg
		data += 24;

		//		assert((data - (int *)gore->tex[TS.lod]) * sizeof(int) == size);
	}
}
#else
struct SVertexTemp
{
	int flags;
	//	int touch;
	//	int newindex;
	//	float tex[2];
	SVertexTemp()
	{
		//		touch=0;
	}
};

#define MAX_GORE_VERTS (3000)
static SVertexTemp GoreVerts[MAX_GORE_VERTS];
#endif

// now we're at poly level, check each model space transformed poly against the model world transfomed ray
static bool G2_TracePolys(const mdxmSurface_t* surface, const mdxmSurfHierarchy_t* surfInfo, CTraceSurface& TS)
{
	// whip through and actually transform each vertex
	const mdxmTriangle_t* tris = (mdxmTriangle_t*)((byte*)surface + surface->ofsTriangles);
	const float* verts = (float*)TS.TransformedVertsArray[surface->thisSurfaceIndex];
	const int numTris = surface->numTriangles;
	for (int j = 0; j < numTris; j++)
	{
		float			face;
		vec3_t	hitPoint, normal;
		// determine actual coords for this triangle
		const float* point1 = &verts[(tris[j].indexes[0] * 5)];
		const float* point2 = &verts[(tris[j].indexes[1] * 5)];
		const float* point3 = &verts[(tris[j].indexes[2] * 5)];
		// did we hit it?
		int i;
		if (G2_SegmentTriangleTest(TS.rayStart, TS.rayEnd, point1, point2, point3, qtrue, qtrue, hitPoint, normal, &face))
		{	// find space in the collision records for this record
			for (i = 0; i < MAX_G2_COLLISIONS; i++)
			{
				if (TS.collRecMap[i].mEntityNum == -1)
				{
					CollisionRecord_t& newCol = TS.collRecMap[i];
					vec3_t			  	distVect;
					float				x_pos = 0, y_pos = 0;

					newCol.mPolyIndex = j;
					newCol.mEntityNum = TS.ent_num;
					newCol.mSurfaceIndex = surface->thisSurfaceIndex;
					newCol.mModelIndex = TS.model_index;
					if (face > 0)
					{
						newCol.mFlags = G2_FRONTFACE;
					}
					else
					{
						newCol.mFlags = G2_BACKFACE;
					}

					VectorSubtract(hitPoint, TS.rayStart, distVect);
					newCol.mDistance = VectorLength(distVect);

					// put the hit point back into world space
					TransformAndTranslatePoint(hitPoint, newCol.mCollisionPosition, &worldMatrix);

					// transform normal (but don't translate) into world angles
					TransformPoint(normal, newCol.mCollisionNormal, &worldMatrix);
					VectorNormalize(newCol.mCollisionNormal);

					newCol.mMaterial = newCol.mLocation = 0;

					// Determine our location within the texture, and barycentric coordinates
					G2_BuildHitPointST(point1, point1[3], point1[4],
						point2, point2[3], point2[4],
						point3, point3[3], point3[4],
						hitPoint, &x_pos, &y_pos, newCol.mBarycentricI, newCol.mBarycentricJ);

					/*
										const shader_t		*shader = 0;
										// now, we know what surface this hit belongs to, we need to go get the shader handle so we can get the correct hit location and hit material info
										if ( cust_shader )
										{
											shader = cust_shader;
										}
										else if ( skin )
										{
											int		j;

											// match the surface name to something in the skin file
											shader = tr.defaultShader;
											for ( j = 0 ; j < skin->numSurfaces ; j++ )
											{
												// the names have both been lowercased
												if ( !strcmp( skin->surfaces[j]->name, surfInfo->name ) )
												{
													shader = skin->surfaces[j]->shader;
													break;
												}
											}
										}
										else
										{
											shader = R_GetShaderByHandle( surfInfo->shaderIndex );
										}

										// do we even care to decide what the hit or location area's are? If we don't have them in the shader there is little point
										if ((shader->hitLocation) || (shader->hitMaterial))
										{
											// ok, we have a floating point position. - determine location in data we need to look at
											if (shader->hitLocation)
											{
												newCol.mLocation = *(hitMatReg[shader->hitLocation].loc +
																	((int)(y_pos * hitMatReg[shader->hitLocation].height) * hitMatReg[shader->hitLocation].width) +
																	((int)(x_pos * hitMatReg[shader->hitLocation].width)));
												ri->Printf( PRINT_ALL, "G2_TracePolys hit location: %d\n", newCol.mLocation);
											}

											if (shader->hitMaterial)
											{
												newCol.mMaterial = *(hitMatReg[shader->hitMaterial].loc +
																	((int)(y_pos * hitMatReg[shader->hitMaterial].height) * hitMatReg[shader->hitMaterial].width) +
																	((int)(x_pos * hitMatReg[shader->hitMaterial].width)));
											}
										}
					*/
					// exit now if we should
					if (TS.traceFlags == G2_RETURNONHIT)
					{
						TS.hitOne = true;
						return true;
					}

					break;
				}
			}
			if (i == MAX_G2_COLLISIONS)
			{
				//assert(i!=MAX_G2_COLLISIONS);		// run out of collision record space - will probalbly never happen
				//It happens. And the assert is bugging me.
				TS.hitOne = true;	//force stop recursion
				return true;	// return true to avoid wasting further time, but no hit will result without a record
			}
		}
	}
	return false;
}

// now we're at poly level, check each model space transformed poly against the model world transfomed ray
static bool G2_RadiusTracePolys(
	const mdxmSurface_t* surface,
	CTraceSurface& TS
)
{
	int		j;
	vec3_t basis1;
	vec3_t basis2;
	vec3_t taxis;
	vec3_t saxis;

	basis2[0] = 0.0f;
	basis2[1] = 0.0f;
	basis2[2] = 1.0f;

	vec3_t v3RayDir;
	VectorSubtract(TS.rayEnd, TS.rayStart, v3RayDir);

	CrossProduct(v3RayDir, basis2, basis1);

	if (DotProduct(basis1, basis1) < .1f)
	{
		basis2[0] = 0.0f;
		basis2[1] = 1.0f;
		basis2[2] = 0.0f;
		CrossProduct(v3RayDir, basis2, basis1);
	}

	CrossProduct(v3RayDir, basis1, basis2);
	// Give me a shot direction not a bunch of zeros :) -Gil
//	assert(DotProduct(basis1,basis1)>.0001f);
//	assert(DotProduct(basis2,basis2)>.0001f);

	VectorNormalize(basis1);
	VectorNormalize(basis2);

	const float c = cos(0.0f);//theta
	const float s = sin(0.0f);//theta

	VectorScale(basis1, 0.5f * c / TS.m_fRadius, taxis);
	VectorMA(taxis, 0.5f * s / TS.m_fRadius, basis2, taxis);

	VectorScale(basis1, -0.5f * s / TS.m_fRadius, saxis);
	VectorMA(saxis, 0.5f * c / TS.m_fRadius, basis2, saxis);

	const float* const verts = (float*)TS.TransformedVertsArray[surface->thisSurfaceIndex];
	const int num_verts = surface->num_verts;

	int flags = 63;
	//rayDir/=lengthSquared(raydir);
	const float f = VectorLengthSquared(v3RayDir);
	v3RayDir[0] /= f;
	v3RayDir[1] /= f;
	v3RayDir[2] /= f;

	for (j = 0; j < num_verts; j++)
	{
		const int pos = j * 5;
		vec3_t delta;
		delta[0] = verts[pos + 0] - TS.rayStart[0];
		delta[1] = verts[pos + 1] - TS.rayStart[1];
		delta[2] = verts[pos + 2] - TS.rayStart[2];
		const float s = DotProduct(delta, saxis) + 0.5f;
		const float t = DotProduct(delta, taxis) + 0.5f;
		const float u = DotProduct(delta, v3RayDir);
		int vflags = 0;

		if (s > 0)
		{
			vflags |= 1;
		}
		if (s < 1)
		{
			vflags |= 2;
		}
		if (t > 0)
		{
			vflags |= 4;
		}
		if (t < 1)
		{
			vflags |= 8;
		}
		if (u > 0)
		{
			vflags |= 16;
		}
		if (u < 1)
		{
			vflags |= 32;
		}

		vflags = (~vflags);
		flags &= vflags;
		GoreVerts[j].flags = vflags;
	}

	if (flags)
	{
		return false; // completely off the gore splotch  (so presumably hit nothing? -Ste)
	}
	const int numTris = surface->numTriangles;
	const mdxmTriangle_t* const tris = (mdxmTriangle_t*)((byte*)surface + surface->ofsTriangles);

	for (j = 0; j < numTris; j++)
	{
		assert(tris[j].indexes[0] >= 0 && tris[j].indexes[0] < num_verts);
		assert(tris[j].indexes[1] >= 0 && tris[j].indexes[1] < num_verts);
		assert(tris[j].indexes[2] >= 0 && tris[j].indexes[2] < num_verts);
		flags = 63 &
			GoreVerts[tris[j].indexes[0]].flags &
			GoreVerts[tris[j].indexes[1]].flags &
			GoreVerts[tris[j].indexes[2]].flags;
		int i;
		if (flags)
		{
			continue;
		}
		// we hit a triangle, so init a collision record...
		//
		for (i = 0; i < MAX_G2_COLLISIONS; i++)
		{
			if (TS.collRecMap[i].mEntityNum == -1)
			{
				CollisionRecord_t& newCol = TS.collRecMap[i];

				newCol.mPolyIndex = j;
				newCol.mEntityNum = TS.ent_num;
				newCol.mSurfaceIndex = surface->thisSurfaceIndex;
				newCol.mModelIndex = TS.model_index;
				//					if (face>0)
				//					{
				newCol.mFlags = G2_FRONTFACE;
				//					}
				//					else
				//					{
				//						newCol.mFlags = G2_BACKFACE;
				//					}

				//get normal from triangle
				const float* A = &verts[(tris[j].indexes[0] * 5)];
				const float* B = &verts[(tris[j].indexes[1] * 5)];
				const float* C = &verts[(tris[j].indexes[2] * 5)];
				vec3_t normal;
				vec3_t edgeAC, edgeBA;

				VectorSubtract(C, A, edgeAC);
				VectorSubtract(B, A, edgeBA);
				CrossProduct(edgeBA, edgeAC, normal);

				// transform normal (but don't translate) into world angles
				TransformPoint(normal, newCol.mCollisionNormal, &worldMatrix);
				VectorNormalize(newCol.mCollisionNormal);

				newCol.mMaterial = newCol.mLocation = 0;
				// exit now if we should
				if (TS.traceFlags == G2_RETURNONHIT)
				{
					TS.hitOne = true;
					return true;
				}

				vec3_t			  distVect;
#if 0
				//i don't know the hitPoint, but let's just assume it's the first vert for now...
				float* hitPoint = (float*)A;
#else
				//yeah, I want the collision point. Let's work out the impact point on the triangle. -rww
				vec3_t hitPoint;
				const float third = -(A[0] * (B[1] * C[2] - C[1] * B[2]) + B[0] * (C[1] * A[2] - A[1] * C[2]) + C[0] * (A[1] * B[2] - B[1] * A[2]));

				VectorSubtract(TS.rayEnd, TS.rayStart, distVect);
				const float side = normal[0] * TS.rayStart[0] + normal[1] * TS.rayStart[1] + normal[2] * TS.rayStart[2] + third;
				const float side2 = normal[0] * distVect[0] + normal[1] * distVect[1] + normal[2] * distVect[2];
				const float dist = side / side2;
				VectorMA(TS.rayStart, -dist, distVect, hitPoint);
#endif

				VectorSubtract(hitPoint, TS.rayStart, distVect);
				newCol.mDistance = VectorLength(distVect);

				// put the hit point back into world space
				TransformAndTranslatePoint(hitPoint, newCol.mCollisionPosition, &worldMatrix);
				newCol.mBarycentricI = newCol.mBarycentricJ = 0.0f;
				break;
			}
		}
		if (i == MAX_G2_COLLISIONS)
		{
			//assert(i!=MAX_G2_COLLISIONS);		// run out of collision record space - happens OFTEN
			TS.hitOne = true;	//force stop recursion
			return true;	// return true to avoid wasting further time, but no hit will result without a record
		}
	}

	return false;
}

// look at a surface and then do the trace on each poly
static void G2_TraceSurfaces(CTraceSurface& TS)
{
	// back track and get the surfinfo struct for this surface
	assert(TS.current_model);
	assert(TS.current_model->mdxm);
	const mdxmSurface_t* surface = static_cast<mdxmSurface_t*>(G2_FindSurface(TS.current_model, TS.surface_num, TS.lod));
	const mdxmHierarchyOffsets_t* surf_indexes = (mdxmHierarchyOffsets_t*)((byte*)TS.current_model->mdxm + sizeof(mdxmHeader_t));
	const mdxmSurfHierarchy_t* surfInfo = (mdxmSurfHierarchy_t*)((byte*)surf_indexes + surf_indexes->offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	const surfaceInfo_t* surfOverride = G2_FindOverrideSurface(TS.surface_num, TS.rootSList);

	// don't allow recursion if we've already hit a polygon
	if (TS.hitOne)
	{
		return;
	}

	// really, we should use the default flags for this surface unless it's been overriden
	int off_flags = surfInfo->flags;

	// set the off flags if we have some
	if (surfOverride)
	{
		off_flags = surfOverride->off_flags;
	}

	// if this surface is not off, try to hit it
	if (!off_flags)
	{
#ifdef _G2_GORE
		if (TS.collRecMap)
		{
#endif
			if (!(fabs(TS.m_fRadius) < 0.1))	// if not a point-trace
			{
				// .. then use radius check
				//
				if (G2_RadiusTracePolys(surface,		// const mdxmSurface_t *surface,
					TS
				)
					&& (TS.traceFlags == G2_RETURNONHIT)
					)
				{
					TS.hitOne = true;
					return;
				}
			}
			else
			{
				// go away and trace the polys in this surface
				if (G2_TracePolys(surface, surfInfo, TS)
					&& (TS.traceFlags == G2_RETURNONHIT)
					)
				{
					// ok, we hit one, *and* we want to return instantly because the returnOnHit is set
					// so indicate we've hit one, so other surfaces don't get hit and return
					TS.hitOne = true;
					return;
				}
			}
#ifdef _G2_GORE
		}
		else
		{
			G2_GorePolys(surface, TS, surfInfo);
		}
#endif
	}

	// if we are turning off all descendants, then stop this recursion now
	if (off_flags & G2SURFACEFLAG_NODESCENDANTS)
	{
		return;
	}

	// now recursively call for the children
	for (int i = 0; i < surfInfo->numChildren && !TS.hitOne; i++)
	{
		TS.surface_num = surfInfo->childIndexes[i];
		G2_TraceSurfaces(TS);
	}
}

#ifdef _G2_GORE
void G2_TraceModels(CGhoul2Info_v& ghoul2, vec3_t rayStart, vec3_t rayEnd, CollisionRecord_t* collRecMap, int ent_num, int e_g2_trace_type, int use_lod, float fRadius, float ssize, float tsize, float theta, int shader, SSkinGoreData* gore, qboolean skipIfLODNotMatch)
#else
void G2_TraceModels(CGhoul2Info_v& ghoul2, vec3_t rayStart, vec3_t rayEnd, CollisionRecord_t* collRecMap, int ent_num, int e_g2_trace_type, int use_lod, float fRadius)
#endif
{
	int lod;
	skin_t* skin;
	shader_t* cust_shader;
	qboolean		firstModelOnly = qfalse;

	if (cg_g2MarksAllModels == nullptr)
	{
		cg_g2MarksAllModels = ri->Cvar_Get("cg_g2MarksAllModels", "0", 0, "");
	}

	if (cg_g2MarksAllModels == nullptr
		|| !cg_g2MarksAllModels->integer)
	{
		firstModelOnly = qtrue;
	}

	// walk each possible model for this entity and try tracing against it
	for (int i = 0; i < ghoul2.size(); i++)
	{
#ifdef _G2_GORE
		goreModelIndex = i;
		// don't bother with models that we don't care about.
		if (ghoul2[i].mModelindex == -1)
		{
			continue;
		}
#endif

		// don't bother with models that we don't care about.
		if (!ghoul2[i].mValid)
		{
			continue;
		}
		//		assert(G2_MODEL_OK(&ghoul2[i]));
				// do we really want to collide with this object?
		if (ghoul2[i].mFlags & GHOUL2_NOCOLLIDE)
		{
			continue;
		}

		if (ghoul2[i].mCustomShader && ghoul2[i].mCustomShader != -20) //rww - -20 is a server instance (hack)
		{
			cust_shader = (shader_t*)R_GetShaderByHandle(ghoul2[i].mCustomShader);
		}
		else
		{
			cust_shader = nullptr;
		}

		// figure out the custom skin thing
		if (ghoul2[i].mSkin > 0 && ghoul2[i].mSkin < tr.numSkins)
		{
			skin = R_GetSkinByHandle(ghoul2[i].mSkin);
		}
		else
		{
			skin = nullptr;
		}

		lod = G2_DecideTraceLod(ghoul2[i], use_lod);
		if (skipIfLODNotMatch)
		{//we only want to hit this SPECIFIC LOD...
			if (lod != use_lod)
			{//doesn't match, skip this model
				continue;
			}
		}

		//reset the quick surface override lookup
		G2_FindOverrideSurface(-1, ghoul2[i].mSlist);

#ifdef _G2_GORE
		CTraceSurface TS(ghoul2[i].mSurfaceRoot, ghoul2[i].mSlist, (model_t*)ghoul2[i].current_model, lod, rayStart, rayEnd, collRecMap, ent_num, i, skin, cust_shader, ghoul2[i].mTransformedVertsArray, e_g2_trace_type, fRadius, ssize, tsize, theta, shader, &ghoul2[i], gore);
#else
		CTraceSurface TS(ghoul2[i].mSurfaceRoot, ghoul2[i].mSlist, (model_t*)ghoul2[i].current_model, lod, rayStart, rayEnd, collRecMap, ent_num, i, skin, cust_shader, ghoul2[i].mTransformedVertsArray, e_g2_trace_type, fRadius);
#endif
		// start the surface recursion loop
		G2_TraceSurfaces(TS);

		// if we've hit one surface on one model, don't bother doing the rest
		if (TS.hitOne)
		{
			break;
		}
#ifdef _G2_GORE
		if (!collRecMap && firstModelOnly)
		{
			// we don't really need to do multiple models for gore.
			break;
		}
#endif
	}
}

void TransformPoint(const vec3_t in, vec3_t out, mdxaBone_t* mat) {
	for (int i = 0; i < 3; i++)
	{
		out[i] = in[0] * mat->matrix[i][0] + in[1] * mat->matrix[i][1] + in[2] * mat->matrix[i][2];
	}
}

void TransformAndTranslatePoint(const vec3_t in, vec3_t out, mdxaBone_t* mat) {
	for (int i = 0; i < 3; i++)
	{
		out[i] = in[0] * mat->matrix[i][0] + in[1] * mat->matrix[i][1] + in[2] * mat->matrix[i][2] + mat->matrix[i][3];
	}
}

// create a matrix using a set of angles
void Create_Matrix(const float* angle, mdxaBone_t* matrix)
{
	matrix3_t	axis;

	// convert angles to axis
	AnglesToAxis(angle, axis);
	matrix->matrix[0][0] = axis[0][0];
	matrix->matrix[1][0] = axis[0][1];
	matrix->matrix[2][0] = axis[0][2];

	matrix->matrix[0][1] = axis[1][0];
	matrix->matrix[1][1] = axis[1][1];
	matrix->matrix[2][1] = axis[1][2];

	matrix->matrix[0][2] = axis[2][0];
	matrix->matrix[1][2] = axis[2][1];
	matrix->matrix[2][2] = axis[2][2];

	matrix->matrix[0][3] = 0;
	matrix->matrix[1][3] = 0;
	matrix->matrix[2][3] = 0;
}

// given a matrix, generate the inverse of that matrix
void Inverse_Matrix(mdxaBone_t* src, mdxaBone_t* dest)
{
	int i, j;

	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			dest->matrix[i][j] = src->matrix[j][i];
		}
	}
	for (i = 0; i < 3; i++)
	{
		dest->matrix[i][3] = 0;
		for (j = 0; j < 3; j++)
		{
			dest->matrix[i][3] -= dest->matrix[i][j] * src->matrix[j][3];
		}
	}
}

// generate the world matrix for a given set of angles and origin - called from lots of places
void G2_GenerateWorldMatrix(const vec3_t angles, const vec3_t origin)
{
	Create_Matrix(angles, &worldMatrix);
	worldMatrix.matrix[0][3] = origin[0];
	worldMatrix.matrix[1][3] = origin[1];
	worldMatrix.matrix[2][3] = origin[2];

	Inverse_Matrix(&worldMatrix, &worldMatrixInv);
}

// go away and determine what the pointer for a specific surface definition within the model definition is
void* G2_FindSurface(void* mod_t, int index, int lod)
{
	// damn include file dependancies
	model_t* mod = static_cast<model_t*>(mod_t);

	// point at first lod list
	byte* current = (byte*)((size_t)mod->mdxm + static_cast<size_t>(mod->mdxm->ofsLODs));

	//walk the lods
	for (int i = 0; i < lod; i++)
	{
		const mdxmLOD_t* lodData = (mdxmLOD_t*)current;
		current += lodData->ofsEnd;
	}

	// avoid the lod pointer data structure
	current += sizeof(mdxmLOD_t);

	const mdxmLODSurfOffset_t* indexes = (mdxmLODSurfOffset_t*)current;
	// we are now looking at the offset array
	current += indexes->offsets[index];

	return (void*)current;
}

#define SURFACE_SAVE_BLOCK_SIZE	sizeof(surfaceInfo_t)
#define BOLT_SAVE_BLOCK_SIZE (sizeof(boltInfo_t) - sizeof(mdxaBone_t))
#define BONE_SAVE_BLOCK_SIZE sizeof(boneInfo_t)

qboolean G2_SaveGhoul2Models(CGhoul2Info_v& ghoul2, char** buffer, int* size)
{
	// is there anything to save?
	if (!ghoul2.size())
	{
		*buffer = static_cast<char*>(Z_Malloc(4, TAG_GHOUL2, qtrue));
		int* tempBuffer = (int*)*buffer;
		*tempBuffer = 0;
		*size = 4;
		return qtrue;
	}

	// yeah, lets get busy
	*size = 0;

	// this one isn't a define since I couldn't work out how to figure it out at compile time
	const int ghoul2BlockSize = (size_t)&ghoul2[0].mTransformedVertsArray - (size_t)&ghoul2[0].mModelindex;

	// add in count for number of ghoul2 models
	*size += 4;
	// start out working out the total size of the buffer we need to allocate
	int i; // Linux GCC is forcing new scoping rules
	for (i = 0; i < ghoul2.size(); i++)
	{
		*size += ghoul2BlockSize;
		// add in count for number of surfaces
		*size += 4;
		*size += (ghoul2[i].mSlist.size() * SURFACE_SAVE_BLOCK_SIZE);
		// add in count for number of bones
		*size += 4;
		*size += (ghoul2[i].mBlist.size() * BONE_SAVE_BLOCK_SIZE);
		// add in count for number of bolts
		*size += 4;
		*size += (ghoul2[i].mBltlist.size() * BOLT_SAVE_BLOCK_SIZE);
	}

	// ok, we should know how much space we need now
	*buffer = static_cast<char*>(Z_Malloc(*size, TAG_GHOUL2, qtrue));

	// now lets start putting the data we care about into the buffer
	char* tempBuffer = *buffer;

	// save out how many ghoul2 models we have
	*(int*)tempBuffer = ghoul2.size();
	tempBuffer += 4;

	for (i = 0; i < ghoul2.size(); i++)
	{
		// first save out the ghoul2 details themselves
//		OutputDebugString(va("G2_SaveGhoul2Models(): ghoul2[%d].mModelindex = %d\n",i,ghoul2[i].mModelindex));
		memcpy(tempBuffer, &ghoul2[i].mModelindex, ghoul2BlockSize);
		tempBuffer += ghoul2BlockSize;

		// save out how many surfaces we have
		*(int*)tempBuffer = ghoul2[i].mSlist.size();
		tempBuffer += 4;

		// now save the all the surface list info
		for (size_t x = 0; x < ghoul2[i].mSlist.size(); x++)
		{
			memcpy(tempBuffer, &ghoul2[i].mSlist[x], SURFACE_SAVE_BLOCK_SIZE);
			tempBuffer += SURFACE_SAVE_BLOCK_SIZE;
		}

		// save out how many bones we have
		*(int*)tempBuffer = ghoul2[i].mBlist.size();
		tempBuffer += 4;

		// now save the all the bone list info
		for (size_t x = 0; x < ghoul2[i].mBlist.size(); x++)
		{
			memcpy(tempBuffer, &ghoul2[i].mBlist[x], BONE_SAVE_BLOCK_SIZE);
			tempBuffer += BONE_SAVE_BLOCK_SIZE;
		}

		// save out how many bolts we have
		*(int*)tempBuffer = ghoul2[i].mBltlist.size();
		tempBuffer += 4;

		// lastly save the all the bolt list info
		for (size_t x = 0; x < ghoul2[i].mBltlist.size(); x++)
		{
			memcpy(tempBuffer, &ghoul2[i].mBltlist[x], BOLT_SAVE_BLOCK_SIZE);
			tempBuffer += BOLT_SAVE_BLOCK_SIZE;
		}
	}

	return qtrue;
}

// have to free space malloced in the save system here because the game DLL can't.
void G2_FreeSaveBuffer(char* buffer)
{
	Z_Free(buffer);
}

qboolean G2_SetupModelPointers(CGhoul2Info* ghl_info);
qboolean G2_SetupModelPointers(CGhoul2Info_v& ghoul2);

void G2_LoadGhoul2Model(CGhoul2Info_v& ghoul2, const char* buffer)
{
	// first thing, lets see how many ghoul2 models we have, and resize our buffers accordingly
	const int newSize = *(int*)buffer;
	ghoul2.resize(newSize);
	buffer += 4;

	// did we actually resize to a value?
	if (!newSize)
	{
		// no, ok, well, done then.
		return;
	}

	// this one isn't a define since I couldn't work out how to figure it out at compile time
	const int ghoul2BlockSize = (size_t)&ghoul2[0].mTransformedVertsArray - (size_t)&ghoul2[0].mModelindex;

	// now we have enough instances, lets go through each one and load up the relevant details
	for (int i = 0; i < ghoul2.size(); i++)
	{
		ghoul2[i].mSkelFrameNum = 0;
		ghoul2[i].mModelindex = -1;
		ghoul2[i].mFileName[0] = 0;
		ghoul2[i].mValid = false;
		// load the ghoul2 info from the buffer
		memcpy(&ghoul2[i].mModelindex, buffer, ghoul2BlockSize);
		//		OutputDebugString(va("G2_LoadGhoul2Model(): ghoul2[%d].mModelindex = %d\n",i,ghoul2[i].mModelindex));
		buffer += ghoul2BlockSize;

		if (ghoul2[i].mModelindex != -1 && ghoul2[i].mFileName[0])
		{
			ghoul2[i].mModelindex = i;
			G2_SetupModelPointers(&ghoul2[i]);
		}

		// give us enough surfaces to load up the data
		ghoul2[i].mSlist.resize(*(int*)buffer);
		buffer += 4;

		// now load all the surfaces
		for (size_t x = 0; x < ghoul2[i].mSlist.size(); x++)
		{
			memcpy(&ghoul2[i].mSlist[x], buffer, SURFACE_SAVE_BLOCK_SIZE);
			buffer += SURFACE_SAVE_BLOCK_SIZE;
		}

		// give us enough bones to load up the data
		ghoul2[i].mBlist.resize(*(int*)buffer);
		buffer += 4;

		// now load all the bones
		for (size_t x = 0; x < ghoul2[i].mBlist.size(); x++)
		{
			memcpy(&ghoul2[i].mBlist[x], buffer, BONE_SAVE_BLOCK_SIZE);
			buffer += BONE_SAVE_BLOCK_SIZE;
		}

		// give us enough bolts to load up the data
		ghoul2[i].mBltlist.resize(*(int*)buffer);
		buffer += 4;

		// now load all the bolts
		for (size_t x = 0; x < ghoul2[i].mBltlist.size(); x++)
		{
			memcpy(&ghoul2[i].mBltlist[x], buffer, BOLT_SAVE_BLOCK_SIZE);
			buffer += BOLT_SAVE_BLOCK_SIZE;
		}
	}
}

void G2_LerpAngles(CGhoul2Info_v& ghoul2, CGhoul2Info_v& nextGhoul2, float interpolation)
{
	// loop each model
	for (int i = 0; i < ghoul2.size(); i++)
	{
		if (ghoul2[i].mModelindex != -1)
		{
			// now walk the bone list
			for (size_t x = 0; x < ghoul2[i].mBlist.size(); x++)
			{
				boneInfo_t& bone = ghoul2[i].mBlist[x];
				// sure we have one to lerp to?
				if ((nextGhoul2.size() > i) &&
					(nextGhoul2[i].mModelindex != -1) &&
					(nextGhoul2[i].mBlist.size() > x) &&
					(nextGhoul2[i].mBlist[x].boneNumber != -1))
				{
					boneInfo_t& nextBone = nextGhoul2[i].mBlist[x];
					// does this bone override actually have anything in it, and if it does, is it a bone angles override?
					if ((bone.boneNumber != -1) && ((bone.flags) & (BONE_ANGLES_TOTAL)))
					{
						const float* nowMatrix = (float*)&bone.matrix;
						const float* nextMatrix = (float*)&nextBone.matrix;
						float* newMatrix = (float*)&bone.newMatrix;
						// now interpolate the matrix
						for (int z = 0; z < 12; z++)
						{
							newMatrix[z] = nowMatrix[z] + interpolation * (nextMatrix[z] - nowMatrix[z]);
						}
					}
				}
				else
				{
					memcpy(&ghoul2[i].mBlist[x].newMatrix, &ghoul2[i].mBlist[x].matrix, sizeof(mdxaBone_t));
				}
			}
		}
	}
}