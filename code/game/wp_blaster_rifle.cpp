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
#include "b_local.h"
#include "w_local.h"
#include "bg_public.h"
#include "surfaceflags.h"
#include "teams.h"
#include "weapons.h"
#include <qcommon\q_shared.h>
#include <qcommon\q_math.h>
#include <qcommon\q_platform.h>
//---------------
//	Blaster
//---------------

extern cvar_t* g_SerenityJediEngineMode;
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean NPC_IsMando(const gentity_t* self);
//---------------------------------------------------------
void WP_FireBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = BLASTER_VELOCITY;
	int damage = alt_fire ? weaponData[WP_BLASTER].altDamage : weaponData[WP_BLASTER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= BLASTER_NPC_VEL_CUT;
			}
			else
			{
				velocity *= BLASTER_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BLASTER;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = BLASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = BLASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				damage = SJE_BLASTER_NPC_DAMAGE_HARD;
			}
			else
			{
				damage = BLASTER_NPC_DAMAGE_HARD;
			}
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

extern qboolean WalkCheck(const gentity_t* self);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern qboolean WalkCheck(const gentity_t* self);

//---------------------------------------------------------
void WP_FireBlaster(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Start from perfect forward aim
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly
	if (!(ent->client && ent->client->NPC_class == CLASS_VEHICLE))
	{
		// Force Sight 2+ gives perfect aim
		if (NPC_IsNotHavingEnoughForceSight(ent))
		{
			const int isPlayer = (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent));
			const int mishap = ent->client ? ent->client->ps.BlasterAttackChainCount : 0;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// Determine movement state
			const int running = (ent->client && PM_RunningAnim(ent->client->ps.legsAnim));
			const int walking = (ent->client && PM_WalkingAnim(ent->client->ps.legsAnim));

			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // main fire
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);
		}
	}

	// Convert back to direction
	AngleVectors(angs, dir, nullptr, nullptr);

	// Fire projectile
	WP_FireBlasterMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	E-5 Carbine
//---------------

//---------------------------------------------------------
void WP_FireBattleDroidMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = BLASTER_VELOCITY;
	int damage = alt_fire ? weaponData[WP_BATTLEDROID].altDamage : weaponData[WP_BATTLEDROID].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= BLASTER_NPC_VEL_CUT;
			}
			else
			{
				velocity *= BLASTER_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BATTLEDROID;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = BLASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = BLASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				damage = SJE_BLASTER_NPC_DAMAGE_HARD;
			}
			else
			{
				damage = BLASTER_NPC_DAMAGE_HARD;
			}
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireBattleDroid(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t dir, angs;

	vectoangles(forward_vec, angs);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		//no inherent aim screw up
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{//force sight 2+ gives perfect aim
		if (alt_fire)
		{
			if (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
			{
				if (PM_RunningAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{ // running or very fatigued
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY)
				{//walking or fatigued a bit
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{// just standing
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{// add some slop to the alt-fire direction for NPC,s
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (ent->client && ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
			{
				if (PM_RunningAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{ // running or very fatigued
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF)
				{//walking or fatigued a bit
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{// just standing
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{// add some slop to the fire direction for NPC,s
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, nullptr, nullptr);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireBattleDroidMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	F-11D
//---------------

//---------------------------------------------------------
void WP_FireFirstOrderMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = BLASTER_VELOCITY;
	int damage = alt_fire ? weaponData[WP_THEFIRSTORDER].altDamage : weaponData[WP_THEFIRSTORDER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= BLASTER_NPC_VEL_CUT;
			}
			else
			{
				velocity *= BLASTER_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_THEFIRSTORDER;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = BLASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = BLASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				damage = SJE_BLASTER_NPC_DAMAGE_HARD;
			}
			else
			{
				damage = BLASTER_NPC_DAMAGE_HARD;
			}
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireFirstOrder(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire the First Order blaster projectile
	WP_FireFirstOrderMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	DH-17
//---------------

//---------------------------------------------------------
void WP_FireRebelBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = REBELBLASTER_VELOCITY;
	int damage = alt_fire ? weaponData[WP_REBELBLASTER].altDamage : weaponData[WP_REBELBLASTER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= REBELBLASTER_NPC_VEL_CUT;
			}
			else
			{
				velocity *= REBELBLASTER_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_REBELBLASTER;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = REBELBLASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REBELBLASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REBELBLASTER_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_REBELBLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REBELBLASTER;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	if (ent && ent->weaponModel[1] > 0)
	{
		//dual pistols, toggle the muzzle point back and forth between the two pistols each time he fires
		ent->count = ent->count ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireRebelBlaster(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire the Rebel Blaster projectile
	WP_FireRebelBlasterMissile(ent, muzzle, dir, alt_fire);
}

//---------------------------------------------------------
void WP_FireRebelBlasterDuals(gentity_t* ent, qboolean alt_fire, qboolean second_pistol)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire from the correct muzzle (left or right pistol)
	if (second_pistol)
	{
		WP_FireRebelBlasterMissile(ent, muzzle2, dir, alt_fire);
	}
	else
	{
		WP_FireRebelBlasterMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	A280
//---------------

//---------------------------------------------------------
void WP_FireRebelRifleMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = REBELRIFLE_VELOCITY;
	int damage = alt_fire ? weaponData[WP_REBELRIFLE].altDamage : weaponData[WP_REBELRIFLE].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= REBELRIFLE_NPC_VEL_CUT;
			}
			else
			{
				velocity *= REBELRIFLE_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_REBELRIFLE;

	// Do the damages
	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = REBELRIFLE_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REBELRIFLE_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REBELRIFLE_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_REBELRIFLE_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REBELRIFLE;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireRebelRifle(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire the Rebel Rifle projectile
	WP_FireRebelRifleMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	Westar 34
//---------------

//---------------------------------------------------------
void WP_FireJangoPistolMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = JANGO_VELOCITY;
	int damage = alt_fire ? weaponData[WP_JANGO].altDamage : weaponData[WP_JANGO].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= JANGO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= JANGO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_JANGO;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = JANGO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = JANGO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = JANGO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_JANGO_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_JANGO;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	if (ent && ent->weaponModel[1] > 0)
	{
		//dual pistols, toggle the muzzle point back and forth between the two pistols each time he fires
		ent->count = ent->count ? 0 : 1;
	}
}

//---------------------------------------------------------
static void WP_FireJangoWristMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = CLONECOMMANDO_VELOCITY;
	int damage = alt_fire ? weaponData[WP_WRIST_BLASTER].altDamage : weaponData[WP_WRIST_BLASTER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONECOMMANDO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONECOMMANDO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "clone_proj";
	missile->s.weapon = WP_WRIST_BLASTER;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_CLONECOMMANDO_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_CLONECOMMANDO;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireJangoDualPistolMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = JANGO_VELOCITY;
	int damage = alt_fire ? weaponData[WP_DUAL_PISTOL].altDamage : weaponData[WP_DUAL_PISTOL].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= JANGO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= JANGO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";

	missile->s.weapon = WP_DUAL_PISTOL;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = JANGO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = JANGO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = JANGO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_JANGO_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_JANGO;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	if (ent && ent->weaponModel[1] > 0)
	{
		//dual pistols, toggle the muzzle point back and forth between the two pistols each time he fires
		ent->count = ent->count ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireJangoDualPistolMissileDuals(gentity_t* ent, vec3_t start, vec3_t dir, qboolean alt_fire)
{
	int velocity = JANGO_VELOCITY;
	int damage = alt_fire ? weaponData[WP_DUAL_PISTOL].altDamage
		: weaponData[WP_DUAL_PISTOL].damage;

	// Vehicle scaling: AT-ST style velocity + triple damage
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (isVehicle)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// NPC fairness rule: slower bolts so the player can dodge
		const qboolean isNPC =
			(ent && ent->client && ent->s.number >= MAX_CLIENTS) ? qtrue : qfalse;

		const qboolean controlled =
			(ent && G_ControlledByPlayer(ent)) ? qtrue : qfalse;

		const qboolean isMando =
			(ent && ent->client && NPC_IsMando(ent)) ? qtrue : qfalse;

		if (isNPC && !controlled && !isMando)
		{
			if (g_spskill->integer < 2)
			{
				velocity *= JANGO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= JANGO_NPC_HARD_VEL_CUT;
			}
		}
	}

	// Ensure muzzle isn't inside geometry
	WP_TraceSetStart(ent, start);

	// Slight auto-aim hinting
	WP_MissileTargetHint(ent, start, dir);

	// Spawn projectile
	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_DUAL_PISTOL;

	// NPC damage scaling (mirrors velocity scaling logic)
	const qboolean isNPC2 =
		(ent && ent->client && ent->s.number >= MAX_CLIENTS) ? qtrue : qfalse;

	const qboolean controlled2 =
		(ent && G_ControlledByPlayer(ent)) ? qtrue : qfalse;

	const qboolean isMando2 =
		(ent && ent->client && NPC_IsMando(ent)) ? qtrue : qfalse;

	if (isNPC2 && !controlled2 && !isMando2)
	{
		if (g_spskill->integer == 0)
		{
			damage = JANGO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = JANGO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = JANGO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = alt_fire ? MOD_JANGO_ALT : MOD_JANGO;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// Prevent infinite bouncing
	missile->bounceCount = 8;

	// Dual pistols: alternate muzzle index (0/1)
	if (ent && ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count ? 0 : 1);
	}
}

//---------------------------------------------------------
void WP_FireJangoPistol(gentity_t* ent, qboolean alt_fire, qboolean second_pistol)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);

			// Recompute forward_vec from modified angles
			AngleVectors(angs, forward_vec, NULL, NULL);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire from the correct muzzle (left or right pistol)
	if (second_pistol)
	{
		WP_FireJangoPistolMissile(ent, muzzle2, dir, alt_fire);
	}
	else
	{
		WP_FireJangoPistolMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------------------------------------------------
void WP_FireWristPistol(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);

			// Recompute forward_vec from modified angles
			AngleVectors(angs, forward_vec, NULL, NULL);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire the wrist-mounted projectile
	WP_FireJangoWristMissile(ent, muzzle, dir, alt_fire);
}

//---------------------------------------------------------
void WP_FireJangoDualPistol(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);

			// Recompute forward_vec from modified angles
			AngleVectors(angs, forward_vec, NULL, NULL);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire the projectile from the main muzzle
	WP_FireJangoDualPistolMissile(ent, muzzle, dir, alt_fire);
}

//---------------------------------------------------------
void WP_FireJangoFPPistolDuals(gentity_t* ent, qboolean alt_fire, qboolean second_pistol)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);

			// Recompute forward_vec from modified angles
			AngleVectors(angs, forward_vec, NULL, NULL);
		}
	}

	// Convert final angles into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire from the correct muzzle (left or right pistol)
	if (second_pistol)
	{
		WP_FireJangoDualPistolMissileDuals(ent, muzzle2, dir, alt_fire);
	}
	else
	{
		WP_FireJangoDualPistolMissileDuals(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	EE-3 Carbine Rifle
//---------------

//---------------------------------------------------------
void WP_FireBobaRifleMissile(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = BOBA_VELOCITY;
	int damage = alt_fire ? weaponData[WP_BOBA].altDamage : weaponData[WP_BOBA].damage;

	if (alt_fire)
	{
		velocity = Q_irand(1500, 3000);
	}

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= BOBA_NPC_VEL_CUT;
			}
			else
			{
				velocity *= BOBA_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BOBA;

	// Do the damages
	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = BOBA_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = BOBA_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = BOBA_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	missile->methodOfDeath = MOD_BOBA;

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireBobaRifle(gentity_t* ent, qboolean alt_fire)
{
	vec3_t angs, dir;

	// Convert perfect forward aim into Euler angles
	vectoangles(forward_vec, angs);

	// Vehicles always fire perfectly (no spread)
	const qboolean isVehicle =
		(ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;

	if (!isVehicle)
	{
		// Force Sight level < 2 → apply spread
		const qboolean applySpread =
			NPC_IsNotHavingEnoughForceSight(ent) ? qtrue : qfalse;

		if (applySpread)
		{
			// Player if human-controlled or client index < MAX_CLIENTS
			const qboolean isPlayer =
				(ent && (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))) ? qtrue : qfalse;

			// Mishap level (0 if no client)
			const int mishap =
				(ent && ent->client) ? ent->client->ps.BlasterAttackChainCount : 0;

			// Movement state
			const qboolean running =
				(ent && ent->client && PM_RunningAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			const qboolean walking =
				(ent && ent->client && PM_WalkingAnim(ent->client->ps.legsAnim)) ? qtrue : qfalse;

			float pitchSpread = 0.0f;
			float yawSpread = 0.0f;

			// ALT-FIRE spread rules
			if (alt_fire)
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.5f;
						yawSpread = RUNNING_SPREAD * 1.5f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HEAVY)
					{
						pitchSpread = WALKING_SPREAD * 1.2f;
						yawSpread = WALKING_SPREAD * 1.2f;
					}
					else
					{
						pitchSpread = BLASTER_ALT_SPREAD;
						yawSpread = BLASTER_ALT_SPREAD;
					}
				}
				else
				{
					// NPC alt-fire spread
					pitchSpread = BLASTER_ALT_SPREAD;
					yawSpread = BLASTER_ALT_SPREAD;
				}
			}
			else // MAIN-FIRE spread rules
			{
				if (isPlayer)
				{
					if (running || mishap >= BLASTERMISHAPLEVEL_FULL)
					{
						pitchSpread = RUNNING_SPREAD * 1.2f;
						yawSpread = RUNNING_SPREAD * 1.2f;
					}
					else if (walking || mishap >= BLASTERMISHAPLEVEL_HALF)
					{
						pitchSpread = WALKING_SPREAD;
						yawSpread = WALKING_SPREAD;
					}
					else
					{
						pitchSpread = BLASTER_MAIN_SPREAD * 0.5f;
						yawSpread = BLASTER_MAIN_SPREAD * 0.5f;
					}
				}
				else
				{
					// NPC main-fire spread
					pitchSpread = BLASTER_NPC_SPREAD * 0.5f;
					yawSpread = BLASTER_NPC_SPREAD * 0.5f;
				}
			}

			// Apply randomised spread to aim angles
			angs[PITCH] += Q_flrand(-pitchSpread, pitchSpread);
			angs[YAW] += Q_flrand(-yawSpread, yawSpread);
		}
	}

	// Convert modified angles back into a direction vector
	AngleVectors(angs, dir, NULL, NULL);

	// Fire the actual projectile
	WP_FireBobaRifleMissile(ent, muzzle, dir, alt_fire);
}

//////// DROIDEKA ////////

//---------------------------------------------------------
static void WP_FireDroidekaDualPistolMissileDuals(gentity_t* ent, vec3_t start, vec3_t dir, const qboolean alt_fire)
//---------------------------------------------------------
{
	int velocity = BLASTER_VELOCITY;
	int damage = alt_fire ? weaponData[WP_DROIDEKA].altDamage : weaponData[WP_DROIDEKA].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
		{
			if (g_spskill->integer < 2)
			{
				velocity *= JANGO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= JANGO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";

	missile->s.weapon = WP_DROIDEKA;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent && ent->client && ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) && !NPC_IsMando(ent)) //not controlled by player and not a mando of any kind
	{
		if (g_spskill->integer == 0)
		{
			damage = JANGO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = JANGO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = JANGO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire)
	{
		missile->methodOfDeath = MOD_JANGO_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_JANGO;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
	// alternate muzzles
	if (ent && ent->client)
	{
		ent->fxID ^= 1;
	}

	if (ent && ent->weaponModel[1] > 0)
	{
		//dual pistols, toggle the muzzle point back and forth between the two pistols each time he fires
		ent->count = ent->count ? 0 : 1;
	}
}

//---------------------------------------------------------
static void WP_FireDroidekaDualPistolMissile(gentity_t* ent, vec3_t start, vec3_t dir, qboolean alt_fire)
{
	int velocity = BLASTER_VELOCITY;
	int damage = alt_fire ? weaponData[WP_DROIDEKA].altDamage
		: weaponData[WP_DROIDEKA].damage;

	const qboolean isNPC = (ent && ent->client && ent->s.number >= MAX_CLIENTS) ? qtrue : qfalse;
	const qboolean isPlayer = (ent && ent->client && ent->s.number < MAX_CLIENTS) ? qtrue : qfalse;
	const qboolean isVehicle = (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE) ? qtrue : qfalse;
	const qboolean isMando = (ent && ent->client && NPC_IsMando(ent)) ? qtrue : qfalse;
	const qboolean controlled = (ent && G_ControlledByPlayer(ent)) ? qtrue : qfalse;

	// Vehicle scaling
	if (isVehicle)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// NPC fairness scaling (non-player, non-Mando)
		if (isNPC && !controlled && !isMando)
		{
			if (g_spskill->integer < 2)
				velocity *= JANGO_NPC_VEL_CUT;
			else
				velocity *= JANGO_NPC_HARD_VEL_CUT;
		}
	}

	// Ensure muzzle is not inside geometry
	WP_TraceSetStart(ent, start);

	// Slight auto-aim hinting
	WP_MissileTargetHint(ent, start, dir);

	// Spawn projectile
	gentity_t* missile = create_missile(start, dir, velocity, 10000, ent, alt_fire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_DROIDEKA;

	// NPC damage scaling (non-player, non-Mando)
	if (isNPC && !controlled && !isMando)
	{
		switch (g_spskill->integer)
		{
		case 0: damage = JANGO_NPC_DAMAGE_EASY;   break;
		case 1: damage = JANGO_NPC_DAMAGE_NORMAL; break;
		default: damage = JANGO_NPC_DAMAGE_HARD;  break;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = alt_fire ? MOD_JANGO_ALT : MOD_JANGO;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->bounceCount = 8;

	// Alternate FX (left/right)
	if (ent && ent->client)
	{
		ent->fxID ^= 1;
	}

	// Dual pistols: alternate muzzle index
	if (ent && ent->weaponModel[1] > 0)
		ent->count = ent->count ? 0 : 1;
}

//---------------------------------------------------------
void WP_FireDroidekaDualPistol(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t dir, angs;

	vectoangles(forward_vec, angs);

	if (ent->client && ent->client->ps.BlasterAttackChainCount <= BLASTERMISHAPLEVEL_HEAVYER)
	{
		//no inherent aim screw up
	}
	else
	{//force sight 2+ gives perfect aim
		vectoangles(forward_vec, angs);

		if (alt_fire)
		{
			// add some slop to the alt-fire direction for NPC,s
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
		}
		else
		{
			// add some slop to the fire direction for NPC,s
			angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
		}

		AngleVectors(angs, forward_vec, nullptr, nullptr);
	}

	AngleVectors(angs, dir, nullptr, nullptr);

	WP_FireDroidekaDualPistolMissile(ent, muzzle, dir, alt_fire);
}

//---------------------------------------------------------
void WP_FireDroidekaFPPistolDuals(gentity_t* ent, const qboolean alt_fire, const qboolean second_pistol)
//---------------------------------------------------------
{
	vec3_t dir, angs;

	vectoangles(forward_vec, angs);

	if (ent->client && ent->client->ps.BlasterAttackChainCount <= BLASTERMISHAPLEVEL_HEAVYER)
	{
		//no inherent aim screw up
	}
	else
	{//force sight 2+ gives perfect aim
		vectoangles(forward_vec, angs);

		if (alt_fire)
		{
			// add some slop to the alt-fire direction for NPC,s
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
		}
		else
		{
			// add some slop to the fire direction for NPC,s
			angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
		}

		AngleVectors(angs, forward_vec, nullptr, nullptr);
	}

	AngleVectors(angs, dir, nullptr, nullptr);

	if (second_pistol)
	{
		WP_FireDroidekaDualPistolMissileDuals(ent, muzzle2, dir, alt_fire);
	}
	else
	{
		WP_FireDroidekaDualPistolMissileDuals(ent, muzzle, dir, alt_fire);
	}
}