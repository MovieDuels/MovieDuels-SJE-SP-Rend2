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

#pragma once

// defines to setup the

#include "ghoul2/ghoul2_shared.h"

//rww - RAGDOLL_BEGIN
class CRagDollUpdateParams;
//rww - RAGDOLL_END

class IHeapAllocator;

#define		GHOUL2_CRAZY_SMOOTH						0x2000		//hack for smoothing during ugly situations. forgive me.

class IGhoul2InfoArray
{
public:
	virtual ~IGhoul2InfoArray() {}

	virtual int New() = 0;
	virtual void Delete(int handle) = 0;
	virtual bool IsValid(int handle) const = 0;
	virtual std::vector<CGhoul2Info>& Get(int handle) = 0;
	virtual const std::vector<CGhoul2Info>& Get(int handle) const = 0;
};

IGhoul2InfoArray& TheGhoul2InfoArray();
class CGhoul2Info_v
{
	static IGhoul2InfoArray& InfoArray()
	{
		return TheGhoul2InfoArray();
	}

	void Alloc()
	{
		assert(!mItem); //already alloced
		mItem = InfoArray().New();
		assert(!Array().size());
	}
	void Free()
	{
		if (mItem)
		{
			assert(InfoArray().IsValid(mItem));
			InfoArray().Delete(mItem);
			mItem = 0;
		}
	}
	std::vector<CGhoul2Info>& Array()
	{
		assert(InfoArray().IsValid(mItem));
		return InfoArray().Get(mItem);
	}
	const std::vector<CGhoul2Info>& Array() const
	{
		assert(InfoArray().IsValid(mItem));
		return InfoArray().Get(mItem);
	}
public:
	int mItem;	//dont' be bad and muck with this
	CGhoul2Info_v()
	{
		mItem = 0;
	}
	CGhoul2Info_v(const int item)
	{	//be VERY carefull with what you pass in here
		mItem = item;
	}
	~CGhoul2Info_v()
	{
		Free(); //this had better be taken care of via the clean ghoul2 models call
	}
	void operator=(const CGhoul2Info_v& other)
	{
		mItem = other.mItem;
	}
	void operator=(const int otherItem)	//assigning one from the VM side item number
	{
		mItem = otherItem;
	}
	void DeepCopy(const CGhoul2Info_v& other)
	{
		Free();
		if (other.mItem)
		{
			Alloc();
			Array() = other.Array();
			for (int i = 0; i < size(); i++)
			{
				Array()[i].mBoneCache = nullptr;
				Array()[i].mTransformedVertsArray = nullptr;
				Array()[i].mSkelFrameNum = 0;
				Array()[i].mMeshFrameNum = 0;
			}
		}
	}
	CGhoul2Info& operator[](int idx)
	{
		assert(mItem);
		assert(idx >= 0 && idx < size());

		return Array()[idx];
	}
	const CGhoul2Info& operator[](int idx) const
	{
		assert(mItem);
		assert(idx >= 0 && idx < size());

		return Array()[idx];
	}
	void resize(int num)
	{
		assert(num >= 0);
		if (num)
		{
			if (!mItem)
			{
				Alloc();
			}
		}
		if (mItem || num)
		{
			Array().resize(num);
		}
	}
	void clear()
	{
		Free();
	}
	void push_back(const CGhoul2Info& model)
	{
		if (!mItem)
		{
			Alloc();
		}
		Array().push_back(model);
	}
	int size() const
	{
		if (!IsValid())
		{
			return 0;
		}
		return Array().size();
	}
	bool IsValid() const
	{
		return InfoArray().IsValid(mItem);
	}
	void kill()
	{
		// this scary method zeros the infovector handle without actually freeing it
		// it is used for some places where a copy is made, but we don't want to go through the trouble
		// of making a deep copy
		mItem = 0;
	}
};

void Create_Matrix(const float* angle, mdxaBone_t* matrix);

extern mdxaBone_t		worldMatrix;
extern mdxaBone_t		worldMatrixInv;

// internal surface calls  G2_surfaces.cpp
qboolean	G2_SetSurfaceOnOff(CGhoul2Info* ghl_info, surfaceInfo_v& slist, const char* surface_name, int off_flags);
int			G2_IsSurfaceOff(CGhoul2Info* ghl_info, surfaceInfo_v& slist, const char* surface_name);
qboolean	G2_SetRootSurface(CGhoul2Info_v& ghoul2, int model_index, const char* surface_name);
int			G2_AddSurface(CGhoul2Info* ghoul2, int surface_number, int poly_number, float barycentric_i, float barycentric_j, int lod);
qboolean	G2_RemoveSurface(surfaceInfo_v& slist, int index);
surfaceInfo_t* G2_FindOverrideSurface(int surface_num, surfaceInfo_v& surfaceList);
int			G2_IsSurfaceLegal(void* mod, const char* surface_name, int* flags);
int			G2_GetParentSurface(const CGhoul2Info* ghl_info, int index);
int			G2_GetSurfaceIndex(const CGhoul2Info* ghl_info, const char* surface_name);
int			G2_IsSurfaceRendered(const CGhoul2Info* ghl_info, const char* surface_name, const surfaceInfo_v& slist);

// internal bone calls - G2_Bones.cpp
qboolean	G2_Set_Bone_Angles(CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, const float* angles, int flags, Eorientations up, Eorientations left, Eorientations forward, qhandle_t* model_list, int model_index, int blend_time, int current_time);
qboolean	G2_Remove_Bone(CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name);
qboolean	G2_Set_Bone_Anim(CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, int start_frame, int end_frame, int flags, float anim_speed, int current_time, float set_frame, int blend_time);
qboolean	G2_Get_Bone_Anim(CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, int current_time, float* current_frame, int* start_frame, int* end_frame, int* flags, float* retAnimSpeed, qhandle_t* model_list, int model_index);
qboolean	G2_Get_Bone_Anim_Range(CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, int* start_frame, int* end_frame);
qboolean	G2_Pause_Bone_Anim(CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, int current_time);
qboolean	G2_IsPaused(const char* fileName, boneInfo_v& blist, const char* bone_name);
qboolean	G2_Stop_Bone_Anim(const char* fileName, boneInfo_v& blist, const char* bone_name);
qboolean	G2_Stop_Bone_Angles(const char* fileName, boneInfo_v& blist, const char* bone_name);
//rww - RAGDOLL_BEGIN
void		G2_Animate_Bone_List(CGhoul2Info_v& ghoul2, int current_time, int index, CRagDollUpdateParams* params);
//rww - RAGDOLL_END
void		G2_Init_Bone_List(boneInfo_v& blist, int num_bones);
int			G2_Find_Bone_In_List(const boneInfo_v& blist, int bone_num);
void		G2_RemoveRedundantBoneOverrides(boneInfo_v& blist, int* activeBones);
qboolean	G2_Set_Bone_Angles_Matrix(const char* fileName, boneInfo_v& blist, const char* bone_name, const mdxaBone_t& matrix, int flags, qhandle_t* model_list, int model_index, int blend_time, int current_time);
int			G2_Get_Bone_Index(CGhoul2Info* ghoul2, const char* bone_name);
qboolean	G2_Set_Bone_Angles_Index(boneInfo_v& blist, int index, const float* angles, int flags, Eorientations yaw, Eorientations pitch, Eorientations roll, qhandle_t* model_list, int model_index, int blend_time, int current_time);
qboolean	G2_Set_Bone_Angles_Matrix_Index(boneInfo_v& blist, int index, const mdxaBone_t& matrix, int flags, qhandle_t* model_list, int model_index, int blend_time, int current_time);
qboolean	G2_Stop_Bone_Anim_Index(boneInfo_v& blist, int index);
qboolean	G2_Stop_Bone_Angles_Index(boneInfo_v& blist, int index);
qboolean	G2_Set_Bone_Anim_Index(boneInfo_v& blist, int index, int start_frame, int end_frame, int flags, float anim_speed, int current_time, float set_frame, int ablend_time, int num_frames);
qboolean	G2_Get_Bone_Anim_Index(boneInfo_v& blist, int index, int current_time, float* current_frame, int* start_frame, int* end_frame, int* flags, float* retAnimSpeed, qhandle_t* model_list, int num_frames);

// misc functions G2_misc.cpp
void		G2_List_Model_Surfaces(const char* fileName);
void		G2_List_Model_Bones(const char* file_name);
qboolean	G2_GetAnimFileName(const char* file_name, char** filename);
#ifdef _G2_GORE
void		G2_TraceModels(CGhoul2Info_v& ghoul2, vec3_t rayStart, vec3_t rayEnd, CollisionRecord_t* collRecMap, int ent_num, int e_g2_trace_type, int use_lod, float fRadius, float ssize, float tsize, float theta, int shader, SSkinGoreData* gore, qboolean skipIfLODNotMatch);
#else
void		G2_TraceModels(CGhoul2Info_v& ghoul2, vec3_t rayStart, vec3_t rayEnd, CollisionRecord_t* collRecMap, int ent_num, int traceFlags, int use_lod, float fRadius);
#endif
void		TransformAndTranslatePoint(const vec3_t in, vec3_t out, mdxaBone_t* mat);
#ifdef _G2_GORE
void		G2_TransformModel(CGhoul2Info_v& ghoul2, int frame_num, vec3_t scale, IHeapAllocator* g2_vert_space, int use_lod, bool ApplyGore);
#else
void		G2_TransformModel(CGhoul2Info_v& ghoul2, const int frame_num, vec3_t scale, IHeapAllocator* g2_vert_space, int use_lod);
#endif
void		G2_GenerateWorldMatrix(const vec3_t angles, const vec3_t origin);
void		TransformPoint(const vec3_t in, vec3_t out, mdxaBone_t* mat);
void		Inverse_Matrix(mdxaBone_t* src, mdxaBone_t* dest);
void* G2_FindSurface(void* mod_t, int index, int lod);
qboolean	G2_SaveGhoul2Models(CGhoul2Info_v& ghoul2, char** buffer, int* size);
void		G2_LoadGhoul2Model(CGhoul2Info_v& ghoul2, const char* buffer);

// internal bolt calls. G2_bolts.cpp
int			G2_Add_Bolt(const CGhoul2Info* ghl_info, boltInfo_v& bltlist, const char* bone_name);
qboolean	G2_Remove_Bolt(boltInfo_v& bltlist, int index);
void		G2_Init_Bolt_List(boltInfo_v& bltlist);
int			G2_Find_Bolt_Bone_Num(const boltInfo_v& bltlist, int bone_num);
int			G2_Find_Bolt_Surface_Num(boltInfo_v& bltlist, int surface_num, int flags);
int			G2_Add_Bolt_Surf_Num(const CGhoul2Info* ghl_info, boltInfo_v& bltlist, const surfaceInfo_v& slist, int surf_num);
void		G2_RemoveRedundantBolts(boltInfo_v& bltlist, surfaceInfo_v& slist, int* activeSurfaces, int* activeBones);

// API calls - G2_API.cpp
void		RestoreGhoul2InfoArray();
void		SaveGhoul2InfoArray();

void		G2API_SetTime(int current_time, int clock);
int			G2API_GetTime(int arg_time);

qhandle_t	G2API_PrecacheGhoul2Model(const char* file_name);

qboolean	G2API_IsGhoul2InfovValid(CGhoul2Info_v& ghoul2);
int			G2API_InitGhoul2Model(CGhoul2Info_v** ghoul2Ptr, const char* fileName, int model_index, qhandle_t customSkin = NULL_HANDLE, qhandle_t customShader = NULL_HANDLE, int modelFlags = 0, int lodBias = 0);
qboolean	G2API_SetLodBias(CGhoul2Info* ghl_info, int lod_bias);
qboolean	G2API_SetSkin(CGhoul2Info_v& ghoul2, int model_index, qhandle_t customSkin, qhandle_t renderSkin);
qboolean	G2API_SetShader(CGhoul2Info* ghl_info, qhandle_t custom_shader);
qboolean	G2API_HasGhoul2ModelOnIndex(CGhoul2Info_v** ghlRemove, int model_index);
qboolean	G2API_RemoveGhoul2Model(CGhoul2Info_v** ghlRemove, int model_index);
qboolean	G2API_RemoveGhoul2Models(CGhoul2Info_v** ghlRemove);
qboolean	G2API_SetSurfaceOnOff(CGhoul2Info_v& ghoul2, const char* surface_name, int flags);
int			G2API_GetSurfaceOnOff(CGhoul2Info* ghl_info, const char* surface_name);
qboolean	G2API_SetRootSurface(CGhoul2Info_v& ghoul2, int model_index, const char* surface_name);
qboolean	G2API_RemoveSurface(CGhoul2Info* ghl_info, int index);
int			G2API_AddSurface(CGhoul2Info* ghl_info, int surface_number, int poly_number, float barycentric_i, float barycentric_j, int lod);
qboolean	G2API_SetBoneAnim(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name, int astart_frame, int aend_frame, int flags, float anim_speed, int current_time, float aset_frame = -1, int blend_time = -1);
qboolean	G2API_GetBoneAnim(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name, int current_time, float* current_frame, int* start_frame, int* end_frame, int* flags, float* anim_speed, qhandle_t* model_list);
qboolean	G2API_GetAnimRange(CGhoul2Info* ghl_info, const char* bone_name, int* start_frame, int* end_frame);
qboolean	G2API_PauseBoneAnim(CGhoul2Info* ghl_info, const char* bone_name, int acurrent_time);
qboolean	G2API_IsPaused(CGhoul2Info* ghl_info, const char* bone_name);
qboolean	G2API_StopBoneAnim(CGhoul2Info* ghl_info, const char* bone_name);

qboolean G2API_SetBoneAngles(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name, const vec3_t angles, int flags, Eorientations up, Eorientations left, Eorientations forward, qhandle_t* model_list, int blend_time, int current_time);

qboolean	G2API_StopBoneAngles(CGhoul2Info* ghl_info, const char* bone_name);
qboolean	G2API_RemoveBone(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name);
void		G2API_AnimateG2Models(CGhoul2Info_v& ghoul2, float speedVar);
qboolean	G2API_RemoveBolt(CGhoul2Info* ghl_info, int index);
int			G2API_AddBolt(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name);
int			G2API_AddBoltSurfNum(CGhoul2Info* ghl_info, int surf_index);
void		G2API_SetBoltInfo(CGhoul2Info_v& ghoul2, int model_index, int bolt_info);
qboolean	G2API_AttachG2Model(CGhoul2Info_v& ghoul2_from, int modelFrom, CGhoul2Info_v& ghoul2To, int toBoltIndex, int toModel);
qboolean	G2API_DetachG2Model(CGhoul2Info* ghl_info);
qboolean	G2API_AttachEnt(int* bolt_info, CGhoul2Info_v& ghoul2, int model_index, int toBoltIndex, int ent_num, int toModelNum);
void		G2API_DetachEnt(int* bolt_info);

qboolean	G2API_GetBoltMatrix(CGhoul2Info_v& ghoul2, int model_index, int bolt_index, mdxaBone_t* matrix, const vec3_t angles, const vec3_t position, int frame_num, qhandle_t* model_list, vec3_t scale);

void		G2API_ListSurfaces(CGhoul2Info* ghl_info);
void		G2API_ListBones(CGhoul2Info* ghl_info, int frame);
qboolean	G2API_HaveWeGhoul2Models(CGhoul2Info_v& ghoul2);
void		G2API_SetGhoul2ModelIndexes(CGhoul2Info_v& ghoul2, qhandle_t* model_list, qhandle_t* skin_list);
qboolean	G2API_SetGhoul2ModelFlags(CGhoul2Info* ghl_info, int flags);
int			G2API_GetGhoul2ModelFlags(CGhoul2Info* ghl_info);

qboolean	G2API_GetAnimFileName(CGhoul2Info* ghl_info, char** filename);
void		G2API_CollisionDetect(CollisionRecord_t* collRecMap, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position, int frameNumber, int ent_num, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, IHeapAllocator* g2_vert_space, int traceFlags, int use_lod, float fRadius);
void		G2API_CollisionDetectCache(CollisionRecord_t* collRecMap, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position, int frameNumber, int ent_num, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, IHeapAllocator* g2_vert_space, int traceFlags, int use_lod, float fRadius);

void		G2API_GiveMeVectorFromMatrix(mdxaBone_t* bolt_matrix, Eorientations flags, vec3_t vec);
int			G2API_CopyGhoul2Instance(CGhoul2Info_v& g2From, CGhoul2Info_v& g2To, int model_index);
void		G2API_CleanGhoul2Models(CGhoul2Info_v** ghoul2Ptr);
int			G2API_GetParentSurface(CGhoul2Info* ghl_info, int index);
int			G2API_GetSurfaceIndex(CGhoul2Info* ghl_info, const char* surface_name);
char* G2API_GetSurfaceName(CGhoul2Info_v& ghoul2, int model_index, int surf_number);
char* G2API_GetGLAName(CGhoul2Info_v& ghoul2, int model_index);
qboolean	G2API_SetBoneAnglesMatrix(CGhoul2Info* ghl_info, const char* bone_name, const mdxaBone_t& matrix, int flags, qhandle_t* model_list, int blend_time = 0, int current_time = 0);
qboolean	G2API_SetNewOrigin(CGhoul2Info_v& ghoul2, int bolt_index);
int			G2API_GetBoneIndex(CGhoul2Info* ghl_info, const char* bone_name);
qboolean	G2API_StopBoneAnglesIndex(CGhoul2Info* ghl_info, int index);
qboolean	G2API_StopBoneAnimIndex(CGhoul2Info* ghl_info, int index);
qboolean	G2API_SetBoneAnglesIndex(CGhoul2Info* ghl_info, int index, const vec3_t angles, int flags, Eorientations yaw, Eorientations pitch, Eorientations roll, qhandle_t* model_list, int blend_time, int current_time);
qboolean	G2API_SetBoneAnglesMatrixIndex(CGhoul2Info* ghl_info, int index, const mdxaBone_t& matrix, int flags, qhandle_t* model_list, int blend_time, int current_time);
qboolean	G2API_DoesBoneExist(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name);
qboolean	G2API_SetBoneAnimIndex(CGhoul2Info* ghl_info, int index, int astart_frame, int aend_frame, int flags, float anim_speed, int acurrent_time, float aset_frame, int blend_time);
qboolean	G2API_SaveGhoul2Models(CGhoul2Info_v& ghoul2, char** buffer, int* size);
void		G2API_LoadGhoul2Models(CGhoul2Info_v& ghoul2, char* buffer);
void		G2API_LoadSaveCodeDestructGhoul2Info(CGhoul2Info_v& ghoul2);
void		G2API_FreeSaveBuffer(char* buffer);
char* G2API_GetAnimFileNameIndex(qhandle_t model_index);
int			G2API_GetSurfaceRenderStatus(CGhoul2Info_v& ghoul2, int model_index, const char* surface_name);
void		G2API_CopySpecificG2Model(CGhoul2Info_v& ghoul2_from, int modelFrom, CGhoul2Info_v& ghoul2To, int modelTo);
void		G2API_DuplicateGhoul2Instance(CGhoul2Info_v& g2From, CGhoul2Info_v** g2To);
void		G2API_SetBoltInfo(CGhoul2Info_v& ghoul2, int model_index, int bolt_info);

class CRagDollUpdateParams;
class CRagDollParams;

void		G2API_AbsurdSmoothing(CGhoul2Info_v& ghoul2, qboolean status);

void		G2API_SetRagDoll(CGhoul2Info_v& ghoul2, CRagDollParams* parms);
void		G2API_ResetRagDoll(CGhoul2Info_v& ghoul2);
void		G2API_AnimateG2ModelsRag(CGhoul2Info_v& ghoul2, int acurrent_time, CRagDollUpdateParams* params);

qboolean	G2API_RagPCJConstraint(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t min, vec3_t max);
qboolean	G2API_RagPCJGradientSpeed(CGhoul2Info_v& ghoul2, const char* bone_name, float speed);
qboolean	G2API_RagEffectorGoal(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t pos);
qboolean	G2API_GetRagBonePos(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t pos, vec3_t ent_angles, vec3_t ent_pos, vec3_t ent_scale);
qboolean	G2API_RagEffectorKick(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t velocity);
qboolean	G2API_RagForceSolve(CGhoul2Info_v& ghoul2, qboolean force);

qboolean	G2API_SetBoneIKState(CGhoul2Info_v& ghoul2, int time, const char* bone_name, int ikState, sharedSetBoneIKStateParams_t* params);
qboolean	G2API_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params);

void		G2API_AttachInstanceToEntNum(CGhoul2Info_v& ghoul2, int entity_num, qboolean server);
void		G2API_ClearAttachedInstance(int entity_num);
void		G2API_CleanEntAttachments(void);
qboolean	G2API_OverrideServerWithClientData(CGhoul2Info_v& ghoul2, int model_index);

extern qboolean gG2_GBMNoReconstruct;
extern qboolean gG2_GBMUseSPMethod;
// From tr_ghoul2.cpp
void		G2_ConstructGhoulSkeleton(CGhoul2Info_v& ghoul2, int frame_num, bool checkForNewOrigin, const vec3_t scale);

qboolean	G2API_SkinlessModel(CGhoul2Info_v& ghoul2, int model_index);

#ifdef _G2_GORE
int			G2API_GetNumGoreMarks(CGhoul2Info_v& ghoul2, int model_index);
void		G2API_AddSkinGore(CGhoul2Info_v& ghoul2, SSkinGoreData& gore);
void		G2API_ClearSkinGore(CGhoul2Info_v& ghoul2);
#endif // _SOF2

int			G2API_Ghoul2Size(CGhoul2Info_v& ghoul2);
void		RemoveBoneCache(CBoneCache* bone_cache);

const char* G2API_GetModelName(CGhoul2Info_v& ghoul2, int model_index);
