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

extern float DotToSpot(vec3_t spot, vec3_t from, vec3_t fromAngles);
#ifdef _GAME //SP or gameside MP
extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern void ChangeWeapon(gentity_t* ent, int newWeapon);
extern int PM_AnimLength(int index, animNumber_t anim);
#endif

extern void BG_SetAnim(playerState_t* ps, animation_t* animations, int setAnimParts, int anim, int setAnimFlags);
extern int BG_GetTime(void);
extern qboolean BG_SabersOff(playerState_t* ps);

//Alright, actually, most of this file is shared between game and cgame for MP.
//I would like to keep it this way, so when modifying for SP please keep in
//mind the bgEntity restrictions imposed. -rww

#define	STRAFERAM_DURATION	8
#define	STRAFERAM_ANGLE		8

qboolean VEH_StartStrafeRam(Vehicle_t* p_veh, qboolean Right, int Duration)
{
	return qfalse;
}

#ifdef _GAME //game-only.. for now
// Like a think or move command, this updates various vehicle properties.
qboolean Update(Vehicle_t* p_veh, const usercmd_t* pUcmd)
{
	if (!g_vehicleInfo[VEHICLE_BASE].Update(p_veh, pUcmd))
	{
		return qfalse;
	}

	// See whether this vehicle should be exploding.
	if (p_veh->m_iDieTime != 0)
	{
		p_veh->m_pVehicleInfo->DeathUpdate(p_veh);
	}

	// Update move direction.

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
	float speedInc, speedIdle, speedMin, speedMax;
	//	playerState_t *pilotPS = NULL;
	int	cur_time;

	playerState_t* parent_ps = p_veh->m_pParentEntity->playerState;
	if (p_veh->m_pPilot)
	{
		//	pilotPS = p_veh->m_pPilot->playerState;
	}

	// If we're flying, make us accelerate at 40% (about half) acceleration rate, and restore the pitch
	// to origin (straight) position (at 5% increments).
	if (p_veh->m_ulFlags & VEH_FLYING)
	{
		speedInc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier * 0.4f;
	}
	else if (!parent_ps->m_iVehicleNum)
	{//drifts to a stop
		speedInc = 0;
		//p_veh->m_ucmd.forwardmove = 127;
	}
	else
	{
		speedInc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
	}
	const float speedIdleDec = p_veh->m_pVehicleInfo->decelIdle * p_veh->m_fTimeModifier;

#ifdef _GAME
	cur_time = level.time;
#elif _CGAME
	//FIXME: pass in ucmd?  Not sure if this is reliable...
	cur_time = pm->cmd.serverTime;
#endif

	if ((p_veh->m_pPilot /*&& (pilotPS->weapon == WP_NONE || pilotPS->weapon == WP_MELEE )*/ &&
		(p_veh->m_ucmd.buttons & BUTTON_ALT_ATTACK) && p_veh->m_pVehicleInfo->turboSpeed)
		/*||
		(parent_ps && parent_ps->electrifyTime > cur_time && p_veh->m_pVehicleInfo->turboSpeed)*/ //make them go!
		)
	{
		if ((parent_ps && parent_ps->electrifyTime > cur_time) ||
			(p_veh->m_pPilot->playerState &&
				(p_veh->m_pPilot->playerState->weapon == WP_MELEE ||
					(p_veh->m_pPilot->playerState->weapon == WP_SABER && BG_SabersOff(p_veh->m_pPilot->playerState)))))
		{
			if ((cur_time - p_veh->m_iTurboTime) > p_veh->m_pVehicleInfo->turboRecharge)
			{
				p_veh->m_iTurboTime = (cur_time + p_veh->m_pVehicleInfo->turboDuration);
				if (p_veh->m_pVehicleInfo->iTurboStartFX)
				{
					for (int i = 0; (i < MAX_VEHICLE_EXHAUSTS && p_veh->m_iExhaustTag[i] != -1); i++)
					{
#ifdef _GAME
						if (p_veh->m_pParentEntity &&
							p_veh->m_pParentEntity->ghoul2 &&
							p_veh->m_pParentEntity->playerState)
						{ //fine, I'll use a tempent for this, but only because it's played only once at the start of a turbo.
							vec3_t bolt_org, boltDir;
							mdxaBone_t bolt_matrix;

							VectorSet(boltDir, 0.0f, p_veh->m_pParentEntity->playerState->viewangles[YAW], 0.0f);

							trap->G2API_GetBoltMatrix(p_veh->m_pParentEntity->ghoul2, 0, p_veh->m_iExhaustTag[i], &bolt_matrix, boltDir, p_veh->m_pParentEntity->playerState->origin, level.time, NULL, p_veh->m_pParentEntity->modelScale);
							BG_GiveMeVectorFromMatrix(&bolt_matrix, ORIGIN, bolt_org);
							BG_GiveMeVectorFromMatrix(&bolt_matrix, ORIGIN, boltDir);
							G_PlayEffectID(p_veh->m_pVehicleInfo->iTurboStartFX, bolt_org, boltDir);
						}
#endif
					}
				}
				parent_ps->speed = p_veh->m_pVehicleInfo->turboSpeed;	// Instantly Jump To Turbo Speed
			}
		}
	}

	// Slide Breaking
	if (p_veh->m_ulFlags & VEH_SLIDEBREAKING)
	{
		if (p_veh->m_ucmd.forwardmove >= 0)
		{
			p_veh->m_ulFlags &= ~VEH_SLIDEBREAKING;
		}
		parent_ps->speed = 0;
	}
	else if (
		(cur_time > p_veh->m_iTurboTime) &&
		!(p_veh->m_ulFlags & VEH_FLYING) &&
		p_veh->m_ucmd.forwardmove < 0 &&
		fabs(p_veh->m_vOrientation[ROLL])>25.0f)
	{
		p_veh->m_ulFlags |= VEH_SLIDEBREAKING;
	}

	if (cur_time < p_veh->m_iTurboTime)
	{
		speedMax = p_veh->m_pVehicleInfo->turboSpeed;
		if (parent_ps)
		{
			parent_ps->eFlags |= EF_JETPACK_ACTIVE;
		}
	}
	else
	{
		speedMax = p_veh->m_pVehicleInfo->speedMax;
		if (parent_ps)
		{
			parent_ps->eFlags &= ~EF_JETPACK_ACTIVE;
		}
	}

	speedIdle = p_veh->m_pVehicleInfo->speedIdle;
	speedMin = p_veh->m_pVehicleInfo->speedMin;

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
		if (!p_veh->m_pVehicleInfo->strafePerc)
		{//if in a strafe-capable vehicle, clear strafing unless using alternate control scheme
			//p_veh->m_ucmd.rightmove = 0;
		}
	}

	if (parent_ps->speed > speedMax)
	{
		parent_ps->speed = speedMax;
	}
	else if (parent_ps->speed < speedMin)
	{
		parent_ps->speed = speedMin;
	}

	if (parent_ps && parent_ps->electrifyTime > cur_time)
	{
		parent_ps->speed *= (p_veh->m_fTimeModifier / 60.0f);
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
//Oh, and please, use "< MAX_CLIENTS" to check for "player" and not
//"!s.number", this is a universal check that will work for both SP
//and MP. -rww
// ProcessOrientCommands the Vehicle.
extern void AnimalProcessOri(Vehicle_t* p_veh);
void ProcessOrientCommands(Vehicle_t* p_veh)
{
	/********************************************************************************/
	/*	BEGIN	Here is where make sure the vehicle is properly oriented.	BEGIN	*/
	/********************************************************************************/
	playerState_t* rider_ps;

	if (p_veh->m_pPilot)
	{
		rider_ps = p_veh->m_pPilot->playerState;
	}
	else
	{
		rider_ps = p_veh->m_pParentEntity->playerState;
	}
	const playerState_t* parent_ps = p_veh->m_pParentEntity->playerState;

	//p_veh->m_vOrientation[YAW] = 0.0f;//rider_ps->viewangles[YAW];
	float angDif = AngleSubtract(p_veh->m_vOrientation[YAW], rider_ps->viewangles[YAW]);
	if (parent_ps && parent_ps->speed)
	{
		float s = parent_ps->speed;
		const float maxDif = p_veh->m_pVehicleInfo->turningSpeed * 4.0f; //magic number hackery
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

		if (parent_ps->electrifyTime > pm->cmd.serverTime)
		{ //do some crazy stuff
			p_veh->m_vOrientation[YAW] += (sin(pm->cmd.serverTime / 1000.0f) * 3.0f) * p_veh->m_fTimeModifier;
		}
	}

	/********************************************************************************/
	/*	END	Here is where make sure the vehicle is properly oriented.	END			*/
	/********************************************************************************/
}

#ifdef _GAME

extern int PM_AnimLength(int index, animNumber_t anim);

// This function makes sure that the vehicle is properly animated.
void AnimateVehicle(Vehicle_t* p_veh)
{
}

#endif //_GAME

//rest of file is shared

//NOTE NOTE NOTE NOTE NOTE NOTE
//I want to keep this function BG too, because it's fairly generic already, and it
//would be nice to have proper prediction of animations. -rww
// This function makes sure that the rider's in this vehicle are properly animated.
void AnimateRiders(Vehicle_t* p_veh)
{
	animNumber_t Anim = BOTH_VS_IDLE;

	// Boarding animation.
	if (p_veh->m_iBoarding != 0)
	{
		// We've just started moarding, set the amount of time it will take to finish moarding.
		if (p_veh->m_iBoarding < 0)
		{
			// Boarding from left...
			if (p_veh->m_iBoarding == -1)
			{
				Anim = BOTH_VS_MOUNT_L;
			}
			else if (p_veh->m_iBoarding == -2)
			{
				Anim = BOTH_VS_MOUNT_R;
			}
			else if (p_veh->m_iBoarding == -3)
			{
				Anim = BOTH_VS_MOUNTJUMP_L;
			}
			else if (p_veh->m_iBoarding == VEH_MOUNT_THROW_LEFT)
			{
				Anim = BOTH_VS_MOUNTTHROW_R;
			}
			else if (p_veh->m_iBoarding == VEH_MOUNT_THROW_RIGHT)
			{
				Anim = BOTH_VS_MOUNTTHROW_L;
			}

			// Set the delay time (which happens to be the time it takes for the animation to complete).
			// NOTE: Here I made it so the delay is actually 40% (0.4f) of the animation time.
			const int iAnimLen = BG_AnimLength(p_veh->m_pPilot->localAnimIndex, Anim) * 0.4f;
			p_veh->m_iBoarding = BG_GetTime() + iAnimLen;
			// Set the animation, which won't be interrupted until it's completed.
			// TODO: But what if he's killed? Should the animation remain persistant???
			const int iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;

			BG_SetAnim(p_veh->m_pPilot->playerState, bgAllAnims[p_veh->m_pPilot->localAnimIndex].anims,
				SETANIM_BOTH, Anim, iFlags);
		}

		return;
	}

	if (1)
		return;
}

#ifndef _GAME
void AttachRidersGeneric(Vehicle_t* p_veh);
#endif

void G_SetSpeederVehicleFunctions(vehicleInfo_t* pVehInfo)
{
#ifdef _GAME
	pVehInfo->AnimateVehicle = AnimateVehicle;
	pVehInfo->AnimateRiders = AnimateRiders;
	//	pVehInfo->ValidateBoard				=		ValidateBoard;
	//	pVehInfo->SetParent					=		SetParent;
	//	pVehInfo->SetPilot					=		SetPilot;
	//	pVehInfo->AddPassenger				=		AddPassenger;
	//	pVehInfo->Animate					=		Animate;
	//	pVehInfo->Board						=		Board;
	//	pVehInfo->Eject						=		Eject;
	//	pVehInfo->EjectAll					=		EjectAll;
	//	pVehInfo->StartDeathDelay			=		StartDeathDelay;
	//	pVehInfo->DeathUpdate				=		DeathUpdate;
	//	pVehInfo->RegisterAssets			=		RegisterAssets;
	//	pVehInfo->Initialize				=		Initialize;
	pVehInfo->Update = Update;
	//	pVehInfo->UpdateRider				=		UpdateRider;
#endif

	//shared
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
void G_CreateSpeederNPC(Vehicle_t** p_veh, const char* strType)
{
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
	(*p_veh)->m_pVehicleInfo = &g_vehicleInfo[BG_VehicleGetIndex(strType)];
}