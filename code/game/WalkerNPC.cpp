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

//seems to be a compiler bug, it doesn't clean out the #ifdefs between dif-compiles
//or something, so the headers spew errors on these defs from the previous compile.
//this fixes that. -rww
#ifdef _JK2MP
//get rid of all the crazy defs we added for this file
#undef currentAngles
#undef currentOrigin
#undef mins
#undef maxs
#undef legsAnimTimer
#undef torsoAnimTimer
#undef bool
#undef false
#undef true

#undef sqrtf
#undef Q_flrand

#undef MOD_EXPLOSIVE
#endif

#ifdef _JK2 //SP does not have this preprocessor for game like MP does
#ifndef _JK2MP
#define _JK2MP
#endif
#endif

#ifndef _JK2MP //if single player
#ifndef QAGAME //I don't think we have a QAGAME define
#define QAGAME //but define it cause in sp we're always in the game
#endif
#endif

#ifdef QAGAME //including game headers on cgame is FORBIDDEN ^_^
#include "g_local.h"
#elif defined _JK2MP
#include "bg_public.h"
#endif

#ifndef _JK2MP
#include "g_functions.h"
#include "g_vehicles.h"
#else
#include "bg_vehicles.h"
#endif
#include <qcommon\q_platform.h>
#include "anims.h"

#ifdef _JK2MP
//this is really horrible, but it works! just be sure not to use any locals or anything
//with these names (excluding bool, false, true). -rww
#define currentAngles r.currentAngles
#define currentOrigin r.currentOrigin
#define mins r.mins
#define maxs r.maxs
#define legsAnimTimer legsTimer
#define torsoAnimTimer torsoTimer
#define bool qboolean
#define false qfalse
#define true qtrue

#define sqrtf sqrt
#define Q_flrand flrand

#define MOD_EXPLOSIVE MOD_SUICIDE
#else
#define bgEntity_t gentity_t
#endif

#ifdef QAGAME //we only want a few of these functions for BG
extern vmCvar_t cg_thirdPersonAlpha;
extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern cvar_t* g_speederControlScheme;
extern void PM_SetAnim(const pmove_t* pm, int set_anim_parts, int anim, int set_anim_flags, int blend_time);
extern int PM_AnimLength(const int index, const animNumber_t anim);
extern void Vehicle_SetAnim(gentity_t* ent, int setAnimParts, int anim, int setAnimFlags, int iBlend);
extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength, const qboolean breakSaberLock);
extern void G_VehicleTrace(trace_t* results, const vec3_t start, const vec3_t tMins, const vec3_t tMaxs,
	const vec3_t end, int passEntityNum, int contentmask);

static void RegisterAssets(Vehicle_t* p_veh)
{
	//atst uses turret weapon
#ifdef _JK2MP
	RegisterItem(BG_FindItemForWeapon(WP_TURRET));
#else
	// PUT SOMETHING HERE...
#endif

	//call the standard RegisterAssets now
	g_vehicleInfo[VEHICLE_BASE].RegisterAssets(p_veh);
}

// Like a think or move command, this updates various vehicle properties.
/*
static bool Update( Vehicle_t *p_veh, const usercmd_t *pUcmd )
{
	return g_vehicleInfo[VEHICLE_BASE].Update( p_veh, pUcmd );
}
*/

// Board this Vehicle (get on). The first entity to board an empty vehicle becomes the Pilot.
static bool Board(Vehicle_t* p_veh, bgEntity_t* pEnt)
{
	if (!g_vehicleInfo[VEHICLE_BASE].Board(p_veh, pEnt))
		return false;

	// Set the board wait time (they won't be able to do anything, including getting off, for this amount of time).
	p_veh->m_iBoarding = level.time + 1500;

	return true;
}
#endif //QAGAME

#ifdef _JK2MP
#include "../namespace_begin.h"
#endif

//MP RULE - ALL PROCESSMOVECOMMANDS FUNCTIONS MUST BE BG-COMPATIBLE!!!
//If you really need to violate this rule for SP, then use ifdefs.
//By BG-compatible, I mean no use of game-specific data - ONLY use
//stuff available in the MP bgEntity (in SP, the bgEntity is #defined
//as a gentity, but the MP-compatible access restrictions are based
//on the bgEntity structure in the MP codebase) -rww
// ProcessMoveCommands the Vehicle.
static void ProcessMoveCommands(Vehicle_t* p_veh)
{
	/************************************************************************************/
	/*	BEGIN	Here is where we move the vehicle (forward or back or whatever). BEGIN	*/
	/************************************************************************************/

	//Client sets ucmds and such for speed alterations
	float speed_inc, speed_idle_dec, speed_idle, /*speedIdleAccel, */speed_min, speed_max;
	float fWalkSpeedMax;
	bgEntity_t* parent = p_veh->m_pParentEntity;
#ifdef _JK2MP
	playerState_t* parent_ps = parent->playerState;
#else
	playerState_t* parent_ps = &parent->client->ps;
#endif

	speed_idle_dec = p_veh->m_pVehicleInfo->decelIdle * p_veh->m_fTimeModifier;
	speed_max = p_veh->m_pVehicleInfo->speedMax;

	speed_idle = p_veh->m_pVehicleInfo->speedIdle;
	//speedIdleAccel = p_veh->m_pVehicleInfo->accelIdle * p_veh->m_fTimeModifier;
	speed_min = p_veh->m_pVehicleInfo->speedMin;

#ifdef _JK2MP
	if (!parent_ps->m_iVehicleNum)
#else
	if (!p_veh->m_pVehicleInfo->Inhabited(p_veh))
#endif
	{
		//drifts to a stop
		speed_inc = speed_idle * p_veh->m_fTimeModifier;
		VectorClear(parent_ps->moveDir);
		//m_ucmd.forwardmove = 127;
		parent_ps->speed = 0;
	}
	else
	{
		speed_inc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
	}

	if (parent_ps->speed || parent_ps->groundEntityNum == ENTITYNUM_NONE ||
		p_veh->m_ucmd.forwardmove || p_veh->m_ucmd.upmove > 0)
	{
		if (p_veh->m_ucmd.forwardmove > 0 && speed_inc)
		{
			parent_ps->speed += speed_inc;
		}
		else if (p_veh->m_ucmd.forwardmove < 0)
		{
			if (parent_ps->speed > speed_idle)
			{
				parent_ps->speed -= speed_inc;
			}
			else if (parent_ps->speed > speed_min)
			{
				parent_ps->speed -= speed_idle_dec;
			}
		}
		// No input, so coast to stop.
		else if (parent_ps->speed > 0.0f)
		{
			parent_ps->speed -= speed_idle_dec;
			if (parent_ps->speed < 0.0f)
			{
				parent_ps->speed = 0.0f;
			}
		}
		else if (parent_ps->speed < 0.0f)
		{
			parent_ps->speed += speed_idle_dec;
			if (parent_ps->speed > 0.0f)
			{
				parent_ps->speed = 0.0f;
			}
		}
	}
	else
	{
		if (p_veh->m_ucmd.forwardmove < 0)
		{
			p_veh->m_ucmd.forwardmove = 0;
		}
		if (p_veh->m_ucmd.upmove < 0)
		{
			p_veh->m_ucmd.upmove = 0;
		}

		p_veh->m_ucmd.rightmove = 0;
	}

	fWalkSpeedMax = speed_max * 0.275f;
	if (p_veh->m_ucmd.buttons & BUTTON_WALKING && parent_ps->speed > fWalkSpeedMax)
	{
		parent_ps->speed = fWalkSpeedMax;
	}
	else if (parent_ps->speed > speed_max)
	{
		parent_ps->speed = speed_max;
	}
	else if (parent_ps->speed < speed_min)
	{
		parent_ps->speed = speed_min;
	}

	/********************************************************************************/
	/*	END Here is where we move the vehicle (forward or back or whatever). END	*/
	/********************************************************************************/
}

//MP RULE - ALL PROCESSORIENTCOMMANDS FUNCTIONS MUST BE BG-COMPATIBLE!!!
//If you really need to violate this rule for SP, then use ifdefs.
//By BG-compatible, I mean no use of game-specific data - ONLY use
//stuff available in the MP bgEntity (in SP, the bgEntity is #defined
//as a gentity, but the MP-compatible access restrictions are based
//on the bgEntity structure in the MP codebase) -rww
// ProcessOrientCommands the Vehicle.
static void ProcessOrientCommands(Vehicle_t* p_veh)
{
	/********************************************************************************/
	/*	BEGIN	Here is where make sure the vehicle is properly oriented.	BEGIN	*/
	/********************************************************************************/
	//float speed;
	bgEntity_t* parent = p_veh->m_pParentEntity;
	playerState_t* parent_ps, * rider_ps;

	gentity_t* rider = parent->owner;

	if (!rider || !rider->client)
	{
		rider = parent;
	}
	parent_ps = &parent->client->ps;
	rider_ps = &rider->client->ps;

	//speed = VectorLength( parent_ps->velocity );

	// If the player is the rider...
	if (rider->s.number < MAX_CLIENTS)
	{
		p_veh->m_vOrientation[YAW] = rider_ps->viewangles[YAW];
		p_veh->m_vOrientation[PITCH] = rider_ps->viewangles[PITCH];
	}
	else
	{
		float turnSpeed = p_veh->m_pVehicleInfo->turningSpeed;
		if (!p_veh->m_pVehicleInfo->turnWhenStopped
			&& !parent_ps->speed) //FIXME: or !p_veh->m_ucmd.forwardmove?
		{
			//can't turn when not moving
			//FIXME: or ramp up to max turnSpeed?
			turnSpeed = 0.0f;
		}

		if (!rider || rider->NPC)
		{
			//help NPCs out some
			turnSpeed *= 2.0f;

			if (parent && parent->client->ps.speed > 200.0f)
			{
				turnSpeed += turnSpeed * parent_ps->speed / 200.0f * 0.05f;
			}
		}
		turnSpeed *= p_veh->m_fTimeModifier;

		//default control scheme: strafing turns, mouse look aims
		if (p_veh->m_ucmd.rightmove < 0)
		{
			p_veh->m_vOrientation[YAW] += turnSpeed;
		}
		else if (p_veh->m_ucmd.rightmove > 0)
		{
			p_veh->m_vOrientation[YAW] -= turnSpeed;
		}

		if (p_veh->m_pVehicleInfo->malfunctionArmorLevel && p_veh->m_iArmor <= p_veh->m_pVehicleInfo->
			malfunctionArmorLevel)
		{
			//damaged badly
		}
	}

	/********************************************************************************/
	/*	END	Here is where make sure the vehicle is properly oriented.	END			*/
	/********************************************************************************/
}

#ifdef QAGAME //back to our game-only functions
// This function makes sure that the vehicle is properly animated.
static void AnimateVehicle(Vehicle_t* p_veh)
{
	animNumber_t anim;
	int i_flags, i_blend;
	auto parent = p_veh->m_pParentEntity;

	// We're dead (boarding is reused here so I don't have to make another variable :-).
	if (parent->health <= 0)
	{
		if (p_veh->m_iBoarding != -999) // Animate the death just once!
		{
			p_veh->m_iBoarding = -999;
			i_flags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;

			// FIXME! Why do you keep repeating over and over!!?!?!? Bastard!
			//Vehicle_SetAnim( parent, SETANIM_LEGS, BOTH_VT_DEATH1, iFlags, iBlend );
		}
		return;
	}

	// Following is redundant to g_vehicles.c
	//	if ( p_veh->m_iBoarding )
	//	{
	//		//we have no boarding anim
	//		if (p_veh->m_iBoarding < level.time)
	//		{ //we are on now
	//			p_veh->m_iBoarding = 0;
	//		}
	//		else
	//		{
	//			return;
	//		}
	//	}

	// Percentage of maximum speed relative to current speed.
	//float fSpeed = VectorLength( client->ps.velocity );
	const float f_speed_perc_to_max = parent->client->ps.speed / p_veh->m_pVehicleInfo->speedMax;

	// If we're moving...
	if (f_speed_perc_to_max > 0.0f) //fSpeedPercToMax >= 0.85f )
	{
		//float fYawDelta;

		i_blend = 300;
		i_flags = SETANIM_FLAG_OVERRIDE;
		//fYawDelta = p_veh->m_vPrevOrientation[YAW] - p_veh->m_vOrientation[YAW];

		// NOTE: Mikes suggestion for fixing the stuttering walk (left/right) is to maintain the
		// current frame between animations. I have no clue how to do this and have to work on other
		// stuff so good luck to him :-p AReis

		// If we're walking (or our speed is less than .275%)...
		if (p_veh->m_ucmd.buttons & BUTTON_WALKING || f_speed_perc_to_max < 0.275f)
		{
			anim = BOTH_WALK1;
		}
		// otherwise we're running.
		else
		{
			anim = BOTH_RUN1;
		}
	}
	else
	{
		// Going in reverse...
		if (f_speed_perc_to_max < -0.018f)
		{
			i_flags = SETANIM_FLAG_NORMAL;
			anim = BOTH_WALKBACK1;
			i_blend = 500;
		}
		else
		{
			//int iChance = Q_irand( 0, 20000 );

			// Every once in a while buck or do a different idle...
			i_flags = SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD;
			i_blend = 600;
#ifdef _JK2MP
			if (parent->client->ps.m_iVehicleNum)
#else
			if (p_veh->m_pVehicleInfo->Inhabited(p_veh))
#endif
			{
				//occupant
				anim = BOTH_STAND1;
			}
			else
			{
				//wide open for you, baby
				anim = BOTH_STAND2;
			}
		}
	}

	Vehicle_SetAnim(parent, SETANIM_LEGS, anim, i_flags, i_blend);
}

//rwwFIXMEFIXME: This is all going to have to be predicted I think, or it will feel awful
//and lagged
#endif //QAGAME

//on the client this function will only set up the process command funcs
void G_SetWalkerVehicleFunctions(vehicleInfo_t* pVehInfo)
{
#ifdef QAGAME
	pVehInfo->AnimateVehicle = AnimateVehicle;
	pVehInfo->Board = Board;
	pVehInfo->RegisterAssets = RegisterAssets;
#endif //QAGAME
	pVehInfo->ProcessMoveCommands = ProcessMoveCommands;
	pVehInfo->ProcessOrientCommands = ProcessOrientCommands;
}

// Create/Allocate a new Animal Vehicle (initializing it as well).
//this is a BG function too in MP so don't un-bg-compatibilify it -rww
void G_CreateWalkerNPC(Vehicle_t** p_veh, const char* str_animal_type)
{
	*p_veh = static_cast<Vehicle_t*>(gi.Malloc(sizeof(Vehicle_t), TAG_G_ALLOC, qtrue));
	(*p_veh)->m_pVehicleInfo = &g_vehicleInfo[BG_VehicleGetIndex(str_animal_type)];
}

#ifdef _JK2MP

#include "../namespace_end.h"

//get rid of all the crazy defs we added for this file
#undef currentAngles
#undef currentOrigin
#undef mins
#undef maxs
#undef legsAnimTimer
#undef torsoAnimTimer
#undef bool
#undef false
#undef true

#undef sqrtf
#undef Q_flrand

#undef MOD_EXPLOSIVE
#endif