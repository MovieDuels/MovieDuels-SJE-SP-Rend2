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
//	Heavy Repeater
//-------------------

//---------------------------------------------------------
static void WP_RepeaterMainFire(gentity_t* ent, vec3_t dir)
//---------------------------------------------------------
{
	vec3_t start;
	int damage = weaponData[WP_REPEATER].damage;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = create_missile(start, dir, REPEATER_VELOCITY, 10000, ent);

	missile->classname = "repeater_proj";
	missile->s.weapon = WP_REPEATER;

	// Do the damages
	if (ent->s.number != 0)
	{
		if (g_spskill->integer == 0)
		{
			damage = REPEATER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REPEATER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REPEATER_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_REPEATER;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
static void WP_RepeaterAltFire(gentity_t* ent)
//---------------------------------------------------------
{
	vec3_t start;
	int damage = weaponData[WP_REPEATER].altDamage;
	gentity_t* missile;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	if (ent->client && ent->client->NPC_class == CLASS_GALAKMECH)
	{
		missile = create_missile(start, ent->client->hiddenDir, ent->client->hiddenDist, 10000, ent, qtrue);
	}
	else
	{
		WP_MissileTargetHint(ent, start, forward_vec);
		missile = create_missile(start, forward_vec, REPEATER_ALT_VELOCITY, 10000, ent, qtrue);
	}

	missile->classname = "repeater_alt_proj";
	missile->s.weapon = WP_REPEATER;
	missile->mass = 10;

	if (ent->client && ent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_FIFTEEN)
	{
		NPC_SetAnim(ent, SETANIM_BOTH, BOTH_H1_S1_TR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}

	// Do the damages
	if (ent->s.number != 0)
	{
		if (g_spskill->integer == 0)
		{
			damage = REPEATER_ALT_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REPEATER_ALT_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REPEATER_ALT_NPC_DAMAGE_HARD;
		}
	}

	VectorSet(missile->maxs, REPEATER_ALT_SIZE, REPEATER_ALT_SIZE, REPEATER_ALT_SIZE);
	VectorScale(missile->maxs, -1, missile->mins);
	missile->s.pos.trType = TR_GRAVITY;
	missile->s.pos.trDelta[2] += 40.0f;

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_REPEATER_ALT;
	missile->splashMethodOfDeath = MOD_REPEATER_ALT;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = weaponData[WP_REPEATER].altSplashDamage;
	missile->splashRadius = weaponData[WP_REPEATER].altSplashRadius;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

extern qboolean WalkCheck(const gentity_t* self);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);

//---------------------------------------------------------
void WP_FireRepeater(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t angs;

	vectoangles(forward_vec, angs);

	if (alt_fire)
	{
		WP_RepeaterAltFire(ent);
	}
	else
	{
		vec3_t dir;
		if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
		{
			//no inherent aim screw up
		}
		else if (NPC_IsNotHavingEnoughForceSight(ent))
		{//force sight 2+ gives perfect aim
			if (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
			{
				if (PM_RunningAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL)
				{ // running or very fatigued
					angs[PITCH] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.5f, 1.5f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) || ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF)
				{//walking or fatigued a bit
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * WALKING_SPREAD;
				}
				else
				{// just standing
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REPEATER_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * REPEATER_SPREAD;
				}
			}
			else
			{// add some slop to the fire direction for NPC,s
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REPEATER_NPC_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * REPEATER_NPC_SPREAD;
			}
		}

		AngleVectors(angs, dir, nullptr, nullptr);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_RepeaterMainFire(ent, dir);
	}
}