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

#ifndef __Q_SHARED_H
#include "../qcommon/q_shared.h"
#endif

#if !defined(TR_LOCAL_H)
#include "tr_local.h"
#endif

#if !defined(G2_H_INC)
#include "../ghoul2/G2.h"
#endif

#define G2_MODEL_OK(g) ((g)&&(g)->mValid&&(g)->aHeader&&(g)->currentModel&&(g)->animModel)

//=====================================================================================================================
// Bolt List handling routines - so entities can attach themselves to any part of the model in question

// Given a bone number, see if that bone is already in our bone list
int G2_Find_Bolt_Bone_Num(const boltInfo_v& bltlist, const int bone_num)
{
	// look through entire list
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		if (bltlist[i].boneNumber == bone_num)
		{
			return i;
		}
	}

	// didn't find it
	return -1;
}

// Given a bone number, see if that surface is already in our surfacelist list
int G2_Find_Bolt_Surface_Num(const boltInfo_v& bltlist, const int surfaceNum, const int flags)
{
	// look through entire list
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		if (bltlist[i].surface_number == surfaceNum && (bltlist[i].surfaceType & flags) == flags)
		{
			return i;
		}
	}

	// didn't find it
	return -1;
}

//=========================================================================================
//// Public Bolt Routines
int G2_Add_Bolt_Surf_Num(const CGhoul2Info* ghlInfo, boltInfo_v& bltlist, const surfaceInfo_v& slist, const int surf_num)
{
	assert(ghlInfo && ghlInfo->mValid);
	boltInfo_t temp_bolt;

	assert(surf_num >= 0 && surf_num < static_cast<int>(slist.size()));
	// ensure surface num is valid
	if (surf_num >= static_cast<int>(slist.size()))
	{
		return -1;
	}

	// look through entire list - see if it's already there first
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		// already there??
		if (bltlist[i].surface_number == surf_num)
		{
			// increment the usage count
			bltlist[i].boltUsed++;
			return i;
		}
	}

	// we have a surface
	// look through entire list - see if it's already there first
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		// if this surface entry has info in it, bounce over it
		if (bltlist[i].boneNumber == -1 && bltlist[i].surface_number == -1)
		{
			// if we found an entry that had a -1 for the bone / surface number, then we hit a surface / bone slot that was empty
			bltlist[i].surface_number = surf_num;
			bltlist[i].surfaceType = G2SURFACEFLAG_GENERATED;
			bltlist[i].boltUsed = 1;
			return i;
		}
	}

	// ok, we didn't find an existing surface of that name, or an empty slot. Lets add an entry
	temp_bolt.surface_number = surf_num;
	temp_bolt.surfaceType = G2SURFACEFLAG_GENERATED;
	temp_bolt.boneNumber = -1;
	temp_bolt.boltUsed = 1;
	bltlist.push_back(temp_bolt);
	return bltlist.size() - 1;
}

void G2_Bolt_Not_Found(const char* boneName);

int G2_Add_Bolt(const CGhoul2Info* ghlInfo, boltInfo_v& bltlist, const surfaceInfo_v& slist, const char* boneName)
{
	assert(ghlInfo && ghlInfo->mValid);
	boltInfo_t			temp_bolt;
	uint32_t			flags;

	assert(G2_MODEL_OK(ghlInfo));

	// first up, we'll search for that which this bolt names in all the surfaces
	const int surf_num = G2_IsSurfaceLegal(ghlInfo->currentModel, boneName, &flags);

	// did we find it as a surface?
	if (surf_num != -1)
	{
		// look through entire list - see if it's already there first
		for (size_t i = 0; i < bltlist.size(); i++)
		{
			// already there??
			if (bltlist[i].surface_number == surf_num)
			{
				// increment the usage count
				bltlist[i].boltUsed++;
				return i;
			}
		}

		// look through entire list - see if we can re-use one
		for (size_t i = 0; i < bltlist.size(); i++)
		{
			// if this surface entry has info in it, bounce over it
			if (bltlist[i].boneNumber == -1 && bltlist[i].surface_number == -1)
			{
				// if we found an entry that had a -1 for the bone / surface number, then we hit a surface / bone slot that was empty
				bltlist[i].surface_number = surf_num;
				bltlist[i].boltUsed = 1;
				bltlist[i].surfaceType = 0;
				return i;
			}
		}

		// ok, we didn't find an existing surface of that name, or an empty slot. Lets add an entry
		temp_bolt.surface_number = surf_num;
		temp_bolt.boneNumber = -1;
		temp_bolt.boltUsed = 1;
		temp_bolt.surfaceType = 0;
		bltlist.push_back(temp_bolt);
		return bltlist.size() - 1;
	}

	// no, check to see if it's a bone then

	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)ghlInfo->aHeader + sizeof(mdxaHeader_t));

	int x;
	// walk the entire list of bones in the gla file for this model and see if any match the name of the bone we want to find
	for (x = 0; x < ghlInfo->aHeader->numBones; x++)
	{
		const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>((byte*)ghlInfo->aHeader + sizeof(mdxaHeader_t) + offsets->offsets[x]);
		// if name is the same, we found it
		if (!Q_stricmp(skel->name, boneName))
		{
			break;
		}
	}

	// check to see we did actually make a match with a bone in the model
	if (x == ghlInfo->aHeader->numBones)
	{
		// didn't find it? Error
		//assert(0&&x == mod_a->mdxa->numBones);
#if _DEBUG
		G2_Bolt_Not_Found(boneName);
#endif
		return -1;
	}

	// look through entire list - see if it's already there first
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		// already there??
		if (bltlist[i].boneNumber == x)
		{
			// increment the usage count
			bltlist[i].boltUsed++;
			return i;
		}
	}

	// look through entire list - see if we can re-use it
	for (size_t i = 0; i < bltlist.size(); i++)
	{
		// if this bone entry has info in it, bounce over it
		if (bltlist[i].boneNumber == -1 && bltlist[i].surface_number == -1)
		{
			// if we found an entry that had a -1 for the bone number, then we hit a bone slot that was empty
			bltlist[i].boneNumber = x;
			bltlist[i].boltUsed = 1;
			bltlist[i].surfaceType = 0;
			return i;
		}
	}

	// ok, we didn't find an existing bone of that name, or an empty slot. Lets add an entry
	temp_bolt.boneNumber = x;
	temp_bolt.surface_number = -1;
	temp_bolt.boltUsed = 1;
	temp_bolt.surfaceType = 0;
	bltlist.push_back(temp_bolt);
	return bltlist.size() - 1;
}

// Given a model handle, and a bone name, we want to remove this bone from the bone override list
qboolean G2_Remove_Bolt(boltInfo_v& bltlist, const int index)
{
	assert(index >= 0 && index < static_cast<int>(bltlist.size()));
	// did we find it?
	if (index != -1)
	{
		bltlist[index].boltUsed--;
		if (!bltlist[index].boltUsed)
		{
			// set this bone to not used
			bltlist[index].boneNumber = -1;
			bltlist[index].surface_number = -1;
		}
		return qtrue;
	}
	return qfalse;
}

// set the bolt list to all unused so the bone transformation routine ignores it.
void G2_Init_Bolt_List(boltInfo_v& bltlist)
{
	bltlist.clear();
}