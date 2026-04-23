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

// cg_snapshot.c -- things that happen on snapshot transition,
// not necessarily every single frame

#include "cg_headers.h"

/*
==================
CG_ResetEntity
==================
*/
static void CG_ResetEntity(centity_t* cent)
{
	// if an event is set, assume it is new enough to use
	// if the event had timed out, it would have been cleared
	cent->previousEvent = 0;

	//	cent->trailTime = cg.snap->serverTime;

	VectorCopy(cent->currentState.origin, cent->lerpOrigin);
	VectorCopy(cent->currentState.angles, cent->lerpAngles);
	if (cent->currentState.eType == ET_PLAYER)
	{
		CG_ResetPlayerEntity(cent);
	}
}

/*
===============
CG_TransitionEntity

cent->nextState is moved to cent->currentState and events are fired
===============
*/
static void CG_TransitionEntity(centity_t* cent)
{
	if (cent->nextState)
	{
		cent->currentState = *cent->nextState;
	}
	cent->currentValid = qtrue;

	// reset if the entity wasn't in the last frame or was teleported
	if (!cent->interpolate)
	{
		CG_ResetEntity(cent);
	}

	// clear the next state.  if will be set by the next CG_SetNextSnap
	cent->interpolate = qfalse;

	if (cent->currentState.number != 0)
	{
		// check for events
		CG_CheckEvents(cent);
	}
}

/*
==================
CG_SetInitialSnapshot

This will only happen on the very first snapshot, or
on tourney restarts.  All other times will use
CG_TransitionSnapshot instead.
==================
*/
static void CG_SetInitialSnapshot(snapshot_t* snap)
{
	cg.snap = snap;

	// sort out solid entities
	//CG_BuildSolidList();

	CG_ExecuteNewServerCommands(snap->serverCommandSequence);

	// set our local weapon selection pointer to
	// what the server has indicated the current weapon is
	CG_Respawn();

	for (int i = 0; i < cg.snap->numEntities; i++)
	{
		const entityState_t* state = &cg.snap->entities[i];
		centity_t* cent = &cg_entities[state->number];

		cent->currentState = *state;
		cent->interpolate = qfalse;
		cent->currentValid = qtrue;

		CG_ResetEntity(cent);

		// check for events
		CG_CheckEvents(cent);
	}
}

/*
===================
CG_TransitionSnapshot

The transition point from snap to nextSnap has passed
===================
*/
static void CG_TransitionSnapshot()
{
	centity_t* cent;
	int i;

	if (!cg.snap)
	{
		CG_Error("CG_TransitionSnapshot: NULL cg.snap");
	}
	if (!cg.nextSnap)
	{
		CG_Error("CG_TransitionSnapshot: NULL cg.nextSnap");
	}

	// execute any server string commands before transitioning entities
	CG_ExecuteNewServerCommands(cg.nextSnap->serverCommandSequence);

	// clear the currentValid flag for all entities in the existing snapshot
	for (i = 0; i < cg.snap->numEntities; i++)
	{
		cent = &cg_entities[cg.snap->entities[i].number];
		cent->currentValid = qfalse;
	}

	// move nextSnap to snap and do the transitions
	snapshot_t* oldFrame = cg.snap;
	cg.snap = cg.nextSnap;

	// sort out solid entities
	//CG_BuildSolidList();

	for (i = 0; i < cg.snap->numEntities; i++)
	{
		if (true)
			//cg.snap->entities[ i ].number != 0 ) // I guess the player adds his/her events elsewhere, so doing this also gives us double events for the player!
		{
			cent = &cg_entities[cg.snap->entities[i].number];
			CG_TransitionEntity(cent);
		}
	}

	cg.nextSnap = nullptr;

	// check for playerstate transition events
	if (oldFrame)
	{
		// if we are not doing client side movement prediction for any
		// reason, then the client events and view changes will be issued now
		//if ( cg_timescale.value >= 1.0f )
		{
			CG_TransitionPlayerState(&cg.snap->ps, &oldFrame->ps);
		}
	}
}

/*
===============
CG_SetEntityNextState

Determine if the entity can be interpolated between the states
present in cg.snap and cg,nextSnap
===============
*/
static void CG_SetEntityNextState(centity_t* cent, const entityState_t* state)
{
	cent->nextState = state;

	// since we can't interpolate ghoul2 stuff from one frame to another, I'm just going to copy the ghoul2 info directly into the current state now
	//	CGhoul2Info *currentModel = &state->ghoul2[1];
	//	cent->gent->ghoul2 = state->ghoul2;
	//	CGhoul2Info *newModel = &cent->gent->ghoul2[1];

	// if this frame is a teleport, or the entity wasn't in the
	// previous frame, don't interpolate
	if (!cent->currentValid || (cent->currentState.eFlags ^ state->eFlags) & EF_TELEPORT_BIT)
	{
		cent->interpolate = qfalse;
	}
	else
	{
		cent->interpolate = qtrue;
	}
}

/*
===================
CG_SetNextSnap

A new snapshot has just been read in from the client system.
===================
*/
static void CG_SetNextSnap(snapshot_t* snap)
{
	cg.nextSnap = snap;

	// check for extrapolation errors
	for (int num = 0; num < snap->numEntities; num++)
	{
		const entityState_t* es = &snap->entities[num];
		centity_t* cent = &cg_entities[es->number];
		CG_SetEntityNextState(cent, es);
	}

	// if the next frame is a teleport for the playerstate,
	if (cg.snap && (snap->ps.eFlags ^ cg.snap->ps.eFlags) & EF_TELEPORT_BIT)
	{
		cg.nextFrameTeleport = qtrue;
	}
	else
	{
		cg.nextFrameTeleport = qfalse;
	}
}

/*
========================
CG_ReadNextSnapshot

This is the only place new snapshots are requested
This may increment cg.processedSnapshotNum multiple
times if the client system fails to return a
valid snapshot.
========================
*/
static snapshot_t* CG_ReadNextSnapshot()
{
	snapshot_t* dest;

	while (cg.processedSnapshotNum < cg.latestSnapshotNum)
	{
		// decide which of the two slots to load it into
		if (cg.snap == &cg.activeSnapshots[0])
		{
			dest = &cg.activeSnapshots[1];
		}
		else
		{
			dest = &cg.activeSnapshots[0];
		}

		// try to read the snapshot from the client system
		cg.processedSnapshotNum++;
		const qboolean r = cgi_GetSnapshot(cg.processedSnapshotNum, dest);

		// if it succeeded, return
		if (r)
		{
			return dest;
		}

		// a GetSnapshot will return failure if the snapshot
		// never arrived, or  is so old that its entities
		// have been shoved off the end of the circular
		// buffer in the client system.

		// record as a dropped packet
		//		CG_AddLagometerSnapshotInfo( NULL );

		// If there are additional snapshots, continue trying to
		// read them.
	}

	// nothing left to read
	return nullptr;
}

/*
=================
CG_RestartLevel

A tournement restart will clear everything, but doesn't
require a reload of all the media
=================
*/
extern void CG_LinkCentsToGents();

static void CG_RestartLevel()
{
	const int snapshotNum = cg.processedSnapshotNum;

	memset(cg_entities, 0, sizeof cg_entities);
	CG_Init_CG();

	CG_LinkCentsToGents();
	CG_InitLocalEntities();
	CG_InitMarkPolys();

	// regrab the first snapshot of the restart

	cg.processedSnapshotNum = snapshotNum;
	const int r = cgi_GetSnapshot(cg.processedSnapshotNum, &cg.activeSnapshots[0]);
	if (!r)
	{
		CG_Error("cgi_GetSnapshot failed on restart");
	}

	CG_SetInitialSnapshot(&cg.activeSnapshots[0]);
	cg.time = cg.snap->serverTime;
}

/*
============
CG_ProcessSnapshots

We are trying to set up a renderable view, so determine
what the simulated time is, and try to get snapshots
both before and after that time if available.

If we don't have a valid cg.snap after exiting this function,
then a 3D game view cannot be rendered.  This should only happen
right after the initial connection.  After cg.snap has been valid
once, it will never turn invalid.

Even if cg.snap is valid, cg.nextSnap may not be, if the snapshot
hasn't arrived yet (it becomes an extrapolating situation instead
of an interpolating one)

============
*/
void CG_ProcessSnapshots(void)
{
	snapshot_t* snap = NULL;
	int latestNum = 0;

	// ------------------------------------------------------------------
	// Query latest snapshot number from engine
	// ------------------------------------------------------------------
	cgi_GetCurrentSnapshotNumber(&latestNum, &cg.latestSnapshotTime);

	if (latestNum != cg.latestSnapshotNum)
	{
		if (latestNum < cg.latestSnapshotNum)
		{
			Com_Printf("CG_ProcessSnapshots WARNING: latestNum < cg.latestSnapshotNum\n");
			return;
		}
		cg.latestSnapshotNum = latestNum;
	}

	// ------------------------------------------------------------------
	// If we have not yet received our first snapshot, try to read one
	// ------------------------------------------------------------------
	if (cg.snap == NULL)
	{
		snap = CG_ReadNextSnapshot();
		if (snap == NULL)
		{
			// Cannot continue until we get a snapshot
			return;
		}

		// Initialize client state from first snapshot
		CG_SetInitialSnapshot(snap);
	}

	// ------------------------------------------------------------------
	// Main snapshot processing loop
	// ------------------------------------------------------------------
	while (qtrue)
	{
		// If we do not have a next snapshot, try to read one
		if (cg.nextSnap == NULL)
		{
			snap = CG_ReadNextSnapshot();

			if (snap == NULL)
			{
				// No next snapshot available → must extrapolate
				break;
			}

			CG_SetNextSnap(snap);

			// Level restart detection: time went backwards
			if (cg.nextSnap->serverTime < cg.snap->serverTime)
			{
				Com_Printf("CG_ProcessSnapshots: Level restart detected\n");
				CG_RestartLevel();
				continue;
			}
		}

		// If current time is before next snapshot time → interpolation OK
		if (cg.time < cg.nextSnap->serverTime)
		{
			break;
		}

		// We have passed the transition point → move nextSnap → snap
		CG_TransitionSnapshot();
	}

	// ------------------------------------------------------------------
	// Time correction: ensure cg.time is within valid bounds
	// ------------------------------------------------------------------
	if (cg.snap != NULL && cg.snap->serverTime > cg.time)
	{
		cg.time = cg.snap->serverTime;
#if _DEBUG
		Com_Printf("CG_ProcessSnapshots: corrected cg.time to cg.snap->serverTime\n");
#endif
	}

	if (cg.nextSnap != NULL && cg.nextSnap->serverTime <= cg.time)
	{
		cg.time = cg.nextSnap->serverTime - 1;
#if _DEBUG
		Com_Printf("CG_ProcessSnapshots: corrected cg.time to nextSnap->serverTime - 1\n");
#endif
	}

	// ------------------------------------------------------------------
	// Final safety checks (debug only)
	// ------------------------------------------------------------------
	if (cg.snap == NULL)
	{
		Com_Printf("CG_ProcessSnapshots ERROR: cg.snap == NULL\n");
		return;
	}

	if (cg.snap->serverTime > cg.time)
	{
		Com_Printf("CG_ProcessSnapshots ERROR: cg.snap->serverTime > cg.time\n");
	}

	if (cg.nextSnap != NULL && cg.nextSnap->serverTime <= cg.time)
	{
		Com_Printf("CG_ProcessSnapshots ERROR: cg.nextSnap->serverTime <= cg.time\n");
	}
}