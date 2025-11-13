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

#include "common_headers.h"

#include "../rd-common/tr_public.h"

// bg_pmove.c -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate
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

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file

#define	GAME_INCLUDE
#include "../qcommon/q_shared.h"
#include "g_shared.h"
#include "bg_local.h"
#include "g_local.h"
#include "g_functions.h"
#include "anims.h"
#include "../cgame/cg_local.h"	// yeah I know this is naughty, but we're shipping soon...
#include "wp_saber.h"
#include "g_vehicles.h"

extern qboolean G_DoDismemberment(gentity_t* self, vec3_t point, int mod, int hit_loc,
	qboolean force = qfalse);
extern qboolean rocket_trooper_player(const gentity_t* self);
extern qboolean G_EntIsUnlockedDoor(int entityNum);
extern qboolean G_EntIsDoor(int entityNum);
extern qboolean InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern qboolean Q3_TaskIDPending(const gentity_t* ent, taskID_t taskType);
extern qboolean WP_SaberLose(gentity_t* self, vec3_t throw_dir);
extern qboolean PM_HasAnimation(const gentity_t* ent, int animation);
extern saber_moveName_t PM_SaberAnimTransitionMove(saber_moveName_t curmove, saber_moveName_t newmove);
extern saber_moveName_t PM_AttackMoveForQuad(int quad);
extern qboolean PM_SaberInTransition(int move);
extern qboolean PM_SaberInTransitionAny(int move);
extern qboolean PM_SaberInBounce(int move);
extern qboolean pm_saber_in_special_attack(int anim);
extern qboolean PM_SaberInAttack(int move);
extern qboolean PM_InAnimForSaberMove(int anim, int saber_move);
extern saber_moveName_t PM_SaberBounceForAttack(int move);
extern saber_moveName_t PM_SaberAttackForMovement(int forwardmove, int rightmove, int curmove);
extern saber_moveName_t PM_BrokenParryForParry(int move);
extern saber_moveName_t PM_KnockawayForParry(int move);
extern qboolean PM_SaberInParry(int move);
extern qboolean PM_SaberInKnockaway(int move);
extern qboolean PM_SaberInBrokenParry(int move);
extern qboolean PM_SaberInReflect(int move);
extern qboolean PM_SaberInIdle(int move);
extern qboolean PM_SaberInStart(int move);
extern qboolean PM_SaberInReturn(int move);
extern qboolean PM_SaberKataDone(int curmove, int newmove);
extern qboolean PM_SaberInSpecial(int move);
extern qboolean PM_InDeathAnim();
extern qboolean PM_StandingAnim(int anim);
extern qboolean PM_KickMove(int move);
extern qboolean PM_KickingAnim(int anim);
extern qboolean PM_InAirKickingAnim(int anim);
extern qboolean PM_SuperBreakLoseAnim(int anim);
extern qboolean PM_SuperBreakWinAnim(int anim);
extern qboolean PM_InCartwheel(int anim);
extern qboolean PM_InButterfly(int anim);
extern qboolean PM_CanRollFromSoulCal(const playerState_t* ps);
extern saber_moveName_t PM_SaberFlipOverAttackMove();
extern qboolean PM_CheckFlipOverAttackMove(qboolean check_enemy);
extern saber_moveName_t PM_SaberJumpForwardAttackMove();
extern saber_moveName_t PM_SaberBackflipAttackMove();
extern qboolean PM_CheckBackflipAttackMove();
extern saber_moveName_t PM_SaberLungeAttackMove(qboolean fallback_to_normal_lunge);
extern qboolean PM_InSecondaryStyle();
extern qboolean PM_KnockDownAnimExtended(int anim);
extern void G_StartMatrixEffect(const gentity_t* ent, int me_flags = 0, int length = 1000, float time_scale = 0.0f, int spin_time = 0);
extern void WP_ForcePowerStop(gentity_t* self, forcePowers_t force_power);
extern qboolean WP_ForcePowerAvailable(const gentity_t* self, forcePowers_t forcePower, int overrideAmt);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t forcePower, int overrideAmt);
extern float G_ForceWallJumpStrength();
extern qboolean G_CheckRollSafety(const gentity_t* self, int anim, float test_dist);
extern saber_moveName_t PM_CheckPullAttack();
extern qboolean JET_Flying(const gentity_t* self);
extern void JET_FlyStart(gentity_t* self);
extern void jet_fly_stop(gentity_t* self);
extern qboolean PM_LockedAnim(int anim);
extern qboolean G_TryingKataAttack(const usercmd_t* cmd);
extern qboolean G_TryingCartwheel(const gentity_t* self, const usercmd_t* cmd);
extern qboolean G_TryingJumpAttack(const gentity_t* self, const usercmd_t* cmd);
extern qboolean G_TryingJumpForwardAttack(const gentity_t* self, const usercmd_t* cmd);
extern void WP_SaberSwingSound(const gentity_t* ent, int saber_num, swingType_t swing_type);
extern qboolean WP_UseFirstValidSaberStyle(const gentity_t* ent, int* saber_anim_level);
extern qboolean WP_SaberStyleValidForSaber(const gentity_t* ent, int saber_anim_level);
extern int jedi_re_calc_parry_time(const gentity_t* self, evasionType_t evasion_type);
extern qboolean PM_SaberInKata(saber_moveName_t saber_move);
extern qboolean PM_InKataAnim(int anim);
extern qboolean Block_Button_Held(const gentity_t* defender);
extern qboolean IsSurrendering(const gentity_t* self);
extern qboolean PM_SaberDoDamageAnim(int anim);
qboolean PM_InKnockDown(const playerState_t* ps);
qboolean PM_InKnockDownOnGround(playerState_t* ps);
qboolean PM_InGetUp(const playerState_t* ps);
qboolean PM_InRoll(const playerState_t* ps);
qboolean PM_SpinningSaberAnim(int anim);
qboolean PM_GettingUpFromKnockDown(float standheight, float crouchheight);
qboolean PM_SpinningAnim(int anim);
qboolean PM_FlippingAnim(int anim);
qboolean PM_PainAnim(int anim);
qboolean PM_RollingAnim(int anim);
qboolean PM_SwimmingAnim(int anim);
qboolean PM_InReboundJump(int anim);
qboolean PM_ForceJumpingAnim(int anim);
void PM_CmdForRoll(playerState_t* ps, usercmd_t* p_cmd);
extern qboolean PM_SaberInAttackPure(int move);
extern qboolean PM_StandingidleAnim(int move);
extern cvar_t* g_SerenityJediEngineMode;
extern cvar_t* g_RealisticBlockingMode;
extern qboolean PM_SaberInDamageMove(int move);
extern saber_moveName_t PM_BrokenParryForAttack(int move);
extern void G_StartStasisEffect_FORCE_LEVEL_1(const gentity_t* ent, int me_flags = 0, int length = 1000,
	float time_scale = 0.0f, int spin_time = 0);
extern cvar_t* d_slowmoaction;
extern void G_SetWeapon(gentity_t* self, int wp);
extern cvar_t* g_AllowLedgeGrab;
extern cvar_t* g_allowslipping;
extern qboolean PM_CheckLungeAttackMove();
extern qboolean PM_FaceProtectAnim(int anim);
extern cvar_t* g_debugFatigueBars;
extern qboolean PlayerAffectedByStasis();
extern qboolean G_TryingSpecial(const usercmd_t* cmd);
extern void TurnBarrierOff(gentity_t* ent);
extern int parryDebounce[];
extern qboolean cg_usingInFrontOf;
extern qboolean player_locked;
extern qboolean MatrixMode;
qboolean waterForceJump;
extern cvar_t* g_timescale;
extern cvar_t* g_speederControlScheme;
extern cvar_t* d_slowmodeath;
extern cvar_t* g_saberNewControlScheme;
extern cvar_t* g_stepSlideFix;
extern cvar_t* g_saberAutoBlocking;
extern cvar_t* g_noIgniteTwirl;
extern cvar_t* g_allowdualpistols;
extern void ItemUse_Jetpack(const gentity_t* ent);
extern void Jetpack_Off(const gentity_t* ent);
static void PM_SetWaterLevelAtPoint(vec3_t org, int* waterlevel, int* watertype);
extern qboolean Bokatan_Dual_Clone_Pistol(const gentity_t* self);
extern qboolean Calo_Nord(const gentity_t* self);
extern cvar_t* g_SaberAttackSpeedMD;
extern cvar_t* g_RealisticBlockingMode;

constexpr auto FLY_NONE = 0;
constexpr auto FLY_NORMAL = 1;
constexpr auto FLY_VEHICLE = 2;
constexpr auto FLY_HOVER = 3;
int Flying = FLY_NONE;

pmove_t* pm;
pml_t pml;

// movement parameters
constexpr float pm_stopspeed = 100.0f;
constexpr float pm_duckScale = 0.50f;
constexpr float pm_swimScale = 0.50f;
constexpr float pm_ladderScale = 0.7f;
constexpr float pm_Laddeeraccelerate = 1.0f;

constexpr float pm_vehicleaccelerate = 36.0f;
constexpr float pm_accelerate = 12.0f;
constexpr float pm_airaccelerate = 4.0f;
constexpr float pm_wateraccelerate = 4.0f;
constexpr float pm_flyaccelerate = 8.0f;

constexpr float pm_friction = 6.0f;
constexpr float pm_waterfriction = 1.0f;
constexpr float pm_flightfriction = 3.0f;

//const float	pm_frictionModifier	= 3.0f;	//Used for "careful" mode (when pressing use)
constexpr float pm_airDecelRate = 1.35f; //Used for air decelleration away from current movement velocity

int c_pmove = 0;

extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torso_anim_timer, int time);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legs_anim_timer, int time);
extern void PM_TorsoAnimation();
extern int PM_TorsoAnimForFrame(gentity_t* ent, int torso_frame);
extern int PM_AnimLength(int index, animNumber_t anim);
extern qboolean PM_InOnGroundAnim(playerState_t* ps);
extern weaponInfo_t cg_weapons[MAX_WEAPONS];
extern int PM_PickAnim(const gentity_t* self, int min_anim, int max_anim);

extern void DoImpact(gentity_t* self, gentity_t* other, qboolean damageSelf, const trace_t* trace);

constexpr auto PHASER_RECHARGE_TIME = 100;
extern saber_moveName_t transitionMove[Q_NUM_QUADS][Q_NUM_QUADS];

extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);

static Vehicle_t* PM_RidingVehicle()
{
	return G_IsRidingVehicle(pm->gent);
}

extern qboolean G_ControlledByPlayer(const gentity_t* self);

qboolean PM_ControlledByPlayer()
{
	return G_ControlledByPlayer(pm->gent);
}

qboolean BG_UnrestrainedPitchRoll(const playerState_t* ps, const Vehicle_t* p_veh)
{
	if (ps->client_num < MAX_CLIENTS //real client
		&& ps->m_iVehicleNum //in a vehicle
		&& p_veh //valid vehicle data pointer
		&& p_veh->m_pVehicleInfo //valid vehicle info
		&& p_veh->m_pVehicleInfo->type == VH_FIGHTER) //fighter
	{
		//can roll and pitch without limitation!
		return qtrue;
	}
	return qfalse;
}

qboolean BG_AllowThirdPersonSpecialMove(const playerState_t* ps)
{
	return static_cast<qboolean>((cg.renderingThirdPerson || cg_trueguns.integer || ps->weapon == WP_SABER || ps->weapon
		==
		WP_MELEE) && !cg.zoomMode);
}

/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent(const int new_event)
{
	AddEventToPlayerstate(new_event, 0, pm->ps);
}

static qboolean PM_PredictJumpSafe()
{
	return qtrue;
}

static void PM_GrabWallForJump(const int anim)
{
	//NOTE!!! assumes an appropriate anim is being passed in!!!
	PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_RESTART | SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
	PM_AddEvent(EV_JUMP); //make sound for grab
	pm->ps->pm_flags |= PMF_STUCK_TO_WALL;
}

//The FP cost of Force Fall
constexpr auto FM_FORCEFALL = 10;

//the velocity to which Force Fall activates and tries to slow you to.
constexpr auto FORCEFALLVELOCITY = -250;

//Rate at which the player brakes
int ForceFallBrakeRate[NUM_FORCE_POWER_LEVELS] =
{
	0, //Can't brake with zero Force Jump skills
	60,
	80,
	100,
};

//time between Force Fall braking actions.
constexpr auto FORCEFALLDEBOUNCE = 100;

static qboolean PM_CanForceFall()
{
	if (!PM_InRoll(pm->ps) // not rolling
		&& !PM_InKnockDown(pm->ps) // not knocked down
		&& !PM_InDeathAnim() // not dead
		&& !pm_saber_in_special_attack(pm->ps->torsoAnim) // not doing special attack
		&& !PM_SaberInAttack(pm->ps->saber_move) // not attacking
		&& !(pm->ps->pm_flags & PMF_JUMP_HELD) // have to have released jump since last press
		&& pm->cmd.upmove > 10 // pressing the jump button
		&& pm->ps->velocity[2] < FORCEFALLVELOCITY // falling
		&& pm->ps->groundEntityNum == ENTITYNUM_NONE // in the air
		&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1 //have force jump level 2 or above
		&& pm->ps->forcePower > FM_FORCEFALL // have atleast 5 force power points
		&& pm->waterlevel < 2 // above water level
		&& pm->ps->gravity > 0 // not in zero-g
		&& g_SerenityJediEngineMode->integer)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_InForceFall()
{
	const char* info = CG_ConfigString(CS_SERVERINFO);
	const char* s = Info_ValueForKey(info, "mapname");

	if (in_camera ||
		strcmp(s, "doom_shields") == 0 ||
		strcmp(s, "md_df2_lvl4") == 0 ||
		strcmp(s, "duel_tower") == 0)
	{
		return qfalse;
	}

	// can player force fall?
	if (PM_CanForceFall())
	{
		const int ff_debounce = pm->ps->forcePowerDebounce[FP_LEVITATION] - pm->ps->forcePowerLevel[FP_LEVITATION] * 100;

		PM_SetAnim(pm, SETANIM_BOTH, BOTH_FORCEINAIR1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 150);

		// reduce falling velocity to a safe speed at set intervals
		if (ff_debounce + FORCEFALLDEBOUNCE < pm->cmd.serverTime)
		{
			if (pm->ps->velocity[2] < FORCEFALLVELOCITY)
			{
				if (FORCEFALLVELOCITY - pm->ps->velocity[2] < ForceFallBrakeRate[pm->ps->forcePowerLevel[
					FP_LEVITATION]])
				{
					pm->ps->velocity[2] = FORCEFALLVELOCITY;
				}
				else
				{
					pm->ps->velocity[2] += ForceFallBrakeRate[pm->ps->forcePowerLevel[FP_LEVITATION]];
				}
			}
		}

		pm->ps->powerups[PW_INVINCIBLE] = pm->cmd.serverTime + 100;

		// is it time to reduce the players force power
		if (pm->ps->forcePowerDebounce[FP_LEVITATION] < pm->cmd.serverTime)
		{
			int force_mana_modifier = 0;
			// reduced the use of force power for duel and power duel matches
			if (g_SerenityJediEngineMode->integer == 2 && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_2)
			{
				force_mana_modifier = -4;
			}
			WP_ForcePowerDrain(pm->gent, FP_HEAL, FM_FORCEFALL + force_mana_modifier);

			// removes force power at a rate of 0.1 secs * force jump level
			pm->ps->forcePowerDebounce[FP_LEVITATION] = pm->cmd.serverTime + pm->ps->forcePowerLevel[FP_LEVITATION] *
				100;
		}

		// player is force falling
		return qtrue;
	}

	// player is not force falling
	return qfalse;
}

qboolean PM_CheckGrabWall(const trace_t* trace)
{
	if (!pm->gent || !pm->gent->client)
	{
		return qfalse;
	}
	if (pm->gent->health <= 0)
	{
		//must be alive
		return qfalse;
	}
	if (pm->gent->client->ps.groundEntityNum != ENTITYNUM_NONE)
	{
		//must be in air
		return qfalse;
	}
	if (trace->plane.normal[2] != 0)
	{
		//must be a flat wall
		return qfalse;
	}
	if (!trace->plane.normal[0] && !trace->plane.normal[1])
	{
		//invalid normal
		return qfalse;
	}
	if (trace->contents & (CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP))
	{
		//can't jump off of clip brushes
		return qfalse;
	}
	if (pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_1)
	{
		//must have at least FJ 1
		return qfalse;
	}
	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_1)
	{
		//player must have force jump 3
		return qfalse;
	}
	if (pm->ps->saber[0].saberFlags & SFL_NO_WALL_GRAB)
	{
		return qfalse;
	}
	if (pm->ps->dualSabers
		&& pm->ps->saber[1].saberFlags & SFL_NO_WALL_GRAB)
	{
		return qfalse;
	}
	if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		//player
		//only if we were in a longjump
		if (pm->ps->legsAnim != BOTH_FORCELONGLEAP_START
			&& pm->ps->legsAnim != BOTH_FORCELONGLEAP_ATTACK)
		{
			return qfalse;
		}
		//hit a flat wall during our long jump, see if we should grab it
		vec3_t move_dir;
		VectorCopy(pm->ps->velocity, move_dir);
		VectorNormalize(move_dir);
		if (DotProduct(move_dir, trace->plane.normal) > -0.65f)
		{
			//not enough of a direct impact, just slide off
			return qfalse;
		}
		if (fabs(trace->plane.normal[2]) > MAX_WALL_GRAB_SLOPE)
		{
			return qfalse;
		}
		//grab it!
		//FIXME: stop Matrix effect!
		VectorClear(pm->ps->velocity);
		//FIXME: stop slidemove!
		//NOTE: we know it's forward, so...
		PM_GrabWallForJump(BOTH_FORCEWALLREBOUND_FORWARD);
		return qtrue;
	}
	//NPCs
	if (PM_InReboundJump(pm->ps->legsAnim))
	{
		//already in a rebound!
		return qfalse;
	}
	if (pm->ps->eFlags & EF_FORCE_GRIPPED)
	{
		//being gripped!
		return qfalse;
	}
	if (pm->ps->eFlags & EF_FORCE_GRASPED)
	{
		//being gripped!
		return qfalse;
	}
	if (pm->gent->NPC && pm->gent->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
	{
		//faling to our death!
		return qfalse;
	}
	//FIXME: random chance, based on skill/rank?
	if (pm->ps->legsAnim != BOTH_FORCELONGLEAP_START
		&& pm->ps->legsAnim != BOTH_FORCELONGLEAP_ATTACK)
	{
		//not in a long-jump
		if (!pm->gent->enemy)
		{
			//no enemy
			return qfalse;
		}
		//see if the enemy is in the direction of the wall or above us
		//if ( pm->gent->enemy->currentOrigin[2] < (pm->ps->origin[2]-128) )
		{
			//enemy is way below us
			vec3_t enemy_dir;
			VectorSubtract(pm->gent->enemy->currentOrigin, pm->ps->origin, enemy_dir);
			enemy_dir[2] = 0;
			VectorNormalize(enemy_dir);
			if (DotProduct(enemy_dir, trace->plane.normal) < 0.65f)
			{
				//jumping off this wall would not launch me in the general direction of my enemy
				return qfalse;
			}
		}
	}
	//FIXME: check for ground close beneath us?
	//FIXME: check for obstructions in the dir we're going to jump
	//		- including "do not enter" brushes!
	//hit a flat wall during our long jump, see if we should grab it
	vec3_t move_dir;
	VectorCopy(pm->ps->velocity, move_dir);
	VectorNormalize(move_dir);
	if (DotProduct(move_dir, trace->plane.normal) > -0.65f)
	{
		//not enough of a direct impact, just slide off
		return qfalse;
	}

	//Okay, now see if jumping off this thing would send us into a do not enter brush
	if (!PM_PredictJumpSafe())
	{
		//we would hit a do not enter brush, so don't grab the wall
		return qfalse;
	}

	//grab it!
	//Pick the proper anim
	int anim;
	vec3_t facing_angles, wall_dir, fwd_dir, rt_dir;
	VectorSubtract(trace->endpos, pm->gent->lastOrigin, wall_dir);
	wall_dir[2] = 0;
	VectorNormalize(wall_dir);
	VectorSet(facing_angles, 0, pm->ps->viewangles[YAW], 0);
	AngleVectors(facing_angles, fwd_dir, rt_dir, nullptr);
	const float f_dot = DotProduct(fwd_dir, wall_dir);
	if (fabs(f_dot) >= 0.5f)
	{
		//hit a wall in front/behind
		if (f_dot > 0.0f)
		{
			//in front
			anim = BOTH_FORCEWALLREBOUND_FORWARD;
		}
		else
		{
			//behind
			anim = BOTH_FORCEWALLREBOUND_BACK;
		}
	}
	else if (DotProduct(rt_dir, wall_dir) > 0)
	{
		//hit a wall on the right
		anim = BOTH_FORCEWALLREBOUND_RIGHT;
	}
	else
	{
		//hit a wall on the left
		anim = BOTH_FORCEWALLREBOUND_LEFT;
	}
	VectorClear(pm->ps->velocity);
	//FIXME: stop slidemove!
	PM_GrabWallForJump(anim);
	return qtrue;
	//return qfalse;
}

/*
===============
qboolean PM_ClientImpact( trace_t *trace, qboolean damageSelf )

===============
*/
qboolean PM_ClientImpact(const trace_t* trace, const qboolean damage_self)
{
	const int other_entity_num = trace->entityNum;

	if (!pm->gent)
	{
		return qfalse;
	}

	const gentity_t* traceEnt = &g_entities[other_entity_num];

	if (other_entity_num == ENTITYNUM_WORLD || traceEnt->bmodel && traceEnt->s.pos.trType == TR_STATIONARY)
	{
		//hit world or a non-moving brush
		if (PM_CheckGrabWall(trace))
		{
			//stopped on the wall
			return qtrue;
		}
	}

	if (VectorLength(pm->ps->velocity) * (pm->gent->mass / 10) >= 100 && (pm->gent->client->NPC_class == CLASS_VEHICLE
		|| pm->ps->lastOnGround + 100 < level.time))
		//was 300 ||(other->material>=MAT_GLASS&&pm->gent->lastImpact+100<=level.time))
	{
		DoImpact(pm->gent, &g_entities[other_entity_num], damage_self, trace);
	}

	if (other_entity_num >= ENTITYNUM_WORLD)
	{
		return qfalse;
	}

	if (!traceEnt || !(traceEnt->contents & pm->tracemask))
	{
		//it's dead or not in my way anymore
		return qtrue;
	}

	return qfalse;
}

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt(const int entityNum)
{
	if (entityNum == ENTITYNUM_WORLD)
	{
		return;
	}
	if (pm->numtouch == MAXTOUCH)
	{
		return;
	}

	// see if it is already added
	for (int i = 0; i < pm->numtouch; i++)
	{
		if (pm->touchents[i] == entityNum)
		{
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entityNum;
	pm->numtouch++;
}

/*
==================
PM_ClipVelocity

Slide off of the impacting surface

  This will pull you down onto slopes if heading away from
  them and push you up them as you go up them.
  Also stops you when you hit walls.

==================
*/
void PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, const float overbounce)
{
	if (pm->ps->pm_flags & PMF_STUCK_TO_WALL)
	{
		//no sliding!
		VectorCopy(in, out);
		return;
	}
	const float old_in_z = in[2];

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
	if (g_stepSlideFix->integer)
	{
		if (pm->ps->client_num < MAX_CLIENTS //normal player
			&& normal[2] < MIN_WALK_NORMAL) //sliding against a steep slope
		{
			if (pm->ps->groundEntityNum != ENTITYNUM_NONE) //on the ground
			{
				//if walking on the ground, don't slide up slopes that are too steep to walk on
				out[2] = old_in_z;
			}
		}
	}
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction()
{
	vec3_t vec;
	float control;
	float friction = pm->ps->friction;

	float* vel = pm->ps->velocity;

	VectorCopy(vel, vec);
	if (pml.walking)
	{
		vec[2] = 0; // ignore slope movement
	}

	const float speed = VectorLength(vec);
	if (speed < 1)
	{
		vel[0] = 0;
		vel[1] = 0; // allow sinking underwater
		// FIXME: still have z friction underwater?
		return;
	}

	float drop = 0;

	// apply ground friction, even if on ladder
	if (pm->gent
		&& pm->gent->client
		&& pm->gent->client->NPC_class == CLASS_VEHICLE && pm->gent->m_pVehicle
		&& pm->gent->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL)
	{
		friction = pm->gent->m_pVehicle->m_pVehicleInfo->friction;

		if (pm->gent->m_pVehicle && pm->gent->m_pVehicle->m_pVehicleInfo->hoverHeight > 0)
		{
			//in a hovering vehicle, have air control
			if (pm->gent->m_pVehicle->m_ulFlags & VEH_FLYING)
			{
				friction = 0.10f;
			}
		}

		if (!(pm->ps->pm_flags & PMF_TIME_KNOCKBACK) && !(pm->ps->pm_flags & PMF_TIME_NOFRICTION))
		{
			control = speed < pm_stopspeed ? pm_stopspeed : speed;
			drop += control * friction * pml.frametime;
		}
	}
	else if (Flying != FLY_NORMAL)
	{
		if (pm->watertype & CONTENTS_LADDER || pm->waterlevel <= 1)
		{
			if (pm->watertype & CONTENTS_LADDER || pml.walking && !(pml.groundTrace.surfaceFlags & SURF_SLICK))
			{
				// if getting knocked back, no friction
				if (!(pm->ps->pm_flags & PMF_TIME_KNOCKBACK) && !(pm->ps->pm_flags & PMF_TIME_NOFRICTION))
				{
					if (pm->ps->legsAnim == BOTH_FORCELONGLEAP_START
						|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_ATTACK
						|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_LAND)
					{
						//super forward jump
						if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
						{
							//not in air
							if (pm->cmd.forwardmove < 0)
							{
								//trying to hold back some
								friction *= 0.5f; //0.25f;
							}
							else
							{
								//free slide
								friction *= 0.2f; //0.1f;
							}
							pm->cmd.forwardmove = pm->cmd.rightmove = 0;
							if (pml.groundPlane && pm->ps->legsAnim == BOTH_FORCELONGLEAP_LAND)
							{
								//slide effect
								G_PlayEffect("env/slide_dust", pml.groundTrace.endpos, pml.groundTrace.plane.normal);
								//FIXME: slide sound
							}
						}
					}

					control = speed < pm_stopspeed ? pm_stopspeed : speed;
					drop += control * friction * pml.frametime;
				}
			}
		}
	}
	else if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& pm->gent
		&& pm->gent->client
		&& (pm->gent->client->NPC_class == CLASS_BOBAFETT || pm->gent->client->NPC_class == CLASS_MANDALORIAN || pm->
			gent->client->NPC_class == CLASS_JANGO || pm->gent->client->NPC_class == CLASS_JANGODUAL || pm->gent->client
			->NPC_class == CLASS_ROCKETTROOPER) && pm->gent->client->moveType == MT_FLYSWIM)
	{
		//player as Boba
		drop += speed * pm_waterfriction * pml.frametime;
	}

	if (Flying == FLY_VEHICLE)
	{
		if (!(pm->ps->pm_flags & PMF_TIME_KNOCKBACK) && !(pm->ps->pm_flags & PMF_TIME_NOFRICTION))
		{
			control = speed < pm_stopspeed ? pm_stopspeed : speed;
			drop += control * friction * pml.frametime;
		}
	}

	// apply water friction even if just wading
	if (!waterForceJump)
	{
		if (pm->waterlevel && !(pm->watertype & CONTENTS_LADDER))
		{
			drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;
		}
	}
	// If on a client then there is no friction
	else if (pm->ps->groundEntityNum < MAX_CLIENTS)
	{
		drop = 0;
	}

	// apply flying friction
	if (pm->ps->pm_type == PM_SPECTATOR)
	{
		drop += speed * pm_flightfriction * pml.frametime;
	}

	// scale the velocity
	float newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/

static void PM_Accelerate(vec3_t wishdir, const float wishspeed, const float accel)
{
	const float currentspeed = DotProduct(pm->ps->velocity, wishdir);

	const float addspeed = wishspeed - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}
	float accelspeed = accel * pml.frametime * wishspeed;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}
	for (int i = 0; i < 3; i++)
	{
		pm->ps->velocity[i] += accelspeed * wishdir[i];
	}
}

/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale(const usercmd_t* cmd)
{
	int max = abs(cmd->forwardmove);

	if (abs(cmd->rightmove) > max)
	{
		max = abs(cmd->rightmove);
	}
	if (abs(cmd->upmove) > max)
	{
		max = abs(cmd->upmove);
	}
	if (!max)
	{
		return 0;
	}
	const float total = sqrt(static_cast<float>(cmd->forwardmove * cmd->forwardmove
		+ cmd->rightmove * cmd->rightmove
		+ cmd->upmove * cmd->upmove));

	const float scale = static_cast<float>(pm->ps->speed) * max / (127.0f * total);

	return scale;
}

/*
================
PM_SetMovementDir

Determine the rotation of the legs reletive
to the facing dir
================
*/
static void PM_SetMovementDir()
{
	if (pm->cmd.forwardmove || pm->cmd.rightmove)
	{
		if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0)
		{
			pm->ps->movementDir = 0;
		}
		else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0)
		{
			pm->ps->movementDir = 1;
		}
		else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0)
		{
			pm->ps->movementDir = 2;
		}
		else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0)
		{
			pm->ps->movementDir = 3;
		}
		else if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0)
		{
			pm->ps->movementDir = 4;
		}
		else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0)
		{
			pm->ps->movementDir = 5;
		}
		else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0)
		{
			pm->ps->movementDir = 6;
		}
		else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0)
		{
			pm->ps->movementDir = 7;
		}
	}
	else
	{
		// if they aren't actively going directly sideways,
		// change the animation to the diagonal so they
		// don't stop too crooked
		if (pm->ps->movementDir == 2)
		{
			pm->ps->movementDir = 1;
		}
		else if (pm->ps->movementDir == 6)
		{
			pm->ps->movementDir = 7;
		}
	}
}

/*
=============
PM_CheckJump
=============
*/
#define METROID_JUMP 1

qboolean PM_InReboundJump(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCEWALLREBOUND_FORWARD:
	case BOTH_FORCEWALLREBOUND_LEFT:
	case BOTH_FORCEWALLREBOUND_BACK:
	case BOTH_FORCEWALLREBOUND_RIGHT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InReboundHold(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCEWALLHOLD_FORWARD:
	case BOTH_FORCEWALLHOLD_LEFT:
	case BOTH_FORCEWALLHOLD_BACK:
	case BOTH_FORCEWALLHOLD_RIGHT:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_InReboundRelease(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCEWALLRELEASE_FORWARD:
	case BOTH_FORCEWALLRELEASE_LEFT:
	case BOTH_FORCEWALLRELEASE_BACK:
	case BOTH_FORCEWALLRELEASE_RIGHT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InWallHoldMove(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCEWALLHOLD_BACK:
	case BOTH_FORCEWALLHOLD_FORWARD:
	case BOTH_FORCEWALLHOLD_LEFT:
	case BOTH_FORCEWALLHOLD_RIGHT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InAmputateMove(const int anim)
{
	switch (anim)
	{
	case BOTH_RIGHTHANDCHOPPEDOFF:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InDrainPainMove(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCE_DRAIN_GRABBED:
	case BOTH_SONICPAIN_HOLD:
	case BOTH_WIND:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InLedgeMove(const int anim)
{
	switch (anim)
	{
	case BOTH_LEDGE_GRAB:
	case BOTH_LEDGE_HOLD:
	case BOTH_LEDGE_LEFT:
	case BOTH_LEDGE_RIGHT:
	case BOTH_LEDGE_MERCPULL:
	case BOTH_LEDGE_JEDIPULL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean In_LedgeIdle(const int anim)
{
	switch (anim)
	{
	case BOTH_LEDGE_GRAB:
	case BOTH_LEDGE_HOLD:
	case BOTH_LEDGE_LEFT:
	case BOTH_LEDGE_RIGHT:
	case BOTH_LEDGE_MERCPULL:
	case BOTH_LEDGE_JEDIPULL:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_InBackFlip(const int anim)
{
	switch (anim)
	{
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_FLIP_BACK3:
	case BOTH_ALORA_FLIP_B:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InSpecialJump(const int anim)
{
	switch (anim)
	{
	case BOTH_WALL_RUN_RIGHT:
	case BOTH_WALL_RUN_RIGHT_STOP:
	case BOTH_WALL_RUN_RIGHT_FLIP:
	case BOTH_WALL_RUN_LEFT:
	case BOTH_WALL_RUN_LEFT_STOP:
	case BOTH_WALL_RUN_LEFT_FLIP:
	case BOTH_WALL_FLIP_RIGHT:
	case BOTH_WALL_FLIP_LEFT:
	case BOTH_WALL_FLIP_BACK1:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPFLIPSLASHDOWN1: //#
	case BOTH_JUMPFLIPSTABDOWN: //#
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_ARIAL_LEFT:
	case BOTH_ARIAL_RIGHT:
	case BOTH_ARIAL_F1:
	case BOTH_CARTWHEEL_LEFT:
	case BOTH_CARTWHEEL_RIGHT:

	case BOTH_FORCELONGLEAP_START:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_FORCEWALLRUNFLIP_START:
	case BOTH_FORCEWALLRUNFLIP_END:
	case BOTH_FORCEWALLRUNFLIP_ALT:
	case BOTH_FLIP_ATTACK7:
	case BOTH_FLIP_HOLD7:
	case BOTH_FLIP_LAND:
	case BOTH_A7_SOULCAL:
		return qtrue;
	default:;
	}
	if (PM_InReboundJump(anim))
	{
		return qtrue;
	}
	if (PM_InReboundHold(anim))
	{
		return qtrue;
	}
	if (PM_InReboundRelease(anim))
	{
		return qtrue;
	}
	if (PM_InBackFlip(anim))
	{
		return qtrue;
	}
	return qfalse;
}

extern void CG_PlayerLockedWeaponSpeech(int jumping);

qboolean PM_ForceJumpingUp(const gentity_t* gent)
{
	if (!gent || !gent->client)
	{
		return qfalse;
	}

	if (gent->NPC)
	{
		//this is ONLY for the player
		if (player
			&& player->client
			&& player->client->ps.viewEntity == gent->s.number)
		{
			//okay to jump if an NPC controlled by the player
		}
		else
		{
			return qfalse;
		}
	}

	if (!(gent->client->ps.forcePowersActive & 1 << FP_LEVITATION) && gent->client->ps.forceJumpCharge)
	{
		//already jumped and let go
		return qfalse;
	}

	if (PM_InSpecialJump(gent->client->ps.legsAnim))
	{
		return qfalse;
	}

	if (PM_InKnockDown(&gent->client->ps))
	{
		return qfalse;
	}

	if ((gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(gent)) && in_camera)
	{
		//player can't use force powers in cinematic
		return qfalse;
	}
	if (gent->client->ps.groundEntityNum == ENTITYNUM_NONE //in air
		&& (gent->client->ps.pm_flags & PMF_JUMPING && gent->client->ps.velocity[2] > 0)
		//jumped & going up or at water surface///*(gent->client->ps.waterHeightLevel==WHL_SHOULDERS&&gent->client->usercmd.upmove>0) ||*/
		&& gent->client->ps.forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 //force-jump capable
		&& !(gent->client->ps.pm_flags & PMF_TRIGGER_PUSHED)) //not pushed by a trigger
	{
		if (gent->flags & FL_LOCK_PLAYER_WEAPONS)
			// yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
		{
			CG_PlayerLockedWeaponSpeech(qtrue);
			return qfalse;
		}
		return qtrue;
	}
	return qfalse;
}

static void PM_JumpForDir()
{
	int anim;
	if (pm->cmd.forwardmove > 0)
	{
		anim = BOTH_JUMP1;
		pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	}
	else if (pm->cmd.forwardmove < 0)
	{
		anim = BOTH_JUMPBACK1;
		pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
	}
	else if (pm->cmd.rightmove > 0)
	{
		anim = BOTH_JUMPRIGHT1;
		pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	}
	else if (pm->cmd.rightmove < 0)
	{
		anim = BOTH_JUMPLEFT1;
		pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	}
	else
	{
		anim = BOTH_JUMP1;
		pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	}
	if (!PM_InDeathAnim())
	{
		PM_SetAnim(pm, SETANIM_LEGS, anim, SETANIM_FLAG_OVERRIDE, 100); // Only blend over 100ms
	}
}

qboolean PM_GentCantJump(const gentity_t* gent)
{
	if (gent && gent->client &&
		(gent->client->NPC_class == CLASS_ATST ||
			gent->client->NPC_class == CLASS_GONK ||
			gent->client->NPC_class == CLASS_MARK1 ||
			gent->client->NPC_class == CLASS_MARK2 ||
			gent->client->NPC_class == CLASS_MOUSE ||
			gent->client->NPC_class == CLASS_PROBE ||
			gent->client->NPC_class == CLASS_PROTOCOL ||
			gent->client->NPC_class == CLASS_R2D2 ||
			gent->client->NPC_class == CLASS_R5D2 ||
			gent->client->NPC_class == CLASS_SEEKER ||
			gent->client->NPC_class == CLASS_REMOTE ||
			gent->client->NPC_class == CLASS_SENTRY))
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_Is_A_Dash_Anim(const int anim)
{
	switch (anim)
	{
	case BOTH_DASH_R:
	case BOTH_DASH_L:
	case BOTH_DASH_B:
	case BOTH_DASH_F:
	case BOTH_HOP_R:
	case BOTH_HOP_L:
	case BOTH_HOP_B:
	case BOTH_HOP_F:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_CheckJump()
{
	//Don't allow jump until all buttons are up
	if (pm->ps->pm_flags & PMF_RESPAWNED)
	{
		return qfalse;
	}

	if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps))
	{
		//in knockdown
		return qfalse;
	}

	if (PM_GentCantJump(pm->gent))
	{
		return qfalse;
	}

	if (PM_KickingAnim(pm->ps->legsAnim) && !PM_InAirKickingAnim(pm->ps->legsAnim))
	{
		//can't jump when in a kicking anim
		return qfalse;
	}

	if (PM_InForceFall())
	{
		return qfalse;
	}

	if (pm->ps->communicatingflags & 1 << DASHING || PM_Is_A_Dash_Anim(pm->ps->torsoAnim))
	{
		return qfalse;
	}

#if METROID_JUMP
	if (pm->ps->hookhasbeenfired || pm->cmd.buttons & BUTTON_GRAPPLE)
	{
		//  turn off force levitation if grapple is used
		// also remove protection from a fall
		pm->ps->forcePowersActive &= ~(1 << FP_LEVITATION);
		pm->ps->forceJumpZStart = 0;
	}
	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& pm->gent && pm->gent->client && pm->ps->jetpackFuel > 15
		&& (pm->gent->client->NPC_class == CLASS_BOBAFETT ||
			pm->gent->client->NPC_class == CLASS_MANDALORIAN ||
			pm->gent->client->NPC_class == CLASS_JANGO ||
			pm->gent->client->NPC_class == CLASS_JANGODUAL ||
			pm->gent->client->NPC_class == CLASS_ROCKETTROOPER || rocket_trooper_player(pm->gent)) && !pm->ps->hookhasbeenfired)
	{
		//player playing as boba fett
		if (pm->cmd.upmove > 0)
		{
			//turn on/go up
			if (pm->ps->groundEntityNum == ENTITYNUM_NONE && !(pm->ps->pm_flags & PMF_JUMP_HELD))
			{
				//double-tap - must activate while in air
				ItemUse_Jetpack(&g_entities[pm->ps->client_num]);
				if (!JET_Flying(pm->gent))
				{
					JET_FlyStart(pm->gent);
				}
			}
		}
		else if (pm->cmd.upmove < 0)
		{
			//turn it off (or should we just go down)?
		}
	}
	else if (pm->waterlevel < 3)
	{
		if (pm->ps->gravity > 0)
		{
			//can't do this in zero-G
			if (pm->ps->legsAnim == BOTH_FORCELONGLEAP_START
				|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_ATTACK
				|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_LAND)
			{
				//in the middle of a force long-jump
				if ((pm->ps->legsAnim == BOTH_FORCELONGLEAP_START || pm->ps->legsAnim == BOTH_FORCELONGLEAP_ATTACK)
					&& pm->ps->legsAnimTimer > 0)
				{
					//in the air
					//FIXME: need an actual set time so it doesn't matter when the attack happens
					//FIXME: make sure we don't jump further than force jump 3 allows
					vec3_t j_fwd_angs, j_fwd_vec;
					VectorSet(j_fwd_angs, 0, pm->ps->viewangles[YAW], 0);
					AngleVectors(j_fwd_angs, j_fwd_vec, nullptr, nullptr);
					float old_z_vel = pm->ps->velocity[2];
					if (pm->ps->legsAnimTimer > 150 && old_z_vel < 0)
					{
						old_z_vel = 0;
					}
					VectorScale(j_fwd_vec, FORCE_LONG_LEAP_SPEED, pm->ps->velocity);
					pm->ps->velocity[2] = old_z_vel;
					pm->ps->pm_flags |= PMF_JUMP_HELD;
					pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
					pm->ps->forcePowersActive |= 1 << FP_LEVITATION;
					return qtrue;
				}
				//landing-slide
				if (pm->ps->legsAnim == BOTH_FORCELONGLEAP_START
					|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_ATTACK)
				{
					//still in start anim, but it's run out
					pm->ps->forcePowersActive |= 1 << FP_LEVITATION;
					if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
					{
						//still in air?
						//hold it for another 50ms
						//PM_SetAnim( pm, SETANIM_BOTH, BOTH_FORCELONGLEAP_START, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
					}
				}
				else
				{
					//in land-slide anim
					//FIXME: force some forward movement?  Less if holding back?
				}
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE //still in air
					&& pm->ps->origin[2] < pm->ps->jumpZStart) //dropped below original jump start
				{
					//slow down
					pm->ps->velocity[0] *= 0.75f;
					pm->ps->velocity[1] *= 0.75f;
					if ((pm->ps->velocity[0] + pm->ps->velocity[1]) * 0.5f <= 10.0f)
					{
						//falling straight down
						PM_SetAnim(pm, SETANIM_BOTH, BOTH_FORCEINAIR1, SETANIM_FLAG_OVERRIDE);
					}
				}
				return qfalse;
			}
			if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player-only for now
				&& pm->cmd.upmove > 0 //trying to jump
				&& pm->ps->forcePowerLevel[FP_LEVITATION] >= FORCE_LEVEL_3 //force jump 3 or better
				&& pm->ps->forcePower >= FORCE_LONGJUMP_POWER //this costs 20 force to do
				&& pm->ps->forcePowersActive & 1 << FP_SPEED //force-speed is on
				&& pm->cmd.forwardmove > 0 //pushing forward
				&& !pm->cmd.rightmove //not strafing
				&& pm->ps->groundEntityNum != ENTITYNUM_NONE //not in mid-air
				&& !(pm->ps->pm_flags & PMF_JUMP_HELD)
				//&& (float)(level.time-pm->ps->lastStationary) >= (3000.0f*g_timescale->value)//have to have a 3 second running start - relative to force speed slowdown
				&& level.time - pm->ps->forcePowerDebounce[FP_SPEED] <= 250
				//have to have just started the force speed within the last half second
				&& pm->gent)
			{
				//start a force long-jump!
				vec3_t j_fwd_angs, j_fwd_vec;
				//BOTH_FORCELONGLEAP_ATTACK if holding attack, too?
				PM_SetAnim(pm, SETANIM_BOTH, BOTH_FORCELONGLEAP_START, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				VectorSet(j_fwd_angs, 0, pm->ps->viewangles[YAW], 0);
				AngleVectors(j_fwd_angs, j_fwd_vec, nullptr, nullptr);
				VectorScale(j_fwd_vec, FORCE_LONG_LEAP_SPEED, pm->ps->velocity);
				pm->ps->velocity[2] = 320;
				pml.groundPlane = qfalse;
				pml.walking = qfalse;
				pm->ps->groundEntityNum = ENTITYNUM_NONE;
				pm->ps->jumpZStart = pm->ps->origin[2];
				pm->ps->pm_flags |= PMF_JUMP_HELD;
				pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
				//start force jump
				pm->ps->forcePowersActive |= 1 << FP_LEVITATION;
				pm->cmd.upmove = 0;
				// keep track of force jump stat
				if (pm->ps->client_num == 0)
				{
					if (pm->gent && pm->gent->client)
					{
						pm->gent->client->sess.missionStats.forceUsed[static_cast<int>(FP_LEVITATION)]++;
					}
				}
				if (g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[FP_SPEED] <
					FORCE_LEVEL_3)
				{
					//short burst
					G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
				}
				else
				{
					//holding it
					G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
				}
				WP_ForcePowerStop(pm->gent, FP_SPEED);
				WP_ForcePowerDrain(pm->gent, FP_LEVITATION, FORCE_LONGJUMP_POWER); //drain the required force power
				G_StartMatrixEffect(pm->gent, 0, pm->ps->legsAnimTimer + 500);
				return qtrue;
			}
			if (PM_InCartwheel(pm->ps->legsAnim)
				|| PM_InButterfly(pm->ps->legsAnim))
			{
				//can't keep jumping up in cartwheels, ariels and butterflies
			}
			//FIXME: still able to pogo-jump...
			else if (PM_ForceJumpingUp(pm->gent) && pm->ps->pm_flags & PMF_JUMP_HELD)
				//||pm->ps->waterHeightLevel==WHL_SHOULDERS) )
			{
				float cur_height = pm->ps->origin[2] - pm->ps->forceJumpZStart;
				//check for max force jump level and cap off & cut z vel
				if ((cur_height <= forceJumpHeight[0] || //still below minimum jump height
					pm->ps->forcePower && pm->cmd.upmove >= 10) &&
					////still have force power available and still trying to jump up
					cur_height < forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]])
					//still below maximum jump height
				{
					//can still go up
					//FIXME: after a certain amount of time of held jump, play force jump sound and flip if a dir is being held
					//FIXME: if hit a wall... should we cut velocity or allow them to slide up it?
					//FIXME: constantly drain force power at a rate by which the usage for maximum height would use up the full cost of force jump
					if (cur_height > forceJumpHeight[0])
					{
						//passed normal jump height  *2?
						if (!(pm->ps->forcePowersActive & 1 << FP_LEVITATION)) //haven't started forcejump yet
						{
							//start force jump
							pm->ps->forcePowersActive |= 1 << FP_LEVITATION;
							if (pm->gent)
							{
								if (g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[
									FP_LEVITATION] < FORCE_LEVEL_3)
								{
									//short burst
									G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
								}
								else
								{
									//holding it
									G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
								}
								// keep track of force jump stat
								if (pm->ps->client_num == 0 && pm->gent->client)
								{
									pm->gent->client->sess.missionStats.forceUsed[static_cast<int>(FP_LEVITATION)]++;
								}
							}
							//play flip
							//FIXME: do this only when they stop the jump (below) or when they're just about to hit the peak of the jump
							if (PM_InAirKickingAnim(pm->ps->legsAnim)
								&& pm->ps->legsAnimTimer)
							{
								//still in kick
							}
							else if ((pm->cmd.forwardmove || pm->cmd.rightmove) && //pushing in a dir
								pm->ps->legsAnim != BOTH_FLIP_F &&
								pm->ps->legsAnim != BOTH_FLIP_B &&
								pm->ps->legsAnim != BOTH_FLIP_R &&
								pm->ps->legsAnim != BOTH_FLIP_L &&
								pm->ps->legsAnim != BOTH_ALORA_FLIP_1 &&
								pm->ps->legsAnim != BOTH_ALORA_FLIP_2 &&
								pm->ps->legsAnim != BOTH_ALORA_FLIP_3
								&& BG_AllowThirdPersonSpecialMove(pm->ps) //third person only
								&& !cg.zoomMode //not zoomed in
								&& !(pm->ps->saber[0].saberFlags & SFL_NO_FLIPS) //okay to do flips with this saber
								&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_FLIPS))
								//okay to do flips with this saber
								)
							{
								//FIXME: this could end up playing twice if the jump is very long...
								int anim = BOTH_FORCEINAIR1;
								int parts = SETANIM_BOTH;

								if (pm->cmd.forwardmove > 0)
								{
									if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ALORA &&
										Q_irand(0, 3))
									{
										anim = Q_irand(BOTH_ALORA_FLIP_1, BOTH_ALORA_FLIP_3);
									}
									else
									{
										anim = BOTH_FLIP_F;
									}
								}
								else if (pm->cmd.forwardmove < 0)
								{
									anim = BOTH_FLIP_B;
								}
								else if (pm->cmd.rightmove > 0)
								{
									anim = BOTH_FLIP_R;
								}
								else if (pm->cmd.rightmove < 0)
								{
									anim = BOTH_FLIP_L;
								}
								if (pm->ps->weaponTime)
								{
									//FIXME: really only care if we're in a saber attack anim...
									parts = SETANIM_LEGS;
								}

								PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
							}
							else if (pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
							{
								//FIXME: really want to know how far off ground we are, probably...
								vec3_t facing_fwd, facing_right, facing_angles = { 0, pm->ps->viewangles[YAW], 0 };
								int anim = -1;
								AngleVectors(facing_angles, facing_fwd, facing_right, nullptr);
								float dot_r = DotProduct(facing_right, pm->ps->velocity);
								float dot_f = DotProduct(facing_fwd, pm->ps->velocity);
								if (fabs(dot_r) > fabs(dot_f) * 1.5)
								{
									if (dot_r > 150)
									{
										anim = BOTH_FORCEJUMPRIGHT1;
									}
									else if (dot_r < -150)
									{
										anim = BOTH_FORCEJUMPLEFT1;
									}
								}
								else
								{
									if (dot_f > 150)
									{
										anim = BOTH_FORCEJUMP1;
									}
									else if (dot_f < -150)
									{
										anim = BOTH_FORCEJUMPBACK1;
									}
								}
								if (anim != -1)
								{
									int parts = SETANIM_BOTH;
									if (pm->ps->weaponTime)
									{
										//FIXME: really only care if we're in a saber attack anim...
										parts = SETANIM_LEGS;
									}

									PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
								}
							}
						}
						else
						{
							if (!pm->ps->legsAnimTimer)
							{
								//not in the middle of a legsAnim
								int anim = pm->ps->legsAnim;
								int new_anim = -1;
								switch (anim)
								{
								case BOTH_FORCEJUMP1:
									new_anim = BOTH_FORCELAND1; //BOTH_FORCEINAIR1;
									break;
								case BOTH_FORCEJUMPBACK1:
									new_anim = BOTH_FORCELANDBACK1; //BOTH_FORCEINAIRBACK1;
									break;
								case BOTH_FORCEJUMPLEFT1:
									new_anim = BOTH_FORCELANDLEFT1; //BOTH_FORCEINAIRLEFT1;
									break;
								case BOTH_FORCEJUMPRIGHT1:
									new_anim = BOTH_FORCELANDRIGHT1; //BOTH_FORCEINAIRRIGHT1;
									break;
								default:;
								}
								if (new_anim != -1)
								{
									int parts = SETANIM_BOTH;
									if (pm->ps->weaponTime)
									{
										//FIXME: really only care if we're in a saber attack anim...
										parts = SETANIM_LEGS;
									}

									PM_SetAnim(pm, parts, new_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
								}
							}
						}
					}

					//need to scale this down, start with height velocity (based on max force jump height) and scale down to regular jump vel
					pm->ps->velocity[2] = (forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]] - cur_height) /
						forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]] * forceJumpStrength[pm->ps->
						forcePowerLevel[FP_LEVITATION]]; //JUMP_VELOCITY;
					pm->ps->velocity[2] /= 10;
					pm->ps->velocity[2] += JUMP_VELOCITY;
					pm->ps->pm_flags |= PMF_JUMP_HELD;
				}
				else if (cur_height > forceJumpHeight[0] && cur_height < forceJumpHeight[pm->ps->forcePowerLevel[
					FP_LEVITATION]] - forceJumpHeight[0])
				{
					//still have some headroom, don't totally stop it
					if (pm->ps->velocity[2] > JUMP_VELOCITY)
					{
						pm->ps->velocity[2] = JUMP_VELOCITY;
					}
				}
				else
				{
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
				return qfalse;
			}
			else if (pm->cmd.upmove > 0 && pm->ps->velocity[2] < -10)
			{
				//can't keep jumping, turn on jetpack?
				if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
					&& pm->gent && pm->gent->client && pm->ps->jetpackFuel > 15
					&& (pm->gent->client->NPC_class == CLASS_BOBAFETT ||
						pm->gent->client->NPC_class == CLASS_MANDALORIAN ||
						pm->gent->client->NPC_class == CLASS_JANGO ||
						pm->gent->client->NPC_class == CLASS_JANGODUAL ||
						pm->gent->client->NPC_class == CLASS_ROCKETTROOPER || rocket_trooper_player(pm->gent)))
				{
					if (!g_entities[pm->ps->client_num].client->jetPackOn)
					{
						ItemUse_Jetpack(&g_entities[pm->ps->client_num]);
					}
				}
			}
		}
	}

#endif

	//Not jumping
	if (pm->cmd.upmove < 10)
	{
		return qfalse;
	}

	// must wait for jump to be released
	if (pm->ps->pm_flags & PMF_JUMP_HELD)
	{
		// clear upmove so cmdscale doesn't lower running speed
		pm->cmd.upmove = 0;
		return qfalse;
	}

	if (pm->ps->gravity <= 0)
	{
		//in low grav, you push in the dir you're facing as long as there is something behind you to shove off of
		vec3_t forward, back;
		trace_t trace;

		AngleVectors(pm->ps->viewangles, forward, nullptr, nullptr);
		VectorMA(pm->ps->origin, -8, forward, back);
		pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, back, pm->ps->client_num,
			pm->tracemask & ~(CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP), static_cast<EG2_Collision>(0), 0);

		pm->cmd.upmove = 0;

		if (trace.fraction < 1.0f)
		{
			VectorMA(pm->ps->velocity, static_cast<float>(JUMP_VELOCITY) / 2, forward, pm->ps->velocity);
			//FIXME: kicking off wall anim?  At least check what anim we're in?
			PM_SetAnim(pm, SETANIM_LEGS, BOTH_FORCEJUMP1,
				SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
		}
		else
		{
			//else no surf close enough to push off of
			return qfalse;
		}

		if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
		{
			//need to set some things and return
			//Jumping
			pm->ps->forceJumpZStart = 0;
			pml.groundPlane = qfalse;
			pml.walking = qfalse;
			pm->ps->pm_flags |= PMF_JUMPING | PMF_JUMP_HELD;
			pm->ps->groundEntityNum = ENTITYNUM_NONE;
			pm->ps->jumpZStart = pm->ps->origin[2];

			if (pm->gent)
			{
				if (!Q3_TaskIDPending(pm->gent, TID_CHAN_VOICE))
				{
					PM_AddEvent(EV_JUMP);
				}
			}
			else
			{
				PM_AddEvent(EV_JUMP);
			}

			return qtrue;
		} //else no surf close enough to push off of
	}
	else if (pm->cmd.upmove > 0 //want to jump
		&& pm->waterlevel < 2 //not in water above ankles
		&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 //have force jump ability
		&& !(pm->ps->pm_flags & PMF_JUMP_HELD) //not holding jump from a previous jump
		&& pm->ps->forceRageRecoveryTime < pm->cmd.serverTime //not in a force Rage recovery period
		&& pm->gent && WP_ForcePowerAvailable(pm->gent, FP_LEVITATION, 0) //have enough force power to jump
		&& (pm->ps->client_num && !PM_ControlledByPlayer() || (pm->ps->client_num < MAX_CLIENTS ||
			PM_ControlledByPlayer()) && BG_AllowThirdPersonSpecialMove(pm->ps) && !cg.zoomMode && !(pm->gent->flags &
				FL_LOCK_PLAYER_WEAPONS)))
		// yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
	{
		if (pm->gent->NPC && pm->gent->NPC->rank != RANK_CREWMAN && pm->gent->NPC->rank <= RANK_LT_JG)
		{
			//reborn who are not acrobats can't do any of these acrobatics
			//FIXME: extern these abilities in the .npc file!
		}
		else if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
		{
			//on the ground
			//check for left-wall and right-wall special jumps
			int anim = -1;
			float vert_push = 0;
			int force_power_cost_override = 0;

			// Cartwheels/ariels/butterflies
			if (pm->ps->weapon == WP_SABER && G_TryingCartwheel(pm->gent, &pm->cmd)
				/*(pm->cmd.buttons&BUTTON_FORCE_FOCUS)*/ && pm->cmd.buttons & BUTTON_ATTACK
				//using saber and holding focus + attack
				//					  ||(pm->ps->weapon!=WP_SABER&&((pm->cmd.buttons&BUTTON_ATTACK)||(pm->cmd.buttons&BUTTON_ALT_ATTACK)) ) )//using any other weapon and hitting either attack button
				&& (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer() && pm->cmd.upmove > 0 && pm->ps->
					velocity[2] >= 0 //jumping NPC, going up already
					|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) &&
					G_TryingCartwheel(pm->gent, &pm->cmd)/*(pm->cmd.buttons&BUTTON_FORCE_FOCUS)*/)
				//focus-holding player
				&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_LR)
				/*pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_LR*/) // have enough power
			{
				//holding attack and jumping
				if (pm->cmd.rightmove > 0)
				{
					// If they're using the staff we do different anims.
					if (pm->ps->saber_anim_level == SS_STAFF
						&& pm->ps->weapon == WP_SABER)
					{
						if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()
							|| pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_2)
						{
							anim = BOTH_BUTTERFLY_RIGHT;
							force_power_cost_override = G_CostForSpecialMove(SABER_ALT_ATTACK_POWER_LR);
						}
					}
					else if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()
						|| pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
					{
						if (!(pm->ps->saber[0].saberFlags & SFL_NO_CARTWHEELS)
							&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_CARTWHEELS)))
						{
							//okay to do cartwheels with this saber
							if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
							{
								//player: since we're on the ground, always do a cartwheel
								/*
								anim = BOTH_CARTWHEEL_RIGHT;
								forcePowerCostOverride = G_CostForSpecialMove( SABER_ALT_ATTACK_POWER_LR );
								*/
							}
							else
							{
								vert_push = JUMP_VELOCITY;
								if (Q_irand(0, 1))
								{
									anim = BOTH_ARIAL_RIGHT;
									force_power_cost_override = G_CostForSpecialMove(SABER_ALT_ATTACK_POWER_LR);
								}
								else
								{
									anim = BOTH_CARTWHEEL_RIGHT;
									force_power_cost_override = G_CostForSpecialMove(SABER_ALT_ATTACK_POWER_LR);
								}
							}
						}
					}
				}
				else if (pm->cmd.rightmove < 0)
				{
					// If they're using the staff we do different anims.
					if (pm->ps->saber_anim_level == SS_STAFF
						&& pm->ps->weapon == WP_SABER)
					{
						if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()
							|| pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_2)
						{
							anim = BOTH_BUTTERFLY_LEFT;
							force_power_cost_override = G_CostForSpecialMove(SABER_ALT_ATTACK_POWER_LR);
						}
					}
					else if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()
						|| pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
					{
						if (!(pm->ps->saber[0].saberFlags & SFL_NO_CARTWHEELS)
							&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_CARTWHEELS)))
						{
							//okay to do cartwheels with this saber
							if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
							{
								//player: since we're on the ground, always do a cartwheel
								/*
								anim = BOTH_CARTWHEEL_LEFT;
								forcePowerCostOverride = G_CostForSpecialMove( SABER_ALT_ATTACK_POWER_LR );
								*/
							}
							else
							{
								vert_push = JUMP_VELOCITY;
								if (Q_irand(0, 1))
								{
									anim = BOTH_ARIAL_LEFT;
									force_power_cost_override = G_CostForSpecialMove(SABER_ALT_ATTACK_POWER_LR);
								}
								else
								{
									anim = BOTH_CARTWHEEL_LEFT;
									force_power_cost_override = G_CostForSpecialMove(SABER_ALT_ATTACK_POWER_LR);
								}
							}
						}
					}
				}
			}
			else if (pm->cmd.rightmove > 0 && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
			{
				//strafing right
				if (pm->cmd.forwardmove > 0)
				{
					//wall-run
					if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_RUNS)
						&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_RUNS)))
					{
						//okay to do wall-runs with this saber
						vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.0f;
						anim = BOTH_WALL_RUN_RIGHT;
					}
				}
				else if (pm->cmd.forwardmove == 0)
				{
					//wall-flip
					if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_FLIPS)
						&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_FLIPS)))
					{
						//okay to do wall-flips with this saber
						vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.25f;
						anim = BOTH_WALL_FLIP_RIGHT;
					}
				}
			}
			else if (pm->cmd.rightmove < 0 && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
			{
				//strafing left
				if (pm->cmd.forwardmove > 0)
				{
					//wall-run
					if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_RUNS)
						&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_RUNS)))
					{
						//okay to do wall-runs with this saber
						vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.0f;
						anim = BOTH_WALL_RUN_LEFT;
					}
				}
				else if (pm->cmd.forwardmove == 0)
				{
					//wall-flip
					if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_FLIPS)
						&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_FLIPS)))
					{
						//okay to do wall-flips with this saber
						vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.25f;
						anim = BOTH_WALL_FLIP_LEFT;
					}
				}
			}
			else if (/*pm->ps->client_num >= MAX_CLIENTS//not the player
				&& !PM_ControlledByPlayer() //not controlled by player
				&&*/ pm->cmd.forwardmove > 0 //pushing forward
				&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1) //have jump 2 or higher
			{
				//step off wall, flip backwards
				if (VectorLengthSquared(pm->ps->velocity) > 40000 /*200*200*/)
				{
					//have to be moving... FIXME: make sure it's opposite the wall... or at least forward?
					if (pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_2)
					{
						//run all the way up wwall
						if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_RUNS)
							&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_RUNS)))
						{
							//okay to do wall-runs with this saber
							vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.0f;
							anim = BOTH_FORCEWALLRUNFLIP_START;
						}
					}
					else
					{
						//run just a couple steps up
						if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_FLIPS)
							&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_FLIPS)))
						{
							//okay to do wall-flips with this saber
							vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.25f;
							anim = BOTH_WALL_FLIP_BACK1;
						}
					}
				}
			}
			else if (pm->cmd.forwardmove < 0 //pushing back
				//&& pm->ps->client_num//not the player
				&& !(pm->cmd.buttons & BUTTON_ATTACK)) //not attacking
			{
				//back-jump does backflip... FIXME: always?!  What about just doing a normal jump backwards?
				if (pm->ps->velocity[2] >= 0)
				{
					//must be going up already
					if (!(pm->ps->saber[0].saberFlags & SFL_NO_FLIPS)
						&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_FLIPS)))
					{
						//okay to do backstabs with this saber
						vert_push = JUMP_VELOCITY;
						if (pm->gent->client && pm->gent->client->NPC_class == CLASS_ALORA && !Q_irand(0, 2))
						{
							anim = BOTH_ALORA_FLIP_B;
						}
						else
						{
							anim = PM_PickAnim(pm->gent, BOTH_FLIP_BACK1, BOTH_FLIP_BACK3);
						}
					}
				}
			}
			else if (VectorLengthSquared(pm->ps->velocity) < 256 /*16 squared*/)
			{
				//not moving
				if (pm->ps->weapon == WP_SABER && pm->cmd.buttons & BUTTON_ATTACK)
				{
					saber_moveName_t override_jump_attack_up_move = LS_INVALID;
					if (pm->ps->saber[0].jumpAtkUpMove != LS_INVALID)
					{
						if (pm->ps->saber[0].jumpAtkUpMove != LS_NONE)
						{
							//actually overriding
							override_jump_attack_up_move = static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkUpMove);
						}
						else if (pm->ps->dualSabers
							&& pm->ps->saber[1].jumpAtkUpMove > LS_NONE)
						{
							//would be cancelling it, but check the second saber, too
							override_jump_attack_up_move = static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkUpMove);
						}
						else
						{
							//nope, just cancel it
							override_jump_attack_up_move = LS_NONE;
						}
					}
					else if (pm->ps->dualSabers
						&& pm->ps->saber[1].jumpAtkUpMove != LS_INVALID)
					{
						//first saber not overridden, check second
						override_jump_attack_up_move = static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkUpMove);
					}
					if (override_jump_attack_up_move != LS_INVALID)
					{
						//do this move instead
						if (override_jump_attack_up_move != LS_NONE)
						{
							anim = saber_moveData[override_jump_attack_up_move].animToUse;
						}
					}
					else if (pm->ps->saber_anim_level == SS_MEDIUM)
					{
						if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
							//NOTE: pretty much useless, so player never does these
						{
							vert_push = forceJumpStrength[FORCE_LEVEL_2] / 1.5f;
							if (pm->gent->client && pm->gent->client->NPC_class == CLASS_ALORA)
							{
								anim = BOTH_ALORA_SPIN;
							}
							else
							{
								anim = Q_irand(BOTH_FJSS_TR_BL, BOTH_FJSS_TL_BR);
							}
						}
					}
				}
			}

			if (anim != -1 && PM_HasAnimation(pm->gent, anim))
			{
				vec3_t fwd, right, traceto, mins = { pm->mins[0], pm->mins[1], 0 }, maxs = { pm->maxs[0], pm->maxs[1], 24 },
					fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
				trace_t trace;
				qboolean do_trace = qfalse;
				int contents = CONTENTS_SOLID;

				AngleVectors(fwd_angles, fwd, right, nullptr);

				//trace-check for a wall, if necc.
				switch (anim)
				{
				case BOTH_WALL_FLIP_LEFT:
					contents |= CONTENTS_BODY;
					//NOTE: purposely falls through to next case!
				case BOTH_WALL_RUN_LEFT:
					do_trace = qtrue;
					VectorMA(pm->ps->origin, -16, right, traceto);
					break;

				case BOTH_WALL_FLIP_RIGHT:
					contents |= CONTENTS_BODY;
					//NOTE: purposely falls through to next case!
				case BOTH_WALL_RUN_RIGHT:
					do_trace = qtrue;
					VectorMA(pm->ps->origin, 16, right, traceto);
					break;

				case BOTH_WALL_FLIP_BACK1:
					contents |= CONTENTS_BODY;
					do_trace = qtrue;
					VectorMA(pm->ps->origin, 32, fwd, traceto); //was 16
					break;

				case BOTH_FORCEWALLRUNFLIP_START:
					contents |= CONTENTS_BODY;
					do_trace = qtrue;
					VectorMA(pm->ps->origin, 32, fwd, traceto); //was 16
					break;
				default:;
				}

				vec3_t ideal_normal = { 0 }, wall_normal = { 0 };
				if (do_trace)
				{
					//FIXME: all these jump ones should check for head clearance
					pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, contents,
						static_cast<EG2_Collision>(0), 0);
					VectorCopy(trace.plane.normal, wall_normal);
					VectorNormalize(wall_normal);
					VectorSubtract(pm->ps->origin, traceto, ideal_normal);
					VectorNormalize(ideal_normal);
					if (anim == BOTH_WALL_FLIP_LEFT)
					{
						//sigh.. check for bottomless pit to the right
						trace_t trace2;
						VectorMA(pm->ps->origin, 128, right, traceto);
						pm->trace(&trace2, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, contents,
							static_cast<EG2_Collision>(0), 0);
						if (!trace2.allsolid && !trace2.startsolid)
						{
							vec3_t start;
							VectorCopy(trace2.endpos, traceto);
							VectorCopy(traceto, start);
							traceto[2] -= 384;
							pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->client_num, contents,
								static_cast<EG2_Collision>(0), 0);
							if (!trace2.allsolid && !trace2.startsolid && trace2.fraction >= 1.0f)
							{
								//bottomless pit!
								trace.fraction = 1.0f; //way to stop it from doing the side-flip
							}
						}
					}
					else if (anim == BOTH_WALL_FLIP_RIGHT)
					{
						//sigh.. check for bottomless pit to the left
						trace_t trace2;
						VectorMA(pm->ps->origin, -128, right, traceto);
						pm->trace(&trace2, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, contents,
							static_cast<EG2_Collision>(0), 0);
						if (!trace2.allsolid && !trace2.startsolid)
						{
							vec3_t start;
							VectorCopy(trace2.endpos, traceto);
							VectorCopy(traceto, start);
							traceto[2] -= 384;
							pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->client_num, contents,
								static_cast<EG2_Collision>(0), 0);
							if (!trace2.allsolid && !trace2.startsolid && trace2.fraction >= 1.0f)
							{
								//bottomless pit!
								trace.fraction = 1.0f; //way to stop it from doing the side-flip
							}
						}
					}
					else
					{
						if (anim == BOTH_WALL_FLIP_BACK1
							|| anim == BOTH_FORCEWALLRUNFLIP_START)
						{
							//trace up and forward a little to make sure the wall it at least 64 tall
							if (contents & CONTENTS_BODY //included entitied
								&& trace.contents & CONTENTS_BODY //hit an entity
								&& g_entities[trace.entityNum].client) //hit a client
							{
								//no need to trace up, it's all good...
								if (PM_InOnGroundAnim(&g_entities[trace.entityNum].client->ps)) //on the ground, no jump
								{
									//can't jump off guys on ground
									trace.fraction = 1.0f; //way to stop if from doing the jump
								}
								else if (anim == BOTH_FORCEWALLRUNFLIP_START)
								{
									//instead of wall-running up, do the backflip
									anim = BOTH_WALL_FLIP_BACK1;
									vert_push = forceJumpStrength[FORCE_LEVEL_2] / 2.25f;
								}
							}
							else if (anim == BOTH_WALL_FLIP_BACK1)
							{
								trace_t trace2;
								vec3_t start;
								VectorCopy(pm->ps->origin, start);
								start[2] += 64;
								VectorMA(start, 32, fwd, traceto);
								pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->client_num, contents,
									static_cast<EG2_Collision>(0), 0);
								if (trace2.allsolid
									|| trace2.startsolid
									|| trace2.fraction >= 1.0f)
								{
									//no room above or no wall in front at that height
									trace.fraction = 1.0f; //way to stop if from doing the jump
								}
							}
						}
						if (trace.fraction < 1.0f)
						{
							//still valid to jump
							if (anim == BOTH_WALL_FLIP_BACK1)
							{
								//sigh.. check for bottomless pit to the rear
								trace_t trace2;
								VectorMA(pm->ps->origin, -128, fwd, traceto);
								pm->trace(&trace2, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, contents,
									static_cast<EG2_Collision>(0), 0);
								if (!trace2.allsolid && !trace2.startsolid)
								{
									vec3_t start;
									VectorCopy(trace2.endpos, traceto);
									VectorCopy(traceto, start);
									traceto[2] -= 384;
									pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->client_num, contents,
										static_cast<EG2_Collision>(0), 0);
									if (!trace2.allsolid && !trace2.startsolid && trace2.fraction >= 1.0f)
									{
										//bottomless pit!
										trace.fraction = 1.0f; //way to stop it from doing the side-flip
									}
								}
							}
						}
					}
				}
				gentity_t* traceEnt = &g_entities[trace.entityNum];

				if (!do_trace || trace.fraction < 1.0f && (trace.entityNum < ENTITYNUM_WORLD && traceEnt && traceEnt->
					s
					.solid != SOLID_BMODEL || DotProduct(wall_normal, ideal_normal) > 0.7))
				{
					//there is a wall there
					if (anim != BOTH_WALL_RUN_LEFT
						&& anim != BOTH_WALL_RUN_RIGHT
						&& anim != BOTH_FORCEWALLRUNFLIP_START
						|| wall_normal[2] >= 0.0f && wall_normal[2] <= MAX_WALL_RUN_Z_NORMAL)
					{
						//wall-runs can only run on relatively flat walls, sorry.
						if (anim == BOTH_ARIAL_LEFT || anim == BOTH_CARTWHEEL_LEFT)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, -185, right, pm->ps->velocity);
						}
						else if (anim == BOTH_ARIAL_RIGHT || anim == BOTH_CARTWHEEL_RIGHT)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, 185, right, pm->ps->velocity);
						}
						else if (anim == BOTH_BUTTERFLY_LEFT)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, -190, right, pm->ps->velocity);
						}
						else if (anim == BOTH_BUTTERFLY_RIGHT)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, 190, right, pm->ps->velocity);
						}
						//move me to side
						else if (anim == BOTH_WALL_FLIP_LEFT)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, 150, right, pm->ps->velocity);
						}
						else if (anim == BOTH_WALL_FLIP_RIGHT)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, -150, right, pm->ps->velocity);
						}
						else if (anim == BOTH_FLIP_BACK1
							|| anim == BOTH_FLIP_BACK2
							|| anim == BOTH_FLIP_BACK3
							|| anim == BOTH_ALORA_FLIP_B
							|| anim == BOTH_WALL_FLIP_BACK1)
						{
							pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
							VectorMA(pm->ps->velocity, -150, fwd, pm->ps->velocity);
						}
						//kick if jumping off an ent
						if (do_trace
							&& anim != BOTH_WALL_RUN_LEFT
							&& anim != BOTH_WALL_RUN_RIGHT
							&& anim != BOTH_FORCEWALLRUNFLIP_START)
						{
							if (pm->gent && trace.entityNum < ENTITYNUM_WORLD)
							{
								if (traceEnt
									&& traceEnt->client
									&& traceEnt->health > 0
									&& traceEnt->takedamage
									&& traceEnt->client->NPC_class != CLASS_GALAKMECH
									&& traceEnt->client->NPC_class != CLASS_DESANN
									&& traceEnt->client->NPC_class != CLASS_SITHLORD
									&& traceEnt->client->NPC_class != CLASS_VADER
									&& !(traceEnt->flags & FL_NO_KNOCKBACK))
								{
									//push them away and do pain
									vec3_t opp_dir, fxDir;
									float strength = VectorNormalize2(pm->ps->velocity, opp_dir);
									VectorScale(opp_dir, -1, opp_dir);
									//FIXME: need knockdown anim
									G_Damage(traceEnt, pm->gent, pm->gent, opp_dir, traceEnt->currentOrigin, 10,
										DAMAGE_NO_ARMOR | DAMAGE_NO_HIT_LOC | DAMAGE_NO_KNOCKBACK, MOD_MELEE);
									VectorCopy(fwd, fxDir);
									VectorScale(fxDir, -1, fxDir);
									G_PlayEffect(G_EffectIndex("melee/kick_impact"), trace.endpos, fxDir);
									//G_Sound( traceEnt, G_SoundIndex( va("sound/weapons/melee/punch%d", Q_irand(1, 4)) ) );
									if (traceEnt->health > 0)
									{
										//didn't kill him
										if (traceEnt->s.number == 0 && !Q_irand(0, g_spskill->integer)
											|| traceEnt->NPC != nullptr && Q_irand(RANK_CIVILIAN, traceEnt->NPC->rank)
											+ Q_irand(-2, 2) < RANK_ENSIGN)
										{
											NPC_SetAnim(traceEnt, SETANIM_BOTH, BOTH_KNOCKDOWN2,
												SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
											G_Throw(traceEnt, opp_dir, strength);
										}
									}
								}
							}
						}
						//up
						if (vert_push)
						{
							pm->ps->velocity[2] = vert_push;
						}
						//animate me
						if (anim == BOTH_BUTTERFLY_RIGHT)
						{
							PM_SetSaberMove(LS_BUTTERFLY_RIGHT);
						}
						else if (anim == BOTH_BUTTERFLY_LEFT)
						{
							PM_SetSaberMove(LS_BUTTERFLY_LEFT);
						}
						else
						{
							//not a proper saber_move, so do set all the details manually
							int parts = SETANIM_LEGS;
							if ( /*anim == BOTH_BUTTERFLY_LEFT ||
								anim == BOTH_BUTTERFLY_RIGHT ||*/
								anim == BOTH_FJSS_TR_BL ||
								anim == BOTH_FJSS_TL_BR)
							{
								parts = SETANIM_BOTH;
								pm->cmd.buttons &= ~BUTTON_ATTACK;
								pm->ps->saber_move = LS_NONE;
								pm->gent->client->ps.SaberActivateTrail(300);
							}
							else if (!pm->ps->weaponTime)
							{
								parts = SETANIM_BOTH;
							}
							PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
							if (anim == BOTH_FJSS_TR_BL
								|| anim == BOTH_FJSS_TL_BR
								|| anim == BOTH_FORCEWALLRUNFLIP_START)
							{
								pm->ps->weaponTime = pm->ps->torsoAnimTimer;
							}
							else if (anim == BOTH_WALL_FLIP_LEFT
								|| anim == BOTH_WALL_FLIP_RIGHT
								|| anim == BOTH_WALL_FLIP_BACK1)
							{
								//let us do some more moves after this
								pm->ps->saberAttackChainCount = 0;
							}
						}
						pm->ps->forceJumpZStart = pm->ps->origin[2]; //so we don't take damage if we land at same height
						pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
						pm->cmd.upmove = 0;

						if (g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[
							FP_LEVITATION] < FORCE_LEVEL_3)
						{
							//short burst
							G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
						}
						else
						{
							//holding it
							G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
						}
						WP_ForcePowerDrain(pm->gent, FP_LEVITATION, force_power_cost_override);
					}
				}
			}
		}
		else
		{
			//in the air
			int legs_anim = pm->ps->legsAnim;

			if (legs_anim == BOTH_WALL_RUN_LEFT || legs_anim == BOTH_WALL_RUN_RIGHT)
			{
				//running on a wall
				vec3_t right, traceto, mins = { pm->mins[0], pm->mins[0], 0 }, maxs = { pm->maxs[0], pm->maxs[0], 24 },
					fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
				trace_t trace;
				int anim = -1;

				AngleVectors(fwd_angles, nullptr, right, nullptr);

				if (legs_anim == BOTH_WALL_RUN_LEFT)
				{
					if (pm->ps->legsAnimTimer > 400)
					{
						//not at the end of the anim
						float animLen = PM_AnimLength(pm->gent->client->clientInfo.animFileIndex, BOTH_WALL_RUN_LEFT);
						if (pm->ps->legsAnimTimer < animLen - 400)
						{
							//not at start of anim
							VectorMA(pm->ps->origin, -16, right, traceto);
							anim = BOTH_WALL_RUN_LEFT_FLIP;
						}
					}
				}
				else if (legs_anim == BOTH_WALL_RUN_RIGHT)
				{
					if (pm->ps->legsAnimTimer > 400)
					{
						//not at the end of the anim
						float anim_len = PM_AnimLength(pm->gent->client->clientInfo.animFileIndex, BOTH_WALL_RUN_RIGHT);
						if (pm->ps->legsAnimTimer < anim_len - 400)
						{
							//not at start of anim
							VectorMA(pm->ps->origin, 16, right, traceto);
							anim = BOTH_WALL_RUN_RIGHT_FLIP;
						}
					}
				}
				if (anim != -1)
				{
					pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num,
						CONTENTS_SOLID | CONTENTS_BODY, static_cast<EG2_Collision>(0), 0);
					if (trace.fraction < 1.0f)
					{
						//flip off wall
						if (anim == BOTH_WALL_RUN_LEFT_FLIP)
						{
							pm->ps->velocity[0] *= 0.5f;
							pm->ps->velocity[1] *= 0.5f;
							VectorMA(pm->ps->velocity, 150, right, pm->ps->velocity);
						}
						else if (anim == BOTH_WALL_RUN_RIGHT_FLIP)
						{
							pm->ps->velocity[0] *= 0.5f;
							pm->ps->velocity[1] *= 0.5f;
							VectorMA(pm->ps->velocity, -150, right, pm->ps->velocity);
						}
						PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
						pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
						pm->cmd.upmove = 0;
					}
				}
				if (pm->cmd.upmove != 0)
				{
					//jump failed, so don't try to do normal jump code, just return
					return qfalse;
				}
			}
			else if (pm->ps->legsAnim == BOTH_FORCEWALLRUNFLIP_START)
			{
				//want to jump off wall
				vec3_t fwd, traceto, mins = { pm->mins[0], pm->mins[0], 0 }, maxs = { pm->maxs[0], pm->maxs[0], 24 },
					fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
				trace_t trace;
				int anim = -1;

				AngleVectors(fwd_angles, fwd, nullptr, nullptr);

				float anim_len = PM_AnimLength(pm->gent->client->clientInfo.animFileIndex, BOTH_FORCEWALLRUNFLIP_START);
				if (pm->ps->legsAnimTimer < anim_len - 250) //was 400
				{
					//not at start of anim
					VectorMA(pm->ps->origin, 16, fwd, traceto);
					anim = BOTH_FORCEWALLRUNFLIP_END;
				}
				if (anim != -1)
				{
					pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num,
						CONTENTS_SOLID | CONTENTS_BODY, static_cast<EG2_Collision>(0), 0);
					if (trace.fraction < 1.0f)
					{
						//flip off wall
						pm->ps->velocity[0] *= 0.5f;
						pm->ps->velocity[1] *= 0.5f;
						VectorMA(pm->ps->velocity, WALL_RUN_UP_BACKFLIP_SPEED, fwd, pm->ps->velocity);
						pm->ps->velocity[2] += 200;
						int parts = SETANIM_LEGS;
						if (!pm->ps->weaponTime)
						{
							//not attacking, set anim on both
							parts = SETANIM_BOTH;
						}
						PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
						//FIXME: do damage to traceEnt, like above?
						pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
						pm->cmd.upmove = 0;
						PM_AddEvent(EV_JUMP);
					}
				}
				if (pm->cmd.upmove != 0)
				{
					//jump failed, so don't try to do normal jump code, just return
					return qfalse;
				}
			}
			else if ((pm->ps->legsAnimTimer <= 100
				|| !PM_InSpecialJump(legs_anim) //not in a special jump anim
				|| PM_InReboundJump(legs_anim) //we're already in a rebound
				|| PM_InBackFlip(legs_anim)) //a backflip (needed so you can jump off a wall behind you)
				//&& pm->ps->velocity[2] <= 0
				&& pm->ps->velocity[2] > -1200 //not falling down very fast
				&& !(pm->ps->pm_flags & PMF_JUMP_HELD) //have to have released jump since last press
				&& (pm->cmd.forwardmove || pm->cmd.rightmove) //pushing in a direction
				//&& pm->ps->forceRageRecoveryTime < pm->cmd.serverTime	//not in a force Rage recovery period
				&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 //level 3 jump or better
				&& pm->ps->forcePower > 10 //have enough force power to do another one
				&& level.time - pm->ps->lastOnGround > 250 //haven't been on the ground in the last 1/4 of a second
				&& (!(pm->ps->pm_flags & PMF_JUMPING) //not jumping
					|| level.time - pm->ps->lastOnGround > 250
					//we are jumping, but have been in the air for at least half a second
					&& pm->ps->origin[2] - pm->ps->forceJumpZStart < forceJumpHeightMax[FORCE_LEVEL_3] -
					G_ForceWallJumpStrength() / 2.0f
					)
				//&& (pm->ps->legsAnim == BOTH_JUMP1 || pm->ps->legsAnim == BOTH_INAIR1 ) )//not in a flip or spin or anything
				)
			{
				//see if we're pushing at a wall and jump off it if so
				if (!(pm->ps->saber[0].saberFlags & SFL_NO_WALL_GRAB)
					&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_WALL_GRAB)))
				{
					//okay to do wall-grabs with this saber
					//FIXME: make sure we have enough force power
					//FIXME: check  to see if we can go any higher
					//FIXME: limit to a certain number of these in a row?
					//FIXME: maybe don't require a ucmd direction, just check all 4?
					//FIXME: should stick to the wall for a second, then push off...
					vec3_t check_dir, mins = { pm->mins[0], pm->mins[1], 0 }, maxs = {
							   pm->maxs[0], pm->maxs[1], 24
					}, fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
					trace_t trace;
					int anim = -1;

					if (pm->cmd.rightmove)
					{
						if (pm->cmd.rightmove > 0)
						{
							anim = BOTH_FORCEWALLREBOUND_RIGHT;
							AngleVectors(fwd_angles, nullptr, check_dir, nullptr);
						}
						else if (pm->cmd.rightmove < 0)
						{
							anim = BOTH_FORCEWALLREBOUND_LEFT;
							AngleVectors(fwd_angles, nullptr, check_dir, nullptr);
							VectorScale(check_dir, -1, check_dir);
						}
					}
					else if (pm->cmd.forwardmove > 0)
					{
						anim = BOTH_FORCEWALLREBOUND_FORWARD;
						AngleVectors(fwd_angles, check_dir, nullptr, nullptr);
					}
					else if (pm->cmd.forwardmove < 0)
					{
						anim = BOTH_FORCEWALLREBOUND_BACK;
						AngleVectors(fwd_angles, check_dir, nullptr, nullptr);
						VectorScale(check_dir, -1, check_dir);
					}
					if (anim != -1)
					{
						vec3_t traceto;
						vec3_t ideal_normal;
						//trace in the dir we're pushing in and see if there's a vertical wall there
						VectorMA(pm->ps->origin, 16, check_dir, traceto); //was 8
						pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, CONTENTS_SOLID,
							static_cast<EG2_Collision>(0), 0); //FIXME: clip brushes too?
						VectorSubtract(pm->ps->origin, traceto, ideal_normal);
						VectorNormalize(ideal_normal);
						gentity_t* traceEnt = &g_entities[trace.entityNum];
						if (trace.fraction < 1.0f
							&& fabs(trace.plane.normal[2]) <= MAX_WALL_GRAB_SLOPE
							&& (trace.entityNum < ENTITYNUM_WORLD && traceEnt && traceEnt->s.solid != SOLID_BMODEL ||
								DotProduct(trace.plane.normal, ideal_normal) > 0.7))
						{
							//there is a wall there
							float dot = DotProduct(pm->ps->velocity, trace.plane.normal);
							if (dot < 1.0f)
							{
								//can't be heading *away* from the wall!
								//grab it!
								PM_GrabWallForJump(anim);
							}
						}
					}
				}
			}
			else
			{
				//FIXME: if in a butterfly, kick people away?
			}
		}
	}

	if (pm->gent
		&& pm->ps->forceRageRecoveryTime < pm->cmd.serverTime //not in a force Rage recovery period
		&& pm->ps->weapon == WP_SABER
		&& (pm->ps->weaponTime > 0 || pm->cmd.buttons & BUTTON_ATTACK)
		&& (pm->ps->client_num && !PM_ControlledByPlayer() || (pm->ps->client_num < MAX_CLIENTS ||
			PM_ControlledByPlayer()) && BG_AllowThirdPersonSpecialMove(pm->ps) && !cg.zoomMode))
	{
		//okay, we just jumped and we're in an attack
		if (!PM_RollingAnim(pm->ps->legsAnim)
			&& !PM_InKnockDown(pm->ps)
			&& !PM_InDeathAnim()
			&& !PM_PainAnim(pm->ps->torsoAnim)
			&& !PM_FlippingAnim(pm->ps->legsAnim)
			&& !PM_SpinningAnim(pm->ps->legsAnim)
			&& !pm_saber_in_special_attack(pm->ps->torsoAnim))
		{
			//HMM... do NPCs need this logic?
			if (!PM_SaberInTransitionAny(pm->ps->saber_move) //not going to/from/between an attack anim
				&& !PM_SaberInAttack(pm->ps->saber_move) //not in attack anim
				&& pm->ps->weaponTime <= 0 //not busy
				&& pm->cmd.buttons & BUTTON_ATTACK) //want to attack
			{
				//not in an attack or finishing/starting/transitioning one
				if (PM_CheckBackflipAttackMove())
				{
					PM_SetSaberMove(PM_SaberBackflipAttackMove()); //backflip attack
				}
			}
		}
	}
	if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
	{
		return qfalse;
	}
	if (pm->cmd.upmove > 0)
	{
		//no special jumps
		pm->ps->velocity[2] = JUMP_VELOCITY;
		pm->ps->forceJumpZStart = pm->ps->origin[2]; //so we don't take damage if we land at same height
		pm->ps->pm_flags |= PMF_JUMPING;
	}

	if (d_JediAI->integer || d_combatinfo->integer || g_DebugSaberCombat->integer)
	{
		if (pm->ps->client_num && pm->ps->weapon == WP_SABER)
		{
			Com_Printf("jumping\n");
		}
	}
	//Jumping
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
	pm->ps->pm_flags |= PMF_JUMP_HELD;
	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pm->ps->jumpZStart = pm->ps->origin[2];

	if (pm->gent)
	{
		if (!Q3_TaskIDPending(pm->gent, TID_CHAN_VOICE))
		{
			PM_AddEvent(EV_JUMP);
		}
	}
	else
	{
		PM_AddEvent(EV_JUMP);
	}

	//Set the animations
	if (pm->ps->gravity > 0 && !PM_InSpecialJump(pm->ps->legsAnim) && !PM_InGetUp(pm->ps))
	{
		PM_JumpForDir();
	}

	return qtrue;
}

/*
=============
PM_CheckWaterJump
=============
*/
static qboolean PM_CheckWaterJump()
{
	vec3_t spot;
	vec3_t flatforward{};

	if (pm->ps->pm_time)
	{
		return qfalse;
	}

	if (pm->cmd.forwardmove <= 0 && pm->cmd.upmove <= 0)
	{
		//they must not want to get out?
		return qfalse;
	}
	// check for water jump
	if (pm->waterlevel != 2)
	{
		return qfalse;
	}

	if (pm->watertype & CONTENTS_LADDER)
	{
		if (pm->ps->velocity[2] <= 0)
			return qfalse;
	}

	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize(flatforward);

	VectorMA(pm->ps->origin, 30, flatforward, spot);
	spot[2] += 24;
	int cont = pm->pointcontents(spot, pm->ps->client_num);
	if (!(cont & CONTENTS_SOLID))
	{
		return qfalse;
	}

	spot[2] += 16;
	cont = pm->pointcontents(spot, pm->ps->client_num);
	if (cont & (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA | CONTENTS_BODY))
	{
		return qfalse;
	}

	// jump out of water
	VectorScale(pml.forward, 200, pm->ps->velocity);
	pm->ps->velocity[2] = 350 + (pm->ps->waterheight - pm->ps->origin[2]) * 2;

	pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
	pm->ps->pm_time = 2000;

	return qtrue;
}

//============================================================================

/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove()
{
	// waterjump has no control, but falls

	PM_StepSlideMove(1);

	pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
	if (pm->ps->velocity[2] < 0)
	{
		// cancel as soon as we are falling down again
		pm->ps->pm_flags &= ~PMF_ALL_TIMES;
		pm->ps->pm_time = 0;
	}
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove()
{
	vec3_t wishvel{};
	float wishspeed;
	vec3_t wishdir;
	float scale;

	if (PM_CheckWaterJump())
	{
		PM_WaterJumpMove();
		return;
	}
	if (pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 && pm->waterlevel < 3)
	{
		if (PM_CheckJump())
		{
			// jumped away
			return;
		}
	}
#if 0
	// jump = head for surface
	if (pm->cmd.upmove >= 10) {
		if (pm->ps->velocity[2] > -300) {
			if (pm->watertype == CONTENTS_WATER) {
				pm->ps->velocity[2] = 100;
			}
			else if (pm->watertype == CONTENTS_SLIME) {
				pm->ps->velocity[2] = 80;
			}
			else {
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
	PM_Friction();

	scale = PM_CmdScale(&pm->cmd);
	//
	// user intentions
	//
	if (!scale)
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		if (pm->watertype & CONTENTS_LADDER)
		{
			wishvel[2] = 0;
		}
		else
		{
			wishvel[2] = -60; // sink towards bottom
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
		}
		wishvel[2] += scale * pm->cmd.upmove;
		if (!(pm->watertype & CONTENTS_LADDER)) //ladder
		{
			const float depth = pm->ps->origin[2] + pm->gent->client->standheight - pm->ps->waterheight;
			if (depth >= 12)
			{
				//too high!
				wishvel[2] -= 120; // sink towards bottom
				if (wishvel[2] > 0)
				{
					wishvel[2] = 0;
				}
			}
			else if (pm->ps->waterHeightLevel >= WHL_UNDER) //!depth && pm->waterlevel == 3 )
			{
			}
			else if (depth < 12)
			{
				//still deep
				wishvel[2] -= 60; // sink towards bottom
				if (wishvel[2] > 30)
				{
					wishvel[2] = 30;
				}
			}
		}
	}

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (pm->watertype & CONTENTS_LADDER) //ladder
	{
		if (wishspeed > pm->ps->speed * pm_ladderScale)
		{
			wishspeed = pm->ps->speed * pm_ladderScale;
		}
		PM_Accelerate(wishdir, wishspeed, pm_Laddeeraccelerate);
	}
	else
	{
		if (pm->ps->gravity < 0)
		{
			//float up
			pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
		}
		if (wishspeed > pm->ps->speed * pm_swimScale)
		{
			wishspeed = pm->ps->speed * pm_swimScale;
		}
		PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);
	}

	// make sure we can go up slopes easily under water
	if (pml.groundPlane && DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal) < 0)
	{
		const float vel = VectorLength(pm->ps->velocity);
		// slide along the ground plane
		PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
			pm->ps->velocity, OVERCLIP);

		VectorNormalize(pm->ps->velocity);
		VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
	}

	PM_SlideMove(qfalse);
}

static qhandle_t grapplemove_loop_sound = 0;

static void PM_GrappleMove()
{
	vec3_t vel, v;
	constexpr int anim = BOTH_GRAPPLE_PULL;
	constexpr int anim2 = BOTH_FORCEINAIR1;
	static qboolean registered = qfalse;

	VectorScale(pml.forward, -16, v);
	VectorAdd(pm->ps->lastHitLoc, v, v);
	VectorSubtract(v, pm->ps->origin, vel);
	const float vlen = VectorLength(vel);
	VectorNormalize(vel);

	if (!registered)
	{
		grapplemove_loop_sound = G_SoundIndex("sound/ambience/outdoor/windy1_lp.wav");
		registered = qtrue;
	}

	if (pm->cmd.buttons & BUTTON_ATTACK || pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		return;
	}

	if (pm->cmd.buttons & BUTTON_BLOCK)
	{
		PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 350);
		pm->gent->s.loopSound = grapplemove_loop_sound;
	}
	else
	{
		PM_SetAnim(pm, SETANIM_BOTH, anim2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS, 350);
	}

	if (pm->ps->userFloat1 == 0)
	{
		if (vlen <= 100)
		{
			VectorScale(vel, 10 * vlen, vel);
		}
		else
		{
			VectorScale(vel, 800, vel);
		}

		VectorCopy(vel, pm->ps->velocity);

		pml.groundPlane = qfalse;
	}
	else
	{
		if (vlen > pm->ps->userFloat1 && !(PM_InSpecialJump(pm->ps->torsoAnim) || pm->ps->pm_flags &
			PMF_STUCK_TO_WALL))
		{
			VectorSubtract(pm->ps->lastHitLoc, pm->ps->origin, vel);
			VectorNormalize(vel);
			VectorMA(pm->ps->origin, vlen - pm->ps->userFloat1, vel, pm->ps->origin);
			VectorScale(vel, vlen - pm->ps->userFloat1, vel);
			VectorAdd(pm->ps->velocity, vel, pm->ps->velocity);
			pm->ps->velocity[2] = 0;
		}
	}
}

/*
===================
PM_FlyVehicleMove

===================
*/
static void PM_FlyVehicleMove()
{
	vec3_t wishvel;
	float wishspeed;
	vec3_t wishdir;

	// We don't use these here because we pre-calculate the movedir in the vehicle update anyways, and if
	// you leave this, you get strange motion during boarding (the player can move the vehicle).
	//fmove = pm->cmd.forwardmove;
	//smove = pm->cmd.rightmove;

	// normal slowdown
	if (pm->ps->gravity && pm->ps->velocity[2] < 0 && pm->ps->groundEntityNum == ENTITYNUM_NONE)
	{
		//falling
		const float z_vel = pm->ps->velocity[2];
		PM_Friction();
		pm->ps->velocity[2] = z_vel;
	}
	else
	{
		PM_Friction();
		if (pm->ps->velocity[2] < 0 && pm->ps->groundEntityNum != ENTITYNUM_NONE)
		{
			pm->ps->velocity[2] = 0; // ignore slope movement
		}
	}

	const float scale = PM_CmdScale(&pm->cmd);

	// Get The WishVel And WishSpeed
	//-------------------------------
	if (pm->ps->client_num)
	{
		//NPC
		// If The UCmds Were Set, But Never Converted Into A MoveDir, Then Make The WishDir From UCmds
		//--------------------------------------------------------------------------------------------
		{
			wishspeed = pm->ps->speed;
			VectorScale(pm->ps->moveDir, pm->ps->speed, wishvel);
			VectorCopy(pm->ps->moveDir, wishdir);
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			constexpr float smove = 0.0f;
			constexpr float fmove = 0.0f;
			wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
		}
		// when going up or down slopes the wish velocity should Not be zero
		//	wishvel[2] = 0;

		VectorCopy(wishvel, wishdir);
		wishspeed = VectorNormalize(wishdir);
		wishspeed *= scale;
	}

	// Handle negative speed.
	if (wishspeed < 0)
	{
		VectorScale(wishvel, -1.0f, wishvel);
		VectorScale(wishdir, -1.0f, wishdir);
	}

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	PM_Accelerate(wishdir, wishspeed, 100);

	PM_StepSlideMove(1);
}

/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove()
{
	vec3_t wishvel{};
	vec3_t wishdir;
	float accel;
	qboolean low_grav_move = qfalse;
	qboolean jet_pack_move = qfalse;

	// normal slowdown
	PM_Friction();

	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& pm->gent
		&& pm->gent->client
		&& (pm->gent->client->NPC_class == CLASS_BOBAFETT || pm->gent->client->NPC_class == CLASS_MANDALORIAN || pm->
			gent->client->NPC_class == CLASS_JANGO || pm->gent->client->NPC_class == CLASS_JANGODUAL || pm->gent->client
			->NPC_class == CLASS_ROCKETTROOPER) && pm->gent->client->moveType == MT_FLYSWIM)
	{
		//jetpack accel
		accel = pm_flyaccelerate;
		jet_pack_move = qtrue;
	}
	else if (pm->ps->gravity <= 0
		&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() || pm->gent && pm->gent->client && pm->gent->
			client->moveType == MT_RUNJUMP))
	{
		PM_CheckJump();
		accel = 1.0f;
		pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->jumpZStart = pm->ps->origin[2]; //so we don't take a lot of damage when the gravity comes back on
		low_grav_move = qtrue;
	}
	else
	{
		accel = pm_flyaccelerate;
	}

	const float scale = PM_CmdScale(&pm->cmd);
	//
	// user intentions
	//
	if (!scale)
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
		}
		if (jet_pack_move)
		{
			wishvel[2] += pm->cmd.upmove;
		}
		else if (low_grav_move)
		{
			wishvel[2] += scale * pm->cmd.upmove;
			VectorScale(wishvel, 0.5f, wishvel);
		}
	}

	VectorCopy(wishvel, wishdir);
	const float wishspeed = VectorNormalize(wishdir);

	PM_Accelerate(wishdir, wishspeed, accel);

	PM_StepSlideMove(1);
}

qboolean PM_GroundSlideOkay(const float z_normal)
{
	if (z_normal > 0)
	{
		if (pm->ps->velocity[2] > 0)
		{
			if (pm->ps->legsAnim == BOTH_WALL_RUN_RIGHT
				|| pm->ps->legsAnim == BOTH_WALL_RUN_LEFT
				|| pm->ps->legsAnim == BOTH_WALL_RUN_RIGHT_STOP
				|| pm->ps->legsAnim == BOTH_WALL_RUN_LEFT_STOP
				|| pm->ps->legsAnim == BOTH_FORCEWALLRUNFLIP_START
				|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_START
				|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_ATTACK
				|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_LAND
				|| PM_InReboundJump(pm->ps->legsAnim))
			{
				return qfalse;
			}
		}
	}
	return qtrue;
}

/*
===================
PM_AirMove

===================
*/
void PM_CheckGrab();

static void PM_AirMove()
{
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	usercmd_t cmd;
	float grav_mod = 1.0f;

#if METROID_JUMP
	PM_CheckJump();
#endif
	PM_CheckGrab();

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	cmd = pm->cmd;
	PM_CmdScale(&cmd);

	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;
	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	const Vehicle_t* p_veh = nullptr;

	if (pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
	{
		p_veh = pm->gent->m_pVehicle;
	}

	if (p_veh && p_veh->m_pVehicleInfo->hoverHeight > 0)
	{
		//in a hovering vehicle, have air control
		// Flying Or Breaking, No Control
		//--------------------------------
		if (p_veh->m_ulFlags & VEH_FLYING || p_veh->m_ulFlags & VEH_SLIDEBREAKING)
		{
			VectorClear(wishvel);
			VectorClear(wishdir);
		}

		// Out Of Control - Maintain pos3 Velocity
		//-----------------------------------------
		else if (p_veh->m_ulFlags & VEH_OUTOFCONTROL || p_veh->m_ulFlags & VEH_STRAFERAM)
		{
			if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
			{
			}
			else
			{
				VectorCopy(pm->gent->pos3, wishvel);
				VectorCopy(wishvel, wishdir);
				VectorNormalize(wishdir);
			}
		}

		// Boarding - Maintain Boarding Velocity
		//---------------------------------------
		else if (p_veh->m_iBoarding)
		{
			VectorCopy(p_veh->m_vBoardingVelocity, wishvel);
			VectorCopy(wishvel, wishdir);
			VectorNormalize(wishdir);
		}

		// Otherwise, Normal Velocity
		//----------------------------
		else
		{
			pm->ps->speed;
			VectorScale(pm->ps->moveDir, pm->ps->speed, wishvel);
			VectorCopy(pm->ps->moveDir, wishdir);
		}
	}
	else if (pm->ps->pm_flags & PMF_SLOW_MO_FALL)
	{
		//no air-control
		VectorClear(wishvel);
	}
	else
	{
		for (int i = 0; i < 2; i++)
		{
			wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
		}
		wishvel[2] = 0;
	}

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (DotProduct(pm->ps->velocity, wishdir) < 0.0f)
	{
		//Encourage deceleration away from the current velocity
		wishspeed *= pm_airDecelRate;
	}

	// not on ground, so little effect on velocity
	float accelerate = pm_airaccelerate;
	if (p_veh && p_veh->m_pVehicleInfo->type == VH_SPEEDER)
	{
		//speeders have more control in air
		//in mid-air
		accelerate = p_veh->m_pVehicleInfo->acceleration;
		if (pml.groundPlane)
		{
			//on a slope of some kind, shouldn't have much control and should slide a lot
			accelerate *= 0.5f;
		}
		if (p_veh->m_ulFlags & VEH_SLIDEBREAKING)
		{
			VectorScale(pm->ps->velocity, 0.80f, pm->ps->velocity);
		}
		if (pm->ps->velocity[2] > 1000.0f)
		{
			pm->ps->velocity[2] = 1000.0f;
		}
	}
	PM_Accelerate(wishdir, wishspeed, accelerate);

	// we may have a ground plane that is very steep, even
	// though we don't have a ground entity
	// slide along the steep plane
	if (pml.groundPlane)
	{
		if (PM_GroundSlideOkay(pml.groundTrace.plane.normal[2]))
		{
			PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
				pm->ps->velocity, OVERCLIP);
		}
	}

	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0
		&& pm->ps->forceJumpZStart
		&& pm->ps->velocity[2] > 0)
	{
		//I am force jumping and I'm not holding the button anymore
		const float cur_height = pm->ps->origin[2] - pm->ps->forceJumpZStart + pm->ps->velocity[2] * pml.frametime;
		const float max_jump_height = forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]];
		if (cur_height >= max_jump_height)
		{
			//reached top, cut velocity
			pm->ps->velocity[2] = 0;
		}
	}
	if (pm->ps->pm_flags & PMF_STUCK_TO_WALL)
	{
		grav_mod = 0.0f;
	}
	PM_StepSlideMove(grav_mod);

	if (p_veh && pm->ps->pm_flags & PMF_BUMPED)
	{
		// Reduce "Bounce Up Wall" Velocity
		//----------------------------------
		if (pm->ps->velocity[2] > 0)
		{
			pm->ps->velocity[2] *= 0.1f;
		}
	}
}

/*
===================
PM_WalkMove

===================
*/
qboolean PM_CrouchAnim(int anim);

static void PM_WalkMove()
{
	int i;
	vec3_t wishvel{};
	vec3_t wishdir;
	float wishspeed;
	usercmd_t cmd;
	float accelerate;

	if (pm->ps->gravity < 0)
	{
		//float away
		pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane = qfalse;
		pml.walking = qfalse;
		if (pm->waterlevel > 1)
		{
			PM_WaterMove();
		}
		else
		{
			PM_AirMove();
		}
		return;
	}

	if (pm->waterlevel > 2 && DotProduct(pml.forward, pml.groundTrace.plane.normal) > 0)
	{
		// begin swimming
		PM_WaterMove();
		return;
	}

	if (PM_CheckJump())
	{
		// jumped away
		if (pm->waterlevel > 1)
		{
			PM_WaterMove();
		}
		else
		{
			PM_AirMove();
		}
		return;
	}

	if (pm->ps->groundEntityNum != ENTITYNUM_NONE && //on ground
		pm->ps->velocity[2] <= 0 && //not going up
		pm->ps->pm_flags & PMF_TIME_KNOCKBACK) //knockback fimter on (stops friction)
	{
		pm->ps->pm_flags &= ~PMF_TIME_KNOCKBACK;
	}

	qboolean slide = qfalse;
	if (pm->ps->pm_type == PM_DEAD)
	{
		//corpse
		if (g_entities[pm->ps->groundEntityNum].client)
		{
			//on a client
			if (g_entities[pm->ps->groundEntityNum].health > 0)
			{
				//a living client
				//no friction
				slide = qtrue;
			}
		}
	}
	if (!slide)
	{
		PM_Friction();
	}

	const float fmove = pm->cmd.forwardmove;
	const float smove = pm->cmd.rightmove;

	cmd = pm->cmd;
	const float scale = PM_CmdScale(&cmd);

	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;

	// project the forward and right directions onto the ground plane
	PM_ClipVelocity(pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP);
	PM_ClipVelocity(pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP);
	//
	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	if (pm->gent->client->NPC_class == CLASS_BOC && pm->gent->NPC)
	{
		pm->gent->client->moveType = MT_RUNJUMP;
	}

	// Get The WishVel And WishSpeed
	//-------------------------------
	if (pm->ps->client_num)
	{
		//NPC
		// If The UCmds Were Set, But Never Converted Into A MoveDir, Then Make The WishDir From UCmds
		//--------------------------------------------------------------------------------------------
		if ((fmove != 0.0f || smove != 0.0f) && VectorCompare(pm->ps->moveDir, vec3_origin))
		{
			//gi.Printf("Generating MoveDir\n");
			for (i = 0; i < 3; i++)
			{
				wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
			}

			VectorCopy(wishvel, wishdir);
			wishspeed = VectorNormalize(wishdir);
			wishspeed *= scale;
		}
		// Otherwise, Use The Move Dir
		//-----------------------------
		else
		{
			wishspeed = pm->ps->speed;
			VectorScale(pm->ps->moveDir, pm->ps->speed, wishvel);
			VectorCopy(pm->ps->moveDir, wishdir);
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
		}
		// when going up or down slopes the wish velocity should Not be zero
		//	wishvel[2] = 0;

		VectorCopy(wishvel, wishdir);
		wishspeed = VectorNormalize(wishdir);
		wishspeed *= scale;
	}

	// Handle negative speed.
	if (wishspeed < 0)
	{
		wishspeed = wishspeed * -1.0f;
		VectorScale(wishvel, -1.0f, wishvel);
		VectorScale(wishdir, -1.0f, wishdir);
	}

	// clamp the speed lower if ducking
	if (pm->ps->pm_flags & PMF_DUCKED && !PM_InKnockDown(pm->ps))
	{
		if (wishspeed > pm->ps->speed * pm_duckScale)
		{
			wishspeed = pm->ps->speed * pm_duckScale;
		}
	}

	// clamp the speed lower if wading or walking on the bottom
	if (pm->waterlevel)
	{
		float water_scale = pm->waterlevel / 3.0;
		water_scale = 1.0 - (1.0 - pm_swimScale) * water_scale;
		if (wishspeed > pm->ps->speed * water_scale)
		{
			wishspeed = pm->ps->speed * water_scale;
		}
	}

	// when a player gets hit, they temporarily lose
	// full control, which allows them to be moved a bit
	if (Flying == FLY_HOVER)
	{
		accelerate = pm_vehicleaccelerate;
	}
	else if (pm->ps->pm_flags & PMF_TIME_KNOCKBACK || pm->ps->pm_flags & PMF_TIME_NOFRICTION)
	{
		accelerate = pm_airaccelerate;
	}
	else if (pml.groundTrace.surfaceFlags & SURF_SLICK)
	{
		if (g_allowslipping->integer &&
			!(pm->cmd.buttons & BUTTON_WALKING))
		{
			accelerate = 0.1f;

			if (!PM_SpinningSaberAnim(pm->gent->client->ps.legsAnim)
				&& !PM_FlippingAnim(pm->gent->client->ps.legsAnim)
				&& !PM_RollingAnim(pm->gent->client->ps.legsAnim)
				&& !PM_InKnockDown(&pm->gent->client->ps))
			{
				int knock_anim; //default knockdown
				if (Q_irand(0, 3) > 2)
				{
					knock_anim = BOTH_KNOCKDOWN1;
				}
				else
				{
					knock_anim = BOTH_KNOCKDOWN4;
				}

				PM_SetAnim(pm, SETANIM_BOTH, knock_anim,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
				pm->gent->client->ps.legsAnimTimer += 1000;
				pm->gent->client->ps.torsoAnimTimer += 1000;

				if (PM_InOnGroundAnim(pm->ps) || PM_InKnockDown(pm->ps))
				{
					pm->gent->client->ps.legsAnimTimer += 1000;
					pm->gent->client->ps.torsoAnimTimer += 1000;
				}
			}
		}
		else
		{
			accelerate = pm_airaccelerate;
		}
	}
	else
	{
		accelerate = pm_accelerate;

		// Wind Affects Acceleration
		//===================================================
		if (wishspeed > 0.0f && pm->gent && !pml.walking)
		{
			if (gi.WE_GetWindGusting(pm->gent->currentOrigin))
			{
				vec3_t wind_dir;
				if (gi.WE_GetWindVector(wind_dir, pm->gent->currentOrigin))
				{
					if (gi.WE_IsOutside(pm->gent->currentOrigin))
					{
						VectorScale(wind_dir, -1.0f, wind_dir);
						accelerate *= 1.0f - DotProduct(wishdir, wind_dir) * 0.55f;
					}
				}
			}
		}
		//===================================================
	}

	PM_Accelerate(wishdir, wishspeed, accelerate);

	if (pml.groundTrace.surfaceFlags & SURF_SLICK || pm->ps->pm_flags & PMF_TIME_KNOCKBACK || pm->ps->pm_flags &
		PMF_TIME_NOFRICTION)
	{
		if (pm->ps->gravity >= 0 && pm->ps->groundEntityNum != ENTITYNUM_NONE && !VectorLengthSquared(pm->ps->velocity)
			&& pml.groundTrace.plane.normal[2] == 1.0)
		{
			//on ground and not moving and on level ground, no reason to do stupid fucking gravity with the clipvelocity!!!!
		}
		else
		{
			if (!(pm->ps->eFlags & EF_FORCE_GRIPPED) && !(pm->ps->eFlags & EF_FORCE_DRAINED) && !(pm->ps->eFlags &
				EF_FORCE_GRASPED))
			{
				pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
			}
		}
	}
	else
	{
		// don't reset the z velocity for slopes
		//		pm->ps->velocity[2] = 0;
	}

	const float vel = VectorLength(pm->ps->velocity);

	// slide along the ground plane
	PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
		pm->ps->velocity, OVERCLIP);

	// don't decrease velocity when going up or down a slope
	VectorNormalize(pm->ps->velocity);
	VectorScale(pm->ps->velocity, vel, pm->ps->velocity);

	// don't do anything if standing still
	if (!pm->ps->velocity[0] && !pm->ps->velocity[1])
	{
		return;
	}

	if (pm->ps->gravity <= 0)
	{
		//need to apply gravity since we're going to float up from ground
		PM_StepSlideMove(1);
	}
	else
	{
		PM_StepSlideMove(0);
	}
}

static void PM_BocMove()
{
	if (pm->gent->client->moveType == MT_RUNJUMP)
	{
		const float cur_height = pm->ps->origin[2] - pm->ps->forceJumpZStart;
		//check for max force jump level and cap off & cut z vel
		if ((cur_height <= forceJumpHeight[0] || pm->ps->forcePower && pm->cmd.upmove >= 10)
			&& cur_height < forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]])
		{
			//can still go up
			if (cur_height > forceJumpHeight[0])
			{
				//passed normal jump height  *2?
				if (!(pm->ps->forcePowersActive & 1 << FP_LEVITATION)) //haven't started forcejump yet
				{
					//start force jump
					pm->ps->forcePowersActive |= 1 << FP_LEVITATION;

					G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.wav");

					if ((pm->cmd.forwardmove || pm->cmd.rightmove) &&
						pm->ps->legsAnim != BOTH_FLIP_F &&
						pm->ps->legsAnim != BOTH_FLIP_B &&
						pm->ps->legsAnim != BOTH_FLIP_R &&
						pm->ps->legsAnim != BOTH_FLIP_L
						&& cg.renderingThirdPerson //third person only
						&& !cg.zoomMode) //not zoomed in
					{
						//FIXME: this could end up playing twice if the jump is very long...
						int anim = BOTH_FORCEINAIR1;
						int parts = SETANIM_BOTH;

						if (pm->cmd.forwardmove > 0)
						{
							anim = BOTH_FLIP_F;
						}
						else if (pm->cmd.forwardmove < 0)
						{
							anim = BOTH_FLIP_B;
						}
						else if (pm->cmd.rightmove > 0)
						{
							anim = BOTH_FLIP_R;
						}
						else if (pm->cmd.rightmove < 0)
						{
							anim = BOTH_FLIP_L;
						}
						if (pm->ps->weaponTime)
						{
							//FIXME: really only care if we're in a saber attack anim...
							parts = SETANIM_LEGS;
						}

						PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					else if (pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
					{
						vec3_t facing_fwd, facing_right;
						const vec3_t facing_angles = { 0, pm->ps->viewangles[YAW], 0 };
						int anim = -1;
						AngleVectors(facing_angles, facing_fwd, facing_right, nullptr);
						const float dot_r = DotProduct(facing_right, pm->ps->velocity);
						const float dot_f = DotProduct(facing_fwd, pm->ps->velocity);
						if (fabs(dot_r) > fabs(dot_f) * 1.5)
						{
							if (dot_r > 150)
							{
								anim = BOTH_FORCEJUMPRIGHT1;
							}
							else if (dot_r < -150)
							{
								anim = BOTH_FORCEJUMPLEFT1;
							}
						}
						else
						{
							if (dot_f > 150)
							{
								anim = BOTH_FORCEJUMP1;
							}
							else if (dot_f < -150)
							{
								anim = BOTH_FORCEJUMPBACK1;
							}
						}
						if (anim != -1)
						{
							int parts = SETANIM_BOTH;
							if (pm->ps->weaponTime)
							{
								parts = SETANIM_LEGS;
							}

							PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						}
					}
				}
				else
				{
					if (!pm->ps->legsAnimTimer)
					{
						//not in the middle of a legsAnim
						const int anim = pm->ps->legsAnim;
						int new_anim = -1;
						switch (anim)
						{
						case BOTH_FORCEJUMP1:
							new_anim = BOTH_FORCELAND1;
							break;
						case BOTH_FORCEJUMPBACK1:
							new_anim = BOTH_FORCELANDBACK1;
							break;
						case BOTH_FORCEJUMPLEFT1:
							new_anim = BOTH_FORCELANDLEFT1;
							break;
						case BOTH_FORCEJUMPRIGHT1:
							new_anim = BOTH_FORCELANDRIGHT1;
							break;
						default:;
						}
						if (new_anim != -1)
						{
							int parts = SETANIM_BOTH;
							if (pm->ps->weaponTime)
							{
								//FIXME: really only care if we're in a saber attack anim...
								parts = SETANIM_LEGS;
							}

							PM_SetAnim(pm, parts, new_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						}
					}
				}
			}

			//need to scale this down, start with height velocity (based on max force jump height) and scale down to regular jump vel
			pm->ps->velocity[2] = (forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]] - cur_height) /
				forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]] * forceJumpStrength[pm->ps->forcePowerLevel[
					FP_LEVITATION]]; //JUMP_VELOCITY;
			pm->ps->velocity[2] /= 10;
			pm->ps->velocity[2] += JUMP_VELOCITY;
			pm->ps->pm_flags |= PMF_JUMP_HELD;
		}
		else if (cur_height > forceJumpHeight[0] && cur_height < forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]]
			-
			forceJumpHeight[0])
		{
			//still have some headroom, don't totally stop it
			if (pm->ps->velocity[2] > JUMP_VELOCITY)
			{
				pm->ps->velocity[2] = JUMP_VELOCITY;
			}
		}
		else
		{
			pm->ps->velocity[2] = 0;
		}
	}
	else
	{
		PM_WalkMove();
	}
}

/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove()
{
	if (!pml.walking)
	{
		return;
	}

	// extra friction

	float forward = VectorLength(pm->ps->velocity);
	forward -= 20;
	if (forward <= 0)
	{
		VectorClear(pm->ps->velocity);
	}
	else
	{
		VectorNormalize(pm->ps->velocity);
		VectorScale(pm->ps->velocity, forward, pm->ps->velocity);
	}
}

/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove()
{
	vec3_t wishvel{};
	vec3_t wishdir;

	if (pm->gent && pm->gent->client)
	{
		pm->ps->viewheight = pm->gent->client->standheight + STANDARD_VIEWHEIGHT_OFFSET;

		VectorCopy(pm->gent->mins, pm->mins);
		VectorCopy(pm->gent->maxs, pm->maxs);
	}
	else
	{
		pm->ps->viewheight = DEFAULT_MAXS_2 + STANDARD_VIEWHEIGHT_OFFSET; //DEFAULT_VIEWHEIGHT;

		if (!DEFAULT_MINS_0 || !DEFAULT_MINS_1 || !DEFAULT_MAXS_0 || !DEFAULT_MAXS_1)
		{
			assert(0);
		}

		pm->mins[0] = DEFAULT_MINS_0;
		pm->mins[1] = DEFAULT_MINS_1;
		pm->mins[2] = DEFAULT_MINS_2;

		pm->maxs[0] = DEFAULT_MAXS_0;
		pm->maxs[1] = DEFAULT_MAXS_1;
		pm->maxs[2] = DEFAULT_MAXS_2;
	}

	// friction

	const float speed = VectorLength(pm->ps->velocity);
	if (speed < 1)
	{
		VectorCopy(vec3_origin, pm->ps->velocity);
	}
	else
	{
		float drop = 0;

		constexpr float friction = pm_friction * 1.5; // extra friction
		const float control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;

		// scale the velocity
		float newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale(pm->ps->velocity, newspeed, pm->ps->velocity);
	}

	// accelerate
	float scale = PM_CmdScale(&pm->cmd);
	if (pm->cmd.buttons & BUTTON_ATTACK)
	{
		//turbo boost
		scale *= 10;
	}
	if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		//turbo boost
		scale *= 10;
	}

	const float fmove = pm->cmd.forwardmove;
	const float smove = pm->cmd.rightmove;

	for (int i = 0; i < 3; i++)
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy(wishvel, wishdir);
	float wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	PM_Accelerate(wishdir, wishspeed, pm_accelerate);

	// move
	VectorMA(pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}

//============================================================================

static float PM_DamageForDelta(const int delta)
{
	float damage = delta;
	if (pm->gent->NPC)
	{
		if (pm->ps->weapon == WP_SABER
			|| pm->gent->client && pm->gent->client->NPC_class == CLASS_REBORN)
		{
			damage = 0;
		}
	}
	else if (pm->ps->client_num < MAX_CLIENTS)
	{
		if (damage < 50)
		{
			if (damage > 24)
			{
				damage = damage - 25;
			}
		}
		else
		{
			damage *= 0.5f;
		}
	}
	return damage * 0.5f;
}

static void PM_CrashLandDamage(int damage)
{
	if (pm->gent)
	{
		int dflags = DAMAGE_NO_ARMOR;
		if (pm->gent->NPC && pm->gent->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
		{
			damage = 1000;
			dflags |= DAMAGE_DIE_ON_IMPACT;
		}
		else
		{
			damage = PM_DamageForDelta(damage);

			if (pm->gent->flags & FL_NO_IMPACT_DMG)
				return;
		}

		if (damage)
		{
			if (PM_InForceFall())
			{
				// no forcefall damage
			}
			else
			{
				pm->gent->painDebounceTime = level.time + 200; // no normal pain sound
				G_Damage(pm->gent, nullptr, player, nullptr, nullptr, damage, dflags, MOD_FALLING);
			}
		}
	}
}

static float PM_CrashLandDelta(vec3_t prev_vel)
{
	if (pm->waterlevel == 3)
	{
		return 0;
	}
	float delta = fabs(prev_vel[2]) / 10; //VectorLength( prev_vel )

	// reduce falling damage if there is standing water
	if (pm->waterlevel == 2)
	{
		delta *= 0.25;
	}
	if (pm->waterlevel == 1)
	{
		delta *= 0.5;
	}

	return delta;
}

static int PM_GetLandingAnim()
{
	int anim = pm->ps->legsAnim;

	//special cases:
	if (anim == BOTH_FLIP_ATTACK7
		|| anim == BOTH_FLIP_HOLD7)
	{
		return BOTH_FLIP_LAND;
	}
	if (anim == BOTH_FLIP_LAND)
	{
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		return BOTH_LAND1;
	}
	if (PM_InAirKickingAnim(anim))
	{
		switch (anim)
		{
		case BOTH_A7_KICK_F_AIR:
		case BOTH_FLYING_KICK:
			return BOTH_FORCELAND1;
		case BOTH_A7_KICK_B_AIR:
			return BOTH_FORCELANDBACK1;
		case BOTH_A7_KICK_R_AIR:
			return BOTH_FORCELANDRIGHT1;
		case BOTH_A7_KICK_L_AIR:
			return BOTH_FORCELANDLEFT1;
		default:;
		}
	}

	if (PM_SpinningAnim(anim) || pm_saber_in_special_attack(anim))
	{
		return -1;
	}
	switch (anim)
	{
	case BOTH_FORCEJUMPLEFT1:
	case BOTH_FORCEINAIRLEFT1:
		anim = BOTH_FORCELANDLEFT1;
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	case BOTH_FORCEJUMPRIGHT1:
	case BOTH_FORCEINAIRRIGHT1:
		anim = BOTH_FORCELANDRIGHT1;
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	case BOTH_FORCEJUMP1:
	case BOTH_FORCEINAIR1:
	case BOTH_GRAPPLE_PULL:
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		anim = BOTH_FORCELAND1;
		break;
	case BOTH_FORCEJUMPBACK1:
	case BOTH_FORCEINAIRBACK1:
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		anim = BOTH_FORCELANDBACK1;
		break;
	case BOTH_JUMPLEFT1:
	case BOTH_INAIRLEFT1:
		anim = BOTH_LANDLEFT1;
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	case BOTH_JUMPRIGHT1:
	case BOTH_INAIRRIGHT1:
		anim = BOTH_LANDRIGHT1;
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	case BOTH_JUMP1:
	case BOTH_INAIR1:
		anim = BOTH_LAND1;
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	case BOTH_JUMPBACK1:
	case BOTH_INAIRBACK1:
		anim = BOTH_LANDBACK1;
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_LUNGE2_B__T_:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_ARIAL_LEFT:
	case BOTH_ARIAL_RIGHT:
	case BOTH_ARIAL_F1:
	case BOTH_CARTWHEEL_LEFT:
	case BOTH_CARTWHEEL_RIGHT:
	case BOTH_JUMPFLIPSLASHDOWN1: //#
	case BOTH_JUMPFLIPSTABDOWN: //#
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_A7_KICK_F:
	case BOTH_KICK_F_MD:
	case BOTH_A7_KICK_B:
	case BOTH_SWEEP_KICK:
	case BOTH_A7_KICK_R:
	case BOTH_A7_KICK_L:
	case BOTH_A7_KICK_S:
	case BOTH_A7_KICK_BF:
	case BOTH_A7_KICK_RL:
	case BOTH_A7_KICK_F_AIR:
	case BOTH_FLYING_KICK:
	case BOTH_A7_KICK_B_AIR:
	case BOTH_A7_KICK_R_AIR:
	case BOTH_A7_KICK_L_AIR:
	case BOTH_A7_SLAP_R:
	case BOTH_A7_SLAP_L:
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_GRAPPLE_FIRE:
		anim = -1;
		break;
	case BOTH_FORCELONGLEAP_START:
	case BOTH_FORCELONGLEAP_ATTACK:
		return BOTH_FORCELONGLEAP_LAND;
	case BOTH_WALL_RUN_LEFT: //#
	case BOTH_WALL_RUN_RIGHT: //#
		if (pm->ps->legsAnimTimer > 500)
		{
			//only land at end of anim
			return -1;
		}
		//NOTE: falls through on purpose!
	default:
		if (pm->ps->pm_flags & PMF_BACKWARDS_JUMP)
		{
			anim = BOTH_LANDBACK1;
		}
		else
		{
			anim = BOTH_LAND1;
		}
		if (!g_allowBunnyhopping->integer)
		{
			//stick landings some
			pm->ps->velocity[0] *= 0.5f;
			pm->ps->velocity[1] *= 0.5f;
		}
		break;
	}
	return anim;
}

static void G_StartDash(gentity_t* ent, const int anim)
{
	NPC_SetAnim(ent, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
	ent->client->ps.weaponTime = ent->client->ps.torsoAnimTimer - 200;
	ent->client->ps.saber_move = LS_NONE;
}

void G_StartRoll(gentity_t* ent, const int anim)
{
	NPC_SetAnim(ent, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
	ent->client->ps.weaponTime = ent->client->ps.torsoAnimTimer - 200;
	//just to make sure it's cleared when roll is done
	G_AddEvent(ent, EV_ROLL, 0);
	ent->client->ps.saber_move = LS_NONE;
}

static qboolean PM_TryRoll()
{
	constexpr float roll_dist = 192; //was 64;

	if (PM_SaberInAttack(pm->ps->saber_move) || pm_saber_in_special_attack(pm->ps->torsoAnim)
		|| PM_SpinningSaberAnim(pm->ps->legsAnim)
		|| PM_SaberInStart(pm->ps->saber_move))
	{
		//attacking or spinning (or, if player, starting an attack)
		if (PM_CanRollFromSoulCal(pm->ps))
		{
			//hehe
		}
		else
		{
			return qfalse;
		}
	}

	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && !BG_AllowThirdPersonSpecialMove(pm->ps))
	{
		//player can't do this in 1st person
		return qfalse;
	}
	if (!pm->gent)
	{
		return qfalse;
	}

	if (pm->gent->client->NPC_class == CLASS_VADER || pm->gent->client->NPC_class == CLASS_DESANN)
	{
		//no anim
		PM_SaberLungeAttackMove(qtrue);
	}

	if (pm->ps->saber[0].saberFlags & SFL_NO_ROLLS)
	{
		return qfalse;
	}
	if (pm->ps->dualSabers
		&& pm->ps->saber[1].saberFlags & SFL_NO_ROLLS)
	{
		return qfalse;
	}

	vec3_t fwd, right, traceto;
	const vec3_t fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
	const vec3_t maxs = { pm->maxs[0], pm->maxs[1], static_cast<float>(pm->gent->client->crouchheight) };
	const vec3_t mins = { pm->mins[0], pm->mins[1], pm->mins[2] + STEPSIZE };
	trace_t trace;
	int anim = -1;
	AngleVectors(fwd_angles, fwd, right, nullptr);

	if (pm->cmd.forwardmove)
	{
		if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
		{
			anim = BOTH_ROLL_B;
			VectorMA(pm->ps->origin, -roll_dist, fwd, traceto);
		}
		else
		{
			anim = BOTH_ROLL_F;
			VectorMA(pm->ps->origin, roll_dist, fwd, traceto);
		}
	}
	else if (pm->cmd.rightmove > 0)
	{
		anim = BOTH_ROLL_R;
		VectorMA(pm->ps->origin, roll_dist, right, traceto);
	}
	else if (pm->cmd.rightmove < 0)
	{
		anim = BOTH_ROLL_L;
		VectorMA(pm->ps->origin, -roll_dist, right, traceto);
	}
	else
	{
		//???
	}

	if (anim != -1)
	{
		qboolean roll = qfalse;
		int clipmask = CONTENTS_SOLID;
		if (pm->ps->client_num)
		{
			clipmask |= CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP;
		}
		else
		{
			if (pm->gent && pm->gent->enemy && pm->gent->enemy->health > 0)
			{
				//player can always roll in combat
				roll = qtrue;
			}
			else
			{
				clipmask |= CONTENTS_PLAYERCLIP;
			}
		}
		if (!roll)
		{
			pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, clipmask,
				static_cast<EG2_Collision>(0), 0);
			if (trace.fraction >= 1.0f)
			{
				//okay, clear, check for a bottomless drop
				vec3_t top;
				VectorCopy(traceto, top);
				traceto[2] -= 256;
				pm->trace(&trace, top, mins, maxs, traceto, pm->ps->client_num, CONTENTS_SOLID,
					static_cast<EG2_Collision>(0), 0);
				if (trace.fraction < 1.0f)
				{
					//not a bottomless drop
					roll = qtrue;
				}
			}
			else
			{
				//hit an architectural obstruction
				if (pm->ps->client_num)
				{
					//NPCs don't care about rolling into walls, just off ledges
					if (!(trace.contents & CONTENTS_BOTCLIP))
					{
						roll = qtrue;
					}
				}
				else if (G_EntIsDoor(trace.entityNum))
				{
					//okay to roll into a door
					if (G_EntIsUnlockedDoor(trace.entityNum))
					{
						//if it's an auto-door
						roll = qtrue;
					}
				}
				else
				{
					//check other conditions
					const gentity_t* traceEnt = &g_entities[trace.entityNum];
					if (traceEnt && traceEnt->svFlags & SVF_GLASS_BRUSH)
					{
						//okay to roll through glass
						roll = qtrue;
					}
				}
			}
		}
		if (roll)
		{
			G_StartRoll(pm->gent, anim);
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean pm_try_roll_md()
{
	constexpr float roll_dist = 192; //was 64;

	if (PM_SaberInAttack(pm->ps->saber_move) || pm_saber_in_special_attack(pm->ps->torsoAnim)
		|| PM_SpinningSaberAnim(pm->ps->legsAnim)
		|| PM_SaberInStart(pm->ps->saber_move))
	{
		//attacking or spinning (or, if player, starting an attack)
		if (PM_CanRollFromSoulCal(pm->ps))
		{
			//hehe
		}
		else
		{
			return qfalse;
		}
	}

	if (PM_InRoll(pm->ps))
	{
		return qfalse;
	}

	if (pm->gent->client->NPC_class == CLASS_VADER || pm->gent->client->NPC_class == CLASS_DESANN)
	{
		//no anim
		PM_SaberLungeAttackMove(qtrue);
	}

	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && !BG_AllowThirdPersonSpecialMove(pm->ps))
	{
		//player can't do this in 1st person
		return qfalse;
	}
	if (!pm->gent)
	{
		return qfalse;
	}
	if (pm->ps->saber[0].saberFlags & SFL_NO_ROLLS)
	{
		return qfalse;
	}
	if (pm->ps->dualSabers
		&& pm->ps->saber[1].saberFlags & SFL_NO_ROLLS)
	{
		return qfalse;
	}

	vec3_t fwd, right, traceto;
	const vec3_t fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
	const vec3_t maxs = { pm->maxs[0], pm->maxs[1], static_cast<float>(pm->gent->client->crouchheight) };
	const vec3_t mins = { pm->mins[0], pm->mins[1], pm->mins[2] + STEPSIZE };
	trace_t trace;
	int anim = -1;
	AngleVectors(fwd_angles, fwd, right, nullptr);

	gentity_t* npc = &g_entities[pm->ps->client_num];

	if (npc->npc_roll_start)
	{
		if (npc->npc_roll_direction == EVASION_ROLL_DIR_BACK)
		{
			// backward roll
			anim = BOTH_ROLL_B;
			VectorMA(pm->ps->origin, -roll_dist, fwd, traceto);
		}
		else if (npc->npc_roll_direction == EVASION_ROLL_DIR_RIGHT)
		{
			//right
			anim = BOTH_ROLL_R;
			VectorMA(pm->ps->origin, roll_dist, right, traceto);
		}
		else if (npc->npc_roll_direction == EVASION_ROLL_DIR_LEFT)
		{
			//left
			anim = BOTH_ROLL_L;
			VectorMA(pm->ps->origin, -roll_dist, right, traceto);
		}
		// No matter what, re-initialize the roll flag, so we dont end up with npcs rolling all around the map :)
		npc->npc_roll_start = qfalse;
	}
	else if (pm->cmd.forwardmove)
	{
		if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
		{
			anim = BOTH_ROLL_B;
			VectorMA(pm->ps->origin, -roll_dist, fwd, traceto);
		}
		else
		{
			anim = BOTH_ROLL_F;
			VectorMA(pm->ps->origin, roll_dist, fwd, traceto);
		}
	}
	else if (pm->cmd.rightmove > 0)
	{
		anim = BOTH_ROLL_R;
		VectorMA(pm->ps->origin, roll_dist, right, traceto);
	}
	else if (pm->cmd.rightmove < 0)
	{
		anim = BOTH_ROLL_L;
		VectorMA(pm->ps->origin, -roll_dist, right, traceto);
	}
	else
	{
		//???
	}

	if (anim != -1)
	{
		qboolean roll = qfalse;
		int clipmask = CONTENTS_SOLID;
		if (pm->ps->client_num)
		{
			clipmask |= CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP;
		}
		else
		{
			if (pm->gent && pm->gent->enemy && pm->gent->enemy->health > 0)
			{
				//player can always roll in combat
				roll = qtrue;
			}
			else
			{
				clipmask |= CONTENTS_PLAYERCLIP;
			}
		}
		if (!roll)
		{
			pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->client_num, clipmask,
				static_cast<EG2_Collision>(0), 0);
			if (trace.fraction >= 1.0f)
			{
				//okay, clear, check for a bottomless drop
				vec3_t top;
				VectorCopy(traceto, top);
				traceto[2] -= 256;
				pm->trace(&trace, top, mins, maxs, traceto, pm->ps->client_num, CONTENTS_SOLID,
					static_cast<EG2_Collision>(0), 0);
				if (trace.fraction < 1.0f)
				{
					//not a bottomless drop
					roll = qtrue;
				}
			}
			else
			{
				//hit an architectural obstruction
				if (pm->ps->client_num)
				{
					//NPCs don't care about rolling into walls, just off ledges
					if (!(trace.contents & CONTENTS_BOTCLIP))
					{
						roll = qtrue;
					}
				}
				else if (G_EntIsDoor(trace.entityNum))
				{
					//okay to roll into a door
					if (G_EntIsUnlockedDoor(trace.entityNum))
					{
						//if it's an auto-door
						roll = qtrue;
					}
				}
				else
				{
					//check other conditions
					const gentity_t* traceEnt = &g_entities[trace.entityNum];
					if (traceEnt && traceEnt->svFlags & SVF_GLASS_BRUSH)
					{
						//okay to roll through glass
						roll = qtrue;
					}
				}
			}
		}
		if (roll)
		{
			G_StartRoll(pm->gent, anim);
			return qtrue;
		}
	}
	return qfalse;
}

extern void CG_LandingEffect(vec3_t origin, vec3_t normal, int material);

static void PM_CrashLandEffect()
{
	if (pm->waterlevel)
	{
		return;
	}
	const float delta = fabs(pml.previous_velocity[2]) / 10; //VectorLength( pml.previous_velocity );?
	if (delta >= 30)
	{
		vec3_t bottom = { pm->ps->origin[0], pm->ps->origin[1], pm->ps->origin[2] + pm->mins[2] + 1 };
		CG_LandingEffect(bottom, pml.groundTrace.plane.normal, pml.groundTrace.surfaceFlags & MATERIAL_MASK);
	}
}

/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand()
{
	float delta = 0;
	qboolean force_landing = qfalse;

	if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
	{
		if (pm->gent->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL)
		{
			const float dot = DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal);
			//Com_Printf("%i:crashland %4.2f\n", c_pmove, pm->ps->velocity[2]);
			if (dot < -100.0f)
			{
				//NOTE: never hits this anyway
				if (pm->gent->m_pVehicle->m_pVehicleInfo->iImpactFX)
				{
					//make sparks
					if (!Q_irand(0, 3))
					{
						//FIXME: debounce
						G_PlayEffect(pm->gent->m_pVehicle->m_pVehicleInfo->iImpactFX, pm->ps->origin,
							pml.groundTrace.plane.normal);
					}
				}
				const int damage = floor(fabs(dot + 100) / 10.0f);
				if (damage >= 0)
				{
					if (PM_InForceFall())
					{
						// no forcefall damage
					}
					else
					{
						G_Damage(pm->gent, nullptr, nullptr, nullptr, nullptr, damage, 0, MOD_FALLING);
					}
				}
			}
		}
		return;
	}

	//Get rid of queued buttons
	pm->cmd.buttons &= ~BUTTON_ATTACK;
	pm->cmd.buttons &= ~BUTTON_BLOCK;

	if (pm->ps->pm_flags & PMF_TRIGGER_PUSHED)
	{
		delta = 21; //?
		force_landing = qtrue;
	}
	else
	{
		if (pm->gent && pm->gent->NPC && pm->gent->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
		{
			//have to do death on impact if we are falling to our death, FIXME: should we avoid any additional damage this func?
			PM_CrashLandDamage(1000);
		}
		else if (pm->gent
			&& pm->gent->client
			&& (pm->gent->client->NPC_class == CLASS_BOBAFETT || pm->gent->client->NPC_class == CLASS_MANDALORIAN || pm
				->gent->client->NPC_class == CLASS_JANGO || pm->gent->client->NPC_class == CLASS_JANGODUAL || pm->gent->
				client->NPC_class == CLASS_ROCKETTROOPER))
		{
			if (JET_Flying(pm->gent))
			{
				if (pm->gent->client->NPC_class == CLASS_BOBAFETT || pm->gent->client->NPC_class == CLASS_MANDALORIAN
					|| pm->gent->client->NPC_class == CLASS_JANGO || pm->gent->client->NPC_class == CLASS_JANGODUAL
					|| pm->gent->client->NPC_class == CLASS_ROCKETTROOPER && pm->gent->NPC && pm->gent->NPC->rank <
					RANK_LT || rocket_trooper_player(pm->gent))
				{
					jet_fly_stop(pm->gent);
				}
				else
				{
					pm->ps->velocity[2] += Q_flrand(100, 200);
				}
				PM_AddEvent(EV_FALL_SHORT);
			}
			if (pm->ps->forceJumpZStart)
			{
				//we were force-jumping
				force_landing = qtrue;
			}
			delta = 1;
		}
		else if (pm->ps->jumpZStart && (pm->ps->forcePowerLevel[FP_LEVITATION] >= FORCE_LEVEL_1 || (pm->ps->client_num <
			MAX_CLIENTS || PM_ControlledByPlayer())))
		{
			//we were force-jumping
			if (pm->ps->origin[2] >= pm->ps->jumpZStart)
			{
				//we landed at same height or higher than we landed
				if (pm->ps->forceJumpZStart)
				{
					//we were force-jumping
					force_landing = qtrue;
				}
				delta = 0;
			}
			else
			{
				//take off some of it, at least
				delta = pm->ps->jumpZStart - pm->ps->origin[2];
				float drop_allow = forceJumpHeight[pm->ps->forcePowerLevel[FP_LEVITATION]];
				if (drop_allow < 128)
				{
					//always allow a drop from 128, at least
					drop_allow = 128;
				}
				if (delta > forceJumpHeight[FORCE_LEVEL_1])
				{
					//will have to use force jump ability to absorb some of it
					force_landing = qtrue; //absorbed some - just to force the correct animation to play below
				}
				delta = (delta - drop_allow) / 2;
			}
			if (delta < 1)
			{
				delta = 1;
			}
		}

		if (!delta)
		{
			delta = PM_CrashLandDelta(pml.previous_velocity);
		}
	}

	PM_CrashLandEffect();

	if (pm->ps->pm_flags & PMF_DUCKED && level.time - pm->ps->lastOnGround > 500)
	{
		//must be crouched and have been inthe air for half a second minimum
		if (!PM_InOnGroundAnim(pm->ps) && !PM_InKnockDown(pm->ps))
		{
			//roll!
			if (PM_TryRoll())
			{
				//absorb some impact
				delta *= 0.5f;
			}
		}
	}

	// If he just entered the water (from a fall presumably), absorb some of the impact.
	if (pm->waterlevel >= 2)
	{
		delta *= 0.4f;
	}

	if (delta < 1)
	{
		AddSoundEvent(pm->gent, pm->ps->origin, 32, AEL_MINOR, qfalse, qtrue);
		return;
	}

	qboolean dead_fall_sound = qfalse;
	if (!PM_InDeathAnim())
	{
		if (PM_InAirKickingAnim(pm->ps->legsAnim)
			&& pm->ps->torsoAnim == pm->ps->legsAnim)
		{
			const int anim = PM_GetLandingAnim();
			if (anim != -1)
			{
				//interrupting a kick clears everything
				PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
				// Only blend over 100ms
				pm->ps->saber_move = LS_READY;
				pm->ps->weaponTime = 0;
				if (!g_allowBunnyhopping->integer)
				{
					//stick landings some
					pm->ps->velocity[0] *= 0.5f;
					pm->ps->velocity[1] *= 0.5f;
				}
			}
		}
		else if (pm->gent
			&& pm->gent->client
			&& pm->gent->client->NPC_class == CLASS_ROCKETTROOPER)
		{
			//rockettroopers are simpler
			const int anim = PM_GetLandingAnim();
			if (anim != -1)
			{
				if (pm->gent->NPC
					&& pm->gent->NPC->rank < RANK_LT)
				{
					//special case: ground-based rocket troopers *always* play land anim on whole body
					PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
					// Only blend over 100ms
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
					// Only blend over 100ms
				}
			}
		}
		else if (pm->cmd.upmove >= 0 && !PM_InKnockDown(pm->ps) && !PM_InRoll(pm->ps))
		{
			//not crouching
			if (delta > 10
				|| pm->ps->pm_flags & PMF_BACKWARDS_JUMP
				|| pm->ps->forcePowersActive & 1 << FP_LEVITATION
				|| force_landing) //EV_FALL_SHORT or jumping back or force-land
			{
				// decide which landing animation to use
				if (pm->gent
					&& pm->gent->client
					&& (pm->gent->client->NPC_class == CLASS_RANCOR || pm->gent->client->NPC_class == CLASS_WAMPA))
				{
				}
				else
				{
					const int anim = PM_GetLandingAnim();
					if (anim != -1)
					{
						if (PM_FlippingAnim(pm->ps->torsoAnim)
							|| PM_SpinningAnim(pm->ps->torsoAnim)
							|| pm->ps->torsoAnim == BOTH_FLIP_LAND)
						{
							//interrupt these if we're going to play a land
							pm->ps->torsoAnimTimer = 0;
						}
						if (anim == BOTH_FORCELONGLEAP_LAND)
						{
							if (pm->gent)
							{
								G_SoundOnEnt(pm->gent, CHAN_AUTO, "sound/player/slide.wav");
							}
							PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
							// Only blend over 100ms
						}
						else if (anim == BOTH_FLIP_LAND
							|| pm->ps->torsoAnim == BOTH_FLIP_LAND)
						{
							PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
							// Only blend over 100ms
						}
						else if (PM_InAirKickingAnim(pm->ps->legsAnim)
							&& pm->ps->torsoAnim == pm->ps->legsAnim)
						{
							//interrupting a kick clears everything
							PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
							// Only blend over 100ms
							pm->ps->saber_move = LS_READY;
							pm->ps->weaponTime = 0;
						}
						else if (PM_ForceJumpingAnim(pm->ps->legsAnim)
							&& pm->ps->torsoAnim == pm->ps->legsAnim)
						{
							//special case: if land during one of these, set the torso, too, if it's not doing something else
							PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
							// Only blend over 100ms
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 100);
							// Only blend over 100ms
						}
					}
				}
			}
		}
	}
	else
	{
		pm->ps->gravity = 1.0;
		//PM_CrashLandDamage( delta );
		if (pm->gent)
		{
			if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE) &&
				//				(!(pml.groundTrace.contents & CONTENTS_NODROP)) &&
				!(pm->pointcontents(pm->ps->origin, pm->ps->client_num) & CONTENTS_NODROP))
			{
				if (pm->waterlevel < 2)
				{
					//don't play fallsplat when impact in the water
					dead_fall_sound = qtrue;
					if (!(pm->ps->eFlags & EF_NODRAW))
					{
						//no sound if no draw
						if (delta >= 75)
						{
							G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/player/fallsplat.wav");
						}
						else
						{
							G_SoundOnEnt(pm->gent, CHAN_BODY, va("sound/player/bodyfall_human%d.wav", Q_irand(1, 3)));
						}
					}
				}
				else
				{
					G_SoundOnEnt(pm->gent, CHAN_BODY, va("sound/player/bodyfall_water%d.wav", Q_irand(1, 3)));
				}
				if (gi.VoiceVolume[pm->ps->client_num]
					&& pm->gent->NPC && pm->gent->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
				{
					//I was talking, so cut it off... with a jump sound?
					if (!(pm->ps->eFlags & EF_NODRAW))
					{
						//no sound if no draw
						G_SoundOnEnt(pm->gent, CHAN_VOICE_ATTEN, "*pain100.wav");
					}
				}
			}
		}
		if (pm->ps->legsAnim == BOTH_FALLDEATH1 || pm->ps->legsAnim == BOTH_FALLDEATH1INAIR)
		{
			//FIXME: add a little bounce?
			//FIXME: cut voice channel?
			const int old_pm_type = pm->ps->pm_type;
			pm->ps->pm_type = PM_NORMAL;
			//Hack because for some reason PM_SetAnim just returns if you're dead...???
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_FALLDEATH1LAND, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->pm_type = old_pm_type;
			AddSoundEvent(pm->gent, pm->ps->origin, 256, AEL_SUSPICIOUS, qfalse, qtrue);
			return;
		}
	}

	// create a local entity event to play the sound

	if (pm->gent && pm->gent->client && pm->gent->client->respawnTime >= level.time - 500)
	{
		//just spawned in, don't make a noise
		return;
	}

	if (delta >= 75)
	{
		if (!dead_fall_sound)
		{
			if (!(pm->ps->eFlags & EF_NODRAW))
			{
				//no sound if no draw
				PM_AddEvent(EV_FALL_FAR);
			}
		}
		if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE))
		{
			PM_CrashLandDamage(delta);
		}
		if (pm->gent)
		{
			if (pm->gent->s.number == 0)
			{
				vec3_t bottom;

				VectorCopy(pm->ps->origin, bottom);
				bottom[2] += pm->mins[2];
				//	if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
				{
					AddSoundEvent(pm->gent, bottom, 256, AEL_SUSPICIOUS, qfalse, qtrue);
				}
			}
			else if (pm->ps->stats[STAT_HEALTH] <= 0 && pm->gent && pm->gent->enemy)
			{
				AddSoundEvent(pm->gent->enemy, pm->ps->origin, 256, AEL_DISCOVERED, qfalse, qtrue);
			}
		}
	}
	else if (delta >= 50)
	{
		// this is a pain grunt, so don't play it if dead
		if (pm->ps->stats[STAT_HEALTH] > 0)
		{
			if (!dead_fall_sound)
			{
				if (!(pm->ps->eFlags & EF_NODRAW))
				{
					//no sound if no draw
					PM_AddEvent(EV_FALL_MEDIUM); //damage is dealt in g_active, ClientEvents
				}
			}
			if (pm->gent)
			{
				if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE))
				{
					PM_CrashLandDamage(delta);
				}
				if (pm->gent->s.number == 0)
				{
					vec3_t bottom;

					VectorCopy(pm->ps->origin, bottom);
					bottom[2] += pm->mins[2];
					//	if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
					{
						AddSoundEvent(pm->gent, bottom, 256, AEL_MINOR, qfalse, qtrue);
					}
				}
			}
		}
	}
	else if (delta >= 30)
	{
		if (!dead_fall_sound)
		{
			if (!(pm->ps->eFlags & EF_NODRAW))
			{
				//no sound if no draw
				PM_AddEvent(EV_FALL_SHORT);
			}
		}
		if (pm->gent)
		{
			if (pm->gent->s.number == 0)
			{
				vec3_t bottom;

				VectorCopy(pm->ps->origin, bottom);
				bottom[2] += pm->mins[2];
				//		if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
				{
					AddSoundEvent(pm->gent, bottom, 128, AEL_MINOR, qfalse, qtrue);
				}
			}
			else
			{
				if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE))
				{
					PM_CrashLandDamage(delta);
				}
			}
		}
	}
	else
	{
		if (!dead_fall_sound)
		{
			if (!(pm->ps->pm_flags & PMF_DUCKED) || !Q_irand(0, 3))
			{
				//only 25% chance of making landing alert when ducked
				AddSoundEvent(pm->gent, pm->ps->origin, 32, AEL_MINOR, qfalse, qtrue);
			}
			if (!(pm->ps->eFlags & EF_NODRAW))
			{
				//no sound if no draw
				if (force_landing)
				{
					//we were force-jumping
					PM_AddEvent(EV_FALL_SHORT);
				}
				else
				{
					//moved to cg_player				PM_AddEvent( PM_FootstepForSurface() );
				}
			}
		}
	}

	if (PM_InForceFall())
	{
		return;
	}

	VectorClear(pm->ps->velocity);

	// start footstep cycle over
	pm->ps->bobCycle = 0;

	if (pm->gent && pm->gent->client)
	{
		//stop the force push effect when you land
		pm->gent->forcePushTime = 0;
	}
}

/*
=============
PM_CorrectAllSolid
=============
*/
static void PM_CorrectAllSolid()
{
	if (pm->debugLevel)
	{
		Com_Printf("%i:allsolid\n", c_pmove); //NOTENOTE: If this ever happens, I'd really like to see this print!
	}

	// FIXME: jitter around

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
}

qboolean FlyingCreature(const gentity_t* ent)
{
	if (ent->client->ps.gravity <= 0 && ent->svFlags & SVF_CUSTOM_GRAVITY)
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_RocketeersAvoidDangerousFalls()
{
	if (pm->gent->NPC
		&& pm->gent->client
		&& (pm->gent->client->NPC_class == CLASS_BOBAFETT || pm->gent->client->NPC_class == CLASS_MANDALORIAN || pm->
			gent->client->NPC_class == CLASS_JANGO || pm->gent->client->NPC_class == CLASS_JANGODUAL || pm->gent->client
			->NPC_class == CLASS_ROCKETTROOPER))
	{
		//fixme:  fall through if jetpack broken?
		if (JET_Flying(pm->gent))
		{
			if (pm->gent->client->NPC_class == CLASS_BOBAFETT ||
				pm->gent->client->NPC_class == CLASS_MANDALORIAN ||
				pm->gent->client->NPC_class == CLASS_JANGO || pm->gent->client->NPC_class == CLASS_JANGODUAL)
			{
				pm->gent->client->jetPackTime = level.time + 2000;
			}
			else
			{
				pm->gent->client->jetPackTime = Q3_INFINITE;
			}
		}
		else
		{
			TIMER_Set(pm->gent, "jetRecharge", 0);
			JET_FlyStart(pm->gent);
		}
		return qtrue;
	}
	return qfalse;
}

static void PM_FallToDeath()
{
	if (!pm->gent)
	{
		return;
	}

	if (PM_RocketeersAvoidDangerousFalls())
	{
		return;
	}

	// If this is a vehicle, more precisely an animal...
	if (pm->gent->client->NPC_class == CLASS_VEHICLE && pm->gent->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
	{
		Vehicle_t* p_veh = pm->gent->m_pVehicle;
		p_veh->m_pVehicleInfo->EjectAll(p_veh);
	}
	else
	{
		if (PM_HasAnimation(pm->gent, BOTH_FALLDEATH1))
		{
			PM_SetAnim(pm, SETANIM_LEGS, BOTH_FALLDEATH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
		else
		{
			PM_SetAnim(pm, SETANIM_LEGS, BOTH_DEATH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
		G_SoundOnEnt(pm->gent, CHAN_VOICE, "*falling1.wav"); //CHAN_VOICE_ATTEN
	}

	if (pm->gent->NPC)
	{
		pm->gent->NPC->aiFlags |= NPCAI_DIE_ON_IMPACT;
		pm->gent->NPC->nextBStateThink = Q3_INFINITE;
	}
	pm->ps->friction = 1;
}

static int PM_ForceJumpAnimForJumpAnim(int anim)
{
	switch (anim)
	{
	case BOTH_JUMP1: //# Jump - wind-up and leave ground
		anim = BOTH_FORCEJUMP1; //# Jump - wind-up and leave ground
		break;
	case BOTH_INAIR1: //# In air loop (from jump)
		anim = BOTH_FORCEINAIR1; //# In air loop (from jump)
		break;
	case BOTH_LAND1: //# Landing (from in air loop)
		anim = BOTH_FORCELAND1; //# Landing (from in air loop)
		break;
	case BOTH_JUMPBACK1: //# Jump backwards - wind-up and leave ground
		anim = BOTH_FORCEJUMPBACK1; //# Jump backwards - wind-up and leave ground
		break;
	case BOTH_INAIRBACK1: //# In air loop (from jump back)
		anim = BOTH_FORCEINAIRBACK1; //# In air loop (from jump back)
		break;
	case BOTH_LANDBACK1: //# Landing backwards(from in air loop)
		anim = BOTH_FORCELANDBACK1; //# Landing backwards(from in air loop)
		break;
	case BOTH_JUMPLEFT1: //# Jump left - wind-up and leave ground
		anim = BOTH_FORCEJUMPLEFT1; //# Jump left - wind-up and leave ground
		break;
	case BOTH_INAIRLEFT1: //# In air loop (from jump left)
		anim = BOTH_FORCEINAIRLEFT1; //# In air loop (from jump left)
		break;
	case BOTH_LANDLEFT1: //# Landing left(from in air loop)
		anim = BOTH_FORCELANDLEFT1; //# Landing left(from in air loop)
		break;
	case BOTH_JUMPRIGHT1: //# Jump right - wind-up and leave ground
		anim = BOTH_FORCEJUMPRIGHT1; //# Jump right - wind-up and leave ground
		break;
	case BOTH_INAIRRIGHT1: //# In air loop (from jump right)
		anim = BOTH_FORCEINAIRRIGHT1; //# In air loop (from jump right)
		break;
	case BOTH_LANDRIGHT1: //# Landing right(from in air loop)
		anim = BOTH_FORCELANDRIGHT1; //# Landing right(from in air loop)
		break;
	default:;
	}
	return anim;
}

static void PM_SetVehicleAngles(vec3_t normal)
{
	Vehicle_t* p_veh = pm->gent->m_pVehicle;
	float pitch_bias;

	if (!pm->gent->client || pm->gent->client->NPC_class != CLASS_VEHICLE)
	{
		return;
	}

	float vehicle_banking_speed = p_veh->m_pVehicleInfo->bankingSpeed; //0.25f
	vec3_t v_angles;

	if (vehicle_banking_speed <= 0
		|| p_veh->m_pVehicleInfo->pitchLimit <= 0 && p_veh->m_pVehicleInfo->rollLimit <= 0)
	{
		//don't bother, this vehicle doesn't bank
		return;
	}

	if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
	{
		pitch_bias = 0.0f;
	}
	else
	{
		pitch_bias = 90.0f * p_veh->m_pVehicleInfo->centerOfGravity[0];
	}

	VectorClear(v_angles);

	if (pm->waterlevel > 0)
	{
		//in water
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			v_angles[PITCH] += (pm->ps->viewangles[PITCH] - v_angles[PITCH]) * 0.75f + pitch_bias * 0.5;
		}
	}
	else if (normal)
	{
		//have a valid surface below me
		pitch_roll_for_slope(pm->gent, normal, v_angles);

		const float delta_pitch = v_angles[PITCH] - p_veh->m_vOrientation[PITCH];

		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			if (pml.groundTrace.contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
			{
				//on water
				v_angles[PITCH] += (pm->ps->viewangles[PITCH] - v_angles[PITCH]) * 0.5f + pitch_bias * 0.5f;
			}
			else
			{
				if (delta_pitch < -10.0f)
				{
					v_angles[PITCH] = p_veh->m_vOrientation[PITCH] - 10.0f;
				}
				else if (delta_pitch > 10.0f)
				{
					v_angles[PITCH] = p_veh->m_vOrientation[PITCH] + 10.0f;
				}
			}
		}
		else
		{
			if (delta_pitch < -10.0f)
			{
				v_angles[PITCH] = p_veh->m_vOrientation[PITCH] - 10.0f;
			}
			else if (delta_pitch > 10.0f)
			{
				v_angles[PITCH] = p_veh->m_vOrientation[PITCH] + 10.0f;
			}
		}
	}
	else
	{
		//in air, let pitch match view...?
		v_angles[PITCH] = p_veh->m_vOrientation[PITCH] - 1;
		if (v_angles[PITCH] < -15)
		{
			v_angles[PITCH] = -15;
		}
		//don't bank so fast when in the air

		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			vehicle_banking_speed *= 0.125f * pml.frametime;
		}
		else
		{
			vehicle_banking_speed *= 0.125f;
		}
	}

	if (p_veh->m_ulFlags & VEH_SPINNING)
	{
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
		}
		else
		{
			v_angles[ROLL] = p_veh->m_vOrientation[ROLL] - 25;
		}
	}
	else if (p_veh->m_ulFlags & VEH_OUTOFCONTROL)
	{
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
		}
		else
		{
			v_angles[ROLL] = p_veh->m_vOrientation[ROLL] + 5;
		}
	}
	else if (p_veh->m_pVehicleInfo->rollLimit > 0)
	{
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			//roll when banking
			vec3_t velocity;
			VectorCopy(pm->ps->velocity, velocity);
			velocity[2] = 0.0f;
			float speed = VectorNormalize(velocity);

			if (speed > 32.0f || speed < -32.0f)
			{
				vec3_t rt, tempVAngles;
				speed *= sin((150 + pml.frametime) * 0.003);

				// Clamp to prevent harsh rolling
				if (speed > 60)
				{
					speed = 60;
				}

				VectorCopy(p_veh->m_vOrientation, tempVAngles);
				tempVAngles[ROLL] = 0;
				AngleVectors(tempVAngles, nullptr, rt, nullptr);
				const float dp = DotProduct(velocity, rt);
				const float side = speed * dp;
				v_angles[ROLL] -= side;
			}
		}
		else
		{
			//roll when banking
			vec3_t velocity;
			VectorCopy(pm->ps->velocity, velocity);
			float speed = VectorNormalize(velocity);

			if (speed > 0.01f)
			{
				vec3_t rt, temp_v_angles;

				VectorCopy(p_veh->m_vOrientation, temp_v_angles);
				temp_v_angles[ROLL] = 0;
				AngleVectors(temp_v_angles, nullptr, rt, nullptr);
				{
					const float dotp = DotProduct(velocity, rt);
					speed *= dotp;
					if (level.time < p_veh->m_iTurboTime)
					{
						speed /= p_veh->m_pVehicleInfo->turboSpeed;
					}
					else
					{
						speed /= p_veh->m_pVehicleInfo->speedMax;
					}
					if (p_veh->m_ulFlags & VEH_SLIDEBREAKING)
					{
						speed *= 3.0f;
					}

					{
						const float side = speed * 75.0f;
						v_angles[ROLL] -= side;
					}
				}
				if (fabsf(v_angles[ROLL]) < 0.001f)
				{
					v_angles[ROLL] = 0.0f;
				}
			}
		}
	}

	//cap
	if (v_angles[PITCH] > p_veh->m_pVehicleInfo->pitchLimit)
	{
		v_angles[PITCH] = p_veh->m_pVehicleInfo->pitchLimit;
	}
	else if (v_angles[PITCH] < -p_veh->m_pVehicleInfo->pitchLimit)
	{
		v_angles[PITCH] = -p_veh->m_pVehicleInfo->pitchLimit;
	}

	if (!(p_veh->m_ulFlags & VEH_SPINNING))
	{
		if (v_angles[ROLL] > p_veh->m_pVehicleInfo->rollLimit)
		{
			v_angles[ROLL] = p_veh->m_pVehicleInfo->rollLimit;
		}
		else if (v_angles[ROLL] < -p_veh->m_pVehicleInfo->rollLimit)
		{
			v_angles[ROLL] = -p_veh->m_pVehicleInfo->rollLimit;
		}
	}

	//do it
	for (int i = 0; i < 3; i++)
	{
		if (i == YAW)
		{
			//yawing done elsewhere
			continue;
		}

		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			if (p_veh->m_vOrientation[i] >= v_angles[i] + vehicle_banking_speed)
			{
				p_veh->m_vOrientation[i] -= vehicle_banking_speed;
			}
			else if (p_veh->m_vOrientation[i] <= v_angles[i] - vehicle_banking_speed)
			{
				p_veh->m_vOrientation[i] += vehicle_banking_speed;
			}
			else
			{
				p_veh->m_vOrientation[i] = v_angles[i];
			}
		}
		else
		{
			if (i == ROLL && p_veh->m_ulFlags & VEH_STRAFERAM)
			{
				//during strafe ram, roll is done elsewhere
				continue;
			}
			{
				p_veh->m_vOrientation[i] = v_angles[i];
			}
		}
	}
}

void BG_VehicleTurnRateForSpeed(const Vehicle_t* p_veh, const float speed, float* m_pitch_override,
	float* m_yaw_override)
{
	if (p_veh && p_veh->m_pVehicleInfo)
	{
		float speed_frac = 1.0f;

		if (p_veh->m_pVehicleInfo->speedDependantTurning)
		{
			if (p_veh->m_LandTrace.fraction >= 1.0f || p_veh->m_LandTrace.plane.normal[2] < MIN_LANDING_SLOPE)
			{
				speed_frac = speed / (p_veh->m_pVehicleInfo->speedMax * 0.75f);
				if (speed_frac < 0.25f)
				{
					speed_frac = 0.25f;
				}
				else if (speed_frac > 1.0f)
				{
					speed_frac = 1.0f;
				}
			}
		}
		if (p_veh->m_pVehicleInfo->mousePitch)
		{
			*m_pitch_override = p_veh->m_pVehicleInfo->mousePitch * speed_frac;
		}
		if (p_veh->m_pVehicleInfo->mouseYaw)
		{
			*m_yaw_override = p_veh->m_pVehicleInfo->mouseYaw * speed_frac;
		}
	}
}

/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed()
{
	trace_t trace;
	qboolean cliff_fall = qfalse;

	if (Flying != FLY_HOVER)
	{
		if (!(pm->ps->eFlags & EF_FORCE_DRAINED))
		{
			vec3_t point;
			//FIXME: if in a contents_falldeath brush, play the falling death anim and sound?
			if (pm->ps->client_num != 0 && pm->gent && pm->gent->NPC && pm->gent->client && pm->gent->client->NPC_class
				!= CLASS_SITHLORD
				&& pm->gent->client->NPC_class != CLASS_DESANN && pm->gent->client->NPC_class != CLASS_VADER)
				//desann never falls to his death
			{
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
				{
					if (pm->ps->stats[STAT_HEALTH] > 0
						&& !(pm->gent->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
						&& !(pm->gent->NPC->aiFlags & NPCAI_JUMP) // doing a path jump
						&& !(pm->gent->NPC->scriptFlags & SCF_NO_FALLTODEATH)
						&& pm->gent->NPC->behaviorState != BS_JUMP) //not being scripted to jump
					{
						if (level.time - pm->gent->client->respawnTime > 2000
							//been in the world for at least 2 seconds
							&& (!pm->gent->NPC->timeOfDeath || level.time - pm->gent->NPC->timeOfDeath < 1000) && pm->
							gent->e_ThinkFunc != thinkF_NPC_RemoveBody
							//Have to do this now because timeOfDeath is used by thinkF_NPC_RemoveBody to debounce removal checks
							&& !(pm->gent->client->ps.forcePowersActive & 1 << FP_LEVITATION))
						{
							if (!FlyingCreature(pm->gent)
								&& g_gravity->value > 0)
							{
								if (!(pm->gent->flags & FL_UNDYING)
									&& !(pm->gent->flags & FL_GODMODE))
								{
									if (!(pm->ps->eFlags & EF_FORCE_GRIPPED)
										&& !(pm->ps->eFlags & EF_FORCE_DRAINED)
										&& !(pm->ps->eFlags & EF_FORCE_GRASPED)
										&& !(pm->ps->pm_flags & PMF_TRIGGER_PUSHED))
									{
										if (!pm->ps->forceJumpZStart || pm->ps->forceJumpZStart > pm->ps->origin[2])
											// && fabs(pm->ps->velocity[0])<10 && fabs(pm->ps->velocity[1])<10 && pm->ps->velocity[2]<0)//either not force-jumping or force-jumped and now fell below original jump start height
										{
											vec3_t vel;

											VectorCopy(pm->ps->velocity, vel);
											float speed = VectorLength(vel);
											if (!speed)
											{
												//damn divide by zero
												speed = 1;
											}
											float time = 400 / speed;
											vel[2] -= 0.5 * time * pm->ps->gravity;
											speed = VectorLength(vel);
											if (!speed)
											{
												//damn divide by zero
												speed = 1;
											}
											time = 400 / speed;
											VectorScale(vel, time, vel);
											VectorAdd(pm->ps->origin, vel, point);

											pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point,
												pm->ps->client_num, pm->tracemask, static_cast<EG2_Collision>(0),
												0);

											if (trace.contents & CONTENTS_LAVA
												&& PM_RocketeersAvoidDangerousFalls())
											{
												//got out of it
											}
											else if (!trace.allsolid && !trace.startsolid && pm->ps->origin[2] - trace.
												endpos[2] >= 128) //>=128 so we don't die on steps!
											{
												if (trace.fraction == 1.0)
												{
													//didn't hit, we're probably going to die
													if (pm->ps->velocity[2] < 0 && pm->ps->origin[2] - point[2] > 256)
													{
														//going down, into a bottomless pit, apparently
														PM_FallToDeath();
														cliff_fall = qtrue;
													}
												}
												else if (trace.entityNum < ENTITYNUM_NONE
													&& pm->ps->weapon != WP_SABER
													&& (!pm->gent || !pm->gent->client || pm->gent->client->NPC_class
														!= CLASS_BOBAFETT && pm->gent->client->NPC_class !=
														CLASS_MANDALORIAN && pm->gent->client->NPC_class != CLASS_JANGO
														&& pm->gent->client->NPC_class != CLASS_JANGODUAL && pm->gent->
														client->NPC_class != CLASS_REBORN && pm->gent->client->NPC_class
														!= CLASS_ROCKETTROOPER))
												{
													//Jedi don't scream and die if they're heading for a hard impact
													const gentity_t* traceEnt = &g_entities[trace.entityNum];
													if (trace.entityNum == ENTITYNUM_WORLD || traceEnt && traceEnt->
														bmodel)
													{
														//hit architecture, find impact force
														float dmg;

														VectorCopy(pm->ps->velocity, vel);
														time = Distance(trace.endpos, pm->ps->origin) / VectorLength(
															vel);
														vel[2] -= 0.5 * time * pm->ps->gravity;

														if (trace.plane.normal[2] > 0.5)
														{
															//use falling damage
															int waterlevel, junk;
															PM_SetWaterLevelAtPoint(trace.endpos, &waterlevel, &junk);
															dmg = PM_CrashLandDelta(vel);
															if (dmg >= 30)
															{
																//there is a minimum fall threshhold
																dmg = PM_DamageForDelta(dmg);
															}
															else
															{
																dmg = 0;
															}
														}
														else
														{
															//use impact damage
															//guestimate
															if (pm->gent->client && pm->gent->client->ps.
																forceJumpZStart)
															{
																//we were force-jumping
																if (pm->gent->currentOrigin[2] >= pm->gent->client->ps.
																	forceJumpZStart)
																{
																	//we landed at same height or higher than we landed
																	dmg = 0;
																}
																else
																{
																	//FIXME: take off some of it, at least?
																	dmg = (pm->gent->client->ps.forceJumpZStart - pm->
																		gent->currentOrigin[2]) / 3;
																}
															}
															dmg = 10 * VectorLength(pm->ps->velocity);
															if (pm->ps->client_num < MAX_CLIENTS)
															{
																dmg /= 2;
															}
															dmg *= 0.01875f; //magic number
														}
														float max_dmg = pm->ps->stats[STAT_HEALTH] > 20
															? pm->ps->stats[STAT_HEALTH]
															: 20;

														//a fall that would do less than 20 points of damage should never make us scream to our deaths
														if (pm->gent && pm->gent->client && pm->gent->client->NPC_class
															== CLASS_HOWLER)
														{
															//howlers can survive long falls, despire their health
															max_dmg *= 5;
														}
														if (dmg >= pm->ps->stats[STAT_HEALTH]) //armor?
														{
															PM_FallToDeath();
															cliff_fall = qtrue;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			if (!cliff_fall)
			{
				if (pm->ps->legsAnim == BOTH_FORCELONGLEAP_START
					|| pm->ps->legsAnim == BOTH_FORCELONGLEAP_ATTACK
					|| pm->ps->legsAnim == BOTH_FORCEWALLRUNFLIP_START
					|| pm->ps->legsAnim == BOTH_FLIP_LAND)
				{
					//let it stay on this anim
				}
				else if (PM_KnockDownAnimExtended(pm->ps->legsAnim))
				{
					//no in-air anims when in knockdown anim
				}
				else if ((pm->ps->legsAnim == BOTH_WALL_RUN_RIGHT
					|| pm->ps->legsAnim == BOTH_WALL_RUN_LEFT
					|| pm->ps->legsAnim == BOTH_WALL_RUN_RIGHT_STOP
					|| pm->ps->legsAnim == BOTH_WALL_RUN_LEFT_STOP
					|| pm->ps->legsAnim == BOTH_WALL_RUN_RIGHT_FLIP
					|| pm->ps->legsAnim == BOTH_WALL_RUN_LEFT_FLIP
					|| pm->ps->legsAnim == BOTH_WALL_FLIP_RIGHT
					|| pm->ps->legsAnim == BOTH_WALL_FLIP_LEFT
					|| pm->ps->legsAnim == BOTH_FORCEWALLREBOUND_FORWARD
					|| pm->ps->legsAnim == BOTH_FORCEWALLREBOUND_LEFT
					|| pm->ps->legsAnim == BOTH_FORCEWALLREBOUND_BACK
					|| pm->ps->legsAnim == BOTH_FORCEWALLREBOUND_RIGHT
					|| pm->ps->legsAnim == BOTH_CEILING_DROP)
					&& !pm->ps->legsAnimTimer)
				{
					//if flip anim is done, okay to use inair
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_FORCEINAIR1, SETANIM_FLAG_OVERRIDE, 350); // Only blend over 100ms
				}
				else if (pm->ps->legsAnim == BOTH_FLIP_ATTACK7
					|| pm->ps->legsAnim == BOTH_FLIP_HOLD7)
				{
					if (!pm->ps->legsAnimTimer)
					{
						//done?
						PM_SetAnim(pm, SETANIM_BOTH, BOTH_FLIP_HOLD7, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 350);
						// Only blend over 100ms
					}
				}
				else if (PM_InCartwheel(pm->ps->legsAnim))
				{
					if (pm->ps->legsAnimTimer > 0)
					{
						//still playing on bottom
					}
					else
					{
						PM_SetAnim(pm, SETANIM_BOTH, BOTH_INAIR1, SETANIM_FLAG_NORMAL, 350);
					}
				}
				else if (PM_InAirKickingAnim(pm->ps->legsAnim))
				{
					if (pm->ps->legsAnimTimer > 0)
					{
						//still playing on bottom
					}
					else
					{
						PM_SetAnim(pm, SETANIM_BOTH, BOTH_INAIR1, SETANIM_FLAG_NORMAL, 350);
						pm->ps->saber_move = LS_READY;
						pm->ps->weaponTime = 0;
					}
				}
				else if (!PM_InRoll(pm->ps)
					&& !PM_SpinningAnim(pm->ps->legsAnim)
					&& !PM_FlippingAnim(pm->ps->legsAnim)
					&& !PM_InSpecialJump(pm->ps->legsAnim))
				{
					if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
					{
						// we just transitioned into freefall
						if (pm->debugLevel)
						{
							Com_Printf("%i:lift\n", c_pmove);
						}

						// if they aren't in a jumping animation and the ground is a ways away, force into it
						// if we didn't do the trace, the player would be backflipping down staircases
						VectorCopy(pm->ps->origin, point);
						point[2] -= 64;

						pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->client_num, pm->tracemask,
							static_cast<EG2_Collision>(0), 0);
						if (trace.fraction == 1.0)
						{
							//FIXME: if velocity[2] < 0 and didn't jump, use some falling anim
							if (pm->ps->velocity[2] <= 0 && !(pm->ps->pm_flags & PMF_JUMP_HELD))
							{
								if (!PM_InDeathAnim())
								{
									// If we're a vehicle, set our falling flag.
									if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
									{
										// We're flying in the air.
										if (pm->gent->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
										{
											pm->gent->m_pVehicle->m_ulFlags |= VEH_FLYING;
										}
									}
									else
									{
										vec3_t move_dir, look_angles, look_dir, look_right;
										int anim;

										VectorCopy(pm->ps->velocity, move_dir);
										move_dir[2] = 0;
										VectorNormalize(move_dir);

										VectorCopy(pm->ps->viewangles, look_angles);
										look_angles[PITCH] = look_angles[ROLL] = 0;
										AngleVectors(look_angles, look_dir, look_right, nullptr);

										float dot = DotProduct(move_dir, look_dir);
										if (dot > 0.5)
										{
											//redundant
											anim = BOTH_INAIR1;
										}
										else if (dot < -0.5)
										{
											anim = BOTH_INAIRBACK1;
										}
										else
										{
											dot = DotProduct(move_dir, look_right);
											if (dot > 0.5)
											{
												anim = BOTH_INAIRRIGHT1;
											}
											else if (dot < -0.5)
											{
												anim = BOTH_INAIRLEFT1;
											}
											else
											{
												//redundant
												anim = BOTH_INAIR1;
											}
										}
										if (pm->ps->forcePowersActive & 1 << FP_LEVITATION)
										{
											anim = PM_ForceJumpAnimForJumpAnim(anim);
										}
										PM_SetAnim(pm, SETANIM_LEGS, anim, SETANIM_FLAG_OVERRIDE, 100);
										// Only blend over 100ms
									}
								}
								pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
							}
							else if (!(pm->ps->forcePowersActive & 1 << FP_LEVITATION))
							{
								if (pm->cmd.forwardmove >= 0)
								{
									if (!PM_InDeathAnim())
									{
										PM_SetAnim(pm, SETANIM_LEGS, BOTH_JUMP1, SETANIM_FLAG_OVERRIDE, 100);
										// Only blend over 100ms
									}
									pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
								}
								else if (pm->cmd.forwardmove < 0)
								{
									if (!PM_InDeathAnim())
									{
										PM_SetAnim(pm, SETANIM_LEGS, BOTH_JUMPBACK1, SETANIM_FLAG_OVERRIDE, 100);
										// Only blend over 100ms
									}
									pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
								}
							}
						}
						else
						{
							// If we're a vehicle, set our falling flag.
							if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
							{
								// We're on the ground.
								if (pm->gent->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
								{
									pm->gent->m_pVehicle->m_ulFlags &= ~VEH_FLYING;
								}
							}
						}
					}
				}
			}
		}

		if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
		{
			pm->ps->jumpZStart = pm->ps->origin[2];
		}
	}
	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace()
{
	vec3_t point{};
	trace_t trace;

	if (pm->ps->eFlags & EF_LOCKED_TO_WEAPON
		|| pm->ps->eFlags & EF_HELD_BY_RANCOR
		|| pm->ps->eFlags & EF_HELD_BY_WAMPA
		|| pm->ps->eFlags & EF_HELD_BY_SAND_CREATURE
		|| PM_RidingVehicle())
	{
		pml.groundPlane = qtrue;
		pml.walking = qtrue;
		pm->ps->groundEntityNum = ENTITYNUM_WORLD;
		pm->ps->lastOnGround = level.time;
		return;
	}
	if (pm->ps->legsAnimTimer > 300
		&& (pm->ps->legsAnim == BOTH_WALL_RUN_RIGHT
			|| pm->ps->legsAnim == BOTH_WALL_RUN_LEFT
			|| pm->ps->legsAnim == BOTH_FORCEWALLRUNFLIP_START))
	{
		//wall-running forces you to be in the air
		pml.groundPlane = qfalse;
		pml.walking = qfalse;
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		return;
	}

	auto minNormal = static_cast<float>(MIN_WALK_NORMAL);
	if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
	{
		//FIXME: extern this as part of vehicle data
		minNormal = pm->gent->m_pVehicle->m_pVehicleInfo->maxSlope;
	}

	point[0] = pm->ps->origin[0];
	point[1] = pm->ps->origin[1];
	point[2] = pm->ps->origin[2] - 0.25f;

	pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->client_num, pm->tracemask,
		static_cast<EG2_Collision>(0), 0);
	pml.groundTrace = trace;

	// do something corrective if the trace starts in a solid...
	if (trace.allsolid)
	{
		PM_CorrectAllSolid();
		return;
	}

	// if the trace didn't hit anything, we are in free fall
	if (trace.fraction == 1.0 || g_gravity->value <= 0)
	{
		PM_GroundTraceMissed();
		pml.groundPlane = qfalse;
		pml.walking = qfalse;
		return;
	}

	// Not a vehicle and not riding one.
	if (pm->gent
		&& pm->gent->client
		&& pm->gent->client->NPC_class != CLASS_SAND_CREATURE
		&& (pm->gent->client->NPC_class != CLASS_VEHICLE && !PM_RidingVehicle()))
	{
		// check if getting thrown off the ground
		if ((pm->ps->velocity[2] > 0 && pm->ps->pm_flags & PMF_TIME_KNOCKBACK || pm->ps->velocity[2] > 100) &&
			DotProduct(pm->ps->velocity, trace.plane.normal) > 10)
		{
			//either thrown off ground (PMF_TIME_KNOCKBACK) or going off the ground at a large velocity
			if (pm->debugLevel)
			{
				Com_Printf("%i:kickoff\n", c_pmove);
			}
			// go into jump animation
			if (PM_FlippingAnim(pm->ps->legsAnim))
			{
				//we're flipping
			}
			else if (PM_InSpecialJump(pm->ps->legsAnim))
			{
				//special jumps
			}
			else if (PM_InKnockDown(pm->ps))
			{
				//in knockdown
			}
			else if (PM_InRoll(pm->ps))
			{
				//in knockdown
			}
			else if (PM_KickingAnim(pm->ps->legsAnim))
			{
				//in kick
			}
			else if (pm->gent
				&& pm->gent->client
				&& (pm->gent->client->NPC_class == CLASS_RANCOR || pm->gent->client->NPC_class == CLASS_WAMPA))
			{
			}
			else
			{
				PM_JumpForDir();
			}

			pm->ps->groundEntityNum = ENTITYNUM_NONE;
			pml.groundPlane = qfalse;
			pml.walking = qfalse;
			return;
		}
	}

	// slopes that are too steep will not be considered onground
	if (trace.plane.normal[2] < minNormal)
	{
		if (pm->debugLevel)
		{
			Com_Printf("%i:steep\n", c_pmove);
		}
		// FIXME: if they can't slide down the slope, let them
		// walk (sharp crevices)
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane = qtrue;
		pml.walking = qfalse;
		return;
	}

	//FIXME: if the ground surface is a "cover surface (like tall grass), add a "cover" flag to me
	pml.groundPlane = qtrue;
	pml.walking = qtrue;

	Jetpack_Off(&g_entities[pm->ps->client_num]);

	// hitting solid ground will end a waterjump
	if (pm->ps->pm_flags & PMF_TIME_WATERJUMP)
	{
		pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
		pm->ps->pm_time = 0;
	}

	if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
	{
		// just hit the ground
		if (pm->debugLevel)
		{
			Com_Printf("%i:Land\n", c_pmove);
		}

		//if ( !PM_ClientImpact( trace.entityNum, qtrue ) )
		{
			PM_CrashLand();

			// don't do landing time if we were just going down a slope
			if (pml.previous_velocity[2] < -200)
			{
				// don't allow another jump for a little while
				pm->ps->pm_flags |= PMF_TIME_LAND;
				pm->ps->pm_time = 250;
			}
			if (!pm->cmd.forwardmove && !pm->cmd.rightmove)
			{
				if (Flying != FLY_HOVER)
				{
					pm->ps->velocity[2] = 0;
					//wouldn't normally want this because of slopes, but we aren't tyring to move...
				}
			}
		}
	}

	pm->ps->groundEntityNum = trace.entityNum;
	pm->ps->lastOnGround = level.time;
	if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		//if a player, clear the jumping "flag" so can't double-jump
		pm->ps->forceJumpCharge = 0;
	}

	// don't reset the z velocity for slopes
	//	pm->ps->velocity[2] = 0;

	PM_AddTouchEnt(trace.entityNum);
}

int LastMatrixJumpTime = 0;
constexpr auto DEBUGMATRIXJUMP = 0;

static void PM_HoverTrace()
{
	if (!pm->gent || !pm->gent->client || pm->gent->client->NPC_class != CLASS_VEHICLE)
	{
		return;
	}

	Vehicle_t* p_veh = pm->gent->m_pVehicle;
	const float hover_height = p_veh->m_pVehicleInfo->hoverHeight;
	vec3_t v_ang{}, fx_axis[3]{};
	trace_t* trace = &pml.groundTrace;
	int trace_contents = pm->tracemask;

	pml.groundPlane = qfalse;

	const float relativeWaterLevel = pm->ps->waterheight - (pm->ps->origin[2] + pm->mins[2]);
	if (pm->waterlevel && relativeWaterLevel >= 0)
	{
		//in water
		if (p_veh->m_pVehicleInfo->bouyancy <= 0.0f)
		{
			//sink like a rock
		}
		else
		{
			//rise up
			const float float_height = p_veh->m_pVehicleInfo->bouyancy * ((pm->maxs[2] - pm->mins[2]) * 0.5f) -
				hover_height * 0.5f; //1.0f should make you float half-in, half-out of water
			if (relativeWaterLevel > float_height)
			{
				//too low, should rise up
				pm->ps->velocity[2] += (relativeWaterLevel - float_height) * p_veh->m_fTimeModifier;
			}
		}
		if (pm->ps->waterheight < pm->ps->origin[2] + pm->maxs[2])
		{
			//part of us is sticking out of water
			if (fabs(pm->ps->velocity[0]) + fabs(pm->ps->velocity[1]) > 100)
			{
				//moving at a decent speed
				if (Q_irand(pml.frametime, 100) >= 50)
				{
					//splash
					v_ang[PITCH] = v_ang[ROLL] = 0;
					v_ang[YAW] = p_veh->m_vOrientation[YAW];
					AngleVectors(v_ang, fx_axis[2], fx_axis[1], fx_axis[0]);
					vec3_t wake_org;
					VectorCopy(pm->ps->origin, wake_org);
					wake_org[2] = pm->ps->waterheight;
					if (p_veh->m_pVehicleInfo->iWakeFX)
					{
						G_PlayEffect(p_veh->m_pVehicleInfo->iWakeFX, wake_org, fx_axis);
					}
				}
			}
			pml.groundPlane = qtrue;
		}
	}
	else
	{
		vec3_t point{};
		const float min_normal = p_veh->m_pVehicleInfo->maxSlope;

		point[0] = pm->ps->origin[0];
		point[1] = pm->ps->origin[1];
		point[2] = pm->ps->origin[2] - hover_height * 3.0f;

		//FIXME: check for water, too?  If over water, go slower and make wave effect
		//		If *in* water, go really slow and use bouyancy stat to determine how far below surface to float

		//NOTE: if bouyancy is 2.0f or higher, you float over water like it's solid ground.
		//		if it's 1.0f, you sink halfway into water.  If it's 0, you sink...
		if (p_veh->m_pVehicleInfo->bouyancy >= 2.0f)
		{
			//sit on water
			trace_contents |= CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA;
		}
		pm->trace(trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->client_num, trace_contents,
			static_cast<EG2_Collision>(0), 0);
		if (trace->plane.normal[2] >= min_normal)
		{
			//not a steep slope, so push us up
			if (trace->fraction < 0.3f)
			{
				//push up off ground
				const float hover_force = p_veh->m_pVehicleInfo->hoverStrength;
				pm->ps->velocity[2] += (0.3f - trace->fraction) * hover_force * p_veh->m_fTimeModifier;

				//	if (pm->ps->velocity[2]>60.0f)
				//	{
				//		pm->ps->velocity[2] = 60.0f;
				//	}

				if (trace->contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
				{
					//hovering on water, make a spash if moving
					if (fabs(pm->ps->velocity[0]) + fabs(pm->ps->velocity[1]) > 100)
					{
						//moving at a decent speed
						if (Q_irand(pml.frametime, 100) >= 50)
						{
							//splash
							v_ang[PITCH] = v_ang[ROLL] = 0;
							v_ang[YAW] = p_veh->m_vOrientation[YAW];
							AngleVectors(v_ang, fx_axis[2], fx_axis[1], fx_axis[0]);
							if (p_veh->m_pVehicleInfo->iWakeFX)
							{
								G_PlayEffect(p_veh->m_pVehicleInfo->iWakeFX, trace->endpos, fx_axis);
							}
						}
					}
				}

				if (p_veh->m_ulFlags & VEH_SLIDEBREAKING)
				{
					if (Q_irand(pml.frametime, 100) >= 50)
					{
						//dust
						VectorClear(fx_axis[0]);
						fx_axis[0][2] = 1;

						VectorCopy(pm->ps->velocity, fx_axis[1]);
						fx_axis[1][2] *= 0.01f;
						VectorMA(pm->ps->origin, 0.25f, fx_axis[1], point);
						G_PlayEffect("ships/swoop_dust", point, fx_axis[0]);
					}
				}
				pml.groundPlane = qtrue;
			}
		}
	}
	if (pml.groundPlane)
	{
		PM_SetVehicleAngles(pml.groundTrace.plane.normal);
		// We're on the ground.
		//		if (p_veh->m_ulFlags&VEH_FLYING && level.time<p_veh->m_iTurboTime)
		//		{
		//			p_veh->m_iTurboTime = 0;	// stop turbo
		//		}
		p_veh->m_ulFlags &= ~VEH_FLYING;
		p_veh->m_vAngularVelocity = 0.0f;
	}
	else
	{
		PM_SetVehicleAngles(nullptr);
		// We're flying in the air.
		p_veh->m_ulFlags |= VEH_FLYING;
		//groundTrace

		if (p_veh->m_vAngularVelocity == 0.0f)
		{
			p_veh->m_vAngularVelocity = p_veh->m_vOrientation[YAW] - p_veh->m_vPrevOrientation[YAW];
			if (p_veh->m_vAngularVelocity < -15.0f)
			{
				p_veh->m_vAngularVelocity = -15.0f;
			}
			if (p_veh->m_vAngularVelocity > 15.0f)
			{
				p_veh->m_vAngularVelocity = 15.0f;
			}

			// BEGIN MATRIX MODE INIT FOR JUMP
			//=================================
			if (pm->gent &&
				pm->gent->owner &&
				(pm->gent->owner->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent->owner)) &&
				p_veh->m_pVehicleInfo->type == VH_SPEEDER &&
				level.time > LastMatrixJumpTime + 5000 && VectorLength(pm->ps->velocity) > 30.0f)
			{
				LastMatrixJumpTime = level.time;
				vec3_t predicted_apx;
				vec3_t predicted_fall_velocity;
				vec3_t predicted_land_position;

				VectorScale(pm->ps->velocity, 2.0f, predicted_fall_velocity); // take friction into account
				predicted_fall_velocity[2] = -(pm->ps->gravity * 1.1f); // take gravity into account

				VectorMA(pm->ps->origin, 0.25f, pm->ps->velocity, predicted_apx);
				VectorMA(predicted_apx, 0.25f, predicted_fall_velocity, predicted_land_position);

				trace_t trace2;
				gi.trace(&trace2, predicted_apx, pm->mins, pm->maxs, predicted_land_position, pm->ps->client_num,
					trace_contents, static_cast<EG2_Collision>(0), 0);
				if (!trace2.startsolid && !trace2.allsolid && trace2.fraction > 0.75 && Q_irand(0, 3) == 0)
				{
					LastMatrixJumpTime += 20000;
					G_StartMatrixEffect(pm->gent, MEF_HIT_GROUND_STOP);
				}
			}
			//=================================
		}
		p_veh->m_vAngularVelocity *= 0.95f; // Angular Velocity Decays Over Time
	}
	PM_GroundTraceMissed();
}

/*
=============
PM_SetWaterLevelAtPoint	FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevelAtPoint(vec3_t org, int* waterlevel, int* watertype)
{
	vec3_t point{};

	//
	// get waterlevel, accounting for ducking
	//
	*waterlevel = 0;
	*watertype = 0;

	point[0] = org[0];
	point[1] = org[1];
	point[2] = org[2] + DEFAULT_MINS_2 + 1;
	if (gi.totalMapContents() & (MASK_WATER | CONTENTS_LADDER))
	{
		int cont = pm->pointcontents(point, pm->ps->client_num);

		if (cont & (MASK_WATER | CONTENTS_LADDER))
		{
			const int sample2 = pm->ps->viewheight - DEFAULT_MINS_2;
			const int sample1 = sample2 / 2;

			*watertype = cont;
			*waterlevel = 1;
			point[2] = org[2] + DEFAULT_MINS_2 + sample1;
			cont = pm->pointcontents(point, pm->ps->client_num);
			if (cont & (MASK_WATER | CONTENTS_LADDER))
			{
				*waterlevel = 2;
				point[2] = org[2] + DEFAULT_MINS_2 + sample2;
				cont = pm->pointcontents(point, pm->ps->client_num);
				if (cont & (MASK_WATER | CONTENTS_LADDER))
				{
					*waterlevel = 3;
				}
			}
		}
	}
}

static void PM_SetWaterHeight()
{
	pm->ps->waterHeightLevel = WHL_NONE;
	if (pm->waterlevel < 1)
	{
		pm->ps->waterheight = pm->ps->origin[2] + DEFAULT_MINS_2 - 4;
		return;
	}
	trace_t trace;
	vec3_t top, bottom;

	VectorCopy(pm->ps->origin, top);
	VectorCopy(pm->ps->origin, bottom);
	top[2] += pm->gent->client->standheight;
	bottom[2] += DEFAULT_MINS_2;

	gi.trace(&trace, top, pm->mins, pm->maxs, bottom, pm->ps->client_num, MASK_WATER, static_cast<EG2_Collision>(0), 0);

	if (trace.startsolid)
	{
		//under water
		pm->ps->waterheight = top[2] + 4;
	}
	else if (trace.fraction < 1.0f)
	{
		//partially in and partially out of water
		pm->ps->waterheight = trace.endpos[2] + pm->mins[2];
	}
	else if (trace.contents & MASK_WATER)
	{
		//water is above me
		pm->ps->waterheight = top[2] + 4;
	}
	else
	{
		//water is below me
		pm->ps->waterheight = bottom[2] - 4;
	}
	const float dist_from_eyes = pm->ps->origin[2] + pm->gent->client->standheight - pm->ps->waterheight;

	if (dist_from_eyes < 0)
	{
		pm->ps->waterHeightLevel = WHL_UNDER;
	}
	else if (dist_from_eyes < 6)
	{
		pm->ps->waterHeightLevel = WHL_HEAD;
	}
	else if (dist_from_eyes < 18)
	{
		pm->ps->waterHeightLevel = WHL_SHOULDERS;
	}
	else if (dist_from_eyes < pm->gent->client->standheight - 8)
	{
		//at least 8 above origin
		pm->ps->waterHeightLevel = WHL_TORSO;
	}
	else
	{
		const float dist_from_org = pm->ps->origin[2] - pm->ps->waterheight;
		if (dist_from_org < 6)
		{
			pm->ps->waterHeightLevel = WHL_WAIST;
		}
		else if (dist_from_org < 16)
		{
			pm->ps->waterHeightLevel = WHL_KNEES;
		}
		else if (dist_from_org > fabs(pm->mins[2]))
		{
			pm->ps->waterHeightLevel = WHL_NONE;
		}
		else
		{
			pm->ps->waterHeightLevel = WHL_ANKLES;
		}
	}
}

/*
==============
PM_SetBounds

Sets mins, maxs
==============
*/
static void PM_SetBounds()
{
	if (pm->gent && pm->gent->client)
	{
		if (!pm->gent->mins[0] || !pm->gent->mins[1] || !pm->gent->mins[2] || !pm->gent->maxs[0] || !pm->gent->maxs[1]
			|| !pm->gent->maxs[2])
		{
			//assert(0);
		}

		VectorCopy(pm->gent->mins, pm->mins);
		VectorCopy(pm->gent->maxs, pm->maxs);
	}
	else
	{
		if (!DEFAULT_MINS_0 || !DEFAULT_MINS_1 || !DEFAULT_MAXS_0 || !DEFAULT_MAXS_1)
		{
			assert(0);
		}

		pm->mins[0] = DEFAULT_MINS_0;
		pm->mins[1] = DEFAULT_MINS_1;

		pm->maxs[0] = DEFAULT_MAXS_0;
		pm->maxs[1] = DEFAULT_MAXS_1;

		pm->mins[2] = DEFAULT_MINS_2;
		pm->maxs[2] = DEFAULT_MAXS_2;
	}
}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps->viewheight
==============
*/
static void PM_CheckDuck()
{
	trace_t trace;
	int standheight;
	int crouchheight;

	if (pm->gent && pm->gent->client)
	{
		if (!pm->gent->mins[0] || !pm->gent->mins[1] || !pm->gent->mins[2] || !pm->gent->maxs[0] || !pm->gent->maxs[1]
			|| !pm->gent->maxs[2])
		{
			//assert(0);
		}

		if (pm->ps->client_num < MAX_CLIENTS
			&& (pm->gent->client->NPC_class == CLASS_ATST || pm->gent->client->NPC_class == CLASS_RANCOR || pm->gent->client->NPC_class == CLASS_DROIDEKA)
			&& !BG_AllowThirdPersonSpecialMove(pm->ps))
		{
			standheight = crouchheight = 128;
		}
		else
		{
			standheight = pm->gent->client->standheight;
			crouchheight = pm->gent->client->crouchheight;
		}
	}
	else
	{
		if (!DEFAULT_MINS_0 || !DEFAULT_MINS_1 || !DEFAULT_MAXS_0 || !DEFAULT_MAXS_1)
		{
			assert(0);
		}

		standheight = DEFAULT_MAXS_2;
		crouchheight = CROUCH_MAXS_2;
	}

	if (PM_RidingVehicle() || pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
	{
		//riding a vehicle or are a vehicle
		//no ducking or rolling when on a vehicle
		//right?  not even on ones that you just ride on top of?
		pm->ps->pm_flags &= ~PMF_DUCKED;
		pm->ps->eFlags &= ~EF_MEDITATING;
		pm->maxs[2] = standheight;
		//FIXME: have a crouchviewheight and standviewheight on ent?
		pm->ps->viewheight = standheight + STANDARD_VIEWHEIGHT_OFFSET; //DEFAULT_VIEWHEIGHT;
		//NOTE: we don't clear the pm->cmd.upmove here because
		//the vehicle code may need it later... but, for riders,
		//it should have already been copied over to the vehicle, right?
		return;
	}

	if (PM_InGetUp(pm->ps))
	{
		//can't do any kind of crouching when getting up
		if (pm->ps->legsAnim == BOTH_GETUP_CROUCH_B1 || pm->ps->legsAnim == BOTH_GETUP_CROUCH_F1)
		{
			//crouched still
			pm->ps->pm_flags |= PMF_DUCKED;
			pm->maxs[2] = crouchheight;
		}
		pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
		return;
	}

	const int old_height = pm->maxs[2];

	if (PM_InRoll(pm->ps))
	{
		pm->maxs[2] = crouchheight;
		pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
		pm->ps->pm_flags |= PMF_DUCKED;
		return;
	}
	if (PM_GettingUpFromKnockDown(standheight, crouchheight))
	{
		pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
		return;
	}
	if (PM_InKnockDown(pm->ps))
	{
		//forced crouch
		if (pm->gent && pm->gent->client)
		{
			//interrupted any potential delayed weapon fires
			pm->gent->client->fireDelay = 0;
		}
		pm->maxs[2] = crouchheight;
		pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
		pm->ps->pm_flags |= PMF_DUCKED;
		return;
	}
	if (pm->cmd.upmove < 0)
	{
		// trying to duck
		pm->maxs[2] = crouchheight;
		pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET; //CROUCH_VIEWHEIGHT;
		if (pm->ps->groundEntityNum == ENTITYNUM_NONE && !PM_SwimmingAnim(pm->ps->legsAnim))
		{
			//Not ducked already and trying to duck in mid-air
			//will raise your feet, unducking whilst in air will drop feet
			if (!(pm->ps->pm_flags & PMF_DUCKED))
			{
				pm->ps->eFlags ^= EF_TELEPORT_BIT;
			}
			if (pm->gent)
			{
				pm->ps->origin[2] += old_height - pm->maxs[2]; //diff will be zero if were already ducking
				//Don't worry, we know we fit in a smaller size
			}
		}
		pm->ps->pm_flags |= PMF_DUCKED;
		if (d_JediAI->integer || d_combatinfo->integer || g_DebugSaberCombat->integer)
		{
			if (pm->ps->client_num && pm->ps->weapon == WP_SABER)
			{
				Com_Printf("ducking\n");
			}
		}
	}
	else
	{
		// want to stop ducking, stand up if possible
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			//Was ducking
			if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
			{
				//unducking whilst in air will try to drop feet
				pm->maxs[2] = standheight;
				pm->ps->origin[2] += old_height - pm->maxs[2];
				pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->client_num, pm->tracemask,
					static_cast<EG2_Collision>(0), 0);
				if (!trace.allsolid)
				{
					pm->ps->eFlags ^= EF_TELEPORT_BIT;
					pm->ps->pm_flags &= ~PMF_DUCKED;
				}
				else
				{
					//Put us back
					pm->ps->origin[2] -= old_height - pm->maxs[2];
				}
				//NOTE: this isn't the best way to check this, you may have room to unduck
				//while in air, but your feet are close to landing.  Probably won't be a
				//noticable shortcoming
			}
			else
			{
				// try to stand up
				pm->maxs[2] = standheight;
				pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->client_num, pm->tracemask,
					static_cast<EG2_Collision>(0), 0);
				if (!trace.allsolid)
				{
					pm->ps->pm_flags &= ~PMF_DUCKED;
				}
			}
		}

		if (pm->ps->eFlags & EF_MEDITATING)
		{
			pm->maxs[2] = crouchheight;
			pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET; //CROUCH_VIEWHEIGHT;
		}
		else if (pm->ps->pm_flags & PMF_DUCKED)
		{
			//Still ducking
			pm->maxs[2] = crouchheight;
			pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET; //CROUCH_VIEWHEIGHT;
		}
		else
		{
			//standing now
			pm->maxs[2] = standheight;
			//FIXME: have a crouchviewheight and standviewheight on ent?
			pm->ps->viewheight = standheight + STANDARD_VIEWHEIGHT_OFFSET; //DEFAULT_VIEWHEIGHT;
		}
	}
}

//===================================================================
static qboolean PM_SaberLockAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BF2LOCK: //#
	case BOTH_BF1LOCK: //#
	case BOTH_CWCIRCLELOCK: //#
	case BOTH_CCWCIRCLELOCK: //#
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_ForceAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_CHOKE1: //being choked...???
	case BOTH_GESTURE1: //taunting...
	case BOTH_RESISTPUSH: //# plant yourself to resist force push/pulls.
	case BOTH_FORCEPUSH: //# Use off-hand to do force power.
	case BOTH_DUAL_PUSH: //# Use off-hand to do force power.
	case BOTH_FORCEPULL: //# Use off-hand to do force power.
	case BOTH_MINDTRICK1: //# Use off-hand to do mind trick
	case BOTH_MINDTRICK2: //# Use off-hand to do distraction
	case BOTH_FORCELIGHTNING: //# Use off-hand to do lightning
	case BOTH_FORCELIGHTNING_START:
	case BOTH_FORCELIGHTNING_HOLD: //# Use off-hand to do lightning
	case BOTH_FORCELIGHTNING_RELEASE: //# Use off-hand to do lightning
	case BOTH_FORCEHEAL_START: //# Healing meditation pose start
	case BOTH_FORCEHEAL_STOP: //# Healing meditation pose end
	case BOTH_FORCEHEAL_QUICK: //# Healing meditation gesture
	case BOTH_FORCEGRIP1: //# temp force-grip anim (actually re-using push)
	case BOTH_FORCEGRIP_HOLD: //# temp force-grip anim (actually re-using push)
	case BOTH_FORCEGRIP_RELEASE: //# temp force-grip anim (actually re-using push)
		//case BOTH_FORCEGRIP3:		//# force-gripping
	case BOTH_FORCE_RAGE:
	case BOTH_FORCE_2HANDEDLIGHTNING:
	case BOTH_FORCE_2HANDEDLIGHTNING_START:
	case BOTH_FORCE_2HANDEDLIGHTNING_HOLD:
	case BOTH_FORCE_2HANDEDLIGHTNING_RELEASE:
	case BOTH_FORCE_DRAIN:
	case BOTH_FORCE_DRAIN_START:
	case BOTH_FORCE_DRAIN_HOLD:
	case BOTH_FORCE_DRAIN_RELEASE:
	case BOTH_FORCE_ABSORB:
	case BOTH_FORCE_ABSORB_START:
	case BOTH_FORCE_ABSORB_END:
	case BOTH_FORCE_PROTECT:
	case BOTH_FORCE_PROTECT_FAST:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InSaberAnim(const int anim)
{
	if (anim >= BOTH_A1_T__B_ && anim <= BOTH_H1_S1_BR)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInMassiveBounce(const int move)
{
	switch (move)
	{
	case BOTH_BASHED1:
	case BOTH_H1_S1_T_:
	case BOTH_H1_S1_TR:
	case BOTH_H1_S1_TL:
	case BOTH_H1_S1_BL:
	case BOTH_H1_S1_B_:
	case BOTH_H1_S1_BR:
		//
	case BOTH_V1_BR_S1:
	case BOTH_V1__R_S1:
	case BOTH_V1_TR_S1:
	case BOTH_V1_T__S1:
	case BOTH_V1_TL_S1:
	case BOTH_V1__L_S1:
	case BOTH_V1_BL_S1:
	case BOTH_V1_B__S1:
		//
	case BOTH_V6_BR_S6:
	case BOTH_V6__R_S6:
	case BOTH_V6_TR_S6:
	case BOTH_V6_T__S6:
	case BOTH_V6_TL_S6:
	case BOTH_V6__L_S6:
	case BOTH_V6_BL_S6:
	case BOTH_V6_B__S6:
		//
	case BOTH_V7_BR_S7:
	case BOTH_V7__R_S7:
	case BOTH_V7_TR_S7:
	case BOTH_V7_T__S7:
	case BOTH_V7_TL_S7:
	case BOTH_V7__L_S7:
	case BOTH_V7_BL_S7:
	case BOTH_V7_B__S7:
		//
	case BOTH_B1_BL___:
	case BOTH_B1_BR___:
	case BOTH_B1_TL___:
	case BOTH_B1_TR___:
	case BOTH_B1_T____:
	case BOTH_B1__L___:
	case BOTH_B1__R___:
		//
	case BOTH_B2_BL___:
	case BOTH_B2_BR___:
	case BOTH_B2_TL___:
	case BOTH_B2_TR___:
	case BOTH_B2_T____:
	case BOTH_B2__L___:
	case BOTH_B2__R___:
		//
	case BOTH_B3_BL___:
	case BOTH_B3_BR___:
	case BOTH_B3_TL___:
	case BOTH_B3_TR___:
	case BOTH_B3_T____:
	case BOTH_B3__L___:
	case BOTH_B3__R___:
		//
	case BOTH_B4_BL___:
	case BOTH_B4_BR___:
	case BOTH_B4_TL___:
	case BOTH_B4_TR___:
	case BOTH_B4_T____:
	case BOTH_B4__L___:
	case BOTH_B4__R___:
		//
	case BOTH_B5_BL___:
	case BOTH_B5_BR___:
	case BOTH_B5_TL___:
	case BOTH_B5_TR___:
	case BOTH_B5_T____:
	case BOTH_B5__L___:
	case BOTH_B5__R___:
		//
	case BOTH_B6_BL___:
	case BOTH_B6_BR___:
	case BOTH_B6_TL___:
	case BOTH_B6_TR___:
	case BOTH_B6_T____:
	case BOTH_B6__L___:
	case BOTH_B6__R___:
		//
	case BOTH_B7_BL___:
	case BOTH_B7_BR___:
	case BOTH_B7_TL___:
	case BOTH_B7_TR___:
	case BOTH_B7_T____:
	case BOTH_B7__L___:
	case BOTH_B7__R___:
		//
	case BOTH_K1_S1_T_: //# knockaway saber top
	case BOTH_K1_S1_TR: //# knockaway saber top right
	case BOTH_K1_S1_TL: //# knockaway saber top left
	case BOTH_K1_S1_BL: //# knockaway saber bottom left
	case BOTH_K1_S1_B_: //# knockaway saber bottom
	case BOTH_K1_S1_BR: //# knockaway saber bottom right
		//
	case BOTH_K6_S6_T_: //# knockaway saber top
	case BOTH_K6_S6_TR: //# knockaway saber top right
	case BOTH_K6_S6_TL: //# knockaway saber top left
	case BOTH_K6_S6_BL: //# knockaway saber bottom left
	case BOTH_K6_S6_B_: //# knockaway saber bottom
	case BOTH_K6_S6_BR: //# knockaway saber bottom right
		//
	case BOTH_K7_S7_T_: //# knockaway saber top
	case BOTH_K7_S7_TR: //# knockaway saber top right
	case BOTH_K7_S7_TL: //# knockaway saber top left
	case BOTH_K7_S7_BL: //# knockaway saber bottom left
	case BOTH_K7_S7_B_: //# knockaway saber bottom
	case BOTH_K7_S7_BR: //# knockaway saber bottom right
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInBashedAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BASHED1:
	case BOTH_PAIN3:
	case BOTH_PAIN2:
	case BOTH_PAIN5:
	case BOTH_PAIN7:
	case BOTH_PAIN12:
	case BOTH_PAIN15:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InForceGetUp(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_FORCE_GETUP_B6:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		if (ps->legsAnimTimer)
		{
			return qtrue;
		}
		break;
	default:;
	}
	return qfalse;
}

qboolean PM_InGetUp(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_GETUP_CROUCH_F1:
	case BOTH_GETUP_CROUCH_B1:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		if (ps->legsAnimTimer)
		{
			return qtrue;
		}
		break;
	default:
		return PM_InForceGetUp(ps);
	}
	//what the hell, redundant, but...
	return qfalse;
}

qboolean PM_InGetUpNoRoll(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_GETUP_CROUCH_F1:
	case BOTH_GETUP_CROUCH_B1:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_FORCE_GETUP_B6:
		if (ps->legsAnimTimer)
		{
			return qtrue;
		}
		break;
	default:;
	}
	return qfalse;
}

qboolean PM_InKnockDown(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
		//special anims:
	case BOTH_RELEASED:
		return qtrue;
	case BOTH_LK_DL_ST_T_SB_1_L:
		if (ps->legsAnimTimer < 550)
		{
			return qtrue;
		}
		break;
	case BOTH_PLAYER_PA_3_FLY:
		if (ps->legsAnimTimer < 300)
		{
			return qtrue;
		}
		break;
	default:
		return PM_InGetUp(ps);
	}
	return qfalse;
}

qboolean PM_Dyinganim(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_DEATH1: //# First Death anim
	case BOTH_DEATH2: //# Second Death anim
	case BOTH_DEATH3: //# Third Death anim
	case BOTH_DEATH4: //# Fourth Death anim
	case BOTH_DEATH5: //# Fifth Death anim
	case BOTH_DEATH6: //# Sixth Death anim
	case BOTH_DEATH7: //# Seventh Death anim
	case BOTH_DEATH8: //#
	case BOTH_DEATH9: //#
	case BOTH_DEATH10: //#
	case BOTH_DEATH11: //#
	case BOTH_DEATH12: //#
	case BOTH_DEATH13: //#
	case BOTH_DEATH14: //#
	case BOTH_DEATH14_UNGRIP: //# Desann's end death (cin #35)
	case BOTH_DEATH14_SITUP: //# Tavion sitting up after having been thrown (cin #23)
	case BOTH_DEATH15: //#
	case BOTH_DEATH16: //#
	case BOTH_DEATH17: //#
	case BOTH_DEATH18: //#
	case BOTH_DEATH19: //#
	case BOTH_DEATH20: //#
	case BOTH_DEATH21: //#
	case BOTH_DEATH22: //#
	case BOTH_DEATH23: //#
	case BOTH_DEATH24: //#
	case BOTH_DEATH25: //#
	case BOTH_DEATHFORWARD1: //# First Death in which they get thrown forward
	case BOTH_DEATHFORWARD2: //# Second Death in which they get thrown forward
	case BOTH_DEATHFORWARD3: //# Tavion's falling in cin# 23
	case BOTH_DEATHBACKWARD1: //# First Death in which they get thrown backward
	case BOTH_DEATHBACKWARD2: //# Second Death in which they get thrown backward
	case BOTH_DEATH1IDLE: //# Idle while close to death
	case BOTH_LYINGDEATH1: //# Death to play when killed lying down
	case BOTH_STUMBLEDEATH1: //# Stumble forward and fall face first death
	case BOTH_FALLDEATH1: //# Fall forward off a high cliff and splat death - start
	case BOTH_FALLDEATH1INAIR: //# Fall forward off a high cliff and splat death - loop
	case BOTH_FALLDEATH1LAND: //# Fall forward off a high cliff and splat death - hit bottom
	case BOTH_DEAD1: //# First Death finished pose
	case BOTH_DEAD2: //# Second Death finished pose
	case BOTH_DEAD3: //# Third Death finished pose
	case BOTH_DEAD4: //# Fourth Death finished pose
	case BOTH_DEAD5: //# Fifth Death finished pose
	case BOTH_DEAD6: //# Sixth Death finished pose
	case BOTH_DEAD7: //# Seventh Death finished pose
	case BOTH_DEAD8: //#
	case BOTH_DEAD9: //#
	case BOTH_DEAD10: //#
	case BOTH_DEAD11: //#
	case BOTH_DEAD12: //#
	case BOTH_DEAD13: //#
	case BOTH_DEAD14: //#
	case BOTH_DEAD15: //#
	case BOTH_DEAD16: //#
	case BOTH_DEAD17: //#
	case BOTH_DEAD18: //#
	case BOTH_DEAD19: //#
	case BOTH_DEAD20: //#
	case BOTH_DEAD21: //#
	case BOTH_DEAD22: //#
	case BOTH_DEAD23: //#
	case BOTH_DEAD24: //#
	case BOTH_DEAD25: //#
	case BOTH_DEADFORWARD1: //# First thrown forward death finished pose
	case BOTH_DEADFORWARD2: //# Second thrown forward death finished pose
	case BOTH_DEADBACKWARD1: //# First thrown backward death finished pose
	case BOTH_DEADBACKWARD2: //# Second thrown backward death finished pose
	case BOTH_LYINGDEAD1: //# Killed lying down death finished pose
	case BOTH_STUMBLEDEAD1: //# Stumble forward death finished pose
	case BOTH_FALLDEAD1LAND: //# Fall forward and splat death finished pose
	case BOTH_DEADFLOP1: //# React to being shot from First Death finished pose
	case BOTH_DEADFLOP2: //# React to being shot from Second Death finished pose
	case BOTH_DISMEMBER_HEAD1: //#
	case BOTH_DISMEMBER_TORSO1: //#
	case BOTH_DISMEMBER_LLEG: //#
	case BOTH_DISMEMBER_RLEG: //#
	case BOTH_DISMEMBER_RARM: //#
	case BOTH_DISMEMBER_LARM: //#
	case BOTH_DEATH_ROLL: //# Death anim from a roll
	case BOTH_DEATH_FLIP: //# Death anim from a flip
	case BOTH_DEATH_SPIN_90_R: //# Death anim when facing 90 degrees right
	case BOTH_DEATH_SPIN_90_L: //# Death anim when facing 90 degrees left
	case BOTH_DEATH_SPIN_180: //# Death anim when facing backwards
	case BOTH_DEATH_LYING_UP: //# Death anim when lying on back
	case BOTH_DEATH_LYING_DN: //# Death anim when lying on front
	case BOTH_DEATH_FALLING_DN: //# Death anim when falling on face
	case BOTH_DEATH_FALLING_UP: //# Death anim when falling on back
	case BOTH_DEATH_CROUCHED: //# Death anim when crouched
		return qtrue;
	default:
		return qfalse;
	}
}

qboolean BG_InKnockDown(const int anim)
{
	switch (anim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
		return qtrue;
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_InFlipBack(const int anim)
{
	switch (anim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_BASHED1:
	case BOTH_ROLL_F:
	case BOTH_ROLL_B:
	case BOTH_ROLL_R:
	case BOTH_ROLL_L:
	case BOTH_PAIN2:
	case BOTH_PAIN3:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_InSlapDown(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_BASHED1:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InKnockDownNoGetup(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
		//special anims:
	case BOTH_RELEASED:
		return qtrue;
	case BOTH_LK_DL_ST_T_SB_1_L:
		if (ps->legsAnimTimer < 550)
		{
			return qtrue;
		}
		break;
	case BOTH_PLAYER_PA_3_FLY:
		if (ps->legsAnimTimer < 300)
		{
			return qtrue;
		}
		break;
	default:;
	}
	return qfalse;
}

static qboolean PM_BoltBlockingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_P7_S1_B_:
	case BOTH_P6_S1_B_:
	case BOTH_P1_S1_B_:
	case BOTH_R7_TR_S7:
	case BOTH_R6_TR_S6:
	case BOTH_R2_TR_S1:
	case BOTH_R2_TL_S1:
	case BOTH_R7_TL_S7:
	case BOTH_R6_TL_S6:
	case BOTH_R1_TL_S1:
	case BOTH_R1_TR_S1:
	case BOTH_K7_S7_T_:
	case BOTH_K6_S6_T_:
	case BOTH_K1_S1_T_:
		//
	case BOTH_B7_TR___:
	case BOTH_B6_TR___:
	case BOTH_K1_S1_TR_ALT:
	case BOTH_B7_TL___:
	case BOTH_B6_TL___:
	case BOTH_K1_S1_TL_ALT:
	case BOTH_B7_BR___:
	case BOTH_B6_BR___:
	case BOTH_B1_BR___:
	case BOTH_B7_BL___:
	case BOTH_B6_BL___:
	case BOTH_B1_BL___:
		//
	case BOTH_V7_T__S7:
	case BOTH_V6_T__S6:
	case BOTH_V1_T__S1:
	case BOTH_V7_TR_S7:
	case BOTH_V6_TR_S6:
	case BOTH_V1_TR_S1:
	case BOTH_V7_TL_S7:
	case BOTH_V6_TL_S6:
	case BOTH_V1_TL_S1:
	case BOTH_V7_BR_S7:
	case BOTH_V6_BR_S6:
	case BOTH_V1_BR_S1:
	case BOTH_V7_BL_S7:
	case BOTH_V6_BL_S6:
	case BOTH_V1_BL_S1:
		//New Bolt block anims
	case BOTH_BOLT_BLOCK_BACKHAND_BOTTOM_LEFT:
	case BOTH_BOLT_BLOCK_BACKHAND_BOTTOM_RIGHT:
	case BOTH_BOLT_BLOCK_BACKHAND_MIDDLE_LEFT:
	case BOTH_BOLT_BLOCK_BACKHAND_MIDDLE_RIGHT:
	case BOTH_BOLT_BLOCK_BACKHAND_TOP_MIDDLE:
	case BOTH_BOLT_BLOCK_BACKHAND_TOP_LEFT:
	case BOTH_BOLT_BLOCK_BACKHAND_TOP_RIGHT:
		//
	case BOTH_BOLT_BLOCK_DUAL_BOTTOM_LEFT:
	case BOTH_BOLT_BLOCK_DUAL_BOTTOM_RIGHT:
	case BOTH_BOLT_BLOCK_DUAL_MIDDLE_LEFT:
	case BOTH_BOLT_BLOCK_DUAL_MIDDLE_RIGHT:
	case BOTH_BOLT_BLOCK_DUAL_TOP_LEFT:
	case BOTH_BOLT_BLOCK_DUAL_TOP_MIDDLE:
	case BOTH_BOLT_BLOCK_DUAL_TOP_RIGHT:
		//
	case BOTH_BOLT_BLOCK_STAFF_BOTTOM_LEFT:
	case BOTH_BOLT_BLOCK_STAFF_BOTTOM_RIGHT:
	case BOTH_BOLT_BLOCK_STAFF_MIDDLE_LEFT:
	case BOTH_BOLT_BLOCK_STAFF_MIDDLE_RIGHT:
	case BOTH_BOLT_BLOCK_STAFF_TOP_LEFT:
	case BOTH_BOLT_BLOCK_STAFF_TOP_MIDDLE:
	case BOTH_BOLT_BLOCK_STAFF_TOP_RIGHT:
		//
	case BOTH_BOLT_BLOCK_SINGLE_HAND_BOTTOM_LEFT:
	case BOTH_BOLT_BLOCK_SINGLE_HAND_BOTTOM_RIGHT:
	case BOTH_BOLT_BLOCK_SINGLE_HAND_MIDDLE_LEFT:
	case BOTH_BOLT_BLOCK_SINGLE_HAND_MIDDLE_RIGHT:
	case BOTH_BOLT_BLOCK_SINGLE_HAND_TOP_LEFT:
	case BOTH_BOLT_BLOCK_SINGLE_HAND_TOP_MIDDLE:
	case BOTH_BOLT_BLOCK_SINGLE_HAND_TOP_RIGHT:
		//
	case BOTH_BOLT_BLOCK_TWO_HAND_BOTTOM_LEFT:
	case BOTH_BOLT_BLOCK_TWO_HAND_BOTTOM_RIGHT:
	case BOTH_BOLT_BLOCK_TWO_HAND_MIDDLE_LEFT:
	case BOTH_BOLT_BLOCK_TWO_HAND_MIDDLE_RIGHT:
	case BOTH_BOLT_BLOCK_TWO_HAND_TOP_LEFT:
	case BOTH_BOLT_BLOCK_TWO_HAND_TOP_MIDDLE:
	case BOTH_BOLT_BLOCK_TWO_HAND_TOP_RIGHT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InKnockDownOnGround(playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
	case BOTH_RELEASED:
		//if ( PM_AnimLength( g_entities[ps->client_num].client->clientInfo.animFileIndex, (animNumber_t)ps->legsAnim ) - ps->legsAnimTimer > 300 )
	{
		//at end of fall down anim
		return qtrue;
	}
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_GETUP_CROUCH_F1:
	case BOTH_GETUP_CROUCH_B1:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_FORCE_GETUP_B6:
		if (PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ps->legsAnim)) - ps->legsAnimTimer < 500)
		{
			//at beginning of getup anim
			return qtrue;
		}
		break;
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		if (PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ps->legsAnim)) - ps->legsAnimTimer < 500)
		{
			//at beginning of getup anim
			return qtrue;
		}
		break;
	case BOTH_LK_DL_ST_T_SB_1_L:
		if (ps->legsAnimTimer < 1000)
		{
			return qtrue;
		}
		break;
	case BOTH_PLAYER_PA_3_FLY:
		if (ps->legsAnimTimer < 300)
		{
			return qtrue;
		}
		break;
	default:;
	}
	return qfalse;
}

static qboolean PM_CrouchGetup(const float crouchheight)
{
	pm->maxs[2] = crouchheight;
	pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
	int anim = -1;
	switch (pm->ps->legsAnim)
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN5:
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
	case BOTH_RELEASED:
	case BOTH_PLAYER_PA_3_FLY:
		anim = BOTH_GETUP_CROUCH_B1;
		break;
	case BOTH_LK_DL_ST_T_SB_1_L:
		anim = BOTH_GETUP_CROUCH_F1;
		break;
	default:;
	}
	if (anim == -1)
	{
		//WTF? stay down?
		pm->ps->legsAnimTimer = 100; //hold this anim for another 10th of a second
		return qfalse;
	}
	//get up into crouch anim
	if (PM_LockedAnim(pm->ps->torsoAnim))
	{
		//need to be able to override this anim
		pm->ps->torsoAnimTimer = 0;
	}
	if (PM_LockedAnim(pm->ps->legsAnim))
	{
		//need to be able to override this anim
		pm->ps->legsAnimTimer = 0;
	}
	PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
	pm->ps->saber_move = pm->ps->saberBounceMove = LS_READY; //don't finish whatever saber anim you may have been in
	pm->ps->saberBlocked = BLOCKED_NONE;
	return qtrue;
}

extern qboolean PM_GoingToAttackDown(const playerState_t* ps);

static qboolean PM_CheckRollGetup()
{
	if (pm->ps->legsAnim == BOTH_KNOCKDOWN1
		|| pm->ps->legsAnim == BOTH_KNOCKDOWN2
		|| pm->ps->legsAnim == BOTH_KNOCKDOWN3
		|| pm->ps->legsAnim == BOTH_KNOCKDOWN4
		|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
		|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
		|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
		|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L
		|| pm->ps->legsAnim == BOTH_PLAYER_PA_3_FLY
		|| pm->ps->legsAnim == BOTH_RELEASED)
	{
		//lying on back or front
		if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
			&& (pm->cmd.rightmove || pm->cmd.forwardmove && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0)
			//player pressing left or right
			|| pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer() && pm->gent->NPC //an NPC
			&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 //have at least force jump 1
			&& !(pm->ps->userInt3 & 1 << FLAG_FATIGUED) //can't do roll getups while fatigued.
			&& pm->gent->enemy //I have an enemy
			&& pm->gent->enemy->client //a client
			&& pm->gent->enemy->enemy == pm->gent //he's mad at me!
			&& (PM_GoingToAttackDown(&pm->gent->enemy->client->ps) || !Q_irand(0, 2))
			//he's attacking downward! (or we just feel like doing it this time)
			&& (pm->gent->client && pm->gent->client->NPC_class == CLASS_ALORA || Q_irand(0, RANK_CAPTAIN) <
				pm->gent->NPC->rank)) //higher rank I am, more likely I am to roll away!
		{
			//roll away!
			int anim;
			qboolean force_get_up = qfalse;
			if (pm->cmd.forwardmove > 0)
			{
				if (pm->ps->legsAnim == BOTH_KNOCKDOWN3
					|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
					|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
					|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
					|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
				{
					anim = BOTH_GETUP_FROLL_F;
				}
				else
				{
					anim = BOTH_GETUP_BROLL_F;
				}
				force_get_up = qtrue;
			}
			else if (pm->cmd.forwardmove < 0)
			{
				if (pm->ps->legsAnim == BOTH_KNOCKDOWN3
					|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
					|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
					|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
					|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
				{
					anim = BOTH_GETUP_FROLL_B;
				}
				else
				{
					anim = BOTH_GETUP_BROLL_B;
				}
				force_get_up = qtrue;
			}
			else if (pm->cmd.rightmove > 0)
			{
				if (pm->ps->legsAnim == BOTH_KNOCKDOWN3
					|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
					|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
					|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
					|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
				{
					anim = BOTH_GETUP_FROLL_R;
				}
				else
				{
					anim = BOTH_GETUP_BROLL_R;
				}
			}
			else if (pm->cmd.rightmove < 0)
			{
				if (pm->ps->legsAnim == BOTH_KNOCKDOWN3
					|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
					|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
					|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
					|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
				{
					anim = BOTH_GETUP_FROLL_L;
				}
				else
				{
					anim = BOTH_GETUP_BROLL_L;
				}
			}
			else
			{
				if (pm->ps->legsAnim == BOTH_KNOCKDOWN3
					|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
					|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
					|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
					|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
				{
					//on your front
					anim = Q_irand(BOTH_GETUP_FROLL_B, BOTH_GETUP_FROLL_R);
				}
				else
				{
					anim = Q_irand(BOTH_GETUP_BROLL_B, BOTH_GETUP_BROLL_R);
				}
			}
			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{
				if (!G_CheckRollSafety(pm->gent, anim, 64))
				{
					//oops, try other one
					if (pm->ps->legsAnim == BOTH_KNOCKDOWN3
						|| pm->ps->legsAnim == BOTH_KNOCKDOWN5
						|| pm->ps->legsAnim == BOTH_SLAPDOWNRIGHT
						|| pm->ps->legsAnim == BOTH_SLAPDOWNLEFT
						|| pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
					{
						if (anim == BOTH_GETUP_FROLL_R)
						{
							anim = BOTH_GETUP_FROLL_L;
						}
						else if (anim == BOTH_GETUP_FROLL_F)
						{
							anim = BOTH_GETUP_FROLL_B;
						}
						else if (anim == BOTH_GETUP_FROLL_B)
						{
							anim = BOTH_GETUP_FROLL_F;
						}
						else
						{
							anim = BOTH_GETUP_FROLL_L;
						}
						if (!G_CheckRollSafety(pm->gent, anim, 64))
						{
							//neither side is clear, screw it
							return qfalse;
						}
					}
					else
					{
						if (anim == BOTH_GETUP_BROLL_R)
						{
							anim = BOTH_GETUP_BROLL_L;
						}
						else if (anim == BOTH_GETUP_BROLL_F)
						{
							anim = BOTH_GETUP_BROLL_B;
						}
						else if (anim == BOTH_GETUP_FROLL_B)
						{
							anim = BOTH_GETUP_BROLL_F;
						}
						else
						{
							anim = BOTH_GETUP_BROLL_L;
						}
						if (!G_CheckRollSafety(pm->gent, anim, 64))
						{
							//neither side is clear, screw it
							return qfalse;
						}
					}
				}
			}
			pm->cmd.rightmove = pm->cmd.forwardmove = 0;
			if (PM_LockedAnim(pm->ps->torsoAnim))
			{
				//need to be able to override this anim
				pm->ps->torsoAnimTimer = 0;
			}
			if (PM_LockedAnim(pm->ps->legsAnim))
			{
				//need to be able to override this anim
				pm->ps->legsAnimTimer = 0;
			}
			PM_SetAnim(pm, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
			pm->ps->weaponTime = pm->ps->torsoAnimTimer - 300; //don't attack until near end of this anim
			pm->ps->saber_move = pm->ps->saberBounceMove = LS_READY;
			//don't finish whatever saber anim you may have been in
			pm->ps->saberBlocked = BLOCKED_NONE;
			if (force_get_up)
			{
				if (pm->gent && pm->gent->client && pm->gent->client->playerTeam == TEAM_ENEMY
					&& pm->gent->NPC && pm->gent->NPC->blockedSpeechDebounceTime < level.time
					&& !Q_irand(0, 1))
				{
					PM_AddEvent(Q_irand(EV_COMBAT1, EV_COMBAT3));
					pm->gent->NPC->blockedSpeechDebounceTime = level.time + 1000;
				}

				if (pm->gent && g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[
					FP_LEVITATION] < FORCE_LEVEL_3)
				{
					//short burst
					G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
				}
				else
				{
					//holding it
					G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
				}
				//launch off ground?
				pm->ps->weaponTime = 300; //just to make sure it's cleared
			}
			return qtrue;
		}
	}
	return qfalse;
}

extern int G_MinGetUpTime(const gentity_t* ent);

qboolean PM_GettingUpFromKnockDown(const float standheight, const float crouchheight)
{
	const int legs_anim = pm->ps->legsAnim;

	if (legs_anim == BOTH_KNOCKDOWN1
		|| legs_anim == BOTH_KNOCKDOWN2
		|| legs_anim == BOTH_KNOCKDOWN3
		|| legs_anim == BOTH_KNOCKDOWN4
		|| legs_anim == BOTH_KNOCKDOWN5
		|| legs_anim == BOTH_SLAPDOWNRIGHT
		|| legs_anim == BOTH_SLAPDOWNLEFT
		|| legs_anim == BOTH_PLAYER_PA_3_FLY
		|| legs_anim == BOTH_LK_DL_ST_T_SB_1_L
		|| legs_anim == BOTH_RELEASED)
	{
		//in a knockdown
		const int min_time_left = G_MinGetUpTime(pm->gent);

		if (pm->ps->legsAnimTimer <= min_time_left)
		{
			//if only a quarter of a second left, allow roll-aways
			if (PM_CheckRollGetup())
			{
				pm->cmd.rightmove = pm->cmd.forwardmove = 0;
				return qtrue;
			}
		}
		if (TIMER_Exists(pm->gent, "noGetUpStraight"))
		{
			if (!TIMER_Done2(pm->gent, "noGetUpStraight", qtrue))
			{
				//not allowed to do straight get-ups for another few seconds
				if (pm->ps->legsAnimTimer <= min_time_left)
				{
					//hold it for a bit
					pm->ps->legsAnimTimer = min_time_left + 1;
				}
			}
		}
		if (!pm->ps->legsAnimTimer
			|| pm->ps->legsAnimTimer <= min_time_left
			&& (pm->cmd.upmove > 0 || pm->gent && pm->gent->client && pm->gent->client->NPC_class ==
				CLASS_ALORA))
		{
			if (pm->cmd.upmove < 0)
			{
				return PM_CrouchGetup(crouchheight);
			}
			trace_t trace;
			// try to stand up
			pm->maxs[2] = standheight;
			pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->client_num, pm->tracemask,
				static_cast<EG2_Collision>(0), 0);

			if (!trace.allsolid)
			{
				//stand up
				int anim = BOTH_GETUP1;
				qboolean force_get_up = qfalse;
				pm->maxs[2] = standheight;
				pm->ps->viewheight = standheight + STANDARD_VIEWHEIGHT_OFFSET;

				switch (pm->ps->legsAnim)
				{
				case BOTH_KNOCKDOWN1:
					if (pm->ps->client_num && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0
						|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() && !(pm->ps->userInt3 & 1 <<
							FLAG_FATIGUED))
						&& pm->cmd.upmove > 0
						&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0)
					{
						anim = Q_irand(BOTH_FORCE_GETUP_B1, BOTH_FORCE_GETUP_B6);
						force_get_up = qtrue;
					}
					else
					{
						anim = BOTH_GETUP1;
					}
					break;
				case BOTH_KNOCKDOWN2:
				case BOTH_PLAYER_PA_3_FLY:
					if (pm->ps->client_num && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0
						|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() && !(pm->ps->userInt3 & 1 <<
							FLAG_FATIGUED))
						&& pm->cmd.upmove > 0
						&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0)
					{
						anim = Q_irand(BOTH_FORCE_GETUP_B1, BOTH_FORCE_GETUP_B6);
						force_get_up = qtrue;
					}
					else
					{
						anim = BOTH_GETUP2;
					}
					break;
				case BOTH_KNOCKDOWN3:
					if (pm->ps->client_num && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0
						|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() && !(pm->ps->userInt3 & 1 <<
							FLAG_FATIGUED))
						&& pm->cmd.upmove > 0
						&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0)
					{
						anim = Q_irand(BOTH_FORCE_GETUP_F1, BOTH_FORCE_GETUP_F2);
						force_get_up = qtrue;
					}
					else
					{
						anim = BOTH_GETUP3;
					}
					break;
				case BOTH_KNOCKDOWN4:
				case BOTH_RELEASED:
					if (pm->ps->client_num && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0
						|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() && !(pm->ps->userInt3 & 1 <<
							FLAG_FATIGUED))
						&& pm->cmd.upmove > 0
						&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0)
					{
						anim = Q_irand(BOTH_FORCE_GETUP_B1, BOTH_FORCE_GETUP_B6);
						force_get_up = qtrue;
					}
					else
					{
						anim = BOTH_GETUP4;
					}
					break;
				case BOTH_KNOCKDOWN5:
				case BOTH_SLAPDOWNRIGHT:
				case BOTH_SLAPDOWNLEFT:
				case BOTH_LK_DL_ST_T_SB_1_L:
					if (pm->ps->client_num && pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0
						|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() && !(pm->ps->userInt3 & 1 <<
							FLAG_FATIGUED))
						&& pm->cmd.upmove > 0
						&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0)
					{
						anim = Q_irand(BOTH_FORCE_GETUP_F1, BOTH_FORCE_GETUP_F2);
						force_get_up = qtrue;
					}
					else
					{
						anim = BOTH_GETUP5;
					}
					break;
				default:;
				}

				if (force_get_up)
				{
					if (pm->gent && pm->gent->client && pm->gent->client->playerTeam == TEAM_ENEMY
						&& pm->gent->NPC && pm->gent->NPC->blockedSpeechDebounceTime < level.time
						&& !Q_irand(0, 1))
					{
						PM_AddEvent(Q_irand(EV_COMBAT1, EV_COMBAT3));
						pm->gent->NPC->blockedSpeechDebounceTime = level.time + 1000;
					}
					if (pm->gent && g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel
						[FP_LEVITATION] < FORCE_LEVEL_3)
					{
						//short burst
						G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
					}
					else
					{
						//holding it
						G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
					}
					//launch off ground?
					pm->ps->weaponTime = 300; //just to make sure it's cleared
				}
				if (PM_LockedAnim(pm->ps->torsoAnim))
				{
					//need to be able to override this anim
					pm->ps->torsoAnimTimer = 0;
				}
				if (PM_LockedAnim(pm->ps->legsAnim))
				{
					//need to be able to override this anim
					pm->ps->legsAnimTimer = 0;
				}
				PM_SetAnim(pm, SETANIM_BOTH, anim,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
				pm->ps->saber_move = pm->ps->saberBounceMove = LS_READY;
				//don't finish whatever saber anim you may have been in
				pm->ps->saberBlocked = BLOCKED_NONE;
				return qtrue;
			}
			return PM_CrouchGetup(crouchheight);
		}
		if (pm->ps->legsAnim == BOTH_LK_DL_ST_T_SB_1_L)
		{
			PM_CmdForRoll(pm->ps, &pm->cmd);
		}
		else
		{
			pm->cmd.rightmove = pm->cmd.forwardmove = 0;
		}
	}
	return qfalse;
}

void PM_CmdForRoll(playerState_t* ps, usercmd_t* p_cmd)
{
	switch (ps->legsAnim)
	{
	case BOTH_ROLL_F:
		p_cmd->forwardmove = 127;
		p_cmd->rightmove = 0;
		break;
	case BOTH_ROLL_B:
		p_cmd->forwardmove = -127;
		p_cmd->rightmove = 0;
		break;
	case BOTH_ROLL_R:
		p_cmd->forwardmove = 0;
		p_cmd->rightmove = 127;
		break;
	case BOTH_ROLL_L:
		p_cmd->forwardmove = 0;
		p_cmd->rightmove = -127;
		break;

	case BOTH_GETUP_BROLL_R:
		p_cmd->forwardmove = 0;
		p_cmd->rightmove = 48;
		//NOTE: speed is 400
		break;

	case BOTH_GETUP_FROLL_R:
		if (ps->legsAnimTimer <= 250)
		{
			//end of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else
		{
			p_cmd->forwardmove = 0;
			p_cmd->rightmove = 48;
			//NOTE: speed is 400
		}
		break;

	case BOTH_GETUP_BROLL_L:
		p_cmd->forwardmove = 0;
		p_cmd->rightmove = -48;
		//NOTE: speed is 400
		break;

	case BOTH_GETUP_FROLL_L:
		if (ps->legsAnimTimer <= 250)
		{
			//end of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else
		{
			p_cmd->forwardmove = 0;
			p_cmd->rightmove = -48;
			//NOTE: speed is 400
		}
		break;

	case BOTH_GETUP_BROLL_B:
		if (ps->torsoAnimTimer <= 250)
		{
			//end of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else if (PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ps->legsAnim)) - ps->torsoAnimTimer < 350)
		{
			//beginning of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else
		{
			//FIXME: ramp down over length of anim
			p_cmd->forwardmove = -64;
			p_cmd->rightmove = 0;
			//NOTE: speed is 400
		}
		break;

	case BOTH_GETUP_FROLL_B:
		if (ps->torsoAnimTimer <= 100)
		{
			//end of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else if (PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ps->legsAnim)) - ps->torsoAnimTimer < 200)
		{
			//beginning of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else
		{
			//FIXME: ramp down over length of anim
			p_cmd->forwardmove = -64;
			p_cmd->rightmove = 0;
			//NOTE: speed is 400
		}
		break;

	case BOTH_GETUP_BROLL_F:
		if (ps->torsoAnimTimer <= 550)
		{
			//end of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else if (PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ps->legsAnim)) - ps->torsoAnimTimer < 150)
		{
			//beginning of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else
		{
			p_cmd->forwardmove = 64;
			p_cmd->rightmove = 0;
			//NOTE: speed is 400
		}
		break;

	case BOTH_GETUP_FROLL_F:
		if (ps->torsoAnimTimer <= 100)
		{
			//end of anim
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		else
		{
			//FIXME: ramp down over length of anim
			p_cmd->forwardmove = 64;
			p_cmd->rightmove = 0;
			//NOTE: speed is 400
		}
		break;
	case BOTH_LK_DL_ST_T_SB_1_L:
		//kicked backwards
		if (ps->legsAnimTimer < 3050 //at least 10 frames in
			&& ps->legsAnimTimer > 550) //at least 6 frames from end
		{
			//move backwards
			p_cmd->forwardmove = -64;
			p_cmd->rightmove = 0;
		}
		else
		{
			p_cmd->forwardmove = p_cmd->rightmove = 0;
		}
		break;
	default:;
	}

	p_cmd->upmove = 0;
}

qboolean PM_InRollIgnoreTimer(const playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_ROLL_F:
	case BOTH_ROLL_B:
	case BOTH_ROLL_R:
	case BOTH_ROLL_L:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InRoll(const playerState_t* ps)
{
	if (ps->legsAnimTimer && PM_InRollIgnoreTimer(ps))
	{
		return qtrue;
	}

	return qfalse;
}

qboolean PM_RestAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_MEDITATE: // default taunt
	case BOTH_MEDITATE_SABER:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_ReloadAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_RECHARGE:
	case BOTH_RELOAD:
	case BOTH_RELOADFAIL:
		//
	case BOTH_RECHARGE_BAZOOKA:
	case BOTH_RECHARGE_BLASTER:
	case BOTH_RECHARGE_DOUBLE_PISTOL:
	case BOTH_RECHARGE_HEAVY:
	case BOTH_RECHARGE_MINIGUN:
	case BOTH_RECHARGE_PISTOL:
		//
	case BOTH_RELOAD_BAZOOKA:
	case BOTH_RELOAD_BLASTER:
	case BOTH_RELOAD_DOUBLE_PISTOL:
	case BOTH_RELOAD_HEAVY:
	case BOTH_RELOAD_MINIGUN:
	case BOTH_RELOAD_PISTOL:
		//
	case BOTH_RELOAD_FAIL_BAZOOKA:
	case BOTH_RELOAD_FAIL_BLASTER:
	case BOTH_RELOAD_FAIL_DOUBLE_PISTOL:
	case BOTH_RELOAD_FAIL_HEAVY:
	case BOTH_RELOAD_FAIL_MINIGUN:
	case BOTH_RELOAD_FAIL_PISTOL:
		//
	case BOTH_RIFLERELOAD:
	case BOTH_RIFLERECHARGE:
	case BOTH_RIFLEFAIL:
		//
	case BOTH_ROCKETRELOAD:
	case BOTH_ROCKETCHARGE:
	case BOTH_ROCKETFAIL:
		//
	case BOTH_PISTOLRELOAD:
	case BOTH_PISTOLCHARGE:
	case BOTH_PISTOLFAIL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_WeponRestAnim(const int anim)
{
	switch (anim)
	{
	case TORSO_WEAPONREST2:
	case TORSO_WEAPONREST3:
	case TORSO_WEAPONREST4:
		//
	case BOTH_STAND1IDLE1:
	case BOTH_STAND9IDLE1:
		//
	case BOTH_STANCE_MINIGUN_IDLE:
		//
	case BOTH_WALK_BLASTER:
	case BOTH_WALK_BAZOOKA:
	case BOTH_WALK_HEAVY:
	case BOTH_WALK_PISTOL:
	case BOTH_WALK_GRENADE:
		//
	case BOTH_JOG_BAZOOKA:
	case BOTH_JOG_BLASTER:
	case BOTH_JOG_DOUBLE_PISTOL:
	case BOTH_JOG_GRENADE:
	case BOTH_JOG_HEAVY:
	case BOTH_JOG_MINIGUN:
	case BOTH_JOG_PISTOL:
		//
	case BOTH_SPRINT_BAZOOKA:
	case BOTH_SPRINT_BLASTER:
	case BOTH_SPRINT_DOUBLE_PISTOL:
	case BOTH_SPRINT_GRENADE:
	case BOTH_SPRINT_HEAVY:
	case BOTH_SPRINT_MINIGUN:
	case BOTH_SPRINT_PISTOL:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_WeponFatiguedAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_H1_S1_TR:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_LungeAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_LUNGE2_B__T_:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_Saberinstab(const int move)
{
	switch (move)
	{
	case LS_ROLL_STAB:
	case LS_A_BACKSTAB:
	case LS_A_FLIP_STAB:
	case LS_A_FLIP_SLASH:
	case LS_UPSIDE_DOWN_ATTACK:
	case LS_PULL_ATTACK_STAB:
	case LS_GRAPPLE_FIRE:
	case LS_PULL_ATTACK_SWING:
	case LS_A_JUMP_T__B_:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_StabAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_ROLL_STAB:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_CrouchAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_SIT1: //# Normal chair sit.
	case BOTH_SIT2: //# Lotus position.
	case BOTH_SIT3: //# Sitting in tired position: elbows on knees
	case BOTH_CROUCH1: //# Transition from standing to crouch
	case BOTH_CROUCH1IDLE: //# Crouching idle
	case BOTH_CROUCH1WALK: //# Walking while crouched
	case BOTH_CROUCH1WALKBACK: //# Walking while crouched
	case BOTH_CROUCH2: //# Transition from standing to crouch
	case BOTH_CROUCH2TOSTAND1: //# going from crouch2 to stand1
	case BOTH_CROUCH3: //# Desann crouching down to Kyle (cin 9)
	case BOTH_KNEES1: //# Tavion on her knees
	case BOTH_CROUCHATTACKBACK1: //FIXME: not if in middle of anim?
	case BOTH_ROLL_STAB:
	case BOTH_STAND_TO_KNEEL:
	case BOTH_KNEEL_TO_STAND:
	case BOTH_TURNCROUCH1:
	case BOTH_CROUCH4:
	case BOTH_KNEES2: //# Tavion on her knees looking down
	case BOTH_KNEES2TO1: //# Transition of KNEES2 to KNEES1
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_CrouchingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_CROUCH1: //# Transition from standing to crouch
	case BOTH_CROUCH1IDLE: //# Crouching idle
	case BOTH_CROUCH1WALK: //# Walking while crouched
	case BOTH_CROUCH1WALKBACK: //# Walking while crouched
	case BOTH_CROUCH2TOSTAND1: //# going from crouch2 to stand1
	case BOTH_CROUCH3: //# Desann crouching down to Kyle (cin 9)
	case BOTH_CROUCHATTACKBACK1: //FIXME: not if in middle of anim?
	case BOTH_TURNCROUCH1:
	case BOTH_CROUCH4:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_PainAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_PAIN1: //# First take pain anim
	case BOTH_PAIN2: //# Second take pain anim
	case BOTH_PAIN3: //# Third take pain anim
	case BOTH_PAIN4: //# Fourth take pain anim
	case BOTH_PAIN5: //# Fifth take pain anim - from behind
	case BOTH_PAIN6: //# Sixth take pain anim - from behind
	case BOTH_PAIN7: //# Seventh take pain anim - from behind
	case BOTH_PAIN8: //# Eigth take pain anim - from behind
	case BOTH_PAIN9: //#
	case BOTH_PAIN10: //#
	case BOTH_PAIN11: //#
	case BOTH_PAIN12: //#
	case BOTH_PAIN13: //#
	case BOTH_PAIN14: //#
	case BOTH_PAIN15: //#
	case BOTH_PAIN16: //#
	case BOTH_PAIN17: //#
	case BOTH_PAIN18: //#
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_EvasionHoldAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_DODGE_HOLD_FL:
	case BOTH_DODGE_HOLD_FR:
	case BOTH_DODGE_HOLD_BL:
	case BOTH_DODGE_HOLD_BR:
	case BOTH_DODGE_HOLD_L:
	case BOTH_DODGE_HOLD_R:
	case BOTH_HOP_L: //# lean-dodge left
	case BOTH_HOP_R: //# lean-dodge right
	case BOTH_CROUCHDODGE: //# lean-dodge right
	case BOTH_DODGE_B:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_EvasionAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_DODGE_FL: //# lean-dodge forward left
	case BOTH_DODGE_FR: //# lean-dodge forward right
	case BOTH_DODGE_BL: //# lean-dodge backwards left
	case BOTH_DODGE_BR: //# lean-dodge backwards right
	case BOTH_DODGE_L: //# lean-dodge left
	case BOTH_DODGE_R: //# lean-dodge right
	case BOTH_HOP_L: //# lean-dodge left
	case BOTH_HOP_R: //# lean-dodge right
	case BOTH_CROUCHDODGE: //# lean-dodge right
	case BOTH_DODGE_B:
		return qtrue;
	default:
		return PM_EvasionHoldAnim(anim);
	}
}

qboolean PM_DodgeHoldAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_DODGE_HOLD_FL:
	case BOTH_DODGE_HOLD_FR:
	case BOTH_DODGE_HOLD_BL:
	case BOTH_DODGE_HOLD_BR:
	case BOTH_DODGE_HOLD_L:
	case BOTH_DODGE_HOLD_R:
	case BOTH_CROUCHDODGE:
	case BOTH_DODGE_B:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_DodgeAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_DODGE_FL: //# lean-dodge forward left
	case BOTH_DODGE_FR: //# lean-dodge forward right
	case BOTH_DODGE_BL: //# lean-dodge backwards left
	case BOTH_DODGE_BR: //# lean-dodge backwards right
	case BOTH_DODGE_L: //# lean-dodge left
	case BOTH_DODGE_R: //# lean-dodge right
	case BOTH_CROUCHDODGE:
	case BOTH_DODGE_B:
	case BOTH_HOP_L:
	case BOTH_HOP_R:
	case BOTH_DASH_L:
	case BOTH_DASH_R:
		return qtrue;
	default:
		return PM_DodgeHoldAnim(anim);
	}
	//return qfalse;
}

qboolean PM_BlockHoldAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BLOCK_HOLD_FL:
	case BOTH_BLOCK_HOLD_FR:
	case BOTH_BLOCK_HOLD_BL:
	case BOTH_BLOCK_HOLD_BR:
	case BOTH_BLOCK_HOLD_L:
	case BOTH_BLOCK_HOLD_R:

	case BOTH_BLOCK_HOLD_L_DUAL:
	case BOTH_BLOCK_HOLD_R_DUAL:
	case BOTH_BLOCK_HOLD_L_STAFF:
	case BOTH_BLOCK_HOLD_R_STAFF:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_BlockAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BLOCK_FL:
	case BOTH_BLOCK_FR:
	case BOTH_BLOCK_BL:
	case BOTH_BLOCK_BR:
	case BOTH_BLOCK_L:
	case BOTH_BLOCK_R:

	case BOTH_BLOCK_L_DUAL:
	case BOTH_BLOCK_R_DUAL:
	case BOTH_BLOCK_L_STAFF:
	case BOTH_BLOCK_R_STAFF:
		return qtrue;
	default:
		return PM_BlockHoldAnim(anim);
	}
}

qboolean PM_BlockHoldDualAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BLOCK_HOLD_L_DUAL:
	case BOTH_BLOCK_HOLD_R_DUAL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_BlockDualAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BLOCK_L_DUAL:
	case BOTH_BLOCK_R_DUAL:
		return qtrue;
	default:
		return PM_BlockHoldDualAnim(anim);
	}
}

qboolean PM_BlockHoldStaffAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BLOCK_HOLD_L_STAFF:
	case BOTH_BLOCK_HOLD_R_STAFF:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_BlockStaffAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BLOCK_L_STAFF:
	case BOTH_BLOCK_R_STAFF:
		return qtrue;
	default:
		return PM_BlockHoldStaffAnim(anim);
	}
}

qboolean PM_ForceJumpingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCEJUMP1: //# Jump - wind-up and leave ground
	case BOTH_FORCEINAIR1: //# In air loop (from jump)
	case BOTH_FORCELAND1: //# Landing (from in air loop)
	case BOTH_FORCEJUMPBACK1: //# Jump backwards - wind-up and leave ground
	case BOTH_FORCEINAIRBACK1: //# In air loop (from jump back)
	case BOTH_FORCELANDBACK1: //# Landing backwards(from in air loop)
	case BOTH_FORCEJUMPLEFT1: //# Jump left - wind-up and leave ground
	case BOTH_FORCEINAIRLEFT1: //# In air loop (from jump left)
	case BOTH_FORCELANDLEFT1: //# Landing left(from in air loop)
	case BOTH_FORCEJUMPRIGHT1: //# Jump right - wind-up and leave ground
	case BOTH_FORCEINAIRRIGHT1: //# In air loop (from jump right)
	case BOTH_FORCELANDRIGHT1: //# Landing right(from in air loop)
	case BOTH_GRAPPLE_PULL: //# Landing right(from in air loop)
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_JumpingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_JUMP1: //# Jump - wind-up and leave ground
	case BOTH_INAIR1: //# In air loop (from jump)
	case BOTH_LAND1: //# Landing (from in air loop)
	case BOTH_LAND2: //# Landing Hard (from a great height)
	case BOTH_JUMPBACK1: //# Jump backwards - wind-up and leave ground
	case BOTH_INAIRBACK1: //# In air loop (from jump back)
	case BOTH_LANDBACK1: //# Landing backwards(from in air loop)
	case BOTH_JUMPLEFT1: //# Jump left - wind-up and leave ground
	case BOTH_INAIRLEFT1: //# In air loop (from jump left)
	case BOTH_LANDLEFT1: //# Landing left(from in air loop)
	case BOTH_JUMPRIGHT1: //# Jump right - wind-up and leave ground
	case BOTH_INAIRRIGHT1: //# In air loop (from jump right)
	case BOTH_LANDRIGHT1: //# Landing right(from in air loop)
		return qtrue;
	default:
		if (PM_InAirKickingAnim(anim))
		{
			return qtrue;
		}
		return PM_ForceJumpingAnim(anim);
	}
	//return qfalse;
}

qboolean PM_LandingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_LAND1: //# Landing (from in air loop)
	case BOTH_LAND2: //# Landing Hard (from a great height)
	case BOTH_LANDBACK1: //# Landing backwards(from in air loop)
	case BOTH_LANDLEFT1: //# Landing left(from in air loop)
	case BOTH_LANDRIGHT1: //# Landing right(from in air loop)
	case BOTH_FORCELAND1: //# Landing (from in air loop)
	case BOTH_FORCELANDBACK1: //# Landing backwards(from in air loop)
	case BOTH_FORCELANDLEFT1: //# Landing left(from in air loop)
	case BOTH_FORCELANDRIGHT1: //# Landing right(from in air loop)
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_FlippingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_FLIP_F: //# Flip forward
	case BOTH_FLIP_B: //# Flip backwards
	case BOTH_FLIP_L: //# Flip left
	case BOTH_FLIP_R: //# Flip right
	case BOTH_ALORA_FLIP_1:
	case BOTH_ALORA_FLIP_2:
	case BOTH_ALORA_FLIP_3:
	case BOTH_WALL_RUN_RIGHT_FLIP:
	case BOTH_WALL_RUN_LEFT_FLIP:
	case BOTH_WALL_FLIP_RIGHT:
	case BOTH_WALL_FLIP_LEFT:
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_FLIP_BACK3:
	case BOTH_WALL_FLIP_BACK1:
	case BOTH_ALORA_FLIP_B:
		//Not really flips, but...
	case BOTH_WALL_RUN_RIGHT:
	case BOTH_WALL_RUN_LEFT:
	case BOTH_WALL_RUN_RIGHT_STOP:
	case BOTH_WALL_RUN_LEFT_STOP:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
		//
	case BOTH_ARIAL_LEFT:
	case BOTH_ARIAL_RIGHT:
	case BOTH_ARIAL_F1:
	case BOTH_CARTWHEEL_LEFT:
	case BOTH_CARTWHEEL_RIGHT:
	case BOTH_JUMPFLIPSLASHDOWN1:
	case BOTH_JUMPFLIPSTABDOWN:
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
		//JKA
	case BOTH_FORCEWALLRUNFLIP_END:
	case BOTH_FORCEWALLRUNFLIP_ALT:
	case BOTH_FLIP_ATTACK7:
	case BOTH_A7_SOULCAL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_WindAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_WIND:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_WalkingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_WALK1: //# Normal walk
	case BOTH_WALK1_MDA:
	case BOTH_WALK2: //# Normal walk with saber
	case BOTH_WALK_STAFF:
	case BOTH_WALK_STAFF_AMD: //# Normal walk with staff
	case BOTH_WALK_DUAL:
	case BOTH_WALK_DUAL_AMD: //# Normal walk with staff
	case BOTH_WALK5: //# Tavion taunting Kyle (cin 22)
	case BOTH_WALK6: //# Slow walk for Luke (cin 12)
	case BOTH_WALK7: //# Fast walk
	case BOTH_WALKBACK1: //# Walk1 backwards
	case BOTH_WALKBACK2: //# Walk2 backwards
	case BOTH_WALKBACK_STAFF: //# Walk backwards with staff
	case BOTH_WALKBACK_DUAL: //# Walk backwards with dual
	case BOTH_MENUIDLE1: //# Walk backwards with dual
	case BOTH_WALK1_STICK: //# Normal yoda walk
		//
	case BOTH_WALKBACK_PISTOL:
	case BOTH_WALKBACK_BLASTER:
	case BOTH_WALKBACK_HEAVY:
	case BOTH_WALKBACK_GRENADE:
	case BOTH_WALKBACK_DUALPISTOL:
		//
	case BOTH_WALK_BAZOOKA:
	case BOTH_WALK_BLASTER:
	case BOTH_WALK_DOUBLE_PISTOL:
	case BOTH_WALK_GRENADE:
	case BOTH_WALK_HEAVY:
	case BOTH_WALK_MINIGUN:
	case BOTH_WALK_PISTOL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_RunningAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_RUN1:
	case BOTH_SPRINT:
	case BOTH_SPRINT_SINGLE_LIGHTSABER:
	case BOTH_SPRINT_STAFF_LIGHTSABER:
	case BOTH_SPRINT_DUAL_LIGHTSABER:
	case BOTH_SPRINT_BAZOOKA:
	case BOTH_SPRINT_BLASTER:
	case BOTH_SPRINT_DOUBLE_PISTOL:
	case BOTH_SPRINT_GRENADE:
	case BOTH_SPRINT_HEAVY:
	case BOTH_SPRINT_MINIGUN:
	case BOTH_SPRINT_PISTOL:
	case BOTH_RUN2:
	case BOTH_RUN4:
	case BOTH_RUN5:
	case BOTH_RUN6:
	case BOTH_RUN7:
	case BOTH_RUN8:
	case BOTH_RUN9:
	case BOTH_RUN10:
		/*case BOTH_SPRINT_SABER:
		case BOTH_SPRINT_MP:
		case BOTH_SPRINT_SABER_MP:*/
	case SBD_RUNBACK_NORMAL:
	case SBD_RUNING_WEAPON:
	case SBD_RUNBACK_WEAPON:
	case BOTH_RUN_STAFF:
	case BOTH_RUN_DUAL:
	case BOTH_RUNBACK1:
	case BOTH_RUNBACK2:
	case BOTH_RUNBACK_STAFF:
	case BOTH_RUN1START: //# Start into full run1
	case BOTH_RUN1STOP: //# Stop from full run1
	case BOTH_RUNSTRAFE_LEFT1: //# Sidestep left: should loop
	case BOTH_RUNSTRAFE_RIGHT1: //# Sidestep right: should loop
		//
	case BOTH_RUNBACK_PISTOL:
	case BOTH_RUNBACK_BLASTER:
	case BOTH_RUNBACK_HEAVY:
	case BOTH_RUNBACK_GRENADE:
	case BOTH_RUNBACK_DUALPISTOL:
		//
	case BOTH_JOG_BAZOOKA:
	case BOTH_JOG_BLASTER:
	case BOTH_JOG_DOUBLE_PISTOL:
	case BOTH_JOG_GRENADE:
	case BOTH_JOG_HEAVY:
	case BOTH_JOG_MINIGUN:
	case BOTH_JOG_PISTOL:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_NotWalkingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_RUN1:
	case BOTH_SPRINT:
	case BOTH_SPRINT_SINGLE_LIGHTSABER:
	case BOTH_SPRINT_STAFF_LIGHTSABER:
	case BOTH_SPRINT_DUAL_LIGHTSABER:
	case BOTH_SPRINT_BAZOOKA:
	case BOTH_SPRINT_BLASTER:
	case BOTH_SPRINT_DOUBLE_PISTOL:
	case BOTH_SPRINT_GRENADE:
	case BOTH_SPRINT_HEAVY:
	case BOTH_SPRINT_MINIGUN:
	case BOTH_SPRINT_PISTOL:
	case BOTH_RUN2:
	case BOTH_RUN4:
	case BOTH_RUN_STAFF:
	case BOTH_RUN_DUAL:
	case BOTH_RUNBACK1:
	case BOTH_RUNBACK2:
	case BOTH_RUNBACK_STAFF:
	case BOTH_RUN1START: //# Start into full run1
	case BOTH_RUN1STOP: //# Stop from full run1
	case BOTH_RUNSTRAFE_LEFT1: //# Sidestep left: should loop
	case BOTH_RUNSTRAFE_RIGHT1: //# Sidestep right: should loop
		//
	case BOTH_RUNBACK_PISTOL:
	case BOTH_RUNBACK_BLASTER:
	case BOTH_RUNBACK_HEAVY:
	case BOTH_RUNBACK_GRENADE:
	case BOTH_RUNBACK_DUALPISTOL:
		//
	case BOTH_JOG_BAZOOKA:
	case BOTH_JOG_BLASTER:
	case BOTH_JOG_DOUBLE_PISTOL:
	case BOTH_JOG_GRENADE:
	case BOTH_JOG_HEAVY:
	case BOTH_JOG_MINIGUN:
	case BOTH_JOG_PISTOL:
		//
	case BOTH_HOP_F:
	case BOTH_HOP_R:
	case BOTH_HOP_L:
	case BOTH_HOP_B:
	case BOTH_DASH_F:
	case BOTH_DASH_R:
	case BOTH_DASH_L:
	case BOTH_DASH_B:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_RollingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_ROLL_F: //# Roll forward
	case BOTH_ROLL_B: //# Roll backward
	case BOTH_ROLL_L: //# Roll left
	case BOTH_ROLL_R: //# Roll right
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_SaberWalkAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_WALK1:
	case BOTH_WALK1_MDA:
	case BOTH_WALK2: //# Normal walk with saber
	case BOTH_WALK_STAFF: //# Normal walk with staff
	case BOTH_WALK_STAFF_AMD:
	case BOTH_WALK_DUAL:
	case BOTH_WALK_DUAL_AMD: //# Normal walk with staff
	case BOTH_WALKBACK1: //# Walk1 backwards
	case BOTH_WALKBACK2: //# Walk2 backwards
	case BOTH_WALKBACK_STAFF: //# Walk backwards with staff
	case BOTH_WALKBACK_DUAL: //# Walk backwards with dual
	case BOTH_MENUIDLE1: //# Walk backwards with dual
	case BOTH_WALK1_STICK: //# Normal yoda walk
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean WalkCheck(const gentity_t* self)
{
	if (PM_RunningAnim(self->client->ps.legsAnim))
	{
		return qfalse;
	}

	return qtrue;
}

qboolean IsWeponPickUpAllowed(const gentity_t* self)
{
	if (PM_NotWalkingAnim(self->client->ps.legsAnim))
	{
		return qfalse;
	}

	return qtrue;
}

qboolean PM_WalkingOrStanding(const gentity_t* self)
{
	//mainly for checking if we can guard well
	if (PM_WalkingAnim(self->client->ps.legsAnim)
		|| PM_StandingAnim(self->client->ps.legsAnim)
		|| PM_CrouchAnim(self->client->ps.legsAnim))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_WalkingOrRunningAnim(const int anim)
{
	//mainly for checking if we can guard well
	if (PM_WalkingAnim(anim)
		|| PM_RunningAnim(anim)
		|| PM_RollingAnim(anim)
		|| PM_SaberWalkAnim(anim))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SwimmingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_SWIM_IDLE1: //# Swimming Idle 1
	case BOTH_SWIMFORWARD: //# Swim forward loop
	case BOTH_SWIMBACKWARD: //# Swim backward loop
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_LeapingSaberAnim(const int anim)
{
	switch (anim)
	{
		//level 7
	case BOTH_T7_BR_TL:
	case BOTH_T7__L_BR:
	case BOTH_T7__L__R:
	case BOTH_T7_BL_BR:
	case BOTH_T7_BL__R:
	case BOTH_T7_BL_TR:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SpinningSaberAnim(const int anim)
{
	switch (anim)
	{
		//level 1 - FIXME: level 1 will have *no* spins
	case BOTH_T1_BR_BL:
	case BOTH_T1__R__L:
	case BOTH_T1__R_BL:
	case BOTH_T1_TR_BL:
	case BOTH_T1_BR_TL:
	case BOTH_T1_BR__L:
	case BOTH_T1_TL_BR:
	case BOTH_T1__L_BR:
	case BOTH_T1__L__R:
	case BOTH_T1_BL_BR:
	case BOTH_T1_BL__R:
	case BOTH_T1_BL_TR:
		//level 2
	case BOTH_T2_BR__L:
	case BOTH_T2_BR_BL:
	case BOTH_T2__R_BL:
	case BOTH_T2__L_BR:
	case BOTH_T2_BL_BR:
	case BOTH_T2_BL__R:
		//level 3
	case BOTH_T3_BR__L:
	case BOTH_T3_BR_BL:
	case BOTH_T3__R_BL:
	case BOTH_T3__L_BR:
	case BOTH_T3_BL_BR:
	case BOTH_T3_BL__R:
		//level 4
	case BOTH_T4_BR__L:
	case BOTH_T4_BR_BL:
	case BOTH_T4__R_BL:
	case BOTH_T4__L_BR:
	case BOTH_T4_BL_BR:
	case BOTH_T4_BL__R:
		//level 5
	case BOTH_T5_BR_BL:
	case BOTH_T5__R__L:
	case BOTH_T5__R_BL:
	case BOTH_T5_TR_BL:
	case BOTH_T5_BR_TL:
	case BOTH_T5_BR__L:
	case BOTH_T5_TL_BR:
	case BOTH_T5__L_BR:
	case BOTH_T5__L__R:
	case BOTH_T5_BL_BR:
	case BOTH_T5_BL__R:
	case BOTH_T5_BL_TR:
		//level 6
	case BOTH_T6_BR_TL:
	case BOTH_T6__R_TL:
	case BOTH_T6__R__L:
	case BOTH_T6__R_BL:
	case BOTH_T6_TR_TL:
	case BOTH_T6_TR__L:
	case BOTH_T6_TR_BL:
	case BOTH_T6_T__TL:
	case BOTH_T6_T__BL:
	case BOTH_T6_TL_BR:
	case BOTH_T6__L_BR:
	case BOTH_T6__L__R:
	case BOTH_T6_TL__R:
	case BOTH_T6_TL_TR:
	case BOTH_T6__L_TR:
	case BOTH_T6__L_T_:
	case BOTH_T6_BL_T_:
	case BOTH_T6_BR__L:
	case BOTH_T6_BR_BL:
	case BOTH_T6_BL_BR:
	case BOTH_T6_BL__R:
	case BOTH_T6_BL_TR:
		//level 7
	case BOTH_T7_BR_TL:
	case BOTH_T7_BR__L:
	case BOTH_T7_BR_BL:
	case BOTH_T7__R__L:
	case BOTH_T7__R_BL:
	case BOTH_T7_TR__L:
	case BOTH_T7_T___R:
	case BOTH_T7_TL_BR:
	case BOTH_T7__L_BR:
	case BOTH_T7__L__R:
	case BOTH_T7_BL_BR:
	case BOTH_T7_BL__R:
	case BOTH_T7_BL_TR:
	case BOTH_T7_TL_TR:
	case BOTH_T7_T__BR:
	case BOTH_T7__L_TR:
	case BOTH_V7_BL_S7:
		//special
		//case BOTH_A2_STABBACK1:
	case BOTH_ATTACK_BACK:
	case BOTH_CROUCHATTACKBACK1:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_JUMPFLIPSLASHDOWN1:
	case BOTH_JUMPFLIPSTABDOWN:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SpinningAnim(const int anim)
{
	return PM_SpinningSaberAnim(anim);
}

static void PM_AnglesForSlope(const float yaw, const vec3_t slope, vec3_t angles)
{
	vec3_t nvf, ovf, ovr, new_angles;

	VectorSet(angles, 0, yaw, 0);
	AngleVectors(angles, ovf, ovr, nullptr);

	vectoangles(slope, new_angles);
	const float pitch = new_angles[PITCH] + 90;
	new_angles[ROLL] = new_angles[PITCH] = 0;

	AngleVectors(new_angles, nvf, nullptr, nullptr);

	float mod = DotProduct(nvf, ovr);

	if (mod < 0)
		mod = -1;
	else
		mod = 1;

	const float dot = DotProduct(nvf, ovf);

	angles[YAW] = 0;
	angles[PITCH] = dot * pitch;
	angles[ROLL] = (1 - Q_fabs(dot)) * pitch * mod;
}

static void PM_FootSlopeTrace(float* p_diff, float* p_interval)
{
	vec3_t foot_l_org, foot_r_org, foot_l_bot, foot_r_bot;
	trace_t trace;
	float diff, interval;
	if (pm->gent->client->NPC_class == CLASS_ATST)
	{
		interval = 10;
	}
	else
	{
		interval = 4; //?
	}

	if (pm->gent->footLBolt == -1 || pm->gent->footRBolt == -1)
	{
		if (p_diff != nullptr)
		{
			*p_diff = 0;
		}
		if (p_interval != nullptr)
		{
			*p_interval = interval;
		}
		return;
	}
#if 1
	for (int i = 0; i < 3; i++)
	{
		if (Q_isnan(pm->gent->client->renderInfo.footLPoint[i])
			|| Q_isnan(pm->gent->client->renderInfo.footRPoint[i]))
		{
			if (p_diff != nullptr)
			{
				*p_diff = 0;
			}
			if (p_interval != nullptr)
			{
				*p_interval = interval;
			}
			return;
		}
	}
#else

	//FIXME: these really should have been gotten on the cgame, but I guess sometimes they're not and we end up with qnan numbers!
	mdxaBone_t	bolt_matrix;
	vec3_t		G2Angles = { 0, pm->gent->client->ps.legsYaw, 0 };
	//get the feet
	gi.G2API_GetBoltMatrix(pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footLBolt,
		&bolt_matrix, G2Angles, pm->ps->origin, (cg.time ? cg.time : level.time),
		nullptr, pm->gent->s.modelScale);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, pm->gent->client->renderInfo.footLPoint);

	gi.G2API_GetBoltMatrix(pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footRBolt,
		&bolt_matrix, G2Angles, pm->ps->origin, (cg.time ? cg.time : level.time),
		nullptr, pm->gent->s.modelScale);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, pm->gent->client->renderInfo.footRPoint);
#endif
	//NOTE: on AT-STs, rotating the foot moves this point, so it will wiggle...
	//		we have to do this extra work (more G2 transforms) to stop the wiggle... is it worth it?
	/*
	if ( pm->gent->client->NPC_class == CLASS_ATST )
	{
		mdxaBone_t	bolt_matrix;
		vec3_t		G2Angles = {0, pm->gent->client->ps.legsYaw, 0};
		//get the feet
		gi.G2API_SetBoneAnglesIndex( &pm->gent->ghoul2[0], pm->gent->footLBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, NULL );
		gi.G2API_GetBoltMatrix( pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footLBolt,
				&bolt_matrix, G2Angles, pm->ps->origin, (cg.time?cg.time:level.time),
						NULL, pm->gent->s.modelScale );
		gi.G2API_GiveMeVectorFromMatrix( bolt_matrix, ORIGIN, pm->gent->client->renderInfo.footLPoint );

		gi.G2API_SetBoneAnglesIndex( &pm->gent->ghoul2[0], pm->gent->footRBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, NULL );
		gi.G2API_GetBoltMatrix( pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footRBolt,
						&bolt_matrix, G2Angles, pm->ps->origin, (cg.time?cg.time:level.time),
						NULL, pm->gent->s.modelScale );
		gi.G2API_GiveMeVectorFromMatrix( bolt_matrix, ORIGIN, pm->gent->client->renderInfo.footRPoint );
	}
	*/
	//get these on the cgame and store it, save ourselves a ghoul2 construct skel call
	VectorCopy(pm->gent->client->renderInfo.footLPoint, foot_l_org);
	VectorCopy(pm->gent->client->renderInfo.footRPoint, foot_r_org);

	//step 2: adjust foot tag z height to bottom of bbox+1
	foot_l_org[2] = pm->gent->currentOrigin[2] + pm->gent->mins[2] + 1;
	foot_r_org[2] = pm->gent->currentOrigin[2] + pm->gent->mins[2] + 1;
	VectorSet(foot_l_bot, foot_l_org[0], foot_l_org[1], foot_l_org[2] - interval * 10);
	VectorSet(foot_r_bot, foot_r_org[0], foot_r_org[1], foot_r_org[2] - interval * 10);

	//step 3: trace down from each, find difference
	vec3_t foot_mins, foot_maxs;
	vec3_t foot_l_slope, foot_r_slope;
	if (pm->gent->client->NPC_class == CLASS_ATST)
	{
		VectorSet(foot_mins, -16, -16, 0);
		VectorSet(foot_maxs, 16, 16, 1);
	}
	else
	{
		VectorSet(foot_mins, -3, -3, 0);
		VectorSet(foot_maxs, 3, 3, 1);
	}

	pm->trace(&trace, foot_l_org, foot_mins, foot_maxs, foot_l_bot, pm->ps->client_num, pm->tracemask,
		static_cast<EG2_Collision>(0), 0);
	VectorCopy(trace.endpos, foot_l_bot);
	VectorCopy(trace.plane.normal, foot_l_slope);

	pm->trace(&trace, foot_r_org, foot_mins, foot_maxs, foot_r_bot, pm->ps->client_num, pm->tracemask,
		static_cast<EG2_Collision>(0), 0);
	VectorCopy(trace.endpos, foot_r_bot);
	VectorCopy(trace.plane.normal, foot_r_slope);

	diff = foot_l_bot[2] - foot_r_bot[2];

	//optional step: for atst, tilt the footpads to match the slopes under it...
	if (pm->gent->client->NPC_class == CLASS_ATST)
	{
		vec3_t foot_angles;
		if (!VectorCompare(foot_l_slope, vec3_origin))
		{
			//rotate the ATST's left foot pad to match the slope
			PM_AnglesForSlope(pm->gent->client->renderInfo.legsYaw, foot_l_slope, foot_angles);
			//Hmm... lerp this?
			gi.G2API_SetBoneAnglesIndex(&pm->gent->ghoul2[0], pm->gent->footLBone, foot_angles,
				BONE_ANGLES_POSTMULT,
				POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, nullptr, 0, 0);
		}
		if (!VectorCompare(foot_r_slope, vec3_origin))
		{
			//rotate the ATST's right foot pad to match the slope
			PM_AnglesForSlope(pm->gent->client->renderInfo.legsYaw, foot_r_slope, foot_angles);
			//Hmm... lerp this?
			gi.G2API_SetBoneAnglesIndex(&pm->gent->ghoul2[0], pm->gent->footRBone, foot_angles,
				BONE_ANGLES_POSTMULT,
				POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, nullptr, 0, 0);
		}
	}

	if (p_diff != nullptr)
	{
		*p_diff = diff;
	}
	if (p_interval != nullptr)
	{
		*p_interval = interval;
	}
}

qboolean PM_InSlopeAnim(const int anim)
{
	switch (anim)
	{
	case LEGS_LEFTUP1: //# On a slope with left foot 4 higher than right
	case LEGS_LEFTUP2: //# On a slope with left foot 8 higher than right
	case LEGS_LEFTUP3: //# On a slope with left foot 12 higher than right
	case LEGS_LEFTUP4: //# On a slope with left foot 16 higher than right
	case LEGS_LEFTUP5: //# On a slope with left foot 20 higher than right
	case LEGS_RIGHTUP1: //# On a slope with RIGHT foot 4 higher than left
	case LEGS_RIGHTUP2: //# On a slope with RIGHT foot 8 higher than left
	case LEGS_RIGHTUP3: //# On a slope with RIGHT foot 12 higher than left
	case LEGS_RIGHTUP4: //# On a slope with RIGHT foot 16 higher than left
	case LEGS_RIGHTUP5: //# On a slope with RIGHT foot 20 higher than left
	case LEGS_S1_LUP1:
	case LEGS_S1_LUP2:
	case LEGS_S1_LUP3:
	case LEGS_S1_LUP4:
	case LEGS_S1_LUP5:
	case LEGS_S1_RUP1:
	case LEGS_S1_RUP2:
	case LEGS_S1_RUP3:
	case LEGS_S1_RUP4:
	case LEGS_S1_RUP5:
	case LEGS_S3_LUP1:
	case LEGS_S3_LUP2:
	case LEGS_S3_LUP3:
	case LEGS_S3_LUP4:
	case LEGS_S3_LUP5:
	case LEGS_S3_RUP1:
	case LEGS_S3_RUP2:
	case LEGS_S3_RUP3:
	case LEGS_S3_RUP4:
	case LEGS_S3_RUP5:
	case LEGS_S4_LUP1:
	case LEGS_S4_LUP2:
	case LEGS_S4_LUP3:
	case LEGS_S4_LUP4:
	case LEGS_S4_LUP5:
	case LEGS_S4_RUP1:
	case LEGS_S4_RUP2:
	case LEGS_S4_RUP3:
	case LEGS_S4_RUP4:
	case LEGS_S4_RUP5:
	case LEGS_S5_LUP1:
	case LEGS_S5_LUP2:
	case LEGS_S5_LUP3:
	case LEGS_S5_LUP4:
	case LEGS_S5_LUP5:
	case LEGS_S5_RUP1:
	case LEGS_S5_RUP2:
	case LEGS_S5_RUP3:
	case LEGS_S5_RUP4:
	case LEGS_S5_RUP5:
	case LEGS_S6_LUP1:
	case LEGS_S6_LUP2:
	case LEGS_S6_LUP3:
	case LEGS_S6_LUP4:
	case LEGS_S6_LUP5:
	case LEGS_S6_RUP1:
	case LEGS_S6_RUP2:
	case LEGS_S6_RUP3:
	case LEGS_S6_RUP4:
	case LEGS_S6_RUP5:
	case LEGS_S7_LUP1:
	case LEGS_S7_LUP2:
	case LEGS_S7_LUP3:
	case LEGS_S7_LUP4:
	case LEGS_S7_LUP5:
	case LEGS_S7_RUP1:
	case LEGS_S7_RUP2:
	case LEGS_S7_RUP3:
	case LEGS_S7_RUP4:
	case LEGS_S7_RUP5:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberStanceAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_STAND1: //not really a saberstance anim, actually... "saber off" stance
	case BOTH_STAND2: //single-saber, medium style
	case BOTH_STAND2_JKA:
	case BOTH_SABERFAST_STANCE: //single-saber, fast style
	case BOTH_STAND_BLOCKING_ON:
	case BOTH_STAND_BLOCKING_ON_DUAL:
	case BOTH_STAND_BLOCKING_ON_STAFF:
		//
	case BOTH_STAND_BLOCKING_ON_FORWARD:
	case BOTH_STAND_BLOCKING_ON_BACK:
	case BOTH_STAND_BLOCKING_ON_RIGHT:
	case BOTH_STAND_BLOCKING_ON_LEFT:
	case BOTH_STAND_BLOCKING_ON_DUAL_FORWARD:
	case BOTH_STAND_BLOCKING_ON_DUAL_BACK:
	case BOTH_STAND_BLOCKING_ON_DUAL_RIGHT:
	case BOTH_STAND_BLOCKING_ON_DUAL_LEFT:
	case BOTH_STAND_BLOCKING_ON_STAFF_FORWARD:
	case BOTH_STAND_BLOCKING_ON_STAFF_BACK:
	case BOTH_STAND_BLOCKING_ON_STAFF_RIGHT:
	case BOTH_STAND_BLOCKING_ON_STAFF_LEFT:
		//
	case BOTH_SABERFAST_STANCE_JKA:
	case BOTH_SABERSLOW_STANCE: //single-saber, strong style
	case BOTH_SABERSLOW_STANCE_JKA:
	case BOTH_SABERSTAFF_STANCE: //saber staff style
	case BOTH_SABERSTAFF_STANCE_JKA: //saber staff style
	case BOTH_SABERSTAFF_WALK_STANCE: //saber staff style
	case BOTH_SABERDUAL_STANCE: //dual saber style
	case BOTH_SABERDUAL_STANCE_JKA: //dual saber style
	case BOTH_SABERTAVION_STANCE: //tavion saberstyle
	case BOTH_SABERTAVION_STANCE_JKA: //tavion saberstyle
	case BOTH_SABERDESANN_STANCE: //desann saber style
	case BOTH_SABERDESANN_STANCE_JKA: //desann saber style
	case BOTH_STANDYODA_STICK:
	case BOTH_STAND_SABER_ON:
	case BOTH_STAND_SABER_ON_DUELS:
	case BOTH_STAND_SABER_ON_STAFF:
	case BOTH_STAND9:
	case BOTH_STAND_SABER_ON_IDLE:
	case BOTH_STAND_SABER_ON_IDLE_DUELS:
	case BOTH_STAND_SABER_ON_IDLE_STAFF:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberDrawPutawayAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_STAND1TO2:
	case BOTH_STAND2TO1:
	case BOTH_S1_S7:
	case BOTH_S1_S7_AMD:
	case BOTH_S7_S1:
	case BOTH_S1_S6:
	case BOTH_S6_S1:
	case BOTH_SABER_IGNITION_JFA:
		return qtrue;
	default:;
	}
	return qfalse;
}

constexpr auto SLOPE_RECALC_INT = 100;
extern qboolean g_standard_humanoid(gentity_t* self);

static qboolean PM_AdjustStandAnimForSlope()
{
	if (!pm->gent || !pm->gent->client)
	{
		return qfalse;
	}
	if (pm->gent->client->NPC_class != CLASS_ATST
		&& (!pm->gent || !g_standard_humanoid(pm->gent)))
	{
		//only ATST and player does this
		return qfalse;
	}
	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& !BG_AllowThirdPersonSpecialMove(pm->ps))
	{
		//first person doesn't do this
		return qfalse;
	}
	if (pm->gent->footLBolt == -1 || pm->gent->footRBolt == -1)
	{
		//need these bolts!
		return qfalse;
	}
	//step 1: find the 2 foot tags
	float diff;
	float interval;
	PM_FootSlopeTrace(&diff, &interval);

	//step 4: based on difference, choose one of the left/right slope-match intervals
	int destAnim;
	if (diff >= interval * 5)
	{
		destAnim = LEGS_LEFTUP5;
	}
	else if (diff >= interval * 4)
	{
		destAnim = LEGS_LEFTUP4;
	}
	else if (diff >= interval * 3)
	{
		destAnim = LEGS_LEFTUP3;
	}
	else if (diff >= interval * 2)
	{
		destAnim = LEGS_LEFTUP2;
	}
	else if (diff >= interval)
	{
		destAnim = LEGS_LEFTUP1;
	}
	else if (diff <= interval * -5)
	{
		destAnim = LEGS_RIGHTUP5;
	}
	else if (diff <= interval * -4)
	{
		destAnim = LEGS_RIGHTUP4;
	}
	else if (diff <= interval * -3)
	{
		destAnim = LEGS_RIGHTUP3;
	}
	else if (diff <= interval * -2)
	{
		destAnim = LEGS_RIGHTUP2;
	}
	else if (diff <= interval * -1)
	{
		destAnim = LEGS_RIGHTUP1;
	}
	else
	{
		return qfalse;
	}

	const int legs_anim = pm->ps->legsAnim;
	if (pm->gent->client->NPC_class != CLASS_ATST)
	{
		//adjust for current legs anim
		switch (legs_anim)
		{
		case BOTH_STAND1:
		case BOTH_STAND9:
		case BOTH_STANDYODA_STICK:
		case BOTH_STAND_SABER_ON:
		case BOTH_STAND_SABER_ON_DUELS:
		case BOTH_STAND_SABER_ON_STAFF:
		case LEGS_S1_LUP1:
		case LEGS_S1_LUP2:
		case LEGS_S1_LUP3:
		case LEGS_S1_LUP4:
		case LEGS_S1_LUP5:
		case LEGS_S1_RUP1:
		case LEGS_S1_RUP2:
		case LEGS_S1_RUP3:
		case LEGS_S1_RUP4:
		case LEGS_S1_RUP5:
			destAnim = LEGS_S1_LUP1 + (destAnim - LEGS_LEFTUP1);
			break;
		case BOTH_STAND2:
		case BOTH_STAND2_JKA:
		case BOTH_SABERFAST_STANCE:
		case BOTH_STAND_BLOCKING_ON:
		case BOTH_STAND_BLOCKING_ON_DUAL:
		case BOTH_STAND_BLOCKING_ON_STAFF:
			//
		case BOTH_STAND_BLOCKING_ON_FORWARD:
		case BOTH_STAND_BLOCKING_ON_BACK:
		case BOTH_STAND_BLOCKING_ON_RIGHT:
		case BOTH_STAND_BLOCKING_ON_LEFT:
		case BOTH_STAND_BLOCKING_ON_DUAL_FORWARD:
		case BOTH_STAND_BLOCKING_ON_DUAL_BACK:
		case BOTH_STAND_BLOCKING_ON_DUAL_RIGHT:
		case BOTH_STAND_BLOCKING_ON_DUAL_LEFT:
		case BOTH_STAND_BLOCKING_ON_STAFF_FORWARD:
		case BOTH_STAND_BLOCKING_ON_STAFF_BACK:
		case BOTH_STAND_BLOCKING_ON_STAFF_RIGHT:
		case BOTH_STAND_BLOCKING_ON_STAFF_LEFT:
			//
		case BOTH_SABERFAST_STANCE_JKA:
		case BOTH_SABERSLOW_STANCE:
		case BOTH_SABERSLOW_STANCE_JKA:
		case BOTH_SABERTAVION_STANCE:
		case BOTH_SABERTAVION_STANCE_JKA: //tavion saberstyle
		case BOTH_SABERDESANN_STANCE:
		case BOTH_SABERDESANN_STANCE_JKA: //desann saber style
		case BOTH_STAND_SABER_ON_IDLE:
		case BOTH_STAND_SABER_ON_IDLE_DUELS:
		case BOTH_STAND_SABER_ON_IDLE_STAFF:
		case BOTH_CROUCH1IDLE:
		case BOTH_CROUCH1:
		case LEGS_LEFTUP1: //# On a slope with left foot 4 higher than right
		case LEGS_LEFTUP2: //# On a slope with left foot 8 higher than right
		case LEGS_LEFTUP3: //# On a slope with left foot 12 higher than right
		case LEGS_LEFTUP4: //# On a slope with left foot 16 higher than right
		case LEGS_LEFTUP5: //# On a slope with left foot 20 higher than right
		case LEGS_RIGHTUP1: //# On a slope with RIGHT foot 4 higher than left
		case LEGS_RIGHTUP2: //# On a slope with RIGHT foot 8 higher than left
		case LEGS_RIGHTUP3: //# On a slope with RIGHT foot 12 higher than left
		case LEGS_RIGHTUP4: //# On a slope with RIGHT foot 16 higher than left
		case LEGS_RIGHTUP5: //# On a slope with RIGHT foot 20 higher than left
			//fine
			break;
		case BOTH_STAND3:
		case LEGS_S3_LUP1:
		case LEGS_S3_LUP2:
		case LEGS_S3_LUP3:
		case LEGS_S3_LUP4:
		case LEGS_S3_LUP5:
		case LEGS_S3_RUP1:
		case LEGS_S3_RUP2:
		case LEGS_S3_RUP3:
		case LEGS_S3_RUP4:
		case LEGS_S3_RUP5:
			destAnim = LEGS_S3_LUP1 + (destAnim - LEGS_LEFTUP1);
			break;
		case BOTH_STAND4:
		case LEGS_S4_LUP1:
		case LEGS_S4_LUP2:
		case LEGS_S4_LUP3:
		case LEGS_S4_LUP4:
		case LEGS_S4_LUP5:
		case LEGS_S4_RUP1:
		case LEGS_S4_RUP2:
		case LEGS_S4_RUP3:
		case LEGS_S4_RUP4:
		case LEGS_S4_RUP5:
			destAnim = LEGS_S4_LUP1 + (destAnim - LEGS_LEFTUP1);
			break;
		case BOTH_STAND5:
		case LEGS_S5_LUP1:
		case LEGS_S5_LUP2:
		case LEGS_S5_LUP3:
		case LEGS_S5_LUP4:
		case LEGS_S5_LUP5:
		case LEGS_S5_RUP1:
		case LEGS_S5_RUP2:
		case LEGS_S5_RUP3:
		case LEGS_S5_RUP4:
		case LEGS_S5_RUP5:
			destAnim = LEGS_S5_LUP1 + (destAnim - LEGS_LEFTUP1);
			break;
		case BOTH_SABERDUAL_STANCE:
		case BOTH_SABERDUAL_STANCE_JKA: //dual saber style
		case LEGS_S6_LUP1:
		case LEGS_S6_LUP2:
		case LEGS_S6_LUP3:
		case LEGS_S6_LUP4:
		case LEGS_S6_LUP5:
		case LEGS_S6_RUP1:
		case LEGS_S6_RUP2:
		case LEGS_S6_RUP3:
		case LEGS_S6_RUP4:
		case LEGS_S6_RUP5:
			destAnim = LEGS_S6_LUP1 + (destAnim - LEGS_LEFTUP1);
			break;
		case BOTH_SABERSTAFF_STANCE:
		case BOTH_SABERSTAFF_STANCE_JKA: //saber staff style
		case BOTH_SABERSTAFF_WALK_STANCE: //saber staff style
		case LEGS_S7_LUP1:
		case LEGS_S7_LUP2:
		case LEGS_S7_LUP3:
		case LEGS_S7_LUP4:
		case LEGS_S7_LUP5:
		case LEGS_S7_RUP1:
		case LEGS_S7_RUP2:
		case LEGS_S7_RUP3:
		case LEGS_S7_RUP4:
		case LEGS_S7_RUP5:
			destAnim = LEGS_S7_LUP1 + (destAnim - LEGS_LEFTUP1);
			break;
		case BOTH_STAND6:
		default:
			return qfalse;
		}
	}
	//step 5: based on the chosen interval and the current legsAnim, pick the correct anim
	//step 6: increment/decrement to the dest anim, not instant
	if (legs_anim >= LEGS_LEFTUP1 && legs_anim <= LEGS_LEFTUP5
		|| legs_anim >= LEGS_S1_LUP1 && legs_anim <= LEGS_S1_LUP5
		|| legs_anim >= LEGS_S3_LUP1 && legs_anim <= LEGS_S3_LUP5
		|| legs_anim >= LEGS_S4_LUP1 && legs_anim <= LEGS_S4_LUP5
		|| legs_anim >= LEGS_S5_LUP1 && legs_anim <= LEGS_S5_LUP5
		|| legs_anim >= LEGS_S6_LUP1 && legs_anim <= LEGS_S6_LUP5
		|| legs_anim >= LEGS_S7_LUP1 && legs_anim <= LEGS_S7_LUP5)
	{
		//already in left-side up
		if (destAnim > legs_anim && pm->gent->client->slopeRecalcTime < level.time)
		{
			pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
		}
		else if (destAnim < legs_anim && pm->gent->client->slopeRecalcTime < level.time)
		{
			pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
		}
		else
		{
			destAnim = legs_anim;
		}
	}
	else if (legs_anim >= LEGS_RIGHTUP1 && legs_anim <= LEGS_RIGHTUP5
		|| legs_anim >= LEGS_S1_RUP1 && legs_anim <= LEGS_S1_RUP5
		|| legs_anim >= LEGS_S3_RUP1 && legs_anim <= LEGS_S3_RUP5
		|| legs_anim >= LEGS_S4_RUP1 && legs_anim <= LEGS_S4_RUP5
		|| legs_anim >= LEGS_S5_RUP1 && legs_anim <= LEGS_S5_RUP5
		|| legs_anim >= LEGS_S6_RUP1 && legs_anim <= LEGS_S6_RUP5
		|| legs_anim >= LEGS_S7_RUP1 && legs_anim <= LEGS_S7_RUP5)
	{
		//already in right-side up
		if (destAnim > legs_anim && pm->gent->client->slopeRecalcTime < level.time)
		{
			pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
		}
		else if (destAnim < legs_anim && pm->gent->client->slopeRecalcTime < level.time)
		{
			pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
		}
		else
		{
			destAnim = legs_anim;
		}
	}
	else
	{
		//in a stand of some sort?
		if (pm->gent->client->NPC_class == CLASS_ATST)
		{
			if (legs_anim == BOTH_STAND1 || legs_anim == BOTH_STAND2 || legs_anim == BOTH_CROUCH1IDLE)
			{
				if (destAnim >= LEGS_LEFTUP1 && destAnim <= LEGS_LEFTUP5)
				{
					//going into left side up
					destAnim = LEGS_LEFTUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_RIGHTUP1 && destAnim <= LEGS_RIGHTUP5)
				{
					//going into right side up
					destAnim = LEGS_RIGHTUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
			}
		}
		else
		{
			switch (legs_anim)
			{
			case BOTH_STAND1:
			case BOTH_STAND9:
			case BOTH_STANDYODA_STICK:
			case BOTH_STAND_SABER_ON:
			case BOTH_STAND_SABER_ON_DUELS:
			case BOTH_STAND_SABER_ON_STAFF:
			case BOTH_STAND_SABER_ON_IDLE:
			case BOTH_STAND_SABER_ON_IDLE_DUELS:
			case BOTH_STAND_SABER_ON_IDLE_STAFF:
				if (destAnim >= LEGS_S1_LUP1 && destAnim <= LEGS_S1_LUP5)
				{
					//going into left side up
					destAnim = LEGS_S1_LUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_S1_RUP1 && destAnim <= LEGS_S1_RUP5)
				{
					//going into right side up
					destAnim = LEGS_S1_RUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_STAND2:
			case BOTH_STAND2_JKA:
			case BOTH_SABERFAST_STANCE:
			case BOTH_STAND_BLOCKING_ON:
			case BOTH_STAND_BLOCKING_ON_DUAL:
			case BOTH_STAND_BLOCKING_ON_STAFF:
				//
			case BOTH_STAND_BLOCKING_ON_FORWARD:
			case BOTH_STAND_BLOCKING_ON_BACK:
			case BOTH_STAND_BLOCKING_ON_RIGHT:
			case BOTH_STAND_BLOCKING_ON_LEFT:
			case BOTH_STAND_BLOCKING_ON_DUAL_FORWARD:
			case BOTH_STAND_BLOCKING_ON_DUAL_BACK:
			case BOTH_STAND_BLOCKING_ON_DUAL_RIGHT:
			case BOTH_STAND_BLOCKING_ON_DUAL_LEFT:
			case BOTH_STAND_BLOCKING_ON_STAFF_FORWARD:
			case BOTH_STAND_BLOCKING_ON_STAFF_BACK:
			case BOTH_STAND_BLOCKING_ON_STAFF_RIGHT:
			case BOTH_STAND_BLOCKING_ON_STAFF_LEFT:
				//
			case BOTH_SABERFAST_STANCE_JKA:
			case BOTH_SABERSLOW_STANCE:
			case BOTH_SABERSLOW_STANCE_JKA:
			case BOTH_SABERTAVION_STANCE:
			case BOTH_SABERTAVION_STANCE_JKA: //tavion saberstyle
			case BOTH_SABERDESANN_STANCE:
			case BOTH_SABERDESANN_STANCE_JKA: //desann saber style
			case BOTH_CROUCH1IDLE:
				if (destAnim >= LEGS_LEFTUP1 && destAnim <= LEGS_LEFTUP5)
				{
					//going into left side up
					destAnim = LEGS_LEFTUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_RIGHTUP1 && destAnim <= LEGS_RIGHTUP5)
				{
					//going into right side up
					destAnim = LEGS_RIGHTUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_STAND3:
				if (destAnim >= LEGS_S3_LUP1 && destAnim <= LEGS_S3_LUP5)
				{
					//going into left side up
					destAnim = LEGS_S3_LUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_S3_RUP1 && destAnim <= LEGS_S3_RUP5)
				{
					//going into right side up
					destAnim = LEGS_S3_RUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_STAND4:
				if (destAnim >= LEGS_S4_LUP1 && destAnim <= LEGS_S4_LUP5)
				{
					//going into left side up
					destAnim = LEGS_S4_LUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_S4_RUP1 && destAnim <= LEGS_S4_RUP5)
				{
					//going into right side up
					destAnim = LEGS_S4_RUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_STAND5:
				if (destAnim >= LEGS_S5_LUP1 && destAnim <= LEGS_S5_LUP5)
				{
					//going into left side up
					destAnim = LEGS_S5_LUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_S5_RUP1 && destAnim <= LEGS_S5_RUP5)
				{
					//going into right side up
					destAnim = LEGS_S5_RUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_SABERDUAL_STANCE:
			case BOTH_SABERDUAL_STANCE_JKA: //dual saber style
				if (destAnim >= LEGS_S6_LUP1 && destAnim <= LEGS_S6_LUP5)
				{
					//going into left side up
					destAnim = LEGS_S6_LUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_S6_RUP1 && destAnim <= LEGS_S6_RUP5)
				{
					//going into right side up
					destAnim = LEGS_S6_RUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_SABERSTAFF_STANCE:
			case BOTH_SABERSTAFF_STANCE_JKA: //saber staff style
			case BOTH_SABERSTAFF_WALK_STANCE: //saber staff style
				if (destAnim >= LEGS_S7_LUP1 && destAnim <= LEGS_S7_LUP5)
				{
					//going into left side up
					destAnim = LEGS_S7_LUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else if (destAnim >= LEGS_S7_RUP1 && destAnim <= LEGS_S7_RUP5)
				{
					//going into right side up
					destAnim = LEGS_S7_RUP1;
					pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
				}
				else
				{
					//will never get here
					return qfalse;
				}
				break;
			case BOTH_STAND6:
			default:
				return qfalse;
			}
		}
	}
	//step 7: set the anim
	PM_SetAnim(pm, SETANIM_LEGS, destAnim, SETANIM_FLAG_NORMAL);

	return qtrue;
}

extern qboolean PM_Bobaspecialanim(int anim);
static qhandle_t flyLoopSound = 0;

static void PM_JetPackAnim()
{
	int anim = BOTH_FORCEJUMP1;
	static qboolean registered = qfalse;

	if (!registered)
	{
		flyLoopSound = G_SoundIndex("sound/chars/boba/bf_jetpack_lp.wav");
		registered = qtrue;
	}

	if (!PM_Bobaspecialanim(pm->ps->torsoAnim))
	{
		if (pm->cmd.rightmove > 0)
		{
			if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
				|| pm->ps->forcePowersActive & 1 << FP_GRIP
				|| (pm->cmd.buttons & BUTTON_USE_FORCE
					|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
					|| pm->cmd.buttons & BUTTON_FORCEGRIP))
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				anim = BOTH_FORCEJUMPRIGHT1;
			}
		}
		else if (pm->cmd.rightmove < 0)
		{
			if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
				|| pm->ps->forcePowersActive & 1 << FP_GRIP
				|| (pm->cmd.buttons & BUTTON_USE_FORCE
					|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
					|| pm->cmd.buttons & BUTTON_FORCEGRIP))
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				anim = BOTH_FORCEJUMPLEFT1;
			}
		}
		else if (pm->cmd.forwardmove > 0)
		{
			if (pm->ps->weapon == WP_SABER) //saber out
			{
				if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
					|| pm->ps->forcePowersActive & 1 << FP_GRIP
					|| (pm->cmd.buttons & BUTTON_USE_FORCE
						|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
						|| pm->cmd.buttons & BUTTON_FORCEGRIP))
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				}
				else
				{
					anim = BOTH_FORCEJUMP1;
				}
			}
			else
			{
				if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
					|| pm->ps->forcePowersActive & 1 << FP_GRIP
					|| (pm->cmd.buttons & BUTTON_USE_FORCE
						|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
						|| pm->cmd.buttons & BUTTON_FORCEGRIP))
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				}
				else
				{
					anim = BOTH_FORCEJUMP1;
				}
			}
		}
		else if (pm->cmd.forwardmove < 0)
		{
			if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
				|| pm->ps->forcePowersActive & 1 << FP_GRIP
				|| (pm->cmd.buttons & BUTTON_USE_FORCE
					|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
					|| pm->cmd.buttons & BUTTON_FORCEGRIP))
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				anim = BOTH_INAIRBACK1;
			}
		}
		else
		{
			if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
				|| pm->ps->forcePowersActive & 1 << FP_GRIP
				|| (pm->cmd.buttons & BUTTON_USE_FORCE
					|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
					|| pm->cmd.buttons & BUTTON_FORCEGRIP))
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				anim = BOTH_FORCEJUMP1;
			}
		}
		int parts = SETANIM_BOTH;
		if (pm->ps->weaponTime)
		{
			//FIXME: really only care if we're in a saber attack anim...
			parts = SETANIM_LEGS;
		}

		PM_SetAnim(pm, parts, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		pm->gent->s.loopSound = flyLoopSound;
	}
}

static void PM_SwimFloatAnim()
{
	const int legs_anim = pm->ps->legsAnim;
	//FIXME: no start or stop anims
	if (pm->cmd.forwardmove || pm->cmd.rightmove || pm->cmd.upmove)
	{
		PM_SetAnim(pm, SETANIM_LEGS, BOTH_SWIMFORWARD, SETANIM_FLAG_NORMAL);
	}
	else
	{
		//stopping
		if (legs_anim == BOTH_SWIMFORWARD)
		{
			//I was swimming
			if (!pm->ps->legsAnimTimer)
			{
				PM_SetAnim(pm, SETANIM_LEGS, BOTH_SWIM_IDLE1, SETANIM_FLAG_NORMAL);
			}
		}
		else
		{
			//idle
			if (!(pm->ps->pm_flags & PMF_DUCKED) && pm->cmd.upmove >= 0)
			{
				//not crouching
				PM_SetAnim(pm, SETANIM_LEGS, BOTH_SWIM_IDLE1, SETANIM_FLAG_NORMAL);
			}
		}
	}
}

/*
===============
PM_Footsteps
===============
*/
int PM_ReadyPoseForSaberAnimLevel();

int PM_BlockingPoseForSaberAnimLevelSingleMD();
int PM_BlockingPoseForSaberAnimLevelDualMD();
int PM_BlockingPoseForSaberAnimLevelStaffMD();

int PM_BlockingPoseForSaberAnimLevelDualAMD();
int PM_BlockingPoseForSaberAnimLevelStaffAMD();
int PM_BlockingPoseForSaberAnimLevelSingleAMD();

static void PM_Footsteps()
{
	float bobmove;
	int old;
	qboolean footstep = qfalse;
	qboolean valid_npc = qfalse;
	qboolean flipping = qfalse;
	int set_anim_flags = SETANIM_FLAG_NORMAL;

	const qboolean is_holding_block_button = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;
	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Holding Block Button

	if (pm->gent == nullptr || pm->gent->client == nullptr)
		return;

	if (pm->ps->eFlags & EF_HELD_BY_WAMPA)
	{
		PM_SetAnim(pm, SETANIM_BOTH, BOTH_HANG_IDLE, SETANIM_FLAG_NORMAL);
		if (pm->ps->legsAnim == BOTH_HANG_IDLE)
		{
			if (pm->ps->torsoAnimTimer < 100)
			{
				pm->ps->torsoAnimTimer = 100;
			}
			if (pm->ps->legsAnimTimer < 100)
			{
				pm->ps->legsAnimTimer = 100;
			}
		}
		return;
	}

	if (PM_SpinningSaberAnim(pm->ps->legsAnim) && pm->ps->legsAnimTimer)
	{
		//spinning
		return;
	}
	if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps))
	{
		//in knockdown
		return;
	}
	if (pm->ps->eFlags & EF_FORCE_DRAINED)
	{
		//being drained
		return;
	}
	if (pm->ps->forcePowersActive & 1 << FP_DRAIN
		&& pm->ps->forceDrainEntityNum < ENTITYNUM_WORLD)
	{
		//draining
		return;
	}
	if (pm->gent->NPC && pm->gent->NPC->aiFlags & NPCAI_KNEEL)
	{
		//kneeling
		return;
	}

	if (pm->gent->NPC != nullptr)
	{
		valid_npc = qtrue;
	}

	pm->gent->client->renderInfo.legsFpsMod = 1.0f;

	pm->xyspeed = sqrt(pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1]);

	if (pm->ps->legsAnim == BOTH_FLIP_F ||
		pm->ps->legsAnim == BOTH_FLIP_B ||
		pm->ps->legsAnim == BOTH_FLIP_L ||
		pm->ps->legsAnim == BOTH_FLIP_R ||
		pm->ps->legsAnim == BOTH_ALORA_FLIP_1 ||
		pm->ps->legsAnim == BOTH_ALORA_FLIP_2 ||
		pm->ps->legsAnim == BOTH_ALORA_FLIP_3)
	{
		flipping = qtrue;
	}

	if (pm->ps->groundEntityNum == ENTITYNUM_NONE
		|| pm->watertype & CONTENTS_LADDER
		|| pm->ps->waterHeightLevel >= WHL_TORSO)
	{
		//in air or submerged in water or in ladder
		// airborne leaves position in cycle intact, but doesn't advance
		if (pm->waterlevel > 0)
		{
			if (pm->watertype & CONTENTS_LADDER)
			{
				//the get off ladder transition anim!
				if (pm->ps->velocity[2])
				{
					//going up or down it
					int anim;
					if (pm->ps->velocity[2] > 0)
					{
						anim = BOTH_LADDER_UP1;
					}
					else
					{
						anim = BOTH_LADDER_DWN1;
					}
					PM_SetAnim(pm, SETANIM_LEGS, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					if (pm->waterlevel >= 2) //arms on ladder
					{
						PM_SetAnim(pm, SETANIM_TORSO, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					if (fabs(pm->ps->velocity[2]) > 5)
					{
						bobmove = 0.005 * fabs(pm->ps->velocity[2]); // climbing bobs slow
						if (bobmove > 0.3)
							bobmove = 0.3F;
						goto DoFootSteps;
					}
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_LADDER_IDLE,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					pm->ps->legsAnimTimer += 300;
					if (pm->waterlevel >= 2) //arms on ladder
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_LADDER_IDLE,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
						pm->ps->torsoAnimTimer += 300;
					}
				}
				return;
			}
			if (pm->ps->waterHeightLevel >= WHL_TORSO
				&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()
					|| pm->ps->weapon == WP_SABER || pm->ps->weapon == WP_NONE || pm->ps->weapon == WP_MELEE))
			{
				if (!PM_ForceJumpingUp(pm->gent))
				{
					if (pm->ps->groundEntityNum != ENTITYNUM_NONE && pm->ps->pm_flags & PMF_DUCKED)
					{
						if (!flipping)
						{
							//you can crouch under water if feet are on ground
							if (pm->cmd.forwardmove || pm->cmd.rightmove)
							{
								if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1WALKBACK, set_anim_flags);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1WALK, set_anim_flags);
								}
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1, SETANIM_FLAG_NORMAL);
							}
							return;
						}
					}
					PM_SwimFloatAnim();
					if (pm->ps->legsAnim != BOTH_SWIM_IDLE1)
					{
						//moving
						old = pm->ps->bobCycle;
						bobmove = 0.15f; // swim is a slow cycle
						pm->ps->bobCycle = static_cast<int>(old + bobmove * pml.msec) & 255;

						// if we just crossed a cycle boundary, play an apropriate footstep event
						if ((old + 64 ^ pm->ps->bobCycle + 64) & 128)
						{
							PM_AddEvent(EV_SWIM);
						}
					}
				}
				return;
			}
			//hmm, in water, but not high enough to swim
			//fall through to walk/run/stand
			if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
			{
				//unless in the air
				//NOTE: this is a dupe of the code just below... for when you are not in the water at all
				if (pm->ps->pm_flags & PMF_DUCKED)
				{
					if (!flipping)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1, SETANIM_FLAG_NORMAL);
					}
				}
				else if (pm->ps->gravity <= 0) //FIXME: or just less than normal?
				{
					if (pm->gent
						&& pm->gent->client
						&& (pm->gent->client->NPC_class == CLASS_BOBAFETT
							|| pm->gent->client->NPC_class == CLASS_MANDALORIAN
							|| pm->gent->client->NPC_class == CLASS_JANGO
							|| pm->gent->client->NPC_class == CLASS_JANGODUAL
							|| pm->gent->client->NPC_class == CLASS_ROCKETTROOPER)
						&& pm->gent->client->moveType == MT_FLYSWIM)
					{
						//flying around with jetpack
						//do something else?
						if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
							|| pm->ps->forcePowersActive & 1 << FP_GRIP
							|| (pm->cmd.buttons & BUTTON_USE_FORCE
								|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
								|| pm->cmd.buttons & BUTTON_FORCEGRIP))
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER,
								SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						}
						else
						{
							PM_JetPackAnim();
						}
					}
					else
					{
						PM_SwimFloatAnim();
					}
				}
				return;
			}
		}
		else
		{
			if (pm->ps->pm_flags & PMF_DUCKED)
			{
				if (!flipping)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1, SETANIM_FLAG_NORMAL);
				}
			}
			else if (pm->ps->gravity <= 0) //FIXME: or just less than normal?
			{
				if (pm->gent
					&& pm->gent->client
					&& (pm->gent->client->NPC_class == CLASS_BOBAFETT ||
						pm->gent->client->NPC_class == CLASS_MANDALORIAN ||
						pm->gent->client->NPC_class == CLASS_JANGO ||
						pm->gent->client->NPC_class == CLASS_JANGODUAL ||
						pm->gent->client->NPC_class == CLASS_ROCKETTROOPER)
					&& pm->gent->client->moveType == MT_FLYSWIM)
				{
					//flying around with jetpack
					//do something else?
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING
						|| pm->ps->forcePowersActive & 1 << FP_GRIP
						|| (pm->cmd.buttons & BUTTON_USE_FORCE
							|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
							|| pm->cmd.buttons & BUTTON_FORCEGRIP))
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_FLAMETHROWER, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					else
					{
						PM_JetPackAnim();
					}
				}
				else
				{
					PM_SwimFloatAnim();
				}
			}
			return;
		}
	}

	if (PM_SwimmingAnim(pm->ps->legsAnim) && pm->waterlevel < 2)
	{
		//legs are in swim anim, and not swimming, be sure to override it
		set_anim_flags |= SETANIM_FLAG_OVERRIDE;
	}

	// if not trying to move
	if (!pm->cmd.forwardmove && !pm->cmd.rightmove)
	{
		if (pm->gent && pm->gent->client && (pm->gent->client->NPC_class == CLASS_ATST || pm->gent->client->NPC_class == CLASS_DROIDEKA))
		{
			if (!PM_AdjustStandAnimForSlope())
			{
				PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
			}
		}
		else if (pm->ps->pm_flags & PMF_DUCKED)
		{
			if (!PM_InOnGroundAnim(pm->ps))
			{
				if (!PM_AdjustStandAnimForSlope())
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1, SETANIM_FLAG_NORMAL);
				}
			}
		}
		else
		{
			if (pm->ps->legsAnimTimer && PM_LandingAnim(pm->ps->legsAnim))
			{
				//still in a landing anim, let it play
				return;
			}
			if (pm->ps->legsAnimTimer
				&& (pm->ps->legsAnim == BOTH_THERMAL_READY
					|| pm->ps->legsAnim == BOTH_THERMAL_THROW
					|| pm->ps->legsAnim == BOTH_ATTACK10))
			{
				//still in a thermal anim, let it play
				return;
			}
			qboolean saberInAir = qtrue;
			if (pm->ps->saberInFlight)
			{
				//guiding saber
				if (PM_SaberInBrokenParry(pm->ps->saber_move) || pm->ps->saberBlocked == BLOCKED_PARRY_BROKEN ||
					PM_DodgeAnim(pm->ps->torsoAnim))
				{
					//we're stuck in a broken parry
					saberInAir = qfalse;
				}
				if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0) //player is 0
				{
					//
					if (&g_entities[pm->ps->saberEntityNum] != nullptr && g_entities[pm->ps->saberEntityNum].s.pos.
						trType == TR_STATIONARY)
					{
						//fell to the ground and we're not trying to pull it back
						saberInAir = qfalse;
					}
				}
			}
			if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
			{
				//NOTE: stand1 is with the helmet retracted, stand1to2 is the helmet going into place
				PM_SetAnim(pm, SETANIM_BOTH, BOTH_STAND2, SETANIM_FLAG_NORMAL);
			}
			else if (pm->ps->weapon == WP_SABER
				&& pm->ps->saberInFlight
				&& saberInAir
				&& (!pm->ps->dualSabers || !pm->ps->saber[1].Active()))
			{
				if (!PM_AdjustStandAnimForSlope())
				{
					if (pm->ps->legsAnim != BOTH_LOSE_SABER && !PM_SaberInMassiveBounce(pm->ps->torsoAnim)
						|| !pm->ps->legsAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_SABERPULL, SETANIM_FLAG_NORMAL);
					}
				}
			}
			else if (pm->ps->weapon == WP_SABER
				&& pm->ps->SaberLength() > 0
				&& (pm->ps->SaberActive() || !g_noIgniteTwirl->integer && !active_blocking
					&& !is_holding_block_button && !IsSurrendering(pm->gent))
				&& !pm->ps->saberInFlight
				&& !PM_SaberDrawPutawayAnim(pm->ps->legsAnim))
			{
				if (!PM_AdjustStandAnimForSlope())
				{
					int legs_anim;

					if ((pm->ps->torsoAnim == BOTH_SPINATTACK6
						|| pm->ps->torsoAnim == BOTH_SPINATTACK7
						|| PM_SaberInAttack(pm->ps->saber_move)
						|| PM_SaberInTransitionAny(pm->ps->saber_move))
						&& pm->ps->legsAnim != BOTH_FORCELONGLEAP_LAND
						&& (pm->ps->groundEntityNum == ENTITYNUM_NONE //in air
							|| !PM_JumpingAnim(pm->ps->torsoAnim) && !PM_InAirKickingAnim(pm->ps->torsoAnim)))
						//OR: on ground and torso not in a jump anim
					{
						legs_anim = pm->ps->torsoAnim;
					}
					else
					{
						switch (pm->ps->saber_anim_level)
						{
						case SS_DUAL:
						case SS_STAFF:
						case SS_FAST:
						case SS_TAVION:
						case SS_STRONG:
						case SS_DESANN:
						case SS_MEDIUM:
							if (is_holding_block_button)
							{
								if (pm->ps->saber_anim_level == SS_DUAL)
								{
									if (g_SerenityJediEngineMode->integer == 2)
									{
										legs_anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
									}
									else
									{
										legs_anim = PM_BlockingPoseForSaberAnimLevelDualMD();
									}
								}
								else if (pm->ps->saber_anim_level == SS_STAFF)
								{
									if (g_SerenityJediEngineMode->integer == 2)
									{
										legs_anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
									}
									else
									{
										legs_anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
									}
								}
								else
								{
									if (g_SerenityJediEngineMode->integer == 2)
									{
										legs_anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
									}
									else
									{
										legs_anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
									}
								}
							}
							else
							{
								legs_anim = PM_ReadyPoseForSaberAnimLevel();
							}
							break;
						case SS_NONE:
						default:
							legs_anim = BOTH_STAND2;
							break;
						}
					}
					PM_SetAnim(pm, SETANIM_LEGS, legs_anim, SETANIM_FLAG_NORMAL);
				}
			}
			else if (valid_npc && pm->ps->weapon > WP_SABER && pm->ps->weapon < WP_DET_PACK)
				//Being careful or carrying a 2-handed weapon
			{
				//Squadmates use BOTH_STAND3
				const int old_anim = pm->ps->legsAnim;
				if (old_anim != BOTH_GUARD_LOOKAROUND1 && old_anim != BOTH_GUARD_IDLE1
					&& old_anim != BOTH_STAND2TO4
					&& old_anim != BOTH_STAND4TO2 && old_anim != BOTH_STAND4)
				{
					//Don't auto-override the guard idles
					if (!PM_AdjustStandAnimForSlope())
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND3, SETANIM_FLAG_NORMAL);
					}
				}
			}
			else
			{
				if (!PM_AdjustStandAnimForSlope())
				{
					// FIXME: Do we need this here... The imps stand is 4, not 1...
					if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_IMPERIAL)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND4, SETANIM_FLAG_NORMAL);
					}
					else if (pm->ps->weapon == WP_TUSKEN_STAFF)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR)
						{
							if (pm->gent->count)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND4, SETANIM_FLAG_NORMAL);
							}
							else if (pm->gent->enemy || pm->gent->wait)
							{
								//have an enemy or have had one since we spawned
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND2, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
						}
						else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_WAMPA)
						{
							if (pm->gent->count)
							{
								//holding a victim
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_HOLD_IDLE, SETANIM_FLAG_NORMAL);
							}
							else
							{
								//not holding a victim
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
						}
						else
						{
							if (pm->gent->client->NPC_class == CLASS_YODA)
							{
								if (pm->ps->weapon == WP_MELEE)
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
								}
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
						}
					}
				}
			}
		}

		if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
			pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
		{
			pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
			pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
			g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
		}
		return;
	}

	//maybe call this every frame, even when moving?
	if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ATST)
	{
		PM_FootSlopeTrace(nullptr, nullptr);
	}

	//trying to move laterally
	if (pm->ps->eFlags & EF_IN_ATST
		|| pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR
		//does this catch NPCs, too?
		|| pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_WAMPA)
		//does this catch NPCs, too?
	{
		//atst, Rancor & Wampa, only override turn anims on legs (no torso)
		if (pm->ps->legsAnim == BOTH_TURN_LEFT1 ||
			pm->ps->legsAnim == BOTH_TURN_RIGHT1)
		{
			//moving overrides turning
			set_anim_flags |= SETANIM_FLAG_OVERRIDE;
		}
	}
	else
	{
		//all other NPCs...
		if (PM_InSaberAnim(pm->ps->legsAnim) && !PM_SpinningSaberAnim(pm->ps->legsAnim)
			|| PM_SaberStanceAnim(pm->ps->legsAnim)
			|| PM_SaberDrawPutawayAnim(pm->ps->legsAnim)
			|| pm->ps->legsAnim == BOTH_SPINATTACK6 //not a full-body spin, just spinning the saber
			|| pm->ps->legsAnim == BOTH_SPINATTACK7 //not a full-body spin, just spinning the saber
			|| pm->ps->legsAnim == BOTH_BUTTON_HOLD
			|| pm->ps->legsAnim == BOTH_BUTTON_RELEASE
			|| pm->ps->legsAnim == BOTH_THERMAL_READY
			|| pm->ps->legsAnim == BOTH_THERMAL_THROW
			|| pm->ps->legsAnim == BOTH_ATTACK10
			|| PM_LandingAnim(pm->ps->legsAnim)
			|| PM_PainAnim(pm->ps->legsAnim)
			|| PM_ForceAnim(pm->ps->legsAnim))
		{
			//legs are in a saber anim, and not spinning, be sure to override it
			set_anim_flags |= SETANIM_FLAG_OVERRIDE;
		}
	}

#define OVERRIDE_ROLL_CHECK g_entities[pm->ps->client_num].npc_roll_start

	if (pm->ps->pm_flags & PMF_DUCKED || OVERRIDE_ROLL_CHECK)
	{
		bobmove = 0.5; // ducked characters bob much faster
		if (!PM_InOnGroundAnim(pm->ps) //not on the ground
			&& (!PM_InRollIgnoreTimer(pm->ps) || !pm->ps->legsAnimTimer && pm->cmd.upmove < 0))
			//not in a roll (or you just finished one and you're still holding crouch)
		{
			qboolean rolled = qfalse;

			if (PM_RunningAnim(pm->ps->legsAnim)
				|| pm->ps->legsAnim == BOTH_FORCEHEAL_START
				|| PM_CanRollFromSoulCal(pm->ps))
			{
				//roll!
				rolled = PM_TryRoll();
			}

			if (PM_CrouchAnim(pm->gent->client->ps.legsAnim)
				&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) &&
				(g_SerenityJediEngineMode->integer && pm->cmd.buttons & BUTTON_DASH))
			{
				//roll!
				rolled = pm_try_roll_md();
			}

			if (OVERRIDE_ROLL_CHECK)
			{
				//roll!
				rolled = pm_try_roll_md();
			}

			if (!rolled)
			{
				if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1WALKBACK, set_anim_flags);
				}
				else
				{
					if (pm->gent->client->NPC_class == CLASS_VADER || pm->gent->client->NPC_class == CLASS_DESANN)
					{
						PM_SetAnim(pm, SETANIM_BOTH, BOTH_WALK2, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_CROUCH1WALK, set_anim_flags);
					}
				}
				if (!Q_irand(0, 19))
				{
					//5% chance of making an alert
					AddSoundEvent(pm->gent, pm->ps->origin, 16, AEL_MINOR, qtrue, qtrue);
				}
			}
			else
			{
				//rolling is a little noisy
				AddSoundEvent(pm->gent, pm->ps->origin, 128, AEL_MINOR, qtrue, qtrue);
			}
		}
		// ducked characters never play footsteps

		if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
			pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
		{
			pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
			pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
			g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
		}
	}
	else if (pm->ps->pm_flags & PMF_BACKWARDS_RUN)
	{
		//Moving backwards
		if (!(pm->cmd.buttons & BUTTON_WALKING))
		{
			//running backwards
			bobmove = 0.4F; // faster speeds bob faster

			if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
			{
				if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK2, set_anim_flags);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK1, set_anim_flags);
				}
			}
			else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR)
			{
				//no run anim
				PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
			}
			else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ATST)
			{
				if (pm->ps->legsAnim != BOTH_RUNBACK1)
				{
					if (pm->ps->legsAnim != BOTH_RUN1START)
					{
						//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1START, set_anim_flags | SETANIM_FLAG_HOLD);
					}
					else if (!pm->ps->legsAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK1, set_anim_flags);
					}
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK1, set_anim_flags);
				}
			}
			else if (!in_camera && pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_DROIDEKA)
			{
				if (pm->ps->legsAnim != BOTH_RUNBACK1)
				{
					if (pm->ps->legsAnim != BOTH_RUN1START)
					{
						//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1START, set_anim_flags | SETANIM_FLAG_HOLD);

						if (pm->ps->powerups[PW_GALAK_SHIELD] || pm->gent->flags & FL_SHIELDED)
						{
							TurnBarrierOff(pm->gent);
						}
					}
					else if (!pm->ps->legsAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK1, set_anim_flags);
					}
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK1, set_anim_flags);
				}
			}
			else
			{
				if (pm->ps->weapon == WP_BLASTER_PISTOL ||
					pm->ps->weapon == WP_BRYAR_PISTOL ||
					pm->ps->weapon == WP_DUAL_PISTOL ||
					pm->ps->weapon == WP_DUAL_CLONEPISTOL ||
					pm->ps->weapon == WP_REY ||
					pm->ps->weapon == WP_CLONEPISTOL ||
					pm->ps->weapon == WP_REBELBLASTER ||
					pm->ps->weapon == WP_JANGO ||
					pm->ps->weapon == WP_STUN_BATON)
				{
					if ((pm->gent->client->NPC_class == CLASS_REBORN)
						&& pm->ps->weapon == WP_BLASTER_PISTOL || pm->gent && pm->gent->weaponModel[1] > 0) //they do this themselves
					{
						//dual blaster pistols
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK_DUALPISTOL, set_anim_flags);
					}
					else if ((pm->gent->client->NPC_class == CLASS_CALONORD)
						&& pm->ps->weapon == WP_REBELBLASTER || pm->gent && pm->gent->weaponModel[1] > 0) //they do this themselves
					{
						//dual blaster pistols
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK_DUALPISTOL, set_anim_flags);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK_PISTOL, set_anim_flags);
					}
				}
				else if (pm->ps->weapon == WP_DISRUPTOR ||
					pm->ps->weapon == WP_BLASTER ||
					pm->ps->weapon == WP_BATTLEDROID ||
					pm->ps->weapon == WP_THEFIRSTORDER ||
					pm->ps->weapon == WP_CLONECARBINE ||
					pm->ps->weapon == WP_CLONERIFLE ||
					pm->ps->weapon == WP_CLONECOMMANDO ||
					pm->ps->weapon == WP_BOBA ||
					pm->ps->weapon == WP_REBELRIFLE ||
					pm->ps->weapon == WP_REPEATER)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK_BLASTER, set_anim_flags);
				}
				else if (pm->ps->weapon == WP_CONCUSSION ||
					pm->ps->weapon == WP_ROCKET_LAUNCHER ||
					pm->ps->weapon == WP_BOWCASTER ||
					pm->ps->weapon == WP_FLECHETTE ||
					pm->ps->weapon == WP_DEMP2 ||
					pm->ps->weapon == WP_Z6_ROTARY_CANNON)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK_HEAVY, set_anim_flags);
				}
				else if (pm->ps->weapon == WP_THERMAL ||
					pm->ps->weapon == WP_DET_PACK ||
					pm->ps->weapon == WP_TRIP_MINE)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK_GRENADE, set_anim_flags);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUNBACK1, set_anim_flags);
				}
			}
			footstep = qtrue;
		}
		else
		{
			//walking backwards
			bobmove = 0.3F; // faster speeds bob faster
			if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
			{
				if (pm->ps->saber_anim_level == SS_DUAL)
				{
					if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_DUAL, set_anim_flags);
					}
				}
				else if (pm->ps->saber_anim_level == SS_STAFF)
				{
					if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
					}
					else
					{
						if (g_SerenityJediEngineMode->integer)
						{
							if (is_holding_block_button)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_STAFF, set_anim_flags);
							}
							else
							{
								if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()) //npc
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
								}
								else
								{
									if (pm->gent->client->NPC_class == CLASS_GALEN)
									{
										PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_STAFF, set_anim_flags);
									}
									else
									{
										PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
									}
								}
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_STAFF, set_anim_flags);
						}
					}
				}
				else
				{
					if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK2, set_anim_flags);
					}
				}
			}
			else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ATST)
			{
				if (pm->ps->legsAnim != BOTH_WALKBACK1)
				{
					if (pm->ps->legsAnim != BOTH_RUN1STOP && pm->ps->legsAnim == BOTH_RUNBACK1)
					{
						//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1STOP, set_anim_flags | SETANIM_FLAG_HOLD);
					}
					else if (!pm->ps->legsAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
					}
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
				}
			}
			else if (!in_camera && pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_DROIDEKA)
			{
				if (pm->ps->legsAnim != BOTH_WALKBACK1)
				{
					if (pm->ps->legsAnim != BOTH_RUN1STOP && pm->ps->legsAnim == BOTH_RUNBACK1)
					{
						//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1STOP, set_anim_flags | SETANIM_FLAG_HOLD);
					}
					else if (!pm->ps->legsAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
					}
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
				}
			}
			else
			{
				if (pm->ps->weapon == WP_BLASTER_PISTOL ||
					pm->ps->weapon == WP_BRYAR_PISTOL ||
					pm->ps->weapon == WP_DUAL_PISTOL ||
					pm->ps->weapon == WP_DUAL_CLONEPISTOL ||
					pm->ps->weapon == WP_REY ||
					pm->ps->weapon == WP_CLONEPISTOL ||
					pm->ps->weapon == WP_REBELBLASTER ||
					pm->ps->weapon == WP_JANGO ||
					pm->ps->weapon == WP_STUN_BATON)
				{
					if ((pm->gent->client->NPC_class == CLASS_REBORN)
						&& pm->ps->weapon == WP_BLASTER_PISTOL || pm->gent && pm->gent->weaponModel[1] > 0) //they do this themselves
					{
						//dual blaster pistols
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_DUALPISTOL, set_anim_flags);
					}
					else if ((pm->gent->client->NPC_class == CLASS_CALONORD)
						&& pm->ps->weapon == WP_REBELBLASTER || pm->gent && pm->gent->weaponModel[1] > 0) //they do this themselves
					{
						//dual blaster pistols
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_DUALPISTOL, set_anim_flags);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_PISTOL, set_anim_flags);
					}
				}
				else if (pm->ps->weapon == WP_DISRUPTOR ||
					pm->ps->weapon == WP_BLASTER ||
					pm->ps->weapon == WP_BATTLEDROID ||
					pm->ps->weapon == WP_THEFIRSTORDER ||
					pm->ps->weapon == WP_CLONECARBINE ||
					pm->ps->weapon == WP_CLONERIFLE ||
					pm->ps->weapon == WP_CLONECOMMANDO ||
					pm->ps->weapon == WP_BOBA ||
					pm->ps->weapon == WP_REBELRIFLE ||
					pm->ps->weapon == WP_REPEATER)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_BLASTER, set_anim_flags);
				}
				else if (pm->ps->weapon == WP_CONCUSSION ||
					pm->ps->weapon == WP_ROCKET_LAUNCHER ||
					pm->ps->weapon == WP_BOWCASTER ||
					pm->ps->weapon == WP_FLECHETTE ||
					pm->ps->weapon == WP_DEMP2 ||
					pm->ps->weapon == WP_Z6_ROTARY_CANNON)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_HEAVY, set_anim_flags);
				}
				else if (pm->ps->weapon == WP_THERMAL ||
					pm->ps->weapon == WP_DET_PACK ||
					pm->ps->weapon == WP_TRIP_MINE)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK_GRENADE, set_anim_flags);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALKBACK1, set_anim_flags);
				}
			}
			if (!Q_irand(0, 9))
			{
				//10% chance of a small alert, mainly for the sand_creature
				AddSoundEvent(pm->gent, pm->ps->origin, 16, AEL_MINOR, qtrue, qtrue);
			}
		}

		if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
			pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
		{
			pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
			pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
			g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
		}
	}
	else
	{
		if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
		{
			bobmove = 0.3F; // walking bobs slow
			if (pm->ps->weapon == WP_NONE)
			{
				//helmet retracted
				PM_SetAnim(pm, SETANIM_BOTH, BOTH_WALK1, SETANIM_FLAG_NORMAL);
			}
			else
			{
				//helmet in place
				PM_SetAnim(pm, SETANIM_BOTH, BOTH_WALK2, SETANIM_FLAG_NORMAL);
			}
			AddSoundEvent(pm->gent, pm->ps->origin, 128, AEL_SUSPICIOUS, qtrue, qtrue);
		}
		else
		{
			if (!(pm->cmd.buttons & BUTTON_WALKING))
			{
				//running //sprinting
				bobmove = 0.4F; // faster speeds bob faster

				if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
				{
					if (is_holding_block_button && pm->ps->sprintFuel > 15)
					{
						if (pm->ps->saber_anim_level == SS_DUAL)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_DUAL_LIGHTSABER, set_anim_flags);
						}
						else if (pm->ps->saber_anim_level == SS_STAFF)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_STAFF_LIGHTSABER, set_anim_flags);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_SINGLE_LIGHTSABER, set_anim_flags);
						}

						if (!(pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING))
						{
							pm->ps->PlayerEffectFlags |= 1 << PEF_SPRINTING;
							g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
							if (pm->ps->sprintFuel < 17) // single sprint here
							{
								pm->ps->sprintFuel -= 10;
							}
						}
					}
					else
					{
						if (pm->ps->saber_anim_level == SS_DUAL)
						{
							if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN_DUAL, set_anim_flags);
							}
						}
						else if (pm->ps->saber_anim_level == SS_STAFF)
						{
							if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN_STAFF, set_anim_flags);
							}
						}
						else
						{
							if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN2, set_anim_flags);
							}
						}

						if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING)
						{
							pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
							g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
						}
					}
				} // NON SABER WEAPONS RUNNING
				else if (pm->ps->weapon == WP_MELEE || pm->ps->weapon == WP_NONE ||
					pm->ps->weapon == WP_SABER && !pm->ps->SaberActive())
				{
					if (pm->cmd.buttons & BUTTON_BLOCK && pm->ps->sprintFuel > 15)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_SINGLE_LIGHTSABER, set_anim_flags);

						if (!(pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING))
						{
							pm->ps->PlayerEffectFlags |= 1 << PEF_SPRINTING;
							g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
							if (pm->ps->sprintFuel < 17) // single sprint here
							{
								pm->ps->sprintFuel -= 10;
							}
						}
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);

						if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
							pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
						{
							pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
							pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
							g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
						}
					}
				}
				else
				{
					if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_JAWA)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN4, set_anim_flags);
					} // NON SABER WEAPONS RUNNING
					else if (pm->ps->weapon == WP_BLASTER_PISTOL ||
						pm->ps->weapon == WP_BRYAR_PISTOL ||
						pm->ps->weapon == WP_DUAL_PISTOL ||
						pm->ps->weapon == WP_DUAL_CLONEPISTOL ||
						pm->ps->weapon == WP_REY ||
						pm->ps->weapon == WP_CLONEPISTOL ||
						pm->ps->weapon == WP_REBELBLASTER ||
						pm->ps->weapon == WP_JANGO ||
						pm->ps->weapon == WP_STUN_BATON ||
						pm->ps->weapon == WP_SBD_BLASTER)
					{
						if (pm->gent && pm->gent->weaponModel[1] > 0) //they do this themselves
						{
							//dual blaster pistols
							if (pm->cmd.buttons & BUTTON_BLOCK && pm->ps->sprintFuel > 15)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_DOUBLE_PISTOL, set_anim_flags);

								if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING)
								{
									pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
								}
								if (!(pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
								{
									pm->ps->PlayerEffectFlags |= 1 << PEF_WEAPONSPRINTING;
									g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
									if (pm->ps->sprintFuel < 17) // single sprint here
									{
										pm->ps->sprintFuel -= 10;
									}
								}
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_JOG_DOUBLE_PISTOL, set_anim_flags);

								if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
									pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
								{
									pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
									pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
									g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
								}
							}
						}
						else
						{
							if (pm->cmd.buttons & BUTTON_BLOCK && pm->ps->sprintFuel > 15)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_PISTOL, set_anim_flags);

								if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING)
								{
									pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
								}
								if (!(pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
								{
									pm->ps->PlayerEffectFlags |= 1 << PEF_WEAPONSPRINTING;
									g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
									if (pm->ps->sprintFuel < 17) // single sprint here
									{
										pm->ps->sprintFuel -= 10;
									}
								}
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_JOG_PISTOL, set_anim_flags);

								if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
									pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
								{
									pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
									pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
									g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
								}
							}
						}
					}
					else if (pm->ps->weapon == WP_DISRUPTOR ||
						pm->ps->weapon == WP_BLASTER ||
						pm->ps->weapon == WP_BATTLEDROID ||
						pm->ps->weapon == WP_THEFIRSTORDER ||
						pm->ps->weapon == WP_CLONECARBINE ||
						pm->ps->weapon == WP_CLONERIFLE ||
						pm->ps->weapon == WP_CLONECOMMANDO ||
						pm->ps->weapon == WP_BOBA ||
						pm->ps->weapon == WP_REBELRIFLE ||
						pm->ps->weapon == WP_REPEATER)
					{
						if (pm->cmd.buttons & BUTTON_BLOCK && pm->ps->sprintFuel > 15)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_BLASTER, set_anim_flags);

							if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING)
							{
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
							}
							if (!(pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
							{
								pm->ps->PlayerEffectFlags |= 1 << PEF_WEAPONSPRINTING;
								g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
								if (pm->ps->sprintFuel < 17) // single sprint here
								{
									pm->ps->sprintFuel -= 10;
								}
							}
						}
						else
						{
							if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_JOG_BLASTER, set_anim_flags);
							}

							if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
								pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
							{
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
								g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
							}
						}
					}
					else if (pm->ps->weapon == WP_CONCUSSION ||
						pm->ps->weapon == WP_ROCKET_LAUNCHER ||
						pm->ps->weapon == WP_BOWCASTER ||
						pm->ps->weapon == WP_FLECHETTE ||
						pm->ps->weapon == WP_DEMP2 ||
						pm->ps->weapon == WP_Z6_ROTARY_CANNON)
					{
						if (pm->cmd.buttons & BUTTON_BLOCK && pm->ps->sprintFuel > 15)
						{
							/*if (pm->ps->weapon == WP_Z6_ROTARY_CANNON) {
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_MINIGUN, set_anim_flags);
							}
							else {
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_HEAVY, set_anim_flags);
							}*/
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_HEAVY, set_anim_flags);

							if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING)
							{
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
							}
							if (!(pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
							{
								pm->ps->PlayerEffectFlags |= 1 << PEF_WEAPONSPRINTING;
								g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
								if (pm->ps->sprintFuel < 17) // single sprint here
								{
									pm->ps->sprintFuel -= 10;
								}
							}
						}
						else
						{
							if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
							/*else if (pm->ps->weapon == WP_Z6_ROTARY_CANNON) {
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_JOG_MINIGUN, set_anim_flags);
							}*/
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_JOG_HEAVY, set_anim_flags);
							}

							if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
								pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
							{
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
								g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
							}
						}
					}
					else if (pm->ps->weapon == WP_THERMAL ||
						pm->ps->weapon == WP_DET_PACK ||
						pm->ps->weapon == WP_TRIP_MINE)
					{
						if (pm->cmd.buttons & BUTTON_BLOCK && pm->ps->sprintFuel > 15)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_SPRINT_GRENADE, set_anim_flags);

							if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING)
							{
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
							}
							if (!(pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
							{
								pm->ps->PlayerEffectFlags |= 1 << PEF_WEAPONSPRINTING;
								g_entities[pm->ps->client_num].client->IsSprinting = qtrue;
								if (pm->ps->sprintFuel < 17) // single sprint here
								{
									pm->ps->sprintFuel -= 10;
								}
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_JOG_GRENADE, set_anim_flags);

							if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
								pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
							{
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
								pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
								g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
							}
						}
					}
					else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ATST)
					{
						if (pm->ps->legsAnim != BOTH_RUN1)
						{
							if (pm->ps->legsAnim != BOTH_RUN1START)
							{
								//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1START, set_anim_flags | SETANIM_FLAG_HOLD);
							}
							else if (!pm->ps->legsAnimTimer)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
						}
					}
					else if (!in_camera && pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_DROIDEKA)
					{
						if (pm->ps->legsAnim != BOTH_RUN1)
						{
							if (pm->ps->legsAnim != BOTH_RUN1START)
							{
								//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1START, set_anim_flags | SETANIM_FLAG_HOLD);

								if (pm->ps->powerups[PW_GALAK_SHIELD] || pm->gent->flags & FL_SHIELDED)
								{
									TurnBarrierOff(pm->gent);
								}
							}
							else if (!pm->ps->legsAnimTimer)
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
						}
					}
					else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_WAMPA)
					{
						if (pm->gent->NPC && pm->gent->NPC->stats.runSpeed == 300)
						{
							//full on run, on all fours
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, SETANIM_FLAG_NORMAL);
						}
						else
						{
							//regular, upright run
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN2, SETANIM_FLAG_NORMAL);
						}
					}
					else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR)
					{
						//no run anim
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, set_anim_flags);
					}
					else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_DROIDEKA)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);
					}
					else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_SBD)
					{
						PM_SetAnim(pm, SETANIM_BOTH, BOTH_RUN1, set_anim_flags | SETANIM_FLAG_HOLD);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1, set_anim_flags);

						if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
							pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
						{
							pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
							pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
							g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
						}
					}
				}
				footstep = qtrue;
			}
			else
			{
				//walking forward
				bobmove = 0.3F; // walking bobs slow

				if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
				{
					if (pm->ps->saber_anim_level == SS_DUAL)
					{
						if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (g_SerenityJediEngineMode->integer == 2)
							{
								if (is_holding_block_button)
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DUAL, set_anim_flags);
								}
								else
								{
									if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()) //npc
									{
										if (in_camera) //cosmetics off
										{
											PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DUAL_AMD, set_anim_flags);
										}
										else
										{
											PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DUAL, set_anim_flags);
										}
									}
									else
									{
										if (pm->gent->client->NPC_class == CLASS_GALEN || pm->gent->client->
											NPC_class == CLASS_AHSOKA)
										{
											PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DUAL, set_anim_flags);
										}
										else
										{
											PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DUAL_AMD, set_anim_flags);
										}
									}
								}
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DUAL, set_anim_flags);
							}
						}
					}
					else if (pm->ps->saber_anim_level == SS_STAFF)
					{
						if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (g_SerenityJediEngineMode->integer == 2)
							{
								if (is_holding_block_button)
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_STAFF, set_anim_flags);
								}
								else
								{
									if (pm->gent->client->NPC_class == CLASS_GALEN)
									{
										PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_STAFF, set_anim_flags);
									}
									else
									{
										if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()) //npc
										{
											if (in_camera) //cosmetics off
											{
												PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_STAFF_AMD, set_anim_flags);
											}
											else
											{
												PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_STAFF, set_anim_flags);
											}
										}
										else
										{
											PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_STAFF_AMD, set_anim_flags);
										}
									}
								}
							}
							else
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_STAFF, set_anim_flags);
							}
						}
					}
					else
					{
						if (PM_SaberDrawPutawayAnim(pm->ps->torsoAnim))
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (g_SerenityJediEngineMode->integer == 2) // amd mode
							{
								if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK2, set_anim_flags);
								}
								else
								{
									if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()) //npc
									{
										if (g_RealisticBlockingMode->integer) //cosmetics off
										{
											if (in_camera && pm->gent->client->NPC_class == CLASS_VADER)
												//cosmetics off
											{
												PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1_MDA, set_anim_flags);
											}
											else
											{
												PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK2, set_anim_flags);
											}
										}
										else
										{
											PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1_MDA, set_anim_flags);
											//cosmetics on
										}
									}
									else
									{
										PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1_MDA, SETANIM_FLAG_NORMAL);
									}
								}
							}
							else
							{
								if (in_camera && pm->gent->client->NPC_class == CLASS_VADER) //cosmetics off
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1_MDA, set_anim_flags);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK2, set_anim_flags);
								}
							}
						}
					}
				}
				else if (!in_camera && pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_DROIDEKA)
				{
					if (pm->ps->legsAnim != BOTH_WALK1 && pm->cmd.forwardmove > 0)
					{
						if (pm->ps->legsAnim != BOTH_RUN1STOP && pm->ps->legsAnim == BOTH_RUN1)
						{
							//Hmm, he should really start slow and have to accelerate... also need to do this for stopping
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_RUN1STOP, set_anim_flags | SETANIM_FLAG_HOLD);
						}
						else if (!pm->ps->legsAnimTimer)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, set_anim_flags);
						}
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, set_anim_flags);
					}
				}
				else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_WAMPA)
				{
					if (pm->gent->health <= 50)
					{
						//hurt walk
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK2, SETANIM_FLAG_NORMAL);
					}
					else
					{
						//normal walk
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, SETANIM_FLAG_NORMAL);
					}
				}
				else if (pm->gent->client->NPC_class == CLASS_YODA && pm->ps->weapon == WP_MELEE)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1_STICK, set_anim_flags);
				}
				else if (pm->gent->client->NPC_class == CLASS_SITHLORD && pm->ps->weapon == WP_MELEE)
				{
					PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1_MDA, set_anim_flags);
				}
				else
				{
					if (pm->ps->weapon == WP_BLASTER_PISTOL ||
						pm->ps->weapon == WP_DUAL_PISTOL ||
						pm->ps->weapon == WP_DUAL_CLONEPISTOL ||
						pm->ps->weapon == WP_BRYAR_PISTOL ||
						pm->ps->weapon == WP_REY ||
						pm->ps->weapon == WP_CLONEPISTOL ||
						pm->ps->weapon == WP_REBELBLASTER ||
						pm->ps->weapon == WP_JANGO ||
						pm->ps->weapon == WP_STUN_BATON)
					{
						if (pm->gent && pm->gent->weaponModel[1] > 0) //they do this themselves
						{
							//dual blaster pistols
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_DOUBLE_PISTOL, set_anim_flags);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_PISTOL, set_anim_flags);
						}
					}
					else if (pm->ps->weapon == WP_DISRUPTOR ||
						pm->ps->weapon == WP_DEMP2 ||
						pm->ps->weapon == WP_BLASTER ||
						pm->ps->weapon == WP_BATTLEDROID ||
						pm->ps->weapon == WP_THEFIRSTORDER ||
						pm->ps->weapon == WP_CLONECARBINE ||
						pm->ps->weapon == WP_CLONERIFLE ||
						pm->ps->weapon == WP_CLONECOMMANDO ||
						pm->ps->weapon == WP_REBELRIFLE ||
						pm->ps->weapon == WP_BOBA ||
						pm->ps->weapon == WP_REPEATER ||
						pm->ps->weapon == WP_BOWCASTER ||
						pm->ps->weapon == WP_FLECHETTE)
					{
						if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, set_anim_flags);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_BLASTER, set_anim_flags);
						}
					}
					else if (pm->ps->weapon == WP_CONCUSSION ||
						pm->ps->weapon == WP_ROCKET_LAUNCHER ||
						pm->ps->weapon == WP_Z6_ROTARY_CANNON)
					{
						if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, set_anim_flags);
						}
						/*else if (pm->ps->weapon == WP_Z6_ROTARY_CANNON) {
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_MINIGUN, set_anim_flags);
						}*/
						else
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_HEAVY, set_anim_flags);
						}
					}
					else if (pm->ps->weapon == WP_THERMAL ||
						pm->ps->weapon == WP_DET_PACK ||
						pm->ps->weapon == WP_TRIP_MINE)
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK_GRENADE, set_anim_flags);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_WALK1, set_anim_flags);
					}
				}
				//Enemy NPCs always make footsteps for the benefit of the player
				if (pm->gent
					&& pm->gent->NPC
					&& pm->gent->client
					&& pm->gent->client->playerTeam != TEAM_PLAYER)
				{
					footstep = qtrue;
				}
				else if (!Q_irand(0, 9))
				{
					//10% chance of a small alert, mainly for the sand_creature
					AddSoundEvent(pm->gent, pm->ps->origin, 16, AEL_MINOR, qtrue, qtrue);
				}

				if (pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
					pm->ps->PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING)
				{
					pm->ps->PlayerEffectFlags &= ~(1 << PEF_SPRINTING);
					pm->ps->PlayerEffectFlags &= ~(1 << PEF_WEAPONSPRINTING);
					g_entities[pm->ps->client_num].client->IsSprinting = qfalse;
				}
			}
		}
	}

	if (pm->gent != nullptr)
	{
		if (pm->gent->client->renderInfo.legsFpsMod > 2)
		{
			pm->gent->client->renderInfo.legsFpsMod = 2;
		}
		else if (pm->gent->client->renderInfo.legsFpsMod < 0.5)
		{
			pm->gent->client->renderInfo.legsFpsMod = 0.5;
		}
	}

DoFootSteps:

	// check for footstep / splash sounds
	old = pm->ps->bobCycle;
	pm->ps->bobCycle = static_cast<int>(old + bobmove * pml.msec) & 255;

	// if we just crossed a cycle boundary, play an apropriate footstep event
	if ((old + 64 ^ pm->ps->bobCycle + 64) & 128)
	{
		if (pm->watertype & CONTENTS_LADDER)
		{
			if (!pm->noFootsteps)
			{
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
				{
					// on ladder
					PM_AddEvent(EV_FOOTSTEP_METAL);
				}
			}
			if (pm->gent && pm->gent->s.number == 0)
			{
				AddSoundEvent(pm->gent, pm->ps->origin, 128, AEL_MINOR, qtrue);
			}
		}
		else if (pm->waterlevel == 0)
		{
			// on ground will only play sounds if running
			if (footstep)
			{
				if (pm->gent && pm->gent->s.number == 0)
				{
					vec3_t bottom;

					VectorCopy(pm->ps->origin, bottom);
					bottom[2] += pm->mins[2];
					AddSoundEvent(pm->gent, bottom, 256, AEL_MINOR, qtrue, qtrue);
				}
			}
		}
		else if (pm->waterlevel == 1)
		{
			// splashing
			if (pm->ps->waterHeightLevel >= WHL_KNEES)
			{
				PM_AddEvent(EV_FOOTWADE);
			}
			else
			{
				PM_AddEvent(EV_FOOTSPLASH);
			}
			if (pm->gent && pm->gent->s.number == 0)
			{
				vec3_t bottom;

				VectorCopy(pm->ps->origin, bottom);
				bottom[2] += pm->mins[2];
				AddSoundEvent(pm->gent, pm->ps->origin, 384, AEL_SUSPICIOUS, qfalse, qtrue); //was bottom
				AddSightEvent(pm->gent, pm->ps->origin, 512, AEL_MINOR);
			}
		}
		else if (pm->waterlevel == 2)
		{
			PM_AddEvent(EV_FOOTWADE);
			if (pm->gent && pm->gent->s.number == 0)
			{
				AddSoundEvent(pm->gent, pm->ps->origin, 256, AEL_MINOR, qfalse, qtrue);
				AddSightEvent(pm->gent, pm->ps->origin, 512, AEL_SUSPICIOUS);
			}
		}
		else
		{
			// or no sound when completely underwater...?
			PM_AddEvent(EV_SWIM);
		}
	}
}

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents()
{
	// FIXME?
	qboolean impact_splash = qfalse;

	if (pm->watertype & CONTENTS_LADDER) //fake water for ladder
	{
		return;
	}
	//
	// if just entered a water volume, play a sound
	//
	if (!pml.previous_waterlevel && pm->waterlevel)
	{
		if (pm->watertype & CONTENTS_LAVA)
		{
			PM_AddEvent(EV_LAVA_TOUCH);
		}
		else
		{
			PM_AddEvent(EV_WATER_TOUCH);
		}
		if (pm->gent)
		{
			if (VectorLengthSquared(pm->ps->velocity) > 40000)
			{
				impact_splash = qtrue;
			}

			if (pm->ps->client_num < MAX_CLIENTS)
			{
				AddSoundEvent(pm->gent, pm->ps->origin, 384, AEL_SUSPICIOUS);
				AddSightEvent(pm->gent, pm->ps->origin, 512, AEL_SUSPICIOUS);
			}
		}
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (pml.previous_waterlevel && !pm->waterlevel)
	{
		if (pm->watertype & CONTENTS_LAVA)
		{
			PM_AddEvent(EV_LAVA_LEAVE);
		}
		else
		{
			PM_AddEvent(EV_WATER_LEAVE);
		}
		if (pm->gent && VectorLengthSquared(pm->ps->velocity) > 40000)
		{
			impact_splash = qtrue;
		}
		if (pm->gent && pm->ps->client_num < MAX_CLIENTS)
		{
			AddSoundEvent(pm->gent, pm->ps->origin, 384, AEL_SUSPICIOUS);
			AddSightEvent(pm->gent, pm->ps->origin, 512, AEL_SUSPICIOUS);
		}
	}

	if (impact_splash)
	{
		//play the splash effect
		trace_t tr;
		vec3_t axis[3]{}, angs, start, end;

		VectorSet(angs, 0, pm->gent->currentAngles[YAW], 0);
		AngleVectors(angs, axis[2], axis[1], axis[0]);

		VectorCopy(pm->ps->origin, start);
		VectorCopy(pm->ps->origin, end);

		// FIXME: set start and end better
		start[2] += 10;
		end[2] -= 40;

		gi.trace(&tr, start, vec3_origin, vec3_origin, end, pm->gent->s.number, MASK_WATER,
			static_cast<EG2_Collision>(0), 0);

		if (tr.fraction < 1.0f)
		{
			if (tr.contents & CONTENTS_LAVA)
			{
				G_PlayEffect("env/lava_splash", tr.endpos, axis);
			}
			else if (tr.contents & CONTENTS_SLIME)
			{
				G_PlayEffect("env/acid_splash", tr.endpos, axis);
			}
			else //must be water
			{
				G_PlayEffect("env/water_impact", tr.endpos, axis);
			}
		}
	}

	//
	// check for head just going under water
	//
	if (pml.previous_waterlevel != 3 && pm->waterlevel == 3)
	{
		if (pm->watertype & CONTENTS_LAVA)
		{
			PM_AddEvent(EV_LAVA_UNDER);
		}
		else
		{
			PM_AddEvent(EV_WATER_UNDER);
		}

		if (pm->gent && pm->ps->client_num < MAX_CLIENTS)
		{
			AddSoundEvent(pm->gent, pm->ps->origin, 256, AEL_MINOR);
			AddSightEvent(pm->gent, pm->ps->origin, 384, AEL_MINOR);
		}
	}

	//
	// check for head just coming out of water
	//
	if (pml.previous_waterlevel == 3 && pm->waterlevel != 3)
	{
		if (!pm->gent || !pm->gent->client || pm->gent->client->airOutTime < level.time + 2000)
		{
			//only do this if we were drowning or about to start drowning
			PM_AddEvent(EV_WATER_CLEAR);
		}
		else
		{
			if (pm->watertype & CONTENTS_LAVA)
			{
				PM_AddEvent(EV_LAVA_LEAVE);
			}
			else
			{
				PM_AddEvent(EV_WATER_LEAVE);
			}
		}
		if (pm->gent && pm->ps->client_num < MAX_CLIENTS)
		{
			AddSoundEvent(pm->gent, pm->ps->origin, 256, AEL_MINOR);
			AddSightEvent(pm->gent, pm->ps->origin, 384, AEL_SUSPICIOUS);
		}
	}
}

/*
===============
PM_BeginWeaponChange
===============
*/
extern qboolean PM_AllowedDualPistol();

static void PM_BeginWeaponChange(const int weapon)
{
	if (pm->gent && pm->gent->client && pm->gent->client->pers.enterTime >= level.time - 500)
	{
		//just entered map
		if (weapon == WP_NONE && pm->ps->weapon != weapon)
		{
			//don't switch to weapon none if just entered map
			return;
		}
	}

	if (weapon < WP_NONE || weapon >= WP_NUM_WEAPONS)
	{
		return;
	}

	if (!pm->ps->weapons[weapon])
	{
		return;
	}

	if (pm->ps->weapon == WP_SABER && pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK || pm->ps->ManualBlockingFlags
		& 1 << HOLDINGBLOCKANDATTACK)
	{
		return;
	}

	if (pm->ps->weaponstate == WEAPON_DROPPING || pm->ps->weaponstate == WEAPON_RELOADING)
	{
		return;
	}

	if (pm->ps->weapon != WP_DUAL_PISTOL)
	{
		//Changing weaps, remove dual weaps
		pm->ps->eFlags &= ~EF2_JANGO_DUALS;
	}

	if (pm->ps->weapon != WP_DUAL_CLONEPISTOL)
	{
		//Changing weaps, remove dual weaps
		pm->ps->eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
	}

	if (!PM_AllowedDualPistol() || g_allowdualpistols->integer < 1)
	{
		//Changing weaps, remove dual weaps
		pm->ps->eFlags &= ~EF2_DUAL_PISTOLS;
	}

	if (!Calo_Nord(pm->gent))
	{
		//Changing weaps, remove dual weaps
		pm->ps->eFlags &= ~EF2_DUAL_CALONORD;
	}

	if (cg.time > 0)
	{
		//this way we don't get that annoying change weapon sound every time a map starts
		PM_AddEvent(EV_CHANGE_WEAPON);
	}

	pm->ps->weaponstate = WEAPON_DROPPING;
	pm->ps->weaponTime += 200;

	if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
	{
		if (pm->gent->alt_fire)
		{
			//FIXME: attack delay?
			PM_SetAnim(pm, SETANIM_TORSO, TORSO_DROPWEAP3, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
		else
		{
			//FIXME: attack delay?
			PM_SetAnim(pm, SETANIM_TORSO, TORSO_DROPWEAP1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}
	else
	{
		if (!(pm->ps->eFlags & EF_HELD_BY_WAMPA) && !G_IsRidingVehicle(pm->gent))
		{
			PM_SetAnim(pm, SETANIM_TORSO, TORSO_DROPWEAP1, SETANIM_FLAG_HOLD);
		}
	}

	// turn of any kind of zooming when weapon switching....except the LA Goggles
	// also ignore if we change to WP_NONE..sorta hacky fix for binoculars using WP_SABER
	if (pm->ps->client_num == 0 && cg.weaponSelect != WP_NONE)
	{
		if (cg.zoomMode > 0 && cg.zoomMode < 3)
		{
			cg.zoomMode = 0;
			cg.zoomTime = cg.time;
		}
	}

	if (pm->gent
		&& pm->gent->client
		&& (pm->gent->client->NPC_class == CLASS_ATST || pm->gent->client->NPC_class == CLASS_RANCOR || pm->gent->client->NPC_class == CLASS_DROIDEKA))
	{
		if (pm->ps->client_num < MAX_CLIENTS)
		{
			gi.cvar_set("cg_thirdperson", "1");
		}
	}
	else if (weapon == WP_SABER)
	{
		//going to switch to lightsaber
	}
	else
	{
		if (pm->ps->weapon == WP_SABER)
		{
			//going to switch away from saber
			if (pm->gent)
			{
				if (pm->ps->SaberActive())
				{
					G_SoundOnEnt(pm->gent, CHAN_WEAPON, "sound/weapons/saber/saberoffquick.wav");
				}
			}
			if (!G_IsRidingVehicle(pm->gent))
			{
				if (!g_noIgniteTwirl->integer && !IsSurrendering(pm->gent)) //twirl on
				{
					PM_SetSaberMove(LS_PUTAWAY);
				}
			}
		}
		//put this back in because saberActive isn't being set somewhere else anymore
		pm->ps->SaberDeactivate();
		pm->ps->SetSaberLength(0.0f);
	}
}

/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange()
{
	qboolean true_switch = qtrue;

	if (pm->gent && pm->gent->client && pm->gent->client->pers.enterTime >= level.time - 500)
	{
		//just entered map
		if (pm->cmd.weapon == WP_NONE && pm->ps->weapon != pm->cmd.weapon)
		{
			//don't switch to weapon none if just entered map
			return;
		}
	}

	int weapon = pm->cmd.weapon;

	if (weapon < WP_NONE || weapon >= WP_NUM_WEAPONS)
	{
		weapon = WP_NONE;
	}

	if (!pm->ps->weapons[weapon])
	{
		weapon = WP_NONE;
	}

	if (pm->ps->weapon == weapon)
	{
		true_switch = qfalse;
	}

	if (true_switch && pm->ps->weapon == WP_EMPLACED_GUN && !(pm->ps->eFlags & EF_LOCKED_TO_WEAPON))
	{
		gitem_t* item = FindItemForWeapon(WP_EMPLACED_GUN);
		gentity_t* dropped = Drop_Item(pm->gent, item, 0, qfalse);
		dropped->count = pm->ps->ammo[AMMO_EMPLACED];
		gi.G2API_InitGhoul2Model(dropped->ghoul2, "models/map_objects/hoth/eweb_model.glm",
			G_ModelIndex("models/map_objects/hoth/eweb_model.glm"), NULL_HANDLE, NULL_HANDLE,
			0, 0);
		gi.G2API_SetSurfaceOnOff(&dropped->ghoul2[0], "eweb_cannon", 0x00000002);
		dropped->s.radius = 10;
		dropped->delay = level.time + 1000;
		pm->ps->ammo[AMMO_EMPLACED] = 0;
		pm->ps->weapons[WP_EMPLACED_GUN] = 0;
	}

	pm->ps->weapon = weapon;
	pm->ps->weaponstate = WEAPON_RAISING;
	pm->ps->weaponTime += 250;

	if (pm->gent && pm->gent->client && (pm->gent->client->NPC_class == CLASS_ATST || pm->gent->client->NPC_class == CLASS_DROIDEKA))
	{
		//do nothing
	}
	else if (weapon == WP_EMPLACED_GUN && !(pm->ps->eFlags & EF_LOCKED_TO_WEAPON))
	{
		if (pm->gent)
		{
			// remove the sabre if we had it.
			G_RemoveWeaponModels(pm->gent);
			//holster sabers
			WP_SaberAddHolsteredG2SaberModels(pm->gent);
			G_CreateG2AttachedWeaponModel(pm->gent, "models/map_objects/hoth/eweb_model.glm", pm->gent->handRBolt,
				0);
		}

		if (!(pm->ps->eFlags & EF_HELD_BY_WAMPA))
		{
			if (pm->ps->weapon != WP_THERMAL
				&& pm->ps->weapon != WP_TRIP_MINE
				&& pm->ps->weapon != WP_DET_PACK
				&& !G_IsRidingVehicle(pm->gent))
			{
				PM_SetAnim(pm, SETANIM_TORSO, TORSO_RAISEWEAP1, SETANIM_FLAG_HOLD);
			}
		}

		if (pm->ps->client_num < MAX_CLIENTS
			&& cg_gunAutoFirst.integer
			&& !PM_RidingVehicle()
			&& weapon != WP_NONE)
		{
			gi.cvar_set("cg_thirdperson", "0");
		}
		pm->ps->saber_move = LS_NONE;
		pm->ps->saberBlocking = BLK_NO;
		pm->ps->saberBlocked = BLOCKED_NONE;
	}
	else if (weapon == WP_SABER)
	{
		//turn on the lightsaber
		if (pm->gent)
		{
			// remove gun if we had it.
			G_RemoveWeaponModels(pm->gent);
			//and remove holstered saber models
			G_RemoveHolsterModels(pm->gent);
		}

		if (!pm->ps->saberInFlight || pm->ps->dualSabers)
		{
			//if it's not in flight or lying around, turn it on!
			if (true_switch)
			{
				//actually did switch weapons, turn it on
				if (PM_RidingVehicle())
				{
					//only turn on the first saber's first blade...?
					pm->ps->SaberBladeActivate(0, 0);
				}
				else
				{
					pm->ps->SaberActivate();

					if (pm->gent && pm->ps->dualSabers && pm->gent->weaponModel[1] == -1)
					{
						G_RemoveHolsterModels(pm->gent);
						WP_SaberAddG2SaberModels(pm->gent, qtrue);
					}
				}
				pm->ps->SetSaberLength(0.0f);
			}

			if (pm->gent)
			{
				WP_SaberAddG2SaberModels(pm->gent);
			}
		}
		else
		{
			//
		}

		if (pm->gent)
		{
			WP_SaberInitBladeData(pm->gent);
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
			{
				gi.cvar_set("cg_thirdperson", "1");
			}
		}
		if (true_switch)
		{
			//actually did switch weapons, play anim
			if (!G_IsRidingVehicle(pm->gent) && !IsSurrendering(pm->gent))
			{
				PM_SetSaberMove(LS_DRAW);
			}
		}
	}
	else
	{
		//switched away from weapon
		if (pm->gent)
		{
			// remove the sabre if we had it.
			G_RemoveWeaponModels(pm->gent);
			//holster sabers
			WP_SaberAddHolsteredG2SaberModels(pm->gent);

			if (com_kotor->integer == 1) //playing kotor
			{
				if (weaponData[weapon].altweaponMdl[0])
				{
					//might be NONE, so check if it has a model
					G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].altweaponMdl, pm->gent->handRBolt, 0);

					if (PM_AllowedDualPistol() && g_allowdualpistols->integer == 1 && pm->gent->client->NPC_class != CLASS_JANGODUAL)
					{
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].altweaponMdl, pm->gent->handLBolt, 1);
						pm->ps->eFlags |= EF2_DUAL_PISTOLS;
					}
					else
					{
						pm->ps->eFlags &= ~EF2_DUAL_PISTOLS;
					}

					if (weapon == WP_DUAL_PISTOL && (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_JANGODUAL))
					{
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DUAL_PISTOL].altweaponMdl, pm->gent->handLBolt, 1);
						pm->ps->eFlags |= EF2_JANGO_DUALS;
					}
					else
					{
						pm->ps->eFlags &= ~EF2_JANGO_DUALS;
					}

					if (weapon == WP_DUAL_CLONEPISTOL && (pm->gent && pm->gent->client && Bokatan_Dual_Clone_Pistol(pm->gent)))
					{
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DUAL_CLONEPISTOL].altweaponMdl, pm->gent->handLBolt, 1);
						pm->ps->eFlags |= EF2_DUAL_CLONE_PISTOLS;
					}
					else
					{
						pm->ps->eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
					}

					if (weapon == WP_DROIDEKA && pm->gent->client->NPC_class == CLASS_DROIDEKA)
					{
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DROIDEKA].altweaponMdl, pm->gent->handLBolt, 1);
					}

					if (weapon == WP_REBELBLASTER && (pm->gent && pm->gent->client && Calo_Nord(pm->gent)))
					{
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_REBELBLASTER].altweaponMdl, pm->gent->handLBolt, 1);
						pm->ps->eFlags |= EF2_DUAL_CALONORD;
					}
					else
					{
						pm->ps->eFlags &= ~EF2_DUAL_CALONORD;
					}
				}
			}
			else
			{
				if (pm->gent->friendlyfaction == FACTION_KOTOR)
				{
					if (weaponData[weapon].altweaponMdl[0])
					{
						//might be NONE, so check if it has a model
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].altweaponMdl, pm->gent->handRBolt, 0);

						if (PM_AllowedDualPistol() && g_allowdualpistols->integer == 1 && pm->gent->client->NPC_class != CLASS_JANGODUAL)
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].altweaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_DUAL_PISTOLS;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_DUAL_PISTOLS;
						}

						if (weapon == WP_DUAL_PISTOL && (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_JANGODUAL))
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DUAL_PISTOL].altweaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_JANGO_DUALS;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_JANGO_DUALS;
						}

						if (weapon == WP_DUAL_CLONEPISTOL && (pm->gent && pm->gent->client && Bokatan_Dual_Clone_Pistol(pm->gent)))
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DUAL_CLONEPISTOL].altweaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_DUAL_CLONE_PISTOLS;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
						}

						if (weapon == WP_DROIDEKA && pm->gent->client->NPC_class == CLASS_DROIDEKA)
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DROIDEKA].altweaponMdl, pm->gent->handLBolt, 1);
						}

						if (weapon == WP_REBELBLASTER && (pm->gent && pm->gent->client && Calo_Nord(pm->gent)))
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_REBELBLASTER].altweaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_DUAL_CALONORD;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_DUAL_CALONORD;
						}
					}
				}
				else
				{
					if (weaponData[weapon].weaponMdl[0])
					{
						//might be NONE, so check if it has a model
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].weaponMdl, pm->gent->handRBolt, 0);

						if (PM_AllowedDualPistol() && g_allowdualpistols->integer == 1 && pm->gent->client->NPC_class != CLASS_JANGODUAL)
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].weaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_DUAL_PISTOLS;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_DUAL_PISTOLS;
						}

						if (weapon == WP_DUAL_PISTOL && (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_JANGODUAL))
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DUAL_PISTOL].weaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_JANGO_DUALS;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_JANGO_DUALS;
						}

						if (weapon == WP_DUAL_CLONEPISTOL && (pm->gent && pm->gent->client && Bokatan_Dual_Clone_Pistol(pm->gent)))
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DUAL_CLONEPISTOL].weaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_DUAL_CLONE_PISTOLS;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
						}

						if (weapon == WP_DROIDEKA && pm->gent->client->NPC_class == CLASS_DROIDEKA)
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_DROIDEKA].weaponMdl, pm->gent->handLBolt, 1);
						}
						if (weapon == WP_REBELBLASTER && (pm->gent && pm->gent->client && Calo_Nord(pm->gent)))
						{
							G_CreateG2AttachedWeaponModel(pm->gent, weaponData[WP_REBELBLASTER].weaponMdl, pm->gent->handLBolt, 1);
							pm->ps->eFlags |= EF2_DUAL_CALONORD;
						}
						else
						{
							pm->ps->eFlags &= ~EF2_DUAL_CALONORD;
						}
					}
				}
			}
		}

		if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
		{
			if (pm->gent->alt_fire)
			{
				//FIXME: attack delay?
				PM_SetAnim(pm, SETANIM_TORSO, TORSO_RAISEWEAP3, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				//FIXME: attack delay?
				PM_SetAnim(pm, SETANIM_TORSO, TORSO_RAISEWEAP1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
		else
		{
			if (!(pm->ps->eFlags & EF_HELD_BY_WAMPA))
			{
				if (pm->ps->weapon != WP_THERMAL
					&& pm->ps->weapon != WP_TRIP_MINE
					&& pm->ps->weapon != WP_DET_PACK
					&& !G_IsRidingVehicle(pm->gent))
				{
					PM_SetAnim(pm, SETANIM_TORSO, TORSO_RAISEWEAP1, SETANIM_FLAG_HOLD);
				}
			}
		}

		if (pm->ps->client_num < MAX_CLIENTS
			&& cg_gunAutoFirst.integer
			&& !PM_RidingVehicle()
			&& weapon != WP_NONE)
		{
			gi.cvar_set("cg_thirdperson", "0");
		}
		pm->ps->saber_move = LS_NONE;
		pm->ps->saberBlocking = BLK_NO;
		pm->ps->saberBlocked = BLOCKED_NONE;
	}

	if (weapon != WP_DUAL_PISTOL)
	{
		pm->ps->eFlags &= ~EF2_JANGO_DUALS;
	}

	if (weapon != WP_DUAL_CLONEPISTOL)
	{
		pm->ps->eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
	}

	if (!PM_AllowedDualPistol() || g_allowdualpistols->integer < 1)
	{
		pm->ps->eFlags &= ~EF2_DUAL_PISTOLS;
	}

	if (!Calo_Nord(pm->gent))
	{
		//Changing weaps, remove dual weaps
		pm->ps->eFlags &= ~EF2_DUAL_CALONORD;
	}
}

//This controls saber movement anims //JaceSolaris
static int PM_ReadyPoseForSaberAnimLevelDucked()
{
	int anim;

	if (PM_RidingVehicle())
	{
		return -1;
	}

	switch (pm->ps->saber_anim_level)
	{
	case SS_DUAL:
		anim = BOTH_SABERDUALCROUCH;
		break;
	case SS_STAFF:
		anim = BOTH_SABERSTAFFCROUCH;
		break;
	case SS_FAST:
	case SS_TAVION:
	case SS_STRONG:
	case SS_MEDIUM:
	case SS_DESANN:
		anim = BOTH_SABERSINGLECROUCH;
		break;
	case SS_NONE:
	default:
		anim = BOTH_SABERSINGLECROUCH;
		break;
	}
	return anim;
}

static int PM_ReadyPoseForSaberAnimLevelNPC()
{
	int anim;

	const qboolean activate_npc_block_stance = pm->ps->ManualBlockingFlags & 1 << MBF_NPCBLOCKSTANCE
		? qtrue
		: qfalse; //Active Blocking

	if (PM_RidingVehicle())
	{
		return -1;
	}

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		anim = PM_ReadyPoseForSaberAnimLevelDucked();
	}
	else
	{
		switch (pm->ps->saber_anim_level)
		{
		case SS_DUAL:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_SABERDUAL_STANCE;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON_DUAL;
						}
						else
						{
							anim = BOTH_SABERDUALCROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_SABERDUAL_STANCE;
				}
			}
			else
			{
				anim = BOTH_SABERDUAL_STANCE_JKA;
			}
			break;
		case SS_STAFF:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_SABERSTAFF_STANCE;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON_STAFF;
						}
						else
						{
							anim = BOTH_SABERSTAFFCROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_SABERSTAFF_STANCE;
				}
			}
			else
			{
				anim = BOTH_SABERSTAFF_STANCE_JKA;
			}
			break;
		case SS_FAST:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_SABERFAST_STANCE;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON;
						}
						else
						{
							anim = BOTH_SABERSINGLECROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_SABERFAST_STANCE;
				}
			}
			else
			{
				anim = BOTH_SABERFAST_STANCE_JKA;
			}
			break;
		case SS_TAVION:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_SABERTAVION_STANCE;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON;
						}
						else
						{
							anim = BOTH_SABERSINGLECROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_SABERTAVION_STANCE;
				}
			}
			else
			{
				anim = BOTH_SABERTAVION_STANCE_JKA;
			}
			break;
		case SS_STRONG:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_SABERSLOW_STANCE;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON;
						}
						else
						{
							anim = BOTH_SABERSINGLECROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_SABERSLOW_STANCE;
				}
			}
			else
			{
				anim = BOTH_SABERSLOW_STANCE_JKA;
			}
			break;
		case SS_DESANN:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_SABERDESANN_STANCE;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON;
						}
						else
						{
							anim = BOTH_SABERSINGLECROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_SABERDESANN_STANCE;
				}
			}
			else
			{
				anim = BOTH_SABERDESANN_STANCE_JKA;
			}
			break;
		case SS_MEDIUM:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (g_RealisticBlockingMode->integer) //cosmetics off
				{
					if (in_camera) //cosmetics off
					{
						anim = BOTH_STAND2;
					}
					else
					{
						if (activate_npc_block_stance)
						{
							anim = BOTH_STAND_BLOCKING_ON;
						}
						else
						{
							anim = BOTH_SABERSINGLECROUCH;
						}
					}
				}
				else
				{
					anim = BOTH_STAND2;
				}
			}
			else
			{
				anim = BOTH_STAND2;
			}
			break;
		case SS_NONE:
		default:
			anim = BOTH_STAND2;
			break;
		}
	}

	return anim;
}

int PM_ReadyPoseForSaberAnimLevelAMD();

int PM_ReadyPoseForSaberAnimLevel()
{
	int anim;

	if (PM_RidingVehicle())
	{
		return -1;
	}

	if (pm->ps->client_num && !PM_ControlledByPlayer())
	{
		anim = PM_ReadyPoseForSaberAnimLevelNPC();
	}
	else if (pm->ps->pm_flags & PMF_DUCKED)
	{
		anim = PM_ReadyPoseForSaberAnimLevelDucked();
	}
	else
	{
		switch (pm->ps->saber_anim_level)
		{
		case SS_DUAL:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_SABERDUAL_STANCE_JKA;
			}
			break;
		case SS_STAFF:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_SABERSTAFF_STANCE_JKA;
			}
			break;
		case SS_FAST:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_SABERFAST_STANCE_JKA;
			}
			break;
		case SS_TAVION:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_SABERTAVION_STANCE_JKA;
			}
			break;
		case SS_STRONG:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_SABERSLOW_STANCE_JKA;
			}
			break;
		case SS_DESANN:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_SABERDESANN_STANCE_JKA;
			}
			break;
		case SS_MEDIUM:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_STAND2;
			}
			break;
		case SS_NONE:
		default:
			if (g_SerenityJediEngineMode->integer == 2)
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND9;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_STAND1;
					}
					else
					{
						anim = PM_ReadyPoseForSaberAnimLevelAMD();
						// this is the SABER STYLE block box in wrong position in many cases.// Cosmetic mode ON
					}
				}
			}
			else
			{
				anim = BOTH_STAND2;
			}
			break;
		}
	}
	return anim;
}

int PM_ReadyPoseForSaberAnimLevelAMD()
{
	int anim;

	if (PM_RidingVehicle())
	{
		return -1;
	}

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		anim = PM_ReadyPoseForSaberAnimLevelDucked();
	}
	else
	{
		switch (pm->ps->saber_anim_level)
		{
		case SS_DUAL:
			anim = BOTH_SABERDUAL_STANCE;
			break;
		case SS_STAFF:
			anim = BOTH_SABERSTAFF_STANCE;
			break;
		case SS_FAST:
			anim = BOTH_SABERFAST_STANCE;
			break;
		case SS_TAVION:
			anim = BOTH_SABERTAVION_STANCE;
			break;
		case SS_STRONG:
			anim = BOTH_SABERSLOW_STANCE;
			break;
		case SS_DESANN:
			anim = BOTH_SABERDESANN_STANCE;
			break;
		case SS_MEDIUM:
			anim = BOTH_STAND2;
			break;
		case SS_NONE:
		default:
			anim = BOTH_STAND2;
			break;
		}
	}
	return anim;
}

int PM_BlockingPoseForSaberAnimLevelSingleAMD()
{
	int anim = PM_ReadyPoseForSaberAnimLevel();

	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	const signed char forwardmove = pm->cmd.forwardmove;
	const signed char rightmove = pm->cmd.rightmove;

	if (pm->cmd.upmove <= 0)
	{
		if (forwardmove < 0)
		{
			//walking backwards
			if (rightmove < 0)
			{
				//back left
				if (active_blocking)
				{
					anim = BOTH_P1_S1_TL_MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//Back right
				if (active_blocking)
				{
					anim = BOTH_P1_S1_TR_MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//straight back
				if (active_blocking)
				{
					anim = BOTH_P1_S1_T__MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_BACK; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
		else if (forwardmove > 0)
		{
			//walking forwards
			if (rightmove < 0)
			{
				//forwards left
				if (active_blocking)
				{
					anim = BOTH_P1_S1_BL_MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//forwards right
				if (active_blocking)
				{
					anim = BOTH_P1_S1_BR_MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//Top Block
				if (active_blocking)
				{
					anim = BOTH_P1_S1_T__MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_FORWARD; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
		else
		{
			//STANDING STILL
			if (rightmove < 0)
			{
				//left block
				if (active_blocking)
				{
					anim = BOTH_P1_S1_TL_MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//right block
				if (active_blocking)
				{
					anim = BOTH_P1_S1_TR_MD;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//STANDING STILL NOT MOVING LEFT OR RIGHT
				if (active_blocking) // pressing block + attack
				{
					if (g_entities[pm->ps->client_num].client->IsBlockingLightning)
						// this is for lightning needs to be here as an override (TOP BLOCK)
					{
						anim = BOTH_P1_S1_T__MD;
						// this is for lightning needs to be here as an override (TOP BLOCK)
					}
					else
					{
						if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
						{
							if (pm->cmd.buttons & BUTTON_WALKING)
							{
								anim = BOTH_P1_S1_T_; // top block JKA method
							}
							else
							{
								anim = BOTH_STAND_BLOCKING_ON; // front block new method// Cosmetic mode ON
							}
						}
						else
						{
							anim = BOTH_P1_S1_T__MD; // top block JKA method
						}
					}
				}
				else
				{
					//Pressing block only
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSINGLECROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
	}
	return anim;
}

int PM_BlockingPoseForSaberAnimLevelDualAMD()
{
	int anim = PM_ReadyPoseForSaberAnimLevel();

	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	const signed char forwardmove = pm->cmd.forwardmove;
	const signed char rightmove = pm->cmd.rightmove;

	if (pm->cmd.upmove <= 0)
	{
		if (forwardmove < 0)
		{
			//walking backwards
			if (rightmove < 0)
			{
				//back left
				if (active_blocking)
				{
					anim = BOTH_P6_S6_TL;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//Back right
				if (active_blocking)
				{
					anim = BOTH_P6_S6_TR;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//straight back
				if (active_blocking)
				{
					anim = BOTH_P6_S6_T_;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_BACK; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
		else if (forwardmove > 0)
		{
			//walking forwards
			if (rightmove < 0)
			{
				//forwards left
				if (active_blocking)
				{
					anim = BOTH_P6_S6_BL;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//forwards right
				if (active_blocking)
				{
					anim = BOTH_P6_S6_BR;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//Top Block
				if (active_blocking)
				{
					anim = BOTH_P6_S6_T_;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_FORWARD; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
		else
		{
			//STANDING STILL
			if (rightmove < 0)
			{
				//left block
				if (active_blocking)
				{
					anim = BOTH_P6_S6_TL;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//right block
				if (active_blocking)
				{
					anim = BOTH_P6_S6_TR;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//STANDING STLL NOT MOVING LEFT OR RIGHT
				if (active_blocking) // pressing block + attack
				{
					if (g_entities[pm->ps->client_num].client->IsBlockingLightning)
						// this is for lightning needs to be here as an override (TOP BLOCK)
					{
						anim = BOTH_P6_S6_T__MD;
						// this is for lightning needs to be here as an override (TOP BLOCK)
					}
					else
					{
						if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
						{
							if (pm->cmd.buttons & BUTTON_WALKING)
							{
								anim = BOTH_P6_S6_T_; // top block JKA method
							}
							else
							{
								anim = BOTH_STAND_BLOCKING_ON_DUAL; // front block new method// Cosmetic mode ON
							}
						}
						else
						{
							anim = BOTH_P6_S6_T__MD; // top block JKA method
						}
					}
				}
				else
				{
					//Pressing block only
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERDUALCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_DUAL; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
	}
	return anim;
}

int PM_BlockingPoseForSaberAnimLevelStaffAMD()
{
	int anim = PM_ReadyPoseForSaberAnimLevel();

	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	const signed char forwardmove = pm->cmd.forwardmove;
	const signed char rightmove = pm->cmd.rightmove;

	if (pm->cmd.upmove <= 0)
	{
		if (forwardmove < 0)
		{
			//walking backwards
			if (rightmove < 0)
			{
				//back left
				if (active_blocking)
				{
					anim = BOTH_P7_S7_TL;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//Back right
				if (active_blocking)
				{
					anim = BOTH_P7_S7_TR;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//straight back
				if (active_blocking)
				{
					anim = BOTH_P7_S7_T_;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_BACK; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
		else if (forwardmove > 0)
		{
			//walking forwards
			if (rightmove < 0)
			{
				//forwards left
				if (active_blocking)
				{
					anim = BOTH_P7_S7_BL;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//forwards right
				if (active_blocking)
				{
					anim = BOTH_P7_S7_BR;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//Top Block
				if (active_blocking)
				{
					anim = BOTH_P7_S7_T_;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_FORWARD; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
		else
		{
			//STANDING STILL
			if (rightmove < 0)
			{
				//left block
				if (active_blocking)
				{
					anim = BOTH_P7_S7_TL;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_LEFT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else if (rightmove > 0)
			{
				//right block
				if (active_blocking)
				{
					anim = BOTH_P7_S7_TR;
				}
				else
				{
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF_RIGHT; // front block new method// Cosmetic mode ON
					}
				}
			}
			else
			{
				//STANDING STLL NOT MOVING LEFT OR RIGHT
				if (active_blocking) // pressing block + attack
				{
					if (g_entities[pm->ps->client_num].client->IsBlockingLightning)
						// this is for lightning needs to be here as an override (TOP BLOCK)
					{
						anim = BOTH_P7_S7_T__MD;
						// this is for lightning needs to be here as an override (TOP BLOCK)
					}
					else
					{
						if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
						{
							if (pm->cmd.buttons & BUTTON_WALKING)
							{
								anim = BOTH_P7_S7_T_; // top block JKA method
							}
							else
							{
								anim = BOTH_STAND_BLOCKING_ON_STAFF; // front block new method// Cosmetic mode ON
							}
						}
						else
						{
							anim = BOTH_P7_S7_T__MD; // top block JKA method
						}
					}
				}
				else
				{
					//Pressing block only
					if (g_RealisticBlockingMode->integer) // Cosmetic mode OFF
					{
						anim = BOTH_SABERSTAFFCROUCH;
						// this is the holding block button block box in correct position
					}
					else
					{
						anim = BOTH_STAND_BLOCKING_ON_STAFF; // front block new method// Cosmetic mode ON
					}
				}
			}
		}
	}
	return anim;
}

int PM_BlockingPoseForSaberAnimLevelSingleMD()
{
	int anim = PM_ReadyPoseForSaberAnimLevel();

	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	const signed char forwardmove = pm->cmd.forwardmove;
	const signed char rightmove = pm->cmd.rightmove;

	if (pm->cmd.upmove <= 0)
	{
		if (forwardmove < 0)
		{
			//walking backwards
			if (rightmove < 0)
			{
				//back left
				anim = BOTH_P1_S1_TL;
			}
			else if (rightmove > 0)
			{
				//Back right
				anim = BOTH_P1_S1_TR;
			}
			else
			{
				//straight back
				anim = BOTH_P1_S1_T_;
			}
		}
		else if (forwardmove > 0)
		{
			//walking forwards
			if (rightmove < 0)
			{
				//forwards left
				anim = BOTH_P1_S1_BL;
			}
			else if (rightmove > 0)
			{
				//forwards right
				anim = BOTH_P1_S1_BR;
			}
			else
			{
				//Top Block
				anim = BOTH_P1_S1_T_;
			}
		}
		else
		{
			//STANDING STILL
			if (rightmove < 0)
			{
				//left block
				anim = BOTH_P1_S1_TL;
			}
			else if (rightmove > 0)
			{
				//right block
				anim = BOTH_P1_S1_TR;
			}
			else
			{
				if (active_blocking)
				{
					anim = BOTH_P1_S1_T_;
				}
				else
				{
					anim = BOTH_STAND_BLOCKING_ON;
				}
			}
		}
	}
	return anim;
}

int PM_BlockingPoseForSaberAnimLevelDualMD()
{
	int anim = PM_ReadyPoseForSaberAnimLevel();

	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	const signed char forwardmove = pm->cmd.forwardmove;
	const signed char rightmove = pm->cmd.rightmove;

	if (pm->cmd.upmove <= 0)
	{
		if (forwardmove < 0)
		{
			//walking backwards
			if (rightmove < 0)
			{
				//back left
				anim = BOTH_P6_S6_TL;
			}
			else if (rightmove > 0)
			{
				//Back right
				anim = BOTH_P6_S6_TR;
			}
			else
			{
				//straight back
				anim = BOTH_P6_S6_T_;
			}
		}
		else if (forwardmove > 0)
		{
			//walking forwards
			if (rightmove < 0)
			{
				//forwards left
				anim = BOTH_P6_S6_BL;
			}
			else if (rightmove > 0)
			{
				//forwards right
				anim = BOTH_P6_S6_BR;
			}
			else
			{
				//Top Block
				anim = BOTH_P6_S6_T_;
			}
		}
		else
		{
			//STANDING STILL
			if (rightmove < 0)
			{
				//left block
				anim = BOTH_P6_S6_TL;
			}
			else if (rightmove > 0)
			{
				//right block
				anim = BOTH_P6_S6_TR;
			}
			else
			{
				if (active_blocking)
				{
					anim = BOTH_P6_S6_T_;
				}
				else
				{
					anim = BOTH_STAND_BLOCKING_ON_DUAL;
				}
			}
		}
	}
	return anim;
}

int PM_BlockingPoseForSaberAnimLevelStaffMD()
{
	int anim = PM_ReadyPoseForSaberAnimLevel();

	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	const signed char forwardmove = pm->cmd.forwardmove;
	const signed char rightmove = pm->cmd.rightmove;

	if (pm->cmd.upmove <= 0)
	{
		if (forwardmove < 0)
		{
			//walking backwards
			if (rightmove < 0)
			{
				//back left
				anim = BOTH_P7_S7_TL;
			}
			else if (rightmove > 0)
			{
				//Back right
				anim = BOTH_P7_S7_TR;
			}
			else
			{
				//straight back
				anim = BOTH_P7_S7_T_;
			}
		}
		else if (forwardmove > 0)
		{
			//walking forwards
			if (rightmove < 0)
			{
				//forwards left
				anim = BOTH_P7_S7_BL;
			}
			else if (rightmove > 0)
			{
				//forwards right
				anim = BOTH_P7_S7_BR;
			}
			else
			{
				//Top Block
				anim = BOTH_P7_S7_T_;
			}
		}
		else
		{
			//STANDING STILL
			if (rightmove < 0)
			{
				//left block
				anim = BOTH_P7_S7_TL;
			}
			else if (rightmove > 0)
			{
				//right block
				anim = BOTH_P7_S7_TR;
			}
			else
			{
				if (active_blocking)
				{
					anim = BOTH_P7_S7_T_;
				}
				else
				{
					anim = BOTH_STAND_BLOCKING_ON_STAFF;
				}
			}
		}
	}
	return anim;
}

static qboolean PM_CanDoDualDoubleAttacks()
{
	if (pm->ps->saber[0].saberFlags & SFL_NO_MIRROR_ATTACKS)
	{
		return qfalse;
	}
	if (pm->ps->dualSabers
		&& pm->ps->saber[1].saberFlags & SFL_NO_MIRROR_ATTACKS)
	{
		return qfalse;
	}
	//NOTE: assumes you're using SS_DUAL style and have both sabers on...
	if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		//player
		return qtrue;
	}
	if (pm->gent && pm->gent->NPC && pm->gent->NPC->rank >= Q_irand(RANK_LT_COMM, RANK_CAPTAIN + 2))
	{
		//high-rank NPC
		return qtrue;
	}
	if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ALORA)
	{
		//Alora
		return qtrue;
	}
	return qfalse;
}

void PM_SetJumped(const float height, const qboolean force)
{
	pm->ps->velocity[2] = height;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pm->ps->pm_flags |= PMF_JUMP_HELD;
	pm->ps->pm_flags |= PMF_JUMPING;
	pm->cmd.upmove = 0;

	if (force)
	{
		pm->ps->jumpZStart = pm->ps->origin[2];
		pm->ps->pm_flags |= PMF_SLOW_MO_FALL;
		//start force jump
		pm->ps->forcePowersActive |= 1 << FP_LEVITATION;

		if (g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] <
			FORCE_LEVEL_3)
		{
			//short burst
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
		}
		else
		{
			//holding it
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
		}
	}
	else
	{
		PM_AddEvent(EV_JUMP);
	}
}

//Add Fatigue to a player
void PM_AddBoltBlockFatigue(playerState_t* ps, const int fatigue)
{
	if (g_SerenityJediEngineMode->integer == 2)
	{
		if (ps->blockPoints > fatigue)
		{
			ps->blockPoints -= fatigue;
		}
		else
		{
			//don't have enough so just completely drain BP then.
			ps->blockPoints = 0;
		}
	}
	else
	{
		//For now, all saber attacks cost one FP.
		if (ps->forcePower > fatigue)
		{
			ps->forcePower -= fatigue;
		}
		else
		{
			//don't have enough so just completely drain FP then.
			ps->forcePower = 0;
		}

		if (g_SerenityJediEngineMode->integer)
		{
			//check for fatigued state.
			if (ps->forcePower <= BLOCKPOINTS_FATIGUE)
			{
				//Pop the Fatigued flag
				ps->userInt3 |= 1 << FLAG_FATIGUED;
			}
		}
	}
}

void PM_AddBlockFatigue(playerState_t* ps, const int fatigue)
{
	if (ps->blockPoints > fatigue)
	{
		ps->blockPoints -= fatigue;
	}
	else
	{
		//don't have enough so just completely drain BP then.
		ps->blockPoints = 0;
	}
}

//Add Fatigue to a player
void PM_AddFatigue(playerState_t* ps, const int fatigue)
{
	//For now, all saber attacks cost one FP.
	if (ps->forcePower > fatigue)
	{
		ps->forcePower -= fatigue;
	}
	else
	{
		//don't have enough so just completely drain FP then.
		ps->forcePower = 0;
	}

	if (g_SerenityJediEngineMode->integer)
	{
		//check for fatigued state.
		if (ps->forcePower <= BLOCKPOINTS_FATIGUE)
		{
			//Pop the Fatigued flag
			ps->userInt3 |= 1 << FLAG_FATIGUED;
		}
	}
}

static int Fatigue_SaberAttack()
{
	//returns the FP cost for a saber attack by this player.
	return FATIGUE_SABERATTACK;
}

//Add Fatigue for all the sabermoves and blocks.
extern qboolean PM_KnockAwayStaffAndDuels(int move);

static void PM_NPCFatigue(playerState_t* ps, const int new_move)
{
	if (g_SerenityJediEngineMode->integer == 2)
	{
		if (ps->saber_move != new_move)
		{
			//wasn't playing that attack before
			if (PM_KickMove(new_move))
			{
				//melee move
				PM_AddBlockFatigue(ps, FATIGUE_MELEE);
			}
			else if (PM_KnockAwayStaffAndDuels(new_move))
			{
				//simple saber attack
				PM_AddBlockFatigue(ps, Fatigue_SaberAttack());
			}
			else if (PM_SaberInTransition(new_move) && pm->ps->userInt3 & 1 << FLAG_ATTACKFAKE)
			{
				//attack fakes cost FP as well
				if (ps->saber_anim_level == SS_DUAL)
				{
					//dual sabers don't have transition/FP costs.
				}
				else
				{
					//single sabers
					PM_AddBlockFatigue(ps, Fatigue_SaberAttack());
				}
			}
		}
	}
	else
	{
		if (ps->saber_move != new_move)
		{
			//wasn't playing that attack before
			if (PM_KickMove(new_move))
			{
				//melee move
				PM_AddFatigue(ps, FATIGUE_MELEE);
			}
			else if (PM_KnockAwayStaffAndDuels(new_move))
			{
				//simple saber attack
				PM_AddFatigue(ps, Fatigue_SaberAttack());
			}
		}
	}
}

static void PM_BlockFatigue(playerState_t* ps, const int new_move)
{
	if (g_SerenityJediEngineMode->integer == 2)
	{
		if (ps->saber_move != new_move)
		{
			if (PM_KickMove(new_move))
			{
				//melee move
				PM_AddBlockFatigue(ps, FATIGUE_MELEE);
			}
			else if (PM_KnockAwayStaffAndDuels(new_move))
			{
				//simple saber attack
				PM_AddBlockFatigue(ps, FATIGUE_AUTOSABERDEFENSE);
			}
		}
	}
	else
	{
		if (ps->saber_move != new_move)
		{
			//wasn't playing that attack before
			if (PM_KickMove(new_move))
			{
				//melee move
				PM_AddFatigue(ps, FATIGUE_MELEE);
			}
			else if (PM_KnockAwayStaffAndDuels(new_move))
			{
				//simple saber attack
				PM_AddFatigue(ps, FATIGUE_AUTOSABERDEFENSE);
			}
		}
	}
}

extern qboolean DarthMaulHolster(const gentity_t* ent);
extern qboolean SaberStaffWeapon(const gentity_t* ent);
void PM_SaberFakeFlagUpdate(int new_move);
void PM_SaberPerfectBlockUpdate(int new_move);

void WP_SaberFatigueRegenerate(const int override_amt)
{
	if (pm->ps->saberFatigueChainCount >= MISHAPLEVEL_NONE)
	{
		if (override_amt)
		{
			pm->ps->saberFatigueChainCount -= override_amt;
		}
		else
		{
			pm->ps->saberFatigueChainCount--;
		}
		if (pm->ps->saberFatigueChainCount > MISHAPLEVEL_MAX)
		{
			pm->ps->saberFatigueChainCount = MISHAPLEVEL_MAX;
		}
	}
}

void WP_BlasterFatigueRegenerate(const int override_amt)
{
	if (pm->ps->BlasterAttackChainCount >= BLASTERMISHAPLEVEL_NONE)
	{
		if (override_amt)
		{
			pm->ps->BlasterAttackChainCount -= override_amt;
		}
		else
		{
			pm->ps->BlasterAttackChainCount--;
		}
		if (pm->ps->BlasterAttackChainCount > BLASTERMISHAPLEVEL_MAX)
		{
			pm->ps->BlasterAttackChainCount = BLASTERMISHAPLEVEL_MAX;
		}
	}
}

void PM_SetSaberMove(saber_moveName_t new_move)
{
	unsigned int setflags = saber_moveData[new_move].animSetFlags;
	int anim = saber_moveData[new_move].animToUse;
	int parts = SETANIM_TORSO;
	qboolean manual_blocking = qfalse;

	if (new_move < LS_NONE || new_move >= LS_MOVE_MAX)
	{
		assert(0);
		return;
	}

	if (pm->ps->eFlags & EF_HELD_BY_WAMPA)
	{
		//no anim
		return;
	}

	if (cg_debugSaber.integer || cg_saberinfo.integer & 0x01 && new_move != LS_READY)
	{
		if (pm->ps->client_num && !PM_ControlledByPlayer()) //npc only
		{
			Com_Printf("SetSaberMove:  From '%s' to '%s'\n", saber_moveData[pm->ps->saber_move].name,
				saber_moveData[new_move].name);
		}
	}

	/////////////////////////////////////////////////////// saber attack chain count /////////////////////////////////////////////////////////////////

	if (g_SerenityJediEngineMode->integer)
	{
		if (g_SerenityJediEngineMode->integer == 2)
		{
			if (pm->ps->blockPoints >= BLOCK_POINTS_MAX)
			{
				pm->ps->blockPoints = BLOCK_POINTS_MAX;
			}
			else if (pm->ps->blockPoints <= BLOCK_POINTS_MIN)
			{
				pm->ps->blockPoints = BLOCK_POINTS_MIN;
			}

			if (pm->ps->blockPoints < BLOCKPOINTS_TEN)
			{
				pm->ps->userInt3 |= 1 << FLAG_BLOCKDRAINED;
			}
			else if (pm->ps->blockPoints > BLOCKPOINTS_TEN)
			{
				pm->ps->userInt3 &= ~(1 << FLAG_BLOCKDRAINED);
			}
		}

		if (PM_SaberInAttack(new_move))
		{
			if (pm->ps->saberFatigueChainCount < MISHAPLEVEL_MAX)
			{
				pm->ps->saberFatigueChainCount++;
			}
		}
	}

	if (new_move == LS_READY || new_move == LS_A_FLIP_STAB || new_move == LS_A_FLIP_SLASH)
	{
		//finished with a kata (or in a special move) reset attack counter
		pm->ps->saberAttackChainCount = 0;
	}
	else if (PM_SaberInAttack(new_move))
	{
		//continuing with a kata, increment attack counter
		pm->ps->saberAttackChainCount++;
	}

	if (pm->ps->BlasterAttackChainCount >= BLASTERMISHAPLEVEL_MAX)
	{
		pm->ps->BlasterAttackChainCount = BLASTERMISHAPLEVEL_MAX;
	}
	else if (pm->ps->BlasterAttackChainCount <= BLASTERMISHAPLEVEL_NONE)
	{
		pm->ps->BlasterAttackChainCount = BLASTERMISHAPLEVEL_NONE;
	}

	if (pm->ps->saberAttackChainCount >= MISHAPLEVEL_OVERLOAD)
	{
		pm->ps->saberAttackChainCount = MISHAPLEVEL_OVERLOAD;
	}
	else if (pm->ps->saberAttackChainCount <= MISHAPLEVEL_NONE)
	{
		pm->ps->saberAttackChainCount = MISHAPLEVEL_NONE;
	}

	if (pm->ps->saberFatigueChainCount >= MISHAPLEVEL_OVERLOAD)
	{
		pm->ps->saberFatigueChainCount = MISHAPLEVEL_OVERLOAD;
	}
	else if (pm->ps->saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		pm->ps->saberFatigueChainCount = MISHAPLEVEL_NONE;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if (new_move == LS_READY)
	{
		if (pm->ps->saberBlockingTime > cg.time)
		{
			manual_blocking = qtrue;

			if (!pm->ps->SaberActive())
			{
				//turn on all blades and sabers if none are currently on
				pm->ps->SaberActivate();
			}

			if (pm->ps->saber[0].type == SABER_CLAW || pm->ps->dualSabers && pm->ps->saber[1].Active()
				// Both sabers active duels
				|| (pm->ps->SaberStaff() && (!pm->ps->saber[0].singleBladeStyle || pm->ps->saber[0].blade[1].active)
					//saber staff with more than first blade active
					|| pm->ps->saber[0].type == SABER_ARC))
			{
				if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
				{
					if (pm->ps->saber_anim_level == SS_DUAL)
					{
						if (g_SerenityJediEngineMode->integer == 2)
						{
							anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
						}
						else
						{
							anim = PM_BlockingPoseForSaberAnimLevelDualMD();
						}
					}
					else if (pm->ps->saber_anim_level == SS_STAFF)
					{
						if (g_SerenityJediEngineMode->integer == 2)
						{
							anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
						}
						else
						{
							anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
						}
					}
					else
					{
						if (g_SerenityJediEngineMode->integer == 2)
						{
							anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
						}
						else
						{
							anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
						}
					}
				}
				else
				{
					anim = PM_ReadyPoseForSaberAnimLevel();
				}
			}
		}
		else if (pm->ps->saber[0].readyAnim != -1)
		{
			if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
			{
				if (pm->ps->saber_anim_level == SS_DUAL)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualMD();
					}
				}
				else if (pm->ps->saber_anim_level == SS_STAFF)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
					}
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
					}
				}
			}
			else
			{
				anim = pm->ps->saber[0].readyAnim;
			}
		}
		else if (pm->ps->dualSabers && pm->ps->saber[1].readyAnim != -1)
		{
			if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
			{
				if (pm->ps->saber_anim_level == SS_DUAL)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualMD();
					}
				}
				else if (pm->ps->saber_anim_level == SS_STAFF)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
					}
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
					}
				}
			}
			else
			{
				anim = pm->ps->saber[1].readyAnim;
			}
		}
		else if (pm->ps->saber[0].type == SABER_LANCE || pm->ps->saber[0].type == SABER_TRIDENT
			|| (pm->ps->dualSabers && pm->ps->saber[1].Active()
				|| (pm->ps->SaberStaff() && (!pm->ps->saber[0].singleBladeStyle
					|| pm->ps->saber[0].blade[1].active)
					|| pm->ps->saber[0].type == SABER_ARC)))
		{
			if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
			{
				if (pm->ps->saber_anim_level == SS_DUAL)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualMD();
					}
				}
				else if (pm->ps->saber_anim_level == SS_STAFF)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
					}
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
					}
				}
			}
			else
			{
				anim = PM_ReadyPoseForSaberAnimLevel();
			}
		}
		else
		{
			if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
			{
				if (pm->ps->saber_anim_level == SS_DUAL)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelDualMD();
					}
				}
				else if (pm->ps->saber_anim_level == SS_STAFF)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
					}
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
					}
					else
					{
						anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
					}
				}
			}
			else
			{
				anim = PM_ReadyPoseForSaberAnimLevel();
			}
		}
	}
	else if (new_move == LS_DRAW)
	{
		if (PM_RunningAnim(pm->ps->torsoAnim))
		{
			pm->ps->saber_move = new_move;
			return;
		}

		if ((pm->ps->saber[0].holsterPlace == HOLSTER_LHIP
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPF
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPB)
			&& !pm->ps->dualSabers && pm->ps->saber[0].drawAnim == -1)
		{
			// anim = BOTH_S1_S6;
		}
		else if ((pm->ps->saber[0].holsterPlace == HOLSTER_LHIP
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPF
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPB) && pm->ps->dualSabers
			&& (pm->ps->saber[1].holsterPlace == HOLSTER_LHIP
				|| pm->ps->saber[1].holsterPlace == HOLSTER_LHIPF
				|| pm->ps->saber[1].holsterPlace == HOLSTER_LHIPB
				|| pm->ps->saber[1].holsterPlace == HOLSTER_RHIP
				|| pm->ps->saber[1].holsterPlace == HOLSTER_RHIPF
				|| pm->ps->saber[1].holsterPlace == HOLSTER_RHIPB)
			&& !(pm->ps->saber[0].stylesForbidden & 1 << SS_DUAL)
			&& !(pm->ps->saber[1].stylesForbidden & 1 << SS_DUAL))
		{
			anim = BOTH_S1_S6;
		}
		else if (pm->ps->saber[0].drawAnim != -1)
		{
			anim = pm->ps->saber[0].drawAnim;
		}
		else if (pm->ps->dualSabers && pm->ps->saber[1].drawAnim != -1)
		{
			anim = pm->ps->saber[1].drawAnim;
		}
		else if (pm->ps->saber[0].holsterPlace == HOLSTER_BACKU
			|| pm->ps->saber[0].holsterPlace == HOLSTER_BACKL
			|| pm->ps->saber[0].holsterPlace == HOLSTER_BACK)
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				anim = BOTH_S1_S7_AMD;
			}
			else
			{
				anim = BOTH_S1_S7;
			}
		}
		else if (DarthMaulHolster(pm->gent))
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				anim = BOTH_S1_S7_AMD;
			}
			else
			{
				anim = BOTH_S1_S7;
			}
		}
		else if (pm->ps->saber[0].stylesLearned == 1 << SS_STAFF)
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				anim = BOTH_S1_S7_AMD;
			}
			else
			{
				anim = BOTH_S1_S7;
			}
		}
		else if (SaberStaffWeapon(pm->gent) && pm->ps->saber[0].holsterPlace == HOLSTER_RHIPB)
		{
			if (g_SerenityJediEngineMode->integer == 2)
			{
				anim = BOTH_S1_S7_AMD;
			}
			else
			{
				anim = BOTH_S1_S7;
			}
		}
		else if (pm->ps->dualSabers
			&& !(pm->ps->saber[0].stylesForbidden & 1 << SS_DUAL)
			&& !(pm->ps->saber[1].stylesForbidden & 1 << SS_DUAL))
		{
			anim = BOTH_S1_S6;
		}
		if (pm->ps->torsoAnim == BOTH_STAND1IDLE1
			|| pm->ps->torsoAnim == BOTH_STAND9IDLE1
			|| pm->ps->torsoAnim == BOTH_STANDYODAIDLE_STICK
			|| pm->ps->torsoAnim == BOTH_STAND_SABER_ON_IDLE
			|| pm->ps->torsoAnim == BOTH_STAND_SABER_ON_IDLE_DUELS
			|| pm->ps->torsoAnim == BOTH_STAND_SABER_ON_IDLE_STAFF)
		{
			setflags |= SETANIM_FLAG_OVERRIDE;
		}
	}
	else if (new_move == LS_PUTAWAY)
	{
		if ((pm->ps->saber[0].holsterPlace == HOLSTER_LHIP
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPF
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPB)
			&& !pm->ps->dualSabers && pm->ps->saber[0].putawayAnim == -1)
		{
			// anim = BOTH_S6_S1;
		}
		else if ((pm->ps->saber[0].holsterPlace == HOLSTER_LHIP
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPF
			|| pm->ps->saber[0].holsterPlace == HOLSTER_LHIPB)
			&& pm->ps->dualSabers
			&& (pm->ps->saber[1].holsterPlace == HOLSTER_LHIP
				|| pm->ps->saber[1].holsterPlace == HOLSTER_LHIPF
				|| pm->ps->saber[1].holsterPlace == HOLSTER_LHIPB
				|| pm->ps->saber[1].holsterPlace == HOLSTER_RHIP
				|| pm->ps->saber[1].holsterPlace == HOLSTER_RHIPF
				|| pm->ps->saber[1].holsterPlace == HOLSTER_RHIPB)
			&& !(pm->ps->saber[0].stylesForbidden & 1 << SS_DUAL)
			&& !(pm->ps->saber[1].stylesForbidden & 1 << SS_DUAL)
			&& pm->ps->saber[1].Active())
		{
			anim = BOTH_S6_S1;
		}
		else if (pm->ps->saber[0].putawayAnim != -1)
		{
			anim = pm->ps->saber[0].putawayAnim;
		}
		else if (pm->ps->dualSabers && pm->ps->saber[1].putawayAnim != -1)
		{
			anim = pm->ps->saber[1].putawayAnim;
		}
		else if (pm->ps->saber[0].holsterPlace == HOLSTER_BACKU
			|| pm->ps->saber[0].holsterPlace == HOLSTER_BACKL
			|| pm->ps->saber[0].holsterPlace == HOLSTER_BACK)
		{
			anim = BOTH_S7_S1;
		}
		else if (DarthMaulHolster(pm->gent))
		{
			anim = BOTH_S7_S1;
		}
		else if (SaberStaffWeapon(pm->gent)
			&& pm->ps->saber[0].holsterPlace == HOLSTER_RHIPB && !DarthMaulHolster(pm->gent))
		{
			anim = BOTH_S7_S1;
		}
		else if (pm->ps->saber[0].stylesLearned == 1 << SS_STAFF && pm->ps->saber[0].blade[1].active)
		{
			anim = BOTH_S7_S1;
		}
		else if (pm->ps->dualSabers
			&& !(pm->ps->saber[0].stylesForbidden & 1 << SS_DUAL)
			&& !(pm->ps->saber[1].stylesForbidden & 1 << SS_DUAL)
			&& pm->ps->saber[1].Active())
		{
			anim = BOTH_S6_S1;
		}
		if (pm->ps->torsoAnim == BOTH_STAND1IDLE1)
		{
			setflags |= SETANIM_FLAG_OVERRIDE;
		}
		if (PM_SaberStanceAnim(pm->ps->legsAnim)
			&& pm->ps->legsAnim != BOTH_STAND1
			&& pm->ps->legsAnim != BOTH_STAND9
			&& pm->ps->legsAnim != BOTH_STANDYODA_STICK
			&& pm->ps->legsAnim != BOTH_STAND_SABER_ON
			&& pm->ps->legsAnim != BOTH_STAND_SABER_ON_DUELS
			&& pm->ps->legsAnim != BOTH_STAND_SABER_ON_STAFF)
		{
			parts = SETANIM_BOTH;
		}
		else
		{
			if (PM_RunningAnim(pm->ps->torsoAnim))
			{
				pm->ps->saber_move = new_move;
				return;
			}
			if (pm->ps->torsoAnim == BOTH_STAND1IDLE1)
			{
				parts = SETANIM_BOTH;
			}
			else
			{
				parts = SETANIM_TORSO;
			}
		}
	}
	else if (new_move == LS_A_JUMP_T__B_ && pm->ps->saber_anim_level == SS_DESANN && g_SerenityJediEngineMode->integer)
	{
		anim = Q_irand(BOTH_FJSS_TR_BL, BOTH_FJSS_TL_BR);
	}
	else if (pm->ps->saber_anim_level == SS_STAFF && new_move >= LS_S_TL2BR && new_move < LS_REFLECT_LL)
	{
		//staff has an entirely new set of anims, besides special attacks
		if (new_move >= LS_V1_BR && new_move <= LS_REFLECT_LL)
		{
			//there aren't 1-7, just 1, 6 and 7, so just set it
			anim = BOTH_P7_S7_T_ + (anim - BOTH_P1_S1_T_); //shift it up to the proper set
		}
		else
		{
			//add the appropriate animLevel
			anim += (pm->ps->saber_anim_level - FORCE_LEVEL_1) * SABER_ANIM_GROUP_SIZE;
		}
	}
	else if ((pm->ps->saber_anim_level == SS_DUAL
		|| pm->ps->dualSabers && pm->ps->saber[1].Active())
		&& new_move >= LS_S_TL2BR
		&& new_move < LS_REFLECT_LL)
	{
		//staff has an entirely new set of anims, besides special attacks
		if (new_move >= LS_V1_BR && new_move <= LS_REFLECT_LL)
		{
			//there aren't 1-7, just 1, 6 and 7, so just set it
			anim = BOTH_P6_S6_T_ + (anim - BOTH_P1_S1_T_); //shift it up to the proper set
		}
		else if ((new_move == LS_A_R2L || new_move == LS_S_R2L
			|| new_move == LS_A_L2R || new_move == LS_S_L2R)
			&& PM_CanDoDualDoubleAttacks()
			&& G_CheckEnemyPresence(pm->gent, DIR_RIGHT, 150.0f)
			&& G_CheckEnemyPresence(pm->gent, DIR_LEFT, 150.0f))
		{
			//enemy both on left and right
			new_move = LS_DUAL_LR;
			anim = saber_moveData[new_move].animToUse;
			//probably already moved, but...
			pm->cmd.rightmove = 0;
		}
		else if ((new_move == LS_A_T2B || new_move == LS_S_T2B
			|| new_move == LS_A_BACK || new_move == LS_A_BACK_CR)
			&& PM_CanDoDualDoubleAttacks()
			&& G_CheckEnemyPresence(pm->gent, DIR_FRONT, 150.0f)
			&& G_CheckEnemyPresence(pm->gent, DIR_BACK, 150.0f))
		{
			//enemy both in front and back
			new_move = LS_DUAL_FB;
			anim = saber_moveData[new_move].animToUse;
			//probably already moved, but...
			pm->cmd.forwardmove = 0;
		}
		else
		{
			//add the appropriate animLevel
			anim += (pm->ps->saber_anim_level - FORCE_LEVEL_1) * SABER_ANIM_GROUP_SIZE;
		}
	}
	else if (pm->ps->saber_anim_level > FORCE_LEVEL_1
		&& !PM_SaberInIdle(new_move)
		&& !PM_SaberInParry(new_move)
		&& !PM_SaberInKnockaway(new_move)
		&& !PM_SaberInBrokenParry(new_move)
		&& !PM_SaberInReflect(new_move)
		&& !PM_SaberInSpecial(new_move))
	{
		//readies, parries and reflections have only 1 level
		if (pm->ps->saber[0].type == SABER_LANCE || pm->ps->saber[0].type == SABER_TRIDENT)
		{
			//FIXME: hack for now - these use the fast anims, but slowed down.  Should have own style
		}
		else
		{
			//increment the anim to the next level of saber anims
			anim += (pm->ps->saber_anim_level - FORCE_LEVEL_1) * SABER_ANIM_GROUP_SIZE;
		}
	}
	else if (new_move == LS_KICK_F_AIR
		|| new_move == LS_KICK_B_AIR
		|| new_move == LS_KICK_R_AIR
		|| new_move == LS_KICK_L_AIR)
	{
		if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
		{
			PM_SetJumped(200, qtrue);
		}
	}

	if ((pm->ps->torsoAnim == anim || pm->ps->legsAnim == anim)
		&& new_move > LS_PUTAWAY)
	{
		setflags |= SETANIM_FLAG_RESTART;
	}

	if (anim == BOTH_STAND1 && (pm->ps->saber[0].type == SABER_ARC || pm->ps->dualSabers && pm->ps->saber[1].
		Active())
		|| anim == BOTH_STAND2
		|| anim == BOTH_SABERDUAL_STANCE
		|| anim == BOTH_SABERFAST_STANCE_JKA
		|| anim == BOTH_SABERSLOW_STANCE
		|| anim == BOTH_SABERSLOW_STANCE_JKA
		|| anim == BOTH_SABERTAVION_STANCE
		|| anim == BOTH_SABERTAVION_STANCE_JKA
		|| anim == BOTH_SABERDESANN_STANCE
		|| anim == BOTH_SABERDESANN_STANCE_JKA)
	{
		//match torso anim to walk/run anim if newMove is just LS_READY
		//FIXME: play both_stand2_random1 when you've been idle for a while
		switch (pm->ps->legsAnim)
		{
		case BOTH_WALK1:
		case BOTH_WALK1_MDA:
		case BOTH_WALK2:
		case BOTH_WALK_STAFF:
		case BOTH_WALK_STAFF_AMD:
		case BOTH_WALK_DUAL:
		case BOTH_WALK_DUAL_AMD:
		case BOTH_WALKBACK1:
		case BOTH_WALKBACK_PISTOL:
		case BOTH_WALKBACK_BLASTER:
		case BOTH_WALKBACK_HEAVY:
		case BOTH_WALKBACK_GRENADE:
		case BOTH_WALKBACK2:
		case BOTH_WALKBACK_STAFF:
		case BOTH_WALKBACK_DUAL:
		case BOTH_WALKBACK_DUALPISTOL:
		case BOTH_RUN1:
		case BOTH_SPRINT:
		case BOTH_SPRINT_SINGLE_LIGHTSABER:
		case BOTH_SPRINT_STAFF_LIGHTSABER:
		case BOTH_SPRINT_DUAL_LIGHTSABER:
		case BOTH_RUN2:
		case BOTH_RUN_STAFF:
		case BOTH_RUN_DUAL:
		case BOTH_RUNBACK1:
		case BOTH_RUNBACK2:
		case BOTH_RUNBACK_STAFF:
		case BOTH_MENUIDLE1:
		case BOTH_SABERSINGLECROUCH:
		case BOTH_SABERDUALCROUCH:
		case BOTH_SABERSTAFFCROUCH:
		case BOTH_WALK1_STICK:
			anim = pm->ps->legsAnim;
			break;
		default:;
		}
	}

	if (!PM_RidingVehicle())
	{
		if (!manual_blocking)
		{
			if (new_move == LS_A_LUNGE
				|| new_move == LS_A_JUMP_T__B_
				|| new_move == LS_A_BACKSTAB
				|| new_move == LS_A_BACK
				|| new_move == LS_A_BACK_CR
				|| new_move == LS_ROLL_STAB
				|| new_move == LS_A_FLIP_STAB
				|| new_move == LS_A_FLIP_SLASH
				|| new_move == LS_JUMPATTACK_DUAL
				|| new_move == LS_JUMPATTACK_ARIAL_LEFT
				|| new_move == LS_JUMPATTACK_ARIAL_RIGHT
				|| new_move == LS_JUMPATTACK_CART_LEFT
				|| new_move == LS_JUMPATTACK_CART_RIGHT
				|| new_move == LS_JUMPATTACK_STAFF_LEFT
				|| new_move == LS_JUMPATTACK_STAFF_RIGHT
				|| new_move == LS_BUTTERFLY_LEFT
				|| new_move == LS_BUTTERFLY_RIGHT
				|| new_move == LS_A_BACKFLIP_ATK
				|| new_move == LS_STABDOWN
				|| new_move == LS_STABDOWN_STAFF
				|| new_move == LS_STABDOWN_DUAL
				|| new_move == LS_DUAL_SPIN_PROTECT
				|| new_move == LS_STAFF_SOULCAL
				|| new_move == LS_A1_SPECIAL
				|| new_move == LS_A2_SPECIAL
				|| new_move == LS_A3_SPECIAL
				|| new_move == LS_UPSIDE_DOWN_ATTACK
				|| new_move == LS_PULL_ATTACK_STAB
				|| new_move == LS_GRAPPLE_FIRE
				|| new_move == LS_PULL_ATTACK_SWING
				|| PM_KickMove(new_move))
			{
				parts = SETANIM_BOTH;
			}
			else if (PM_SpinningSaberAnim(anim))
			{
				//spins must be played on entire body
				parts = SETANIM_BOTH;
			}
			else if (PM_SpinningSaberAnim(pm->ps->legsAnim))
			{
				//spins must be played on entire body
				parts = SETANIM_BOTH;
				pm->ps->legsAnimTimer = pm->ps->torsoAnimTimer = 0;
			}
			else if (!pm->cmd.forwardmove && !pm->cmd.rightmove && !pm->cmd.upmove || pm->ps->speed == 0)
			{
				//not trying to run, duck or jump
				if (!PM_FlippingAnim(pm->ps->legsAnim) &&
					!PM_InRoll(pm->ps) &&
					!PM_InKnockDown(pm->ps) &&
					!PM_JumpingAnim(pm->ps->legsAnim) &&
					!PM_PainAnim(pm->ps->legsAnim) &&
					!PM_InSpecialJump(pm->ps->legsAnim) &&
					!PM_InSlopeAnim(pm->ps->legsAnim) &&
					!(pm->ps->pm_flags & PMF_DUCKED) &&
					new_move != LS_PUTAWAY)
				{
					parts = SETANIM_BOTH;
				}
				else if (!(pm->ps->pm_flags & PMF_DUCKED)
					&& (new_move == LS_SPINATTACK_DUAL || new_move == LS_SPINATTACK))
				{
					parts = SETANIM_BOTH;
				}
			}
		}
	}
	else
	{
		if (!pm->ps->saberBlocked)
		{
			parts = SETANIM_BOTH;
			setflags &= ~SETANIM_FLAG_RESTART;
		}
	}
	if (anim != -1)
	{
		PM_SetAnim(pm, parts, anim, setflags, saber_moveData[new_move].blend_time);
	}

	if (pm->ps->torsoAnim == anim)
	{
		//successfully changed anims
		//special check for *starting* a saber swing
		if (pm->gent && pm->ps->SaberLength() > 1)
		{
			if (g_SerenityJediEngineMode->integer)
			{
				if (pm->ps->client_num && !PM_ControlledByPlayer())
				{
					PM_NPCFatigue(pm->ps, new_move); //drain blockpoints low cost
				}
			}
			//update the attack fake flag
			PM_SaberFakeFlagUpdate(new_move);

			PM_SaberPerfectBlockUpdate(new_move);

			if (!PM_SaberInBounce(new_move) && !PM_SaberInReturn(new_move))
			{
				//switched away from a slow bounce move, remove the flags.
				pm->ps->userInt3 &= ~(1 << FLAG_SLOWBOUNCE);
				pm->ps->userInt3 &= ~(1 << FLAG_OLDSLOWBOUNCE);
				pm->ps->userInt3 &= ~(1 << FLAG_PARRIED);
				pm->ps->userInt3 &= ~(1 << FLAG_BLOCKING);
				pm->ps->userInt3 &= ~(1 << FLAG_BLOCKED);
			}

			if (!PM_SaberInBounce(new_move) && !PM_SaberInReturn(new_move) && !PM_SaberInMassiveBounce(
				pm->ps->torsoAnim))
			{
				//cancel out pre-block flag
				pm->ps->userInt3 &= ~(1 << FLAG_MBLOCKBOUNCE);
			}

			if (!PM_SaberInParry(new_move))
			{
				//cancel out pre-block flag
				pm->ps->userInt3 &= ~(1 << FLAG_PREBLOCK);
			}
			if (PM_SaberInAttack(new_move) || pm_saber_in_special_attack(anim))
			{
				//playing an attack
				if (pm->ps->saber_move != new_move)
				{
					//wasn't playing that attack before
					if (pm_saber_in_special_attack(anim))
					{
						WP_SaberSwingSound(pm->gent, 0, SWING_FAST);
						if (!PM_InCartwheel(pm->ps->torsoAnim))
						{
							//can still attack during a cartwheel/arial
							pm->ps->weaponTime = pm->ps->torsoAnimTimer; //so we know our weapon is busy
						}
					}
					else
					{
						switch (pm->ps->saber_anim_level)
						{
						case SS_DESANN:
						case SS_STRONG:
							WP_SaberSwingSound(pm->gent, 0, SWING_STRONG);
							break;
						case SS_MEDIUM:
						case SS_DUAL:
						case SS_STAFF:
							WP_SaberSwingSound(pm->gent, 0, SWING_MEDIUM);
							break;
						case SS_TAVION:
						case SS_FAST:
							WP_SaberSwingSound(pm->gent, 0, SWING_FAST);
							break;
						default:;
						}
					}
				}
				else if (setflags & SETANIM_FLAG_RESTART && pm_saber_in_special_attack(anim))
				{
					//sigh, if restarted a special, then set the weaponTime *again*
					if (!PM_InCartwheel(pm->ps->torsoAnim))
					{
						//can still attack during a cartwheel/arial
						pm->ps->weaponTime = pm->ps->torsoAnimTimer; //so we know our weapon is busy
					}
				}
			}
			else if (PM_SaberInStart(new_move))
			{
				//don't damage on the first few frames of a start anim because it may pop from one position to some drastically different one, killing the enemy without hitting them.
				if (pm->ps->saber_anim_level == SS_STRONG)
				{
					WP_SaberSwingSound(pm->gent, 0, SWING_FAST);
				}
			}
		}

		//Some special attacks can be started when sabers are off, make sure we turn them on, first!
		switch (new_move)
		{
			//make sure the saber is on!
		case LS_ROLL_STAB:
			if (PM_InSecondaryStyle())
			{
				//staff as medium or dual as fast
				if (pm->ps->dualSabers)
				{
					//only force on the first saber
					pm->ps->saber[0].Activate();
				}
				else if (pm->ps->saber[0].numBlades > 1)
				{
					//only force on the first saber's first blade
					pm->ps->SaberBladeActivate(0, 0);
				}
			}
			else
			{
				//turn on all blades on all sabers
				pm->ps->SaberActivate();
			}
			break;
		case LS_SPINATTACK_DUAL:
		case LS_DUAL_SPIN_PROTECT:
			if (pm->ps->dualSabers)
			{
				//force on
				if (pm->gent->weaponModel[1] == -1)
				{
					G_RemoveHolsterModels(pm->gent);
					WP_SaberAddG2SaberModels(pm->gent, qtrue);
				}
				pm->ps->saber[0].Activate();
				pm->ps->saber[1].Activate();
			}
			else
			{
				//turn on all blades on all sabers
				pm->ps->SaberActivate();
			}
			break;
		case LS_A_LUNGE:
		case LS_A1_SPECIAL:
			if (pm->ps->dualSabers)
			{
				//only force on the first saber
				pm->ps->saber[0].Activate();
			}
			else
			{
				//turn on all blades on all sabers
				pm->ps->saber[0].Activate();
			}
			break;
		case LS_SPINATTACK_ALORA:
		case LS_SPINATTACK:
		case LS_A2_SPECIAL:
		case LS_A3_SPECIAL:
		case LS_STAFF_SOULCAL:
			pm->ps->SaberActivate();
			break;
		default:
			break;
		}

		pm->ps->saber_move = new_move;
		pm->ps->saberBlocking = saber_moveData[new_move].blocking;

		if (pm->ps->client_num == 0 || PM_ControlledByPlayer())
		{
			if (pm->ps->saberBlocked >= BLOCKED_UPPER_RIGHT_PROJ && pm->ps->saberBlocked <= BLOCKED_TOP_PROJ
				&& new_move >= LS_REFLECT_UP && new_move <= LS_REFLECT_LL)
			{
				//don't clear it when blocking projectiles
			}
			else
			{
				pm->ps->saberBlocked = BLOCKED_NONE;
			}
		}
		else if (pm->ps->saberBlocked <= BLOCKED_ATK_BOUNCE || !pm->ps->SaberActive() || (new_move < LS_PARRY_UR ||
			new_move > LS_REFLECT_LL))
		{
			//NPCs only clear blocked if not blocking?
			pm->ps->saberBlocked = BLOCKED_NONE;
		}

		if (pm->gent && pm->gent->client)
		{
			if (saber_moveData[new_move].trailLength > 0)
			{
				pm->gent->client->ps.SaberActivateTrail(saber_moveData[new_move].trailLength);
				// saber trail lasts for 75ms...feel free to change this if you want it longer or shorter
			}
			else
			{
				pm->gent->client->ps.SaberDeactivateTrail(0);
			}
		}
	}
}

/*
==============
PM_Use

Generates a use event
==============
*/
constexpr auto USE_DELAY = 250;

static void PM_Use()
{
	if (pm->ps->useTime > 0)
	{
		pm->ps->useTime -= pml.msec;
		if (pm->ps->useTime < 0)
		{
			pm->ps->useTime = 0;
		}
	}

	if (pm->ps->useTime > 0)
	{
		return;
	}

	if (!(pm->cmd.buttons & BUTTON_USE))
	{
		pm->useEvent = 0;
		pm->ps->useTime = 0;
		return;
	}

	pm->useEvent = EV_USE;
	pm->ps->useTime = USE_DELAY;
}

extern saber_moveName_t PM_AttackForEnemyPos(qboolean allow_fb, qboolean allow_stab_down);
extern saber_moveName_t PM_NPC_Force_Leap_Attack(void);
extern qboolean PM_Can_Do_Kill_Lunge(void);
extern qboolean PM_Can_Do_Kill_Lunge_back(void);
int Next_Kill_Attack_Move_Check[32]; // Next special move check.
saber_moveName_t PM_DoAI_Fake(const int curmove);

saber_moveName_t PM_NPCSaberAttackFromQuad(const int quad)
{
	saber_moveName_t auto_move = LS_NONE;
	saber_moveName_t newmove = LS_NONE;

	if (pm->gent &&
		(pm->gent->NPC && pm->gent->NPC->rank != RANK_ENSIGN && pm->gent->NPC->rank != RANK_CIVILIAN
			|| pm->gent->client &&
			(pm->gent->client->NPC_class == CLASS_TAVION || pm->gent->client->NPC_class == CLASS_YODA || pm->gent->client->NPC_class == CLASS_GALEN || pm->gent->client->NPC_class == CLASS_ALORA)))
	{
		auto_move = PM_AttackForEnemyPos(qtrue, qtrue);
	}

	if (auto_move != LS_NONE && PM_SaberInSpecial(auto_move))
	{
		//if have opportunity to do a special attack, do one
		return auto_move;
	}

	//pick another one

	switch (quad)
	{
	case Q_T: //blocked top
		if (Q_irand(0, 1))
		{
			newmove = LS_A_T2B;
		}
		else
		{
			newmove = LS_A_TR2BL;
		}
		break;
	case Q_TR:
		if (!Q_irand(0, 2))
		{
			newmove = LS_A_R2L;
		}
		else if (!Q_irand(0, 1))
		{
			newmove = LS_A_TR2BL;
		}
		else
		{
			newmove = LS_T1_TR_BR;
		}
		break;
	case Q_TL:
		if (!Q_irand(0, 2))
		{
			newmove = LS_A_L2R;
		}
		else if (!Q_irand(0, 1))
		{
			newmove = LS_A_TL2BR;
		}
		else
		{
			newmove = LS_T1_TL_BL;
		}
		break;
	case Q_BR:
		if (!Q_irand(0, 2))
		{
			newmove = LS_A_BR2TL;
		}
		else if (!Q_irand(0, 1))
		{
			newmove = LS_T1_BR_TR;
		}
		else
		{
			newmove = LS_A_R2L;
		}
		break;
	case Q_BL:
		if (!Q_irand(0, 2))
		{
			newmove = LS_A_BL2TR;
		}
		else if (!Q_irand(0, 1))
		{
			newmove = LS_T1_BL_TL;
		}
		else
		{
			newmove = LS_A_L2R;
		}
		break;
	case Q_L:
		if (!Q_irand(0, 2))
		{
			newmove = LS_A_L2R;
		}
		else if (!Q_irand(0, 1))
		{
			newmove = LS_T1__L_T_;
		}
		else
		{
			newmove = LS_A_R2L;
		}
		break;
	case Q_R:
		if (!Q_irand(0, 2))
		{
			newmove = LS_A_R2L;
		}
		else if (!Q_irand(0, 1))
		{
			newmove = LS_T1__R_T_;
		}
		else
		{
			newmove = LS_A_L2R;
		}
		break;
	case Q_B:
		newmove = PM_SaberLungeAttackMove(qtrue);
		break;
	default:
		break;
	}

	if (g_SerenityJediEngineMode->integer)
	{
		if (g_spskill->integer > 1 &&
			G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER, qtrue) &&
			!pm->ps->forcePowersActive && !in_camera)
		{
			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{// Some special AI stuff.
				if (Next_Kill_Attack_Move_Check[pm->ps->client_num] <= level.time && g_attackskill->integer >= 0)
				{
					int check_val = 0; // Times 500 for next check interval.

					if (PM_Can_Do_Kill_Lunge_back())
					{ //BACKSTAB
						if ((pm->ps->pm_flags & PMF_DUCKED) || pm->cmd.upmove < 0)
						{
							newmove = LS_A_BACK_CR;
						}
						else
						{
							int choice = rand() % 3;

							if (choice == 1)
							{
								newmove = LS_A_BACK;
							}
							else if (choice == 2)
							{
								newmove = PM_SaberBackflipAttackMove();
							}
							else if (choice == 3)
							{
								newmove = LS_A_BACKFLIP_ATK;
							}
							else
							{
								newmove = LS_A_BACKSTAB;
							}
						}
					}
					else if (PM_Can_Do_Kill_Lunge())
					{
						if (pm->ps->pm_flags & PMF_DUCKED)
						{
							newmove = PM_SaberLungeAttackMove(qtrue);

							if (d_attackinfo->integer)
							{
								gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 0\n");
							}
						}
						else
						{
							const int choice = rand() % 9;

							switch (choice)
							{
							case 0:
								newmove = PM_NPC_Force_Leap_Attack();

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 1\n");
								}
								break;
							case 1:
								newmove = PM_DoAI_Fake(qtrue);

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 2\n");
								}
								break;
							case 2:
								newmove = LS_A1_SPECIAL;

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 3\n");
								}
								break;
							case 3:
								newmove = LS_A2_SPECIAL;

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 4\n");
								}
								break;
							case 4:
								newmove = LS_A3_SPECIAL;

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 5\n");
								}
								break;
							case 5:
								newmove = LS_JUMPATTACK_STAFF_RIGHT;

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 6\n");
								}
								break;
							case 6:
								newmove = LS_JUMPATTACK_STAFF_LEFT;

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 7\n");
								}
								break;
							case 7:
								if (pm->ps->saber_anim_level == SS_DUAL)
								{
									newmove = LS_DUAL_SPIN_PROTECT;
								}
								else if (pm->ps->saber_anim_level == SS_STAFF)
								{
									newmove = LS_STAFF_SOULCAL;
								}
								else
								{
									newmove = LS_A_JUMP_T__B_;
								}

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 8\n");
								}
								break;
							case 8:
								if (pm->ps->saber_anim_level == SS_DUAL)
								{
									newmove = LS_JUMPATTACK_DUAL;
								}
								else if (pm->ps->saber_anim_level == SS_STAFF)
								{
									newmove = LS_JUMPATTACK_STAFF_RIGHT;
								}
								else
								{
									newmove = LS_SPINATTACK;
								}

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 9\n");
								}
								break;
							default:
								newmove = PM_SaberFlipOverAttackMove();

								if (d_attackinfo->integer)
								{
									gi.Printf(S_COLOR_MAGENTA"Next_Kill_Attack_Move_Check 10\n");
								}
								break;
							}
						}
						pm->ps->weaponstate = WEAPON_FIRING;
						WP_ForcePowerDrain(pm->gent, FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER_FB);
					}

					check_val = g_attackskill->integer;

					if (check_val <= 0)
					{
						check_val = 1;
					}

					Next_Kill_Attack_Move_Check[pm->ps->client_num] = level.time + (40000 / check_val); // 20 secs / g_attackskill->integer
				}
			}
		}
	}

	return newmove;
}

static int PM_SaberMoveQuadrantForMovement(const usercmd_t* ucmd)
{
	if (ucmd->rightmove > 0)
	{
		//moving right
		if (ucmd->forwardmove > 0)
		{
			//forward right = TL2BR slash
			return Q_TL;
		}
		if (ucmd->forwardmove < 0)
		{
			//backward right = BL2TR uppercut
			return Q_BL;
		}
		//just right is a left slice
		return Q_L;
	}
	if (ucmd->rightmove < 0)
	{
		//moving left
		if (ucmd->forwardmove > 0)
		{
			//forward left = TR2BL slash
			return Q_TR;
		}
		if (ucmd->forwardmove < 0)
		{
			//backward left = BR2TL uppercut
			return Q_BR;
		}
		//just left is a right slice
		return Q_R;
	}
	//not moving left or right
	if (ucmd->forwardmove > 0)
	{
		//forward= T2B slash
		return Q_T;
	}
	if (ucmd->forwardmove < 0)
	{
		//backward= T2B slash	//or B2T uppercut?
		return Q_T;
	}
	//if ( curmove == LS_READY )//???
	//Not moving at all
	return Q_R;
	//return Q_R;//????
}

void PM_SetAnimFrame(gentity_t* gent, const int frame, const qboolean torso, const qboolean legs)
{
	if (!gi.G2API_HaveWeGhoul2Models(gent->ghoul2))
	{
		return;
	}
	const int actual_time = cg.time ? cg.time : level.time;
	if (torso && gent->lowerLumbarBone != -1) //gent->upperLumbarBone
	{
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, //gent->upperLumbarBone
			frame, frame + 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1, actual_time,
			frame, 150);
		if (gent->motionBone != -1)
		{
			gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone, //gent->upperLumbarBone
				frame, frame + 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1, actual_time,
				frame, 150);
		}
	}
	if (torso && gent->headModel > 0 && gent->headLowerLumbarBone != -1)
	{
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->headModel], gent->headLowerLumbarBone, //gent->upperLumbarBone
			frame, frame + 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1, actual_time,
			frame, 150);
		if (gent->motionBone != -1)
		{
			gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->headModel], gent->headMotionBone, //gent->upperLumbarBone
				frame, frame + 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1, actual_time,
				frame, 150);
		}
	}
	if (legs && gent->rootBone != -1)
	{
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone,
			frame, frame + 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1, actual_time,
			frame, 150);
	}
	if (legs && gent->headModel > 0 && gent->headRootBone != -1)
	{
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->headModel], gent->headRootBone,
			frame, frame + 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1, actual_time,
			frame, 150);
	}
}

static int PM_SaberLockWinAnim(const saberLockResult_t result, const int break_type)
{
	int win_anim = -1;
	switch (pm->ps->torsoAnim)
	{
	case BOTH_BF2LOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			win_anim = BOTH_LK_S_S_T_SB_1_W;
		}
		else if (result == LOCK_DRAW)
		{
			win_anim = BOTH_BF1BREAK;
		}
		else
		{
			pm->ps->saber_move = LS_A_T2B;
			win_anim = BOTH_A3_T__B_;
		}
		break;
	case BOTH_BF1LOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			win_anim = BOTH_LK_S_S_T_SB_1_W;
		}
		else if (result == LOCK_DRAW)
		{
			win_anim = BOTH_KNOCKDOWN4;
		}
		else
		{
			pm->ps->saber_move = LS_K1_T_;
			win_anim = BOTH_K1_S1_T_;
		}
		break;
	case BOTH_CWCIRCLELOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			win_anim = BOTH_LK_S_S_S_SB_1_W;
		}
		else if (result == LOCK_DRAW)
		{
			pm->ps->saber_move = pm->ps->saberBounceMove = LS_V1_BL;
			pm->ps->saberBlocked = BLOCKED_PARRY_BROKEN;
			win_anim = BOTH_V1_BL_S1;
		}
		else
		{
			win_anim = BOTH_CWCIRCLEBREAK;
		}
		break;
	case BOTH_CCWCIRCLELOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			win_anim = BOTH_LK_S_S_S_SB_1_W;
		}
		else if (result == LOCK_DRAW)
		{
			pm->ps->saber_move = pm->ps->saberBounceMove = LS_V1_BR;
			pm->ps->saberBlocked = BLOCKED_PARRY_BROKEN;
			win_anim = BOTH_V1_BR_S1;
		}
		else
		{
			win_anim = BOTH_CCWCIRCLEBREAK;
		}
		break;
	default:
		//must be using new system:
		break;
	}
	if (win_anim != -1)
	{
		PM_SetAnim(pm, SETANIM_BOTH, win_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		pm->ps->weaponTime = pm->ps->torsoAnimTimer;
		pm->ps->saberBlocked = BLOCKED_NONE;
		pm->ps->weaponstate = WEAPON_FIRING;
		if (break_type == SABERLOCK_SUPERBREAK
			&& win_anim != BOTH_LK_ST_DL_T_SB_1_W)
		{
			//going to attack with saber, do a saber trail
			pm->ps->SaberActivateTrail(200);
		}
	}
	return win_anim;
}

static int PM_SaberLockLoseAnim(gentity_t* genemy, const saberLockResult_t result, const int break_type)
{
	int lose_anim = -1;
	switch (genemy->client->ps.torsoAnim)
	{
	case BOTH_BF2LOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			lose_anim = BOTH_LK_S_S_T_SB_1_L;
		}
		else if (result == LOCK_DRAW)
		{
			lose_anim = BOTH_BF1BREAK;
		}
		else
		{
			if (result == LOCK_STALEMATE)
			{
				//no-one won
				genemy->client->ps.saber_move = LS_K1_T_;
				lose_anim = BOTH_K1_S1_T_;
			}
			else
			{
				//FIXME: this anim needs to transition back to ready when done
				lose_anim = BOTH_BF1BREAK;
			}
		}
		break;
	case BOTH_BF1LOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			lose_anim = BOTH_LK_S_S_T_SB_1_L;
		}
		else if (result == LOCK_DRAW)
		{
			lose_anim = BOTH_KNOCKDOWN4;
		}
		else
		{
			if (result == LOCK_STALEMATE)
			{
				//no-one won
				genemy->client->ps.saber_move = LS_A_T2B;
				lose_anim = BOTH_A3_T__B_;
			}
			else
			{
				lose_anim = BOTH_KNOCKDOWN4;
			}
		}
		break;
	case BOTH_CWCIRCLELOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			lose_anim = BOTH_LK_S_S_S_SB_1_L;
		}
		else if (result == LOCK_DRAW)
		{
			genemy->client->ps.saber_move = genemy->client->ps.saberBounceMove = LS_V1_BL;
			genemy->client->ps.saberBlocked = BLOCKED_PARRY_BROKEN;
			lose_anim = BOTH_V1_BL_S1;
		}
		else
		{
			if (result == LOCK_STALEMATE)
			{
				//no-one won
				lose_anim = BOTH_CCWCIRCLEBREAK;
			}
			else
			{
				genemy->client->ps.saber_move = genemy->client->ps.saberBounceMove = LS_V1_BL;
				genemy->client->ps.saberBlocked = BLOCKED_PARRY_BROKEN;
				lose_anim = BOTH_V1_BL_S1;
			}
		}
		break;
	case BOTH_CCWCIRCLELOCK:
		if (break_type == SABERLOCK_SUPERBREAK)
		{
			lose_anim = BOTH_LK_S_S_S_SB_1_L;
		}
		else if (result == LOCK_DRAW)
		{
			genemy->client->ps.saber_move = genemy->client->ps.saberBounceMove = LS_V1_BR;
			genemy->client->ps.saberBlocked = BLOCKED_PARRY_BROKEN;
			lose_anim = BOTH_V1_BR_S1;
		}
		else
		{
			if (result == LOCK_STALEMATE)
			{
				//no-one won
				lose_anim = BOTH_CWCIRCLEBREAK;
			}
			else
			{
				genemy->client->ps.saber_move = genemy->client->ps.saberBounceMove = LS_V1_BR;
				genemy->client->ps.saberBlocked = BLOCKED_PARRY_BROKEN;
				lose_anim = BOTH_V1_BR_S1;
			}
		}
		break;
	default:;
	}
	if (lose_anim != -1)
	{
		NPC_SetAnim(genemy, SETANIM_BOTH, lose_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		genemy->client->ps.weaponTime = genemy->client->ps.torsoAnimTimer; // + 250;
		genemy->client->ps.saberBlocked = BLOCKED_NONE;
		genemy->client->ps.weaponstate = WEAPON_READY;
	}
	return lose_anim;
}

static int PM_SaberLockResultAnim(gentity_t* duelist, const int lock_or_break_or_super_break, const int win_or_lose)
{
	int base_anim = duelist->client->ps.torsoAnim;
	switch (base_anim)
	{
	case BOTH_LK_S_S_S_L_2: //lock if I'm using single vs. a single and other intitiated
		base_anim = BOTH_LK_S_S_S_L_1;
		break;
	case BOTH_LK_S_S_T_L_2: //lock if I'm using single vs. a single and other initiated
		base_anim = BOTH_LK_S_S_T_L_1;
		break;
	case BOTH_LK_DL_DL_S_L_2: //lock if I'm using dual vs. dual and other initiated
		base_anim = BOTH_LK_DL_DL_S_L_1;
		break;
	case BOTH_LK_DL_DL_T_L_2: //lock if I'm using dual vs. dual and other initiated
		base_anim = BOTH_LK_DL_DL_T_L_1;
		break;
	case BOTH_LK_ST_ST_S_L_2: //lock if I'm using staff vs. a staff and other initiated
		base_anim = BOTH_LK_ST_ST_S_L_1;
		break;
	case BOTH_LK_ST_ST_T_L_2: //lock if I'm using staff vs. a staff and other initiated
		base_anim = BOTH_LK_ST_ST_T_L_1;
		break;
	default:;
	}
	//what kind of break?
	if (lock_or_break_or_super_break == SABERLOCK_BREAK)
	{
		base_anim -= 2;
	}
	else if (lock_or_break_or_super_break == SABERLOCK_SUPERBREAK)
	{
		base_anim += 1;
	}
	else
	{
		//WTF?  Not a valid result
		return -1;
	}
	//win or lose?
	if (win_or_lose == SABERLOCK_WIN)
	{
		base_anim += 1;
	}
	//play the anim and hold it
	NPC_SetAnim(duelist, SETANIM_BOTH, base_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

	if (lock_or_break_or_super_break == SABERLOCK_SUPERBREAK
		&& win_or_lose == SABERLOCK_LOSE)
	{
		//if you lose a superbreak, you're defenseless
		//make saberent not block
		gentity_t* saberent = &g_entities[duelist->client->ps.saberEntityNum];
		if (saberent)
		{
			VectorClear(saberent->mins);
			VectorClear(saberent->maxs);
			G_SetOrigin(saberent, duelist->currentOrigin);
		}
		//set sabermove to none
		duelist->client->ps.saber_move = LS_NONE;
		//Hold the anim a little longer than it is
		duelist->client->ps.torsoAnimTimer += 250;
	}

	//no attacking during this anim
	duelist->client->ps.weaponTime = duelist->client->ps.torsoAnimTimer;
	duelist->client->ps.saberBlocked = BLOCKED_NONE;
	if (lock_or_break_or_super_break == SABERLOCK_SUPERBREAK
		&& win_or_lose == SABERLOCK_WIN
		&& base_anim != BOTH_LK_ST_DL_T_SB_1_W)
	{
		//going to attack with saber, do a saber trail
		duelist->client->ps.SaberActivateTrail(200);
	}
	return base_anim;
}

constexpr auto AMPUTATE_DAMAGE = 1000;

static void PM_SaberLockBreak(gentity_t* gent, gentity_t* genemy, const saberLockResult_t result, int victory_strength)
{
	int break_type;
	qboolean single_vs_single = qtrue;

	if (result == LOCK_VICTORY
		&& Q_irand(0, 7) < victory_strength)
	{
		if (genemy
			&& genemy->NPC
			&& (genemy->NPC->aiFlags & NPCAI_BOSS_CHARACTER
				|| genemy->NPC->aiFlags & NPCAI_SUBBOSS_CHARACTER
				|| genemy->client && genemy->client->NPC_class == CLASS_SHADOWTROOPER
				|| genemy->client && genemy->client->NPC_class == CLASS_JEDIMASTER)
			&& Q_irand(0, 4))
		{
			//less of a chance of getting a superbreak against a boss
			break_type = SABERLOCK_BREAK;
		}
		else
		{
			break_type = SABERLOCK_SUPERBREAK;
		}
	}
	else
	{
		break_type = SABERLOCK_BREAK;
	}
	int win_anim = PM_SaberLockWinAnim(result, break_type);
	if (win_anim != -1)
	{
		//a single vs. single break
		if (genemy && genemy->client)
		{
			PM_SaberLockLoseAnim(genemy, result, break_type);
		}
	}
	else
	{
		//must be a saberlock that's not between single and single...
		single_vs_single = qfalse;
		win_anim = PM_SaberLockResultAnim(gent, break_type, SABERLOCK_WIN);
		pm->ps->weaponstate = WEAPON_FIRING;
		if (genemy && genemy->client)
		{
			PM_SaberLockResultAnim(genemy, break_type, SABERLOCK_LOSE);
			genemy->client->ps.weaponstate = WEAPON_READY;
		}
	}

	pm->ps->saberLockTime = genemy->client->ps.saberLockTime = 0;
	pm->ps->saberLockEnemy = genemy->client->ps.saberLockEnemy = ENTITYNUM_NONE;
	pm->ps->saberMoveNext = LS_NONE;
	if (genemy && genemy->client)
	{
		genemy->client->ps.saberMoveNext = LS_NONE;
	}

	PM_AddEvent(EV_JUMP);

	if (result == LOCK_STALEMATE)
	{
		//no-one won
		G_AddEvent(genemy, EV_JUMP, 0);
	}
	else
	{
		if (pm->ps->client_num)
		{
			//an NPC
			pm->ps->saberEventFlags |= SEF_LOCK_WON; //tell the winner to press the advantage
		}
		//painDebounceTime will stop them from doing anything
		genemy->painDebounceTime = level.time + genemy->client->ps.torsoAnimTimer + 500;

		if (Q_irand(0, 1))
		{
			G_AddEvent(genemy, EV_PAIN, Q_irand(0, 75));
		}
		else
		{
			if (genemy->NPC)
			{
				genemy->NPC->blockedSpeechDebounceTime = 0;
			}
			G_AddVoiceEvent(genemy, Q_irand(EV_PUSHED1, EV_PUSHED3), 500);
		}
		if (result == LOCK_VICTORY)
		{
			//one person won
			if (Q_irand(FORCE_LEVEL_1, FORCE_LEVEL_2) < pm->ps->forcePowerLevel[FP_SABER_OFFENSE])
			{
				vec3_t throw_dir = { 0, 0, 350 };
				int win_move = pm->ps->saber_move;
				if (!single_vs_single)
				{
					//all others have their own super breaks
					//so it doesn't try to set some other anim below
					win_anim = -1;
				}
				else if (win_anim == BOTH_LK_S_S_S_SB_1_W || win_anim == BOTH_LK_S_S_T_SB_1_W)
				{
					//doing a superbreak on single-vs-single, don't do the old superbreaks this time
					//so it doesn't try to set some other anim below
					win_anim = -1;
				}
				else
				{
					//JK2-style
					switch (win_anim)
					{
					case BOTH_A3_T__B_:
						win_anim = BOTH_D1_TL___;
						win_move = LS_D1_TL;
						//FIXME: mod throwDir?
						break;
					case BOTH_K1_S1_T_:
						//FIXME: mod throwDir?
						break;
					case BOTH_CWCIRCLEBREAK:
						//FIXME: mod throwDir?
						break;
					case BOTH_CCWCIRCLEBREAK:
						win_anim = BOTH_A1_BR_TL;
						win_move = LS_A_BR2TL;
						//FIXME: mod throwDir?
						break;
					default:;
					}
					if (win_anim != BOTH_CCWCIRCLEBREAK)
					{
						if (!genemy->s.number && genemy->health <= 25 //player low on health
							|| genemy->s.number && genemy->client->NPC_class != CLASS_KYLE && genemy->client->
							NPC_class != CLASS_LUKE && genemy->client->NPC_class != CLASS_YODA && genemy->client
							->NPC_class != CLASS_TAVION && genemy->client->NPC_class != CLASS_ALORA && genemy->
							client->NPC_class != CLASS_VADER && genemy->client->NPC_class != CLASS_DESANN
							//any NPC that's not a boss character
							|| genemy->s.number && genemy->health <= 50)
							//boss character with less than 50 health left
						{
							//possibly knock saber out of hand OR cut hand off!
							if (Q_irand(0, 25) < victory_strength
								&& (!genemy->s.number && genemy->health <= 10 || genemy->s.number))
							{
								NPC_SetAnim(genemy, SETANIM_BOTH, BOTH_RIGHTHANDCHOPPEDOFF,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD); //force this
								genemy->client->dismembered = false;

								G_DoDismemberment(genemy, genemy->client->renderInfo.handRPoint, MOD_SABER,
									HL_HAND_RT,
									qtrue);

								if (genemy->health >= 1000)
								{
									G_Damage(genemy, gent, gent, throw_dir, genemy->client->renderInfo.handRPoint,
										genemy->health + AMPUTATE_DAMAGE,
										DAMAGE_NO_PROTECTION | DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK |
										DAMAGE_NO_HIT_LOC, MOD_SABER, HL_NONE);
								}
								else
								{
									G_Damage(genemy, gent, gent, throw_dir, genemy->client->renderInfo.handRPoint,
										genemy->health + 10,
										DAMAGE_NO_PROTECTION | DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK |
										DAMAGE_NO_HIT_LOC, MOD_SABER, HL_NONE);
								}

								if (gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(gent))
								{
									if (d_slowmoaction->integer)
									{
										G_StartStasisEffect_FORCE_LEVEL_1(gent, MEF_NO_SPIN, 200, 0.3f, 0);
									}
									CGCam_BlockShakeSP(0.45f, 100);
								}

								if (genemy->s.number && genemy->health > 1)
								{
									if (genemy->s.number && genemy->health > 100)
									{
										genemy->client->AmputateDamage = 200;
										genemy->client->AmputateTime = level.time + 1000;
									}
									else
									{
										genemy->client->AmputateDamage = 20;
										genemy->client->AmputateTime = level.time + 1000;
									}
								}

								PM_SetAnim(pm, SETANIM_BOTH, win_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
								pm->ps->weaponTime = pm->ps->torsoAnimTimer + 500;
								pm->ps->saber_move = win_move;
								pm->ps->saberBlocked = BLOCKED_NONE;
								pm->ps->weaponstate = WEAPON_FIRING;
								return;
							}
						}
					}
				}
				if (!(genemy->client->ps.saber[0].saberFlags & SFL_NOT_DISARMABLE))
				{
					//add disarmBonus into this check
					victory_strength += pm->ps->SaberDisarmBonus(0) * 2;
					if (genemy->client->ps.saber[0].saberFlags & SFL_TWO_HANDED
						|| genemy->client->ps.dualSabers && genemy->client->ps.saber[1].Active())
					{
						//defender gets a bonus for using a 2-handed saber or 2 sabers
						victory_strength -= 2;
					}
					if (pm->ps->forcePowersActive & 1 << FP_RAGE)
					{
						victory_strength += gent->client->ps.forcePowerLevel[FP_RAGE];
					}
					else if (pm->ps->forceRageRecoveryTime > pm->cmd.serverTime)
					{
						victory_strength--;
					}
					if (genemy->client->ps.forceRageRecoveryTime > pm->cmd.serverTime)
					{
						victory_strength++;
					}
					if (Q_irand(0, 10) < victory_strength)
					{
						if (!(genemy->client->ps.saber[0].saberFlags & SFL_TWO_HANDED)
							|| !Q_irand(0, 1))
						{
							//if it's a two-handed saber, it has a 50% chance of resisting a disarming
							WP_SaberLose(genemy, throw_dir);
							if (win_anim != -1)
							{
								PM_SetAnim(pm, SETANIM_BOTH, win_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
								pm->ps->weaponTime = pm->ps->torsoAnimTimer;
								pm->ps->saber_move = win_move;
								pm->ps->saberBlocked = BLOCKED_NONE;
								pm->ps->weaponstate = WEAPON_FIRING;
							}
						}
					}
				}
			}
		}
	}
}

static int G_SaberLockStrength(const gentity_t* gent)
{
	int strength = gent->client->ps.saber[0].lockBonus;

	if (gent->client->ps.saber[0].saberFlags & SFL_TWO_HANDED)
	{
		strength += 1;
	}
	if (gent->client->ps.dualSabers && gent->client->ps.saber[1].Active())
	{
		strength += 1 + gent->client->ps.saber[1].lockBonus;
	}
	if (gent->client->ps.forcePowersActive & 1 << FP_RAGE)
	{
		strength += gent->client->ps.forcePowerLevel[FP_RAGE];
	}
	else if (gent->client->ps.forceRageRecoveryTime > pm->cmd.serverTime)
	{
		strength--;
	}

	if (gent->client->ps.saberLockHitCheckTime < level.time)
	{//have moved to next frame since last lock push
		if (gent->s.number >= MAX_CLIENTS && !PM_ControlledByPlayer())
		{
			gent->client->ps.saberLockHitCheckTime = level.time + 25; //check for AI pushes much slower.

			if (gent->client->ps.saberLockHitIncrementTime < level.time)
			{//have moved to next frame since last saberlock attack button press
				gent->client->ps.saberLockHitIncrementTime = level.time + 150;//so we don't register an attack key press more than once per server frame

				if (gent->client->NPC_class == CLASS_DESANN || gent->client->NPC_class == CLASS_VADER || gent->client->NPC_class == CLASS_LUKE)
				{
					strength += 2 + gent->client->ps.forcePowerLevel[FP_SABER_OFFENSE] + Q_irand(0, g_spskill->integer);
				}
				else
				{
					strength += gent->client->ps.forcePowerLevel[FP_SABER_OFFENSE] + Q_irand(0, g_spskill->integer);
				}
			}
		}
		else
		{
			pm->ps->saberLockHitCheckTime = level.time + 25;//so we don't push more than once per server frame

			if (gent->client->ps.saberLockHitIncrementTime < level.time)
			{//have moved to next frame since last saberlock attack button press
				gent->client->ps.saberLockHitIncrementTime = level.time + 50;//so we don't register an attack key press more than once per server frame

				if (g_SerenityJediEngineMode->integer)
				{
					if (gent->client->ps.communicatingflags & 1 << CF_SABERLOCK_ADVANCE)
					{
						strength += gent->client->ps.forcePowerLevel[FP_SABER_OFFENSE] + Q_irand(0, g_spskill->integer) + Q_irand(0, pm->cmd.buttons & BUTTON_ATTACK);
					}
					else
					{
						strength += pm->cmd.buttons & BUTTON_ATTACK + Q_irand(0, g_spskill->integer);
					}
				}
				else
				{
					strength += gent->client->ps.forcePowerLevel[FP_SABER_OFFENSE] + Q_irand(0, g_spskill->integer) + Q_irand(0, 1);
				}
			}
		}
	}
	return strength;
}

static qboolean PM_InSaberLockOld(const int anim)
{
	switch (anim)
	{
	case BOTH_BF2LOCK:
	case BOTH_BF1LOCK:
	case BOTH_CWCIRCLELOCK:
	case BOTH_CCWCIRCLELOCK:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_InSaberLock(const int anim)
{
	switch (anim)
	{
	case BOTH_LK_S_DL_S_L_1: //lock if I'm using single vs. a dual
	case BOTH_LK_S_DL_T_L_1: //lock if I'm using single vs. a dual
	case BOTH_LK_S_ST_S_L_1: //lock if I'm using single vs. a staff
	case BOTH_LK_S_ST_T_L_1: //lock if I'm using single vs. a staff
	case BOTH_LK_S_S_S_L_1: //lock if I'm using single vs. a single and I initiated
	case BOTH_LK_S_S_T_L_1: //lock if I'm using single vs. a single and I initiated
	case BOTH_LK_DL_DL_S_L_1: //lock if I'm using dual vs. dual and I initiated
	case BOTH_LK_DL_DL_T_L_1: //lock if I'm using dual vs. dual and I initiated
	case BOTH_LK_DL_ST_S_L_1: //lock if I'm using dual vs. a staff
	case BOTH_LK_DL_ST_T_L_1: //lock if I'm using dual vs. a staff
	case BOTH_LK_DL_S_S_L_1: //lock if I'm using dual vs. a single
	case BOTH_LK_DL_S_T_L_1: //lock if I'm using dual vs. a single
	case BOTH_LK_ST_DL_S_L_1: //lock if I'm using staff vs. dual
	case BOTH_LK_ST_DL_T_L_1: //lock if I'm using staff vs. dual
	case BOTH_LK_ST_ST_S_L_1: //lock if I'm using staff vs. a staff and I initiated
	case BOTH_LK_ST_ST_T_L_1: //lock if I'm using staff vs. a staff and I initiated
	case BOTH_LK_ST_S_S_L_1: //lock if I'm using staff vs. a single
	case BOTH_LK_ST_S_T_L_1: //lock if I'm using staff vs. a single
	case BOTH_LK_S_S_S_L_2:
	case BOTH_LK_S_S_T_L_2:
	case BOTH_LK_DL_DL_S_L_2:
	case BOTH_LK_DL_DL_T_L_2:
	case BOTH_LK_ST_ST_S_L_2:
	case BOTH_LK_ST_ST_T_L_2:
		return qtrue;
	default:
		return PM_InSaberLockOld(anim);
	}
	//return qfalse;
}

extern qboolean ValidAnimFileIndex(int index);
extern qboolean G_CheckIncrementLockAnim(int anim, int win_or_lose);

static qboolean PM_SaberLocked()
{
	if (pm->ps->saberLockEnemy == ENTITYNUM_NONE)
	{
		if (PM_InSaberLock(pm->ps->torsoAnim))
		{
			//wtf?  Maybe enemy died?
			PM_SaberLockWinAnim(LOCK_STALEMATE, SABERLOCK_BREAK);
		}
		return qfalse;
	}
	gentity_t* gent = pm->gent;
	const int index_end = Q_irand(1, 5);
	if (!gent)
	{
		return qfalse;
	}
	gentity_t* genemy = &g_entities[pm->ps->saberLockEnemy];
	if (!genemy)
	{
		return qfalse;
	}
	if (PM_InSaberLock(pm->ps->torsoAnim) && PM_InSaberLock(genemy->client->ps.torsoAnim))
	{
		if (pm->ps->saberLockTime <= level.time + 500
			&& pm->ps->saberLockEnemy != ENTITYNUM_NONE)
		{
			//lock just ended
			const int strength = G_SaberLockStrength(gent);
			const int e_strength = G_SaberLockStrength(genemy);
			if (strength > 1 && e_strength > 1 && !Q_irand(0, abs(strength - e_strength) + 1))
			{
				//both knock each other down!
				PM_SaberLockBreak(gent, genemy, LOCK_DRAW, 0);
				G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
			}
			else
			{
				//both "win"
				PM_SaberLockBreak(gent, genemy, LOCK_STALEMATE, 0);
				G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
			}
			return qtrue;
		}

		if (pm->ps->saberLockTime < level.time)
		{
			//done... tie breaker above should have handled this, but...?
			if (PM_InSaberLock(pm->ps->torsoAnim) && pm->ps->torsoAnimTimer > 0)
			{
				pm->ps->torsoAnimTimer = 0;
				G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
			}
			if (PM_InSaberLock(pm->ps->legsAnim) && pm->ps->legsAnimTimer > 0)
			{
				pm->ps->legsAnimTimer = 0;
				G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
			}
			return qfalse;
		}
		if (pm->cmd.buttons & BUTTON_ATTACK)
		{
			//holding attack
			if (!(pm->ps->pm_flags & PMF_ATTACK_HELD))
			{
				//tapping
				int remaining;
				if (ValidAnimFileIndex(gent->client->clientInfo.animFileIndex))
				{
					float current_frame, junk2;
					int cur_frame, junk;
					int strength = G_SaberLockStrength(gent);
					const animation_t* anim = &level.knownAnimFileSets[gent->client->clientInfo.animFileIndex].
						animations[pm->ps->torsoAnim];

#ifdef _DEBUG
					const qboolean ret =
#endif
						gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone,
							cg.time ? cg.time : level.time, &current_frame, &junk, &junk,
							&junk,
							&junk2, nullptr);
#ifdef _DEBUG
					assert(ret); // this would be pretty bad, the below code seems to assume the call succeeds. -gil
#endif

					if (PM_InSaberLockOld(pm->ps->torsoAnim))
					{
						//old locks
						if (pm->ps->torsoAnim == BOTH_CCWCIRCLELOCK ||
							pm->ps->torsoAnim == BOTH_BF2LOCK)
						{
							cur_frame = floor(current_frame) - strength;
							//drop my frame one
							if (cur_frame <= anim->firstFrame)
							{
								//I won!  Break out
								PM_SaberLockBreak(gent, genemy, LOCK_VICTORY, strength);
								G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
								return qtrue;
							}
							PM_SetAnimFrame(gent, cur_frame, qtrue, qtrue);
							remaining = cur_frame - anim->firstFrame;
							if (d_saberCombat->integer)
							{
								Com_Printf("%s pushing in saber lock, %d frames to go!\n", gent->NPC_type,
									remaining);
							}
						}
						else
						{
							cur_frame = ceil(current_frame) + strength;
							//advance my frame one
							if (cur_frame >= anim->firstFrame + anim->numFrames)
							{
								//I won!  Break out
								PM_SaberLockBreak(gent, genemy, LOCK_VICTORY, strength);
								G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
								return qtrue;
							}
							PM_SetAnimFrame(gent, cur_frame, qtrue, qtrue);
							remaining = anim->firstFrame + anim->numFrames - cur_frame;
							if (d_saberCombat->integer)
							{
								Com_Printf("%s pushing in saber lock, %d frames to go!\n", gent->NPC_type,
									remaining);
							}
						}
					}
					else
					{
						//new locks
						if (G_CheckIncrementLockAnim(pm->ps->torsoAnim, SABERLOCK_WIN))
						{
							cur_frame = ceil(current_frame) + strength;
							//advance my frame one
							if (cur_frame >= anim->firstFrame + anim->numFrames)
							{
								//I won!  Break out
								PM_SaberLockBreak(gent, genemy, LOCK_VICTORY, strength);
								G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
								return qtrue;
							}
							PM_SetAnimFrame(gent, cur_frame, qtrue, qtrue);
							remaining = anim->firstFrame + anim->numFrames - cur_frame;
							if (d_saberCombat->integer)
							{
								Com_Printf("%s pushing in saber lock, %d frames to go!\n", gent->NPC_type,
									remaining);
							}
						}
						else
						{
							cur_frame = floor(current_frame) - strength;
							//drop my frame one
							if (cur_frame <= anim->firstFrame)
							{
								//I won!  Break out
								PM_SaberLockBreak(gent, genemy, LOCK_VICTORY, strength);
								G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
								return qtrue;
							}
							PM_SetAnimFrame(gent, cur_frame, qtrue, qtrue);
							remaining = cur_frame - anim->firstFrame;
							if (d_saberCombat->integer)
							{
								Com_Printf("%s pushing in saber lock, %d frames to go!\n", gent->NPC_type,
									remaining);
							}
						}
					}
					if (!Q_irand(0, 2))
					{
						if (pm->ps->client_num < MAX_CLIENTS && g_saberLockCinematicCamera->integer < 1)
						{
							if (!Q_irand(0, 3))
							{
								//PM_AddEvent(EV_JUMP);
							}
							else
							{
								PM_AddEvent(Q_irand(EV_PUSHED1, EV_PUSHED3));
							}
						}
						else
						{
							if (!Q_irand(0, 2) && gent->NPC && gent->NPC->blockedSpeechDebounceTime < level.time)
							{
								switch (Q_irand(0, 3))
								{
								case 0:
									PM_AddEvent(EV_JUMP);
									gent->NPC->blockedSpeechDebounceTime = level.time + 10000;
									break;
								case 1:
									PM_AddEvent(Q_irand(EV_ANGER1, EV_ANGER3));
									gent->NPC->blockedSpeechDebounceTime = level.time + 10000;
									break;
								case 2:
									PM_AddEvent(Q_irand(EV_TAUNT1, EV_TAUNT5));
									gent->NPC->blockedSpeechDebounceTime = level.time + 10000;
									break;
								case 3:
									PM_AddEvent(Q_irand(EV_GLOAT1, EV_GLOAT3));
									gent->NPC->blockedSpeechDebounceTime = level.time + 10000;
									break;
								default:;
								}
							}
						}
					}
				}
				else
				{
					return qfalse;
				}

				if (ValidAnimFileIndex(genemy->client->clientInfo.animFileIndex))
				{
					const animation_t* anim = &level.knownAnimFileSets[genemy->client->clientInfo.animFileIndex].animations[genemy->client->ps.torsoAnim];

					if (!Q_irand(0, 2) && genemy->NPC && genemy->NPC->blockedSpeechDebounceTime < level.time)
					{
						switch (Q_irand(0, 3))
						{
						case 0:
							G_AddEvent(genemy, EV_PAIN, floor(static_cast<float>(genemy->health) / genemy->max_health * 100.0f));
							genemy->NPC->blockedSpeechDebounceTime = level.time + 10000;
							break;
						case 1:
							G_AddVoiceEvent(genemy, Q_irand(EV_PUSHED1, EV_PUSHED3), 10000);
							genemy->NPC->blockedSpeechDebounceTime = level.time + 10000;
							break;
						case 2:
							G_AddVoiceEvent(genemy, Q_irand(EV_CHOKE1, EV_CHOKE3), 10000);
							genemy->NPC->blockedSpeechDebounceTime = level.time + 10000;
							break;
						case 3:
							G_AddVoiceEvent(genemy, EV_PUSHFAIL, 10000);
							genemy->NPC->blockedSpeechDebounceTime = level.time + 10000;
							break;
						default:;
						}
					}

					if (PM_InSaberLockOld(genemy->client->ps.torsoAnim))
					{
						if (genemy->client->ps.torsoAnim == BOTH_CCWCIRCLELOCK ||
							genemy->client->ps.torsoAnim == BOTH_BF2LOCK)
						{
							PM_SetAnimFrame(genemy, anim->firstFrame + anim->numFrames - remaining, qtrue, qtrue);
						}
						else
						{
							PM_SetAnimFrame(genemy, anim->firstFrame + remaining, qtrue, qtrue);
						}
					}
					else
					{
						//new locks
						//???
						if (G_CheckIncrementLockAnim(genemy->client->ps.torsoAnim, SABERLOCK_LOSE))
						{
							PM_SetAnimFrame(genemy, anim->firstFrame + anim->numFrames - remaining, qtrue, qtrue);
						}
						else
						{
							PM_SetAnimFrame(genemy, anim->firstFrame + remaining, qtrue, qtrue);
						}
					}
				}
			}
		}
		else
		{
			//FIXME: other ways out of a saberlock?
			//force-push?  (requires more force power?)
			//kick?  (requires anim ... hit jump key?)
			//roll?
			//backflip?
		}
	}
	else
	{
		//something broke us out of it
		if (gent->painDebounceTime > level.time && genemy->painDebounceTime > level.time)
		{
			PM_SaberLockBreak(gent, genemy, LOCK_DRAW, 0);
			G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
		}
		else if (gent->painDebounceTime > level.time)
		{
			PM_SaberLockBreak(genemy, gent, LOCK_VICTORY, 0);
			G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
		}
		else if (genemy->painDebounceTime > level.time)
		{
			PM_SaberLockBreak(gent, genemy, LOCK_VICTORY, 0);
			G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
		}
		else
		{
			PM_SaberLockBreak(gent, genemy, LOCK_STALEMATE, 0);
			G_Sound(pm->gent, G_SoundIndex(va("sound/weapons/saber/saber_locking_end%d.mp3", index_end)));
		}
	}
	return qtrue;
}

static qboolean G_EnemyInKickRange(const gentity_t* self, const gentity_t* enemy)
{
	if (!self || !enemy)
	{
		return qfalse;
	}
	if (fabs(self->currentOrigin[2] - enemy->currentOrigin[2]) < 32)
	{
		//generally at same height
		if (DistanceHorizontal(self->currentOrigin, enemy->currentOrigin) <= STAFF_KICK_RANGE + 8.0f + self->maxs[
			0] * 1.5f + enemy->maxs[0] * 1.5f)
		{
			//within kicking range!
			return qtrue;
		}
	}
	return qfalse;
}

qboolean G_CanKickEntity(const gentity_t* self, const gentity_t* target)
{
	if (target && target->client
		&& !PM_InKnockDown(&target->client->ps)
		&& !PM_SaberInMassiveBounce(self->client->ps.torsoAnim)
		&& G_EnemyInKickRange(self, target))
	{
		return qtrue;
	}
	return qfalse;
}

static float PM_GroundDistance()
{
	trace_t tr;
	vec3_t down;

	VectorCopy(pm->ps->origin, down);

	down[2] -= 4096;

	pm->trace(&tr, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->client_num, pm->tracemask,
		static_cast<EG2_Collision>(0), 0);

	VectorSubtract(pm->ps->origin, tr.endpos, down);

	return VectorLength(down);
}

static float G_GroundDistance(const gentity_t* self)
{
	if (!self)
	{
		//wtf?!!
		return Q3_INFINITE;
	}
	trace_t tr;
	vec3_t down;

	VectorCopy(self->currentOrigin, down);

	down[2] -= 4096;

	gi.trace(&tr, self->currentOrigin, self->mins, self->maxs, down, self->s.number, self->clipmask,
		static_cast<EG2_Collision>(0), 0);

	VectorSubtract(self->currentOrigin, tr.endpos, down);

	return VectorLength(down);
}

saber_moveName_t G_PickAutoKick(const gentity_t* self, const gentity_t* enemy, const qboolean store_move)
{
	saber_moveName_t kick_move = LS_NONE;
	if (!self || !self->client)
	{
		return LS_NONE;
	}
	if (!enemy)
	{
		return LS_NONE;
	}
	if (PM_SaberInMassiveBounce(self->client->ps.torsoAnim) || PM_SaberInBashedAnim(self->client->ps.torsoAnim))
	{
		return LS_NONE;
	}
	vec3_t v_fwd, v_rt, enemy_dir;
	const vec3_t fwd_angs = { 0, self->client->ps.viewangles[YAW], 0 };
	VectorSubtract(enemy->currentOrigin, self->currentOrigin, enemy_dir);
	VectorNormalize(enemy_dir); //not necessary, I guess, but doesn't happen often
	AngleVectors(fwd_angs, v_fwd, v_rt, nullptr);
	const float f_dot = DotProduct(enemy_dir, v_fwd);
	const float r_dot = DotProduct(enemy_dir, v_rt);
	if (fabs(r_dot) > 0.5f && fabs(f_dot) < 0.5f)
	{
		//generally to one side
		if (r_dot > 0)
		{
			//kick right
			if (g_SerenityJediEngineMode->integer == 2 && self->client->ps.weapon == WP_SABER && self->client->ps.
				SaberActive())
			{
				kick_move = LS_SLAP_R;
			}
			else
			{
				kick_move = LS_KICK_R;
			}
		}
		else
		{
			//kick left
			if (g_SerenityJediEngineMode->integer == 2 && self->client->ps.weapon == WP_SABER && self->client->ps.
				SaberActive())
			{
				kick_move = LS_SLAP_L;
			}
			else
			{
				kick_move = LS_KICK_L;
			}
		}
	}
	else if (fabs(f_dot) > 0.5f && fabs(r_dot) < 0.5f)
	{
		//generally in front or behind us
		if (f_dot > 0)
		{
			//kick fwd
			if (self->client->ps.groundEntityNum != ENTITYNUM_NONE && g_SerenityJediEngineMode->integer == 2 && self
				->client->ps.weapon == WP_SABER && self->client->ps.SaberActive())
			{
				if (G_CheckEnemyPresence(self, DIR_FRONT, 64, 12.0f))
				{
					if (self->client->ps.saberFatigueChainCount >= MISHAPLEVEL_TEN)
					{
						kick_move = LS_SMACK_L;
					}
					else
					{
						kick_move = LS_HILT_BASH;
					}
				}
				else
				{
					kick_move = LS_KICK_F_MD;
				}
			}
			else
			{
				kick_move = LS_KICK_F;
			}
		}
		else
		{
			//kick back
			if (PM_CrouchingAnim(self->client->ps.legsAnim))
			{
				kick_move = LS_KICK_SWEEP;
			}
			else
			{
				kick_move = LS_KICK_B;
			}
		}
	}
	else
	{
		//diagonal to us, kick would miss
	}
	if (kick_move != LS_NONE)
	{
		//have a valid one to do
		if (self->client->ps.groundEntityNum == ENTITYNUM_NONE)
		{
			//if in air, convert kick to an in-air kick
			const float g_dist = G_GroundDistance(self);
			//let's only allow air kicks if a certain distance from the ground
			//it's silly to be able to do them right as you land.
			//also looks wrong to transition from a non-complete flip anim...
			if ((!PM_FlippingAnim(self->client->ps.legsAnim) || self->client->ps.legsAnimTimer <= 0) &&
				g_dist > 64.0f && //strict minimum
				g_dist > -self->client->ps.velocity[2] - 64.0f)
				//make sure we are high to ground relative to downward velocity as well
			{
				switch (kick_move)
				{
				case LS_KICK_F:
					kick_move = LS_KICK_F_AIR;
					break;
				case LS_KICK_F_MD:
					kick_move = LS_KICK_F_AIR;
					break;
				case LS_KICK_B:
					kick_move = LS_KICK_B_AIR;
					break;
				case LS_KICK_SWEEP:
					kick_move = LS_KICK_B_AIR;
					break;
				case LS_KICK_R:
					kick_move = LS_KICK_R_AIR;
					break;
				case LS_SMACK_R:
					kick_move = LS_KICK_R_AIR;
					break;
				case LS_SLAP_R:
					kick_move = LS_KICK_R_AIR;
					break;
				case LS_KICK_L:
					kick_move = LS_KICK_L_AIR;
					break;
				case LS_SMACK_L:
					kick_move = LS_KICK_L_AIR;
					break;
				case LS_SLAP_L:
					kick_move = LS_KICK_L_AIR;
					break;
				default: //oh well, can't do any other kick move while in-air
					kick_move = LS_NONE;
					break;
				}
			}
			else
			{
				//leave it as a normal kick unless we're too high up
				if (g_dist > 128.0f || self->client->ps.velocity[2] >= 0)
				{
					//off ground, but too close to ground
					kick_move = LS_NONE;
				}
			}
		}
		if (store_move)
		{
			self->client->ps.saberMoveNext = kick_move;
		}
	}
	return kick_move;
}

static saber_moveName_t PM_PickAutoKick(const gentity_t* enemy)
{
	return G_PickAutoKick(pm->gent, enemy, qfalse);
}

saber_moveName_t g_pick_auto_multi_kick(gentity_t* self, const qboolean allow_singles, const qboolean store_move)
{
	gentity_t* entity_list[MAX_GENTITIES];
	vec3_t mins{}, maxs{};
	const int radius = self->maxs[0] * 1.5f + self->maxs[0] * 1.5f + STAFF_KICK_RANGE + 24.0f;
	//a little wide on purpose
	vec3_t center;
	saber_moveName_t kick_move, best_kick = LS_NONE;
	gentity_t* best_ent = nullptr;
	int enemies_front = 0;
	int enemies_back = 0;
	int enemies_right = 0;
	int enemies_left = 0;
	int enemies_spin = 0;

	if (!self || !self->client)
	{
		return LS_NONE;
	}
	if (PM_SaberInMassiveBounce(self->client->ps.torsoAnim) || PM_SaberInBashedAnim(self->client->ps.torsoAnim))
	{
		return LS_NONE;
	}

	VectorCopy(self->currentOrigin, center);

	for (int i = 0; i < 3; i++)
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	const int num_listed_entities = gi.EntitiesInBox(mins, maxs, entity_list, MAX_GENTITIES);

	for (int e = 0; e < num_listed_entities; e++)
	{
		gentity_t* ent = entity_list[e];

		if (ent == self)
			continue;
		if (ent->owner == self)
			continue;
		if (!ent->inuse)
			continue;
		//not a client?
		if (!ent->client)
			continue;
		//ally?
		if (ent->client->playerTeam == self->client->playerTeam)
			continue;
		//dead?
		if (ent->health <= 0)
			continue;
		//too far?
		const float dist_to_ent = DistanceSquared(ent->currentOrigin, center);
		if (dist_to_ent > radius * radius)
			continue;
		kick_move = G_PickAutoKick(self, ent, qfalse);

		if (kick_move == LS_KICK_F_AIR
			|| kick_move == LS_FLYING_KICK
			|| kick_move == LS_KICK_B_AIR
			|| kick_move == LS_KICK_R_AIR
			|| kick_move == LS_KICK_L_AIR)
		{
			//in air?  Can't do multikicks
		}
		else
		{
			switch (kick_move)
			{
			case LS_KICK_F:
			case LS_KICK_F_MD:
				enemies_front++;
				break;
			case LS_KICK_B:
			case LS_KICK_SWEEP:
				enemies_back++;
				break;
			case LS_KICK_R:
				enemies_right++;
				break;
			case LS_KICK_L:
				enemies_left++;
				break;
			default:
				enemies_spin++;
				break;
			}
		}
		if (allow_singles)
		{
			constexpr float best_dist_to_ent = Q3_INFINITE;
			if (kick_move != LS_NONE
				&& dist_to_ent < best_dist_to_ent)
			{
				best_kick = kick_move;
				best_ent = ent;
			}
		}
	}
	kick_move = LS_NONE;

	if (self->client->ps.groundEntityNum != ENTITYNUM_NONE)
	{
		//can't do the multikicks in air
		if (enemies_front && enemies_back
			&& enemies_front + enemies_back - (enemies_right + enemies_left) > 1)
		{
			//more enemies in front/back than left/right
			if (self->client->NPC_class == CLASS_VADER)
			{
				kick_move = LS_KICK_F;
			}
			else
			{
				kick_move = LS_KICK_BF;
			}
		}
		else if (enemies_right && enemies_left
			&& enemies_right + enemies_left - (enemies_front + enemies_back) > 1)
		{
			//more enemies on left & right than front/back
			if (self->client->NPC_class == CLASS_VADER)
			{
				kick_move = LS_KICK_R;
			}
			else
			{
				kick_move = LS_KICK_RL;
			}
		}
		else if ((enemies_front || enemies_back) && (enemies_right || enemies_left))
		{
			//at least 2 enemies around us, not aligned
			if (self->client->NPC_class == CLASS_VADER)
			{
				kick_move = LS_KICK_SWEEP;
			}
			else
			{
				kick_move = LS_KICK_S;
			}
		}
		else if (enemies_spin > 1)
		{
			//at least 2 enemies around us, not aligned
			if (self->client->NPC_class == CLASS_VADER)
			{
				kick_move = LS_KICK_SWEEP;
			}
			else
			{
				kick_move = LS_KICK_S;
			}
		}
	}
	if (kick_move == LS_NONE
		&& best_kick != LS_NONE)
	{
		//no good multi-kick move, but we do have a nice single-kick we found
		kick_move = best_kick;
		//get mad at him so he knows he's being targetted
		if ((self->s.number < MAX_CLIENTS || G_ControlledByPlayer(self))
			&& best_ent != nullptr)
		{
			//player
			G_SetEnemy(self, best_ent);
		}
	}
	if (kick_move != LS_NONE)
	{
		if (store_move)
		{
			self->client->ps.saberMoveNext = kick_move;
		}
	}
	return kick_move;
}

static qboolean PM_PickAutoMultiKick(const qboolean allow_singles)
{
	const saber_moveName_t kick_move = g_pick_auto_multi_kick(pm->gent, allow_singles, qfalse);
	if (kick_move != LS_NONE)
	{
		PM_SetSaberMove(kick_move);
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_CheckKickAttack()
{
	if (pm->cmd.buttons & BUTTON_KICK
		&& !(pm->cmd.buttons & BUTTON_DASH)
		&& pm->gent->client->NPC_class != CLASS_DROIDEKA
		&& !(pm->ps->forcePowersActive & 1 << FP_LIGHTNING)
		&& (!PM_FlippingAnim(pm->ps->legsAnim) || pm->ps->legsAnimTimer <= 250))
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_CheckAltKickAttack()
{
	if (pm->cmd.buttons & BUTTON_ALT_ATTACK || pm->cmd.buttons & BUTTON_KICK
		&& !(pm->cmd.buttons & BUTTON_DASH)
		&& pm->gent->client->NPC_class != CLASS_DROIDEKA
		&& (!PM_FlippingAnim(pm->ps->legsAnim) || pm->ps->legsAnimTimer <= 250))
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_CheckUpsideDownAttack()
{
	if (pm->ps->saber_move != LS_READY)
	{
		return qfalse;
	}
	if (!(pm->cmd.buttons & BUTTON_ATTACK))
	{
		return qfalse;
	}
	if (pm->ps->saber_anim_level < SS_FAST
		|| pm->ps->saber_anim_level > SS_STRONG)
	{
		return qfalse;
	}
	if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
	{
		//FIXME: check ranks?
		return qfalse;
	}

	switch (pm->ps->legsAnim)
	{
	case BOTH_WALL_RUN_RIGHT_FLIP:
	case BOTH_WALL_RUN_LEFT_FLIP:
	case BOTH_WALL_FLIP_RIGHT:
	case BOTH_WALL_FLIP_LEFT:
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_FLIP_BACK3:
	case BOTH_WALL_FLIP_BACK1:
	case BOTH_ALORA_FLIP_B:
		//JKA
	case BOTH_FORCEWALLRUNFLIP_END:
	{
		const float anim_length = PM_AnimLength(pm->gent->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(pm->ps->legsAnim));
		const float elapsed_time = anim_length - pm->ps->legsAnimTimer;
		const float mid_point = anim_length / 2.0f;
		if (elapsed_time < mid_point - 100.0f
			|| elapsed_time > mid_point + 100.0f)
		{
			//only a 200ms window (in middle of anim) of opportunity to do this move in these anims
			return qfalse;
		}
	}
	//NOTE: falls through on purpose
	case BOTH_FLIP_HOLD7:
		pm->ps->pm_flags |= PMF_SLOW_MO_FALL;
		PM_SetSaberMove(LS_UPSIDE_DOWN_ATTACK);
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_SaberMoveOkayForKata()
{
	if (g_saberNewControlScheme->integer)
	{
		if (pm->ps->saber_move == LS_READY //not doing anything
			|| PM_SaberInReflect(pm->ps->saber_move)) //interrupt a projectile blocking move
		{
			return qtrue;
		}
		return qfalse;
	}
	//old control scheme, allow it to interrupt a start or ready
	if (pm->ps->saber_move == LS_READY
		|| PM_SaberInReflect(pm->ps->saber_move) //interrupt a projectile blocking move
		|| PM_SaberInStart(pm->ps->saber_move))
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_CanDoKata()
{
	const qboolean is_holding_block_button = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;
	//Holding Block Button
	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	if (is_holding_block_button || active_blocking)
	{
		return qfalse;
	}

	if (g_SerenityJediEngineMode->integer)
	{
		if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer() //npc kata restrictions
			&& (g_SerenityJediEngineMode->integer == 2 && pm->ps->blockPoints <= BLOCKPOINTS_FOURTY
				|| g_SerenityJediEngineMode->integer == 1 && pm->ps->forcePower <= BLOCKPOINTS_FOURTY))
		{
			return qfalse;
		}
	}

	if (!pm->ps->saberInFlight //not throwing saber
		&& PM_SaberMoveOkayForKata()
		&& !PM_SaberInKata(static_cast<saber_moveName_t>(pm->ps->saber_move))
		&& !PM_InKataAnim(pm->ps->legsAnim)
		&& !PM_InKataAnim(pm->ps->torsoAnim)
		&& !PM_FaceProtectAnim(pm->ps->legsAnim)
		&& !PM_FaceProtectAnim(pm->ps->torsoAnim)
		&& pm->ps->groundEntityNum != ENTITYNUM_NONE //not in the air
		&& pm->cmd.buttons & BUTTON_ATTACK //pressing attack
		&& pm->cmd.forwardmove >= 0 //not moving back (used to be !pm->cmd.forwardmove)
		&& !pm->cmd.rightmove //not moving r/l
		&& pm->cmd.upmove <= 0 //not jumping...?
		&& G_TryingKataAttack(&pm->cmd)
		&& pm->ps->damageTime < cg.time //Not Already taking damage
		&& pm->ps->saberFatigueChainCount < MISHAPLEVEL_TEN
		&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_KATA_ATTACK_POWER, qtrue)) // have enough power
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_CanDoRollStab()
{
	if (!pm->ps->saberInFlight //not throwing saber
		&& pm->ps->groundEntityNum != ENTITYNUM_NONE //not in the air
		&& pm->cmd.buttons & BUTTON_ATTACK //pressing attack
		&& pm->cmd.forwardmove >= 0 //not moving back (used to be !pm->cmd.forwardmove)
		&& !pm->cmd.rightmove //not moving r/l
		&& pm->cmd.upmove <= 0 //not jumping...?
		&& (!(pm->ps->saber[0].saberFlags & SFL_NO_ROLL_STAB)
			&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_ROLL_STAB)))
		&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_KATA_ATTACK_POWER, qtrue)) // have enough power
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_Can_Do_Kill_Move()
{
	if (!pm->ps->saberInFlight //not throwing saber
		&& pm->cmd.buttons & BUTTON_ATTACK //pressing attack
		&& pm->cmd.forwardmove >= 0 //not moving back (used to be !pm->cmd.forwardmove)
		&& !pm->cmd.rightmove //not moving r/l
		&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB, qtrue)) // have enough power
	{
		return qtrue;
	}
	return qfalse;
}

static void PM_SaberDroidWeapon()
{
	// make weapon function
	if (pm->ps->weaponTime > 0)
	{
		pm->ps->weaponTime -= pml.msec;
		if (pm->ps->weaponTime <= 0)
		{
			pm->ps->weaponTime = 0;
		}
	}

	// Now we react to a block action by the player's lightsaber.
	if (pm->ps->saberBlocked)
	{
		switch (pm->ps->saberBlocked)
		{
		case BLOCKED_PARRY_BROKEN:
			PM_SetAnim(pm, SETANIM_BOTH, Q_irand(BOTH_PAIN1, BOTH_PAIN3),
				SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_ATK_BOUNCE:
			PM_SetAnim(pm, SETANIM_BOTH, Q_irand(BOTH_PAIN1, BOTH_PAIN3),
				SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_UPPER_RIGHT:
		case BLOCKED_UPPER_RIGHT_PROJ:
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_P1_S1_TR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->legsAnimTimer += Q_irand(200, 1000);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_LOWER_RIGHT:
		case BLOCKED_LOWER_RIGHT_PROJ:
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_P1_S1_BR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->legsAnimTimer += Q_irand(200, 1000);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_UPPER_LEFT:
		case BLOCKED_UPPER_LEFT_PROJ:
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_P1_S1_TL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->legsAnimTimer += Q_irand(200, 1000);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_LOWER_LEFT:
		case BLOCKED_LOWER_LEFT_PROJ:
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_P1_S1_BL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->legsAnimTimer += Q_irand(200, 1000);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_TOP:
		case BLOCKED_TOP_PROJ:
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_P1_S1_T_, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->legsAnimTimer += Q_irand(200, 1000);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		case BLOCKED_BACK:
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_P1_S1_B_, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			pm->ps->legsAnimTimer += Q_irand(200, 1000);
			pm->ps->weaponTime = pm->ps->legsAnimTimer;
			break;
		default:
			pm->ps->saberBlocked = BLOCKED_NONE;
			break;
		}

		pm->ps->saberBlocked = BLOCKED_NONE;
		pm->ps->saberBounceMove = LS_NONE;
		pm->ps->weaponstate = WEAPON_READY;
	}
}

static void PM_TryGrab()
{
	if (pm->ps->groundEntityNum != ENTITYNUM_NONE
		//&& !pm->ps->saberInFlight
		&& pm->ps->weaponTime <= 0) //< 200 )
	{
		PM_SetAnim(pm, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		pm->ps->torsoAnimTimer += 200;
		pm->ps->weaponTime = pm->ps->torsoAnimTimer;
		pm->ps->saber_move = pm->ps->saberMoveNext = LS_READY;
		VectorClear(pm->ps->velocity);
		VectorClear(pm->ps->moveDir);
		pm->cmd.rightmove = pm->cmd.forwardmove = pm->cmd.upmove = 0;
		if (pm->gent)
		{
			pm->gent->painDebounceTime = level.time + pm->ps->torsoAnimTimer;
		}
		pm->ps->SaberDeactivate();
	}
}

static void PM_TryAirKick(const saber_moveName_t kick_move)
{
	if (pm->ps->groundEntityNum < ENTITYNUM_NONE)
	{
		//just do it
		PM_SetSaberMove(kick_move);
	}
	else
	{
		const float g_dist = PM_GroundDistance();
		//let's only allow air kicks if a certain distance from the ground
		//it's silly to be able to do them right as you land.
		//also looks wrong to transition from a non-complete flip anim...
		if ((!PM_FlippingAnim(pm->ps->legsAnim) || pm->ps->legsAnimTimer <= 0) &&
			g_dist > 64.0f && //strict minimum
			g_dist > -pm->ps->velocity[2] - 64.0f)
		{
			PM_SetSaberMove(kick_move);
		}
		else
		{
			//leave it as a normal kick unless we're too high up
			if (g_dist > 128.0f || pm->ps->velocity[2] >= 0)
			{
				//off ground, but too close to ground
			}
			else
			{
				//high close enough to ground to do a normal kick, convert it
				switch (kick_move)
				{
				case LS_KICK_F_AIR:
				case LS_FLYING_KICK:
					PM_SetSaberMove(LS_KICK_F);
					break;
				case LS_KICK_B_AIR:
					PM_SetSaberMove(LS_KICK_B);
					break;
				case LS_KICK_R_AIR:
					PM_SetSaberMove(LS_KICK_R);
					break;
				case LS_KICK_L_AIR:
					PM_SetSaberMove(LS_KICK_L);
					break;
				default:
					break;
				}
			}
		}
	}
}

static void PM_CheckKick()
{
	if (pm->gent && (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent))) //player only restrictions
	{
		if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive() && !pm->ps->saberInFlight)
		{
			if (!(pm->ps->communicatingflags & 1 << KICKING))
			{
				return;
			}
		}
	}

	if (!PM_SaberInBounce(pm->ps->saber_move)
		&& !PM_SaberInKnockaway(pm->ps->saber_move)
		&& !PM_SaberInBrokenParry(pm->ps->saber_move)
		&& !PM_KickMove(pm->ps->saber_move)
		&& !PM_KickingAnim(pm->ps->torsoAnim)
		&& !PM_KickingAnim(pm->ps->legsAnim)
		&& !PM_InRoll(pm->ps)
		&& !PM_InKnockDown(pm->ps)) //not already in a kick
	{
		//player kicks
		if (pm->cmd.rightmove)
		{
			//kick to side
			if (pm->cmd.rightmove > 0)
			{
				//kick right
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					PM_TryAirKick(LS_KICK_R_AIR);
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
						{
							PM_SetSaberMove(LS_SMACK_R);
						}
						else
						{
							PM_SetSaberMove(LS_KICK_R);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_R);
					}
				}
				//pm->ps->userInt3 |= (1 << FLAG_SLAPPING);
			}
			else
			{
				//kick left
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					PM_TryAirKick(LS_KICK_L_AIR);
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
						{
							PM_SetSaberMove(LS_SMACK_L);
						}
						else
						{
							PM_SetSaberMove(LS_KICK_L);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_L);
					}
				}
				//pm->ps->userInt3 |= (1 << FLAG_SLAPPING);
			}
			pm->cmd.rightmove = 0;
		}
		else if (pm->cmd.forwardmove)
		{
			//kick front/back
			if (pm->cmd.forwardmove > 0)
			{
				//kick fwd
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						PM_TryAirKick(LS_FLYING_KICK);
					}
					else
					{
						PM_TryAirKick(LS_KICK_F_AIR);
					}
				}
				else if (pm->ps->groundEntityNum != ENTITYNUM_NONE && g_SerenityJediEngineMode->integer == 2 && pm->
					ps->weapon == WP_SABER && pm->ps->SaberActive())
				{
					if (G_CheckEnemyPresence(pm->gent, DIR_FRONT, 64, 12.0f))
					{
						if (pm->ps->saberFatigueChainCount >= MISHAPLEVEL_TEN)
						{
							PM_SetSaberMove(LS_SMACK_L);
						}
						else
						{
							PM_SetSaberMove(LS_HILT_BASH);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_F_MD);
					}
				}
				else
				{
					PM_SetSaberMove(LS_KICK_F);
				}
			}
			else if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
			{
				PM_TryAirKick(LS_KICK_B_AIR);
			}
			else
			{
				//kick back
				if (PM_CrouchingAnim(pm->ps->legsAnim))
				{
					PM_SetSaberMove(LS_KICK_SWEEP);
				}
				else
				{
					PM_SetSaberMove(LS_KICK_B);
				}
			}
			pm->cmd.forwardmove = 0;
		}
		else if (pm->gent
			&& pm->gent->enemy
			&& G_CanKickEntity(pm->gent, pm->gent->enemy))
		{
			//auto-pick?
			if (PM_PickAutoMultiKick(qfalse))
			{
				//kicked!
				if (pm->ps->saber_move == LS_KICK_RL)
				{
					//just pull back
					if (d_slowmodeath->integer > 3)
					{
						G_StartMatrixEffect(pm->gent, MEF_NO_SPIN, pm->ps->legsAnimTimer + 500);
					}
					else
					{
						if (d_slowmoaction->integer && (pm->gent->owner->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(pm->gent->owner)))
						{
							G_StartStasisEffect_FORCE_LEVEL_1(pm->gent, MEF_NO_SPIN, pm->ps->legsAnimTimer + 500);
						}
					}
				}
				else
				{
					//normal spin
					if (d_slowmodeath->integer > 3)
					{
						G_StartMatrixEffect(pm->gent, 0, pm->ps->legsAnimTimer + 500);
					}
				}
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE
					&& (pm->ps->saber_move == LS_KICK_S
						|| pm->ps->saber_move == LS_KICK_BF
						|| pm->ps->saber_move == LS_KICK_RL))
				{
					//in the air and doing a jump-kick, which is a ground anim, so....
					//cut z velocity...?
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
			}
			else
			{
				const saber_moveName_t kick_move = PM_PickAutoKick(pm->gent->enemy);
				if (kick_move != LS_NONE)
				{
					//Matrix?
					PM_SetSaberMove(kick_move);
					int me_flags = 0;
					switch (kick_move)
					{
					case LS_KICK_B: //just pull back
					case LS_KICK_SWEEP:
					case LS_KICK_B_AIR: //just pull back
						me_flags = MEF_NO_SPIN;
						break;
					case LS_KICK_L: //spin to the left
					case LS_KICK_L_AIR: //spin to the left
						me_flags = MEF_REVERSE_SPIN;
						break;
					default:
						break;
					}
					if (d_slowmodeath->integer > 3)
					{
						G_StartMatrixEffect(pm->gent, me_flags, pm->ps->legsAnimTimer + 500);
					}
				}
			}
		}
		else
		{
			if (PM_PickAutoMultiKick(qtrue))
			{
				int me_flags = 0;
				switch (pm->ps->saber_move)
				{
				case LS_KICK_RL: //just pull back
				case LS_KICK_B: //just pull back
				case LS_KICK_SWEEP:
				case LS_KICK_B_AIR: //just pull back
					me_flags = MEF_NO_SPIN;
					break;
				case LS_KICK_L: //spin to the left
				case LS_KICK_L_AIR: //spin to the left
					me_flags = MEF_REVERSE_SPIN;
					break;
				default:
					break;
				}
				if (d_slowmodeath->integer > 3)
				{
					G_StartMatrixEffect(pm->gent, me_flags, pm->ps->legsAnimTimer + 500);
				}
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE
					&& (pm->ps->saber_move == LS_KICK_S
						|| pm->ps->saber_move == LS_KICK_BF
						|| pm->ps->saber_move == LS_KICK_RL))
				{
					//in the air and doing a jump-kick, which is a ground anim, so....
					//cut z velocity...?
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
			}
		}
	}
}

static void PM_MeleeKickForConditions()
{
	if (!PM_InRoll(pm->ps)
		&& !PM_InKnockDown(pm->ps)) //not already in a kick
	{
		//player kicks
		if (pm->cmd.rightmove)
		{
			//kick to side
			if (pm->cmd.rightmove > 0)
			{
				//kick right
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					PM_TryAirKick(LS_KICK_R_AIR);
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
						{
							PM_SetSaberMove(LS_SMACK_R);
						}
						else
						{
							PM_SetSaberMove(LS_KICK_R);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_R);
					}
				}
			}
			else
			{
				//kick left
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					PM_TryAirKick(LS_KICK_L_AIR);
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
						{
							PM_SetSaberMove(LS_SMACK_L);
						}
						else
						{
							PM_SetSaberMove(LS_KICK_L);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_L);
					}
				}
			}
			pm->cmd.rightmove = 0;
		}
		else if (pm->cmd.forwardmove)
		{
			//kick front/back
			if (pm->cmd.forwardmove > 0)
			{
				//kick fwd
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						PM_TryAirKick(LS_FLYING_KICK);
					}
					else
					{
						PM_TryAirKick(LS_KICK_F_AIR);
					}
				}
				else if (pm->ps->groundEntityNum != ENTITYNUM_NONE && g_SerenityJediEngineMode->integer == 2 && pm->
					ps->weapon == WP_SABER && pm->ps->SaberActive())
				{
					if (G_CheckEnemyPresence(pm->gent, DIR_FRONT, 64, 12.0f))
					{
						if (pm->ps->saberFatigueChainCount >= MISHAPLEVEL_TEN)
						{
							PM_SetSaberMove(LS_SMACK_L);
						}
						else
						{
							PM_SetSaberMove(LS_HILT_BASH);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_F_MD);
					}
				}
				else
				{
					PM_SetSaberMove(LS_KICK_F);
				}
			}
			else if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
			{
				PM_TryAirKick(LS_KICK_B_AIR);
			}
			else
			{
				//kick back
				if (PM_CrouchingAnim(pm->ps->legsAnim))
				{
					PM_SetSaberMove(LS_KICK_SWEEP);
				}
				else
				{
					PM_SetSaberMove(LS_KICK_B);
				}
			}
			pm->cmd.forwardmove = 0;
		}
		else if (pm->gent
			&& pm->gent->enemy
			&& G_CanKickEntity(pm->gent, pm->gent->enemy))
		{
			//auto-pick?
			if (PM_PickAutoMultiKick(qfalse))
			{
				//kicked!
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE
					&& (pm->ps->saber_move == LS_KICK_S
						|| pm->ps->saber_move == LS_KICK_BF
						|| pm->ps->saber_move == LS_KICK_RL))
				{
					//in the air and doing a jump-kick, which is a ground anim, so....
					//cut z velocity...?
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
			}
			else
			{
				const saber_moveName_t kick_move = PM_PickAutoKick(pm->gent->enemy);
				if (kick_move != LS_NONE)
				{
					//Matrix?
					PM_SetSaberMove(kick_move);
					int me_flags;
					switch (kick_move)
					{
					case LS_KICK_B: //just pull back
					case LS_KICK_SWEEP:
					case LS_KICK_B_AIR: //just pull back
						me_flags = MEF_NO_SPIN;
						break;
					case LS_KICK_L: //spin to the left
					case LS_SLAP_L: //spin to the left
					case LS_KICK_L_AIR: //spin to the left
						me_flags = MEF_REVERSE_SPIN;
						break;
					default:
						break;
					}
				}
			}
		}
		else
		{
			if (PM_PickAutoMultiKick(qtrue))
			{
				int me_flags;
				switch (pm->ps->saber_move)
				{
				case LS_KICK_RL: //just pull back
				case LS_KICK_B: //just pull back
				case LS_KICK_SWEEP:
				case LS_KICK_B_AIR: //just pull back
					me_flags = MEF_NO_SPIN;
					break;
				case LS_KICK_L: //spin to the left
				case LS_SLAP_L: //spin to the left
				case LS_KICK_L_AIR: //spin to the left
					me_flags = MEF_REVERSE_SPIN;
					break;
				default:
					break;
				}
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE
					&& (pm->ps->saber_move == LS_KICK_S
						|| pm->ps->saber_move == LS_KICK_BF
						|| pm->ps->saber_move == LS_KICK_RL))
				{
					//in the air and doing a jump-kick, which is a ground anim, so....
					//cut z velocity...?
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
			}
			//pm->ps->userInt3 &= ~(1 << FLAG_SLAPPING);
		}
	}
}

static void pm_melee_move_for_conditions()
{
	if (pm->gent && (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent))) //player only restrictions
	{
		if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive() && !pm->ps->saberInFlight)
		{
			if (!(pm->ps->communicatingflags & 1 << KICKING))
			{
				return;
			}
		}
	}

	if (!PM_SaberInBounce(pm->ps->saber_move)
		&& !PM_SaberInKnockaway(pm->ps->saber_move)
		&& !PM_SaberInBrokenParry(pm->ps->saber_move)
		&& !PM_KickMove(pm->ps->saber_move)
		&& !PM_KickingAnim(pm->ps->torsoAnim)
		&& !PM_KickingAnim(pm->ps->legsAnim)
		&& !PM_InRoll(pm->ps)
		&& !PM_InKnockDown(pm->ps)) //not already in a kick
	{
		//player kicks
		if (pm->cmd.rightmove)
		{
			//kick to side
			if (pm->cmd.rightmove > 0)
			{
				//kick right
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					PM_TryAirKick(LS_KICK_R_AIR);
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
						{
							PM_SetSaberMove(LS_SMACK_R);
						}
						else
						{
							PM_SetSaberMove(LS_KICK_R);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_R);
					}
				}
				//pm->ps->userInt3 |= (1 << FLAG_SLAPPING);
			}
			else
			{
				//kick left
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					PM_TryAirKick(LS_KICK_L_AIR);
				}
				else
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						if (pm->ps->weapon == WP_SABER && pm->ps->SaberActive())
						{
							PM_SetSaberMove(LS_SMACK_L);
						}
						else
						{
							PM_SetSaberMove(LS_KICK_L);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_L);
					}
				}
				//pm->ps->userInt3 |= (1 << FLAG_SLAPPING);
			}
			pm->cmd.rightmove = 0;
		}
		else if (pm->cmd.forwardmove)
		{
			//kick front/back
			if (pm->cmd.forwardmove > 0)
			{
				//kick fwd
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						PM_TryAirKick(LS_FLYING_KICK);
					}
					else
					{
						PM_TryAirKick(LS_KICK_F_AIR);
					}
				}
				else if (pm->ps->groundEntityNum != ENTITYNUM_NONE && g_SerenityJediEngineMode->integer == 2 && pm->
					ps->weapon == WP_SABER && pm->ps->SaberActive())
				{
					if (G_CheckEnemyPresence(pm->gent, DIR_FRONT, 64, 12.0f))
					{
						if (pm->ps->saberFatigueChainCount >= MISHAPLEVEL_TEN)
						{
							PM_SetSaberMove(LS_SMACK_L);
						}
						else
						{
							PM_SetSaberMove(LS_HILT_BASH);
						}
					}
					else
					{
						PM_SetSaberMove(LS_KICK_F_MD);
					}
				}
				else
				{
					PM_SetSaberMove(LS_KICK_F);
				}
			}
			else if (pm->ps->groundEntityNum == ENTITYNUM_NONE || pm->cmd.upmove > 0)
			{
				PM_TryAirKick(LS_KICK_B_AIR);
			}
			else
			{
				//kick back
				if (PM_CrouchingAnim(pm->ps->legsAnim))
				{
					PM_SetSaberMove(LS_KICK_SWEEP);
				}
				else
				{
					PM_SetSaberMove(LS_KICK_B);
				}
			}
			pm->cmd.forwardmove = 0;
		}
		else if (pm->gent
			&& pm->gent->enemy
			&& G_CanKickEntity(pm->gent, pm->gent->enemy))
		{
			//auto-pick?
			if (PM_PickAutoMultiKick(qfalse))
			{
				//kicked!
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE
					&& (pm->ps->saber_move == LS_KICK_S
						|| pm->ps->saber_move == LS_KICK_BF
						|| pm->ps->saber_move == LS_KICK_RL))
				{
					//in the air and doing a jump-kick, which is a ground anim, so....
					//cut z velocity...?
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
			}
			else
			{
				const saber_moveName_t kick_move = PM_PickAutoKick(pm->gent->enemy);
				if (kick_move != LS_NONE)
				{
					//Matrix?
					PM_SetSaberMove(kick_move);
					int me_flags;
					switch (kick_move)
					{
					case LS_KICK_B: //just pull back
					case LS_KICK_SWEEP:
					case LS_KICK_B_AIR: //just pull back
						me_flags = MEF_NO_SPIN;
						break;
					case LS_KICK_L: //spin to the left
					case LS_SLAP_L: //spin to the left
					case LS_KICK_L_AIR: //spin to the left
						me_flags = MEF_REVERSE_SPIN;
						break;
					default:
						break;
					}
				}
			}
		}
		else
		{
			if (PM_PickAutoMultiKick(qtrue))
			{
				int me_flags;
				switch (pm->ps->saber_move)
				{
				case LS_KICK_RL: //just pull back
				case LS_KICK_B: //just pull back
				case LS_KICK_SWEEP:
				case LS_KICK_B_AIR: //just pull back
					me_flags = MEF_NO_SPIN;
					break;
				case LS_KICK_L: //spin to the left
				case LS_SLAP_L: //spin to the left
				case LS_KICK_L_AIR: //spin to the left
					me_flags = MEF_REVERSE_SPIN;
					break;
				default:
					break;
				}
				if (pm->ps->groundEntityNum == ENTITYNUM_NONE
					&& (pm->ps->saber_move == LS_KICK_S
						|| pm->ps->saber_move == LS_KICK_BF
						|| pm->ps->saber_move == LS_KICK_RL))
				{
					//in the air and doing a jump-kick, which is a ground anim, so....
					//cut z velocity...?
					pm->ps->velocity[2] = 0;
				}
				pm->cmd.upmove = 0;
			}
			//pm->ps->userInt3 &= ~(1 << FLAG_SLAPPING);
		}
	}
}

static void PM_CheckClearSaberBlock()
{
	if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		//player
		if (pm->ps->saberBlocked >= BLOCKED_UPPER_RIGHT_PROJ && pm->ps->saberBlocked <= BLOCKED_TOP_PROJ)
		{
			//blocking a projectile
			if (pm->ps->forcePowerDebounce[FP_SABER_DEFENSE] < level.time)
			{
				//block is done or breaking out of it with an attack
				pm->ps->weaponTime = 0;
				pm->ps->saberBlocked = BLOCKED_NONE;
			}
			else if (pm->cmd.buttons & BUTTON_ATTACK && !(pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK))
			{
				//block is done or breaking out of it with an attack
				pm->ps->weaponTime = 0;
				pm->ps->saberBlocked = BLOCKED_NONE;
			}
		}
	}
}

static qboolean InSaberDelayAnimation(const int move)
{
	if (move >= 665 && move <= 669
		|| move >= 690 && move <= 694
		|| move >= 715 && move <= 719)
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean PM_SaberBlocking()
{
	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking

	if (g_SerenityJediEngineMode->integer)
	{
		if (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent))
		{
			// Now we react to a block action by the player's lightsaber.
			if (pm->ps->saberBlocked)
			{
				qboolean was_attacked_by_gun = qfalse;

				if (pm->ps->saber_move > LS_PUTAWAY && pm->ps->saber_move <= LS_A_BL2TR && pm->ps->saberBlocked !=
					BLOCKED_PARRY_BROKEN &&
					(pm->ps->saberBlocked < BLOCKED_UPPER_RIGHT_PROJ || pm->ps->saberBlocked > BLOCKED_TOP_PROJ))
				{
					//we parried another lightsaber while attacking, so treat it as a bounce
					pm->ps->saberBlocked = BLOCKED_ATK_BOUNCE;
				}
				else if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
				{
					if (pm->ps->saberBlocked >= BLOCKED_UPPER_RIGHT_PROJ && pm->ps->saberBlocked <=
						BLOCKED_TOP_PROJ)
					{
						//blocking a projectile
						if (active_blocking)
						{
							//trying to attack
							if (pm->ps->saber_move == LS_READY || PM_SaberInReflect(pm->ps->saber_move))
							{
								pm->ps->saberBlocked = BLOCKED_NONE;
								pm->ps->saberBounceMove = LS_NONE;
								pm->ps->weaponstate = WEAPON_READY;
								if (PM_SaberInReflect(pm->ps->saber_move) && pm->ps->weaponTime > 0)
								{
									pm->ps->weaponTime = 0;
								}
								return qfalse;
							}
						}
						else if (pm->cmd.buttons & BUTTON_ATTACK && !(pm->ps->ManualBlockingFlags & 1 <<
							HOLDINGBLOCK))
						{
							//block is done or breaking out of it with an attack
							pm->ps->weaponTime = 0;
							pm->ps->saberBlocked = BLOCKED_NONE;
						}
					}
				}

				switch (pm->ps->saberBlocked)
				{
				case BLOCKED_BOUNCE_MOVE:
				{
					//act as a bounceMove and reset the saber_move instead of using a seperate value for it
					pm->ps->torsoAnimTimer = 0;
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saber_move));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					pm->ps->saberBlocked = 0;
				}
				break;
				case BLOCKED_PARRY_BROKEN:
					//whatever parry we were is in now broken, play the appropriate knocked-away anim
				{
					saber_moveName_t next_move;
					if (PM_SaberInBrokenParry(pm->ps->saberBounceMove))
					{
						//already have one...?
						next_move = static_cast<saber_moveName_t>(pm->ps->saberBounceMove);
					}
					else
					{
						next_move = PM_BrokenParryForParry(pm->ps->saber_move);
					}
					if (next_move != LS_NONE)
					{
						PM_SetSaberMove(next_move);
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						//Maybe in a knockaway?
					}
				}
				break;
				case BLOCKED_ATK_BOUNCE: // sje player
					if (pm->ps->saber_move >= LS_T1_BR__R
						|| PM_SaberInBounce(pm->ps->saber_move)
						|| PM_SaberInReturn(pm->ps->saber_move)
						|| PM_SaberInMassiveBounce(pm->ps->torsoAnim))
					{
						//an actual bounce?  Other bounces before this are actually transitions?
						pm->ps->saberBlocked = BLOCKED_NONE;
					}
					else if (pm->ps->userInt3 & 1 << FLAG_SLOWBOUNCE && !(pm->ps->userInt3 & 1 <<
						FLAG_OLDSLOWBOUNCE)
						|| !PM_SaberInAttack(pm->ps->saber_move) && !PM_SaberInStart(pm->ps->saber_move))
					{
						//already in the bounce, go into an attack or transition to ready.. should never get here since can't be blocked in a bounce!
						int next_move;

						if (pm->cmd.buttons & BUTTON_ATTACK)
						{
							//transition to a new attack
							if (pm->ps->client_num && !PM_ControlledByPlayer())
							{
								//NPC
								next_move = saber_moveData[pm->ps->saber_move].chain_attack;
							}
							else
							{
								//player
								int newQuad = PM_SaberMoveQuadrantForMovement(&pm->cmd);
								while (newQuad == saber_moveData[pm->ps->saber_move].startQuad)
								{
									//player is still in same attack quad, don't repeat that attack because it looks bad,
									newQuad = Q_irand(Q_BR, Q_BL);
								}
								next_move = transitionMove[saber_moveData[pm->ps->saber_move].startQuad][newQuad];
							}
						}
						else
						{
							//return to ready
							if (pm->ps->client_num && !PM_ControlledByPlayer())
							{
								//NPC
								next_move = saber_moveData[pm->ps->saber_move].chain_idle;
							}
							else
							{
								//player
								if (saber_moveData[pm->ps->saber_move].startQuad == Q_T)
								{
									next_move = Q_irand(LS_R_BL2TR, LS_R_BR2TL);
								}
								else if (saber_moveData[pm->ps->saber_move].startQuad < Q_T)
								{
									next_move = LS_R_TL2BR + static_cast<saber_moveName_t>(saber_moveData[pm->ps->
										saber_move].startQuad - Q_BR);
								}
								else
								{
									next_move = LS_R_BR2TL + static_cast<saber_moveName_t>(saber_moveData[pm->ps->
										saber_move].startQuad - Q_TL);
								}
							}
						}
						PM_SetSaberMove(static_cast<saber_moveName_t>(next_move));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						//start the bounce move
						saber_moveName_t bounce_move;
						if (pm->ps->saberBounceMove != LS_NONE)
						{
							bounce_move = static_cast<saber_moveName_t>(pm->ps->saberBounceMove);
						}
						else
						{
							bounce_move = PM_SaberBounceForAttack(pm->ps->saber_move);
						}
						PM_SetSaberMove(bounce_move);
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}

					if (cg_debugSaber.integer >= 3)
					{
						Com_Printf("Saber Block: Bounce\n");
					}
					break;

					/////////////////////////////////////////// SABER BLOCK POSITIONS /////////////////////////////////////////
				case BLOCKED_UPPER_RIGHT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_UR);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry UR\n");
					}
					break;
				case BLOCKED_UPPER_RIGHT_PROJ:
					PM_SetSaberMove(LS_REFLECT_UR);
					was_attacked_by_gun = qtrue;

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect UR\n");
					}
					break;
				case BLOCKED_UPPER_LEFT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_UL);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry UL\n");
					}
					break;
				case BLOCKED_UPPER_LEFT_PROJ:
					PM_SetSaberMove(LS_REFLECT_UL);
					was_attacked_by_gun = qtrue;

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect UL\n");
					}
					break;
				case BLOCKED_LOWER_RIGHT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_LR);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry LR\n");
					}
					break;
				case BLOCKED_LOWER_RIGHT_PROJ:
					PM_SetSaberMove(LS_REFLECT_LR);
					was_attacked_by_gun = qtrue;

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect LR\n");
					}
					break;
				case BLOCKED_LOWER_LEFT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_LL);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry LL\n");
					}
					break;
				case BLOCKED_LOWER_LEFT_PROJ:
					PM_SetSaberMove(LS_REFLECT_LL);
					was_attacked_by_gun = qtrue;

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect LL\n");
					}
					break;
				case BLOCKED_TOP:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_UP);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry Top\n");
					}
					break;
				case BLOCKED_TOP_PROJ:
					PM_SetSaberMove(LS_REFLECT_UP);
					was_attacked_by_gun = qtrue;

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect Top\n");
					}
					break;
				case BLOCKED_BACK:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_REFLECT_B);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry BACK\n");
					}
					break;
				default:
					pm->ps->saberBlocked = BLOCKED_NONE;
					break;
				}

				if (InSaberDelayAnimation(pm->ps->torsoAnim) && pm->cmd.buttons & BUTTON_ATTACK &&
					was_attacked_by_gun)
				{
					pm->ps->weaponTime += 700;
					pm->ps->torsoAnimTimer += 1500;

					switch (pm->ps->saberBlocked)
					{
					case BLOCKED_UPPER_RIGHT:
					case BLOCKED_UPPER_LEFT:
					case BLOCKED_LOWER_RIGHT:
					case BLOCKED_LOWER_LEFT:
					case BLOCKED_TOP:
					case BLOCKED_BACK:
						if (pm->ps->torsoAnimTimer < pm->ps->weaponTime)
						{
							//hold the parry for a bit
							pm->ps->torsoAnimTimer = pm->ps->weaponTime;
						}
						break;
					default:;
					}
				}

				//clear block
				pm->ps->saberBlocked = 0;

				// Charging is like a lead-up before attacking again.  This is an appropriate use, or we can create a new weaponstate for blocking
				pm->ps->saberBounceMove = LS_NONE;
				pm->ps->weaponstate = WEAPON_READY;

				// Done with block, so stop these active weapon branches.
				return qtrue;
			}
		}
		else
		{
			// Now we react to a block action by the player's lightsaber.
			if (pm->ps->saberBlocked)
			{
				if (pm->ps->saber_move > LS_PUTAWAY && pm->ps->saber_move <= LS_A_BL2TR && pm->ps->saberBlocked !=
					BLOCKED_PARRY_BROKEN &&
					(pm->ps->saberBlocked < BLOCKED_UPPER_RIGHT_PROJ || pm->ps->saberBlocked > BLOCKED_TOP_PROJ))
				{
					//we parried another lightsaber while attacking, so treat it as a bounce
					pm->ps->saberBlocked = BLOCKED_ATK_BOUNCE;
				}
				else if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
				{
					if (pm->ps->saberBlocked >= BLOCKED_UPPER_RIGHT_PROJ
						&& pm->ps->saberBlocked <= BLOCKED_TOP_PROJ)
					{
						//blocking a projectile
						if (pm->cmd.buttons & BUTTON_ATTACK)
						{
							//trying to attack
							if (pm->ps->saber_move == LS_READY
								|| PM_SaberInReflect(pm->ps->saber_move))
							{
								//not already busy or already in projectile deflection
								//trying to attack during projectile blocking, so do the attack instead
								pm->ps->saberBlocked = BLOCKED_NONE;
								pm->ps->saberBounceMove = LS_NONE;
								pm->ps->weaponstate = WEAPON_READY;
								if (PM_SaberInReflect(pm->ps->saber_move) && pm->ps->weaponTime > 0)
								{
									//interrupt the current deflection move
									pm->ps->weaponTime = 0;
								}
								return qfalse;
							}
						}
					}
				}

				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					if (pm->ps->saberBlocked != BLOCKED_ATK_BOUNCE)
					{
						//can't attack for twice whatever your skill level's parry debounce time is
						if (pm->gent)
						{
							pm->ps->weaponTime = jedi_re_calc_parry_time(pm->gent, EVASION_PARRY);
						}
						else
						{
							pm->ps->weaponTime = parryDebounce[pm->ps->forcePowerLevel[FP_SABER_DEFENSE]] * 2;
						}
					}
				}

				switch (pm->ps->saberBlocked)
				{
				case BLOCKED_BOUNCE_MOVE:
				{
					//act as a bounceMove and reset the saber_move instead of using a seperate value for it
					pm->ps->torsoAnimTimer = 0;
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saber_move));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					pm->ps->saberBlocked = 0;
				}
				break;
				case BLOCKED_PARRY_BROKEN:
					//whatever parry we were is in now broken, play the appropriate knocked-away anim
				{
					saber_moveName_t next_move;
					if (PM_SaberInBrokenParry(pm->ps->saberBounceMove))
					{
						//already have one...?
						next_move = static_cast<saber_moveName_t>(pm->ps->saberBounceMove);
					}
					else
					{
						next_move = PM_BrokenParryForParry(pm->ps->saber_move);
					}
					if (next_move != LS_NONE)
					{
						PM_SetSaberMove(next_move);
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						//Maybe in a knockaway?
					}
				}
				break;
				case BLOCKED_ATK_BOUNCE: // sje npc
					if (pm->ps->saber_move >= LS_T1_BR__R)
					{
						//an actual bounce?  Other bounces before this are actually transitions?
						pm->ps->saberBlocked = BLOCKED_NONE;
					}
					else if (PM_SaberInBounce(pm->ps->saber_move) || !PM_SaberInAttack(pm->ps->saber_move))
					{
						//already in the bounce, go into an attack or transition to ready.. should never get here since can't be blocked in a bounce!
						int next_move;

						if (pm->cmd.buttons & BUTTON_ATTACK)
						{
							//transition to a new attack
							if (pm->ps->client_num && !PM_ControlledByPlayer())
							{
								//NPC
								next_move = saber_moveData[pm->ps->saber_move].chain_attack;
							}
							else
							{
								//player
								int newQuad = PM_SaberMoveQuadrantForMovement(&pm->cmd);
								while (newQuad == saber_moveData[pm->ps->saber_move].startQuad)
								{
									newQuad = Q_irand(Q_BR, Q_BL);
								} //else player is switching up anyway, take the new attack dir
								next_move = transitionMove[saber_moveData[pm->ps->saber_move].startQuad][newQuad];
							}
						}
						else
						{
							//return to ready
							if (pm->ps->client_num && !PM_ControlledByPlayer())
							{
								//NPC
								next_move = saber_moveData[pm->ps->saber_move].chain_idle;
							}
							else
							{
								//player
								if (saber_moveData[pm->ps->saber_move].startQuad == Q_T)
								{
									next_move = LS_R_BL2TR;
								}
								else if (saber_moveData[pm->ps->saber_move].startQuad < Q_T)
								{
									next_move = LS_R_TL2BR + static_cast<saber_moveName_t>(saber_moveData[pm->ps->
										saber_move].startQuad - Q_BR);
								}
								else
								{
									next_move = LS_R_BR2TL + static_cast<saber_moveName_t>(saber_moveData[pm->ps->
										saber_move].startQuad - Q_TL);
								}
							}
						}
						PM_SetSaberMove(static_cast<saber_moveName_t>(next_move));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						//start the bounce move
						saber_moveName_t bounce_move;
						if (pm->ps->saberBounceMove != LS_NONE)
						{
							bounce_move = static_cast<saber_moveName_t>(pm->ps->saberBounceMove);
						}
						else
						{
							bounce_move = PM_SaberBounceForAttack(pm->ps->saber_move);
						}
						PM_SetSaberMove(bounce_move);
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Bounce\n");
					}
					break;
				case BLOCKED_UPPER_RIGHT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_UR);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry UR\n");
					}
					break;
				case BLOCKED_UPPER_RIGHT_PROJ:
					PM_SetSaberMove(LS_REFLECT_UR);

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect UR\n");
					}
					break;
				case BLOCKED_UPPER_LEFT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_UL);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry UL\n");
					}
					break;
				case BLOCKED_UPPER_LEFT_PROJ:
					PM_SetSaberMove(LS_REFLECT_UL);

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect UL\n");
					}
					break;
				case BLOCKED_LOWER_RIGHT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_LR);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry LR\n");
					}
					break;
				case BLOCKED_LOWER_RIGHT_PROJ:
					PM_SetSaberMove(LS_REFLECT_LR);

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect LR\n");
					}
					break;
				case BLOCKED_LOWER_LEFT:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_LL);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry LL\n");
					}
					break;
				case BLOCKED_LOWER_LEFT_PROJ:
					PM_SetSaberMove(LS_REFLECT_LL);

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect LL\n");
					}
					break;
				case BLOCKED_TOP:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						//pm->ps->saberBounceMove = LS_NONE;
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_PARRY_UP);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry Top\n");
					}
					break;
				case BLOCKED_TOP_PROJ:
					PM_SetSaberMove(LS_REFLECT_UP);

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Deflect Top\n");
					}
					break;
				case BLOCKED_BACK:
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
					else
					{
						PM_SetSaberMove(LS_REFLECT_B);
					}

					if (cg_debugSaber.integer >= 2)
					{
						Com_Printf("Saber Block: Parry BACK\n");
					}
					break;
				default:
					pm->ps->saberBlocked = BLOCKED_NONE;
					break;
				}

				// Charging is like a lead-up before attacking again.  This is an appropriate use, or we can create a new weaponstate for blocking
				pm->ps->saberBounceMove = LS_NONE;
				pm->ps->weaponstate = WEAPON_READY;

				// Done with block, so stop these active weapon branches.
				return qtrue;
			}
		}
	}
	else
	{
		// Now we react to a block action by the player's lightsaber.
		if (pm->ps->saberBlocked)
		{
			if (pm->ps->saber_move > LS_PUTAWAY && pm->ps->saber_move <= LS_A_BL2TR && pm->ps->saberBlocked !=
				BLOCKED_PARRY_BROKEN &&
				(pm->ps->saberBlocked < BLOCKED_UPPER_RIGHT_PROJ || pm->ps->saberBlocked > BLOCKED_TOP_PROJ))
			{
				//we parried another lightsaber while attacking, so treat it as a bounce
				pm->ps->saberBlocked = BLOCKED_ATK_BOUNCE;
			}
			else if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
			{
				if (pm->ps->saberBlocked >= BLOCKED_UPPER_RIGHT_PROJ
					&& pm->ps->saberBlocked <= BLOCKED_TOP_PROJ)
				{
					//blocking a projectile
					if (pm->cmd.buttons & BUTTON_ATTACK)
					{
						//trying to attack
						if (pm->ps->saber_move == LS_READY
							|| PM_SaberInReflect(pm->ps->saber_move))
						{
							//not already busy or already in projectile deflection
							//trying to attack during projectile blocking, so do the attack instead
							pm->ps->saberBlocked = BLOCKED_NONE;
							pm->ps->saberBounceMove = LS_NONE;
							pm->ps->weaponstate = WEAPON_READY;
							if (PM_SaberInReflect(pm->ps->saber_move) && pm->ps->weaponTime > 0)
							{
								//interrupt the current deflection move
								pm->ps->weaponTime = 0;
							}
							return qfalse;
						}
					}
				}
			}

			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{
				if (pm->ps->saberBlocked != BLOCKED_ATK_BOUNCE)
				{
					//can't attack for twice whatever your skill level's parry debounce time is
					if (pm->gent)
					{
						pm->ps->weaponTime = jedi_re_calc_parry_time(pm->gent, EVASION_PARRY);
					}
					else
					{
						pm->ps->weaponTime = parryDebounce[pm->ps->forcePowerLevel[FP_SABER_DEFENSE]] * 2;
					}
				}
			}

			switch (pm->ps->saberBlocked)
			{
			case BLOCKED_BOUNCE_MOVE:
			{
				//act as a bounceMove and reset the saber_move instead of using a separate value for it
				pm->ps->torsoAnimTimer = 0;
				PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saber_move));
				pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				pm->ps->saberBlocked = 0;
			}
			break;
			case BLOCKED_PARRY_BROKEN:
				//whatever parry we were is in now broken, play the appropriate knocked-away anim
			{
				saber_moveName_t next_move;
				if (PM_SaberInBrokenParry(pm->ps->saberBounceMove))
				{
					//already have one...?
					next_move = static_cast<saber_moveName_t>(pm->ps->saberBounceMove);
				}
				else
				{
					next_move = PM_BrokenParryForParry(pm->ps->saber_move);
				}
				if (next_move != LS_NONE)
				{
					PM_SetSaberMove(next_move);
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					//Maybe in a knockaway?
				}
			}
			break;
			case BLOCKED_ATK_BOUNCE: //jka all
				if (pm->ps->saber_move >= LS_T1_BR__R)
				{
					//an actual bounce?  Other bounces before this are actually transitions?
					pm->ps->saberBlocked = BLOCKED_NONE;
				}
				else if (PM_SaberInBounce(pm->ps->saber_move) || !PM_SaberInAttack(pm->ps->saber_move))
				{
					//already in the bounce, go into an attack or transition to ready.. should never get here since can't be blocked in a bounce!
					int next_move;

					if (pm->cmd.buttons & BUTTON_ATTACK)
					{
						//transition to a new attack
						if (pm->ps->client_num && !PM_ControlledByPlayer())
						{
							//NPC
							next_move = saber_moveData[pm->ps->saber_move].chain_attack;
						}
						else
						{
							//player
							int new_quad = PM_SaberMoveQuadrantForMovement(&pm->cmd);
							while (new_quad == saber_moveData[pm->ps->saber_move].startQuad)
							{
								//player is still in same attack quad, don't repeat that attack because it looks bad,
								new_quad = Q_irand(Q_BR, Q_BL);
							} //else player is switching up anyway, take the new attack dir
							next_move = transitionMove[saber_moveData[pm->ps->saber_move].startQuad][new_quad];
						}
					}
					else
					{
						//return to ready
						if (pm->ps->client_num && !PM_ControlledByPlayer())
						{
							//NPC
							next_move = saber_moveData[pm->ps->saber_move].chain_idle;
						}
						else
						{
							//player
							if (saber_moveData[pm->ps->saber_move].startQuad == Q_T)
							{
								next_move = LS_R_BL2TR;
							}
							else if (saber_moveData[pm->ps->saber_move].startQuad < Q_T)
							{
								next_move = LS_R_TL2BR + static_cast<saber_moveName_t>(saber_moveData[pm->ps->
									saber_move].startQuad - Q_BR);
							}
							else
							{
								next_move = LS_R_BR2TL + static_cast<saber_moveName_t>(saber_moveData[pm->ps->
									saber_move].startQuad - Q_TL);
							}
						}
					}
					PM_SetSaberMove(static_cast<saber_moveName_t>(next_move));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					//start the bounce move
					saber_moveName_t bounce_move;
					if (pm->ps->saberBounceMove != LS_NONE)
					{
						bounce_move = static_cast<saber_moveName_t>(pm->ps->saberBounceMove);
					}
					else
					{
						bounce_move = PM_SaberBounceForAttack(pm->ps->saber_move);
					}
					PM_SetSaberMove(bounce_move);
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Bounce\n");
				}
				break;
			case BLOCKED_UPPER_RIGHT:
				if (pm->ps->saberBounceMove != LS_NONE)
				{
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					PM_SetSaberMove(LS_PARRY_UR);
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Parry UR\n");
				}
				break;
			case BLOCKED_UPPER_RIGHT_PROJ:
				PM_SetSaberMove(LS_REFLECT_UR);

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Deflect UR\n");
				}
				break;
			case BLOCKED_UPPER_LEFT:
				if (pm->ps->saberBounceMove != LS_NONE)
				{
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					PM_SetSaberMove(LS_PARRY_UL);
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Parry UL\n");
				}
				break;
			case BLOCKED_UPPER_LEFT_PROJ:
				PM_SetSaberMove(LS_REFLECT_UL);

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Deflect UL\n");
				}
				break;
			case BLOCKED_LOWER_RIGHT:
				if (pm->ps->saberBounceMove != LS_NONE)
				{
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					PM_SetSaberMove(LS_PARRY_LR);
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Parry LR\n");
				}
				break;
			case BLOCKED_LOWER_RIGHT_PROJ:
				PM_SetSaberMove(LS_REFLECT_LR);

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Deflect LR\n");
				}
				break;
			case BLOCKED_LOWER_LEFT:
				if (pm->ps->saberBounceMove != LS_NONE)
				{
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					PM_SetSaberMove(LS_PARRY_LL);
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Parry LL\n");
				}
				break;
			case BLOCKED_LOWER_LEFT_PROJ:
				PM_SetSaberMove(LS_REFLECT_LL);

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Deflect LL\n");
				}
				break;
			case BLOCKED_TOP:
				if (pm->ps->saberBounceMove != LS_NONE)
				{
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					PM_SetSaberMove(LS_PARRY_UP);
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Parry Top\n");
				}
				break;
			case BLOCKED_TOP_PROJ:
				PM_SetSaberMove(LS_REFLECT_UP);

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Deflect Top\n");
				}
				break;
			case BLOCKED_BACK:
				if (pm->ps->saberBounceMove != LS_NONE)
				{
					PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberBounceMove));
					pm->ps->weaponTime = pm->ps->torsoAnimTimer;
				}
				else
				{
					PM_SetSaberMove(LS_REFLECT_B);
				}

				if (cg_debugSaber.integer >= 2)
				{
					Com_Printf("Saber Block: Parry BACK\n");
				}
				break;
			default:
				pm->ps->saberBlocked = BLOCKED_NONE;
				break;
			}

			// Charging is like a lead-up before attacking again.  This is an appropriate use, or we can create a new weaponstate for blocking
			pm->ps->saberBounceMove = LS_NONE;
			pm->ps->weaponstate = WEAPON_READY;

			// Done with block, so stop these active weapon branches.
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean PM_NPCCheckAttackRoll()
{
	if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer() //NPC
		&& pm->gent
		&& pm->gent->NPC
		&& (pm->gent->NPC->rank > RANK_CREWMAN && !Q_irand(0, 3 - g_spskill->integer))
		&& pm->gent->enemy
		&& fabs(pm->gent->enemy->currentOrigin[2] - pm->ps->origin[2]) < 32
		&& DistanceHorizontalSquared(pm->gent->enemy->currentOrigin, pm->ps->origin) < 128.0f * 128.0f
		&& InFOV(pm->gent->enemy->currentOrigin, pm->ps->origin, pm->ps->viewangles, 30, 90))
	{
		//stab!
		return qtrue;
	}
	return qfalse;
}

/*
=================
PM_WeaponLightsaber

Consults a chart to choose what to do with the lightsaber.
While this is a little different than the Quake 3 code, there is no clean way of using the Q3 code for this kind of thing.
=================
*/
// Ultimate goal is to set the sabermove to the proper next location
// Note that if the resultant animation is NONE, then the animation is essentially "idle", and is set in WP_TorsoAnim

int pm_returnfor_quad(const int quad)
{
	switch (quad)
	{
	case Q_BR:
		return LS_R_TL2BR;
	case Q_R:
		return LS_R_L2R;
	case Q_TR:
		return LS_R_BL2TR;
	case Q_T:
		return LS_R_BL2TR;
	case Q_TL:
		return LS_R_BR2TL;
	case Q_L:
		return LS_R_R2L;
	case Q_BL:
		return LS_R_TR2BL;
	case Q_B:
		return LS_R_T2B;
	default:
		return LS_READY;
	}
}

int BlockedforQuad(const int quad)
{
	//returns the saberBlocked direction for given quad.
	switch (quad)
	{
	case Q_BR:
	{
		return BLOCKED_LOWER_RIGHT;
	}
	case Q_R:
	{
		return BLOCKED_UPPER_RIGHT;
	}
	case Q_TR:
	{
		return BLOCKED_UPPER_RIGHT;
	}
	case Q_T:
	{
		return BLOCKED_TOP;
	}
	case Q_TL:
	{
		return BLOCKED_UPPER_LEFT;
	}
	case Q_L:
	{
		return BLOCKED_UPPER_LEFT;
	}
	case Q_BL:
	{
		return BLOCKED_LOWER_LEFT;
	}
	case Q_B:
	{
		return BLOCKED_BACK;
	}
	default:
		return BLOCKED_TOP;
	}
}

int InvertQuad(const int quad)
{
	//Returns the reflection quad for the given quad.
	//This is used for setting a player's block direction based on his attacker's move's start/end quad.
	switch (quad)
	{
	case Q_B:
		return Q_B;
	case Q_BR:
		return Q_BL;
	case Q_R:
		return Q_L;
	case Q_TR:
		return Q_TL;
	case Q_T:
		return Q_T;
	case Q_TL:
		return Q_TR;
	case Q_L:
		return Q_R;
	case Q_BL:
		return Q_BR;
	default:
		return quad;
	}
}

static int PM_irand_timesync(const int val1, const int val2)
{
	int i = val1 - 1 + Q_random(&pm->cmd.serverTime) * (val2 - val1) + 1;
	if (i < val1)
	{
		i = val1;
	}
	if (i > val2)
	{
		i = val2;
	}

	return i;
}

static saber_moveName_t PM_DoFake(const int curmove)
{
	int new_quad = -1;

	if (pm->ps->userInt3 & 1 << FLAG_ATTACKFAKE)
	{
		//already attack faking, can't do another one until this one is over.
		return LS_NONE;
	}

	if (pm->ps->client_num && !PM_ControlledByPlayer())
	{
		//NPCs never do attack fakes, just follow thru with attack.
		return LS_NONE;
	}

	if (in_camera)
	{
		return LS_NONE;
	}

	if (pm->cmd.rightmove > 0)
	{
		//moving right
		if (pm->cmd.forwardmove > 0)
		{
			//forward right = TL2BR slash
			new_quad = Q_TL;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			//backward right = BL2TR uppercut
			new_quad = Q_BL;
		}
		else
		{
			//just right is a left slice
			new_quad = Q_L;
		}
	}
	else if (pm->cmd.rightmove < 0)
	{
		//moving left
		if (pm->cmd.forwardmove > 0)
		{
			//forward left = TR2BL slash
			new_quad = Q_TR;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			//backward left = BR2TL uppercut
			new_quad = Q_BR;
		}
		else
		{
			//just left is a right slice
			new_quad = Q_R;
		}
	}
	else
	{
		//not moving left or right
		if (pm->cmd.forwardmove > 0)
		{
			//forward= T2B slash
			new_quad = Q_T;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			//backward= T2B slash	//or B2T uppercut?
			new_quad = Q_T;
		}
		else
		{
			//Not moving at all
		}
	}

	if (new_quad == -1)
	{
		//assume that we're trying to fake in our current direction so we'll automatically fake
		//in the completely opposite direction.  This allows the player to do a fake while standing still.
		new_quad = saber_moveData[curmove].endQuad;
	}

	if (new_quad == saber_moveData[curmove].endQuad)
	{
		//player is attempting to do a fake move to the same quadrant
		//as such, fake to the completely opposite quad
		new_quad += 4;
		if (new_quad > Q_B)
		{
			//rotated past Q_B, shift back to get the proper quadrant
			new_quad -= Q_NUM_QUADS;
		}
	}

	if (new_quad == Q_B)
	{
		//attacks can't be launched from this quad, just randomly fake to the bottom left/right
		if (PM_irand_timesync(0, 9) <= 4)
		{
			new_quad = Q_BL;
		}
		else
		{
			new_quad = Q_BR;
		}
	}

	//add faking flag
	pm->ps->userInt3 |= 1 << FLAG_ATTACKFAKE;
	return transitionMove[saber_moveData[curmove].endQuad][new_quad];
}

saber_moveName_t PM_DoAI_Fake(const int curmove)
{
	int new_quad = -1;

	if (in_camera)
	{
		return LS_NONE;
	}

	if (pm->cmd.rightmove > 0)
	{
		//moving right
		if (pm->cmd.forwardmove > 0)
		{
			//forward right = TL2BR slash
			new_quad = Q_TL;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			//backward right = BL2TR uppercut
			new_quad = Q_BL;
		}
		else
		{
			//just right is a left slice
			new_quad = Q_L;
		}
	}
	else if (pm->cmd.rightmove < 0)
	{
		//moving left
		if (pm->cmd.forwardmove > 0)
		{
			//forward left = TR2BL slash
			new_quad = Q_TR;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			//backward left = BR2TL uppercut
			new_quad = Q_BR;
		}
		else
		{
			//just left is a right slice
			new_quad = Q_R;
		}
	}
	else
	{
		//not moving left or right
		if (pm->cmd.forwardmove > 0)
		{
			//forward= T2B slash
			new_quad = Q_T;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			//backward= T2B slash	//or B2T uppercut?
			new_quad = Q_T;
		}
		else
		{
			//Not moving at all
		}
	}

	if (new_quad == -1)
	{
		//assume that we're trying to fake in our current direction so we'll automatically fake
		//in the completely opposite direction.  This allows the player to do a fake while standing still.
		new_quad = saber_moveData[curmove].endQuad;
	}

	if (new_quad == saber_moveData[curmove].endQuad)
	{
		//player is attempting to do a fake move to the same quadrant
		//as such, fake to the completely opposite quad
		new_quad += 4;
		if (new_quad > Q_B)
		{
			//rotated past Q_B, shift back to get the proper quadrant
			new_quad -= Q_NUM_QUADS;
		}
	}

	if (new_quad == Q_B)
	{
		//attacks can't be launched from this quad, just randomly fake to the bottom left/right
		if (PM_irand_timesync(0, 9) <= 4)
		{
			new_quad = Q_BL;
		}
		else
		{
			new_quad = Q_BR;
		}
	}

	//add faking flag
	pm->ps->userInt3 |= 1 << FLAG_ATTACKFAKE;
	return transitionMove[saber_moveData[curmove].endQuad][new_quad];
}

static void PM_WeaponLightsaber()
{
	qboolean delayed_fire = qfalse, anim_level_overridden = qfalse;
	int anim = -1;
	int newmove = LS_NONE;

	const qboolean is_holding_block_button = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;
	//Holding Block Button
	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking
	const qboolean walking_blocking = pm->ps->ManualBlockingFlags & 1 << MBF_BLOCKWALKING ? qtrue : qfalse;
	//Walking Blocking

	if (pm->gent
		&& pm->gent->client
		&& (pm->gent->client->NPC_class == CLASS_SABER_DROID || pm->gent->client->NPC_class == CLASS_SBD))
	{
		//Saber droid does it's own attack logic
		PM_SaberDroidWeapon();
		return;
	}

	if (g_SerenityJediEngineMode->integer >= 0)
	{
		if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		{
			//preblocks can be interrupted
			if (PM_SaberInParry(pm->ps->saber_move) && pm->ps->userInt3 & 1 << FLAG_PREBLOCK // in a pre-block
				&& (pm->cmd.buttons & BUTTON_ALT_ATTACK || pm->cmd.buttons & BUTTON_ATTACK))
				//and attempting an attack
			{
				//interrupting a preblock
				pm->ps->weaponTime = 0;
				pm->ps->torsoAnimTimer = 0;
			}
		}

		if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
		{
			if (!(pm->cmd.buttons & BUTTON_ATTACK)
				&& !PM_SaberInSpecial(pm->ps->saber_move)
				&& !PM_SaberInStart(pm->ps->saber_move)
				&& !PM_SaberInTransition(pm->ps->saber_move)
				&& !PM_SaberInAttack(pm->ps->saber_move)
				&& !debugNPCFreeze->integer
				&& pm->ps->damageTime < cg.time
				&& pm->ps->saber_move > LS_PUTAWAY)
			{
				// Always return to ready when attack is released...
				PM_SetSaberMove(LS_READY);
				return;
			}
		}
	}

	// don't allow attack until all buttons are up
	if (pm->ps->pm_flags & PMF_RESPAWNED)
	{
		return;
	}

	// check for dead player
	if (pm->ps->stats[STAT_HEALTH] <= 0)
	{
		if (pm->gent)
		{
			pm->gent->s.loopSound = 0;
		}
		return;
	}

	// make weapon function
	if (pm->ps->weaponTime > 0)
	{
		//check for special pull move while busy
		const saber_moveName_t pullmove = PM_CheckPullAttack();
		if (pullmove != LS_NONE)
		{
			pm->ps->weaponTime = 0;
			pm->ps->torsoAnimTimer = 0;
			pm->ps->legsAnimTimer = 0;

			pm->ps->weaponstate = WEAPON_READY;
			PM_SetSaberMove(pullmove);
			return;
		}

		pm->ps->weaponTime -= pml.msec;
	}
	else
	{
		if (pm->cmd.buttons & BUTTON_KICK && pm->ps->communicatingflags & 1 << KICKING
			&& pm->gent->client->NPC_class != CLASS_DROIDEKA)
		{
			//allow them to do the kick now!
			pm->ps->weaponTime = 0;
			pm_melee_move_for_conditions();
			return;
		}
		pm->ps->weaponstate = WEAPON_READY;
	}

	if (pm->ps->weapon == WP_SCEPTER
		&& !pm->ps->dualSabers
		&& pm->gent
		&& pm->gent->weaponModel[1])
	{
		//holding scepter in left hand, use dual style
		pm->ps->saber_anim_level = SS_DUAL;
		anim_level_overridden = qtrue;
	}
	else if (pm->ps->saber[0].singleBladeStyle != SS_NONE
		&& !pm->ps->dualSabers
		&& pm->ps->saber[0].blade[0].active
		&& !pm->ps->saber[0].blade[1].active)
	{
		//using a staff, but only with first blade turned on, so use is as a normal saber...?
		//override so that single-blade staff must be used with specified style
		pm->ps->saber_anim_level = pm->ps->saber[0].singleBladeStyle;
		anim_level_overridden = qtrue;
	}
	else if (pm->gent
		&& cg.saberAnimLevelPending != pm->ps->saber_anim_level
		&& WP_SaberStyleValidForSaber(pm->gent, cg.saberAnimLevelPending))
	{
		//go ahead and use the cg.saberAnimLevelPending below
		anim_level_overridden = qfalse;
	}
	else if (pm->gent
		&& (WP_SaberStyleValidForSaber(pm->gent, pm->ps->saber_anim_level)
			|| WP_UseFirstValidSaberStyle(pm->gent, &pm->ps->saber_anim_level)))
	{
		//style we are using is not valid, switched us to a valid one
		anim_level_overridden = qtrue;
	}
	else if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].Active())
		{
			//if second saber is on, must use dual style
			pm->ps->saber_anim_level = SS_DUAL;
			anim_level_overridden = qtrue;
		}
		else if (pm->ps->saber[0].Active())
		{
			//with only one saber on, use fast style
			pm->ps->saber_anim_level = SS_FAST;
			anim_level_overridden = qtrue;
		}
	}
	if (!anim_level_overridden)
	{
		if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
			&& cg.saberAnimLevelPending > SS_NONE
			&& cg.saberAnimLevelPending != pm->ps->saber_anim_level)
		{
			if (!PM_SaberInStart(pm->ps->saber_move)
				&& !PM_SaberInTransition(pm->ps->saber_move)
				&& !PM_SaberInAttack(pm->ps->saber_move))
			{
				//don't allow changes when in the middle of an attack set...(or delay the change until it's done)
				pm->ps->saber_anim_level = cg.saberAnimLevelPending;
			}
		}
	}
	else if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		//if override the player's saber_anim_level, let the cgame know
		cg.saberAnimLevelPending = pm->ps->saber_anim_level;
	}

	if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps))
	{
		//in knockdown
		if (pm->ps->legsAnim == BOTH_ROLL_F ||
			pm->ps->legsAnim == BOTH_ROLL_F1 ||
			pm->ps->legsAnim == BOTH_ROLL_F2
			&& pm->ps->legsAnimTimer <= 250)
		{
			if (pm->cmd.buttons & BUTTON_ATTACK
				|| PM_NPCCheckAttackRoll())
			{
				if (PM_CanDoRollStab())
				{
					//okay to do roll-stab
					PM_SetSaberMove(LS_ROLL_STAB);
					WP_ForcePowerDrain(pm->gent, FP_SABER_OFFENSE, SABER_KATA_ATTACK_POWER);
				}
			}
		}
		return;
	}

	if (PM_SaberLocked())
	{
		pm->ps->saber_move = LS_NONE;
		return;
	}

	if (pm->ps->torsoAnim == BOTH_FORCELONGLEAP_LAND
		|| pm->ps->torsoAnim == BOTH_FORCELONGLEAP_START && !(pm->cmd.buttons & BUTTON_ATTACK))
	{
		//if you're in the long-jump and you're not attacking (or are landing), you're not doing anything
		if (pm->ps->torsoAnimTimer)
		{
			return;
		}
	}

	if (pm->ps->legsAnim == BOTH_FLIP_HOLD7
		&& !(pm->cmd.buttons & BUTTON_ATTACK))
	{
		//if you're in the upside-down attack hold, don't do anything unless you're attacking
		return;
	}

	if (PM_KickingAnim(pm->ps->legsAnim) ||
		PM_KickingAnim(pm->ps->torsoAnim))
	{
		if (pm->ps->legsAnimTimer > 0)
		{
			//you're kicking, no interruptions
			return;
		}
		//done?  be immediately ready to do an attack
		pm->ps->saber_move = LS_READY;
		pm->ps->weaponTime = 0;
	}

	if (pm->ps->saberMoveNext != LS_NONE
		&& (pm->ps->saber_move == LS_READY || pm->ps->saber_move == LS_NONE)) //ready for another one
	{
		//something is forcing us to set a specific next saber_move
		PM_SetSaberMove(static_cast<saber_moveName_t>(pm->ps->saberMoveNext));
		pm->ps->saberMoveNext = LS_NONE; //clear it now that we played it
		return;
	}

	if (pm->ps->saberEventFlags & SEF_INWATER) //saber in water
	{
		pm->cmd.buttons &= ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_FORCE_FOCUS | BUTTON_DASH);
	}

	qboolean saber_in_air = qtrue;

	if (!PM_SaberInBrokenParry(pm->ps->saber_move) && pm->ps->saberBlocked != BLOCKED_PARRY_BROKEN && !PM_DodgeAnim(
		pm->ps->torsoAnim) &&
		pm->ps->weaponstate != WEAPON_CHARGING_ALT && pm->ps->weaponstate != WEAPON_CHARGING)
	{
		//we're not stuck in a broken parry
		if (pm->ps->saberInFlight)
		{
			//guiding saber
			if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0) //player is 0
			{
				//
				if (&g_entities[pm->ps->saberEntityNum] != nullptr && g_entities[pm->ps->saberEntityNum].s.pos.
					trType == TR_STATIONARY)
				{
					//fell to the ground and we're not trying to pull it back
					saber_in_air = qfalse;
				}
			}
			if (pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING)
			{
				if (pm->ps->weapon != pm->cmd.weapon)
				{
					PM_BeginWeaponChange(pm->cmd.weapon);
				}
			}
			else if (pm->ps->weapon == WP_SABER
				&& (!pm->ps->dualSabers || !pm->ps->saber[1].Active()))
			{
				//guiding saber
				if (saber_in_air)
				{
					if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300)
					{
						//don't interrupt a force power anim
						if (pm->ps->torsoAnim != BOTH_LOSE_SABER && !PM_SaberInMassiveBounce(pm->ps->torsoAnim)
							|| !pm->ps->torsoAnimTimer)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERPULL,
								SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						}
					}
				}
				return;
			}
		}
	}

	if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
	{
		//FIXME: this is going to fire off one frame before you expect, actually
		pm->gent->client->fireDelay -= pml.msec;
		if (pm->gent->client->fireDelay <= 0)
		{
			//just finished delay timer
			pm->gent->client->fireDelay = 0;
			delayed_fire = qtrue;
		}
	}

	PM_CheckClearSaberBlock();

	if (PM_LockedAnim(pm->ps->torsoAnim)
		&& pm->ps->torsoAnimTimer)
	{
		//can't interrupt these anims ever
		return;
	}

	if ((PM_SuperBreakLoseAnim(pm->ps->torsoAnim) || PM_SuperBreakWinAnim(pm->ps->torsoAnim))
		&& pm->ps->torsoAnimTimer)
	{
		//don't interrupt these anims
		return;
	}

	if (PM_SaberBlocking())
	{
		//busy blocking, don't do attacks
		return;
	}

	// check for weapon change
	// can't change if weapon is firing, but can change again if lowering or raising
	if ((pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING) && pm->ps->weaponstate !=
		WEAPON_CHARGING_ALT && pm->ps->weaponstate != WEAPON_CHARGING)
	{
		if (pm->ps->weapon != pm->cmd.weapon)
		{
			PM_BeginWeaponChange(pm->cmd.weapon);
		}
	}

	if (pm->ps->weaponTime > 0)
	{
		if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
			&& PM_SaberInReturn(pm->ps->saber_move) //in a saber return move - FIXME: what about transitions?
			&& pm->ps->saberBlocked == BLOCKED_NONE //not interacting with any other saber
			&& !(pm->cmd.buttons & BUTTON_ATTACK) //not trying to swing the saber
			&& (pm->cmd.forwardmove || pm->cmd.rightmove) &&
			pm->ps->communicatingflags & 1 << KICKING) //trying to kick in a specific direction
		{
			if (PM_CheckAltKickAttack()) //trying to do a kick
			{
				//allow them to do the kick now!
				pm->ps->weaponTime = 0;
				PM_CheckKick();
				return;
			}
		}
		else
		{
			if (!pm->cmd.rightmove
				&& !pm->cmd.forwardmove
				&& pm->cmd.buttons & BUTTON_ATTACK)
			{
				if (!g_saberNewControlScheme->integer)
				{
					const saber_moveName_t pullAtk = PM_CheckPullAttack();
					if (pullAtk != LS_NONE)
					{
						PM_SetSaberMove(pullAtk);
						pm->ps->weaponstate = WEAPON_FIRING;
						return;
					}
				}
			}
			else
			{
				return;
			}
		}
	}

	// *********************************************************
	// WEAPON_DROPPING
	// *********************************************************

	// change weapon if time
	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		PM_FinishWeaponChange();
		return;
	}

	// *********************************************************
	// WEAPON_RAISING
	// *********************************************************

	if (pm->ps->weaponstate == WEAPON_RAISING)
	{
		//Just selected the weapon
		pm->ps->weaponstate = WEAPON_IDLE;

		if (pm->gent && (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent)))
		{
			switch (pm->ps->legsAnim)
			{
			case BOTH_WALK1:
			case BOTH_WALK1_MDA:
			case BOTH_WALK2:
			case BOTH_WALK_STAFF:
			case BOTH_WALK_STAFF_AMD:
			case BOTH_WALK_DUAL:
			case BOTH_WALK_DUAL_AMD:
			case BOTH_WALKBACK1:
			case BOTH_WALKBACK_PISTOL:
			case BOTH_WALKBACK_BLASTER:
			case BOTH_WALKBACK_HEAVY:
			case BOTH_WALKBACK_GRENADE:
			case BOTH_WALKBACK2:
			case BOTH_WALKBACK_STAFF:
			case BOTH_WALKBACK_DUAL:
			case BOTH_WALKBACK_DUALPISTOL:
			case BOTH_RUN1:
			case BOTH_SPRINT:
			case BOTH_SPRINT_SINGLE_LIGHTSABER:
			case BOTH_SPRINT_STAFF_LIGHTSABER:
			case BOTH_SPRINT_DUAL_LIGHTSABER:
			case BOTH_RUN2:
			case BOTH_RUN_STAFF:
			case BOTH_RUN_DUAL:
			case BOTH_RUNBACK1:
			case BOTH_RUNBACK2:
			case BOTH_RUNBACK_STAFF:
			case BOTH_MENUIDLE1:
			case BOTH_SABERSINGLECROUCH:
			case BOTH_SABERDUALCROUCH:
			case BOTH_SABERSTAFFCROUCH:
			case BOTH_WALK1_STICK:
				PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
				break;
			default:
				if (is_holding_block_button)
				{
					if (pm->ps->saber_anim_level == SS_DUAL)
					{
						if (g_SerenityJediEngineMode->integer == 2)
						{
							anim = PM_BlockingPoseForSaberAnimLevelDualAMD();
						}
						else
						{
							anim = PM_BlockingPoseForSaberAnimLevelDualMD();
						}
					}
					else if (pm->ps->saber_anim_level == SS_STAFF)
					{
						if (g_SerenityJediEngineMode->integer == 2)
						{
							anim = PM_BlockingPoseForSaberAnimLevelStaffAMD();
						}
						else
						{
							anim = PM_BlockingPoseForSaberAnimLevelStaffMD();
						}
					}
					else
					{
						if (g_SerenityJediEngineMode->integer == 2)
						{
							anim = PM_BlockingPoseForSaberAnimLevelSingleAMD();
						}
						else
						{
							anim = PM_BlockingPoseForSaberAnimLevelSingleMD();
						}
					}
				}
				else
				{
					anim = PM_ReadyPoseForSaberAnimLevel();
				}
				if (anim != -1)
				{
					PM_SetAnim(pm, SETANIM_TORSO, anim, SETANIM_FLAG_NORMAL);
				}
				break;
			}
		}
		else
		{
			qboolean in_air = qtrue;
			if (pm->ps->saberInFlight)
			{
				//guiding saber
				if (PM_SaberInBrokenParry(pm->ps->saber_move) || pm->ps->saberBlocked == BLOCKED_PARRY_BROKEN ||
					PM_DodgeAnim(pm->ps->torsoAnim))
				{
					//we're stuck in a broken parry
					in_air = qfalse;
				}
				if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0) //player is 0
				{
					//
					if (&g_entities[pm->ps->saberEntityNum] != nullptr && g_entities[pm->ps->saberEntityNum].s.pos.
						trType == TR_STATIONARY)
					{
						//fell to the ground and we're not trying to pull it back
						in_air = qfalse;
					}
				}
			}
			if (pm->ps->weapon == WP_SABER
				&& pm->ps->saberInFlight
				&& in_air
				&& (!pm->ps->dualSabers || !pm->ps->saber[1].Active()))
			{
				//guiding saber
				if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300)
				{
					//don't interrupt a force power anim
					if (pm->ps->torsoAnim != BOTH_LOSE_SABER && !PM_SaberInMassiveBounce(pm->ps->torsoAnim)
						|| !pm->ps->torsoAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERPULL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
				}
			}
			else
			{
				PM_SetSaberMove(LS_READY);
			}
		}
		return;
	}

	// *********************************************************
	// Check for WEAPON ATTACK
	// *********************************************************

	if (PM_CanDoKata())
	{
		saber_moveName_t override_move = LS_INVALID;
		//see if we have an overridden (or cancelled) kata move
		if (pm->ps->saber[0].kataMove != LS_INVALID)
		{
			if (pm->ps->saber[0].kataMove != LS_NONE)
			{
				override_move = static_cast<saber_moveName_t>(pm->ps->saber[0].kataMove);
			}
		}
		if (override_move == LS_INVALID)
		{
			//not overridden by first saber, check second
			if (pm->ps->dualSabers)
			{
				if (pm->ps->saber[1].kataMove != LS_INVALID)
				{
					if (pm->ps->saber[1].kataMove != LS_NONE)
					{
						override_move = static_cast<saber_moveName_t>(pm->ps->saber[1].kataMove);
					}
				}
			}
		}
		//no overrides, cancelled?
		if (override_move == LS_INVALID)
		{
			if (pm->ps->saber[0].kataMove == LS_NONE)
			{
				override_move = LS_NONE;
			}
			else if (pm->ps->dualSabers)
			{
				if (pm->ps->saber[1].kataMove == LS_NONE)
				{
					override_move = LS_NONE;
				}
			}
		}
		if (override_move == LS_INVALID)
		{
			switch (pm->ps->saber_anim_level)
			{
			case SS_FAST:
				if (pm->ps->dualSabers && pm->ps->saber[1].Active())
				{
					PM_SetSaberMove(LS_DUAL_SPIN_PROTECT);
				}
				else
				{
					PM_SetSaberMove(LS_A1_SPECIAL);
				}
				break;
			case SS_TAVION:
				PM_SetSaberMove(LS_A1_SPECIAL);
				break;
			case SS_MEDIUM:
				PM_SetSaberMove(LS_A2_SPECIAL);
				break;
			case SS_STRONG:
				PM_SetSaberMove(LS_A3_SPECIAL);
				break;
			case SS_DESANN:
				PM_SetSaberMove(LS_A3_SPECIAL);
				break;
			case SS_DUAL:
				PM_SetSaberMove(LS_DUAL_SPIN_PROTECT);
				break;
			case SS_STAFF:
				PM_SetSaberMove(LS_STAFF_SOULCAL);
				break;
			default:;
			}
			pm->ps->weaponstate = WEAPON_FIRING;
			G_DrainPowerForSpecialMove(pm->gent, FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER, qtrue);
		}
		else if (override_move != LS_NONE)
		{
			PM_SetSaberMove(override_move);
			pm->ps->weaponstate = WEAPON_FIRING;
			G_DrainPowerForSpecialMove(pm->gent, FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER, qtrue);
		}
		if (override_move != LS_NONE)
		{
			//not cancelled
			return;
		}
	}

	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
		&& PM_CheckAltKickAttack() &&
		pm->ps->communicatingflags & 1 << KICKING)
	{
		//trying to do a kick
		if (pm->ps->saber_anim_level == SS_STAFF)
		{
			//NPCs spin the staff
			PM_SetSaberMove(LS_SPINATTACK);
			return;
		}
		PM_CheckKick();
		return;
	}

	if (PM_CheckUpsideDownAttack())
	{
		return;
	}

	if (!delayed_fire)
	{
		int curmove;
		// Start with the current move, and cross index it with the current control states.
		if (pm->ps->saber_move > LS_NONE && pm->ps->saber_move < LS_MOVE_MAX)
		{
			curmove = static_cast<saber_moveName_t>(pm->ps->saber_move);
		}
		else
		{
			curmove = LS_READY;
		}

		if (curmove == LS_A_JUMP_T__B_ || pm->ps->torsoAnim == BOTH_FORCELEAP2_T__B_)
		{
			//must transition back to ready from this anim
			newmove = LS_R_T2B;
		}
		else if (!(pm->cmd.buttons & BUTTON_ATTACK) || is_holding_block_button || active_blocking || walking_blocking)
		{
			//not attacking
			pm->ps->weaponTime = 0;

			if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
			{
				//Still firing
				pm->ps->weaponstate = WEAPON_FIRING;
			}
			else if (pm->ps->weaponstate != WEAPON_READY)
			{
				pm->ps->weaponstate = WEAPON_IDLE;
			}
			//Check for finishing an anim if necc.
			if (curmove >= LS_S_TL2BR && curmove <= LS_S_T2B)
			{
				//started a swing, must continue from here
				if (g_SerenityJediEngineMode->integer == 2)
				{
					if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
					{
						//NPCs never do attack fakes, just follow thru with attack.
						newmove = LS_A_TL2BR + (curmove - LS_S_TL2BR);
					}
					else
					{
						//perform attack fake
						newmove = pm_returnfor_quad(saber_moveData[curmove].endQuad);
					}
				}
				else
				{
					newmove = LS_A_TL2BR + (curmove - LS_S_TL2BR);
				}
			}
			else if (curmove >= LS_A_TL2BR && curmove <= LS_A_T2B)
			{
				//finished an attack, must continue from here
				newmove = LS_R_TL2BR + (curmove - LS_A_TL2BR);
			}
			else if (PM_SaberInTransition(curmove))
			{
				//in a transition, must play sequential attack
				if (g_SerenityJediEngineMode->integer == 2)
				{
					if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
					{
						//NPCs never stop attacking mid-attack, just follow thru with attack.
						newmove = saber_moveData[curmove].chain_attack;
					}
					else
					{
						//exit out of transition without attacking
						newmove = pm_returnfor_quad(saber_moveData[curmove].endQuad);
					}
				}
				else
				{
					newmove = saber_moveData[curmove].chain_attack;
				}
			}
			else if (PM_SaberInBounce(curmove))
			{
				//in a bounce
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					//NPCs must play sequential attack
					if (PM_SaberKataDone(LS_NONE, LS_NONE))
					{
						//done with this kata, must return to ready before attack again
						newmove = saber_moveData[curmove].chain_idle;
					}
					else
					{
						//okay to chain to another attack
						newmove = saber_moveData[curmove].chain_attack;
						//we assume they're attacking, even if they're not
						pm->ps->saberAttackChainCount++;

						if (g_SerenityJediEngineMode->integer)
						{
							if (pm->ps->saberFatigueChainCount < MISHAPLEVEL_MAX)
							{
								pm->ps->saberFatigueChainCount++;
							}
						}
					}
				}
				else
				{
					//player gets his by directional control
					newmove = saber_moveData[curmove].chain_idle; //oops, not attacking, so don't chain
				}
			}
			else
			{
				if (pm->ps->saberBlockingTime > cg.time)
				{
					PM_SetSaberMove(LS_READY);
				}
				return;
			}
		}
		else if (pm->cmd.buttons & BUTTON_ATTACK && pm->cmd.buttons & BUTTON_USE && g_SerenityJediEngineMode->
			integer
			&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()))
		{
			//do some fancy faking stuff.
			if (pm->ps->weaponstate != WEAPON_READY)
			{
				pm->ps->weaponstate = WEAPON_IDLE;
			}
			//Check for finishing an anim if necc.
			if (curmove >= LS_S_TL2BR && curmove <= LS_S_T2B)
			{
				//allow the player to fake into another transition
				newmove = PM_DoFake(curmove);
				if (newmove == LS_NONE)
				{
					//no movement, just do the attack
					newmove = LS_A_TL2BR + (curmove - LS_S_TL2BR);
				}
			}
			else if (curmove >= LS_A_TL2BR && curmove <= LS_A_T2B)
			{
				//finished attack, let attack code handle the next step.
			}
			else if (PM_SaberInTransition(curmove))
			{
				//in a transition, must play sequential attack
				newmove = PM_DoFake(curmove);
				if (newmove == LS_NONE)
				{
					//no movement, just let the normal attack code handle it
					newmove = saber_moveData[curmove].chain_attack;
				}
			}
			else if (PM_SaberInBounce(curmove))
			{
				//in a bounce
			}
			else
			{
				//returning from a parry I think.
			}
		}

		// ***************************************************
		// Pressing attack, so we must look up the proper attack move.
		qboolean in_air = qtrue;

		if (pm->ps->saberInFlight)
		{
			//guiding saber
			if (PM_SaberInBrokenParry(pm->ps->saber_move) || pm->ps->saberBlocked == BLOCKED_PARRY_BROKEN ||
				PM_DodgeAnim(pm->ps->torsoAnim))
			{
				//we're stuck in a broken parry
				in_air = qfalse;
			}
			if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)
			{
				if (&g_entities[pm->ps->saberEntityNum] != nullptr && g_entities[pm->ps->saberEntityNum].s.pos.
					trType == TR_STATIONARY)
				{
					//fell to the ground and we're not trying to pull it back
					in_air = qfalse;
				}
			}
		}

		if (pm->ps->weapon == WP_SABER
			&& pm->ps->saberInFlight
			&& in_air
			&& (!pm->ps->dualSabers || !pm->ps->saber[1].Active()))
		{
			//guiding saber
			if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300)
			{
				//don't interrupt a force power anim
				if (pm->ps->torsoAnim != BOTH_LOSE_SABER && !PM_SaberInMassiveBounce(pm->ps->torsoAnim)
					|| !pm->ps->torsoAnimTimer)
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERPULL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				}
			}
		}

		// ***************************************************
		// Pressing attack, so we must look up the proper attack move.

		else if (pm->ps->weaponTime > 0)
		{
			// Last attack is not yet complete.
			pm->ps->weaponstate = WEAPON_FIRING;
			return;
		}
		else
		{
			int both = qfalse;
			if (pm->ps->torsoAnim == BOTH_FORCELONGLEAP_ATTACK
				|| pm->ps->torsoAnim == BOTH_FORCELONGLEAP_LAND)
			{
				//can't attack in these anims
				return;
			}
			if (pm->ps->torsoAnim == BOTH_FORCELONGLEAP_START)
			{
				//only 1 attack you can do from this anim
				if (pm->ps->torsoAnimTimer >= 200)
				{
					//hit it early enough to do the attack
					if (!pm->ps->SaberActive())
					{
						pm->ps->SaberActivate();
					}
					//hit it early enough to do the attack
					PM_SetSaberMove(LS_LEAP_ATTACK);
				}
				return;
			}
			if (curmove >= LS_PARRY_UP && curmove <= LS_REFLECT_LL)
			{
				//from a parry or reflection, can go directly into an attack
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{//NPCs
					newmove = PM_NPCSaberAttackFromQuad(saber_moveData[curmove].endQuad);
				}
				else
				{
					newmove = PM_SaberAttackForMovement(pm->cmd.forwardmove, pm->cmd.rightmove, curmove);
				}
			}

			if (newmove != LS_NONE)
			{
				//have a valid, final LS_ move picked, so skip finding the transition move and just get the anim
				if (PM_HasAnimation(pm->gent, saber_moveData[newmove].animToUse))
				{
					anim = saber_moveData[newmove].animToUse;
				}
			}

			if (anim == -1)
			{
				if (PM_SaberInTransition(curmove))
				{
					//in a transition, must play sequential attack
					newmove = saber_moveData[curmove].chain_attack;
				}
				else if (curmove >= LS_S_TL2BR && curmove <= LS_S_T2B)
				{
					//started a swing, must continue from here
					newmove = LS_A_TL2BR + (curmove - LS_S_TL2BR);
				}
				else if (PM_SaberInBrokenParry(curmove))
				{
					//broken parries must always return to ready
					newmove = LS_READY;
				}
				else if (PM_SaberInBounce(curmove) && pm->ps->userInt3 & 1 << FLAG_BLOCKED)
				{
					//can't combo if we were parried.
					newmove = LS_READY;
				}
				else
				{
					//get attack move from movement command
					if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()
						&& (Q_irand(0, pm->ps->forcePowerLevel[FP_SABER_OFFENSE] - 1)
							|| pm->gent && pm->gent->enemy && pm->gent->enemy->client
							&& PM_InKnockDownOnGround(&pm->gent->enemy->client->ps)
							//enemy knocked down, use some logic
							|| pm->ps->saber_anim_level == SS_FAST && pm->gent
							&& pm->gent->NPC && pm->gent->NPC->rank >= RANK_LT_JG && Q_irand(0, 1)))
						//minor change to make fast-attack users use the special attacks more
					{//NPCs use more randomized attacks the more skilled they are
						newmove = PM_NPCSaberAttackFromQuad(saber_moveData[curmove].endQuad);
					}
					else
					{
						newmove = PM_SaberAttackForMovement(pm->cmd.forwardmove, pm->cmd.rightmove, curmove);

						if (g_SerenityJediEngineMode->integer)
						{
							if (pm->ps->saberFatigueChainCount < MISHAPLEVEL_TEN) //npc or low fatigue
							{
								if ((PM_SaberInBounce(curmove) || PM_SaberInBrokenParry(curmove))
									&& saber_moveData[newmove].startQuad == saber_moveData[curmove].endQuad)
								{
									//this attack would be a repeat of the last (which was blocked), so don't actually use it, use the default chain attack for this bounce
									newmove = saber_moveData[curmove].chain_attack;
								}
							}
							else
							{
								if ((PM_SaberInBounce(curmove) || PM_SaberInParry(curmove))
									&& newmove >= LS_A_TL2BR && newmove <= LS_A_T2B)
								{
									//prevent similar attack directions to prevent lightning-like bounce attacks.
									if (saber_moveData[newmove].startQuad == saber_moveData[curmove].endQuad)
									{
										//can't attack in the same direction
										newmove = LS_READY;
									}
								}
							}
						}
						else
						{
							if ((PM_SaberInBounce(curmove) || PM_SaberInBrokenParry(curmove))
								&& saber_moveData[newmove].startQuad == saber_moveData[curmove].endQuad)
							{
								//this attack would be a repeat of the last (which was blocked), so don't actually use it, use the default chain attack for this bounce
								newmove = saber_moveData[curmove].chain_attack;
							}
						}

						//starting a new attack, as such, remove the attack fake flag.
						pm->ps->userInt3 &= ~(1 << FLAG_ATTACKFAKE);
					}

					if (PM_SaberKataDone(curmove, newmove))
					{
						//cannot chain this time
						newmove = saber_moveData[curmove].chain_idle;
					}
				}

				if (newmove != LS_NONE)
				{
					if (!PM_InCartwheel(pm->ps->legsAnim))
					{
						//don't do transitions when cartwheeling - could make you spin!
						//Now get the proper transition move
						newmove = PM_SaberAnimTransitionMove(static_cast<saber_moveName_t>(curmove),
							static_cast<saber_moveName_t>(newmove));

						if (PM_HasAnimation(pm->gent, saber_moveData[newmove].animToUse))
						{
							anim = saber_moveData[newmove].animToUse;
						}
					}
				}
			}

			if (anim == -1)
			{
				//not side-stepping, pick neutral anim
				if (!G_TryingSpecial(&pm->cmd))
				{
					//but only if not trying one of the special attacks!
					if (PM_InCartwheel(pm->ps->legsAnim)
						&& pm->ps->legsAnimTimer > 100)
					{
						//if in the middle of a cartwheel, the chain attack is just a normal attack
						//NOTE: this should match the switch in PM_InCartwheel!
						switch (pm->ps->legsAnim)
						{
						case BOTH_ARIAL_LEFT: //swing from l to r
						case BOTH_CARTWHEEL_LEFT:
							newmove = LS_A_L2R;
							break;
						case BOTH_ARIAL_RIGHT: //swing from r to l
						case BOTH_CARTWHEEL_RIGHT:
							newmove = LS_A_R2L;
							break;
						case BOTH_ARIAL_F1: //random l/r attack
							if (Q_irand(0, 1))
							{
								newmove = LS_A_L2R;
							}
							else
							{
								newmove = LS_A_R2L;
							}
							break;
						default:;
						}
					}
					else
					{
						// Add randomness for prototype?
						newmove = saber_moveData[curmove].chain_attack;
					}
					if (newmove != LS_NONE)
					{
						if (PM_HasAnimation(pm->gent, saber_moveData[newmove].animToUse))
						{
							anim = saber_moveData[newmove].animToUse;
						}
					}
				}

				if (!pm->cmd.forwardmove && !pm->cmd.rightmove && pm->cmd.upmove >= 0 && pm->ps->groundEntityNum !=
					ENTITYNUM_NONE)
				{
					//not moving at all, so set the anim on entire body
					both = qtrue;
				}
			}

			if (anim == -1)
			{
				switch (pm->ps->legsAnim)
				{
				case BOTH_WALK1:
				case BOTH_WALK1_MDA:
				case BOTH_WALK2:
				case BOTH_WALK_STAFF:
				case BOTH_WALK_STAFF_AMD:
				case BOTH_WALK_DUAL:
				case BOTH_WALK_DUAL_AMD:
				case BOTH_WALKBACK1:
				case BOTH_WALKBACK_PISTOL:
				case BOTH_WALKBACK_BLASTER:
				case BOTH_WALKBACK_HEAVY:
				case BOTH_WALKBACK_GRENADE:
				case BOTH_WALKBACK2:
				case BOTH_WALKBACK_STAFF:
				case BOTH_WALKBACK_DUAL:
				case BOTH_WALKBACK_DUALPISTOL:
				case BOTH_RUN1:
				case BOTH_SPRINT:
				case BOTH_SPRINT_SINGLE_LIGHTSABER:
				case BOTH_SPRINT_STAFF_LIGHTSABER:
				case BOTH_SPRINT_DUAL_LIGHTSABER:
				case BOTH_RUN2:
				case BOTH_RUN_STAFF:
				case BOTH_RUN_DUAL:
				case BOTH_RUNBACK1:
				case BOTH_RUNBACK2:
				case BOTH_RUNBACK_STAFF:
				case BOTH_MENUIDLE1:
				case BOTH_WALK1_STICK:
					pm->ps->legsAnim;
					break;
				default:
					if (is_holding_block_button)
					{
						if (pm->ps->saber_anim_level == SS_DUAL)
						{
							if (g_SerenityJediEngineMode->integer == 2)
							{
								PM_BlockingPoseForSaberAnimLevelDualAMD();
							}
							else
							{
								PM_BlockingPoseForSaberAnimLevelDualMD();
							}
						}
						else if (pm->ps->saber_anim_level == SS_STAFF)
						{
							if (g_SerenityJediEngineMode->integer == 2)
							{
								PM_BlockingPoseForSaberAnimLevelStaffAMD();
							}
							else
							{
								PM_BlockingPoseForSaberAnimLevelStaffMD();
							}
						}
						else
						{
							if (g_SerenityJediEngineMode->integer == 2)
							{
								PM_BlockingPoseForSaberAnimLevelSingleAMD();
							}
							else
							{
								PM_BlockingPoseForSaberAnimLevelSingleMD();
							}
						}
					}
					else
					{
						PM_ReadyPoseForSaberAnimLevel();
					}
					break;
				}
				newmove = LS_READY;
			}

			if (!pm->ps->SaberActive())
			{
				//turn on the saber if it's not on
				pm->ps->SaberActivate();
			}

			PM_SetSaberMove(static_cast<saber_moveName_t>(newmove));

			if (both && pm->ps->legsAnim != pm->ps->torsoAnim)
			{
				PM_SetAnim(pm, SETANIM_LEGS, pm->ps->torsoAnim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			if (!PM_InCartwheel(pm->ps->torsoAnim))
			{
				//can still attack during a cartwheel/arial
				//don't fire again until anim is done
				pm->ps->weaponTime = pm->ps->torsoAnimTimer;
			}
		}
	}

	// *********************************************************
	// WEAPON_FIRING
	// *********************************************************

	pm->ps->weaponstate = WEAPON_FIRING;

	if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
	{
		//FIXME: this is going to fire off one frame before you expect, actually
		// Clear these out since we're not actually firing yet
		pm->ps->eFlags &= ~EF_FIRING;
		pm->ps->eFlags &= ~EF_ALT_FIRING;
		return;
	}

	int add_time = pm->ps->weaponTime;

	if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		PM_AddEvent(EV_ALT_FIRE);

		if (!add_time)
		{
			add_time = weaponData[pm->ps->weapon].altFireTime;

			if (g_timescale != nullptr)
			{
				if (g_timescale->value < 1.0f)
				{
					if (!MatrixMode)
					{
						//Special test for Matrix Mode (tm)
						if (pm->ps->client_num == 0 && !player_locked && !PlayerAffectedByStasis() && pm->ps->
							forcePowersActive & 1 << FP_SPEED)
						{
							//player always fires at normal speed
							add_time *= g_timescale->value;
						}
						else if (g_entities[pm->ps->client_num].client
							&& pm->ps->forcePowersActive & 1 << FP_SPEED)
						{
							add_time *= g_timescale->value;
						}
					}
				}
			}
		}
	}
	else
	{
		PM_AddEvent(EV_FIRE_WEAPON);

		if (!add_time)
		{
			add_time = weaponData[pm->ps->weapon].fireTime;

			if (g_timescale != nullptr)
			{
				if (g_timescale->value < 1.0f)
				{
					if (!MatrixMode)
					{
						//Special test for Matrix Mode (tm)
						if (pm->ps->client_num == 0 && !player_locked && !PlayerAffectedByStasis() && pm->ps->
							forcePowersActive & 1 << FP_SPEED)
						{
							//player always fires at normal speed
							add_time *= g_timescale->value;
						}
						else if (g_entities[pm->ps->client_num].client
							&& pm->ps->forcePowersActive & 1 << FP_SPEED)
						{
							add_time *= g_timescale->value;
						}
					}
				}
			}
		}
	}

	if (pm->ps->forcePowersActive & 1 << FP_RAGE)
	{
		add_time *= 0.75;
	}
	else if (pm->ps->forceRageRecoveryTime > pm->cmd.serverTime)
	{
		add_time *= 1.5;
	}

	//If the phaser has been fired, delay the next recharge time
	if (!PM_ControlledByPlayer())
	{
		if (pm->gent && pm->gent->NPC != nullptr)
		{
			//NPCs have their own re fire logic
			//FIXME: this really should be universal...
			return;
		}
	}

	if (!PM_InCartwheel(pm->ps->torsoAnim))
	{
		//can still attack during a cartwheel/arial
		pm->ps->weaponTime = add_time;
	}
}

//---------------------------------------
static bool PM_DoChargedWeapons()
//---------------------------------------
{
	qboolean charging = qfalse,
		alt_fire = qfalse;

	//FIXME: make jedi aware they're being aimed at with a charged-up weapon (strafe and be evasive?)
	// If you want your weapon to be a charging weapon, just set this bit up
	switch (pm->ps->weapon)
	{
		//------------------
	case WP_BRYAR_PISTOL:
	case WP_BLASTER_PISTOL:
	case WP_SBD_BLASTER:

		// alt-fire charges the weapon
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			charging = qtrue;
			alt_fire = qtrue;
		}
		break;

	case WP_REY:

		// alt-fire charges the weapon
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			charging = qtrue;
			alt_fire = qtrue;
		}
		break;

	case WP_CLONEPISTOL:

		// alt-fire charges the weapon
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			charging = qtrue;
			alt_fire = qtrue;
		}
		break;

		//------------------
	case WP_DISRUPTOR:

		// alt-fire charges the weapon...but due to zooming being controlled by the alt-button, the main button actually charges...but only when zoomed.
		//	lovely, eh?
		if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		{
			if (cg.zoomMode == 2)
			{
				if (pm->cmd.buttons & BUTTON_ATTACK)
				{
					charging = qtrue;
					alt_fire = qtrue; // believe it or not, it really is an alt-fire in this case!
				}
			}
		}
		else if (pm->gent && pm->gent->NPC)
		{
			if (pm->gent->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				if (pm->gent->fly_sound_debounce_time > level.time)
				{
					charging = qtrue;
					alt_fire = qtrue;
				}
			}
		}
		break;

		//------------------
	case WP_BOWCASTER:

		// main-fire charges the weapon
		if (pm->cmd.buttons & BUTTON_ATTACK)
		{
			charging = qtrue;
		}
		break;

		//------------------
	case WP_DEMP2:

		// alt-fire charges the weapon
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			charging = qtrue;
			alt_fire = qtrue;
		}
		break;

		//------------------
	case WP_ROCKET_LAUNCHER:

		// Not really a charge weapon, but we still want to delay fire until the button comes up so that we can
		//	implement our alt-fire locking stuff
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			charging = qtrue;
			alt_fire = qtrue;
		}
		break;

		//------------------
	case WP_THERMAL:
		//			FIXME: Really should have a wind-up anim for player
		//			as he holds down the fire button to throw, then play
		//			the actual throw when he lets go...
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			alt_fire = qtrue; // override default of not being an alt-fire
			charging = qtrue;
		}
		else if (pm->cmd.buttons & BUTTON_ATTACK)
		{
			charging = qtrue;
		}
		break;
	default:;
	} // end switch

	// set up the appropriate weapon state based on the button that's down.
	//	Note that we ALWAYS return if charging is set ( meaning the buttons are still down )
	if (charging)
	{
		if (alt_fire)
		{
			if (pm->ps->weaponstate != WEAPON_CHARGING_ALT && pm->ps->weaponstate != WEAPON_DROPPING)
			{
				if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] <= 0)
				{
					PM_AddEvent(EV_NOAMMO);
					pm->ps->weaponTime += 500;
					return true;
				}

				// charge isn't started, so do it now
				pm->ps->weaponstate = WEAPON_CHARGING_ALT;
				pm->ps->weaponChargeTime = level.time;

				if (cg_weapons[pm->ps->weapon].altChargeSound)
				{
					G_SoundOnEnt(pm->gent, CHAN_WEAPON, weaponData[pm->ps->weapon].altChargeSnd);
				}
			}
		}
		else
		{
			if (pm->ps->weaponstate != WEAPON_CHARGING && pm->ps->weaponstate != WEAPON_DROPPING)
			{
				if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] <= 0)
				{
					PM_AddEvent(EV_NOAMMO);
					pm->ps->weaponTime += 500;
					return true;
				}

				// charge isn't started, so do it now
				pm->ps->weaponstate = WEAPON_CHARGING;
				pm->ps->weaponChargeTime = level.time;

				if (cg_weapons[pm->ps->weapon].chargeSound && pm->gent && !pm->gent->NPC)
					// HACK: !NPC mostly for bowcaster and weequay
				{
					G_SoundOnEnt(pm->gent, CHAN_WEAPON, weaponData[pm->ps->weapon].chargeSnd);
				}
			}
		}

		return true; // short-circuit rest of weapon code
	}

	// Only charging weapons should be able to set these states...so....
	//	let's see which fire mode we need to set up now that the buttons are up
	if (pm->ps->weaponstate == WEAPON_CHARGING)
	{
		// weapon has a charge, so let us do an attack
		// dumb, but since we shoot a charged weapon on button-up, we need to repress this button for now
		pm->cmd.buttons |= BUTTON_ATTACK;
		pm->ps->eFlags |= EF_FIRING;
	}
	else if (pm->ps->weaponstate == WEAPON_CHARGING_ALT)
	{
		// weapon has a charge, so let us do an alt-attack
		// dumb, but since we shoot a charged weapon on button-up, we need to repress this button for now
		pm->cmd.buttons |= BUTTON_ALT_ATTACK;
		pm->ps->eFlags |= EF_FIRING | EF_ALT_FIRING;
	}

	return false; // continue with the rest of the weapon code
}

// Specific weapons can opt to modify the ammo usage based on charges, otherwise if no special case code
//	is handled below, regular ammo usage will happen
//---------------------------------------
static int PM_DoChargingAmmoUsage(int* amount)
//---------------------------------------
{
	int count = 0;

	if (pm->ps->weapon == WP_BOWCASTER && !(pm->cmd.buttons & BUTTON_ALT_ATTACK))
	{
		// this code is duplicated ( I know, I know ) in G_weapon.cpp for the bowcaster alt-fire
		count = (level.time - pm->ps->weaponChargeTime) / BOWCASTER_CHARGE_UNIT;

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

		// Only bother with these checks if we don't have infinite ammo
		if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1)
		{
			const int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

			// If we have enough ammo to do the full charged shot, we are ok
			if (dif < 0)
			{
				// we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
				count += floor(dif / static_cast<float>(*amount));

				if (count < 1)
				{
					count = 1;
				}

				// now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
				pm->ps->weaponChargeTime = level.time - count * BOWCASTER_CHARGE_UNIT;
			}
		}

		// now that count is cool, get the real ammo usage
		*amount *= count;
	}
	else if (pm->ps->weapon == WP_BRYAR_PISTOL && pm->cmd.buttons & BUTTON_ALT_ATTACK
		|| pm->ps->weapon == WP_SBD_BLASTER && pm->cmd.buttons & BUTTON_ALT_ATTACK
		|| pm->ps->weapon == WP_BLASTER_PISTOL && pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		// this code is duplicated ( I know, I know ) in G_weapon.cpp for the bryar alt-fire
		count = (level.time - pm->ps->weaponChargeTime) / BRYAR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		// Only bother with these checks if we don't have infinite ammo
		if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1)
		{
			const int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

			// If we have enough ammo to do the full charged shot, we are ok
			if (dif < 0)
			{
				// we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
				count += floor(dif / static_cast<float>(*amount));

				if (count < 1)
				{
					count = 1;
				}

				// now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
				pm->ps->weaponChargeTime = level.time - count * BRYAR_CHARGE_UNIT;
			}
		}

		// now that count is cool, get the real ammo usage
		*amount *= count;
	}
	else if (pm->ps->weapon == WP_DEMP2 && pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		// this code is duplicated ( I know, I know ) in G_weapon.cpp for the demp2 alt-fire
		count = (level.time - pm->ps->weaponChargeTime) / DEMP2_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 3)
		{
			count = 3;
		}

		// Only bother with these checks if we don't have infinite ammo
		if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1)
		{
			const int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

			// If we have enough ammo to do the full charged shot, we are ok
			if (dif < 0)
			{
				// we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
				count += floor(dif / static_cast<float>(*amount));

				if (count < 1)
				{
					count = 1;
				}

				// now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
				pm->ps->weaponChargeTime = level.time - count * DEMP2_CHARGE_UNIT;
			}
		}

		// now that count is cool, get the real ammo usage
		*amount *= count;

		// this is an after-thought.  should probably re-write the function to do this naturally.
		if (*amount > pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex])
		{
			*amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex];
		}
	}
	else if (pm->ps->weapon == WP_DISRUPTOR && pm->cmd.buttons & BUTTON_ALT_ATTACK)
		// BUTTON_ATTACK will have been mapped to BUTTON_ALT_ATTACK if we are zoomed
	{
		// this code is duplicated ( I know, I know ) in G_weapon.cpp for the disruptor alt-fire
		count = (level.time - pm->ps->weaponChargeTime) / DISRUPTOR_CHARGE_UNIT;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 10)
		{
			count = 10;
		}

		// Only bother with these checks if we don't have infinite ammo
		if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1)
		{
			const int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

			// If we have enough ammo to do the full charged shot, we are ok
			if (dif < 0)
			{
				// we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
				count += floor(dif / static_cast<float>(*amount));

				if (count < 1)
				{
					count = 1;
				}

				// now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
				pm->ps->weaponChargeTime = level.time - count * DISRUPTOR_CHARGE_UNIT;
			}
		}

		// now that count is cool, get the real ammo usage
		*amount *= count;

		// this is an after-thought.  should probably re-write the function to do this naturally.
		if (*amount > pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex])
		{
			*amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex];
		}
	}

	return count;
}

qboolean PM_DroidMelee(const int npc_class)
{
	if (npc_class == CLASS_PROBE
		|| npc_class == CLASS_SEEKER
		|| npc_class == CLASS_INTERROGATOR
		|| npc_class == CLASS_SENTRY
		|| npc_class == CLASS_REMOTE)
	{
		return qtrue;
	}
	return qfalse;
}

static void PM_WeaponWampa()
{
	// make weapon function
	if (pm->ps->weaponTime > 0)
	{
		pm->ps->weaponTime -= pml.msec;
		if (pm->ps->weaponTime <= 0)
		{
			pm->ps->weaponTime = 0;
		}
	}

	// check for weapon change
	// can't change if weapon is firing, but can change again if lowering or raising
	if (pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING)
	{
		if (pm->ps->weapon != pm->cmd.weapon)
		{
			PM_BeginWeaponChange(pm->cmd.weapon);
		}
	}

	if (pm->ps->weaponTime > 0)
	{
		return;
	}

	// change weapon if time
	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		PM_FinishWeaponChange();
		return;
	}

	if (pm->ps->weapon == WP_SABER
		&& pm->cmd.buttons & BUTTON_ATTACK
		&& pm->ps->torsoAnim == BOTH_HANG_IDLE)
	{
		pm->ps->SaberActivate();
		if (pm->ps->dualSabers && pm->gent->weaponModel[1] == -1)
		{
			G_RemoveHolsterModels(pm->gent);
			WP_SaberAddG2SaberModels(pm->gent, qtrue);
		}
		pm->ps->SaberActivateTrail(150);
		PM_SetAnim(pm, SETANIM_BOTH, BOTH_HANG_ATTACK, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		pm->ps->weaponstate = WEAPON_FIRING;
		pm->ps->saberBlocked = BLOCKED_NONE;
		pm->ps->saber_move = LS_READY;
		pm->ps->saberMoveNext = LS_NONE;
	}
	else if (pm->ps->torsoAnim == BOTH_HANG_IDLE)
	{
		pm->ps->SaberDeactivateTrail(0);
		pm->ps->weaponstate = WEAPON_READY;
		pm->ps->saber_move = LS_READY;
		pm->ps->saberMoveNext = LS_NONE;
	}
}

static qboolean PM_IsFatiguedGunner()
{
	switch (pm->ps->weapon)
	{
	case WP_BLASTER_PISTOL:
	case WP_BLASTER:
	case WP_DISRUPTOR:
	case WP_BOWCASTER:
	case WP_REPEATER:
	case WP_DEMP2:
	case WP_FLECHETTE:
	case WP_ROCKET_LAUNCHER:
	case WP_THERMAL:
	case WP_TRIP_MINE:
	case WP_DET_PACK:
	case WP_CONCUSSION:
	case WP_STUN_BATON:
	case WP_BRYAR_PISTOL:
	case WP_JAWA:
	case WP_TUSKEN_RIFLE:
	case WP_TUSKEN_STAFF:
	case WP_NOGHRI_STICK:
	case WP_BATTLEDROID:
	case WP_THEFIRSTORDER:
	case WP_CLONECARBINE:
	case WP_REBELBLASTER:
	case WP_CLONERIFLE:
	case WP_CLONECOMMANDO:
	case WP_Z6_ROTARY_CANNON:
	case WP_REBELRIFLE:
	case WP_REY:
	case WP_JANGO:
	case WP_BOBA:
	case WP_CLONEPISTOL:
	case WP_SBD_BLASTER:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
	case WP_DROIDEKA:
		return qtrue;
	default:;
	}
	return qfalse;
}

/*
==============
PM_Weapon

Generates weapon events and modifes the weapon counter
==============
*/
static void PM_Weapon()
{
	int add_time, amount, true_count = 1;
	qboolean delayed_fire = qfalse;

	if (pm->ps->eFlags & EF_HELD_BY_WAMPA)
	{
		PM_WeaponWampa();
		return;
	}
	if (pm->ps->eFlags & EF_FORCE_DRAINED)
	{
		//being drained
		return;
	}
	if (pm->ps->forcePowersActive & 1 << FP_DRAIN
		&& pm->ps->forceDrainEntityNum < ENTITYNUM_WORLD)
	{
		//draining
		return;
	}
	if (pm->ps->weapon == WP_SABER && (cg.zoomMode == 3 || !cg.zoomMode || pm->ps->client_num)) // WP_LIGHTSABER
	{
		// Separate logic for lightsaber, but not for player when zoomed
		PM_WeaponLightsaber();
		if (pm->gent && pm->gent->client && pm->ps->saber[0].Active() && pm->ps->saberInFlight)
		{
			//FIXME: put saberTrail in playerState
			if (pm->gent->client->ps.saberEntityState == SES_RETURNING)
			{
				//turn off the saber trail
				pm->gent->client->ps.SaberDeactivateTrail(75);
			}
			else
			{
				//turn on the saber trail
				pm->gent->client->ps.SaberActivateTrail(150);
			}
		}
		return;
	}

	if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps))
	{
		//in knockdown
		if (pm->ps->weaponTime > 0)
		{
			pm->ps->weaponTime -= pml.msec;
			if (pm->ps->weaponTime <= 0)
			{
				pm->ps->weaponTime = 0;
			}
		}
		return;
	}

	if (pm->gent && pm->gent->client)
	{
		if (pm->gent->client->fireDelay > 0)
		{
			//FIXME: this is going to fire off one frame before you expect, actually
			pm->gent->client->fireDelay -= pml.msec;
			if (pm->gent->client->fireDelay <= 0)
			{
				//just finished delay timer
				if (pm->ps->client_num && pm->ps->weapon == WP_ROCKET_LAUNCHER)
				{
					G_SoundOnEnt(pm->gent, CHAN_WEAPON, "sound/weapons/rocket/lock.wav");
					pm->cmd.buttons |= BUTTON_ALT_ATTACK;
				}
				pm->gent->client->fireDelay = 0;
				delayed_fire = qtrue;
				if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
					&& pm->ps->weapon == WP_THERMAL
					&& pm->gent->alt_fire)
				{
					pm->cmd.buttons |= BUTTON_ALT_ATTACK;
				}
			}
			else
			{
				if (pm->ps->client_num && pm->ps->weapon == WP_ROCKET_LAUNCHER && Q_irand(0, 1))
				{
					G_SoundOnEnt(pm->gent, CHAN_WEAPON, "sound/weapons/rocket/tick.wav");
				}
			}
		}
	}

	// don't allow attack until all buttons are up
	if (pm->ps->pm_flags & PMF_RESPAWNED)
	{
		return;
	}

	// check for dead player
	if (pm->ps->stats[STAT_HEALTH] <= 0)
	{
		if (pm->gent && pm->gent->client)
		{
			pm->ps->weapon = WP_NONE;
		}

		if (pm->gent)
		{
			pm->gent->s.loopSound = 0;
		}
		return;
	}

	// make weapon function
	if (pm->ps->weaponTime > 0)
	{
		pm->ps->weaponTime -= pml.msec;
	}
	else
	{
		if (pm->cmd.buttons & BUTTON_KICK
			&& pm->gent->client->NPC_class != CLASS_DROIDEKA)
		{
			//allow them to do the kick now!
			if (PM_CheckKickAttack()) //trying to do a kick
			{
				//allow them to do the kick now!
				pm->ps->weaponTime = 0;
				PM_MeleeKickForConditions();
				return;
			}
		}
	}

	// check for weapon change
	// can't change if weapon is firing, but can change again if lowering or raising
	if ((pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING) && pm->ps->weaponstate !=
		WEAPON_CHARGING_ALT && pm->ps->weaponstate != WEAPON_CHARGING)
	{
		if (pm->ps->weapon != pm->cmd.weapon && (!pm->ps->viewEntity || pm->ps->viewEntity >= ENTITYNUM_WORLD) && !
			PM_DoChargedWeapons())
		{
			PM_BeginWeaponChange(pm->cmd.weapon);
		}
	}

	if (pm->ps->weaponTime > 0)
	{
		return;
	}

	// change weapon if time
	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		PM_FinishWeaponChange();
		return;
	}

	if (pm->ps->weapon == WP_NONE)
	{
		return;
	}

	if (pm->ps->weaponstate == WEAPON_RAISING)
	{
		//Just selected the weapon
		pm->ps->weaponstate = WEAPON_IDLE;

		if (pm->gent && (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent)))
		{
			if (pm->gent->client->NPC_class == CLASS_YODA)
			{
				if (pm->ps->weapon == WP_MELEE)
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
				}
			}
			else
			{
				if (pm->gent->client->NPC_class == CLASS_SITHLORD)
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND9, SETANIM_FLAG_NORMAL);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
				}
			}
		}
		else
		{
			switch (pm->ps->weapon)
			{
			case WP_BRYAR_PISTOL:
			case WP_BLASTER_PISTOL:
			case WP_SBD_BLASTER:
			case WP_JAWA:
			case WP_REY:
			case WP_JANGO:
			case WP_CLONEPISTOL:
			case WP_DUAL_PISTOL:
			case WP_DUAL_CLONEPISTOL:
				if (pm->gent && pm->gent->weaponModel[1] > 0)
				{
					//dual pistols
					PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2P, SETANIM_FLAG_NORMAL);
				}
				else
				{
					//single pistol
					PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2, SETANIM_FLAG_NORMAL);
				}
				break;
			case WP_DROIDEKA:
				if (pm->gent && pm->gent->weaponModel[1] > 0)
				{
					//dual pistols
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
				}
				else
				{
					//single pistol
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND2, SETANIM_FLAG_NORMAL);
				}
				break;

			default:
				PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
				break;
			}
		}
		return;
	}

	if (pm->gent)
	{
		//ready to throw thermal again, add it
		if (pm->ps->weapon == WP_THERMAL
			&& pm->gent->weaponModel[0] == -1)
		{
			//add the thermal model back in our hand
			// remove anything if we have anything already
			G_RemoveWeaponModels(pm->gent);

			if (com_kotor->integer == 1) //playing kotor
			{
				if (weaponData[pm->ps->weapon].altweaponMdl[0])
				{
					//might be NONE, so check if it has a model
					G_CreateG2AttachedWeaponModel(pm->gent, weaponData[pm->ps->weapon].altweaponMdl, pm->gent->handRBolt, 0);
					//make it sound like we took another one out from... uh.. somewhere...
					if (cg.time > 0)
					{
						//this way we don't get that annoying change weapon sound every time a map starts
						PM_AddEvent(EV_CHANGE_WEAPON);
					}
				}
			}
			else
			{
				if (pm->gent->friendlyfaction == FACTION_KOTOR)
				{
					if (weaponData[pm->ps->weapon].altweaponMdl[0])
					{
						//might be NONE, so check if it has a model
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[pm->ps->weapon].altweaponMdl, pm->gent->handRBolt, 0);
						//make it sound like we took another one out from... uh.. somewhere...
						if (cg.time > 0)
						{
							//this way we don't get that annoying change weapon sound every time a map starts
							PM_AddEvent(EV_CHANGE_WEAPON);
						}
					}
				}
				else
				{
					if (weaponData[pm->ps->weapon].weaponMdl[0])
					{
						//might be NONE, so check if it has a model
						G_CreateG2AttachedWeaponModel(pm->gent, weaponData[pm->ps->weapon].weaponMdl, pm->gent->handRBolt, 0);
						//make it sound like we took another one out from... uh.. somewhere...
						if (cg.time > 0)
						{
							//this way we don't get that annoying change weapon sound every time a map starts
							PM_AddEvent(EV_CHANGE_WEAPON);
						}
					}
				}
			}
		}
	}

	if (!delayed_fire)
	{
		//didn't just finish a fire delay
		if (PM_DoChargedWeapons())
		{
			// In some cases the charged weapon code may want us to short circuit the rest of the firing code
			return;
		}
		if (!pm->gent->client->fireDelay //not already waiting to fire
			&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
			&& pm->ps->weapon == WP_THERMAL //holding thermal
			&& pm->gent //gent
			&& pm->gent->client //client
			&& pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK)) //holding fire
		{
			//delay the actual firing of the missile until the anim has played some
			if (PM_StandingAnim(pm->ps->legsAnim) || pm->ps->legsAnim == BOTH_THERMAL_READY)
			{
				PM_SetAnim(pm, SETANIM_LEGS, BOTH_THERMAL_THROW, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_THERMAL_THROW,
				SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
			pm->gent->client->fireDelay = 300;
			pm->ps->weaponstate = WEAPON_FIRING;
			pm->gent->alt_fire = static_cast<qboolean>(pm->cmd.buttons & BUTTON_ALT_ATTACK);
			return;
		}

		if (!pm->gent->client->fireDelay //not already waiting to fire
			&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //player
			&& PM_WeponFatiguedAnim(pm->gent->client->ps.torsoAnim)
			&& PM_IsFatiguedGunner()
			&& pm->gent //gent
			&& pm->gent->client //client
			&& pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK)) //holding fire
		{
			//delay the actual firing of the missile until the anim has played some
			pm->gent->client->fireDelay = 200;
			pm->ps->weaponstate = WEAPON_FIRING;
			pm->gent->alt_fire = static_cast<qboolean>(pm->cmd.buttons & BUTTON_ALT_ATTACK);
			return;
		}
	}

	if (!delayed_fire)
	{
		if (pm->ps->weapon == WP_MELEE && (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()))
		{
			//melee
			if ((pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK)) != (BUTTON_ATTACK | BUTTON_ALT_ATTACK))
			{
				//not holding both buttons
				if (pm->cmd.buttons & BUTTON_ATTACK && pm->ps->pm_flags & PMF_ATTACK_HELD)
				{
					//held button
					//clear it
					pm->cmd.buttons &= ~BUTTON_ATTACK;
				}
				if (pm->cmd.buttons & BUTTON_ALT_ATTACK && pm->ps->pm_flags & PMF_ALT_ATTACK_HELD)
				{
					//held button
					//clear it
					pm->cmd.buttons &= ~BUTTON_ALT_ATTACK;
				}
			}
		}
		// check for fire
		if (!(pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK)))
		{
			pm->ps->weaponTime = 0;

			if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
			{
				//Still firing
				pm->ps->weaponstate = WEAPON_FIRING;
			}
			else if (pm->ps->weaponstate != WEAPON_READY)
			{
				if (!pm->gent || !pm->gent->NPC || pm->gent->attackDebounceTime < level.time)
				{
					pm->ps->weaponstate = WEAPON_IDLE;
				}
			}

			if (pm->ps->weapon == WP_MELEE
				&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
				&& PM_KickMove(pm->ps->saber_move))
			{
				//melee, not attacking, clear move
				pm->ps->saber_move = LS_NONE;
			}
			return;
		}

		if (pm->gent->s.m_iVehicleNum != 0 || pm->ps->weaponfiredelaytime > level.time ||
			pm->ps->torsoAnim == BOTH_PISTOLFAIL ||
			pm->ps->torsoAnim == BOTH_RIFLEFAIL ||
			pm->ps->torsoAnim == BOTH_ROCKETFAIL ||
			pm->ps->torsoAnim == BOTH_2PISTOLFAIL ||
			pm->ps->torsoAnim == BOTH_RELOAD_FAIL_MINIGUN)
		{
			// No Anims if on Veh
		}
		// start the animation even if out of ammo
		else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ROCKETTROOPER)
		{
			if (pm->gent->client->moveType == MT_FLYSWIM)
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK2,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
			}
			else
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK1,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
			}
		}
		else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
		{
			// Kneel attack
			//--------------
			if (pm->cmd.upmove == -127)
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_KNEELATTACK,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
			}
			else
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK1,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
			}

			// Standing attack
			//-----------------
		}
		else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ASSASSIN_DROID)
		{
			// Crouched Attack
			if (PM_CrouchAnim(pm->gent->client->ps.legsAnim))
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK2,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLDLESS);
			}

			// Standing Attack
			//-----------------
			else
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLDLESS);
			}
		}
		else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_DROIDEKA)
		{
			// Crouched Attack
			if (PM_CrouchAnim(pm->gent->client->ps.legsAnim))
			{
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK1,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLDLESS);
			}

			// Standing Attack
			//-----------------
			else
			{
				if (pm->gent && pm->gent->weaponModel[1] > 0)
				{
					//dual pistols
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
				else
				{
					//single pistol
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
			}
		}
		else
		{
			switch (pm->ps->weapon)
			{
			case WP_BRYAR_PISTOL:
			case WP_SBD_BLASTER:
			case WP_JAWA:
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK2,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				break;

			case WP_BLASTER_PISTOL:
			case WP_REY:
			case WP_JANGO:
			case WP_CLONEPISTOL:
			case WP_DUAL_PISTOL:
			case WP_DUAL_CLONEPISTOL:
			case WP_REBELBLASTER:
				if (pm->gent && pm->gent->weaponModel[1] > 0)
				{
					//dual pistols
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
				else
				{
					//single pistol
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK2,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
				break;

			case WP_MELEE:

				// since there's no RACE_BOTS, I listed all the droids that have might have melee attacks - dmv
				if (pm->gent && pm->gent->client)
				{
					if (PM_DroidMelee(pm->gent->client->NPC_class))
					{
						if (rand() & 1)
							PM_SetAnim(pm, SETANIM_BOTH, BOTH_MELEE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						else
							PM_SetAnim(pm, SETANIM_BOTH, BOTH_MELEE2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					else
					{
						int anim = -1;
						if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
						{
							if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
							{
								if (pm->cmd.buttons & BUTTON_ATTACK)
								{
									PM_TryGrab();
								}
								else if (!(pm->ps->pm_flags & PMF_ALT_ATTACK_HELD))
								{
									PM_CheckKick();
								}
							}
							else if (pm->cmd.buttons & BUTTON_KICK
								&& pm->gent->client->NPC_class != CLASS_DROIDEKA)
							{
								PM_MeleeKickForConditions();
							}
							else if (!(pm->ps->pm_flags & PMF_ATTACK_HELD))
							{
								anim = PM_PickAnim(pm->gent, BOTH_MELEE1, BOTH_MELEE2);
							}
						}
						else
						{
							anim = PM_PickAnim(pm->gent, BOTH_MELEE1, BOTH_MELEE2);
						}
						if (anim != -1)
						{
							if (VectorCompare(pm->ps->velocity, vec3_origin) && pm->cmd.upmove >= 0)
							{
								PM_SetAnim(pm, SETANIM_BOTH, anim,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, anim,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
							}
						}
					}
				}
				break;

			case WP_TUSKEN_RIFLE:
				if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
				{
					//shoot
					//in alt-fire, sniper mode
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				}
				else
				{
					//melee
					const int anim = PM_PickAnim(pm->gent, BOTH_TUSKENATTACK1, BOTH_TUSKENATTACK3); // Rifle
					if (VectorCompare(pm->ps->velocity, vec3_origin) && pm->cmd.upmove >= 0)
					{
						PM_SetAnim(pm, SETANIM_BOTH, anim,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, anim,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				break;

			case WP_TUSKEN_STAFF:

				if (pm->gent && pm->gent->client)
				{
					int anim;
					int flags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART;
					if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
					{
						//player
						if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
						{
							if (pm->cmd.buttons & BUTTON_ATTACK)
							{
								anim = BOTH_TUSKENATTACK3;
							}
							else
							{
								anim = BOTH_TUSKENATTACK2;
							}
						}
						else
						{
							anim = BOTH_TUSKENATTACK1;
						}
					}
					else
					{
						// npc
						if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
						{
							anim = BOTH_TUSKENLUNGE1;
							if (pm->ps->torsoAnimTimer > 0)
							{
								flags &= ~SETANIM_FLAG_RESTART;
							}
						}
						else
						{
							anim = PM_PickAnim(pm->gent, BOTH_TUSKENATTACK1, BOTH_TUSKENATTACK3);
						}
					}
					if (VectorCompare(pm->ps->velocity, vec3_origin) && pm->cmd.upmove >= 0)
					{
						PM_SetAnim(pm, SETANIM_BOTH, anim, flags, 0);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, anim, flags, 0);
					}
				}
				break;

			case WP_NOGHRI_STICK:

				if (pm->gent && pm->gent->client)
				{
					int anim;
					if (pm->cmd.buttons & BUTTON_ATTACK)
					{
						anim = BOTH_ATTACK3;
					}
					else
					{
						anim = PM_PickAnim(pm->gent, BOTH_TUSKENATTACK1, BOTH_TUSKENATTACK3);
					}
					if (anim != BOTH_ATTACK3 && VectorCompare(pm->ps->velocity, vec3_origin) && pm->cmd.upmove >= 0)
					{
						PM_SetAnim(pm, SETANIM_BOTH, anim,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, anim,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				break;

			case WP_FLECHETTE:
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
				break;

			case WP_CLONERIFLE:
			case WP_ROCKET_LAUNCHER:
			case WP_CONCUSSION:
			case WP_BOWCASTER:
			case WP_DEMP2:
			case WP_Z6_ROTARY_CANNON:
				if (cg.renderingThirdPerson)
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
				}
				break;

			/*case WP_Z6_ROTARY_CANNON:
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_MINIGUN,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
				break;*/

			case WP_BLASTER:
			case WP_THEFIRSTORDER:
			case WP_BOBA:
			case WP_REBELRIFLE:
			case WP_CLONECARBINE:
			case WP_BATTLEDROID:
			case WP_CLONECOMMANDO:
				if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
				{
					if (cg.renderingThirdPerson)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				else
				{
					if (cg.renderingThirdPerson)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK4,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				break;

			case WP_REPEATER:
				if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
				{
					//
					if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK1,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
					}
				}
				else
				{
					if (cg.renderingThirdPerson)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				break;

			case WP_DISRUPTOR:
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer() && pm->gent && pm->gent->NPC &&
					pm->gent->NPC->scriptFlags & SCF_ALT_FIRE ||
					(pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.zoomMode == 2)
				{
					//NPC or player in alt-fire, sniper mode
					if (cg.renderingThirdPerson)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
				}
				else
				{
					//in primary fire mode
					if (cg.renderingThirdPerson)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK4,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				break;

			case WP_BOT_LASER:
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK1,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				break;

			case WP_THERMAL:
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					if (PM_StandingAnim(pm->ps->legsAnim))
					{
						PM_SetAnim(pm, SETANIM_LEGS, BOTH_ATTACK10, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK10,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
				else
				{
					if (BG_AllowThirdPersonSpecialMove(pm->ps))
					{
						if (PM_StandingAnim(pm->ps->legsAnim)
							|| pm->ps->legsAnim == BOTH_THERMAL_READY)
						{
							PM_SetAnim(pm, SETANIM_LEGS, BOTH_THERMAL_THROW,
								SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						}
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_THERMAL_THROW,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD); //|SETANIM_FLAG_RESTART
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK2,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
					}
				}
				break;

			case WP_EMPLACED_GUN:
				// Guess we don't play an attack animation?  Maybe we should have a custom one??
				break;

			case WP_NONE:
				// no anim
				break;

			case WP_TRIP_MINE:
			case WP_DET_PACK:
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK11,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				break;

			default: //2-handed heavy weapon

				if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
				}
				else
				{
					if (cg.renderingThirdPerson)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK4,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK3,
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					}
				}
				break;
			}
		}
	}

	if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		amount = weaponData[pm->ps->weapon].altEnergyPerShot;
	}
	else
	{
		amount = weaponData[pm->ps->weapon].energyPerShot;
	}

	if (pm->ps->weaponstate == WEAPON_CHARGING || pm->ps->weaponstate == WEAPON_CHARGING_ALT)
	{
		// charging weapons may want to do their own ammo logic.
		true_count = PM_DoChargingAmmoUsage(&amount);
	}

	pm->ps->weaponstate = WEAPON_FIRING;

	// take an ammo away if not infinite
	if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1)
	{
		// enough energy to fire this weapon?
		if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - amount >= 0)
		{
			pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] -= amount;
		}
		else // Not enough energy
		{
			if (!(pm->ps->eFlags & EF_LOCKED_TO_WEAPON))
			{
				// Switch weapons
				PM_AddEvent(EV_NOAMMO);
				pm->ps->weaponTime += 500;
			}
			return;
		}
	}

	if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
	{
		//FIXME: this is going to fire off one frame before you expect, actually
		// Clear these out since we're not actually firing yet
		pm->ps->eFlags &= ~EF_FIRING;
		pm->ps->eFlags &= ~EF_ALT_FIRING;
		return;
	}

	if (pm->ps->weapon == WP_EMPLACED_GUN)
	{
		if (pm->gent
			&& pm->gent->owner
			&& pm->gent->owner->e_UseFunc == useF_eweb_use)
		{
			//eweb always shoots alt-fire, for proper effects and sounds
			PM_AddEvent(EV_ALT_FIRE);
			add_time = weaponData[pm->ps->weapon].altFireTime;
		}
		else
		{
			//emplaced gun always shoots normal fire
			PM_AddEvent(EV_FIRE_WEAPON);
			add_time = weaponData[pm->ps->weapon].fireTime;
		}
	}
	else if (pm->ps->weapon == WP_MELEE && pm->ps->client_num >= MAX_CLIENTS
		|| pm->ps->weapon == WP_TUSKEN_STAFF
		|| pm->ps->weapon == WP_TUSKEN_RIFLE && !(pm->cmd.buttons & BUTTON_ALT_ATTACK))
	{
		PM_AddEvent(EV_FIRE_WEAPON);
		add_time = pm->ps->torsoAnimTimer;
	}
	else if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		PM_AddEvent(EV_ALT_FIRE);
		add_time = weaponData[pm->ps->weapon].altFireTime;
		if (pm->ps->weapon == WP_THERMAL)
		{
			//threw our thermal
			if (pm->gent)
			{
				// remove the thermal model if we had it.
				G_RemoveWeaponModels(pm->gent);
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					//NPCs need to know when to put the thermal back in their hand
					pm->ps->weaponTime = pm->ps->torsoAnimTimer - 500;
				}
			}
		}
	}
	else
	{
		if (pm->ps->client_num //NPC
			&& !PM_ControlledByPlayer() //not under player control
			&& pm->ps->weapon == WP_THERMAL //using thermals
			&& pm->ps->torsoAnim != BOTH_ATTACK10) //not in the throw anim
		{
			//oops, got knocked out of the anim, don't throw the thermal
			return;
		}
		PM_AddEvent(EV_FIRE_WEAPON);
		add_time = weaponData[pm->ps->weapon].fireTime;

		switch (pm->ps->weapon)
		{
		case WP_REPEATER:
			// repeater is supposed to do smoke after sustained bursts
			pm->ps->weaponShotCount++;
			break;
		case WP_BOWCASTER:
			add_time *= true_count < 3 ? 0.35f : 1.0f;
			// if you only did a small charge shot with the bowcaster, use less time between shots
			break;
		case WP_THERMAL:
			if (pm->gent)
			{
				// remove the thermal model if we had it.
				G_RemoveWeaponModels(pm->gent);
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					//NPCs need to know when to put the thermal back in their hand
					pm->ps->weaponTime = pm->ps->torsoAnimTimer - 500;
				}
			}
			break;
		default:;
		}
	}

	if (!PM_ControlledByPlayer())
	{
		if (pm->gent && pm->gent->NPC != nullptr)
		{
			//NPCs have their own refire logic
			return;
		}
	}

	if (g_timescale != nullptr)
	{
		if (g_timescale->value < 1.0f)
		{
			if (!MatrixMode)
			{
				//Special test for Matrix Mode (tm)
				if (pm->ps->client_num == 0 && !player_locked && !PlayerAffectedByStasis() && (pm->ps->
					forcePowersActive & 1 << FP_SPEED || pm->ps->forcePowersActive & 1 << FP_RAGE))
				{
					//player always fires at normal speed
					add_time *= g_timescale->value;
				}
				else if (g_entities[pm->ps->client_num].client
					&& (pm->ps->forcePowersActive & 1 << FP_SPEED || pm->ps->forcePowersActive & 1 << FP_RAGE))
				{
					add_time *= g_timescale->value;
				}
			}
		}
	}

	pm->ps->weaponTime += add_time;
	pm->ps->lastShotTime = level.time; //so we know when the last time we fired our gun is

	// HACK!!!!!
	if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] <= 0)
	{
		if (pm->ps->weapon == WP_THERMAL || pm->ps->weapon == WP_TRIP_MINE)
		{
			// because these weapons have the ammo attached to the hand, we should switch weapons when the last one is thrown, otherwise it will look silly
			//	NOTE: could also switch to an empty had version, but was told we aren't getting any new models at this point
			CG_OutOfAmmoChange();
			PM_SetAnim(pm, SETANIM_TORSO, TORSO_DROPWEAP1 + 2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			// hack weapon down!
			pm->ps->weaponTime = 50;
		}
	}
}

/*
==============
PM_VehicleWeapon

Generates weapon events and modifes the weapon counter
==============
*/
static void PM_VehicleWeapon()
{
	int add_time = 0;
	qboolean delayed_fire = qfalse;

	if (pm->ps->weapon == WP_NONE)
	{
		return;
	}

	if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
	{
		//FIXME: this is going to fire off one frame before you expect, actually
		pm->gent->client->fireDelay -= pml.msec;
		if (pm->gent->client->fireDelay <= 0)
		{
			//just finished delay timer
			if (pm->ps->client_num && pm->ps->weapon == WP_ROCKET_LAUNCHER)
			{
				G_SoundOnEnt(pm->gent, CHAN_WEAPON, "sound/weapons/rocket/lock.wav");
				pm->cmd.buttons |= BUTTON_ALT_ATTACK;
			}
			pm->gent->client->fireDelay = 0;
			delayed_fire = qtrue;
		}
		else
		{
			if (pm->ps->client_num && pm->ps->weapon == WP_ROCKET_LAUNCHER && Q_irand(0, 1))
			{
				G_SoundOnEnt(pm->gent, CHAN_WEAPON, "sound/weapons/rocket/tick.wav");
			}
		}
	}

	// don't allow attack until all buttons are up
	if (pm->ps->pm_flags & PMF_RESPAWNED)
	{
		return;
	}

	// check for dead player
	if (pm->ps->stats[STAT_HEALTH] <= 0)
	{
		if (pm->gent)
		{
			pm->gent->s.loopSound = 0;
		}
		return;
	}

	// make weapon function
	if (pm->ps->weaponTime > 0)
	{
		pm->ps->weaponTime -= pml.msec;
		if (pm->ps->weaponTime <= 0)
		{
			pm->ps->weaponTime = 0;
		}
	}

	if (pm->ps->weaponTime > 0)
	{
		return;
	}

	// change weapon if time
	if (pm->ps->weaponstate == WEAPON_DROPPING)
	{
		PM_FinishWeaponChange();
		return;
	}

	if (PM_DoChargedWeapons())
	{
		// In some cases the charged weapon code may want us to short circuit the rest of the firing code
		return;
	}

	if (!delayed_fire)
	{
		// check for fire
		if (!(pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK)))
		{
			pm->ps->weaponTime = 0;

			if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
			{
				//Still firing
				pm->ps->weaponstate = WEAPON_FIRING;
			}
			else if (pm->ps->weaponstate != WEAPON_READY)
			{
				if (!pm->gent || !pm->gent->NPC || pm->gent->attackDebounceTime < level.time)
				{
					pm->ps->weaponstate = WEAPON_IDLE;
				}
			}

			return;
		}
	}

	pm->ps->weaponstate = WEAPON_FIRING;

	if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0)
	{
		//FIXME: this is going to fire off one frame before you expect, actually
		// Clear these out since we're not actually firing yet
		pm->ps->eFlags &= ~EF_FIRING;
		pm->ps->eFlags &= ~EF_ALT_FIRING;
		return;
	}

	if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		PM_AddEvent(EV_ALT_FIRE);
	}
	else
	{
		PM_AddEvent(EV_FIRE_WEAPON);
	}

	if (g_timescale != nullptr)
	{
		if (g_timescale->value < 1.0f)
		{
			if (!MatrixMode)
			{
				//Special test for Matrix Mode (tm)
				if (pm->ps->client_num == 0 && !player_locked && !PlayerAffectedByStasis() && (pm->ps->
					forcePowersActive & 1 << FP_SPEED || pm->ps->forcePowersActive & 1 << FP_RAGE))
				{
					//player always fires at normal speed
					add_time *= g_timescale->value;
				}
				else if (g_entities[pm->ps->client_num].client
					&& (pm->ps->forcePowersActive & 1 << FP_SPEED || pm->ps->forcePowersActive & 1 << FP_RAGE))
				{
					add_time *= g_timescale->value;
				}
			}
		}
	}

	pm->ps->weaponTime += add_time;
	pm->ps->lastShotTime = level.time; //so we know when the last time we fired our gun is
}

extern void ForceHeal(gentity_t* self);
extern void ForceTelepathy(gentity_t* self);
extern void ForceRage(gentity_t* self);
extern void ForceProtect(gentity_t* self);
extern void ForceAbsorb(gentity_t* self);
extern void ForceSeeing(gentity_t* self);
extern void ForceDestruction(gentity_t* self);
extern void ForceStasis(gentity_t* self);
extern void ForceJediRepulse(gentity_t* self);
extern void ForceFear(gentity_t* self);
extern void ForceLightningStrike(gentity_t* self);
extern void ForceDeadlySight(gentity_t* self);
extern void ForceProjection(gentity_t* self);
extern void ForceBlast(gentity_t* self);

void PM_CheckForceUseButton(gentity_t* ent, usercmd_t* ucmd)
{
	if (!ent)
	{
		return;
	}

	if (ucmd->buttons & BUTTON_USE_FORCE && !ent->client->ps.forcePowersKnown)
	{
		gi.SendConsoleCommand("invuse");
		return;
	}

	if (ucmd->buttons & BUTTON_USE_FORCE)
	{
		if (!(ent->client->ps.pm_flags & PMF_USEFORCE_HELD))
		{
			//impulse one shot
			switch (showPowers[cg.forcepowerSelect])
			{
			case FP_HEAL:
				ForceHeal(ent);
				break;
			case FP_SPEED:
				ForceSpeed(ent);
				break;
			case FP_PUSH:
				if (g_SerenityJediEngineMode->integer)
				{
					if (ent->client->NPC_class == CLASS_GALEN
						&& (ent->s.weapon == WP_MELEE || ent->s.weapon == WP_NONE || ent->s.weapon == WP_SABER &&
							!ent->client->ps.SaberActive())
						&& ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
					{
						ForceRepulse(ent, qfalse);
					}
					else
					{
						ForceThrow_MD(ent, qfalse);
					}
				}
				else
				{
					ForceThrow_JKA(ent, qfalse);
				}
				break;
			case FP_PULL:
				if (g_SerenityJediEngineMode->integer)
				{
					if (ent->client->NPC_class == CLASS_GALEN
						&& (ent->s.weapon == WP_MELEE || ent->s.weapon == WP_NONE || ent->s.weapon == WP_SABER &&
							!ent->client->ps.SaberActive())
						&& ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
					{
						ForceRepulse(ent, qtrue);
					}
					else
					{
						ForceThrow_MD(ent, qtrue);
					}
				}
				else
				{
					ForceThrow_JKA(ent, qtrue);
				}
				break;
			case FP_TELEPATHY:
				ForceTelepathy(ent);
				break;
				// Added 01/20/03 by AReis.
				// New Jedi Academy powers.
			case FP_RAGE:
				//duration - speed, invincibility and extra damage for short period, drains your health and leaves you weak and slow afterwards.
				ForceRage(ent);
				break;
			case FP_PROTECT:
				//duration - protect against physical/energy (level 1 stops blaster/energy bolts, level 2 stops projectiles, level 3 protects against explosions)
				ForceProtect(ent);
				break;
			case FP_ABSORB:
				//duration - protect against dark force powers (grip, lightning, drain - maybe push/pull, too?)
				ForceAbsorb(ent);
				break;
			case FP_SEE: //duration - detect/see hidden enemies
				ForceSeeing(ent);
				break;
			case FP_DESTRUCTION:
				ForceDestruction(ent);
				break;
			case FP_STASIS:
				ForceStasis(ent);
				break;
			case FP_FEAR:
				ForceFear(ent);
				break;
			case FP_LIGHTNING_STRIKE:
				ForceLightningStrike(ent);
				break;
			case FP_DEADLYSIGHT:
				ForceDeadlySight(ent);
				break;
			case FP_PROJECTION:
				ForceProjection(ent);
				break;
			case FP_BLAST:
				ForceBlast(ent);
				break;
			default:;
			}
		}
		//these stay are okay to call every frame button is down
		switch (showPowers[cg.forcepowerSelect])
		{
		case FP_LEVITATION:
			ucmd->upmove = 127;
			break;
		case FP_GRIP:
			ucmd->buttons |= BUTTON_FORCEGRIP;
			break;
		case FP_LIGHTNING:
			ucmd->buttons |= BUTTON_FORCE_LIGHTNING;
			break;
		case FP_DRAIN:
			ucmd->buttons |= BUTTON_FORCE_DRAIN;
			break;
		case FP_GRASP:
			ucmd->buttons |= BUTTON_FORCEGRASP;
			break;
		case FP_REPULSE:
			ucmd->buttons |= BUTTON_REPULSE;
			break;
		case FP_LIGHTNING_STRIKE:
			ucmd->buttons |= BUTTON_LIGHTNING_STRIKE;
			break;
		case FP_PROJECTION:
			ucmd->buttons |= BUTTON_PROJECTION;
			break;
		default:;
		}
		ent->client->ps.pm_flags |= PMF_USEFORCE_HELD;
	}
	else
	{
		ent->client->ps.pm_flags &= ~PMF_USEFORCE_HELD;
	}
}

/*
================
PM_ForcePower
================
sends event to client for client side fx, not used
*/

/*
static void PM_ForcePower(void)
{
	// check for item using
	if ( pm->cmd.buttons & BUTTON_USE_FORCE )
	{
		if ( ! ( pm->ps->pm_flags & PMF_USE_FORCE ) )
		{
			pm->ps->pm_flags |= PMF_USE_FORCE;
			PM_AddEvent( EV_USE_FORCE);
			return;
		}
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_USE_FORCE;
	}
}
*/

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers()
{
	// drop misc timing counter
	if (pm->ps->pm_time)
	{
		if (pml.msec >= pm->ps->pm_time)
		{
			pm->ps->pm_flags &= ~PMF_ALL_TIMES;
			pm->ps->pm_time = 0;
		}
		else
		{
			pm->ps->pm_time -= pml.msec;
		}
	}

	// drop legs animation counter
	if (pm->ps->legsAnimTimer > 0)
	{
		int new_time = pm->ps->legsAnimTimer - pml.msec;

		if (new_time < 0)
		{
			new_time = 0;
		}

		PM_SetLegsAnimTimer(pm->gent, &pm->ps->legsAnimTimer, new_time);
	}

	// drop torso animation counter
	if (pm->ps->torsoAnimTimer > 0)
	{
		int new_time = pm->ps->torsoAnimTimer - pml.msec;

		if (new_time < 0)
		{
			new_time = 0;
		}

		PM_SetTorsoAnimTimer(pm->gent, &pm->ps->torsoAnimTimer, new_time);
	}
}

static void PM_SetSpecialMoveValues()
{
	Flying = 0;
	if (pm->gent)
	{
		if (pm->gent->client && pm->gent->client->moveType == MT_FLYSWIM)
		{
			Flying = FLY_NORMAL;
		}
		else if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
		{
			if (pm->gent->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
			{
				Flying = FLY_VEHICLE;
			}
			else if (pm->gent->m_pVehicle->m_pVehicleInfo->hoverHeight > 0)
			{
				//FIXME: or just check for hoverHeight?
				Flying = FLY_HOVER;
			}
		}
	}

	if (g_timescale != nullptr)
	{
		if (g_timescale->value < 1.0f)
		{
			if (!MatrixMode)
			{
				if (pm->ps->client_num == 0 && !player_locked && !PlayerAffectedByStasis() && (pm->ps->
					forcePowersActive & 1 << FP_SPEED || pm->ps->forcePowersActive & 1 << FP_RAGE))
				{
					pml.frametime *= 1.0f / g_timescale->value;
				}
				else if (g_entities[pm->ps->client_num].client
					&& (pm->ps->forcePowersActive & 1 << FP_SPEED || pm->ps->forcePowersActive & 1 << FP_RAGE))
				{
					pml.frametime *= 1.0f / g_timescale->value;
				}
			}
		}
	}
}

extern float cg_zoomFov; //from cg_view.cpp

//-------------------------------------------
static void PM_AdjustAttackStates(pmove_t* pm)
//-------------------------------------------
{
	int amount;

	if (!g_saberAutoBlocking->integer
		&& !g_saberNewControlScheme->integer
		&& (pm->cmd.buttons & BUTTON_FORCE_FOCUS || pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK
			/* || pm->ps->ManualBlockingFlags & 1 << MBF_NPCBLOCKING*/))
	{
		pm->ps->saberBlockingTime = pm->cmd.serverTime + 100;
		pm->cmd.buttons &= ~BUTTON_ATTACK;
		pm->cmd.buttons &= ~BUTTON_ALT_ATTACK;
	}
	// get ammo usage
	if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - weaponData[pm->ps->weapon].altEnergyPerShot;
	}
	else
	{
		amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - weaponData[pm->ps->weapon].energyPerShot;
	}

	if (pm->ps->weapon == WP_SABER && (!cg.zoomMode || pm->ps->client_num))
	{
		//don't let the alt-attack be interpreted as an actual attack command
		if (pm->ps->saberInFlight)
		{
			pm->cmd.buttons &= ~BUTTON_ALT_ATTACK;
			//FIXME: what about alt-attack modifier button?
			if (!pm->ps->dualSabers || !pm->ps->saber[1].Active())
			{
				//saber not in hand, can't swing it
				pm->cmd.buttons &= ~BUTTON_ATTACK;
			}
		}
		//saber staff alt-attack does a special attack anim, non-throwable sabers do kicks
		if (!(pm->ps->saber[0].saberFlags & SFL_NOT_THROWABLE))
		{
			//using a throwable saber, so remove the saber throw button
			if (!g_saberNewControlScheme->integer && PM_CanDoKata())
			{
				//old control scheme - alt-attack + attack does kata
			}
			else
			{
				//new control scheme - alt-attack doesn't have anything to do with katas, safe to clear it here
				pm->cmd.buttons &= ~BUTTON_ALT_ATTACK;
			}
		}
	}

	// disruptor alt-fire should toggle the zoom mode, but only bother doing this for the player?
	if (pm->ps->weapon == WP_DISRUPTOR && pm->gent && (pm->gent->s.number < MAX_CLIENTS ||
		G_ControlledByPlayer(pm->gent)) && pm->ps->weaponstate != WEAPON_DROPPING)
	{
		// we are not alt-firing yet, but the alt-attack button was just pressed and
		//	we either are ducking ( in which case we don't care if they are moving )...or they are not ducking...and also not moving right/forward.
		if (!(pm->ps->eFlags & EF_ALT_FIRING) && pm->cmd.buttons & BUTTON_ALT_ATTACK
			&& (pm->cmd.upmove < 0 || !pm->cmd.forwardmove && !pm->cmd.rightmove))
		{
			// We just pressed the alt-fire key
			if (cg.zoomMode == 0 || cg.zoomMode == 3)
			{
				G_SoundOnEnt(pm->gent, CHAN_AUTO, "sound/weapons/disruptor/zoomstart.wav");
				// not already zooming, so do it now
				cg.zoomMode = 2;
				cg.zoomLocked = qfalse;
				cg_zoomFov = 80.0f; //(cg.overrides.active&CG_OVERRIDE_FOV) ? cg.overrides.fov : cg_fov.value;
			}
			else if (cg.zoomMode == 2)
			{
				G_SoundOnEnt(pm->gent, CHAN_AUTO, "sound/weapons/disruptor/zoomend.wav");
				// already zooming, so must be wanting to turn it off
				cg.zoomMode = 0;
				cg.zoomTime = cg.time;
				cg.zoomLocked = qfalse;
			}
		}
		else if (!(pm->cmd.buttons & BUTTON_ALT_ATTACK))
		{
			// Not pressing zoom any more
			if (cg.zoomMode == 2)
			{
				// were zooming in, so now lock the zoom
				cg.zoomLocked = qtrue;
			}
		}

		if (pm->cmd.buttons & BUTTON_ATTACK)
		{
			// If we are zoomed, we should switch the ammo usage to the alt-fire, otherwise, we'll
			//	just use whatever ammo was selected from above
			if (cg.zoomMode == 2)
			{
				amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] -
					weaponData[pm->ps->weapon].altEnergyPerShot;
			}
		}
		else
		{
			// alt-fire button pressing doesn't use any ammo
			amount = 0;
		}
	}

	// Check for binocular specific mode
	if (cg.zoomMode == 1 && pm->gent && (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent))) //
	{
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK && pm->ps->batteryCharge)
		{
			// zooming out
			cg.zoomLocked = qfalse;
			cg.zoomDir = 1;
		}
		else if (pm->cmd.buttons & BUTTON_ATTACK && pm->ps->batteryCharge)
		{
			// zooming in
			cg.zoomLocked = qfalse;
			cg.zoomDir = -1;
		}
		else
		{
			// if no buttons are down, we should be in a locked state
			cg.zoomLocked = qtrue;
		}

		// kill buttons and associated firing flags so we can't fire
		pm->ps->eFlags &= ~EF_FIRING;
		pm->ps->eFlags &= ~EF_ALT_FIRING;
		pm->cmd.buttons &= ~(BUTTON_ALT_ATTACK | BUTTON_ATTACK);
	}

	// set the firing flag for continuous beam weapons, phaser will fire even if out of ammo
	if ((pm->cmd.buttons & BUTTON_ATTACK || pm->cmd.buttons & BUTTON_ALT_ATTACK) && (amount >= 0 || pm->ps->weapon
		== WP_SABER))
	{
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			pm->ps->eFlags |= EF_ALT_FIRING;
			if (pm->ps->client_num < MAX_CLIENTS && pm->gent && pm->ps->eFlags & EF_IN_ATST)
			{
				//switch ATST barrels
				pm->gent->alt_fire = qtrue;
			}
		}
		else
		{
			pm->ps->eFlags &= ~EF_ALT_FIRING;
			if (pm->ps->client_num < MAX_CLIENTS && pm->gent && pm->ps->eFlags & EF_IN_ATST)
			{
				//switch ATST barrels
				pm->gent->alt_fire = qfalse;
			}
		}

		// This flag should always get set, even when alt-firing
		pm->ps->eFlags |= EF_FIRING;
	}
	else
	{
		// Clear 'em out
		pm->ps->eFlags &= ~EF_FIRING;
		pm->ps->eFlags &= ~EF_ALT_FIRING;
	}

	// disruptor should convert a main fire to an alt-fire if the gun is currently zoomed
	if (pm->ps->weapon == WP_DISRUPTOR && pm->gent && (pm->gent->s.number < MAX_CLIENTS ||
		G_ControlledByPlayer(pm->gent)))
	{
		if (pm->cmd.buttons & BUTTON_ATTACK && cg.zoomMode == 2)
		{
			// converting the main fire to an alt-fire
			pm->cmd.buttons |= BUTTON_ALT_ATTACK;
			pm->ps->eFlags |= EF_ALT_FIRING;
		}
		else
		{
			// don't let an alt-fire through
			pm->cmd.buttons &= ~BUTTON_ALT_ATTACK;
		}
	}
}

static qboolean PM_WeaponOkOnVehicle(const int weapon)
{
	switch (weapon)
	{
	case WP_NONE:
	case WP_SABER:
	case WP_BLASTER:
		//
	case WP_BLASTER_PISTOL:
	case WP_BRYAR_PISTOL:
	case WP_BOWCASTER:
	case WP_REPEATER:
	case WP_DEMP2:
	case WP_FLECHETTE:
		//
	case WP_BATTLEDROID:
	case WP_THEFIRSTORDER:
	case WP_CLONECARBINE:
	case WP_REBELBLASTER:
	case WP_CLONERIFLE:
	case WP_CLONECOMMANDO:
	case WP_Z6_ROTARY_CANNON:
	case WP_REBELRIFLE:
	case WP_REY:
	case WP_JANGO:
	case WP_BOBA:
	case WP_CLONEPISTOL:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
		return qtrue;
	default:;
	}
	return qfalse;
}

static void PM_CheckInVehicleSaberAttackAnim()
{
	//A bit of a hack, but makes the vehicle saber attacks act like any other saber attack...
	// make weapon function
	if (pm->ps->weaponTime > 0)
	{
		pm->ps->weaponTime -= pml.msec;
		if (pm->ps->weaponTime <= 0)
		{
			pm->ps->weaponTime = 0;
		}
	}
	PM_CheckClearSaberBlock();

	saber_moveName_t saber_move = LS_INVALID;
	switch (pm->ps->torsoAnim)
	{
	case BOTH_VS_ATR_S:
		saber_move = LS_SWOOP_ATTACK_RIGHT;
		break;
	case BOTH_VS_ATL_S:
		saber_move = LS_SWOOP_ATTACK_LEFT;
		break;
	case BOTH_VT_ATR_S:
		saber_move = LS_TAUNTAUN_ATTACK_RIGHT;
		break;
	case BOTH_VT_ATL_S:
		saber_move = LS_TAUNTAUN_ATTACK_LEFT;
		break;
	default:;
	}
	if (saber_move != LS_INVALID)
	{
		if (pm->ps->saber_move == saber_move)
		{
			//already playing it
			if (!pm->ps->torsoAnimTimer)
			{
				//anim was done, set it back to ready
				PM_SetSaberMove(LS_READY);
				pm->ps->saber_move = LS_READY;
				pm->ps->weaponstate = WEAPON_IDLE;
				if (pm->cmd.buttons & BUTTON_ATTACK)
				{
					if (!pm->ps->weaponTime)
					{
						PM_SetSaberMove(saber_move);
						pm->ps->weaponstate = WEAPON_FIRING;
						pm->ps->weaponTime = pm->ps->torsoAnimTimer;
					}
				}
			}
		}
		else if (pm->ps->torsoAnimTimer && !pm->ps->weaponTime)
		{
			PM_SetSaberMove(LS_READY);
			pm->ps->saber_move = LS_READY;
			pm->ps->weaponstate = WEAPON_IDLE;
			PM_SetSaberMove(saber_move);
			pm->ps->weaponstate = WEAPON_FIRING;
			pm->ps->weaponTime = pm->ps->torsoAnimTimer;
		}
	}
	pm->ps->saberBlocking = saber_moveData[pm->ps->saber_move].blocking;
}

//force the vehicle to turn and travel to its forced destination point
static void PM_VehForcedTurning(const gentity_t* veh)
{
	const gentity_t* dst = &g_entities[pm->ps->vehTurnaroundIndex];
	vec3_t dir;

	if (!veh || !veh->m_pVehicle)
	{
		return;
	}

	if (!dst)
	{
		//can't find dest ent?
		return;
	}

	pm->cmd.upmove = veh->m_pVehicle->m_ucmd.upmove = 127;
	pm->cmd.forwardmove = veh->m_pVehicle->m_ucmd.forwardmove = 0;
	pm->cmd.rightmove = veh->m_pVehicle->m_ucmd.rightmove = 0;

	VectorSubtract(dst->s.origin, veh->currentOrigin, dir);
	vectoangles(dir, dir);

	float yaw_d = AngleSubtract(pm->ps->viewangles[YAW], dir[YAW]);
	float pitch_d = AngleSubtract(pm->ps->viewangles[PITCH], dir[PITCH]);

	yaw_d *= 0.2f * pml.frametime;
	pitch_d *= 0.6f * pml.frametime;

	pm->ps->viewangles[YAW] = AngleSubtract(pm->ps->viewangles[YAW], yaw_d);
	pm->ps->viewangles[PITCH] = AngleSubtract(pm->ps->viewangles[PITCH], pitch_d);

	SetClientViewAngle(pm->gent, pm->ps->viewangles);
}

static void PM_VehFaceHyperspacePoint(const gentity_t* veh)
{
	if (!veh || !veh->m_pVehicle)
	{
		return;
	}
	const float time_frac = static_cast<float>(pm->cmd.serverTime - veh->client->ps.hyperSpaceTime) /
		HYPERSPACE_TIME;
	int matched_axes = 0;

	pm->cmd.upmove = veh->m_pVehicle->m_ucmd.upmove = 127;
	pm->cmd.forwardmove = veh->m_pVehicle->m_ucmd.forwardmove = 0;
	pm->cmd.rightmove = veh->m_pVehicle->m_ucmd.rightmove = 0;

	const float turn_rate = 90.0f * pml.frametime;
	for (int i = 0; i < 3; i++)
	{
		float a_delta = AngleSubtract(veh->client->ps.hyperSpaceAngles[i], veh->m_pVehicle->m_vOrientation[i]);
		if (fabs(a_delta) < turn_rate)
		{
			//all is good
			pm->ps->viewangles[i] = veh->client->ps.hyperSpaceAngles[i];
			matched_axes++;
		}
		else
		{
			a_delta = AngleSubtract(veh->client->ps.hyperSpaceAngles[i], pm->ps->viewangles[i]);
			if (fabs(a_delta) < turn_rate)
			{
				pm->ps->viewangles[i] = veh->client->ps.hyperSpaceAngles[i];
			}
			else if (a_delta > 0)
			{
				if (i == YAW)
				{
					pm->ps->viewangles[i] = AngleNormalize360(pm->ps->viewangles[i] + turn_rate);
				}
				else
				{
					pm->ps->viewangles[i] = AngleNormalize180(pm->ps->viewangles[i] + turn_rate);
				}
			}
			else
			{
				if (i == YAW)
				{
					pm->ps->viewangles[i] = AngleNormalize360(pm->ps->viewangles[i] - turn_rate);
				}
				else
				{
					pm->ps->viewangles[i] = AngleNormalize180(pm->ps->viewangles[i] - turn_rate);
				}
			}
		}
	}

	SetClientViewAngle(pm->gent, pm->ps->viewangles);

	if (time_frac < HYPERSPACE_TELEPORT_FRAC)
	{
		//haven't gone through yet
		if (matched_axes < 3)
		{
			//not facing the right dir yet
			//keep hyperspace time up to date
			veh->client->ps.hyperSpaceTime += pml.msec;
		}
		else if (!(veh->client->ps.eFlags2 & EF2_HYPERSPACE))
		{
			//flag us as ready to hyperspace!
			veh->client->ps.eFlags2 |= EF2_HYPERSPACE;
		}
	}
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void PM_AdjustAngleForWallGrab(playerState_t* ps, usercmd_t* ucmd);

static void PM_SetPMViewAngle(playerState_t* ps, vec3_t angle, const usercmd_t* ucmd)
{
	for (int i = 0; i < 3; i++)
	{
		const int cmd_angle = ANGLE2SHORT(angle[i]);
		ps->delta_angles[i] = cmd_angle - ucmd->angles[i];
	}
	VectorCopy(angle, ps->viewangles);
}

static void PmoveSingle()
{
	PM_AdjustAngleForWallGrab(pm->ps, &pm->cmd);
}

extern qboolean BG_FullBodyTauntAnim(int anim);

void Pmove(pmove_t* pmove)
{
	Vehicle_t* p_veh = nullptr;

	pm = pmove;

	// this counter lets us debug movement problems with a journal by setting a conditional breakpoint fot the previous frame
	c_pmove++;

	// clear results
	pm->numtouch = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	// Clear the blocked flag
	//pm->ps->pm_flags &= ~PMF_BLOCKED;
	pm->ps->pm_flags &= ~PMF_BUMPED;

	// In certain situations, we may want to control which attack buttons are pressed and what kind of functionality
	//	is attached to them
	PM_AdjustAttackStates(pm);

	// clear the respawned flag if attack and use are cleared
	if (pm->ps->stats[STAT_HEALTH] > 0 &&
		!(pm->cmd.buttons & BUTTON_ATTACK))
	{
		pm->ps->pm_flags &= ~PMF_RESPAWNED;
	}

	// clear all pmove local vars
	memset(&pml, 0, sizeof pml);

	// determine the time
	pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
	if (pml.msec < 1)
	{
		pml.msec = 1;
	}
	else if (pml.msec > 200)
	{
		pml.msec = 200;
	}

	pm->ps->commandTime = pmove->cmd.serverTime;

	PmoveSingle();

	// save old org in case we get stuck
	VectorCopy(pm->ps->origin, pml.previous_origin);

	// save old velocity for crashlanding
	VectorCopy(pm->ps->velocity, pml.previous_velocity);

	pml.frametime = pml.msec * 0.001;

	if (pm->ps->client_num >= MAX_CLIENTS &&
		pm->gent &&
		pm->gent->client &&
		pm->gent->client->NPC_class == CLASS_VEHICLE)
	{
		//we are a vehicle
		p_veh = pm->gent->m_pVehicle;
		assert(p_veh);
		if (p_veh)
		{
			p_veh->m_fTimeModifier = pml.frametime * 60.0f; //at 16.67ms (60fps), should be 1.0f
		}
	}
	else if (pm->gent && PM_RidingVehicle())
	{
		if (pm->ps->vehTurnaroundIndex
			&& pm->ps->vehTurnaroundTime > pm->cmd.serverTime)
		{
			//riding this vehicle, turn my view too
			PM_VehForcedTurning(&g_entities[pm->gent->s.m_iVehicleNum]);
		}
	}
	//else if ( pm->gent && PM_RidingVehicle() )
	//{
	//	if ((&g_entities[pm->gent->s.m_iVehicleNum])->client &&
	//		(pm->cmd.serverTime - (&g_entities[pm->gent->s.m_iVehicleNum])->client->ps.hyperSpaceTime) < HYPERSPACE_TIME)
	//	{ //going into hyperspace, turn to face the right angles
	//		PM_VehFaceHyperspacePoint(&g_entities[pm->gent->s.m_iVehicleNum]);
	//	}
	//	else if ((&g_entities[pm->gent->s.m_iVehicleNum])->client && (&g_entities[pm->gent->s.m_iVehicleNum])->client->ps.vehTurnaroundIndex
	//		&& (&g_entities[pm->gent->s.m_iVehicleNum])->client->ps.vehTurnaroundTime > pm->cmd.serverTime)
	//	{ //riding this vehicle, turn my view too
	//		Com_Printf("forced turning!\n");
	//		PM_VehForcedTurning(&g_entities[pm->gent->s.m_iVehicleNum]);
	//	}
	//}

	PM_SetSpecialMoveValues();

	// update the viewangles
	PM_UpdateViewAngles(pm->ps->saber_anim_level, pm->ps, &pm->cmd, pm->gent);

	AngleVectors(pm->ps->viewangles, pml.forward, pml.right, pml.up);

	if (pm->cmd.upmove < 10)
	{
		// not holding jump
		pm->ps->pm_flags &= ~PMF_JUMP_HELD;
	}

	// disable attacks when using grappling hook
	if (pm->ps->hookhasbeenfired)
	{
		pmove->cmd.buttons &= ~BUTTON_ATTACK;
		pmove->cmd.buttons &= ~BUTTON_ALT_ATTACK;
	}

	// decide if backpedaling animations should be used
	if (pm->cmd.forwardmove < 0)
	{
		pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
	}
	else if (pm->cmd.forwardmove > 0 || pm->cmd.forwardmove == 0 && pm->cmd.rightmove)
	{
		pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
	}

	if (pm->ps->pm_type >= PM_DEAD)
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.rightmove = 0;
		pm->cmd.upmove = 0;
		if (pm->ps->viewheight > -12)
		{
			//slowly sink view to ground
			pm->ps->viewheight -= 1;
		}
	}

	if (pm->ps->pm_type == PM_SPECTATOR)
	{
		PM_CheckDuck();
		PM_FlyMove();
		PM_DropTimers();
		return;
	}

	if (pm->ps->pm_type == PM_NOCLIP)
	{
		PM_NoclipMove();
		PM_DropTimers();
		return;
	}

	if (pm->ps->pm_type == PM_FREEZE)
	{
		return; // no movement at all
	}

	if (pm->ps->pm_type == PM_INTERMISSION)
	{
		return; // no movement at all
	}

	if (pm->ps->pm_flags & PMF_SLOW_MO_FALL)
	{
		//half grav
		pm->ps->gravity *= 0.5;
	}

	// set watertype, and waterlevel
	PM_SetWaterLevelAtPoint(pm->ps->origin, &pm->waterlevel, &pm->watertype);

	PM_SetWaterHeight();

	if (!(pm->watertype & CONTENTS_LADDER))
	{
		//Don't want to remember this for ladders, is only for waterlevel change events (sounds)
		pml.previous_waterlevel = pmove->waterlevel;
	}

	waterForceJump = qfalse;
	if (pmove->waterlevel && pm->ps->client_num)
	{
		if (pm->ps->forceJumpZStart //force jumping
			|| pm->gent && pm->gent->NPC && level.time < pm->gent->NPC->jumpTime)
			//TIMER_Done(pm->gent, "forceJumpChasing" )) )//force-jumping
		{
			waterForceJump = qtrue;
		}
	}

	// set mins, maxs, and viewheight
	PM_SetBounds();

	if (!Flying && !(pm->watertype & CONTENTS_LADDER) && pm->ps->pm_type != PM_DEAD)
	{
		//NOTE: noclippers shouldn't jump or duck either, no?
		PM_CheckDuck();
	}

	// set groundentity
	PM_GroundTrace();
	if (Flying == FLY_HOVER)
	{
		//never stick to the ground
		PM_HoverTrace();
	}

	if (pm->ps->pm_type == PM_DEAD)
	{
		PM_DeadMove();
	}

	PM_DropTimers();

	/*
	if ( PM_RidingVehicle() )
	{
		PM_NoclipMove();
	}
	else */
	if (pm->ps && (pm->ps->eFlags & EF_LOCKED_TO_WEAPON
		|| pm->ps->eFlags & EF_HELD_BY_RANCOR
		|| pm->ps->eFlags & EF_HELD_BY_WAMPA
		|| pm->ps->eFlags & EF_HELD_BY_SAND_CREATURE))
	{
		//in an emplaced gun
		PM_NoclipMove();
	}
	else if (Flying == FLY_NORMAL) //|| pm->ps->gravity <= 0 )
	{
		// flight powerup doesn't allow jump and has different friction
		PM_FlyMove();
	}
	else if (Flying == FLY_VEHICLE)
	{
		PM_FlyVehicleMove();
	}
	else if (pm->ps && pm->ps->pm_flags & PMF_TIME_WATERJUMP)
	{
		PM_WaterJumpMove();
	}
	else if (pm->waterlevel > 1 //in water
		&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() || !waterForceJump))
		//player or NPC not force jumping
	{
		//force-jumping NPCs should
		// swimming or in ladder
		PM_WaterMove();
	}
	else if (pm->gent && pm->gent->NPC && pm->gent->NPC->jumpTime != 0)
	{
		ucmd.forwardmove = 0;
		ucmd.rightmove = 0;
		ucmd.upmove = 0;
		pm->ps->speed = 0;
		VectorClear(pm->ps->moveDir);

		PM_AirMove();
	}
	else if (pm->ps && pm->ps->pm_flags & PMF_GRAPPLE_PULL)
	{
		PM_GrappleMove();
		PM_AirMove();
	}
	else if (pml.walking)
	{
		// walking on ground
		vec3_t old_org;

		VectorCopy(pm->ps->origin, old_org);

		if (pm->gent->client->NPC_class == CLASS_BOC)
		{
			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{
				if (!PM_SaberInAttack(pm->ps->saber_move)
					&& !PM_SaberInDamageMove(pm->ps->saber_move)
					&& !PM_SaberInTransitionAny(pm->ps->saber_move)
					&& !pm_saber_in_special_attack(pm->ps->torsoAnim)
					&& !PM_SaberDoDamageAnim(pm->ps->torsoAnim)
					&& !PM_InSaberLock(pm->ps->torsoAnim)
					&& !PM_SuperBreakWinAnim(pm->ps->torsoAnim)
					&& !PM_InKnockDown(pm->ps)
					&& !PM_InRoll(pm->ps)
					&& !PM_KickingAnim(pm->ps->legsAnim)
					&& !PM_FlippingAnim(pm->ps->legsAnim)
					&& !PM_SpinningAnim(pm->ps->legsAnim)
					&& !PM_CrouchAnim(pm->ps->legsAnim)
					&& !pm->ps->saberInFlight
					&& !BG_FullBodyTauntAnim(pm->ps->torsoAnim)
					&& !PM_SaberInParry(pm->ps->saber_move)
					&& !PM_SaberInKnockaway(pm->ps->saber_move)
					&& !PM_SaberInBrokenParry(pm->ps->saber_move)
					&& !PM_SaberInReflect(pm->ps->saber_move)
					&& !PM_SaberInMassiveBounce(pm->ps->torsoAnim)
					&& !PM_PainAnim(pm->ps->torsoAnim)
					&& !PM_ForceAnim(pm->ps->torsoAnim)
					&& !pm->ps->weaponTime
					&& debugNPCFreeze->integer < 1
					&& !in_camera)
				{
					PM_BocMove();
				}
				else
				{
					PM_WalkMove();
				}
			}
			else
			{
				PM_WalkMove();
			}
		}
		else
		{
			PM_WalkMove();
		}

		float thresh_hold = 0.001f;
		const float moved_dist = DistanceSquared(old_org, pm->ps->origin);
		if (PM_StandingAnim(pm->ps->legsAnim) || pm->ps->legsAnim == BOTH_CROUCH1)
		{
			thresh_hold = 0.005f;
		}

		if (moved_dist < thresh_hold)
		{
			//didn't move, play no legs anim
			//	pm->cmd.forwardmove = pm->cmd.rightmove = 0;
		}
	}
	else
	{
		if (pm->ps->gravity <= 0)
		{
			PM_FlyMove();
		}
		else
		{
			// airborne
			PM_AirMove();
		}
	}

	//PM_Animate();

	// If we didn't move at all, then why bother doing this again -MW.
	if (!VectorCompare(pm->ps->origin, pml.previous_origin))
	{
		PM_GroundTrace();
		if (Flying == FLY_HOVER)
		{
			//never stick to the ground
			PM_HoverTrace();
		}
	}

	if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
	{
		//on ground
		pm->ps->forceJumpZStart = 0;
		pm->ps->jumpZStart = 0;
		pm->ps->pm_flags &= ~PMF_JUMPING;
		pm->ps->pm_flags &= ~PMF_TRIGGER_PUSHED;
		pm->ps->pm_flags &= ~PMF_SLOW_MO_FALL;
	}

	// If we didn't move at all, then why bother doing this again -MW.
	// Note: ok, so long as we don't have water levels that change.
	if (!VectorCompare(pm->ps->origin, pml.previous_origin))
	{
		PM_SetWaterLevelAtPoint(pm->ps->origin, &pm->waterlevel, &pm->watertype);
		PM_SetWaterHeight();
	}

	// If we're a vehicle, do our special weapon function.
	if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_VEHICLE)
	{
		p_veh = pm->gent->m_pVehicle;
		PM_VehicleWeapon();
	}
	// If we are riding a vehicle...
	else if (PM_RidingVehicle())
	{
		if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
		{
			// alt attack always does other stuff when riding a vehicle (turbo)
		}
		else if (pm->ps->eFlags & EF_NODRAW)
		{
			//inside a vehicle?  don't do any weapon stuff
		}
		else if (pm->ps->weapon == WP_NONE
			|| pm->ps->weapon == WP_SABER
			|| pm->ps->weapon == WP_BLASTER
			//
			|| pm->ps->weapon == WP_BLASTER_PISTOL
			|| pm->ps->weapon == WP_BRYAR_PISTOL
			|| pm->ps->weapon == WP_BOWCASTER
			|| pm->ps->weapon == WP_REPEATER
			|| pm->ps->weapon == WP_DEMP2
			|| pm->ps->weapon == WP_FLECHETTE
			//
			|| pm->ps->weapon == WP_BATTLEDROID
			|| pm->ps->weapon == WP_THEFIRSTORDER
			|| pm->ps->weapon == WP_CLONECARBINE
			|| pm->ps->weapon == WP_REBELBLASTER
			|| pm->ps->weapon == WP_CLONERIFLE
			|| pm->ps->weapon == WP_CLONECOMMANDO
			|| pm->ps->weapon == WP_Z6_ROTARY_CANNON
			|| pm->ps->weapon == WP_REBELRIFLE
			|| pm->ps->weapon == WP_REY
			|| pm->ps->weapon == WP_JANGO
			|| pm->ps->weapon == WP_BOBA
			|| pm->ps->weapon == WP_CLONEPISTOL
			|| pm->ps->weapon == WP_DUAL_CLONEPISTOL
			|| pm->ps->weapon == WP_DUAL_PISTOL
			//
			|| pm->ps->weaponstate == WEAPON_DROPPING //changing weapon - dropping
			|| pm->ps->weaponstate == WEAPON_RAISING //changing weapon - raising
			|| pm->cmd.weapon != pm->ps->weapon && PM_WeaponOkOnVehicle(pm->cmd.weapon))
		{
			PM_Weapon();
		}
	}
	// otherwise do the normal weapon function.
	else
	{
		PM_Weapon();
	}

	if (pm->cmd.buttons & BUTTON_ATTACK)
	{
		pm->ps->pm_flags |= PMF_ATTACK_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_ATTACK_HELD;
	}
	if (pm->cmd.buttons & BUTTON_ALT_ATTACK)
	{
		pm->ps->pm_flags |= PMF_ALT_ATTACK_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_ALT_ATTACK_HELD;
	}
	if (pm->cmd.buttons & BUTTON_FORCE_FOCUS)
	{
		pm->ps->pm_flags |= PMF_FORCE_FOCUS_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_FORCE_FOCUS_HELD;
	}
	if (pm->cmd.buttons & BUTTON_USE)
	{
		pm->ps->pm_flags |= PMF_USE_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_USE_HELD;
	}

	if (pm->cmd.buttons & BUTTON_DASH && !(pm->cmd.buttons & BUTTON_KICK))
	{
		pm->ps->pm_flags |= PMF_DASH_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_DASH_HELD;
	}

	if (pm->cmd.buttons & BUTTON_BLOCK)
	{
		pm->ps->pm_flags |= PMF_BLOCK_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_BLOCK_HELD;
	}

	if (pm->cmd.buttons & BUTTON_KICK && !(pm->cmd.buttons & BUTTON_DASH))
	{
		pm->ps->pm_flags |= PMF_KICK_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_KICK_HELD;
	}

	if (pm->cmd.buttons & BUTTON_WALKING && pm->cmd.buttons & BUTTON_BLOCK && pm->cmd.buttons & BUTTON_ATTACK)
	{
		pm->ps->pm_flags |= PMF_ACCURATE_MISSILE_BLOCK_HELD;
	}
	else
	{
		pm->ps->pm_flags &= ~PMF_ACCURATE_MISSILE_BLOCK_HELD;
	}

	if (pm->gent) //&& pm->gent->s.number == 0 )//player only?
	{
		// Use
		PM_Use();
	}

	// Calculate the resulting speed of the last pmove
	//-------------------------------------------------
	if (pm->gent)
	{
		pm->gent->resultspeed = Distance(pm->ps->origin, pm->gent->currentOrigin) / pml.msec * 1000;
		if (pm->gent->resultspeed > 5.0f)
		{
			pm->gent->lastMoveTime = level.time;
		}

		// If Have Not Been Moving For A While, Stop
		//-------------------------------------------
		if (pml.walking && level.time - pm->gent->lastMoveTime > 1000)
		{
			pm->cmd.forwardmove = pm->cmd.rightmove = 0;
		}
	}

	// ANIMATION
	//================================

	// TEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMP
	if (pm->gent && pm->ps && pm->ps->eFlags & EF_LOCKED_TO_WEAPON)
	{
		if (pm->gent->owner && pm->gent->owner->e_UseFunc == useF_emplaced_gun_use) //ugly way to tell, but...
		{
			//full body
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_GUNSIT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//SETANIM_FLAG_NORMAL
		}
		else
		{
			//stand (or could be overridden by strafe anims)
			PM_SetAnim(pm, SETANIM_LEGS, BOTH_STAND1, SETANIM_FLAG_NORMAL);
		}
	}
	else if (pm->gent && pm->ps && pm->ps->eFlags & EF_HELD_BY_RANCOR)
	{
		PM_SetAnim(pm, SETANIM_LEGS, BOTH_SWIM_IDLE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		//SETANIM_FLAG_NORMAL
	}
	// If we are a vehicle, animate...
	else if (p_veh)
	{
		p_veh->m_pVehicleInfo->Animate(p_veh);
	}
	// If we're riding a vehicle, don't do anything!.
	else if ((p_veh = PM_RidingVehicle()) != nullptr)
	{
		PM_CheckInVehicleSaberAttackAnim();
	}
	else // TEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMP
	{
		// footstep events / legs animations
		if (pm->ps->forceLowerAnimTimer > level.time)
		{
			PM_SetAnim(pm, SETANIM_LEGS, pm->ps->forceLowerAnim, SETANIM_FLAG_OVERRIDE);
		}
		else if (pm->ps->stasisTime < level.time)
		{
			PM_Footsteps();
		}
		else if (pm->ps->stasisJediTime < level.time)
		{
			PM_Footsteps();
		}
	}
	// torso animation
	if (!p_veh)
	{
		//not riding a vehicle
		if (pm->ps->forceUpperAnimTimer > level.time)
		{
			PM_SetAnim(pm, SETANIM_TORSO, pm->ps->forceUpperAnim, SETANIM_FLAG_OVERRIDE);
		}
		else if (pm->ps->stasisTime < level.time)
		{
			PM_TorsoAnimation();
		}
		else if (pm->ps->stasisJediTime < level.time)
		{
			PM_TorsoAnimation();
		}
	}

	// entering / leaving water splashes
	PM_WaterEvents();

	if (!pm->cmd.rightmove && !pm->cmd.forwardmove && pm->cmd.upmove <= 0)
	{
		if (VectorCompare(pm->ps->velocity, vec3_origin))
		{
			pm->ps->lastStationary = level.time;
		}
	}

	if (pm->gent && pm->ps->pm_flags & PMF_SLOW_MO_FALL)
	{
		//half grav
		pm->ps->gravity *= 2;
	}
}

void PM_SaberFakeFlagUpdate(const int new_move)
{
	//checks to see if the attack fake flag needs to be removed.
	if (!PM_SaberInTransition(new_move) && !PM_SaberInStart(new_move) && !PM_SaberInAttackPure(new_move))
	{
		//not going into an attack move, clear the flag
		pm->ps->userInt3 &= ~(1 << FLAG_ATTACKFAKE);
	}
}

void PM_SaberPerfectBlockUpdate(const int new_move)
{
	const qboolean is_holding_block_button = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;

	//checks to see if the flag needs to be removed.
	if ((!(is_holding_block_button)) || PM_SaberInBounce(new_move) || PM_SaberInMassiveBounce(pm->ps->torsoAnim) || PM_SaberInAttack(new_move))
	{
		pm->ps->userInt3 &= ~(1 << FLAG_PERFECTBLOCK);
	}
}

//saber status utility tools
static qboolean BG_SaberInFullDamageMove(const playerState_t* ps)
{
	//The player is attacking with a saber attack that does full damage
	if (PM_SaberInAttack(ps->saber_move)
		|| PM_SaberInDamageMove(ps->saber_move)
		|| pm_saber_in_special_attack(ps->torsoAnim) //jacesolaris 2019 test for idle kill
		|| PM_SaberDoDamageAnim(ps->torsoAnim)
		|| PM_SuperBreakWinAnim(ps->torsoAnim))
	{
		//in attack animation
		if (ps->saberBlocked == BLOCKED_NONE)
		{
			//and not attempting to do some sort of block animation
			return qtrue;
		}
	}
	return qfalse;
}

qboolean BG_SaberInTransitionDamageMove(const playerState_t* ps)
{
	//player is in a saber move where it does transitional damage
	if (PM_SaberInTransition(ps->saber_move))
	{
		if (ps->saberBlocked == BLOCKED_NONE)
		{
			//and not attempting to do some sort of block animation
			return qtrue;
		}
	}
	return qfalse;
}

qboolean BG_SaberInNonIdleDamageMove(const playerState_t* ps)
{
	//player is in a saber move that does something more than idle saber damage
	return BG_SaberInFullDamageMove(ps);
}

extern qboolean PM_BounceAnim(int anim);
extern qboolean PM_SaberReturnAnim(int anim);

qboolean BG_InSlowBounce(const playerState_t* ps)
{
	//checks for a bounce/return animation in combination with the slow bounce flag
	if (ps->userInt3 & 1 << FLAG_SLOWBOUNCE
		&& (PM_BounceAnim(ps->torsoAnim) || PM_SaberReturnAnim(ps->torsoAnim)))
	{
		//in slow bounce
		return qtrue;
	}
	if (PM_SaberInBounce(ps->saber_move))
	{
		//in slow bounce
		return qtrue;
	}
	return qfalse;
}

qboolean PM_InSlowBounce(const playerState_t* ps)
{
	//checks for a bounce/return animation in combination with the slow bounce flag
	if (ps->userInt3 & 1 << FLAG_SLOWBOUNCE && (PM_BounceAnim(ps->torsoAnim)))
	{
		//in slow bounce
		return qtrue;
	}
	if (PM_SaberInBounce(ps->saber_move))
	{
		//in slow bounce
		return qtrue;
	}
	return qfalse;
}

///////////////////////////////////////// ledge grab /////////////////////////////////////////////////

//The height level at which you grab ledges.  In terms of player origin
constexpr auto LEDGEGRABMAXHEIGHT = 70.0f;
constexpr auto LEDGEGRABHEIGHT = 52.4f;
#define LEDGEVERTOFFSET			LEDGEGRABHEIGHT
constexpr auto LEDGEGRABMINHEIGHT = 40.0f;

//max distance you can be from the ledge for ledge grabbing to work
constexpr auto LEDGEGRABDISTANCE = 40.0f;

//min distance you can be from the ledge for ledge grab to work
constexpr auto LEDGEGRABMINDISTANCE = 22.0f;

//distance at which the animation grabs the ledge
constexpr auto LEDGEHOROFFSET = 22.3f;

static qboolean PM_IsMerc()
{
	switch (pm->ps->weapon)
	{
	case WP_BLASTER_PISTOL:
	case WP_BLASTER:
	case WP_DISRUPTOR:
	case WP_BOWCASTER:
	case WP_REPEATER:
	case WP_DEMP2:
	case WP_FLECHETTE:
	case WP_ROCKET_LAUNCHER:
	case WP_THERMAL:
	case WP_TRIP_MINE:
	case WP_DET_PACK:
	case WP_CONCUSSION:
	case WP_STUN_BATON:
	case WP_BRYAR_PISTOL:
	case WP_JAWA:
	case WP_TUSKEN_RIFLE:
	case WP_TUSKEN_STAFF:
	case WP_NOGHRI_STICK:
	case WP_BATTLEDROID:
	case WP_THEFIRSTORDER:
	case WP_CLONECARBINE:
	case WP_REBELBLASTER:
	case WP_CLONERIFLE:
	case WP_CLONECOMMANDO:
	case WP_Z6_ROTARY_CANNON:
	case WP_REBELRIFLE:
	case WP_REY:
	case WP_JANGO:
	case WP_BOBA:
	case WP_CLONEPISTOL:
	case WP_SBD_BLASTER:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
		return qtrue;
	default:;
	}
	return qfalse;
}

//Get the point in the leg animation and return a percentage of the current point in the anim between 0 and the total anim length (0.0f - 1.0f)
static float GetSelfLegAnimPointforLedge()
{
	float current = 0.0f;
	int end = 0;
	int start = 0;

	if (!!gi.G2API_GetBoneAnimIndex(&pm->gent->ghoul2[pm->gent->playerModel],
		pm->gent->rootBone,
		level.time,
		&current,
		&start,
		&end,
		nullptr,
		nullptr,
		nullptr))
	{
		const float percent_complete = (current - start) / (end - start);

		return percent_complete;
	}

	return 0.0f;
}

//lets go of a ledge

static void G_LetGoOfLedge(const gentity_t* ent)
{
	ent->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
	ent->client->ps.legsAnimTimer = 0;
	ent->client->ps.torsoAnimTimer = 0;
}

void BG_LetGoofLedge(playerState_t* ps)
{
	ps->pm_flags &= ~PMF_STUCK_TO_WALL;
	ps->torsoAnimTimer = 0;
	ps->legsAnimTimer = 0;
}

static qboolean LedgeGrabableEntity(const int entityNum)
{
	//indicates if the given entity is an entity that can be ledge grabbed.
	const gentity_t* ent = &g_entities[entityNum];

	switch (ent->s.eType)
	{
	case ET_PLAYER:
	case ET_ITEM:
	case ET_MISSILE:
		return qfalse;
	default:
		return qtrue;
	}
}

static void PM_SetVelocityforLedgeMove(playerState_t* ps, const int anim)
{
	vec3_t fwd_angles, move_dir;
	const float animationpoint = GetSelfLegAnimPointforLedge();

	switch (anim)
	{
	case BOTH_LEDGE_GRAB:
	case BOTH_LEDGE_HOLD:
		VectorClear(ps->velocity);
		return;
	case BOTH_LEDGE_LEFT:
		if (animationpoint > .333 && animationpoint < .666)
		{
			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, nullptr, move_dir, nullptr);
			VectorScale(move_dir, -30, move_dir);
			VectorCopy(move_dir, ps->velocity);
		}
		else
		{
			VectorClear(ps->velocity);
		}
		break;
	case BOTH_LEDGE_RIGHT:
		if (animationpoint > .333 && animationpoint < .666)
		{
			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, nullptr, move_dir, nullptr);
			VectorScale(move_dir, 30, move_dir);
			VectorCopy(move_dir, ps->velocity);
		}
		else
		{
			VectorClear(ps->velocity);
		}
		break;
	case BOTH_LEDGE_JEDIPULL:
		if (animationpoint > .210 && animationpoint < .65)
		{
			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, nullptr, nullptr, move_dir);
			VectorScale(move_dir, 90, move_dir);
			VectorCopy(move_dir, ps->velocity);
		}
		else if (animationpoint > .8 && animationpoint < .925)
		{
			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, move_dir, nullptr, nullptr);
			VectorScale(move_dir, 70, move_dir);
			VectorCopy(move_dir, ps->velocity);
		}
		else
		{
			VectorClear(ps->velocity);
		}
		break;
	case BOTH_LEDGE_MERCPULL:
		if (animationpoint > .210 && animationpoint < .65)
		{
			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, nullptr, nullptr, move_dir);
			VectorScale(move_dir, 90, move_dir);
			VectorCopy(move_dir, ps->velocity);
		}
		else if (animationpoint > .8 && animationpoint < .925)
		{
			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, move_dir, nullptr, nullptr);
			VectorScale(move_dir, 70, move_dir);
			VectorCopy(move_dir, ps->velocity);
		}
		else
		{
			VectorClear(ps->velocity);
		}
		break;
	default:
		VectorClear(ps->velocity);
	}
}

void PM_AdjustAngleForWallGrab(playerState_t* ps, usercmd_t* ucmd)
{
	if (ps->pm_flags & PMF_STUCK_TO_WALL && PM_InLedgeMove(ps->legsAnim))
	{
		//still holding onto the ledge stick our view to the wall angles
		if (ps->legsAnim != BOTH_LEDGE_MERCPULL && ps->legsAnim != BOTH_LEDGE_JEDIPULL)
		{
			vec3_t trace_to, trace_from, fwd, fwd_angles, dir{};
			trace_t trace;

			VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0);
			AngleVectors(fwd_angles, fwd, nullptr, nullptr);
			VectorNormalize(fwd);

			VectorCopy(ps->origin, trace_from);
			trace_from[2] += LEDGEGRABHEIGHT - 1;

			VectorMA(trace_from, LEDGEGRABDISTANCE, fwd, trace_to);

			gi.trace(&trace, trace_from, nullptr, nullptr, trace_to, ps->client_num, pm->tracemask,
				static_cast<EG2_Collision>(0), 0);

			if (trace.fraction == 1 || !LedgeGrabableEntity(trace.entityNum)
				|| pm->cmd.buttons & BUTTON_USE_FORCE
				|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
				|| pm->cmd.buttons & BUTTON_LIGHTNING_STRIKE
				|| pm->cmd.buttons & BUTTON_PROJECTION
				|| pm->cmd.buttons & BUTTON_FORCE_DRAIN
				|| pm->cmd.buttons & BUTTON_FORCEGRIP
				|| pm->cmd.buttons & BUTTON_BLOCK
				|| pm->cmd.buttons & BUTTON_KICK
				|| pm->cmd.buttons & BUTTON_DASH
				|| pm->gent->painDebounceTime > level.time
				|| pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{
				//that's not good, we lost the ledge so let go.
				BG_LetGoofLedge(ps);
				return;
			}

			//lock the view viewangles
			vectoangles(dir, dir);
			float pitch_d = AngleSubtract(pm->ps->viewangles[PITCH], dir[PITCH]);
			pitch_d *= 0.6f * pml.frametime;
			pm->ps->viewangles[PITCH] = AngleSubtract(pm->ps->viewangles[PITCH], pitch_d);

			ps->viewangles[YAW] = vectoyaw(trace.plane.normal) + 180;
			ucmd->angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - ps->delta_angles[YAW];

			SetClientViewAngle(pm->gent, pm->ps->viewangles);
		}
		else
		{
			//lock viewangles
			PM_SetPMViewAngle(ps, ps->viewangles, ucmd);
			ucmd->angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - ps->delta_angles[YAW];
		}

		if (ps->legsAnimTimer <= 50)
		{
			//Try switching to idle
			if (ps->legsAnim == BOTH_LEDGE_MERCPULL)
			{
				//pull up done, bail.
				ps->pm_flags &= ~PMF_STUCK_TO_WALL;
			}
			else if (ps->legsAnim == BOTH_LEDGE_JEDIPULL)
			{
				//pull up done, bail.
				ps->pm_flags &= ~PMF_STUCK_TO_WALL;
			}
			else
			{
				PM_SetAnim(pm, SETANIM_BOTH, BOTH_LEDGE_HOLD, SETANIM_FLAG_OVERRIDE);
				ps->torsoAnimTimer = 500;
				ps->legsAnimTimer = 500;
				//hold weapon time so people can't do attacks while in ledgegrab
				ps->weaponTime = ps->legsAnimTimer;
			}
		}
		else if (ps->legsAnim == BOTH_LEDGE_HOLD)
		{
			if (ucmd->rightmove)
			{
				//trying to move left/right
				if (ucmd->rightmove < 0)
				{
					//shimmy left
					PM_SetAnim(pm, SETANIM_BOTH, BOTH_LEDGE_LEFT, AFLAG_LEDGE);
					//hold weapontime so people can't do attacks while in ledgegrab
					ps->weaponTime = ps->legsAnimTimer;
				}
				else
				{
					//shimmy right
					PM_SetAnim(pm, SETANIM_BOTH, BOTH_LEDGE_RIGHT, AFLAG_LEDGE);
					//hold weapontime so people can't do attacks while in ledgegrab
					ps->weaponTime = ps->legsAnimTimer;
				}
			}
			else if (ucmd->forwardmove < 0 || pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()
				|| pm->cmd.buttons & BUTTON_USE_FORCE
				|| pm->cmd.buttons & BUTTON_FORCE_LIGHTNING
				|| pm->cmd.buttons & BUTTON_LIGHTNING_STRIKE
				|| pm->cmd.buttons & BUTTON_PROJECTION
				|| pm->cmd.buttons & BUTTON_FORCE_DRAIN
				|| pm->cmd.buttons & BUTTON_FORCEGRIP
				|| pm->cmd.buttons & BUTTON_BLOCK
				|| pm->cmd.buttons & BUTTON_KICK
				|| pm->cmd.buttons & BUTTON_DASH
				|| pm->gent->painDebounceTime > level.time)
			{
				//letting go
				BG_LetGoofLedge(ps);
			}
			else if (ucmd->forwardmove > 0 || ucmd->upmove > 0)
			{
				//Pull up
				if (pm->ps->weapon == WP_SABER && !pm->ps->SaberActive())
				{
					PM_SetAnim(pm, SETANIM_BOTH, BOTH_LEDGE_JEDIPULL,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
				}
				else
				{
					PM_SetAnim(pm, SETANIM_BOTH, BOTH_LEDGE_MERCPULL,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
				}

				//hold weapontime so people can't do attacks while in ledgegrab
				ps->weaponTime = ps->legsAnimTimer;
			}
			else
			{
				//keep holding on
				ps->torsoAnimTimer = 500;
				ps->legsAnimTimer = 500;
				//hold weapontime so people can't do attacks while in ledgegrab
				ps->weaponTime = ps->legsAnimTimer;
			}
		}

		//set movement velocity
		PM_SetVelocityforLedgeMove(ps, ps->legsAnim);

		//clear movement commands to prevent movement
		ucmd->rightmove = 0;
		ucmd->upmove = 0;
		ucmd->forwardmove = 0;
	}
}

static qboolean LedgeTrace(trace_t* trace, vec3_t dir, float* lerpup, float* lerpfwd, float* lerpyaw)
{
	//scan for for a ledge in the given direction
	vec3_t trace_to, trace_from, wallangles;
	VectorMA(pm->ps->origin, LEDGEGRABDISTANCE, dir, trace_to);
	VectorCopy(pm->ps->origin, trace_from);

	trace_from[2] += LEDGEGRABMINHEIGHT;
	trace_to[2] += LEDGEGRABMINHEIGHT;

	gi.trace(trace, trace_from, nullptr, nullptr, trace_to, pm->ps->client_num, pm->tracemask,
		static_cast<EG2_Collision>(0), 0);

	if (trace->fraction < 1 && LedgeGrabableEntity(trace->entityNum))
	{
		//hit a wall, pop into the wall and fire down to find top of wall
		VectorMA(trace->endpos, 0.5, dir, trace_to);

		VectorCopy(trace_to, trace_from);

		trace_from[2] += LEDGEGRABMAXHEIGHT - LEDGEGRABMINHEIGHT;

		gi.trace(trace, trace_from, nullptr, nullptr, trace_to, pm->ps->client_num, pm->tracemask,
			static_cast<EG2_Collision>(0), 0);

		if (trace->fraction == 1.0 || trace->startsolid || !LedgeGrabableEntity(trace->entityNum))
		{
			return qfalse;
		}
	}

	//check to make sure we found a good top surface and go from there
	vectoangles(trace->plane.normal, wallangles);

	if (wallangles[PITCH] > -45)
	{
		//no ledge or the ledge is too steep
		return qfalse;
	}
	VectorCopy(trace->endpos, trace_to);
	*lerpup = trace->endpos[2] - pm->ps->origin[2] - LEDGEVERTOFFSET;

	VectorCopy(pm->ps->origin, trace_from);
	trace_to[2] -= 1;

	trace_from[2] = trace_to[2];

	gi.trace(trace, trace_from, nullptr, nullptr, trace_to, pm->ps->client_num, pm->tracemask,
		static_cast<EG2_Collision>(0), 0);

	vectoangles(trace->plane.normal, wallangles);
	if (trace->fraction == 1.0
		|| wallangles[PITCH] > 20 || wallangles[PITCH] < -20
		|| !LedgeGrabableEntity(trace->entityNum))
	{
		//no ledge or too steep of a ledge
		return qfalse;
	}

	*lerpfwd = Distance(trace->endpos, trace_from) - LEDGEHOROFFSET;
	*lerpyaw = vectoyaw(trace->plane.normal) + 180;
	return qtrue;
}

//check for ledge grab

void PM_CheckGrab()
{
	vec3_t check_dir, trace_to, fwd_angles;
	trace_t trace;
	float lerpup = 0;
	float lerpfwd = 0;
	float lerpyaw = 0;
	qboolean skipcmdtrace = qfalse;

	if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
	{
		//not in the air don't attempt a ledge grab
		return;
	}

	if (g_entities[pm->ps->client_num].client->jetPackOn)
	{
		//don't do ledgegrab checks while using the jetpack
		return;
	}

	if (PM_InLedgeMove(pm->gent->client->ps.legsAnim)
		|| PM_InSpecialJump(pm->gent->client->ps.legsAnim)
		|| PM_CrouchAnim(pm->gent->client->ps.legsAnim)
		|| pm->ps->pm_flags & PMF_DUCKED)
	{
		//already on ledge, a spectator, or in a special jump
		return;
	}

	if (pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK ||
		pm->ps->PlayerEffectFlags & 1 << PEF_SPRINTING ||
		pm->ps->BlasterAttackChainCount >= BLASTERMISHAPLEVEL_TWELVE)
	{
		return;
	}

	if (pm->ps->velocity[0] == 0)
	{
		//Not moving forward
		return;
	}

	if (g_AllowLedgeGrab->integer == 0)
	{
		//Not allowed
		return;
	}

	if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
	{
		return;
	}

	//try looking in front of us first
	VectorSet(fwd_angles, 0, pm->ps->viewangles[YAW], 0.0f);
	AngleVectors(fwd_angles, check_dir, nullptr, nullptr);

	if (!VectorCompare(pm->ps->velocity, vec3_origin))
	{
		//player is moving
		if (LedgeTrace(&trace, check_dir, &lerpup, &lerpfwd, &lerpyaw))
		{
			skipcmdtrace = qtrue;
		}
	}

	if (!skipcmdtrace)
	{
		//no luck finding a ledge to grab based on movement.  Try looking for a ledge based on where the player is
		//TRYING to go.
		if (!pm->cmd.rightmove && !pm->cmd.forwardmove)
		{
			//no dice abort
			return;
		}
		if (pm->cmd.rightmove)
		{
			if (pm->cmd.rightmove > 0)
			{
				AngleVectors(fwd_angles, nullptr, check_dir, nullptr);
				VectorNormalize(check_dir);
			}
			else if (pm->cmd.rightmove < 0)
			{
				AngleVectors(fwd_angles, nullptr, check_dir, nullptr);
				VectorScale(check_dir, -1, check_dir);
				VectorNormalize(check_dir);
			}
		}
		else if (pm->cmd.forwardmove > 0)
		{
			//already tried this direction.
			return;
		}
		else if (pm->cmd.forwardmove < 0)
		{
			AngleVectors(fwd_angles, check_dir, nullptr, nullptr);
			VectorScale(check_dir, -1, check_dir);
			VectorNormalize(check_dir);
		}

		if (!LedgeTrace(&trace, check_dir, &lerpup, &lerpfwd, &lerpyaw))
		{
			//no dice
			return;
		}
	}

	VectorCopy(pm->ps->origin, trace_to);
	VectorMA(pm->ps->origin, lerpfwd, check_dir, trace_to);
	trace_to[2] += lerpup;

	//check to see if we can actually latch to that position.
	gi.trace(&trace, pm->ps->origin, pm->mins, pm->maxs, trace_to, pm->ps->client_num, MASK_PLAYERSOLID,
		static_cast<EG2_Collision>(0), 0);

	if (trace.fraction != 1 || trace.startsolid)
	{
		return;
	}

	//turn to face wall
	pm->ps->viewangles[YAW] = lerpyaw;
	PM_SetPMViewAngle(pm->ps, pm->ps->viewangles, &pm->cmd);
	pm->cmd.angles[YAW] = ANGLE2SHORT(pm->ps->viewangles[YAW]) - pm->ps->delta_angles[YAW];

	// you couldnt move
	pm->ps->weaponTime = 0;
	pm->ps->saber_move = 0;
	pm->cmd.upmove = 0;

	//We are clear to latch to the wall
	if (pm->ps->SaberActive())
	{
		pm->ps->SaberDeactivate();
		G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/saber/saberoff.mp3");
	}
	else
	{
		if (PM_IsMerc() && pm->ps->BlasterAttackChainCount <= BLASTERMISHAPLEVEL_TWELVE)
		{
			G_SetWeapon(pm->gent, WP_MELEE);
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/change.wav");
		}
	}
	VectorCopy(trace.endpos, pm->ps->origin);
	VectorCopy(vec3_origin, pm->ps->velocity);
	PM_GrabWallForJump(BOTH_LEDGE_GRAB);
	pm->ps->weaponTime = pm->ps->legsAnimTimer;
}