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
#if !defined(G2_H_INC)
#define G2_H_INC

class CMiniHeap;

// defines to setup the
constexpr auto ENTITY_WIDTH = 12;
constexpr auto MODEL_WIDTH = 10;
constexpr auto BOLT_WIDTH = 10;

#define		MODEL_AND	((1<<MODEL_WIDTH)-1)
#define		BOLT_AND	((1<<BOLT_WIDTH)-1)
#define		ENTITY_AND	((1<<ENTITY_WIDTH)-1)

constexpr auto BOLT_SHIFT = 0;
#define		MODEL_SHIFT	(BOLT_SHIFT + BOLT_WIDTH)
#define		ENTITY_SHIFT (MODEL_SHIFT + MODEL_WIDTH)

//rww - RAGDOLL_BEGIN
class CRagDollUpdateParams;
//rww - RAGDOLL_END

struct model_s;
// internal surface calls  G2_surfaces.cpp
qboolean G2_SetSurfaceOnOff(CGhoul2Info* ghlInfo, const char* surfaceName, const int offFlags);

qboolean G2_SetRootSurface(CGhoul2Info_v& ghoul2, const int modelIndex, const char* surfaceName);

int G2_AddSurface(CGhoul2Info* ghoul2, const int surface_number, const int poly_number, const float barycentric_i, const float barycentric_j, int lod);

qboolean G2_RemoveSurface(surfaceInfo_v& slist, const int index);

const surfaceInfo_t* G2_FindOverrideSurface(const int surfaceNum, const surfaceInfo_v& surface_list);

int G2_IsSurfaceLegal(const model_s* mod_m, const char* surfaceName, uint32_t* flags);

int G2_GetParentSurface(const CGhoul2Info* ghlInfo, const int index);

int G2_GetSurfaceIndex(const CGhoul2Info* ghlInfo, const char* surfaceName);

int G2_IsSurfaceRendered(const CGhoul2Info* ghlInfo, const char* surfaceName, const surfaceInfo_v& slist);

// internal bone calls - G2_Bones.cpp
qboolean G2_Set_Bone_Angles(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName, const float* angles, const int flags, const Eorientations up, const Eorientations left, const Eorientations forward, const int blend_time, const int current_time, const vec3_t offset);
qboolean G2_Remove_Bone(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName);
qboolean G2_Remove_Bone_Index(boneInfo_v& blist, const int index);

qboolean G2_Set_Bone_Anim(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int current_time, const float set_frame, const int blend_time);

qboolean G2_Get_Bone_Anim(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName, const int current_time, float* current_frame, int* startFrame, int* endFrame, int* flags, float* retAnimSpeed);

qboolean G2_Get_Bone_Anim_Range(const CGhoul2Info* ghlInfo, const boneInfo_v& blist, const char* boneName, int* startFrame, int* endFrame);

qboolean G2_Get_Bone_Anim_Range_Index(const boneInfo_v& blist, const int bone_index, int* startFrame, int* endFrame);
qboolean G2_Pause_Bone_Anim(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName, const int current_time);
qboolean G2_Pause_Bone_Anim_Index(boneInfo_v& blist, const int bone_index, const int current_time, const int numFrames);
qboolean G2_IsPaused(const CGhoul2Info* ghlInfo, const boneInfo_v& blist, const char* boneName);
qboolean G2_Stop_Bone_Anim(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName);
qboolean G2_Stop_Bone_Angles(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName);
//rww - RAGDOLL_BEGIN
void G2_Animate_Bone_List(CGhoul2Info_v& ghoul2, const int current_time, const int index, CRagDollUpdateParams* params);
//rww - RAGDOLL_END

void G2_Init_Bone_List(boneInfo_v& blist, const int numBones);
int G2_Find_Bone_In_List(const boneInfo_v& blist, const int bone_num);

qboolean G2_Set_Bone_Angles_Matrix(const CGhoul2Info* ghlInfo, boneInfo_v& blist, const char* boneName, const mdxaBone_t& matrix, const int flags, const int blend_time, const int current_time);

int G2_Get_Bone_Index(CGhoul2Info* ghoul2, const char* boneName, const qboolean bAddIfNotFound);
qboolean G2_Set_Bone_Angles_Index(CGhoul2Info* ghlInfo, boneInfo_v& blist, const int index, const float* angles, const int flags, const Eorientations yaw, const Eorientations pitch, const Eorientations roll, const int blend_time, const int current_time, const vec3_t offset);
qboolean G2_Set_Bone_Angles_Matrix_Index(boneInfo_v& blist, const int index, const mdxaBone_t& matrix, const int flags, const int blend_time, const int current_time);
qboolean G2_Stop_Bone_Anim_Index(boneInfo_v& blist, const int index);
qboolean G2_Stop_Bone_Angles_Index(boneInfo_v& blist, const int index);
qboolean G2_Set_Bone_Anim_Index(boneInfo_v& blist, const int index, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int current_time, const float set_frame, const int ablend_time, const int numFrames);
qboolean G2_Get_Bone_Anim_Index(boneInfo_v& blist, const int index, const int current_time, float* retcurrent_frame, int* startFrame, int* endFrame, int* flags, float* retAnimSpeed, const int numFrames);

// misc functions G2_misc.cpp
void G2_List_Model_Surfaces(const char* fileName);
void G2_List_Model_Bones(const char* fileName);
qboolean G2_GetAnimFileName(const char* fileName, char** filename);

#ifdef _G2_GORE
void G2_TraceModels(CGhoul2Info_v& ghoul2, vec3_t rayStart, vec3_t rayEnd, CCollisionRecord* collRecMap, int entNum, EG2_Collision eG2TraceType, int useLod, float fRadius, const float ssize, const float tsize, const float theta, const int shader, SSkinGoreData* gore, const qboolean skipIfLODNotMatch);
#else
void G2_TraceModels(CGhoul2Info_v& ghoul2, vec3_t rayStart, vec3_t rayEnd, CCollisionRecord* collRecMap, int entNum, EG2_Collision eG2TraceType, int useLod, float fRadius);
#endif
#ifdef _G2_GORE
void G2_TransformModel(CGhoul2Info_v& ghoul2, const int frameNum, vec3_t scale, CMiniHeap* G2VertSpace, int useLod, bool ApplyGore, SSkinGoreData* gore = NULL);
#else
void G2_TransformModel(CGhoul2Info_v& ghoul2, const int frameNum, vec3_t scale, CMiniHeap* G2VertSpace, int useLod);
#endif

void TransformAndTranslatePoint(const vec3_t in, vec3_t out, mdxaBone_t* mat);
void G2_GenerateWorldMatrix(const vec3_t angles, const vec3_t origin);
void TransformPoint(const vec3_t in, vec3_t out, mdxaBone_t* mat);
void Inverse_Matrix(mdxaBone_t* src, mdxaBone_t* dest);
void* G2_FindSurface(const model_s* mod, const int index, const int lod);
void G2_SaveGhoul2Models(CGhoul2Info_v& ghoul2);
void G2_LoadGhoul2Model(CGhoul2Info_v& ghoul2, char* buffer);

// internal bolt calls. G2_bolts.cpp
int G2_Add_Bolt(const CGhoul2Info* ghlInfo, boltInfo_v& bltlist, const surfaceInfo_v& slist, const char* boneName);

qboolean G2_Remove_Bolt(boltInfo_v& bltlist, const int index);
void G2_Init_Bolt_List(boltInfo_v& bltlist);
int G2_Find_Bolt_Bone_Num(const boltInfo_v& bltlist, const int bone_num);
int G2_Find_Bolt_Surface_Num(const boltInfo_v& bltlist, const int surfaceNum, const int flags);
int G2_Add_Bolt_Surf_Num(const CGhoul2Info* ghlInfo, boltInfo_v& bltlist, const surfaceInfo_v& slist, const int surf_num);

// API calls - G2_API.cpp
void RestoreGhoul2InfoArray();
void SaveGhoul2InfoArray();

qhandle_t G2API_PrecacheGhoul2Model(const char* fileName);

int G2API_InitGhoul2Model(CGhoul2Info_v& ghoul2, const char* fileName, const int modelIndex, const qhandle_t customSkin = NULL_HANDLE, const qhandle_t customShader = NULL_HANDLE, const int model_flags = 0, const int lod_bias = 0);

qboolean G2API_SetLodBias(CGhoul2Info* ghlInfo, const int lod_bias);

qboolean G2API_SetSkin(CGhoul2Info* ghlInfo, const qhandle_t customSkin, const qhandle_t render_skin = 0);

qboolean G2API_SetShader(CGhoul2Info* ghlInfo, const qhandle_t customShader);
qboolean G2API_RemoveGhoul2Model(CGhoul2Info_v& ghlInfo, const int modelIndex);
qboolean G2API_SetSurfaceOnOff(CGhoul2Info* ghlInfo, const char* surfaceName, const int flags);
qboolean G2API_SetRootSurface(CGhoul2Info_v& ghlInfo, const int modelIndex, const char* surfaceName);
qboolean G2API_RemoveSurface(CGhoul2Info* ghlInfo, const int index);

int G2API_AddSurface(CGhoul2Info* ghlInfo, const int surface_number, const int poly_number, const float barycentric_i, const float barycentric_j, const int lod);

qboolean G2API_SetBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int acurrent_time, const float set_frame = -1, const int blend_time = -1);

qboolean G2API_GetBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int acurrent_time, float* current_frame, int* startFrame, int* endFrame, int* flags, float* animSpeed, qhandle_t* model_list);

qboolean G2API_GetBoneAnimIndex(CGhoul2Info* ghlInfo, const int iBoneIndex, const int acurrent_time, float* current_frame, int* startFrame, int* endFrame, int* flags, float* animSpeed, qhandle_t* model_list);

qboolean G2API_GetAnimRange(CGhoul2Info* ghlInfo, const char* boneName, int* startFrame, int* endFrame);
qboolean G2API_GetAnimRangeIndex(CGhoul2Info* ghlInfo, const int bone_index, int* startFrame, int* endFrame);

qboolean G2API_PauseBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int acurrent_time);
qboolean G2API_PauseBoneAnimIndex(CGhoul2Info* ghlInfo, const int bone_index, const int acurrent_time);

qboolean G2API_IsPaused(CGhoul2Info* ghlInfo, const char* boneName);
qboolean G2API_StopBoneAnim(CGhoul2Info* ghlInfo, const char* boneName);

qboolean G2API_SetBoneAngles(CGhoul2Info* ghlInfo, const char* boneName, const vec3_t angles, const int flags, const Eorientations up, const Eorientations left, const Eorientations forward, qhandle_t* model_list, const int blend_time = 0, const int acurrent_time = 0);

qboolean G2API_SetBoneAnglesOffset(CGhoul2Info* ghlInfo, const char* boneName, const vec3_t angles, const int flags, const Eorientations up, const Eorientations left, const Eorientations forward, qhandle_t* model_list, const int blend_time = 0, const int acurrent_time = 0, const vec3_t offset = nullptr);

qboolean G2API_StopBoneAngles(CGhoul2Info* ghlInfo, const char* boneName);
qboolean G2API_RemoveBone(CGhoul2Info* ghlInfo, const char* boneName);
qboolean G2API_RemoveBolt(CGhoul2Info* ghlInfo, const int index);
int G2API_AddBolt(CGhoul2Info* ghlInfo, const char* boneName);
int G2API_AddBoltSurfNum(CGhoul2Info* ghlInfo, const int surf_index);
qboolean G2API_AttachG2Model(CGhoul2Info* ghlInfo, CGhoul2Info* ghlInfoTo, int toBoltIndex, int to_model);
qboolean G2API_DetachG2Model(CGhoul2Info* ghlInfo);
qboolean G2API_AttachEnt(int* boltInfo, CGhoul2Info* ghlInfoTo, int toBoltIndex, int entNum, int to_model_num);
void G2API_DetachEnt(int* boltInfo);

qboolean G2API_GetBoltMatrix(CGhoul2Info_v& ghoul2, const int modelIndex, const int bolt_index, mdxaBone_t* matrix, const vec3_t angles, const vec3_t position, const int aframe_num, qhandle_t* model_list, const vec3_t scale);

void G2API_ListSurfaces(CGhoul2Info* ghlInfo);
void G2API_ListBones(CGhoul2Info* ghlInfo, const int frame);
qboolean G2API_HaveWeGhoul2Models(const CGhoul2Info_v& ghoul2);
void G2API_SetGhoul2ModelIndexes(CGhoul2Info_v& ghoul2, qhandle_t* model_list, const qhandle_t* skin_list);
qboolean G2API_SetGhoul2ModelFlags(CGhoul2Info* ghlInfo, const int flags);
int G2API_GetGhoul2ModelFlags(CGhoul2Info* ghlInfo);

qboolean G2API_GetAnimFileName(CGhoul2Info* ghlInfo, char** filename);
void G2API_CollisionDetect(CCollisionRecord* collRecMap, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position, int aframe_number, int entNum, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, CMiniHeap* G2VertSpace, EG2_Collision eG2TraceType, int useLod, float fRadius);
void G2API_GiveMeVectorFromMatrix(mdxaBone_t& bolt_matrix, const Eorientations flags, vec3_t& vec);
void G2API_CopyGhoul2Instance(const CGhoul2Info_v& ghoul2_from, CGhoul2Info_v& ghoul2_to, int modelIndex = -1);
void G2API_CleanGhoul2Models(CGhoul2Info_v& ghoul2);
int G2API_GetParentSurface(CGhoul2Info* ghlInfo, const int index);
int G2API_GetSurfaceIndex(CGhoul2Info* ghlInfo, const char* surfaceName);
char* G2API_GetSurfaceName(CGhoul2Info* ghlInfo, const int surfNumber);
char* G2API_GetGLAName(CGhoul2Info* ghlInfo);

qboolean G2API_SetBoneAnglesMatrix(CGhoul2Info* ghlInfo, const char* boneName, const mdxaBone_t& matrix, const int flags, qhandle_t* model_list, const int blend_time = 0, const int acurrent_time = 0);

qboolean G2API_SetNewOrigin(CGhoul2Info* ghlInfo, const int bolt_index);
int G2API_GetBoneIndex(CGhoul2Info* ghlInfo, const char* boneName, const qboolean bAddIfNotFound);
qboolean G2API_StopBoneAnglesIndex(CGhoul2Info* ghlInfo, const int index);
qboolean G2API_StopBoneAnimIndex(CGhoul2Info* ghlInfo, const int index);

qboolean G2API_SetBoneAnglesIndex(CGhoul2Info* ghlInfo, const int index, const vec3_t angles, const int flags, const Eorientations yaw, const Eorientations pitch, const Eorientations roll, qhandle_t* model_list, const int blend_time, const int acurrent_time);

qboolean G2API_SetBoneAnglesMatrixIndex(CGhoul2Info* ghlInfo, const int index, const mdxaBone_t& matrix, const int flags, qhandle_t* model_list, const int blend_time, const int acurrent_time);

qboolean G2API_SetBoneAnimIndex(CGhoul2Info* ghlInfo, const int index, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int acurrent_time, const float set_frame, const int blend_time);

qboolean G2API_SetAnimIndex(CGhoul2Info* ghlInfo, const int index);
int G2API_GetAnimIndex(const CGhoul2Info* ghlInfo);
void G2API_SaveGhoul2Models(CGhoul2Info_v& ghoul2);
void G2API_LoadGhoul2Models(CGhoul2Info_v& ghoul2, char* buffer);
void G2API_LoadSaveCodeDestructGhoul2Info(CGhoul2Info_v& ghoul2);
char* G2API_GetAnimFileNameIndex(const qhandle_t modelIndex);
char* G2API_GetAnimFileInternalNameIndex(const qhandle_t modelIndex);
int G2API_GetSurfaceRenderStatus(CGhoul2Info* ghlInfo, const char* surfaceName);

// From tr_ghoul2.cpp
void G2_ConstructGhoulSkeleton(CGhoul2Info_v& ghoul2, int frameNum, bool checkForNewOrigin, const vec3_t scale);
void G2_GetBoltMatrixLow(CGhoul2Info& ghoul2, int boltNum, const vec3_t scale, mdxaBone_t& retMatrix);
void G2_TimingModel(boneInfo_t& bone, const int current_time, const int numFramesInFile, int& current_frame, int& newFrame, float& lerp);

bool G2_SetupModelPointers(CGhoul2Info_v& ghoul2); // returns true if any model is properly set up
bool G2_SetupModelPointers(CGhoul2Info* ghlInfo); // returns true if the model is properly set up

//#ifdef _G2_GORE	// These exist regardless, non-gore versions are empty
void G2API_AddSkinGore(CGhoul2Info_v& ghoul2, SSkinGoreData& gore);
void G2API_ClearSkinGore(CGhoul2Info_v& ghoul2);
//#endif

#endif // G2_H_INC
