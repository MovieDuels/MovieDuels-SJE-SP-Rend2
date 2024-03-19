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
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"
#include "../cgame/cg_local.h"

extern qboolean WP_DoingForcedAnimationForForcePowers(const gentity_t* ent);
extern int WP_SaberMustDisruptorBlock(gentity_t* self, const gentity_t* atk, qboolean check_b_box_block, vec3_t point,
	int rSaberNum, int rBladeNum);
extern qboolean WP_SaberBlockBolt_MD(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern void G_MissileReflectEffect(const gentity_t* ent, vec3_t dir);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern int WP_SaberBlockCost(gentity_t* defender, const gentity_t* attacker, vec3_t hit_locs);
extern qboolean G_ControlledByPlayer(const gentity_t* self);

static void WP_FireConcussionAlt(gentity_t* ent)
{
	//a rail-gun-like beam
	int damage = weaponData[WP_CONCUSSION].altDamage;
	constexpr int traces = DISRUPTOR_ALT_TRACES;
	qboolean render_impact = qtrue;
	vec3_t start;
	vec3_t muzzle2, spot, dir;
	trace_t tr;
	gentity_t* tent;
	qboolean hit_dodged = qfalse;

	if (ent->s.number >= MAX_CLIENTS)
	{
		vec3_t angles;
		vectoangles(forward_vec, angles);
		angles[PITCH] += Q_flrand(-1.0f, 1.0f) * (CONC_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f); //was 0.5f
		angles[YAW] += Q_flrand(-1.0f, 1.0f) * (CONC_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f); //was 0.5f
		AngleVectors(angles, forward_vec, vright_vec, up);
	}

	//Shove us backwards for half a second
	VectorMA(ent->client->ps.velocity, -200, forward_vec, ent->client->ps.velocity);
	ent->client->ps.groundEntityNum = ENTITYNUM_NONE;

	if (ent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_FIFTEEN)
	{
		NPC_SetAnim(ent, SETANIM_BOTH, BOTH_H1_S1_TR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}
	else
	{
		NPC_SetAnim(ent, SETANIM_LEGS, BOTH_H1_S1_T_, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}

	if (ent->client->ps.pm_flags & PMF_DUCKED)
	{
		//hunkered down
		ent->client->ps.pm_time = 100;
	}
	else
	{
		ent->client->ps.pm_time = 250;
	}
	ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK | PMF_TIME_NOFRICTION;
	//FIXME: only if on ground?  So no "rocket jump"?  Or: (see next FIXME)
	//FIXME: instead, set a forced ucmd backmove instead of this sliding

	VectorCopy(muzzle, muzzle2); // making a backup copy

	// The trace start will originate at the eye so we can ensure that it hits the crosshair.
	if (ent->NPC)
	{
		switch (g_spskill->integer)
		{
		case 0:
			damage = CONC_ALT_NPC_DAMAGE_EASY;
			break;
		case 1:
			damage = CONC_ALT_NPC_DAMAGE_MEDIUM;
			break;
		case 2:
		default:
			damage = CONC_ALT_NPC_DAMAGE_HARD;
			break;
		}
	}
	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);

	int skip = ent->s.number;

	//Make it a little easier to hit guys at long range
	vec3_t shot_mins, shot_maxs;
	VectorSet(shot_mins, -1, -1, -1);
	VectorSet(shot_maxs, 1, 1, 1);

	for (int i = 0; i < traces; i++)
	{
		constexpr float shot_range = 8192;
		vec3_t end;
		VectorMA(start, shot_range, forward_vec, end);

		//NOTE: if you want to be able to hit guys in emplaced guns, use "G2_COLLIDE, 10" instead of "G2_RETURNONHIT, 0"
		//alternately, if you end up hitting an emplaced_gun that has a sitter, just redo this one trace with the "G2_COLLIDE, 10" to see if we it the sitter
		//gi.trace( &tr, start, NULL, NULL, end, skip, MASK_SHOT, G2_COLLIDE, 10 );//G2_RETURNONHIT, 0 );
		gi.trace(&tr, start, shot_mins, shot_maxs, end, skip, MASK_SHOT, G2_COLLIDE, 10); //G2_RETURNONHIT, 0 );

		if (tr.surfaceFlags & SURF_NOIMPACT)
		{
			render_impact = qfalse;
		}

		if (tr.entityNum == ent->s.number)
		{
			// should never happen, but basically we don't want to consider a hit to ourselves?
			// Get ready for an attempt to trace through another person
			VectorCopy(tr.endpos, muzzle2);
			VectorCopy(tr.endpos, start);
			skip = tr.entityNum;
#ifdef _DEBUG
			gi.Printf("BAD! Concussion gun shot somehow traced back and hit the owner!\n");
#endif
			continue;
		}

		if (tr.fraction >= 1.0f)
		{
			// draw the beam but don't do anything else
			break;
		}

		gentity_t* traceEnt = &g_entities[tr.entityNum];

		if (traceEnt)
		{
			if (WP_SaberMustDisruptorBlock(traceEnt, ent, qfalse, tr.endpos, -1, -1) && !
				WP_DoingForcedAnimationForForcePowers(traceEnt))
			{
				G_MissileReflectEffect(traceEnt, tr.plane.normal);
				WP_ForcePowerDrain(traceEnt, FP_SABER_DEFENSE, WP_SaberBlockCost(traceEnt, ent, tr.endpos));
				//force player into a projective block move.
				hit_dodged = WP_SaberBlockBolt_MD(traceEnt, tr.endpos, qtrue);
			}
			else
			{
				if (traceEnt->s.number < MAX_CLIENTS || G_ControlledByPlayer(traceEnt))
				{
					hit_dodged = jedi_disruptor_dodge_evasion(traceEnt, ent, &tr, HL_NONE);
				}
				else
				{
					hit_dodged = jedi_npc_disruptor_dodge_evasion(traceEnt, ent, &tr, HL_NONE);
				}
				//acts like we didn't even hit him
			}
		}
		if (!hit_dodged)
		{
			if (render_impact)
			{
				if (tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage
					|| !Q_stricmp(traceEnt->classname, "misc_model_breakable")
					|| traceEnt->s.eType == ET_MOVER)
				{
					// Create a simple impact type mark that doesn't last long in the world
					G_PlayEffect(G_EffectIndex("concussion/alt_hit"), tr.endpos, tr.plane.normal);

					if (traceEnt->client && LogAccuracyHit(traceEnt, ent))
					{
						//NOTE: hitting multiple ents can still get you over 100% accuracy
						ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
					}

					const int hit_loc = G_GetHitLocFromTrace(&tr, MOD_CONC_ALT);
					const auto no_knock_back = static_cast<qboolean>((traceEnt->flags & FL_NO_KNOCKBACK) != 0);
					//will be set if they die, I want to know if it was on *before* they died
					if (traceEnt && traceEnt->client && traceEnt->client->NPC_class == CLASS_GALAKMECH)
					{
						//hehe
						G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, 10,
							DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC,
							MOD_CONC_ALT, hit_loc);
						break;
					}
					G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, damage,
						DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC,
						MOD_CONC_ALT, hit_loc);

					//do knockback and knockdown manually
					if (traceEnt->client)
					{
						//only if we hit a client
						vec3_t push_dir;
						VectorCopy(forward_vec, push_dir);
						if (push_dir[2] < 0.2f)
						{
							push_dir[2] = 0.2f;
						} //hmm, re-normalize?  nah...
						//if ( traceEnt->NPC || Q_irand(0,g_spskill->integer+1) )
						{
							if (!no_knock_back)
							{
								//knock-backable
								G_Throw(traceEnt, push_dir, 200);
								if (traceEnt->client->NPC_class == CLASS_ROCKETTROOPER)
								{
									traceEnt->client->ps.pm_time = Q_irand(1500, 3000);
								}
							}
							if (traceEnt->health > 0)
							{
								//alive
								if (G_HasKnockdownAnims(traceEnt))
								{
									//knock-downable
									G_Knockdown(traceEnt, ent, push_dir, 400, qtrue);
								}
							}
						}
					}

					if (traceEnt->s.eType == ET_MOVER)
					{
						//stop the traces on any mover
						break;
					}
				}
				else
				{
					// we only make this mark on things that can't break or move
					tent = G_TempEntity(tr.endpos, EV_CONC_ALT_MISS);
					tent->svFlags |= SVF_BROADCAST;
					VectorCopy(tr.plane.normal, tent->pos1);
					break;
					// hit solid, but doesn't take damage, so stop the shot...we _could_ allow it to shoot through walls, might be cool?
				}
			}
			else // not rendering impact, must be a skybox or other similar thing?
			{
				break; // don't try anymore traces
			}
		}
		// Get ready for an attempt to trace through another person
		VectorCopy(tr.endpos, muzzle2);
		VectorCopy(tr.endpos, start);
		skip = tr.entityNum;
		hit_dodged = qfalse;
	}
	//just draw one beam all the way to the end
	tent = G_TempEntity(tr.endpos, EV_CONC_ALT_SHOT);
	tent->svFlags |= SVF_BROADCAST;
	VectorCopy(muzzle, tent->s.origin2);

	// now go along the trail and make sight events
	VectorSubtract(tr.endpos, muzzle, dir);

	const float shot_dist = VectorNormalize(dir);

	//FIXME: if shoot *really* close to someone, the alert could be way out of their FOV
	for (float dist = 0; dist < shot_dist; dist += 64)
	{
		//FIXME: on a really long shot, this could make a LOT of alerts in one frame...
		VectorMA(muzzle, dist, dir, spot);
		AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
		//FIXME: creates *way* too many effects, make it one effect somehow?
		G_PlayEffect(G_EffectIndex("concussion/alt_ring"), spot, forward_vec);
	}
	//FIXME: spawn a temp ent that continuously spawns sight alerts here?  And 1 sound alert to draw their attention?
	VectorMA(start, shot_dist - 4, forward_vec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);

	G_PlayEffect(G_EffectIndex("concussion/altmuzzle_flash"), muzzle, forward_vec);
}

static void WP_FireConcussion(gentity_t* ent)
{
	//a fast rocket-like projectile
	vec3_t start;
	int damage = weaponData[WP_CONCUSSION].damage;
	constexpr float vel = CONC_VELOCITY;

	if (ent->s.number >= MAX_CLIENTS)
	{
		vec3_t angles;
		vectoangles(forward_vec, angles);
		angles[PITCH] += Q_flrand(-1.0f, 1.0f) * (CONC_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f); //was 0.5f
		angles[YAW] += Q_flrand(-1.0f, 1.0f) * (CONC_NPC_SPREAD + (6 - ent->NPC->currentAim) * 0.25f); //was 0.5f
		AngleVectors(angles, forward_vec, vright_vec, up);
	}

	//hold us still for a bit
	ent->client->ps.pm_time = 300;
	ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	//add viewkick
	if (ent->s.number < MAX_CLIENTS //player only
		&& !cg.renderingThirdPerson) //gives an advantage to being in 3rd person, but would look silly otherwise
	{
		//kick the view back
		cg.kick_angles[PITCH] = Q_flrand(-10, -15);
		cg.kick_time = level.time;
	}

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);
	//make sure our start point isn't on the other side of a wall

	gentity_t* missile = create_missile(start, forward_vec, vel, 10000, ent, qfalse);

	missile->classname = "conc_proj";
	missile->s.weapon = WP_CONCUSSION;
	missile->mass = 10;

	// Do the damages
	if (ent->s.number != 0)
	{
		if (g_spskill->integer == 0)
		{
			damage = CONC_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CONC_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CONC_NPC_DAMAGE_HARD;
		}
	}

	// Make it easier to hit things
	VectorSet(missile->maxs, ROCKET_SIZE, ROCKET_SIZE, ROCKET_SIZE);
	VectorScale(missile->maxs, -1, missile->mins);

	missile->damage = damage;
	missile->dflags = DAMAGE_EXTRA_KNOCKBACK;

	missile->methodOfDeath = MOD_CONC;
	missile->splashMethodOfDeath = MOD_CONC;

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = weaponData[WP_CONCUSSION].splashDamage;
	missile->splashRadius = weaponData[WP_CONCUSSION].splashRadius;

	// we don't want it to ever bounce
	missile->bounceCount = 0;
}

void WP_Concussion(gentity_t* ent, const qboolean alt_fire)
{
	if (alt_fire)
	{
		WP_FireConcussionAlt(ent);
	}
	else
	{
		WP_FireConcussion(ent);
	}
}