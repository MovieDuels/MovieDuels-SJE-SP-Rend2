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

#ifndef __CM_PUBLIC_H__
#define __CM_PUBLIC_H__

#include "qfiles.h"

qboolean CM_DeleteCachedMap(qboolean b_guaranteed_ok_to_delete);
void CM_LoadMap(const char* name, qboolean clientload, int* checksum, qboolean sub_bsp);
void CM_ClearMap();
int CM_TotalMapContents();

clipHandle_t CM_InlineModel(int index); // 0 = world, 1 + are bmodels
clipHandle_t CM_TempBoxModel(const vec3_t mins, const vec3_t maxs); //, const int contents );

int CM_ModelContents(clipHandle_t model, int sub_bsp_index);

int CM_NumInlineModels();
char* CM_EntityString();
char* CM_SubBSPEntityString(int index);
int CM_LoadSubBSP(const char* name, qboolean clientload);
int CM_FindSubBSP(int modelIndex);

// returns an ORed contents mask
int CM_PointContents(const vec3_t p, clipHandle_t model);
int CM_TransformedPointContents(const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles);

void CM_BoxTrace(trace_t* results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs,
	clipHandle_t model, int brushmask);
void CM_TransformedBoxTrace(trace_t* results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs,
	clipHandle_t model, int brushmask,
	const vec3_t origin, const vec3_t angles);

byte* CM_ClusterPVS(int cluster);

int CM_PointLeafnum(const vec3_t p);

// only returns non-solid leafs
// overflow if return listsize and if *lastLeaf != list[listsize-1]
int CM_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int* box_list,
	int listsize, int* last_leaf);

int CM_LeafCluster(int leafnum);
int CM_LeafArea(int leafnum);

void CM_AdjustAreaPortalState(int area1, int area2, qboolean open);
qboolean CM_AreasConnected(int area1, int area2);

int CM_WriteAreaBits(byte* buffer, int area);

//for savegames
void CM_WritePortalState();
void CM_ReadPortalState();

// cm_marks.c
int CM_MarkFragments(int numPoints, const vec3_t* points, const vec3_t projection,
	int max_points, vec3_t point_buffer, int max_fragments, markFragment_t* fragment_buffer);

// cm_patch.c
void CM_DrawDebugSurface(void (*draw_poly)(int color, int numPoints, const float* points));

#endif //__CM_PUBLIC_H__
