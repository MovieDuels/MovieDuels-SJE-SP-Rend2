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
///						      System designed by Serenity and modded by JaceSolaris. (c) 2019 SJE   		                    ///
///								    https://www.moddb.com/mods/serenityjediengine-20											///
///																																///
/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ///

#include "g_local.h"
#include "anims.h"
#include "b_local.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "../qcommon/tri_coll_test.h"
#include "../cgame/cg_local.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean pm_saber_innonblockable_attack(int anim);
extern void sab_beh_saber_should_be_disarmed_attacker(gentity_t* attacker, int saber_num);
extern void wp_saber_clear_damage_for_ent_num(gentity_t* attacker, int entityNum, int saber_num, int blade_num);
extern cvar_t* g_SerenityJediEngineMode;
extern void PM_AddBlockFatigue(playerState_t* ps, int fatigue);
extern void PM_AddFatigue(playerState_t* ps, int fatigue);
extern qboolean BG_SaberInNonIdleDamageMove(const playerState_t* ps);
extern qboolean PM_SuperBreakWinAnim(int anim);
extern void sab_beh_add_balance(const gentity_t* self, int amount);
extern void sab_beh_animate_heavy_slow_bounce_attacker(gentity_t* attacker);
extern qboolean WP_SabersCheckLock(gentity_t* ent1, gentity_t* ent2);
extern cvar_t* g_debugFatigueBars;
extern void SabBeh_AnimateSmallBounce(const gentity_t* self);
extern qboolean PM_InForceGetUp(const playerState_t* ps);
extern qboolean PM_InGetUp(const playerState_t* ps);
extern qboolean PM_SaberInMassiveBounce(int move);
extern void G_StartStasisEffect_FORCE_LEVEL_1(const gentity_t* ent, int me_flags = 0, int length = 1000, float time_scale = 0.0f, int spin_time = 0);
extern cvar_t* d_slowmoaction;
extern void wp_block_points_regenerate_over_ride(const gentity_t* self, int override_amt);
extern void WP_ForcePowerRegenerate(const gentity_t* self, int override_amt);
extern void G_Stagger(gentity_t* hit_ent);
extern void SabBeh_SaberShouldBeDisarmedBlocker(gentity_t* blocker, int saber_num);
extern void WP_BlockPointsRegenerate(const gentity_t* self, int override_amt);
extern qboolean WP_SaberMBlock(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern qboolean WP_SaberFatiguedParry(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern qboolean WP_SaberParry(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num);
extern qboolean WP_SaberBlockedBounceBlock(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern qboolean WP_SaberNPCParry(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern qboolean WP_SaberNPCFatiguedParry(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern qboolean WP_SaberNPCMBlock(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern qboolean WP_SaberSlowBounceBlock(gentity_t* victim, gentity_t* attacker, int saber_num, int blade_num, vec3_t hit_loc);
extern cvar_t* g_saberAutoBlocking;
extern void g_do_m_block_response(const gentity_t* speaker_npc_self);
extern qboolean PM_SaberInKata(saber_moveName_t saber_move);
extern qboolean g_accurate_blocking(const gentity_t* blocker, const gentity_t* attacker, vec3_t hit_loc);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void g_fatigue_bp_knockaway(gentity_t* blocker)
{
	if (PM_InGetUp(&blocker->client->ps) || PM_InForceGetUp(&blocker->client->ps))
	{
		return;
	}

	const int anim_choice = irand(0, 5);

	if (blocker->client->ps.saberAnimLevel == SS_DUAL)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K6_S6_T_, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K6_S6_TR, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K6_S6_TL, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K6_S6_BL, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K6_S6_B_, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K6_S6_BR, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (blocker->client->ps.saberAnimLevel == SS_STAFF)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K7_S7_T_, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K7_S7_TR, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K7_S7_TL, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K7_S7_BL, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K7_S7_B_, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K7_S7_BR, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K1_S1_T_, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K1_S1_TR, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K1_S1_TL, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K1_S1_BL, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K1_S1_B_, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(blocker, SETANIM_TORSO, BOTH_K1_S1_BR, SETANIM_AFLAG_PACE);
			break;
		}
	}

	if (PM_SaberInMassiveBounce(blocker->client->ps.torsoAnim))
	{
		blocker->client->ps.saber_move = LS_NONE;
		blocker->client->ps.saberBlocked = BLOCKED_NONE;
		blocker->client->ps.weaponTime = blocker->client->ps.torsoAnimTimer;
		blocker->client->MassiveBounceAnimTime = blocker->client->ps.torsoAnimTimer + level.time;
	}
	else
	{
		blocker->client->ps.saber_move = LS_READY;
	}
}

static void g_bounce_attacker(gentity_t* atk)
{
	if (PM_InGetUp(&atk->client->ps) || PM_InForceGetUp(&atk->client->ps))
	{
		return;
	}

	const int anim_choice = irand(0, 6);

	if (atk->client->ps.saberAnimLevel == SS_DUAL)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B6__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (atk->client->ps.saberAnimLevel == SS_STAFF)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B7__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (atk->client->ps.saberAnimLevel == SS_FAST)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B1__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (atk->client->ps.saberAnimLevel == SS_MEDIUM)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B2__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (atk->client->ps.saberAnimLevel == SS_STRONG)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B3__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (atk->client->ps.saberAnimLevel == SS_DESANN)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B4__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}
	else if (atk->client->ps.saberAnimLevel == SS_TAVION)
	{
		switch (anim_choice)
		{
		default:
		case 0:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5_BL___, SETANIM_AFLAG_PACE);
			break;
		case 1:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5_BR___, SETANIM_AFLAG_PACE);
			break;
		case 2:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5_TL___, SETANIM_AFLAG_PACE);
			break;
		case 3:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5_TR___, SETANIM_AFLAG_PACE);
			break;
		case 4:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5_T____, SETANIM_AFLAG_PACE);
			break;
		case 5:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5__L___, SETANIM_AFLAG_PACE);
			break;
		case 6:
			NPC_SetAnim(atk, SETANIM_TORSO, BOTH_B5__R___, SETANIM_AFLAG_PACE);
			break;
		}
	}

	if (PM_SaberInMassiveBounce(atk->client->ps.torsoAnim))
	{
		atk->client->ps.saber_move = LS_NONE;
		atk->client->ps.saberBlocked = BLOCKED_NONE;
		atk->client->ps.weaponTime = atk->client->ps.torsoAnimTimer;
		atk->client->MassiveBounceAnimTime = atk->client->ps.torsoAnimTimer + level.time;
	}
	else
	{
		atk->client->ps.saber_move = LS_READY;
	}
}

static void sab_beh_animate_attack_bounce(gentity_t* attacker)
{
	if (!attacker->client->ps.saberInFlight)
	{
		g_bounce_attacker(attacker);
	}
}

static void sab_beh_add_mishap_attacker(gentity_t* attacker, const int saber_num)
{
	if (attacker->client->ps.blockPoints <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.blockPoints = MISHAPLEVEL_NONE;
	}
	else if (attacker->client->ps.forcePower <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.forcePower = MISHAPLEVEL_NONE;
	}
	else if (attacker->client->ps.saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		if (g_debugFatigueBars->integer)
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_MIN;
		}
		else
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
		}
	}
	else
	{
		//overflowing causes a full mishap.
		const int rand_num = Q_irand(0, 2);

		switch (rand_num)
		{
		case 0:
			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_YELLOW"Attacker staggering\n");
			}
			sab_beh_animate_heavy_slow_bounce_attacker(attacker);
			break;
		case 1:
			if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);
					if (d_attackinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_YELLOW"Attacker staggering\n");
					}
				}
				else
				{
					sab_beh_saber_should_be_disarmed_attacker(attacker, saber_num);
					if (d_attackinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_RED"NPC Attacker lost his saber\n");
					}
				}
			}
			else
			{
				sab_beh_saber_should_be_disarmed_attacker(attacker, saber_num);
				if (d_attackinfo->integer || g_DebugSaberCombat->integer)
				{
					gi.Printf(S_COLOR_RED"player Attacker lost his saber\n");
				}
			}
			break;
		default:;
		}
	}
}

static void sab_beh_add_mishap_Fake_attacker(gentity_t* attacker, const gentity_t* blocker, const int saber_num)
{
	if (attacker->client->ps.blockPoints <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.blockPoints = MISHAPLEVEL_NONE;
	}
	else if (attacker->client->ps.forcePower <= MISHAPLEVEL_NONE)
	{
		attacker->client->ps.forcePower = MISHAPLEVEL_NONE;
	}
	else if (attacker->client->ps.saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		if (g_debugFatigueBars->integer)
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_MIN;
		}
		else
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
		}
	}
	else
	{
		//overflowing causes a full mishap.
		const int rand_num = Q_irand(0, 2);

		switch (rand_num)
		{
		case 0:
			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_YELLOW"Attacker staggering\n");
			}
			sab_beh_animate_heavy_slow_bounce_attacker(attacker);
			break;
		case 1:
			if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);
					if (d_attackinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_YELLOW"Attacker staggering\n");
					}
				}
				else
				{
					sab_beh_saber_should_be_disarmed_attacker(attacker, saber_num);
					if (d_attackinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_RED"NPC Attacker lost his saber\n");
					}
				}
			}
			else
			{
				sab_beh_saber_should_be_disarmed_attacker(attacker, saber_num);
				if (d_attackinfo->integer || g_DebugSaberCombat->integer)
				{
					gi.Printf(S_COLOR_RED"player Attacker lost his saber\n");
				}
			}
			break;
		default:;
		}
	}
}

static qboolean sab_beh_attack_blocked(gentity_t* attacker, gentity_t* blocker, const int saber_num,
	const qboolean force_mishap)
{
	//JaceSolaris
	//if the attack is blocked -(Im the attacker)
	const qboolean m_blocking = blocker->client->ps.ManualBlockingFlags & 1 << PERFECTBLOCKING ? qtrue : qfalse;
	//perfect Blocking (Timed Block)

	if (!attacker->client->ps.saberInFlight)
	{
		if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_MAX)
		{
			//hard mishap.

			if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					sab_beh_add_mishap_attacker(attacker, saber_num);
				}
				else
				{
					sab_beh_animate_heavy_slow_bounce_attacker(attacker);
				}

				attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_MIN;

				if (d_attackinfo->integer || g_DebugSaberCombat->integer)
				{
					gi.Printf(S_COLOR_GREEN"Attacker Advanced npc attacker is fatigued\n");
				}
			}
			else
			{
				if (d_attackinfo->integer || g_DebugSaberCombat->integer)
				{
					gi.Printf(S_COLOR_GREEN"Attacker Advanced player attacker is fatigued\n");
				}
				sab_beh_add_mishap_attacker(attacker, saber_num);
			}
			return qtrue;
		}
		if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_HUDFLASH)
		{
			//slow bounce
			if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
			{
				SabBeh_AnimateSmallBounce(attacker);
				attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
			}
			else
			{
				sab_beh_animate_heavy_slow_bounce_attacker(attacker);
			}

			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				if (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker))
				{
					gi.Printf(S_COLOR_GREEN"player attack stagger\n");
				}
				else
				{
					gi.Printf(S_COLOR_GREEN"npc attack stagger\n");
				}
			}
			return qtrue;
		}
		if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_LIGHT)
		{
			//slow bounce
			SabBeh_AnimateSmallBounce(attacker);

			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				if (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker))
				{
					gi.Printf(S_COLOR_GREEN"player light blocked bounce\n");
				}
				else
				{
					gi.Printf(S_COLOR_GREEN"npc light blocked bounce\n");
				}
			}
			return qtrue;
		}
		if (force_mishap)
		{
			//two attacking sabers bouncing off each other
			if (!attacker->client->ps.saberInFlight)
			{
				sab_beh_animate_attack_bounce(attacker);
			}
			sab_beh_animate_attack_bounce(blocker);

			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				if (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker))
				{
					gi.Printf(S_COLOR_GREEN"player two attacking sabers bouncing off each other\n");
				}
				else
				{
					gi.Printf(S_COLOR_GREEN"npc two attacking sabers bouncing off each other\n");
				}
			}
			return qtrue;
		}
		if (!m_blocking)
		{
			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				if (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker))
				{
					gi.Printf(S_COLOR_GREEN"player blocked bounce\n");
				}
				else
				{
					gi.Printf(S_COLOR_GREEN"npc blocked bounce\n");
				}
			}
		}
	}
	return qtrue;
}

static void sab_beh_add_mishap_blocker(gentity_t* blocker, const int saber_num)
{
	if (blocker->client->ps.blockPoints <= MISHAPLEVEL_NONE)
	{
		blocker->client->ps.blockPoints = MISHAPLEVEL_NONE;
	}
	else if (blocker->client->ps.forcePower <= MISHAPLEVEL_NONE)
	{
		blocker->client->ps.forcePower = MISHAPLEVEL_NONE;
	}
	else if (blocker->client->ps.saberFatigueChainCount <= MISHAPLEVEL_NONE)
	{
		if (g_debugFatigueBars->integer)
		{
			blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_MIN;
		}
		else
		{
			blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE;
		}
	}
	else
	{
		//overflowing causes a full mishap.
		const int rand_num = Q_irand(0, 2);

		switch (rand_num)
		{
		case 0:
			G_Stagger(blocker);
			if (d_blockinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_YELLOW"blocker staggering\n");
			}
			break;
		case 1:
			if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
			{
				if (!Q_irand(0, 4))
				{
					//20% chance
					G_Stagger(blocker);
					if (d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_YELLOW"NPC blocker staggering\n");
					}
				}
				else
				{
					SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
					wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
					if (d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_RED"NPC blocker lost his saber\n");
					}
				}
			}
			else
			{
				SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
				if (d_blockinfo->integer || g_DebugSaberCombat->integer)
				{
					gi.Printf(S_COLOR_RED"Player blocker lost his saber\n");
				}
			}
			break;
		default:;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static qboolean sab_beh_attack_vs_attack(gentity_t* attacker, gentity_t* blocker, const int saber_num)
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
	else if (PM_SaberInKata(static_cast<saber_moveName_t>(attacker->client->ps.saber_move)))
	{
		sab_beh_add_balance(attacker, MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, -MPCOST_PARRIED);

		if (g_SerenityJediEngineMode->integer == 2)
		{
			if (blocker->client->ps.blockPoints < BLOCKPOINTS_TEN)
			{
				//Low points = bad blocks
				SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
				wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
			}
			else
			{
				//Low points = bad blocks
				G_Stagger(blocker);
				PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
			}
		}
		else
		{
			if (blocker->client->ps.forcePower < BLOCKPOINTS_TEN)
			{
				//Low points = bad blocks
				SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
				WP_ForcePowerRegenerate(blocker, BLOCKPOINTS_FATIGUE);
			}
			else
			{
				//Low points = bad blocks
				G_Stagger(blocker);
				PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
			}
		}
	}
	else if (PM_SaberInKata(static_cast<saber_moveName_t>(blocker->client->ps.saber_move)))
	{
		sab_beh_add_balance(attacker, -MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, MPCOST_PARRIED);

		if (g_SerenityJediEngineMode->integer == 2)
		{
			if (blocker->client->ps.blockPoints < BLOCKPOINTS_TEN)
			{
				//Low points = bad blocks
				SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
				wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
			}
			else
			{
				//Low points = bad blocks
				G_Stagger(blocker);
				PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
			}
		}
		else
		{
			if (blocker->client->ps.forcePower < BLOCKPOINTS_TEN)
			{
				//Low points = bad blocks
				SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
				WP_ForcePowerRegenerate(blocker, BLOCKPOINTS_FATIGUE);
			}
			else
			{
				//Low points = bad blocks
				G_Stagger(blocker);
				PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
			}
		}
	}
	else
	{
		//either both are faking or neither is faking.  Either way, it's canceled out
		//set self
		sab_beh_add_balance(attacker, MPCOST_PARRIED);
		//set otherOwner
		sab_beh_add_balance(blocker, MPCOST_PARRIED);

		sab_beh_attack_blocked(attacker, blocker, saber_num, qtrue);

		sab_beh_attack_blocked(blocker, attacker, saber_num, qtrue);
	}
	return qtrue;
}

qboolean sab_beh_attack_vs_block(gentity_t* attacker, gentity_t* blocker, const int saber_num, const int blade_num, vec3_t hit_loc)
{
	//if the attack is blocked -(Im the attacker)
	const qboolean accurate_parry = g_accurate_blocking(blocker, attacker, hit_loc); // Perfect Normal Blocking

	const qboolean blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;	//Normal Blocking (just holding block button)
	const qboolean m_blocking = blocker->client->ps.ManualBlockingFlags & 1 << PERFECTBLOCKING ? qtrue : qfalse;	//perfect Blocking (Timed Block)
	const qboolean active_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;	//Active Blocking (Holding Block button = Attack button)
	const qboolean npc_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_NPCBLOCKING ? qtrue : qfalse;

	//(Npc Blocking function)
	const qboolean atkfake = attacker->client->ps.userInt3 & 1 << FLAG_ATTACKFAKE ? qtrue : qfalse;

	if (pm_saber_innonblockable_attack(attacker->client->ps.torsoAnim))
	{
		//perfect Blocking
		if (m_blocking) // A perfectly timed block
		{
			sab_beh_saber_should_be_disarmed_attacker(attacker, saber_num);
			//just so attacker knows that he was blocked
			attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
			//since it was parried, take away any damage done
			wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);

			if (g_SerenityJediEngineMode->integer == 2)
			{
				PM_AddBlockFatigue(&attacker->client->ps, BLOCKPOINTS_TEN); //BP Punish Attacker
			}
			else
			{
				PM_AddFatigue(&attacker->client->ps, BLOCKPOINTS_TEN); //BP Punish Attacker
			}
		}
		else
		{
			//This must be Unblockable
			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_MAGENTA"Attacker must be Unblockable\n");
			}
			attacker->client->ps.saberEventFlags &= ~SEF_BLOCKED;
		}
	}
	else if (BG_SaberInNonIdleDamageMove(&blocker->client->ps))
	{
		//and blocker is attacking
		if (d_attackinfo->integer || g_DebugSaberCombat->integer)
		{
			gi.Printf(S_COLOR_YELLOW"Both Attacker and Blocker are now attacking\n");
		}

		sab_beh_attack_vs_attack(blocker, attacker, saber_num);
	}
	else if (PM_SuperBreakWinAnim(attacker->client->ps.torsoAnim))
	{
		//attacker was attempting a superbreak and he hit someone who could block the move, rail him for screwing up.
		sab_beh_add_balance(attacker, MPCOST_PARRIED);

		sab_beh_animate_heavy_slow_bounce_attacker(attacker);

		sab_beh_add_balance(blocker, -MPCOST_PARRIED);
		if (d_attackinfo->integer || g_DebugSaberCombat->integer)
		{
			gi.Printf(S_COLOR_YELLOW"Attacker Super break win / fail\n");
		}
	}
	else if (atkfake)
	{
		//attacker faked but it was blocked here
		if (m_blocking || npc_blocking)
		{
			//defender parried the attack fake.
			sab_beh_add_balance(attacker, MPCOST_PARRIED_ATTACKFAKE);

			if (npc_blocking) //blocker is
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_BLOCKED;
			}
			else
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_PARRIED;
			}

			sab_beh_add_balance(blocker, MPCOST_PARRYING_ATTACKFAKE);
			sab_beh_add_mishap_Fake_attacker(attacker, blocker, saber_num);

			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_YELLOW"Attackers Attack Fake was P-Blocked\n");
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

			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_YELLOW"Attacker forced a saberlock\n");
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
				if (npc_blocking && (g_SerenityJediEngineMode->integer == 2 && blocker->client->ps.blockPoints >= BLOCKPOINTS_MISSILE
					|| g_SerenityJediEngineMode->integer == 1 && blocker->client->ps.forcePower >= BLOCKPOINTS_MISSILE)
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

				if (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker))
				{
					CGCam_BlockShakeSP(0.45f, 100);
				}
			}
			else
			{
				attacker->client->ps.userInt3 |= 1 << FLAG_PARRIED;
			}

			if (!m_blocking)
			{
				sab_beh_attack_blocked(attacker, blocker, saber_num, qfalse);
			}

			sab_beh_add_balance(blocker, -MPCOST_PARRIED);

			if (d_attackinfo->integer || g_DebugSaberCombat->integer)
			{
				gi.Printf(S_COLOR_YELLOW"Attackers Attack was Blocked\n");
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
					if (d_attackinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_YELLOW"Attack an Unblockable attack\n");
					}
				}
				else
				{
					sab_beh_attack_blocked(attacker, blocker, saber_num, qtrue);

					G_Stagger(blocker);

					if (d_attackinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_ORANGE"Attacker All the rest of the types of contact\n");
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
	const qboolean active_blocking = blocker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;	//Active Blocking
	const qboolean npc_blocking = blocker->client->ps.ManualBlockingFlags & 1 << MBF_NPCBLOCKING ? qtrue : qfalse;	//Active NPC Blocking

	if (g_SerenityJediEngineMode->integer)
	{
		if (g_SerenityJediEngineMode->integer == 2)
		{
			if (!pm_saber_innonblockable_attack(attacker->client->ps.torsoAnim))
			{
				if (blocker->client->ps.blockPoints <= BLOCKPOINTS_FATIGUE) // blocker has less than 20BP
				{
					if (blocker->client->ps.blockPoints <= BLOCKPOINTS_TEN) // blocker has less than 10BP
					{
						//Low points = bad blocks
						if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
						{
							sab_beh_add_mishap_blocker(blocker, saber_num);
						}
						else
						{
							SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
						}

						if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
						{
							WP_BlockPointsRegenerate(attacker, BLOCKPOINTS_FATIGUE);
						}
						else
						{
							if (!blocker->client->ps.saberInFlight)
							{
								WP_BlockPointsRegenerate(blocker, BLOCKPOINTS_FATIGUE);
							}
						}

						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker))
						{
							gi.Printf(S_COLOR_CYAN"Blocker was disarmed with very low bp, recharge bp 20bp\n");
						}

						//just so blocker knows that he has parried the attacker
						blocker->client->ps.saberEventFlags |= SEF_PARRIED;
						//just so attacker knows that he was blocked
						attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
						wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
					}
					else
					{
						//Low points = bad blocks
						g_fatigue_bp_knockaway(blocker);

						PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_DANGER);

						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker)))
						{
							gi.Printf(S_COLOR_CYAN"Blocker stagger drain 4 bp\n");
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
							WP_SaberMBlock(blocker, attacker, saber_num, blade_num, hit_loc);

							if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_THIRTEEN)
							{
								sab_beh_add_mishap_attacker(attacker, saber_num);
							}
							else
							{
								sab_beh_animate_heavy_slow_bounce_attacker(attacker);
								attacker->client->ps.userInt3 |= 1 << FLAG_MBLOCKBOUNCE;
							}

							blocker->client->ps.userInt3 |= 1 << FLAG_PERFECTBLOCK;

							if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
							{
								g_do_m_block_response(attacker);
							}

							if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
							{
								if (d_slowmoaction->integer)
								{
									G_StartStasisEffect_FORCE_LEVEL_1(blocker, MEF_NO_SPIN, 200, 0.3f, 0);
								}
								CGCam_BlockShakeSP(0.45f, 100);
							}

							G_Sound(blocker, G_SoundIndex(va("sound/weapons/saber/saber_perfectblock%d.mp3", Q_irand(1, 3))));

							if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS
								|| G_ControlledByPlayer(blocker))
							{
								gi.Printf(S_COLOR_CYAN"Blocker Perfect blocked reward 20\n");
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
							if (blocker->client->ps.blockPoints <= BLOCKPOINTS_HALF)
							{
								WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
							}
							else
							{
								if (attacker->client->ps.saberAnimLevel == SS_DESANN || attacker->client->ps.saberAnimLevel == SS_STRONG)
								{
									WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
								}
								else
								{
									WP_SaberParry(blocker, attacker, saber_num, blade_num);
								}
							}

							if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
							{
								PM_AddBlockFatigue(&attacker->client->ps, BLOCKPOINTS_THREE);
							}

							PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_FIVE);

							if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
							{
								CGCam_BlockShakeSP(0.45f, 100);
							}

							if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS
								|| G_ControlledByPlayer(blocker))
							{
								gi.Printf(S_COLOR_CYAN"Blocker Spamming block + attack cost 5\n");
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
						if (blocker->client->ps.blockPoints <= BLOCKPOINTS_HALF)
						{
							WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
						}
						else
						{
							if (attacker->client->ps.saberAnimLevel == SS_DESANN || attacker->client->ps.saberAnimLevel == SS_STRONG)
							{
								WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
							}
							else
							{
								WP_SaberBlockedBounceBlock(blocker, attacker, saber_num, blade_num, hit_loc);
							}
						}

						if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
						{
							//
						}
						else
						{
							PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
						}

						if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
						{
							wp_block_points_regenerate_over_ride(attacker, BLOCKPOINTS_DANGER);
						}

						if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
						{
							CGCam_BlockShakeSP(0.45f, 100);
						}
						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker))
						{
							gi.Printf(S_COLOR_CYAN"Blocker Holding block button only (spamming block) cost 10\n");
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
						if (attacker->client->ps.saberAnimLevel == SS_DESANN || attacker->client->ps.saberAnimLevel == SS_STRONG)
						{
							WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
						}
						else
						{
							if (blocker->client->ps.blockPoints <= BLOCKPOINTS_MISSILE)
							{
								if (blocker->client->ps.blockPoints <= BLOCKPOINTS_FOURTY)
								{
									WP_SaberNPCFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);

									if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
										G_ControlledByPlayer(blocker)))
									{
										gi.Printf(S_COLOR_CYAN"NPC Fatigued Parry\n");
									}

									PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_FAIL);
								}
								else
								{
									WP_SaberNPCParry(blocker, attacker, saber_num, blade_num, hit_loc);

									if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
										G_ControlledByPlayer(blocker)))
									{
										gi.Printf(S_COLOR_CYAN"NPC normal Parry\n");
									}

									PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_THREE);
								}
							}
							else
							{
								WP_SaberNPCMBlock(blocker, attacker, saber_num, blade_num, hit_loc);

								if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
								{
									g_do_m_block_response(blocker);
								}

								if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
									G_ControlledByPlayer(blocker)))
								{
									gi.Printf(S_COLOR_CYAN"NPC good Parry\n");
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

						G_Sound(blocker, G_SoundIndex(va("sound/weapons/saber/saber_goodparry%d.mp3", Q_irand(1, 3))));
					}
					else
					{
						sab_beh_add_mishap_blocker(blocker, saber_num);

						if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
						{
							//
						}
						else
						{
							PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
						}
						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker))
						{
							gi.Printf(S_COLOR_CYAN"Blocker Not holding block drain 10\n");
						}
					}
				}
			}
			else
			{
				//perfect Blocking
				if (m_blocking) // A perfectly timed block
				{
					if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
					{
						if (d_slowmoaction->integer)
						{
							G_StartStasisEffect_FORCE_LEVEL_1(blocker, MEF_NO_SPIN, 200, 0.3f, 0);
						}
						CGCam_BlockShakeSP(0.45f, 100);
					}

					G_Sound(blocker, G_SoundIndex(va("sound/weapons/saber/saber_perfectblock%d.mp3", Q_irand(1, 3))));

					if (d_blockinfo->integer && blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
					{
						gi.Printf(S_COLOR_CYAN"Blocker Perfect blocked an Unblockable attack reward 20\n");
					}

					//just so blocker knows that he has parried the attacker
					blocker->client->ps.saberEventFlags |= SEF_PARRIED;

					wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE); //BP Reward blocker
					blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE; //SAC Reward blocker
				}
				else
				{
					//This must be Unblockable
					if (blocker->client->ps.blockPoints < BLOCKPOINTS_TEN)
					{
						//Low points = bad blocks
						SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
						wp_block_points_regenerate_over_ride(blocker, BLOCKPOINTS_FATIGUE);
					}
					else
					{
						g_fatigue_bp_knockaway(blocker);
						PM_AddBlockFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
					}
					if (d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_MAGENTA"Blocker can not block Unblockable\n");
					}
					blocker->client->ps.saberEventFlags &= ~SEF_PARRIED;
				}
			}
		}
		else
		{
			if (!pm_saber_innonblockable_attack(attacker->client->ps.torsoAnim))
			{
				if (blocker->client->ps.forcePower <= BLOCKPOINTS_FATIGUE) // blocker has less than 20BP
				{
					if (blocker->client->ps.forcePower <= BLOCKPOINTS_TEN) // blocker has less than 10BP
					{
						//Low points = bad blocks
						if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
						{
							sab_beh_add_mishap_blocker(blocker, saber_num);
						}
						else
						{
							SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
						}

						if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
						{
							WP_ForcePowerRegenerate(attacker, BLOCKPOINTS_FATIGUE);
						}
						else
						{
							if (!blocker->client->ps.saberInFlight)
							{
								WP_ForcePowerRegenerate(blocker, BLOCKPOINTS_FATIGUE);
							}
						}

						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker))
						{
							gi.Printf(S_COLOR_CYAN"Blocker was disarmed with very low bp, recharge bp 20bp\n");
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

						PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_DANGER);

						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker)))
						{
							gi.Printf(S_COLOR_CYAN"Blocker stagger drain 4 bp\n");
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
							WP_SaberMBlock(blocker, attacker, saber_num, blade_num, hit_loc);

							if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_THIRTEEN)
							{
								sab_beh_add_mishap_attacker(attacker, saber_num);
							}
							else
							{
								sab_beh_animate_heavy_slow_bounce_attacker(attacker);
								attacker->client->ps.userInt3 |= 1 << FLAG_MBLOCKBOUNCE;
							}

							blocker->client->ps.userInt3 |= 1 << FLAG_PERFECTBLOCK;

							if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
							{
								g_do_m_block_response(attacker);
							}

							if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
							{
								if (d_slowmoaction->integer)
								{
									G_StartStasisEffect_FORCE_LEVEL_1(blocker, MEF_NO_SPIN, 200, 0.3f, 0);
								}
								CGCam_BlockShakeSP(0.45f, 100);
							}

							G_Sound(blocker, G_SoundIndex(va("sound/weapons/saber/saber_perfectblock%d.mp3",
								Q_irand(1, 3))));

							if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS
								|| G_ControlledByPlayer(blocker))
							{
								gi.Printf(S_COLOR_CYAN"Blocker Perfect blocked reward 20\n");
							}

							//just so blocker knows that he has parried the attacker
							blocker->client->ps.saberEventFlags |= SEF_PARRIED;
							//just so attacker knows that he was blocked
							attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
							//since it was parried, take away any damage done
							wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);

							WP_ForcePowerRegenerate(blocker, BLOCKPOINTS_TEN); //BP Reward blocker
							blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE; //SAC Reward blocker
							PM_AddFatigue(&attacker->client->ps, BLOCKPOINTS_TEN); //BP Punish Attacker
						}
						else
						{
							//Spamming block + attack buttons
							if (blocker->client->ps.forcePower <= BLOCKPOINTS_HALF)
							{
								WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
							}
							else
							{
								if (attacker->client->ps.saberAnimLevel == SS_DESANN || attacker->client->ps.saberAnimLevel == SS_STRONG)
								{
									WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
								}
								else
								{
									WP_SaberParry(blocker, attacker, saber_num, blade_num);
								}
							}

							if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
							{
								PM_AddFatigue(&attacker->client->ps, BLOCKPOINTS_THREE);
							}

							PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_FIVE);

							if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
							{
								CGCam_BlockShakeSP(0.45f, 100);
							}

							if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS
								|| G_ControlledByPlayer(blocker))
							{
								gi.Printf(S_COLOR_CYAN"Blocker Spamming block + attack cost 5\n");
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
						if (blocker->client->ps.forcePower <= BLOCKPOINTS_HALF)
						{
							WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
						}
						else
						{
							if (attacker->client->ps.saberAnimLevel == SS_DESANN || attacker->client->ps.saberAnimLevel
								== SS_STRONG)
							{
								WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
							}
							else
							{
								WP_SaberBlockedBounceBlock(blocker, attacker, saber_num, blade_num, hit_loc);
							}
						}

						if (attacker->NPC && !G_ControlledByPlayer(attacker)) //NPC only
						{
							PM_AddFatigue(&attacker->client->ps, BLOCKPOINTS_THREE);
						}

						if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
						{
							//
						}
						else
						{
							PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
						}

						if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
						{
							CGCam_BlockShakeSP(0.45f, 100);
						}
						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker))
						{
							gi.Printf(S_COLOR_CYAN"Blocker Holding block button only (spamming block) cost 10\n");
						}

						//just so blocker knows that he has parried the attacker
						blocker->client->ps.saberEventFlags |= SEF_PARRIED;
						//just so attacker knows that he was blocked
						attacker->client->ps.saberEventFlags |= SEF_BLOCKED;
						//since it was parried, take away any damage done
						wp_saber_clear_damage_for_ent_num(attacker, blocker->s.number, saber_num, blade_num);
					}
					else if (npc_blocking || (g_saberAutoBlocking->integer && blocker->NPC && !
						G_ControlledByPlayer(blocker) || blocker->client->ps.saberBlockingTime > level.time && blocker->
						NPC && !G_ControlledByPlayer(blocker))) //Other types and npc,s
					{
						if (attacker->client->ps.saberAnimLevel == SS_DESANN || attacker->client->ps.saberAnimLevel ==
							SS_STRONG)
						{
							WP_SaberFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);
						}
						else
						{
							if (blocker->client->ps.forcePower <= BLOCKPOINTS_MISSILE)
							{
								WP_SaberNPCParry(blocker, attacker, saber_num, blade_num, hit_loc);

								if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
									G_ControlledByPlayer(blocker)))
								{
									gi.Printf(S_COLOR_CYAN"NPC normal Parry\n");
								}

								PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_THREE);
							}
							else if (blocker->client->ps.forcePower <= BLOCKPOINTS_HALF)
							{
								WP_SaberNPCFatiguedParry(blocker, attacker, saber_num, blade_num, hit_loc);

								if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
									G_ControlledByPlayer(blocker)))
								{
									gi.Printf(S_COLOR_CYAN"NPC Fatigued Parry\n");
								}

								PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_FIVE);
							}
							else
							{
								WP_SaberNPCMBlock(blocker, attacker, saber_num, blade_num, hit_loc);

								if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
								{
									g_do_m_block_response(blocker);
								}

								if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && (blocker->NPC && !
									G_ControlledByPlayer(blocker)))
								{
									gi.Printf(S_COLOR_CYAN"NPC good Parry\n");
								}

								PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_THREE);

								G_Sound(blocker, G_SoundIndex(va("sound/weapons/saber/saber_goodparry%d.mp3",
									Q_irand(1, 3))));
							}
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
						sab_beh_add_mishap_blocker(blocker, saber_num);

						if (blocker->NPC && !G_ControlledByPlayer(blocker)) //NPC only
						{
							//
						}
						else
						{
							PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
						}
						if ((d_blockinfo->integer || g_DebugSaberCombat->integer) && blocker->s.number < MAX_CLIENTS ||
							G_ControlledByPlayer(blocker))
						{
							gi.Printf(S_COLOR_CYAN"Blocker Not holding block drain 10\n");
						}
					}
				}
			}
			else
			{
				//perfect Blocking
				if (m_blocking) // A perfectly timed block
				{
					if (blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
					{
						if (d_slowmoaction->integer)
						{
							G_StartStasisEffect_FORCE_LEVEL_1(blocker, MEF_NO_SPIN, 200, 0.3f, 0);
						}
						CGCam_BlockShakeSP(0.45f, 100);
					}

					blocker->client->ps.userInt3 |= 1 << FLAG_PERFECTBLOCK;

					G_Sound(blocker, G_SoundIndex(va("sound/weapons/saber/saber_perfectblock%d.mp3", Q_irand(1, 3))));

					if (d_blockinfo->integer && blocker->s.number < MAX_CLIENTS || G_ControlledByPlayer(blocker))
					{
						gi.Printf(S_COLOR_CYAN"Blocker Perfect blocked an Unblockable attack reward 20\n");
					}

					//just so blocker knows that he has parried the attacker
					blocker->client->ps.saberEventFlags |= SEF_PARRIED;
					WP_ForcePowerRegenerate(blocker, BLOCKPOINTS_FATIGUE); //BP Reward blocker
					blocker->client->ps.saberFatigueChainCount = MISHAPLEVEL_NONE; //SAC Reward blocker
				}
				else
				{
					//This must be Unblockable
					if (blocker->client->ps.blockPoints < BLOCKPOINTS_TEN)
					{
						//Low points = bad blocks
						SabBeh_SaberShouldBeDisarmedBlocker(blocker, saber_num);
						WP_ForcePowerRegenerate(blocker, BLOCKPOINTS_FATIGUE);
					}
					else
					{
						g_fatigue_bp_knockaway(blocker);
						PM_AddFatigue(&blocker->client->ps, BLOCKPOINTS_TEN);
					}
					if (d_blockinfo->integer || g_DebugSaberCombat->integer)
					{
						gi.Printf(S_COLOR_MAGENTA"Blocker can not block Unblockable\n");
					}
					blocker->client->ps.saberEventFlags &= ~SEF_PARRIED;
				}
			}
		}
	}
	return qtrue;
}

/////////Functions//////////////

/////////////////////// 20233 new build ////////////////////////////////