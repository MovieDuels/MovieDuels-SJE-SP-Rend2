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

#include "bg_public.h"
#include "bg_vehicles.h"

#ifdef _GAME
#include "g_local.h"
#elif _CGAME
#include "cgame/cg_local.h"
#endif

extern float DotToSpot(vec3_t spot, vec3_t from, vec3_t fromAngles);
#ifdef _GAME //SP or gameside MP
extern vmCvar_t	cg_thirdPersonAlpha;
extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern void ChangeWeapon(gentity_t* ent, int newWeapon);
extern int PM_AnimLength(int index, animNumber_t anim);
extern void G_VehicleTrace(trace_t* results, const vec3_t start, const vec3_t tMins, const vec3_t tMaxs, const vec3_t end, int pass_entity_num, int contentmask);
#endif

extern qboolean BG_UnrestrainedPitchRoll(playerState_t* ps, Vehicle_t* p_veh);
extern void BG_SetAnim(playerState_t* ps, animation_t* animations, int setAnimParts, int anim, int setAnimFlags);
extern int BG_GetTime(void);

//this stuff has got to be predicted, so..
qboolean BG_FighterUpdate(Vehicle_t* p_veh, const usercmd_t* pUcmd, vec3_t trMins, vec3_t trMaxs, float gravity, void (*traceFunc)(trace_t* results, const vec3_t start, const vec3_t lmins, const vec3_t lmaxs, const vec3_t end, int pass_entity_num, int content_mask))
{
	vec3_t		bottom;
	playerState_t* parent_ps;
	//qboolean	isDead = qfalse;
#ifdef _GAME //don't do this on client

	// Make sure the riders are not visible or collidable.
	p_veh->m_pVehicleInfo->Ghost(p_veh, p_veh->m_pPilot);
	for (int i = 0; i < p_veh->m_pVehicleInfo->maxPassengers; i++)
	{
		p_veh->m_pVehicleInfo->Ghost(p_veh, p_veh->m_ppPassengers[i]);
	}
#endif

	parent_ps = p_veh->m_pParentEntity->playerState;

	if (!parent_ps)
	{
		Com_Error(ERR_DROP, "NULL PS in BG_FighterUpdate (%s)", p_veh->m_pVehicleInfo->name);
		return qfalse;
	}

	// If we have a pilot, take out gravity (it's a flying craft...).
	if (p_veh->m_pPilot)
	{
		parent_ps->gravity = 0;
	}
	else
	{
		if (p_veh->m_pVehicleInfo->gravity)
		{
			parent_ps->gravity = p_veh->m_pVehicleInfo->gravity;
		}
		else
		{ //it doesn't have gravity specified apparently
			parent_ps->gravity = gravity;
		}
	}

	/*
	isDead = (qboolean)((parent_ps->eFlags&EF_DEAD)!=0);

	if ( isDead ||
		(p_veh->m_pVehicleInfo->surfDestruction &&
			p_veh->m_iRemovedSurfaces ) )
	{//can't land if dead or spiralling out of control
		p_veh->m_LandTrace.fraction = 1.0f;
		p_veh->m_LandTrace.contents = p_veh->m_LandTrace.surfaceFlags = 0;
		VectorClear( p_veh->m_LandTrace.plane.normal );
		p_veh->m_LandTrace.allsolid = qfalse;
		p_veh->m_LandTrace.startsolid = qfalse;
	}
	else
	{
	*/
	//argh, no, I need to have a way to see when they impact the ground while damaged. -rww

		// Check to see if the fighter has taken off yet (if it's a certain height above ground).
	VectorCopy(parent_ps->origin, bottom);
	bottom[2] -= p_veh->m_pVehicleInfo->landingHeight;

	traceFunc(&p_veh->m_LandTrace, parent_ps->origin, trMins, trMaxs, bottom, p_veh->m_pParentEntity->s.number, (MASK_NPCSOLID & ~CONTENTS_BODY));
	//}

	return qtrue;
}

#ifdef _GAME //ONLY in SP or on server, not cgame

// Like a think or move command, this updates various vehicle properties.
static qboolean Update(Vehicle_t* p_veh, const usercmd_t* pUcmd)
{
	assert(p_veh->m_pParentEntity);
	if (!BG_FighterUpdate(p_veh, pUcmd, ((gentity_t*)p_veh->m_pParentEntity)->r.mins,
		((gentity_t*)p_veh->m_pParentEntity)->r.maxs,
		g_gravity.value,
		G_VehicleTrace))
	{
		return qfalse;
	}

	if (!g_vehicleInfo[VEHICLE_BASE].Update(p_veh, pUcmd))
	{
		return qfalse;
	}

	return qtrue;
}

// Board this Vehicle (get on). The first entity to board an empty vehicle becomes the Pilot.
static qboolean Board(Vehicle_t* p_veh, bgEntity_t* p_ent)
{
	if (!g_vehicleInfo[VEHICLE_BASE].Board(p_veh, p_ent))
		return qfalse;

	// Set the board wait time (they won't be able to do anything, including getting off, for this amount of time).
	p_veh->m_iBoarding = level.time + 1500;

	return qtrue;
}

// Eject an entity from the vehicle.
static qboolean Eject(Vehicle_t* p_veh, bgEntity_t* p_ent, qboolean forceEject)
{
	if (g_vehicleInfo[VEHICLE_BASE].Eject(p_veh, p_ent, forceEject))
	{
		return qtrue;
	}

	return qfalse;
}

#endif //end game-side only

//method of decrementing the given angle based on the given taking variable frame times into account
static float PredictedAngularDecrement(float scale, float time_mod, float original_angle)
{
	float fixedBaseDec = original_angle * 0.05f;
	float r = 0.0f;

	if (fixedBaseDec < 0.0f)
	{
		fixedBaseDec = -fixedBaseDec;
	}

	fixedBaseDec *= (1.0f + (1.0f - scale));

	if (fixedBaseDec < 0.1f)
	{ //don't increment in incredibly small fractions, it would eat up unnecessary bandwidth.
		fixedBaseDec = 0.1f;
	}

	fixedBaseDec *= (time_mod * 0.1f);
	if (original_angle > 0.0f)
	{ //subtract
		r = (original_angle - fixedBaseDec);
		if (r < 0.0f)
		{
			r = 0.0f;
		}
	}
	else if (original_angle < 0.0f)
	{ //add
		r = (original_angle + fixedBaseDec);
		if (r > 0.0f)
		{
			r = 0.0f;
		}
	}

	return r;
}

#ifdef _GAME//only do this check on game side, because if it's cgame, it's being predicted, and it's only predicted if the local client is the driver
qboolean FighterIsInSpace(gentity_t* gParent)
{
	if (gParent
		&& gParent->client
		&& gParent->client->inSpaceIndex
		&& gParent->client->inSpaceIndex < ENTITYNUM_WORLD)
	{
		return qtrue;
	}
	return qfalse;
}
#endif

qboolean FighterOverValidLandingSurface(Vehicle_t* p_veh)
{
	if (p_veh->m_LandTrace.fraction < 1.0f //ground present
		&& p_veh->m_LandTrace.plane.normal[2] >= MIN_LANDING_SLOPE)//flat enough
		//FIXME: also check for a certain surface flag ... "landing zones"?
	{
		return qtrue;
	}
	return qfalse;
}

qboolean FighterIsLanded(Vehicle_t* p_veh, playerState_t* parent_ps)
{
	if (FighterOverValidLandingSurface(p_veh)
		&& !parent_ps->speed)//stopped
	{
		return qtrue;
	}
	return qfalse;
}

qboolean FighterIsLanding(Vehicle_t* p_veh, playerState_t* parent_ps)
{
	if (FighterOverValidLandingSurface(p_veh)
#ifdef _GAME//only do this check on game side, because if it's cgame, it's being predicted, and it's only predicted if the local client is the driver
		&& p_veh->m_pVehicleInfo->Inhabited(p_veh)//has to have a driver in order to be capable of landing
#endif
		&& (p_veh->m_ucmd.forwardmove < 0 || p_veh->m_ucmd.upmove < 0) //decelerating or holding crouch button
		&& parent_ps->speed <= MIN_LANDING_SPEED)//going slow enough to start landing - was using p_veh->m_pVehicleInfo->speedIdle, but that's still too fast
	{
		return qtrue;
	}
	return qfalse;
}

qboolean FighterIsLaunching(Vehicle_t* p_veh, playerState_t* parent_ps)
{
	if (FighterOverValidLandingSurface(p_veh)
#ifdef _GAME//only do this check on game side, because if it's cgame, it's being predicted, and it's only predicted if the local client is the driver
		&& p_veh->m_pVehicleInfo->Inhabited(p_veh)//has to have a driver in order to be capable of landing
#endif
		&& p_veh->m_ucmd.upmove > 0 //trying to take off
		&& parent_ps->speed <= 200.0f)//going slow enough to start landing - was using p_veh->m_pVehicleInfo->speedIdle, but that's still too fast
	{
		return qtrue;
	}
	return qfalse;
}

qboolean FighterSuspended(Vehicle_t* p_veh, playerState_t* parent_ps)
{
#ifdef _GAME//only do this check on game side, because if it's cgame, it's being predicted, and it's only predicted if the local client is the driver
	if (!p_veh->m_pPilot//empty
		&& !parent_ps->speed//not moving
		&& p_veh->m_ucmd.forwardmove <= 0//not trying to go forward for whatever reason
		&& p_veh->m_pParentEntity != NULL
		&& (((gentity_t*)p_veh->m_pParentEntity)->spawnflags & 2))//SUSPENDED spawnflag is on
	{
		return qtrue;
	}
	return qfalse;
#elif _CGAME
	return qfalse;
#endif
}

//MP RULE - ALL PROCESSMOVECOMMANDS FUNCTIONS MUST BE BG-COMPATIBLE!!!
//If you really need to violate this rule for SP, then use ifdefs.
//By BG-compatible, I mean no use of game-specific data - ONLY use
//stuff available in the MP bgEntity (in SP, the bgEntity is #defined
//as a gentity, but the MP-compatible access restrictions are based
//on the bgEntity structure in the MP codebase) -rww
// ProcessMoveCommands the Vehicle.
#define FIGHTER_MIN_TAKEOFF_FRACTION 0.7f
static void ProcessMoveCommands(Vehicle_t* p_veh)
{
	/************************************************************************************/
	/*	BEGIN	Here is where we move the vehicle (forward or back or whatever). BEGIN	*/
	/************************************************************************************/

	//Client sets ucmds and such for speed alterations
	float speedMax;
	bgEntity_t* parent = p_veh->m_pParentEntity;
	//this function should only be called from pmove.. if it gets called elsehwere,
	//obviously this will explode.
	const int cur_time = pm->cmd.serverTime;

	playerState_t* parent_ps = parent->playerState;

	if (parent_ps->hyperSpaceTime
		&& cur_time - parent_ps->hyperSpaceTime < HYPERSPACE_TIME)
	{//Going to Hyperspace
		//totally override movement
		const float timeFrac = ((float)(cur_time - parent_ps->hyperSpaceTime)) / HYPERSPACE_TIME;
		if (timeFrac < HYPERSPACE_TELEPORT_FRAC)
		{//for first half, instantly jump to top speed!
			if (!(parent_ps->eFlags2 & EF2_HYPERSPACE))
			{//waiting to face the right direction, do nothing
				parent_ps->speed = 0.0f;
			}
			else
			{
				if (parent_ps->speed < HYPERSPACE_SPEED)
				{//just started hyperspace
//MIKE: This is going to play the sound twice for the predicting client, I suggest using
//a predicted event or only doing it game-side. -rich
#ifdef _GAME
					//G_EntitySound( ((gentity_t *)(p_veh->m_pParentEntity)), CHAN_LOCAL, p_veh->m_pVehicleInfo->soundHyper );
#elif _CGAME
					trap->S_StartSound(NULL, pm->ps->client_num, CHAN_LOCAL, p_veh->m_pVehicleInfo->soundHyper);
#endif
				}

				parent_ps->speed = HYPERSPACE_SPEED;
			}
		}
		else
		{//slow from top speed to 200...
			parent_ps->speed = 200.0f + ((1.0f - timeFrac) * (1.0f / HYPERSPACE_TELEPORT_FRAC) * (HYPERSPACE_SPEED - 200.0f));
			//don't mess with acceleration, just pop to the high velocity
			if (VectorLength(parent_ps->velocity) < parent_ps->speed)
			{
				VectorScale(parent_ps->moveDir, parent_ps->speed, parent_ps->velocity);
			}
		}
		return;
	}

	if (p_veh->m_iDropTime >= cur_time)
	{//no speed, just drop
		parent_ps->speed = 0.0f;
		parent_ps->gravity = 800;
		return;
	}

	const qboolean isLandingOrLaunching = (FighterIsLanding(p_veh, parent_ps) || FighterIsLaunching(p_veh, parent_ps));

	// If we are hitting the ground, just allow the fighter to go up and down.
	if (isLandingOrLaunching//going slow enough to start landing
		&& (p_veh->m_ucmd.forwardmove <= 0 || p_veh->m_LandTrace.fraction <= FIGHTER_MIN_TAKEOFF_FRACTION))//not trying to accelerate away already (or: you are trying to, but not high enough off the ground yet)
	{//FIXME: if start to move forward and fly over something low while still going relatively slow, you may try to land even though you don't mean to...
		//float fInvFrac = 1.0f - p_veh->m_LandTrace.fraction;

		if (p_veh->m_ucmd.upmove > 0)
		{
			if (parent_ps->velocity[2] <= 0
				&& p_veh->m_pVehicleInfo->soundTakeOff)
			{//taking off for the first time
#ifdef _GAME//MP GAME-side
				G_EntitySound(((gentity_t*)(p_veh->m_pParentEntity)), CHAN_AUTO, p_veh->m_pVehicleInfo->soundTakeOff);
#endif
			}
			parent_ps->velocity[2] += p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;// * ( /*fInvFrac **/ 1.5f );
		}
		else if (p_veh->m_ucmd.upmove < 0)
		{
			parent_ps->velocity[2] -= p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;// * ( /*fInvFrac **/ 1.8f );
		}
		else if (p_veh->m_ucmd.forwardmove < 0)
		{
			if (p_veh->m_LandTrace.fraction != 0.0f)
			{
				parent_ps->velocity[2] -= p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
			}

			if (p_veh->m_LandTrace.fraction <= FIGHTER_MIN_TAKEOFF_FRACTION)
			{
				//p_veh->m_pParentEntity->client->ps.velocity[0] *= p_veh->m_LandTrace.fraction;
				//p_veh->m_pParentEntity->client->ps.velocity[1] *= p_veh->m_LandTrace.fraction;

				//remember to always base this stuff on the time modifier! otherwise, you create
				//framerate-dependancy issues and break prediction in MP -rww
				//parent_ps->velocity[2] *= p_veh->m_LandTrace.fraction;
				//it's not an angle, but hey
				parent_ps->velocity[2] = PredictedAngularDecrement(p_veh->m_LandTrace.fraction, p_veh->m_fTimeModifier * 5.0f, parent_ps->velocity[2]);

				parent_ps->speed = 0;
			}
		}

		// Make sure they don't pitch as they near the ground.
		//p_veh->m_vOrientation[PITCH] *= 0.7f;
		p_veh->m_vOrientation[PITCH] = PredictedAngularDecrement(0.7f, p_veh->m_fTimeModifier * 10.0f, p_veh->m_vOrientation[PITCH]);

		return;
	}

	if ((p_veh->m_ucmd.upmove > 0) && p_veh->m_pVehicleInfo->turboSpeed)
	{
		if ((cur_time - p_veh->m_iTurboTime) > p_veh->m_pVehicleInfo->turboRecharge)
		{
			p_veh->m_iTurboTime = (cur_time + p_veh->m_pVehicleInfo->turboDuration);

#ifdef _GAME//MP GAME-side
			//NOTE: turbo sound can't be part of effect if effect is played on every muzzle!
			if (p_veh->m_pVehicleInfo->soundTurbo)
			{
				G_EntitySound(((gentity_t*)(p_veh->m_pParentEntity)), CHAN_AUTO, p_veh->m_pVehicleInfo->soundTurbo);
			}
#endif
		}
	}
	float speedInc = p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
	if (cur_time < p_veh->m_iTurboTime)
	{//going turbo speed
		speedMax = p_veh->m_pVehicleInfo->turboSpeed;
		//double our acceleration
		//speedInc *= 2.0f;
		//no no no! this would el breako el predictiono! we want the following... -rww
		speedInc = (p_veh->m_pVehicleInfo->acceleration * 2.0f) * p_veh->m_fTimeModifier;

		//force us to move forward
		p_veh->m_ucmd.forwardmove = 127;
		//add flag to let cgame know to draw the iTurboFX effect
		parent_ps->eFlags |= EF_JETPACK_ACTIVE;
	}
	/*
	//FIXME: if turbotime is up and we're waiting for it to recharge, should our max speed drop while we recharge?
	else if ( (cur_time - p_veh->m_iTurboTime)<3000 )
	{//still waiting for the recharge
		speedMax = p_veh->m_pVehicleInfo->speedMax*0.75;
	}
	*/
	else
	{//normal max speed
		speedMax = p_veh->m_pVehicleInfo->speedMax;
		if ((parent_ps->eFlags & EF_JETPACK_ACTIVE))
		{//stop cgame from playing the turbo exhaust effect
			parent_ps->eFlags &= ~EF_JETPACK_ACTIVE;
		}
	}
	float speedIdleDec = p_veh->m_pVehicleInfo->decelIdle * p_veh->m_fTimeModifier;
	const float speedIdle = p_veh->m_pVehicleInfo->speedIdle;
	const float speedIdleAccel = p_veh->m_pVehicleInfo->accelIdle * p_veh->m_fTimeModifier;
	const float speedMin = p_veh->m_pVehicleInfo->speedMin;

	if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_BACK_HEAVY)))
	{//engine has taken heavy damage
		speedMax *= 0.8f;//at 80% speed
	}
	else if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_BACK_LIGHT)))
	{//engine has taken light damage
		speedMax *= 0.6f;//at 60% speed
	}

	if (p_veh->m_iRemovedSurfaces
		|| parent_ps->electrifyTime >= cur_time)
	{ //go out of control
		parent_ps->speed += speedInc;
		//Why set forwardmove?  PMove code doesn't use it... does it?
		p_veh->m_ucmd.forwardmove = 127;
	}
#ifdef _GAME //well, the thing is always going to be inhabited if it's being predicted!
	else if (FighterSuspended(p_veh, parent_ps))
	{
		parent_ps->speed = 0;
		p_veh->m_ucmd.forwardmove = 0;
	}
	else if (!p_veh->m_pVehicleInfo->Inhabited(p_veh)
		&& parent_ps->speed > 0)
	{//pilot jumped out while we were moving forward (not landing or landed) so just keep the throttle locked
		//Why set forwardmove?  PMove code doesn't use it... does it?
		p_veh->m_ucmd.forwardmove = 127;
	}
#endif
	else if ((parent_ps->speed || parent_ps->groundEntityNum == ENTITYNUM_NONE ||
		p_veh->m_ucmd.forwardmove || p_veh->m_ucmd.upmove > 0) && p_veh->m_LandTrace.fraction >= 0.05f)
	{
		if (p_veh->m_ucmd.forwardmove > 0 && speedInc)
		{
			parent_ps->speed += speedInc;
			p_veh->m_ucmd.forwardmove = 127;
		}
		else if (p_veh->m_ucmd.forwardmove < 0
			|| p_veh->m_ucmd.upmove < 0)
		{//decelerating or braking
			if (p_veh->m_ucmd.upmove < 0)
			{//braking (trying to land?), slow down faster
				if (p_veh->m_ucmd.forwardmove)
				{//decelerator + brakes
					speedInc += p_veh->m_pVehicleInfo->braking;
					speedIdleDec += p_veh->m_pVehicleInfo->braking;
				}
				else
				{//just brakes
					speedInc = speedIdleDec = p_veh->m_pVehicleInfo->braking;
				}
			}
			if (parent_ps->speed > speedIdle)
			{
				parent_ps->speed -= speedInc;
			}
			else if (parent_ps->speed > speedMin)
			{
				if (FighterOverValidLandingSurface(p_veh))
				{//there's ground below us and we're trying to slow down, slow down faster
					parent_ps->speed -= speedInc;
				}
				else
				{
					parent_ps->speed -= speedIdleDec;
					if (parent_ps->speed < MIN_LANDING_SPEED)
					{//unless you can land, don't drop below the landing speed!!!  This way you can't come to a dead stop in mid-air
						parent_ps->speed = MIN_LANDING_SPEED;
					}
				}
			}
			if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
			{
				p_veh->m_ucmd.forwardmove = 127;
			}
			else if (speedMin >= 0)
			{
				p_veh->m_ucmd.forwardmove = 0;
			}
		}
		//else not accel, decel or braking
		else if (p_veh->m_pVehicleInfo->throttleSticks)
		{//we're using a throttle that sticks at current speed
			if (parent_ps->speed <= MIN_LANDING_SPEED)
			{//going less than landing speed
				if (FighterOverValidLandingSurface(p_veh))
				{//close to ground and not going very fast
					//slow to a stop if within landing height and not accel/decel/braking
					if (parent_ps->speed > 0)
					{//slow down
						parent_ps->speed -= speedIdleDec;
					}
					else if (parent_ps->speed < 0)
					{//going backwards, slow down
						parent_ps->speed += speedIdleDec;
					}
				}
				else
				{//not over a valid landing surf, but going too slow
					//speed up to idle speed if not over a valid landing surf and not accel/decel/braking
					if (parent_ps->speed < speedIdle)
					{
						parent_ps->speed += speedIdleAccel;
						if (parent_ps->speed > speedIdle)
						{
							parent_ps->speed = speedIdle;
						}
					}
				}
			}
		}
		else
		{//then speed up or slow down to idle speed
			//accelerate to cruising speed only, otherwise, just coast
			// If they've launched, apply some constant motion.
			if ((p_veh->m_LandTrace.fraction >= 1.0f //no ground
				|| p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE)//or can't land on ground below us
				&& speedIdle > 0)
			{//not above ground and have an idle speed
				//float fSpeed = p_veh->m_pParentEntity->client->ps.speed;
				if (parent_ps->speed < speedIdle)
				{
					parent_ps->speed += speedIdleAccel;
					if (parent_ps->speed > speedIdle)
					{
						parent_ps->speed = speedIdle;
					}
				}
				else if (parent_ps->speed > 0)
				{//slow down
					parent_ps->speed -= speedIdleDec;

					if (parent_ps->speed < speedIdle)
					{
						parent_ps->speed = speedIdle;
					}
				}
			}
			else//either close to ground or no idle speed
			{//slow to a stop if no idle speed or within landing height and not accel/decel/braking
				if (parent_ps->speed > 0)
				{//slow down
					parent_ps->speed -= speedIdleDec;
				}
				else if (parent_ps->speed < 0)
				{//going backwards, slow down
					parent_ps->speed += speedIdleDec;
				}
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
	}

#if 1//This is working now, but there are some transitional jitters... Rich?
	//STRAFING==============================================================================
	if (p_veh->m_pVehicleInfo->strafePerc
#ifdef _GAME//only do this check on game side, because if it's cgame, it's being predicted, and it's only predicted if the local client is the driver
		&& p_veh->m_pVehicleInfo->Inhabited(p_veh)//has to have a driver in order to be capable of landing
#endif
		&& !p_veh->m_iRemovedSurfaces
		&& parent_ps->electrifyTime < cur_time
		&& (p_veh->m_LandTrace.fraction >= 1.0f//no grounf
			|| p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE//can't land here
			|| parent_ps->speed>MIN_LANDING_SPEED)//going too fast to land
		&& p_veh->m_ucmd.rightmove)
	{//strafe
		vec3_t vAngles, vRight;
		float strafeSpeed = (p_veh->m_pVehicleInfo->strafePerc * speedMax) * 5.0f;
		VectorCopy(p_veh->m_vOrientation, vAngles);
		vAngles[PITCH] = vAngles[ROLL] = 0;
		AngleVectors(vAngles, NULL, vRight, NULL);

		if (p_veh->m_ucmd.rightmove > 0)
		{//strafe right
			//FIXME: this will probably make it possible to cheat and
			//		go faster than max speed if you keep turning and strafing...
			if (parent_ps->hackingTime > -MAX_STRAFE_TIME)
			{//can strafe right for 2 seconds
				const float curStrafeSpeed = DotProduct(parent_ps->velocity, vRight);
				if (curStrafeSpeed > 0.0f)
				{//if > 0, already strafing right
					strafeSpeed -= curStrafeSpeed;//so it doesn't add up
				}
				if (strafeSpeed > 0)
				{
					VectorMA(parent_ps->velocity, strafeSpeed * p_veh->m_fTimeModifier, vRight, parent_ps->velocity);
				}
				parent_ps->hackingTime -= 50 * p_veh->m_fTimeModifier;
			}
		}
		else
		{//strafe left
			if (parent_ps->hackingTime < MAX_STRAFE_TIME)
			{//can strafe left for 2 seconds
				const float curStrafeSpeed = DotProduct(parent_ps->velocity, vRight);
				if (curStrafeSpeed < 0.0f)
				{//if < 0, already strafing left
					strafeSpeed += curStrafeSpeed;//so it doesn't add up
				}
				if (strafeSpeed > 0)
				{
					VectorMA(parent_ps->velocity, -strafeSpeed * p_veh->m_fTimeModifier, vRight, parent_ps->velocity);
				}
				parent_ps->hackingTime += 50 * p_veh->m_fTimeModifier;
			}
		}
		//strafing takes away from forward speed?  If so, strafePerc above should use speedMax
		//parent_ps->speed *= (1.0f-p_veh->m_pVehicleInfo->strafePerc);
	}
	else//if ( parent_ps->hackingTimef )
	{
		if (parent_ps->hackingTime > 0)
		{
			parent_ps->hackingTime -= 50 * p_veh->m_fTimeModifier;
			if (parent_ps->hackingTime < 0)
			{
				parent_ps->hackingTime = 0.0f;
			}
		}
		else if (parent_ps->hackingTime < 0)
		{
			parent_ps->hackingTime += 50 * p_veh->m_fTimeModifier;
			if (parent_ps->hackingTime > 0)
			{
				parent_ps->hackingTime = 0.0f;
			}
		}
	}
	//STRAFING==============================================================================
#endif

	if (parent_ps->speed > speedMax)
	{
		parent_ps->speed = speedMax;
	}
	else if (parent_ps->speed < speedMin)
	{
		parent_ps->speed = speedMin;
	}

#ifdef _GAME//FIXME: get working in game and cgame
	if ((p_veh->m_vOrientation[PITCH] * 0.1f) > 10.0f)
	{ //pitched downward, increase speed more and more based on our tilt
		if (FighterIsInSpace((gentity_t*)parent))
		{//in space, do nothing with speed base on pitch...
		}
		else
		{
			//really should only do this when on a planet
			float mult = p_veh->m_vOrientation[PITCH] * 0.1f;
			if (mult < 1.0f)
			{
				mult = 1.0f;
			}
			parent_ps->speed = PredictedAngularDecrement(mult, p_veh->m_fTimeModifier * 10.0f, parent_ps->speed);
		}
	}

	if (p_veh->m_iRemovedSurfaces
		|| parent_ps->electrifyTime >= cur_time)
	{ //going down
		if (FighterIsInSpace((gentity_t*)parent))
		{//we're in a valid trigger_space brush
			//simulate randomness
			if (!(parent->s.number & 3))
			{//even multiple of 3, don't do anything
				parent_ps->gravity = 0;
			}
			else if (!(parent->s.number & 2))
			{//even multiple of 2, go up
				parent_ps->gravity = -500.0f;
				parent_ps->velocity[2] = 80.0f;
			}
			else
			{//odd number, go down
				parent_ps->gravity = 500.0f;
				parent_ps->velocity[2] = -80.0f;
			}
		}
		else
		{//over a planet
			parent_ps->gravity = 500.0f;
			parent_ps->velocity[2] = -80.0f;
		}
	}
	else if (FighterSuspended(p_veh, parent_ps))
	{
		parent_ps->gravity = 0;
	}
	else if ((!parent_ps->speed || parent_ps->speed < speedIdle) && p_veh->m_ucmd.upmove <= 0)
	{//slowing down or stopped and not trying to take off
		if (FighterIsInSpace((gentity_t*)parent))
		{//we're in space, stopping doesn't make us drift downward
			if (FighterOverValidLandingSurface(p_veh))
			{//well, there's something below us to land on, so go ahead and lower us down to it
				parent_ps->gravity = (speedIdle - parent_ps->speed) / 4;
			}
		}
		else
		{//over a planet
			parent_ps->gravity = (speedIdle - parent_ps->speed) / 4;
		}
	}
	else
	{
		parent_ps->gravity = 0;
	}
#else//FIXME: get above checks working in game and cgame
	parent_ps->gravity = 0;
#endif

	/********************************************************************************/
	/*	END Here is where we move the vehicle (forward or back or whatever). END	*/
	/********************************************************************************/
}

extern void BG_VehicleTurnRateForSpeed(Vehicle_t* p_veh, float speed, float* mPitchOverride, float* mYawOverride);
static void FighterWingMalfunctionCheck(Vehicle_t* p_veh, playerState_t* parent_ps)
{
	float mPitchOverride = 1.0f;
	float mYawOverride = 1.0f;
	BG_VehicleTurnRateForSpeed(p_veh, parent_ps->speed, &mPitchOverride, &mYawOverride);
	//check right wing damage
	if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_RIGHT_HEAVY)))
	{//right wing has taken heavy damage
		p_veh->m_vOrientation[ROLL] += (sin(p_veh->m_ucmd.serverTime * 0.001) + 1.0f) * p_veh->m_fTimeModifier * mYawOverride * 50.0f;
	}
	else if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_RIGHT_LIGHT)))
	{//right wing has taken light damage
		p_veh->m_vOrientation[ROLL] += (sin(p_veh->m_ucmd.serverTime * 0.001) + 1.0f) * p_veh->m_fTimeModifier * mYawOverride * 12.5f;
	}

	//check left wing damage
	if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_LEFT_HEAVY)))
	{//left wing has taken heavy damage
		p_veh->m_vOrientation[ROLL] -= (sin(p_veh->m_ucmd.serverTime * 0.001) + 1.0f) * p_veh->m_fTimeModifier * mYawOverride * 50.0f;
	}
	else if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_LEFT_LIGHT)))
	{//left wing has taken light damage
		p_veh->m_vOrientation[ROLL] -= (sin(p_veh->m_ucmd.serverTime * 0.001) + 1.0f) * p_veh->m_fTimeModifier * mYawOverride * 12.5f;
	}
}

static void FighterNoseMalfunctionCheck(Vehicle_t* p_veh, playerState_t* parent_ps)
{
	float mPitchOverride = 1.0f;
	float mYawOverride = 1.0f;
	BG_VehicleTurnRateForSpeed(p_veh, parent_ps->speed, &mPitchOverride, &mYawOverride);
	//check nose damage
	if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_FRONT_HEAVY)))
	{//nose has taken heavy damage
		//pitch up and down over time
		p_veh->m_vOrientation[PITCH] += sin(p_veh->m_ucmd.serverTime * 0.001) * p_veh->m_fTimeModifier * mPitchOverride * 50.0f;
	}
	else if ((parent_ps->brokenLimbs & (1 << SHIPSURF_DAMAGE_FRONT_LIGHT)))
	{//nose has taken heavy damage
		//pitch up and down over time
		p_veh->m_vOrientation[PITCH] += sin(p_veh->m_ucmd.serverTime * 0.001) * p_veh->m_fTimeModifier * mPitchOverride * 20.0f;
	}
}

static void FighterDamageRoutine(Vehicle_t* p_veh, bgEntity_t* parent, playerState_t* parent_ps, playerState_t* rider_ps, qboolean isDead)
{
	if (!p_veh->m_iRemovedSurfaces)
	{//still in one piece
		if (p_veh->m_pParentEntity && isDead)
		{//death spiral
			p_veh->m_ucmd.upmove = 0;
			//FIXME: don't bias toward pitching down when not in space
			/*
			if ( FighterIsInSpace( p_veh->m_pParentEntity ) )
			{
			}
			else
			*/
			if (!(p_veh->m_pParentEntity->s.number % 3))
			{//NOT everyone should do this
				p_veh->m_vOrientation[PITCH] += p_veh->m_fTimeModifier;
				if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
				{
					if (p_veh->m_vOrientation[PITCH] > 60.0f)
					{
						p_veh->m_vOrientation[PITCH] = 60.0f;
					}
				}
			}
			else if (!(p_veh->m_pParentEntity->s.number % 2))
			{
				p_veh->m_vOrientation[PITCH] -= p_veh->m_fTimeModifier;
				if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
				{
					if (p_veh->m_vOrientation[PITCH] > -60.0f)
					{
						p_veh->m_vOrientation[PITCH] = -60.0f;
					}
				}
			}
			if ((p_veh->m_pParentEntity->s.number % 2))
			{
				p_veh->m_vOrientation[YAW] += p_veh->m_fTimeModifier;
				p_veh->m_vOrientation[ROLL] += p_veh->m_fTimeModifier * 4.0f;
			}
			else
			{
				p_veh->m_vOrientation[YAW] -= p_veh->m_fTimeModifier;
				p_veh->m_vOrientation[ROLL] -= p_veh->m_fTimeModifier * 4.0f;
			}
		}
		return;
	}

	//if we get into here we have at least one broken piece
	p_veh->m_ucmd.upmove = 0;

	//if you're off the ground and not suspended, pitch down
	//FIXME: not in space!
	if (p_veh->m_LandTrace.fraction >= 0.1f)
	{
		if (!FighterSuspended(p_veh, parent_ps))
		{
			//p_veh->m_ucmd.forwardmove = 0;
			//FIXME: don't bias towards pitching down when in space...
			if (!(p_veh->m_pParentEntity->s.number % 2))
			{//NOT everyone should do this
				p_veh->m_vOrientation[PITCH] += p_veh->m_fTimeModifier;
				if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
				{
					if (p_veh->m_vOrientation[PITCH] > 60.0f)
					{
						p_veh->m_vOrientation[PITCH] = 60.0f;
					}
				}
			}
			else if (!(p_veh->m_pParentEntity->s.number % 3))
			{
				p_veh->m_vOrientation[PITCH] -= p_veh->m_fTimeModifier;
				if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
				{
					if (p_veh->m_vOrientation[PITCH] > -60.0f)
					{
						p_veh->m_vOrientation[PITCH] = -60.0f;
					}
				}
			}
			//else: just keep going forward
		}
	}
#ifdef _GAME
	if (p_veh->m_LandTrace.fraction < 1.0f)
	{ //if you land at all when pieces of your ship are missing, then die
		gentity_t* vparent = (gentity_t*)p_veh->m_pParentEntity;
		gentity_t* killer = vparent;
		if (vparent->client->ps.otherKiller < ENTITYNUM_WORLD &&
			vparent->client->ps.otherKillerTime > level.time)
		{
			gentity_t* potentialKiller = &g_entities[vparent->client->ps.otherKiller];

			if (potentialKiller->inuse && potentialKiller->client)
			{ //he's valid I guess
				killer = potentialKiller;
			}
		}
		G_Damage(vparent, killer, killer, vec3_origin, vparent->client->ps.origin, 99999, DAMAGE_NO_ARMOR, MOD_SUICIDE);
	}
#endif

	if (((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) ||
		(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D)) &&
		((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) ||
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F)))
	{ //wings on both side broken
		float factor = 2.0f;
		if ((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D))
		{ //all wings broken
			factor *= 2.0f;
		}

		if (!(p_veh->m_pParentEntity->s.number % 4) || !(p_veh->m_pParentEntity->s.number % 5))
		{//won't yaw, so increase roll factor
			factor *= 4.0f;
		}

		p_veh->m_vOrientation[ROLL] += (p_veh->m_fTimeModifier * factor); //do some spiralling
	}
	else if ((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) ||
		(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D))
	{ //left wing broken
		float factor = 2.0f;
		if ((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D))
		{ //if both are broken..
			factor *= 2.0f;
		}

		if (!(p_veh->m_pParentEntity->s.number % 4) || !(p_veh->m_pParentEntity->s.number % 5))
		{//won't yaw, so increase roll factor
			factor *= 4.0f;
		}

		p_veh->m_vOrientation[ROLL] += factor * p_veh->m_fTimeModifier;
	}
	else if ((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) ||
		(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F))
	{ //right wing broken
		float factor = 2.0f;
		if ((p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F))
		{ //if both are broken..
			factor *= 2.0f;
		}

		if (!(p_veh->m_pParentEntity->s.number % 4) || !(p_veh->m_pParentEntity->s.number % 5))
		{//won't yaw, so increase roll factor
			factor *= 4.0f;
		}

		p_veh->m_vOrientation[ROLL] -= factor * p_veh->m_fTimeModifier;
	}
}

#ifdef VEH_CONTROL_SCHEME_4

#define FIGHTER_TURNING_MULTIPLIER 0.8f//was 1.6f //magic number hackery
#define FIGHTER_TURNING_DEADZONE 0.25f//no turning if offset is this much
void FighterRollAdjust(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps)
{
	/*
		float angDif = AngleSubtract(p_veh->m_vOrientation[YAW], rider_ps->viewangles[YAW]);
	*/
	float angDif = AngleSubtract(p_veh->m_vPrevRiderViewAngles[YAW], rider_ps->viewangles[YAW]);///2.0f;//AngleSubtract(p_veh->m_vPrevRiderViewAngles[YAW], rider_ps->viewangles[YAW]);
	/*
	if ( fabs( angDif ) < FIGHTER_TURNING_DEADZONE )
	{
		angDif = 0.0f;
	}
	else if ( angDif >= FIGHTER_TURNING_DEADZONE )
	{
		angDif -= FIGHTER_TURNING_DEADZONE;
	}
	else if ( angDif <= -FIGHTER_TURNING_DEADZONE )
	{
		angDif += FIGHTER_TURNING_DEADZONE;
	}
	*/

	angDif *= 0.5f;
	if (angDif > 0.0f)
	{
		angDif *= angDif;
	}
	else if (angDif < 0.0f)
	{
		angDif *= -angDif;
	}

	if (parent_ps && parent_ps->speed)
	{
		float maxDif = p_veh->m_pVehicleInfo->turningSpeed * FIGHTER_TURNING_MULTIPLIER;

		if (p_veh->m_pVehicleInfo->speedDependantTurning)
		{
			float speedFrac = 1.0f;
			if (p_veh->m_LandTrace.fraction >= 1.0f
				|| p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE)
			{
				float s = parent_ps->speed;
				if (s < 0.0f)
				{
					s = -s;
				}
				speedFrac = (s / (p_veh->m_pVehicleInfo->speedMax * 0.75f));
				if (speedFrac < 0.25f)
				{
					speedFrac = 0.25f;
				}
				else if (speedFrac > 1.0f)
				{
					speedFrac = 1.0f;
				}
			}
			angDif *= speedFrac;
		}
		if (angDif > maxDif)
		{
			angDif = maxDif;
		}
		else if (angDif < -maxDif)
		{
			angDif = -maxDif;
		}
		p_veh->m_vOrientation[ROLL] = AngleNormalize180(p_veh->m_vOrientation[ROLL] + angDif * (p_veh->m_fTimeModifier * 0.2f));
	}
}

void FighterYawAdjust(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps)
{
	float angDif = AngleSubtract(p_veh->m_vPrevRiderViewAngles[YAW], rider_ps->viewangles[YAW]);///2.0f;//AngleSubtract(p_veh->m_vPrevRiderViewAngles[YAW], rider_ps->viewangles[YAW]);
	if (fabs(angDif) < FIGHTER_TURNING_DEADZONE)
	{
		angDif = 0.0f;
	}
	else if (angDif >= FIGHTER_TURNING_DEADZONE)
	{
		angDif -= FIGHTER_TURNING_DEADZONE;
	}
	else if (angDif <= -FIGHTER_TURNING_DEADZONE)
	{
		angDif += FIGHTER_TURNING_DEADZONE;
	}

	angDif *= 0.5f;
	if (angDif > 0.0f)
	{
		angDif *= angDif;
	}
	else if (angDif < 0.0f)
	{
		angDif *= -angDif;
	}

	if (parent_ps && parent_ps->speed)
	{
		float maxDif = p_veh->m_pVehicleInfo->turningSpeed * FIGHTER_TURNING_MULTIPLIER;

		if (p_veh->m_pVehicleInfo->speedDependantTurning)
		{
			float speedFrac = 1.0f;
			if (p_veh->m_LandTrace.fraction >= 1.0f
				|| p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE)
			{
				float s = parent_ps->speed;
				if (s < 0.0f)
				{
					s = -s;
				}
				speedFrac = (s / (p_veh->m_pVehicleInfo->speedMax * 0.75f));
				if (speedFrac < 0.25f)
				{
					speedFrac = 0.25f;
				}
				else if (speedFrac > 1.0f)
				{
					speedFrac = 1.0f;
				}
			}
			angDif *= speedFrac;
		}
		if (angDif > maxDif)
		{
			angDif = maxDif;
		}
		else if (angDif < -maxDif)
		{
			angDif = -maxDif;
		}
		p_veh->m_vOrientation[YAW] = AngleNormalize180(p_veh->m_vOrientation[YAW] - (angDif * (p_veh->m_fTimeModifier * 0.2f)));
	}
}

void FighterPitchAdjust(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps)
{
	float angDif = AngleSubtract(0, rider_ps->viewangles[PITCH]);//AngleSubtract(p_veh->m_vPrevRiderViewAngles[PITCH], rider_ps->viewangles[PITCH]);
	if (fabs(angDif) < FIGHTER_TURNING_DEADZONE)
	{
		angDif = 0.0f;
	}
	else if (angDif >= FIGHTER_TURNING_DEADZONE)
	{
		angDif -= FIGHTER_TURNING_DEADZONE;
	}
	else if (angDif <= -FIGHTER_TURNING_DEADZONE)
	{
		angDif += FIGHTER_TURNING_DEADZONE;
	}

	angDif *= 0.5f;
	if (angDif > 0.0f)
	{
		angDif *= angDif;
	}
	else if (angDif < 0.0f)
	{
		angDif *= -angDif;
	}

	if (parent_ps && parent_ps->speed)
	{
		float maxDif = p_veh->m_pVehicleInfo->turningSpeed * FIGHTER_TURNING_MULTIPLIER;

		if (p_veh->m_pVehicleInfo->speedDependantTurning)
		{
			float speedFrac = 1.0f;
			if (p_veh->m_LandTrace.fraction >= 1.0f
				|| p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE)
			{
				float s = parent_ps->speed;
				if (s < 0.0f)
				{
					s = -s;
				}
				speedFrac = (s / (p_veh->m_pVehicleInfo->speedMax * 0.75f));
				if (speedFrac < 0.25f)
				{
					speedFrac = 0.25f;
				}
				else if (speedFrac > 1.0f)
				{
					speedFrac = 1.0f;
				}
			}
			angDif *= speedFrac;
		}
		if (angDif > maxDif)
		{
			angDif = maxDif;
		}
		else if (angDif < -maxDif)
		{
			angDif = -maxDif;
		}
		p_veh->m_vOrientation[PITCH] = AngleNormalize180(p_veh->m_vOrientation[PITCH] - (angDif * (p_veh->m_fTimeModifier * 0.2f)));
	}
}

#else// VEH_CONTROL_SCHEME_4

void FighterYawAdjust(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps)
{
	float angDif = AngleSubtract(p_veh->m_vOrientation[YAW], rider_ps->viewangles[YAW]);

	if (parent_ps && parent_ps->speed)
	{
		float s = parent_ps->speed;
		const float maxDif = p_veh->m_pVehicleInfo->turningSpeed * 0.8f; //magic number hackery

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

void FighterPitchAdjust(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps)
{
	float angDif = AngleSubtract(p_veh->m_vOrientation[PITCH], rider_ps->viewangles[PITCH]);

	if (parent_ps && parent_ps->speed)
	{
		float s = parent_ps->speed;
		const float maxDif = p_veh->m_pVehicleInfo->turningSpeed * 0.8f; //magic number hackery

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
		p_veh->m_vOrientation[PITCH] = AngleNormalize360(p_veh->m_vOrientation[PITCH] - angDif * (p_veh->m_fTimeModifier * 0.2f));
	}
}
#endif// VEH_CONTROL_SCHEME_4

void FighterPitchClamp(Vehicle_t* p_veh, playerState_t* rider_ps, playerState_t* parent_ps, int cur_time)
{
	if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
	{//cap pitch reasonably
		if (p_veh->m_pVehicleInfo->pitchLimit != -1
			&& !p_veh->m_iRemovedSurfaces
			&& parent_ps->electrifyTime < cur_time)
		{
			if (p_veh->m_vOrientation[PITCH] > p_veh->m_pVehicleInfo->pitchLimit)
			{
				p_veh->m_vOrientation[PITCH] = p_veh->m_pVehicleInfo->pitchLimit;
			}
			else if (p_veh->m_vOrientation[PITCH] < -p_veh->m_pVehicleInfo->pitchLimit)
			{
				p_veh->m_vOrientation[PITCH] = -p_veh->m_pVehicleInfo->pitchLimit;
			}
		}
	}
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

	bgEntity_t* parent = p_veh->m_pParentEntity;
	playerState_t* parent_ps, * rider_ps;
	float angleTimeMod;
#ifdef _GAME
	const float groundFraction = 0.1f;
#endif
	float	curRoll = 0.0f;
	qboolean isDead = qfalse;
	qboolean isLandingOrLanded = qfalse;
#ifdef _GAME
	int cur_time = level.time;
#elif _CGAME
	//FIXME: pass in ucmd?  Not sure if this is reliable...
	int cur_time = pm->cmd.serverTime;
#endif

	const bgEntity_t* rider = NULL;
	if (parent->s.owner != ENTITYNUM_NONE)
	{
		rider = PM_BGEntForNum(parent->s.owner); //&g_entities[parent->r.ownerNum];
	}

	if (!rider)
	{
		rider = parent;
	}

	parent_ps = parent->playerState;
	rider_ps = rider->playerState;
	isDead = (qboolean)((parent_ps->eFlags & EF_DEAD) != 0);

#ifdef VEH_CONTROL_SCHEME_4
	if (parent_ps->hyperSpaceTime
		&& (cur_time - parent_ps->hyperSpaceTime) < HYPERSPACE_TIME)
	{//Going to Hyperspace
		//VectorCopy( rider_ps->viewangles, p_veh->m_vOrientation );
		//VectorCopy( rider_ps->viewangles, parent_ps->viewangles );
		VectorCopy(parent_ps->viewangles, p_veh->m_vOrientation);
		VectorClear(p_veh->m_vPrevRiderViewAngles);
		p_veh->m_vPrevRiderViewAngles[YAW] = AngleNormalize180(rider_ps->viewangles[YAW]);
		FighterPitchClamp(p_veh, rider_ps, parent_ps, cur_time);
		return;
	}
	else if (parent_ps->vehTurnaroundIndex
		&& parent_ps->vehTurnaroundTime > cur_time)
	{//in turn-around brush
		//VectorCopy( rider_ps->viewangles, p_veh->m_vOrientation );
		//VectorCopy( rider_ps->viewangles, parent_ps->viewangles );
		VectorCopy(parent_ps->viewangles, p_veh->m_vOrientation);
		VectorClear(p_veh->m_vPrevRiderViewAngles);
		p_veh->m_vPrevRiderViewAngles[YAW] = AngleNormalize180(rider_ps->viewangles[YAW]);
		FighterPitchClamp(p_veh, rider_ps, parent_ps, cur_time);
		return;
	}

#else// VEH_CONTROL_SCHEME_4

	if (parent_ps->hyperSpaceTime
		&& (cur_time - parent_ps->hyperSpaceTime) < HYPERSPACE_TIME)
	{//Going to Hyperspace
		VectorCopy(rider_ps->viewangles, p_veh->m_vOrientation);
		VectorCopy(rider_ps->viewangles, parent_ps->viewangles);
		return;
	}
#endif// VEH_CONTROL_SCHEME_4

	if (p_veh->m_iDropTime >= cur_time)
	{//you can only YAW during this
		parent_ps->viewangles[YAW] = p_veh->m_vOrientation[YAW] = rider_ps->viewangles[YAW];
#ifdef VEH_CONTROL_SCHEME_4
		VectorClear(p_veh->m_vPrevRiderViewAngles);
		p_veh->m_vPrevRiderViewAngles[YAW] = AngleNormalize180(rider_ps->viewangles[YAW]);
#endif// VEH_CONTROL_SCHEME_4
		return;
	}

	angleTimeMod = p_veh->m_fTimeModifier;

	if (isDead || parent_ps->electrifyTime >= cur_time ||
		(p_veh->m_pVehicleInfo->surfDestruction &&
			p_veh->m_iRemovedSurfaces &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) &&
			(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F)))
	{ //do some special stuff for when all the wings are torn off
		FighterDamageRoutine(p_veh, parent, parent_ps, rider_ps, isDead);
		p_veh->m_vOrientation[ROLL] = AngleNormalize180(p_veh->m_vOrientation[ROLL]);
		return;
	}

	if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
	{
		p_veh->m_vOrientation[ROLL] = PredictedAngularDecrement(0.95f, angleTimeMod * 2.0f, p_veh->m_vOrientation[ROLL]);
	}

	isLandingOrLanded = (FighterIsLanding(p_veh, parent_ps) || FighterIsLanded(p_veh, parent_ps));

	if (!isLandingOrLanded)
	{ //don't do this stuff while landed.. I guess. I don't want ships spinning in place, looks silly.
		int m = 0;

		FighterWingMalfunctionCheck(p_veh, parent_ps);

		while (m < 3)
		{
			const float aVelDif = p_veh->m_vFullAngleVelocity[m];

			if (aVelDif != 0.0f)
			{
				const float dForVel = (aVelDif * 0.1f) * p_veh->m_fTimeModifier;
				if (dForVel > 1.0f || dForVel < -1.0f)
				{
					p_veh->m_vOrientation[m] += dForVel;
					p_veh->m_vOrientation[m] = AngleNormalize180(p_veh->m_vOrientation[m]);
					if (m == PITCH)
					{ //don't pitch downward into ground even more.
						if (p_veh->m_vOrientation[m] > 90.0f && (p_veh->m_vOrientation[m] - dForVel) < 90.0f)
						{
							p_veh->m_vOrientation[m] = 90.0f;
							p_veh->m_vFullAngleVelocity[m] = -p_veh->m_vFullAngleVelocity[m];
						}
					}
					p_veh->m_vFullAngleVelocity[m] -= dForVel;
				}
				else
				{
					p_veh->m_vFullAngleVelocity[m] = 0.0f;
				}
			}

			m++;
		}
	}
	else
	{ //clear decr/incr angles once landed.
		VectorClear(p_veh->m_vFullAngleVelocity);
	}

	curRoll = p_veh->m_vOrientation[ROLL];

	// If we're landed, we shouldn't be able to do anything but take off.
	if (isLandingOrLanded //going slow enough to start landing
		&& !p_veh->m_iRemovedSurfaces
		&& parent_ps->electrifyTime < cur_time)//not spiraling out of control
	{
		if (parent_ps->speed > 0.0f)
		{//Uh... what?  Why?
			if (p_veh->m_LandTrace.fraction < 0.3f)
			{
				p_veh->m_vOrientation[PITCH] = 0.0f;
			}
			else
			{
				p_veh->m_vOrientation[PITCH] = PredictedAngularDecrement(0.83f, angleTimeMod * 10.0f, p_veh->m_vOrientation[PITCH]);
			}
		}
		if (p_veh->m_LandTrace.fraction > 0.1f
			|| p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE)
		{//off the ground, at least (or not on a valid landing surf)
			// Dampen the turn rate based on the current height.
			FighterYawAdjust(p_veh, rider_ps, parent_ps);
		}
#ifdef VEH_CONTROL_SCHEME_4
		else
		{
			VectorClear(p_veh->m_vPrevRiderViewAngles);
			p_veh->m_vPrevRiderViewAngles[YAW] = AngleNormalize180(rider_ps->viewangles[YAW]);
		}
#endif// VEH_CONTROL_SCHEME_4
	}
	else if ((p_veh->m_iRemovedSurfaces || parent_ps->electrifyTime >= cur_time)//spiralling out of control
		&& (!(p_veh->m_pParentEntity->s.number % 4) || !(p_veh->m_pParentEntity->s.number % 5)))
	{//no yaw control
	}
	else if (p_veh->m_pPilot && p_veh->m_pPilot->s.number < MAX_CLIENTS && parent_ps->speed > 0.0f)//&& !( p_veh->m_ucmd.forwardmove > 0 && p_veh->m_LandTrace.fraction != 1.0f ) )
	{
#ifdef VEH_CONTROL_SCHEME_4
		if (0)
#else// VEH_CONTROL_SCHEME_4
		if (BG_UnrestrainedPitchRoll(rider_ps, p_veh))
#endif// VEH_CONTROL_SCHEME_4
		{
			VectorCopy(rider_ps->viewangles, p_veh->m_vOrientation);
			VectorCopy(rider_ps->viewangles, parent_ps->viewangles);

			curRoll = p_veh->m_vOrientation[ROLL];

			FighterNoseMalfunctionCheck(p_veh, parent_ps);

			//VectorCopy( p_veh->m_vOrientation, parent_ps->viewangles );
		}
		else
		{
			/*
			float fTurnAmt[3];
			//PITCH
			fTurnAmt[PITCH] = rider_ps->viewangles[PITCH] * 0.08f;
			//YAW
			fTurnAmt[YAW] = rider_ps->viewangles[YAW] * 0.065f;
			fTurnAmt[YAW] *= fTurnAmt[YAW];
			// Dampen the turn rate based on the current height.
			if ( rider_ps->viewangles[YAW] < 0 )
			{//must keep it negative because squaring a negative makes it positive
				fTurnAmt[YAW] = -fTurnAmt[YAW];
			}
			fTurnAmt[YAW] *= p_veh->m_LandTrace.fraction;
			//ROLL
			fTurnAmt[2] = 0.0f;
			*/

			//Actal YAW
			/*
			p_veh->m_vOrientation[ROLL] = curRoll;
			FighterRollAdjust(p_veh, rider_ps, parent_ps);
			curRoll = p_veh->m_vOrientation[ROLL];
			*/
			FighterYawAdjust(p_veh, rider_ps, parent_ps);

			// If we are not hitting the ground, allow the fighter to pitch up and down.
			if (!FighterOverValidLandingSurface(p_veh)
				|| parent_ps->speed > MIN_LANDING_SPEED)
				//if ( ( p_veh->m_LandTrace.fraction >= 1.0f || p_veh->m_ucmd.forwardmove != 0 ) && p_veh->m_LandTrace.fraction >= 0.0f )
			{
				float fYawDelta;

				FighterPitchAdjust(p_veh, rider_ps, parent_ps);
#ifdef VEH_CONTROL_SCHEME_4
				FighterPitchClamp(p_veh, rider_ps, parent_ps, cur_time);
#endif// VEH_CONTROL_SCHEME_4

				FighterNoseMalfunctionCheck(p_veh, parent_ps);

				// Adjust the roll based on the turn amount and dampen it a little.
				fYawDelta = AngleSubtract(p_veh->m_vOrientation[YAW], p_veh->m_vPrevOrientation[YAW]); //p_veh->m_vOrientation[YAW] - p_veh->m_vPrevOrientation[YAW];
				if (fYawDelta > 8.0f)
				{
					fYawDelta = 8.0f;
				}
				else if (fYawDelta < -8.0f)
				{
					fYawDelta = -8.0f;
				}
				curRoll -= fYawDelta;
				curRoll = PredictedAngularDecrement(0.93f, angleTimeMod * 2.0f, curRoll);

				//cap it reasonably
				//NOTE: was hardcoded to 40.0f, now using extern data
#ifdef VEH_CONTROL_SCHEME_4
				if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
				{
					if (p_veh->m_pVehicleInfo->rollLimit != -1)
					{
						if (curRoll > p_veh->m_pVehicleInfo->rollLimit)
						{
							curRoll = p_veh->m_pVehicleInfo->rollLimit;
						}
						else if (curRoll < -p_veh->m_pVehicleInfo->rollLimit)
						{
							curRoll = -p_veh->m_pVehicleInfo->rollLimit;
						}
					}
				}
#else// VEH_CONTROL_SCHEME_4
				if (p_veh->m_pVehicleInfo->rollLimit != -1)
				{
					if (curRoll > p_veh->m_pVehicleInfo->rollLimit)
					{
						curRoll = p_veh->m_pVehicleInfo->rollLimit;
					}
					else if (curRoll < -p_veh->m_pVehicleInfo->rollLimit)
					{
						curRoll = -p_veh->m_pVehicleInfo->rollLimit;
					}
				}
#endif// VEH_CONTROL_SCHEME_4
			}
		}
	}

	// If you are directly impacting the ground, even out your pitch.
	if (isLandingOrLanded)
	{//only if capable of landing
		if (!isDead
			&& parent_ps->electrifyTime < cur_time
			&& (!p_veh->m_pVehicleInfo->surfDestruction || !p_veh->m_iRemovedSurfaces))
		{//not crashing or spiralling out of control...
			if (p_veh->m_vOrientation[PITCH] > 0)
			{
				p_veh->m_vOrientation[PITCH] = PredictedAngularDecrement(0.2f, angleTimeMod * 10.0f, p_veh->m_vOrientation[PITCH]);
			}
			else
			{
				p_veh->m_vOrientation[PITCH] = PredictedAngularDecrement(0.75f, angleTimeMod * 10.0f, p_veh->m_vOrientation[PITCH]);
			}
		}
	}

	/*
	//NOTE: all this is redundant now since we have the FighterDamageRoutine func...
		if ( isDead )
		{//going to explode
			//FIXME: maybe make it erratically jerk or spin or start and stop?
			if (1)
			{
				p_veh->m_ucmd.rightmove = Q_irand( -64, 64 );
			}
			else
			{
				p_veh->m_ucmd.rightmove = 0;
			}
			p_veh->m_ucmd.forwardmove = Q_irand( -32, 127 );
			p_veh->m_ucmd.upmove = Q_irand( -127, 127 );
			p_veh->m_vOrientation[YAW] += Q_flrand( -10, 10 );
			p_veh->m_vOrientation[PITCH] += p_veh->m_fTimeModifier;
			if ( p_veh->m_vOrientation[PITCH] > 60.0f )
			{
				p_veh->m_vOrientation[PITCH] = 60.0f;
			}
			if ( p_veh->m_LandTrace.fraction != 0.0f )
			{
				parent_ps->velocity[2] -= p_veh->m_pVehicleInfo->acceleration * p_veh->m_fTimeModifier;
			}
		}
	*/
	// If no one is in this vehicle and it's up in the sky, pitch it forward as it comes tumbling down.
#ifdef _GAME //never gonna happen on client anyway, we can't be getting predicted unless the predicting client is boarded
	if (!p_veh->m_pVehicleInfo->Inhabited(p_veh)
		&& p_veh->m_LandTrace.fraction >= groundFraction
		&& !FighterIsInSpace((gentity_t*)parent)
		&& !FighterSuspended(p_veh, parent_ps))
	{
		p_veh->m_ucmd.upmove = 0;
		//p_veh->m_ucmd.forwardmove = 0;
		p_veh->m_vOrientation[PITCH] += p_veh->m_fTimeModifier;
		if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
		{
			if (p_veh->m_vOrientation[PITCH] > 60.0f)
			{
				p_veh->m_vOrientation[PITCH] = 60.0f;
			}
		}
	}
#endif

	if (!parent_ps->hackingTime)
	{//use that roll
		p_veh->m_vOrientation[ROLL] = curRoll;
		//NOTE: this seems really backwards...
		if (p_veh->m_vOrientation[ROLL])
		{ //continually adjust the yaw based on the roll..
			if ((p_veh->m_iRemovedSurfaces || parent_ps->electrifyTime >= cur_time)//spiralling out of control
				&& (!(p_veh->m_pParentEntity->s.number % 4) || !(p_veh->m_pParentEntity->s.number % 5)))
			{//leave YAW alone
			}
			else
			{
				if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
				{
					p_veh->m_vOrientation[YAW] -= ((p_veh->m_vOrientation[ROLL]) * 0.05f) * p_veh->m_fTimeModifier;
				}
			}
		}
	}
	else
	{//add in strafing roll
		const float strafeRoll = (parent_ps->hackingTime / MAX_STRAFE_TIME) * p_veh->m_pVehicleInfo->rollLimit;//p_veh->m_pVehicleInfo->bankingSpeed*
		const float strafeDif = AngleSubtract(strafeRoll, p_veh->m_vOrientation[ROLL]);
		p_veh->m_vOrientation[ROLL] += (strafeDif * 0.1f) * p_veh->m_fTimeModifier;
		if (!BG_UnrestrainedPitchRoll(rider_ps, p_veh))
		{//cap it reasonably
			if (p_veh->m_pVehicleInfo->rollLimit != -1
				&& !p_veh->m_iRemovedSurfaces
				&& parent_ps->electrifyTime < cur_time)
			{
				if (p_veh->m_vOrientation[ROLL] > p_veh->m_pVehicleInfo->rollLimit)
				{
					p_veh->m_vOrientation[ROLL] = p_veh->m_pVehicleInfo->rollLimit;
				}
				else if (p_veh->m_vOrientation[ROLL] < -p_veh->m_pVehicleInfo->rollLimit)
				{
					p_veh->m_vOrientation[ROLL] = -p_veh->m_pVehicleInfo->rollLimit;
				}
			}
		}
	}

	if (p_veh->m_pVehicleInfo->surfDestruction)
	{
		FighterDamageRoutine(p_veh, parent, parent_ps, rider_ps, isDead);
	}
	p_veh->m_vOrientation[ROLL] = AngleNormalize180(p_veh->m_vOrientation[ROLL]);

	/********************************************************************************/
	/*	END	Here is where make sure the vehicle is properly oriented.	END			*/
	/********************************************************************************/
}

#ifdef _GAME //ONLY on server, not cgame

// This function makes sure that the vehicle is properly animated.
static void AnimateVehicle(Vehicle_t* p_veh)
{
	int Anim = -1;
	const int iFlags = SETANIM_FLAG_NORMAL;
	playerState_t* parent_ps = p_veh->m_pParentEntity->playerState;
	const int cur_time = level.time;

	if (parent_ps->hyperSpaceTime
		&& cur_time - parent_ps->hyperSpaceTime < HYPERSPACE_TIME)
	{//Going to Hyperspace
		//close the wings (FIXME: makes sense on X-Wing, not Shuttle?)
		if (p_veh->m_ulFlags & VEH_WINGSOPEN)
		{
			p_veh->m_ulFlags &= ~VEH_WINGSOPEN;
			Anim = BOTH_WINGS_CLOSE;
		}
	}
	else
	{
		const qboolean isLanding = FighterIsLanding(p_veh, parent_ps);
		const qboolean isLanded = FighterIsLanded(p_veh, parent_ps);

		// if we're above launch height (way up in the air)...
		if (!isLanding && !isLanded)
		{
			if (!(p_veh->m_ulFlags & VEH_WINGSOPEN))
			{
				p_veh->m_ulFlags |= VEH_WINGSOPEN;
				p_veh->m_ulFlags &= ~VEH_GEARSOPEN;
				Anim = BOTH_WINGS_OPEN;
			}
		}
		// otherwise we're below launch height and still taking off.
		else
		{
			if ((p_veh->m_ucmd.forwardmove < 0 || p_veh->m_ucmd.upmove < 0 || isLanded)
				&& p_veh->m_LandTrace.fraction <= 0.4f
				&& p_veh->m_LandTrace.plane.normal[2] >= MIN_LANDING_SLOPE)
			{//already landed or trying to land and close to ground
				// Open gears.
				if (!(p_veh->m_ulFlags & VEH_GEARSOPEN))
				{
					if (p_veh->m_pVehicleInfo->soundLand)
					{//just landed?
						G_EntitySound(((gentity_t*)(p_veh->m_pParentEntity)), CHAN_AUTO, p_veh->m_pVehicleInfo->soundLand);
					}
					p_veh->m_ulFlags |= VEH_GEARSOPEN;
					Anim = BOTH_GEARS_OPEN;
				}
			}
			else
			{//trying to take off and almost halfway off the ground
				// Close gears (if they're open).
				if (p_veh->m_ulFlags & VEH_GEARSOPEN)
				{
					p_veh->m_ulFlags &= ~VEH_GEARSOPEN;
					Anim = BOTH_GEARS_CLOSE;
					//iFlags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;
				}
				// If gears are closed, and we are below launch height, close the wings.
				else
				{
					if (p_veh->m_ulFlags & VEH_WINGSOPEN)
					{
						p_veh->m_ulFlags &= ~VEH_WINGSOPEN;
						Anim = BOTH_WINGS_CLOSE;
					}
				}
			}
		}
	}

	if (Anim != -1)
	{
		BG_SetAnim(p_veh->m_pParentEntity->playerState, bgAllAnims[p_veh->m_pParentEntity->localAnimIndex].anims, SETANIM_BOTH, Anim, iFlags);
	}
}

// This function makes sure that the rider's in this vehicle are properly animated.
static void AnimateRiders(Vehicle_t* p_veh)
{
}

#endif //game-only

#ifdef _CGAME
void AttachRidersGeneric(Vehicle_t* p_veh);
#endif

void G_SetFighterVehicleFunctions(vehicleInfo_t* pVehInfo)
{
#ifdef _GAME //ONLY in SP or on server, not cgame
	pVehInfo->AnimateVehicle = AnimateVehicle;
	pVehInfo->AnimateRiders = AnimateRiders;
	//	pVehInfo->ValidateBoard				=		ValidateBoard;
	//	pVehInfo->SetParent					=		SetParent;
	//	pVehInfo->SetPilot					=		SetPilot;
	//	pVehInfo->AddPassenger				=		AddPassenger;
	//	pVehInfo->Animate					=		Animate;
	pVehInfo->Board = Board;
	pVehInfo->Eject = Eject;
	//	pVehInfo->EjectAll					=		EjectAll;
	//	pVehInfo->StartDeathDelay			=		StartDeathDelay;
	//	pVehInfo->DeathUpdate				=		DeathUpdate;
	//	pVehInfo->RegisterAssets			=		RegisterAssets;
	//	pVehInfo->Initialize				=		Initialize;
	pVehInfo->Update = Update;
	//	pVehInfo->UpdateRider				=		UpdateRider;
#endif //game-only
	pVehInfo->ProcessMoveCommands = ProcessMoveCommands;
	pVehInfo->ProcessOrientCommands = ProcessOrientCommands;

#ifdef _CGAME //cgame prediction attachment func
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
void G_CreateFighterNPC(Vehicle_t** p_veh, const char* strType)
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
	(*p_veh)->m_pVehicleInfo = &g_vehicleInfo[BG_VehicleGetIndex(strType)];
}