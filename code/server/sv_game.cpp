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

// sv_game.c -- interface to the game dll

#include "../server/exe_headers.h"

#include "../qcommon/cm_local.h"

#include "server.h"
#include "../client/vmachine.h"
#include "../client/client.h"
#include "qcommon/ojk_saved_game.h"
/*#include "..\renderer\tr_local.h"
#include "..\renderer\tr_WorldEffects.h"*/
/*
Ghoul2 Insert Start
*/
#if !defined(G2_H_INC)
#include "../ghoul2/G2.h"
#endif

/*
Ghoul2 Insert End
*/

static void* gameLibrary;

//prototypes
extern void Com_WriteCam(const char* text);
extern void Com_FlushCamFile();

extern int s_entityWavVol[MAX_GENTITIES];

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
/*
int	SV_NumForGentity( gentity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)ge->gentities ) / ge->gentitySize;

	return num;
}
*/
gentity_t* SV_GentityNum(const int num)
{
	assert(num >= 0);
	auto* ent = reinterpret_cast<gentity_t*>(reinterpret_cast<byte*>(ge->gentities) + ge->gentitySize * num);

	return ent;
}

svEntity_t* SV_SvEntityForGentity(const gentity_t* g_ent)
{
	if (!g_ent || g_ent->s.number < 0 || g_ent->s.number >= MAX_GENTITIES)
	{
		Com_Error(ERR_DROP, "SV_SvEntityForGentity: bad gEnt");
	}
	return &sv.svEntities[g_ent->s.number];
}

gentity_t* SV_GEntityForSvEntity(const svEntity_t* sv_ent)
{
	const int num = sv_ent - sv.svEntities;
	return SV_GentityNum(num);
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand(const int clientNum, const char* fmt, ...)
{
	char msg[8192];
	va_list argptr;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof msg, fmt, argptr);
	va_end(argptr);

	if (clientNum == -1)
	{
		SV_SendServerCommand(nullptr, "%s", msg);
	}
	else
	{
		if (clientNum < 0 || clientNum >= 1)
		{
			return;
		}
		SV_SendServerCommand(svs.clients + clientNum, "%s", msg);
	}
}

/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient(const int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= 1)
	{
		return;
	}
	SV_DropClient(svs.clients + clientNum, reason);
}

/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel(gentity_t* ent, const char* name)
{
	clipHandle_t h;
	vec3_t mins, maxs;

	if (!name)
	{
		Com_Error(ERR_DROP, "SV_SetBrushModel: NULL model for ent number %d", ent->s.number);
	}

	if (name[0] == '*')
	{
		ent->s.modelindex = atoi(name + 1);

		if (sv.mLocalSubBSPIndex != -1)
		{
			ent->s.modelindex += sv.mLocalSubBSPModelOffset;
		}

		h = CM_InlineModel(ent->s.modelindex);

		if (sv.mLocalSubBSPIndex != -1)
		{
			CM_ModelBounds(h, mins, maxs);
		}
		else
		{
			CM_ModelBounds(h, mins, maxs);
		}

		//CM_ModelBounds( h, mins, maxs );

		VectorCopy(mins, ent->mins);
		VectorCopy(maxs, ent->maxs);
		ent->bmodel = qtrue;

		ent->contents = CM_ModelContents(h, -1);
	}
	else if (name[0] == '#')
	{
		ent->s.modelindex = CM_LoadSubBSP(va("maps/%s.bsp", name + 1), qfalse);
		CM_ModelBounds(ent->s.modelindex, mins, maxs);

		VectorCopy(mins, ent->mins);
		VectorCopy(maxs, ent->maxs);
		ent->bmodel = qtrue;

		//rwwNOTE: We don't ever want to set contents -1, it includes CONTENTS_LIGHTSABER.
		//Lots of stuff will explode if there's a brush with CONTENTS_LIGHTSABER that isn't attached to a client owner.
		//ent->contents = -1;		// we don't know exactly what is in the brushes
		h = CM_InlineModel(ent->s.modelindex);
		ent->contents = CM_ModelContents(h, CM_FindSubBSP(ent->s.modelindex));
		//	ent->contents = CONTENTS_SOLID;
	}
	else
	{
		Com_Error(ERR_DROP, "SV_SetBrushModel: %s isn't a brush model (ent %d)", name, ent->s.number);
	}
}

const char* SV_SetActiveSubBSP(const int index)
{
	if (index >= 0)
	{
		sv.mLocalSubBSPIndex = CM_FindSubBSP(index);
		sv.mLocalSubBSPModelOffset = index;
		sv.mLocalSubBSPEntityParsePoint = CM_SubBSPEntityString(sv.mLocalSubBSPIndex);
		return sv.mLocalSubBSPEntityParsePoint;
	}
	sv.mLocalSubBSPIndex = -1;

	return nullptr;
}

/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS(const vec3_t p1, const vec3_t p2)
{
	int start = 0;

	if (com_speeds->integer)
	{
		start = Sys_Milliseconds();
	}
	int leafnum = CM_PointLeafnum(p1);
	int cluster = CM_LeafCluster(leafnum);
	const int area1 = CM_LeafArea(leafnum);
	const byte* mask = CM_ClusterPVS(cluster);

	leafnum = CM_PointLeafnum(p2);
	cluster = CM_LeafCluster(leafnum);
	const int area2 = CM_LeafArea(leafnum);
	if (mask && !(mask[cluster >> 3] & 1 << (cluster & 7)))
	{
		if (com_speeds->integer)
		{
			timeInPVSCheck += Sys_Milliseconds() - start;
		}
		return qfalse;
	}

	if (!CM_AreasConnected(area1, area2))
	{
		timeInPVSCheck += Sys_Milliseconds() - start;
		return qfalse; // a door blocks sight
	}

	if (com_speeds->integer)
	{
		timeInPVSCheck += Sys_Milliseconds() - start;
	}
	return qtrue;
}

/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals(const vec3_t p1, const vec3_t p2)
{
	int start = 0;

	if (com_speeds->integer)
	{
		start = Sys_Milliseconds();
	}

	int leafnum = CM_PointLeafnum(p1);
	int cluster = CM_LeafCluster(leafnum);
	const byte* mask = CM_ClusterPVS(cluster);

	leafnum = CM_PointLeafnum(p2);
	cluster = CM_LeafCluster(leafnum);

	if (mask && !(mask[cluster >> 3] & 1 << (cluster & 7)))
	{
		if (com_speeds->integer)
		{
			timeInPVSCheck += Sys_Milliseconds() - start;
		}
		return qfalse;
	}

	if (com_speeds->integer)
	{
		timeInPVSCheck += Sys_Milliseconds() - start;
	}
	return qtrue;
}

/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState(gentity_t* ent, const qboolean open)
{
#ifndef JK2_MODE
	if (!(ent->contents & CONTENTS_OPAQUE))
	{
#ifndef FINAL_BUILD
		//		Com_Printf( "INFO: entity number %d not opaque: not affecting area portal!\n", ent->s.number );
#endif
		return;
	}
#endif

	const svEntity_t* sv_ent = SV_SvEntityForGentity(ent);
	if (sv_ent->areanum2 == -1)
	{
		return;
	}
	CM_AdjustAreaPortalState(sv_ent->areanum, sv_ent->areanum2, open);
}

/*
==================
SV_GameAreaEntities
==================
*/
qboolean SV_EntityContact(const vec3_t mins, const vec3_t maxs, const gentity_t* g_ent)
{
	trace_t trace;

	// check for exact collision
	const float* origin = g_ent->currentOrigin;
	const float* angles = g_ent->currentAngles;

	const clipHandle_t ch = SV_ClipHandleForEntity(g_ent);
	CM_TransformedBoxTrace(&trace, vec3_origin, vec3_origin, mins, maxs,
		ch, -1, origin, angles);

	return trace.startsolid;
}

/*
===============
SV_GetServerinfo

===============
*/
void SV_GetServerinfo(char* buffer, const int buffer_size)
{
	if (buffer_size < 1)
	{
		Com_Error(ERR_DROP, "SV_GetServerinfo: buffer_size == %i", buffer_size);
	}
	Q_strncpyz(buffer, Cvar_InfoString(CVAR_SERVERINFO), buffer_size);
}

qboolean SV_GetEntityToken(char* buffer, const int buffer_size)
{
	char* s;

	if (sv.mLocalSubBSPIndex == -1)
	{
		s = COM_Parse(const_cast<const char**>(&sv.entityParsePoint));
		Q_strncpyz(buffer, s, buffer_size);
		if (!sv.entityParsePoint && !s[0])
		{
			return qfalse;
		}
		return qtrue;
	}
	s = COM_Parse(const_cast<const char**>(&sv.mLocalSubBSPEntityParsePoint));
	Q_strncpyz(buffer, s, buffer_size);
	if (!sv.mLocalSubBSPEntityParsePoint && !s[0])
	{
		return qfalse;
	}
	return qtrue;
}

//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs()
{
	if (!ge)
	{
		return;
	}
	ge->Shutdown();

	SCR_StopCinematic();
	CL_ShutdownCGame(); //we have cgame buried in here.

	Sys_UnloadDll(gameLibrary);

	ge = nullptr;
	cgvm.entryPoint = nullptr;
}

// this is a compile-helper function since Z_Malloc can now become a macro with __LINE__ etc
//
static void* G_ZMalloc_Helper(const int iSize, const memtag_t eTag, const qboolean b_zeroit)
{
	return Z_Malloc(iSize, eTag, b_zeroit);
}

static int SV_G2API_AddBolt(CGhoul2Info* ghlInfo, const char* boneName)
{
	return re.G2API_AddBolt(ghlInfo, boneName);
}

static int SV_G2API_AddBoltSurfNum(CGhoul2Info* ghlInfo, const int surf_index)
{
	return re.G2API_AddBoltSurfNum(ghlInfo, surf_index);
}

static int SV_G2API_AddSurface(CGhoul2Info* ghlInfo, const int surface_number, const int poly_number, const float barycentric_i,
	const float barycentric_j, const int lod)
{
	return re.G2API_AddSurface(ghlInfo, surface_number, poly_number, barycentric_i, barycentric_j, lod);
}

static void SV_G2API_AnimateG2Models(CGhoul2Info_v& ghoul2, const int acurrent_time, CRagDollUpdateParams* params)
{
	re.G2API_AnimateG2Models(ghoul2, acurrent_time, params);
}

static qboolean SV_G2API_AttachEnt(int* boltInfo, CGhoul2Info* ghlInfoTo, const int toBoltIndex, const int entNum, const int to_model_num)
{
	return re.G2API_AttachEnt(boltInfo, ghlInfoTo, toBoltIndex, entNum, to_model_num);
}

static qboolean SV_G2API_AttachG2Model(CGhoul2Info* ghlInfo, CGhoul2Info* ghlInfoTo, const int toBoltIndex, const int to_model)
{
	return re.G2API_AttachG2Model(ghlInfo, ghlInfoTo, toBoltIndex, to_model);
}

static void SV_G2API_CleanGhoul2Models(CGhoul2Info_v& ghoul2)
{
	return re.G2API_CleanGhoul2Models(ghoul2);
}

static void SV_G2API_CollisionDetect(
	CCollisionRecord* coll_rec_map, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position,
	const int aframe_number, const int entNum, vec3_t ray_start, vec3_t ray_end, vec3_t scale, CMiniHeap* mini_heap,
	const EG2_Collision eG2TraceType, const int useLod, const float f_radius)
{
	re.G2API_CollisionDetect(coll_rec_map, ghoul2, angles, position, aframe_number,
		entNum, ray_start, ray_end, scale, mini_heap, eG2TraceType, useLod, f_radius);
}

static void SV_G2API_CopyGhoul2Instance(const CGhoul2Info_v& ghoul2_from, CGhoul2Info_v& ghoul2_to, const int modelIndex)
{
	re.G2API_CopyGhoul2Instance(ghoul2_from, ghoul2_to, modelIndex);
}

static void SV_G2API_DetachEnt(int* boltInfo)
{
	re.G2API_DetachEnt(boltInfo);
}

static qboolean SV_G2API_DetachG2Model(CGhoul2Info* ghlInfo)
{
	return re.G2API_DetachG2Model(ghlInfo);
}

static qboolean SV_G2API_GetAnimFileName(CGhoul2Info* ghlInfo, char** filename)
{
	return re.G2API_GetAnimFileName(ghlInfo, filename);
}

static char* SV_G2API_GetAnimFileNameIndex(const qhandle_t modelIndex)
{
	return re.G2API_GetAnimFileNameIndex(modelIndex);
}

static char* SV_G2API_GetAnimFileInternalNameIndex(const qhandle_t modelIndex)
{
	return re.G2API_GetAnimFileInternalNameIndex(modelIndex);
}

static int SV_G2API_GetAnimIndex(const CGhoul2Info* ghlInfo)
{
	return re.G2API_GetAnimIndex(ghlInfo);
}

static qboolean SV_G2API_GetAnimRange(CGhoul2Info* ghlInfo, const char* boneName, int* startFrame, int* endFrame)
{
	return re.G2API_GetAnimRange(ghlInfo, boneName, startFrame, endFrame);
}

static qboolean SV_G2API_GetAnimRangeIndex(CGhoul2Info* ghlInfo, const int bone_index, int* startFrame, int* endFrame)
{
	return re.G2API_GetAnimRangeIndex(ghlInfo, bone_index, startFrame, endFrame);
}

static qboolean SV_G2API_GetBoneAnim(
	CGhoul2Info* ghlInfo, const char* boneName, const int acurrent_time,
	float* current_frame, int* startFrame, int* endFrame, int* flags, float* animSpeed, int* model_list)
{
	return re.G2API_GetBoneAnim(ghlInfo, boneName, acurrent_time, current_frame,
		startFrame, endFrame, flags, animSpeed, model_list);
}

static qboolean SV_G2API_GetBoneAnimIndex(CGhoul2Info* ghlInfo, const int iBoneIndex, const int acurrent_time,
	float* current_frame, int* startFrame, int* endFrame, int* flags,
	float* animSpeed, int* model_list)
{
	return re.G2API_GetBoneAnimIndex(ghlInfo, iBoneIndex, acurrent_time, current_frame,
		startFrame, endFrame, flags, animSpeed, model_list);
}

static int SV_G2API_GetBoneIndex(CGhoul2Info* ghlInfo, const char* boneName, const qboolean bAddIfNotFound)
{
	return re.G2API_GetBoneIndex(ghlInfo, boneName, bAddIfNotFound);
}

static qboolean SV_G2API_GetBoltMatrix(
	CGhoul2Info_v& ghoul2, const int modelIndex, const int bolt_index, mdxaBone_t* matrix, const vec3_t angles,
	const vec3_t position, const int aframe_num, qhandle_t* model_list, const vec3_t scale)
{
	return re.G2API_GetBoltMatrix(ghoul2, modelIndex, bolt_index, matrix, angles,
		position, aframe_num, model_list, scale);
}

static int SV_G2API_GetGhoul2ModelFlags(CGhoul2Info* ghlInfo)
{
	return re.G2API_GetGhoul2ModelFlags(ghlInfo);
}

static char* SV_G2API_GetGLAName(CGhoul2Info* ghlInfo)
{
	return re.G2API_GetGLAName(ghlInfo);
}

static int SV_G2API_GetParentSurface(CGhoul2Info* ghlInfo, const int index)
{
	return re.G2API_GetParentSurface(ghlInfo, index);
}

static qboolean SV_G2API_GetRagBonePos(
	CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos, vec3_t entAngles, vec3_t ent_pos, vec3_t entScale)
{
	return re.G2API_GetRagBonePos(ghoul2, boneName, pos, entAngles, ent_pos, entScale);
}

static int SV_G2API_GetSurfaceIndex(CGhoul2Info* ghlInfo, const char* surfaceName)
{
	return re.G2API_GetSurfaceIndex(ghlInfo, surfaceName);
}

static char* SV_G2API_GetSurfaceName(CGhoul2Info* ghlInfo, const int surf_number)
{
	return re.G2API_GetSurfaceName(ghlInfo, surf_number);
}

static int SV_G2API_GetSurfaceRenderStatus(CGhoul2Info* ghlInfo, const char* surfaceName)
{
	return re.G2API_GetSurfaceRenderStatus(ghlInfo, surfaceName);
}

static void SV_G2API_GiveMeVectorFromMatrix(mdxaBone_t& bolt_matrix, const Eorientations flags, vec3_t& vec)
{
	re.G2API_GiveMeVectorFromMatrix(bolt_matrix, flags, vec);
}

static qboolean SV_G2API_HaveWeGhoul2Models(const CGhoul2Info_v& ghoul2)
{
	return re.G2API_HaveWeGhoul2Models(ghoul2);
}

static qboolean SV_G2API_IKMove(CGhoul2Info_v& ghoul2, const int time, sharedIKMoveParams_t* params)
{
	return re.G2API_IKMove(ghoul2, time, params);
}

static int SV_G2API_InitGhoul2Model(CGhoul2Info_v& ghoul2, const char* fileName, const int modelIndex,
	const qhandle_t customSkin, const qhandle_t customShader, const int model_flags, const int lod_bias)
{
	return re.G2API_InitGhoul2Model(ghoul2, fileName, modelIndex, customSkin, customShader, model_flags, lod_bias);
}

static qboolean SV_G2API_IsPaused(CGhoul2Info* ghlInfo, const char* boneName)
{
	return re.G2API_IsPaused(ghlInfo, boneName);
}

static void SV_G2API_ListBones(CGhoul2Info* ghlInfo, const int frame)
{
	return re.G2API_ListBones(ghlInfo, frame);
}

static void SV_G2API_ListSurfaces(CGhoul2Info* ghlInfo)
{
	return re.G2API_ListSurfaces(ghlInfo);
}

static void SV_G2API_LoadGhoul2Models(CGhoul2Info_v& ghoul2, char* buffer)
{
	return re.G2API_LoadGhoul2Models(ghoul2, buffer);
}

static void SV_G2API_LoadSaveCodeDestructGhoul2Info(CGhoul2Info_v& ghoul2)
{
	return re.G2API_LoadSaveCodeDestructGhoul2Info(ghoul2);
}

static qboolean SV_G2API_PauseBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int acurrent_time)
{
	return re.G2API_PauseBoneAnim(ghlInfo, boneName, acurrent_time);
}

static qboolean SV_G2API_PauseBoneAnimIndex(CGhoul2Info* ghlInfo, const int bone_index, const int acurrent_time)
{
	return re.G2API_PauseBoneAnimIndex(ghlInfo, bone_index, acurrent_time);
}

static qhandle_t SV_G2API_PrecacheGhoul2Model(const char* fileName)
{
	return re.G2API_PrecacheGhoul2Model(fileName);
}

static qboolean SV_G2API_RagEffectorGoal(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos)
{
	return re.G2API_RagEffectorGoal(ghoul2, boneName, pos);
}

static qboolean SV_G2API_RagEffectorKick(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t velocity)
{
	return re.G2API_RagEffectorKick(ghoul2, boneName, velocity);
}

static qboolean SV_G2API_RagForceSolve(CGhoul2Info_v& ghoul2, const qboolean force)
{
	return re.G2API_RagForceSolve(ghoul2, force);
}

static qboolean SV_G2API_RagPCJConstraint(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t min, vec3_t max)
{
	return re.G2API_RagPCJConstraint(ghoul2, boneName, min, max);
}

static qboolean SV_G2API_RagPCJGradientSpeed(CGhoul2Info_v& ghoul2, const char* boneName, const float speed)
{
	return re.G2API_RagPCJGradientSpeed(ghoul2, boneName, speed);
}

static qboolean SV_G2API_RemoveBolt(CGhoul2Info* ghlInfo, const int index)
{
	return re.G2API_RemoveBolt(ghlInfo, index);
}

static qboolean SV_G2API_RemoveBone(CGhoul2Info* ghlInfo, const char* boneName)
{
	return re.G2API_RemoveBone(ghlInfo, boneName);
}

static qboolean SV_G2API_RemoveGhoul2Model(CGhoul2Info_v& ghlInfo, const int modelIndex)
{
	return re.G2API_RemoveGhoul2Model(ghlInfo, modelIndex);
}

static qboolean SV_G2API_RemoveSurface(CGhoul2Info* ghlInfo, const int index)
{
	return re.G2API_RemoveSurface(ghlInfo, index);
}

static void SV_G2API_SaveGhoul2Models(CGhoul2Info_v& ghoul2)
{
	return re.G2API_SaveGhoul2Models(ghoul2);
}

static qboolean SV_G2API_SetAnimIndex(CGhoul2Info* ghlInfo, const int index)
{
	return re.G2API_SetAnimIndex(ghlInfo, index);
}

static qboolean SV_G2API_SetBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int startFrame,
	const int endFrame,
	const int flags, const float animSpeed, const int acurrent_time,
	const float set_frame, const int blend_time)
{
	return re.G2API_SetBoneAnim(ghlInfo, boneName, startFrame, endFrame, flags,
		animSpeed, acurrent_time, set_frame, blend_time);
}

static qboolean SV_G2API_SetBoneAnimIndex(CGhoul2Info* ghlInfo, const int index, const int startFrame,
	const int endFrame,
	const int flags, const float animSpeed, const int acurrent_time,
	const float setFrame, const int blend_time)
{
	return re.G2API_SetBoneAnimIndex(ghlInfo, index, startFrame, endFrame, flags,
		animSpeed, acurrent_time, setFrame, blend_time);
}

static qboolean SV_G2API_SetBoneAngles(CGhoul2Info* ghlInfo, const char* boneName, const vec3_t angles, const int flags,
	const Eorientations up, const Eorientations left, const Eorientations forward,
	qhandle_t* model_list,
	const int blend_time, const int acurrent_time)
{
	return re.G2API_SetBoneAngles(ghlInfo, boneName, angles, flags, up, left, forward,
		model_list, blend_time, acurrent_time);
}

static qboolean SV_G2API_SetBoneAnglesOffset(CGhoul2Info* ghlInfo, const char* boneName, const vec3_t angles,
	const int flags,
	const Eorientations up, const Eorientations right,
	const Eorientations forward, qhandle_t* model_list,
	const int blend_time, const int current_time, const vec3_t offset)
{
	return re.G2API_SetBoneAnglesOffset(ghlInfo, boneName, angles, flags, up, right, forward, model_list, blend_time,
		current_time, offset);
}

static qboolean SV_G2API_SetBoneAnglesIndex(CGhoul2Info* ghlInfo, const int index, const vec3_t angles, const int flags,
	const Eorientations yaw, const Eorientations pitch,
	const Eorientations roll, qhandle_t* model_list,
	const int blend_time, const int acurrent_time)
{
	return re.G2API_SetBoneAnglesIndex(ghlInfo, index, angles, flags, yaw, pitch, roll,
		model_list, blend_time, acurrent_time);
}

static qboolean SV_G2API_SetBoneAnglesMatrix(CGhoul2Info* ghlInfo, const char* boneName, const mdxaBone_t& matrix,
	const int flags, qhandle_t* model_list, const int blend_time, const int acurrent_time)
{
	return re.G2API_SetBoneAnglesMatrix(ghlInfo, boneName, matrix, flags, model_list, blend_time, acurrent_time);
}

static qboolean SV_G2API_SetBoneAnglesMatrixIndex(CGhoul2Info* ghlInfo, const int index, const mdxaBone_t& matrix,
	const int flags, qhandle_t* model_list, const int blandeTime,
	const int acurrent_time)
{
	return re.G2API_SetBoneAnglesMatrixIndex(ghlInfo, index, matrix, flags, model_list, blandeTime, acurrent_time);
}

static qboolean SV_G2API_SetBoneIKState(CGhoul2Info_v& ghoul2, const int time, const char* boneName, const int ikState,
	sharedSetBoneIKStateParams_t* params)
{
	return re.G2API_SetBoneIKState(ghoul2, time, boneName, ikState, params);
}

static qboolean SV_G2API_SetGhoul2ModelFlags(CGhoul2Info* ghlInfo, const int flags)
{
	return re.G2API_SetGhoul2ModelFlags(ghlInfo, flags);
}

static qboolean SV_G2API_SetLodBias(CGhoul2Info* ghlInfo, const int lod_bias)
{
	return re.G2API_SetLodBias(ghlInfo, lod_bias);
}

static qboolean SV_G2API_SetNewOrigin(CGhoul2Info* ghlInfo, const int bolt_index)
{
	return re.G2API_SetNewOrigin(ghlInfo, bolt_index);
}

static void SV_G2API_SetRagDoll(CGhoul2Info_v& ghoul2, CRagDollParams* parms)
{
	return re.G2API_SetRagDoll(ghoul2, parms);
}

static qboolean SV_G2API_SetRootSurface(CGhoul2Info_v& ghlInfo, const int modelIndex, const char* surfaceName)
{
	return re.G2API_SetRootSurface(ghlInfo, modelIndex, surfaceName);
}

static qboolean SV_G2API_SetShader(CGhoul2Info* ghlInfo, const qhandle_t customShader)
{
	return re.G2API_SetShader(ghlInfo, customShader);
}

static qboolean SV_G2API_SetSkin(CGhoul2Info* ghlInfo, const qhandle_t customSkin, const qhandle_t render_skin)
{
	return re.G2API_SetSkin(ghlInfo, customSkin, render_skin);
}

static qboolean SV_G2API_SetSurfaceOnOff(CGhoul2Info* ghlInfo, const char* surfaceName, const int flags)
{
	return re.G2API_SetSurfaceOnOff(ghlInfo, surfaceName, flags);
}

static qboolean SV_G2API_StopBoneAnim(CGhoul2Info* ghlInfo, const char* boneName)
{
	return re.G2API_StopBoneAnim(ghlInfo, boneName);
}

static qboolean SV_G2API_StopBoneAnimIndex(CGhoul2Info* ghlInfo, const int index)
{
	return re.G2API_StopBoneAnimIndex(ghlInfo, index);
}

static qboolean SV_G2API_StopBoneAngles(CGhoul2Info* ghlInfo, const char* boneName)
{
	return re.G2API_StopBoneAngles(ghlInfo, boneName);
}

static qboolean SV_G2API_StopBoneAnglesIndex(CGhoul2Info* ghlInfo, const int index)
{
	return re.G2API_StopBoneAnglesIndex(ghlInfo, index);
}

#ifdef _G2_GORE
static void SV_G2API_AddSkinGore(CGhoul2Info_v& ghoul2, SSkinGoreData& gore)
{
	return re.G2API_AddSkinGore(ghoul2, gore);
}

static void SV_G2API_ClearSkinGore(CGhoul2Info_v& ghoul2)
{
	return re.G2API_ClearSkinGore(ghoul2);
}
#else
static void SV_G2API_AddSkinGore(
	CGhoul2Info_v& ghoul2,
	SSkinGoreData& gore)
{
	static_cast<void>(ghoul2);
	static_cast<void>(gore);
}

static void SV_G2API_ClearSkinGore(
	CGhoul2Info_v& ghoul2)
{
	static_cast<void>(ghoul2);
}
#endif

static IGhoul2InfoArray& SV_TheGhoul2InfoArray()
{
	return re.TheGhoul2InfoArray();
}

static qhandle_t SV_RE_RegisterSkin(const char* name)
{
	return re.RegisterSkin(name);
}

static int SV_RE_GetAnimationCFG(const char* ps_cfg_filename, char* ps_dest, const int i_dest_size)
{
	return re.GetAnimationCFG(ps_cfg_filename, ps_dest, i_dest_size);
}

static bool SV_WE_GetWindVector(vec3_t wind_vector, vec3_t at_point)
{
	return re.GetWindVector(wind_vector, at_point);
}

static bool SV_WE_GetWindGusting(vec3_t atpoint)
{
	return re.GetWindGusting(atpoint);
}

static bool SV_WE_IsOutside(vec3_t pos)
{
	return re.IsOutside(pos);
}

static float SV_WE_IsOutsideCausingPain(vec3_t pos)
{
	return re.IsOutsideCausingPain(pos);
}

static float SV_WE_GetChanceOfSaberFizz()
{
	return re.GetChanceOfSaberFizz();
}

static bool SV_WE_IsShaking(vec3_t pos)
{
	return re.IsShaking(pos);
}

static void SV_WE_AddWeatherZone(vec3_t mins, vec3_t maxs)
{
	return re.AddWeatherZone(mins, maxs);
}

static bool SV_WE_SetTempGlobalFogColor(vec3_t color)
{
	return re.SetTempGlobalFogColor(color);
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SV_InitGameProgs()
{
	game_import_t import{};
	int i;

	// unload anything we have now
	if (ge)
	{
		SV_ShutdownGameProgs();
	}

	// load a new game dll
import.Printf = Com_Printf;
import.WriteCam = Com_WriteCam;
import.FlushCamFile = Com_FlushCamFile;
import.Error = Com_Error;

import.Milliseconds = Sys_Milliseconds2;

import.DropClient = SV_GameDropClient;

import.SendServerCommand = SV_GameSendServerCommand;

import.linkentity = SV_LinkEntity;
import.unlinkentity = SV_UnlinkEntity;
import.EntitiesInBox = SV_AreaEntities;
import.EntityContact = SV_EntityContact;
import.trace = SV_Trace;
import.pointcontents = SV_PointContents;
import.totalMapContents = CM_TotalMapContents;
import.SetBrushModel = SV_SetBrushModel;

import.inPVS = SV_inPVS;
import.inPVSIgnorePortals = SV_inPVSIgnorePortals;

import.SetConfigstring = SV_SetConfigstring;
import.GetConfigstring = SV_GetConfigstring;

import.SetUserinfo = SV_SetUserinfo;
import.GetUserinfo = SV_GetUserinfo;

import.GetServerinfo = SV_GetServerinfo;

import.cvar = Cvar_Get;
import.cvar_set = Cvar_Set;
import.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;
import.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;

import.argc = Cmd_Argc;
import.argv = Cmd_Argv;
import.SendConsoleCommand = Cbuf_AddText;

import.FS_FOpenFile = FS_FOpenFileByMode;
import.FS_Read = FS_Read;
import.FS_Write = FS_Write;
import.FS_FCloseFile = FS_FCloseFile;
import.FS_ReadFile = FS_ReadFile;
import.FS_FreeFile = FS_FreeFile;
import.FS_GetFileList = FS_GetFileList;

import.saved_game = &ojk::SavedGame::get_instance();

import.AdjustAreaPortalState = SV_AdjustAreaPortalState;
import.AreasConnected = CM_AreasConnected;

import.VoiceVolume = s_entityWavVol;

import.Malloc = G_ZMalloc_Helper;
import.Free = Z_Free;
import.bIsFromZone = Z_IsFromZone;

import.G2API_AddBolt = SV_G2API_AddBolt;
import.G2API_AttachEnt = SV_G2API_AttachEnt;
import.G2API_AttachG2Model = SV_G2API_AttachG2Model;
import.G2API_CollisionDetect = SV_G2API_CollisionDetect;
import.G2API_DetachEnt = SV_G2API_DetachEnt;
import.G2API_DetachG2Model = SV_G2API_DetachG2Model;
import.G2API_GetAnimFileName = SV_G2API_GetAnimFileName;
import.G2API_GetBoltMatrix = SV_G2API_GetBoltMatrix;
import.G2API_GetBoneAnim = SV_G2API_GetBoneAnim;
import.G2API_GetBoneAnimIndex = SV_G2API_GetBoneAnimIndex;
import.G2API_AddSurface = SV_G2API_AddSurface;
import.G2API_HaveWeGhoul2Models = SV_G2API_HaveWeGhoul2Models;
import.G2API_InitGhoul2Model = SV_G2API_InitGhoul2Model;
import.G2API_SetBoneAngles = SV_G2API_SetBoneAngles;
import.G2API_SetBoneAnglesOffset = SV_G2API_SetBoneAnglesOffset;
import.G2API_SetBoneAnglesMatrix = SV_G2API_SetBoneAnglesMatrix;
import.G2API_SetBoneAnim = SV_G2API_SetBoneAnim;
import.G2API_SetSkin = SV_G2API_SetSkin;
import.G2API_CopyGhoul2Instance = SV_G2API_CopyGhoul2Instance;
import.G2API_SetBoneAnglesIndex = SV_G2API_SetBoneAnglesIndex;
import.G2API_SetBoneAnimIndex = SV_G2API_SetBoneAnimIndex;
import.G2API_IsPaused = SV_G2API_IsPaused;
import.G2API_ListBones = SV_G2API_ListBones;
import.G2API_ListSurfaces = SV_G2API_ListSurfaces;
import.G2API_PauseBoneAnim = SV_G2API_PauseBoneAnim;
import.G2API_PauseBoneAnimIndex = SV_G2API_PauseBoneAnimIndex;
import.G2API_PrecacheGhoul2Model = SV_G2API_PrecacheGhoul2Model;
import.G2API_RemoveBolt = SV_G2API_RemoveBolt;
import.G2API_RemoveBone = SV_G2API_RemoveBone;
import.G2API_RemoveGhoul2Model = SV_G2API_RemoveGhoul2Model;
import.G2API_SetLodBias = SV_G2API_SetLodBias;
import.G2API_SetRootSurface = SV_G2API_SetRootSurface;
import.G2API_SetShader = SV_G2API_SetShader;
import.G2API_SetSurfaceOnOff = SV_G2API_SetSurfaceOnOff;
import.G2API_StopBoneAngles = SV_G2API_StopBoneAngles;
import.G2API_StopBoneAnim = SV_G2API_StopBoneAnim;
import.G2API_SetGhoul2ModelFlags = SV_G2API_SetGhoul2ModelFlags;
import.G2API_AddBoltSurfNum = SV_G2API_AddBoltSurfNum;
import.G2API_RemoveSurface = SV_G2API_RemoveSurface;
import.G2API_GetAnimRange = SV_G2API_GetAnimRange;
import.G2API_GetAnimRangeIndex = SV_G2API_GetAnimRangeIndex;
import.G2API_GiveMeVectorFromMatrix = SV_G2API_GiveMeVectorFromMatrix;
import.G2API_GetGhoul2ModelFlags = SV_G2API_GetGhoul2ModelFlags;
import.G2API_CleanGhoul2Models = SV_G2API_CleanGhoul2Models;
import.TheGhoul2InfoArray = SV_TheGhoul2InfoArray;
import.G2API_GetParentSurface = SV_G2API_GetParentSurface;
import.G2API_GetSurfaceIndex = SV_G2API_GetSurfaceIndex;
import.G2API_GetSurfaceName = SV_G2API_GetSurfaceName;
import.G2API_GetGLAName = SV_G2API_GetGLAName;
import.G2API_SetNewOrigin = SV_G2API_SetNewOrigin;
import.G2API_GetBoneIndex = SV_G2API_GetBoneIndex;
import.G2API_StopBoneAnglesIndex = SV_G2API_StopBoneAnglesIndex;
import.G2API_StopBoneAnimIndex = SV_G2API_StopBoneAnimIndex;
import.G2API_SetBoneAnglesMatrixIndex = SV_G2API_SetBoneAnglesMatrixIndex;
import.G2API_SetAnimIndex = SV_G2API_SetAnimIndex;
import.G2API_GetAnimIndex = SV_G2API_GetAnimIndex;

import.G2API_SaveGhoul2Models = SV_G2API_SaveGhoul2Models;
import.G2API_LoadGhoul2Models = SV_G2API_LoadGhoul2Models;
import.G2API_LoadSaveCodeDestructGhoul2Info = SV_G2API_LoadSaveCodeDestructGhoul2Info;
import.G2API_GetAnimFileNameIndex = SV_G2API_GetAnimFileNameIndex;
import.G2API_GetAnimFileInternalNameIndex = SV_G2API_GetAnimFileInternalNameIndex;
import.G2API_GetSurfaceRenderStatus = SV_G2API_GetSurfaceRenderStatus;

import.G2API_SetRagDoll = SV_G2API_SetRagDoll;
import.G2API_AnimateG2Models = SV_G2API_AnimateG2Models;

import.G2API_RagPCJConstraint = SV_G2API_RagPCJConstraint;
import.G2API_RagPCJGradientSpeed = SV_G2API_RagPCJGradientSpeed;
import.G2API_RagEffectorGoal = SV_G2API_RagEffectorGoal;
import.G2API_GetRagBonePos = SV_G2API_GetRagBonePos;
import.G2API_RagEffectorKick = SV_G2API_RagEffectorKick;
import.G2API_RagForceSolve = SV_G2API_RagForceSolve;

import.G2API_SetBoneIKState = SV_G2API_SetBoneIKState;
import.G2API_IKMove = SV_G2API_IKMove;

import.G2API_AddSkinGore = SV_G2API_AddSkinGore;
import.G2API_ClearSkinGore = SV_G2API_ClearSkinGore;

import.SetActiveSubBSP = SV_SetActiveSubBSP;

import.RE_RegisterSkin = SV_RE_RegisterSkin;
import.RE_GetAnimationCFG = SV_RE_GetAnimationCFG;

import.WE_GetWindVector = SV_WE_GetWindVector;
import.WE_GetWindGusting = SV_WE_GetWindGusting;
import.WE_IsOutside = SV_WE_IsOutside;
import.WE_IsOutsideCausingPain = SV_WE_IsOutsideCausingPain;
import.WE_GetChanceOfSaberFizz = SV_WE_GetChanceOfSaberFizz;
import.WE_IsShaking = SV_WE_IsShaking;
import.WE_AddWeatherZone = SV_WE_AddWeatherZone;
import.WE_SetTempGlobalFogColor = SV_WE_SetTempGlobalFogColor;

#ifdef JK2_MODE
	const char* gamename = "jospgame";
#else
	auto gamename = "MovieDuels-game";
#endif

	GetGameAPIProc* get_game_api;
	gameLibrary = Sys_LoadSPGameDll(gamename, &get_game_api);
	if (!gameLibrary)
		Com_Error(ERR_DROP, "Failed to load %s library", gamename);

	ge = static_cast<game_export_t*>(get_game_api(&import));
	if (!ge)
	{
		Sys_UnloadDll(gameLibrary);
		Com_Error(ERR_DROP, "Failed to load %s library", gamename);
	}

	if (ge->apiversion != GAME_API_VERSION)
	{
		int apiVersion = ge->apiversion;
		Sys_UnloadDll(gameLibrary);
		Com_Error(ERR_DROP, "game is version %i, not %i", apiVersion, GAME_API_VERSION);
	}

	//hook up the client while we're here
	if (!CL_InitCGameVM(gameLibrary))
	{
		Sys_UnloadDll(gameLibrary);
		Com_Error(ERR_DROP, "Failed to load client game functions");
	}

	sv.entityParsePoint = CM_EntityString();

	// use the current msec count for a random seed
	Z_TagFree(TAG_G_ALLOC);
	ge->Init(sv_mapname->string, sv_spawntarget->string, sv_mapChecksum->integer, CM_EntityString(), sv.time,
		com_frameTime, Com_Milliseconds(), e_saved_game_just_loaded, qbLoadTransition);

	// clear all gentity pointers that might still be set from
	// a previous level
	for (i = 0; i < 1; i++)
	{
		svs.clients[i].gentity = nullptr;
	}
}

/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand()
{
	if (sv.state != SS_GAME)
	{
		return qfalse;
	}

	return ge->ConsoleCommand();
}