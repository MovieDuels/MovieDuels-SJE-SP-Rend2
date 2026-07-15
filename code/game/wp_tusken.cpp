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
extern qboolean PM_CrouchAnim(const int anim);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean WP_DoingForcedAnimationForForcePowers(const gentity_t* ent);
extern void G_MissileReflectEffect(const gentity_t* ent, vec3_t dir);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern int WP_SaberBlockCost(gentity_t* defender, const gentity_t* attacker, vec3_t hit_locs);
extern int WP_SaberMustDisruptorBlock(gentity_t* self, const gentity_t* atk, qboolean check_b_box_block, vec3_t point, int rSaberNum, int rBladeNum);
extern qboolean WP_SaberBlockBolt_MD(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
//---------------------------------------------------------
void WP_FireTuskenRifle(gentity_t* ent)
//---------------------------------------------------------
{
	// SAFETY: ent or ent->client may be NULL in edge cases
	if (ent == NULL || ent->client == NULL)
	{
		Com_Printf(S_COLOR_YELLOW "WP_FireTuskenRifle: NULL ent or ent->client\n");
		return;
	}
	vec3_t start;
	trace_t tr;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);

	int ignore = ent->s.number;
	int traces = 0;
	while (traces < 10)
	{
		constexpr float shot_range = 8192;
		vec3_t end;
		VectorMA(start, shot_range, forward_vec, end);
		//need to loop this in case we hit a Jedi who dodges the shot
		gi.trace(&tr, start, nullptr, nullptr, end, ignore, MASK_SHOT, G2_RETURNONHIT, 0);

		gentity_t* traceEnt = &g_entities[tr.entityNum];

		if (traceEnt)
		{
			//players can block or dodge disruptor shots.
			if (WP_SaberMustDisruptorBlock(traceEnt, ent, qfalse, tr.endpos, -1, -1) && !
				WP_DoingForcedAnimationForForcePowers(traceEnt))
			{
				G_MissileReflectEffect(traceEnt, tr.plane.normal);
				WP_ForcePowerDrain(traceEnt, FP_SABER_DEFENSE, WP_SaberBlockCost(traceEnt, ent, tr.endpos));
				//force player into a projective block move.
				WP_SaberBlockBolt_MD(traceEnt, tr.endpos, qtrue);

				VectorCopy(tr.endpos, start);
				ignore = tr.entityNum;
				traces++;
				continue;
			}
			if (jedi_disruptor_dodge_evasion(traceEnt, ent, &tr, HL_NONE))
			{
				//act like we didn't even hit him
				VectorCopy(tr.endpos, start);
				ignore = tr.entityNum;
				traces++;
				continue;
			}
		}
		//a Jedi is not dodging this shot
		break;
	}
	//make sure our start point isn't on the other side of a wall

	if (ent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_FIFTEEN)
	{
		NPC_SetAnim(ent, SETANIM_BOTH, BOTH_H1_S1_TR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		//no inherent aim screw up
	}
	else if (NPC_IsNotHavingEnoughForceSight(ent) == qtrue)
	{//force sight 2+ gives perfect aim
		vec3_t angs;

		vectoangles(forward_vec, angs); const qboolean is_player_or_controlled = ((ent->s.number < MAX_CLIENTS) || (G_ControlledByPlayer(ent) == qtrue)) ? qtrue : qfalse;

		if (is_player_or_controlled == qtrue)
		{
			if (PM_CrouchAnim(ent->client->ps.legsAnim) == qtrue || g_entities[ent->s.number].client->IsAiming == qtrue)
			{
				angs[PITCH] += Q_flrand(-0.0f, 0.0f);
				angs[YAW] += Q_flrand(-0.0f, 0.0f);
			}
			else
			{
				if (PM_RunningAnim(ent->client->ps.legsAnim) == qtrue ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FULL && g_entities[ent->s.number].client->IsAiming == qfalse)
				{
					angs[PITCH] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
					angs[YAW] += Q_flrand(-1.2f, 1.2f) * RUNNING_SPREAD;
				}
				else if (PM_WalkingAnim(ent->client->ps.legsAnim) == qtrue ||
					ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_HALF && g_entities[ent->s.number].client->IsAiming == qfalse)
				{
					angs[PITCH] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
					angs[YAW] += Q_flrand(-1.0f, 1.0f) * WALKING_SPREAD;
				}
				else
				{
					angs[PITCH] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
					angs[YAW] += Q_flrand(-0.5f, 0.5f) * BLASTER_MAIN_SPREAD;
				}
			}
		}
		else
		{// add some slop to the fire direction for NPC,s
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BLASTER_NPC_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BLASTER_NPC_SPREAD;
		}

		AngleVectors(angs, forward_vec, nullptr, nullptr);
	}

	WP_MissileTargetHint(ent, start, forward_vec);

	gentity_t* missile = CreateMissile(start, forward_vec, TUSKEN_RIFLE_VEL, 10000, ent, qfalse);

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

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	missile->methodOfDeath = MOD_BRYAR; //???

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}