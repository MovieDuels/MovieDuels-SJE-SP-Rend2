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
#include "wp_saber.h"
#include "w_local.h"
#include <qcommon/q_platform.h>
#include "bg_public.h"
#include <qcommon/q_math.h>
#include "weapons.h"
#include "teams.h"
#include <qcommon/q_shared.h>
#include "surfaceflags.h"

//---------------
//	Bryar Pistol
//---------------

extern qboolean WalkCheck(const gentity_t* self);
extern qboolean PM_CrouchAnim(const int anim);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean PM_RunningAnim(const int anim);
extern qboolean PM_WalkingAnim(const int anim);
//---------------------------------------------------------
void WP_FireBryarPistol(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot.
	vec3_t start;
	VectorCopy(muzzle, start);

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_BRYAR_PISTOL].damage
		: weaponData[WP_BRYAR_PISTOL].altDamage;

	// Make sure our start point isn't on the other side of a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent) == qtrue && ent->client != NULL)
	{
		// Force Sight 2+ gives perfect aim; below that we add spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			((ent->s.number < MAX_CLIENTS) || (G_ControlledByPlayer(ent) == qtrue))
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity alt-fire spread.
				if (PM_RunningAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY)
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity primary-fire spread.
				if (PM_RunningAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF)
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "bryar_proj";

	// Weapon identity: Bryar vs Blaster Pistol / Jawa.
	if (ent->s.weapon == WP_BLASTER_PISTOL || ent->s.weapon == WP_JAWA)
	{
		// *SIGH*... I hate our weapon system...
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_BRYAR_PISTOL;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;

		// Used in projectile rendering to make a beefier effect.
		missile->count = count;
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_BRYAR_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BRYAR;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// We don't want it to bounce forever.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point between the two pistols each time he fires.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireBryarPistolDuals(gentity_t* ent, const qboolean alt_fire, const qboolean second_pistol)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot (primary vs secondary pistol muzzle).
	vec3_t start;
	if (second_pistol == qtrue)
	{
		VectorCopy(muzzle2, start);
	}
	else
	{
		VectorCopy(muzzle, start);
	}

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_BLASTER_PISTOL].damage
		: weaponData[WP_BLASTER_PISTOL].altDamage;

	// Make sure our start point isn't on the other side of a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent) == qtrue && ent->client != NULL)
	{
		// Force Sight 2+ gives perfect aim; below that we add spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			((ent->s.number < MAX_CLIENTS) || (G_ControlledByPlayer(ent) == qtrue))
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity alt-fire spread.
				if (PM_RunningAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY)
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity primary-fire spread.
				if (PM_RunningAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF)
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "bryar_proj";

	// Weapon identity: Bryar vs Blaster Pistol / Jawa.
	if (ent->s.weapon == WP_BRYAR_PISTOL || ent->s.weapon == WP_JAWA)
	{
		// *SIGH*... I hate our weapon system...
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_BLASTER_PISTOL;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;

		// Used in projectile rendering to make a beefier effect.
		missile->count = count;
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_BRYAR_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BRYAR;
	}

	// Collision behaviour.
	missile->clipmask = MASK_SHOT;

	// We don't want it to bounce forever.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point between the two pistols each time he fires.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------
//	LPA NN-14
//---------------

//---------------------------------------------------------
void WP_FireReyPistol(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot.
	vec3_t start;
	VectorCopy(muzzle, start);

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_REY].damage
		: weaponData[WP_REY].altDamage;

	// Make sure our start point isn't on the other side of a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{
		// Force Sight 2+ gives perfect aim; below that we add spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			(ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent) == qtrue)
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity alt-fire spread.
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY))
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity primary-fire spread.
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF))
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "bryar_proj";

	// Weapon identity: Rey vs Blaster Pistol / Jawa.
	if (ent->s.weapon == WP_BLASTER_PISTOL || ent->s.weapon == WP_JAWA)
	{
		//*SIGH*... I hate our weapon system...
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_REY;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;

		// Used in projectile rendering to make a beefier effect.
		missile->count = count;
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_REY_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REY;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// We don't want it to bounce forever.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point between the two pistols each time he fires.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireReyPistolDuals(gentity_t* ent, const qboolean alt_fire, const qboolean second_pistol)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot (primary vs secondary pistol muzzle).
	vec3_t start;
	if (second_pistol == qtrue)
	{
		VectorCopy(muzzle2, start);
	}
	else
	{
		VectorCopy(muzzle, start);
	}

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_REY].damage
		: weaponData[WP_REY].altDamage;

	// Make sure our start point isn't on the other side of a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{
		// Force Sight 2+ gives perfect aim; below that we add spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			(ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent) == qtrue)
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity alt-fire spread.
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY))
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity primary-fire spread.
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF))
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "bryar_proj";

	// Weapon identity: Rey vs Blaster Pistol / Jawa.
	if (ent->s.weapon == WP_BLASTER_PISTOL || ent->s.weapon == WP_JAWA)
	{
		//*SIGH*... I hate our weapon system...
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_REY;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;

		// Used in projectile rendering to make a beefier effect.
		missile->count = count;
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_REY_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REY;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// We don't want it to bounce forever.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point between the two pistols each time he fires.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------
//	DC-17 Hand Pistol
//---------------

//---------------------------------------------------------
void WP_FireClonePistol(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot.
	vec3_t start;
	VectorCopy(muzzle, start);

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_CLONEPISTOL].damage
		: weaponData[WP_CLONEPISTOL].altDamage;

	// Make sure our start point isn't on the other side of a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent) == qtrue && ent->client != NULL)
	{
		// Force Sight 2+ gives perfect aim; below that we add spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			((ent->s.number < MAX_CLIENTS) || (G_ControlledByPlayer(ent) == qtrue))
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity alt-fire spread.
				if (PM_RunningAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY)
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				// Player / controlled entity primary-fire spread.
				if (PM_RunningAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF)
				{
					// Walking or somewhat fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "clone_proj";

	// Weapon identity: Clone pistol vs Blaster Pistol / Jawa.
	if (ent->s.weapon == WP_BLASTER_PISTOL || ent->s.weapon == WP_JAWA)
	{
		// *SIGH*... I hate our weapon system...
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_CLONEPISTOL;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;

		// Used in projectile rendering to make a beefier effect.
		missile->count = count;
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_CLONEPISTOL_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_CLONEPISTOL;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point between the two pistols each time he fires.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireClonePistolDuals(gentity_t* ent, const qboolean alt_fire, const qboolean second_pistol)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Determine muzzle origin (primary vs secondary pistol).
	vec3_t start;
	if (second_pistol == qtrue)
	{
		VectorCopy(muzzle2, start);
	}
	else
	{
		VectorCopy(muzzle, start);
	}

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_CLONEPISTOL].damage
		: weaponData[WP_CLONEPISTOL].altDamage;

	// Ensure muzzle isn't inside a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{
		// Force Sight < 2 → apply spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			(ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent) == qtrue)
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY))
				{
					// Walking or moderately fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF))
				{
					// Walking or moderately fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "clone_proj";

	// Weapon identity: Clone Pistol vs Blaster Pistol / Jawa.
	if (ent->s.weapon == WP_BLASTER_PISTOL || ent->s.weapon == WP_JAWA)
	{
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_CLONEPISTOL;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;
		missile->count = count; // Used in projectile rendering for beefier effect.
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_CLONEPISTOL_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_CLONEPISTOL;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// Prevent infinite bouncing.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point each shot.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireMandoClonePistolDuals(gentity_t* ent, const qboolean alt_fire, const qboolean second_pistol)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Determine muzzle origin (primary vs secondary pistol).
	vec3_t start;
	if (second_pistol == qtrue)
	{
		VectorCopy(muzzle2, start);
	}
	else
	{
		VectorCopy(muzzle, start);
	}

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_DUAL_CLONEPISTOL].damage
		: weaponData[WP_DUAL_CLONEPISTOL].altDamage;

	// Ensure muzzle isn't inside a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{
		// Force Sight < 2 → apply spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			(ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent) == qtrue)
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY))
				{
					// Walking or moderately fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF))
				{
					// Walking or moderately fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "clone_proj";

	// Weapon identity: Dual Clone Pistols vs Single Clone Pistol.
	if (ent->s.weapon == WP_CLONEPISTOL)
	{
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_DUAL_CLONEPISTOL;
	}

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;
		missile->count = count; // Used in projectile rendering for beefier effect.
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_CLONEPISTOL_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_CLONEPISTOL;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// Prevent infinite bouncing.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point each shot.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}

//---------------------------------------------------------
void WP_FireSBDPistol(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot.
	vec3_t start;
	VectorCopy(muzzle, start);

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_SBD_BLASTER].damage
		: weaponData[WP_SBD_BLASTER].altDamage;

	// Ensure muzzle isn't inside a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{
		// Force Sight < 2 → apply spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			(ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent) == qtrue)
			? qtrue
			: qfalse;

		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue)
			{
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY))
				{
					// Walking or moderately fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				}
			}
			else
			{
				// NPC alt-fire spread.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_ALT_SPREAD;
			}
		}
		else
		{
			if (is_player_or_controlled == qtrue)
			{
				if (ent->client != NULL &&
					(PM_RunningAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL))
				{
					// Running or very fatigued.
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (ent->client != NULL &&
					(PM_WalkingAnim(ent->client->ps.legsAnim) ||
						ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF))
				{
					// Walking or moderately fatigued.
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					// Standing still.
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
			else
			{
				// NPC primary-fire spread.
				angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "bryar_proj";
	missile->s.weapon = WP_SBD_BLASTER;

	// ---------------------------------------------------------------------
	// ALT-FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;
		missile->count = count; // Used in projectile rendering for beefier effect.
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_BRYAR_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BRYAR;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// Prevent infinite bouncing.
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireJawaPistol(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	// Validate caller.
	if (ent == NULL)
	{
		return;
	}

	// Starting point of the shot.
	vec3_t start;
	VectorCopy(muzzle, start);

	// Base damage (primary vs alt).
	int damage = (alt_fire == qfalse)
		? weaponData[WP_JAWA].damage
		: weaponData[WP_JAWA].altDamage;

	// Ensure muzzle isn't inside a wall.
	WP_TraceSetStart(ent, start);

	// ---------------------------------------------------------------------
	// AIM / SPREAD LOGIC
	// ---------------------------------------------------------------------
	if (ent->client != NULL && ent->client->NPC_class == CLASS_VEHICLE)
	{
		// Vehicles: no inherent aim screw up.
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent) == qtrue && ent->client != NULL)
	{
		// Force Sight < 2 → apply spread.
		vec3_t angs;
		vectoangles(forward_vec, angs);

		const qboolean is_player_or_controlled =
			((ent->s.number < MAX_CLIENTS) || (G_ControlledByPlayer(ent) == qtrue))
			? qtrue
			: qfalse;

		// -------------------------
		// ALT‑FIRE SPREAD
		// -------------------------
		if (alt_fire == qtrue)
		{
			if (is_player_or_controlled == qtrue && WalkCheck(ent) == qfalse)
			{
				// Running → very inaccurate.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_ALT_SPREAD;
			}
			else if (ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
			{
				// Very fatigued.
				angs[PITCH] += Q_flrand(-2.0f, 2.0f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-2.0f, 2.0f) * BLASTER_ALT_SPREAD;
			}
			else if (ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY)
			{
				// Moderately fatigued.
				angs[PITCH] += Q_flrand(-1.5f, 1.5f) * BLASTER_ALT_SPREAD;
				angs[YAW] += Q_flrand(-1.5f, 1.5f) * BLASTER_ALT_SPREAD;
			}
			else if (PM_CrouchAnim(ent->client->ps.legsAnim))
			{
				// Crouching → perfect aim (no spread).
			}
			else
			{
				// Standing → perfect aim (no spread).
			}
		}
		// -------------------------
		// PRIMARY‑FIRE SPREAD
		// -------------------------
		else
		{
			// NPC accuracy tier logic.
			if (ent->NPC != NULL && ent->NPC->currentAim < 5)
			{
				if (ent->client != NULL &&
					(ent->client->NPC_class == CLASS_STORMTROOPER ||
						ent->client->NPC_class == CLASS_CLONETROOPER ||
						ent->client->NPC_class == CLASS_STORMCOMMANDO ||
						ent->client->NPC_class == CLASS_SWAMPTROOPER ||
						ent->client->NPC_class == CLASS_DROIDEKA ||
						ent->client->NPC_class == CLASS_SBD ||
						ent->client->NPC_class == CLASS_IMPWORKER ||
						ent->client->NPC_class == CLASS_REBEL ||
						ent->client->NPC_class == CLASS_WOOKIE ||
						ent->client->NPC_class == CLASS_BATTLEDROID))
				{
					const float npc_spread = BLASTER_NPC_SPREAD + (1.0f - ent->NPC->currentAim) * 0.25f;

					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * npc_spread;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * npc_spread;
				}
			}
			else if (is_player_or_controlled == qtrue && WalkCheck(ent) == qfalse)
			{
				// Running → very inaccurate.
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONERIFLE_MAIN_SPREAD;
			}
			else if (ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
			{
				// Very fatigued.
				angs[PITCH] += Q_flrand(-2.0f, 2.0f) * BLASTER_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-2.0f, 2.0f) * BLASTER_MAIN_SPREAD;
			}
			else if (ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HEAVY)
			{
				// Moderately fatigued.
				angs[PITCH] += Q_flrand(-1.5f, 1.5f) * BLASTER_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.5f, 1.5f) * BLASTER_MAIN_SPREAD;
			}
			else if (PM_CrouchAnim(ent->client->ps.legsAnim))
			{
				// Crouching → perfect aim.
			}
			else
			{
				// Standing → perfect aim.
			}
		}

		AngleVectors(angs, forward_vec, NULL, NULL);
	}

	// Provide target hinting for homing / assist logic.
	WP_MissileTargetHint(ent, start, forward_vec);

	// ---------------------------------------------------------------------
	// MISSILE CREATION
	// ---------------------------------------------------------------------
	gentity_t* missile = create_missile(start, forward_vec, BRYAR_PISTOL_VEL, 10000, ent, alt_fire);
	if (missile == NULL)
	{
		return;
	}

	missile->classname = "bryar_proj";

	// Weapon identity: Jawa vs Blaster Pistol.
	if (ent->s.weapon == WP_BLASTER_PISTOL)
	{
		missile->s.weapon = ent->s.weapon;
	}
	else
	{
		missile->s.weapon = WP_JAWA;
	}

	// ---------------------------------------------------------------------
	// ALT‑FIRE CHARGE LOGIC
	// ---------------------------------------------------------------------
	if (alt_fire == qtrue && ent->client != NULL)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		damage *= count;
		missile->count = count; // Used in projectile rendering for beefier effect.
	}

	// ---------------------------------------------------------------------
	// DAMAGE / DEATH SETUP
	// ---------------------------------------------------------------------
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (alt_fire == qtrue)
	{
		missile->methodOfDeath = MOD_BRYAR_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BRYAR;
	}

	// Collision and bounce behaviour.
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// Prevent infinite bouncing.
	missile->bounceCount = 8;

	// Dual pistols: toggle muzzle point each shot.
	if (ent->weaponModel[1] > 0)
	{
		ent->count = (ent->count != 0) ? 0 : 1;
	}
}
