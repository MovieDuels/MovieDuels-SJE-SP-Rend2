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

//seems to be a compiler problem, it doesn't clean out the #ifdefs between dif-compiles
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
#include "g_vehicles.h"
#include "../game/wp_saber.h"
#include "../cgame/cg_local.h"
#else
#include "bg_vehicles.h"
#endif

#ifdef _JK2MP
//this is really horrible, but it works! just be sure not to use any locals or anything
//with these names (exluding bool, false, true). -rww
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
extern void NPC_SetAnim(gentity_t* ent, int set_anim_parts, int anim, int set_anim_flags, int i_blend);
#endif

#ifdef QAGAME //SP or game side MP
extern vmCvar_t cg_thirdPersonAlpha;
extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern cvar_t* g_speederControlScheme;
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern void PM_SetAnim(const pmove_t* pm, int set_anim_parts, int anim, int set_anim_flags, int blend_time);
extern int PM_AnimLength(const int index, const animNumber_t anim);
#endif

//Alright, actually, most of this file is shared between game and cgame for MP.
//I would like to keep it this way, so when modifying for SP please keep in
//mind the bgEntity restrictions imposed. -rww

constexpr auto STRAFERAM_DURATION = 8;
constexpr auto STRAFERAM_ANGLE = 8;

bool VEH_StartStrafeRam(Vehicle_t* p_veh, const bool Right)
{
	if (!(p_veh->m_ulFlags & VEH_STRAFERAM))
	{
		const float speed = VectorLength(p_veh->m_pParentEntity->client->ps.velocity);
		if (speed > 400.0f)
		{
			// Compute Pos3
			//--------------
			vec3_t right;
			AngleVectors(p_veh->m_vOrientation, nullptr, right, nullptr);
			VectorMA(p_veh->m_pParentEntity->client->ps.velocity, Right ? speed : -speed, right,
				p_veh->m_pParentEntity->pos3);

			p_veh->m_ulFlags |= VEH_STRAFERAM;
			p_veh->m_fStrafeTime = Right ? STRAFERAM_DURATION : -STRAFERAM_DURATION;

			if (p_veh->m_iSoundDebounceTimer < level.time && Q_irand(0, 1) == 0)
			{
				int shift_sound = Q_irand2(1, 10);
				switch (shift_sound)
				{
				case 1:
					shift_sound = p_veh->m_pVehicleInfo->soundShift1;
					break;
				case 2:
					shift_sound = p_veh->m_pVehicleInfo->soundShift2;
					break;
				case 3:
					shift_sound = p_veh->m_pVehicleInfo->soundShift3;
					break;
				case 4:
					shift_sound = p_veh->m_pVehicleInfo->soundShift4;
					break;
				case 5:
					shift_sound = p_veh->m_pVehicleInfo->soundShift5;
					break;
				case 6:
					shift_sound = p_veh->m_pVehicleInfo->soundShift6;
					break;
				case 7:
					shift_sound = p_veh->m_pVehicleInfo->soundShift7;
					break;
				case 8:
					shift_sound = p_veh->m_pVehicleInfo->soundShift8;
					break;
				case 9:
					shift_sound = p_veh->m_pVehicleInfo->soundShift9;
					break;
				case 10:
					shift_sound = p_veh->m_pVehicleInfo->soundShift10;
					break;
				default:;
				}
				if (shift_sound)
				{
					p_veh->m_iSoundDebounceTimer = level.time + Q_irand(1000, 3000);
					G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, shift_sound);
				}
			}
			return true;
		}
	}
	return false;
}

// Like a think or move command, this updates various vehicle properties.
static bool Update(Vehicle_t* p_veh, const usercmd_t* p_ucmd)
{
	const float speed = VectorLength(p_veh->m_pParentEntity->client->ps.velocity);

	if (!g_vehicleInfo[VEHICLE_BASE].Update(p_veh, p_ucmd))
	{
		return false;
	}

	// See whether this vehicle should be exploding.
	if (p_veh->m_iDieTime != 0)
	{
		p_veh->m_pVehicleInfo->DeathUpdate(p_veh);
	}

	// Update move direction.
	const auto parent = p_veh->m_pParentEntity;

	if (p_veh->m_ulFlags & VEH_FLYING)
	{
		vec3_t vVehAngles;
		VectorSet(vVehAngles, 0, p_veh->m_vOrientation[YAW], 0);
		AngleVectors(vVehAngles, parent->client->ps.moveDir, nullptr, nullptr);
	}
	else
	{
		vec3_t vVehAngles;
		VectorSet(vVehAngles, p_veh->m_vOrientation[PITCH], p_veh->m_vOrientation[YAW], 0);
		AngleVectors(vVehAngles, parent->client->ps.moveDir, nullptr, nullptr);
	}

	// Check For A Strafe Ram
	//------------------------
	if (!(p_veh->m_ulFlags & VEH_STRAFERAM) && !(p_veh->m_ulFlags & VEH_FLYING))
	{
		// Started A Strafe
		//------------------
		if (p_veh->m_ucmd.rightmove && !p_veh->m_fStrafeTime)
		{
			p_veh->m_fStrafeTime = p_veh->m_ucmd.rightmove > 0 ? level.time : -1 * level.time;
		}

		// Ended A Strafe
		//----------------
		else if (!p_veh->m_ucmd.rightmove && p_veh->m_fStrafeTime)
		{
			// If It Was A Short Burst, Start The Strafe Ram
			//-----------------------------------------------
			if (level.time - abs(p_veh->m_fStrafeTime) < 300)
			{
				if (!VEH_StartStrafeRam(p_veh, p_veh->m_fStrafeTime > 0))
				{
					p_veh->m_fStrafeTime = 0;
				}
			}

			// Otherwise, Clear The Timer
			//----------------------------
			else
			{
				p_veh->m_fStrafeTime = 0;
			}
		}
	}

	// If Currently In A StrafeRam, Check To See If It Is Done (Timed Out)
	//---------------------------------------------------------------------
	else if (!p_veh->m_fStrafeTime)
	{
		p_veh->m_ulFlags &= ~VEH_STRAFERAM;
	}

	// Exhaust Effects Start And Stop When The Accelerator Is Pressed
	//----------------------------------------------------------------
	if (p_veh->m_pVehicleInfo->iExhaustFX)
	{
		// Start It On Each Exhaust Bolt
		//-------------------------------
		if (p_veh->m_ucmd.forwardmove && !(p_veh->m_ulFlags & VEH_ACCELERATORON))
		{
			p_veh->m_ulFlags |= VEH_ACCELERATORON;
			for (int i = 0; i < MAX_VEHICLE_EXHAUSTS && p_veh->m_iExhaustTag[i] != -1; i++)
			{
				G_PlayEffect(p_veh->m_pVehicleInfo->iExhaustFX, parent->playerModel, p_veh->m_iExhaustTag[i],
					parent->s.number, parent->currentOrigin, 1, qtrue);
			}
			// accelerate sound when going slow and advancing to speed
			if (speed < 100.0f)
			{
				if (p_veh->m_iSoundDebounceTimer < level.time)
				{
					if (p_veh->m_pVehicleInfo->soundAccelerate)
					{
						p_veh->m_iSoundDebounceTimer = level.time + Q_irand(1000, 3000);
						G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, p_veh->m_pVehicleInfo->soundAccelerate);
					}
				}
			}
		}

		// Stop It On Each Exhaust Bolt
		//------------------------------
		else if (!p_veh->m_ucmd.forwardmove && p_veh->m_ulFlags & VEH_ACCELERATORON)
		{
			p_veh->m_ulFlags &= ~VEH_ACCELERATORON;
			for (int i = 0; i < MAX_VEHICLE_EXHAUSTS && p_veh->m_iExhaustTag[i] != -1; i++)
			{
				G_StopEffect(p_veh->m_pVehicleInfo->iExhaustFX, parent->playerModel, p_veh->m_iExhaustTag[i],
					parent->s.number);

				// breaking
				if (speed > 100.0f)
				{
					if (p_veh->m_pVehicleInfo->soundBraking)
					{
						G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, p_veh->m_pVehicleInfo->soundBraking);
					}
				}
			}
		}
		else
		{
			if (p_veh->m_ucmd.forwardmove) // holding forward accelerate and going at speed
			{
				if (speed > 300.0f)
				{
					if (p_veh->m_iSoundDebounceTimer < level.time && Q_irand(0, 1) == 0)
					{
						int shift_sound = Q_irand(1, 10);
						switch (shift_sound)
						{
						case 1:
							shift_sound = p_veh->m_pVehicleInfo->soundShift1;
							break;
						case 2:
							shift_sound = p_veh->m_pVehicleInfo->soundShift2;
							break;
						case 3:
							shift_sound = p_veh->m_pVehicleInfo->soundShift3;
							break;
						case 4:
							shift_sound = p_veh->m_pVehicleInfo->soundShift4;
							break;
						case 5:
							shift_sound = p_veh->m_pVehicleInfo->soundShift5;
							break;
						case 6:
							shift_sound = p_veh->m_pVehicleInfo->soundShift6;
							break;
						case 7:
							shift_sound = p_veh->m_pVehicleInfo->soundShift7;
							break;
						case 8:
							shift_sound = p_veh->m_pVehicleInfo->soundShift8;
							break;
						case 9:
							shift_sound = p_veh->m_pVehicleInfo->soundShift9;
							break;
						case 10:
							shift_sound = p_veh->m_pVehicleInfo->soundShift10;
							break;
						default:;
						}
						if (shift_sound)
						{
							p_veh->m_iSoundDebounceTimer = level.time + Q_irand(1000, 4000);
							G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, shift_sound);
						}
					}
					return true;
				}
			}
		}
	}

	if (!(p_veh->m_ulFlags & VEH_ARMORLOW) && p_veh->m_iArmor <= p_veh->m_pVehicleInfo->armor / 3)
	{
		p_veh->m_ulFlags |= VEH_ARMORLOW;
	}

	// Armor Gone Effects (Fire)
	//---------------------------
	if (p_veh->m_pVehicleInfo->iArmorGoneFX)
	{
		if (!(p_veh->m_ulFlags & VEH_ARMORGONE) && p_veh->m_iArmor <= 0)
		{
			p_veh->m_ulFlags |= VEH_ARMORGONE;
			G_PlayEffect(p_veh->m_pVehicleInfo->iArmorGoneFX, parent->playerModel, parent->crotchBolt, parent->s.number,
				parent->currentOrigin, 1, qtrue);
			parent->s.loopSound = G_SoundIndex("sound/vehicles/common/fire_lp.wav");
		}
	}

	return true;
}

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
	float speedInc, speedIdleDec, speedIdle, /*speedIdleAccel, */speedMin, speedMax;
	playerState_t* parent_ps;
	//playerState_t *pilotPS = NULL;
	int curTime;

#ifdef _JK2MP
	parent_ps = p_veh->m_pParentEntity->playerState;
	if (p_veh->m_pPilot)
	{
		//pilotPS = p_veh->m_pPilot->playerState;
	}
#else
	parent_ps = &p_veh->m_pParentEntity->client->ps;
	if (p_veh->m_pPilot)
	{
		//pilotPS = &p_veh->m_pPilot->client->ps;
	}
#endif

	// If we're flying, make us accelerate at 40% (about half) acceleration rate, and restore the pitch
	// to origin (straight) position (at 5% increments).
	if (p_veh->m_ulFlags & VEH_FLYING)
	{
		speedInc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier * 0.4f;
	}
#ifdef _JK2MP
	else if (!parent_ps->m_iVehicleNum)
#else
	else if (!p_veh->m_pVehicleInfo->Inhabited(p_veh))
#endif
	{
		//drifts to a stop
		speedInc = 0;
		//p_veh->m_ucmd.forwardmove = 127;
	}
	else
	{
		speedInc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
	}
	speedIdleDec = p_veh->m_pVehicleInfo->decelIdle * p_veh->m_fTimeModifier;

#ifndef _JK2MP//SP
	curTime = level.time;
#elif defined QAGAME//MP GAME
	curTime = level.time;
#elif defined CGAME//MP CGAME
	//FIXME: pass in ucmd?  Not sure if this is reliable...
	curTime = pm->cmd.serverTime;
#endif

	if (p_veh->m_pPilot &&
		p_veh->m_ucmd.buttons & BUTTON_ALT_ATTACK && p_veh->m_pVehicleInfo->turboSpeed
#ifdef _JK2MP
		||
		(parent_ps && parent_ps->electrifyTime > curTime && p_veh->m_pVehicleInfo->turboSpeed) //make them go!
#endif
		)
	{
#ifdef _JK2MP
		if ((parent_ps && parent_ps->electrifyTime > curTime) ||
			(p_veh->m_pPilot->playerState &&
				(p_veh->m_pPilot->playerState->weapon == WP_MELEE ||
					(p_veh->m_pPilot->playerState->weapon == WP_SABER && p_veh->m_pPilot->playerState->saberHolstered))))
		{
#endif
			if (curTime - p_veh->m_iTurboTime > p_veh->m_pVehicleInfo->turboRecharge)
			{
				p_veh->m_iTurboTime = curTime + p_veh->m_pVehicleInfo->turboDuration;
				if (p_veh->m_pVehicleInfo->iTurboStartFX)
				{
					for (int i = 0; i < MAX_VEHICLE_EXHAUSTS && p_veh->m_iExhaustTag[i] != -1; i++)
					{
#ifndef _JK2MP//SP
						// Start The Turbo Fx Start
						//--------------------------
						G_PlayEffect(p_veh->m_pVehicleInfo->iTurboStartFX, p_veh->m_pParentEntity->playerModel,
							p_veh->m_iExhaustTag[i], p_veh->m_pParentEntity->s.number,
							p_veh->m_pParentEntity->currentOrigin);

						// Start The Looping Effect
						//--------------------------
						if (p_veh->m_pVehicleInfo->iTurboFX)
						{
							G_PlayEffect(p_veh->m_pVehicleInfo->iTurboFX, p_veh->m_pParentEntity->playerModel,
								p_veh->m_iExhaustTag[i], p_veh->m_pParentEntity->s.number,
								p_veh->m_pParentEntity->currentOrigin, p_veh->m_pVehicleInfo->turboDuration, qtrue);
						}

#else
#ifdef QAGAME
						if (p_veh->m_pParentEntity &&
							p_veh->m_pParentEntity->ghoul2 &&
							p_veh->m_pParentEntity->playerState)
						{ //fine, I'll use a tempent for this, but only because it's played only once at the start of a turbo.
							vec3_t bolt_org, boltDir;
							mdxaBone_t bolt_matrix;

							VectorSet(boltDir, 0.0f, p_veh->m_pParentEntity->playerState->viewangles[YAW], 0.0f);

							trap_G2API_GetBoltMatrix(p_veh->m_pParentEntity->ghoul2, 0, p_veh->m_iExhaustTag[i], &bolt_matrix, boltDir, p_veh->m_pParentEntity->playerState->origin, level.time, nullptr, p_veh->m_pParentEntity->modelScale);
							BG_GiveMeVectorFromMatrix(&bolt_matrix, ORIGIN, bolt_org);
							BG_GiveMeVectorFromMatrix(&bolt_matrix, ORIGIN, boltDir);
							G_PlayEffectID(p_veh->m_pVehicleInfo->iTurboStartFX, bolt_org, boltDir);
						}
#endif
#endif
					}
				}
#ifndef _JK2MP //kill me now
				if (p_veh->m_pVehicleInfo->soundTurbo)
				{
					int turbo_sound = Q_irand2(1, 4);
					switch (turbo_sound)
					{
					case 1:
						turbo_sound = p_veh->m_pVehicleInfo->soundTurbo;
						break;
					case 2:
						turbo_sound = p_veh->m_pVehicleInfo->soundTurbo2;
						break;
					case 3:
						turbo_sound = p_veh->m_pVehicleInfo->soundTurbo3;
						break;
					case 4:
						turbo_sound = p_veh->m_pVehicleInfo->soundTurbo4;
						break;
					default:;
					}
					if (turbo_sound)
					{
						G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, turbo_sound);
					}
				}
#endif
				parent_ps->speed = p_veh->m_pVehicleInfo->turboSpeed; // Instantly Jump To Turbo Speed
			}
#ifdef _JK2MP
		}
#endif
	}

	// Slide Breaking
	if (p_veh->m_ulFlags & VEH_SLIDEBREAKING)
	{
		if (p_veh->m_ucmd.forwardmove >= 0
#ifndef _JK2MP
			|| level.time - p_veh->m_pParentEntity->lastMoveTime > 500
#endif
			)
		{
			p_veh->m_ulFlags &= ~VEH_SLIDEBREAKING;
		}
		parent_ps->speed = 0;
	}
	else if (
		curTime > p_veh->m_iTurboTime &&
		!(p_veh->m_ulFlags & VEH_FLYING) &&
		p_veh->m_ucmd.forwardmove < 0 &&
		fabs(p_veh->m_vOrientation[ROLL]) > 25.0f)
	{
		p_veh->m_ulFlags |= VEH_SLIDEBREAKING;
	}

	if (curTime < p_veh->m_iTurboTime)
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
	//speedIdleAccel = p_veh->m_pVehicleInfo->accelIdle * p_veh->m_fTimeModifier;
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
		if (!p_veh->m_pVehicleInfo->strafePerc
#ifdef _JK2MP
			|| (0 && p_veh->m_pParentEntity->s.number < MAX_CLIENTS))
#else
			|| !g_speederControlScheme->value && !p_veh->m_pParentEntity->s.number)
#endif
		{
			//if in a strafe-capable vehicle, clear strafing unless using alternate control scheme
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

#ifndef _JK2MP
	// In SP, The AI Pilots Can Directly Control The Speed Of Their Bike In Order To
	// Match The Speed Of The Person They Are Trying To Chase
	//-------------------------------------------------------------------------------
	if (p_veh->m_pPilot && p_veh->m_ucmd.buttons & BUTTON_VEH_SPEED)
	{
		parent_ps->speed = p_veh->m_pPilot->client->ps.speed;
	}
#endif

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
#ifdef _JK2MP //temp hack til mp speeder controls are sorted -rww
extern void AnimalProcessOri(Vehicle_t* p_veh);
#endif
static void ProcessOrientCommands(Vehicle_t* p_veh)
{
	/********************************************************************************/
	/*	BEGIN	Here is where make sure the vehicle is properly oriented.	BEGIN	*/
	/********************************************************************************/
	playerState_t* rider_ps;

#ifdef _JK2MP
	playerState_t* parent_ps;
	float angDif;

	if (p_veh->m_pPilot)
	{
		rider_ps = p_veh->m_pPilot->playerState;
	}
	else
	{
		rider_ps = p_veh->m_pParentEntity->playerState;
	}
	//parent_ps = p_veh->m_pParentEntity->playerState;

	//p_veh->m_vOrientation[YAW] = 0.0f;//rider_ps->viewangles[YAW];
	angDif = AngleSubtract(p_veh->m_vOrientation[YAW], rider_ps->viewangles[YAW]);
	if (parent_ps && parent_ps->speed)
	{
		float s = parent_ps->speed;
		float maxDif = p_veh->m_pVehicleInfo->turningSpeed * 4.0f; //magic number hackery
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

#else
	const gentity_t* rider = p_veh->m_pParentEntity->owner;
	if (!rider || !rider->client)
	{
		rider_ps = &p_veh->m_pParentEntity->client->ps;
	}
	else
	{
		rider_ps = &rider->client->ps;
	}
	//parent_ps = &p_veh->m_pParentEntity->client->ps;

	if (p_veh->m_ulFlags & VEH_FLYING)
	{
		p_veh->m_vOrientation[YAW] += p_veh->m_vAngularVelocity;
	}
	else if (
		p_veh->m_ulFlags & VEH_SLIDEBREAKING || // No Angles Control While Out Of Control
		p_veh->m_ulFlags & VEH_OUTOFCONTROL // No Angles Control While Out Of Control
		)
	{
		// Any ability to change orientation?
	}
	else if (
		p_veh->m_ulFlags & VEH_STRAFERAM // No Angles Control While Strafe Ramming
		)
	{
		if (p_veh->m_fStrafeTime > 0)
		{
			p_veh->m_fStrafeTime--;
			p_veh->m_vOrientation[ROLL] += p_veh->m_fStrafeTime < STRAFERAM_DURATION / 2
				? -STRAFERAM_ANGLE
				: STRAFERAM_ANGLE;
		}
		else if (p_veh->m_fStrafeTime < 0)
		{
			p_veh->m_fStrafeTime++;
			p_veh->m_vOrientation[ROLL] += p_veh->m_fStrafeTime > -STRAFERAM_DURATION / 2
				? STRAFERAM_ANGLE
				: -STRAFERAM_ANGLE;
		}
	}
	else
	{
		p_veh->m_vOrientation[YAW] = rider_ps->viewangles[YAW];
	}
#endif

	/********************************************************************************/
	/*	END	Here is where make sure the vehicle is properly oriented.	END			*/
	/********************************************************************************/
}

#ifdef QAGAME

extern void PM_SetAnim(const pmove_t* pm, int set_anim_parts, int anim, int set_anim_flags, int blend_time);
extern int PM_AnimLength(const int index, const animNumber_t anim);

// This function makes sure that the vehicle is properly animated.
static void AnimateVehicle(Vehicle_t* p_veh)
{
}

#endif //QAGAME

//rest of file is shared

#ifndef	_JK2MP
extern void CG_ChangeWeapon(int num);
#endif

#ifndef _JK2MP
extern void G_StartMatrixEffect(const gentity_t* ent, int me_flags = 0, int length = 1000, float time_scale = 0.0f, int spin_time = 0);
#endif

//NOTE NOTE NOTE NOTE NOTE NOTE
//I want to keep this function BG too, because it's fairly generic already, and it
//would be nice to have proper prediction of animations. -rww
// This function makes sure that the rider's in this vehicle are properly animated.
static void AnimateRiders(Vehicle_t* p_veh)
{
	animNumber_t Anim = BOTH_VS_IDLE;
	int iFlags, iBlend = 300;
	playerState_t* pilotPS;
	int curTime;

	// Boarding animation.
	if (p_veh->m_iBoarding != 0)
	{
		// We've just started boarding, set the amount of time it will take to finish boarding.
		if (p_veh->m_iBoarding < 0)
		{
			int i_anim_len;

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
				iBlend = 0;
				Anim = BOTH_VS_MOUNTTHROW_R;
			}
			else if (p_veh->m_iBoarding == VEH_MOUNT_THROW_RIGHT)
			{
				iBlend = 0;
				Anim = BOTH_VS_MOUNTTHROW_L;
			}

			// Set the delay time (which happens to be the time it takes for the animation to complete).
			// NOTE: Here I made it so the delay is actually 40% (0.4f) of the animation time.
			if (p_veh->m_iBoarding == BOTH_VS_MOUNTJUMP_L)
			{
				i_anim_len = PM_AnimLength(p_veh->m_pPilot->client->clientInfo.animFileIndex, Anim);
			}
			else
			{
				i_anim_len = PM_AnimLength(p_veh->m_pPilot->client->clientInfo.animFileIndex, Anim); // * 0.4f;
			}

			if (p_veh->m_iBoarding != VEH_MOUNT_THROW_LEFT && p_veh->m_iBoarding != VEH_MOUNT_THROW_RIGHT)
			{
				if (p_veh->m_iBoarding == BOTH_VS_MOUNTJUMP_L)
				{
					p_veh->m_iBoarding = level.time + i_anim_len;
				}
				else
				{
					p_veh->m_iBoarding = level.time + i_anim_len * 0.4f;
				}
			}
			else
			{
				p_veh->m_iBoarding = level.time + i_anim_len;
			}
			// Set the animation, which won't be interrupted until it's completed.
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;
			NPC_SetAnim(p_veh->m_pPilot, SETANIM_BOTH, Anim, iFlags, iBlend);

			if (p_veh->m_pOldPilot)
			{
				PM_AnimLength(p_veh->m_pPilot->client->clientInfo.animFileIndex, BOTH_VS_MOUNTTHROWEE);
				NPC_SetAnim(p_veh->m_pOldPilot, SETANIM_BOTH, BOTH_VS_MOUNTTHROWEE, iFlags, iBlend);
			}
		}

#ifndef _JK2MP
		if (p_veh->m_pOldPilot && p_veh->m_pOldPilot->client->ps.torsoAnimTimer <= 0)
		{
			if (Q_irand(0, player->count) == 0)
			{
				player->count++;
				player->lastEnemy = p_veh->m_pOldPilot;
				G_StartMatrixEffect(player, MEF_LOOK_AT_ENEMY | MEF_NO_RANGEVAR | MEF_NO_VERTBOB | MEF_NO_SPIN, 1000);
			}

			gentity_t* oldPilot = p_veh->m_pOldPilot;
			p_veh->m_pVehicleInfo->Eject(p_veh, p_veh->m_pOldPilot, qtrue); // will set pointer to zero

			// Kill Him
			//----------
			oldPilot->client->noRagTime = -1; // no ragdoll for you
			G_Damage(oldPilot, p_veh->m_pPilot, p_veh->m_pPilot, p_veh->m_pPilot->currentAngles,
				p_veh->m_pPilot->currentOrigin, 1000, 0, MOD_CRUSH);

			// Compute THe Throw Direction As Backwards From The Vehicle's Velocity
			//----------------------------------------------------------------------
			vec3_t throwDir;
			VectorScale(p_veh->m_pParentEntity->client->ps.velocity, -1.0f, throwDir);
			VectorNormalize(throwDir);
			throwDir[2] += 0.3f; // up a little

			// Now Throw Him Out
			//-------------------
			G_Throw(oldPilot, throwDir, VectorLength(p_veh->m_pParentEntity->client->ps.velocity) / 10.0f);
			NPC_SetAnim(oldPilot, SETANIM_BOTH, BOTH_DEATHBACKWARD1, SETANIM_FLAG_OVERRIDE, iBlend);
		}
#endif

		return;
	}

#ifdef _JK2MP //fixme
	if (1) return;
#endif

#ifdef _JK2MP
	pilotPS = p_veh->m_pPilot->playerState;
	//parent_ps = p_veh->m_pPilot->playerState;
#else
	pilotPS = &p_veh->m_pPilot->client->ps;
	//parent_ps = &p_veh->m_pParentEntity->client->ps;
#endif

#ifndef _JK2MP//SP
	curTime = level.time;
#elif defined QAGAME//MP GAME
	curTime = level.time;
#elif defined CGAME//MP CGAME
	//FIXME: pass in ucmd?  Not sure if this is reliable...
	curTime = pm->cmd.serverTime;
#endif

	{
		const bool HasWeapon = pilotPS->weapon != WP_NONE && pilotPS->weapon != WP_MELEE;
		const bool Attacking = HasWeapon && !!(p_veh->m_ucmd.buttons & BUTTON_ATTACK);
#ifdef _JK2MP //fixme: flying tends to spaz out a lot
		bool		Flying = false;
		bool		Crashing = false;
#else
		bool Flying = !!(p_veh->m_ulFlags & VEH_FLYING);
		bool Crashing = !!(p_veh->m_ulFlags & VEH_CRASHING);
#endif
		bool Right = p_veh->m_ucmd.rightmove > 0;
		bool Left = p_veh->m_ucmd.rightmove < 0;
		const bool Turbo = curTime < p_veh->m_iTurboTime;
		EWeaponPose WeaponPose = WPOSE_NONE;

		// Remove Crashing Flag
		//----------------------
		p_veh->m_ulFlags &= ~VEH_CRASHING;

		// Put Away Saber When It Is Not Active
		//--------------------------------------
#ifndef _JK2MP
		if (HasWeapon &&
			(p_veh->m_pPilot->s.number >= MAX_CLIENTS || cg.weaponSelectTime + 500 < cg.time) &&
			(Turbo || pilotPS->weapon == WP_SABER && !pilotPS->SaberActive()))
		{
			if (p_veh->m_pPilot->s.number < MAX_CLIENTS)
			{
				p_veh->m_pPilot->client->ps.weapons[WP_NONE] = 1; // Riding means you get WP_NONE
				CG_ChangeWeapon(WP_NONE);
			}

			p_veh->m_pPilot->client->ps.weapon = WP_NONE;
			G_RemoveWeaponModels(p_veh->m_pPilot);
		}
#endif

		// Don't Interrupt Attack Anims
		//------------------------------
#ifdef _JK2MP
		if (pilotPS->weaponTime > 0)
		{
			return;
		}
#else
		if (pilotPS->torsoAnim >= BOTH_VS_ATL_S && pilotPS->torsoAnim <= BOTH_VS_ATF_G)
		{
			float bodyCurrent = 0.0f;
			int bodyEnd = 0;
			if (!!gi.G2API_GetBoneAnimIndex(&p_veh->m_pPilot->ghoul2[p_veh->m_pPilot->playerModel],
				p_veh->m_pPilot->rootBone, level.time, &bodyCurrent, nullptr, &bodyEnd,
				nullptr, nullptr, nullptr))
			{
				if (bodyCurrent <= static_cast<float>(bodyEnd) - 1.5f)
				{
					return;
				}
			}
		}
#endif

		// Compute The Weapon Pose
		//--------------------------//PM_WeaponOkOnVehicle
		if (pilotPS->weapon == WP_BLASTER_PISTOL ||
			pilotPS->weapon == WP_BLASTER ||
			pilotPS->weapon == WP_BRYAR_PISTOL ||
			pilotPS->weapon == WP_BOWCASTER ||
			pilotPS->weapon == WP_REPEATER ||
			pilotPS->weapon == WP_DEMP2 ||
			pilotPS->weapon == WP_FLECHETTE ||
			//
			pilotPS->weapon == WP_BATTLEDROID ||
			pilotPS->weapon == WP_THEFIRSTORDER ||
			pilotPS->weapon == WP_CLONECARBINE ||
			pilotPS->weapon == WP_REBELBLASTER ||
			pilotPS->weapon == WP_CLONERIFLE ||
			pilotPS->weapon == WP_CLONECOMMANDO ||
			pilotPS->weapon == WP_Z6_ROTARY_CANNON ||
			pilotPS->weapon == WP_REBELRIFLE ||
			pilotPS->weapon == WP_REY ||
			pilotPS->weapon == WP_JANGO ||
			pilotPS->weapon == WP_BOBA ||
			pilotPS->weapon == WP_CLONEPISTOL ||
			pilotPS->weapon == WP_DUAL_CLONEPISTOL ||
			pilotPS->weapon == WP_DUAL_PISTOL)
		{
			WeaponPose = WPOSE_BLASTER;
		}
		else if (pilotPS->weapon == WP_SABER)
		{
			if (p_veh->m_ulFlags & VEH_SABERINLEFTHAND && pilotPS->torsoAnim == BOTH_VS_ATL_TO_R_S)
			{
				p_veh->m_ulFlags &= ~VEH_SABERINLEFTHAND;
			}
			if (!(p_veh->m_ulFlags & VEH_SABERINLEFTHAND) && pilotPS->torsoAnim == BOTH_VS_ATR_TO_L_S)
			{
				p_veh->m_ulFlags |= VEH_SABERINLEFTHAND;
			}
			WeaponPose = p_veh->m_ulFlags & VEH_SABERINLEFTHAND ? WPOSE_SABERLEFT : WPOSE_SABERRIGHT;
		}

		if (Attacking && WeaponPose)
		{
			// Attack!
			iBlend = 100;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART;

			// Auto Aiming
			//===============================================
			if (!Left && !Right) // Allow player strafe keys to override
			{
#ifndef _JK2MP
				if (p_veh->m_pPilot->enemy)
				{
					vec3_t toEnemy;
					//float	toEnemyDistance;
					vec3_t actorRight;

					VectorSubtract(p_veh->m_pPilot->currentOrigin, p_veh->m_pPilot->enemy->currentOrigin, toEnemy);
					/*toEnemyDistance = */
					VectorNormalize(toEnemy);

					AngleVectors(p_veh->m_pParentEntity->currentAngles, nullptr, actorRight, nullptr);
					const float actorRightDot = DotProduct(toEnemy, actorRight);

					if (fabsf(actorRightDot) > 0.5f || pilotPS->weapon == WP_SABER)
					{
						Left = actorRightDot > 0.0f;
						Right = !Left;
					}
					else
					{
						Right = Left = false;
					}
				}
				else
#endif
					if (pilotPS->weapon == WP_SABER && !Left && !Right)
					{
						Left = WeaponPose == WPOSE_SABERLEFT;
						Right = !Left;
					}
			}

			if (Left)
			{
				// Attack Left
				switch (WeaponPose)
				{
				case WPOSE_BLASTER: Anim = BOTH_VS_ATL_G;
					break;
				case WPOSE_SABERLEFT: Anim = BOTH_VS_ATL_S;
					break;
				case WPOSE_SABERRIGHT: Anim = BOTH_VS_ATR_TO_L_S;
					break;
				default: assert(0);
				}
			}
			else if (Right)
			{
				// Attack Right
				switch (WeaponPose)
				{
				case WPOSE_BLASTER: Anim = BOTH_VS_ATR_G;
					break;
				case WPOSE_SABERLEFT: Anim = BOTH_VS_ATL_TO_R_S;
					break;
				case WPOSE_SABERRIGHT: Anim = BOTH_VS_ATR_S;
					break;
				default: assert(0);
				}
			}
			else
			{
				// Attack Ahead
				switch (WeaponPose)
				{
				case WPOSE_BLASTER: Anim = BOTH_VS_ATF_G;
					break;
				default: assert(0);
				}
			}
		}
		else if (Left && p_veh->m_ucmd.buttons & BUTTON_USE)
		{
			// Look To The Left Behind
			iBlend = 400;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;
			switch (WeaponPose)
			{
			case WPOSE_SABERLEFT: Anim = BOTH_VS_IDLE_SL;
				break;
			case WPOSE_SABERRIGHT: Anim = BOTH_VS_IDLE_SR;
				break;
			default: Anim = BOTH_VS_LOOKLEFT;
			}
		}
		else if (Right && p_veh->m_ucmd.buttons & BUTTON_USE)
		{
			// Look To The Right Behind
			iBlend = 400;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;
			switch (WeaponPose)
			{
			case WPOSE_SABERLEFT: Anim = BOTH_VS_IDLE_SL;
				break;
			case WPOSE_SABERRIGHT: Anim = BOTH_VS_IDLE_SR;
				break;
			default: Anim = BOTH_VS_LOOKRIGHT;
			}
		}
		else if (Turbo)
		{
			// Kicked In Turbo
			iBlend = 50;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS;
			Anim = BOTH_VS_TURBO;
		}
		else if (Flying)
		{
			// Off the ground in a jump
			iBlend = 800;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;

			switch (WeaponPose)
			{
			case WPOSE_NONE: Anim = BOTH_VS_AIR;
				break;
			case WPOSE_BLASTER: Anim = BOTH_VS_AIR_G;
				break;
			case WPOSE_SABERLEFT: Anim = BOTH_VS_AIR_SL;
				break;
			case WPOSE_SABERRIGHT: Anim = BOTH_VS_AIR_SR;
				break;
			default: assert(0);
			}
		}
		else if (Crashing)
		{
			// Hit the ground!
			iBlend = 100;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS;

			switch (WeaponPose)
			{
			case WPOSE_NONE: Anim = BOTH_VS_LAND;
				break;
			case WPOSE_BLASTER: Anim = BOTH_VS_LAND_G;
				break;
			case WPOSE_SABERLEFT: Anim = BOTH_VS_LAND_SL;
				break;
			case WPOSE_SABERRIGHT: Anim = BOTH_VS_LAND_SR;
				break;
			default: assert(0);
			}
		}
		else
		{
			// No Special Moves
			iBlend = 300;
			iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS;

			if (p_veh->m_vOrientation[ROLL] <= -20)
			{
				// Lean Left
				switch (WeaponPose)
				{
				case WPOSE_NONE: Anim = BOTH_VS_LEANL;
					break;
				case WPOSE_BLASTER: Anim = BOTH_VS_LEANL_G;
					break;
				case WPOSE_SABERLEFT: Anim = BOTH_VS_LEANL_SL;
					break;
				case WPOSE_SABERRIGHT: Anim = BOTH_VS_LEANL_SR;
					break;
				default: assert(0);
				}
			}
			else if (p_veh->m_vOrientation[ROLL] >= 20)
			{
				// Lean Right
				switch (WeaponPose)
				{
				case WPOSE_NONE: Anim = BOTH_VS_LEANR;
					break;
				case WPOSE_BLASTER: Anim = BOTH_VS_LEANR_G;
					break;
				case WPOSE_SABERLEFT: Anim = BOTH_VS_LEANR_SL;
					break;
				case WPOSE_SABERRIGHT: Anim = BOTH_VS_LEANR_SR;
					break;
				default: assert(0);
				}
			}
			else
			{
				// No Lean
				switch (WeaponPose)
				{
				case WPOSE_NONE: Anim = BOTH_VS_IDLE;
					break;
				case WPOSE_BLASTER: Anim = BOTH_VS_IDLE_G;
					break;
				case WPOSE_SABERLEFT: Anim = BOTH_VS_IDLE_SL;
					break;
				case WPOSE_SABERRIGHT: Anim = BOTH_VS_IDLE_SR;
					break;
				default: assert(0);
				}
			}
		} // No Special Moves
	} // Going backwards?
	NPC_SetAnim(p_veh->m_pPilot, SETANIM_BOTH, Anim, iFlags, iBlend);
}

void G_SetSpeederVehicleFunctions(vehicleInfo_t* pVehInfo)
{
#ifdef QAGAME
	pVehInfo->AnimateVehicle = AnimateVehicle;
	pVehInfo->AnimateRiders = AnimateRiders;
	pVehInfo->Update = Update;
#endif

	//shared
	pVehInfo->ProcessMoveCommands = ProcessMoveCommands;
	pVehInfo->ProcessOrientCommands = ProcessOrientCommands;
}

// Create/Allocate a new Animal Vehicle (initializing it as well).
void G_CreateSpeederNPC(Vehicle_t** p_veh, const char* strType)
{
	*p_veh = static_cast<Vehicle_t*>(gi.Malloc(sizeof(Vehicle_t), TAG_G_ALLOC, qtrue));
	(*p_veh)->m_pVehicleInfo = &g_vehicleInfo[BG_VehicleGetIndex(strType)];
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