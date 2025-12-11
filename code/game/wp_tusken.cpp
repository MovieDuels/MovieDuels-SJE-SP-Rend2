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

extern cvar_t* g_SerenityJediEngineMode;
extern qboolean WalkCheck(const gentity_t* self);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
//---------------------------------------------------------
void WP_FireTuskenRifle(gentity_t* ent)
//---------------------------------------------------------
{
	vec3_t start;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	if (ent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_FIFTEEN)
	{
		NPC_SetAnim(ent, SETANIM_BOTH, BOTH_H1_S1_TR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		//no inherent aim screw up
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent))
	{//force sight 2+ gives perfect aim
		vec3_t angs;

		vectoangles(forward_vec, angs);

		if (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
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

		AngleVectors(angs, forward_vec, nullptr, nullptr);
	}

	WP_MissileTargetHint(ent, start, forward_vec);

	gentity_t* missile = create_missile(start, forward_vec, TUSKEN_RIFLE_VEL, 10000, ent, qfalse);

	missile->classname = "trifle_proj";
	missile->s.weapon = WP_TUSKEN_RIFLE;

	if (ent->s.number < MAX_CLIENTS || g_spskill->integer == 2)
	{
		missile->damage = TUSKEN_RIFLE_DAMAGE_HARD;
	}
	else if (g_spskill->integer > 0)
	{
		missile->damage = TUSKEN_RIFLE_DAMAGE_MEDIUM;
	}
	else
	{
		missile->damage = TUSKEN_RIFLE_DAMAGE_EASY;
	}
	if (g_SerenityJediEngineMode->integer == 2)
	{
		missile->dflags = DAMAGE_DEATH_KNOCKBACK | DAMAGE_EXTRA_KNOCKBACK;
	}
	else
	{
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	}

	missile->methodOfDeath = MOD_BRYAR; //???

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}