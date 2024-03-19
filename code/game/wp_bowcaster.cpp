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

//-------------------
//	Wookiee Bowcaster
//-------------------

extern cvar_t* g_SerenityJediEngineMode;

extern qboolean WalkCheck(const gentity_t* self);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
//---------------------------------------------------------
static void WP_BowcasterMainFire(gentity_t* ent)
//---------------------------------------------------------
{
	int damage = weaponData[WP_BOWCASTER].damage;
	vec3_t angs, start;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	// Do the damages
	if (ent->s.number != 0)
	{
		if (g_spskill->integer == 0)
		{
			damage = BOWCASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = BOWCASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = BOWCASTER_NPC_DAMAGE_HARD;
		}
	}

	int count = (level.time - ent->client->ps.weaponChargeTime) / BOWCASTER_CHARGE_UNIT;

	if (count < 1)
	{
		count = 1;
	}
	else if (count > 5)
	{
		count = 5;
	}

	if (!(count & 1))
	{
		// if we aren't odd, knock us down a level
		count--;
	}

	WP_MissileTargetHint(ent, start, forward_vec);
	for (int i = 0; i < count; i++)
	{
		vec3_t dir;
		// create a range of different velocities
		const float vel = BOWCASTER_VELOCITY * (Q_flrand(-1.0f, 1.0f) * BOWCASTER_VEL_RANGE + 1.0f);

		vectoangles(forward_vec, angs);

		if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
		{
			//no inherent aim screw up
		}
		else if (NPC_IsNotHavingEnoughForceSight(ent))
		{//force sight 2+ gives perfect aim
			if (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
			{
				if (PM_CrouchAnim(ent->client->ps.legsAnim))
				{// firing position
					angs[PITCH] += Q_flrand(-0.0f, 0.0f);
					angs[YAW] += Q_flrand(-0.0f, 0.0f);
				}
				else
				{
					if (PM_RunningAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
					{ // running or very fatigued
						angs[PITCH] += Q_flrand(-2.0f, 2.0f) * RUNNING_SPREAD;
						angs[YAW] += Q_flrand(-2.0f, 2.0f) * RUNNING_SPREAD;
					}
					else if (PM_WalkingAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF)
					{//walking or fatigued a bit
						angs[PITCH] += Q_flrand(-1.1f, 1.1f) * WALKING_SPREAD;
						angs[YAW] += Q_flrand(-1.1f, 1.1f) * WALKING_SPREAD;
					}
					else
					{// just standing
						angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_MAIN_SPREAD;
						angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_MAIN_SPREAD;
					}
				}
			}
			else
			{// add some slop to the fire direction for NPC,s
				angs[PITCH] += Q_flrand(-0.0f, 0.5f);
				angs[YAW] += Q_flrand(-0.0f, 0.5f);
			}
		}

		AngleVectors(angs, dir, nullptr, nullptr);

		gentity_t* missile = create_missile(start, dir, vel, 10000, ent);

		missile->classname = "bowcaster_proj";
		missile->s.weapon = WP_BOWCASTER;

		VectorSet(missile->maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
		VectorScale(missile->maxs, -1, missile->mins);

		missile->damage = damage;

		if (g_SerenityJediEngineMode->integer == 2)
		{
			missile->dflags = DAMAGE_DEATH_KNOCKBACK | DAMAGE_EXTRA_KNOCKBACK;
		}
		else
		{
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
		}
		missile->methodOfDeath = MOD_BOWCASTER;
		missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
		missile->splashDamage = weaponData[WP_BOWCASTER].splashDamage;
		missile->splashRadius = weaponData[WP_BOWCASTER].splashRadius;

		// we don't want it to bounce
		missile->bounceCount = 0;
		ent->client->sess.missionStats.shotsFired++;
	}
}

//---------------------------------------------------------
static void WP_BowcasterAltFire(gentity_t* ent)
//---------------------------------------------------------
{
	vec3_t start;
	int damage = weaponData[WP_BOWCASTER].altDamage;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, forward_vec);

	gentity_t* missile = create_missile(start, forward_vec, BOWCASTER_VELOCITY, 10000, ent, qtrue);

	missile->classname = "bowcaster_alt_proj";
	missile->s.weapon = WP_BOWCASTER;

	// Do the damages
	if (ent->s.number != 0)
	{
		if (g_spskill->integer == 0)
		{
			damage = BOWCASTER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = BOWCASTER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = BOWCASTER_NPC_DAMAGE_HARD;
		}
	}

	VectorSet(missile->maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
	VectorScale(missile->maxs, -1, missile->mins);

	if (ent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_FIFTEEN)
	{
		NPC_SetAnim(ent, SETANIM_BOTH, BOTH_H1_S1_TR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}

	missile->s.eFlags |= EF_BOUNCE;
	missile->bounceCount = 3;

	missile->damage = damage;

	if (g_SerenityJediEngineMode->integer == 2)
	{
		missile->dflags = DAMAGE_DEATH_KNOCKBACK | DAMAGE_EXTRA_KNOCKBACK;
	}
	else
	{
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	}
	missile->methodOfDeath = MOD_BOWCASTER_ALT;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = weaponData[WP_BOWCASTER].altSplashDamage;
	missile->splashRadius = weaponData[WP_BOWCASTER].altSplashRadius;
}

//---------------------------------------------------------
void WP_FireBowcaster(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	if (alt_fire)
	{
		WP_BowcasterAltFire(ent);
	}
	else
	{
		WP_BowcasterMainFire(ent);
	}
}