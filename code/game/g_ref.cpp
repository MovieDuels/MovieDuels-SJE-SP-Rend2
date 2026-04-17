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

// Reference tag utility functions
#include "g_local.h"
#include "g_functions.h"
#include "g_nav.h"

extern int delayedShutDown;

#define	TAG_GENERIC_NAME	"__WORLD__"	//If a designer chooses this name, cut a finger off as an example to the others

using refTag_v = std::vector<referenceTag_t*>;
using refTag_m = std::map<std::string, referenceTag_t*>;

using tagOwner_t = struct tagOwner_s
{
	refTag_v tags;
	refTag_m tagMap;
};

using refTagOwner_m = std::map<std::string, tagOwner_t*>;

refTagOwner_m refTagOwnerMap;

/*
-------------------------
TAG_ShowTags
-------------------------
*/

void TAG_ShowTags(int flags)
{
	refTagOwner_m::iterator rtoi;

	STL_ITERATE(rtoi, refTagOwnerMap)
	{
		refTag_v::iterator rti;
		STL_ITERATE(rti, (((*rtoi).second)->tags))
		{
			if ((*rti)->flags & RTF_NAVGOAL)
			{
				if (gi.inPVS(g_entities[0].currentOrigin, (*rti)->origin))
					CG_DrawNode((*rti)->origin, NODE_NAVGOAL);
			}
		}
	}
}

/*
-------------------------
TAG_Init
-------------------------
*/

void TAG_Init()
{
	//Delete all owners
	for (auto rtoi = refTagOwnerMap.begin(); rtoi != refTagOwnerMap.end(); ++rtoi)
	{
		if ((*rtoi).second == nullptr)
		{
			assert(0); //FIXME: This is not good
			continue;
		}

		refTag_v::iterator rti;

		//Delete all tags within the owner's scope
		for (rti = (*rtoi).second->tags.begin(); rti != (*rtoi).second->tags.end(); ++rti)
		{
			if (*rti == nullptr)
			{
				assert(0); //FIXME: Bad bad
				continue;
			}

			//Free it
			delete* rti;
		}

		//Clear the containers
		(*rtoi).second->tags.clear();
		(*rtoi).second->tagMap.clear();

		//Delete the owner
		delete (*rtoi).second;
	}

	//Clear the container
	refTagOwnerMap.clear();
}

/*
-------------------------
TAG_FindOwner
-------------------------
*/

static tagOwner_t* TAG_FindOwner(const char* owner)
{
	const auto rtoi = refTagOwnerMap.find(owner);

	if (rtoi == refTagOwnerMap.end())
		return nullptr;

	return (*rtoi).second;
}

/*
-------------------------
TAG_Find
-------------------------
*/

static referenceTag_t* TAG_Find(const char* owner, const char* name)
{
	tagOwner_t* tagOwner = VALIDSTRING(owner) ? TAG_FindOwner(owner) : TAG_FindOwner(TAG_GENERIC_NAME);

	//Not found...
	if (tagOwner == nullptr)
	{
		tagOwner = TAG_FindOwner(TAG_GENERIC_NAME);

		if (tagOwner == nullptr)
			return nullptr;
	}

	auto rti = tagOwner->tagMap.find(name);

	if (rti == tagOwner->tagMap.end())
	{
		//Try the generic owner instead
		tagOwner = TAG_FindOwner(TAG_GENERIC_NAME);

		if (tagOwner == nullptr)
			return nullptr;

		char tempName[MAX_REFNAME];

		Q_strncpyz(tempName, name, MAX_REFNAME);
		Q_strlwr(tempName); //NOTENOTE: For case insensitive searches on a map

		rti = tagOwner->tagMap.find(tempName);

		if (rti == tagOwner->tagMap.end())
			return nullptr;
	}

	return (*rti).second;
}

/*
-------------------------
TAG_Add
-------------------------
*/

referenceTag_t* TAG_Add(const char* name, const char* owner, vec3_t origin, vec3_t angles, const int radius,
	const int flags)
{
	const auto tag = new referenceTag_t;
	VALIDATEP(tag);

	//Copy the information
	VectorCopy(origin, tag->origin);
	VectorCopy(angles, tag->angles);
	tag->radius = radius;
	tag->flags = flags;

	if (VALIDSTRING(name) == false)
	{
		//gi.Error("Nameless ref_tag found at (%i %i %i)", (int)origin[0], (int)origin[1], (int)origin[2]);
		gi.Printf(S_COLOR_RED"ERROR: Nameless ref_tag found at (%i %i %i)\n", static_cast<int>(origin[0]),
			static_cast<int>(origin[1]), static_cast<int>(origin[2]));
		delayedShutDown = level.time + 100;
		delete tag;
		return nullptr;
	}

	//Copy the name
	Q_strncpyz(tag->name, name, MAX_REFNAME);
	Q_strlwr(tag->name); //NOTENOTE: For case insensitive searches on a map

	//Make sure this tag's name isn't alread in use
	if (TAG_Find(owner, name))
	{
		delayedShutDown = level.time + 100;
		gi.Printf(S_COLOR_RED"ERROR: Duplicate tag name \"%s\"\n", name);
		delete tag;
		return nullptr;
	}

	//Attempt to add this to the owner's list
	if (VALIDSTRING(owner) == false)
	{
		//If the owner isn't found, use the generic world name
		owner = TAG_GENERIC_NAME;
	}

	tagOwner_t* tagOwner = TAG_FindOwner(owner);

	//If the owner is valid, add this tag to it
	if VALID(tagOwner)
	{
		tagOwner->tags.insert(tagOwner->tags.end(), tag);
		tagOwner->tagMap[reinterpret_cast<char*>(&tag->name)] = tag;
	}
	else
	{
		//Create a new owner list
		const auto tag_owner = new tagOwner_t;

		VALIDATEP(tag_owner);

		//Insert the information
		tag_owner->tags.insert(tag_owner->tags.end(), tag);
		tag_owner->tagMap[static_cast<char*>(tag->name)] = tag;

		//Map it
		refTagOwnerMap[owner] = tag_owner;
	}

	return tag;
}

/*
-------------------------
TAG_GetOrigin
-------------------------
*/

int TAG_GetOrigin(const char* owner, const char* name, vec3_t origin)
{
	const referenceTag_t* tag = TAG_Find(owner, name);

	if (!tag)
	{
		VectorClear(origin);
		return false;
	}

	VALIDATEB(tag);

	VectorCopy(tag->origin, origin);

	return true;
}

/*
-------------------------
TAG_GetOrigin2
Had to get rid of that damn assert for dev
-------------------------
*/

int TAG_GetOrigin2(const char* owner, const char* name, vec3_t origin)
{
	const referenceTag_t* tag = TAG_Find(owner, name);

	if (tag == nullptr)
	{
		return qfalse;
	}

	VectorCopy(tag->origin, origin);

	return qtrue;
}

/*
-------------------------
TAG_GetAngles
-------------------------
*/

int TAG_GetAngles(const char* owner, const char* name, vec3_t angles)
{
	const referenceTag_t* tag = TAG_Find(owner, name);

	VALIDATEB(tag);

	VectorCopy(tag->angles, angles);

	return true;
}

/*
-------------------------
TAG_GetRadius
-------------------------
*/

int TAG_GetRadius(const char* owner, const char* name)
{
	const referenceTag_t* tag = TAG_Find(owner, name);

	VALIDATEB(tag);

	return tag->radius;
}

/*
-------------------------
TAG_GetFlags
-------------------------
*/

int TAG_GetFlags(const char* owner, const char* name)
{
	const referenceTag_t* tag = TAG_Find(owner, name);

	VALIDATEB(tag);

	return tag->flags;
}

/*
==============================================================================

Spawn functions

==============================================================================
*/

/*QUAKED ref_tag (0.5 0.5 1) (-8 -8 -8) (8 8 8)

Reference tags which can be positioned throughout the level.
These tags can later be refered to by the scripting system
so that their origins and angles can be referred to.

If you set angles on the tag, these will be retained.

If you target a ref_tag at an entity, that will set the ref_tag's
angles toward that entity.

If you set the ref_tag's ownername to the ownername of an entity,
it makes that entity is the owner of the ref_tag.  This means
that the owner, and only the owner, may refer to that tag.

Tags may not have the same name as another tag with the same
owner.  However, tags with different owners may have the same
name as one another.  In this way, scripts can generically
refer to tags by name, and their owners will automatically
specifiy which tag is being referred to.

targetname	- the name of this tag
ownername	- the owner of this tag
target		- use to point the tag at something for angles
*/

void ref_link(gentity_t* ent)
{
	if (ent->target)
	{
		//TODO: Find the target and set our angles to that direction
		const gentity_t* target = G_Find(nullptr, FOFS(targetname), ent->target);

		if (target)
		{
			vec3_t dir;
			//Find the direction to the target
			VectorSubtract(target->s.origin, ent->s.origin, dir);
			VectorNormalize(dir);
			vectoangles(dir, ent->s.angles);

			//FIXME: Does pitch get flipped?
		}
		else
		{
			gi.Printf(S_COLOR_RED"ERROR: ref_tag (%s) has invalid target (%s)", ent->targetname, ent->target);
		}
	}

	//Add the tag
	/*tag = */
	TAG_Add(ent->targetname, ent->ownername, ent->s.origin, ent->s.angles, 16, 0);

	//Delete immediately, cannot be refered to as an entity again
	//NOTE: this means if you wanted to link them in a chain for, say, a path, you can't
	G_FreeEntity(ent);
}

void SP_referenceTag(gentity_t* ent)
{
	if (ent->target)
	{
		//Init cannot occur until all entities have been spawned
		ent->e_ThinkFunc = thinkF_ref_link;
		ent->nextthink = level.time + START_TIME_LINK_ENTS;
	}
	else
	{
		ref_link(ent);
	}
}