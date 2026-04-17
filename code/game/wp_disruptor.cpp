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

//---------------------
//	Tenloss Disruptor
//---------------------

extern qboolean WP_DoingForcedAnimationForForcePowers(const gentity_t* ent);
extern int WP_SaberMustDisruptorBlock(gentity_t* self, const gentity_t* atk, qboolean check_b_box_block, vec3_t point, int rSaberNum, int rBladeNum);
extern qboolean WP_SaberBlockBolt_MD(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern void G_MissileReflectEffect(const gentity_t* ent, vec3_t dir);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern int WP_SaberBlockCost(gentity_t* defender, const gentity_t* attacker, vec3_t hit_locs);
extern qboolean G_ControlledByPlayer(const gentity_t* self);

//---------------------------------------------------------
static void WP_DisruptorMainFire(gentity_t* ent)
//---------------------------------------------------------
{
	int damage = weaponData[WP_DISRUPTOR].damage;
	qboolean render_impact = qtrue;
	vec3_t start, end, spot;
	trace_t tr;
	gentity_t* traceEnt = nullptr;
	constexpr float shot_range = 8192;

	if (ent->NPC)
	{
		switch (g_spskill->integer)
		{
		case 0:
			damage = DISRUPTOR_NPC_MAIN_DAMAGE_EASY;
			break;
		case 1:
			damage = DISRUPTOR_NPC_MAIN_DAMAGE_MEDIUM;
			break;
		case 2:
		default:
			damage = DISRUPTOR_NPC_MAIN_DAMAGE_HARD;
			break;
		}
	}

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);

	WP_MissileTargetHint(ent, start, forward_vec);
	VectorMA(start, shot_range, forward_vec, end);

	int ignore = ent->s.number;
	int traces = 0;
	while (traces < 10)
	{
		//need to loop this in case we hit a Jedi who dodges the shot
		gi.trace(&tr, start, nullptr, nullptr, end, ignore, MASK_SHOT, G2_RETURNONHIT, 0);

		traceEnt = &g_entities[tr.entityNum];

		if (traceEnt)
		{
			if (traceEnt->s.number < MAX_CLIENTS || G_ControlledByPlayer(traceEnt))
			{
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
			else
			{
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
				if (jedi_npc_disruptor_dodge_evasion(traceEnt, ent, &tr, HL_NONE))
				{
					//act like we didn't even hit him
					VectorCopy(tr.endpos, start);
					ignore = tr.entityNum;
					traces++;
					continue;
				}
			}
		}
		//a Jedi is not dodging this shot
		break;
	}

	if (tr.surfaceFlags & SURF_NOIMPACT)
	{
		render_impact = qfalse;
	}

	// always render a shot beam, doing this the old way because I don't much feel like overriding the effect.
	gentity_t* tent = G_TempEntity(tr.endpos, EV_DISRUPTOR_MAIN_SHOT);
	tent->svFlags |= SVF_BROADCAST;
	VectorCopy(muzzle, tent->s.origin2);

	if (render_impact)
	{
		if (tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage)
		{
			// Create a simple impact type mark that doesn't last long in the world
			G_PlayEffect(G_EffectIndex("disruptor/flesh_impact"), tr.endpos, tr.plane.normal);

			if (traceEnt->client && LogAccuracyHit(traceEnt, ent))
			{
				ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
			}

			const int hit_loc = G_GetHitLocFromTrace(&tr, MOD_DISRUPTOR);
			if (traceEnt && traceEnt->client && traceEnt->client->NPC_class == CLASS_GALAKMECH)
			{
				//hehe
				G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, 3, DAMAGE_DEATH_KNOCKBACK, MOD_DISRUPTOR, hit_loc);
			}
			else
			{
				G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, damage, DAMAGE_DEATH_KNOCKBACK, MOD_DISRUPTOR,
					hit_loc);
			}
		}
		else
		{
			G_PlayEffect(G_EffectIndex("disruptor/wall_impact"), tr.endpos, tr.plane.normal);
		}
	}

	const float shotDist = shot_range * tr.fraction;

	for (float dist = 0; dist < shotDist; dist += 64)
	{
		//FIXME: on a really long shot, this could make a LOT of alerts in one frame...
		VectorMA(start, dist, forward_vec, spot);
		AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
	}
	VectorMA(start, shotDist - 4, forward_vec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
}

//---------------------------------------------------------
static void WP_DisruptorAltFire(gentity_t* ent)
//---------------------------------------------------------
{
	int damage = weaponData[WP_DISRUPTOR].altDamage, traces = DISRUPTOR_ALT_TRACES;
	qboolean render_impact = qtrue;
	vec3_t start;
	vec3_t muzzle2, spot, dir;
	trace_t tr;
	gentity_t* tent;
	qboolean hit_dodged = qfalse, full_charge = qfalse;

	VectorCopy(muzzle, muzzle2); // making a backup copy

	// The trace start will originate at the eye so we can ensure that it hits the crosshair.
	if (ent->NPC)
	{
		switch (g_spskill->integer)
		{
		case 0:
			damage = DISRUPTOR_NPC_ALT_DAMAGE_EASY;
			break;
		case 1:
			damage = DISRUPTOR_NPC_ALT_DAMAGE_MEDIUM;
			break;
		case 2:
		default:
			damage = DISRUPTOR_NPC_ALT_DAMAGE_HARD;
			break;
		}
		VectorCopy(muzzle, start);

		full_charge = qtrue;
	}
	else
	{
		VectorCopy(ent->client->renderInfo.eyePoint, start);
		AngleVectors(ent->client->renderInfo.eyeAngles, forward_vec, nullptr, nullptr);

		// don't let NPC's do charging
		int count = (level.time - ent->client->ps.weaponChargeTime - 50) / DISRUPTOR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count >= 10)
		{
			count = 10;
			full_charge = qtrue;
		}

		// more powerful charges go through more things
		if (count < 3)
		{
			traces = 1;
		}
		else if (count < 6)
		{
			traces = 2;
		}
		//else do full traces

		damage = damage * count + weaponData[WP_DISRUPTOR].damage * 0.5f; // give a boost to low charge shots
	}

	int skip = ent->s.number;

	for (int i = 0; i < traces; i++)
	{
		constexpr float shot_range = 8192;
		vec3_t end;
		VectorMA(start, shot_range, forward_vec, end);

		//NOTE: if you want to be able to hit guys in emplaced guns, use "G2_COLLIDE, 10" instead of "G2_RETURNONHIT, 0"
		//alternately, if you end up hitting an emplaced_gun that has a sitter, just redo this one trace with the "G2_COLLIDE, 10" to see if we it the sitter
		gi.trace(&tr, start, nullptr, nullptr, end, skip, MASK_SHOT, G2_COLLIDE, 10); //G2_RETURNONHIT, 0 );

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
			gi.Printf("BAD! Disruptor gun shot somehow traced back and hit the owner!\n");
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
					G_PlayEffect(G_EffectIndex("disruptor/alt_hit"), tr.endpos, tr.plane.normal);

					if (traceEnt->client && LogAccuracyHit(traceEnt, ent))
					{
						//NOTE: hitting multiple ents can still get you over 100% accuracy
						ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
					}

					const int hit_loc = G_GetHitLocFromTrace(&tr, MOD_DISRUPTOR);
					if (traceEnt && traceEnt->client && traceEnt->client->NPC_class == CLASS_GALAKMECH)
					{
						//hehe
						G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, 10,
							DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC,
							full_charge ? MOD_SNIPER : MOD_DISRUPTOR, hit_loc);
						break;
					}
					G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, damage,
						DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC,
						full_charge ? MOD_SNIPER : MOD_DISRUPTOR, hit_loc);
					if (traceEnt->s.eType == ET_MOVER)
					{
						//stop the traces on any mover
						break;
					}
				}
				else
				{
					// we only make this mark on things that can't break or move
					tent = G_TempEntity(tr.endpos, EV_DISRUPTOR_SNIPER_MISS);
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
	//just draw one solid beam all the way to the end...
	tent = G_TempEntity(tr.endpos, EV_DISRUPTOR_MAIN_SHOT);
	tent->svFlags |= SVF_BROADCAST;
	tent->alt_fire = full_charge; // mark us so we can alter the effect
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
	}
	//FIXME: spawn a temp ent that continuously spawns sight alerts here?  And 1 sound alert to draw their attention?
	VectorMA(start, shot_dist - 4, forward_vec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
}

//---------------------------------------------------------
void WP_FireDisruptor(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	if (alt_fire)
	{
		WP_DisruptorAltFire(ent);
	}
	else
	{
		WP_DisruptorMainFire(ent);
	}

	G_PlayEffect(G_EffectIndex("disruptor/line_cap"), muzzle, forward_vec);
}