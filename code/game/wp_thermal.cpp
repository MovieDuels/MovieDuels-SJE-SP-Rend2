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
#include "w_local.h"

//---------------------
//	Thermal Detonator
//---------------------

extern cvar_t* g_SerenityJediEngineMode;
//---------------------------------------------------------
void thermalDetonatorExplode(gentity_t* ent)
//---------------------------------------------------------
{
	const int thermal_effect = Q_irand(0, 3);

	if (ent->s.eFlags & EF_HELD_BY_SAND_CREATURE)
	{
		ent->takedamage = qfalse; // don't allow double deaths!

		G_Damage(ent->activator, ent, ent->owner, vec3_origin, ent->currentOrigin, weaponData[WP_THERMAL].altDamage, 0,
			MOD_EXPLOSIVE);
		G_PlayEffect("thermal/explosion", ent->currentOrigin);
		G_PlayEffect("thermal/shockwave", ent->currentOrigin);

		G_FreeEntity(ent);
	}
	else if (!ent->count)
	{
		G_Sound(ent, G_SoundIndex("sound/weapons/thermal/warning.wav"));
		ent->count = 1;
		ent->nextthink = level.time + 800;
		ent->svFlags |= SVF_BROADCAST; //so everyone hears/sees the explosion?
	}
	else
	{
		vec3_t pos;

		VectorSet(pos, ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2] + 8);

		ent->takedamage = qfalse; // don't allow double deaths!

		G_RadiusDamage(ent->currentOrigin, ent->owner, weaponData[WP_THERMAL].splashDamage,
			weaponData[WP_THERMAL].splashRadius, nullptr, MOD_EXPLOSIVE_SPLASH);

		if (ent->owner && ent->owner->client->NPC_class == CLASS_JAN && g_SerenityJediEngineMode->integer == 2)
		{
			switch (thermal_effect)
			{
			case 3:
				G_PlayEffect("smoke/smokegrenade", ent->currentOrigin);
				break;
			case 2:
				G_PlayEffect("cryoban/explosion", ent->currentOrigin);
				break;
			case 1:
				G_PlayEffect("flash/faceflash", ent->currentOrigin);
				break;
			default:
				G_PlayEffect("thermal/explosion", ent->currentOrigin);
				break;
			}
		}
		else
		{
			G_PlayEffect("thermal/explosion", ent->currentOrigin);
		}

		G_PlayEffect("thermal/shockwave", ent->currentOrigin);

		G_FreeEntity(ent);
	}
}

//-------------------------------------------------------------------------------------------------------------
void thermal_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod, int d_flags,
	int hit_loc)
	//-------------------------------------------------------------------------------------------------------------
{
	thermalDetonatorExplode(self);
}

//---------------------------------------------------------
qboolean WP_LobFire(const gentity_t* self, vec3_t start, vec3_t target, vec3_t mins, vec3_t maxs, const int clipmask,
	vec3_t velocity, const qboolean trace_path, const int ignore_ent_num, const int enemy_num,
	float ideal_speed, const qboolean must_hit)
	//---------------------------------------------------------
{
	constexpr float speed_inc = 100;
	float best_impact_dist = Q3_INFINITE; //fireSpeed,
	vec3_t shot_vel, fail_case = { 0.0f };
	trace_t trace;
	trajectory_t tr{};
	int hit_count = 0;
	constexpr int max_hits = 7;

	if (!ideal_speed)
	{
		ideal_speed = 300;
	}
	else if (ideal_speed < speed_inc)
	{
		ideal_speed = speed_inc;
	}
	float shot_speed = ideal_speed;
	const int skip_num = (ideal_speed - speed_inc) / speed_inc;

	while (hit_count < max_hits)
	{
		vec3_t target_dir;
		VectorSubtract(target, start, target_dir);
		const float target_dist = VectorNormalize(target_dir);

		VectorScale(target_dir, shot_speed, shot_vel);
		float travel_time = target_dist / shot_speed;
		shot_vel[2] += travel_time * 0.5 * g_gravity->value;

		if (!hit_count)
		{
			//save the first (ideal) one as the failCase (fallback value)
			if (!must_hit)
			{
				//default is fine as a return value
				VectorCopy(shot_vel, fail_case);
			}
		}

		if (trace_path)
		{
			vec3_t last_pos;
			constexpr int time_step = 500;
			//do a rough trace of the path
			qboolean blocked = qfalse;

			VectorCopy(start, tr.trBase);
			VectorCopy(shot_vel, tr.trDelta);
			tr.trType = TR_GRAVITY;
			tr.trTime = level.time;
			travel_time *= 1000.0f;
			VectorCopy(start, last_pos);

			//This may be kind of wasteful, especially on long throws... use larger steps?  Divide the travelTime into a certain hard number of slices?  Trace just to apex and down?
			for (int elapsed_time = time_step; elapsed_time < floor(travel_time) + time_step; elapsed_time += time_step)
			{
				vec3_t test_pos;
				if (static_cast<float>(elapsed_time) > travel_time)
				{
					//cap it
					elapsed_time = floor(travel_time);
				}
				EvaluateTrajectory(&tr, level.time + elapsed_time, test_pos);
				gi.trace(&trace, last_pos, mins, maxs, test_pos, ignore_ent_num, clipmask, static_cast<EG2_Collision>(0),
					0);

				if (trace.allsolid || trace.startsolid)
				{
					blocked = qtrue;
					break;
				}
				if (trace.fraction < 1.0f)
				{
					//hit something
					if (trace.entityNum == enemy_num)
					{
						//hit the enemy, that's perfect!
						break;
					}
					if (trace.plane.normal[2] > 0.7 && DistanceSquared(trace.endpos, target) < 4096)
						//hit within 64 of desired location, should be okay
					{
						//close enough!
						break;
					}
					//FIXME: maybe find the extents of this brush and go above or below it on next try somehow?
					const float impact_dist = DistanceSquared(trace.endpos, target);
					if (impact_dist < best_impact_dist)
					{
						best_impact_dist = impact_dist;
						VectorCopy(shot_vel, fail_case);
					}
					blocked = qtrue;
					//see if we should store this as the failCase
					if (trace.entityNum < ENTITYNUM_WORLD)
					{
						//hit an ent
						const gentity_t* traceEnt = &g_entities[trace.entityNum];
						if (traceEnt && traceEnt->takedamage && !OnSameTeam(self, traceEnt))
						{
							//hit something breakable, so that's okay
							//we haven't found a clear shot yet so use this as the failcase
							VectorCopy(shot_vel, fail_case);
						}
					}
					break;
				}
				if (elapsed_time == floor(travel_time))
				{
					//reached end, all clear
					break;
				}
				//all clear, try next slice
				VectorCopy(test_pos, last_pos);
			}
			if (blocked)
			{
				//hit something, adjust speed (which will change arc)
				hit_count++;
				shot_speed = ideal_speed + (hit_count - skip_num) * speed_inc; //from min to max (skipping ideal)
				if (hit_count >= skip_num)
				{
					//skip ideal since that was the first value we tested
					shot_speed += speed_inc;
				}
			}
			else
			{
				//made it!
				break;
			}
		}
		else
		{
			//no need to check the path, go with first calc
			break;
		}
	}

	if (hit_count >= max_hits)
	{
		//NOTE: worst case scenario, use the one that impacted closest to the target (or just use the first try...?)
		assert(fail_case[0] + fail_case[1] + fail_case[2] > 0.0f);
		VectorCopy(fail_case, velocity);
		return qfalse;
	}
	VectorCopy(shot_vel, velocity);
	return qtrue;
}

//---------------------------------------------------------
void WP_ThermalThink(gentity_t* ent)
//---------------------------------------------------------
{
	qboolean blow = qfalse;

	// Thermal detonators for the player do occasional radius checks and blow up if there are entities in the blast radius
	//	This is done so that the main fire is actually useful as an attack.  We explode anyway after delay expires.

	if (ent->s.eFlags & EF_HELD_BY_SAND_CREATURE)
	{
		//blow once creature is underground (done with anim)
		//FIXME: chance of being spit out?  Especially if lots of delay left...
		ent->e_TouchFunc = touchF_NULL; //don't impact on anything
		if (!ent->activator
			|| !ent->activator->client
			|| !ent->activator->client->ps.legsAnimTimer)
		{
			//either something happened to the sand creature or it's done with it's attack anim
			//blow!
			ent->e_ThinkFunc = thinkF_thermalDetonatorExplode;
			ent->nextthink = level.time + Q_irand(50, 2000);
		}
		else
		{
			//keep checking
			ent->nextthink = level.time + TD_THINK_TIME;
		}
		return;
	}
	if (ent->delay > level.time)
	{
		//	Finally, we force it to bounce at least once before doing the special checks, otherwise it's just too easy for the player?
		if (ent->has_bounced)
		{
			const int count = G_RadiusList(ent->currentOrigin, TD_TEST_RAD, ent, qtrue, ent_list);

			for (int i = 0; i < count; i++)
			{
				if (ent_list[i]->s.number == 0)
				{
					// avoid deliberately blowing up next to the player, no matter how close any enemy is..
					//	...if the delay time expires though, there is no saving the player...muwhaaa haa ha
					blow = qfalse;
					break;
				}
				if (ent_list[i]->client
					&& ent_list[i]->client->NPC_class != CLASS_SAND_CREATURE //ignore sand creatures
					&& ent_list[i]->health > 0)
				{
					//FIXME! sometimes the ent_list order changes, so we should make sure that the player isn't anywhere in this list
					blow = qtrue;
				}
			}
		}
	}
	else
	{
		// our death time has arrived, even if nothing is near us
		blow = qtrue;
	}

	if (blow)
	{
		ent->e_ThinkFunc = thinkF_thermalDetonatorExplode;
		ent->nextthink = level.time + 50;
	}
	else
	{
		// we probably don't need to do this thinking logic very often...maybe this is fast enough?
		ent->nextthink = level.time + TD_THINK_TIME;
	}
}

//---------------------------------------------------------
gentity_t* WP_FireThermalDetonator(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t dir, start;
	float damage_scale = 1.0f;

	VectorCopy(forward_vec, dir);
	VectorCopy(muzzle, start);

	gentity_t* bolt = G_Spawn();

	bolt->classname = "thermal_detonator";

	if (ent->s.number != 0)
	{
		// If not the player, cut the damage a bit so we don't get pounded on so much
		damage_scale = TD_NPC_DAMAGE_CUT;
	}

	if (!alt_fire && ent->s.number == 0)
	{
		// Main fires for the players do a little bit of extra thinking
		bolt->e_ThinkFunc = thinkF_WP_ThermalThink;
		bolt->nextthink = level.time + TD_THINK_TIME;
		bolt->delay = level.time + TD_TIME; // How long 'til she blows
	}
	else
	{
		bolt->e_ThinkFunc = thinkF_thermalDetonatorExplode;
		bolt->nextthink = level.time + TD_TIME; // How long 'til she blows
	}

	bolt->mass = 10;

	// How 'bout we give this thing a size...
	VectorSet(bolt->mins, -4.0f, -4.0f, -4.0f);
	VectorSet(bolt->maxs, 4.0f, 4.0f, 4.0f);
	bolt->clipmask = MASK_SHOT;
	bolt->clipmask &= ~CONTENTS_CORPSE;
	bolt->contents = CONTENTS_SHOTCLIP;
	bolt->takedamage = qtrue;
	bolt->health = 15;
	bolt->e_DieFunc = dieF_thermal_die;

	WP_TraceSetStart(ent, start); //make sure our start point isn't on the other side of a wall

	float charge_amount = 1.0f; // default of full charge

	if (ent->client)
	{
		charge_amount = level.time - ent->client->ps.weaponChargeTime;
	}

	// get charge amount
	charge_amount = charge_amount / static_cast<float>(TD_VELOCITY);

	if (charge_amount > 1.0f)
	{
		charge_amount = 1.0f;
	}
	else if (charge_amount < TD_MIN_CHARGE)
	{
		charge_amount = TD_MIN_CHARGE;
	}

	float thrown_speed = TD_VELOCITY;
	const auto this_is_a_shooter = static_cast<qboolean>(!Q_stricmp("misc_weapon_shooter", ent->classname));

	if (this_is_a_shooter)
	{
		if (ent->delay != 0)
		{
			thrown_speed = ent->delay;
		}
	}

	// normal ones bounce, alt ones explode on impact
	bolt->s.pos.trType = TR_GRAVITY;
	bolt->owner = ent;
	VectorScale(dir, thrown_speed * charge_amount, bolt->s.pos.trDelta);

	if (ent->health > 0)
	{
		bolt->s.pos.trDelta[2] += 120;

		if ((ent->NPC || ent->s.number && this_is_a_shooter)
			&& ent->enemy)
		{
			//NPC or misc_weapon_shooter
			//FIXME: we're assuming he's actually facing this direction...
			vec3_t target;

			VectorCopy(ent->enemy->currentOrigin, target);
			if (target[2] <= start[2])
			{
				vec3_t vec;
				VectorSubtract(target, start, vec);
				VectorNormalize(vec);
				VectorMA(target, Q_flrand(0, -32), vec, target); //throw a little short
			}

			target[0] += Q_flrand(-5, 5) + Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2;
			target[1] += Q_flrand(-5, 5) + Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2;
			target[2] += Q_flrand(-5, 5) + Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2;

			WP_LobFire(ent, start, target, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue,
				ent->s.number, ent->enemy->s.number);
		}
		else if (this_is_a_shooter && ent->target && !VectorCompare(ent->pos1, vec3_origin))
		{
			//misc_weapon_shooter firing at a position
			WP_LobFire(ent, start, ent->pos1, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue,
				ent->s.number, ent->enemy->s.number);
		}
	}

	if (alt_fire)
	{
		bolt->alt_fire = qtrue;
	}
	else
	{
		bolt->s.eFlags |= EF_BOUNCE_HALF;
	}

	bolt->s.loopSound = G_SoundIndex("sound/weapons/thermal/thermloop.wav");

	bolt->damage = weaponData[WP_THERMAL].damage * damage_scale;
	bolt->dflags = 0;
	bolt->splashDamage = weaponData[WP_THERMAL].splashDamage * damage_scale;
	bolt->splashRadius = weaponData[WP_THERMAL].splashRadius;

	bolt->s.eType = ET_MISSILE;
	bolt->svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_THERMAL;

	if (alt_fire)
	{
		bolt->methodOfDeath = MOD_THERMAL_ALT;
		bolt->splashMethodOfDeath = MOD_THERMAL_ALT; //? SPLASH;
	}
	else
	{
		bolt->methodOfDeath = MOD_THERMAL;
		bolt->splashMethodOfDeath = MOD_THERMAL; //? SPLASH;
	}

	bolt->s.pos.trTime = level.time; // move a bit on the very first frame
	VectorCopy(start, bolt->s.pos.trBase);

	SnapVector(bolt->s.pos.trDelta); // save net bandwidth
	VectorCopy(start, bolt->currentOrigin);

	VectorCopy(start, bolt->pos2);

	return bolt;
}

//---------------------------------------------------------
gentity_t* WP_DropThermal(gentity_t* ent)
//---------------------------------------------------------
{
	AngleVectors(ent->client->ps.viewangles, forward_vec, vright_vec, up);
	CalcEntitySpot(ent, SPOT_WEAPON, muzzle);
	return WP_FireThermalDetonator(ent, qfalse);
}