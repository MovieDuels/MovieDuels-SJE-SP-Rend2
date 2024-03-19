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

/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ///
///																																///
///																																///
///													SERENITY JEDI ENGINE														///
///										          LIGHTSABER COMBAT SYSTEM													    ///
///																																///
///						      System designed by Serenity and modded by JaceSolaris. (c) 2023 SJE   		                    ///
///								    https://www.moddb.com/mods/serenityjediengine-20											///
///																																///
/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ///

#include "g_local.h"
#include "bg_local.h"
#include "w_saber.h"
#include "ai_main.h"

//////////Defines////////////////
extern qboolean BG_SaberInNonIdleDamageMove(const playerState_t* ps, int anim_index);
extern qboolean PM_SaberInBounce(int move);
extern qboolean BG_InSlowBounce(const playerState_t* ps);
extern bot_state_t* botstates[MAX_CLIENTS];
extern qboolean PM_SaberInTransitionAny(int move);
extern qboolean PM_SuperBreakWinAnim(int anim);
extern qboolean walk_check(const gentity_t* self);
extern qboolean WP_SabersCheckLock(gentity_t* ent1, gentity_t* ent2);
extern void PM_AddFatigue(playerState_t* ps, int fatigue);
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern qboolean npc_is_dark_jedi(const gentity_t* self);
extern saber_moveName_t PM_BrokenParryForParry(int move);
extern saber_moveName_t pm_broken_parry_for_attack(int move);
extern qboolean PM_InGetUp(const playerState_t* ps);
extern qboolean PM_InForceGetUp(const playerState_t* ps);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern void wp_block_points_regenerate(const gentity_t* self, int override_amt);
extern void PM_AddBlockFatigue(playerState_t* ps, int fatigue);
extern saber_moveName_t pm_block_the_attack(int move);
extern int g_block_the_attack(int move);
extern saber_moveName_t PM_SaberBounceForAttack(int move);
extern void G_Stagger(gentity_t* hit_ent);
extern void g_fatigue_bp_knockaway(gentity_t* blocker);
extern qboolean PM_SuperBreakLoseAnim(int anim);
extern qboolean ButterFingers(gentity_t* saberent, gentity_t* saber_owner, const gentity_t* other, const trace_t* tr);
extern qboolean pm_saber_innonblockable_attack(int anim);
extern qboolean pm_saber_in_special_attack(int anim);
extern int G_GetParryForBlock(int block);
extern qboolean WP_SaberMBlockDirection(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean WP_SaberBlockNonRandom(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean WP_SaberBouncedSaberDirection(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern qboolean WP_SaberFatiguedParryDirection(gentity_t* self, vec3_t hitloc, qboolean missileBlock);
extern void wp_block_points_regenerate_over_ride(const gentity_t* self, int override_amt);
void sab_beh_animate_heavy_slow_bounce_attacker(gentity_t* attacker);
extern void G_StaggerAttacker(gentity_t* atk);
extern void G_BounceAttacker(gentity_t* atk);
extern void wp_saber_clear_damage_for_ent_num(gentity_t* attacker, int entity_num, int saber_num, int blade_num);
extern void g_do_m_block_response(const gentity_t* speaker_npc_self);
//////////Defines////////////////

void sab_beh_saber_should_be_disarmed_attacker(gentity_t* attacker, const gentity_t* blocker)
{
	static trace_t tr;

	if (!(attacker->client->saber[0].saberFlags & SFL_NOT_DISARMABLE))
	{
		G_Stagger(attacker);

		ButterFingers(&g_entities[attacker->client->ps.saberEntityNum], attacker, blocker, &tr);
	}
}

void sab_beh_saber_should_be_disarmed_blocker(gentity_t* blocker, const gentity_t* attacker)
{
	static trace_t tr;

	if (!(blocker->client->saber[0].saberFlags & SFL_NOT_DISARMABLE))
	{
		G_Stagger(blocker);

		ButterFingers(&g_entities[blocker->client->ps.saberEntityNum], blocker, attacker, &tr);
	}
}

qboolean g_accurate_blocking(const gentity_t* blocker, const gentity_t* attacker, vec3_t hit_loc)
{
	//determines if self (who is blocking) is actively blocking (parrying)
	vec3_t p_angles;
	vec3_t p_right;
	vec3_t parrier_move;
	vec3_t hit_pos;
	vec3_t hit_flat; //flatten 2D version of the hitPos.
	const qboolean in_front_of_me = in_front(attacker->client->ps.origin, blocker->client->ps.origin, blocker->client->ps.viewangles, 0.0f);

	if (!(blocker->r.svFlags & SVF_BOT))
	{
		if (!(blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK))
		{
			return qfalse;
		}
	}

	if (!in_front_of_me)
	{
		//can't parry attacks to the rear.
		return qfalse;
	}
	if (PM_SaberInKnockaway(blocker->client->ps.saber_move))
	{
		//already in parry move, continue parrying anything that hits us as long as
		//the attacker is in the same general area that we're facing.
		return qtrue;
	}

	if (PM_KickingAnim(blocker->client->ps.legsAnim))
	{
		//can't parry in kick.
		return qfalse;
	}

	if (BG_SaberInNonIdleDamageMove(&blocker->client->ps, blocker->localAnimIndex)
		|| PM_SaberInBounce(blocker->client->ps.saber_move) || BG_InSlowBounce(&blocker->client->ps))
	{
		//can't parry if we're transitioning into a block from an attack state.
		return qfalse;
	}

	if (blocker->client->ps.pm_flags & PMF_DUCKED)
	{
		//can't parry while ducked or running
		return qfalse;
	}

	if (PM_InKnockDown(&blocker->client->ps))
	{
		//can't block while knocked down or getting up from knockdown, or we are staggered.
		return qfalse;
	}

	if (blocker->client->ps.ManualblockStartTime >= 3000) //3 sec
	{
		//cant perfect parry if your too slow
		return qfalse;
	}

	//set up flatten version of the location of the incoming attack in orientation
	//to the player.
	VectorSubtract(hit_loc, blocker->client->ps.origin, hit_pos);
	VectorSet(p_angles, 0, blocker->client->ps.viewangles[YAW], 0);
	AngleVectors(p_angles, NULL, p_right, NULL);
	hit_flat[0] = 0;
	hit_flat[1] = DotProduct(p_right, hit_pos);

	//just bump the hit pos down for the blocking since the average left/right slice happens at about origin +10
	hit_flat[2] = hit_pos[2] - 10;
	VectorNormalize(hit_flat);

	//set up the vector for the direction the player is trying to parry in.
	parrier_move[0] = 0;
	parrier_move[1] = blocker->client->pers.cmd.rightmove;
	parrier_move[2] = -blocker->client->pers.cmd.forwardmove;
	VectorNormalize(parrier_move);

	const float block_dot = DotProduct(hit_flat, parrier_move);

	if (block_dot >= 0.4f)
	{
		//player successfully blocked in the right direction to do a full parry.
		return qtrue;
	}
	//player didn't parry in the correct direction, do blockPoints punishment
	if (blocker->r.svFlags & SVF_BOT)
	{
		//bots just randomly parry to make up for them not intelligently parrying.
		if (BOT_PARRYRATE * botstates[blocker->s.number]->settings.skill > Q_irand(0, 999))
		{
			return qtrue;
		}
	}

	return qfalse;
}

void sab_beh_add_mishap_attacker(gentity_t* attacker, const gentity_t* blocker)
{
	if (attacker->client->ps.fd.blockPoints <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.fd.blockPoints = MISHAPLEVEL_NONE;
	}
	else if (attacker->client->ps.saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
	}
	else
	{
		//overflowing causes a full mishap.
		const int rand_num = Q_irand(0, 2);

		switch (rand_num)
		{
		case 0:
			if (blocker->r.svFlags & SVF_BOT) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);
					if (d_attackinfo.integer || g_DebugSaberCombat.integer && attacker->r.svFlags & SVF_BOT)
					{
						Com_Printf(S_COLOR_YELLOW"NPC Attacker staggering\n");
					}
				}
				else
				{
					sab_beh_saber_should_be_disarmed_attacker(attacker, blocker);
					if (d_attackinfo.integer || g_DebugSaberCombat.integer && attacker->r.svFlags & SVF_BOT)
					{
						Com_Printf(S_COLOR_RED"NPC Attacker lost his saber\n");
					}
				}
			}
			else
			{
				sab_beh_saber_should_be_disarmed_attacker(attacker, blocker);
				if (d_attackinfo.integer || g_DebugSaberCombat.integer && !(attacker->r.svFlags & SVF_BOT))
				{
					Com_Printf(S_COLOR_RED"Player Attacker lost his saber\n");
				}
			}
			break;
		case 1:
			sab_beh_animate_heavy_slow_bounce_attacker(attacker);
			if (d_attackinfo.integer || g_DebugSaberCombat.integer && !(attacker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_RED"Player Attacker staggering\n");
			}
			break;
		default:;
		}
	}
}

void sab_beh_add_mishap_Fake_attacker(gentity_t* attacker, const gentity_t* blocker)
{
	if (attacker->client->ps.fd.blockPoints <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.fd.blockPoints = MISHAPLEVEL_NONE;
	}
	else if (attacker->client->ps.saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
	}
	else
	{
		//overflowing causes a full mishap.
		const int rand_num = Q_irand(0, 2);

		switch (rand_num)
		{
		case 0:
			if (blocker->r.svFlags & SVF_BOT) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					sab_beh_saber_should_be_disarmed_attacker(attacker, blocker);
					if (d_attackinfo.integer || g_DebugSaberCombat.integer && attacker->r.svFlags & SVF_BOT)
					{
						Com_Printf(S_COLOR_RED"NPC Attacker lost his saber\n");
					}
				}
				else
				{
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);
					if (d_attackinfo.integer || g_DebugSaberCombat.integer && attacker->r.svFlags & SVF_BOT)
					{
						Com_Printf(S_COLOR_YELLOW"NPC Attacker staggering\n");
					}
				}
			}
			else
			{
				sab_beh_saber_should_be_disarmed_attacker(attacker, blocker);
				if (d_attackinfo.integer || g_DebugSaberCombat.integer && !(attacker->r.svFlags & SVF_BOT))
				{
					Com_Printf(S_COLOR_RED"Player Attacker lost his saber\n");
				}
			}
			break;
		case 1:
			sab_beh_animate_heavy_slow_bounce_attacker(attacker);
			if (d_attackinfo.integer || g_DebugSaberCombat.integer && !(attacker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_RED"Player Attacker staggering\n");
			}
			break;
		default:;
		}
	}
}

void sab_beh_add_mishap_blocker(gentity_t* blocker, const gentity_t* attacker)
{
	if (blocker->client->ps.fd.blockPoints <= MISHAPLEVEL_NONE)
	{
		blocker->client->ps.fd.blockPoints = MISHAPLEVEL_NONE;
	}
	else if (blocker->client->ps.saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
	}
	else
	{
		//overflowing causes a full mishap.
		const int rand_num = Q_irand(0, 2);

		switch (rand_num)
		{
		case 0:
			G_Stagger(blocker);
			if (d_blockinfo.integer || g_DebugSaberCombat.integer)
			{
				Com_Printf(S_COLOR_RED"blocker staggering\n");
			}
			break;
		case 1:
			if (blocker->r.svFlags & SVF_BOT) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					G_Stagger(blocker);
					if (d_blockinfo.integer || g_DebugSaberCombat.integer)
					{
						Com_Printf(S_COLOR_RED"NPC blocker staggering\n");
					}
				}
				else
				{
					sab_beh_saber_should_be_disarmed_blocker(blocker, attacker);
					wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
					if (d_blockinfo.integer || g_DebugSaberCombat.integer)
					{
						Com_Printf(S_COLOR_RED"NPC blocker lost his saber\n");
					}
				}
			}
			else
			{
				sab_beh_saber_should_be_disarmed_blocker(blocker, attacker);
				if (d_blockinfo.integer || g_DebugSaberCombat.integer)
				{
					Com_Printf(S_COLOR_RED"blocker lost his saber\n");
				}
			}
			break;
		default:;
		}
	}
}

////////Bounces//////////

void sab_beh_animate_heavy_slow_bounce_attacker(gentity_t* attacker)
{
	G_StaggerAttacker(attacker);
	attacker->client->ps.userInt3 |= 1 << FLAG_SLOWBOUNCE;
	attacker->client->ps.userInt3 |= 1 << FLAG_OLDSLOWBOUNCE;
}

void sab_beh_animate_small_bounce(gentity_t* attacker)
{
	if (attacker->r.svFlags & SVF_BOT) //NPC only
	{
		attacker->client->ps.userInt3 |= 1 << FLAG_SLOWBOUNCE;
		attacker->client->ps.userInt3 |= 1 << FLAG_OLDSLOWBOUNCE;
		G_BounceAttacker(attacker);
	}
	else
	{
		attacker->client->ps.userInt3 |= 1 << FLAG_SLOWBOUNCE;
		attacker->client->ps.saberBounceMove = LS_D1_BR + (saber_moveData[attacker->client->ps.saber_move].startQuad - Q_BR);
		attacker->client->ps.saberBlocked = BLOCKED_ATK_BOUNCE;
	}
}

void sab_beh_animate_heavy_slow_bounce_blocker(gentity_t* blocker, gentity_t* attacker)
{
	blocker->client->ps.userInt3 |= 1 << FLAG_SLOWBOUNCE;
	blocker->client->ps.userInt3 |= 1 << FLAG_OLDSLOWBOUNCE;

	G_AddEvent(blocker, Q_irand(EV_PUSHED1, EV_PUSHED3), 0);
	G_AddEvent(attacker, Q_irand(EV_DEFLECT1, EV_DEFLECT3), 0);

	blocker->client->ps.saberBounceMove = pm_broken_parry_for_attack(blocker->client->ps.saber_move);
	blocker->client->ps.saberBlocked = BLOCKED_PARRY_BROKEN;
}

void sab_beh_animate_slow_bounce_blocker(gentity_t* blocker)
{
	blocker->client->ps.userInt3 |= 1 << FLAG_SLOWBOUNCE;
	blocker->client->ps.userInt3 |= 1 << FLAG_OLDSLOWBOUNCE;

	G_AddEvent(blocker, Q_irand(EV_PUSHED1, EV_PUSHED3), 0);

	blocker->client->ps.saberBounceMove = PM_BrokenParryForParry(G_GetParryForBlock(blocker->client->ps.saberBlocked));
	blocker->client->ps.saberBlocked = BLOCKED_PARRY_BROKEN;
}

////////Bounces//////////

qboolean sab_beh_attack_blocked(gentity_t* attacker, gentity_t* blocker, const qboolean force_mishap)
{
	//if the attack is blocked -(Im the attacker)
	const qboolean m_blocking = blocker->client->ps.ManualBlockingFlags & 1 << PERFECTBLOCKING ? qtrue : qfalse;
	//perfect Blocking (Timed Block)

	if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_MAX)
	{
		//hard mishap.

		if (attacker->r.svFlags & SVF_BOT) //NPC only
		{
			if (!Q_irand(0, 4))
			{
				//20% chance
				sab_beh_add_mishap_attacker(attacker, blocker);
			}
			else
			{
				sab_beh_animate_heavy_slow_bounce_attacker(attacker);
			}
			if (d_attackinfo.integer || g_DebugSaberCombat.integer)
			{
				Com_Printf(S_COLOR_GREEN"Attacker npc is fatigued\n");
			}

			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_MIN;
		}
		else
		{
			if (d_attackinfo.integer || g_DebugSaberCombat.integer)
			{
				Com_Printf(S_COLOR_GREEN"Attacker player is fatigued\n");
			}
			sab_beh_add_mishap_attacker(attacker, blocker);
		}
		return qtrue;
	}
	if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_HUDFLASH)
	{
		//slow bounce
		if (!(attacker->r.svFlags & SVF_BOT))
		{
			sab_beh_animate_heavy_slow_bounce_attacker(attacker);
		}
		else
		{
			sab_beh_animate_small_bounce(attacker);
		}

		if (attacker->r.svFlags & SVF_BOT) //NPC only
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
		}

		if (d_attackinfo.integer || g_DebugSaberCombat.integer)
		{
			if (!(attacker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_GREEN"player attack stagger\n");
			}
			else
			{
				Com_Printf(S_COLOR_GREEN"npc attack stagger\n");
			}
		}
		return qtrue;
	}
	if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_LIGHT)
	{
		//slow bounce
		sab_beh_animate_small_bounce(attacker);

		if (d_attackinfo.integer || g_DebugSaberCombat.integer)
		{
			if (!(attacker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_GREEN"player light blocked bounce\n");
			}
			else
			{
				Com_Printf(S_COLOR_GREEN"npc light blocked bounce\n");
			}
		}
		return qtrue;
	}
	if (force_mishap)
	{
		//two attacking sabers bouncing off each other
		sab_beh_animate_small_bounce(attacker);
		sab_beh_animate_small_bounce(blocker);

		if (d_attackinfo.integer || g_DebugSaberCombat.integer)
		{
			if (!(attacker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_GREEN"player two attacking sabers bouncing off each other\n");
			}
			else
			{
				Com_Printf(S_COLOR_GREEN"npc two attacking sabers bouncing off each other\n");
			}
		}
		return qtrue;
	}
	if (!m_blocking)
	{
		if (d_attackinfo.integer || g_DebugSaberCombat.integer)
		{
			if (!(attacker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_GREEN"player blocked bounce\n");
			}
			else
			{
				Com_Printf(S_COLOR_GREEN"npc blocked bounce\n");
			}
		}
		sab_beh_animate_small_bounce(attacker);
	}
	return qtrue;
}

void sab_beh_add_balance(const gentity_t* self, int amount)
{
	if (!walk_check(self))
	{
		//running or moving very fast, can't balance as well
		if (amount > 0)
		{
			amount *= 2;
		}
		else
		{
			amount = amount * .5f;
		}
	}

	self->client->ps.saberFatigueChainCount += amount;

	if (self->client->ps.saberFatigueChainCount < MISHAPLEVEL_NONE)
	{
		self->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
	}
	else if (self->client->ps.saberFatigueChainCount > MISHAPLEVEL_OVERLOAD)
	{
		self->client->ps.saberFatigueChainCount = MISHAPLEVEL_MAX;
	}
}

//////////Actions////////////////

/////////Functions//////////////

qboolean sab_beh_attack_vs_attack(gentity_t* attacker, gentity_t* blocker)
{
	//set the saber behavior for two attacking blades hitting each other
	const qboolean atkfake = attacker->client->ps.userInt3 & 1 << FLAG_ATTACKFAKE ? qtrue : qfalse;
	const qboolean otherfake = blocker->client->ps.userInt3 & 1 << FLAG_ATTACKFAKE ? qtrue : qfalse;

	if (atkfake && !otherfake)
	{
		//self is solo faking
		//set self
		sab_beh_add_balance(attacker, MPCOST_PARRIED);
		//set otherOwner

		if (WP_SabersCheckLock(attacker, blocker))
		{
			attacker->client->ps.userInt3 |= 1 << FLAG_SABERLOCK_ATTACKER;
			attacker->client->ps.saberBlocked = BLOCKED_NONE;
			blocker->client->ps.saberBlocked = BLOCKED_NONE;
		}
		sab_beh_add_balance(blocker, -MPCOST_PARRIED);
	}
	else if (!atkfake && otherfake)
	{
		//only otherOwner is faking
		//set self
		if (WP_SabersCheckLock(blocker, attacker))
		{
			attacker->client->ps.saberBlocked = BLOCKED_NONE;
			blocker->client->ps.userInt3 |= 1 << FLAG_SABERLOCK_ATTACKER;
			blocker->client->ps.saberBlocked = BLOCKED_NONE;
		}
		sab_beh_add_balance(attacker, -MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, MPCOST_PARRIED);
	}
	else if (atkfake && otherfake)
	{
		//both faking
		//set self
		if (WP_SabersCheckLock(attacker, blocker))
		{
			attacker->client->ps.userInt3 |= 1 << FLAG_SABERLOCK_ATTACKER;
			attacker->client->ps.saberBlocked = BLOCKED_NONE;

			blocker->client->ps.userInt3 |= 1 << FLAG_SABERLOCK_ATTACKER;
			blocker->client->ps.saberBlocked = BLOCKED_NONE;
		}
		sab_beh_add_balance(attacker, MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, MPCOST_PARRIED);
	}
	else if (PM_SaberInKata(attacker->client->ps.saber_move))
	{
		sab_beh_add_balance(attacker, MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, -MPCOST_PARRIED);

		if (blocker->client->ps.fd.blockPoints < BLOCKPOINTS_TEN)
		{
			//Low points = bad blocks
			sab_beh_saber_should_be_disarmed_blocker(blocker, attacker);
			wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
		}
		else
		{
			//Low points = bad blocks
			G_Stagger(blocker);
			PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
		}
	}
	else if (PM_SaberInKata(blocker->client->ps.saber_move))
	{
		sab_beh_add_balance(attacker, -MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, MPCOST_PARRIED);

		if (attacker->client->ps.fd.blockPoints < BLOCKPOINTS_TEN)
		{
			//Low points = bad blocks
			sab_beh_saber_should_be_disarmed_attacker(attacker, blocker);
			wp_block_points_regenerate_over_ride(attacker, BLOCKPOINTS_FATIGUE);
		}
		else
		{
			//Low points = bad blocks
			G_Stagger(attacker);
			PM_AddBlockFatigue(&attacker->client->ps, BLOCKPOINTS_TEN);
		}
	}
	else
	{
		//either both are faking or neither is faking.  Either way, it's cancelled out
		//set self
		sab_beh_add_balance(attacker, MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, MPCOST_PARRIED);

		sab_beh_attack_blocked(attacker, blocker, qtrue);

		sab_beh_attack_blocked(blocker, attacker, qtrue);
	}
	return qtrue;
}

qboolean sab_beh_attack_vs_block(gentity_t* attacker, gentity_t* blocker, const int saber_num, const int blade_num, vec3_t hit_loc)
{
	//if the attack is blocked -(Im the attacker)
	const qboolean accurate_parry = g_accurate_blocking(blocker, attacker, hit_loc); // Perfect Normal Blocking

	const qboolean blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;	//Normal Blocking (just holding block button)
	const qboolean m_blocking = blocker->client->ps.ManualBlockingFlags & 1 << PERFECTBLOCKING ? qtrue : qfalse; //perfect Blocking (Timed Block)
	const qboolean active_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse; //Active Blocking (Holding Block button + Attack button)
	const qboolean npc_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_NPCBLOCKING ? qtrue : qfalse; //(Npc Blocking function)

	const qboolean atkfake = attacker->client->ps.userInt3 & 1 << FLAG_ATTACKFAKE ? qtrue : qfalse;

	if (pm_saber_innonblockable_attack(attacker->client->ps.torsoAnim))
	{
		//perfect Blocking
		if (m_blocking) // A perfectly timed block
		{
			sab_beh_saber_should_be_disarmed_attacker(attacker, blocker);
			//just so attacker knows that he was blocked
			attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
			//since it was parried, take away any damage done
			wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
			PM_AddBlockFatigue(&attacker->client->ps, BLOCKPOINTS_TEN); //BP Punish Attacker
		}
		else
		{
			//This must be Unblockable
			if (d_attackinfo.integer || g_DebugSaberCombat.integer)
			{
				Com_Printf(S_COLOR_MAGENTA"Attacker must be Unblockable\n");
			}
			attacker->client->ps.saberEventFlags &= ~SEF_BLOCKED;
		}
	}
	else if (BG_SaberInNonIdleDamageMove(&blocker->client->ps, blocker->localAnimIndex))
	{
		//and blocker is attacking
		if ((d_attackinfo.integer || g_DebugSaberCombat.integer))
		{
			Com_Printf(S_COLOR_YELLOW"Both Attacker and Blocker are now attacking\n");
		}

		sab_beh_attack_vs_attack(blocker, attacker);
	}
	else if (PM_SuperBreakWinAnim(attacker->client->ps.torsoAnim))
	{
		//attacker was attempting a superbreak and he hit someone who could block the move, rail him for screwing up.
		sab_beh_add_balance(attacker, MPCOST_PARRIED);

		sab_beh_animate_heavy_slow_bounce_attacker(attacker);

		sab_beh_add_balance(blocker, -MPCOST_PARRIED);
		if ((d_attackinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
		{
			Com_Printf(S_COLOR_YELLOW"Attacker Super break win / fail\n");
		}
	}
	else if (atkfake)
	{
		//attacker faked but it was blocked here
		if (m_blocking || npc_blocking)
		{
			//defender parried the attack fake.
			sab_beh_add_balance(attacker, MPCOST_PARRIED_ATTACKFAKE);

			if (npc_blocking)
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_BLOCKED;
			}
			else
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_PARRIED;
			}

			sab_beh_add_balance(blocker, MPCOST_PARRYING_ATTACKFAKE);
			sab_beh_add_mishap_Fake_attacker(attacker, blocker);

			if ((d_attackinfo.integer || g_DebugSaberCombat.integer))
			{
				Com_Printf(S_COLOR_YELLOW"Attackers Attack Fake was P-Blocked\n");
			}
		}
		else
		{
			//otherwise, the defender stands a good chance of having his defensive broken.
			sab_beh_add_balance(attacker, -MPCOST_PARRIED);

			if (WP_SabersCheckLock(attacker, blocker))
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_SABERLOCK_ATTACKER;
				attacker->client->ps.saberBlocked = BLOCKED_NONE;
				blocker->client->ps.saberBlocked = BLOCKED_NONE;
			}

			if ((d_attackinfo.integer || g_DebugSaberCombat.integer))
			{
				Com_Printf(S_COLOR_YELLOW"Attacker forced a saberlock\n");
			}
		}
	}
	else
	{
		//standard attack.
		if (accurate_parry || blocking || m_blocking || active_blocking || npc_blocking) // All types of active blocking
		{
			if (m_blocking || active_blocking || npc_blocking)
			{
				if (npc_blocking && blocker->client->ps.fd.blockPoints >= BLOCKPOINTS_MISSILE
					&& attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_HUDFLASH
					&& !Q_irand(0, 4))
				{
					//20% chance
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);
					attacker->client->ps.userInt3 |= 1 << FLAG_MBLOCKBOUNCE;
				}
				else
				{
					attacker->client->ps.userInt3 |= 1 << FLAG_BLOCKED;
				}

				if (!(attacker->r.svFlags & SVF_BOT))
				{
					CGCam_BlockShakeMP(attacker->s.origin, attacker, 0.45f, 100);
				}
			}
			else
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_PARRIED;
			}

			if (!m_blocking)
			{
				sab_beh_attack_blocked(attacker, blocker, qfalse);
			}

			sab_beh_add_balance(blocker, -MPCOST_PARRIED);

			if ((d_attackinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_YELLOW"Attackers Attack was Blocked\n");
			}
		}
		else
		{
			//Backup in case i missed some

			if (!m_blocking)
			{
				if (pm_saber_innonblockable_attack(blocker->client->ps.torsoAnim))
				{
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);

					sab_beh_add_balance(blocker, -MPCOST_PARRIED);
					if ((d_attackinfo.integer || g_DebugSaberCombat.integer))
					{
						Com_Printf(S_COLOR_ORANGE"Attack an Unblockable attack\n");
					}
				}
				else
				{
					sab_beh_attack_blocked(attacker, blocker, qtrue);

					G_Stagger(blocker);

					if ((d_attackinfo.integer || g_DebugSaberCombat.integer))
					{
						Com_Printf(S_COLOR_ORANGE"Attacker All the rest of the types of contact\n");
					}
				}
			}
		}
	}
	return qtrue;
}

qboolean sab_beh_block_vs_attack(gentity_t* blocker, gentity_t* attacker, const int saber_num, const int blade_num, vec3_t hit_loc)
{
	//-(Im the blocker)
	const qboolean accurate_parry = g_accurate_blocking(blocker, attacker, hit_loc); // Perfect Normal Blocking
	const qboolean blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;	//Normal Blocking
	const qboolean m_blocking = blocker->client->ps.ManualBlockingFlags & 1 << PERFECTBLOCKING ? qtrue : qfalse;	//perfect Blocking
	const qboolean active_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse; //Active Blocking
	const qboolean npc_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_NPCBLOCKING ? qtrue : qfalse;//Active NPC Blocking

	if (!pm_saber_innonblockable_attack(attacker->client->ps.torsoAnim))
	{
		if (blocker->client->ps.fd.blockPoints <= BLOCKPOINTS_FATIGUE) // blocker has less than 20BP
		{
			if (blocker->client->ps.fd.blockPoints <= BLOCKPOINTS_TEN) // blocker has less than 10BP
			{
				//Low points = bad blocks
				if (blocker->r.svFlags & SVF_BOT) //NPC only
				{
					sab_beh_add_mishap_blocker(blocker, attacker);
				}
				else
				{
					sab_beh_saber_should_be_disarmed_blocker(blocker, attacker);
				}

				if (attacker->r.svFlags & SVF_BOT) //NPC only
				{
					wp_block_points_regenerate(attacker, BLOCKPOINTS_FATIGUE);
				}
				else
				{
					if (!blocker->client->ps.saberInFlight)
					{
						wp_block_points_regenerate(blocker, BLOCKPOINTS_FATIGUE);
					}
				}

				if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
				{
					Com_Printf(S_COLOR_CYAN"Blocker was disarmed with very low bp, recharge bp 20bp\n");
				}

				//just so blocker knows that he has parried the attacker
				blocker->client->ps.saberEventFlags |= SEF_PARRIED;
				//just so attacker knows that he was blocked
				attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
				//since it was parried, take away any damage done
				wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
			}
			else
			{
				//Low points = bad blocks
				g_fatigue_bp_knockaway(blocker);

				PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_DANGER);

				if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
				{
					Com_Printf(S_COLOR_CYAN"Blocker stagger drain 4 bp\n");
				}

				//just so blocker knows that he has parried the attacker
				blocker->client->ps.saberEventFlags |= SEF_PARRIED;
				//just so attacker knows that he was blocked
				attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
				//since it was parried, take away any damage done
				wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
			}
		}
		else
		{
			//just block it //jacesolaris
			if (active_blocking) //Holding Block Button + attack button
			{
				//perfect Blocking
				if (m_blocking) // A perfectly timed block
				{
					WP_SaberMBlockDirection(blocker, hit_loc, qfalse);

					if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_THIRTEEN)
					{
						sab_beh_add_mishap_attacker(attacker, blocker);
					}
					else
					{
						sab_beh_animate_heavy_slow_bounce_attacker(attacker);
						attacker->client->ps.userInt3 |= 1 << FLAG_MBLOCKBOUNCE;
					}

					blocker->client->ps.userInt3 |= 1 << FLAG_PERFECTBLOCK;

					if (attacker->r.svFlags & SVF_BOT) //NPC only
					{
						g_do_m_block_response(attacker);
					}

					if (!(blocker->r.svFlags & SVF_BOT))
					{
						CGCam_BlockShakeMP(blocker->s.origin, blocker, 0.45f, 100);
					}
					G_Sound(blocker, CHAN_AUTO, G_SoundIndex(va("sound/weapons/saber/saber_perfectblock%d.mp3", Q_irand(1, 3))));

					if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
					{
						Com_Printf(S_COLOR_CYAN"Blocker Perfect blocked reward 20\n");
					}

					//just so blocker knows that he has parried the attacker
					blocker->client->ps.saberEventFlags |= SEF_PARRIED;
					//just so attacker knows that he was blocked
					attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
					//since it was parried, take away any damage done
					wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);

					wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE); //BP Reward blocker
					blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE; //SAC Reward blocker
					PM_AddBlockFatigue(&attacker->client->ps, BLOCKPOINTS_TEN); //BP Punish Attacker
				}
				else
				{
					//Spamming block + attack buttons
					if (blocker->client->ps.fd.blockPoints <= BLOCKPOINTS_HALF)
					{
						WP_SaberFatiguedParryDirection(blocker, hit_loc, qfalse);
					}
					else
					{
						if (attacker->client->ps.fd.saber_anim_level == SS_DESANN || attacker->client->ps.fd.saber_anim_level == SS_STRONG)
						{
							WP_SaberFatiguedParryDirection(blocker, hit_loc, qfalse);
						}
						else
						{
							WP_SaberBlockNonRandom(blocker, hit_loc, qfalse);
						}
					}

					if (attacker->r.svFlags & SVF_BOT) //NPC only
					{
						PM_AddBlockFatigue(&attacker->client->ps, BLOCKPOINTS_THREE);
					}

					PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_FIVE);

					if (!(blocker->r.svFlags & SVF_BOT))
					{
						CGCam_BlockShakeMP(blocker->s.origin, blocker, 0.45f, 100);
					}

					if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
					{
						Com_Printf(S_COLOR_CYAN"Blocker Spamming block + attack cost 5\n");
					}

					//just so blocker knows that he has parried the attacker
					blocker->client->ps.saberEventFlags |= SEF_PARRIED;
					//just so attacker knows that he was blocked
					attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
					//since it was parried, take away any damage done
					wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
				}
			}
			else if (blocking && !active_blocking) //Holding block button only (spamming block)
			{
				if (blocker->client->ps.fd.blockPoints <= BLOCKPOINTS_HALF)
				{
					WP_SaberFatiguedParryDirection(blocker, hit_loc, qfalse);
				}
				else
				{
					if (attacker->client->ps.fd.saber_anim_level == SS_DESANN || attacker->client->ps.fd.saber_anim_level == SS_STRONG)
					{
						WP_SaberFatiguedParryDirection(blocker, hit_loc, qfalse);
					}
					else
					{
						WP_SaberBouncedSaberDirection(blocker, hit_loc, qfalse);
					}
				}

				if (blocker->r.svFlags & SVF_BOT) //NPC only
				{
					//
				}
				else
				{
					PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
				}

				if (attacker->r.svFlags & SVF_BOT) //NPC only
				{
					wp_block_points_regenerate_over_ride(attacker, BLOCKPOINTS_DANGER);
				}
				if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
				{
					Com_Printf(S_COLOR_CYAN"Blocker Holding block button only (spamming block) cost 10\n");
				}

				if (!(blocker->r.svFlags & SVF_BOT))
				{
					CGCam_BlockShakeMP(blocker->s.origin, blocker, 0.45f, 100);
				}

				//just so blocker knows that he has parried the attacker
				blocker->client->ps.saberEventFlags |= SEF_PARRIED;
				//just so attacker knows that he was blocked
				attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
				//since it was parried, take away any damage done
				wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
			}
			else if ((accurate_parry || npc_blocking)) //Other types and npc,s
			{
				if (attacker->client->ps.fd.saber_anim_level == SS_DESANN || attacker->client->ps.fd.saber_anim_level == SS_STRONG)
				{
					WP_SaberFatiguedParryDirection(blocker, hit_loc, qfalse);
				}
				else
				{
					if (blocker->client->ps.fd.blockPoints <= BLOCKPOINTS_MISSILE)
					{
						if (blocker->client->ps.fd.blockPoints <= BLOCKPOINTS_FOURTY)
						{
							WP_SaberFatiguedParryDirection(blocker, hit_loc, qfalse);

							if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
							{
								Com_Printf(S_COLOR_CYAN"NPC Fatigued Parry\n");
							}

							PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_FAIL);
						}
						else
						{
							WP_SaberBlockNonRandom(blocker, hit_loc, qfalse);

							if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
							{
								Com_Printf(S_COLOR_CYAN"NPC normal Parry\n");
							}

							PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_THREE);
						}
					}
					else
					{
						WP_SaberMBlockDirection(blocker, hit_loc, qfalse);

						if (blocker->r.svFlags & SVF_BOT) //NPC only
						{
							g_do_m_block_response(blocker);
						}

						if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
						{
							Com_Printf(S_COLOR_CYAN"NPC good Parry\n");
						}

						PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_THREE);
					}
				}

				//just so blocker knows that he has parried the attacker
				blocker->client->ps.saberEventFlags |= SEF_PARRIED;
				//just so attacker knows that he was blocked
				attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
				//since it was parried, take away any damage done
				wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);

				G_Sound(blocker, CHAN_AUTO, G_SoundIndex(va("sound/weapons/saber/saber_goodparry%d.mp3", Q_irand(1, 3))));
			}
			else
			{
				sab_beh_add_mishap_blocker(blocker, attacker);

				if (blocker->r.svFlags & SVF_BOT) //NPC only
				{
					//
				}
				else
				{
					PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
				}
				if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
				{
					Com_Printf(S_COLOR_CYAN"Blocker Not holding block drain 10\n");
				}
			}
		}
	}
	else
	{
		//perfect Blocking
		if (m_blocking) // A perfectly timed block
		{
			if (!(blocker->r.svFlags & SVF_BOT))
			{
				CGCam_BlockShakeMP(blocker->s.origin, blocker, 0.45f, 100);
			}

			blocker->client->ps.userInt3 |= 1 << FLAG_PERFECTBLOCK;

			G_Sound(blocker, CHAN_AUTO,
				G_SoundIndex(va("sound/weapons/saber/saber_perfectblock%d.mp3", Q_irand(1, 3))));

			if ((d_blockinfo.integer || g_DebugSaberCombat.integer) && !(blocker->r.svFlags & SVF_BOT))
			{
				Com_Printf(S_COLOR_MAGENTA"Blocker Perfect blocked an Unblockable attack reward 20\n");
			}

			//just so blocker knows that he has parried the attacker
			blocker->client->ps.saberEventFlags |= SEF_PARRIED;

			wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE); //BP Reward blocker
			blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE; //SAC Reward blocker
		}
		else
		{
			//This must be Unblockable
			if (blocker->client->ps.fd.blockPoints < BLOCKPOINTS_TEN)
			{
				//Low points = bad blocks
				sab_beh_saber_should_be_disarmed_blocker(blocker, attacker);
				wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
			}
			else
			{
				//Low points = bad blocks
				g_fatigue_bp_knockaway(blocker);
				PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
			}
			if (d_blockinfo.integer || g_DebugSaberCombat.integer)
			{
				Com_Printf(S_COLOR_MAGENTA"Blocker can not block Unblockable\n");
			}
			blocker->client->ps.saberEventFlags &= ~SEF_PARRIED;
		}
	}
	return qtrue;
}

/////////Functions//////////////
//
/////////////////////// 20233 new build ////////////////////////////////