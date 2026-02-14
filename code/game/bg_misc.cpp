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

#include "common_headers.h"

// included in both game dll and client

#include "g_local.h"
#include "bg_public.h"
#include "g_items.h"
#include "g_vehicles.h"

extern weaponData_t weaponData[WP_NUM_WEAPONS];
extern ammoData_t ammoData[AMMO_MAX];

constexpr auto PICKUPSOUND = "sound/weapons/w_pkup.wav";

gitem_t bg_itemlist[ITM_NUM_ITEMS + 1]; //need a null on the end
const int bg_numItems = ITM_NUM_ITEMS;

/*
===============
FindItemForWeapon

===============
*/
gitem_t* FindItemForWeapon(const weapon_t weapon)
{
	for (int i = 1; i < bg_numItems; i++)
	{
		if (bg_itemlist[i].giType == IT_WEAPON && bg_itemlist[i].giTag == weapon)
		{
			return &bg_itemlist[i];
		}
	}

	Com_Error(ERR_DROP, "Couldn't find item for weapon %i", weapon);
}

//----------------------------------------------
gitem_t* FindItemForInventory(const int inv)
{
	// Now just check for any other kind of item.
	for (int i = 1; i < bg_numItems; i++)
	{
		gitem_t* it = &bg_itemlist[i];

		if (it->giType == IT_HOLDABLE)
		{
			if (it->giTag == inv)
			{
				return it;
			}
		}
	}

	Com_Error(ERR_DROP, "Couldn't find item for inventory %i", inv);
}

/*
===============
FindItemForWeapon

===============
*/
gitem_t* FindItemForAmmo(const ammo_t ammo)
{
	for (int i = 1; i < bg_numItems; i++)
	{
		if (bg_itemlist[i].giType == IT_AMMO && bg_itemlist[i].giTag == ammo)
		{
			return &bg_itemlist[i];
		}
	}

	Com_Error(ERR_DROP, "Couldn't find item for ammo %i", ammo);
}

/*
===============
FindItem

===============
*/
gitem_t* FindItem(const char* className)
{
	for (int i = 1; i < bg_numItems; i++)
	{
		if (!Q_stricmp(bg_itemlist[i].classname, className))
			return &bg_itemlist[i];
	}

	return nullptr;
}

/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.
================
*/
qboolean BG_CanItemBeGrabbed(const entityState_t* ent, const playerState_t* ps)
{
	if (ent->modelindex < 1 || ent->modelindex >= bg_numItems)
	{
		Com_Error(ERR_DROP, "BG_CanItemBeGrabbed: index out of range");
	}

	const gitem_t* item = &bg_itemlist[ent->modelindex];

	switch (item->giType)
	{
	case IT_WEAPON:
		// See if we already have this weapon.
	{
		// See if we already have this weapon.
		if (!ps->weapons[item->giTag])
		{
			// Don't have this weapon yet, so pick it up.
			return qtrue;
		}
		if (item->giTag == WP_SABER)
		{
			//always pick up a saber, might be a new one?
			return qtrue;
		}
	}

	// Make sure that we aren't already full on ammo for this weapon
	if (ps->ammo[weaponData[item->giTag].ammoIndex] >= ammoData[weaponData[item->giTag].ammoIndex].max)
	{
		// full, so don't grab the item
		return qfalse;
	}

	return qtrue; // could use more of this type of ammo, so grab the item

	case IT_AMMO:

		if (item->giTag != AMMO_FORCE)
		{
			// since the ammo is the weapon in this case, picking up ammo should actually give you the weapon
			switch (item->giTag)
			{
			case AMMO_THERMAL:
				if (!ps->weapons[WP_THERMAL])
				{
					return qtrue;
				}
				break;
			case AMMO_DETPACK:
				if (!ps->weapons[WP_DET_PACK])
				{
					return qtrue;
				}
				break;
			case AMMO_TRIPMINE:
				if (!ps->weapons[WP_TRIP_MINE])
				{
					return qtrue;
				}
				break;
			default:;
			}

			if (ps->ammo[item->giTag] >= ammoData[item->giTag].max) // checkme
			{
				return qfalse; // can't hold any more
			}
		}
		else
		{
			if (ps->forcePower >= ammoData[item->giTag].max * 2)
			{
				return qfalse; // can't hold any more
			}
		}

		return qtrue;

	case IT_ARMOR:
		// we also clamp armor to the maxhealth for handicapping
		if (ps->stats[STAT_ARMOR] >= ps->stats[STAT_MAX_HEALTH])
		{
			return qfalse;
		}
		return qtrue;

	case IT_HEALTH:
		if (ps->forcePowersActive & 1 << FP_RAGE)
		{
			//ragers can't use health
			return qfalse;
		}
		// don't pick up if already at max
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return qfalse;
		}
		return qtrue;

	case IT_BATTERY:
		// don't pick up if already at max
		if (ps->batteryCharge >= MAX_BATTERIES)
		{
			return qfalse;
		}
		return qtrue;

	case IT_HOLOCRON:
		// pretty lame but for now you can always pick these up
		return qtrue;

	case IT_HOLDABLE:
		if (item->giTag >= INV_ELECTROBINOCULARS && item->giTag <= INV_SENTRY)
		{
			// hardcoded--can only pick up five of any holdable
			if (ps->inventory[item->giTag] >= 5)
			{
				return qfalse;
			}
		}
		return qtrue;

	default:
		break;
	}

	return qfalse;
}

//======================================================================

/*
================
EvaluateTrajectory

================
*/
void EvaluateTrajectory(const trajectory_t* tr, int at_time, vec3_t result)
{
	float delta_time;
	float phase;

	switch (tr->trType)
	{
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorCopy(tr->trBase, result);
		break;
	case TR_LINEAR:
		delta_time = (at_time - tr->trTime) * 0.001F; // milliseconds to seconds
		VectorMA(tr->trBase, delta_time, tr->trDelta, result);
		break;
	case TR_SINE:
		delta_time = (at_time - tr->trTime) / static_cast<float>(tr->trDuration);
		phase = sin(delta_time * M_PI * 2);
		VectorMA(tr->trBase, phase, tr->trDelta, result);
		break;
	case TR_LINEAR_STOP:
		if (at_time > tr->trTime + tr->trDuration)
		{
			at_time = tr->trTime + tr->trDuration;
		}
		//old totally linear
		delta_time = (at_time - tr->trTime) * 0.001F; // milliseconds to seconds
		if (delta_time < 0)
		{
			//going past the total duration
			delta_time = 0;
		}
		VectorMA(tr->trBase, delta_time, tr->trDelta, result);
		break;
	case TR_NONLINEAR_STOP:
		if (at_time > tr->trTime + tr->trDuration)
		{
			at_time = tr->trTime + tr->trDuration;
		}
		//new slow-down at end
		if (at_time - tr->trTime > tr->trDuration || at_time - tr->trTime <= 0)
		{
			delta_time = 0;
		}
		else
		{
			//FIXME: maybe scale this somehow?  So that it starts out faster and stops faster?
			delta_time = tr->trDuration * 0.001f * cos(DEG2RAD(
				90.0f - 90.0f * (static_cast<float>(at_time) - tr->trTime) / static_cast<float>(tr->trDuration)));
		}
		VectorMA(tr->trBase, delta_time, tr->trDelta, result);
		break;
	case TR_GRAVITY:
		delta_time = (at_time - tr->trTime) * 0.001F; // milliseconds to seconds
		VectorMA(tr->trBase, delta_time, tr->trDelta, result);
		result[2] -= 0.5F * g_gravity->value * delta_time * delta_time; //DEFAULT_GRAVITY
		break;
	default:
		Com_Error(ERR_DROP, "EvaluateTrajectory: unknown trType: %i", tr->trTime);
	}
}

/*
================
EvaluateTrajectoryDelta

Returns current speed at given time
================
*/
void EvaluateTrajectoryDelta(const trajectory_t* tr, const int at_time, vec3_t result)
{
	float delta_time;
	float phase;

	switch (tr->trType)
	{
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear(result);
		break;
	case TR_LINEAR:
		VectorCopy(tr->trDelta, result);
		break;
	case TR_SINE:
		delta_time = (at_time - tr->trTime) / static_cast<float>(tr->trDuration);
		phase = cos(delta_time * M_PI * 2); // derivative of sin = cos
		phase *= 0.5;
		VectorScale(tr->trDelta, phase, result);
		break;
	case TR_LINEAR_STOP:
		if (at_time > tr->trTime + tr->trDuration)
		{
			VectorClear(result);
			return;
		}
		VectorCopy(tr->trDelta, result);
		break;
	case TR_NONLINEAR_STOP:
		if (at_time - tr->trTime > tr->trDuration || at_time - tr->trTime <= 0)
		{
			VectorClear(result);
			return;
		}
		delta_time = tr->trDuration * 0.001f * cos(DEG2RAD(
			90.0f - 90.0f * (static_cast<float>(at_time) - tr->trTime) / static_cast<float>(tr->trDuration)));
		VectorScale(tr->trDelta, delta_time, result);
		break;
	case TR_GRAVITY:
		delta_time = (at_time - tr->trTime) * 0.001F; // milliseconds to seconds
		VectorCopy(tr->trDelta, result);
		result[2] -= g_gravity->value * delta_time; // DEFAULT_GRAVITY
		break;
	default:
		Com_Error(ERR_DROP, "EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime);
	}
}

/*
===============
AddEventToPlayerstate

Handles the sequence numbers
===============
*/
void AddEventToPlayerstate(const int new_event, const int event_parm, playerState_t* ps)
{
	ps->events[ps->eventSequence & MAX_PS_EVENTS - 1] = new_event;
	ps->eventParms[ps->eventSequence & MAX_PS_EVENTS - 1] = event_parm;
	ps->eventSequence++;
}

/*
===============
CurrentPlayerstateEvent

===============
*/
int CurrentPlayerstateEvent(const playerState_t* ps)
{
	return ps->events[ps->eventSequence - 1 & MAX_PS_EVENTS - 1];
}

/*
========================
PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void PlayerStateToEntityState(playerState_t* ps, entityState_t* s)
{
	if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR)
	{
		s->eType = ET_INVISIBLE;
	}
	else
	{
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	VectorCopy(ps->origin, s->pos.trBase);
	//SnapVector( s->pos.trBase );

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy(ps->viewangles, s->apos.trBase);
	//SnapVector( s->apos.trBase );

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum; // ET_PLAYER looks here instead of at number
	// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	s->eFlags2 = ps->eFlags2;

	// new sabre stuff
	s->saberActive = ps->SaberActive(); //WHY is this on the entityState_t, too???
	s->saberInFlight = ps->saberInFlight;

	// NOTE: Although we store this stuff locally on a vehicle, who's to say we
	// can't bring back these variables and fill them at the appropriate time? -Aurelio
	// We need to bring these in from the vehicle NPC.
	if (g_entities[ps->clientNum].client && g_entities[ps->clientNum].client->NPC_class == CLASS_VEHICLE && g_entities
		[
			ps->clientNum].NPC)
	{
		const Vehicle_t* p_veh = g_entities[ps->clientNum].m_pVehicle;
		s->vehicleArmor = p_veh->m_iArmor;
		VectorCopy(p_veh->m_vOrientation, s->vehicleAngles);
	}

	s->vehicleModel = ps->vehicleModel;

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->ManualBlockingFlags = ps->ManualBlockingFlags; //Blockingflag on
	s->ManualBlockingTime = ps->ManualBlockingTime; //Blocking time 1 on
	s->ManualblockStartTime = ps->ManualblockStartTime; //Blocking 2
	s->ManualblockLastStartTime = ps->ManualblockLastStartTime; //Blocking 3
	s->BoltblockStartTime = ps->BoltblockStartTime; //Blocking 4
	s->Manual_m_blockingTime = ps->Manual_m_blockingTime;
	s->BoltstasisStartTime = ps->BoltstasisStartTime; //Blocking 6

	s->DodgeStartTime = ps->DodgeStartTime; //Blocking 2
	s->DodgeLastStartTime = ps->DodgeLastStartTime; //Blocking 3

	s->RESPECTINGtime = ps->RESPECTINGtime;
	s->RESPECTINGstartTime = ps->RESPECTINGstartTime;
	s->RESPECTINGlaststartTime = ps->RESPECTINGlaststartTime;

	s->gesturingtime = ps->gesturingtime;
	s->gesturingstartTime = ps->gesturingstartTime;
	s->gesturinglaststartTime = ps->gesturinglaststartTime;

	s->surrendertimeplayer = ps->surrendertimeplayer;
	s->surrenderstartTime = ps->surrenderstartTime;
	s->surrenderlaststartTime = ps->surrenderlaststartTime;

	s->dashstartTime = ps->dashstartTime;
	s->dashlaststartTime = ps->dashlaststartTime;

	s->destructtimeplayer = ps->destructtimeplayer;
	s->destructstartTime = ps->destructstartTime;
	s->destructlaststartTime = ps->destructlaststartTime;

	s->projecttimeplayer = ps->projecttimeplayer;
	s->projectstartTime = ps->projectstartTime;
	s->projectlaststartTime = ps->projectlaststartTime;

	s->grappletimeplayer = ps->grappletimeplayer;
	s->grapplestartTime = ps->grapplestartTime;
	s->grapplelaststartTime = ps->grapplelaststartTime;

	s->kickstartTime = ps->kickstartTime;
	s->kicklaststartTime = ps->kicklaststartTime;

	s->communicatingflags = ps->communicatingflags;

	s->forceUpperAnim = ps->forceUpperAnim;
	s->forceLowerAnim = ps->forceLowerAnim;
	s->forceUpperAnimTimer = ps->forceUpperAnimTimer;
	s->forceLowerAnimTimer = ps->forceLowerAnimTimer;
	s->forceUpperAnimSpeed = ps->forceUpperAnimSpeed;
	s->forceLowerAnimSpeed = ps->forceLowerAnimSpeed;

	s->saberCollisions = ps->saberCollisions;

	s->hackingTime = ps->hackingTime;
	s->hackingBaseTime = ps->hackingBaseTime;

	s->PlayerEffectFlags = ps->PlayerEffectFlags;
	s->userInt3 = ps->userInt3;

	s->powerups = 0;
	for (int i = 0; i < MAX_POWERUPS; i++)
	{
		if (ps->powerups[i])
		{
			s->powerups |= 1 << i;
		}
	}
#if 0
	if (ps->externalEvent) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	}
	else {
		int		seq;

		seq = (ps->eventSequence - 1) & (MAX_PS_EVENTS - 1);
		s->event = ps->events[seq] | ((ps->eventSequence & 3) << 8);
		s->eventParm = ps->eventParms[seq];
	}

	// show some roll in the body based on velocity and angle
	if (ps->stats[STAT_HEALTH] > 0) {
		vec3_t		right;
		float		sign;
		float		side;
		float		value;

		AngleVectors(ps->viewangles, NULL, right, NULL);

		side = DotProduct(ps->velocity, right);
		sign = side < 0 ? -1 : 1;
		side = fabs(side);

		value = 2;	// g_rollangle->value;

		if (side < 200 /* g_rollspeed->value */)
			side = side * value / 200; // g_rollspeed->value;
		else
			side = value;

		s->angles[ROLL] = (int)(side * sign * 4);
	}
#endif
}

/*
============
BG_PlayerTouchesItem

Items can be picked up without actually touching their physical bounds
============
*/
qboolean BG_PlayerTouchesItem(const playerState_t* ps, const entityState_t* item, const int at_time)
{
	vec3_t origin = { 0.0f };

	EvaluateTrajectory(&item->pos, at_time, origin);

	// we are ignoring ducked differences here
	if (ps->origin[0] - origin[0] > 44
		|| ps->origin[0] - origin[0] < -50
		|| ps->origin[1] - origin[1] > 36
		|| ps->origin[1] - origin[1] < -36
		|| ps->origin[2] - origin[2] > 36
		|| ps->origin[2] - origin[2] < -36)
	{
		return qfalse;
	}

	return qtrue;
}

/*
=================
BG_EmplacedView

Shared code for emplaced angle gun constriction
=================
*/
static int BG_EmplacedView(vec3_t base_angles, vec3_t angles, float* new_yaw, const float constraint)
{
	float dif = AngleSubtract(base_angles[YAW], angles[YAW]);

	if (dif > constraint ||
		dif < -constraint)
	{
		float amt;

		if (dif > constraint)
		{
			amt = dif - constraint;
			dif = constraint;
		}
		else if (dif < -constraint)
		{
			amt = dif + constraint;
			dif = -constraint;
		}
		else
		{
			amt = 0.0f;
		}

		*new_yaw = AngleSubtract(angles[YAW], -dif);

		if (amt > 1.0f || amt < -1.0f)
		{
			//significant, force the view
			return 2;
		}
		//just a little out of range
		return 1;
	}

	return 0;
}