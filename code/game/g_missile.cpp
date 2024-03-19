/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ///
///																																///
///																																///
///													SERENITY JEDI ENGINE														///
///										          LIGHTSABER COMBAT SYSTEM													    ///
///																																///
///						      System designed by Serenity and modded by JaceSolaris. (c) 2019 SJE   		                    ///
///								    https://www.moddb.com/mods/serenityjediengine-20											///
///																																///
/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ///

#include "g_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "bg_local.h"
#include "../cgame/cg_local.h"
#include "b_local.h"

#ifdef _DEBUG
#include <float.h>
#endif //_DEBUG

constexpr auto TASER_DAMAGE = 20;
extern cvar_t* g_DebugSaberCombat;
extern qboolean WP_DoingForcedAnimationForForcePowers(const gentity_t* ent);
qboolean LogAccuracyHit(const gentity_t* target, const gentity_t* attacker);
extern qboolean InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern qboolean PM_SaberInParry(int move);
extern qboolean PM_SaberInReflect(int move);
extern qboolean PM_SaberInIdle(int move);
extern qboolean PM_SaberInAttack(int move);
extern qboolean PM_SaberInTransitionAny(int move);
extern qboolean pm_saber_in_special_attack(int anim);
extern qboolean WP_SaberBlockBolt_MD(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean WP_SaberBlockBolt_AMD(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean WP_SaberBlockNonRandom(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean WP_SaberBounceDirection(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean G_ControlledByNPC(const gentity_t* self);
extern qboolean PM_SaberInStart(int move);
extern qboolean PM_SaberInReturn(int move);
extern int WP_SaberMustBoltBlockJKAMode(gentity_t* self, const gentity_t* atk, qboolean check_b_box_block, vec3_t point,
	int rSaberNum, int rBladeNum);
extern int WP_SaberMustBoltBlock(gentity_t* self, const gentity_t* atk, qboolean check_b_box_block, vec3_t point,
	int rSaberNum,
	int rBladeNum);
extern float VectorBlockDistance(vec3_t v1, vec3_t v2);
extern void PM_AddBoltBlockFatigue(playerState_t* ps, int fatigue);
extern void PM_VelocityForSaberMove(const playerState_t* ps, vec3_t throw_dir);
extern qboolean WP_SaberLose(gentity_t* self, vec3_t throw_dir);
extern qboolean WP_BoltBlockVictimFatigued(gentity_t* victim);
extern qboolean wp_bolt_block_victim_reflected(gentity_t* victim);
void WP_HandleBoltBlock(gentity_t* player, gentity_t* bolt, vec3_t forward);
extern cvar_t* g_SaberPerfectBlockingTimerEasy;
extern cvar_t* g_SaberPerfectBlockingTimerNormal;
extern cvar_t* g_SaberPerfectBlockingTimerHard;
extern cvar_t* g_SaberPerfectBlockingTimerMissile;
extern cvar_t* g_SerenityJediEngineMode;
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern void WP_BlockPointsDrain(const gentity_t* self, int fatigue);
extern int WP_SaberBoltBlockCost(gentity_t* defender, const gentity_t* attacker);
extern void PM_AddBlockFatigue(playerState_t* ps, int fatigue);
extern void jedi_decloak(gentity_t* self);
extern void player_decloak(gentity_t* self);
extern qboolean FighterIsLanded(const Vehicle_t* p_veh, playerState_t* parent_ps);
extern void G_BlastDown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength);
constexpr auto MISSILE_PRESTEP_TIME = 50;
//-------------------------------------------------------------------------
static void G_MissileBounceEffect(const gentity_t* ent, vec3_t org, vec3_t dir, const qboolean hit_world)
{
	switch (ent->s.weapon)
	{
	case WP_BOWCASTER:
		if (hit_world)
		{
			G_PlayEffect("bowcaster/bounce_wall", org, dir);
		}
		else
		{
			G_PlayEffect("bowcaster/deflect", ent->currentOrigin, dir);
		}
		break;
	case WP_BLASTER:
	case WP_BRYAR_PISTOL:
	case WP_SBD_BLASTER:
	case WP_BLASTER_PISTOL:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
	case WP_DROIDEKA:
	case WP_JAWA:
		G_PlayEffect("blaster/deflect", ent->currentOrigin, dir);
		break;
	default:
	{
		gentity_t* tent = G_TempEntity(org, EV_GRENADE_BOUNCE);
		VectorCopy(dir, tent->pos1);
		tent->s.weapon = ent->s.weapon;
	}
	break;
	}
}

void G_MissileReflectEffect(const gentity_t* ent, vec3_t dir)
{
	switch (ent->s.weapon)
	{
	case WP_BOWCASTER:
		G_PlayEffect("bowcaster/deflect", ent->currentOrigin, dir);
		break;
	case WP_BLASTER:
	case WP_BRYAR_PISTOL:
	case WP_SBD_BLASTER:
	case WP_BLASTER_PISTOL:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
	case WP_DROIDEKA:
	case WP_JAWA:
		G_PlayEffect("blaster/deflect", ent->currentOrigin, dir);
		break;
	default:
		G_PlayEffect("blaster/deflect", ent->currentOrigin, dir);
		break;
	}

	if (ent->owner && !ent->owner->NPC)
	{
		CGCam_BlockShakeSP(0.45f, 100);
	}
}

static void G_MissileBounceBeskarEffect(const gentity_t* ent, vec3_t dir)
{
	G_PlayEffect("blaster/beskar_impact", ent->currentOrigin, dir);

	if (ent->owner && !ent->owner->NPC)
	{
		CGCam_BlockShakeSP(0.45f, 100);
	}
}

//-------------------------------------------------------------------------
static void G_MissileStick(gentity_t* missile, gentity_t* other, trace_t* tr)
{
	if (other->NPC || !Q_stricmp(other->classname, "misc_model_breakable"))
	{
		vec3_t velocity;

		const int hit_time = level.previousTime + (level.time - level.previousTime) * tr->fraction;

		EvaluateTrajectoryDelta(&missile->s.pos, hit_time, velocity);

		const float dot = DotProduct(velocity, tr->plane.normal);
		G_SetOrigin(missile, tr->endpos);
		VectorMA(velocity, -1.6f * dot, tr->plane.normal, missile->s.pos.trDelta);
		VectorMA(missile->s.pos.trDelta, 10, tr->plane.normal, missile->s.pos.trDelta);
		missile->s.pos.trTime = level.time - 10; // move a bit on the first frame

		// check for stop
		if (tr->entityNum >= 0 && tr->entityNum < ENTITYNUM_WORLD &&
			tr->plane.normal[2] > 0.7 && missile->s.pos.trDelta[2] < 40)
			//this can happen even on very slightly sloped walls, so changed it from > 0 to > 0.7
		{
			missile->nextthink = level.time + 100;
		}
		else
		{
			// fall till we hit the ground
			missile->s.pos.trType = TR_GRAVITY;
		}

		return; // don't stick yet
	}

	if (missile->e_TouchFunc != touchF_NULL)
	{
		GEntity_TouchFunc(missile, other, tr);
	}

	G_AddEvent(missile, EV_MISSILE_STICK, 0);

	if (other->s.eType == ET_MOVER || other->e_DieFunc == dieF_funcBBrushDie || other->e_DieFunc == dieF_funcGlassDie)
	{
		// movers and breakable brushes need extra info...so sticky missiles can ride lifts and blow up when the thing they are attached to goes away.
		missile->s.groundEntityNum = tr->entityNum;
	}
}

/*
================
  Reflect the missile roughly back at it's owner
================
*/
extern gentity_t* jedi_find_enemy_in_cone(const gentity_t* self, gentity_t* fallback, float min_dot);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingOrStanding(const gentity_t* self);
vec3_t g_crosshairWorldCoord = { 0, 0, 0 };

void G_MissileBouncedoffSaber(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean perfect_reflection = qfalse;
	qboolean reflected = qfalse;
	gentity_t* owner = ent;

	if (ent->owner)
	{
		owner = ent->owner;
	}

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	if (ent && owner && owner->client &&
		(owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] > FORCE_LEVEL_2 && !Q_irand(0, 3)))
	{
		perfect_reflection = qtrue;
	}

	if (owner && owner->client->ps.saberInFlight)
	{
		//but need saber in-hand for perfect reflection
		perfect_reflection = qfalse;
	}

	if (g_spskill->integer == 2 && owner->client->ps.saberBlockingTime < level.time)
	{
		//but need to be blocking for perfect reflection on higher difficulties
		perfect_reflection = qfalse;
	}

	if (perfect_reflection)
	{
		if (owner->s.client_num >= MAX_CLIENTS)
		{
			gentity_t* enemy;
			if (owner->enemy && Q_irand(0, 3))
			{
				//toward current enemy 75% of the time
				enemy = owner->enemy;
			}
			else
			{
				//find another enemy
				enemy = jedi_find_enemy_in_cone(owner, owner->enemy, 0.3f);
			}
			if (enemy)
			{
				vec3_t bullseye;
				CalcEntitySpot(enemy, SPOT_CHEST, bullseye);
				bullseye[0] += Q_irand(-4, 4);
				bullseye[1] += Q_irand(-4, 4);
				bullseye[2] += Q_irand(-16, 4);
				VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
				VectorNormalize(bounce_dir);
				if (!PM_SaberInParry(owner->client->ps.saber_move)
					&& !PM_SaberInReflect(owner->client->ps.saber_move)
					&& !PM_SaberInIdle(owner->client->ps.saber_move))
				{
					//a bit more wild
					if (PM_SaberInAttack(owner->client->ps.saber_move)
						|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
						|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
					{
						//moderately more wild
						for (i = 0; i < 3; i++)
						{
							bounce_dir[i] += Q_flrand(-0.6f, 0.6f);
						}
					}
					else
					{
						//mildly more wild
						for (i = 0; i < 3; i++)
						{
							bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
						}
					}
				}
				VectorNormalize(bounce_dir);
				reflected = qtrue;
			}
		}
		else
		{
			VectorSubtract(g_crosshairWorldCoord, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.6f, 0.6f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
					}
				}
			}
			VectorNormalize(bounce_dir);
			reflected = qtrue;
		}
	}
	if (!reflected)
	{
		if (owner && owner->s.client_num >= MAX_CLIENTS)
		{
			if (missile->owner && missile->s.weapon != WP_SABER)
			{
				//bounce back at them if you can
				VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
				VectorNormalize(bounce_dir);
			}
			else
			{
				vec3_t missile_dir;

				VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
				VectorCopy(missile->s.pos.trDelta, bounce_dir);
				VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
				VectorNormalize(bounce_dir);
			}
		}
		else
		{
			vec3_t deflect_dir, missile_dir;
			float force_factor;
			VectorSubtract(g_crosshairWorldCoord, missile->currentOrigin, deflect_dir);
			VectorCopy(missile->s.pos.trDelta, missile_dir);
			VectorNormalize(missile_dir);
			VectorNormalize(deflect_dir);

			//bigger forceFactors make the reflected shots go closer to the cross hair
			switch (owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE])
			{
			case FORCE_LEVEL_1:
				force_factor = 1.0f;
				break;
			case FORCE_LEVEL_2:
				force_factor = 2.0f;
				break;
			default:
				force_factor = 3.0f;
				break;
			}

			VectorMA(missile_dir, force_factor, deflect_dir, bounce_dir);

			VectorNormalize(bounce_dir);
		}
		if (owner->s.weapon == WP_SABER && owner->client)
		{
			//saber
			if (owner->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1 or below
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
				}
			}
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.6f, 0.6f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
			}
		}
	}
	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = owner;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

void G_ReflectMissileAuto(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean reflected = qfalse;
	gentity_t* owner = ent;

	if (ent->owner)
	{
		owner = ent->owner;
	}

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	if (ent && owner && owner->client && !owner->client->ps.saberInFlight &&
		(owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] > FORCE_LEVEL_2 || owner->client->ps.forcePowerLevel[
			FP_SABER_DEFENSE] > FORCE_LEVEL_1 && !Q_irand(0, 3)))
	{
		//if high enough defense skill and saber in-hand (100% at level 3, 25% at level 2, 0% at level 1), reflections are perfectly deflected toward an enemy
		gentity_t* enemy;
		if (owner->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			enemy = owner->enemy;
		}
		else
		{
			//find another enemy
			enemy = jedi_find_enemy_in_cone(owner, owner->enemy, 0.3f);
		}
		if (enemy)
		{
			vec3_t bullseye;
			CalcEntitySpot(enemy, SPOT_HEAD, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
			}
			VectorNormalize(bounce_dir);
			reflected = qtrue;
		}
	}
	if (!reflected)
	{
		if (missile->owner && missile->s.weapon != WP_SABER)
		{
			//bounce back at them if you can
			VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);
		}
		else
		{
			vec3_t missile_dir;

			VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
			VectorCopy(missile->s.pos.trDelta, bounce_dir);
			VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
			VectorNormalize(bounce_dir);
		}
		if (owner && owner->client && owner->s.weapon == WP_SABER)
		{
			//saber
			if (owner->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
				}
			}
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
			}
		}
	}
	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(
		!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]
		));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = owner;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

void G_ReflectMissileNPC(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean reflected = qfalse;
	gentity_t* owner = ent;

	if (ent->owner)
	{
		owner = ent->owner;
	}

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	if (ent && owner && owner->client && !owner->client->ps.saberInFlight &&
		(owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] > FORCE_LEVEL_2 || owner->client->ps.forcePowerLevel[
			FP_SABER_DEFENSE] > FORCE_LEVEL_1 && !Q_irand(0, 3)))
	{
		//if high enough defense skill and saber in-hand (100% at level 3, 25% at level 2, 0% at level 1), reflections are perfectly deflected toward an enemy
		gentity_t* enemy;
		if (owner->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			enemy = owner->enemy;
		}
		else
		{
			//find another enemy
			enemy = jedi_find_enemy_in_cone(owner, owner->enemy, 0.3f);
		}
		if (enemy)
		{
			vec3_t bullseye;
			CalcEntitySpot(enemy, SPOT_HEAD, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}
			VectorNormalize(bounce_dir);
			reflected = qtrue;
		}
	}
	if (!reflected)
	{
		if (missile->owner && missile->s.weapon != WP_SABER)
		{
			//bounce back at them if you can
			VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);
		}
		else
		{
			vec3_t missile_dir;

			VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
			VectorCopy(missile->s.pos.trDelta, bounce_dir);
			VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
			VectorNormalize(bounce_dir);
		}
		if (owner && owner->client && owner->s.weapon == WP_SABER)
		{
			//saber
			if (owner->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
				}
			}
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
			}
		}
	}
	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(
		!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]
		));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = owner;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

extern qboolean WalkCheck(const gentity_t* self);

static void G_BoltBlockMissile(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean saber_block_reflection = qfalse;
	qboolean bolt_block_reflection = qfalse;
	qboolean npc_reflection = qfalse;
	qboolean reflected = qfalse;
	gentity_t* blocker = ent;
	constexpr int punish = BLOCKPOINTS_TEN;

	if (ent->owner)
	{
		blocker = ent->owner;
	}

	const qboolean manual_blocking = blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] > FORCE_LEVEL_0 ? qtrue : qfalse;
	const qboolean manual_proj_blocking = WalkCheck(blocker) ? qtrue : qfalse;
	const qboolean np_cis_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_JKAMODENPCBLOCKING ? qtrue : qfalse;
	float slop_factor = (FATIGUE_AUTOBOLTBLOCK - 6) * (static_cast<float>(FORCE_LEVEL_3) - blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE]) / FORCE_LEVEL_3;

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	AngleVectors(blocker->client->ps.viewangles, forward, nullptr, nullptr);

	if (ent && blocker && blocker->client && manual_blocking && !manual_proj_blocking)
	{
		saber_block_reflection = qtrue;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && manual_blocking && manual_proj_blocking && blocker->client->ps.SaberActive())
	{
		bolt_block_reflection = qtrue;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && np_cis_blocking)
	{
		npc_reflection = qtrue;
	}

	if (ent && blocker && blocker->client && blocker->client->ps.saberInFlight)
	{
		//but need saber in-hand for perfect reflection
		saber_block_reflection = qfalse;
		bolt_block_reflection = qfalse;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
	{
		//but need saber in-hand for perfect reflection
		saber_block_reflection = qfalse;
		bolt_block_reflection = qfalse;
		npc_reflection = qfalse;
	}

	//play projectile block animation
	if (saber_block_reflection && !npc_reflection) //GOES TO CROSSHAIR
	{
		vec3_t angs;
		if (level.time - blocker->client->ps.ManualblockStartTime < 3000) //Blocking 2
		{
			// good
			vectoangles(forward, angs);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		else if (blocker->client->pers.cmd.forwardmove >= 0)
		{
			//bad
			slop_factor += Q_irand(1, 5);
			vectoangles(forward, angs);
			angs[PITCH] += Q_irand(-slop_factor, slop_factor);
			angs[YAW] += Q_irand(-slop_factor, slop_factor);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		else
		{
			//average
			slop_factor += Q_irand(1, 3);
			vectoangles(forward, angs);
			angs[PITCH] += Q_irand(-slop_factor, slop_factor);
			angs[YAW] += Q_irand(-slop_factor, slop_factor);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		VectorCopy(forward, bounce_dir);

		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_GREEN"JKA Mode Crosshair Deflection\n");
		}

		if (blocker->client->ps.forcePower < BLOCKPOINTS_THIRTY)
		{
			//very Low points = bad blocks
			if (blocker->client->ps.forcePower < BLOCKPOINTS_FATIGUE)
			{
				//Low points = bad blocks
				WP_BoltBlockVictimFatigued(blocker);
				blocker->client->ps.saberBlocked = BLOCKED_NONE;
				blocker->client->ps.saberBounceMove = LS_NONE;
			}
			else
			{
				WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
			}
		}
		else
		{
			WP_SaberBlockNonRandom(blocker, missile->currentOrigin, qfalse);
		}

		const int force_points_used_used = WP_SaberBoltBlockCost(blocker, missile);

		if (blocker->client->ps.forcePower < force_points_used_used)
		{
			blocker->client->ps.forcePower = 0;
		}
		else
		{
			WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, force_points_used_used);
		}
		reflected = qtrue;
	}
	else if (bolt_block_reflection && !npc_reflection) //GOES TO ENEMY
	{
		gentity_t* attacker;

		if (blocker->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			attacker = blocker->enemy;
		}
		else
		{
			//find another enemy
			attacker = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}

		if (attacker)
		{
			vec3_t bullseye;
			CalcEntitySpot(attacker, SPOT_HEAD, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim)
					|| blocker->client->ps.forcePower < BLOCKPOINTS_KNOCKAWAY)
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}

			VectorNormalize(bounce_dir);

			if (blocker->client->ps.forcePower < BLOCKPOINTS_THIRTY)
			{
				//very Low points = bad blocks
				if (blocker->client->ps.forcePower < BLOCKPOINTS_FATIGUE)
				{
					//Low points = bad blocks
					WP_BoltBlockVictimFatigued(blocker);
					blocker->client->ps.saberBlocked = BLOCKED_NONE;
					blocker->client->ps.saberBounceMove = LS_NONE;
				}
				else
				{
					WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
				}
			}
			else
			{
				WP_SaberBlockBolt_MD(blocker, missile->currentOrigin, qfalse);
			}

			const int force_points_used_used = WP_SaberBoltBlockCost(blocker, missile);

			if (blocker->client->ps.forcePower < force_points_used_used)
			{
				blocker->client->ps.forcePower = 0;
			}
			else
			{
				WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, force_points_used_used);
			}

			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_GREEN"JKA Mode Bolt Blocking to enemy\n");
			}
			reflected = qtrue;
		}
	}
	else if (npc_reflection && !saber_block_reflection && !bolt_block_reflection) //GOES TO ENEMY
	{
		gentity_t* enemy;

		if (blocker->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			enemy = blocker->enemy;
		}
		else
		{
			//find another enemy
			enemy = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}

		if (enemy)
		{
			vec3_t bullseye;
			CalcEntitySpot(enemy, SPOT_CHEST, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}

			VectorNormalize(bounce_dir);

			if (blocker->client->ps.forcePower < BLOCKPOINTS_THIRTY)
			{
				//very Low points = bad blocks
				if (blocker->client->ps.forcePower < BLOCKPOINTS_FATIGUE)
				{
					//Low points = bad blocks
					WP_BoltBlockVictimFatigued(blocker);
					blocker->client->ps.saberBlocked = BLOCKED_NONE;
					blocker->client->ps.saberBounceMove = LS_NONE;
				}
				else
				{
					WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
				}
			}
			else
			{
				WP_SaberBlockBolt_MD(blocker, missile->currentOrigin, qfalse);
			}

			const int force_points_used_used = WP_SaberBoltBlockCost(blocker, missile);

			if (blocker->client->ps.forcePower < force_points_used_used)
			{
				blocker->client->ps.forcePower = 0;
			}
			else
			{
				WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, force_points_used_used);
			}

			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_CYAN"JKA Mode G_ReflectMissileNPC\n");
			}
			reflected = qtrue;
		}
	}

	if (!reflected) //GOES TO CROSSHAIR AREA
	{
		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_CYAN"Saber Not Blocked but Reflected off saber.\n");
		}

		if (blocker && blocker->s.client_num >= MAX_CLIENTS)
		{
			if (missile->owner && missile->s.weapon != WP_SABER)
			{
				//bounce back at them if you can
				VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
				VectorNormalize(bounce_dir);
			}
			else
			{
				vec3_t missile_dir;

				VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
				VectorCopy(missile->s.pos.trDelta, bounce_dir);
				VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
				VectorNormalize(bounce_dir);
			}
		}
		else //deflect off at an angle.
		{
			vec3_t deflect_dir, missile_dir;
			float force_factor;
			VectorSubtract(g_crosshairWorldCoord, missile->currentOrigin, deflect_dir);
			VectorCopy(missile->s.pos.trDelta, missile_dir);
			VectorNormalize(missile_dir);
			VectorNormalize(deflect_dir);

			//bigger forceFactors make the reflected shots go closer to the crosshair
			switch (blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE])
			{
			case FORCE_LEVEL_1:
				force_factor = 2.0f;
				break;
			case FORCE_LEVEL_2:
				force_factor = 3.0f;
				break;
			default:
				force_factor = 10.0f;
				break;
			}

			VectorMA(missile_dir, force_factor, deflect_dir, bounce_dir);

			VectorNormalize(bounce_dir);
		}
		if (blocker->s.weapon == WP_SABER && blocker->client)
		{
			//saber
			if (blocker->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1 or below
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
				}
			}
			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
			}
		}
		WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, punish);
	}

	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(
		!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]
		));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = blocker;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

static qhandle_t stasisLoopSound = 0;
gentity_t* tgt_list[MAX_GENTITIES];
void G_StasisMissile(gentity_t* ent, gentity_t* missile)
{
	vec3_t	bounce_dir;
	gentity_t* blocker = ent;
	static qboolean registered = qfalse;

	if (!registered)
	{
		stasisLoopSound = G_SoundIndex("sound/effects/blaster_stasis_loop.wav");
		registered = qtrue;
	}

	if (ent->owner)
	{
		blocker = ent->owner;
	}
	const qboolean missile_in_stasis = blocker->client->ps.ManualBlockingFlags & 1 << MBF_MISSILESTASIS ? qtrue : qfalse;

	//save the original speed
	const float stasisspeed = VectorNormalize(missile->s.pos.trDelta) / 50;
	const float normalspeed = VectorNormalize(missile->s.pos.trDelta) / 25;

	if (ent &&
		blocker &&
		blocker->client)
	{
		gentity_t* enemy;

		if (blocker->enemy && Q_irand(0, 3))
		{//toward current enemy 75% of the time
			enemy = blocker->enemy;
		}
		else
		{//find another enemy
			enemy = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}
		if (enemy)
		{
			vec3_t	bullseye;
			CalcEntitySpot(enemy, SPOT_CHEST, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			for (int i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
			}

			VectorNormalize(bounce_dir);
		}
	}
	VectorNormalize(bounce_dir);
	missile->s.loopSound = stasisLoopSound;

	if (missile_in_stasis)
	{
		VectorScale(bounce_dir, stasisspeed, missile->s.pos.trDelta);

#ifdef _DEBUG
		assert(!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]));
#endif// _DEBUG
		missile->s.pos.trTime = level.time - 10;		// move a bit on the very first frame
		VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
		if (missile->s.weapon != WP_SABER)
		{//you are mine, now!
			if (!missile->lastEnemy)
			{//remember who originally shot this missile
				missile->lastEnemy = missile->owner;
			}
			missile->owner = blocker;
		}

		if (missile->s.weapon == WP_ROCKET_LAUNCHER || missile->s.weapon == WP_THERMAL)
		{//stop homing
			qboolean	blow = qfalse;
			if (missile->delay > level.time)
			{
				const int count = G_RadiusList(missile->currentOrigin, 200, missile, qtrue, tgt_list);

				for (int i = 0; i < count; i++)
				{
					if (tgt_list[i]->client && tgt_list[i]->health > 0 && missile->activator && tgt_list[i]->s.number != missile->activator->s.number)
					{
						blow = qtrue;
						break;
					}
				}
			}
			else
			{
				// well, we must die now
				blow = qtrue;
			}

			if (blow)
			{
				missile->e_ThinkFunc = thinkF_WP_flechette_alt_blow;
				missile->nextthink = level.time + 2000;
			}
			else
			{
				//stop homing
				missile->e_ThinkFunc = thinkF_NULL;
			}
		}
	}
	else
	{
		VectorScale(bounce_dir, normalspeed, missile->s.pos.trDelta);

#ifdef _DEBUG
		assert(!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]));
#endif// _DEBUG
		missile->s.pos.trTime = level.time - 10;		// move a bit on the very first frame
		VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
		if (missile->s.weapon != WP_SABER)
		{//you are mine, now!
			if (!missile->lastEnemy)
			{//remember who originally shot this missile
				missile->lastEnemy = missile->owner;
			}
			missile->owner = blocker;
		}
		if (missile->s.weapon == WP_ROCKET_LAUNCHER || missile->s.weapon == WP_THERMAL)
		{//stop homing
			qboolean	blow = qfalse;
			if (missile->delay > level.time)
			{
				const int count = G_RadiusList(missile->currentOrigin, 200, missile, qtrue, tgt_list);

				for (int i = 0; i < count; i++)
				{
					if (tgt_list[i]->client && tgt_list[i]->health > 0 && missile->activator && tgt_list[i]->s.number != missile->activator->s.number)
					{
						blow = qtrue;
						break;
					}
				}
			}
			else
			{
				// well, we must die now
				blow = qtrue;
			}

			if (blow)
			{
				missile->e_ThinkFunc = thinkF_WP_flechette_alt_blow;
				missile->nextthink = level.time + 2000;
			}
			else
			{
				//stop homing
				missile->e_ThinkFunc = thinkF_NULL;
			}
		}
	}
}

void G_ReflectMissile_JKA(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean reflected = qfalse;
	gentity_t* owner = ent;

	if (ent->owner)
	{
		owner = ent->owner;
	}

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	if (ent && owner && owner->client && !owner->client->ps.saberInFlight &&
		(owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] > FORCE_LEVEL_2 || owner->client->ps.forcePowerLevel[
			FP_SABER_DEFENSE] > FORCE_LEVEL_1 && !Q_irand(0, 3)))
	{
		//if high enough defense skill and saber in-hand (100% at level 3, 25% at level 2, 0% at level 1), reflections are perfectly deflected toward an enemy
		gentity_t* enemy;
		if (owner->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			enemy = owner->enemy;
		}
		else
		{
			//find another enemy
			enemy = jedi_find_enemy_in_cone(owner, owner->enemy, 0.3f);
		}
		if (enemy)
		{
			vec3_t bullseye;
			CalcEntitySpot(enemy, SPOT_HEAD, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}
			VectorNormalize(bounce_dir);
			reflected = qtrue;
		}
	}
	if (!reflected)
	{
		if (missile->owner && missile->s.weapon != WP_SABER)
		{
			//bounce back at them if you can
			VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);
		}
		else
		{
			vec3_t missile_dir;

			VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
			VectorCopy(missile->s.pos.trDelta, bounce_dir);
			VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
			VectorNormalize(bounce_dir);
		}
		if (owner && owner->client && owner->s.weapon == WP_SABER)
		{
			//saber
			if (owner->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
				}
			}
			if (!PM_SaberInParry(owner->client->ps.saber_move)
				&& !PM_SaberInReflect(owner->client->ps.saber_move)
				&& !PM_SaberInIdle(owner->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(owner->client->ps.saber_move)
					|| PM_SaberInTransitionAny(owner->client->ps.saber_move)
					|| pm_saber_in_special_attack(owner->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
			}
		}
	}
	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(
		!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]
		));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = owner;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

/*
================
G_BounceRollMissile

================
*/
static void G_BounceRollMissile(gentity_t* ent, const trace_t* trace)
{
	vec3_t velocity, normal;

	// reflect the velocity on the trace plane
	const int hit_time = level.previousTime + (level.time - level.previousTime) * trace->fraction;
	EvaluateTrajectoryDelta(&ent->s.pos, hit_time, velocity);
	const float velocity_z = velocity[2];
	velocity[2] = 0;
	const float speed_xy = VectorLength(velocity); //friction
	VectorCopy(trace->plane.normal, normal);
	const float normalZ = normal[2];
	normal[2] = 0;
	float dot = DotProduct(velocity, normal);
	VectorMA(velocity, -2 * dot, normal, ent->s.pos.trDelta);
	VectorSet(velocity, 0, 0, velocity_z);
	VectorSet(normal, 0, 0, normalZ);
	dot = DotProduct(velocity, normal) * -1;
	if (dot > 10)
	{
		ent->s.pos.trDelta[2] = dot * 0.3f; //not very bouncy
	}
	else
	{
		ent->s.pos.trDelta[2] = 0;
	}

	// check for stop
	if (speed_xy <= 0)
	{
		G_SetOrigin(ent, trace->endpos);
		VectorCopy(ent->currentAngles, ent->s.apos.trBase);
		VectorClear(ent->s.apos.trDelta);
		ent->s.apos.trType = TR_STATIONARY;
		return;
	}

	VectorCopy(ent->currentAngles, ent->s.apos.trBase);
	VectorCopy(ent->s.pos.trDelta, ent->s.apos.trDelta);

	VectorCopy(trace->endpos, ent->currentOrigin);
	ent->s.pos.trTime = hit_time - 10;
	VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
}

/*
================
G_BounceMissile

================
*/
void G_BounceMissile(gentity_t* ent, trace_t* trace)
{
	vec3_t velocity;

	// reflect the velocity on the trace plane
	const int hitTime = level.previousTime + (level.time - level.previousTime) * trace->fraction;
	EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity);
	const float dot = DotProduct(velocity, trace->plane.normal);
	VectorMA(velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta);

	if (ent->s.eFlags & EF_BOUNCE_SHRAPNEL)
	{
		VectorScale(ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta);
		ent->s.pos.trType = TR_GRAVITY;

		// check for stop
		if (trace->plane.normal[2] > 0.7 && ent->s.pos.trDelta[2] < 40)
			//this can happen even on very slightly sloped walls, so changed it from > 0 to > 0.7
		{
			G_SetOrigin(ent, trace->endpos);
			ent->nextthink = level.time + 100;
			return;
		}
	}
	else if (ent->s.eFlags & EF_BOUNCE_HALF)
	{
		VectorScale(ent->s.pos.trDelta, 0.5, ent->s.pos.trDelta);

		// check for stop
		if (trace->plane.normal[2] > 0.7 && ent->s.pos.trDelta[2] < 40)
			//this can happen even on very slightly sloped walls, so changed it from > 0 to > 0.7
		{
			if (ent->s.weapon == WP_THERMAL)
			{
				//roll when you "stop"
				ent->s.pos.trType = TR_INTERPOLATE;
			}
			else
			{
				G_SetOrigin(ent, trace->endpos);
				ent->nextthink = level.time + 500;
				return;
			}
		}

		if (ent->s.weapon == WP_THERMAL)
		{
			ent->has_bounced = qtrue;
		}
	}

#if 0
	// OLD--this looks so wrong.  It looked wrong in EF.  It just must be wrong.
	VectorAdd(ent->currentOrigin, trace->plane.normal, ent->currentOrigin);

	ent->s.pos.trTime = level.time - 10;
#else
	// NEW--It would seem that we want to set our trBase to the trace endpos
	//	and set the trTime to the actual time of impact....
	VectorAdd(trace->endpos, trace->plane.normal, ent->currentOrigin);
	if (hitTime >= level.time)
	{
		//trace fraction must have been 1
		ent->s.pos.trTime = level.time - 10;
	}
	else
	{
		ent->s.pos.trTime = hitTime - 10;
		// this is kinda dumb hacking, but it pushes the missile away from the impact plane a bit
	}
#endif

	VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
	VectorCopy(trace->plane.normal, ent->pos1);

	if (ent->s.weapon != WP_SABER
		&& ent->s.weapon != WP_THERMAL
		&& ent->e_clThinkFunc != clThinkF_CG_Limb
		&& ent->e_ThinkFunc != thinkF_LimbThink)
	{
		//not a saber, bouncing thermal or limb
		//now you can damage the guy you came from
		ent->owner = nullptr;
	}
}

/*
================
G_MissileImpact

================
*/

void NoghriGasCloudThink(gentity_t* self)
{
	self->nextthink = level.time + FRAMETIME;

	AddSightEvent(self->owner, self->currentOrigin, 200, AEL_DANGER, 50);

	if (self->fx_time < level.time)
	{
		constexpr vec3_t up = { 0, 0, 1 };
		G_PlayEffect("noghri_stick/gas_cloud", self->currentOrigin, up);
		self->fx_time = level.time + 250;
	}

	if (level.time - self->s.time <= 2500)
	{
		if (!Q_irand(0, 3 - g_spskill->integer))
		{
			G_RadiusDamage(self->currentOrigin, self->owner, Q_irand(1, 4), self->splashRadius,
				self->owner, self->splashMethodOfDeath);
		}
	}

	if (level.time - self->s.time > 3000)
	{
		G_FreeEntity(self);
	}
}

static void G_SpawnNoghriGasCloud(gentity_t* ent)
{
	ent->freeAfterEvent = qfalse;
	ent->e_TouchFunc = touchF_NULL;

	G_SetOrigin(ent, ent->currentOrigin);
	ent->e_ThinkFunc = thinkF_NoghriGasCloudThink;
	ent->nextthink = level.time + FRAMETIME;

	constexpr vec3_t up = { 0, 0, 1 };
	G_PlayEffect("noghri_stick/gas_cloud", ent->currentOrigin, up);

	ent->fx_time = level.time + 250;
	ent->s.time = level.time;
}

extern qboolean W_AccuracyLoggableWeapon(int weapon, qboolean alt_fire, int mod);

void G_MissileImpacted(gentity_t* ent, gentity_t* other, vec3_t impact_pos, vec3_t normal, const int hit_loc = HL_NONE)
{
	// impact damage
	if (other->takedamage)
	{
		if (ent->damage)
		{
			vec3_t velocity;

			EvaluateTrajectoryDelta(&ent->s.pos, level.time, velocity);
			if (VectorLength(velocity) == 0)
			{
				velocity[2] = 1; // stepped on a grenade
			}

			const int damage = ent->damage;

			if (other->client)
			{
				const class_t npc_class = other->client->NPC_class;

				// If we are a robot and we aren't currently doing the full body electricity...
				if (npc_class == CLASS_ATST || npc_class == CLASS_GONK ||
					npc_class == CLASS_INTERROGATOR || npc_class == CLASS_MARK1 ||
					npc_class == CLASS_MARK2 || npc_class == CLASS_MOUSE ||
					npc_class == CLASS_PROBE || npc_class == CLASS_PROTOCOL ||
					npc_class == CLASS_R2D2 || npc_class == CLASS_R5D2 ||
					npc_class == CLASS_SEEKER || npc_class == CLASS_SENTRY ||
					//npc_class == CLASS_SBD || npc_class == CLASS_BATTLEDROID ||
					/*npc_class == CLASS_DROIDEKA ||*/ npc_class == CLASS_OBJECT ||
					npc_class == CLASS_ASSASSIN_DROID || npc_class == CLASS_SABER_DROID)
				{
					// special droid only behaviors
					if (other->client->ps.powerups[PW_SHOCKED] < level.time + 100)
					{
						// ... do the effect for a split second for some more feedback
						other->s.powerups |= 1 << PW_SHOCKED;
						other->client->ps.powerups[PW_SHOCKED] = level.time + Q_irand(1500, 2000);
					}
				}
			}

			G_Damage(other, ent, ent->owner, velocity, impact_pos, damage, ent->dflags, ent->methodOfDeath, hit_loc);

			if (ent->s.weapon == WP_DEMP2)
			{
				//a hit with demp2 decloaks saboteurs
				if (other && other->client && other->client->NPC_class == CLASS_SABOTEUR)
				{
					//FIXME: make this disabled cloak hold for some amount of time?
					Saboteur_Decloak(other, Q_irand(3000, 10000));
					if (ent->methodOfDeath == MOD_DEMP2_ALT)
					{
						//direct hit with alt disabled cloak forever
						if (other->NPC)
						{
							//permanently disable the saboteur's cloak
							other->NPC->aiFlags &= ~NPCAI_SHIELDS;
						}
					}
				}
				if (other && other->client && other->client->NPC_class == CLASS_VEHICLE)
				{
					//hit a vehicle
					if (other->m_pVehicle //valid vehicle ent
						&& other->m_pVehicle->m_pVehicleInfo //valid stats
						&& (other->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER //always affect speeders
							|| other->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER && ent->classname &&
							Q_stricmp("vehicle_proj", ent->classname) == 0)
						//only vehicle ion weapons affect a fighter in this manner
						&& !FighterIsLanded(other->m_pVehicle, &other->client->ps) //not landed
						&& !(other->spawnflags & 2)) //and not suspended
					{
						//vehicles hit by "ion cannons" lose control
						if (other->client->ps.electrifyTime > level.time)
						{
							//add onto it
							other->client->ps.electrifyTime += level.time + Q_irand(1500, 2000);
							if (other->client->ps.electrifyTime > level.time + 4000)
							{
								//cap it
								other->client->ps.electrifyTime = level.time + 4000;
							}
						}
						else
						{
							//start it
							other->client->ps.electrifyTime = level.time + Q_irand(1500, 2000);
						}
					}
				}
				else if (other && other->client && other->client->ps.powerups[PW_CLOAKED])
				{
					player_decloak(other);
					jedi_decloak(other);
					if (ent->methodOfDeath == MOD_DEMP2_ALT)
					{
						//direct hit with alt disables cloak forever //permanently disable the saboteur's cloak
						other->client->cloakToggleTime = Q3_INFINITE;
					}
					else
					{
						//temp disable
						other->client->cloakToggleTime = level.time + Q_irand(3000, 10000);
					}
				}
			}
		}
	}

	if (other && other->client && other->takedamage)
	{
		G_AddEvent(ent, EV_MISSILE_HIT, DirToByte(normal));
		ent->s.otherentity_num = other->s.number;
	}
	else
	{
		G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(normal));
		ent->s.otherentity_num = other->s.number;
	}

	VectorCopy(normal, ent->pos1);

	if (ent->owner)
	{
		//Add the event
		AddSoundEvent(ent->owner, ent->currentOrigin, 256, AEL_SUSPICIOUS, qfalse, qtrue);
		AddSightEvent(ent->owner, ent->currentOrigin, 512, AEL_DISCOVERED, 75);
	}

	ent->freeAfterEvent = qtrue;

	// change over to a normal entity right at the point of impact
	ent->s.eType = ET_GENERAL;

	VectorCopy(impact_pos, ent->s.pos.trBase);

	G_SetOrigin(ent, impact_pos);

	// splash damage (doesn't apply to person directly hit)
	if (ent->splashDamage)
	{
		G_RadiusDamage(impact_pos, ent->owner, ent->splashDamage, ent->splashRadius,
			other, ent->splashMethodOfDeath);
	}

	if (ent->s.weapon == WP_NOGHRI_STICK)
	{
		G_SpawnNoghriGasCloud(ent);
	}

	gi.linkentity(ent);
}

//------------------------------------------------
static void G_MissileAddAlerts(gentity_t* ent)
{
	//Add the event
	if (ent->s.weapon == WP_THERMAL && (ent->delay - level.time < 2000 || ent->s.pos.trType == TR_INTERPOLATE))
	{
		//a thermal about to explode or rolling
		if (ent->delay - level.time < 500)
		{
			//half a second before it explodes!
			AddSoundEvent(ent->owner, ent->currentOrigin, ent->splashRadius * 2, AEL_DANGER_GREAT, qfalse, qtrue);
			AddSightEvent(ent->owner, ent->currentOrigin, ent->splashRadius * 2, AEL_DANGER_GREAT, 20);
		}
		else
		{
			//2 seconds until it explodes or it's rolling
			AddSoundEvent(ent->owner, ent->currentOrigin, ent->splashRadius * 2, AEL_DANGER, qfalse, qtrue);
			AddSightEvent(ent->owner, ent->currentOrigin, ent->splashRadius * 2, AEL_DANGER, 20);
		}
	}
	else
	{
		AddSoundEvent(ent->owner, ent->currentOrigin, 128, AEL_DISCOVERED);
		AddSightEvent(ent->owner, ent->currentOrigin, 256, AEL_DISCOVERED, 40);
	}
}

//------------------------------------------------------
static void G_MissileImpact_MD(gentity_t* ent, trace_t* trace, const int hit_loc = HL_NONE)
{
	vec3_t diff;

	gentity_t* other = &g_entities[trace->entityNum];

	if (other == ent)
	{
		assert(0 && "missile hit itself!!!");
		return;
	}
	if (trace->plane.normal[0] == 0.0f &&
		trace->plane.normal[1] == 0.0f &&
		trace->plane.normal[2] == 0.0f)
	{
		//model moved into missile in flight probably...
		trace->plane.normal[0] = -ent->s.pos.trDelta[0];
		trace->plane.normal[1] = -ent->s.pos.trDelta[1];
		trace->plane.normal[2] = -ent->s.pos.trDelta[2];
		VectorNormalize(trace->plane.normal);
	}

	if (ent->owner && (other->takedamage || other->client))
	{
		if (!ent->lastEnemy || ent->lastEnemy == ent->owner)
		{
			//a missile that was not reflected or, if so, still is owned by original owner
			if (LogAccuracyHit(other, ent->owner))
			{
				ent->owner->client->ps.persistant[PERS_ACCURACY_HITS]++;
			}
			if (ent->owner->client && !ent->owner->s.number)
			{
				if (W_AccuracyLoggableWeapon(ent->s.weapon, qfalse, ent->methodOfDeath))
				{
					ent->owner->client->sess.missionStats.hits++;
				}
			}
		}
	}

	// check for bounce
	auto bounce = static_cast<qboolean>(!other->takedamage && ent->s.eFlags & (EF_BOUNCE | EF_BOUNCE_HALF)
		|| (trace->surfaceFlags & SURF_FORCEFIELD || other->flags & FL_SHIELDED)
		&& !ent->splashDamage && !ent->splashRadius && ent->s.weapon != WP_NOGHRI_STICK);

	auto beskar = static_cast<qboolean>((other->flags & FL_DINDJARIN)
		&& !ent->splashDamage
		&& !ent->splashRadius
		&& ent->methodOfDeath != MOD_SABER
		&& ent->methodOfDeath != MOD_REPEATER_ALT
		&& ent->methodOfDeath != MOD_FLECHETTE_ALT
		&& ent->methodOfDeath != MOD_ROCKET
		&& ent->methodOfDeath != MOD_ROCKET_ALT
		&& ent->methodOfDeath != MOD_CONC_ALT
		&& ent->methodOfDeath != MOD_THERMAL
		&& ent->methodOfDeath != MOD_THERMAL_ALT
		&& ent->methodOfDeath != MOD_DEMP2
		&& ent->methodOfDeath != MOD_DEMP2_ALT
		&& ent->methodOfDeath != MOD_EXPLOSIVE
		&& ent->methodOfDeath != MOD_DETPACK
		&& ent->methodOfDeath != MOD_LASERTRIP
		&& ent->methodOfDeath != MOD_LASERTRIP_ALT
		&& ent->methodOfDeath != MOD_SEEKER
		&& ent->methodOfDeath != MOD_CONC
		&& ent->methodOfDeath != WP_NOGHRI_STICK
		&& (!Q_irand(0, 1)));

	auto boba_fett = static_cast<qboolean>((other->flags & FL_BOBAFETT)
		&& !ent->splashDamage
		&& !ent->splashRadius
		&& ent->methodOfDeath != MOD_SABER
		&& ent->methodOfDeath != MOD_REPEATER_ALT
		&& ent->methodOfDeath != MOD_FLECHETTE_ALT
		&& ent->methodOfDeath != MOD_ROCKET
		&& ent->methodOfDeath != MOD_ROCKET_ALT
		&& ent->methodOfDeath != MOD_CONC_ALT
		&& ent->methodOfDeath != MOD_THERMAL
		&& ent->methodOfDeath != MOD_THERMAL_ALT
		&& ent->methodOfDeath != MOD_DEMP2
		&& ent->methodOfDeath != MOD_DEMP2_ALT
		&& ent->methodOfDeath != MOD_EXPLOSIVE
		&& ent->methodOfDeath != MOD_DETPACK
		&& ent->methodOfDeath != MOD_LASERTRIP
		&& ent->methodOfDeath != MOD_LASERTRIP_ALT
		&& ent->methodOfDeath != MOD_SEEKER
		&& ent->methodOfDeath != MOD_CONC
		&& ent->methodOfDeath != WP_NOGHRI_STICK);

	if (ent->dflags & DAMAGE_HEAVY_WEAP_CLASS)
	{
		// heavy class missiles generally never bounce.
		bounce = qfalse;
		beskar = qfalse;
		boba_fett = qfalse;
	}

	if (other->flags & (FL_DMG_BY_HEAVY_WEAP_ONLY | FL_SHIELDED))
	{
		if (strcmp("misc_ion_cannon", other->classname) == 0 && other->flags & FL_SHIELDED)
		{
			// Anything will bounce off of us.
			bounce = qtrue;
			// Not exactly the debounce time, but rather the impact time for the shield effect...play effect for 1 second
			other->painDebounceTime = level.time + 1000;
		}
	}

	if (ent->s.weapon == WP_DEMP2)
	{
		// demp2 shots can never bounce
		bounce = qfalse;
		beskar = qfalse;
		boba_fett = qfalse;
		// in fact, alt-charge shots will not call the regular impact functions
		if (ent->alt_fire)
		{
			// detonate at the trace end
			VectorCopy(trace->endpos, ent->currentOrigin);
			VectorCopy(trace->plane.normal, ent->pos1);
			DEMP2_AltDetonate(ent);
			return;
		}
	}

	if (beskar || boba_fett)
	{
		bounce = qfalse;
		// Check to see if there is a bounce count
		if (ent->bounceCount)
		{
			// decrement number of bounces and then see if it should be done bouncing
			if (!--ent->bounceCount)
			{
				// He (or she) will bounce no more (after this current bounce, that is).
				ent->s.eFlags &= ~(EF_BOUNCE | EF_BOUNCE_HALF);
			}
		}

		G_BounceMissile(ent, trace);
		NPC_SetAnim(other, SETANIM_TORSO, Q_irand(BOTH_PAIN1, BOTH_PAIN3), SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

		if (ent->owner)
		{
			G_MissileAddAlerts(ent);
		}
		G_MissileBounceBeskarEffect(ent, trace->plane.normal);
		return;
	}

	if (bounce)
	{
		// Check to see if there is a bounce count
		if (ent->bounceCount)
		{
			// decrement number of bounces and then see if it should be done bouncing
			if (!--ent->bounceCount)
			{
				// He (or she) will bounce no more (after this current bounce, that is).
				ent->s.eFlags &= ~(EF_BOUNCE | EF_BOUNCE_HALF);
			}
		}

		if (other->NPC)
		{
			G_Damage(other, ent, ent->owner, ent->currentOrigin, ent->s.pos.trDelta, 0, DAMAGE_NO_DAMAGE, MOD_UNKNOWN);
		}

		G_BounceMissile(ent, trace);

		if (ent->owner)
		{
			G_MissileAddAlerts(ent);
		}
		G_MissileBounceEffect(ent, trace->endpos, trace->plane.normal,
			static_cast<qboolean>(trace->entityNum == ENTITYNUM_WORLD));

		return;
	}

	if (other->NPC)
	{
		if (!other->takedamage && ent->s.eFlags & EF_BOUNCE_SHRAPNEL
			|| trace->surfaceFlags & SURF_FORCEFIELD && !ent->splashDamage && !ent->splashRadius)
		{
			if (!(other->contents & CONTENTS_LIGHTSABER)
				|| g_spskill->integer <= 0 //on easy, it reflects all shots
				|| g_spskill->integer == 1 && ent->s.weapon != WP_FLECHETTE && ent->s.weapon != WP_DEMP2
				//on medium it won't reflect flechette or demp shots
				|| g_spskill->integer == 2 && ent->s.weapon != WP_FLECHETTE && ent->s.weapon != WP_DEMP2 && ent->s.
				weapon != WP_BOWCASTER && ent->s.weapon != WP_REPEATER)
				//on hard it won't reflect flechette, demp, repeater or bowcaster shots
			{
				G_BounceMissile(ent, trace);

				if (--ent->bounceCount < 0)
				{
					ent->s.eFlags &= ~EF_BOUNCE_SHRAPNEL;
				}
				G_MissileBounceEffect(ent, trace->endpos, trace->plane.normal,
					static_cast<qboolean>(trace->entityNum == ENTITYNUM_WORLD));
				return;
			}
		}
	}
	else
	{
		if (!other->takedamage && ent->s.eFlags & EF_BOUNCE_SHRAPNEL
			|| trace->surfaceFlags & SURF_FORCEFIELD && !ent->splashDamage && !ent->splashRadius)
		{
			if (!(other->contents & CONTENTS_LIGHTSABER))
			{
				G_BounceMissile(ent, trace);

				if (--ent->bounceCount < 0)
				{
					ent->s.eFlags &= ~EF_BOUNCE_SHRAPNEL;
				}
				G_MissileBounceEffect(ent, trace->endpos, trace->plane.normal,
					static_cast<qboolean>(trace->entityNum == ENTITYNUM_WORLD));
				return;
			}
		}
	}

	if ((!other->takedamage || other->client && other->health <= 0)
		&& ent->s.weapon == WP_THERMAL
		&& !ent->alt_fire)
	{
		//rolling thermal det
		if (ent->owner)
		{
			G_MissileAddAlerts(ent);
		}
		return;
	}

	// check for sticking
	if (ent->s.eFlags & EF_MISSILE_STICK)
	{
		if (ent->owner)
		{
			//Add the event
			if (ent->s.weapon == WP_TRIP_MINE)
			{
				AddSoundEvent(ent->owner, ent->currentOrigin, ent->splashRadius / static_cast<float>(2), AEL_DISCOVERED, qfalse, qtrue);
				AddSightEvent(ent->owner, ent->currentOrigin, ent->splashRadius * 2, AEL_DISCOVERED, 60);
			}
			else
			{
				AddSoundEvent(ent->owner, ent->currentOrigin, 128, AEL_DISCOVERED, qfalse, qtrue);
				AddSightEvent(ent->owner, ent->currentOrigin, 256, AEL_DISCOVERED, 10);
			}
		}

		G_MissileStick(ent, other, trace);
		return;
	}

	if (strcmp(ent->classname, "stun") == 0)
	{
		vec3_t v{};
		gentity_t* nent = G_Spawn();

		if (other->client || other->s.eType == ET_MOVER)
		{
			G_PlayEffect("stunBaton/flesh_impact", ent->currentOrigin);
			nent->s.otherentity_num2 = other->s.number;
			ent->enemy = other;

			if (other->takedamage && other->client)
			{
				other->client->stunDamage = 20;
				other->client->stunTime = level.time + 500;
				if (other->client->ps.powerups[PW_SHOCKED] < level.time + 100)
				{
					// ... do the effect for a split second for some more feedback
					other->s.powerups |= 1 << PW_SHOCKED;
					other->client->ps.powerups[PW_SHOCKED] = level.time + 6000;
				}
				G_Damage(other, ent, ent, v, ent->currentOrigin, TASER_DAMAGE, DAMAGE_NO_KNOCKBACK, MOD_ELECTROCUTE);

				if (other->client->ps.stats[STAT_HEALTH] <= 0) // if we are dead
				{
					vec3_t spot;

					VectorCopy(other->currentOrigin, spot);

					other->flags |= FL_DISINTEGRATED;
					other->svFlags |= SVF_BROADCAST;
					gentity_t* tent = G_TempEntity(spot, EV_DISINTEGRATION);
					tent->s.eventParm = PW_DISRUPTION;
					tent->svFlags |= SVF_BROADCAST;
					tent->owner = other;

					if (other->playerModel >= 0)
					{
						// don't let 'em animate
						gi.G2API_PauseBoneAnimIndex(&other->ghoul2[ent->playerModel], other->rootBone, level.time);
						gi.G2API_PauseBoneAnimIndex(&other->ghoul2[ent->playerModel], other->motionBone, level.time);
						gi.G2API_PauseBoneAnimIndex(&other->ghoul2[ent->playerModel], other->lowerLumbarBone,
							level.time);
					}

					//not solid anymore
					other->contents = 0;
					other->maxs[2] = -8;

					//need to pad deathtime some to stick around long enough for death effect to play
					other->NPC->timeOfDeath = level.time + 2000;
				}
				else
				{
					G_BlastDown(other, ent, v, 25);
				}
			}
		}
		else
		{
			VectorCopy(trace->endpos, v);
			G_PlayEffect("impacts/droid_impact1", ent->currentOrigin);
			ent->enemy = nullptr;
		}

		nent->freeAfterEvent = qtrue;
		nent->s.eType = ET_GENERAL;
		ent->s.eType = ET_GENERAL;

		G_SetOrigin(ent, v);
		G_SetOrigin(nent, v);

		gi.linkentity(ent);
		gi.linkentity(nent);

		return;
	}

	if (strcmp(ent->classname, "hook") == 0)
	{
		vec3_t v{};
		gentity_t* nent = G_Spawn();

		if (other->client || other->s.eType == ET_MOVER)
		{
			G_PlayEffect("blaster/flesh_impact", ent->currentOrigin);
			nent->s.otherentity_num2 = other->s.number;
			ent->enemy = other;

			if (other->takedamage && other->client)
			{
				G_Damage(other, ent, ent, v, ent->currentOrigin, TASER_DAMAGE, DAMAGE_NO_KNOCKBACK, MOD_IMPACT);

				GEntity_PainFunc(other, ent, ent, other->currentOrigin, 0, MOD_IMPACT);
			}
			nent->s.otherentity_num2 = other->s.number;

			ent->enemy = other;
			v[0] = other->currentOrigin[0] + (other->mins[0] + other->maxs[0]) * 0.5f;
			v[1] = other->currentOrigin[1] + (other->mins[1] + other->maxs[1]) * 0.5f;
			v[2] = other->currentOrigin[2] + (other->mins[2] + other->maxs[2]) * 0.5f;
			SnapVectorTowards(v, ent->s.pos.trBase); // save net bandwidth
		}
		else
		{
			VectorCopy(trace->endpos, v);
			G_PlayEffect("impacts/droid_impact1", ent->currentOrigin);
			ent->enemy = nullptr;
		}

		SnapVectorTowards(v, ent->s.pos.trBase);

		nent->freeAfterEvent = qtrue;
		nent->s.eType = ET_GENERAL;
		ent->s.eType = ET_GENERAL;

		G_SetOrigin(ent, v);
		G_SetOrigin(nent, v);

		ent->e_ThinkFunc = thinkF_Weapon_HookThink;
		ent->nextthink = level.time + FRAMETIME;

		if (!other->takedamage)
		{
			ent->parent->client->ps.pm_flags |= PMF_GRAPPLE_PULL;
			VectorCopy(ent->currentOrigin, ent->parent->client->ps.lastHitLoc);
		}

		gi.linkentity(ent);
		gi.linkentity(nent);

		return;
	}

	// check for hitting a lightsaber
	if (WP_SaberMustBoltBlock(other, ent, qfalse, trace->endpos, -1, -1)
		&& !WP_DoingForcedAnimationForForcePowers(other))
	{
		//play projectile block animation
		if (other->client && !PM_SaberInAttack(other->client->ps.saber_move)
			|| other->client && (pm->cmd.buttons & BUTTON_USE_FORCE
				|| pm->cmd.buttons & BUTTON_FORCEGRIP
				|| pm->cmd.buttons & BUTTON_DASH
				|| pm->cmd.buttons & BUTTON_LIGHTNING_STRIKE
				|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING))
		{
			other->client->ps.weaponTime = 0;
		}

		VectorSubtract(ent->currentOrigin, other->currentOrigin, diff);
		VectorNormalize(diff);

		WP_HandleBoltBlock(other, ent, diff);

		if (g_SerenityJediEngineMode->integer == 2)
		{
			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_RED"Advanced MD Mode HandleBoltBlock Blocking\n");
			}
		}
		else
		{
			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_RED"MD Mode HandleBoltBlock Blocking\n");
			}
		}

		if (other->owner && !other->owner->s.number && other->owner->client)
		{
			other->owner->client->sess.missionStats.saberBlocksCnt++;
		}

		if (other->owner && other->owner->client)
		{
			other->owner->client->ps.saberEventFlags |= SEF_DEFLECTED;
		}

		//do the effect
		VectorCopy(ent->s.pos.trDelta, diff);
		VectorNormalize(diff);
		G_MissileReflectEffect(ent, trace->plane.normal);

		return;
	}
	if (other->contents & CONTENTS_LIGHTSABER)
	{
		//hit this person's saber, so..
		if (other->owner && !other->owner->s.number && other->owner->client)
		{
			other->owner->client->sess.missionStats.saberBlocksCnt++;
		}

		if (other->owner->takedamage && other->owner->client &&
			(!ent->splashDamage || !ent->splashRadius) &&
			ent->s.weapon != WP_ROCKET_LAUNCHER &&
			ent->s.weapon != WP_THERMAL &&
			ent->s.weapon != WP_TRIP_MINE &&
			ent->s.weapon != WP_DET_PACK &&
			ent->s.weapon != WP_NOGHRI_STICK &&
			ent->methodOfDeath != MOD_REPEATER_ALT &&
			ent->methodOfDeath != MOD_FLECHETTE_ALT &&
			ent->methodOfDeath != MOD_CONC &&
			ent->methodOfDeath != MOD_CONC_ALT)
		{
			if (!other->owner || !other->owner->client || other->owner->client->ps.saberInFlight
				|| InFront(ent->currentOrigin, other->owner->currentOrigin, other->owner->client->ps.viewangles, SABER_REFLECT_MISSILE_CONE)
				&& !WP_DoingForcedAnimationForForcePowers(other))
			{
				//Jedi cannot block shots from behind!
				VectorSubtract(ent->currentOrigin, other->currentOrigin, diff);
				VectorNormalize(diff);

				if (other->owner->client && !PM_SaberInAttack(other->owner->client->ps.saber_move)
					|| other->owner->client && (pm->cmd.buttons & BUTTON_USE_FORCE
						|| pm->cmd.buttons & BUTTON_FORCEGRIP
						|| pm->cmd.buttons & BUTTON_DASH
						|| pm->cmd.buttons & BUTTON_LIGHTNING_STRIKE
						|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING))
				{
					other->owner->client->ps.weaponTime = 0;
				}

				WP_HandleBoltBlock(other, ent, diff);

				if (g_SerenityJediEngineMode->integer == 2 || g_DebugSaberCombat->integer)
				{
					if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						Com_Printf(S_COLOR_ORANGE"Advanced MD Mode ReflectMissileBolt Reflecting\n");
					}
				}
				else
				{
					if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						Com_Printf(S_COLOR_ORANGE"MD Mode ReflectMissileBolt Reflecting\n");
					}
				}

				if (other->owner && other->owner->client)
				{
					other->owner->client->ps.saberEventFlags |= SEF_DEFLECTED;
				}
				//do the effect
				VectorCopy(ent->s.pos.trDelta, diff);
				VectorNormalize(diff);
				G_MissileReflectEffect(ent, trace->plane.normal);

				return;
			}
		}
		else
		{
			//still do the bounce effect
			G_MissileReflectEffect(ent, trace->plane.normal);
		}
	}
	G_MissileImpacted(ent, other, trace->endpos, trace->plane.normal, hit_loc);
}

static void G_MissileImpactJKA(gentity_t* ent, trace_t* trace, const int hit_loc = HL_NONE)
{
	vec3_t diff;

	gentity_t* other = &g_entities[trace->entityNum];

	if (other == ent)
	{
		assert(0 && "missile hit itself!!!");
		return;
	}
	if (trace->plane.normal[0] == 0.0f &&
		trace->plane.normal[1] == 0.0f &&
		trace->plane.normal[2] == 0.0f)
	{
		//model moved into missile in flight probably...
		trace->plane.normal[0] = -ent->s.pos.trDelta[0];
		trace->plane.normal[1] = -ent->s.pos.trDelta[1];
		trace->plane.normal[2] = -ent->s.pos.trDelta[2];
		VectorNormalize(trace->plane.normal);
	}

	if (ent->owner && (other->takedamage || other->client))
	{
		if (!ent->lastEnemy || ent->lastEnemy == ent->owner)
		{
			//a missile that was not reflected or, if so, still is owned by original owner
			if (LogAccuracyHit(other, ent->owner))
			{
				ent->owner->client->ps.persistant[PERS_ACCURACY_HITS]++;
			}
			if (ent->owner->client && !ent->owner->s.number)
			{
				if (W_AccuracyLoggableWeapon(ent->s.weapon, qfalse, ent->methodOfDeath))
				{
					ent->owner->client->sess.missionStats.hits++;
				}
			}
		}
	}
	// check for bounce
	//OR: if the surfaceParm is has a reflect property (magnetic shielding) and the missile isn't an exploding missile
	auto bounce = static_cast<qboolean>(!other->takedamage && ent->s.eFlags & (EF_BOUNCE | EF_BOUNCE_HALF)
		|| (trace->surfaceFlags & SURF_FORCEFIELD || other->flags & FL_SHIELDED)
		&& !ent->splashDamage && !ent->splashRadius && ent->s.weapon != WP_NOGHRI_STICK);

	auto beskar = static_cast<qboolean>((other->flags & FL_DINDJARIN)
		&& !ent->splashDamage
		&& !ent->splashRadius
		&& ent->methodOfDeath != MOD_SABER
		&& ent->methodOfDeath != MOD_REPEATER_ALT
		&& ent->methodOfDeath != MOD_FLECHETTE_ALT
		&& ent->methodOfDeath != MOD_ROCKET
		&& ent->methodOfDeath != MOD_ROCKET_ALT
		&& ent->methodOfDeath != WP_NOGHRI_STICK
		&& ent->methodOfDeath != MOD_CONC_ALT
		&& ent->methodOfDeath != MOD_THERMAL
		&& ent->methodOfDeath != MOD_THERMAL_ALT
		&& ent->methodOfDeath != MOD_DEMP2
		&& ent->methodOfDeath != MOD_DEMP2_ALT
		&& ent->methodOfDeath != MOD_EXPLOSIVE
		&& ent->methodOfDeath != MOD_DETPACK
		&& ent->methodOfDeath != MOD_LASERTRIP
		&& ent->methodOfDeath != MOD_LASERTRIP_ALT
		&& ent->methodOfDeath != MOD_SEEKER
		&& ent->methodOfDeath != MOD_CONC
		&& (!Q_irand(0, 1)));

	auto boba_fett = static_cast<qboolean>((other->flags & FL_BOBAFETT)
		&& !ent->splashDamage
		&& !ent->splashRadius
		&& ent->methodOfDeath != MOD_SABER
		&& ent->methodOfDeath != MOD_REPEATER_ALT
		&& ent->methodOfDeath != MOD_FLECHETTE_ALT
		&& ent->methodOfDeath != MOD_ROCKET
		&& ent->methodOfDeath != MOD_ROCKET_ALT
		&& ent->methodOfDeath != WP_NOGHRI_STICK
		&& ent->methodOfDeath != MOD_CONC_ALT
		&& ent->methodOfDeath != MOD_THERMAL
		&& ent->methodOfDeath != MOD_THERMAL_ALT
		&& ent->methodOfDeath != MOD_DEMP2
		&& ent->methodOfDeath != MOD_DEMP2_ALT
		&& ent->methodOfDeath != MOD_EXPLOSIVE
		&& ent->methodOfDeath != MOD_DETPACK
		&& ent->methodOfDeath != MOD_LASERTRIP
		&& ent->methodOfDeath != MOD_LASERTRIP_ALT
		&& ent->methodOfDeath != MOD_SEEKER
		&& ent->methodOfDeath != MOD_CONC);

	if (ent->dflags & DAMAGE_HEAVY_WEAP_CLASS)
	{
		// heavy class missiles generally never bounce.
		bounce = qfalse;
		beskar = qfalse;
		boba_fett = qfalse;
	}

	if (other->flags & (FL_DMG_BY_HEAVY_WEAP_ONLY | FL_SHIELDED))
	{
		// Dumb assumption, but I guess we must be a shielded ion_cannon??  We should probably verify
		// if it's an ion_cannon that's Heavy Weapon only, we don't want to make it shielded do we...?
		if (strcmp("misc_ion_cannon", other->classname) == 0 && other->flags & FL_SHIELDED)
		{
			// Anything will bounce off of us.
			bounce = qtrue;

			// Not exactly the debounce time, but rather the impact time for the shield effect...play effect for 1 second
			other->painDebounceTime = level.time + 1000;
		}
	}

	if (ent->s.weapon == WP_DEMP2)
	{
		// demp2 shots can never bounce
		bounce = qfalse;
		beskar = qfalse;
		boba_fett = qfalse;
		// in fact, alt-charge shots will not call the regular impact functions
		if (ent->alt_fire)
		{
			// detonate at the trace end
			VectorCopy(trace->endpos, ent->currentOrigin);
			VectorCopy(trace->plane.normal, ent->pos1);
			DEMP2_AltDetonate(ent);
			return;
		}
	}

	if (beskar || boba_fett)
	{
		bounce = qfalse;
		// Check to see if there is a bounce count
		if (ent->bounceCount)
		{
			// decrement number of bounces and then see if it should be done bouncing
			if (!--ent->bounceCount)
			{
				// He (or she) will bounce no more (after this current bounce, that is).
				ent->s.eFlags &= ~(EF_BOUNCE | EF_BOUNCE_HALF);
			}
		}

		G_BounceMissile(ent, trace);
		NPC_SetAnim(other, SETANIM_TORSO, Q_irand(BOTH_PAIN1, BOTH_PAIN3), SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

		if (ent->owner)
		{
			G_MissileAddAlerts(ent);
		}
		G_MissileBounceBeskarEffect(ent, trace->plane.normal);
		return;
	}

	if (bounce)
	{
		// Check to see if there is a bounce count
		if (ent->bounceCount)
		{
			// decrement number of bounces and then see if it should be done bouncing
			if (!--ent->bounceCount)
			{
				// He (or she) will bounce no more (after this current bounce, that is).
				ent->s.eFlags &= ~(EF_BOUNCE | EF_BOUNCE_HALF);
			}
		}

		if (other->NPC)
		{
			G_Damage(other, ent, ent->owner, ent->currentOrigin, ent->s.pos.trDelta, 0, DAMAGE_NO_DAMAGE, MOD_UNKNOWN);
		}

		G_BounceMissile(ent, trace);

		if (ent->owner)
		{
			G_MissileAddAlerts(ent);
		}
		G_MissileBounceEffect(ent, trace->endpos, trace->plane.normal,
			static_cast<qboolean>(trace->entityNum == ENTITYNUM_WORLD));

		return;
	}

	if (!other->takedamage && ent->s.eFlags & EF_BOUNCE_SHRAPNEL
		|| trace->surfaceFlags & SURF_FORCEFIELD && !ent->splashDamage && !ent->splashRadius)
	{
		if (!(other->contents & CONTENTS_LIGHTSABER)
			|| g_spskill->integer <= 0 //on easy, it reflects all shots
			|| g_spskill->integer == 1 && ent->s.weapon != WP_FLECHETTE && ent->s.weapon != WP_DEMP2
			//on medium it won't reflect flechette or demp shots
			|| g_spskill->integer == 2 && ent->s.weapon != WP_FLECHETTE && ent->s.weapon != WP_DEMP2 && ent->s.weapon
			!= WP_BOWCASTER && ent->s.weapon != WP_REPEATER)
			//on hard it won't reflect flechette, demp, repeater or bowcaster shots
		{
			G_BounceMissile(ent, trace);

			if (--ent->bounceCount < 0)
			{
				ent->s.eFlags &= ~EF_BOUNCE_SHRAPNEL;
			}
			G_MissileBounceEffect(ent, trace->endpos, trace->plane.normal,
				static_cast<qboolean>(trace->entityNum == ENTITYNUM_WORLD));
			return;
		}
	}

	if ((!other->takedamage || other->client && other->health <= 0)
		&& ent->s.weapon == WP_THERMAL
		&& !ent->alt_fire)
	{
		//rolling thermal det - FIXME: make this an eFlag like bounce & stick!!!
		if (ent->owner)
		{
			G_MissileAddAlerts(ent);
		}
		return;
	}

	// check for sticking
	if (ent->s.eFlags & EF_MISSILE_STICK)
	{
		if (ent->owner)
		{
			//Add the event
			if (ent->s.weapon == WP_TRIP_MINE)
			{
				AddSoundEvent(ent->owner, ent->currentOrigin, ent->splashRadius / static_cast<float>(2), AEL_DISCOVERED, qfalse, qtrue);
				AddSightEvent(ent->owner, ent->currentOrigin, ent->splashRadius * 2, AEL_DISCOVERED, 60);
			}
			else
			{
				AddSoundEvent(ent->owner, ent->currentOrigin, 128, AEL_DISCOVERED, qfalse, qtrue);
				AddSightEvent(ent->owner, ent->currentOrigin, 256, AEL_DISCOVERED, 10);
			}
		}

		G_MissileStick(ent, other, trace);
		return;
	}

	// check for hitting a lightsaber
	if (WP_SaberMustBoltBlockJKAMode(other, ent, qfalse, trace->endpos, -1, -1)
		&& !WP_DoingForcedAnimationForForcePowers(other))
	{
		//play projectile block animation
		if (other->client && !PM_SaberInAttack(other->client->ps.saber_move)
			|| other->client && (pm->cmd.buttons & BUTTON_USE_FORCE
				|| pm->cmd.buttons & BUTTON_FORCEGRIP
				|| pm->cmd.buttons & BUTTON_DASH
				|| pm->cmd.buttons & BUTTON_LIGHTNING_STRIKE
				|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING))
		{
			other->client->ps.weaponTime = 0;
		}

		VectorSubtract(ent->currentOrigin, other->currentOrigin, diff);
		VectorNormalize(diff);

		G_BoltBlockMissile(other, ent, diff);

		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_RED"G_ReflectMissile_JKA\n");
		}

		if (other->owner && !other->owner->s.number && other->owner->client)
		{
			other->owner->client->sess.missionStats.saberBlocksCnt++;
		}

		if (other->owner && other->owner->client)
		{
			other->owner->client->ps.saberEventFlags |= SEF_DEFLECTED;
		}

		//do the effect
		VectorCopy(ent->s.pos.trDelta, diff);
		VectorNormalize(diff);
		G_MissileReflectEffect(ent, trace->plane.normal);

		return;
	}
	if (other->contents & CONTENTS_LIGHTSABER)
	{
		if (other->owner && !other->owner->s.number && other->owner->client)
		{
			other->owner->client->sess.missionStats.saberBlocksCnt++;
		}
		if ((g_spskill->integer <= 0 //on easy, it reflects all shots
			|| g_spskill->integer == 1 && ent->s.weapon != WP_FLECHETTE && ent->s.weapon != WP_DEMP2
			|| g_spskill->integer == 2 && ent->s.weapon != WP_FLECHETTE && ent->s.weapon != WP_DEMP2 && ent->s.
			weapon != WP_BOWCASTER && ent->s.weapon != WP_REPEATER)
			&& (!ent->splashDamage || !ent->splashRadius) && ent->s.weapon != WP_NOGHRI_STICK)
		{
			if (!other->owner || !other->owner->client || other->owner->client->ps.saberInFlight
				|| InFront(ent->currentOrigin, other->owner->currentOrigin, other->owner->client->ps.viewangles,
					SABER_REFLECT_MISSILE_CONE) &&
				!WP_DoingForcedAnimationForForcePowers(other))
			{
				//Jedi cannot block shots from behind!
				int block_chance = 0;

				switch (other->owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE])
				{
					//level 1 reflects 50% of the time, level 2 reflects 75% of the time
				case FORCE_LEVEL_3:
					block_chance = 10;
					break;
				case FORCE_LEVEL_2:
					block_chance = 3;
					break;
				case FORCE_LEVEL_1:
					block_chance = 1;
					break;
				default:;
				}
				if (block_chance && other->owner->client->ps.forcePowersActive & 1 << FP_SPEED)
				{
					//in in force speed, better chance of deflecting the shot
					block_chance += other->owner->client->ps.forcePowerLevel[FP_SPEED] * 2;
				}
				if (Q_irand(0, block_chance))
				{
					VectorSubtract(ent->currentOrigin, other->currentOrigin, diff);
					VectorNormalize(diff);
					G_BoltBlockMissile(other, ent, diff);
					if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						Com_Printf(S_COLOR_MAGENTA"G_ReflectMissile_JKA\n");
					}
					if (other->owner && other->owner->client)
					{
						other->owner->client->ps.saberEventFlags |= SEF_DEFLECTED;
					}
					//do the effect
					VectorCopy(ent->s.pos.trDelta, diff);
					VectorNormalize(diff);
					G_MissileReflectEffect(ent, trace->plane.normal);
					return;
				}
			}
		}
		else
		{
			//still do the bounce effect
			G_MissileReflectEffect(ent, trace->plane.normal);
		}
	}

	G_MissileImpacted(ent, other, trace->endpos, trace->plane.normal, hit_loc);
}

/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
static void G_ExplodeMissile(gentity_t* ent)
{
	vec3_t dir{};
	vec3_t origin;

	EvaluateTrajectory(&ent->s.pos, level.time, origin);
	SnapVector(origin);
	G_SetOrigin(ent, origin);

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	if (ent->owner) //&& ent->owner->s.number == 0 )
	{
		//Add the event
		AddSoundEvent(ent->owner, ent->currentOrigin, 256, AEL_DISCOVERED, qfalse, qtrue);
		AddSightEvent(ent->owner, ent->currentOrigin, 512, AEL_DISCOVERED, 100);
	}

	// splash damage
	if (ent->splashDamage)
	{
		G_RadiusDamage(ent->currentOrigin, ent->owner, ent->splashDamage, ent->splashRadius, nullptr, ent->splashMethodOfDeath);
	}

	G_FreeEntity(ent);
}

static void G_RunStuckMissile(gentity_t* ent)
{
	if (ent->takedamage)
	{
		if (ent->s.groundEntityNum >= 0 && ent->s.groundEntityNum < ENTITYNUM_WORLD)
		{
			gentity_t* other = &g_entities[ent->s.groundEntityNum];

			if (!VectorCompare(vec3_origin, other->s.pos.trDelta) && other->s.pos.trType != TR_STATIONARY ||
				!VectorCompare(vec3_origin, other->s.apos.trDelta) && other->s.apos.trType != TR_STATIONARY)
			{
				//thing I stuck to is moving or rotating now, kill me
				G_Damage(ent, other, other, nullptr, nullptr, 99999, 0, MOD_CRUSH);
				return;
			}
		}
	}
	// check think function
	G_RunThink(ent);
}

/*
==================

G_GroundTrace

==================
*/
static int G_GroundTrace(const gentity_t* ent, pml_t* pPml)
{
	vec3_t point{};
	trace_t trace;

	point[0] = ent->currentOrigin[0];
	point[1] = ent->currentOrigin[1];
	point[2] = ent->currentOrigin[2] - 0.25;

	gi.trace(&trace, ent->currentOrigin, ent->mins, ent->maxs, point, ent->s.number, ent->clipmask,
		static_cast<EG2_Collision>(0), 0);
	pPml->groundTrace = trace;

	// do something corrective if the trace starts in a solid...
	if (trace.allsolid)
	{
		pPml->groundPlane = qfalse;
		pPml->walking = qfalse;
		return ENTITYNUM_NONE;
	}

	// if the trace didn't hit anything, we are in free fall
	if (trace.fraction == 1.0)
	{
		pPml->groundPlane = qfalse;
		pPml->walking = qfalse;
		return ENTITYNUM_NONE;
	}

	// check if getting thrown off the ground
	if (ent->s.pos.trDelta[2] > 0 && DotProduct(ent->s.pos.trDelta, trace.plane.normal) > 10)
	{
		pPml->groundPlane = qfalse;
		pPml->walking = qfalse;
		return ENTITYNUM_NONE;
	}

	// slopes that are too steep will not be considered on ground
	if (trace.plane.normal[2] < MIN_WALK_NORMAL)
	{
		pPml->groundPlane = qtrue;
		pPml->walking = qfalse;
		return ENTITYNUM_NONE;
	}

	pPml->groundPlane = qtrue;
	pPml->walking = qtrue;

	return trace.entityNum;
}

static void G_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, const float overbounce)
{
	float backoff = DotProduct(in, normal);

	if (backoff < 0)
	{
		backoff *= overbounce;
	}
	else
	{
		backoff /= overbounce;
	}

	for (int i = 0; i < 3; i++)
	{
		const float change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
}

/*
==================

G_RollMissile

reworking the rolling object code,
still needs to stop bobbling up & down,
need to get roll angles right,
and need to maybe make the transfer of velocity happen on impacts?
Also need bounce sound for bounces off a floor.
Also need to not bounce as much off of enemies
Also gets stuck inside thrower if looking down when thrown

==================
*/
constexpr auto MAX_CLIP_PLANES = 5;
constexpr auto BUMPCLIP = 1.5f;

static void G_RollMissile(gentity_t* ent)
{
	int numplanes;
	vec3_t planes[MAX_CLIP_PLANES]{};
	vec3_t primal_velocity;
	int i;
	trace_t trace;
	vec3_t end_velocity;
	pml_t objPML;
	float bounceAmt;

	memset(&objPML, 0, sizeof objPML);

	G_GroundTrace(ent, &objPML);

	objPML.frametime = (level.time - level.previousTime) * 0.001;

	constexpr int numbumps = 4;

	VectorCopy(ent->s.pos.trDelta, primal_velocity);

	VectorCopy(ent->s.pos.trDelta, end_velocity);
	end_velocity[2] -= g_gravity->value * objPML.frametime;
	ent->s.pos.trDelta[2] = (ent->s.pos.trDelta[2] + end_velocity[2]) * 0.5;
	primal_velocity[2] = end_velocity[2];
	if (objPML.groundPlane)
	{
		//FIXME: never happens!
		// slide along the ground plane
		G_ClipVelocity(ent->s.pos.trDelta, objPML.groundTrace.plane.normal, ent->s.pos.trDelta, BUMPCLIP);
		VectorScale(ent->s.pos.trDelta, 0.9f, ent->s.pos.trDelta);
	}

	float time_left = objPML.frametime;

	// never turn against the ground plane
	if (objPML.groundPlane)
	{
		numplanes = 1;
		VectorCopy(objPML.groundTrace.plane.normal, planes[0]);
	}
	else
	{
		numplanes = 0;
	}

	for (int bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		vec3_t end;
		// calculate position we are trying to move to
		VectorMA(ent->currentOrigin, time_left, ent->s.pos.trDelta, end);

		// see if we can make it there
		gi.trace(&trace, ent->currentOrigin, ent->mins, ent->maxs, end, ent->s.number, ent->clipmask, G2_RETURNONHIT,
			10);

		//had to move this up above the trace.allsolid check now that for some reason ghoul2 impacts tell me I'm allsolid?!
		//this needs to be fixed, really
		if (trace.entityNum < ENTITYNUM_WORLD)
		{
			//hit another ent
			gentity_t* hit_ent = &g_entities[trace.entityNum];

			if (hit_ent && (hit_ent->takedamage || hit_ent->contents & CONTENTS_LIGHTSABER))
			{
				if (g_SerenityJediEngineMode->integer)
				{
					G_MissileImpact_MD(hit_ent, &trace);
				}
				else
				{
					G_MissileImpactJKA(hit_ent, &trace);
				}

				if (ent->s.eType == ET_GENERAL)
				{
					//exploded
					return;
				}
			}
		}

		if (trace.allsolid)
		{
			ent->s.pos.trDelta[2] = 0; // don't build up falling damage, but allow sideways acceleration
			return; // qtrue;
		}

		if (trace.fraction > 0)
		{
			// actually covered some distance
			VectorCopy(trace.endpos, ent->currentOrigin);
		}

		if (trace.fraction == 1)
		{
			break; // moved the entire distance
		}

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear(ent->s.pos.trDelta);
			return; // qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for (i = 0; i < numplanes; i++)
		{
			if (DotProduct(trace.plane.normal, planes[i]) > 0.99)
			{
				VectorAdd(trace.plane.normal, ent->s.pos.trDelta, ent->s.pos.trDelta);
				break;
			}
		}
		if (i < numplanes)
		{
			continue;
		}
		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//
		if (&g_entities[trace.entityNum] != nullptr && g_entities[trace.entityNum].client)
		{
			//hit a person, bounce off much less
			bounceAmt = OVERCLIP;
		}
		else
		{
			bounceAmt = BUMPCLIP;
		}

		// find a plane that it enters
		for (i = 0; i < numplanes; i++)
		{
			vec3_t end_clip_velocity;
			vec3_t clip_velocity;
			const float into = DotProduct(ent->s.pos.trDelta, planes[i]);
			if (into >= 0.1)
			{
				continue; // move doesn't interact with the plane
			}

			// see how hard we are hitting things
			if (-into > pml.impactSpeed)
			{
				pml.impactSpeed = -into;
			}

			// slide along the plane
			G_ClipVelocity(ent->s.pos.trDelta, planes[i], clip_velocity, bounceAmt);

			// slide along the plane
			G_ClipVelocity(end_velocity, planes[i], end_clip_velocity, bounceAmt);

			// see if there is a second plane that the new move enters
			for (int j = 0; j < numplanes; j++)
			{
				vec3_t dir;
				if (j == i)
				{
					continue;
				}
				if (DotProduct(clip_velocity, planes[j]) >= 0.1)
				{
					continue; // move doesn't interact with the plane
				}

				// try clipping the move to the plane
				G_ClipVelocity(clip_velocity, planes[j], clip_velocity, bounceAmt);
				G_ClipVelocity(end_clip_velocity, planes[j], end_clip_velocity, bounceAmt);

				// see if it goes back into the first clip plane
				if (DotProduct(clip_velocity, planes[i]) >= 0)
				{
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				float d = DotProduct(dir, ent->s.pos.trDelta);
				VectorScale(dir, d, clip_velocity);

				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				d = DotProduct(dir, end_velocity);
				VectorScale(dir, d, end_clip_velocity);

				// see if there is a third plane the the new move enters
				for (int k = 0; k < numplanes; k++)
				{
					if (k == i || k == j)
					{
						continue;
					}
					if (DotProduct(clip_velocity, planes[k]) >= 0.1)
					{
						continue; // move doesn't interact with the plane
					}

					// stop dead at a triple plane interaction
					VectorClear(ent->s.pos.trDelta);
					return; // qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy(clip_velocity, ent->s.pos.trDelta);
			VectorCopy(end_clip_velocity, end_velocity);
			break;
		}
		VectorScale(end_velocity, 0.975f, end_velocity);
	}

	VectorCopy(end_velocity, ent->s.pos.trDelta);
}

/*
================
G_RunMissile

================
*/
void G_MoverTouchPushTriggers(gentity_t* ent, vec3_t old_org);

void G_RunMissile(gentity_t* ent)
{
	vec3_t old_org;
	trace_t tr;
	int tr_hit_loc = HL_NONE;

	if (ent->s.eFlags & EF_HELD_BY_SAND_CREATURE)
	{
		//in a sand creature's mouth
		if (ent->activator)
		{
			mdxaBone_t bolt_matrix;
			// Getting the bolt here
			//in hand
			vec3_t sc_angles = { 0 };
			sc_angles[YAW] = ent->activator->currentAngles[YAW];
			gi.G2API_GetBoltMatrix(ent->activator->ghoul2, ent->activator->playerModel, ent->activator->gutBolt,
				&bolt_matrix, sc_angles, ent->activator->currentOrigin,
				cg.time ? cg.time : level.time,
				nullptr, ent->activator->s.modelScale);
			// Storing ent position, bolt position, and bolt axis
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent->currentOrigin);
			G_SetOrigin(ent, ent->currentOrigin);
		}
		// check think function
		G_RunThink(ent);
		return;
	}

	VectorCopy(ent->currentOrigin, old_org);

	// get current position
	if (ent->s.pos.trType == TR_INTERPOLATE)
	{
		//rolling missile?
		G_RollMissile(ent);
		if (ent->s.eType != ET_GENERAL)
		{
			//didn't explode
			VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
			gi.trace(&tr, old_org, ent->mins, ent->maxs, ent->currentOrigin, ent->s.number, ent->clipmask,
				G2_RETURNONHIT, 10);
			if (VectorCompare(ent->s.pos.trDelta, vec3_origin))
			{
				VectorClear(ent->s.apos.trDelta);
			}
			else
			{
				vec3_t ang, fwd_dir, rt_dir;

				ent->s.apos.trType = TR_INTERPOLATE;
				VectorSet(ang, 0, ent->s.apos.trBase[1], 0);
				AngleVectors(ang, fwd_dir, rt_dir, nullptr);
				const float speed = VectorLength(ent->s.pos.trDelta) * 4;

				//HMM, this works along an axis-aligned dir, but not along diagonals
				//This is because when roll gets to 90, pitch becomes yaw, and vice-versa
				//Maybe need to just set the angles directly?
				ent->s.apos.trDelta[0] = DotProduct(fwd_dir, ent->s.pos.trDelta);
				ent->s.apos.trDelta[1] = 0; //never spin!
				ent->s.apos.trDelta[2] = DotProduct(rt_dir, ent->s.pos.trDelta);

				VectorNormalize(ent->s.apos.trDelta);
				VectorScale(ent->s.apos.trDelta, speed, ent->s.apos.trDelta);

				ent->s.apos.trTime = level.previousTime;
			}
		}
	}
	else
	{
		vec3_t origin;
		EvaluateTrajectory(&ent->s.pos, level.time, origin);
		// trace a line from the previous position to the current position,
		// ignoring interactions with the missile owner
		gi.trace(&tr, ent->currentOrigin, ent->mins, ent->maxs, origin,
			ent->owner ? ent->owner->s.number : ent->s.number, ent->clipmask, G2_COLLIDE, 10);

		if (tr.entityNum != ENTITYNUM_NONE)
		{
			const gentity_t* other = &g_entities[tr.entityNum];
			// check for hitting a lightsaber
			if (other->contents & CONTENTS_LIGHTSABER)
			{
				//hit a lightsaber bbox
				if (other->owner
					&& other->owner->client
					&& !other->owner->client->ps.saberInFlight
					&& (Q_irand(
						0, other->owner->client->ps.forcePowerLevel[FP_SABER_DEFENSE] * other->owner->client->ps.
						forcePowerLevel[FP_SABER_DEFENSE]) == 0
						|| !InFront(ent->currentOrigin, other->owner->currentOrigin,
							other->owner->client->ps.viewangles, SABER_REFLECT_MISSILE_CONE)))
					//other->owner->s.number == 0 &&
				{
					//Jedi cannot block shots from behind!	//re-trace from here, ignoring the lightsaber
					gi.trace(&tr, tr.endpos, ent->mins, ent->maxs, origin, tr.entityNum, ent->clipmask, G2_RETURNONHIT,
						10);
				}
			}
		}

		VectorCopy(tr.endpos, ent->currentOrigin);
	}

	// get current angles
	VectorMA(ent->s.apos.trBase, (level.time - ent->s.apos.trTime) * 0.001, ent->s.apos.trDelta, ent->s.apos.trBase);

	//FIXME: Rolling things hitting G2 polys is weird
	///////////////////////////////////////////////////////
	//?	if ( tr.fraction != 1 )
	{
		// did we hit or go near a Ghoul2 model?
		for (auto& i : tr.G2CollisionMap)
		{
			if (i.mEntityNum == -1)
			{
				break;
			}

			CCollisionRecord& coll = i;
			const gentity_t* hit_ent = &g_entities[coll.mEntityNum];

			// process collision records here...
			// make sure we only do this once, not for all the entrance wounds we might generate
			if (coll.mFlags & G2_FRONTFACE/* && !(hitModel)*/ && hit_ent->health)
			{
				if (tr_hit_loc == HL_NONE)
				{
					G_GetHitLocFromSurfName(&g_entities[coll.mEntityNum],
						gi.G2API_GetSurfaceName(
							&g_entities[coll.mEntityNum].ghoul2[coll.mModelIndex],
							coll.mSurfaceIndex), &tr_hit_loc, coll.mCollisionPosition, nullptr,
						nullptr, ent->methodOfDeath);
				}

				break;
				// NOTE: the way this whole section was working, it would only get inside of this IF once anyway, might as well break out now
			}
		}
	}
	/////////////////////////////////////////////////////////

	if (tr.startsolid)
	{
		tr.fraction = 0;
	}

	gi.linkentity(ent);

	if (ent->s.pos.trType == TR_STATIONARY && ent->s.eFlags & EF_MISSILE_STICK)
	{
		//stuck missiles should check some special stuff
		G_RunStuckMissile(ent);
		return;
	}

	// check think function
	G_RunThink(ent);

	if (ent->s.eType != ET_MISSILE)
	{
		return; // exploded
	}

	if (ent->mass)
	{
		G_MoverTouchPushTriggers(ent, old_org);
	}

	AddSightEvent(ent->owner, ent->currentOrigin, 512, AEL_DISCOVERED, 75);
	//wakes them up when see a shot passes in front of them
	if (!Q_irand(0, 10))
	{
		//not so often...
		if (ent->splashDamage && ent->splashRadius)
		{
			//I'm an exploder, let people around me know danger is coming
			if (ent->s.weapon == WP_TRIP_MINE)
			{
				//???
			}
			else
			{
				if (ent->s.weapon == WP_ROCKET_LAUNCHER && ent->e_ThinkFunc == thinkF_rocketThink)
				{
					//homing rocket- run like hell!
					AddSightEvent(ent->owner, ent->currentOrigin, ent->splashRadius, AEL_DANGER_GREAT, 50);
				}
				else
				{
					AddSightEvent(ent->owner, ent->currentOrigin, ent->splashRadius, AEL_DANGER, 50);
				}
				AddSoundEvent(ent->owner, ent->currentOrigin, ent->splashRadius, AEL_DANGER);
			}
		}
		else
		{
			//makes them run from near misses
			AddSightEvent(ent->owner, ent->currentOrigin, 48, AEL_DANGER, 50);
		}
	}

	if (tr.fraction == 1)
	{
		if (ent->s.weapon == WP_THERMAL && ent->s.pos.trType == TR_INTERPOLATE)
		{
			//a rolling thermal that didn't hit anything
			G_MissileAddAlerts(ent);
		}
		return;
	}

	if (tr.fraction != 1)
	{
		// never explode or bounce on sky
		if (tr.surfaceFlags & SURF_NOIMPACT)
		{
			if (ent->parent && ent->parent->client && ent->parent->client->hook == ent)
			{
				ent->parent->client->hook = nullptr;
				ent->parent->client->hookhasbeenfired = qfalse;
				ent->parent->client->fireHeld = qfalse;
			}
			else if (ent->parent && ent->parent->client && ent->parent->client->stun == ent)
			{
				ent->parent->client->stun = nullptr;
				ent->parent->client->stunhasbeenfired = qfalse;
				ent->parent->client->stunHeld = qfalse;
			}
			else
			{
				G_FreeEntity(ent);
				return;
			}
		}
	}

	if (g_SerenityJediEngineMode->integer)
	{
		G_MissileImpact_MD(ent, &tr, tr_hit_loc); //Controlled by the player
	}
	else
	{
		G_MissileImpactJKA(ent, &tr, tr_hit_loc);
	}
}

static void wp_handle_bolt_block_sje_blockpoints(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean saber_block_reflection = qfalse;
	qboolean bolt_block_reflection = qfalse;
	qboolean npc_reflection = qfalse;
	qboolean reflected = qfalse;
	gentity_t* blocker = ent;
	constexpr int punish = BLOCKPOINTS_TWELVE;

	if (ent->owner)
	{
		blocker = ent->owner;
	}

	const qboolean manual_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;
	const qboolean manual_proj_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	const qboolean np_cis_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_NPCBLOCKING ? qtrue : qfalse;
	float slop_factor = (FATIGUE_AUTOBOLTBLOCK - 6) * (static_cast<float>(FORCE_LEVEL_3) - blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE]) / FORCE_LEVEL_3;
	const qboolean accurate_missile_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_ACCURATEMISSILEBLOCKING ? qtrue : qfalse;

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	AngleVectors(blocker->client->ps.viewangles, forward, nullptr, nullptr);

	if (ent && blocker && blocker->client && manual_blocking && !manual_proj_blocking)
	{
		saber_block_reflection = qtrue;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && manual_blocking && manual_proj_blocking)
	{
		bolt_block_reflection = qtrue;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && np_cis_blocking)
	{
		npc_reflection = qtrue;
	}
	if (ent && blocker && blocker->client && blocker->client->ps.saberInFlight)
	{
		//but need saber in-hand for perfect reflection
		saber_block_reflection = qfalse;
		bolt_block_reflection = qfalse;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
	{
		//but need saber in-hand for perfect reflection
		saber_block_reflection = qfalse;
		bolt_block_reflection = qfalse;
		npc_reflection = qfalse;
	}

	//play projectile block animation
	if (saber_block_reflection && !npc_reflection) //GOES TO CROSSHAIR
	{
		vec3_t angs;
		if (level.time - blocker->client->ps.ManualblockStartTime < 3000) //Blocking 2
		{
			// good
			vectoangles(forward, angs);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		else if (blocker->client->pers.cmd.forwardmove >= 0)
		{
			//bad
			slop_factor += Q_irand(1, 5);
			vectoangles(forward, angs);
			angs[PITCH] += Q_irand(-slop_factor, slop_factor);
			angs[YAW] += Q_irand(-slop_factor, slop_factor);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		else
		{
			//average
			slop_factor += Q_irand(1, 3);
			vectoangles(forward, angs);
			angs[PITCH] += Q_irand(-slop_factor, slop_factor);
			angs[YAW] += Q_irand(-slop_factor, slop_factor);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		VectorCopy(forward, bounce_dir);

		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_GREEN"Advanced MD Mode Crosshair Deflection\n");
		}

		if (blocker->client->ps.blockPoints < BLOCKPOINTS_THIRTY)
		{
			//very Low points = bad blocks
			if (blocker->client->ps.blockPoints < BLOCKPOINTS_FATIGUE)
			{
				//Low points = bad blocks
				WP_BoltBlockVictimFatigued(blocker);
				blocker->client->ps.saberBlocked = BLOCKED_NONE;
				blocker->client->ps.saberBounceMove = LS_NONE;
			}
			else
			{
				WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
			}
		}
		else
		{
			WP_SaberBlockBolt_AMD(blocker, missile->currentOrigin, qfalse);
		}

		int block_points_used_used;

		if (accurate_missile_blocking)
		{
			// excellent
			block_points_used_used = 2;
		}
		else
		{
			block_points_used_used = WP_SaberBoltBlockCost(blocker, missile);
		}

		if (blocker->client->ps.blockPoints < block_points_used_used)
		{
			blocker->client->ps.blockPoints = 0;
		}
		else
		{
			WP_BlockPointsDrain(blocker, block_points_used_used);
		}
		reflected = qtrue;
	}
	else if (bolt_block_reflection && !npc_reflection) //GOES TO ENEMY
	{
		gentity_t* attacker;

		if (blocker->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			attacker = blocker->enemy;
		}
		else
		{
			//find another enemy
			attacker = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}

		if (attacker)
		{
			vec3_t bullseye;
			CalcEntitySpot(attacker, SPOT_HEAD, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim)
					|| blocker->client->ps.blockPoints < BLOCKPOINTS_KNOCKAWAY)
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}

			VectorNormalize(bounce_dir);

			if (blocker->client->ps.blockPoints < BLOCKPOINTS_THIRTY)
			{
				//very Low points = bad blocks
				if (blocker->client->ps.blockPoints < BLOCKPOINTS_FATIGUE)
				{
					//Low points = bad blocks
					WP_BoltBlockVictimFatigued(blocker);
					blocker->client->ps.saberBlocked = BLOCKED_NONE;
					blocker->client->ps.saberBounceMove = LS_NONE;
				}
				else
				{
					WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
				}
			}
			else
			{
				WP_SaberBlockBolt_MD(blocker, missile->currentOrigin, qfalse);
			}

			int block_points_used_used;

			if (accurate_missile_blocking)
			{
				// excellent
				block_points_used_used = 2;
			}
			else
			{
				block_points_used_used = WP_SaberBoltBlockCost(blocker, missile);
			}

			if (blocker->client->ps.blockPoints < block_points_used_used)
			{
				blocker->client->ps.blockPoints = 0;
			}
			else
			{
				WP_BlockPointsDrain(blocker, block_points_used_used);
			}

			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_GREEN"Advanced MD Mode Bolt Blocking to enemy\n");
			}
			reflected = qtrue;
		}
	}
	else if (npc_reflection && !saber_block_reflection && !bolt_block_reflection) //GOES TO ENEMY
	{
		gentity_t* enemy;

		if (blocker->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			enemy = blocker->enemy;
		}
		else
		{
			//find another enemy
			enemy = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}

		if (enemy)
		{
			vec3_t bullseye;
			CalcEntitySpot(enemy, SPOT_CHEST, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}

			VectorNormalize(bounce_dir);

			if (blocker->client->ps.blockPoints < BLOCKPOINTS_THIRTY)
			{
				//very Low points = bad blocks
				if (blocker->client->ps.blockPoints < BLOCKPOINTS_FATIGUE)
				{
					//Low points = bad blocks
					WP_BoltBlockVictimFatigued(blocker);
					blocker->client->ps.saberBlocked = BLOCKED_NONE;
					blocker->client->ps.saberBounceMove = LS_NONE;
				}
				else
				{
					WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
				}
			}
			else
			{
				WP_SaberBlockBolt_MD(blocker, missile->currentOrigin, qfalse);

				if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
					G_ControlledByPlayer(blocker)))
				{
					gi.Printf(S_COLOR_CYAN"NPC normal AMD Bolt Block\n");
				}
			}

			int block_points_used_used;

			if (accurate_missile_blocking)
			{
				// excellent
				block_points_used_used = 2;
			}
			else
			{
				block_points_used_used = WP_SaberBoltBlockCost(blocker, missile);
			}

			if (blocker->client->ps.blockPoints < block_points_used_used)
			{
				blocker->client->ps.blockPoints = 0;
			}
			else
			{
				WP_BlockPointsDrain(blocker, block_points_used_used);
			}

			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_GREEN"Advanced MD Mode G_ReflectMissileNPC\n");
			}
			reflected = qtrue;
		}
	}

	if (!reflected) //GOES TO CROSSHAIR AREA
	{
		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_CYAN"Saber Not Blocked but Reflected off saber.\n");
		}

		if (blocker && blocker->s.client_num >= MAX_CLIENTS)
		{
			if (missile->owner && missile->s.weapon != WP_SABER)
			{
				//bounce back at them if you can
				VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
				VectorNormalize(bounce_dir);
			}
			else
			{
				vec3_t missile_dir;

				VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
				VectorCopy(missile->s.pos.trDelta, bounce_dir);
				VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
				VectorNormalize(bounce_dir);
			}
		}
		else //deflect off at an angle.
		{
			vec3_t deflect_dir, missile_dir;
			float force_factor;
			VectorSubtract(g_crosshairWorldCoord, missile->currentOrigin, deflect_dir);
			VectorCopy(missile->s.pos.trDelta, missile_dir);
			VectorNormalize(missile_dir);
			VectorNormalize(deflect_dir);

			//bigger forceFactors make the reflected shots go closer to the cross hair
			switch (blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE])
			{
			case FORCE_LEVEL_1:
				force_factor = 2.0f;
				break;
			case FORCE_LEVEL_2:
				force_factor = 3.0f;
				break;
			default:
				force_factor = 10.0f;
				break;
			}

			VectorMA(missile_dir, force_factor, deflect_dir, bounce_dir);

			VectorNormalize(bounce_dir);
		}
		if (blocker->s.weapon == WP_SABER && blocker->client)
		{
			//saber
			if (blocker->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1 or below
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
			}
		}
		WP_BlockPointsDrain(blocker, punish);
		wp_bolt_block_victim_reflected(blocker);
		blocker->client->ps.saberBlocked = BLOCKED_NONE;
		blocker->client->ps.saberBounceMove = LS_NONE;
	}

	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(
		!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]
		));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = blocker;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

static void wp_handle_bolt_block_sje_forcepoints(gentity_t* ent, gentity_t* missile, vec3_t forward)
{
	vec3_t bounce_dir;
	int i;
	qboolean saber_block_reflection = qfalse;
	qboolean bolt_block_reflection = qfalse;
	qboolean npc_reflection = qfalse;
	qboolean reflected = qfalse;
	gentity_t* blocker = ent;

	constexpr int punish = BLOCKPOINTS_TWELVE;

	if (ent->owner)
	{
		blocker = ent->owner;
	}

	const qboolean manual_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;
	const qboolean manual_proj_blocking =
		blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	const qboolean np_cis_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_NPCBLOCKING ? qtrue : qfalse;
	const qboolean accurate_missile_blocking =
		blocker->client->ps.ManualBlockingFlags & 1 << MBF_ACCURATEMISSILEBLOCKING ? qtrue : qfalse;
	float slop_factor = (FATIGUE_AUTOBOLTBLOCK - 6) * (static_cast<float>(FORCE_LEVEL_3) - blocker->client->ps.forcePowerLevel[
		FP_SABER_DEFENSE]) / FORCE_LEVEL_3;

	//save the original speed
	const float speed = VectorNormalize(missile->s.pos.trDelta);

	AngleVectors(blocker->client->ps.viewangles, forward, nullptr, nullptr);

	if (ent && blocker && blocker->client && manual_blocking && !manual_proj_blocking)
	{
		saber_block_reflection = qtrue;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && manual_blocking && manual_proj_blocking)
	{
		bolt_block_reflection = qtrue;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && np_cis_blocking)
	{
		npc_reflection = qtrue;
	}
	if (ent && blocker && blocker->client && blocker->client->ps.saberInFlight)
	{
		//but need saber in-hand for perfect reflection
		saber_block_reflection = qfalse;
		bolt_block_reflection = qfalse;
		npc_reflection = qfalse;
	}

	if (ent && blocker && blocker->client && blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
	{
		//but need saber in-hand for perfect reflection
		saber_block_reflection = qfalse;
		bolt_block_reflection = qfalse;
		npc_reflection = qfalse;
	}

	//play projectile block animation
	if (saber_block_reflection && !npc_reflection) //GOES TO CROSSHAIR
	{
		vec3_t angs;
		if (level.time - blocker->client->ps.ManualblockStartTime < 3000) //Blocking 2
		{
			// good
			vectoangles(forward, angs);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		else if (blocker->client->pers.cmd.forwardmove >= 0)
		{
			//bad
			slop_factor += Q_irand(1, 5);
			vectoangles(forward, angs);
			angs[PITCH] += Q_irand(-slop_factor, slop_factor);
			angs[YAW] += Q_irand(-slop_factor, slop_factor);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		else
		{
			//average
			slop_factor += Q_irand(1, 3);
			vectoangles(forward, angs);
			angs[PITCH] += Q_irand(-slop_factor, slop_factor);
			angs[YAW] += Q_irand(-slop_factor, slop_factor);
			AngleVectors(angs, forward, nullptr, nullptr);
		}
		VectorCopy(forward, bounce_dir);

		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_GREEN"MD Mode Crosshair Deflection\n");
		}

		if (blocker->client->ps.forcePower < BLOCKPOINTS_THIRTY)
		{
			//very Low points = bad blocks
			if (blocker->client->ps.forcePower < BLOCKPOINTS_FATIGUE)
			{
				//Low points = bad blocks
				WP_BoltBlockVictimFatigued(blocker);
				blocker->client->ps.saberBlocked = BLOCKED_NONE;
				blocker->client->ps.saberBounceMove = LS_NONE;
			}
			else
			{
				WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
			}
		}
		else
		{
			WP_SaberBlockBolt_AMD(blocker, missile->currentOrigin, qfalse);
		}

		int force_points_used_used;

		if (accurate_missile_blocking)
		{
			// excellent
			force_points_used_used = 2;
		}
		else
		{
			force_points_used_used = WP_SaberBoltBlockCost(blocker, missile);
		}

		if (blocker->client->ps.forcePower < force_points_used_used)
		{
			blocker->client->ps.forcePower = 0;
		}
		else
		{
			WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, force_points_used_used);
		}
		reflected = qtrue;
	}
	else if (bolt_block_reflection && !npc_reflection) //GOES TO ENEMY
	{
		gentity_t* attacker;

		if (blocker->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			attacker = blocker->enemy;
		}
		else
		{
			//find another enemy
			attacker = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}

		if (attacker)
		{
			vec3_t bullseye;
			CalcEntitySpot(attacker, SPOT_HEAD, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim)
					|| blocker->client->ps.forcePower < BLOCKPOINTS_KNOCKAWAY)
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}

			VectorNormalize(bounce_dir);

			if (blocker->client->ps.forcePower < BLOCKPOINTS_THIRTY)
			{
				//very Low points = bad blocks
				if (blocker->client->ps.forcePower < BLOCKPOINTS_FATIGUE)
				{
					//Low points = bad blocks
					WP_BoltBlockVictimFatigued(blocker);
					blocker->client->ps.saberBlocked = BLOCKED_NONE;
					blocker->client->ps.saberBounceMove = LS_NONE;
				}
				else
				{
					WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
				}
			}
			else
			{
				WP_SaberBlockBolt_MD(blocker, missile->currentOrigin, qfalse);
			}

			int force_points_used_used;

			if (accurate_missile_blocking)
			{
				// excellent
				force_points_used_used = 2;
			}
			else
			{
				force_points_used_used = WP_SaberBoltBlockCost(blocker, missile);
			}

			if (blocker->client->ps.forcePower < force_points_used_used)
			{
				blocker->client->ps.forcePower = 0;
			}
			else
			{
				WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, force_points_used_used);
			}

			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_GREEN"MD Mode Bolt Blocking to enemy\n");
			}
			reflected = qtrue;
		}
	}
	else if (npc_reflection && !saber_block_reflection && !bolt_block_reflection) //GOES TO ENEMY
	{
		gentity_t* enemy;

		if (blocker->enemy && Q_irand(0, 3))
		{
			//toward current enemy 75% of the time
			enemy = blocker->enemy;
		}
		else
		{
			//find another enemy
			enemy = jedi_find_enemy_in_cone(blocker, blocker->enemy, 0.3f);
		}

		if (enemy)
		{
			vec3_t bullseye;
			CalcEntitySpot(enemy, SPOT_CHEST, bullseye);
			bullseye[0] += Q_irand(-4, 4);
			bullseye[1] += Q_irand(-4, 4);
			bullseye[2] += Q_irand(-16, 4);
			VectorSubtract(bullseye, missile->currentOrigin, bounce_dir);
			VectorNormalize(bounce_dir);

			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim))
				{
					//moderately more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.2f, 0.2f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}

			VectorNormalize(bounce_dir);

			if (blocker->client->ps.forcePower < BLOCKPOINTS_THIRTY)
			{
				//very Low points = bad blocks
				if (blocker->client->ps.forcePower < BLOCKPOINTS_FATIGUE)
				{
					//Low points = bad blocks
					WP_BoltBlockVictimFatigued(blocker);
					blocker->client->ps.saberBlocked = BLOCKED_NONE;
					blocker->client->ps.saberBounceMove = LS_NONE;
				}
				else
				{
					WP_SaberBounceDirection(blocker, missile->currentOrigin, qfalse);
				}
			}
			else
			{
				WP_SaberBlockBolt_MD(blocker, missile->currentOrigin, qfalse);

				if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
					G_ControlledByPlayer(blocker)))
				{
					gi.Printf(S_COLOR_CYAN"NPC normal MD Bolt Block\n");
				}
			}

			int force_points_used_used;

			if (accurate_missile_blocking)
			{
				// excellent
				force_points_used_used = 2;
			}
			else
			{
				force_points_used_used = WP_SaberBoltBlockCost(blocker, missile);
			}

			if (blocker->client->ps.forcePower < force_points_used_used)
			{
				blocker->client->ps.forcePower = 0;
			}
			else
			{
				WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, force_points_used_used);
			}

			if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				Com_Printf(S_COLOR_CYAN"MD Mode G_ReflectMissileNPC\n");
			}
			reflected = qtrue;
		}
	}

	if (!reflected) //GOES TO CROSSHAIR AREA
	{
		if (d_JediAI->integer || d_blockinfo->integer || g_DebugSaberCombat->integer)
		{
			Com_Printf(S_COLOR_CYAN"Saber Not Blocked but Reflected off saber.\n");
		}

		if (blocker && blocker->s.client_num >= MAX_CLIENTS)
		{
			if (missile->owner && missile->s.weapon != WP_SABER)
			{
				//bounce back at them if you can
				VectorSubtract(missile->owner->currentOrigin, missile->currentOrigin, bounce_dir);
				VectorNormalize(bounce_dir);
			}
			else
			{
				vec3_t missile_dir;

				VectorSubtract(ent->currentOrigin, missile->currentOrigin, missile_dir);
				VectorCopy(missile->s.pos.trDelta, bounce_dir);
				VectorScale(bounce_dir, DotProduct(forward, missile_dir), bounce_dir);
				VectorNormalize(bounce_dir);
			}
		}
		else //deflect off at an angle.
		{
			vec3_t deflect_dir, missile_dir;
			float force_factor;
			VectorSubtract(g_crosshairWorldCoord, missile->currentOrigin, deflect_dir);
			VectorCopy(missile->s.pos.trDelta, missile_dir);
			VectorNormalize(missile_dir);
			VectorNormalize(deflect_dir);

			//bigger forceFactors make the reflected shots go closer to the crosshair
			switch (blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE])
			{
			case FORCE_LEVEL_1:
				force_factor = 2.0f;
				break;
			case FORCE_LEVEL_2:
				force_factor = 3.0f;
				break;
			default:
				force_factor = 10.0f;
				break;
			}

			VectorMA(missile_dir, force_factor, deflect_dir, bounce_dir);

			VectorNormalize(bounce_dir);
		}
		if (blocker->s.weapon == WP_SABER && blocker->client)
		{
			//saber
			if (blocker->client->ps.saberInFlight)
			{
				//reflecting off a thrown saber is totally wild
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.8f, 0.8f);
				}
			}
			else if (blocker->client->ps.forcePowerLevel[FP_SABER_DEFENSE] <= FORCE_LEVEL_1)
			{
				// at level 1 or below
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			else
			{
				// at level 2
				for (i = 0; i < 3; i++)
				{
					bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
				}
			}
			if (!PM_SaberInParry(blocker->client->ps.saber_move)
				&& !PM_SaberInReflect(blocker->client->ps.saber_move)
				&& !PM_SaberInIdle(blocker->client->ps.saber_move))
			{
				//a bit more wild
				if (PM_SaberInAttack(blocker->client->ps.saber_move)
					|| PM_SaberInTransitionAny(blocker->client->ps.saber_move)
					|| pm_saber_in_special_attack(blocker->client->ps.torsoAnim))
				{
					//really wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.3f, 0.3f);
					}
				}
				else
				{
					//mildly more wild
					for (i = 0; i < 3; i++)
					{
						bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
					}
				}
			}
		}
		else
		{
			//some other kind of reflection
			for (i = 0; i < 3; i++)
			{
				bounce_dir[i] += Q_flrand(-0.4f, 0.4f);
			}
		}
		WP_ForcePowerDrain(blocker, FP_SABER_DEFENSE, punish);
		wp_bolt_block_victim_reflected(blocker);
		blocker->client->ps.saberBlocked = BLOCKED_NONE;
		blocker->client->ps.saberBounceMove = LS_NONE;
	}

	VectorNormalize(bounce_dir);
	VectorScale(bounce_dir, speed, missile->s.pos.trDelta);
#ifdef _DEBUG
	assert(
		!Q_isnan(missile->s.pos.trDelta[0]) && !Q_isnan(missile->s.pos.trDelta[1]) && !Q_isnan(missile->s.pos.trDelta[2]
		));
#endif// _DEBUG
	missile->s.pos.trTime = level.time - 10; // move a bit on the very first frame
	VectorCopy(missile->currentOrigin, missile->s.pos.trBase);
	if (missile->s.weapon != WP_SABER)
	{
		//you are mine, now!
		if (!missile->lastEnemy)
		{
			//remember who originally shot this missile
			missile->lastEnemy = missile->owner;
		}
		missile->owner = blocker;
	}
	if (missile->s.weapon == WP_ROCKET_LAUNCHER)
	{
		//stop homing
		missile->e_ThinkFunc = thinkF_NULL;
	}
}

void WP_HandleBoltBlock(gentity_t* player, gentity_t* bolt, vec3_t forward)
{
	//handles all the behavior needed to saber block a blaster bolt.
	if (g_SerenityJediEngineMode->integer == 2)
	{
		wp_handle_bolt_block_sje_blockpoints(player, bolt, forward);
	}
	else
	{
		wp_handle_bolt_block_sje_forcepoints(player, bolt, forward);
	}
}

gentity_t* fire_stun(gentity_t* self, vec3_t start, vec3_t dir)
{
	VectorNormalize(dir);

	gentity_t* stun = G_Spawn();
	stun->classname = "stun";
	stun->nextthink = level.time + 10000;
	stun->e_ThinkFunc = thinkF_Weapon_StunFree;
	stun->s.eType = ET_MISSILE;
	stun->svFlags = SVF_USE_CURRENT_ORIGIN;
	stun->s.weapon = WP_STUN_BATON;
	stun->ownerNum = self->s.number;
	stun->methodOfDeath = MOD_ELECTROCUTE;
	stun->clipmask = MASK_SHOT;
	stun->parent = self;
	stun->target_ent = nullptr;
	stun->s.pos.trType = TR_LINEAR;
	stun->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;
	stun->s.otherentity_num = self->s.number;
	VectorCopy(start, stun->s.pos.trBase);
	VectorScale(dir, 2000, stun->s.pos.trDelta);
	SnapVector(stun->s.pos.trDelta);
	VectorCopy(start, stun->currentOrigin);
	self->client->stun = stun;

	return stun;
}

constexpr auto HOOK_PRESTEP_TIME = 50;
/*
=================
fire_grapple
=================
*/
gentity_t* fire_grapple(gentity_t* self, vec3_t start, vec3_t dir)
{
	VectorNormalize(dir);

	gentity_t* hook = G_Spawn();
	hook->classname = "hook";
	hook->nextthink = level.time + 10000;
	hook->e_ThinkFunc = thinkF_Weapon_HookFree;
	hook->s.eType = ET_MISSILE;
	hook->svFlags = SVF_USE_CURRENT_ORIGIN;
	hook->s.weapon = WP_MELEE;
	hook->ownerNum = self->s.number;
	hook->methodOfDeath = MOD_IMPACT;
	hook->clipmask = MASK_SHOT;
	hook->parent = self;
	hook->target_ent = nullptr;
	hook->s.pos.trType = TR_LINEAR;
	hook->s.pos.trTime = level.time - HOOK_PRESTEP_TIME;
	hook->s.otherentity_num = self->s.number;
	VectorCopy(start, hook->s.pos.trBase);
	VectorScale(dir, 1200, hook->s.pos.trDelta);
	SnapVector(hook->s.pos.trDelta);
	VectorCopy(start, hook->currentOrigin);
	self->client->hook = hook;

	return hook;
}