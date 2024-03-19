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

// cg_predict.c -- this file generates cg.predictedPlayerState by either
// interpolating between snapshots from the server or locally predicting
// ahead the client's movement

// this line must stay at top so the whole PCH thing works...
#include "cg_headers.h"

#include "cg_media.h"

#include "../game/g_vehicles.h"

static pmove_t cg_pmove;

static int cg_numSolidEntities;
static centity_t* cg_solidEntities[MAX_ENTITIES_IN_SNAPSHOT];

/*
====================
CG_BuildSolidList

When a new cg.snap has been set, this function builds a sublist
of the entities that are actually solid, to make for more
efficient collision detection
====================
*/
void CG_BuildSolidList()
{
	int i;
	centity_t* cent;

	cg_numSolidEntities = 0;

	if (!cg.snap)
	{
		return;
	}

	for (i = 0; i < cg.snap->numEntities; i++)
	{
		if (cg.snap->entities[i].number < ENTITYNUM_WORLD)
		{
			cent = &cg_entities[cg.snap->entities[i].number];

			if (cent->gent != nullptr && cent->gent->s.solid)
			{
				cg_solidEntities[cg_numSolidEntities] = cent;
				cg_numSolidEntities++;
			}
		}
	}

	float dsquared = 5000 + 500;
	dsquared *= dsquared;

	for (i = 0; i < cg_numpermanents; i++)
	{
		vec3_t difference;
		cent = cg_permanents[i];
		VectorSubtract(cent->lerpOrigin, cg.snap->ps.origin, difference);
		if (cent->currentState.eType == ET_TERRAIN ||
			difference[0] * difference[0] + difference[1] * difference[1] + difference[2] * difference[2] <=
			dsquared)
		{
			cent->currentValid = qtrue;
			if (cent->nextState && cent->nextState->solid)
			{
				cg_solidEntities[cg_numSolidEntities] = cent;
				cg_numSolidEntities++;
			}
		}
		else
		{
			cent->currentValid = qfalse;
		}
	}
}

/*
====================
CG_ClipMoveToEntities

====================
*/
void CG_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
	const int skip_number, const int mask, trace_t* tr)
{
	trace_t trace;
	clipHandle_t cmodel;
	vec3_t bmins{}, bmaxs{};

	for (int i = 0; i < cg_numSolidEntities; i++)
	{
		vec3_t angles;
		vec3_t origin;
		const centity_t* cent = cg_solidEntities[i];
		const entityState_t* ent = &cent->currentState;

		if (ent->number == skip_number)
		{
			continue;
		}

		if (ent->eType == ET_PUSH_TRIGGER)
		{
			continue;
		}
		if (ent->eType == ET_TELEPORT_TRIGGER)
		{
			continue;
		}

		if (ent->solid == SOLID_BMODEL)
		{
			// special value for bmodel
			cmodel = cgi_CM_InlineModel(ent->modelindex);
			VectorCopy(cent->lerpAngles, angles);

			//Hmm... this would cause traces against brush movers to snap at 20fps (as with the third person camera)...
			//Let's use the lerpOrigin for now and see if it breaks anything...
			//EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, origin );
			VectorCopy(cent->lerpOrigin, origin);
		}
		else
		{
			// encoded bbox
			const int x = ent->solid & 255;
			const int zd = ent->solid >> 8 & 255;
			const int zu = (ent->solid >> 16 & 255) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			cmodel = cgi_CM_TempBoxModel(bmins, bmaxs); //, cent->gent->contents );
			VectorCopy(vec3_origin, angles);
			VectorCopy(cent->lerpOrigin, origin);
		}

		cgi_CM_TransformedBoxTrace(&trace, start, end,
			mins, maxs, cmodel, mask, origin, angles);

		if (trace.allsolid || trace.fraction < tr->fraction)
		{
			trace.entityNum = ent->number;
			*tr = trace;
		}
		else if (trace.startsolid)
		{
			tr->startsolid = qtrue;
		}
		if (tr->allsolid)
		{
			return;
		}
	}
}

/*
================
CG_Trace
================
*/
void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
	const int skip_number, const int mask)
{
	trace_t t;

	cgi_CM_BoxTrace(&t, start, end, mins, maxs, 0, mask);
	t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	// check all other solid models
	CG_ClipMoveToEntities(start, mins, maxs, end, skip_number, mask, &t);

	*result = t;
}

/*
================
CG_PointContents
================
*/

#define USE_SV_PNT_CONTENTS (1)

#if USE_SV_PNT_CONTENTS
int CG_PointContents(const vec3_t point, const int passEntityNum)
{
	return gi.pointcontents(point, passEntityNum);
}
#else
int		CG_PointContents(const vec3_t point, int passEntityNum) {
	int			i;
	entityState_t* ent;
	centity_t* cent;
	clipHandle_t cmodel;
	int			contents;

	contents = cgi_CM_PointContents(point, 0);

	for (i = 0; i < cg_numSolidEntities; i++) {
		cent = cg_solidEntities[i];

		ent = &cent->currentState;

		if (ent->number == passEntityNum) {
			continue;
		}

		if (ent->solid != SOLID_BMODEL) { // special value for bmodel
			continue;
		}

		cmodel = cgi_CM_InlineModel(ent->modelindex);
		if (!cmodel) {
			continue;
		}

		contents |= cgi_CM_TransformedPointContents(point, cmodel, ent->origin, ent->angles);
	}

	return contents;
}
#endif

void CG_SetClientViewAngles(vec3_t angles, const qboolean override_view_ent)
{
	if (cg.snap->ps.viewEntity <= 0 || cg.snap->ps.viewEntity >= ENTITYNUM_WORLD || override_view_ent)
	{
		//don't clamp angles when looking through a viewEntity
		for (int i = 0; i < 3; i++)
		{
			cg.predictedPlayerState.viewangles[i] = angles[i];
			cg.predictedPlayerState.delta_angles[i] = 0;
			cg.snap->ps.viewangles[i] = angles[i];
			cg.snap->ps.delta_angles[i] = 0;
			g_entities[0].client->pers.cmd_angles[i] = ANGLE2SHORT(angles[i]);
		}
		cgi_SetUserCmdAngles(angles[PITCH], angles[YAW], angles[ROLL]);
	}
}

extern qboolean PM_AdjustAnglesToGripper(gentity_t* gent, usercmd_t* cmd);
extern qboolean PM_AdjustAnglesForSpinningFlip(gentity_t* ent, usercmd_t* ucmd, qboolean angles_only);
extern qboolean G_CheckClampUcmd(gentity_t* ent, usercmd_t* ucmd);
extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);

qboolean CG_CheckModifyUCmd(usercmd_t* cmd, vec3_t viewangles)
{
	qboolean overridAngles = qfalse;
	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		//controlling something else
		memset(cmd, 0, sizeof(usercmd_t));
		VectorCopy(g_entities[0].pos4, viewangles);
		overridAngles = qtrue;
	}
	else if (G_IsRidingVehicle(&g_entities[0]))
	{
		overridAngles = qtrue;
	}

	if (g_entities[0].inuse && g_entities[0].client)
	{
		if (!PM_AdjustAnglesToGripper(&g_entities[0], cmd))
		{
			if (PM_AdjustAnglesForSpinningFlip(&g_entities[0], cmd, qtrue))
			{
				CG_SetClientViewAngles(g_entities[0].client->ps.viewangles, qfalse);
				if (viewangles)
				{
					VectorCopy(g_entities[0].client->ps.viewangles, viewangles);
					overridAngles = qtrue;
				}
			}
		}
		else
		{
			CG_SetClientViewAngles(g_entities[0].client->ps.viewangles, qfalse);
			if (viewangles)
			{
				VectorCopy(g_entities[0].client->ps.viewangles, viewangles);
				overridAngles = qtrue;
			}
		}
		if (G_CheckClampUcmd(&g_entities[0], cmd))
		{
			CG_SetClientViewAngles(g_entities[0].client->ps.viewangles, qfalse);
			if (viewangles)
			{
				VectorCopy(g_entities[0].client->ps.viewangles, viewangles);
				overridAngles = qtrue;
			}
		}
	}
	return overridAngles;
}

qboolean CG_OnMovingPlat(const playerState_t* ps)
{
	if (ps->groundEntityNum != ENTITYNUM_NONE)
	{
		const entityState_t* es = &cg_entities[ps->groundEntityNum].currentState;
		if (es->eType == ET_MOVER)
		{
			//on a mover
			if (es->pos.trType != TR_STATIONARY)
			{
				if (es->pos.trType != TR_LINEAR_STOP && es->pos.trType != TR_NONLINEAR_STOP)
				{
					//a constant mover
					if (!VectorCompare(vec3_origin, es->pos.trDelta))
					{
						//is moving
						return qtrue;
					}
				}
				else
				{
					//a linear-stop mover
					if (es->pos.trTime + es->pos.trDuration > cg.time)
					{
						//still moving
						return qtrue;
					}
				}
			}
		}
	}
	return qfalse;
}

/*
========================
CG_InterpolatePlayerState

Generates cg.predictedPlayerState by interpolating between
cg.snap->player_state and cg.nextFrame->player_state
========================
*/
void CG_InterpolatePlayerState(const qboolean grab_angles)
{
	int i;
	vec3_t old_org;

	playerState_t* out = &cg.predictedPlayerState;
	const snapshot_t* prev = cg.snap;
	const snapshot_t* next = cg.nextSnap;

	VectorCopy(out->origin, old_org);
	*out = cg.snap->ps;

	// if we are still allowing local input, short circuit the view angles
	if (grab_angles)
	{
		usercmd_t cmd;

		const int cmdNum = cgi_GetCurrentCmdNumber();
		cgi_GetUserCmd(cmdNum, &cmd);

		const qboolean skip = CG_CheckModifyUCmd(&cmd, out->viewangles);

		if (!skip)
		{
			//NULL so that it doesn't execute a block of code that must be run from game
			PM_UpdateViewAngles(out->saber_anim_level, out, &cmd, nullptr);
		}
	}

	// if the next frame is a teleport, we can't lerp to it
	if (cg.nextFrameTeleport)
	{
		return;
	}

	if (!(!next || next->serverTime <= prev->serverTime))
	{
		const float f = static_cast<float>(cg.time - prev->serverTime) / (next->serverTime - prev->serverTime);

		i = next->ps.bobCycle;
		if (i < prev->ps.bobCycle)
		{
			i += 256; // handle wraparound
		}
		out->bobCycle = prev->ps.bobCycle + f * (i - prev->ps.bobCycle);

		for (i = 0; i < 3; i++)
		{
			out->origin[i] = prev->ps.origin[i] + f * (next->ps.origin[i] - prev->ps.origin[i]);
			if (!grab_angles)
			{
				out->viewangles[i] = LerpAngle(
					prev->ps.viewangles[i], next->ps.viewangles[i], f);
			}
			out->velocity[i] = prev->ps.velocity[i] +
				f * (next->ps.velocity[i] - prev->ps.velocity[i]);
		}
	}

	bool on_plat = false;
	const centity_t* pent = nullptr;
	if (out->groundEntityNum > 0)
	{
		pent = &cg_entities[out->groundEntityNum];
		if (pent->currentState.eType == ET_MOVER)

		{
			on_plat = true;
		}
	}

	if (
		cg.validPPS &&
		cg_smoothPlayerPos.value > 0.0f &&
		cg_smoothPlayerPos.value < 1.0f &&
		!on_plat
		)
	{
		// 0 = no smoothing, 1 = no movement
		for (i = 0; i < 3; i++)
		{
			out->origin[i] = cg_smoothPlayerPos.value * (old_org[i] - out->origin[i]) + out->origin[i];
		}
	}
	else if (on_plat && cg_smoothPlayerPlat.value > 0.0f && cg_smoothPlayerPlat.value < 1.0f)
	{
		//		if (cg.frametime<150)
		//		{
		assert(pent);
		vec3_t p1, p2, vel{};
		float lerp_time;

		EvaluateTrajectory(&pent->currentState.pos, cg.snap->serverTime, p1);
		if (cg.nextSnap && cg.nextSnap->serverTime > cg.snap->serverTime && pent->nextState)
		{
			EvaluateTrajectory(&pent->nextState->pos, cg.nextSnap->serverTime, p2);
			lerp_time = static_cast<float>(cg.nextSnap->serverTime - cg.snap->serverTime);
		}
		else
		{
			EvaluateTrajectory(&pent->currentState.pos, cg.snap->serverTime + 50, p2);
			lerp_time = 50.0f;
		}

		float accel = cg_smoothPlayerPlatAccel.value * cg.frametime / lerp_time;

		if (accel > 20.0f)
		{
			accel = 20.0f;
		}

		for (i = 0; i < 3; i++)
		{
			vel[i] = accel * (p2[i] - p1[i]);
		}

		VectorAdd(out->origin, vel, out->origin);

		if (cg.validPPS &&
			cg_smoothPlayerPlat.value > 0.0f &&
			cg_smoothPlayerPlat.value < 1.0f
			)
		{
			// 0 = no smoothing, 1 = no movement
			for (i = 0; i < 3; i++)
			{
				out->origin[i] = cg_smoothPlayerPlat.value * (old_org[i] - out->origin[i]) + out->origin[i];
			}
		}
		//		}
	}
}

/*
===================
CG_TouchItem
===================
*/
void CG_TouchItem(centity_t* cent)
{
	// never pick an item up twice in a prediction
	if (cent->miscTime == cg.time)
	{
		return;
	}

	if (!BG_PlayerTouchesItem(&cg.predictedPlayerState, &cent->currentState, cg.time))
	{
		return;
	}

	if (!BG_CanItemBeGrabbed(&cent->currentState, &cg.predictedPlayerState))
	{
		return; // can't hold it
	}

	const gitem_t* item = &bg_itemlist[cent->currentState.modelindex];

	// grab it
	AddEventToPlayerstate(EV_ITEM_PICKUP, cent->currentState.modelindex, &cg.predictedPlayerState);

	// remove it from the frame so it won't be drawn
	cent->currentState.eFlags |= EF_NODRAW;

	// don't touch it again this prediction
	cent->miscTime = cg.time;

	// if its a weapon, give them some predicted ammo so the autoswitch will work
	if (item->giType == IT_WEAPON)
	{
		const int ammotype = weaponData[item->giTag].ammoIndex;
		cg.predictedPlayerState.weapons[item->giTag] = 1;
		if (!cg.predictedPlayerState.ammo[ammotype])
		{
			cg.predictedPlayerState.ammo[ammotype] = 1;
		}
	}
}

/*
=========================
CG_TouchTriggerPrediction

Predict push triggers and items
Only called for the last command
=========================
*/
void CG_TouchTriggerPrediction()
{
	trace_t trace;

	// dead clients don't activate triggers
	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	const auto spectator = static_cast<qboolean>(cg.predictedPlayerState.pm_type == PM_SPECTATOR);

	if (cg.predictedPlayerState.pm_type != PM_NORMAL && !spectator)
	{
		return;
	}

	for (int i = 0; i < cg.snap->numEntities; i++)
	{
		centity_t* cent = &cg_entities[cg.snap->entities[i].number];
		const entityState_t* ent = &cent->currentState;

		if (ent->eType == ET_ITEM && !spectator)
		{
			CG_TouchItem(cent);
			continue;
		}

		if (ent->eType != ET_PUSH_TRIGGER && ent->eType != ET_TELEPORT_TRIGGER)
		{
			continue;
		}

		if (ent->solid != SOLID_BMODEL)
		{
			continue;
		}

		const clipHandle_t cmodel = cgi_CM_InlineModel(ent->modelindex);
		if (!cmodel)
		{
			continue;
		}

		cgi_CM_BoxTrace(&trace, cg.predictedPlayerState.origin, cg.predictedPlayerState.origin,
			cg_pmove.mins, cg_pmove.maxs, cmodel, -1);

		if (!trace.startsolid)
		{
			continue;
		}

		if (ent->eType == ET_TELEPORT_TRIGGER)
		{
			cg.hyperspace = qtrue;
		}
		else
		{
			// we hit this push trigger
			if (spectator)
			{
				continue;
			}

			VectorCopy(ent->origin2, cg.predictedPlayerState.velocity);
		}
	}
}

/*
=================
CG_PredictPlayerState

Generates cg.predictedPlayerState for the current cg.time
cg.predictedPlayerState is guaranteed to be valid after exiting.

For normal gameplay, it will be the result of predicted usercmd_t on
top of the most recent playerState_t received from the server.

Each new refdef will usually have exactly one new usercmd over the last,
but we have to simulate all unacknowledged commands since the last snapshot
received.  This means that on an internet connection, quite a few
pmoves may be issued each frame.

OPTIMIZE: don't re-simulate unless the newly arrived snapshot playerState_t
differs from the predicted one.

We detect prediction errors and allow them to be decayed off over several frames
to ease the jerk.
=================
*/
extern qboolean player_locked;
extern qboolean PlayerAffectedByStasis();

void CG_PredictPlayerState()
{
	cg.hyperspace = qfalse; // will be set if touching a trigger_teleport

	// if this is the first frame we must guarantee
	// predictedPlayerState is valid even if there is some
	// other error condition
	if (!cg.validPPS)
	{
		cg.validPPS = qtrue;
		cg.predictedPlayerState = cg.snap->ps;
	}

	if (true) //cg_timescale.value >= 1.0f )
	{
		// non-predicting local movement will grab the latest angles
		CG_InterpolatePlayerState(qtrue);
	}
}