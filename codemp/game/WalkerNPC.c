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

#ifdef _GAME //including game headers on cgame is FORBIDDEN ^_^
#include "g_local.h"
#endif

#include "bg_public.h"
#include "bg_vehicles.h"

#ifdef _GAME //we only want a few of these functions for BG

extern float DotToSpot(vec3_t spot, vec3_t from, vec3_t fromAngles);
extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern int PM_AnimLength(int index, animNumber_t anim);
extern void Vehicle_SetAnim(gentity_t* ent, int setAnimParts, int anim, int setAnimFlags, int iBlend);
extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength, qboolean breakSaberLock);
extern void G_VehicleTrace(trace_t* results, const vec3_t start, const vec3_t tMins, const vec3_t tMaxs, const vec3_t end, int pass_entity_num, int contentmask);

static void RegisterAssets(Vehicle_t* p_veh)
{
	//atst uses turret weapon
	RegisterItem(BG_FindItemForWeapon(WP_TURRET));

	//call the standard RegisterAssets now
	g_vehicleInfo[VEHICLE_BASE].RegisterAssets(p_veh);
}

// Like a think or move command, this updates various vehicle properties.
/*
static qboolean Update( Vehicle_t *p_veh, const usercmd_t *pUcmd )
{
	return g_vehicleInfo[VEHICLE_BASE].Update( p_veh, pUcmd );
}
*/

// Board this Vehicle (get on). The first entity to board an empty vehicle becomes the Pilot.
static qboolean Board(Vehicle_t* p_veh, bgEntity_t* p_ent)
{
	if (!g_vehicleInfo[VEHICLE_BASE].Board(p_veh, p_ent))
		return qfalse;

	// Set the board wait time (they won't be able to do anything, including getting off, for this amount of time).
	p_veh->m_iBoarding = level.time + 1500;

	return qtrue;
}
#endif //_GAME

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
	float speedInc;
	const bgEntity_t* parent = p_veh->m_pParentEntity;
	playerState_t* parent_ps = parent->playerState;

	const float speedIdleDec = p_veh->m_pVehicleInfo->decelIdle * p_veh->m_fTimeModifier;
	float speedMax = p_veh->m_pVehicleInfo->speedMax;

	const float speedIdle = p_veh->m_pVehicleInfo->speedIdle;
	const float speedMin = p_veh->m_pVehicleInfo->speedMin;

	if (!parent_ps->m_iVehicleNum)
	{//drifts to a stop
		speedInc = speedIdle * p_veh->m_fTimeModifier;
		VectorClear(parent_ps->moveDir);
		//m_ucmd.forwardmove = 127;
		parent_ps->speed = 0;
	}
	else
	{
		speedInc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
	}

	if (parent_ps->speed || parent_ps->groundEntityNum == ENTITYNUM_NONE ||
		p_veh->m_ucmd.forwardmove || p_veh->m_ucmd.upmove > 0)
	{
		if (p_veh->m_ucmd.forwardmove > 0 && speedInc)
		{
			parent_ps->speed += speedInc;
		}
		else if (p_veh->m_ucmd.forwardmove < 0)
		{
			if (parent_ps->speed > speedIdle)
			{
				parent_ps->speed -= speedInc;
			}
			else if (parent_ps->speed > speedMin)
			{
				parent_ps->speed -= speedIdleDec;
			}
		}
		// No input, so coast to stop.
		else if (parent_ps->speed > 0.0f)
		{
			parent_ps->speed -= speedIdleDec;
			if (parent_ps->speed < 0.0f)
			{
				parent_ps->speed = 0.0f;
			}
		}
		else if (parent_ps->speed < 0.0f)
		{
			parent_ps->speed += speedIdleDec;
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

		/*if ( !p_veh->m_pVehicleInfo->strafePerc
			|| (!g_speederControlScheme->value && !parent->s.number) )
		{//if in a strafe-capable vehicle, clear strafing unless using alternate control scheme
			p_veh->m_ucmd.rightmove = 0;
		}*/
	}

	if (parent_ps && parent_ps->electrifyTime > pm->cmd.serverTime)
	{
		speedMax *= 0.5f;
	}

	const float fWalkSpeedMax = speedMax * 0.275f;
	if (p_veh->m_ucmd.buttons & BUTTON_WALKING && parent_ps->speed > fWalkSpeedMax)
	{
		parent_ps->speed = fWalkSpeedMax;
	}
	else if (parent_ps->speed > speedMax)
	{
		parent_ps->speed = speedMax;
	}
	else if (parent_ps->speed < speedMin)
	{
		parent_ps->speed = speedMin;
	}

	if (parent_ps->stats[STAT_HEALTH] <= 0)
	{ //don't keep moving while you're dying!
		parent_ps->speed = 0;
	}

	/********************************************************************************/
	/*	END Here is where we move the vehicle (forward or back or whatever). END	*/
	/********************************************************************************/
}

void WalkerYawAdjust(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps)
{
	float angDif = AngleSubtract(p_veh->m_vOrientation[YAW], rider_ps->viewangles[YAW]);

	if (parent_ps && parent_ps->speed)
	{
		float s = parent_ps->speed;
		const float maxDif = p_veh->m_pVehicleInfo->turningSpeed * 1.5f; //magic number hackery

		if (s < 0.0f)
		{
			s = -s;
		}
		angDif *= s / p_veh->m_pVehicleInfo->speedMax;
		if (angDif > maxDif)
		{
			angDif = maxDif;
		}
		else if (angDif < -maxDif)
		{
			angDif = -maxDif;
		}
		p_veh->m_vOrientation[YAW] = AngleNormalize180(p_veh->m_vOrientation[YAW] - angDif * (p_veh->m_fTimeModifier * 0.2f));
	}
}

/*
void WalkerPitchAdjust(Vehicle_t *p_veh, playerState_t *rider_ps, playerState_t *parent_ps)
{
	float angDif = AngleSubtract(p_veh->m_vOrientation[PITCH], rider_ps->viewangles[PITCH]);

	if (parent_ps && parent_ps->speed)
	{
		float s = parent_ps->speed;
		float maxDif = p_veh->m_pVehicleInfo->turningSpeed*0.8f; //magic number hackery

		if (s < 0.0f)
		{
			s = -s;
		}
		angDif *= s/p_veh->m_pVehicleInfo->speedMax;
		if (angDif > maxDif)
		{
			angDif = maxDif;
		}
		else if (angDif < -maxDif)
		{
			angDif = -maxDif;
		}
		p_veh->m_vOrientation[PITCH] = AngleNormalize360(p_veh->m_vOrientation[PITCH] - angDif*(p_veh->m_fTimeModifier*0.2f));
	}
}
*/

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
	const bgEntity_t* parent = p_veh->m_pParentEntity;

	const bgEntity_t* rider = NULL;
	if (parent->s.owner != ENTITYNUM_NONE)
	{
		rider = PM_BGEntForNum(parent->s.owner); //&g_entities[parent->r.ownerNum];
	}

	if (!rider)
	{
		rider = parent;
	}

	playerState_t* parent_ps = parent->playerState;
	playerState_t* rider_ps = rider->playerState;

	// If the player is the rider...
	if (rider->s.number < MAX_CLIENTS)
	{//FIXME: use the vehicle's turning stat in this calc
		WalkerYawAdjust(p_veh, rider_ps, parent_ps);
		//FighterPitchAdjust(p_veh, rider_ps, parent_ps);
		p_veh->m_vOrientation[PITCH] = rider_ps->viewangles[PITCH];
	}
	else
	{
		float turnSpeed = p_veh->m_pVehicleInfo->turningSpeed;
		if (!p_veh->m_pVehicleInfo->turnWhenStopped
			&& !parent_ps->speed)//FIXME: or !p_veh->m_ucmd.forwardmove?
		{//can't turn when not moving
			//FIXME: or ramp up to max turnSpeed?
			turnSpeed = 0.0f;
		}
		if (rider->s.eType == ET_NPC)
		{//help NPCs out some
			turnSpeed *= 2.0f;
			if (parent_ps->speed > 200.0f)
			{
				turnSpeed += turnSpeed * parent_ps->speed / 200.0f * 0.05f;
			}
		}
		turnSpeed *= p_veh->m_fTimeModifier;

		//default control scheme: strafing turns, mouselook aims
		if (p_veh->m_ucmd.rightmove < 0)
		{
			p_veh->m_vOrientation[YAW] += turnSpeed;
		}
		else if (p_veh->m_ucmd.rightmove > 0)
		{
			p_veh->m_vOrientation[YAW] -= turnSpeed;
		}

		if (p_veh->m_pVehicleInfo->malfunctionArmorLevel && p_veh->m_iArmor <= p_veh->m_pVehicleInfo->malfunctionArmorLevel)
		{//damaged badly
		}
	}

	/********************************************************************************/
	/*	END	Here is where make sure the vehicle is properly oriented.	END			*/
	/********************************************************************************/
}

#ifdef _GAME //back to our game-only functions
// This function makes sure that the vehicle is properly animated.
static void AnimateVehicle(Vehicle_t* p_veh)
{
	animNumber_t Anim = BOTH_STAND1;
	int iFlags = SETANIM_FLAG_NORMAL, iBlend = 300;
	gentity_t* parent = (gentity_t*)p_veh->m_pParentEntity;

	// We're dead (boarding is reused here so I don't have to make another variable :-).
	if (parent->health <= 0)
	{
		/*
		if ( p_veh->m_iBoarding != -999 )	// Animate the death just once!
		{
			p_veh->m_iBoarding = -999;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;

			// FIXME! Why do you keep repeating over and over!!?!?!? Bastard!
			//Vehicle_SetAnim( parent, SETANIM_LEGS, BOTH_VT_DEATH1, iFlags, iBlend );
		}
		*/
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
	const float fSpeedPercToMax = parent->client->ps.speed / p_veh->m_pVehicleInfo->speedMax;

	// If we're moving...
	if (fSpeedPercToMax > 0.0f) //fSpeedPercToMax >= 0.85f )
	{
		//	float fYawDelta;

		iBlend = 300;
		iFlags = SETANIM_FLAG_OVERRIDE;
		//	fYawDelta = p_veh->m_vPrevOrientation[YAW] - p_veh->m_vOrientation[YAW];

			// NOTE: Mikes suggestion for fixing the stuttering walk (left/right) is to maintain the
			// current frame between animations. I have no clue how to do this and have to work on other
			// stuff so good luck to him :-p AReis

			// If we're walking (or our speed is less than .275%)...
		if ((p_veh->m_ucmd.buttons & BUTTON_WALKING) || fSpeedPercToMax < 0.275f)
		{
			// Make them lean if we're turning.
			/*if ( fYawDelta < -0.0001f )
			{
				Anim = BOTH_VT_WALK_FWD_L;
			}
			else if ( fYawDelta > 0.0001 )
			{
				Anim = BOTH_VT_WALK_FWD_R;
			}
			else*/
			{
				Anim = BOTH_WALK1;
			}
		}
		// otherwise we're running.
		else
		{
			// Make them lean if we're turning.
			/*if ( fYawDelta < -0.0001f )
			{
				Anim = BOTH_VT_RUN_FWD_L;
			}
			else if ( fYawDelta > 0.0001 )
			{
				Anim = BOTH_VT_RUN_FWD_R;
			}
			else*/
			{
				Anim = BOTH_RUN1;
			}
		}
	}
	else
	{
		// Going in reverse...
		if (fSpeedPercToMax < -0.018f)
		{
			iFlags = SETANIM_FLAG_NORMAL;
			Anim = BOTH_WALKBACK1;
			iBlend = 500;
		}
		else
		{
			//int iChance = Q_irand( 0, 20000 );

			// Every once in a while buck or do a different idle...
			iFlags = SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD;
			iBlend = 600;
			if (parent->client->ps.m_iVehicleNum)
			{//occupado
				Anim = BOTH_STAND1;
			}
			else
			{//wide open for you, baby
				Anim = BOTH_STAND2;
			}
		}
	}

	Vehicle_SetAnim(parent, SETANIM_LEGS, Anim, iFlags, iBlend);
}

//rwwFIXMEFIXME: This is all going to have to be predicted I think, or it will feel awful
//and lagged
#endif //_GAME

#ifndef _GAME
void AttachRidersGeneric(Vehicle_t* p_veh);
#endif

//on the client this function will only set up the process command funcs
void G_SetWalkerVehicleFunctions(vehicleInfo_t* pVehInfo)
{
#ifdef _GAME
	pVehInfo->AnimateVehicle = AnimateVehicle;
	//	pVehInfo->AnimateRiders				=		AnimateRiders;
	//	pVehInfo->ValidateBoard				=		ValidateBoard;
	//	pVehInfo->SetParent					=		SetParent;
	//	pVehInfo->SetPilot					=		SetPilot;
	//	pVehInfo->AddPassenger				=		AddPassenger;
	//	pVehInfo->Animate					=		Animate;
	pVehInfo->Board = Board;
	//	pVehInfo->Eject						=		Eject;
	//	pVehInfo->EjectAll					=		EjectAll;
	//	pVehInfo->StartDeathDelay			=		StartDeathDelay;
	//	pVehInfo->DeathUpdate				=		DeathUpdate;
	pVehInfo->RegisterAssets = RegisterAssets;
	//	pVehInfo->Initialize				=		Initialize;
	//	pVehInfo->Update					=		Update;
	//	pVehInfo->UpdateRider				=		UpdateRider;
#endif //_GAME
	pVehInfo->ProcessMoveCommands = ProcessMoveCommands;
	pVehInfo->ProcessOrientCommands = ProcessOrientCommands;

#ifndef _GAME //cgame prediction attachment func
	pVehInfo->AttachRiders = AttachRidersGeneric;
#endif
	//	pVehInfo->AttachRiders				=		AttachRiders;
	//	pVehInfo->Ghost						=		Ghost;
	//	pVehInfo->UnGhost					=		UnGhost;
	//	pVehInfo->Inhabited					=		Inhabited;
}

// Following is only in game, not in namespace

#ifdef _GAME
extern void G_AllocateVehicleObject(Vehicle_t** p_veh);
#endif

// Create/Allocate a new Animal Vehicle (initializing it as well).
//this is a BG function too in MP so don't un-bg-compatibilify it -rww
void G_CreateWalkerNPC(Vehicle_t** p_veh, const char* str_animal_type)
{
	// Allocate the Vehicle.
#ifdef _GAME
	//these will remain on entities on the client once allocated because the pointer is
	//never stomped. on the server, however, when an ent is freed, the entity struct is
	//memset to 0, so this memory would be lost..
	G_AllocateVehicleObject(p_veh);
#else
	if (!*p_veh)
	{ //only allocate a new one if we really have to
		(*p_veh) = (Vehicle_t*)BG_Alloc(sizeof(Vehicle_t));
	}
#endif
	memset(*p_veh, 0, sizeof(Vehicle_t));
	(*p_veh)->m_pVehicleInfo = &g_vehicleInfo[BG_VehicleGetIndex(str_animal_type)];
}