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

class CQuickOverride
{
	int mOverride[512];
	int mAt[512];
	int mCurrentTouch;

public:
	CQuickOverride() : mAt{}
	{
		mCurrentTouch = 1;
		for (int i = 0; i < 512; i++)
		{
			mOverride[i] = 0;
		}
	}

	void Invalidate()
	{
		mCurrentTouch++;
	}

	void Set(const int index, const int pos)
	{
		if (index == 10000)
		{
			return;
		}
		assert(index >= 0 && index < 512);
		mOverride[index] = mCurrentTouch;
		mAt[index] = pos;
	}

	int Test(const int index) const
	{
		assert(index >= 0 && index < 512);
		if (mOverride[index] != mCurrentTouch)
		{
			return -1;
		}
		return mAt[index];
	}
};

static CQuickOverride QuickOverride;

// find a particular surface in the surface override list
const surfaceInfo_t* G2_FindOverrideSurface(const int surfaceNum, const surfaceInfo_v& surface_list)
{
	if (surfaceNum < 0)
	{
		// starting a new lookup
		QuickOverride.Invalidate();
		for (size_t i = 0; i < surface_list.size(); i++)
		{
			if (surface_list[i].surface >= 0)
			{
				QuickOverride.Set(surface_list[i].surface, i);
			}
		}
		return nullptr;
	}
	const int idx = QuickOverride.Test(surfaceNum);
	if (idx < 0)
	{
		if (surfaceNum == 10000)
		{
			for (const auto& i : surface_list)
			{
				if (i.surface == surfaceNum)
				{
					return &i;
				}
			}
		}
#if _DEBUG
		// look through entire list
		size_t i;
		for (i = 0; i < surface_list.size(); i++)
		{
			if (surface_list[i].surface == surfaceNum)
			{
				break;
			}
		}
		// didn't find it.
		assert(i == surface_list.size()); // our quickoverride is not working right
#endif
		return nullptr;
	}
	assert(idx >= 0 && idx < static_cast<int>(surface_list.size()));
	assert(surface_list[idx].surface == surfaceNum);
	return &surface_list[idx];
}

// given a surface name, lets see if it's legal in the model
int G2_IsSurfaceLegal(const model_s* mod_m, const char* surfaceName, uint32_t* flags)
{
	assert(mod_m);
	assert(mod_m->mdxm);
	auto surf = reinterpret_cast<mdxmSurfHierarchy_t*>(reinterpret_cast<byte*>(mod_m->mdxm) + mod_m->mdxm->
		ofsSurfHierarchy);

	for (int i = 0; i < mod_m->mdxm->numSurfaces; i++)
	{
		if (!Q_stricmp(surfaceName, surf->name))
		{
			*flags = surf->flags;
			return i;
		}
		// find the next surface
		surf = reinterpret_cast<mdxmSurfHierarchy_t*>(reinterpret_cast<byte*>(surf) + reinterpret_cast<intptr_t>(&
			static_cast<mdxmSurfHierarchy_t*>(nullptr)->
			childIndexes[surf->numChildren]));
	}
	return -1;
}

/************************************************************************************************
 * G2_FindSurface
 *    find a surface in a ghoul2 surface override list based on it's name
 *
 * Input
 *    filename of model, surface list of model instance, name of surface, int to be filled in
 * with the index of this surface (defaults to NULL)
 *
 * Output
 *    pointer to surface if successful, false otherwise
 *
 ************************************************************************************************/
static const mdxmSurface_t* G2_FindSurface(const CGhoul2Info* ghlInfo, const surfaceInfo_v& slist, const char* surfaceName, int* surf_index)
{
	// find the model we want
	assert(G2_MODEL_OK(ghlInfo));

	const mdxmHierarchyOffsets_t* surf_indexes = reinterpret_cast<mdxmHierarchyOffsets_t*>(reinterpret_cast<byte*>(
		ghlInfo->currentModel->mdxm) + sizeof(mdxmHeader_t));

	// first find if we already have this surface in the list
	for (int i = slist.size() - 1; i >= 0; i--)
	{
		if (slist[i].surface != 10000 && slist[i].surface != -1)
		{
			const mdxmSurface_t* surf = static_cast<mdxmSurface_t*>(G2_FindSurface(
				ghlInfo->currentModel, slist[i].surface, 0));
			// back track and get the surfinfo struct for this surface
			const mdxmSurfHierarchy_t* surf_info = reinterpret_cast<mdxmSurfHierarchy_t*>((byte*)surf_indexes +
				surf_indexes->offsets[surf->thisSurfaceIndex]);

			// are these the droids we're looking for?
			if (!Q_stricmp(surf_info->name, surfaceName))
			{
				// yup
				if (surf_index)
				{
					*surf_index = i;
				}
				return surf;
			}
		}
	}
	// didn't find it
	if (surf_index)
	{
		*surf_index = -1;
	}
	return nullptr;
}

// set a named surface offFlags - if it doesn't find a surface with this name in the list then it will add one.
qboolean G2_SetSurfaceOnOff(CGhoul2Info* ghlInfo, const char* surfaceName, const int offFlags)
{
	int surf_index = -1;

	// find the model we want
	// first find if we already have this surface in the list
	const mdxmSurface_t* surf = G2_FindSurface(ghlInfo, ghlInfo->mSlist, surfaceName, &surf_index);
	if (surf)
	{
		// set descendants value

		// slist[surf_index].offFlags = offFlags;
		// seems to me that we shouldn't overwrite the other flags.
		// the only bit we really care about in the incoming flags is the off bit
		ghlInfo->mSlist[surf_index].offFlags &= ~(G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);
		ghlInfo->mSlist[surf_index].offFlags |= offFlags & (G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);
		return qtrue;
	}
	// ok, not in the list already - in that case, lets verify this surface exists in the model mesh
	uint32_t flags;
	const int surfaceNum = G2_IsSurfaceLegal(ghlInfo->currentModel, surfaceName, &flags);
	if (surfaceNum != -1)
	{
		uint32_t newflags = flags;
		// the only bit we really care about in the incoming flags is the off bit
		newflags &= ~(G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);
		newflags |= offFlags & (G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);

		if (newflags != flags)
		{
			surfaceInfo_t temp_slist_entry;
			// insert here then because it changed, no need to add an override otherwise
			temp_slist_entry.offFlags = newflags;
			temp_slist_entry.surface = surfaceNum;

			ghlInfo->mSlist.push_back(temp_slist_entry);
		}
		return qtrue;
	}
	return qfalse;
}

static void G2_FindRecursiveSurface(const model_t* currentModel, int surfaceNum, surfaceInfo_v& root_list, int* active_surfaces)
{
	assert(currentModel);
	assert(currentModel->mdxm);
	const mdxmSurface_t* surface = static_cast<mdxmSurface_t*>(G2_FindSurface(currentModel, surfaceNum, 0));
	const mdxmHierarchyOffsets_t* surf_indexes = reinterpret_cast<mdxmHierarchyOffsets_t*>(reinterpret_cast<byte*>(
		currentModel->mdxm) + sizeof(mdxmHeader_t));
	const mdxmSurfHierarchy_t* surf_info = reinterpret_cast<mdxmSurfHierarchy_t*>((byte*)surf_indexes + surf_indexes->
		offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	const surfaceInfo_t* surf_override = G2_FindOverrideSurface(surfaceNum, root_list);

	// really, we should use the default flags for this surface unless it's been overriden
	int offFlags = surf_info->flags;

	// set the off flags if we have some
	if (surf_override)
	{
		offFlags = surf_override->offFlags;
	}

	// if this surface is not off, indicate as such in the active surface list
	if (!(offFlags & G2SURFACEFLAG_OFF))
	{
		active_surfaces[surfaceNum] = 1;
	}
	else
		// if we are turning off all descendants, then stop this recursion now
		if (offFlags & G2SURFACEFLAG_NODESCENDANTS)
		{
			return;
		}

	// now recursively call for the children
	for (int i = 0; i < surf_info->numChildren; i++)
	{
		surfaceNum = surf_info->childIndexes[i];
		G2_FindRecursiveSurface(currentModel, surfaceNum, root_list, active_surfaces);
	}
}

qboolean G2_SetRootSurface(CGhoul2Info_v& ghoul2, const int modelIndex, const char* surfaceName)
{
	uint32_t flags;
	assert(modelIndex >= 0 && modelIndex < ghoul2.size());
	assert(ghoul2[modelIndex].currentModel);
	assert(ghoul2[modelIndex].currentModel->mdxm);
	// first find if we already have this surface in the list
	const int surf = G2_IsSurfaceLegal(ghoul2[modelIndex].currentModel, surfaceName, &flags);
	if (surf != -1)
	{
		ghoul2[modelIndex].mSurfaceRoot = surf;
		return qtrue;
	}
	assert(0);
	return qfalse;
}

extern int G2_DecideTraceLod(CGhoul2Info& ghoul2, int useLod);

int G2_AddSurface(CGhoul2Info* ghoul2, const int surface_number, const int poly_number, const float barycentric_i, const float barycentric_j, int lod)
{
	lod = G2_DecideTraceLod(*ghoul2, lod);

	// first up, see if we have a free one already set up  - look only from the end of the constant surfaces onwards
	size_t i;
	for (i = 0; i < ghoul2->mSlist.size(); i++)
	{
		// is the surface count -1? That would indicate it's free
		if (ghoul2->mSlist[i].surface == -1)
		{
			break;
		}
	}
	if (i == ghoul2->mSlist.size())
	{
		ghoul2->mSlist.emplace_back();
	}
	ghoul2->mSlist[i].offFlags = G2SURFACEFLAG_GENERATED;
	ghoul2->mSlist[i].surface = 10000; // no model will ever have 10000 surfaces
	ghoul2->mSlist[i].genBarycentricI = barycentric_i;
	ghoul2->mSlist[i].genBarycentricJ = barycentric_j;
	ghoul2->mSlist[i].genPolySurfaceIndex = (poly_number & 0xffff) << 16 | surface_number & 0xffff;
	ghoul2->mSlist[i].genLod = lod;
	return i;
}

qboolean G2_RemoveSurface(surfaceInfo_v& slist, const int index)
{
	if (index != -1)
	{
		slist[index].surface = -1;
		return qtrue;
	}
	assert(0);
	return qfalse;
}

int G2_GetParentSurface(const CGhoul2Info* ghlInfo, const int index)
{
	assert(ghlInfo->currentModel);
	assert(ghlInfo->currentModel->mdxm);
	const mdxmHierarchyOffsets_t* surf_indexes = reinterpret_cast<mdxmHierarchyOffsets_t*>(reinterpret_cast<byte*>(
		ghlInfo->currentModel->mdxm) + sizeof(mdxmHeader_t));

	// walk each surface and see if this index is listed in it's children
	const mdxmSurface_t* surf = static_cast<mdxmSurface_t*>(G2_FindSurface(ghlInfo->currentModel, index, 0));
	const mdxmSurfHierarchy_t* surf_info = reinterpret_cast<mdxmSurfHierarchy_t*>((byte*)surf_indexes + surf_indexes->
		offsets[surf->thisSurfaceIndex]);

	return surf_info->parentIndex;
}

int G2_GetSurfaceIndex(const CGhoul2Info* ghlInfo, const char* surfaceName)
{
	uint32_t flags;
	assert(ghlInfo->currentModel);
	return G2_IsSurfaceLegal(ghlInfo->currentModel, surfaceName, &flags);
}

int G2_IsSurfaceRendered(const CGhoul2Info* ghlInfo, const char* surfaceName, const surfaceInfo_v& slist)
{
	uint32_t flags = 0u; //, surfFlags = 0;
	int surf_index = 0;
	assert(ghlInfo->currentModel);
	assert(ghlInfo->currentModel->mdxm);
	if (!ghlInfo->currentModel->mdxm)
	{
		return -1;
	}

	// now travel up the skeleton to see if any of it's ancestors have a 'no descendants' turned on

	// find the original surface in the surface list
	int surf_num = G2_IsSurfaceLegal(ghlInfo->currentModel, surfaceName, &flags);
	if (surf_num != -1)
	{
		//must be legal
		const mdxmHierarchyOffsets_t* surf_indexes = reinterpret_cast<mdxmHierarchyOffsets_t*>(reinterpret_cast<byte*>(
			ghlInfo->currentModel->mdxm) + sizeof(mdxmHeader_t));
		const mdxmSurfHierarchy_t* surf_info = reinterpret_cast<mdxmSurfHierarchy_t*>((byte*)surf_indexes + surf_indexes->
			offsets[surf_num]);
		surf_num = surf_info->parentIndex;
		// walk the surface hierarchy up until we hit the root
		while (surf_num != -1)
		{
			uint32_t parent_flags = 0u;

			const mdxmSurfHierarchy_t* parent_surf_info = reinterpret_cast<mdxmSurfHierarchy_t*>((byte*)surf_indexes +
				surf_indexes->offsets[
					surf_num]);

			// find the original surface in the surface list
			//G2 was error, above comment was accurate, but we don't want the original flags, we want the parent flags
			G2_IsSurfaceLegal(ghlInfo->currentModel, parent_surf_info->name, &parent_flags);

			// now see if we already have overriden this surface in the slist
			const mdxmSurface_t* parent_surf = G2_FindSurface(ghlInfo, slist, parent_surf_info->name, &surf_index);
			if (parent_surf)
			{
				// set descendants value
				parent_flags = slist[surf_index].offFlags;
			}
			// now we have the parent flags, lets see if any have the 'no descendants' flag set
			if (parent_flags & G2SURFACEFLAG_NODESCENDANTS)
			{
				flags |= G2SURFACEFLAG_OFF;
				break;
			}
			// set up scan of next parent
			surf_num = parent_surf_info->parentIndex;
		}
	}
	else
	{
		return -1;
	}
	if (flags == 0)
	{
		//it's not being overridden by a parent
		// now see if we already have overriden this surface in the slist
		const mdxmSurface_t* surf = G2_FindSurface(ghlInfo, slist, surfaceName, &surf_index);
		if (surf)
		{
			// set descendants value
			flags = slist[surf_index].offFlags;
		}
		// ok, at this point in flags we have what this surface is set to, and the index of the surface itself
	}
	return flags;
}