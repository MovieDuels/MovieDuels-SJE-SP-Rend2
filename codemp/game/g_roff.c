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

#include "g_local.h"
#include "g_roff.h"

roff_list_t roffs[MAX_ROFFSNEW];
int num_roffs = 0;

qboolean g_bCollidableRoffs = qfalse;

extern void Q3_TaskIDComplete(gentity_t* ent, taskID_t taskType);

static void G_RoffNotetrackCallback(const gentity_t* cent, const char* notetrack)
{
	int i = 0, r = 0;
	char type[256];
	char argument[512];
	char addlArg[512];
	char errMsg[256];
	int addlArgs = 0;

	if (!cent || !notetrack)
	{
		return;
	}

	while (notetrack[i] && notetrack[i] != ' ')
	{
		type[i] = notetrack[i];
		i++;
	}

	type[i] = '\0';

	if (notetrack[i] != ' ')
	{
		//didn't pass in a valid notetrack type, or forgot the argument for it
		return;
	}

	i++;

	while (notetrack[i] && notetrack[i] != ' ')
	{
		if (notetrack[i] != '\n' && notetrack[i] != '\r')
		{
			//don't read line ends for an argument
			argument[r] = notetrack[i];
			r++;
		}
		i++;
	}
	argument[r] = '\0';

	if (!r)
	{
		return;
	}

	if (notetrack[i] == ' ')
	{
		//additional arguments...
		addlArgs = 1;

		i++;
		r = 0;
		while (notetrack[i])
		{
			addlArg[r] = notetrack[i];
			r++;
			i++;
		}
		addlArg[r] = '\0';
	}

	if (strcmp(type, "effect") == 0)
	{
		vec3_t parsedOffset;
		char teststr[256];
		char t[64];
		int posoffsetGathered = 0;
		int r2 = 0;
		if (!addlArgs)
		{
			VectorClear(parsedOffset);
			goto defaultoffsetposition;
		}

		i = 0;

		while (posoffsetGathered < 3)
		{
			r = 0;
			while (addlArg[i] && addlArg[i] != '+' && addlArg[i] != ' ')
			{
				t[r] = addlArg[i];
				r++;
				i++;
			}
			t[r] = '\0';
			i++;
			if (!r)
			{
				//failure..
				VectorClear(parsedOffset);
				i = 0;
				goto defaultoffsetposition;
			}
			parsedOffset[posoffsetGathered] = atof(t);
			posoffsetGathered++;
		}

		if (posoffsetGathered < 3)
		{
			sprintf(errMsg, "Offset position argument for 'effect' type is invalid.");
			goto functionend;
		}

		i--;

		if (addlArg[i] != ' ')
		{
			addlArgs = 0;
		}

	defaultoffsetposition:

		r = 0;
		if (argument[r] == '/')
		{
			r++;
		}
		while (argument[r] && argument[r] != '/')
		{
			teststr[r2] = argument[r];
			r2++;
			r++;
		}
		teststr[r2] = '\0';

		if (r2 && strstr(teststr, "effects"))
		{
			//get rid of the leading "effects" since it's auto-inserted
			r++;
			r2 = 0;

			while (argument[r])
			{
				teststr[r2] = argument[r];
				r2++;
				r++;
			}
			teststr[r2] = '\0';

			strcpy(argument, teststr);
		}

		const int objectID = G_EffectIndex(argument);

		if (objectID)
		{
			vec3_t up;
			vec3_t right;
			vec3_t forward;
			vec3_t useOrigin;
			vec3_t useAngles;
			if (addlArgs)
			{
				vec3_t parsedAngles;
				int anglesGathered = 0;
				//if there is an additional argument for an effect it is expected to be XANGLE-YANGLE-ZANGLE
				i++;
				while (anglesGathered < 3)
				{
					r = 0;
					while (addlArg[i] && addlArg[i] != '-')
					{
						t[r] = addlArg[i];
						r++;
						i++;
					}
					t[r] = '\0';
					i++;

					if (!r)
					{
						//failed to get a new part of the vector
						anglesGathered = 0;
						break;
					}

					parsedAngles[anglesGathered] = atof(t);
					anglesGathered++;
				}

				if (anglesGathered)
				{
					VectorCopy(parsedAngles, useAngles);
				}
				else
				{
					//failed to parse angles from the extra argument provided..
					VectorCopy(cent->s.apos.trBase, useAngles);
				}
			}
			else
			{
				//if no constant angles, play in direction entity is facing
				VectorCopy(cent->s.apos.trBase, useAngles);
			}

			AngleVectors(useAngles, forward, right, up);

			VectorCopy(cent->s.pos.trBase, useOrigin);

			//forward
			useOrigin[0] += forward[0] * parsedOffset[0];
			useOrigin[1] += forward[1] * parsedOffset[0];
			useOrigin[2] += forward[2] * parsedOffset[0];

			//right
			useOrigin[0] += right[0] * parsedOffset[1];
			useOrigin[1] += right[1] * parsedOffset[1];
			useOrigin[2] += right[2] * parsedOffset[1];

			//up
			useOrigin[0] += up[0] * parsedOffset[2];
			useOrigin[1] += up[1] * parsedOffset[2];
			useOrigin[2] += up[2] * parsedOffset[2];

			G_PlayEffect(objectID, useOrigin, useAngles);
		}
	}
	/*else if (strcmp(type, "sound") == 0)
	{
		objectID = G_SoundIndex(argument);
		cgi_S_StartSound(cent->s.pos.trBase, cent->s.number, CHAN_BODY, objectID);
	}*/
	//else if ...
	else
	{
		if (type[0])
		{
			Com_Printf("Warning: \"%s\" is an invalid ROFF notetrack function\n", type);
		}
		else
		{
			Com_Printf("Warning: Note track is missing function and/or arguments\n");
		}
	}

	return;

functionend:
	Com_Printf("Type-specific notetrack error: %s\n", errMsg);
}

static qboolean G_ValidRoff(roff_hdr2_t* header)
{
	if (strncmp(header->mHeader, "ROFF", 4) == 0)
	{
		if (header->mCount > 0 && header->mVersion == ROFF_VERSIONNEW2)
		{
			return qtrue;
		}
		if (header->mVersion == ROFF_VERSIONNEW && ((roff_hdr_t*)header)->mCount > 0.0f)
		{
			// version 1 defines the count as a float, so we best do the count check as a float or we'll get bogus results
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean G_InitRoff(char* file, unsigned char* data)
{
	roff_hdr_t* header = (roff_hdr_t*)data;
	int count = (int)header->mCount;

	if (count >= MAXNUMDATANEW)
	{
		//have more frames than data slots. give an error.
		Com_Printf(S_COLOR_RED"ROFF file: %s has more frames than currently supported.\n", file);
	}

	//set filename
	roffs[num_roffs].fileName = G_NewString(file);

	if (header->mVersion == ROFF_VERSIONNEW)
	{
		// We are Old School(tm)
		roffs[num_roffs].type = 1;

		roffs[num_roffs].mFrameTime = 100; // old school ones have a hard-coded frame time
		roffs[num_roffs].mLerp = 10;
		roffs[num_roffs].mNumNoteTracks = 0;
		roffs[num_roffs].NumData = 0;

		//if ( mem )
		{
			// Step past the header to get to the goods
			move_rotate_t* roff_data = (move_rotate_t*)&header[1];

			// The allocation worked, so stash this stuff off so we can reference the data later if needed
			roffs[num_roffs].frames = count;

			// Copy all of the goods into our ROFF cache
			for (int i = 0; i < count; i++, roff_data++)
			{
				// Copy just the delta position and orientation which can be applied to anything at a later point
				VectorCopy(roff_data->origin_delta, roffs[num_roffs].data[i].origin_delta);
				VectorCopy(roff_data->rotate_delta, roffs[num_roffs].data[i].rotate_delta);
				roffs[num_roffs].NumData++;
			}
			return qtrue;
		}
	}
	if (header->mVersion == ROFF_VERSIONNEW2)
	{
		// Version 2.0, heck yeah!
		roff_hdr2_t* hdr = (roff_hdr2_t*)data;
		count = hdr->mCount;

		roffs[num_roffs].frames = count;
		roffs[num_roffs].NumData = 0;

		//if ( mem )
		{
			roffs[num_roffs].mFrameTime = hdr->mFrameRate;
			roffs[num_roffs].mLerp = 1000 / hdr->mFrameRate;
			roffs[num_roffs].mNumNoteTracks = hdr->mNumNotes;
			roffs[num_roffs].NumData = 0;

			if (roffs[num_roffs].mFrameTime < 50)
			{
				Com_Printf(S_COLOR_RED"Error: \"%s\" has an invalid ROFF framerate (%d < 50)\n", file,
					roffs[num_roffs].mFrameTime);
			}
			assert(roffs[num_roffs].mFrameTime >= 50); //HAS to be at least 50 to be reliable

			// Step past the header to get to the goods
			const move_rotate2_t* roff_data = (move_rotate2_t*)&hdr[1];

			roffs[num_roffs].type = 2; //rww - any reason this wasn't being set already?

			// Copy all of the goods into our ROFF cache
			for (int i = 0; i < count; i++)
			{
				VectorCopy(roff_data[i].origin_delta, roffs[num_roffs].data[i].origin_delta);
				VectorCopy(roff_data[i].rotate_delta, roffs[num_roffs].data[i].rotate_delta);

				roffs[num_roffs].data[i].mStartNote = roff_data[i].mStartNote;
				roffs[num_roffs].data[i].mNumNotes = roff_data[i].mNumNotes;
				roffs[num_roffs].NumData++;
			}

			if (hdr->mNumNotes)
			{
				Com_Printf(S_COLOR_RED"NoteTracks for ROFFs not implimented\n");
			}
			return qtrue;
		}
	}

	return qfalse;
}

//-------------------------------------------------------
// G_LoadRoffs
//
// Does the fun work of loading and caching a roff file
//	If the file is already cached, it just returns an
//	ID to the cached file.
//-------------------------------------------------------

int G_LoadRoffs(const char* fileName)
{
	char file[MAX_QPATH];
	char data[ROFF_INFO_SIZENEW];
	fileHandle_t f;
	roff_hdr2_t* header;
	int len, roff_id = 0;

	// Before even bothering with all of this, make sure we have a place to store it.
	if (num_roffs >= MAX_ROFFSNEW)
	{
		Com_Printf(S_COLOR_RED"MAX_ROFFS count exceeded.  Skipping load of .ROF '%s'\n", fileName);
		return roff_id;
	}

	// The actual path
	sprintf(file, "%s/%s.rof", Q3_SCRIPT_DIR, fileName);

	// See if I'm already precached
	for (int i = 0; i < num_roffs; i++)
	{
		if (stricmp(file, roffs[i].fileName) == 0)
		{
			// Good, just return me...avoid zero index
			return i + 1;
		}
	}

#ifdef _DEBUG
	//	Com_Printf( S_COLOR_GREEN"Caching ROF: '%s'\n", file );
#endif

	// Read the file in one fell swoop
	len = trap->FS_Open(file, &f, FS_READ);

	if (len <= 0)
	{
		Com_Printf(S_COLOR_RED"Could not open .ROF file '%s'\n", fileName);
		return roff_id;
	}

	if (len >= ROFF_INFO_SIZENEW)
	{
		Com_Printf(S_COLOR_RED".ROF file '%s': Too large for file buffer.\n", fileName);
		return roff_id;
	}

	trap->FS_Read(data, len, f); //read data in buffer

	trap->FS_Close(f); //close file

	// Now let's check the header info...
	header = (roff_hdr2_t*)data;

	// ..and make sure it's reasonably valid
	if (!G_ValidRoff(header))
	{
		Com_Printf(S_COLOR_RED"Invalid roff format '%s'\n", fileName);
	}
	else
	{
		G_InitRoff(file, data);

		// Done loading this roff, so save off an id to it..increment first to avoid zero index
		roff_id = ++num_roffs;
	}

	return roff_id;
}

//-------------------------------------------------------
// G_Roffs
//
// Handles applying the roff data to the specified ent
//-------------------------------------------------------

void G_Roffs(gentity_t* ent)
{
	//updates roff scripting for this entity.
	vec3_t org, ang;

	if (!ent->next_roff_time)
	{
		return;
	}

	if (ent->next_roff_time > level.time)
	{
		// either I don't think or it's just not time to have me think yet
		return;
	}

	const int roff_id = G_LoadRoffs(ent->roffname);

	if (!roff_id)
	{
		// Couldn't cache this rof
		return;
	}

	// The ID is one higher than the array index
	const roff_list_t* roff = &roffs[roff_id - 1];

	if (roff->type == 2)
	{
		const move_rotate2_t* data = &roffs[roff_id - 1].data[ent->roff_ctr];
		VectorCopy(data->origin_delta, org);
		VectorCopy(data->rotate_delta, ang);
		if (data->mStartNote != -1 || data->mNumNotes)
		{
			G_RoffNotetrackCallback(ent, roffs[roff_id - 1].mNoteTrackIndexes[data->mStartNote]);
		}
	}
	else
	{
		const move_rotate2_t* data = &roffs[roff_id - 1].data[ent->roff_ctr];
		VectorCopy(data->origin_delta, org);
		VectorCopy(data->rotate_delta, ang);
	}

#ifdef _DEBUG
	if (developer.integer)
	{
		Com_Printf(S_COLOR_GREEN"ROFF dat: num: %d o:<%.2f %.2f %.2f> a:<%.2f %.2f %.2f>\n",
			ent->roff_ctr,
			org[0], org[1], org[2],
			ang[0], ang[1], ang[2]);
	}
#endif

	if (ent->client)
	{
		// Set up the angle interpolation
		//-------------------------------------
		VectorAdd(ent->s.apos.trBase, ang, ent->s.apos.trBase);
		ent->s.apos.trTime = level.time;
		ent->s.apos.trType = TR_INTERPOLATE;

		// Store what the next apos->trBase should be
		VectorCopy(ent->s.apos.trBase, ent->client->ps.viewangles);
		VectorCopy(ent->s.apos.trBase, ent->r.currentAngles);
		VectorCopy(ent->s.apos.trBase, ent->s.angles);
		if (ent->NPC)
		{
			ent->NPC->desiredYaw = ent->s.apos.trBase[YAW];
		}

		// Set up the origin interpolation
		//-------------------------------------
		VectorAdd(ent->s.pos.trBase, org, ent->s.pos.trBase);
		ent->s.pos.trTime = level.time;
		ent->s.pos.trType = TR_INTERPOLATE;

		// Store what the next pos->trBase should be
		VectorCopy(ent->s.pos.trBase, ent->client->ps.origin);
		VectorCopy(ent->s.pos.trBase, ent->r.currentOrigin);
	}
	else
	{
		// Set up the angle interpolation
		//-------------------------------------
		VectorScale(ang, roff->mLerp, ent->s.apos.trDelta);
		VectorCopy(ent->pos2, ent->s.apos.trBase);
		ent->s.apos.trTime = level.time;
		ent->s.apos.trType = TR_LINEAR;

		// Store what the next apos->trBase should be
		VectorAdd(ent->pos2, ang, ent->pos2);

		// Set up the origin interpolation
		//-------------------------------------
		VectorScale(org, roff->mLerp, ent->s.pos.trDelta);
		VectorCopy(ent->pos1, ent->s.pos.trBase);
		ent->s.pos.trTime = level.time;
		ent->s.pos.trType = TR_LINEAR;

		// Store what the next apos->trBase should be
		VectorAdd(ent->pos1, org, ent->pos1);

		//make it true linear... FIXME: sticks around after ROFF is done, but do we really care?
		ent->alt_fire = qtrue;

		if (!ent->think
			&& ent->s.eType != ET_MISSILE
			&& ent->s.eType != ET_ITEM
			&& ent->s.eType != ET_MOVER)
		{
			//will never set currentAngles & currentOrigin itself ( why do we limit which one's get set?, just set all the time? )
			BG_EvaluateTrajectory(&ent->s.apos, level.time, ent->r.currentAngles);
			BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->r.currentOrigin);
		}
	}

	// Link just in case.
	trap->LinkEntity((sharedEntity_t*)ent);

	// See if the ROFF playback is done
	//-------------------------------------
	if (++ent->roff_ctr >= roff->frames)
	{
		// We are done, so let me think no more, then tell the task that we're done.
		ent->next_roff_time = 0;

		// Stop any rotation or movement.
		VectorClear(ent->s.pos.trDelta);
		VectorClear(ent->s.apos.trDelta);

		trap->ICARUS_TaskIDComplete((sharedEntity_t*)ent, TID_MOVE_NAV);

		return;
	}

	ent->next_roff_time = level.time + roff->mFrameTime;
}