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

#include "../server/exe_headers.h"

#include "../client/client.h"	//FIXME!! EVIL - just include the definitions needed
#include "../client/vmachine.h"

#if !defined(TR_LOCAL_H)
#include "tr_local.h"
#endif

#include "tr_common.h"

#include "qcommon/matcomp.h"
#if !defined(_QCOMMON_H_)
#include "../qcommon/qcommon.h"
#endif
#if !defined(G2_H_INC)
#include "../ghoul2/G2.h"
#endif

#ifdef _G2_GORE
#include "../ghoul2/ghoul2_gore.h"
#endif

#define	LL(x) x=LittleLong(x)
#define	LS(x) x=LittleShort(x)
#define	LF(x) x=LittleFloat(x)

#ifdef G2_PERFORMANCE_ANALYSIS
#include "../qcommon/timing.h"
timing_c G2PerformanceTimer_RB_SurfaceGhoul;

int G2PerformanceCounter_G2_TransformGhoulBones = 0;

int G2Time_RB_SurfaceGhoul = 0;

void G2Time_ResetTimers(void)
{
	G2Time_RB_SurfaceGhoul = 0;
	G2PerformanceCounter_G2_TransformGhoulBones = 0;
}

void G2Time_ReportTimers(void)
{
	Com_Printf("\n---------------------------------\nRB_SurfaceGhoul: %i\nTransformGhoulBones calls: %i\n---------------------------------\n\n",
		G2Time_RB_SurfaceGhoul,
		G2PerformanceCounter_G2_TransformGhoulBones
	);
}
#endif

//rww - RAGDOLL_BEGIN
#include <cfloat>
//rww - RAGDOLL_END

extern	cvar_t* r_Ghoul2UnSqash;
extern	cvar_t* r_Ghoul2AnimSmooth;
extern	cvar_t* r_Ghoul2NoLerp;
extern	cvar_t* r_Ghoul2NoBlend;
extern	cvar_t* r_Ghoul2UnSqashAfterSmooth;

bool HackadelicOnClient = false; // means this is a render traversal

// I hate doing this, but this is the simplest way to get this into the routines it needs to be
mdxaBone_t		worldMatrix;
mdxaBone_t		worldMatrixInv;
#ifdef _G2_GORE
qhandle_t		goreShader = -1;
#endif

constexpr static mdxaBone_t		identityMatrix =
{
	{
		{ 0.0f, -1.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f }
	}
};

class CTransformBone
{
public:
	//rww - RAGDOLL_BEGIN
	int				touchRender;
	//rww - RAGDOLL_END

	mdxaBone_t		boneMatrix; //final matrix
	int				parent; // only set once
	int				touch; // for minimal recalculation
	CTransformBone() : boneMatrix(), parent(0)
	{
		touch = 0;

		//rww - RAGDOLL_BEGIN
		touchRender = 0;
		//rww - RAGDOLL_END
	}
};

struct SBoneCalc
{
	int				newFrame;
	int				current_frame;
	float			backlerp;
	float			blendFrame;
	int				blendOldFrame;
	bool			blendMode;
	float			blendLerp;
};

class CBoneCache;
void G2_TransformBone(int index, const CBoneCache& cb);

class CBoneCache
{
	void EvalLow(const int index)
	{
		assert(index >= 0 && index < mNumBones);
		if (mFinalBones[index].touch != mCurrentTouch)
		{
			// need to evaluate the bone
			assert((mFinalBones[index].parent >= 0 && mFinalBones[index].parent < mNumBones) || (index == 0 && mFinalBones[index].parent == -1));
			if (mFinalBones[index].parent >= 0)
			{
				EvalLow(mFinalBones[index].parent); // make sure parent is evaluated
				const SBoneCalc& par = mBones[mFinalBones[index].parent];
				mBones[index].newFrame = par.newFrame;
				mBones[index].current_frame = par.current_frame;
				mBones[index].backlerp = par.backlerp;
				mBones[index].blendFrame = par.blendFrame;
				mBones[index].blendOldFrame = par.blendOldFrame;
				mBones[index].blendMode = par.blendMode;
				mBones[index].blendLerp = par.blendLerp;
			}
			G2_TransformBone(index, *this);
			mFinalBones[index].touch = mCurrentTouch;
		}
	}

	//rww - RAGDOLL_BEGIN
	void SmoothLow(const int index) const
	{
		if (mSmoothBones[index].touch == mLastTouch)
		{
			float* old_m = &mSmoothBones[index].boneMatrix.matrix[0][0];
			float* new_m = &mFinalBones[index].boneMatrix.matrix[0][0];
			for (int i = 0; i < 12; i++, old_m++, new_m++)
			{
				*old_m = mSmoothFactor * (*old_m - *new_m) + *new_m;
			}
		}
		else
		{
			memcpy(&mSmoothBones[index].boneMatrix, &mFinalBones[index].boneMatrix, sizeof(mdxaBone_t));
		}
		const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)header + sizeof(mdxaHeader_t));
		const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>((byte*)header + sizeof(mdxaHeader_t) + offsets->offsets[index]);
		mdxaBone_t temp_matrix;
		Multiply_3x4Matrix(&temp_matrix, &mSmoothBones[index].boneMatrix, &skel->BasePoseMat);
		const float maxl = VectorLength(&skel->BasePoseMat.matrix[0][0]);
		VectorNormalize(&temp_matrix.matrix[0][0]);
		VectorNormalize(&temp_matrix.matrix[1][0]);
		VectorNormalize(&temp_matrix.matrix[2][0]);

		VectorScale(&temp_matrix.matrix[0][0], maxl, &temp_matrix.matrix[0][0]);
		VectorScale(&temp_matrix.matrix[1][0], maxl, &temp_matrix.matrix[1][0]);
		VectorScale(&temp_matrix.matrix[2][0], maxl, &temp_matrix.matrix[2][0]);
		Multiply_3x4Matrix(&mSmoothBones[index].boneMatrix, &temp_matrix, &skel->BasePoseMatInv);
		// Added by BTO (VV) - I hope this is right.
		mSmoothBones[index].touch = mCurrentTouch;
#ifdef _DEBUG
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				assert(!Q_isnan(mSmoothBones[index].boneMatrix.matrix[i][j]));
			}
		}
#endif// _DEBUG
	}
	//rww - RAGDOLL_END

public:
	int					frameSize;
	const mdxaHeader_t* header;
	const model_t* mod;

	// these are split for better cpu cache behavior
	SBoneCalc* mBones;
	CTransformBone* mFinalBones;

	CTransformBone* mSmoothBones; // for render smoothing
	mdxaSkel_t** mSkels;

	int				mNumBones;

	boneInfo_v* rootBoneList;
	mdxaBone_t		rootMatrix;
	int				incomingTime;

	int				mCurrentTouch;

	//rww - RAGDOLL_BEGIN
	int				mCurrentTouchRender;
	int				mLastTouch;
	int				mLastLastTouch;
	//rww - RAGDOLL_END

	// for render smoothing
	bool			mSmoothingActive;
	bool			mUnsquash;
	float			mSmoothFactor;
	//	int				mWraithID; // this is just used for debug prints, can use it for any int of interest in JK2

	CBoneCache(const model_t* amod, const mdxaHeader_t* aheader) : frameSize(0),
		header(aheader),
		mod(amod), rootBoneList(nullptr), rootMatrix(),
		incomingTime(0), mCurrentTouchRender(0)
	{
		assert(amod);
		assert(aheader);
		mSmoothingActive = false;
		mUnsquash = false;
		mSmoothFactor = 0.0f;

		mNumBones = header->numBones;
		mBones = new SBoneCalc[mNumBones];
		mFinalBones = static_cast<CTransformBone*>(R_Malloc(sizeof(CTransformBone) * mNumBones, TAG_GHOUL2, qtrue));
		mSmoothBones = static_cast<CTransformBone*>(R_Malloc(sizeof(CTransformBone) * mNumBones, TAG_GHOUL2, qtrue));
		mSkels = new mdxaSkel_t * [mNumBones];
		const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)header + sizeof(mdxaHeader_t));

		for (int i = 0; i < mNumBones; i++)
		{
			const auto skel = reinterpret_cast<mdxaSkel_t*>((byte*)header + sizeof(mdxaHeader_t) + offsets->offsets[i]);
			mSkels[i] = skel;
			mFinalBones[i].parent = skel->parent;
		}
		mCurrentTouch = 3;

		//rww - RAGDOLL_BEGIN
		mLastTouch = 2;
		mLastLastTouch = 1;
		//rww - RAGDOLL_END
	}

	~CBoneCache()
	{
		delete[] mBones;
		// Alignment
		R_Free(mFinalBones);
		R_Free(mSmoothBones);
		delete[] mSkels;
	}

	SBoneCalc& Root() const
	{
		assert(mNumBones);
		return mBones[0];
	}
	const mdxaBone_t& EvalUnsmooth(const int index)
	{
		EvalLow(index);
		if (mSmoothingActive && mSmoothBones[index].touch)
		{
			return mSmoothBones[index].boneMatrix;
		}
		return mFinalBones[index].boneMatrix;
	}
	const mdxaBone_t& Eval(const int index)
	{
		//all above is not necessary, smoothing is taken care of when we want to use smoothlow (only when evalrender)
		assert(index >= 0 && index < mNumBones);
		if (mFinalBones[index].touch != mCurrentTouch)
		{
			EvalLow(index);
		}
		return mFinalBones[index].boneMatrix;
	}

	//rww - RAGDOLL_BEGIN
	const mdxaBone_t& EvalRender(const int index)
	{
		assert(index >= 0 && index < mNumBones);
		if (mFinalBones[index].touch != mCurrentTouch)
		{
			mFinalBones[index].touchRender = mCurrentTouchRender;
			EvalLow(index);
		}
		if (mSmoothingActive)
		{
			if (mSmoothBones[index].touch != mCurrentTouch)
			{
				SmoothLow(index);
			}
			return mSmoothBones[index].boneMatrix;
		}
		return mFinalBones[index].boneMatrix;
	}
	//rww - RAGDOLL_END
	//rww - RAGDOLL_BEGIN
	bool WasRendered(const int index) const
	{
		assert(index >= 0 && index < mNumBones);
		return mFinalBones[index].touchRender == mCurrentTouchRender;
	}
	int GetParent(const int index) const
	{
		if (index == 0)
		{
			return -1;
		}
		assert(index >= 0 && index < mNumBones);
		return mFinalBones[index].parent;
	}
	//rww - RAGDOLL_END

	// Added by BTO (VV) - This is probably broken
	// Need to add in smoothing step?
	CTransformBone* EvalFull(int index)
	{
#ifdef JK2_MODE
		//		Eval(index);

		// FIXME BBi Was commented
		Eval(index);
#else
		EvalRender(index);
#endif // JK2_MODE
		if (mSmoothingActive)
		{
			return mSmoothBones + index;
		}
		return mFinalBones + index;
	}
};

static float G2_GetVertBoneWeightNotSlow(const mdxmVertex_t* p_vert, const int iWeightNum)
{
	int i_temp = p_vert->BoneWeightings[iWeightNum];

	i_temp |= (p_vert->uiNmWeightsAndBoneIndexes >> (iG2_BONEWEIGHT_TOPBITS_SHIFT + (iWeightNum * 2))) & iG2_BONEWEIGHT_TOPBITS_AND;

	const float fBoneWeight = fG2_BONEWEIGHT_RECIPROCAL_MULT * i_temp;

	return fBoneWeight;
}

//rww - RAGDOLL_BEGIN
const mdxaHeader_t* G2_GetModA(const CGhoul2Info& ghoul2)
{
	if (!ghoul2.mBoneCache)
	{
		return nullptr;
	}

	const CBoneCache& boneCache = *ghoul2.mBoneCache;
	return boneCache.header;
}

int G2_GetBoneDependents(CGhoul2Info& ghoul2, const int bone_num, int* tempDependents, int maxDep)
{
	// fixme, these should be precomputed
	if (!ghoul2.mBoneCache || !maxDep)
	{
		return 0;
	}

	const CBoneCache& boneCache = *ghoul2.mBoneCache;
	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t));
	const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[bone_num]);
	int i;
	int ret = 0;
	for (i = 0; i < skel->numChildren; i++)
	{
		if (!maxDep)
		{
			return i; // number added
		}
		*tempDependents = skel->children[i];
		assert(*tempDependents > 0 && *tempDependents < boneCache.header->numBones);
		maxDep--;
		tempDependents++;
		ret++;
	}
	for (i = 0; i < skel->numChildren; i++)
	{
		const int num = G2_GetBoneDependents(ghoul2, skel->children[i], tempDependents, maxDep);
		tempDependents += num;
		ret += num;
		maxDep -= num;
		assert(maxDep >= 0);
		if (!maxDep)
		{
			break;
		}
	}
	return ret;
}

bool G2_WasBoneRendered(const CGhoul2Info& ghoul2, const int bone_num)
{
	if (!ghoul2.mBoneCache)
	{
		return false;
	}
	const CBoneCache& boneCache = *ghoul2.mBoneCache;

	return boneCache.WasRendered(bone_num);
}

void G2_GetBoneBasepose(const CGhoul2Info& ghoul2, const int bone_num, mdxaBone_t*& retBasepose, mdxaBone_t*& retBaseposeInv)
{
	if (!ghoul2.mBoneCache)
	{
		// yikes
		retBasepose = const_cast<mdxaBone_t*>(&identityMatrix);
		retBaseposeInv = const_cast<mdxaBone_t*>(&identityMatrix);
		return;
	}
	assert(ghoul2.mBoneCache);
	const CBoneCache& boneCache = *ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(bone_num >= 0 && bone_num < boneCache.header->numBones);

	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t));
	const auto skel = reinterpret_cast<mdxaSkel_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[bone_num]);
	retBasepose = &skel->BasePoseMat;
	retBaseposeInv = &skel->BasePoseMatInv;
}

char* G2_GetBoneNameFromSkel(const CGhoul2Info& ghoul2, const int bone_num)
{
	if (!ghoul2.mBoneCache)
	{
		return nullptr;
	}
	const CBoneCache& boneCache = *ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(bone_num >= 0 && bone_num < boneCache.header->numBones);

	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t));
	const auto skel = reinterpret_cast<mdxaSkel_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[bone_num]);

	return skel->name;
}

void G2_RagGetBoneBasePoseMatrixLow(const CGhoul2Info& ghoul2, const int bone_num, const mdxaBone_t& boneMatrix, mdxaBone_t& retMatrix, vec3_t scale)
{
	assert(ghoul2.mBoneCache);
	const CBoneCache& boneCache = *ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(bone_num >= 0 && bone_num < boneCache.header->numBones);

	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t));
	const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[bone_num]);
	Multiply_3x4Matrix(&retMatrix, &boneMatrix, &skel->BasePoseMat);

	if (scale[0])
	{
		retMatrix.matrix[0][3] *= scale[0];
	}
	if (scale[1])
	{
		retMatrix.matrix[1][3] *= scale[1];
	}
	if (scale[2])
	{
		retMatrix.matrix[2][3] *= scale[2];
	}

	VectorNormalize(reinterpret_cast<float*>(&retMatrix.matrix[0]));
	VectorNormalize(reinterpret_cast<float*>(&retMatrix.matrix[1]));
	VectorNormalize(reinterpret_cast<float*>(&retMatrix.matrix[2]));
}

void G2_GetBoneMatrixLow(const CGhoul2Info& ghoul2, const int bone_num, const vec3_t scale, mdxaBone_t& retMatrix, mdxaBone_t*& retBasepose, mdxaBone_t*& retBaseposeInv)
{
	if (!ghoul2.mBoneCache)
	{
		retMatrix = identityMatrix;
		// yikes
		retBasepose = const_cast<mdxaBone_t*>(&identityMatrix);
		retBaseposeInv = const_cast<mdxaBone_t*>(&identityMatrix);
		return;
	}
	mdxaBone_t bolt;
	assert(ghoul2.mBoneCache);
	CBoneCache& boneCache = *ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(bone_num >= 0 && bone_num < boneCache.header->numBones);

	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t));
	const auto skel = reinterpret_cast<mdxaSkel_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[bone_num]);
	Multiply_3x4Matrix(&bolt, &boneCache.Eval(bone_num), &skel->BasePoseMat); // DEST FIRST ARG
	retBasepose = &skel->BasePoseMat;
	retBaseposeInv = &skel->BasePoseMatInv;

	if (scale[0])
	{
		bolt.matrix[0][3] *= scale[0];
	}
	if (scale[1])
	{
		bolt.matrix[1][3] *= scale[1];
	}
	if (scale[2])
	{
		bolt.matrix[2][3] *= scale[2];
	}
	VectorNormalize(reinterpret_cast<float*>(&bolt.matrix[0]));
	VectorNormalize(reinterpret_cast<float*>(&bolt.matrix[1]));
	VectorNormalize(reinterpret_cast<float*>(&bolt.matrix[2]));

	Multiply_3x4Matrix(&retMatrix, &worldMatrix, &bolt);

#ifdef _DEBUG
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			assert(!Q_isnan(retMatrix.matrix[i][j]));
		}
	}
#endif// _DEBUG
}

int G2_GetParentBoneMatrixLow(const CGhoul2Info& ghoul2, const int bone_num, const vec3_t scale, mdxaBone_t& retMatrix, mdxaBone_t*& retBasepose, mdxaBone_t*& retBaseposeInv)
{
	int parent = -1;
	if (ghoul2.mBoneCache)
	{
		const CBoneCache& boneCache = *ghoul2.mBoneCache;
		assert(boneCache.mod);
		assert(bone_num >= 0 && bone_num < boneCache.header->numBones);
		parent = boneCache.GetParent(bone_num);
		if (parent < 0 || parent >= boneCache.header->numBones)
		{
			parent = -1;
			retMatrix = identityMatrix;
			// yikes
			retBasepose = const_cast<mdxaBone_t*>(&identityMatrix);
			retBaseposeInv = const_cast<mdxaBone_t*>(&identityMatrix);
		}
		else
		{
			G2_GetBoneMatrixLow(ghoul2, parent, scale, retMatrix, retBasepose, retBaseposeInv);
		}
	}
	return parent;
}
//rww - RAGDOLL_END

void RemoveBoneCache(const CBoneCache* boneCache)
{
	delete boneCache;
}

const mdxaBone_t& EvalBoneCache(const int index, CBoneCache* boneCache)
{
	assert(boneCache);
	return boneCache->Eval(index);
}

class CRenderSurface
{
public:
	int				surfaceNum;
	surfaceInfo_v& rootSList;
	const shader_t* cust_shader;
	int				fogNum;
	qboolean		personalModel;
	CBoneCache* boneCache;
	int				renderfx;
	const skin_t* skin;
	const model_t* currentModel;
	int				lod;
	boltInfo_v& boltList;
#ifdef _G2_GORE
	shader_t* gore_shader;
	CGoreSet* gore_set;
#endif

	CRenderSurface(
		const int				initsurfaceNum,
		surfaceInfo_v& initrootSList,
		const shader_t* initcust_shader,
		const int				initfogNum,
		const qboolean		initpersonalModel,
		CBoneCache* initboneCache,
		const int				initrenderfx,
		const skin_t* initskin,
		const model_t* initcurrentModel,
		const int				initlod,
#ifdef _G2_GORE
		boltInfo_v& initboltList,
		shader_t* initgore_shader,
		CGoreSet* initgore_set) :
#else
		boltInfo_v& initboltList):
#endif
	surfaceNum(initsurfaceNum),
		rootSList(initrootSList),
		cust_shader(initcust_shader),
		fogNum(initfogNum),
		personalModel(initpersonalModel),
		boneCache(initboneCache),
		renderfx(initrenderfx),
		skin(initskin),
		currentModel(initcurrentModel),
		lod(initlod),
#ifdef _G2_GORE
		boltList(initboltList),
		gore_shader(initgore_shader),
		gore_set(initgore_set)
#else
		boltList(initboltList)
#endif
	{}
};

#define MAX_RENDER_SURFACES (2048)
static CRenderableSurface RSStorage[MAX_RENDER_SURFACES];
static unsigned int NextRS = 0;

static CRenderableSurface* AllocRS()
{
	CRenderableSurface* ret = &RSStorage[NextRS];
	ret->Init();
	NextRS++;
	NextRS %= MAX_RENDER_SURFACES;
	return ret;
}

/*

All bones should be an identity orientation to display the mesh exactly
as it is specified.

For all other frames, the bones represent the transformation from the
orientation of the bone in the base frame to the orientation in this
frame.

*/

/*
=============
R_ACullModel
=============
*/
static int R_GCullModel(const trRefEntity_t* ent) {
	// scale the radius if need be
	float largestScale = ent->e.modelScale[0];

	if (ent->e.modelScale[1] > largestScale)
	{
		largestScale = ent->e.modelScale[1];
	}
	if (ent->e.modelScale[2] > largestScale)
	{
		largestScale = ent->e.modelScale[2];
	}
	if (!largestScale)
	{
		largestScale = 1;
	}

	// cull bounding sphere
	switch (R_CullLocalPointAndRadius(vec3_origin, ent->e.radius * largestScale))
	{
	case CULL_OUT:
		tr.pc.c_sphere_cull_md3_out++;
		return CULL_OUT;

	case CULL_IN:
		tr.pc.c_sphere_cull_md3_in++;
		return CULL_IN;

	case CULL_CLIP:
		tr.pc.c_sphere_cull_md3_clip++;
		return CULL_IN;
	default:;
	}
	return CULL_IN;
}

/*
=================
R_AComputeFogNum

=================
*/
static int R_GComputeFogNum(const trRefEntity_t* ent) {
	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return 0;
	}

	if (tr.refdef.doLAGoggles)
	{
		return tr.world->numfogs;
	}

	int partialFog = 0;
	for (int i = 1; i < tr.world->numfogs; i++) {
		const fog_t* fog = &tr.world->fogs[i];
		if (ent->e.origin[0] - ent->e.radius >= fog->bounds[0][0]
			&& ent->e.origin[0] + ent->e.radius <= fog->bounds[1][0]
			&& ent->e.origin[1] - ent->e.radius >= fog->bounds[0][1]
			&& ent->e.origin[1] + ent->e.radius <= fog->bounds[1][1]
			&& ent->e.origin[2] - ent->e.radius >= fog->bounds[0][2]
			&& ent->e.origin[2] + ent->e.radius <= fog->bounds[1][2])
		{//totally inside it
			return i;
		}
		if ((ent->e.origin[0] - ent->e.radius >= fog->bounds[0][0] && ent->e.origin[1] - ent->e.radius >= fog->bounds[0][1] && ent->e.origin[2] - ent->e.radius >= fog->bounds[0][2] &&
			ent->e.origin[0] - ent->e.radius <= fog->bounds[1][0] && ent->e.origin[1] - ent->e.radius <= fog->bounds[1][1] && ent->e.origin[2] - ent->e.radius <= fog->bounds[1][2]) ||
			(ent->e.origin[0] + ent->e.radius >= fog->bounds[0][0] && ent->e.origin[1] + ent->e.radius >= fog->bounds[0][1] && ent->e.origin[2] + ent->e.radius >= fog->bounds[0][2] &&
				ent->e.origin[0] + ent->e.radius <= fog->bounds[1][0] && ent->e.origin[1] + ent->e.radius <= fog->bounds[1][1] && ent->e.origin[2] + ent->e.radius <= fog->bounds[1][2]))
		{
			//partially inside it
			if (tr.refdef.fogIndex == i || R_FogParmsMatch(tr.refdef.fogIndex, i))
			{//take new one only if it's the same one that the viewpoint is in
				return i;
			}
			if (!partialFog)
			{//first partialFog
				partialFog = i;
			}
		}
	}
	//if nothing else, use the first partial fog you found
	return partialFog;
}

// work out lod for this entity.
static int G2_ComputeLOD(trRefEntity_t* ent, const model_t* currentModel, int lodBias)
{
	float flod;
	float projectedRadius;

	if (currentModel->numLods < 2)
	{	// model has only 1 LOD level, skip computations and bias
		return 0;
	}

	if (r_lodbias->integer > lodBias)
	{
		lodBias = r_lodbias->integer;
	}

	//**early out, it's going to be max lod
	if (lodBias >= currentModel->numLods)
	{
		return currentModel->numLods - 1;
	}

	// scale the radius if need be
	float largestScale = ent->e.modelScale[0];

	if (ent->e.modelScale[1] > largestScale)
	{
		largestScale = ent->e.modelScale[1];
	}
	if (ent->e.modelScale[2] > largestScale)
	{
		largestScale = ent->e.modelScale[2];
	}
	if (!largestScale)
	{
		largestScale = 1;
	}

	if ((projectedRadius = ProjectRadius(0.75 * largestScale * ent->e.radius, ent->e.origin)) != 0)	//we reduce the radius to make the LOD match other model types which use the actual bound box size
	{
		float lodscale = r_lodscale->value;
		if (lodscale > 20) lodscale = 20;
		flod = 1.0f - projectedRadius * lodscale;
	}
	else
	{
		// object intersects near view plane, e.g. view weapon
		flod = 0;
	}

	flod *= currentModel->numLods;
	int lod = Q_ftol(flod);

	if (lod < 0)
	{
		lod = 0;
	}
	else if (lod >= currentModel->numLods)
	{
		lod = currentModel->numLods - 1;
	}

	lod += lodBias;

	if (lod >= currentModel->numLods)
		lod = currentModel->numLods - 1;
	if (lod < 0)
		lod = 0;

	return lod;
}

void Multiply_3x4Matrix(mdxaBone_t* out, const  mdxaBone_t* in2, const mdxaBone_t* in)
{
	// first row of out
	out->matrix[0][0] = in2->matrix[0][0] * in->matrix[0][0] + in2->matrix[0][1] * in->matrix[1][0] + in2->matrix[0][2] * in->matrix[2][0];
	out->matrix[0][1] = in2->matrix[0][0] * in->matrix[0][1] + in2->matrix[0][1] * in->matrix[1][1] + in2->matrix[0][2] * in->matrix[2][1];
	out->matrix[0][2] = in2->matrix[0][0] * in->matrix[0][2] + in2->matrix[0][1] * in->matrix[1][2] + in2->matrix[0][2] * in->matrix[2][2];
	out->matrix[0][3] = in2->matrix[0][0] * in->matrix[0][3] + in2->matrix[0][1] * in->matrix[1][3] + in2->matrix[0][2] * in->matrix[2][3] + in2->matrix[0][3];
	// second row of outf out
	out->matrix[1][0] = in2->matrix[1][0] * in->matrix[0][0] + in2->matrix[1][1] * in->matrix[1][0] + in2->matrix[1][2] * in->matrix[2][0];
	out->matrix[1][1] = in2->matrix[1][0] * in->matrix[0][1] + in2->matrix[1][1] * in->matrix[1][1] + in2->matrix[1][2] * in->matrix[2][1];
	out->matrix[1][2] = in2->matrix[1][0] * in->matrix[0][2] + in2->matrix[1][1] * in->matrix[1][2] + in2->matrix[1][2] * in->matrix[2][2];
	out->matrix[1][3] = in2->matrix[1][0] * in->matrix[0][3] + in2->matrix[1][1] * in->matrix[1][3] + in2->matrix[1][2] * in->matrix[2][3] + in2->matrix[1][3];
	// third row of out  out
	out->matrix[2][0] = in2->matrix[2][0] * in->matrix[0][0] + in2->matrix[2][1] * in->matrix[1][0] + in2->matrix[2][2] * in->matrix[2][0];
	out->matrix[2][1] = in2->matrix[2][0] * in->matrix[0][1] + in2->matrix[2][1] * in->matrix[1][1] + in2->matrix[2][2] * in->matrix[2][1];
	out->matrix[2][2] = in2->matrix[2][0] * in->matrix[0][2] + in2->matrix[2][1] * in->matrix[1][2] + in2->matrix[2][2] * in->matrix[2][2];
	out->matrix[2][3] = in2->matrix[2][0] * in->matrix[0][3] + in2->matrix[2][1] * in->matrix[1][3] + in2->matrix[2][2] * in->matrix[2][3] + in2->matrix[2][3];
}

static int G2_GetBonePoolIndex(const mdxaHeader_t* pMDXAHeader, const int iFrame, const int iBone)
{
	assert(iFrame >= 0 && iFrame < pMDXAHeader->numFrames);
	assert(iBone >= 0 && iBone < pMDXAHeader->numBones);

	const int iOffsetToIndex = iFrame * pMDXAHeader->numBones * 3 + iBone * 3;
	const mdxaIndex_t* pIndex = reinterpret_cast<mdxaIndex_t*>((byte*)pMDXAHeader + pMDXAHeader->ofsFrames + iOffsetToIndex);

	return (pIndex->iIndex[2] << 16) + (pIndex->iIndex[1] << 8) + pIndex->iIndex[0];
}

static void UnCompressBone(float mat[3][4], const int iBoneIndex, const mdxaHeader_t* pMDXAHeader, const int iFrame)
{
	const mdxaCompQuatBone_t* pCompBonePool = reinterpret_cast<mdxaCompQuatBone_t*>((byte*)pMDXAHeader + pMDXAHeader->ofsCompBonePool);
	MC_UnCompressQuat(mat, pCompBonePool[G2_GetBonePoolIndex(pMDXAHeader, iFrame, iBoneIndex)].Comp);
}

#define DEBUG_G2_TIMING (0)
#define DEBUG_G2_TIMING_RENDER_ONLY (1)

void G2_TimingModel(boneInfo_t& bone, const int current_time, const int numFramesInFile, int& current_frame, int& newFrame, float& lerp)
{
	assert(bone.startFrame >= 0);
	assert(bone.startFrame <= numFramesInFile);
	assert(bone.endFrame >= 0);
	assert(bone.endFrame <= numFramesInFile);

	// yes - add in animation speed to current frame
	const float	animSpeed = bone.animSpeed;
	float	time;
	if (bone.pauseTime)
	{
		time = (bone.pauseTime - bone.startTime) / 50.0f;
	}
	else
	{
		time = (current_time - bone.startTime) / 50.0f;
	}
	if (time < 0.0f)
	{
		time = 0.0f;
	}
	float	newFrame_g = bone.startFrame + time * animSpeed;

	const int		animSize = bone.endFrame - bone.startFrame;
	const float	endFrame = static_cast<float>(bone.endFrame);
	// we are supposed to be animating right?
	if (animSize)
	{
		// did we run off the end?
		if ((animSpeed > 0.0f && newFrame_g > endFrame - 1) ||
			(animSpeed < 0.0f && newFrame_g < endFrame + 1))
		{
			// yep - decide what to do
			if (bone.flags & BONE_ANIM_OVERRIDE_LOOP)
			{
				// get our new animation frame back within the bounds of the animation set
				if (animSpeed < 0.0f)
				{
					// we don't use this case, or so I am told
					// if we do, let me know, I need to insure the mod works

					// should we be creating a virtual frame?
					if (newFrame_g < endFrame + 1 && newFrame_g >= endFrame)
					{
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = endFrame + 1 - newFrame_g;
						// frames are easy to calculate
						current_frame = endFrame;
						assert(current_frame >= 0 && current_frame < numFramesInFile);
						newFrame = bone.startFrame;
						assert(newFrame >= 0 && newFrame < numFramesInFile);
					}
					else
					{
						if (newFrame_g <= endFrame + 1)
						{
							newFrame_g = endFrame + fmod(newFrame_g - endFrame, animSize) - animSize;
						}
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = ceil(newFrame_g) - newFrame_g;
						// frames are easy to calculate
						current_frame = ceil(newFrame_g);
						assert(current_frame >= 0 && current_frame < numFramesInFile);
						// should we be creating a virtual frame?
						if (current_frame <= endFrame + 1)
						{
							newFrame = bone.startFrame;
							assert(newFrame >= 0 && newFrame < numFramesInFile);
						}
						else
						{
							newFrame = current_frame - 1;
							assert(newFrame >= 0 && newFrame < numFramesInFile);
						}
					}
				}
				else
				{
					// should we be creating a virtual frame?
					if (newFrame_g > endFrame - 1 && newFrame_g < endFrame)
					{
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = newFrame_g - static_cast<int>(newFrame_g);
						// frames are easy to calculate
						current_frame = static_cast<int>(newFrame_g);
						assert(current_frame >= 0 && current_frame < numFramesInFile);
						newFrame = bone.startFrame;
						assert(newFrame >= 0 && newFrame < numFramesInFile);
					}
					else
					{
						if (newFrame_g >= endFrame)
						{
							newFrame_g = endFrame + fmod(newFrame_g - endFrame, animSize) - animSize;
						}
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = newFrame_g - static_cast<int>(newFrame_g);
						// frames are easy to calculate
						current_frame = static_cast<int>(newFrame_g);
						assert(current_frame >= 0 && current_frame < numFramesInFile);
						// should we be creating a virtual frame?
						if (newFrame_g >= endFrame - 1)
						{
							newFrame = bone.startFrame;
							assert(newFrame >= 0 && newFrame < numFramesInFile);
						}
						else
						{
							newFrame = current_frame + 1;
							assert(newFrame >= 0 && newFrame < numFramesInFile);
						}
					}
				}
				// sanity check
				assert((newFrame < endFrame && newFrame >= bone.startFrame) || animSize < 10);
			}
			else
			{
				if ((bone.flags & BONE_ANIM_OVERRIDE_FREEZE) == BONE_ANIM_OVERRIDE_FREEZE)
				{
					// if we are supposed to reset the default anim, then do so
					if (animSpeed > 0.0f)
					{
						current_frame = bone.endFrame - 1;
						assert(current_frame >= 0 && current_frame < numFramesInFile);
					}
					else
					{
						current_frame = bone.endFrame + 1;
						assert(current_frame >= 0 && current_frame < numFramesInFile);
					}

					newFrame = current_frame;
					assert(newFrame >= 0 && newFrame < numFramesInFile);
					lerp = 0;
				}
				else
				{
					bone.flags &= ~(BONE_ANIM_TOTAL);
				}
			}
		}
		else
		{
			if (animSpeed > 0.0)
			{
				// frames are easy to calculate
				current_frame = static_cast<int>(newFrame_g);

				// figure out the difference between the two frames	- we have to decide what frame and what percentage of that
				// frame we want to display
				lerp = newFrame_g - current_frame;

				assert(current_frame >= 0 && current_frame < numFramesInFile);

				newFrame = current_frame + 1;
				// are we now on the end frame?
				assert(static_cast<int>(endFrame) <= numFramesInFile);
				if (newFrame >= static_cast<int>(endFrame))
				{
					// we only want to lerp with the first frame of the anim if we are looping
					if (bone.flags & BONE_ANIM_OVERRIDE_LOOP)
					{
						newFrame = bone.startFrame;
						assert(newFrame >= 0 && newFrame < numFramesInFile);
					}
					// if we intend to end this anim or freeze after this, then just keep on the last frame
					else
					{
						newFrame = bone.endFrame - 1;
						assert(newFrame >= 0 && newFrame < numFramesInFile);
					}
				}
				assert(newFrame >= 0 && newFrame < numFramesInFile);
			}
			else
			{
				lerp = ceil(newFrame_g) - newFrame_g;
				// frames are easy to calculate
				current_frame = ceil(newFrame_g);
				if (current_frame > bone.startFrame)
				{
					current_frame = bone.startFrame;
					newFrame = current_frame;
					lerp = 0.0f;
				}
				else
				{
					newFrame = current_frame - 1;
					// are we now on the end frame?
					if (newFrame < endFrame + 1)
					{
						// we only want to lerp with the first frame of the anim if we are looping
						if (bone.flags & BONE_ANIM_OVERRIDE_LOOP)
						{
							newFrame = bone.startFrame;
							assert(newFrame >= 0 && newFrame < numFramesInFile);
						}
						// if we intend to end this anim or freeze after this, then just keep on the last frame
						else
						{
							newFrame = bone.endFrame + 1;
							assert(newFrame >= 0 && newFrame < numFramesInFile);
						}
					}
				}
				assert(current_frame >= 0 && current_frame < numFramesInFile);
				assert(newFrame >= 0 && newFrame < numFramesInFile);
			}
		}
	}
	else
	{
		if (animSpeed < 0.0)
		{
			current_frame = bone.endFrame + 1;
		}
		else
		{
			current_frame = bone.endFrame - 1;
		}
		if (current_frame < 0)
		{
			current_frame = 0;
		}
		assert(current_frame >= 0 && current_frame < numFramesInFile);
		newFrame = current_frame;
		assert(newFrame >= 0 && newFrame < numFramesInFile);
		lerp = 0;
	}
}

//basically construct a seperate skeleton with full hierarchy to store a matrix
//off which will give us the desired settling position given the frame in the skeleton
//that should be used -rww
int G2_Add_Bone(const model_t* mod, boneInfo_v& blist, const char* boneName);
int G2_Find_Bone(const CGhoul2Info* ghlInfo, const boneInfo_v& blist, const char* boneName);

void G2_RagGetAnimMatrix(CGhoul2Info& ghoul2, const int bone_num, mdxaBone_t& matrix, const int frame)
{
	mdxaBone_t animMatrix{};
	mdxaSkel_t* skel;
	mdxaSkelOffsets_t* offsets;
	int parent;
	int bListIndex;
#ifdef _RAG_PRINT_TEST
	bool actuallySet = false;
#endif

	assert(ghoul2.mBoneCache);
	assert(ghoul2.animModel);

	offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)ghoul2.mBoneCache->header + sizeof(mdxaHeader_t));
	skel = reinterpret_cast<mdxaSkel_t*>((byte*)ghoul2.mBoneCache->header + sizeof(mdxaHeader_t) + offsets->offsets[bone_num]);

	//find/add the bone in the list
	if (!skel->name[0])
	{
		bListIndex = -1;
	}
	else
	{
		bListIndex = G2_Find_Bone(&ghoul2, ghoul2.mBlist, skel->name);
		if (bListIndex == -1)
		{
#ifdef _RAG_PRINT_TEST
			Com_Printf("Attempting to add %s\n", skel->name);
#endif
			bListIndex = G2_Add_Bone(ghoul2.animModel, ghoul2.mBlist, skel->name);
		}
	}

	assert(bListIndex != -1);

	boneInfo_t& bone = ghoul2.mBlist[bListIndex];

	if (bone.hasAnimFrameMatrix == frame)
	{ //already calculated so just grab it
		matrix = bone.animFrameMatrix;
		return;
	}

	//get the base matrix for the specified frame
	UnCompressBone(animMatrix.matrix, bone_num, ghoul2.mBoneCache->header, frame);

	parent = skel->parent;
	if (bone_num > 0 && parent > -1)
	{
		int parentBlistIndex;
		//recursively call to assure all parent matrices are set up
		G2_RagGetAnimMatrix(ghoul2, parent, matrix, frame);

		//assign the new skel ptr for our parent
		mdxaSkel_t* pskel = reinterpret_cast<mdxaSkel_t*>((byte*)ghoul2.mBoneCache->header + sizeof(mdxaHeader_t) + offsets->offsets[
			parent]);

		//taking bone matrix for the skeleton frame and parent's animFrameMatrix into account, determine our final animFrameMatrix
		if (!pskel->name[0])
		{
			parentBlistIndex = -1;
		}
		else
		{
			parentBlistIndex = G2_Find_Bone(&ghoul2, ghoul2.mBlist, pskel->name);
			if (parentBlistIndex == -1)
			{
				parentBlistIndex = G2_Add_Bone(ghoul2.animModel, ghoul2.mBlist, pskel->name);
			}
		}

		assert(parentBlistIndex != -1);

		const boneInfo_t& pbone = ghoul2.mBlist[parentBlistIndex];

		assert(pbone.hasAnimFrameMatrix == frame); //this should have been calc'd in the recursive call

		Multiply_3x4Matrix(&bone.animFrameMatrix, &pbone.animFrameMatrix, &animMatrix);

#ifdef _RAG_PRINT_TEST
		if (parentBlistIndex != -1 && bListIndex != -1)
		{
			actuallySet = true;
		}
		else
		{
			Com_Printf("BAD LIST INDEX: %s, %s [%i]\n", skel->name, pskel->name, parent);
		}
#endif
	}
	else
	{ //root
		Multiply_3x4Matrix(&bone.animFrameMatrix, &ghoul2.mBoneCache->rootMatrix, &animMatrix);
#ifdef _RAG_PRINT_TEST
		if (bListIndex != -1)
		{
			actuallySet = true;
		}
		else
		{
			Com_Printf("BAD LIST INDEX: %s\n", skel->name);
		}
#endif
		//bone.animFrameMatrix = ghoul2.mBoneCache->mFinalBones[bone_num].boneMatrix;
		//Maybe use this for the root, so that the orientation is in sync with the current
		//root matrix? However this would require constant recalculation of this base
		//skeleton which I currently do not want.
	}

	//never need to figure it out again
	bone.hasAnimFrameMatrix = frame;

#ifdef _RAG_PRINT_TEST
	if (!actuallySet)
	{
		Com_Printf("SET FAILURE\n");
	}
#endif

	matrix = bone.animFrameMatrix;
}

// transform each individual bone's information - making sure to use any override information provided, both for angles and for animations, as
// well as multiplying each bone's matrix by it's parents matrix
void G2_TransformBone(const int index, const CBoneCache& cb)
{
	SBoneCalc& tb = cb.mBones[index];
	mdxaBone_t		tbone[6]{};
	// 	mdxaFrame_t		*aFrame=0;
	//	mdxaFrame_t		*bFrame=0;
	//	mdxaFrame_t		*aoldFrame=0;
	//	mdxaFrame_t		*boldFrame=0;
	mdxaSkel_t* skel;
	mdxaSkelOffsets_t* offsets;
	boneInfo_v& bone_list = *cb.rootBoneList;
	int				j, bone_list_index;
	int				angle_override = 0;

#if DEBUG_G2_TIMING
	bool printTiming = false;
#endif
	// should this bone be overridden by a bone in the bone list?
	bone_list_index = G2_Find_Bone_In_List(bone_list, index);
	if (bone_list_index != -1)
	{
		// we found a bone in the list - we need to override something here.

		// do we override the rotational angles?
		if (bone_list[bone_list_index].flags & (BONE_ANGLES_TOTAL))
		{
			angle_override = bone_list[bone_list_index].flags & (BONE_ANGLES_TOTAL);
		}

		// set blending stuff if we need to
		if (bone_list[bone_list_index].flags & BONE_ANIM_BLEND)
		{
			float blend_time = cb.incomingTime - bone_list[bone_list_index].blendStart;
			// only set up the blend anim if we actually have some blend time left on this bone anim - otherwise we might corrupt some blend higher up the hiearchy
			if (blend_time >= 0.0f && blend_time < bone_list[bone_list_index].blend_time)
			{
				tb.blendFrame = bone_list[bone_list_index].blendFrame;
				tb.blendOldFrame = bone_list[bone_list_index].blendLerpFrame;
				tb.blendLerp = blend_time / bone_list[bone_list_index].blend_time;
				tb.blendMode = true;
			}
			else
			{
				tb.blendMode = false;
			}
		}
		else if (r_Ghoul2NoBlend->integer || bone_list[bone_list_index].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
			// turn off blending if we are just doing a straing animation override
		{
			tb.blendMode = false;
		}

		// should this animation be overridden by an animation in the bone list?
		if (bone_list[bone_list_index].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
		{
			G2_TimingModel(bone_list[bone_list_index], cb.incomingTime, cb.header->numFrames, tb.current_frame, tb.newFrame, tb.backlerp);
		}
#if DEBUG_G2_TIMING
		printTiming = true;
#endif
		if (r_Ghoul2NoLerp->integer || bone_list[bone_list_index].flags & BONE_ANIM_NO_LERP)
		{
			tb.backlerp = 0.0f;
		}
	}
	// figure out where the location of the bone animation data is
	assert(tb.newFrame >= 0 && tb.newFrame < cb.header->numFrames);
	if (!(tb.newFrame >= 0 && tb.newFrame < cb.header->numFrames))
	{
		tb.newFrame = 0;
	}
	//	aFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + TB.newFrame * BC.frameSize );
	assert(tb.current_frame >= 0 && tb.current_frame < cb.header->numFrames);
	if (!(tb.current_frame >= 0 && tb.current_frame < cb.header->numFrames))
	{
		tb.current_frame = 0;
	}
	//	aoldFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + TB.current_frame * BC.frameSize );

		// figure out where the location of the blended animation data is
	assert(!(tb.blendFrame < 0.0 || tb.blendFrame >= cb.header->numFrames + 1));
	if (tb.blendFrame < 0.0 || tb.blendFrame >= cb.header->numFrames + 1)
	{
		tb.blendFrame = 0.0;
	}
	//	bFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + (int)TB.blendFrame * BC.frameSize );
	assert(tb.blendOldFrame >= 0 && tb.blendOldFrame < cb.header->numFrames);
	if (!(tb.blendOldFrame >= 0 && tb.blendOldFrame < cb.header->numFrames))
	{
		tb.blendOldFrame = 0;
	}
#if DEBUG_G2_TIMING

#if DEBUG_G2_TIMING_RENDER_ONLY
	if (!HackadelicOnClient)
	{
		printTiming = false;
	}
#endif
	if (printTiming)
	{
		char mess[1000];
		if (TB.blendMode)
		{
			sprintf(mess, "b %2d %5d   %4d %4d %4d %4d  %f %f\n", boneListIndex, BC.incomingTime, (int)TB.newFrame, (int)TB.current_frame, (int)TB.blendFrame, (int)TB.blendOldFrame, TB.backlerp, TB.blendLerp);
		}
		else
		{
			sprintf(mess, "a %2d %5d   %4d %4d            %f\n", boneListIndex, BC.incomingTime, TB.newFrame, TB.current_frame, TB.backlerp);
		}
		OutputDebugString(mess);
		const boneInfo_t& bone = boneList[boneListIndex];
		if (bone.flags & BONE_ANIM_BLEND)
		{
			sprintf(mess, "                                                                    bfb[%2d] %5d  %5d  (%5d-%5d) %4.2f %4x   bt(%5d-%5d) %7.2f %5d\n",
				boneListIndex,
				BC.incomingTime,
				bone.startTime,
				bone.startFrame,
				bone.endFrame,
				bone.animSpeed,
				bone.flags,
				bone.blendStart,
				bone.blendStart + bone.blend_time,
				bone.blendFrame,
				bone.blendLerpFrame
			);
		}
		else
		{
			sprintf(mess, "                                                                    bfa[%2d] %5d  %5d  (%5d-%5d) %4.2f %4x\n",
				boneListIndex,
				BC.incomingTime,
				bone.startTime,
				bone.startFrame,
				bone.endFrame,
				bone.animSpeed,
				bone.flags
			);
		}
		//		OutputDebugString(mess);
	}
#endif
	//	boldFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + TB.blendOldFrame * BC.frameSize );

	//	mdxaCompBone_t	*compBonePointer = (mdxaCompBone_t *)((byte *)BC.header + BC.header->ofsCompBonePool);

	assert(index >= 0 && index < cb.header->numBones);
	//	assert(bFrame->boneIndexes[child]>=0);
	//	assert(boldFrame->boneIndexes[child]>=0);
	//	assert(aFrame->boneIndexes[child]>=0);
	//	assert(aoldFrame->boneIndexes[child]>=0);

		// decide where the transformed bone is going

		// are we blending with another frame of anim?
	if (tb.blendMode)
	{
		float backlerp = tb.blendFrame - static_cast<int>(tb.blendFrame);
		const float frontlerp = 1.0 - backlerp;

		// 		MC_UnCompress(tbone[3].matrix,compBonePointer[bFrame->boneIndexes[child]].Comp);
		// 		MC_UnCompress(tbone[4].matrix,compBonePointer[boldFrame->boneIndexes[child]].Comp);
		UnCompressBone(tbone[3].matrix, index, cb.header, tb.blendFrame);
		UnCompressBone(tbone[4].matrix, index, cb.header, tb.blendOldFrame);

		for (j = 0; j < 12; j++)
		{
			reinterpret_cast<float*>(&tbone[5])[j] = backlerp * reinterpret_cast<float*>(&tbone[3])[j]
				+ frontlerp * reinterpret_cast<float*>(&tbone[4])[j];
		}
	}

	//
	// lerp this bone - use the temp space on the ref entity to put the bone transforms into
	//
	if (!tb.backlerp)
	{
		// 		MC_UnCompress(tbone[2].matrix,compBonePointer[aoldFrame->boneIndexes[child]].Comp);
		UnCompressBone(tbone[2].matrix, index, cb.header, tb.current_frame);

		// blend in the other frame if we need to
		if (tb.blendMode)
		{
			const float blendFrontlerp = 1.0 - tb.blendLerp;
			for (j = 0; j < 12; j++)
			{
				reinterpret_cast<float*>(&tbone[2])[j] = tb.blendLerp * reinterpret_cast<float*>(&tbone[2])[j]
					+ blendFrontlerp * reinterpret_cast<float*>(&tbone[5])[j];
			}
		}

		if (!index)
		{
			// now multiply by the root matrix, so we can offset this model should we need to
			Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.rootMatrix, &tbone[2]);
		}
	}
	else
	{
		const float frontlerp = 1.0 - tb.backlerp;
		// 		MC_UnCompress(tbone[0].matrix,compBonePointer[aFrame->boneIndexes[child]].Comp);
		//		MC_UnCompress(tbone[1].matrix,compBonePointer[aoldFrame->boneIndexes[child]].Comp);
		UnCompressBone(tbone[0].matrix, index, cb.header, tb.newFrame);
		UnCompressBone(tbone[1].matrix, index, cb.header, tb.current_frame);

		for (j = 0; j < 12; j++)
		{
			reinterpret_cast<float*>(&tbone[2])[j] = tb.backlerp * reinterpret_cast<float*>(&tbone[0])[j]
				+ frontlerp * reinterpret_cast<float*>(&tbone[1])[j];
		}

		// blend in the other frame if we need to
		if (tb.blendMode)
		{
			const float blendFrontlerp = 1.0 - tb.blendLerp;
			for (j = 0; j < 12; j++)
			{
				reinterpret_cast<float*>(&tbone[2])[j] = tb.blendLerp * reinterpret_cast<float*>(&tbone[2])[j]
					+ blendFrontlerp * reinterpret_cast<float*>(&tbone[5])[j];
			}
		}

		if (!index)
		{
			// now multiply by the root matrix, so we can offset this model should we need to
			Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.rootMatrix, &tbone[2]);
		}
	}
	// figure out where the bone hirearchy info is
	offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)cb.header + sizeof(mdxaHeader_t));
	skel = reinterpret_cast<mdxaSkel_t*>((byte*)cb.header + sizeof(mdxaHeader_t) + offsets->offsets[index]);

	const int parent = cb.mFinalBones[index].parent;
	assert((parent == -1 && index == 0) || (parent >= 0 && parent < cb.mNumBones));
	if (angle_override & BONE_ANGLES_REPLACE)
	{
		bool isRag = !!(angle_override & BONE_ANGLES_RAGDOLL);
		if (!isRag)
		{ //do the same for ik.. I suppose.
			isRag = !!(angle_override & BONE_ANGLES_IK);
		}

		mdxaBone_t& bone = cb.mFinalBones[index].boneMatrix;
		const boneInfo_t& boneOverride = bone_list[bone_list_index];

		if (isRag)
		{
			mdxaBone_t temp, firstPass;
			// give us the matrix the animation thinks we should have, so we can get the correct X&Y coors
			Multiply_3x4Matrix(&firstPass, &cb.mFinalBones[parent].boneMatrix, &tbone[2]);
			// this is crazy, we are gonna drive the animation to ID while we are doing post mults to compensate.
			Multiply_3x4Matrix(&temp, &firstPass, &skel->BasePoseMat);
			const float	matrixScale = VectorLength(reinterpret_cast<float*>(&temp));
			static mdxaBone_t		toMatrix =
			{
				{
					{ 1.0f, 0.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f, 0.0f }
				}
			};
			toMatrix.matrix[0][0] = matrixScale;
			toMatrix.matrix[1][1] = matrixScale;
			toMatrix.matrix[2][2] = matrixScale;
			toMatrix.matrix[0][3] = temp.matrix[0][3];
			toMatrix.matrix[1][3] = temp.matrix[1][3];
			toMatrix.matrix[2][3] = temp.matrix[2][3];

			Multiply_3x4Matrix(&temp, &toMatrix, &skel->BasePoseMatInv); //dest first arg

			float blend_time = cb.incomingTime - bone_list[bone_list_index].boneBlendStart;
			float blendLerp = blend_time / bone_list[bone_list_index].boneBlendTime;
			if (blendLerp > 0.0f)
			{
				// has started
				if (blendLerp > 1.0f)
				{
					// done
//					Multiply_3x4Matrix(&bone, &BC.mFinalBones[parent].boneMatrix,&temp);
					memcpy(&bone, &temp, sizeof(mdxaBone_t));
				}
				else
				{
					//					mdxaBone_t lerp;
										// now do the blend into the destination
					const float blendFrontlerp = 1.0 - blendLerp;
					for (j = 0; j < 12; j++)
					{
						reinterpret_cast<float*>(&bone)[j] = blendLerp * reinterpret_cast<float*>(&temp)[j]
							+ blendFrontlerp * reinterpret_cast<float*>(&tbone[2])[j];
					}
					//					Multiply_3x4Matrix(&bone, &BC.mFinalBones[parent].boneMatrix,&lerp);
				}
			}
		}
		else
		{
			mdxaBone_t temp, firstPass;

			// give us the matrix the animation thinks we should have, so we can get the correct X&Y coors
			Multiply_3x4Matrix(&firstPass, &cb.mFinalBones[parent].boneMatrix, &tbone[2]);

			// are we attempting to blend with the base animation? and still within blend time?
			if (boneOverride.boneBlendTime && boneOverride.boneBlendTime + boneOverride.boneBlendStart < cb.incomingTime)
			{
				// ok, we are supposed to be blending. Work out lerp
				float blend_time = cb.incomingTime - bone_list[bone_list_index].boneBlendStart;
				float blendLerp = blend_time / bone_list[bone_list_index].boneBlendTime;

				if (blendLerp <= 1)
				{
					if (blendLerp < 0)
					{
						assert(0);
					}

					// now work out the matrix we want to get *to* - firstPass is where we are coming *from*
					Multiply_3x4Matrix(&temp, &firstPass, &skel->BasePoseMat);

					const float	matrixScale = VectorLength(reinterpret_cast<float*>(&temp));

					mdxaBone_t	newMatrixTemp{};

					if (HackadelicOnClient)
					{
						for (int i = 0; i < 3; i++)
						{
							for (int x = 0; x < 3; x++)
							{
								newMatrixTemp.matrix[i][x] = boneOverride.newMatrix.matrix[i][x] * matrixScale;
							}
						}

						newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
						newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
						newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
					}
					else
					{
						for (int i = 0; i < 3; i++)
						{
							for (int x = 0; x < 3; x++)
							{
								newMatrixTemp.matrix[i][x] = boneOverride.matrix.matrix[i][x] * matrixScale;
							}
						}

						newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
						newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
						newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
					}

					Multiply_3x4Matrix(&temp, &newMatrixTemp, &skel->BasePoseMatInv);

					// now do the blend into the destination
					const float blendFrontlerp = 1.0 - blendLerp;
					for (j = 0; j < 12; j++)
					{
						reinterpret_cast<float*>(&bone)[j] = blendLerp * reinterpret_cast<float*>(&temp)[j]
							+ blendFrontlerp * reinterpret_cast<float*>(&firstPass)[j];
					}
				}
				else
				{
					bone = firstPass;
				}
			}
			// no, so just override it directly
			else
			{
				Multiply_3x4Matrix(&temp, &firstPass, &skel->BasePoseMat);
				const float	matrixScale = VectorLength(reinterpret_cast<float*>(&temp));

				mdxaBone_t	newMatrixTemp{};

				if (HackadelicOnClient)
				{
					for (int i = 0; i < 3; i++)
					{
						for (int x = 0; x < 3; x++)
						{
							newMatrixTemp.matrix[i][x] = boneOverride.newMatrix.matrix[i][x] * matrixScale;
						}
					}

					newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
					newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
					newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
				}
				else
				{
					for (int i = 0; i < 3; i++)
					{
						for (int x = 0; x < 3; x++)
						{
							newMatrixTemp.matrix[i][x] = boneOverride.matrix.matrix[i][x] * matrixScale;
						}
					}

					newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
					newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
					newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
				}

				Multiply_3x4Matrix(&bone, &newMatrixTemp, &skel->BasePoseMatInv);
			}
		}
	}
	else if (angle_override & BONE_ANGLES_PREMULT)
	{
		if (angle_override & BONE_ANGLES_RAGDOLL || angle_override & BONE_ANGLES_IK)
		{
			mdxaBone_t	tmp;
			if (!index)
			{
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&tmp, &cb.rootMatrix, &bone_list[bone_list_index].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&tmp, &cb.rootMatrix, &bone_list[bone_list_index].matrix);
				}
			}
			else
			{
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&tmp, &cb.mFinalBones[parent].boneMatrix, &bone_list[bone_list_index].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&tmp, &cb.mFinalBones[parent].boneMatrix, &bone_list[bone_list_index].matrix);
				}
			}
			Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &tmp, &tbone[2]);
		}
		else
		{
			if (!index)
			{
				// use the in coming root matrix as our basis
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.rootMatrix, &bone_list[bone_list_index].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.rootMatrix, &bone_list[bone_list_index].matrix);
				}
			}
			else
			{
				// convert from 3x4 matrix to a 4x4 matrix
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.mFinalBones[parent].boneMatrix, &bone_list[bone_list_index].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.mFinalBones[parent].boneMatrix, &bone_list[bone_list_index].matrix);
				}
			}
		}
	}
	else
		// now transform the matrix by it's parent, asumming we have a parent, and we aren't overriding the angles absolutely
		if (index)
		{
			Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &cb.mFinalBones[parent].boneMatrix, &tbone[2]);
		}

	// now multiply our resulting bone by an override matrix should we need to
	if (angle_override & BONE_ANGLES_POSTMULT)
	{
		mdxaBone_t	tempMatrix;
		memcpy(&tempMatrix, &cb.mFinalBones[index].boneMatrix, sizeof(mdxaBone_t));
		if (HackadelicOnClient)
		{
			Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &tempMatrix, &bone_list[bone_list_index].newMatrix);
		}
		else
		{
			Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &tempMatrix, &bone_list[bone_list_index].matrix);
		}
	}
	if (r_Ghoul2UnSqash->integer)
	{
		mdxaBone_t tempMatrix;
		Multiply_3x4Matrix(&tempMatrix, &cb.mFinalBones[index].boneMatrix, &skel->BasePoseMat);
		const float maxl = VectorLength(&skel->BasePoseMat.matrix[0][0]);
		VectorNormalize(&tempMatrix.matrix[0][0]);
		VectorNormalize(&tempMatrix.matrix[1][0]);
		VectorNormalize(&tempMatrix.matrix[2][0]);

		VectorScale(&tempMatrix.matrix[0][0], maxl, &tempMatrix.matrix[0][0]);
		VectorScale(&tempMatrix.matrix[1][0], maxl, &tempMatrix.matrix[1][0]);
		VectorScale(&tempMatrix.matrix[2][0], maxl, &tempMatrix.matrix[2][0]);
		Multiply_3x4Matrix(&cb.mFinalBones[index].boneMatrix, &tempMatrix, &skel->BasePoseMatInv);
	}
}

#define		GHOUL2_RAG_STARTED						0x0010

// start the recursive hirearchial bone transform and lerp process for this model
void G2_TransformGhoulBones(boneInfo_v& rootBoneList, const mdxaBone_t& rootMatrix, CGhoul2Info& ghoul2, const int time, const bool smooth = true)
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceCounter_G2_TransformGhoulBones++;
#endif
	assert(ghoul2.aHeader);
	assert(ghoul2.currentModel);
	assert(ghoul2.currentModel->mdxm);
	if (!ghoul2.aHeader->numBones)
	{
		assert(0); // this would be strange
		return;
	}
	if (!ghoul2.mBoneCache)
	{
		ghoul2.mBoneCache = new CBoneCache(ghoul2.currentModel, ghoul2.aHeader);
	}
	ghoul2.mBoneCache->mod = ghoul2.currentModel;
	ghoul2.mBoneCache->header = ghoul2.aHeader;
	assert(ghoul2.mBoneCache->mNumBones == ghoul2.aHeader->numBones);

	ghoul2.mBoneCache->mSmoothingActive = false;
	ghoul2.mBoneCache->mUnsquash = false;

	// master smoothing control
	float val = r_Ghoul2AnimSmooth->value;
	if (smooth && val > 0.0f && val < 1.0f)
	{
		ghoul2.mBoneCache->mLastTouch = ghoul2.mBoneCache->mLastLastTouch;

		if (ghoul2.mFlags & GHOUL2_RAG_STARTED)
		{
			for (const boneInfo_t& bone : rootBoneList)
			{
				if (bone.flags & BONE_ANGLES_RAGDOLL)
				{
					if (bone.firstCollisionTime &&
						bone.firstCollisionTime > time - 250 &&
						bone.firstCollisionTime < time)
					{
						val = 0.9f;//(val+0.8f)/2.0f;
					}
					else if (bone.airTime > time)
					{
						val = 0.2f;
					}
					else
					{
						val = 0.8f;
					}
					break;
				}
			}
		}

		ghoul2.mBoneCache->mSmoothFactor = val;
		ghoul2.mBoneCache->mSmoothingActive = true;
		if (r_Ghoul2UnSqashAfterSmooth->integer)
		{
			ghoul2.mBoneCache->mUnsquash = true;
		}
	}
	else
	{
		ghoul2.mBoneCache->mSmoothFactor = 1.0f;
	}
	ghoul2.mBoneCache->mCurrentTouch++;

	//rww - RAGDOLL_BEGIN
	if (HackadelicOnClient)
	{
		ghoul2.mBoneCache->mLastLastTouch = ghoul2.mBoneCache->mCurrentTouch;
		ghoul2.mBoneCache->mCurrentTouchRender = ghoul2.mBoneCache->mCurrentTouch;
	}
	else
	{
		ghoul2.mBoneCache->mCurrentTouchRender = 0;
	}
	//rww - RAGDOLL_END

	//	ghoul2.mBoneCache->mWraithID=0;
	ghoul2.mBoneCache->frameSize = 0;// can be deleted in new G2 format	//(int)( &((mdxaFrame_t *)0)->boneIndexes[ ghoul2.aHeader->numBones ] );

	ghoul2.mBoneCache->rootBoneList = &rootBoneList;
	ghoul2.mBoneCache->rootMatrix = rootMatrix;
	ghoul2.mBoneCache->incomingTime = time;

	SBoneCalc& TB = ghoul2.mBoneCache->Root();
	TB.newFrame = 0;
	TB.current_frame = 0;
	TB.backlerp = 0.0f;
	TB.blendFrame = 0;
	TB.blendOldFrame = 0;
	TB.blendMode = false;
	TB.blendLerp = 0;
}

#define MDX_TAG_ORIGIN 2

//======================================================================
//
// Surface Manipulation code

// We've come across a surface that's designated as a bolt surface, process it and put it in the appropriate bolt place
void G2_ProcessSurfaceBolt2(CBoneCache& boneCache, const mdxmSurface_t* surface, int boltNum, boltInfo_v& boltList, const surfaceInfo_t* surfInfo, const model_t* mod, mdxaBone_t& retMatrix)
{
	float			pTri[3][3]{};
	int k;

	// now there are two types of tag surface - model ones and procedural generated types - lets decide which one we have here.
	if (surfInfo && surfInfo->offFlags == G2SURFACEFLAG_GENERATED)
	{
		const int surf_number = surfInfo->genPolySurfaceIndex & 0x0ffff;
		const int	polyNumber = surfInfo->genPolySurfaceIndex >> 16 & 0x0ffff;

		// find original surface our original poly was in.
		mdxmSurface_t* originalSurf = static_cast<mdxmSurface_t*>(G2_FindSurface(mod, surf_number, surfInfo->genLod));
		const mdxmTriangle_t* originalTriangleIndexes = reinterpret_cast<mdxmTriangle_t*>(reinterpret_cast<byte*>(originalSurf) + originalSurf->ofsTriangles);

		// get the original polys indexes
		const int index0 = originalTriangleIndexes[polyNumber].indexes[0];
		const int index1 = originalTriangleIndexes[polyNumber].indexes[1];
		const int index2 = originalTriangleIndexes[polyNumber].indexes[2];

		// decide where the original verts are
		mdxmVertex_t* vert0 = reinterpret_cast<mdxmVertex_t*>(reinterpret_cast<byte*>(originalSurf) + originalSurf->ofsVerts);
		vert0 += index0;

		mdxmVertex_t* vert1 = reinterpret_cast<mdxmVertex_t*>(reinterpret_cast<byte*>(originalSurf) + originalSurf->ofsVerts);
		vert1 += index1;

		mdxmVertex_t* vert2 = reinterpret_cast<mdxmVertex_t*>(reinterpret_cast<byte*>(originalSurf) + originalSurf->ofsVerts);
		vert2 += index2;

		// clear out the triangle verts to be
		VectorClear(pTri[0]);
		VectorClear(pTri[1]);
		VectorClear(pTri[2]);
		const int* piBoneReferences = reinterpret_cast<int*>(reinterpret_cast<byte*>(originalSurf) + originalSurf->ofsBoneReferences);

		//		mdxmWeight_t	*w;

				// now go and transform just the points we need from the surface that was hit originally
		//		w = vert0->weights;
		float fTotalWeight = 0.0f;
		int iNumWeights = G2_GetVertWeights(vert0);
		for (k = 0; k < iNumWeights; k++)
		{
			const int		iBoneIndex = G2_GetVertBoneIndex(vert0, k);
			const float	fBoneWeight = G2_GetVertBoneWeight(vert0, k, fTotalWeight, iNumWeights);

			const mdxaBone_t& bone = boneCache.Eval(piBoneReferences[iBoneIndex]);

			pTri[0][0] += fBoneWeight * (DotProduct(bone.matrix[0], vert0->vertCoords) + bone.matrix[0][3]);
			pTri[0][1] += fBoneWeight * (DotProduct(bone.matrix[1], vert0->vertCoords) + bone.matrix[1][3]);
			pTri[0][2] += fBoneWeight * (DotProduct(bone.matrix[2], vert0->vertCoords) + bone.matrix[2][3]);
		}

		//		w = vert1->weights;
		fTotalWeight = 0.0f;
		iNumWeights = G2_GetVertWeights(vert1);
		for (k = 0; k < iNumWeights; k++)
		{
			const int		iBoneIndex = G2_GetVertBoneIndex(vert1, k);
			const float	fBoneWeight = G2_GetVertBoneWeight(vert1, k, fTotalWeight, iNumWeights);

			const mdxaBone_t& bone = boneCache.Eval(piBoneReferences[iBoneIndex]);

			pTri[1][0] += fBoneWeight * (DotProduct(bone.matrix[0], vert1->vertCoords) + bone.matrix[0][3]);
			pTri[1][1] += fBoneWeight * (DotProduct(bone.matrix[1], vert1->vertCoords) + bone.matrix[1][3]);
			pTri[1][2] += fBoneWeight * (DotProduct(bone.matrix[2], vert1->vertCoords) + bone.matrix[2][3]);
		}

		//		w = vert2->weights;
		fTotalWeight = 0.0f;
		iNumWeights = G2_GetVertWeights(vert2);
		for (k = 0; k < iNumWeights; k++)
		{
			const int		iBoneIndex = G2_GetVertBoneIndex(vert2, k);
			const float	fBoneWeight = G2_GetVertBoneWeight(vert2, k, fTotalWeight, iNumWeights);

			const mdxaBone_t& bone = boneCache.Eval(piBoneReferences[iBoneIndex]);

			pTri[2][0] += fBoneWeight * (DotProduct(bone.matrix[0], vert2->vertCoords) + bone.matrix[0][3]);
			pTri[2][1] += fBoneWeight * (DotProduct(bone.matrix[1], vert2->vertCoords) + bone.matrix[1][3]);
			pTri[2][2] += fBoneWeight * (DotProduct(bone.matrix[2], vert2->vertCoords) + bone.matrix[2][3]);
		}

		vec3_t normal;
		vec3_t up{};
		vec3_t right;
		vec3_t vec0, vec1;
		// work out baryCentricK
		const float baryCentricK = 1.0 - (surfInfo->genBarycentricI + surfInfo->genBarycentricJ);

		// now we have the model transformed into model space, now generate an origin.
		retMatrix.matrix[0][3] = pTri[0][0] * surfInfo->genBarycentricI + pTri[1][0] * surfInfo->genBarycentricJ + pTri[2][0] * baryCentricK;
		retMatrix.matrix[1][3] = pTri[0][1] * surfInfo->genBarycentricI + pTri[1][1] * surfInfo->genBarycentricJ + pTri[2][1] * baryCentricK;
		retMatrix.matrix[2][3] = pTri[0][2] * surfInfo->genBarycentricI + pTri[1][2] * surfInfo->genBarycentricJ + pTri[2][2] * baryCentricK;

		// generate a normal to this new triangle
		VectorSubtract(pTri[0], pTri[1], vec0);
		VectorSubtract(pTri[2], pTri[1], vec1);

		CrossProduct(vec0, vec1, normal);
		VectorNormalize(normal);

		// forward vector
		retMatrix.matrix[0][0] = normal[0];
		retMatrix.matrix[1][0] = normal[1];
		retMatrix.matrix[2][0] = normal[2];

		// up will be towards point 0 of the original triangle.
		// so lets work it out. Vector is hit point - point 0
		up[0] = retMatrix.matrix[0][3] - pTri[0][0];
		up[1] = retMatrix.matrix[1][3] - pTri[0][1];
		up[2] = retMatrix.matrix[2][3] - pTri[0][2];

		// normalise it
		VectorNormalize(up);

		// that's the up vector
		retMatrix.matrix[0][1] = up[0];
		retMatrix.matrix[1][1] = up[1];
		retMatrix.matrix[2][1] = up[2];

		// right is always straight

		CrossProduct(normal, up, right);
		// that's the up vector
		retMatrix.matrix[0][2] = right[0];
		retMatrix.matrix[1][2] = right[1];
		retMatrix.matrix[2][2] = right[2];
	}
	// no, we are looking at a normal model tag
	else
	{
		int j;
		vec3_t sides[3];
		vec3_t axes[3];
		// whip through and actually transform each vertex
		mdxmVertex_t* v = reinterpret_cast<mdxmVertex_t*>((byte*)surface + surface->ofsVerts);
		const int* piBoneReferences = reinterpret_cast<int*>((byte*)surface + surface->ofsBoneReferences);
		for (j = 0; j < 3; j++)
		{
			// 			mdxmWeight_t	*w;

			VectorClear(pTri[j]);
			//			w = v->weights;

			const int iNumWeights = G2_GetVertWeights(v);

			float fTotalWeight = 0.0f;
			for (k = 0; k < iNumWeights; k++)
			{
				const int		iBoneIndex = G2_GetVertBoneIndex(v, k);
				const float	fBoneWeight = G2_GetVertBoneWeight(v, k, fTotalWeight, iNumWeights);

				const mdxaBone_t& bone = boneCache.Eval(piBoneReferences[iBoneIndex]);

				pTri[j][0] += fBoneWeight * (DotProduct(bone.matrix[0], v->vertCoords) + bone.matrix[0][3]);
				pTri[j][1] += fBoneWeight * (DotProduct(bone.matrix[1], v->vertCoords) + bone.matrix[1][3]);
				pTri[j][2] += fBoneWeight * (DotProduct(bone.matrix[2], v->vertCoords) + bone.matrix[2][3]);
			}

			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
		}

		// clear out used arrays
		memset(axes, 0, sizeof axes);
		memset(sides, 0, sizeof sides);

		// work out actual sides of the tag triangle
		for (j = 0; j < 3; j++)
		{
			sides[j][0] = pTri[(j + 1) % 3][0] - pTri[j][0];
			sides[j][1] = pTri[(j + 1) % 3][1] - pTri[j][1];
			sides[j][2] = pTri[(j + 1) % 3][2] - pTri[j][2];
		}

		// do math trig to work out what the matrix will be from this triangle's translated position
		VectorNormalize2(sides[iG2_TRISIDE_LONGEST], axes[0]);
		VectorNormalize2(sides[iG2_TRISIDE_SHORTEST], axes[1]);

		// project shortest side so that it is exactly 90 degrees to the longer side
		const float d = DotProduct(axes[0], axes[1]);
		VectorMA(axes[0], -d, axes[1], axes[0]);
		VectorNormalize2(axes[0], axes[0]);

		CrossProduct(sides[iG2_TRISIDE_LONGEST], sides[iG2_TRISIDE_SHORTEST], axes[2]);
		VectorNormalize2(axes[2], axes[2]);

		// set up location in world space of the origin point in out going matrix
		retMatrix.matrix[0][3] = pTri[MDX_TAG_ORIGIN][0];
		retMatrix.matrix[1][3] = pTri[MDX_TAG_ORIGIN][1];
		retMatrix.matrix[2][3] = pTri[MDX_TAG_ORIGIN][2];

		// copy axis to matrix - do some magic to orient minus Y to positive X and so on so bolt on stuff is oriented correctly
		retMatrix.matrix[0][0] = axes[1][0];
		retMatrix.matrix[0][1] = axes[0][0];
		retMatrix.matrix[0][2] = -axes[2][0];

		retMatrix.matrix[1][0] = axes[1][1];
		retMatrix.matrix[1][1] = axes[0][1];
		retMatrix.matrix[1][2] = -axes[2][1];

		retMatrix.matrix[2][0] = axes[1][2];
		retMatrix.matrix[2][1] = axes[0][2];
		retMatrix.matrix[2][2] = -axes[2][2];
	}
}

void G2_GetBoltMatrixLow(CGhoul2Info& ghoul2, const int boltNum, const vec3_t scale, mdxaBone_t& retMatrix)
{
	if (!ghoul2.mBoneCache)
	{
		retMatrix = identityMatrix;
		return;
	}
	assert(ghoul2.mBoneCache);
	CBoneCache& boneCache = *ghoul2.mBoneCache;
	assert(boneCache.mod);
	boltInfo_v& boltList = ghoul2.mBltlist;
	assert(boltNum >= 0 && boltNum < static_cast<int>(boltList.size()));
	if (boltList[boltNum].boneNumber >= 0)
	{
		const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t));
		const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>((byte*)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boltList[boltNum]
			.boneNumber]);
		Multiply_3x4Matrix(&retMatrix, &boneCache.EvalUnsmooth(boltList[boltNum].boneNumber), &skel->BasePoseMat);
	}
	else if (boltList[boltNum].surface_number >= 0)
	{
		const surfaceInfo_t* surfInfo = nullptr;
		{
			for (surfaceInfo_t& t : ghoul2.mSlist)
			{
				if (t.surface == boltList[boltNum].surface_number)
				{
					surfInfo = &t;
				}
			}
		}
		const mdxmSurface_t* surface = nullptr;
		if (!surfInfo)
		{
			surface = static_cast<mdxmSurface_t*>(G2_FindSurface(boneCache.mod, boltList[boltNum].surface_number, 0));
		}
		if (!surface && surfInfo && surfInfo->surface < 10000)
		{
			surface = static_cast<mdxmSurface_t*>(G2_FindSurface(boneCache.mod, surfInfo->surface, 0));
		}
		G2_ProcessSurfaceBolt2(boneCache, surface, boltNum, boltList, surfInfo, boneCache.mod, retMatrix);
	}
	else
	{
		// we have a bolt without a bone or surface, not a huge problem but we ought to at least clear the bolt matrix
		retMatrix = identityMatrix;
	}
}

void G2API_SetSurfaceOnOffFromSkin(CGhoul2Info* ghlInfo, const qhandle_t render_skin)
{
	const skin_t* skin = R_GetSkinByHandle(render_skin);
	//FIXME:  using skin handles means we have to increase the numsurfs in a skin, but reading directly would cause file hits, we need another way to cache or just deal with the larger skin_t

	if (skin)
	{
		ghlInfo->mSlist.clear();	//remove any overrides we had before.
		ghlInfo->mMeshFrameNum = 0;
		for (int j = 0; j < skin->numSurfaces; j++)
		{
			uint32_t flags;
			const int surfaceNum = G2_IsSurfaceLegal(ghlInfo->currentModel, skin->surfaces[j]->name, &flags);
			// the names have both been lowercased
			if (!(flags & G2SURFACEFLAG_OFF) && strcmp(skin->surfaces[j]->shader->name, "*off") == 0)
			{
				G2_SetSurfaceOnOff(ghlInfo, skin->surfaces[j]->name, G2SURFACEFLAG_OFF);
			}
			else
			{
				//if ( strcmp( &skin->surfaces[j]->name[strlen(skin->surfaces[j]->name)-4],"_off") )
				if (surfaceNum != -1 && !(flags & G2SURFACEFLAG_OFF))	//only turn on if it's not an "_off" surface
				{
					//G2_SetSurfaceOnOff(ghlInfo, skin->surfaces[j]->name, 0);
				}
			}
		}
	}
}

// set up each surface ready for rendering in the back end
void RenderSurfaces(CRenderSurface& RS)
{
	assert(RS.currentModel);
	assert(RS.currentModel->mdxm);
	// back track and get the surfinfo struct for this surface
	mdxmSurface_t* surface = static_cast<mdxmSurface_t*>(G2_FindSurface(RS.currentModel, RS.surfaceNum, RS.lod));
	mdxmHierarchyOffsets_t* surf_indexes = reinterpret_cast<mdxmHierarchyOffsets_t*>(reinterpret_cast<byte*>(RS.currentModel->mdxm) + sizeof(mdxmHeader_t));
	const mdxmSurfHierarchy_t* surfInfo = reinterpret_cast<mdxmSurfHierarchy_t*>(reinterpret_cast<byte*>(surf_indexes) + surf_indexes->offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	const surfaceInfo_t* surfOverride = G2_FindOverrideSurface(RS.surfaceNum, RS.rootSList);

	// really, we should use the default flags for this surface unless it's been overriden
	int offFlags = surfInfo->flags;

	// set the off flags if we have some
	if (surfOverride)
	{
		offFlags = surfOverride->offFlags;
	}

	// if this surface is not off, add it to the shader render list
	if (!offFlags)
	{
		const shader_t* shader;
		if (RS.cust_shader)
		{
			shader = RS.cust_shader;
		}
		else if (RS.skin)
		{
			// match the surface name to something in the skin file
			shader = R_GetShaderByHandle(surfInfo->shaderIndex);	//tr.defaultShader;
			for (int j = 0; j < RS.skin->numSurfaces; j++)
			{
				// the names have both been lowercased
				if (strcmp(RS.skin->surfaces[j]->name, surfInfo->name) == 0)
				{
					shader = RS.skin->surfaces[j]->shader;
					break;
				}
			}
		}
		else
		{
			shader = R_GetShaderByHandle(surfInfo->shaderIndex);
		}

		// we will add shadows even if the main object isn't visible in the view
		// stencil shadows can't do personal models unless I polyhedron clip
		//using z-fail now so can do personal models -rww
		if (r_AdvancedsurfaceSprites->integer)
		{
			if (r_shadows->integer == 2
				&& !(RS.renderfx & (RF_DEPTHHACK))
				&& shader->sort == SS_OPAQUE)
			{
				// set the surface info to point at the where the transformed bone list is going to be for when the surface gets rendered out
				CRenderableSurface* newSurf = AllocRS();
				if (surface->numVerts >= SHADER_MAX_VERTEXES / 2)
				{
					//we need numVerts*2 xyz slots free in tess to do shadow, if this surf is going to exceed that then let's try the lowest lod -rww
					const auto lowsurface = static_cast<mdxmSurface_t*>(G2_FindSurface(
						RS.currentModel, RS.surfaceNum, RS.currentModel->numLods - 1));
					newSurf->surfaceData = lowsurface;
				}
				else
				{
					newSurf->surfaceData = surface;
				}
				newSurf->boneCache = RS.boneCache;
				R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(newSurf), tr.shadowShader, 0, qfalse);
			}
		}
		else
		{
			if (r_shadows->integer == 2
				&& (RS.renderfx & RF_SHADOW_PLANE)
				&& !(RS.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
				&& shader->sort == SS_OPAQUE)
			{
				// set the surface info to point at the where the transformed bone list is going to be for when the surface gets rendered out
				CRenderableSurface* newSurf = AllocRS();
				if (surface->numVerts >= SHADER_MAX_VERTEXES / 2)
				{
					//we need numVerts*2 xyz slots free in tess to do shadow, if this surf is going to exceed that then let's try the lowest lod -rww
					const auto lowsurface = static_cast<mdxmSurface_t*>(G2_FindSurface(
						RS.currentModel, RS.surfaceNum, RS.currentModel->numLods - 1));
					newSurf->surfaceData = lowsurface;
				}
				else
				{
					newSurf->surfaceData = surface;
				}
				newSurf->boneCache = RS.boneCache;
				R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(newSurf), tr.shadowShader, 0, qfalse);
			}
		}

		// projection shadows work fine with personal models
		if (r_shadows->integer == 3
			&& RS.renderfx & RF_SHADOW_PLANE
			&& !(RS.renderfx & RF_NOSHADOW)
			&& shader->sort == SS_OPAQUE)
		{
			// set the surface info to point at the where the transformed bone list is going to be for when the surface gets rendered out
			CRenderableSurface* newSurf = AllocRS();
			newSurf->surfaceData = surface;
			newSurf->boneCache = RS.boneCache;
			R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(newSurf), tr.projectionShadowShader, 0, qfalse);
		}

		// don't add third_person objects if not viewing through a portal
		if (!RS.personalModel)
		{
			// set the surface info to point at the where the transformed bone list is going to be for when the surface gets rendered out
			CRenderableSurface* newSurf = AllocRS();
			newSurf->surfaceData = surface;
			newSurf->boneCache = RS.boneCache;
			R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(newSurf), shader, RS.fogNum, qfalse);

#ifdef _G2_GORE
			if (RS.gore_set)
			{
				const int curTime = G2API_GetTime(tr.refdef.time);
				const std::pair<std::multimap<int, SGoreSurface>::iterator, std::multimap<int, SGoreSurface>::iterator> range =
					RS.gore_set->mGoreRecords.equal_range(RS.surfaceNum);
				CRenderableSurface* last = newSurf;
				for (std::multimap<int, SGoreSurface>::iterator k = range.first; k != range.second;)
				{
					std::multimap<int, SGoreSurface>::iterator kcur = k;
					++k;
					GoreTextureCoordinates* tex = FindGoreRecord((*kcur).second.mGoreTag);
					if (!tex ||											 // it is gone, lets get rid of it
						(kcur->second.mDeleteTime && curTime >= kcur->second.mDeleteTime)) // out of time
					{
						if (tex)
						{
							(*tex).~GoreTextureCoordinates();
							//I don't know what's going on here, it should call the destructor for
							//this when it erases the record but sometimes it doesn't. -rww
						}

						RS.gore_set->mGoreRecords.erase(kcur);
					}
					else if (tex->tex[RS.lod])
					{
						CRenderableSurface* newSurf2 = AllocRS();
						*newSurf2 = *newSurf;
						newSurf2->goreChain = nullptr;
						newSurf2->alternateTex = tex->tex[RS.lod];
						newSurf2->scale = 1.0f;
						newSurf2->fade = 1.0f;
						newSurf2->impactTime = 1.0f;	// done with
						constexpr int magicFactor42 = 500; // ms, impact time
						if (curTime > (*kcur).second.mGoreGrowStartTime && curTime < (*kcur).second.mGoreGrowStartTime + magicFactor42)
						{
							newSurf2->impactTime = static_cast<float>(curTime - (*kcur).second.mGoreGrowStartTime) / static_cast<float>(magicFactor42);  // linear
						}
						if (curTime < (*kcur).second.mGoreGrowEndTime)
						{
							newSurf2->scale = 1.0f / ((curTime - (*kcur).second.mGoreGrowStartTime) * (*kcur).second.mGoreGrowFactor + (*kcur).second.mGoreGrowOffset);
							if (newSurf2->scale < 1.0f)
							{
								newSurf2->scale = 1.0f;
							}
						}
						shader_t* gshader;
						if ((*kcur).second.shader)
						{
							gshader = R_GetShaderByHandle((*kcur).second.shader);
						}
						else
						{
							gshader = R_GetShaderByHandle(goreShader);
						}

						// Set fade on surf.
						//Only if we have a fade time set, and let us fade on rgb if we want -rww
						if ((*kcur).second.mDeleteTime && (*kcur).second.mFadeTime)
						{
							if ((*kcur).second.mDeleteTime - curTime < (*kcur).second.mFadeTime)
							{
								newSurf2->fade = static_cast<float>((*kcur).second.mDeleteTime - curTime) / (*kcur).second.mFadeTime;
								if ((*kcur).second.mFadeRGB)
								{ //RGB fades are scaled from 2.0f to 3.0f (simply to differentiate)
									newSurf2->fade += 2.0f;

									if (newSurf2->fade < 2.01f)
									{
										newSurf2->fade = 2.01f;
									}
								}
							}
						}

						last->goreChain = newSurf2;
						last = newSurf2;
						R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(newSurf2), gshader, RS.fogNum, qfalse);
					}
				}
			}
#endif
		}
	}

	// if we are turning off all descendants, then stop this recursion now
	if (offFlags & G2SURFACEFLAG_NODESCENDANTS)
	{
		return;
	}

	// now recursively call for the children
	for (int i = 0; i < surfInfo->numChildren; i++)
	{
		RS.surfaceNum = surfInfo->childIndexes[i];
		RenderSurfaces(RS);
	}
}

// sort all the ghoul models in this list so if they go in reference order. This will ensure the bolt on's are attached to the right place
// on the previous model, since it ensures the model being attached to is built and rendered first.

// NOTE!! This assumes at least one model will NOT have a parent. If it does - we are screwed
static void G2_Sort_Models(CGhoul2Info_v& ghoul2, int* const model_list, int* const modelCount)
{
	int		i;

	*modelCount = 0;

	// first walk all the possible ghoul2 models, and stuff the out array with those with no parents
	for (i = 0; i < ghoul2.size(); i++)
	{
		// have a ghoul model here?
		if (ghoul2[i].mModelindex == -1 || !ghoul2[i].mValid)
		{
			continue;
		}
		// are we attached to anything?
		if (ghoul2[i].mModelBoltLink == -1)
		{
			// no, insert us first
			model_list[(*modelCount)++] = i;
		}
	}

	int startPoint = 0;
	int endPoint = *modelCount;

	// now, using that list of parentless models, walk the descendant tree for each of them, inserting the descendents in the list
	while (startPoint != endPoint)
	{
		for (i = 0; i < ghoul2.size(); i++)
		{
			// have a ghoul model here?
			if (ghoul2[i].mModelindex == -1 || !ghoul2[i].mValid)
			{
				continue;
			}

			// what does this model think it's attached to?
			if (ghoul2[i].mModelBoltLink != -1)
			{
				const int boltTo = ghoul2[i].mModelBoltLink >> MODEL_SHIFT & MODEL_AND;
				// is it any of the models we just added to the list?
				for (int j = startPoint; j < endPoint; j++)
				{
					// is this my parent model?
					if (boltTo == model_list[j])
					{
						// yes, insert into list and exit now
						model_list[(*modelCount)++] = i;
						break;
					}
				}
			}
		}
		// update start and end points
		startPoint = endPoint;
		endPoint = *modelCount;
	}
}

static void RootMatrix(CGhoul2Info_v& ghoul2, const int time, const vec3_t scale, mdxaBone_t& retMatrix)
{
	for (int i = 0; i < ghoul2.size(); i++)
	{
		if (ghoul2[i].mModelindex != -1 && ghoul2[i].mValid)
		{
			if (ghoul2[i].mFlags & GHOUL2_NEWORIGIN)
			{
				mdxaBone_t bolt;
				mdxaBone_t		tempMatrix{};

				G2_ConstructGhoulSkeleton(ghoul2, time, false, scale);
				G2_GetBoltMatrixLow(ghoul2[i], ghoul2[i].mNewOrigin, scale, bolt);
				tempMatrix.matrix[0][0] = 1.0f;
				tempMatrix.matrix[0][1] = 0.0f;
				tempMatrix.matrix[0][2] = 0.0f;
				tempMatrix.matrix[0][3] = -bolt.matrix[0][3];
				tempMatrix.matrix[1][0] = 0.0f;
				tempMatrix.matrix[1][1] = 1.0f;
				tempMatrix.matrix[1][2] = 0.0f;
				tempMatrix.matrix[1][3] = -bolt.matrix[1][3];
				tempMatrix.matrix[2][0] = 0.0f;
				tempMatrix.matrix[2][1] = 0.0f;
				tempMatrix.matrix[2][2] = 1.0f;
				tempMatrix.matrix[2][3] = -bolt.matrix[2][3];
				Multiply_3x4Matrix(&retMatrix, &tempMatrix, &identityMatrix);
				return;
			}
		}
	}
	retMatrix = identityMatrix;
}

extern cvar_t* r_shadowRange;
static bool bInShadowRange(vec3_t location)
{
	const float c = DotProduct(tr.viewParms.ori.axis[0], tr.viewParms.ori.origin);
	const float dist = DotProduct(tr.viewParms.ori.axis[0], location) - c;

	//	return (dist < tr.distanceCull/1.5f);
	return dist < r_shadowRange->value;
}

/*
==============
R_AddGHOULSurfaces
==============
*/
void R_AddGhoulSurfaces(trRefEntity_t* ent)
{
	shader_t* cust_shader;
	int				i, whichLod;
	skin_t* skin;
	int				modelCount;
	mdxaBone_t		rootMatrix;

	// if we don't want ghoul2 models, then return
	if (r_noGhoul2->integer)
	{
		return;
	}

	assert(ent->e.ghoul2);	//entity is foo if it has a glm model handle but no ghoul2 pointer!
	CGhoul2Info_v& ghoul2 = *((CGhoul2Info_v*)ent->e.ghoul2);

	if (!G2_SetupModelPointers(ghoul2))
	{
		return;
	}

	const int current_time = G2API_GetTime(tr.refdef.time);

	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	const int cull = R_GCullModel(ent);
	if (cull == CULL_OUT)
	{
		return;
	}
	HackadelicOnClient = true;
	// are any of these models setting a new origin?
	RootMatrix(ghoul2, current_time, ent->e.modelScale, rootMatrix);

	// don't add third_person objects if not in a portal
	qboolean personalModel = static_cast<qboolean>(ent->e.renderfx & RF_THIRD_PERSON && !tr.viewParms.is_portal);

	int model_list[32]{};
	assert(ghoul2.size() <= 31);
	model_list[31] = 548;

	// set up lighting now that we know we aren't culled
	if (!personalModel || r_shadows->integer > 1) {
		R_SetupEntityLighting(&tr.refdef, ent);
	}

	// see if we are in a fog volume
	int fogNum = R_GComputeFogNum(ent);

	// sort the ghoul 2 models so bolt ons get bolted to the right model
	G2_Sort_Models(ghoul2, model_list, &modelCount);
	assert(model_list[31] == 548);

#ifdef _G2_GORE
	if (goreShader == -1)
	{
		goreShader = RE_RegisterShader("gfx/damage/burnmark1");
	}
#endif

	// construct a world matrix for this entity
	G2_GenerateWorldMatrix(ent->e.angles, ent->e.origin);

	// walk each possible model for this entity and try rendering it out
	for (int j = 0; j < modelCount; j++)
	{
		i = model_list[j];
		if (ghoul2[i].mValid && !(ghoul2[i].mFlags & GHOUL2_NOMODEL) && !(ghoul2[i].mFlags & GHOUL2_NORENDER))
		{
			shader_t* gore_shader = nullptr;
			//
			// figure out whether we should be using a custom shader for this model
			//
			skin = nullptr;
			if (ent->e.customShader)
			{
				cust_shader = R_GetShaderByHandle(ent->e.customShader);
			}
			else
			{
				cust_shader = nullptr;
				// figure out the custom skin thing
				if (ent->e.customSkin)
				{
					skin = R_GetSkinByHandle(ent->e.customSkin);
				}
				else if (ghoul2[i].mSkin > 0 && ghoul2[i].mSkin < tr.numSkins)
				{
					skin = R_GetSkinByHandle(ghoul2[i].mSkin);
				}
			}

			if (j && ghoul2[i].mModelBoltLink != -1)
			{
				const int	boltMod = ghoul2[i].mModelBoltLink >> MODEL_SHIFT & MODEL_AND;
				const int	boltNum = ghoul2[i].mModelBoltLink >> BOLT_SHIFT & BOLT_AND;
				mdxaBone_t bolt;
				G2_GetBoltMatrixLow(ghoul2[boltMod], boltNum, ent->e.modelScale, bolt);
				G2_TransformGhoulBones(ghoul2[i].mBlist, bolt, ghoul2[i], current_time);
			}
			else
			{
				G2_TransformGhoulBones(ghoul2[i].mBlist, rootMatrix, ghoul2[i], current_time);
			}
			if (ent->e.renderfx & RF_G2MINLOD)
			{
				whichLod = G2_ComputeLOD(ent, ghoul2[i].currentModel, 10);
			}
			else
			{
				whichLod = G2_ComputeLOD(ent, ghoul2[i].currentModel, ghoul2[i].mLodBias);
			}
			G2_FindOverrideSurface(-1, ghoul2[i].mSlist); //reset the quick surface override lookup;
#ifdef _G2_GORE
			CGoreSet* gore = nullptr;
			if (ghoul2[i].mGoreSetTag)
			{
				gore = FindGoreSet(ghoul2[i].mGoreSetTag);
				if (!gore) // my gore is gone, so remove it
				{
					ghoul2[i].mGoreSetTag = 0;
				}
			}

			CRenderSurface RS(ghoul2[i].mSurfaceRoot, ghoul2[i].mSlist, cust_shader, fogNum, personalModel, ghoul2[i].mBoneCache, ent->e.renderfx, skin, ghoul2[i].currentModel, whichLod, ghoul2[i].mBltlist, gore_shader, gore);
#else
			CRenderSurface RS(ghoul2[i].mSurfaceRoot, ghoul2[i].mSlist, cust_shader, fogNum, personalModel, ghoul2[i].mBoneCache, ent->e.renderfx, skin, ghoul2[i].currentModel, whichLod, ghoul2[i].mBltlist);
#endif
			if (!personalModel && RS.renderfx & RF_SHADOW_PLANE && !bInShadowRange(ent->e.origin))
			{
				RS.renderfx |= RF_NOSHADOW;
			}
			RenderSurfaces(RS);
		}
	}
	HackadelicOnClient = false;
}

bool G2_NeedsRecalc(CGhoul2Info* ghlInfo, const int frameNum)
{
	G2_SetupModelPointers(ghlInfo);
	// not sure if I still need this test, probably
	if (ghlInfo->mSkelFrameNum != frameNum ||
		!ghlInfo->mBoneCache ||
		ghlInfo->mBoneCache->mod != ghlInfo->currentModel)
	{
		ghlInfo->mSkelFrameNum = frameNum;
		return true;
	}
	return false;
}

/*
==============
G2_ConstructGhoulSkeleton - builds a complete skeleton for all ghoul models in a CGhoul2Info_v class	- using LOD 0
==============
*/
void G2_ConstructGhoulSkeleton(CGhoul2Info_v& ghoul2, const int frameNum, const bool checkForNewOrigin, const vec3_t scale)
{
	int				modelCount;
	mdxaBone_t		rootMatrix;

	int model_list[32]{};
	assert(ghoul2.size() <= 31);
	model_list[31] = 548;

	if (checkForNewOrigin)
	{
		RootMatrix(ghoul2, frameNum, scale, rootMatrix);
	}
	else
	{
		rootMatrix = identityMatrix;
	}

	G2_Sort_Models(ghoul2, model_list, &modelCount);
	assert(model_list[31] == 548);

	for (int j = 0; j < modelCount; j++)
	{
		// get the sorted model to play with
		const int i = model_list[j];

		if (ghoul2[i].mValid)
		{
			if (j && ghoul2[i].mModelBoltLink != -1)
			{
				const int	boltMod = ghoul2[i].mModelBoltLink >> MODEL_SHIFT & MODEL_AND;
				const int	boltNum = ghoul2[i].mModelBoltLink >> BOLT_SHIFT & BOLT_AND;

				mdxaBone_t bolt;
				G2_GetBoltMatrixLow(ghoul2[boltMod], boltNum, scale, bolt);
				G2_TransformGhoulBones(ghoul2[i].mBlist, bolt, ghoul2[i], frameNum, checkForNewOrigin);
			}
			else
			{
				G2_TransformGhoulBones(ghoul2[i].mBlist, rootMatrix, ghoul2[i], frameNum, checkForNewOrigin);
			}
		}
	}
}

/*
==============
RB_SurfaceGhoul
==============
*/
void RB_SurfaceGhoul(CRenderableSurface* surf)
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceTimer_RB_SurfaceGhoul.Start();
#endif

	int				j, k;
	int				baseIndex, baseVertex;
	int				numVerts;
	mdxmVertex_t* v;
	int* triangles;
	int				indexes;
	glIndex_t* tessIndexes;
	mdxmVertexTexCoord_t* pTexCoords;
	int* piBoneReferences;

#ifdef _G2_GORE
	if (surf->alternateTex)
	{
		// a gore surface ready to go.

		/*
			sizeof(int)+ // num verts
			sizeof(int)+ // num tris
			sizeof(int)*newNumVerts+ // which verts to copy from original surface
			sizeof(float)*4*newNumVerts+ // storgage for deformed verts
			sizeof(float)*4*newNumVerts+ // storgage for deformed normal
			sizeof(float)*2*newNumVerts+ // texture coordinates
			sizeof(int)*newNumTris*3;  // new indecies
		*/

		int* data = reinterpret_cast<int*>(surf->alternateTex);
		numVerts = *data++;
		indexes = *data++;
		// first up, sanity check our numbers
		RB_CheckOverflow(numVerts, indexes);
		indexes *= 3;

		data += numVerts;

		baseIndex = tess.numIndexes;
		baseVertex = tess.numVertexes;

		memcpy(&tess.xyz[baseVertex][0], data, sizeof(float) * 4 * numVerts);
		data += 4 * numVerts;
		memcpy(&tess.normal[baseVertex][0], data, sizeof(float) * 4 * numVerts);
		data += 4 * numVerts;
		assert(numVerts > 0);

		//float *texCoords = tess.texCoords[0][baseVertex];
		float* texCoords = tess.texCoords[baseVertex][0];
		int hack = baseVertex;
		//rww - since the array is arranged as such we cannot increment
		//the relative memory position to get where we want. Maybe this
		//is why sof2 has the texCoords array reversed. In any case, I
		//am currently too lazy to get around it.
		//Or can you += array[.][x]+2?
		if (surf->scale > 1.0f)
		{
			for (j = 0; j < numVerts; j++)
			{
				texCoords[0] = (*reinterpret_cast<float*>(data) - 0.5f) * surf->scale + 0.5f;
				data++;
				texCoords[1] = (*reinterpret_cast<float*>(data) - 0.5f) * surf->scale + 0.5f;
				data++;
				//texCoords+=2;// Size of gore (s,t).
				hack++;
				texCoords = tess.texCoords[hack][0];
			}
		}
		else
		{
			for (j = 0; j < numVerts; j++)
			{
				texCoords[0] = *reinterpret_cast<float*>(data++);
				texCoords[1] = *reinterpret_cast<float*>(data++);
				//				texCoords+=2;// Size of gore (s,t).
				hack++;
				texCoords = tess.texCoords[hack][0];
			}
		}

		//now check for fade overrides -rww
		if (surf->fade)
		{
			static int lFade;
			static int i;

			if (surf->fade < 1.0)
			{
				tess.fading = true;
				lFade = Q_ftol(254.4f * surf->fade);

				for (i = 0; i < numVerts; i++)
				{
					tess.svars.colors[i + baseVertex][3] = lFade;
				}
			}
			else if (surf->fade > 2.0f && surf->fade < 3.0f)
			{ //hack to fade out on RGB if desired (don't want to add more to CRenderableSurface) -rww
				tess.fading = true;
				lFade = Q_ftol(254.4f * (surf->fade - 2.0f));

				for (i = 0; i < numVerts; i++)
				{
					if (lFade < tess.svars.colors[i + baseVertex][0])
					{ //don't set it unless the fade is less than the current r value (to avoid brightening suddenly before we start fading)
						tess.svars.colors[i + baseVertex][0] = tess.svars.colors[i + baseVertex][1] = tess.svars.colors[i + baseVertex][2] = lFade;
					}

					//Set the alpha as well I suppose, no matter what
					tess.svars.colors[i + baseVertex][3] = lFade;
				}
			}
		}

		glIndex_t* indexPtr = &tess.indexes[baseIndex];
		triangles = data;
		for (j = indexes; j; j--)
		{
			*indexPtr++ = baseVertex + *triangles++;
		}
		tess.numIndexes += indexes;
		tess.numVertexes += numVerts;
		return;
	}
#endif

	// grab the pointer to the surface info within the loaded mesh file
	mdxmSurface_t* surface = surf->surfaceData;

	CBoneCache* bones = surf->boneCache;

	// first up, sanity check our numbers
	RB_CheckOverflow(surface->numVerts, surface->numTriangles);

	//
	// deform the vertexes by the lerped bones
	//

	// first up, sanity check our numbers
	baseVertex = tess.numVertexes;
	triangles = reinterpret_cast<int*>(reinterpret_cast<byte*>(surface) + surface->ofsTriangles);
	baseIndex = tess.numIndexes;
#if 0
	indexes = surface->numTriangles * 3;
	for (j = 0; j < indexes; j++) {
		tess.indexes[baseIndex + j] = baseVertex + triangles[j];
	}
	tess.numIndexes += indexes;
#else
	indexes = surface->numTriangles; //*3;	//unrolled 3 times, don't multiply
	tessIndexes = &tess.indexes[baseIndex];
	for (j = 0; j < indexes; j++) {
		*tessIndexes++ = baseVertex + *triangles++;
		*tessIndexes++ = baseVertex + *triangles++;
		*tessIndexes++ = baseVertex + *triangles++;
	}
	tess.numIndexes += indexes * 3;
#endif

	numVerts = surface->numVerts;

	piBoneReferences = reinterpret_cast<int*>(reinterpret_cast<byte*>(surface) + surface->ofsBoneReferences);
	baseVertex = tess.numVertexes;
	v = reinterpret_cast<mdxmVertex_t*>(reinterpret_cast<byte*>(surface) + surface->ofsVerts);
	pTexCoords = reinterpret_cast<mdxmVertexTexCoord_t*>(&v[numVerts]);

	//	if (r_ghoul2fastnormals&&r_ghoul2fastnormals->integer==0)
#if 0
	if (0)
	{
		for (j = 0; j < numVerts; j++, baseVertex++, v++)
		{
			const int iNumWeights = G2_GetVertWeights(v);

			float fTotalWeight = 0.0f;

			k = 0;
			int		iBoneIndex = G2_GetVertBoneIndex(v, k);
			float	fBoneWeight = G2_GetVertBoneWeight(v, k, fTotalWeight, iNumWeights);
			const mdxaBone_t* bone = &bones->EvalRender(piBoneReferences[iBoneIndex]);

			tess.xyz[baseVertex][0] = fBoneWeight * (DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3]);
			tess.xyz[baseVertex][1] = fBoneWeight * (DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3]);
			tess.xyz[baseVertex][2] = fBoneWeight * (DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3]);

			tess.normal[baseVertex][0] = fBoneWeight * DotProduct(bone->matrix[0], v->normal);
			tess.normal[baseVertex][1] = fBoneWeight * DotProduct(bone->matrix[1], v->normal);
			tess.normal[baseVertex][2] = fBoneWeight * DotProduct(bone->matrix[2], v->normal);

			for (k++; k < iNumWeights; k++)
			{
				iBoneIndex = G2_GetVertBoneIndex(v, k);
				fBoneWeight = G2_GetVertBoneWeight(v, k, fTotalWeight, iNumWeights);

				bone = &bones->EvalRender(piBoneReferences[iBoneIndex]);

				tess.xyz[baseVertex][0] += fBoneWeight * (DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3]);
				tess.xyz[baseVertex][1] += fBoneWeight * (DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3]);
				tess.xyz[baseVertex][2] += fBoneWeight * (DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3]);

				tess.normal[baseVertex][0] += fBoneWeight * DotProduct(bone->matrix[0], v->normal);
				tess.normal[baseVertex][1] += fBoneWeight * DotProduct(bone->matrix[1], v->normal);
				tess.normal[baseVertex][2] += fBoneWeight * DotProduct(bone->matrix[2], v->normal);
			}

			tess.texCoords[baseVertex][0][0] = pTexCoords[j].texCoords[0];
			tess.texCoords[baseVertex][0][1] = pTexCoords[j].texCoords[1];
		}
	}
	else
	{
#endif
		float fTotalWeight;
		float fBoneWeight;
		float t1;
		float t2;
		const mdxaBone_t* bone;
		const mdxaBone_t* bone2;
		for (j = 0; j < numVerts; j++, baseVertex++, v++)
		{
#ifdef JK2_MODE
			bone = &bones->Eval(piBoneReferences[G2_GetVertBoneIndex(v, 0)]);
#else
			bone = &bones->EvalRender(piBoneReferences[G2_GetVertBoneIndex(v, 0)]);
#endif // JK2_MODE
			int iNumWeights = G2_GetVertWeights(v);
			tess.normal[baseVertex][0] = DotProduct(bone->matrix[0], v->normal);
			tess.normal[baseVertex][1] = DotProduct(bone->matrix[1], v->normal);
			tess.normal[baseVertex][2] = DotProduct(bone->matrix[2], v->normal);

			if (iNumWeights == 1)
			{
				tess.xyz[baseVertex][0] = DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3];
				tess.xyz[baseVertex][1] = DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3];
				tess.xyz[baseVertex][2] = DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3];
			}
			else
			{
				fBoneWeight = G2_GetVertBoneWeightNotSlow(v, 0);
				if (iNumWeights == 2)
				{
#ifdef JK2_MODE
					bone2 = &bones->Eval(piBoneReferences[G2_GetVertBoneIndex(v, 1)]);
#else
					bone2 = &bones->EvalRender(piBoneReferences[G2_GetVertBoneIndex(v, 1)]);
#endif // JK2_MODE
					/*
					useless transposition
					tess.xyz[baseVertex][0] =
					v[0]*(w*(bone->matrix[0][0]-bone2->matrix[0][0])+bone2->matrix[0][0])+
					v[1]*(w*(bone->matrix[0][1]-bone2->matrix[0][1])+bone2->matrix[0][1])+
					v[2]*(w*(bone->matrix[0][2]-bone2->matrix[0][2])+bone2->matrix[0][2])+
					w*(bone->matrix[0][3]-bone2->matrix[0][3]) + bone2->matrix[0][3];
					*/
					t1 = DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3];
					t2 = DotProduct(bone2->matrix[0], v->vertCoords) + bone2->matrix[0][3];
					tess.xyz[baseVertex][0] = fBoneWeight * (t1 - t2) + t2;
					t1 = DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3];
					t2 = DotProduct(bone2->matrix[1], v->vertCoords) + bone2->matrix[1][3];
					tess.xyz[baseVertex][1] = fBoneWeight * (t1 - t2) + t2;
					t1 = DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3];
					t2 = DotProduct(bone2->matrix[2], v->vertCoords) + bone2->matrix[2][3];
					tess.xyz[baseVertex][2] = fBoneWeight * (t1 - t2) + t2;
				}
				else
				{
					tess.xyz[baseVertex][0] = fBoneWeight * (DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3]);
					tess.xyz[baseVertex][1] = fBoneWeight * (DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3]);
					tess.xyz[baseVertex][2] = fBoneWeight * (DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3]);

					fTotalWeight = fBoneWeight;
					for (k = 1; k < iNumWeights - 1; k++)
					{
#ifdef JK2_MODE
						bone = &bones->Eval(piBoneReferences[G2_GetVertBoneIndex(v, k)]);
#else
						bone = &bones->EvalRender(piBoneReferences[G2_GetVertBoneIndex(v, k)]);
#endif // JK2_MODE

						fBoneWeight = G2_GetVertBoneWeightNotSlow(v, k);
						fTotalWeight += fBoneWeight;

						tess.xyz[baseVertex][0] += fBoneWeight * (DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3]);
						tess.xyz[baseVertex][1] += fBoneWeight * (DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3]);
						tess.xyz[baseVertex][2] += fBoneWeight * (DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3]);
					}

#ifdef JK2_MODE
					bone = &bones->Eval(piBoneReferences[G2_GetVertBoneIndex(v, k)]);
#else
					bone = &bones->EvalRender(piBoneReferences[G2_GetVertBoneIndex(v, k)]);
#endif // JK2_MODE
					fBoneWeight = 1.0f - fTotalWeight;

					tess.xyz[baseVertex][0] += fBoneWeight * (DotProduct(bone->matrix[0], v->vertCoords) + bone->matrix[0][3]);
					tess.xyz[baseVertex][1] += fBoneWeight * (DotProduct(bone->matrix[1], v->vertCoords) + bone->matrix[1][3]);
					tess.xyz[baseVertex][2] += fBoneWeight * (DotProduct(bone->matrix[2], v->vertCoords) + bone->matrix[2][3]);
				}
			}

			tess.texCoords[baseVertex][0][0] = pTexCoords[j].texCoords[0];
			tess.texCoords[baseVertex][0][1] = pTexCoords[j].texCoords[1];
		}
#if 0
	}
#endif

#ifdef _G2_GORE
	while (surf->goreChain)
	{
		surf = static_cast<CRenderableSurface*>(surf->goreChain);
		if (surf->alternateTex)
		{
			// get a gore surface ready to go.

			/*
				sizeof(int)+ // num verts
				sizeof(int)+ // num tris
				sizeof(int)*newNumVerts+ // which verts to copy from original surface
				sizeof(float)*4*newNumVerts+ // storgage for deformed verts
				sizeof(float)*4*newNumVerts+ // storgage for deformed normal
				sizeof(float)*2*newNumVerts+ // texture coordinates
				sizeof(int)*newNumTris*3;  // new indecies
			*/

			int* data = reinterpret_cast<int*>(surf->alternateTex);
			int gnumVerts = *data++;
			data++;

			float* fdata = reinterpret_cast<float*>(data);
			fdata += gnumVerts;
			for (j = 0; j < gnumVerts; j++)
			{
				assert(data[j] >= 0 && data[j] < numVerts);
				memcpy(fdata, &tess.xyz[tess.numVertexes + data[j]][0], sizeof(float) * 3);
				fdata += 4;
			}
			for (j = 0; j < gnumVerts; j++)
			{
				assert(data[j] >= 0 && data[j] < numVerts);
				memcpy(fdata, &tess.normal[tess.numVertexes + data[j]][0], sizeof(float) * 3);
				fdata += 4;
			}
		}
		else
		{
			assert(0);
		}
	}

	// NOTE: This is required because a ghoul model might need to be rendered twice a frame (don't cringe,
	// it's not THAT bad), so we only delete it when doing the glow pass. Warning though, this assumes that
	// the glow is rendered _second_!!! If that changes, change this!
#endif
	tess.numVertexes += surface->numVerts;

#ifdef G2_PERFORMANCE_ANALYSIS
	G2Time_RB_SurfaceGhoul += G2PerformanceTimer_RB_SurfaceGhoul.End();
#endif
}

/*
=================
R_LoadMDXM - load a Ghoul 2 Mesh file
=================
*/
/*

Some information used in the creation of the JK2 - JKA bone remap table

These are the old bones:
Complete list of all 72 bones:

*/

int OldToNewRemapTable[72] = {
0,// Bone 0:   "model_root":           Parent: ""  (index -1)
1,// Bone 1:   "pelvis":               Parent: "model_root"  (index 0)
2,// Bone 2:   "Motion":               Parent: "pelvis"  (index 1)
3,// Bone 3:   "lfemurYZ":             Parent: "pelvis"  (index 1)
4,// Bone 4:   "lfemurX":              Parent: "pelvis"  (index 1)
5,// Bone 5:   "ltibia":               Parent: "pelvis"  (index 1)
6,// Bone 6:   "ltalus":               Parent: "pelvis"  (index 1)
6,// Bone 7:   "ltarsal":              Parent: "pelvis"  (index 1)
7,// Bone 8:   "rfemurYZ":             Parent: "pelvis"  (index 1)
8,// Bone 9:   "rfemurX":	            Parent: "pelvis"  (index 1)
9,// Bone10:   "rtibia":	            Parent: "pelvis"  (index 1)
10,// Bone11:   "rtalus":	            Parent: "pelvis"  (index 1)
10,// Bone12:   "rtarsal":              Parent: "pelvis"  (index 1)
11,// Bone13:   "lower_lumbar":         Parent: "pelvis"  (index 1)
12,// Bone14:   "upper_lumbar":	        Parent: "lower_lumbar"  (index 13)
13,// Bone15:   "thoracic":	            Parent: "upper_lumbar"  (index 14)
14,// Bone16:   "cervical":	            Parent: "thoracic"  (index 15)
15,// Bone17:   "cranium":              Parent: "cervical"  (index 16)
16,// Bone18:   "ceyebrow":	            Parent: "face_always_"  (index 71)
17,// Bone19:   "jaw":                  Parent: "face_always_"  (index 71)
18,// Bone20:   "lblip2":	            Parent: "face_always_"  (index 71)
19,// Bone21:   "leye":		            Parent: "face_always_"  (index 71)
20,// Bone22:   "rblip2":	            Parent: "face_always_"  (index 71)
21,// Bone23:   "ltlip2":               Parent: "face_always_"  (index 71)
22,// Bone24:   "rtlip2":	            Parent: "face_always_"  (index 71)
23,// Bone25:   "reye":		            Parent: "face_always_"  (index 71)
24,// Bone26:   "rclavical":            Parent: "thoracic"  (index 15)
25,// Bone27:   "rhumerus":             Parent: "thoracic"  (index 15)
26,// Bone28:   "rhumerusX":            Parent: "thoracic"  (index 15)
27,// Bone29:   "rradius":              Parent: "thoracic"  (index 15)
28,// Bone30:   "rradiusX":             Parent: "thoracic"  (index 15)
29,// Bone31:   "rhand":                Parent: "thoracic"  (index 15)
29,// Bone32:   "mc7":                  Parent: "thoracic"  (index 15)
34,// Bone33:   "r_d5_j1":              Parent: "thoracic"  (index 15)
35,// Bone34:   "r_d5_j2":              Parent: "thoracic"  (index 15)
35,// Bone35:   "r_d5_j3":              Parent: "thoracic"  (index 15)
30,// Bone36:   "r_d1_j1":              Parent: "thoracic"  (index 15)
31,// Bone37:   "r_d1_j2":              Parent: "thoracic"  (index 15)
31,// Bone38:   "r_d1_j3":              Parent: "thoracic"  (index 15)
32,// Bone39:   "r_d2_j1":              Parent: "thoracic"  (index 15)
33,// Bone40:   "r_d2_j2":              Parent: "thoracic"  (index 15)
33,// Bone41:   "r_d2_j3":              Parent: "thoracic"  (index 15)
32,// Bone42:   "r_d3_j1":			    Parent: "thoracic"  (index 15)
33,// Bone43:   "r_d3_j2":		        Parent: "thoracic"  (index 15)
33,// Bone44:   "r_d3_j3":              Parent: "thoracic"  (index 15)
34,// Bone45:   "r_d4_j1":              Parent: "thoracic"  (index 15)
35,// Bone46:   "r_d4_j2":	            Parent: "thoracic"  (index 15)
35,// Bone47:   "r_d4_j3":		        Parent: "thoracic"  (index 15)
36,// Bone48:   "rhang_tag_bone":	    Parent: "thoracic"  (index 15)
37,// Bone49:   "lclavical":            Parent: "thoracic"  (index 15)
38,// Bone50:   "lhumerus":	            Parent: "thoracic"  (index 15)
39,// Bone51:   "lhumerusX":	        Parent: "thoracic"  (index 15)
40,// Bone52:   "lradius":	            Parent: "thoracic"  (index 15)
41,// Bone53:   "lradiusX":	            Parent: "thoracic"  (index 15)
42,// Bone54:   "lhand":	            Parent: "thoracic"  (index 15)
42,// Bone55:   "mc5":		            Parent: "thoracic"  (index 15)
43,// Bone56:   "l_d5_j1":	            Parent: "thoracic"  (index 15)
44,// Bone57:   "l_d5_j2":	            Parent: "thoracic"  (index 15)
44,// Bone58:   "l_d5_j3":	            Parent: "thoracic"  (index 15)
43,// Bone59:   "l_d4_j1":	            Parent: "thoracic"  (index 15)
44,// Bone60:   "l_d4_j2":	            Parent: "thoracic"  (index 15)
44,// Bone61:   "l_d4_j3":	            Parent: "thoracic"  (index 15)
45,// Bone62:   "l_d3_j1":	            Parent: "thoracic"  (index 15)
46,// Bone63:   "l_d3_j2":	            Parent: "thoracic"  (index 15)
46,// Bone64:   "l_d3_j3":	            Parent: "thoracic"  (index 15)
45,// Bone65:   "l_d2_j1":	            Parent: "thoracic"  (index 15)
46,// Bone66:   "l_d2_j2":	            Parent: "thoracic"  (index 15)
46,// Bone67:   "l_d2_j3":	            Parent: "thoracic"  (index 15)
47,// Bone68:   "l_d1_j1":				Parent: "thoracic"  (index 15)
48,// Bone69:   "l_d1_j2":	            Parent: "thoracic"  (index 15)
48,// Bone70:   "l_d1_j3":				Parent: "thoracic"  (index 15)
52// Bone71:   "face_always_":			Parent: "cranium"  (index 17)
};

/*

Bone   0:   "model_root":
			Parent: ""  (index -1)
			#Kids:  1
			Child 0: (index 1), name "pelvis"

Bone   1:   "pelvis":
			Parent: "model_root"  (index 0)
			#Kids:  4
			Child 0: (index 2), name "Motion"
			Child 1: (index 3), name "lfemurYZ"
			Child 2: (index 7), name "rfemurYZ"
			Child 3: (index 11), name "lower_lumbar"

Bone   2:   "Motion":
			Parent: "pelvis"  (index 1)
			#Kids:  0

Bone   3:   "lfemurYZ":
			Parent: "pelvis"  (index 1)
			#Kids:  3
			Child 0: (index 4), name "lfemurX"
			Child 1: (index 5), name "ltibia"
			Child 2: (index 49), name "ltail"

Bone   4:   "lfemurX":
			Parent: "lfemurYZ"  (index 3)
			#Kids:  0

Bone   5:   "ltibia":
			Parent: "lfemurYZ"  (index 3)
			#Kids:  1
			Child 0: (index 6), name "ltalus"

Bone   6:   "ltalus":
			Parent: "ltibia"  (index 5)
			#Kids:  0

Bone   7:   "rfemurYZ":
			Parent: "pelvis"  (index 1)
			#Kids:  3
			Child 0: (index 8), name "rfemurX"
			Child 1: (index 9), name "rtibia"
			Child 2: (index 50), name "rtail"

Bone   8:   "rfemurX":
			Parent: "rfemurYZ"  (index 7)
			#Kids:  0

Bone   9:   "rtibia":
			Parent: "rfemurYZ"  (index 7)
			#Kids:  1
			Child 0: (index 10), name "rtalus"

Bone  10:   "rtalus":
			Parent: "rtibia"  (index 9)
			#Kids:  0

Bone  11:   "lower_lumbar":
			Parent: "pelvis"  (index 1)
			#Kids:  1
			Child 0: (index 12), name "upper_lumbar"

Bone  12:   "upper_lumbar":
			Parent: "lower_lumbar"  (index 11)
			#Kids:  1
			Child 0: (index 13), name "thoracic"

Bone  13:   "thoracic":
			Parent: "upper_lumbar"  (index 12)
			#Kids:  5
			Child 0: (index 14), name "cervical"
			Child 1: (index 24), name "rclavical"
			Child 2: (index 25), name "rhumerus"
			Child 3: (index 37), name "lclavical"
			Child 4: (index 38), name "lhumerus"

Bone  14:   "cervical":
			Parent: "thoracic"  (index 13)
			#Kids:  1
			Child 0: (index 15), name "cranium"

Bone  15:   "cranium":
			Parent: "cervical"  (index 14)
			#Kids:  1
			Child 0: (index 52), name "face_always_"

Bone  16:   "ceyebrow":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  17:   "jaw":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  18:   "lblip2":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  19:   "leye":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  20:   "rblip2":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  21:   "ltlip2":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  22:   "rtlip2":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  23:   "reye":
			Parent: "face_always_"  (index 52)
			#Kids:  0

Bone  24:   "rclavical":
			Parent: "thoracic"  (index 13)
			#Kids:  0

Bone  25:   "rhumerus":
			Parent: "thoracic"  (index 13)
			#Kids:  2
			Child 0: (index 26), name "rhumerusX"
			Child 1: (index 27), name "rradius"

Bone  26:   "rhumerusX":
			Parent: "rhumerus"  (index 25)
			#Kids:  0

Bone  27:   "rradius":
			Parent: "rhumerus"  (index 25)
			#Kids:  9
			Child 0: (index 28), name "rradiusX"
			Child 1: (index 29), name "rhand"
			Child 2: (index 30), name "r_d1_j1"
			Child 3: (index 31), name "r_d1_j2"
			Child 4: (index 32), name "r_d2_j1"
			Child 5: (index 33), name "r_d2_j2"
			Child 6: (index 34), name "r_d4_j1"
			Child 7: (index 35), name "r_d4_j2"
			Child 8: (index 36), name "rhang_tag_bone"

Bone  28:   "rradiusX":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  29:   "rhand":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  30:   "r_d1_j1":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  31:   "r_d1_j2":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  32:   "r_d2_j1":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  33:   "r_d2_j2":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  34:   "r_d4_j1":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  35:   "r_d4_j2":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  36:   "rhang_tag_bone":
			Parent: "rradius"  (index 27)
			#Kids:  0

Bone  37:   "lclavical":
			Parent: "thoracic"  (index 13)
			#Kids:  0

Bone  38:   "lhumerus":
			Parent: "thoracic"  (index 13)
			#Kids:  2
			Child 0: (index 39), name "lhumerusX"
			Child 1: (index 40), name "lradius"

Bone  39:   "lhumerusX":
			Parent: "lhumerus"  (index 38)
			#Kids:  0

Bone  40:   "lradius":
			Parent: "lhumerus"  (index 38)
			#Kids:  9
			Child 0: (index 41), name "lradiusX"
			Child 1: (index 42), name "lhand"
			Child 2: (index 43), name "l_d4_j1"
			Child 3: (index 44), name "l_d4_j2"
			Child 4: (index 45), name "l_d2_j1"
			Child 5: (index 46), name "l_d2_j2"
			Child 6: (index 47), name "l_d1_j1"
			Child 7: (index 48), name "l_d1_j2"
			Child 8: (index 51), name "lhang_tag_bone"

Bone  41:   "lradiusX":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  42:   "lhand":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  43:   "l_d4_j1":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  44:   "l_d4_j2":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  45:   "l_d2_j1":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  46:   "l_d2_j2":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  47:   "l_d1_j1":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  48:   "l_d1_j2":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  49:   "ltail":
			Parent: "lfemurYZ"  (index 3)
			#Kids:  0

Bone  50:   "rtail":
			Parent: "rfemurYZ"  (index 7)
			#Kids:  0

Bone  51:   "lhang_tag_bone":
			Parent: "lradius"  (index 40)
			#Kids:  0

Bone  52:   "face_always_":
			Parent: "cranium"  (index 15)
			#Kids:  8
			Child 0: (index 16), name "ceyebrow"
			Child 1: (index 17), name "jaw"
			Child 2: (index 18), name "lblip2"
			Child 3: (index 19), name "leye"
			Child 4: (index 20), name "rblip2"
			Child 5: (index 21), name "ltlip2"
			Child 6: (index 22), name "rtlip2"
			Child 7: (index 23), name "reye"

*/

qboolean R_LoadMDXM(model_t* mod, void* buffer, const char* mod_name, qboolean& b_already_cached) {
	int					i, j;
	mdxmHeader_t* pinmodel, * mdxm;
	mdxmLOD_t* lod;
	mdxmSurface_t* surf;
	int					version;
	int					size;
	shader_t* sh;
	mdxmSurfHierarchy_t* surfInfo;

#ifdef Q3_BIG_ENDIAN
	int					k;
	mdxmTriangle_t* tri;
	mdxmVertex_t* v;
	int* boneRef;
	mdxmLODSurfOffset_t* indexes;
	mdxmVertexTexCoord_t* pTexCoords;
	mdxmHierarchyOffsets_t* surf_indexes;
#endif

	pinmodel = static_cast<mdxmHeader_t*>(buffer);
	//
	// read some fields from the binary, but only LittleLong() them when we know this wasn't an already-cached model...
	//
	version = pinmodel->version;
	size = pinmodel->ofsEnd;

	if (!b_already_cached)
	{
		LL(version);
		LL(size);
	}

	if (version != MDXM_VERSION) {
#ifdef _DEBUG
		Com_Error(ERR_DROP, "R_LoadMDXM: %s has wrong version (%i should be %i)\n", mod_name, version, MDXM_VERSION);
#else
		ri.Printf(PRINT_WARNING, "R_LoadMDXM: %s has wrong version (%i should be %i)\n", mod_name, version, MDXM_VERSION);
#endif
	}

	mod->type = MOD_MDXM;
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;
	mdxm = mod->mdxm = static_cast<mdxmHeader_t*>(RE_RegisterModels_Malloc(size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_GLM));

	assert(b_already_cached == bAlreadyFound);

	if (!bAlreadyFound)
	{
		// horrible new hackery, if !bAlreadyFound then we've just done a tag-morph, so we need to set the
		//	bool reference passed into this function to true, to tell the caller NOT to do an FS_Freefile since
		//	we've hijacked that memory block...
		//
		// Aaaargh. Kill me now...
		//
		b_already_cached = qtrue;
		assert(mdxm == buffer);
		//		memcpy( mdxm, buffer, size );	// and don't do this now, since it's the same thing

		LL(mdxm->ident);
		LL(mdxm->version);
		LL(mdxm->numBones);
		LL(mdxm->numLODs);
		LL(mdxm->ofsLODs);
		LL(mdxm->numSurfaces);
		LL(mdxm->ofsSurfHierarchy);
		LL(mdxm->ofsEnd);
	}

	// first up, go load in the animation file we need that has the skeletal animation info for this model
	mdxm->animIndex = RE_RegisterModel(va("%s.gla", mdxm->animName));

	const char* mapname = sv_mapname->string;

	if (strcmp(mapname, "nomap") != 0)
	{
		char animGLAName[MAX_QPATH];
		if (strrchr(mapname, '/'))	//maps in subfolders use the root name, ( presuming only one level deep!)
		{
			mapname = strrchr(mapname, '/') + 1;
		}
		//stripped name of GLA for this model
		Q_strncpyz(animGLAName, mdxm->animName, sizeof animGLAName);
		char* slash = strrchr(animGLAName, '/');
		if (slash)
		{
			*slash = 0;
		}
		char* strippedName = COM_SkipPath(animGLAName);
		if (VALIDSTRING(strippedName))
		{
			RE_RegisterModel(va("models/players/%s_%s/%s_%s.gla", strippedName, mapname, strippedName, mapname));
		}
	}

#ifndef JK2_MODE
	bool isAnOldModelFile = false;
	if (mdxm->numBones == 72 && strstr(mdxm->animName, "_humanoid"))
	{
		isAnOldModelFile = true;
	}
#endif

	if (!mdxm->animIndex)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDXM: missing animation file %s for mesh %s\n", mdxm->animName, mdxm->name);
		return qfalse;
	}
#ifndef JK2_MODE
	else
	{
		// let us mix JK2/JKA models and animations
		if (tr.models[mdxm->animIndex]->mdxa->numBones != 53 && tr.models[mdxm->animIndex]->mdxa->numBones != 72 && mdxm->numBones != 53 &&
			mdxm->numBones != 72)
		{
			assert(tr.models[mdxm->animIndex]->mdxa->numBones == mdxm->numBones);
		}
		if (tr.models[mdxm->animIndex]->mdxa->numBones != mdxm->numBones)
		{
			if (isAnOldModelFile)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDXM: converting jk2 model %s\n", mod_name);
			}
			else
			{
#ifdef _DEBUG
				Com_Error(ERR_DROP, "R_LoadMDXM: %s has different bones than anim (%i != %i)\n", mod_name, mdxm->numBones, tr.models[mdxm->animIndex]->mdxa->numBones);
#else
				ri.Printf(PRINT_WARNING, "R_LoadMDXM: %s has different bones than anim (%i != %i)\n", mod_name, mdxm->numBones, tr.models[mdxm->animIndex]->mdxa->numBones);
#endif
			}
			if (!isAnOldModelFile)
			{//hmm, load up the old JK2 ones anyway?
				return qfalse;
			}
		}
	}
#endif

	mod->numLods = mdxm->numLODs - 1;	//copy this up to the model for ease of use - it wil get inced after this.

	if (bAlreadyFound)
	{
		return qtrue;	// All done. Stop, go no further, do not LittleLong(), do not pass Go...
	}

	surfInfo = reinterpret_cast<mdxmSurfHierarchy_t*>(reinterpret_cast<byte*>(mdxm) + mdxm->ofsSurfHierarchy);
#ifdef Q3_BIG_ENDIAN
	surf_indexes = (mdxmHierarchyOffsets_t*)((byte*)mdxm + sizeof(mdxmHeader_t));
#endif
	for (i = 0; i < mdxm->numSurfaces; i++)
	{
		LL(surfInfo->flags);
		LL(surfInfo->numChildren);
		LL(surfInfo->parentIndex);

#ifndef JK2_MODE
		Q_strlwr(surfInfo->name);	//just in case
		if (strcmp(&surfInfo->name[strlen(surfInfo->name) - 4], "_off") == 0)
		{
			surfInfo->name[strlen(surfInfo->name) - 4] = 0;	//remove "_off" from name
		}
#endif

		if (surfInfo->shader[0] == '[')
		{
			surfInfo->shader[0] = 0;	//kill the stupid [nomaterial] since carcass doesn't
		}

		// do all the children indexs
		for (j = 0; j < surfInfo->numChildren; j++)
		{
			LL(surfInfo->childIndexes[j]);
		}

		// get the shader name
		sh = R_FindShader(surfInfo->shader, lightmapsNone, stylesDefault, qtrue);
		// insert it in the surface list

		if (!sh)
		{
			surfInfo = reinterpret_cast<mdxmSurfHierarchy_t*>(reinterpret_cast<byte*>(surfInfo) + reinterpret_cast<intptr_t>(&static_cast<mdxmSurfHierarchy_t*>(nullptr)->
				childIndexes[surfInfo->numChildren]));
			continue;
		}

		if (!sh->defaultShader)
		{
			surfInfo->shaderIndex = sh->index;
		}

		if (surfInfo->shaderIndex)
		{
			RE_RegisterModels_StoreShaderRequest(mod_name, &surfInfo->shader[0], &surfInfo->shaderIndex);
		}

#ifdef Q3_BIG_ENDIAN
		// swap the surface offset
		LL(surf_indexes->offsets[i]);
		assert(surfInfo == (mdxmSurfHierarchy_t*)((byte*)surf_indexes + surf_indexes->offsets[i]));
#endif

		// find the next surface
		surfInfo = reinterpret_cast<mdxmSurfHierarchy_t*>(reinterpret_cast<byte*>(surfInfo) + reinterpret_cast<intptr_t>(&static_cast<mdxmSurfHierarchy_t*>(nullptr)->
			childIndexes[surfInfo->numChildren]));
	}

	// swap all the LOD's	(we need to do the middle part of this even for intel, because of shader reg and err-check)
	lod = reinterpret_cast<mdxmLOD_t*>(reinterpret_cast<byte*>(mdxm) + mdxm->ofsLODs);
	for (int l = 0; l < mdxm->numLODs; l++)
	{
		int	triCount = 0;

		LL(lod->ofsEnd);
		// swap all the surfaces
		surf = reinterpret_cast<mdxmSurface_t*>(reinterpret_cast<byte*>(lod) + sizeof(mdxmLOD_t) + mdxm->numSurfaces * sizeof(mdxmLODSurfOffset_t));
		for (i = 0; i < mdxm->numSurfaces; i++)
		{
			LL(surf->thisSurfaceIndex);
			LL(surf->ofsHeader);
			LL(surf->numVerts);
			LL(surf->ofsVerts);
			LL(surf->numTriangles);
			LL(surf->ofsTriangles);
			LL(surf->numBoneReferences);
			LL(surf->ofsBoneReferences);
			LL(surf->ofsEnd);

			triCount += surf->numTriangles;

			if (surf->numVerts > SHADER_MAX_VERTEXES) {
				Com_Error(ERR_DROP, "R_LoadMDXM: %s has more than %i verts on a surface (%i)",
					mod_name, SHADER_MAX_VERTEXES, surf->numVerts);
			}
			if (surf->numTriangles * 3 > SHADER_MAX_INDEXES) {
				Com_Error(ERR_DROP, "R_LoadMDXM: %s has more than %i triangles on a surface (%i)",
					mod_name, SHADER_MAX_INDEXES / 3, surf->numTriangles);
			}

			// change to surface identifier
			surf->ident = SF_MDX;

			// register the shaders
#ifdef Q3_BIG_ENDIAN
			// swap the LOD offset
			indexes = (mdxmLODSurfOffset_t*)((byte*)lod + sizeof(mdxmLOD_t));
			LL(indexes->offsets[surf->thisSurfaceIndex]);

			// do all the bone reference data
			boneRef = (int*)((byte*)surf + surf->ofsBoneReferences);
			for (j = 0; j < surf->numBoneReferences; j++)
			{
				LL(boneRef[j]);
			}

			// swap all the triangles
			tri = (mdxmTriangle_t*)((byte*)surf + surf->ofsTriangles);
			for (j = 0; j < surf->numTriangles; j++, tri++)
			{
				LL(tri->indexes[0]);
				LL(tri->indexes[1]);
				LL(tri->indexes[2]);
			}

			// swap all the vertexes
			v = (mdxmVertex_t*)((byte*)surf + surf->ofsVerts);
			pTexCoords = (mdxmVertexTexCoord_t*)&v[surf->numVerts];

			for (j = 0; j < surf->numVerts; j++)
			{
				LF(v->normal[0]);
				LF(v->normal[1]);
				LF(v->normal[2]);

				LF(v->vertCoords[0]);
				LF(v->vertCoords[1]);
				LF(v->vertCoords[2]);

				LF(pTexCoords[j].texCoords[0]);
				LF(pTexCoords[j].texCoords[1]);

				LL(v->uiNmWeightsAndBoneIndexes);

				v++;
			}
#endif

#ifndef JK2_MODE
			if (isAnOldModelFile)
			{
				int* boneRef = reinterpret_cast<int*>(reinterpret_cast<byte*>(surf) + surf->ofsBoneReferences);
				for (j = 0; j < surf->numBoneReferences; j++)
				{
					assert(boneRef[j] >= 0 && boneRef[j] < 72);
					if (boneRef[j] >= 0 && boneRef[j] < 72)
					{
						boneRef[j] = OldToNewRemapTable[boneRef[j]];
					}
					else
					{
						boneRef[j] = 0;
					}
				}
			}
#endif
			// find the next surface
			surf = reinterpret_cast<mdxmSurface_t*>(reinterpret_cast<byte*>(surf) + surf->ofsEnd);
		}
		// find the next LOD
		lod = reinterpret_cast<mdxmLOD_t*>(reinterpret_cast<byte*>(lod) + lod->ofsEnd);
	}

	return qtrue;
}

/*
=================
R_LoadMDXA - load a Ghoul 2 animation file
=================
*/
qboolean R_LoadMDXA(model_t* mod, void* buffer, const char* mod_name, qboolean& b_already_cached) {
	mdxaHeader_t* pinmodel, * mdxa;
	int					version;
	int					size;

#ifdef Q3_BIG_ENDIAN
	int					j, k, i;
	mdxaSkel_t* boneInfo;
	mdxaSkelOffsets_t* offsets;
	int					maxBoneIndex = 0;
	mdxaCompQuatBone_t* pCompBonePool;
	unsigned short* pwIn;
	mdxaIndex_t* pIndex;
#endif

	pinmodel = static_cast<mdxaHeader_t*>(buffer);
	//
	// read some fields from the binary, but only LittleLong() them when we know this wasn't an already-cached model...
	//
	version = pinmodel->version;
	size = pinmodel->ofsEnd;

	if (!b_already_cached)
	{
		LL(version);
		LL(size);
	}

	if (version != MDXA_VERSION) {
		ri.Printf(PRINT_WARNING, "R_LoadMDXA: %s has wrong version (%i should be %i)\n",
			mod_name, version, MDXA_VERSION);
		return qfalse;
	}

	mod->type = MOD_MDXA;
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;
	mdxa = mod->mdxa = static_cast<mdxaHeader_t*>(RE_RegisterModels_Malloc(size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_GLA));

	assert(b_already_cached == bAlreadyFound);

	if (!bAlreadyFound)
	{
		// horrible new hackery, if !bAlreadyFound then we've just done a tag-morph, so we need to set the
		//	bool reference passed into this function to true, to tell the caller NOT to do an FS_Freefile since
		//	we've hijacked that memory block...
		//
		// Aaaargh. Kill me now...
		//
		b_already_cached = qtrue;
		assert(mdxa == buffer);
		//		memcpy( mdxa, buffer, size );	// and don't do this now, since it's the same thing

		LL(mdxa->ident);
		LL(mdxa->version);
		//LF(mdxa->fScale);
		LL(mdxa->numFrames);
		LL(mdxa->ofsFrames);
		LL(mdxa->numBones);
		LL(mdxa->ofsCompBonePool);
		LL(mdxa->ofsSkel);
		LL(mdxa->ofsEnd);
	}

	if (mdxa->numFrames < 1) {
		ri.Printf(PRINT_WARNING, "R_LoadMDXA: %s has no frames\n", mod_name);
		return qfalse;
	}

	if (bAlreadyFound)
	{
		return qtrue;	// All done, stop here, do not LittleLong() etc. Do not pass go...
	}

#ifdef Q3_BIG_ENDIAN
	// swap the bone info
	offsets = (mdxaSkelOffsets_t*)((byte*)mdxa + sizeof(mdxaHeader_t));
	for (i = 0; i < mdxa->numBones; i++)
	{
		LL(offsets->offsets[i]);
		boneInfo = (mdxaSkel_t*)((byte*)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[i]);
		LL(boneInfo->flags);
		LL(boneInfo->parent);
		for (j = 0; j < 3; j++)
		{
			for (k = 0; k < 4; k++)
			{
				LF(boneInfo->BasePoseMat.matrix[j][k]);
				LF(boneInfo->BasePoseMatInv.matrix[j][k]);
			}
		}
		LL(boneInfo->numChildren);

		for (k = 0; k < boneInfo->numChildren; k++)
		{
			LL(boneInfo->children[k]);
		}
	}

	// find the largest index, since the actual number of compressed bone pools is not stored anywhere
	for (i = 0; i < mdxa->numFrames; i++)
	{
		for (j = 0; j < mdxa->numBones; j++)
		{
			k = (i * mdxa->numBones * 3) + (j * 3); // iOffsetToIndex
			pIndex = (mdxaIndex_t*)((byte*)mdxa + mdxa->ofsFrames + k);

			// 3 byte ints, yeah...
			int tmp = pIndex->iIndex & 0xFFFFFF00;
			LL(tmp);

			if (maxBoneIndex < tmp)
				maxBoneIndex = tmp;
		}
	}

	// swap the compressed bones
	pCompBonePool = (mdxaCompQuatBone_t*)((byte*)mdxa + mdxa->ofsCompBonePool);
	for (i = 0; i <= maxBoneIndex; i++)
	{
		pwIn = (unsigned short*)pCompBonePool[i].Comp;

		for (k = 0; k < 7; k++)
			LS(pwIn[k]);
	}
#endif
	return qtrue;
}