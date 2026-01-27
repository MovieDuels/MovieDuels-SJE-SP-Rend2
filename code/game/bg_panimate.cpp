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

#include "common_headers.h"

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
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
#define GAME_INCLUDE

#include "../qcommon/q_shared.h"
#include "g_shared.h"
#include "bg_local.h"
#include "../cgame/cg_local.h"
#include "anims.h"
#include "Q3_Interface.h"
#include "g_local.h"
#include "wp_saber.h"
#include "g_vehicles.h"

extern pmove_t* pm;
extern pml_t pml;
extern cvar_t* g_ICARUSDebug;
extern cvar_t* g_timescale;
extern cvar_t* g_synchSplitAnims;
#ifndef FINAL_BUILD
extern cvar_t* g_AnimWarning;
#endif
extern cvar_t* g_noFootSlide;
extern cvar_t* g_noFootSlideRunScale;
extern cvar_t* g_noFootSlideWalkScale;
extern cvar_t* g_saberAnimSpeed;
extern cvar_t* g_saberAutoAim;
extern cvar_t* g_speederControlScheme;
extern cvar_t* g_saberNewControlScheme;
extern cvar_t* g_noIgniteTwirl;
extern cvar_t* g_SaberAttackSpeedMD;
extern qboolean IsSurrendering(const gentity_t* self);
extern qboolean PM_SaberInMassiveBounce(int move);
extern qboolean InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern qboolean ValidAnimFileIndex(int index);
extern qboolean PM_ControlledByPlayer();
extern qboolean PM_DroidMelee(int npc_class);
extern qboolean PM_PainAnim(int anim);
extern qboolean PM_JumpingAnim(int anim);
extern qboolean PM_FlippingAnim(int anim);
extern qboolean PM_RollingAnim(int anim);
extern qboolean PM_SwimmingAnim(int anim);
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern qboolean PM_InRoll(const playerState_t* ps);
extern qboolean PM_DodgeAnim(int anim);
extern qboolean PM_InSlopeAnim(int anim);
extern qboolean PM_ForceAnim(int anim);
extern qboolean PM_InKnockDownOnGround(playerState_t* ps);
extern qboolean PM_InSpecialJump(int anim);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern qboolean PM_SwimmingAnim(int anim);
extern qboolean PM_SaberStanceAnim(int anim);
extern qboolean PM_SaberDrawPutawayAnim(int anim);
extern void PM_SetJumped(float height, qboolean force);
extern qboolean PM_InGetUpNoRoll(const playerState_t* ps);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean G_TryingKataAttack(const usercmd_t* cmd);
extern qboolean G_TryingCartwheel(const gentity_t* self, const usercmd_t* cmd);
extern qboolean G_TryingJumpAttack(const gentity_t* self, const usercmd_t* cmd);
extern qboolean G_TryingJumpForwardAttack(const gentity_t* self, const usercmd_t* cmd);
extern qboolean G_TryingLungeAttack(const gentity_t* self, const usercmd_t* cmd);
extern qboolean G_TryingPullAttack(const gentity_t* self, const usercmd_t* cmd, qboolean am_pulling);
extern qboolean G_InCinematicSaberAnim(const gentity_t* self);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean PM_Can_Do_Kill_Move();
extern qboolean G_TryingSpecial(const usercmd_t* cmd);
extern int g_crosshairEntNum;
extern cvar_t* g_RealisticBlockingMode;
int PM_AnimLength(int index, animNumber_t anim);
qboolean PM_LockedAnim(int anim);
qboolean PM_StandingAnim(int anim);
qboolean PM_InOnGroundAnim(playerState_t* ps);
qboolean PM_SuperBreakWinAnim(int anim);
qboolean PM_SuperBreakLoseAnim(int anim);
qboolean PM_LockedAnim(int anim);
saber_moveName_t PM_SaberFlipOverAttackMove();
qboolean PM_CheckFlipOverAttackMove(qboolean check_enemy);
saber_moveName_t PM_SaberJumpForwardAttackMove();
saber_moveName_t PM_SaberLungeAttackMove(qboolean fallback_to_normal_lunge);
qboolean PM_CheckLungeAttackMove();
qboolean PM_StandingidleAnim(int anim);
extern qboolean PM_WindAnim(int anim);
extern cvar_t* g_SerenityJediEngineMode;
qboolean PM_StandingAtReadyAnim(int anim);
extern qboolean PM_InKataAnim(int anim);

// Okay, here lies the much-dreaded Pat-created FSM movement chart...  Heretic II strikes again!
// Why am I inflicting this on you?  Well, it's better than hardcoded states.
// Ideally this will be replaced with an external file or more sophisticated move-picker
// once the game gets out of prototype stage.

// Silly, but I'm replacing these macros so they are shorter!
#define AFLAG_IDLE	(SETANIM_FLAG_NORMAL)
#define AFLAG_ACTIVE (SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS)
#define AFLAG_WAIT (SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS)
#define AFLAG_FINISH (SETANIM_FLAG_HOLD)

//FIXME: add the alternate anims for each style?
saber_moveData_t saber_moveData[LS_MOVE_MAX] = {
	//							NB:randomized
	// name			anim(do all styles?)startQ	endQ	setanimflag		blend,	blocking	chain_idle		chain_attack	trailLen
	{"None", BOTH_STAND1, Q_R, Q_R, AFLAG_IDLE, 350, BLK_NO, LS_NONE, LS_NONE, 0}, // LS_NONE		= 0,

	// General movements with saber
	{"Ready", BOTH_STAND2, Q_R, Q_R, AFLAG_IDLE, 350, BLK_WIDE, LS_READY, LS_S_R2L, 0}, // LS_READY,
	{"Draw", BOTH_STAND1TO2, Q_R, Q_R, AFLAG_FINISH, 350, BLK_NO, LS_READY, LS_S_R2L, 0}, // LS_DRAW,
	{"Putaway", BOTH_STAND2TO1, Q_R, Q_R, AFLAG_FINISH, 350, BLK_NO, LS_READY, LS_S_R2L, 0}, // LS_PUTAWAY,

	// Attacks
	//UL2LR
	{"TL2BR Att", BOTH_A1_TL_BR, Q_TL, Q_BR, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_R_TL2BR, LS_R_TL2BR, 200}, // LS_A_TL2BR
	//SLASH LEFT
	{"L2R Att", BOTH_A1__L__R, Q_L, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_R_L2R, LS_R_L2R, 200}, // LS_A_L2R
	//LL2UR
	{"BL2TR Att", BOTH_A1_BL_TR, Q_BL, Q_TR, AFLAG_ACTIVE, 50, BLK_TIGHT, LS_R_BL2TR, LS_R_BL2TR, 200}, // LS_A_BL2TR
	//LR2UL
	{"BR2TL Att", BOTH_A1_BR_TL, Q_BR, Q_TL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_R_BR2TL, LS_R_BR2TL, 200}, // LS_A_BR2TL
	//SLASH RIGHT
	{"R2L Att", BOTH_A1__R__L, Q_R, Q_L, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_R_R2L, LS_R_R2L, 200}, // LS_A_R2L
	//UR2LL
	{"TR2BL Att", BOTH_A1_TR_BL, Q_TR, Q_BL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_R_TR2BL, LS_R_TR2BL, 200}, // LS_A_TR2BL
	//SLASH DOWN
	{"T2B Att", BOTH_A1_T__B_, Q_T, Q_B, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_R_T2B, LS_R_T2B, 200}, // LS_A_T2B
	//special attacks
	{"Back Stab", BOTH_A2_STABBACK1, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_A_BACKSTAB
	{"Back Att", BOTH_ATTACK_BACK, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_A_BACK
	{"CR Back Att", BOTH_CROUCHATTACKBACK1, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_A_BACK_CR
	{"RollStab", BOTH_ROLL_STAB, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_ROLL_STAB
	{"Lunge Att", BOTH_LUNGE2_B__T_, Q_B, Q_T, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_A_LUNGE
	{"Jump Att", BOTH_FORCELEAP2_T__B_, Q_T, Q_B, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_A_JUMP_T__B_
	{"Flip Stab", BOTH_JUMPFLIPSTABDOWN, Q_R, Q_T, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1_T___R, 200},
	// LS_A_FLIP_STAB
	{"Flip Slash", BOTH_JUMPFLIPSLASHDOWN1, Q_L, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1__R_T_, 200},
	// LS_A_FLIP_SLASH
	{"DualJump Atk", BOTH_JUMPATTACK6, Q_R, Q_BL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1_BL_TR, 200},
	// LS_JUMPATTACK_DUAL

	{"DualJumpAtkL_A", BOTH_ARIAL_LEFT, Q_R, Q_TL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_A_TL2BR, 200},
	// LS_JUMPATTACK_ARIAL_LEFT
	{"DualJumpAtkR_A", BOTH_ARIAL_RIGHT, Q_R, Q_TR, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_A_TR2BL, 200},
	// LS_JUMPATTACK_ARIAL_RIGHT

	{"DualJumpAtkL_A", BOTH_CARTWHEEL_LEFT, Q_R, Q_TL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1_TL_BR, 200},
	// LS_JUMPATTACK_CART_LEFT
	{"DualJumpAtkR_A", BOTH_CARTWHEEL_RIGHT, Q_R, Q_TR, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1_TR_BL, 200},
	// LS_JUMPATTACK_CART_RIGHT

	{"DualJumpAtkLStaff", BOTH_BUTTERFLY_FL1, Q_R, Q_L, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1__L__R, 200},
	// LS_JUMPATTACK_STAFF_LEFT
	{"DualJumpAtkRStaff", BOTH_BUTTERFLY_FR1, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1__R__L, 200},
	// LS_JUMPATTACK_STAFF_RIGHT

	{"ButterflyLeft", BOTH_BUTTERFLY_LEFT, Q_R, Q_L, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1__L__R, 200},
	// LS_BUTTERFLY_LEFT
	{"ButterflyRight", BOTH_BUTTERFLY_RIGHT, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1__R__L, 200},
	// LS_BUTTERFLY_RIGHT

	{"BkFlip Atk", BOTH_JUMPATTACK7, Q_B, Q_T, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_T1_T___R, 200},
	// LS_A_BACKFLIP_ATK
	{"DualSpinAtk", BOTH_SPINATTACK6, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_SPINATTACK_DUAL
	{"StfSpinAtk", BOTH_SPINATTACK7, Q_L, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_SPINATTACK
	{"LngLeapAtk", BOTH_FORCELONGLEAP_ATTACK, Q_R, Q_L, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_LEAP_ATTACK
	{"SwoopAtkR", BOTH_VS_ATR_S, Q_R, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 200}, // LS_SWOOP_ATTACK_RIGHT
	{"SwoopAtkL", BOTH_VS_ATL_S, Q_L, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 200}, // LS_SWOOP_ATTACK_LEFT
	{"TauntaunAtkR", BOTH_VT_ATR_S, Q_R, Q_T, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_TAUNTAUN_ATTACK_RIGHT
	{"TauntaunAtkL", BOTH_VT_ATL_S, Q_L, Q_T, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_TAUNTAUN_ATTACK_LEFT
	{"StfKickFwd", BOTH_A7_KICK_F, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_KICK_F
	{"StfKickFwd2", BOTH_KICK_F_MD, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_KICK_F_MD
	{"StfKickBack", BOTH_A7_KICK_B, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_KICK_B
	{"Sweepkick", BOTH_SWEEP_KICK, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_KICK_SWEEP
	{"StfKickRight", BOTH_A7_KICK_R, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_KICK_R
	{"StfsmackRight", BOTH_A7_SLAP_R, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_SMACK_R
	{"StfKickLeft", BOTH_A7_KICK_L, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_KICK_L
	{"StfsmackLeft", BOTH_A7_SLAP_L, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_SMACK_L
	{"StfKickSpin", BOTH_A7_KICK_S, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_S_R2L, 200}, // LS_KICK_S
	{"StfKickBkFwd", BOTH_A7_KICK_BF, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_S_R2L, 200}, // LS_KICK_BF
	{"StfKickSplit", BOTH_A7_KICK_RL, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_S_R2L, 200}, // LS_KICK_RL
	{"StfKickFwdAir", BOTH_A7_KICK_F_AIR, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_KICK_F_AIR
	{"StfKickFwdAir2", BOTH_FLYING_KICK, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_FLYING_KICK
	{"StfKickBackAir", BOTH_A7_KICK_B_AIR, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_KICK_B_AIR
	{"StfKickRightAir", BOTH_A7_KICK_R_AIR, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_KICK_R_AIR
	{"StfKickLeftAir", BOTH_A7_KICK_L_AIR, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_KICK_L_AIR
	{"StabDown", BOTH_STABDOWN, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_STABDOWN
	{"StabDownStf", BOTH_STABDOWN_STAFF, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_STABDOWN_STAFF
	{"StabDownDual", BOTH_STABDOWN_DUAL, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200},
	// LS_STABDOWN_DUAL
	{"dualspinprot", BOTH_A6_SABERPROTECT, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 500},
	// LS_DUAL_SPIN_PROTECT
	{"StfSoulCal", BOTH_A7_SOULCAL, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 500},
	// LS_STAFF_SOULCAL
	{"specialfast", BOTH_A1_SPECIAL, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 2000}, // LS_A1_SPECIAL
	{"specialmed", BOTH_A2_SPECIAL, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 2000}, // LS_A2_SPECIAL
	{"specialstr", BOTH_A3_SPECIAL, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 2000}, // LS_A3_SPECIAL
	{"upsidedwnatk", BOTH_FLIP_ATTACK7, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_UPSIDE_DOWN_ATTACK
	{"pullatkstab", BOTH_PULL_IMPALE_STAB, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_PULL_ATTACK_STAB
	{"grapplefire", BOTH_GRAPPLE_FIRE, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_GRAPPLE_FIRE
	{"pullatkswing", BOTH_PULL_IMPALE_SWING, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_PULL_ATTACK_SWING
	{"AloraSpinAtk", BOTH_ALORA_SPIN_SLASH, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200},
	// LS_SPINATTACK_ALORA
	{"Dual FB Atk", BOTH_A6_FB, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_DUAL_FB
	{"Dual LR Atk", BOTH_A6_LR, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_DUAL_LR
	{"StfHiltBash", BOTH_A7_HILT, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_HILT_BASH
	{"StfslapRight", BOTH_SLAP_R, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_SLAP_R
	{"StfslapLeft", BOTH_SLAP_L, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_READY, LS_S_R2L, 200}, // LS_SLAP_L

	//starts
	{"TL2BR St", BOTH_S1_S1_TL, Q_R, Q_TL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_TL2BR, LS_A_TL2BR, 200}, // LS_S_TL2BR
	{"L2R St", BOTH_S1_S1__L, Q_R, Q_L, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_L2R, LS_A_L2R, 200}, // LS_S_L2R
	{"BL2TR St", BOTH_S1_S1_BL, Q_R, Q_BL, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_BL2TR, LS_A_BL2TR, 200}, // LS_S_BL2TR
	{"BR2TL St", BOTH_S1_S1_BR, Q_R, Q_BR, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_BR2TL, LS_A_BR2TL, 200}, // LS_S_BR2TL
	{"R2L St", BOTH_S1_S1__R, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_R2L, LS_A_R2L, 200}, // LS_S_R2L
	{"TR2BL St", BOTH_S1_S1_TR, Q_R, Q_TR, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_TR2BL, LS_A_TR2BL, 200}, // LS_S_TR2BL
	{"T2B St", BOTH_S1_S1_T_, Q_R, Q_T, AFLAG_ACTIVE, 100, BLK_TIGHT, LS_A_T2B, LS_A_T2B, 200}, // LS_S_T2B

	//returns
	{"TL2BR Ret", BOTH_R1_BR_S1, Q_BR, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_TL2BR
	{"L2R Ret", BOTH_R1__R_S1, Q_R, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_L2R
	{"BL2TR Ret", BOTH_R1_TR_S1, Q_TR, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_BL2TR
	{"BR2TL Ret", BOTH_R1_TL_S1, Q_TL, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_BR2TL
	{"R2L Ret", BOTH_R1__L_S1, Q_L, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_R2L
	{"TR2BL Ret", BOTH_R1_BL_S1, Q_BL, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_TR2BL
	{"T2B Ret", BOTH_R1_B__S1, Q_B, Q_R, AFLAG_FINISH, 100, BLK_TIGHT, LS_READY, LS_READY, 200}, // LS_R_T2B

	//Transitions
	{"BR2R Trans", BOTH_T1_BR__R, Q_BR, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_A_R2L, 150},
	//# Fast arc bottom right to right
	{"BR2TR Trans", BOTH_T1_BR_TR, Q_BR, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_TR2BL, 150},
	//# Fast arc bottom right to top right		(use: BOTH_T1_TR_BR)
	{"BR2T Trans", BOTH_T1_BR_T_, Q_BR, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_T2B, 150},
	//# Fast arc bottom right to top			(use: BOTH_T1_T__BR)
	{"BR2TL Trans", BOTH_T1_BR_TL, Q_BR, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_A_TL2BR, 150},
	//# Fast weak spin bottom right to top left
	{"BR2L Trans", BOTH_T1_BR__L, Q_BR, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_A_L2R, 150},
	//# Fast weak spin bottom right to left
	{"BR2BL Trans", BOTH_T1_BR_BL, Q_BR, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_A_BL2TR, 150},
	//# Fast weak spin bottom right to bottom left
	{"R2BR Trans", BOTH_T1__R_BR, Q_R, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_A_BR2TL, 150},
	//# Fast arc right to bottom right			(use: BOTH_T1_BR__R)
	{"R2TR Trans", BOTH_T1__R_TR, Q_R, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_TR2BL, 150},
	//# Fast arc right to top right
	{"R2T Trans", BOTH_T1__R_T_, Q_R, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_T2B, 150},
	//# Fast ar right to top				(use: BOTH_T1_T___R)
	{"R2TL Trans", BOTH_T1__R_TL, Q_R, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_A_TL2BR, 150},
	//# Fast arc right to top left
	{"R2L Trans", BOTH_T1__R__L, Q_R, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_A_L2R, 150},
	//# Fast weak spin right to left
	{"R2BL Trans", BOTH_T1__R_BL, Q_R, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_A_BL2TR, 150},
	//# Fast weak spin right to bottom left
	{"TR2BR Trans", BOTH_T1_TR_BR, Q_TR, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_A_BR2TL, 150},
	//# Fast arc top right to bottom right
	{"TR2R Trans", BOTH_T1_TR__R, Q_TR, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_A_R2L, 150},
	//# Fast arc top right to right			(use: BOTH_T1__R_TR)
	{"TR2T Trans", BOTH_T1_TR_T_, Q_TR, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_T2B, 150},
	//# Fast arc top right to top				(use: BOTH_T1_T__TR)
	{"TR2TL Trans", BOTH_T1_TR_TL, Q_TR, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_A_TL2BR, 150},
	//# Fast arc top right to top left
	{"TR2L Trans", BOTH_T1_TR__L, Q_TR, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_A_L2R, 150},
	//# Fast arc top right to left
	{"TR2BL Trans", BOTH_T1_TR_BL, Q_TR, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_A_BL2TR, 150},
	//# Fast weak spin top right to bottom left
	{"T2BR Trans", BOTH_T1_T__BR, Q_T, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_A_BR2TL, 150},
	//# Fast arc top to bottom right
	{"T2R Trans", BOTH_T1_T___R, Q_T, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_A_R2L, 150},
	//# Fast arc top to right
	{"T2TR Trans", BOTH_T1_T__TR, Q_T, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_TR2BL, 150},
	//# Fast arc top to top right
	{"T2TL Trans", BOTH_T1_T__TL, Q_T, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_A_TL2BR, 150},
	//# Fast arc top to top left
	{"T2L Trans", BOTH_T1_T___L, Q_T, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_A_L2R, 150}, //# Fast arc top to left
	{"T2BL Trans", BOTH_T1_T__BL, Q_T, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_A_BL2TR, 150},
	//# Fast arc top to bottom left
	{"TL2BR Trans", BOTH_T1_TL_BR, Q_TL, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_A_BR2TL, 150},
	//# Fast weak spin top left to bottom right
	{"TL2R Trans", BOTH_T1_TL__R, Q_TL, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_A_R2L, 150},
	//# Fast arc top left to right			(use: BOTH_T1__R_TL)
	{"TL2TR Trans", BOTH_T1_TL_TR, Q_TL, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_TR2BL, 150},
	//# Fast arc top left to top right			(use: BOTH_T1_TR_TL)
	{"TL2T Trans", BOTH_T1_TL_T_, Q_TL, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_T2B, 150},
	//# Fast arc top left to top				(use: BOTH_T1_T__TL)
	{"TL2L Trans", BOTH_T1_TL__L, Q_TL, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_A_L2R, 150},
	//# Fast arc top left to left				(use: BOTH_T1__L_TL)
	{"TL2BL Trans", BOTH_T1_TL_BL, Q_TL, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_A_BL2TR, 150},
	//# Fast arc top left to bottom left
	{"L2BR Trans", BOTH_T1__L_BR, Q_L, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_A_BR2TL, 150},
	//# Fast weak spin left to bottom right
	{"L2R Trans", BOTH_T1__L__R, Q_L, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_A_R2L, 150},
	//# Fast weak spin left to right
	{"L2TR Trans", BOTH_T1__L_TR, Q_L, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_TR2BL, 150},
	//# Fast arc left to top right			(use: BOTH_T1_TR__L)
	{"L2T Trans", BOTH_T1__L_T_, Q_L, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_T2B, 150},
	//# Fast arc left to top				(use: BOTH_T1_T___L)
	{"L2TL Trans", BOTH_T1__L_TL, Q_L, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_A_TL2BR, 150},
	//# Fast arc left to top left
	{"L2BL Trans", BOTH_T1__L_BL, Q_L, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_A_BL2TR, 150},
	//# Fast arc left to bottom left			(use: BOTH_T1_BL__L)
	{"BL2BR Trans", BOTH_T1_BL_BR, Q_BL, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_A_BR2TL, 150},
	//# Fast weak spin bottom left to bottom right
	{"BL2R Trans", BOTH_T1_BL__R, Q_BL, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_A_R2L, 150},
	//# Fast weak spin bottom left to right
	{"BL2TR Trans", BOTH_T1_BL_TR, Q_BL, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_TR2BL, 150},
	//# Fast weak spin bottom left to top right
	{"BL2T Trans", BOTH_T1_BL_T_, Q_BL, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_A_T2B, 150},
	//# Fast arc bottom left to top			(use: BOTH_T1_T__BL)
	{"BL2TL Trans", BOTH_T1_BL_TL, Q_BL, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_A_TL2BR, 150},
	//# Fast arc bottom left to top left		(use: BOTH_T1_TL_BL)
	{"BL2L Trans", BOTH_T1_BL__L, Q_BL, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_A_L2R, 150},
	//# Fast arc bottom left to left

	//Bounces
	{"Bounce BR", BOTH_B1_BR___, Q_BR, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_T1_BR_TR, 150},
	{"Bounce R", BOTH_B1__R___, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_T1__R__L, 150},
	{"Bounce TR", BOTH_B1_TR___, Q_TR, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_T1_TR_TL, 150},
	{"Bounce T", BOTH_B1_T____, Q_T, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_T1_T__BL, 150},
	{"Bounce TL", BOTH_B1_TL___, Q_TL, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_T1_TL_TR, 150},
	{"Bounce L", BOTH_B1__L___, Q_L, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_T1__L__R, 150},
	{"Bounce BL", BOTH_B1_BL___, Q_BL, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_T1_BL_TR, 150},

	//Deflected attacks (like bounces, but slide off enemy saber, not straight back)
	{"Deflect BR", BOTH_D1_BR___, Q_BR, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TL2BR, LS_T1_BR_TR, 150},
	{"Deflect R", BOTH_D1__R___, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_R_L2R, LS_T1__R__L, 150},
	{"Deflect TR", BOTH_D1_TR___, Q_TR, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_T1_TR_TL, 150},
	{"Deflect T", BOTH_B1_T____, Q_T, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_T1_T__BL, 150},
	{"Deflect TL", BOTH_D1_TL___, Q_TL, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BR2TL, LS_T1_TL_TR, 150},
	{"Deflect L", BOTH_D1__L___, Q_L, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_R_R2L, LS_T1__L__R, 150},
	{"Deflect BL", BOTH_D1_BL___, Q_BL, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_R_TR2BL, LS_T1_BL_TR, 150},
	{"Deflect B", BOTH_D1_B____, Q_B, Q_B, AFLAG_ACTIVE, 100, BLK_NO, LS_R_BL2TR, LS_T1_T__BL, 150},

	//Reflected attacks
	{"Reflected BR", BOTH_V1_BR_S1, Q_BR, Q_BR, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1_BR
	{"Reflected R", BOTH_V1__R_S1, Q_R, Q_R, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1__R
	{"Reflected TR", BOTH_V1_TR_S1, Q_TR, Q_TR, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1_TR
	{"Reflected T", BOTH_V1_T__S1, Q_T, Q_T, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1_T_
	{"Reflected TL", BOTH_V1_TL_S1, Q_TL, Q_TL, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1_TL
	{"Reflected L", BOTH_V1__L_S1, Q_L, Q_L, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1__L
	{"Reflected BL", BOTH_V1_BL_S1, Q_BL, Q_BL, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1_BL
	{"Reflected B", BOTH_V1_B__S1, Q_B, Q_B, AFLAG_ACTIVE, 100, BLK_NO, LS_READY, LS_READY, 150}, //	LS_V1_B_

	// Broken parries
	{"BParry Top", BOTH_H1_S1_T_, Q_T, Q_B, AFLAG_ACTIVE, 50, BLK_NO, LS_READY, LS_READY, 150}, // LS_PARRY_UP,
	{"BParry UR", BOTH_H1_S1_TR, Q_TR, Q_BL, AFLAG_ACTIVE, 50, BLK_NO, LS_READY, LS_READY, 150}, // LS_PARRY_UR,
	{"BParry UL", BOTH_H1_S1_TL, Q_TL, Q_BR, AFLAG_ACTIVE, 50, BLK_NO, LS_READY, LS_READY, 150}, // LS_PARRY_UL,
	{"BParry LR", BOTH_H1_S1_BR, Q_BL, Q_TR, AFLAG_ACTIVE, 50, BLK_NO, LS_READY, LS_READY, 150}, // LS_PARRY_LR,
	{"BParry Bot", BOTH_H1_S1_B_, Q_B, Q_T, AFLAG_ACTIVE, 50, BLK_NO, LS_READY, LS_READY, 150}, // LS_PARRY_LL
	{"BParry LL", BOTH_H1_S1_BL, Q_BR, Q_TL, AFLAG_ACTIVE, 50, BLK_NO, LS_READY, LS_READY, 150}, // LS_PARRY_LL

	// Knockaways
	{"Knock Top", BOTH_K1_S1_T_, Q_R, Q_T, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_T1_T__BR, 150}, // LS_K1_T_,
	{"Knock UR", BOTH_K1_S1_TR, Q_R, Q_TR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_T1_TR__R, 150}, // LS_K1_TR,
	{"Knock UR_MD", BOTH_K1_S1_TR_MD, Q_R, Q_TR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_T1_TR__R, 150},
	// LS_K1_TR_MD,
	{"Knock UR_PB", BOTH_K1_S1_TR_PB, Q_R, Q_TR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_T1_TR__R, 150},
	// LS_K1_TR_PB,
	{"Knock UL", BOTH_K1_S1_TL, Q_R, Q_TL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BR2TL, LS_T1_TL__L, 150}, // LS_K1_TL,
	{"Knock UL_MD", BOTH_K1_S1_TL_MD, Q_R, Q_TL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BR2TL, LS_T1_TL__L, 150},
	// LS_K1_TL_MD,
	{"Knock UL_PB", BOTH_K1_S1_TL_PB, Q_R, Q_TL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BR2TL, LS_T1_TL__L, 150},
	// LS_K1_TL_PB,
	{"Knock LR", BOTH_K1_S1_BR, Q_R, Q_BL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TL2BR, LS_T1_BL_TL, 150}, // LS_K1_BR,
	{"Knock LL", BOTH_K1_S1_BL, Q_R, Q_BR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TR2BL, LS_T1_BR_TR, 150}, // LS_K1_BL

	// Parry
	{"Parry Top", BOTH_P1_S1_T_, Q_R, Q_T, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_A_T2B, 150}, // LS_PARRY_UP,
	{"Parry UR", BOTH_P1_S1_TR, Q_R, Q_TL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_A_TR2BL, 150}, // LS_PARRY_UR,
	{"Parry UL", BOTH_P1_S1_TL, Q_R, Q_TR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BR2TL, LS_A_TL2BR, 150}, // LS_PARRY_UL,
	{"Parry LR", BOTH_P1_S1_BR, Q_R, Q_BR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TL2BR, LS_A_BR2TL, 150}, // LS_PARRY_LR,
	{"Parry LL", BOTH_P1_S1_BL, Q_R, Q_BL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TR2BL, LS_A_BL2TR, 150}, // LS_PARRY_LL
	{"parry B", BOTH_P1_S1_B_, Q_R, Q_B, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TR2BL, LS_R_T2B, 150}, // LS_PARRY_B

	// Reflecting a missile
	{"Reflect Top", BOTH_P1_S1_T_, Q_R, Q_T, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_A_T2B, 150}, // LS_PARRY_UP,
	{"Reflect UR", BOTH_P1_S1_TR, Q_R, Q_TL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BL2TR, LS_A_TR2BL, 150}, // LS_PARRY_UR,
	{"Reflect UL", BOTH_P1_S1_TL, Q_R, Q_TR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_BR2TL, LS_A_TL2BR, 150}, // LS_PARRY_UL,
	{"Reflect LR", BOTH_P1_S1_BR, Q_R, Q_BR, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TL2BR, LS_A_BR2TL, 150}, // LS_PARRY_LR,
	{"Reflect LL", BOTH_P1_S1_BL, Q_R, Q_BL, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TR2BL, LS_A_BL2TR, 150}, // LS_PARRY_LL
	{"Reflect B", BOTH_P1_S1_B_, Q_R, Q_B, AFLAG_ACTIVE, 50, BLK_WIDE, LS_R_TR2BL, LS_R_T2B, 150}, // LS_PARRY_B
};

saber_moveName_t transitionMove[Q_NUM_QUADS][Q_NUM_QUADS] =
{
	{
		LS_NONE, //Can't transition to same pos!
		LS_T1_BR__R, //40
		LS_T1_BR_TR,
		LS_T1_BR_T_,
		LS_T1_BR_TL,
		LS_T1_BR__L,
		LS_T1_BR_BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1__R_BR, //46
		LS_NONE, //Can't transition to same pos!
		LS_T1__R_TR,
		LS_T1__R_T_,
		LS_T1__R_TL,
		LS_T1__R__L,
		LS_T1__R_BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1_TR_BR, //52
		LS_T1_TR__R,
		LS_NONE, //Can't transition to same pos!
		LS_T1_TR_T_,
		LS_T1_TR_TL,
		LS_T1_TR__L,
		LS_T1_TR_BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1_T__BR, //58
		LS_T1_T___R,
		LS_T1_T__TR,
		LS_NONE, //Can't transition to same pos!
		LS_T1_T__TL,
		LS_T1_T___L,
		LS_T1_T__BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1_TL_BR, //64
		LS_T1_TL__R,
		LS_T1_TL_TR,
		LS_T1_TL_T_,
		LS_NONE, //Can't transition to same pos!
		LS_T1_TL__L,
		LS_T1_TL_BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1__L_BR, //70
		LS_T1__L__R,
		LS_T1__L_TR,
		LS_T1__L_T_,
		LS_T1__L_TL,
		LS_NONE, //Can't transition to same pos!
		LS_T1__L_BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1_BL_BR, //76
		LS_T1_BL__R,
		LS_T1_BL_TR,
		LS_T1_BL_T_,
		LS_T1_BL_TL,
		LS_T1_BL__L,
		LS_NONE, //Can't transition to same pos!
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	},
	{
		LS_T1_BL_BR, //NOTE: there are no transitions from bottom, so re-use the bottom right transitions
		LS_T1_BR__R,
		LS_T1_BR_TR,
		LS_T1_BR_T_,
		LS_T1_BR_TL,
		LS_T1_BR__L,
		LS_T1_BR_BL,
		LS_NONE //No transitions to bottom, and no anims start there, so shouldn't need any
	}
};

void PM_VelocityForSaberMove(const playerState_t* ps, vec3_t throw_dir)
{
	vec3_t v_forward = { 0.0f }, v_right = { 0.0f }, v_up = { 0.0f }, start_q = { 0.0f }, end_q = { 0.0f };

	AngleVectors(ps->viewangles, v_forward, v_right, v_up);

	switch (saber_moveData[ps->saber_move].startQuad)
	{
	case Q_BR:
		VectorScale(v_right, 1, start_q);
		VectorMA(start_q, -1, v_up, start_q);
		break;
	case Q_R:
		VectorScale(v_right, 2, start_q);
		break;
	case Q_TR:
		VectorScale(v_right, 1, start_q);
		VectorMA(start_q, 1, v_up, start_q);
		break;
	case Q_T:
		VectorScale(v_up, 2, start_q);
		break;
	case Q_TL:
		VectorScale(v_right, -1, start_q);
		VectorMA(start_q, 1, v_up, start_q);
		break;
	case Q_L:
		VectorScale(v_right, -2, start_q);
		break;
	case Q_BL:
		VectorScale(v_right, -1, start_q);
		VectorMA(start_q, -1, v_up, start_q);
		break;
	case Q_B:
		VectorScale(v_up, -2, start_q);
		break;
	default:;
	}
	switch (saber_moveData[ps->saber_move].endQuad)
	{
	case Q_BR:
		VectorScale(v_right, 1, end_q);
		VectorMA(end_q, -1, v_up, end_q);
		break;
	case Q_R:
		VectorScale(v_right, 2, end_q);
		break;
	case Q_TR:
		VectorScale(v_right, 1, end_q);
		VectorMA(end_q, 1, v_up, end_q);
		break;
	case Q_T:
		VectorScale(v_up, 2, end_q);
		break;
	case Q_TL:
		VectorScale(v_right, -1, end_q);
		VectorMA(end_q, 1, v_up, end_q);
		break;
	case Q_L:
		VectorScale(v_right, -2, end_q);
		break;
	case Q_BL:
		VectorScale(v_right, -1, end_q);
		VectorMA(end_q, -1, v_up, end_q);
		break;
	case Q_B:
		VectorScale(v_up, -2, end_q);
		break;
	default:;
	}
	VectorMA(end_q, 2, v_forward, end_q);
	VectorScale(throw_dir, 125, throw_dir); //FIXME: pass in the throw strength?
	VectorSubtract(end_q, start_q, throw_dir);
}

qboolean PM_VelocityForBlockedMove(const playerState_t* ps, vec3_t throw_dir)
{
	vec3_t v_forward, v_right, v_up;
	AngleVectors(ps->viewangles, v_forward, v_right, v_up);
	switch (ps->saberBlocked)
	{
	case BLOCKED_UPPER_RIGHT:
		VectorScale(v_right, 1, throw_dir);
		VectorMA(throw_dir, 1, v_up, throw_dir);
		break;
	case BLOCKED_UPPER_LEFT:
		VectorScale(v_right, -1, throw_dir);
		VectorMA(throw_dir, 1, v_up, throw_dir);
		break;
	case BLOCKED_LOWER_RIGHT:
		VectorScale(v_right, 1, throw_dir);
		VectorMA(throw_dir, -1, v_up, throw_dir);
		break;
	case BLOCKED_LOWER_LEFT:
		VectorScale(v_right, -1, throw_dir);
		VectorMA(throw_dir, -1, v_up, throw_dir);
		break;
	case BLOCKED_TOP:
		VectorScale(v_up, 2, throw_dir);
		break;
	case BLOCKED_BACK:
		VectorScale(v_up, 2, throw_dir);
		break;
	default:
		return qfalse;
	}
	VectorMA(throw_dir, 2, v_forward, throw_dir);
	VectorScale(throw_dir, 250, throw_dir);
	return qtrue;
}

static int PM_AnimLevelForSaberAnim(const int anim)
{
	if (anim >= BOTH_A1_T__B_ && anim <= BOTH_D1_B____)
	{
		return FORCE_LEVEL_1;
	}
	if (anim >= BOTH_A2_T__B_ && anim <= BOTH_D2_B____)
	{
		return FORCE_LEVEL_2;
	}
	if (anim >= BOTH_A3_T__B_ && anim <= BOTH_D3_B____)
	{
		return FORCE_LEVEL_3;
	}
	if (anim >= BOTH_A4_T__B_ && anim <= BOTH_D4_B____)
	{
		//desann
		return FORCE_LEVEL_4;
	}
	if (anim >= BOTH_A5_T__B_ && anim <= BOTH_D5_B____)
	{
		//tavion
		return FORCE_LEVEL_5;
	}
	if (anim >= BOTH_A6_T__B_ && anim <= BOTH_D6_B____)
	{
		//dual
		return SS_DUAL;
	}
	if (anim >= BOTH_A7_T__B_ && anim <= BOTH_D7_B____)
	{
		//staff
		return SS_STAFF;
	}
	return FORCE_LEVEL_0;
}

int PM_PowerLevelForSaberAnim(const playerState_t* ps, const int saber_num)
{
	int anim = ps->torsoAnim;
	const int anim_time_elapsed = PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
		static_cast<animNumber_t>(anim)) - ps->torsoAnimTimer;
	if (anim >= BOTH_A1_T__B_ && anim <= BOTH_D1_B____)
	{
		//FIXME: these two need their own style
		if (ps->saber[0].type == SABER_LANCE)
		{
			return FORCE_LEVEL_4;
		}
		if (ps->saber[0].type == SABER_TRIDENT)
		{
			return FORCE_LEVEL_3;
		}
		return FORCE_LEVEL_1;
	}
	if (anim >= BOTH_A2_T__B_ && anim <= BOTH_D2_B____)
	{
		return FORCE_LEVEL_2;
	}
	if (anim >= BOTH_A3_T__B_ && anim <= BOTH_D3_B____)
	{
		return FORCE_LEVEL_3;
	}
	if (anim >= BOTH_A4_T__B_ && anim <= BOTH_D4_B____)
	{
		//desann
		return FORCE_LEVEL_4;
	}
	if (anim >= BOTH_A5_T__B_ && anim <= BOTH_D5_B____)
	{
		//tavion
		return FORCE_LEVEL_2;
	}
	if (anim >= BOTH_A6_T__B_ && anim <= BOTH_D6_B____)
	{
		//dual
		return FORCE_LEVEL_2;
	}
	if (anim >= BOTH_A7_T__B_ && anim <= BOTH_D7_B____)
	{
		//staff
		return FORCE_LEVEL_2;
	}
	if (anim >= BOTH_P1_S1_T_ && anim <= BOTH_P1_S1_BR
		|| anim >= BOTH_P6_S6_T_ && anim <= BOTH_P6_S6_BR
		|| anim >= BOTH_P7_S7_T_ && anim <= BOTH_P7_S7_BR)
	{
		//parries
		switch (ps->saber_anim_level)
		{
		case SS_STRONG:
		case SS_DESANN:
			return FORCE_LEVEL_3;
		case SS_TAVION:
		case SS_STAFF:
		case SS_DUAL:
		case SS_MEDIUM:
			return FORCE_LEVEL_2;
		case SS_FAST:
			return FORCE_LEVEL_1;
		default:
			return FORCE_LEVEL_0;
		}
	}
	if (anim >= BOTH_K1_S1_T_ && anim <= BOTH_K1_S1_BR
		|| anim >= BOTH_K6_S6_T_ && anim <= BOTH_K6_S6_BR
		|| anim >= BOTH_K7_S7_T_ && anim <= BOTH_K7_S7_BR)
	{
		//knockaways
		return FORCE_LEVEL_3;
	}
	if (anim >= BOTH_V1_BR_S1 && anim <= BOTH_V1_B__S1
		|| anim >= BOTH_V6_BR_S6 && anim <= BOTH_V6_B__S6
		|| anim >= BOTH_V7_BR_S7 && anim <= BOTH_V7_B__S7)
	{
		//knocked-away attacks
		return FORCE_LEVEL_1;
	}
	if (anim >= BOTH_H1_S1_T_ && anim <= BOTH_H1_S1_BR
		|| anim >= BOTH_H6_S6_T_ && anim <= BOTH_H6_S6_BR
		|| anim >= BOTH_H7_S7_T_ && anim <= BOTH_H7_S7_BR)
	{
		//broken parries
		return FORCE_LEVEL_0;
	}
	switch (anim)
	{
	case BOTH_A2_STABBACK1:
	{
		if (ps->torsoAnimTimer < 450)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 400)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_ATTACK_BACK:
		if (ps->torsoAnimTimer < 500)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_3;
	case BOTH_CROUCHATTACKBACK1:
		if (ps->torsoAnimTimer < 800)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_3;
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
		//FIXME: break up?
		return FORCE_LEVEL_3;
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
		//FIXME: break up?
		return FORCE_LEVEL_3;
	case BOTH_K1_S1_T_: //# knockaway saber top
	case BOTH_K1_S1_TR: //# knockaway saber top right
	case BOTH_K1_S1_TR_MD: //# knockaway saber top right
	case BOTH_K1_S1_TR_PB: //# knockaway saber top right
	case BOTH_K1_S1_TL: //# knockaway saber top left
	case BOTH_K1_S1_TL_MD: //# knockaway saber top left
	case BOTH_K1_S1_TL_PB: //# knockaway saber top left
	case BOTH_K1_S1_BL: //# knockaway saber bottom left
	case BOTH_K1_S1_B_: //# knockaway saber bottom
	case BOTH_K1_S1_BR: //# knockaway saber bottom right
		return FORCE_LEVEL_3;
	case BOTH_LUNGE2_B__T_:
	{
		if (ps->torsoAnimTimer < 400)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 150)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_FORCELEAP2_T__B_:
	{
		if (ps->torsoAnimTimer < 400)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 550)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_VS_ATR_S:
	case BOTH_VS_ATL_S:
	case BOTH_VT_ATR_S:
	case BOTH_VT_ATL_S:
		return FORCE_LEVEL_3; //???
	case BOTH_JUMPFLIPSLASHDOWN1:
	{
		if (ps->torsoAnimTimer <= 900)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 550)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_JUMPFLIPSTABDOWN:
	{
		if (ps->torsoAnimTimer <= 1200)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed <= 250)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_JUMPATTACK6:
		if (ps->torsoAnimTimer >= 1450
			&& anim_time_elapsed >= 400
			|| ps->torsoAnimTimer >= 400
			&& anim_time_elapsed >= 1100)
		{
			//pretty much sideways
			return FORCE_LEVEL_3;
		}
		return FORCE_LEVEL_0;
	case BOTH_JUMPATTACK7:
	{
		if (ps->torsoAnimTimer <= 1200)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_SPINATTACK6:
		if (anim_time_elapsed <= 200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_3;
	case BOTH_SPINATTACK7:
	{
		if (ps->torsoAnimTimer <= 500)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 500)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_FORCELONGLEAP_ATTACK:
		if (anim_time_elapsed <= 200)
		{
			//1st four frames of anim
			return FORCE_LEVEL_3;
		}
		break;
	case BOTH_STABDOWN:
		if (ps->torsoAnimTimer <= 900)
		{
			//end of anim
			return FORCE_LEVEL_3;
		}
		break;
	case BOTH_STABDOWN_STAFF:
		if (ps->torsoAnimTimer <= 850)
		{
			//end of anim
			return FORCE_LEVEL_3;
		}
		break;
	case BOTH_STABDOWN_DUAL:
		if (ps->torsoAnimTimer <= 900)
		{
			//end of anim
			return FORCE_LEVEL_3;
		}
		break;
	case BOTH_A6_SABERPROTECT:
		if (ps->torsoAnimTimer < 650)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_3;
	case BOTH_A7_SOULCAL:
	{
		if (ps->torsoAnimTimer < 650)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 600)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_A1_SPECIAL:
	{
		if (ps->torsoAnimTimer < 600)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_A2_SPECIAL:
	{
		if (ps->torsoAnimTimer < 300)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_A3_SPECIAL:
	{
		if (ps->torsoAnimTimer < 700)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_FLIP_ATTACK7:
		return FORCE_LEVEL_3;
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_GRAPPLE_FIRE:
		if (ps->torsoAnimTimer < 1000)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_3;
	case BOTH_PULL_IMPALE_SWING:
	{
		if (ps->torsoAnimTimer < 500) //750 )
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 650) //600 )
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_ALORA_SPIN_SLASH:
	{
		if (ps->torsoAnimTimer < 900)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 250)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_A6_FB:
	{
		if (ps->torsoAnimTimer < 250)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 250)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_A6_LR:
	{
		if (ps->torsoAnimTimer < 250)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 250)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	case BOTH_A7_HILT:
	case BOTH_SLAP_R:
	case BOTH_SLAP_L:
		return FORCE_LEVEL_0;
		//===SABERLOCK SUPERBREAKS START===========================================================================
	case BOTH_LK_S_DL_T_SB_1_W:
		if (ps->torsoAnimTimer < 700)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_5;
	case BOTH_LK_S_ST_S_SB_1_W:
		if (ps->torsoAnimTimer < 300)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_5;
	case BOTH_LK_S_DL_S_SB_1_W:
	case BOTH_LK_S_S_S_SB_1_W:
	{
		if (ps->torsoAnimTimer < 700)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 400)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_5;
	case BOTH_LK_S_ST_T_SB_1_W:
	case BOTH_LK_S_S_T_SB_1_W:
	{
		if (ps->torsoAnimTimer < 150)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 400)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_5;
	case BOTH_LK_DL_DL_T_SB_1_W:
		return FORCE_LEVEL_5;
	case BOTH_LK_DL_DL_S_SB_1_W:
	case BOTH_LK_DL_ST_S_SB_1_W:
		if (anim_time_elapsed < 1000)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_5;
	case BOTH_LK_DL_ST_T_SB_1_W:
	{
		if (ps->torsoAnimTimer < 950)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 650)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_5;
	case BOTH_LK_DL_S_S_SB_1_W:
		if (saber_num != 0)
		{
			//only right hand saber does damage in this suberbreak
			return FORCE_LEVEL_0;
		}
		if (ps->torsoAnimTimer < 900)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 450)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_5;
	case BOTH_LK_DL_S_T_SB_1_W:
		if (saber_num != 0)
		{
			//only right hand saber does damage in this suberbreak
			return FORCE_LEVEL_0;
		}
		if (ps->torsoAnimTimer < 250)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 150)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_5;
	case BOTH_LK_ST_DL_S_SB_1_W:
		return FORCE_LEVEL_5;
	case BOTH_LK_ST_DL_T_SB_1_W:
		//special suberbreak - doesn't kill, just kicks them backwards
		return FORCE_LEVEL_0;
	case BOTH_LK_ST_ST_S_SB_1_W:
	case BOTH_LK_ST_S_S_SB_1_W:
	{
		if (ps->torsoAnimTimer < 800)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 350)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_5;
	case BOTH_LK_ST_ST_T_SB_1_W:
	case BOTH_LK_ST_S_T_SB_1_W:
		return FORCE_LEVEL_5;
		//===SABERLOCK SUPERBREAKS START===========================================================================
	case BOTH_HANG_ATTACK:
		//FIME: break up
	{
		//FIME: break up
		if (ps->torsoAnimTimer < 1000)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (anim_time_elapsed < 250)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
		//sweet spot
		return FORCE_LEVEL_5;
	}
	case BOTH_ROLL_STAB:
	{
		if (anim_time_elapsed > 400)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		return FORCE_LEVEL_3;
	}
	default:;
	}
	return FORCE_LEVEL_0;
}

qboolean PM_InAnimForSaberMove(int anim, const int saber_move)
{
	switch (anim)
	{
		//special case anims
	case BOTH_A2_STABBACK1:
	case BOTH_ATTACK_BACK:
	case BOTH_CROUCHATTACKBACK1:
	case BOTH_ROLL_STAB:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_LUNGE2_B__T_:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPFLIPSLASHDOWN1: //#
	case BOTH_JUMPFLIPSTABDOWN: //#
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_SPINATTACK6:
	case BOTH_SPINATTACK7:
	case BOTH_VS_ATR_S:
	case BOTH_VS_ATL_S:
	case BOTH_VT_ATR_S:
	case BOTH_VT_ATL_S:
	case BOTH_FORCELONGLEAP_ATTACK:
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
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_FLIP_ATTACK7:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_ALORA_SPIN_SLASH:
	case BOTH_A6_FB:
	case BOTH_A6_LR:
	case BOTH_A7_HILT:
	case BOTH_SLAP_L:
	case BOTH_SLAP_R:
	case BOTH_A7_SLAP_R:
	case BOTH_A7_SLAP_L:
	case BOTH_LK_S_DL_S_SB_1_W:
	case BOTH_LK_S_DL_T_SB_1_W:
	case BOTH_LK_S_ST_S_SB_1_W:
	case BOTH_LK_S_ST_T_SB_1_W:
	case BOTH_LK_S_S_S_SB_1_W:
	case BOTH_LK_S_S_T_SB_1_W:
	case BOTH_LK_DL_DL_S_SB_1_W:
	case BOTH_LK_DL_DL_T_SB_1_W:
	case BOTH_LK_DL_ST_S_SB_1_W:
	case BOTH_LK_DL_ST_T_SB_1_W:
	case BOTH_LK_DL_S_S_SB_1_W:
	case BOTH_LK_DL_S_T_SB_1_W:
	case BOTH_LK_ST_DL_S_SB_1_W:
	case BOTH_LK_ST_DL_T_SB_1_W:
	case BOTH_LK_ST_ST_S_SB_1_W:
	case BOTH_LK_ST_ST_T_SB_1_W:
	case BOTH_LK_ST_S_S_SB_1_W:
	case BOTH_LK_ST_S_T_SB_1_W:
	case BOTH_HANG_ATTACK:
	case BOTH_GRAPPLE_FIRE:
		return qtrue;
	default:;
	}
	if (PM_SaberDrawPutawayAnim(anim))
	{
		if (saber_move == LS_DRAW || saber_move == LS_PUTAWAY)
		{
			return qtrue;
		}
		return qfalse;
	}
	if (PM_SaberStanceAnim(anim))
	{
		if (saber_move == LS_READY)
		{
			return qtrue;
		}
		return qfalse;
	}
	const int anim_level = PM_AnimLevelForSaberAnim(anim);
	if (anim_level <= 0)
	{
		//NOTE: this will always return false for the ready poses and putaway/draw...
		return qfalse;
	}
	//drop the anim to the first level and start the checks there
	anim -= (anim_level - FORCE_LEVEL_1) * SABER_ANIM_GROUP_SIZE;
	//check level 1
	if (anim == saber_moveData[saber_move].animToUse)
	{
		return qtrue;
	}
	//check level 2
	anim += SABER_ANIM_GROUP_SIZE;
	if (anim == saber_moveData[saber_move].animToUse)
	{
		return qtrue;
	}
	//check level 3
	anim += SABER_ANIM_GROUP_SIZE;
	if (anim == saber_moveData[saber_move].animToUse)
	{
		return qtrue;
	}
	//check level 4
	anim += SABER_ANIM_GROUP_SIZE;
	if (anim == saber_moveData[saber_move].animToUse)
	{
		return qtrue;
	}
	//check level 5
	anim += SABER_ANIM_GROUP_SIZE;
	if (anim == saber_moveData[saber_move].animToUse)
	{
		return qtrue;
	}
	if (anim >= BOTH_P1_S1_T_ && anim <= BOTH_H1_S1_BR)
	{
		//parries, knockaways and broken parries
		return static_cast<qboolean>(anim == saber_moveData[saber_move].animToUse);
	}
	return qfalse;
}

qboolean PM_SaberInDamageMove(const int move)
{
	if (move >= LS_A_TL2BR && move <= LS_A_T2B)
	{
		return qtrue;
	}
	switch (move)
	{
	case LS_A_BACK:
	case LS_A_BACK_CR:
	case LS_A_BACKSTAB:
	case LS_ROLL_STAB:
	case LS_A_LUNGE:
	case LS_A_JUMP_T__B_:
	case LS_A_FLIP_STAB:
	case LS_A_FLIP_SLASH:
	case LS_JUMPATTACK_DUAL:
	case LS_JUMPATTACK_ARIAL_LEFT:
	case LS_JUMPATTACK_ARIAL_RIGHT:
	case LS_JUMPATTACK_CART_LEFT:
	case LS_JUMPATTACK_CART_RIGHT:
	case LS_JUMPATTACK_STAFF_LEFT:
	case LS_JUMPATTACK_STAFF_RIGHT:
	case LS_BUTTERFLY_LEFT:
	case LS_BUTTERFLY_RIGHT:
	case LS_A_BACKFLIP_ATK:
	case LS_SPINATTACK_DUAL:
	case LS_SPINATTACK:
	case LS_LEAP_ATTACK:
	case LS_SWOOP_ATTACK_RIGHT:
	case LS_SWOOP_ATTACK_LEFT:
	case LS_TAUNTAUN_ATTACK_RIGHT:
	case LS_TAUNTAUN_ATTACK_LEFT:
	case LS_STABDOWN:
	case LS_STABDOWN_STAFF:
	case LS_STABDOWN_DUAL:
	case LS_DUAL_SPIN_PROTECT:
	case LS_STAFF_SOULCAL:
	case LS_A1_SPECIAL:
	case LS_A2_SPECIAL:
	case LS_A3_SPECIAL:
	case LS_UPSIDE_DOWN_ATTACK:
	case LS_PULL_ATTACK_STAB:
	case LS_GRAPPLE_FIRE:
	case LS_PULL_ATTACK_SWING:
	case LS_SPINATTACK_ALORA:
	case LS_DUAL_FB:
	case LS_DUAL_LR:
	case LS_HILT_BASH:
	case LS_SLAP_R:
	case LS_SLAP_L:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberDoDamageAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_A2_STABBACK1:
	case BOTH_ATTACK_BACK:
	case BOTH_CROUCHATTACKBACK1:
	case BOTH_ROLL_STAB:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_LUNGE2_B__T_:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPFLIPSLASHDOWN1: //#
	case BOTH_JUMPFLIPSTABDOWN: //#
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_SPINATTACK6:
	case BOTH_SPINATTACK7:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_VS_ATR_S:
	case BOTH_VS_ATL_S:
	case BOTH_VT_ATR_S:
	case BOTH_VT_ATL_S:
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_FLIP_ATTACK7:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_ALORA_SPIN_SLASH:
	case BOTH_A6_FB:
	case BOTH_A6_LR:
	case BOTH_A7_HILT:
	case BOTH_SLAP_L:
	case BOTH_SLAP_R:
	case BOTH_LK_S_DL_S_SB_1_W:
	case BOTH_LK_S_DL_T_SB_1_W:
	case BOTH_LK_S_ST_S_SB_1_W:
	case BOTH_LK_S_ST_T_SB_1_W:
	case BOTH_LK_S_S_S_SB_1_W:
	case BOTH_LK_S_S_T_SB_1_W:
	case BOTH_LK_DL_DL_S_SB_1_W:
	case BOTH_LK_DL_DL_T_SB_1_W:
	case BOTH_LK_DL_ST_S_SB_1_W:
	case BOTH_LK_DL_ST_T_SB_1_W:
	case BOTH_LK_DL_S_S_SB_1_W:
	case BOTH_LK_DL_S_T_SB_1_W:
	case BOTH_LK_ST_DL_S_SB_1_W:
	case BOTH_LK_ST_DL_T_SB_1_W:
	case BOTH_LK_ST_ST_S_SB_1_W:
	case BOTH_LK_ST_ST_T_SB_1_W:
	case BOTH_LK_ST_S_S_SB_1_W:
	case BOTH_LK_ST_S_T_SB_1_W:
	case BOTH_HANG_ATTACK:
	case BOTH_GRAPPLE_FIRE:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInIdle(const int move)
{
	switch (move)
	{
	case LS_NONE:
	case LS_READY:
	case LS_DRAW:
	case LS_PUTAWAY:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_BounceAnim(const int anim)
{
	//check for saber bounce animation
	if (anim >= BOTH_B1_BR___ && anim <= BOTH_B1_BL___
		|| anim >= BOTH_B2_BR___ && anim <= BOTH_B2_BL___
		|| anim >= BOTH_B3_BR___ && anim <= BOTH_B3_BL___
		|| anim >= BOTH_B4_BR___ && anim <= BOTH_B4_BL___
		|| anim >= BOTH_B5_BR___ && anim <= BOTH_B5_BL___
		|| anim >= BOTH_B6_BR___ && anim <= BOTH_B6_BL___
		|| anim >= BOTH_B7_BR___ && anim <= BOTH_B7_BL___)
	{
		return qtrue;
	}

	return qfalse;
}

qboolean PM_SaberReturnAnim(const int anim)
{
	if (anim >= BOTH_R1_B__S1 && anim <= BOTH_R1_TR_S1
		|| anim >= BOTH_R2_B__S1 && anim <= BOTH_R2_TR_S1
		|| anim >= BOTH_R3_B__S1 && anim <= BOTH_R3_TR_S1
		|| anim >= BOTH_R4_B__S1 && anim <= BOTH_R4_TR_S1
		|| anim >= BOTH_R5_B__S1 && anim <= BOTH_R5_TR_S1
		|| anim >= BOTH_R6_B__S6 && anim <= BOTH_R6_TR_S6
		|| anim >= BOTH_R7_B__S7 && anim <= BOTH_R7_TR_S7)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean pm_saber_in_special_attack(const int anim)
{
	switch (anim)
	{
	case BOTH_A2_STABBACK1:
	case BOTH_ATTACK_BACK:
	case BOTH_CROUCHATTACKBACK1:
	case BOTH_ROLL_STAB:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_LUNGE2_B__T_:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPFLIPSLASHDOWN1: //#
	case BOTH_JUMPFLIPSTABDOWN: //#
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_SPINATTACK6:
	case BOTH_SPINATTACK7:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_VS_ATR_S:
	case BOTH_VS_ATL_S:
	case BOTH_VT_ATR_S:
	case BOTH_VT_ATL_S:
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
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_FLIP_ATTACK7:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_ALORA_SPIN_SLASH:
	case BOTH_A6_FB:
	case BOTH_A6_LR:
	case BOTH_A7_HILT:
	case BOTH_SLAP_L:
	case BOTH_SLAP_R:
	case BOTH_A7_SLAP_R:
	case BOTH_A7_SLAP_L:
	case BOTH_LK_S_DL_S_SB_1_W:
	case BOTH_LK_S_DL_T_SB_1_W:
	case BOTH_LK_S_ST_S_SB_1_W:
	case BOTH_LK_S_ST_T_SB_1_W:
	case BOTH_LK_S_S_S_SB_1_W:
	case BOTH_LK_S_S_T_SB_1_W:
	case BOTH_LK_DL_DL_S_SB_1_W:
	case BOTH_LK_DL_DL_T_SB_1_W:
	case BOTH_LK_DL_ST_S_SB_1_W:
	case BOTH_LK_DL_ST_T_SB_1_W:
	case BOTH_LK_DL_S_S_SB_1_W:
	case BOTH_LK_DL_S_T_SB_1_W:
	case BOTH_LK_ST_DL_S_SB_1_W:
	case BOTH_LK_ST_DL_T_SB_1_W:
	case BOTH_LK_ST_ST_S_SB_1_W:
	case BOTH_LK_ST_ST_T_SB_1_W:
	case BOTH_LK_ST_S_S_SB_1_W:
	case BOTH_LK_ST_S_T_SB_1_W:
	case BOTH_HANG_ATTACK:
	case BOTH_GRAPPLE_FIRE:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInInstankillKillAttack(const int anim)
{
	switch (anim)
	{
	case BOTH_FORCELONGLEAP_ATTACK:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInKillAttack(const int anim)
{
	switch (anim)
	{
	case BOTH_ROLL_STAB:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
		//case BOTH_LUNGE2_B__T_:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_SPINATTACK6:
	case BOTH_SPINATTACK7:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_VS_ATR_S:
	case BOTH_VS_ATL_S:
	case BOTH_VT_ATR_S:
	case BOTH_VT_ATL_S:
	case BOTH_FLIP_ATTACK7:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_ALORA_SPIN_SLASH:
	case BOTH_A6_FB:
	case BOTH_A6_LR:
	case BOTH_HANG_ATTACK:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInLockWin(const int anim)
{
	switch (anim)
	{
	case BOTH_LK_S_DL_S_SB_1_W:
	case BOTH_LK_S_DL_T_SB_1_W:
	case BOTH_LK_S_ST_S_SB_1_W:
	case BOTH_LK_S_ST_T_SB_1_W:
	case BOTH_LK_S_S_S_SB_1_W:
	case BOTH_LK_S_S_T_SB_1_W:
	case BOTH_LK_DL_DL_S_SB_1_W:
	case BOTH_LK_DL_DL_T_SB_1_W:
	case BOTH_LK_DL_ST_S_SB_1_W:
	case BOTH_LK_DL_ST_T_SB_1_W:
	case BOTH_LK_DL_S_S_SB_1_W:
	case BOTH_LK_DL_S_T_SB_1_W:
	case BOTH_LK_ST_DL_S_SB_1_W:
	case BOTH_LK_ST_DL_T_SB_1_W:
	case BOTH_LK_ST_ST_S_SB_1_W:
	case BOTH_LK_ST_ST_T_SB_1_W:
	case BOTH_LK_ST_S_S_SB_1_W:
	case BOTH_LK_ST_S_T_SB_1_W:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean pm_saber_innonblockable_attack(const int anim)
{
	switch (anim)
	{
	case BOTH_ATTACK_BACK:
	case BOTH_A2_STABBACK1:
	case BOTH_CROUCHATTACKBACK1:
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
	case BOTH_SPINATTACK6:
	case BOTH_SPINATTACK7:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_FLIP_ATTACK7:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_ALORA_SPIN_SLASH:
	case BOTH_A6_FB:
	case BOTH_A6_LR:
	case BOTH_GRAPPLE_FIRE:
		return qtrue;
	default:;
	}
	return qfalse;
}

int BG_InGrappleMove(const int anim)
{
	switch (anim)
	{
	case BOTH_KYLE_GRAB:
	case BOTH_KYLE_MISS:
		return 1; //grabbing at someone
	case BOTH_KYLE_PA_1:
	case BOTH_KYLE_PA_2:
	case BOTH_KYLE_PA_3:
		return 2; //beating the shit out of someone
	case BOTH_PLAYER_PA_1:
	case BOTH_PLAYER_PA_2:
	case BOTH_PLAYER_PA_3:
	case BOTH_PLAYER_PA_FLY:
		return 3; //getting the shit beaten out of you
	default:;
	}

	return 0;
}

qboolean PM_SaberInAttack(const int move)
{
	if (move >= LS_A_TL2BR && move <= LS_A_T2B)
	{
		return qtrue;
	}
	switch (move)
	{
	case LS_A_BACK:
	case LS_A_BACK_CR:
	case LS_A_BACKSTAB:
	case LS_ROLL_STAB:
	case LS_A_LUNGE:
	case LS_A_JUMP_T__B_:
	case LS_A_FLIP_STAB:
	case LS_A_FLIP_SLASH:
	case LS_JUMPATTACK_DUAL:
	case LS_JUMPATTACK_ARIAL_LEFT:
	case LS_JUMPATTACK_ARIAL_RIGHT:
	case LS_JUMPATTACK_CART_LEFT:
	case LS_JUMPATTACK_CART_RIGHT:
	case LS_JUMPATTACK_STAFF_LEFT:
	case LS_JUMPATTACK_STAFF_RIGHT:
	case LS_BUTTERFLY_LEFT:
	case LS_BUTTERFLY_RIGHT:
	case LS_A_BACKFLIP_ATK:
	case LS_SPINATTACK_DUAL:
	case LS_SPINATTACK:
	case LS_LEAP_ATTACK:
	case LS_SWOOP_ATTACK_RIGHT:
	case LS_SWOOP_ATTACK_LEFT:
	case LS_TAUNTAUN_ATTACK_RIGHT:
	case LS_TAUNTAUN_ATTACK_LEFT:
	case LS_KICK_F:
	case LS_KICK_F_MD:
	case LS_KICK_B:
	case LS_KICK_SWEEP:
	case LS_KICK_R:
	case LS_SMACK_R:
	case LS_KICK_L:
	case LS_SMACK_L:
	case LS_KICK_S:
	case LS_KICK_BF:
	case LS_KICK_RL:
	case LS_KICK_F_AIR:
	case LS_FLYING_KICK:
	case LS_KICK_B_AIR:
	case LS_KICK_R_AIR:
	case LS_KICK_L_AIR:
	case LS_STABDOWN:
	case LS_STABDOWN_STAFF:
	case LS_STABDOWN_DUAL:
	case LS_DUAL_SPIN_PROTECT:
	case LS_STAFF_SOULCAL:
	case LS_A1_SPECIAL:
	case LS_A2_SPECIAL:
	case LS_A3_SPECIAL:
	case LS_UPSIDE_DOWN_ATTACK:
	case LS_PULL_ATTACK_STAB:
	case LS_GRAPPLE_FIRE:
	case LS_PULL_ATTACK_SWING:
	case LS_SPINATTACK_ALORA:
	case LS_DUAL_FB:
	case LS_DUAL_LR:
	case LS_HILT_BASH:
	case LS_SLAP_R:
	case LS_SLAP_L:
		return qtrue;
	default:;
	}
	return qfalse;
}

static qboolean PM_SaberInAttackback(const int move)
{
	if (move == LS_A_BACK || move == LS_A_BACK_CR || move == LS_A_BACKSTAB)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInAttackPure(const int move)
{
	if (move >= LS_A_TL2BR && move <= LS_A_T2B)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInTransition(const int move)
{
	if (move >= LS_T1_BR__R && move <= LS_T1_BL__L)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInStart(const int move)
{
	if (move >= LS_S_TL2BR && move <= LS_S_T2B)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInReturn(const int move)
{
	if (move >= LS_R_TL2BR && move <= LS_R_T2B)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInTransitionAny(const int move)
{
	if (PM_SaberInStart(move))
	{
		return qtrue;
	}
	if (PM_SaberInTransition(move))
	{
		return qtrue;
	}
	if (PM_SaberInReturn(move))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInBounce(const int move)
{
	if (move >= LS_B1_BR && move <= LS_B1_BL)
	{
		return qtrue;
	}
	if (move >= LS_D1_BR && move <= LS_D1_BL)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInBrokenParry(const int move)
{
	if (move >= LS_V1_BR && move <= LS_V1_B_)
	{
		return qtrue;
	}
	if (move >= LS_H1_T_ && move <= LS_H1_BL)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInDeflect(const int move)
{
	if (move >= LS_D1_BR && move <= LS_D1_B_)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInParry(const int move)
{
	switch (move)
	{
	case LS_PARRY_UP:
	case LS_PARRY_UR:
	case LS_PARRY_UL:
	case LS_PARRY_LR:
	case LS_PARRY_LL:
	case LS_PARRY_B:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInKnockaway(const int move)
{
	switch (move)
	{
	case LS_K1_T_:
	case LS_K1_TR:
	case LS_K1_TR_MD:
	case LS_K1_TR_PB:
	case LS_K1_TL:
	case LS_K1_TL_MD:
	case LS_K1_TL_PB:
	case LS_K1_BR:
	case LS_K1_BL:
		//
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInReflect(const int move)
{
	if (move >= LS_REFLECT_UP && move <= LS_REFLECT_LL)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_SaberInSpecial(const int move)
{
	switch (move)
	{
	case LS_A_BACK:
	case LS_A_BACK_CR:
	case LS_A_BACKSTAB:
	case LS_ROLL_STAB:
	case LS_A_LUNGE:
	case LS_A_JUMP_T__B_:
	case LS_A_FLIP_STAB:
	case LS_A_FLIP_SLASH:
	case LS_JUMPATTACK_DUAL:
	case LS_JUMPATTACK_ARIAL_LEFT:
	case LS_JUMPATTACK_ARIAL_RIGHT:
	case LS_JUMPATTACK_CART_LEFT:
	case LS_JUMPATTACK_CART_RIGHT:
	case LS_JUMPATTACK_STAFF_LEFT:
	case LS_JUMPATTACK_STAFF_RIGHT:
	case LS_BUTTERFLY_LEFT:
	case LS_BUTTERFLY_RIGHT:
	case LS_A_BACKFLIP_ATK:
	case LS_SPINATTACK_DUAL:
	case LS_SPINATTACK:
	case LS_LEAP_ATTACK:
	case LS_SWOOP_ATTACK_RIGHT:
	case LS_SWOOP_ATTACK_LEFT:
	case LS_TAUNTAUN_ATTACK_RIGHT:
	case LS_TAUNTAUN_ATTACK_LEFT:
	case LS_KICK_F:
	case LS_KICK_F_MD:
	case LS_KICK_B:
	case LS_KICK_SWEEP:
	case LS_KICK_R:
	case LS_SMACK_R:
	case LS_KICK_L:
	case LS_SMACK_L:
	case LS_KICK_S:
	case LS_KICK_BF:
	case LS_KICK_RL:
	case LS_KICK_F_AIR:
	case LS_FLYING_KICK:
	case LS_KICK_B_AIR:
	case LS_KICK_R_AIR:
	case LS_KICK_L_AIR:
	case LS_STABDOWN:
	case LS_STABDOWN_STAFF:
	case LS_STABDOWN_DUAL:
	case LS_DUAL_SPIN_PROTECT:
	case LS_STAFF_SOULCAL:
	case LS_A1_SPECIAL:
	case LS_A2_SPECIAL:
	case LS_A3_SPECIAL:
	case LS_UPSIDE_DOWN_ATTACK:
	case LS_PULL_ATTACK_STAB:
	case LS_GRAPPLE_FIRE:
	case LS_PULL_ATTACK_SWING:
	case LS_SPINATTACK_ALORA:
	case LS_DUAL_FB:
	case LS_DUAL_LR:
	case LS_HILT_BASH:
	case LS_SLAP_R:
	case LS_SLAP_L:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_KickMove(const int move)
{
	switch (move)
	{
	case LS_KICK_F:
	case LS_KICK_F_MD:
	case LS_KICK_B:
	case LS_KICK_SWEEP:
	case LS_KICK_R:
	case LS_SMACK_R:
	case LS_KICK_L:
	case LS_SMACK_L:
	case LS_KICK_S:
	case LS_KICK_BF:
	case LS_KICK_RL:
	case LS_HILT_BASH:
	case LS_SLAP_R:
	case LS_SLAP_L:
	case LS_KICK_F_AIR:
	case LS_FLYING_KICK:
	case LS_KICK_B_AIR:
	case LS_KICK_R_AIR:
	case LS_KICK_L_AIR:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberCanInterruptMove(const int move, const int anim)
{
	if (PM_InAnimForSaberMove(anim, move))
	{
		switch (move)
		{
		case LS_A_BACK:
		case LS_A_BACK_CR:
		case LS_A_BACKSTAB:
		case LS_ROLL_STAB:
		case LS_A_LUNGE:
		case LS_A_JUMP_T__B_:
		case LS_A_FLIP_STAB:
		case LS_A_FLIP_SLASH:
		case LS_JUMPATTACK_DUAL:
		case LS_JUMPATTACK_CART_LEFT:
		case LS_JUMPATTACK_CART_RIGHT:
		case LS_JUMPATTACK_STAFF_LEFT:
		case LS_JUMPATTACK_STAFF_RIGHT:
		case LS_BUTTERFLY_LEFT:
		case LS_BUTTERFLY_RIGHT:
		case LS_A_BACKFLIP_ATK:
		case LS_SPINATTACK_DUAL:
		case LS_SPINATTACK:
		case LS_LEAP_ATTACK:
		case LS_SWOOP_ATTACK_RIGHT:
		case LS_SWOOP_ATTACK_LEFT:
		case LS_TAUNTAUN_ATTACK_RIGHT:
		case LS_TAUNTAUN_ATTACK_LEFT:
		case LS_KICK_S:
		case LS_KICK_BF:
		case LS_KICK_RL:
		case LS_STABDOWN:
		case LS_STABDOWN_STAFF:
		case LS_STABDOWN_DUAL:
		case LS_DUAL_SPIN_PROTECT:
		case LS_STAFF_SOULCAL:
		case LS_A1_SPECIAL:
		case LS_A2_SPECIAL:
		case LS_A3_SPECIAL:
		case LS_UPSIDE_DOWN_ATTACK:
		case LS_PULL_ATTACK_STAB:
		case LS_GRAPPLE_FIRE:
		case LS_PULL_ATTACK_SWING:
		case LS_SPINATTACK_ALORA:
		case LS_DUAL_FB:
		case LS_DUAL_LR:
		case LS_HILT_BASH:
		case LS_SLAP_R:
		case LS_SLAP_L:
			return qfalse;
		default:;
		}

		if (PM_SaberInAttackPure(move))
		{
			return qfalse;
		}
		if (PM_SaberInStart(move))
		{
			return qfalse;
		}
		if (PM_SaberInTransition(move))
		{
			return qfalse;
		}
		if (PM_SaberInBounce(move))
		{
			return qfalse;
		}
		if (PM_SaberInBrokenParry(move))
		{
			return qfalse;
		}
		if (PM_SaberInDeflect(move))
		{
			return qfalse;
		}
		if (PM_SaberInParry(move))
		{
			return qfalse;
		}
		if (PM_SaberInKnockaway(move))
		{
			return qfalse;
		}
		if (PM_SaberInReflect(move))
		{
			return qfalse;
		}
	}
	switch (anim)
	{
	case BOTH_A2_STABBACK1:
	case BOTH_ATTACK_BACK:
	case BOTH_CROUCHATTACKBACK1:
	case BOTH_ROLL_STAB:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_LUNGE2_B__T_:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPFLIPSLASHDOWN1: //#
	case BOTH_JUMPFLIPSTABDOWN: //#
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_SPINATTACK6:
	case BOTH_SPINATTACK7:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_VS_ATR_S:
	case BOTH_VS_ATL_S:
	case BOTH_VT_ATR_S:
	case BOTH_VT_ATL_S:
	case BOTH_A7_KICK_S:
	case BOTH_A7_KICK_BF:
	case BOTH_A7_KICK_RL:
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_FLIP_ATTACK7:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_ALORA_SPIN_SLASH:
	case BOTH_A6_FB:
	case BOTH_A6_LR:
	case BOTH_A7_HILT:
	case BOTH_SLAP_L:
	case BOTH_SLAP_R:
	case BOTH_LK_S_DL_S_SB_1_W:
	case BOTH_LK_S_DL_T_SB_1_W:
	case BOTH_LK_S_ST_S_SB_1_W:
	case BOTH_LK_S_ST_T_SB_1_W:
	case BOTH_LK_S_S_S_SB_1_W:
	case BOTH_LK_S_S_T_SB_1_W:
	case BOTH_LK_DL_DL_S_SB_1_W:
	case BOTH_LK_DL_DL_T_SB_1_W:
	case BOTH_LK_DL_ST_S_SB_1_W:
	case BOTH_LK_DL_ST_T_SB_1_W:
	case BOTH_LK_DL_S_S_SB_1_W:
	case BOTH_LK_DL_S_T_SB_1_W:
	case BOTH_LK_ST_DL_S_SB_1_W:
	case BOTH_LK_ST_DL_T_SB_1_W:
	case BOTH_LK_ST_ST_S_SB_1_W:
	case BOTH_LK_ST_ST_T_SB_1_W:
	case BOTH_LK_ST_S_S_SB_1_W:
	case BOTH_LK_ST_S_T_SB_1_W:
	case BOTH_HANG_ATTACK:
	case BOTH_GRAPPLE_FIRE:
		return qfalse;
	default:;
	}
	return qtrue;
}

saber_moveName_t PM_SaberAttackStagger(const int move)
{
	switch (saber_moveData[move].startQuad)
	{
	case Q_B:
	case Q_BR:
		return LS_H1_BR;
	case Q_R:
		return LS_H1_TR;
	case Q_TR:
		return LS_H1_TR;
	case Q_T:
		return LS_H1_T_;
	case Q_TL:
		return LS_H1_TL;
	case Q_L:
		return LS_H1_TL;
	case Q_BL:
		return LS_H1_BL;
	default:;
	}
	return LS_NONE;
}

saber_moveName_t PM_BrokenParryForAttack(const int move)
{
	switch (saber_moveData[move].startQuad)
	{
	case Q_B:
		return LS_V1_B_;
	case Q_BR:
		return LS_V1_BR;
	case Q_R:
		return LS_V1__R;
	case Q_TR:
		return LS_V1_TR;
	case Q_T:
		return LS_V1_T_;
	case Q_TL:
		return LS_V1_TL;
	case Q_L:
		return LS_V1__L;
	case Q_BL:
		return LS_V1_BL;
	default:;
	}
	return LS_NONE;
}

qboolean PM_KnockAwayStaffAndDuels(const int move)
{
	switch (move)
	{
	case BOTH_K6_S6_BL:
	case BOTH_K6_S6_BR:
	case BOTH_K6_S6_B_:
	case BOTH_K6_S6_TL:
	case BOTH_K6_S6_TR:
	case BOTH_K6_S6_T_:
	case BOTH_K7_S7_BL:
	case BOTH_K7_S7_BR:
	case BOTH_K7_S7_B_:
	case BOTH_K7_S7_TL:
	case BOTH_K7_S7_TR:
	case BOTH_K7_S7_T_:
		return qtrue;
	default:;
	}
	return qfalse;
}

saber_moveName_t PM_BrokenParryForParry(const int move)
{
	switch (move)
	{
	case LS_PARRY_UP:
		return LS_H1_T_;
	case LS_PARRY_UR:
		return LS_H1_TR;
	case LS_PARRY_UL:
		return LS_H1_TL;
	case LS_PARRY_LR:
		return LS_H1_BR;
	case LS_PARRY_LL:
		return LS_H1_BL;
	case LS_PARRY_B:
		return LS_H1_T_;
	case LS_READY:
		return LS_H1_B_;
	default:;
	}
	return LS_NONE;
}

//OLD KNOCKAWAY//
saber_moveName_t PM_KnockawayForParry(const int move)
{
	switch (move)
	{
	case BLOCKED_TOP:
		return LS_K1_T_; //push up
	case BLOCKED_UPPER_RIGHT:
	default: //case LS_READY:
		return LS_K1_TR; //push up, slightly to right
	case BLOCKED_UPPER_LEFT:
		return LS_K1_TL; //push up and to left
	case BLOCKED_LOWER_RIGHT:
		return LS_K1_BR; //push down and to left
	case BLOCKED_LOWER_LEFT:
		return LS_K1_BL; //push down and to right
	case BLOCKED_BACK:
		return LS_K1_T_; //push down and to right
	}
}

saber_moveName_t PM_SaberBounceForAttack(const int move)
{
	switch (saber_moveData[move].startQuad)
	{
	case Q_B:
	case Q_BR:
		return LS_B1_BR;
	case Q_R:
		return LS_B1__R;
	case Q_TR:
		return LS_B1_TR;
	case Q_T:
		return LS_B1_T_;
	case Q_TL:
		return LS_B1_TL;
	case Q_L:
		return LS_B1__L;
	case Q_BL:
		return LS_B1_BL;
	default:;
	}
	return LS_NONE;
}

int PM_SaberDeflectionForQuad(const int quad)
{
	switch (quad)
	{
	case Q_B:
		return LS_D1_B_;
	case Q_BR:
		return LS_D1_BR;
	case Q_R:
		return LS_D1__R;
	case Q_TR:
		return LS_D1_TR;
	case Q_T:
		return LS_D1_T_;
	case Q_TL:
		return LS_D1_TL;
	case Q_L:
		return LS_D1__L;
	case Q_BL:
		return LS_D1_BL;
	default:;
	}
	return LS_NONE;
}

saber_moveName_t PM_AttackMoveForQuad(const int quad)
{
	switch (quad)
	{
	case Q_B:
	case Q_BR:
		return LS_A_BR2TL;
	case Q_R:
		return LS_A_R2L;
	case Q_TR:
		return LS_A_TR2BL;
	case Q_T:
		return LS_A_T2B;
	case Q_TL:
		return LS_A_TL2BR;
	case Q_L:
		return LS_A_L2R;
	case Q_BL:
		return LS_A_BL2TR;
	default:;
	}
	return LS_NONE;
}

int saberMoveTransitionAngle[Q_NUM_QUADS][Q_NUM_QUADS] =
{
	{
		0, //Q_BR,Q_BR,
		45, //Q_BR,Q_R,
		90, //Q_BR,Q_TR,
		135, //Q_BR,Q_T,
		180, //Q_BR,Q_TL,
		215, //Q_BR,Q_L,
		270, //Q_BR,Q_BL,
		45, //Q_BR,Q_B,
	},
	{
		45, //Q_R,Q_BR,
		0, //Q_R,Q_R,
		45, //Q_R,Q_TR,
		90, //Q_R,Q_T,
		135, //Q_R,Q_TL,
		180, //Q_R,Q_L,
		215, //Q_R,Q_BL,
		90, //Q_R,Q_B,
	},
	{
		90, //Q_TR,Q_BR,
		45, //Q_TR,Q_R,
		0, //Q_TR,Q_TR,
		45, //Q_TR,Q_T,
		90, //Q_TR,Q_TL,
		135, //Q_TR,Q_L,
		180, //Q_TR,Q_BL,
		135, //Q_TR,Q_B,
	},
	{
		135, //Q_T,Q_BR,
		90, //Q_T,Q_R,
		45, //Q_T,Q_TR,
		0, //Q_T,Q_T,
		45, //Q_T,Q_TL,
		90, //Q_T,Q_L,
		135, //Q_T,Q_BL,
		180, //Q_T,Q_B,
	},
	{
		180, //Q_TL,Q_BR,
		135, //Q_TL,Q_R,
		90, //Q_TL,Q_TR,
		45, //Q_TL,Q_T,
		0, //Q_TL,Q_TL,
		45, //Q_TL,Q_L,
		90, //Q_TL,Q_BL,
		135, //Q_TL,Q_B,
	},
	{
		215, //Q_L,Q_BR,
		180, //Q_L,Q_R,
		135, //Q_L,Q_TR,
		90, //Q_L,Q_T,
		45, //Q_L,Q_TL,
		0, //Q_L,Q_L,
		45, //Q_L,Q_BL,
		90, //Q_L,Q_B,
	},
	{
		270, //Q_BL,Q_BR,
		215, //Q_BL,Q_R,
		180, //Q_BL,Q_TR,
		135, //Q_BL,Q_T,
		90, //Q_BL,Q_TL,
		45, //Q_BL,Q_L,
		0, //Q_BL,Q_BL,
		45, //Q_BL,Q_B,
	},
	{
		45, //Q_B,Q_BR,
		90, //Q_B,Q_R,
		135, //Q_B,Q_TR,
		180, //Q_B,Q_T,
		135, //Q_B,Q_TL,
		90, //Q_B,Q_L,
		45, //Q_B,Q_BL,
		0 //Q_B,Q_B,
	}
};

static int PM_SaberAttackChainAngle(const int move1, const int move2)
{
	if (move1 == -1 || move2 == -1)
	{
		return -1;
	}
	return saberMoveTransitionAngle[saber_moveData[move1].endQuad][saber_moveData[move2].startQuad];
}

qboolean PM_SaberKataDone(const int curmove = LS_NONE, const int newmove = LS_NONE)
{
	if (pm->ps->forceRageRecoveryTime > level.time)
	{
		//rage recovery, only 1 swing at a time (tired)
		if (pm->ps->saberAttackChainCount > 0)
		{
			//swung once
			return qtrue;
		}
		//allow one attack
		return qfalse;
	}
	if (pm->ps->forcePowersActive & 1 << FP_RAGE)
	{
		//infinite chaining when raged
		return qfalse;
	}
	if (pm->ps->saber[0].maxChain == -1)
	{
		return qfalse;
	}
	if (pm->ps->saber[0].maxChain != 0)
	{
		if (pm->ps->saberAttackChainCount >= pm->ps->saber[0].maxChain)
		{
			return qtrue;
		}
		return qfalse;
	}

	if (pm->ps->saber_anim_level == SS_DESANN || pm->ps->saber_anim_level == SS_TAVION)
	{
		//desann and tavion can link up as many attacks as they want
		return qfalse;
	}
	if (pm->ps->saber_anim_level == SS_STAFF)
	{
		return qfalse;
	}
	if (pm->ps->saber_anim_level == SS_DUAL)
	{
		return qfalse;
	}
	if (pm->ps->saber_anim_level == FORCE_LEVEL_3)
	{
		if (curmove == LS_NONE || newmove == LS_NONE)
		{
			if (pm->ps->saber_anim_level >= FORCE_LEVEL_3 && pm->ps->saberAttackChainCount > Q_irand(0, 1))
			{
				return qtrue;
			}
		}
		else if (pm->ps->saberAttackChainCount > Q_irand(2, 3))
		{
			return qtrue;
		}
		else if (pm->ps->saberAttackChainCount > 0)
		{
			const int chain_angle = PM_SaberAttackChainAngle(curmove, newmove);
			if (chain_angle < 135 || chain_angle > 215)
			{
				//if trying to chain to a move that doesn't continue the momentum
				return qtrue;
			}
			if (chain_angle == 180)
			{
				//continues the momentum perfectly, allow it to chain 66% of the time
				if (pm->ps->saberAttackChainCount > 1)
				{
					return qtrue;
				}
			}
			else
			{
				//would continue the movement somewhat, 50% chance of continuing
				if (pm->ps->saberAttackChainCount > 2)
				{
					return qtrue;
				}
			}
		}
	}
	else
	{
		if ((pm->ps->saber_anim_level == FORCE_LEVEL_2 || pm->ps->saber_anim_level == SS_DUAL)
			&& pm->ps->saberAttackChainCount > Q_irand(2, 5))
		{
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean PM_CheckEnemyInBack(const float back_check_dist)
{
	if (!pm->gent || !pm->gent->client)
	{
		return qfalse;
	}
	if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		&& !g_saberAutoAim->integer && pm->cmd.forwardmove >= 0)
	{
		//don't auto-backstab
		return qfalse;
	}
	if (pm->ps->groundEntityNum == ENTITYNUM_NONE)
	{
		//only when on ground
		return qfalse;
	}
	trace_t trace;
	vec3_t end, fwd;
	const vec3_t fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };

	AngleVectors(fwd_angles, fwd, nullptr, nullptr);
	VectorMA(pm->ps->origin, -back_check_dist, fwd, end);

	pm->trace(&trace, pm->ps->origin, vec3_origin, vec3_origin, end, pm->ps->client_num, CONTENTS_SOLID | CONTENTS_BODY,
		static_cast<EG2_Collision>(0), 0);
	if (trace.fraction < 1.0f && trace.entityNum < ENTITYNUM_WORLD)
	{
		gentity_t* traceEnt = &g_entities[trace.entityNum];
		if (traceEnt
			&& traceEnt->health > 0
			&& traceEnt->client
			&& traceEnt->client->playerTeam == pm->gent->client->enemyTeam
			&& traceEnt->client->ps.groundEntityNum != ENTITYNUM_NONE)
		{
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
			{
				//player
				if (pm->gent)
				{
					//set player enemy to traceEnt so he auto-aims at him
					pm->gent->enemy = traceEnt;
				}
			}
			return qtrue;
		}
	}
	return qfalse;
}

static saber_moveName_t PM_PickBackStab()
{
	if (!pm->gent || !pm->gent->client)
	{
		return LS_READY;
	}
	if (pm->ps->dualSabers && pm->ps->saber[1].Active())
	{
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			return LS_A_BACK_CR;
		}
		return LS_A_BACK;
	}
	if (pm->gent->client->ps.saber_anim_level == SS_TAVION)
	{
		return LS_A_BACKSTAB;
	}
	if (pm->gent->client->ps.saber_anim_level == SS_DESANN || pm->gent->client->ps.saber_anim_level == SS_STRONG)
	{
		if (pm->ps->saber_move == LS_READY || !Q_irand(0, 3))
		{
			return LS_A_BACKSTAB;
		}
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			return LS_A_BACK_CR;
		}
		return LS_A_BACK;
	}
	if (pm->ps->saber_anim_level == FORCE_LEVEL_2 || pm->ps->saber_anim_level == SS_DUAL)
	{
		//using medium attacks or dual sabers
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			return LS_A_BACK_CR;
		}
		return LS_A_BACK;
	}
	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		return LS_A_BACK_CR;
	}
	return LS_A_BACKSTAB;
}

static saber_moveName_t PM_CheckStabDown()
{
	if (!pm->gent || !pm->gent->enemy || !pm->gent->enemy->client)
	{
		return LS_NONE;
	}
	if (pm->ps->saber[0].saberFlags & SFL_NO_STABDOWN)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers
		&& pm->ps->saber[1].saberFlags & SFL_NO_STABDOWN)
	{
		return LS_NONE;
	}

	if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING)
	{
		return LS_NONE;
	}

	if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		//player
		if (G_TryingKataAttack(&pm->cmd)) //Holding focus
		{
			//want to try a special
			return LS_NONE;
		}
		//player
		if (pm->ps->groundEntityNum == ENTITYNUM_NONE) //in air
		{
			//sorry must be on ground (or have just jumped)
			if (level.time - pm->ps->lastOnGround <= 50 && pm->ps->pm_flags & PMF_JUMPING)
			{
				//just jumped, it's okay
			}
			else
			{
				return LS_NONE;
			}
		}
		pm->ps->velocity[2] = 0;
		pm->cmd.upmove = 0;
	}
	else if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
	{
		//NPC
		if (pm->ps->groundEntityNum == ENTITYNUM_NONE) //in air
		{
			//sorry must be on ground (or have just jumped)
			if (level.time - pm->ps->lastOnGround <= 250 && pm->ps->pm_flags & PMF_JUMPING)
			{
				//just jumped, it's okay
			}
			else
			{
				return LS_NONE;
			}
		}
		if (!pm->gent->NPC)
		{
			//wtf???
			return LS_NONE;
		}
		if (Q_irand(0, RANK_CAPTAIN) > pm->gent->NPC->rank)
		{
			//lower ranks do this less often
			return LS_NONE;
		}
	}
	vec3_t enemy_dir, face_fwd;
	const vec3_t facing_angles = { 0, pm->ps->viewangles[YAW], 0 };
	AngleVectors(facing_angles, face_fwd, nullptr, nullptr);
	VectorSubtract(pm->gent->enemy->currentOrigin, pm->ps->origin, enemy_dir);
	const float enemy_z_diff = enemy_dir[2];
	enemy_dir[2] = 0;
	const float enemy_h_dist = VectorNormalize(enemy_dir) - (pm->gent->maxs[0] + pm->gent->enemy->maxs[0]);
	const float dot = DotProduct(enemy_dir, face_fwd);

	if ( //(pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
		dot > 0.65f
		//&& enemyHDist >= 32 //was 48
		&& enemy_h_dist <= 164 //was 112
		&& PM_InKnockDownOnGround(&pm->gent->enemy->client->ps) //still on ground
		&& !PM_InGetUpNoRoll(&pm->gent->enemy->client->ps) //not getting up yet
		&& enemy_z_diff <= 20)
	{
		//guy is on the ground below me, do a top-down attack
		if (pm->gent->enemy->s.number >= MAX_CLIENTS
			|| !G_ControlledByPlayer(pm->gent->enemy))
		{
			//don't get up while I'm doing this
			//stop them from trying to get up for at least another 3 seconds
			TIMER_Set(pm->gent->enemy, "noGetUpStraight", 3000);
		}
		//pick the right anim
		if (pm->ps->saber_anim_level == SS_DUAL
			|| pm->ps->dualSabers && pm->ps->saber[1].Active())
		{
			return LS_STABDOWN_DUAL;
		}
		if (pm->ps->saber_anim_level == SS_STAFF)
		{
			return LS_STABDOWN_STAFF;
		}
		return LS_STABDOWN;
	}
	return LS_NONE;
}

extern saber_moveName_t PM_NPCSaberAttackFromQuad(int quad);

saber_moveName_t PM_AttackForEnemyPos(const qboolean allow_fb, const qboolean allow_stab_down)
{
	saber_moveName_t auto_move = LS_INVALID;

	if (!pm->gent->enemy)
	{
		return LS_NONE;
	}

	vec3_t enemy_org, enemy_dir, face_fwd, face_right, face_up;
	const vec3_t facing_angles = { 0, pm->ps->viewangles[YAW], 0 };
	AngleVectors(facing_angles, face_fwd, face_right, face_up);
	//FIXME: predict enemy position?
	if (pm->gent->enemy->client)
	{
		//VectorCopy( pm->gent->enemy->currentOrigin, enemy_org );
		//HMM... using this will adjust for bbox size, so let's do that...
		vec3_t size;
		VectorSubtract(pm->gent->enemy->absmax, pm->gent->enemy->absmin, size);
		VectorMA(pm->gent->enemy->absmin, 0.5, size, enemy_org);

		VectorSubtract(pm->gent->enemy->client->renderInfo.eyePoint, pm->ps->origin, enemy_dir);
	}
	else
	{
		if (pm->gent->enemy->bmodel && VectorCompare(vec3_origin, pm->gent->enemy->currentOrigin))
		{
			//a brush model without an origin brush
			vec3_t size;
			VectorSubtract(pm->gent->enemy->absmax, pm->gent->enemy->absmin, size);
			VectorMA(pm->gent->enemy->absmin, 0.5, size, enemy_org);
		}
		else
		{
			VectorCopy(pm->gent->enemy->currentOrigin, enemy_org);
		}
		VectorSubtract(enemy_org, pm->ps->origin, enemy_dir);
	}
	const float enemy_z_diff = enemy_dir[2];
	const float enemy_dist = VectorNormalize(enemy_dir);
	const float dot = DotProduct(enemy_dir, face_fwd);
	if (dot > 0)
	{
		//enemy is in front
		if (allow_stab_down)
		{
			//okay to try this
			const saber_moveName_t stabDownMove = PM_CheckStabDown();
			if (stabDownMove != LS_NONE)
			{
				return stabDownMove;
			}
		}
		if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
			&& dot > 0.65f
			&& enemy_dist <= 64 && pm->gent->enemy->client
			&& (enemy_z_diff <= 20 || PM_InKnockDownOnGround(&pm->gent->enemy->client->ps) || PM_CrouchAnim(
				pm->gent->enemy->client->ps.legsAnim)))
		{
			//swing down at them
			return LS_A_T2B;
		}
		if (allow_fb)
		{
			//directly in front anim allowed
			if (!(pm->ps->saber[0].saberFlags & SFL_NO_BACK_ATTACK)
				&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_BACK_ATTACK)))
			{
				//okay to do backstabs with this saber
				if (enemy_dist > 200 || pm->gent->enemy->health <= 0)
				{
					//hmm, look in back for an enemy
					if (pm->ps->client_num && !PM_ControlledByPlayer())
					{
						//player should never do this automatically
						if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
						{
							//only when on ground
							if (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= RANK_LT_JG &&
								Q_irand(0, pm->gent->NPC->rank) > RANK_ENSIGN)
							{
								//only fencers and higher can do this, higher rank does it more
								if (PM_CheckEnemyInBack(100))
								{
									return PM_PickBackStab();
								}
							}
						}
					}
				}
			}
			//this is the default only if they're *right* in front...
			if (pm->ps->client_num && !PM_ControlledByPlayer()
				|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) &&
				BG_AllowThirdPersonSpecialMove(pm->ps) && !cg.zoomMode)
			{
				//NPC or player not in 1st person
				if (PM_CheckFlipOverAttackMove(qtrue))
				{
					//enemy must be close and in front
					return PM_SaberFlipOverAttackMove();
				}
			}
			if (PM_CheckLungeAttackMove())
			{
				//NPC
				auto_move = PM_SaberLungeAttackMove(qtrue);
			}
			else
			{
				auto_move = LS_A_T2B;
			}
		}
		else
		{
			//pick a random one
			if (Q_irand(0, 1))
			{
				auto_move = LS_A_TR2BL;
			}
			else
			{
				auto_move = LS_A_TL2BR;
			}
		}
		const float dot_r = DotProduct(enemy_dir, face_right);
		if (dot_r > 0.35)
		{
			//enemy is to far right
			auto_move = LS_A_L2R;
		}
		else if (dot_r < -0.35)
		{
			//far left
			auto_move = LS_A_R2L;
		}
		else if (dot_r > 0.15)
		{
			//enemy is to near right
			auto_move = LS_A_TR2BL;
		}
		else if (dot_r < -0.15)
		{
			//near left
			auto_move = LS_A_TL2BR;
		}
		if (DotProduct(enemy_dir, face_up) > 0.5)
		{
			//enemy is above me
			if (auto_move == LS_A_TR2BL)
			{
				auto_move = LS_A_BL2TR;
			}
			else if (auto_move == LS_A_TL2BR)
			{
				auto_move = LS_A_BR2TL;
			}
		}
	}
	else if (allow_fb)
	{
		//back attack allowed
		if (!(pm->ps->saber[0].saberFlags & SFL_NO_BACK_ATTACK)
			&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_BACK_ATTACK)))
		{
			//okay to do backstabs with this saber
			if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
			{
				//only when on ground
				if (!pm->gent->enemy->client || pm->gent->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{
					//enemy not a client or is a client and on ground
					if (dot < -0.75f
						&& enemy_dist < 128
						&& (pm->ps->saber_anim_level == SS_FAST || pm->ps->saber_anim_level == SS_STAFF || pm->gent->client
							&& (pm->gent->client->NPC_class == CLASS_TAVION || pm->gent->client->NPC_class == CLASS_YODA
								|| pm->gent->client->NPC_class == CLASS_GALEN || pm->gent->client->NPC_class ==
								CLASS_ALORA) && Q_irand(0, 2)))
					{
						//fast back-stab
						if (!(pm->ps->pm_flags & PMF_DUCKED) && pm->cmd.upmove >= 0)
						{
							//can't do it while ducked?
							if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() || pm->gent->NPC && pm->
								gent->NPC->rank >= RANK_LT_JG)
							{
								//only fencers and above can do this
								auto_move = LS_A_BACKSTAB;
							}
						}
					}
					else if (pm->ps->saber_anim_level != SS_FAST && pm->ps->saber_anim_level != SS_STAFF)
					{
						//higher level back spin-attacks
						if (pm->ps->client_num && !PM_ControlledByPlayer() || (pm->ps->client_num < MAX_CLIENTS ||
							PM_ControlledByPlayer()) && BG_AllowThirdPersonSpecialMove(pm->ps) && !cg.zoomMode)
						{
							if (pm->ps->pm_flags & PMF_DUCKED || pm->cmd.upmove < 0)
							{
								auto_move = LS_A_BACK_CR;
							}
							else
							{
								auto_move = LS_A_BACK;
							}
						}
					}
				}
			}
		}
	}
	return auto_move;
}

qboolean PM_InSecondaryStyle()
{
	if (pm->ps->saber[0].numBlades > 1
		&& pm->ps->saber[0].singleBladeStyle
		&& pm->ps->saber[0].stylesForbidden & 1 << pm->ps->saber[0].singleBladeStyle
		&& pm->ps->saber_anim_level == pm->ps->saber[0].singleBladeStyle)
	{
		return qtrue;
	}

	if (pm->ps->dualSabers && !pm->ps->saber[1].Active())
	{
		return qtrue;
	}
	return qfalse;
}

saber_moveName_t PM_SaberLungeAttackMove(const qboolean fallback_to_normal_lunge)
{
	vec3_t fwd_angles, jumpFwd;

	WP_ForcePowerDrain(pm->gent, FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER_FB);
	//see if we have an overridden (or cancelled) lunge move
	if (pm->ps->saber[0].lungeAtkMove != LS_INVALID)
	{
		if (pm->ps->saber[0].lungeAtkMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].lungeAtkMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].lungeAtkMove != LS_INVALID)
		{
			if (pm->ps->saber[1].lungeAtkMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].lungeAtkMove);
			}
		}
	}
	//no overrides, cancelled?
	if (pm->ps->saber[0].lungeAtkMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].lungeAtkMove == LS_NONE)
		{
			return LS_NONE;
		}
	}

	//do normal checks
	if (!pm->ps->dualSabers && pm->gent->client->NPC_class == CLASS_ALORA && !Q_irand(0, 3))
	{
		//alora NPC
		return LS_SPINATTACK_ALORA;
	}

	switch (pm->ps->saber_anim_level)
	{
	case SS_DUAL:
		return LS_SPINATTACK_DUAL;
		break;
	case SS_STAFF:
		return LS_SPINATTACK;
		break;
	case SS_TAVION:
		VectorCopy(pm->ps->viewangles, fwd_angles);
		fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
		//do the lunge
		AngleVectors(fwd_angles, jumpFwd, nullptr, nullptr);
		VectorScale(jumpFwd, 150, pm->ps->velocity);
		pm->ps->velocity[2] = 50;
		PM_AddEvent(EV_JUMP);

		if (pm->ps->forcePower < BLOCKPOINTS_KNOCKAWAY)
		{
			return LS_A_LUNGE;
		}
		else
		{
			return LS_PULL_ATTACK_STAB;
		}
		break;
	case SS_FAST:
		VectorCopy(pm->ps->viewangles, fwd_angles);
		fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
		//do the lunge
		AngleVectors(fwd_angles, jumpFwd, nullptr, nullptr);
		VectorScale(jumpFwd, 150, pm->ps->velocity);
		pm->ps->velocity[2] = 50;
		PM_AddEvent(EV_JUMP);
		return LS_A_LUNGE;
		break;
	case SS_STRONG:
	case SS_DESANN:
		if (fallback_to_normal_lunge)
		{
			VectorCopy(pm->ps->viewangles, fwd_angles);
			fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
			//do the lunge
			AngleVectors(fwd_angles, jumpFwd, nullptr, nullptr);
			VectorScale(jumpFwd, 150, pm->ps->velocity);
			pm->ps->velocity[2] = 50;
			PM_AddEvent(EV_JUMP);
			return LS_A_JUMP_T__B_;
		}
		break;
	default: //normal lunge
		if (fallback_to_normal_lunge)
		{
			VectorCopy(pm->ps->viewangles, fwd_angles);
			fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
			//do the lunge
			AngleVectors(fwd_angles, jumpFwd, nullptr, nullptr);
			VectorScale(jumpFwd, 150, pm->ps->velocity);
			pm->ps->velocity[2] = 50;
			PM_AddEvent(EV_JUMP);
			return LS_A_LUNGE;
		}
		break;
	}
	return LS_NONE;
}

qboolean PM_CheckLungeAttackMove()
{
	//check to see if it's cancelled?
	if (pm->ps->saber[0].lungeAtkMove == LS_NONE)
	{
		if (pm->ps->dualSabers)
		{
			if (pm->ps->saber[1].lungeAtkMove == LS_NONE
				|| pm->ps->saber[1].lungeAtkMove == LS_INVALID)
			{
				return qfalse;
			}
		}
		else
		{
			return qfalse;
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].lungeAtkMove == LS_NONE)
		{
			if (pm->ps->saber[0].lungeAtkMove == LS_NONE
				|| pm->ps->saber[0].lungeAtkMove == LS_INVALID)
			{
				return qfalse;
			}
		}
	}
	//do normal checks
	if (pm->ps->saber_anim_level == SS_FAST //fast
		|| pm->ps->saber_anim_level == SS_MEDIUM
		|| pm->ps->saber_anim_level == SS_STRONG
		|| pm->ps->saber_anim_level == SS_DUAL //dual
		|| pm->ps->saber_anim_level == SS_STAFF //staff
		|| pm->ps->saber_anim_level == SS_DESANN
		|| pm->ps->saber_anim_level == SS_TAVION
		|| pm->ps->dualSabers)
	{
		//alt+back+attack using fast, dual or staff attacks
		if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
		{
			//NPC
			if (pm->cmd.upmove < 0 || pm->ps->pm_flags & PMF_DUCKED)
			{
				//ducking
				if (pm->ps->legsAnim == BOTH_STAND2
					|| pm->ps->legsAnim == BOTH_STAND2_JKA
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_FORWARD
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_BACK
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_RIGHT
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_LEFT
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_FORWARD
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_BACK
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_RIGHT
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_LEFT
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_FORWARD
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_BACK
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_RIGHT
					|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_LEFT
					|| pm->ps->legsAnim == BOTH_SABERFAST_STANCE
					|| pm->ps->legsAnim == BOTH_SABERFAST_STANCE_JKA
					|| pm->ps->legsAnim == BOTH_SABERSLOW_STANCE
					|| pm->ps->legsAnim == BOTH_SABERSLOW_STANCE_JKA
					|| pm->ps->legsAnim == BOTH_SABERSTAFF_STANCE
					|| pm->ps->legsAnim == BOTH_SABERSTAFF_STANCE_JKA
					|| pm->ps->legsAnim == BOTH_SABERSTAFF_WALK_STANCE
					|| pm->ps->legsAnim == BOTH_SABERDUAL_STANCE
					|| pm->ps->legsAnim == BOTH_SABERDUAL_STANCE_JKA
					|| pm->ps->legsAnim == BOTH_SABERTAVION_STANCE
					|| pm->ps->legsAnim == BOTH_SABERTAVION_STANCE_JKA
					|| pm->ps->legsAnim == BOTH_SABERDESANN_STANCE
					|| pm->ps->legsAnim == BOTH_SABERDESANN_STANCE_JKA
					|| level.time - pm->ps->lastStationary <= 500)
				{
					//standing or just stopped standing
					if (pm->gent
						&& pm->gent->NPC //NPC
						&& pm->gent->NPC->rank >= RANK_LT_JG //high rank
						&& (pm->gent->NPC->rank == RANK_LT_JG || Q_irand(-3, pm->gent->NPC->rank) >= RANK_LT_JG)
						//Q_irand( 0, pm->gent->NPC->rank ) >= RANK_LT_JG )
						&& !Q_irand(0, 3 - g_spskill->integer))
					{
						//only fencer and higher can do this
						if (pm->ps->saber_anim_level == SS_DESANN)
						{
							if (!Q_irand(0, 4))
							{
								return qtrue;
							}
						}
						else
						{
							return qtrue;
						}
					}
				}
			}
		}
		else
		{
			//player
			if (G_TryingLungeAttack(pm->gent, &pm->cmd)
				&& PM_Can_Do_Kill_Move())
				//have enough force power to pull it off
			{
				return qtrue;
			}
		}
	}

	return qfalse;
}

saber_moveName_t PM_SaberJumpForwardAttackMove()
{
	//see if we have an overridden (or cancelled) kata move
	if (pm->ps->saber[0].jumpAtkFwdMove != LS_INVALID)
	{
		if (pm->ps->saber[0].jumpAtkFwdMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkFwdMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove != LS_INVALID)
		{
			if (pm->ps->saber[1].jumpAtkFwdMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkFwdMove);
			}
		}
	}
	//no overrides, cancelled?
	if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE)
		{
			return LS_NONE;
		}
	}

	if (!PM_Can_Do_Kill_Move())
	{
		return LS_NONE;
	}
	if (pm->ps->saber_anim_level == SS_DUAL
		|| pm->ps->saber_anim_level == SS_STAFF)
	{
		pm->cmd.upmove = 0; //no jump just yet

		if (pm->ps->saber_anim_level == SS_STAFF)
		{
			if (Q_irand(0, 1))
			{
				return LS_JUMPATTACK_STAFF_LEFT;
			}
			return LS_JUMPATTACK_STAFF_RIGHT;
		}

		return LS_JUMPATTACK_DUAL;
	}
	vec3_t fwd_angles, jump_fwd;

	VectorCopy(pm->ps->viewangles, fwd_angles);
	fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
	AngleVectors(fwd_angles, jump_fwd, nullptr, nullptr);
	VectorScale(jump_fwd, 200, pm->ps->velocity);
	pm->ps->velocity[2] = 180;
	pm->ps->forceJumpZStart = pm->ps->origin[2]; //so we don't take damage if we land at same height
	pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;

	//FIXME: NPCs yell?
	PM_AddEvent(EV_JUMP);
	if (g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_3)
	{
		//short burst
		G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
	}
	else
	{
		//holding it
		G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
	}
	pm->cmd.upmove = 0;
	WP_ForcePowerDrain(pm->gent, FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER_FB);

	return LS_A_JUMP_T__B_;
}

saber_moveName_t PM_NPC_Force_Leap_Attack()
{
	vec3_t fwd_angles, jump_fwd;

	//see if we have an overridden (or cancelled) kata move
	if (pm->ps->saber[0].jumpAtkFwdMove != LS_INVALID)
	{
		if (pm->ps->saber[0].jumpAtkFwdMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkFwdMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove != LS_INVALID)
		{
			if (pm->ps->saber[1].jumpAtkFwdMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkFwdMove);
			}
		}
	}
	//no overrides, cancelled?
	if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE)
		{
			return LS_NONE;
		}
	}

	if (pm->ps->saber_anim_level == SS_DUAL || pm->ps->saber_anim_level == SS_STAFF)
	{
		pm->cmd.upmove = 0; //no jump just yet

		if (pm->ps->saber_anim_level == SS_STAFF)
		{
			if (Q_irand(0, 1))
			{
				return LS_JUMPATTACK_STAFF_LEFT;
			}
			return LS_JUMPATTACK_STAFF_RIGHT;
		}
		return LS_JUMPATTACK_DUAL;
	}
	else if (pm->ps->saber_anim_level == SS_FAST || pm->ps->saber_anim_level == SS_TAVION || pm->ps->saber_anim_level == SS_MEDIUM)
	{
		VectorCopy(pm->ps->viewangles, fwd_angles);
		fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
		AngleVectors(fwd_angles, jump_fwd, nullptr, nullptr);
		VectorScale(jump_fwd, 150, pm->ps->velocity);
		pm->ps->velocity[2] = 250;

		if (pm->gent && pm->gent->enemy)
		{
			//go higher for taller enemies
			pm->ps->velocity[2] *= (pm->gent->enemy->maxs[2] - pm->gent->enemy->mins[2]) / 64.0f;
			//go higher for enemies higher than you, lower for those lower than you
			const float z_diff = pm->gent->enemy->currentOrigin[2] - pm->ps->origin[2];
			pm->ps->velocity[2] += z_diff * 1.5f;
			//clamp to decent-looking values
			if (z_diff <= 0 && pm->ps->velocity[2] < 200)
			{
				//if we're on same level, don't let me jump so low, I clip into the ground
				pm->ps->velocity[2] = 200;
			}
			else if (pm->ps->velocity[2] < 50)
			{
				pm->ps->velocity[2] = 50;
			}
			else if (pm->ps->velocity[2] > 400)
			{
				pm->ps->velocity[2] = 400;
			}
		}
		pm->ps->forceJumpZStart = pm->ps->origin[2]; //so we don't take damage if we land at same height
		pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;

		PM_AddEvent(EV_JUMP);

		if (pm->gent && pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_3)
		{
			//short burst
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
		}
		else
		{
			//holding it
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
		}
		pm->cmd.upmove = 0;
		pm->gent->angle = pm->ps->viewangles[YAW]; //so we know what yaw we started this at

		if (pm->ps->saber_anim_level == SS_MEDIUM)
		{
			return LS_A_FLIP_SLASH;
		}
		else
		{
			return LS_A_FLIP_STAB;
		}
	}
	else
	{
		VectorCopy(pm->ps->viewangles, fwd_angles);
		fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
		AngleVectors(fwd_angles, jump_fwd, nullptr, nullptr);
		VectorScale(jump_fwd, 150, pm->ps->velocity);
		pm->ps->velocity[2] = 180;
		pm->ps->forceJumpZStart = pm->ps->origin[2]; //so we don't take damage if we land at same height
		pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
		PM_AddEvent(EV_JUMP);

		if (pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_3)
		{
			//short burst
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
		}
		else
		{
			//holding it
			G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
		}
		pm->cmd.upmove = 0;
		return LS_A_JUMP_T__B_;
	}
}

#define SPECIAL_ATTACK_DISTANCE 128
qboolean PM_Can_Do_Kill_Lunge(void)
{
	trace_t tr;
	vec3_t flatAng;
	vec3_t fwd, back{};
	const vec3_t trmins = { -15, -15, -8 };
	const vec3_t trmaxs = { 15, 15, 8 };

	VectorCopy(pm->ps->viewangles, flatAng);
	flatAng[PITCH] = 0;

	AngleVectors(flatAng, fwd, 0, 0);

	back[0] = pm->ps->origin[0] + fwd[0] * SPECIAL_ATTACK_DISTANCE;
	back[1] = pm->ps->origin[1] + fwd[1] * SPECIAL_ATTACK_DISTANCE;
	back[2] = pm->ps->origin[2] + fwd[2] * SPECIAL_ATTACK_DISTANCE;

	pm->trace(&tr, pm->ps->origin, trmins, trmaxs, back, pm->ps->client_num, MASK_PLAYERSOLID, static_cast<EG2_Collision>(0), 0);

	if (tr.fraction != 1.0 && tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS)
	{
		//We don't have real entity access here so we can't do an in depth check. But if it's a client, I guess that's reason enough to attack
		return qtrue;
	}

	return qfalse;
}

qboolean PM_Can_Do_Kill_Lunge_back(void)
{
	trace_t tr;
	vec3_t flatAng;
	vec3_t fwd, back{};
	vec3_t trmins = { -15, -15, -8 };
	vec3_t trmaxs = { 15, 15, 8 };

	VectorCopy(pm->ps->viewangles, flatAng);
	flatAng[PITCH] = 0;

	AngleVectors(flatAng, fwd, 0, 0);

	back[0] = pm->ps->origin[0] - fwd[0] * SPECIAL_ATTACK_DISTANCE;
	back[1] = pm->ps->origin[1] - fwd[1] * SPECIAL_ATTACK_DISTANCE;
	back[2] = pm->ps->origin[2] - fwd[2] * SPECIAL_ATTACK_DISTANCE;

	pm->trace(&tr, pm->ps->origin, trmins, trmaxs, back, pm->ps->client_num, MASK_PLAYERSOLID, static_cast<EG2_Collision>(0), 0);

	if (tr.fraction != 1.0 && tr.entityNum >= 0 && (tr.entityNum < MAX_CLIENTS))
	{ //We don't have real entity access here so we can't do an indepth check. But if it's a client and it's behind us, I guess that's reason enough to stab backward
		return qtrue;
	}

	return qfalse;
}

static qboolean PM_CheckJumpForwardAttackMove()
{
	if (pm->ps->client_num < MAX_CLIENTS
		&& PM_InSecondaryStyle())
	{
		return qfalse;
	}

	//check to see if it's cancelled?
	if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE)
	{
		if (pm->ps->dualSabers)
		{
			if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE
				|| pm->ps->saber[1].jumpAtkFwdMove == LS_INVALID)
			{
				return qfalse;
			}
		}
		else
		{
			return qfalse;
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE)
		{
			if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE
				|| pm->ps->saber[0].jumpAtkFwdMove == LS_INVALID)
			{
				return qfalse;
			}
		}
	}
	//do normal checks

	if (pm->cmd.forwardmove > 0 //going forward
		&& pm->ps->forceRageRecoveryTime < pm->cmd.serverTime //not in a force Rage recovery period
		&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1 //can force jump
		&& pm->gent && !(pm->gent->flags & FL_LOCK_PLAYER_WEAPONS)
		// yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
		&& (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)
		//on ground or just jumped (if not player)
		)
	{
		if (pm->ps->saber_anim_level == SS_DUAL
			|| pm->ps->saber_anim_level == SS_STAFF)
		{
			//dual and staff
			if (!PM_SaberInTransitionAny(pm->ps->saber_move) //not going to/from/between an attack anim
				&& !PM_SaberInAttack(pm->ps->saber_move) //not in attack anim
				&& pm->ps->weaponTime <= 0 //not busy
				&& pm->cmd.buttons & BUTTON_ATTACK) //want to attack
			{
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					//NPC
					if (pm->cmd.upmove > 0 || pm->ps->pm_flags & PMF_JUMPING) //jumping NPC
					{
						if (pm->gent
							&& pm->gent->NPC
							&& (pm->gent->NPC->rank == RANK_CREWMAN || pm->gent->NPC->rank >= RANK_LT))
						{
							return qtrue;
						}
					}
				}
				else
				{
					//PLAYER
					if (G_TryingJumpForwardAttack(pm->gent, &pm->cmd)
						&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB))
						//have enough power to attack
					{
						return qtrue;
					}
				}
			}
		}
		//check strong
		else if (pm->ps->saber_anim_level == SS_STRONG //strong style
			|| pm->ps->saber_anim_level == SS_DESANN) //desann
		{
			if (!pm->ps->dualSabers)
			{
				//strong attack: jump-hack
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					//NPC
					if (pm->cmd.upmove > 0 || pm->ps->pm_flags & PMF_JUMPING) //NPC jumping
					{
						if (pm->gent
							&& pm->gent->NPC
							&& (pm->gent->NPC->rank == RANK_CREWMAN || pm->gent->NPC->rank >= RANK_LT))
						{
							//only acrobat or boss and higher can do this
							if (pm->ps->legsAnim == BOTH_STAND2
								|| pm->ps->legsAnim == BOTH_STAND2_JKA
								|| pm->ps->legsAnim == BOTH_SABERFAST_STANCE
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_FORWARD
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_BACK
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_RIGHT
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_LEFT
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_FORWARD
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_BACK
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_RIGHT
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_LEFT
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_FORWARD
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_BACK
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_RIGHT
								|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_LEFT
								|| pm->ps->legsAnim == BOTH_SABERFAST_STANCE_JKA
								|| pm->ps->legsAnim == BOTH_SABERSLOW_STANCE
								|| pm->ps->legsAnim == BOTH_SABERSLOW_STANCE_JKA
								|| pm->ps->legsAnim == BOTH_SABERTAVION_STANCE
								|| pm->ps->legsAnim == BOTH_SABERTAVION_STANCE_JKA
								|| pm->ps->legsAnim == BOTH_SABERDESANN_STANCE
								|| pm->ps->legsAnim == BOTH_SABERDESANN_STANCE_JKA
								|| level.time - pm->ps->lastStationary <= 250)
							{
								//standing or just started moving
								if (pm->gent->client
									&& (pm->gent->client->NPC_class == CLASS_DESANN || pm->gent->client->NPC_class ==
										CLASS_VADER))
								{
									if (!Q_irand(0, 1))
									{
										return qtrue;
									}
								}
								else
								{
									return qtrue;
								}
							}
						}
					}
				}
				else
				{
					//player
					if (G_TryingJumpForwardAttack(pm->gent, &pm->cmd)
						&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB))
					{
						return qtrue;
					}
				}
			}
		}
	}
	return qfalse;
}

saber_moveName_t PM_SaberFlipOverAttackMove()
{
	//see if we have an overridden (or cancelled) kata move
	if (pm->ps->saber[0].jumpAtkFwdMove != LS_INVALID)
	{
		if (pm->ps->saber[0].jumpAtkFwdMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkFwdMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove != LS_INVALID)
		{
			if (pm->ps->saber[1].jumpAtkFwdMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkFwdMove);
			}
		}
	}
	//no overrides, canceled?
	if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE)
		{
			return LS_NONE;
		}
	}

	if (!PM_Can_Do_Kill_Move())
	{
		return LS_NONE;
	}

	vec3_t fwd_angles, jump_fwd;

	VectorCopy(pm->ps->viewangles, fwd_angles);
	fwd_angles[PITCH] = fwd_angles[ROLL] = 0;
	AngleVectors(fwd_angles, jump_fwd, nullptr, nullptr);
	VectorScale(jump_fwd, 150, pm->ps->velocity);
	pm->ps->velocity[2] = 250;
	//250 is normalized for a standing enemy at your z level, about 64 tall... adjust for actual maxs[2]-mins[2] of enemy and for zdiff in origins
	if (pm->gent && pm->gent->enemy)
	{
		//go higher for taller enemies
		pm->ps->velocity[2] *= (pm->gent->enemy->maxs[2] - pm->gent->enemy->mins[2]) / 64.0f;
		//go higher for enemies higher than you, lower for those lower than you
		const float z_diff = pm->gent->enemy->currentOrigin[2] - pm->ps->origin[2];
		pm->ps->velocity[2] += z_diff * 1.5f;
		//clamp to decent-looking values
		//FIXME: still jump too low sometimes
		if (z_diff <= 0 && pm->ps->velocity[2] < 200)
		{
			//if we're on same level, don't let me jump so low, I clip into the ground
			pm->ps->velocity[2] = 200;
		}
		else if (pm->ps->velocity[2] < 50)
		{
			pm->ps->velocity[2] = 50;
		}
		else if (pm->ps->velocity[2] > 400)
		{
			pm->ps->velocity[2] = 400;
		}
	}
	pm->ps->forceJumpZStart = pm->ps->origin[2]; //so we don't take damage if we land at same height
	pm->ps->pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;

	//FIXME: NPCs yell?
	PM_AddEvent(EV_JUMP);

	if (g_SerenityJediEngineMode->integer == 2 && pm->gent->client->ps.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_3)
	{
		//short burst
		G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
	}
	else
	{
		//holding it
		G_SoundOnEnt(pm->gent, CHAN_BODY, "sound/weapons/force/jump.mp3");
	}
	pm->cmd.upmove = 0;
	//FIXME: don't allow this to land on other people

	pm->gent->angle = pm->ps->viewangles[YAW]; //so we know what yaw we started this at

	WP_ForcePowerDrain(pm->gent, FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER_FB);

	if (pm->ps->saber_anim_level == SS_FAST || pm->ps->saber_anim_level == SS_TAVION)
	{
		return LS_A_FLIP_STAB;
	}
	return LS_A_FLIP_SLASH;
}

qboolean PM_CheckFlipOverAttackMove(const qboolean check_enemy)
{
	if (pm->ps->client_num < MAX_CLIENTS
		&& PM_InSecondaryStyle())
	{
		return qfalse;
	}
	//check to see if it's cancelled?
	if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE)
	{
		if (pm->ps->dualSabers)
		{
			if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE
				|| pm->ps->saber[1].jumpAtkFwdMove == LS_INVALID)
			{
				return qfalse;
			}
		}
		else
		{
			return qfalse;
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkFwdMove == LS_NONE)
		{
			if (pm->ps->saber[0].jumpAtkFwdMove == LS_NONE
				|| pm->ps->saber[0].jumpAtkFwdMove == LS_INVALID)
			{
				return qfalse;
			}
		}
	}
	//do normal checks

	if ((pm->ps->saber_anim_level == SS_MEDIUM //medium
		|| pm->ps->saber_anim_level == SS_FAST
		|| pm->ps->saber_anim_level == SS_TAVION) //tavion
		&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1 //can force jump
		&& !(pm->gent->flags & FL_LOCK_PLAYER_WEAPONS)
		// yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
		&& (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)
		//on ground or just jumped
		)
	{
		qboolean try_move = qfalse;
		if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
		{
			//NPC
			if (pm->cmd.upmove > 0 //want to jump
				|| pm->ps->pm_flags & PMF_JUMPING) //jumping
			{
				//flip over-forward down-attack
				if (pm->gent->NPC
					&& (pm->gent->NPC->rank == RANK_CREWMAN || pm->gent->NPC->rank >= RANK_LT)
					&& !Q_irand(0, 2)) //NPC who can do this, 33% chance
				{
					//only player or acrobat or boss and higher can do this
					try_move = qtrue;
				}
			}
		}
		else
		{
			//player
			if (G_TryingJumpForwardAttack(pm->gent, &pm->cmd)
				&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB)) //have enough power
			{
				if (!pm->cmd.rightmove)
				{
					if (pm->ps->legsAnim == BOTH_JUMP1
						|| pm->ps->legsAnim == BOTH_FORCEJUMP1
						|| pm->ps->legsAnim == BOTH_INAIR1
						|| pm->ps->legsAnim == BOTH_FORCEINAIR1
						|| pm->ps->legsAnim == BOTH_GRAPPLE_PULL)
					{
						//in a non-flip forward jump
						try_move = qtrue;
					}
				}
			}
		}

		if (try_move)
		{
			if (!check_enemy)
			{
				//based just on command input
				return qtrue;
			}
			//based on presence of enemy
			if (pm->gent->enemy) //have an enemy
			{
				vec3_t fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
				if (pm->gent->enemy->health > 0
					&& pm->ps->forceRageRecoveryTime < pm->cmd.serverTime //not in a force Rage recovery period
					&& pm->gent->enemy->maxs[2] > 12
					&& (!pm->gent->enemy->client || !PM_InKnockDownOnGround(&pm->gent->enemy->client->ps))
					&& DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 10000
					&& InFront(pm->gent->enemy->currentOrigin, pm->gent->currentOrigin, fwd_angles, 0.3f))
				{
					//enemy must be alive, not low to ground, close and in front
					return qtrue;
				}
			}
			return qfalse;
		}
	}
	return qfalse;
}

saber_moveName_t PM_SaberBackflipAttackMove()
{
	//see if we have an overridden (or cancelled) kata move
	if (pm->ps->saber[0].jumpAtkBackMove != LS_INVALID)
	{
		if (pm->ps->saber[0].jumpAtkBackMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkBackMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkBackMove != LS_INVALID)
		{
			if (pm->ps->saber[1].jumpAtkBackMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkBackMove);
			}
		}
	}
	//no overrides, cancelled?
	if (pm->ps->saber[0].jumpAtkBackMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkBackMove == LS_NONE)
		{
			return LS_NONE;
		}
	}
	pm->cmd.upmove = 0; //no jump just yet
	return LS_A_BACKFLIP_ATK;
}

qboolean PM_CheckBackflipAttackMove()
{
	if (pm->ps->client_num < MAX_CLIENTS
		&& PM_InSecondaryStyle())
	{
		return qfalse;
	}

	//check to see if it's cancelled?
	if (pm->ps->saber[0].jumpAtkBackMove == LS_NONE)
	{
		if (pm->ps->dualSabers)
		{
			if (pm->ps->saber[1].jumpAtkBackMove == LS_NONE
				|| pm->ps->saber[1].jumpAtkBackMove == LS_INVALID)
			{
				return qfalse;
			}
		}
		else
		{
			return qfalse;
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].jumpAtkBackMove == LS_NONE)
		{
			if (pm->ps->saber[0].jumpAtkBackMove == LS_NONE
				|| pm->ps->saber[0].jumpAtkBackMove == LS_INVALID)
			{
				return qfalse;
			}
		}
	}
	//do normal checks

	if (pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1 //can force jump
		&& pm->ps->forceRageRecoveryTime < pm->cmd.serverTime //not in a force Rage recovery period
		&& pm->gent && !(pm->gent->flags & FL_LOCK_PLAYER_WEAPONS)
		// yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
		&& (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250))
		//on ground or just jumped (if not player)
	{
		if (pm->cmd.forwardmove < 0 //moving backwards
			&& pm->ps->saber_anim_level == SS_STAFF //using staff
			&& (pm->cmd.upmove > 0 || pm->ps->pm_flags & PMF_JUMPING)) //jumping
		{
			//jumping backwards and using staff
			if (!PM_SaberInTransitionAny(pm->ps->saber_move) //not going to/from/between an attack anim
				&& !PM_SaberInAttack(pm->ps->saber_move) //not in attack anim
				&& pm->ps->weaponTime <= 0 //not busy
				&& pm->cmd.buttons & BUTTON_ATTACK) //want to attack
			{
				//not already attacking
				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
				{
					//NPC
					if (pm->gent
						&& pm->gent->NPC
						&& (pm->gent->NPC->rank == RANK_CREWMAN || pm->gent->NPC->rank >= RANK_LT))
					{
						//acrobat or boss and higher can do this
						return qtrue;
					}
				}
				else
				{
					//player
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

static saber_moveName_t PM_CheckDualSpinProtect()
{
	if (pm->ps->client_num < MAX_CLIENTS && PM_InSecondaryStyle())
	{
		return LS_NONE;
	}
	if (!pm->ps->saber[0].Active() || !pm->ps->saber[1].Active())
	{
		return LS_NONE;
	}

	//see if we have an overridden (or cancelled) kata move
	if (pm->ps->saber[0].kataMove != LS_INVALID)
	{
		if (pm->ps->saber[0].kataMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].kataMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].kataMove != LS_INVALID)
		{
			if (pm->ps->saber[1].kataMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].kataMove);
			}
		}
	}
	//no overrides, cancelled?
	if (pm->ps->saber[0].kataMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].kataMove == LS_NONE)
		{
			return LS_NONE;
		}
	}
	//do normal checks
	const qboolean isPlayer = (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) ? qtrue : qfalse;
	if (pm->ps->saber_move == LS_READY //ready
		&& pm->ps->saber_anim_level == SS_DUAL //using dual saber style
		&& pm->ps->saber[0].Active() && pm->ps->saber[1].Active() //both sabers on
		&& G_TryingKataAttack(&pm->cmd)
		&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER, qtrue, isPlayer)
		&& pm->cmd.buttons & BUTTON_ATTACK)
	{
		if (pm->gent)
		{
			G_DrainPowerForSpecialMove(pm->gent, FP_PUSH, SABER_ALT_ATTACK_POWER, qtrue);
		}
		return LS_DUAL_SPIN_PROTECT;
	}
	return LS_NONE;
}

static saber_moveName_t PM_CheckStaffKata()
{
	if (pm->ps->client_num < MAX_CLIENTS
		&& PM_InSecondaryStyle())
	{
		return LS_NONE;
	}

	//see if we have an overridden (or cancelled) kata move
	if (pm->ps->saber[0].kataMove != LS_INVALID)
	{
		if (pm->ps->saber[0].kataMove != LS_NONE)
		{
			return static_cast<saber_moveName_t>(pm->ps->saber[0].kataMove);
		}
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].kataMove != LS_INVALID)
		{
			if (pm->ps->saber[1].kataMove != LS_NONE)
			{
				return static_cast<saber_moveName_t>(pm->ps->saber[1].kataMove);
			}
		}
	}
	//no overrides, cancelled?
	if (pm->ps->saber[0].kataMove == LS_NONE)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers)
	{
		if (pm->ps->saber[1].kataMove == LS_NONE)
		{
			return LS_NONE;
		}
	}
	//do normal checks
	const qboolean isPlayer = (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) ? qtrue : qfalse;
	if (pm->ps->saber_move == LS_READY //ready
		//&& (pm->ps->client_num < MAX_CLIENTS||PM_ControlledByPlayer())//PLAYER ONLY...?
		//&& pm->ps->viewangles[0] > 30 //looking down
		&& pm->ps->saber_anim_level == SS_STAFF //using dual saber style
		&& pm->ps->saber[0].Active() //saber on
		//&& pm->ps->forcePowerLevel[FP_PUSH]>=FORCE_LEVEL_3//force push 3
		//&& ((pm->ps->forcePowersActive&(1<<FP_PUSH))||pm->ps->forcePowerDebounce[FP_PUSH]>level.time)//force-pushing
		&& G_TryingKataAttack(&pm->cmd) //(pm->cmd.buttons&BUTTON_FORCE_FOCUS)//holding focus
		&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER, qtrue, isPlayer)
		//pm->ps->forcePower >= SABER_ALT_ATTACK_POWER//DUAL_SPIN_PROTECT_POWER//force push 3
		&& pm->cmd.buttons & BUTTON_ATTACK //pressing attack
		)
	{
		//FIXME: some NPC logic to do this?
		/*
		if ( (pm->ps->pm_flags&PMF_DUCKED||pm->cmd.upmove<0)//crouching
			&& g_crosshairEntNum >= ENTITYNUM_WORLD )
		*/
		{
			if (pm->gent)
			{
				G_DrainPowerForSpecialMove(pm->gent, FP_LEVITATION, SABER_ALT_ATTACK_POWER, qtrue);
				//drain the required force power
			}
			return LS_STAFF_SOULCAL;
		}
	}
	return LS_NONE;
}

extern qboolean WP_ForceThrowable(gentity_t* ent, const gentity_t* forward_ent, const gentity_t* self, qboolean pull,
	float cone,
	float radius, vec3_t forward);

saber_moveName_t PM_CheckPullAttack()
{
	if (pm->ps->client_num < MAX_CLIENTS
		&& PM_InSecondaryStyle())
	{
		return LS_NONE;
	}

	if (pm->ps->saber[0].saberFlags & SFL_NO_PULL_ATTACK)
	{
		return LS_NONE;
	}
	if (pm->ps->dualSabers
		&& pm->ps->saber[1].saberFlags & SFL_NO_PULL_ATTACK)
	{
		return LS_NONE;
	}

	const qboolean isPlayer = (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) ? qtrue : qfalse;
	if ((pm->ps->saber_move == LS_READY || PM_SaberInReturn(pm->ps->saber_move) || PM_SaberInReflect(pm->ps->saber_move))
		//ready
		&& pm->ps->saber_anim_level >= SS_FAST //single saber styles - FIXME: Tavion?
		&& pm->ps->saber_anim_level <= SS_STRONG //single saber styles - FIXME: Tavion?
		&& G_TryingPullAttack(pm->gent, &pm->cmd, qfalse)
		&& pm->cmd.buttons & BUTTON_ATTACK //attacking
		&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER, qfalse, isPlayer))
	{
		//FIXME: some NPC logic to do this?
		qboolean do_move = g_saberNewControlScheme->integer ? qtrue : qfalse;
		//in new control scheme, can always do this, even if there's no-one to do it to
		if (g_saberNewControlScheme->integer
			|| g_crosshairEntNum < ENTITYNUM_WORLD) //in old control scheme, there has to be someone there
		{
			saber_moveName_t pullAttackMove;
			if (pm->ps->saber_anim_level == SS_FAST)
			{
				pullAttackMove = LS_PULL_ATTACK_STAB;
			}
			else
			{
				pullAttackMove = LS_PULL_ATTACK_SWING;
			}

			if (g_crosshairEntNum < ENTITYNUM_WORLD && pm->gent && pm->gent->client)
			{
				gentity_t* targ_ent = &g_entities[g_crosshairEntNum];
				if (targ_ent->client
					&& targ_ent->health > 0
					//FIXME: check other things like in knockdown, saberlock, uninterpretable anims, etc.
					&& !PM_InOnGroundAnim(&targ_ent->client->ps)
					&& !PM_LockedAnim(targ_ent->client->ps.legsAnim)
					&& !PM_SuperBreakLoseAnim(targ_ent->client->ps.legsAnim)
					&& !PM_SuperBreakWinAnim(targ_ent->client->ps.legsAnim)
					&& targ_ent->client->ps.saberLockTime <= 0
					&& WP_ForceThrowable(targ_ent, targ_ent, pm->gent, qtrue, 1.0f, 0.0f, nullptr))
				{
					if (!g_saberNewControlScheme->integer)
					{
						//in old control scheme, make sure they're close or far enough away for the move we'll be doing
						const float targ_dist = Distance(targ_ent->currentOrigin, pm->ps->origin);
						if (pullAttackMove == LS_PULL_ATTACK_STAB)
						{
							//must be closer than 512
							if (targ_dist > 384.0f)
							{
								return LS_NONE;
							}
						}
						else //if ( pullAttackMove == LS_PULL_ATTACK_SWING )
						{
							//must be farther than 256
							if (targ_dist > 512.0f)
							{
								return LS_NONE;
							}
							if (targ_dist < 192.0f)
							{
								return LS_NONE;
							}
						}
					}

					vec3_t targ_angles = { 0, targ_ent->client->ps.viewangles[YAW], 0 };
					if (InFront(pm->ps->origin, targ_ent->currentOrigin, targ_angles))
					{
						NPC_SetAnim(targ_ent, SETANIM_BOTH, BOTH_PULLED_INAIR_F, SETANIM_FLAG_OVERRIDE,
							SETANIM_FLAG_HOLD);
					}
					else
					{
						NPC_SetAnim(targ_ent, SETANIM_BOTH, BOTH_PULLED_INAIR_B, SETANIM_FLAG_OVERRIDE,
							SETANIM_FLAG_HOLD);
					}
					//hold the anim until I'm with done pull anim
					targ_ent->client->ps.legsAnimTimer = targ_ent->client->ps.torsoAnimTimer = PM_AnimLength(
						pm->gent->client->clientInfo.animFileIndex,
						static_cast<animNumber_t>(saber_moveData[pullAttackMove].animToUse));
					//set pullAttackTime
					pm->gent->client->ps.pullAttackTime = targ_ent->client->ps.pullAttackTime = level.time + targ_ent->
						client->ps.legsAnimTimer;
					//make us know about each other
					pm->gent->client->ps.pullAttackEntNum = g_crosshairEntNum;
					targ_ent->client->ps.pullAttackEntNum = pm->ps->client_num;
					//do effect and sound on me
					pm->ps->powerups[PW_FORCE_PUSH] = level.time + 1000;
					if (pm->gent)
					{
						G_Sound(pm->gent, G_SoundIndex("sound/weapons/force/pull.wav"));
					}
					do_move = qtrue;
				}
			}
			if (do_move)
			{
				if (pm->gent)
				{
					G_DrainPowerForSpecialMove(pm->gent, FP_PULL, SABER_ALT_ATTACK_POWER);
				}
				return pullAttackMove;
			}
		}
	}
	return LS_NONE;
}

static saber_moveName_t PM_CheckPlayerAttackFromParry(const int curmove)
{
	if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
	{
		if (curmove >= LS_PARRY_UP && curmove <= LS_REFLECT_LL)
		{
			//in a parry
			switch (saber_moveData[curmove].endQuad)
			{
			case Q_T:
				return LS_A_T2B;
			case Q_TR:
				return LS_A_TR2BL;
			case Q_TL:
				return LS_A_TL2BR;
			case Q_BR:
				return LS_A_BR2TL;
			case Q_BL:
				return LS_A_BL2TR;
			default:;
				//shouldn't be a parry that ends at L, R or B
			}
		}
	}
	return LS_NONE;
}

saber_moveName_t PM_SaberAttackForMovement(const int forwardmove, const int rightmove, const int curmove)
{
	qboolean no_specials = qfalse;
	saber_moveName_t newmove = LS_NONE;

	if (pm->ps->client_num < MAX_CLIENTS
		&& PM_InSecondaryStyle())
	{
		no_specials = qtrue;
	}

	saber_moveName_t overrideJumpRightAttackMove = LS_INVALID;
	if (pm->ps->saber[0].jumpAtkRightMove != LS_INVALID)
	{
		if (pm->ps->saber[0].jumpAtkRightMove != LS_NONE)
		{
			//actually overriding
			overrideJumpRightAttackMove = static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkRightMove);
		}
		else if (pm->ps->dualSabers
			&& pm->ps->saber[1].jumpAtkRightMove > LS_NONE)
		{
			//would be cancelling it, but check the second saber, too
			overrideJumpRightAttackMove = static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkRightMove);
		}
		else
		{
			//nope, just cancel it
			overrideJumpRightAttackMove = LS_NONE;
		}
	}
	else if (pm->ps->dualSabers
		&& pm->ps->saber[1].jumpAtkRightMove != LS_INVALID)
	{
		//first saber not overridden, check second
		overrideJumpRightAttackMove = static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkRightMove);
	}

	saber_moveName_t overrideJumpLeftAttackMove = LS_INVALID;
	if (pm->ps->saber[0].jumpAtkLeftMove != LS_INVALID)
	{
		if (pm->ps->saber[0].jumpAtkLeftMove != LS_NONE)
		{
			//actually overriding
			overrideJumpLeftAttackMove = static_cast<saber_moveName_t>(pm->ps->saber[0].jumpAtkLeftMove);
		}
		else if (pm->ps->dualSabers
			&& pm->ps->saber[1].jumpAtkLeftMove > LS_NONE)
		{
			//would be cancelling it, but check the second saber, too
			overrideJumpLeftAttackMove = static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkLeftMove);
		}
		else
		{
			//nope, just cancel it
			overrideJumpLeftAttackMove = LS_NONE;
		}
	}
	else if (pm->ps->dualSabers
		&& pm->ps->saber[1].jumpAtkLeftMove != LS_INVALID)
	{
		//first saber not overridden, check second
		overrideJumpLeftAttackMove = static_cast<saber_moveName_t>(pm->ps->saber[1].jumpAtkLeftMove);
	}

	const qboolean isPlayer = (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) ? qtrue : qfalse;
	if (rightmove > 0)
	{
		//moving right
		if (!no_specials
			&& overrideJumpRightAttackMove != LS_NONE
			&& (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)
			//on ground or just jumped
			&& pm->cmd.buttons & BUTTON_ATTACK //hitting attack
			&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 //have force jump 1 at least
			&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_LR, qfalse, isPlayer) //have enough power
			&& ((!isPlayer && pm->cmd.upmove > 0) //jumping NPC
				|| (isPlayer && G_TryingCartwheel(pm->gent, &pm->cmd)))) //focus-holding player
		{
			//cartwheel right
			vec3_t right;
			const vec3_t fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
			if (pm->gent)
			{
				G_DrainPowerForSpecialMove(pm->gent, FP_LEVITATION, SABER_ALT_ATTACK_POWER_LR);
			}
			pm->cmd.upmove = 0;
			if (overrideJumpRightAttackMove != LS_INVALID)
			{
				//overridden with another move
				return overrideJumpRightAttackMove;
			}
			if (pm->ps->saber_anim_level == SS_STAFF)
			{
				AngleVectors(fwd_angles, nullptr, right, nullptr);
				pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
				VectorMA(pm->ps->velocity, 190, right, pm->ps->velocity);
				return LS_BUTTERFLY_RIGHT;
			}
			if (!(pm->ps->saber[0].saberFlags & SFL_NO_CARTWHEELS)
				&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_CARTWHEELS)))
			{
				//okay to do cartwheels with this saber
				{
					//in air
					AngleVectors(fwd_angles, nullptr, right, nullptr);
					pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
					VectorMA(pm->ps->velocity, 190, right, pm->ps->velocity);
					PM_SetJumped(JUMP_VELOCITY, qtrue);
					return LS_JUMPATTACK_ARIAL_RIGHT;
				}
			}
		}
		else if (pm->ps->legsAnim != BOTH_CARTWHEEL_RIGHT
			&& pm->ps->legsAnim != BOTH_ARIAL_RIGHT)
		{
			//not in a cartwheel/arial
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}
			//checked all special attacks, if we're in a parry, attack from that move
			const saber_moveName_t parry_attack_move = PM_CheckPlayerAttackFromParry(curmove);
			if (parry_attack_move != LS_NONE)
			{
				return parry_attack_move;
			}
			//check regular attacks
			if (forwardmove > 0)
			{
				//forward right = TL2BR slash
				return LS_A_TL2BR;
			}
			if (forwardmove < 0)
			{
				//backward right = BL2TR uppercut
				return LS_A_BL2TR;
			}
			//just right is a left slice
			return LS_A_L2R;
		}
	}
	else if (rightmove < 0)
	{
		//moving left
		if (!no_specials
			&& overrideJumpLeftAttackMove != LS_NONE
			&& (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)
			//on ground or just jumped
			&& pm->cmd.buttons & BUTTON_ATTACK //hitting attack
			&& pm->ps->forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_0 //have force jump 1 at least
			&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_LR, qfalse, isPlayer) //have enough power
			&& ((!isPlayer && pm->cmd.upmove > 0) //jumping NPC
				|| (isPlayer && G_TryingCartwheel(pm->gent, &pm->cmd)))) //focus-holding player
		{
			//cartwheel left
			vec3_t right;
			const vec3_t fwd_angles = { 0, pm->ps->viewangles[YAW], 0 };
			if (pm->gent)
			{
				G_DrainPowerForSpecialMove(pm->gent, FP_LEVITATION, SABER_ALT_ATTACK_POWER_LR);
			}
			pm->cmd.upmove = 0;
			if (overrideJumpRightAttackMove != LS_INVALID)
			{
				//overridden with another move
				return overrideJumpRightAttackMove;
			}
			if (pm->ps->saber_anim_level == SS_STAFF)
			{
				AngleVectors(fwd_angles, nullptr, right, nullptr);
				pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
				VectorMA(pm->ps->velocity, -190, right, pm->ps->velocity);
				return LS_BUTTERFLY_LEFT;
			}
			if (!(pm->ps->saber[0].saberFlags & SFL_NO_CARTWHEELS)
				&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_CARTWHEELS)))
			{
				//okay to do cartwheels with this saber
				{
					AngleVectors(fwd_angles, nullptr, right, nullptr);
					pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
					VectorMA(pm->ps->velocity, -190, right, pm->ps->velocity);
					PM_SetJumped(JUMP_VELOCITY, qtrue);
					return LS_JUMPATTACK_CART_LEFT;
				}
			}
		}
		else if (pm->ps->legsAnim != BOTH_CARTWHEEL_LEFT
			&& pm->ps->legsAnim != BOTH_ARIAL_LEFT)
		{
			//not in a left cartwheel/arial
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}
			//checked all special attacks, if we're in a parry, attack from that move
			const saber_moveName_t parry_attack_move = PM_CheckPlayerAttackFromParry(curmove);
			if (parry_attack_move != LS_NONE)
			{
				return parry_attack_move;
			}
			//check regular attacks
			if (forwardmove > 0)
			{
				//forward left = TR2BL slash
				return LS_A_TR2BL;
			}
			if (forwardmove < 0)
			{
				//backward left = BR2TL uppercut
				return LS_A_BR2TL;
			}
			//just left is a right slice
			return LS_A_R2L;
		}
	}
	else
	{
		//not moving left or right
		if (forwardmove > 0)
		{
			//forward= T2B slash
			const saber_moveName_t stab_down_move = no_specials ? LS_NONE : PM_CheckStabDown();
			if (stab_down_move != LS_NONE)
			{
				return stab_down_move;
			}
			if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.
				zoomMode) //player in third person, not zoomed in
			{
				//player in thirdperson, not zoomed in
				//flip-over attack logic
				if (!no_specials && PM_CheckFlipOverAttackMove(qfalse))
				{
					//flip over-forward down-attack
					return PM_SaberFlipOverAttackMove();
				}
				//lunge attack logic
				if (PM_CheckLungeAttackMove())
				{
					return PM_SaberLungeAttackMove(qtrue);
				}
				//jump forward attack logic
				if (!no_specials && PM_CheckJumpForwardAttackMove())
				{
					return PM_SaberJumpForwardAttackMove();
				}
			}

			//player NPC with enemy: autoMove logic
			if (pm->gent
				&& pm->gent->enemy
				&& pm->gent->enemy->client)
			{
				//I have an active enemy
				if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer())
				{
					//a player who is running at an enemy
					//if the enemy is not a jedi, don't use top-down, pick a diagonal or side attack
					if (pm->gent->enemy->s.weapon != WP_SABER
						&& pm->gent->enemy->client->NPC_class != CLASS_REMOTE //too small to do auto-aiming accurately
						&& pm->gent->enemy->client->NPC_class != CLASS_SEEKER //too small to do auto-aiming accurately
						&& pm->gent->enemy->client->NPC_class != CLASS_GONK //too short to do auto-aiming accurately
						&& pm->gent->enemy->client->NPC_class != CLASS_HOWLER //too short to do auto-aiming accurately
						&& g_saberAutoAim->integer)
					{
						const saber_moveName_t auto_move = PM_AttackForEnemyPos(
							qfalse,
							static_cast<qboolean>(pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()));
						if (auto_move != LS_INVALID)
						{
							return auto_move;
						}
					}
				}

				if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()) //NPC ONLY
				{
					//NPC
					if (PM_CheckFlipOverAttackMove(qtrue))
					{
						return PM_SaberFlipOverAttackMove();
					}
				}
			}

			//Regular NPCs
			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer()) //NPC ONLY
			{
				//NPC or player in third person, not zoomed in
				//fwd jump attack logic
				if (PM_CheckJumpForwardAttackMove())
				{
					return PM_SaberJumpForwardAttackMove();
				}
				//lunge attack logic
				if (PM_CheckLungeAttackMove())
				{
					return PM_SaberLungeAttackMove(qtrue);
				}
			}

			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}

			//checked all special attacks, if we're in a parry, attack from that move
			const saber_moveName_t parry_attack_move = PM_CheckPlayerAttackFromParry(curmove);
			if (parry_attack_move != LS_NONE)
			{
				return parry_attack_move;
			}
			//check regular attacks
			return LS_A_T2B;
		}
		if (forwardmove < 0)
		{
			//backward= T2B slash//B2T uppercut?
			if (g_saberNewControlScheme->integer)
			{
				const saber_moveName_t pull_atk = PM_CheckPullAttack();
				if (pull_atk != LS_NONE)
				{
					return pull_atk;
				}
			}

			if (g_saberNewControlScheme->integer
				&& (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
				&& pm->cmd.buttons & BUTTON_FORCE_FOCUS) //Holding focus, trying special backwards attacks
			{
				//player lunge attack logic
				if ((pm->ps->dualSabers //or dual
					|| pm->ps->saber_anim_level == SS_STAFF) //or staff
					&& G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB))
					//have enough force power to pull it off
				{
					//alt+back+attack using fast, dual or staff attacks
					PM_SaberLungeAttackMove(qfalse);
				}
			}
			else if (pm->ps->client_num && !PM_ControlledByPlayer() //NPC
				|| (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.
				zoomMode) //player in third person, not zooomed
			{
				//NPC or player in third person, not zoomed
				if (PM_CheckBackflipAttackMove())
				{
					return PM_SaberBackflipAttackMove(); //backflip attack
				}
				if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
				{
					//player
					if (G_TryingSpecial(&pm->cmd)) //Holding focus
					{
						//if no special worked, do nothing
						return LS_NONE;
					}
				}
				//check backstabs
				if (!(pm->ps->saber[0].saberFlags & SFL_NO_BACK_ATTACK)
					&& (!pm->ps->dualSabers || !(pm->ps->saber[1].saberFlags & SFL_NO_BACK_ATTACK)))
				{
					//okay to do backstabs with this saber
					if (pm->ps->groundEntityNum != ENTITYNUM_NONE)
					{
						//only when on ground
						if (pm->gent && pm->gent->enemy)
						{
							//FIXME: or just trace for a valid enemy standing behind me?  And no enemy in front?
							vec3_t enemy_dir, face_fwd;
							const vec3_t facing_angles = { 0, pm->ps->viewangles[YAW], 0 };
							AngleVectors(facing_angles, face_fwd, nullptr, nullptr);
							VectorSubtract(pm->gent->enemy->currentOrigin, pm->ps->origin, enemy_dir);
							const float dot = DotProduct(enemy_dir, face_fwd);
							if (dot < 0)
							{
								//enemy is behind me
								if (dot < -0.75f
									&& DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 16384
									//128 squared
									&& (pm->ps->saber_anim_level == SS_FAST || pm->ps->saber_anim_level == SS_STAFF || pm->
										gent->client && (pm->gent->client->NPC_class == CLASS_TAVION || pm->gent->client
											->NPC_class == CLASS_ALORA) && Q_irand(0, 1)))
								{
									//fast attacks and Tavion
									if (!(pm->ps->pm_flags & PMF_DUCKED) && pm->cmd.upmove >= 0)
									{
										//can't do it while ducked?
										if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() || pm->gent->NPC
											&& pm->gent->NPC->rank >= RANK_LT_JG)
										{
											//only fencers and above can do this
											return LS_A_BACKSTAB;
										}
									}
								}
								else if (pm->ps->saber_anim_level != SS_FAST && pm->ps->saber_anim_level != SS_STAFF)
								{
									//medium and higher attacks
									if (pm->ps->pm_flags & PMF_DUCKED || pm->cmd.upmove < 0)
									{
										return LS_A_BACK_CR;
									}
									return LS_A_BACK;
								}
							}
							else
							{
								//enemy in front
								const float enemy_dist_sq = DistanceSquared(
									pm->gent->currentOrigin, pm->gent->enemy->currentOrigin);
								if ((pm->ps->saber_anim_level == FORCE_LEVEL_1 ||
									pm->ps->saber_anim_level == SS_STAFF ||
									pm->gent->client->NPC_class == CLASS_TAVION ||
									pm->gent->client->NPC_class == CLASS_ALORA ||
									pm->gent->client->NPC_class == CLASS_VADER ||
									pm->gent->client->NPC_class == CLASS_DESANN && !Q_irand(0, 3)) &&
									enemy_dist_sq > 16384 ||
									pm->gent->enemy->health <= 0) //128 squared
								{
									//my enemy is pretty far in front of me and I'm using fast attacks
									if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() ||
										pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >=
										RANK_LT_JG && Q_irand(0, pm->gent->NPC->rank) > RANK_ENSIGN)
									{
										//only fencers and higher can do this, higher rank does it more
										if (PM_CheckEnemyInBack(128))
										{
											return PM_PickBackStab();
										}
									}
								}
								else if ((pm->ps->saber_anim_level >= FORCE_LEVEL_2
									|| pm->gent->client->NPC_class == CLASS_DESANN
									|| pm->gent->client->NPC_class == CLASS_VADER)
									&& enemy_dist_sq > 40000 || pm->gent->enemy->health <= 0) //200 squared
								{
									//enemy is very faw away and I'm using medium/strong attacks
									if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer() ||
										pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >=
										RANK_LT_JG && Q_irand(0, pm->gent->NPC->rank) > RANK_ENSIGN)
									{
										//only fencers and higher can do this, higher rank does it more
										if (PM_CheckEnemyInBack(164))
										{
											return PM_PickBackStab();
										}
									}
								}
							}
						}
						else
						{
							//no current enemy
							if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->gent && pm->gent->
								client)
							{
								//only player
								if (PM_CheckEnemyInBack(128))
								{
									return PM_PickBackStab();
								}
							}
						}
					}
				}
			}

			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}

			//checked all special attacks, if we're in a parry, attack from that move
			const saber_moveName_t parry_attack_move = PM_CheckPlayerAttackFromParry(curmove);
			if (parry_attack_move != LS_NONE)
			{
				return parry_attack_move;
			}
			//check regular attacks else just swing down
			return LS_A_T2B;
		}
		//not moving in any direction
		if (PM_SaberInParry(curmove) || PM_SaberInBrokenParry(curmove))
		{
			//parries, return to the start position if a direction isn't given.
			return LS_READY;
		}
		if (PM_SaberInBounce(curmove) || PM_SaberInMassiveBounce(pm->ps->torsoAnim))
		{
			//bounces should go to their default attack if you don't specify a direction but are attacking
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}

			if (pm->ps->client_num && !PM_ControlledByPlayer() && Q_irand(0, 3))
			{//use NPC random
				newmove = PM_NPCSaberAttackFromQuad(saber_moveData[curmove].endQuad);
			}
			else
			{//player uses chain-attack
				newmove = saber_moveData[curmove].chain_attack;
			}
			if (PM_SaberKataDone(curmove, newmove))
			{
				return saber_moveData[curmove].chain_idle;
			}
			return newmove;
		}
		if (PM_SaberInKnockaway(curmove))
		{
			//bounces should go to their default attack if you don't specify a direction but are attacking
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}
			if (pm->ps->client_num && !PM_ControlledByPlayer() && Q_irand(0, 3))
			{//use NPC random
				newmove = PM_NPCSaberAttackFromQuad(saber_moveData[curmove].endQuad);
			}
			else
			{
				if (pm->ps->saber_anim_level == SS_FAST ||
					pm->ps->saber_anim_level == SS_TAVION)
				{
					//player is in fast attacks, so come right back down from the same spot
					newmove = PM_AttackMoveForQuad(saber_moveData[curmove].endQuad);
				}
				else
				{
					//use a transition to wrap to another attack from a different dir
					newmove = saber_moveData[curmove].chain_attack;
				}
			}
			if (PM_SaberKataDone(curmove, newmove))
			{
				return saber_moveData[curmove].chain_idle;
			}
			return newmove;
		}
		if (curmove == LS_A_FLIP_STAB
			|| curmove == LS_A_FLIP_SLASH
			|| curmove >= LS_PARRY_UP
			&& curmove <= LS_REFLECT_LL)
		{
			//Not moving at all, not too busy to attack
			//push + lookdown + attack + dual sabers = LS_DUAL_SPIN_PROTECT
			if (g_saberNewControlScheme->integer)
			{
				if (PM_CheckDualSpinProtect())
				{
					return LS_DUAL_SPIN_PROTECT;
				}
				if (PM_CheckStaffKata())
				{
					return LS_STAFF_SOULCAL;
				}
			}
			if (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) //PLAYER ONLY
			{
				//player
				if (G_TryingSpecial(&pm->cmd)) //Holding focus
				{
					//if no special worked, do nothing
					return LS_NONE;
				}
			}
			//checked all special attacks, if we're in a parry, attack from that move
			const saber_moveName_t parry_attack_move = PM_CheckPlayerAttackFromParry(curmove);
			if (parry_attack_move != LS_NONE)
			{
				return parry_attack_move;
			}
			//check regular attacks
			if (pm->ps->client_num || g_saberAutoAim->integer)
			{
				//auto-aim
				if (pm->gent && pm->gent->enemy)
				{
					//based on enemy position, pick a proper attack
					const saber_moveName_t auto_move = PM_AttackForEnemyPos(
						qtrue, static_cast<qboolean>(pm->ps->client_num >= MAX_CLIENTS));
					if (auto_move != LS_INVALID)
					{
						return auto_move;
					}
				}
				else if (fabs(pm->ps->viewangles[0]) > 30)
				{
					//looking far up or far down uses the top to bottom attack, presuming you want a vertical attack
					return LS_A_T2B;
				}
			}
			else
			{
				//for now, just pick a random attack
				return static_cast<saber_moveName_t>(Q_irand(LS_A_TL2BR, LS_A_T2B));
			}
		}
		else if (curmove == LS_READY)
		{
			//for now, just pick a random attack
			if (g_SerenityJediEngineMode->integer == 2)
			{
				return static_cast<saber_moveName_t>(Q_irand(LS_A_TL2BR, LS_A_T2B));
			}
			return LS_A_T2B; //decided we don't like random attacks when idle, use an overhead instead.
		}
	}
	return newmove;
}

extern int pm_returnfor_quad(int quad);

saber_moveName_t PM_SaberAnimTransitionMove(const saber_moveName_t curmove, const saber_moveName_t newmove)
{
	int retmove = newmove;

	if (curmove == LS_READY)
	{
		//just standing there
		switch (newmove)
		{
		case LS_A_TL2BR:
		case LS_A_L2R:
		case LS_A_BL2TR:
		case LS_A_BR2TL:
		case LS_A_R2L:
		case LS_A_TR2BL:
		case LS_A_T2B:
			//transition is the start
			retmove = LS_S_TL2BR + (newmove - LS_A_TL2BR);
			break;
		default:
			break;
		}
	}
	else
	{
		switch (newmove)
		{
			//transitioning to ready pose
		case LS_READY:
			switch (curmove)
			{
				//transitioning from an attack
			case LS_A_TL2BR:
			case LS_A_L2R:
			case LS_A_BL2TR:
			case LS_A_BR2TL:
			case LS_A_R2L:
			case LS_A_TR2BL:
			case LS_A_T2B:
				//transition is the return
				retmove = LS_R_TL2BR + (newmove - LS_A_TL2BR);
				break;
			case LS_B1_BR:
			case LS_B1__R:
			case LS_B1_TR:
			case LS_B1_T_:
			case LS_B1_TL:
			case LS_B1__L:
			case LS_B1_BL:
				//transitioning from a parry/reflection/knockaway/broken parry
			case LS_PARRY_UP:
			case LS_PARRY_UR:
			case LS_PARRY_UL:
			case LS_PARRY_LR:
			case LS_PARRY_LL:
			case LS_PARRY_B:
			case LS_REFLECT_UP:
			case LS_REFLECT_UR:
			case LS_REFLECT_UL:
			case LS_REFLECT_LR:
			case LS_REFLECT_LL:
			case LS_REFLECT_B:
			case LS_K1_T_:
			case LS_K1_TR:
			case LS_K1_TR_MD:
			case LS_K1_TR_PB:
			case LS_K1_TL:
			case LS_K1_TL_MD:
			case LS_K1_TL_PB:
			case LS_K1_BR:
			case LS_K1_BL:
			case LS_V1_BR:
			case LS_V1__R:
			case LS_V1_TR:
			case LS_V1_T_:
			case LS_V1_TL:
			case LS_V1__L:
			case LS_V1_BL:
			case LS_V1_B_:
			case LS_H1_T_:
			case LS_H1_TR:
			case LS_H1_TL:
			case LS_H1_BR:
			case LS_H1_BL:
				retmove = pm_returnfor_quad(saber_moveData[curmove].endQuad);
				break;
			default:;
			}
			break;
			//transitioning to an attack
		case LS_A_TL2BR:
		case LS_A_L2R:
		case LS_A_BL2TR:
		case LS_A_BR2TL:
		case LS_A_R2L:
		case LS_A_TR2BL:
		case LS_A_T2B:
			if (newmove == curmove)
			{
				if (PM_SaberKataDone(curmove, newmove))
				{
					//done with this kata, must return to ready before attack again
					retmove = LS_R_TL2BR + (newmove - LS_A_TL2BR);
				}
				else
				{
					//okay to chain to another attack
					retmove = transitionMove[saber_moveData[curmove].endQuad][saber_moveData[newmove].startQuad];
				}
			}
			else if (saber_moveData[curmove].endQuad == saber_moveData[newmove].startQuad)
			{
				//new move starts from same quadrant
				retmove = newmove;
			}
			else
			{
				switch (curmove)
				{
					//transitioning from an attack
				case LS_A_TL2BR:
				case LS_A_L2R:
				case LS_A_BL2TR:
				case LS_A_BR2TL:
				case LS_A_R2L:
				case LS_A_TR2BL:
				case LS_A_T2B:
				case LS_D1_BR:
				case LS_D1__R:
				case LS_D1_TR:
				case LS_D1_T_:
				case LS_D1_TL:
				case LS_D1__L:
				case LS_D1_BL:
				case LS_D1_B_:
					retmove = transitionMove[saber_moveData[curmove].endQuad][saber_moveData[newmove].startQuad];
					break;
					//transitioning from a return
				case LS_R_TL2BR:
				case LS_R_L2R:
				case LS_R_BL2TR:
				case LS_R_BR2TL:
				case LS_R_R2L:
				case LS_R_TR2BL:
				case LS_R_T2B:
					//transition is the start
					retmove = LS_S_TL2BR + (newmove - LS_A_TL2BR);
					break;
					//transitioning from a bounce
					//bounces should transition to transitions before attacks.
				case LS_B1_BR:
				case LS_B1__R:
				case LS_B1_TR:
				case LS_B1_T_:
				case LS_B1_TL:
				case LS_B1__L:
				case LS_B1_BL:
					//transitioning from a parry/reflection/knockaway/broken parry
				case LS_PARRY_UP:
				case LS_PARRY_UR:
				case LS_PARRY_UL:
				case LS_PARRY_LR:
				case LS_PARRY_LL:
				case LS_PARRY_B:
				case LS_REFLECT_UP:
				case LS_REFLECT_UR:
				case LS_REFLECT_UL:
				case LS_REFLECT_LR:
				case LS_REFLECT_LL:
				case LS_REFLECT_B:
				case LS_K1_T_:
				case LS_K1_TR:
				case LS_K1_TR_MD:
				case LS_K1_TR_PB:
				case LS_K1_TL:
				case LS_K1_TL_MD:
				case LS_K1_TL_PB:
				case LS_K1_BR:
				case LS_K1_BL:
				case LS_V1_BR:
				case LS_V1__R:
				case LS_V1_TR:
				case LS_V1_T_:
				case LS_V1_TL:
				case LS_V1__L:
				case LS_V1_BL:
				case LS_V1_B_:
				case LS_H1_T_:
				case LS_H1_TR:
				case LS_H1_TL:
				case LS_H1_BR:
				case LS_H1_BL:
					retmove = transitionMove[saber_moveData[curmove].endQuad][saber_moveData[newmove].startQuad];
					break;
					//NB: transitioning from transitions is fine
				default:
					break;
				}
			}
			break;
			//transitioning to any other anim is not supported
		default:
			break;
		}
	}

	if (retmove == LS_NONE)
	{
		return newmove;
	}

	return static_cast<saber_moveName_t>(retmove);
}

/*
-------------------------
PM_LegsAnimForFrame
Returns animNumber for current frame
-------------------------
*/
int PM_LegsAnimForFrame(gentity_t* ent, const int legs_frame)
{
	//Must be a valid client
	if (ent->client == nullptr)
		return -1;

	//Must have a file index entry
	if (ValidAnimFileIndex(ent->client->clientInfo.animFileIndex) == qfalse)
		return -1;

	const animation_t* animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
	const int gla_index = gi.G2API_GetAnimIndex(&ent->ghoul2[0]);

	for (int animation = 0; animation < BOTH_CIN_1; animation++) //first anim after last legs
	{
		if (animation >= TORSO_DROPWEAP1 && animation < LEGS_TURN1) //first legs only anim
		{
			//not a possible legs anim
			continue;
		}

		if (animations[animation].glaIndex != gla_index)
		{
			continue;
		}

		if (animations[animation].firstFrame > legs_frame)
		{
			//This anim starts after this frame
			continue;
		}

		if (animations[animation].firstFrame + animations[animation].numFrames < legs_frame)
		{
			//This anim ends before this frame
			continue;
		}
		//else, must be in this anim!
		return animation;
	}

	//Not in ANY torsoAnim?  SHOULD NEVER HAPPEN
	//	assert(0);
	return -1;
}

int PM_ValidateAnimRange(const int startFrame, const int endFrame, const float animSpeed)
{
	//given a startframe and endframe, see if that lines up with any known animation
	const animation_t* animations = level.knownAnimFileSets[0].animations;

	for (int anim = 0; anim < MAX_ANIMATIONS; anim++)
	{
		if (animSpeed < 0)
		{
			//playing backwards
			if (animations[anim].firstFrame == endFrame)
			{
				if (animations[anim].numFrames + animations[anim].firstFrame == startFrame)
				{
					//Com_Printf( "valid reverse anim: %s\n", anim_table[anim].name );
					return anim;
				}
			}
		}
		else
		{
			//playing forwards
			if (animations[anim].firstFrame == startFrame)
			{
				//This anim starts on this frame
				if (animations[anim].firstFrame + animations[anim].numFrames == endFrame)
				{
					//This anim ends on this frame
					//Com_Printf( "valid forward anim: %s\n", anim_table[anim].name );
					return anim;
				}
			}
		}
		//else, must not be this anim!
	}

	//Not in ANY anim?  SHOULD NEVER HAPPEN
	Com_Printf("invalid anim range %d to %d, speed %4.2f\n", startFrame, endFrame, animSpeed);
	return -1;
}

/*
-------------------------
PM_TorsoAnimForFrame
Returns animNumber for current frame
-------------------------
*/
int PM_TorsoAnimForFrame(gentity_t* ent, const int torso_frame)
{
	//Must be a valid client
	if (ent->client == nullptr)
		return -1;

	//Must have a file index entry
	if (ValidAnimFileIndex(ent->client->clientInfo.animFileIndex) == qfalse)
		return -1;

	const animation_t* animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
	const int gla_index = gi.G2API_GetAnimIndex(&ent->ghoul2[0]);

	for (int animation = 0; animation < LEGS_TURN1; animation++) //first legs only anim
	{
		if (animations[animation].glaIndex != gla_index)
		{
			continue;
		}

		if (animations[animation].firstFrame > torso_frame)
		{
			//This anim starts after this frame
			continue;
		}

		if (animations[animation].firstFrame + animations[animation].numFrames < torso_frame)
		{
			//This anim ends before this frame
			continue;
		}
		//else, must be in this anim!
		return animation;
	}
	return -1;
}

qboolean PM_FinishedCurrentLegsAnim(gentity_t* self)
{
	int junk;
	float current_frame, animSpeed;

	if (!self->client)
	{
		return qtrue;
	}

	gi.G2API_GetBoneAnimIndex(&self->ghoul2[self->playerModel], self->rootBone, cg.time ? cg.time : level.time,
		&current_frame, &junk, &junk, &junk, &animSpeed, nullptr);
	const int cur_frame = floor(current_frame);

	const int legs_anim = self->client->ps.legsAnim;
	const animation_t* animations = level.knownAnimFileSets[self->client->clientInfo.animFileIndex].animations;

	if (cur_frame >= animations[legs_anim].firstFrame + (animations[legs_anim].numFrames - 2))
	{
		return qtrue;
	}

	return qfalse;
}

/*
-------------------------
PM_HasAnimation
-------------------------
*/

qboolean PM_HasAnimation(const gentity_t* ent, const int animation)
{
	//Must be a valid client
	if (!ent || ent->client == nullptr)
		return qfalse;

	//must be a valid anim number
	if (animation < 0 || animation >= MAX_ANIMATIONS)
	{
		return qfalse;
	}
	//Must have a file index entry
	if (ValidAnimFileIndex(ent->client->clientInfo.animFileIndex) == qfalse)
		return qfalse;

	const animation_t* animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;

	//No frames, no anim
	if (animations[animation].numFrames == 0)
		return qfalse;

	//Has the sequence
	return qtrue;
}

int PM_PickAnim(const gentity_t* self, const int min_anim, const int max_anim)
{
	int anim;
	int count = 0;

	if (!self)
	{
		return Q_irand(min_anim, max_anim);
	}

	do
	{
		anim = Q_irand(min_anim, max_anim);
		count++;
	} while (!PM_HasAnimation(self, anim) && count < 1000);

	return anim;
}

/*
-------------------------
PM_AnimLength
-------------------------
*/

int PM_AnimLength(const int index, const animNumber_t anim)
{
	if (!ValidAnimFileIndex(index) || static_cast<int>(anim) < 0 || anim >= MAX_ANIMATIONS)
	{
		return 0;
	}

	return level.knownAnimFileSets[index].animations[anim].numFrames * abs(
		level.knownAnimFileSets[index].animations[anim].frameLerp);
}

/*
-------------------------
PM_SetLegsAnimTimer
-------------------------
*/

void PM_SetLegsAnimTimer(gentity_t* ent, int* legs_anim_timer, const int time)
{
	*legs_anim_timer = time;

	if (*legs_anim_timer < 0 && time != -1)
	{
		//Cap timer to 0 if was counting down, but let it be -1 if that was intentional
		*legs_anim_timer = 0;
	}

	if (!*legs_anim_timer && ent && Q3_TaskIDPending(ent, TID_ANIM_LOWER))
	{
		//Waiting for legsAnimTimer to complete, and it just got set to zero
		if (!Q3_TaskIDPending(ent, TID_ANIM_BOTH))
		{
			//Not waiting for top
			Q3_TaskIDComplete(ent, TID_ANIM_LOWER);
		}
		else
		{
			//Waiting for both to finish before complete
			Q3_TaskIDClear(&ent->taskID[TID_ANIM_LOWER]); //Bottom is done, regardless
			if (!Q3_TaskIDPending(ent, TID_ANIM_UPPER))
			{
				//top is done and we're done
				Q3_TaskIDComplete(ent, TID_ANIM_BOTH);
			}
		}
	}
}

/*
-------------------------
PM_SetTorsoAnimTimer
-------------------------
*/

void PM_SetTorsoAnimTimer(gentity_t* ent, int* torso_anim_timer, const int time)
{
	*torso_anim_timer = time;

	if (*torso_anim_timer < 0 && time != -1)
	{
		//Cap timer to 0 if was counting down, but let it be -1 if that was intentional
		*torso_anim_timer = 0;
	}

	if (!*torso_anim_timer && ent && Q3_TaskIDPending(ent, TID_ANIM_UPPER))
	{
		//Waiting for torsoAnimTimer to complete, and it just got set to zero
		if (!Q3_TaskIDPending(ent, TID_ANIM_BOTH))
		{
			//Not waiting for bottom
			Q3_TaskIDComplete(ent, TID_ANIM_UPPER);
		}
		else
		{
			//Waiting for both to finish before complete
			Q3_TaskIDClear(&ent->taskID[TID_ANIM_UPPER]); //Top is done, regardless
			if (!Q3_TaskIDPending(ent, TID_ANIM_LOWER))
			{
				//lower is done and we're done
				Q3_TaskIDComplete(ent, TID_ANIM_BOTH);
			}
		}
	}
}

extern qboolean PM_SpinningSaberAnim(int anim);
extern qboolean PM_InSaberAnim(int anim);

void PM_SaberStartTransAnim(const int saber_anim_level, const int anim, float* animSpeed, const gentity_t* gent,
	const int fatigued)
{
	char buf[128];

	gi.Cvar_VariableStringBuffer("g_saberAnimSpeed", buf, sizeof buf);
	const float saberanimscale = atof(buf);

	if (gent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	//Base JKA speeds for all modes
	if (anim >= BOTH_A1_T__B_ && anim <= BOTH_ROLL_STAB)
	{
		if (g_saberAnimSpeed->value != 1.0f)
		{
			*animSpeed *= g_saberAnimSpeed->value;
		}
		else if (gent && gent->client && gent->client->ps.weapon == WP_SABER)
		{
			if (gent->client->ps.saber[0].animSpeedScale != 1.0f)
			{
				*animSpeed *= gent->client->ps.saber[0].animSpeedScale;
			}
			if (gent->client->ps.dualSabers && gent->client->ps.saber[1].animSpeedScale != 1.0f)
			{
				*animSpeed *= gent->client->ps.saber[1].animSpeedScale;
			}
		}
	}

	if (gent
		&& gent->client
		&& gent->client->ps.weapons[WP_SCEPTER]
		&& gent->client->ps.dualSabers
		&& saber_anim_level == SS_DUAL
		&& gent->weaponModel[1])
	{
		if (anim >= BOTH_A1_T__B_ && anim <= BOTH_H7_S7_BR)
		{
			*animSpeed *= 0.75;
		}
	}

	if (gent && gent->client && gent->client->ps.forceRageRecoveryTime > level.time)
	{
		//rage recovery
		if (anim >= BOTH_A1_T__B_ && anim <= BOTH_H1_S1_BR)
		{
			//animate slower
			*animSpeed *= 0.75;
		}
	}
	else if (gent && gent->NPC && gent->NPC->rank == RANK_CIVILIAN)
	{
		//grunt reborn
		if (anim >= BOTH_A1_T__B_ && anim <= BOTH_R1_TR_S1)
		{
			//his fast attacks are slower
			if (!PM_SpinningSaberAnim(anim))
			{
				*animSpeed *= 0.75;
			}
			return;
		}
	}
	else if (gent && gent->client)
	{
		if (gent->client->ps.saber[0].type == SABER_LANCE || gent->client->ps.saber[0].type == SABER_TRIDENT)
		{
			if (anim >= BOTH_A1_T__B_ && anim <= BOTH_R1_TR_S1)
			{
				if (!PM_SpinningSaberAnim(anim))
				{
					*animSpeed *= 0.75;
				}
				return;
			}
		}
	}

	//AMD and MD Modes speeds
	if (g_SaberAttackSpeedMD->integer)
	{
		if (anim >= BOTH_A1_T__B_ && anim <= BOTH_H7_S7_BR ||
			anim >= BOTH_T1_BR__R && anim <= BOTH_T1_BL_TL ||
			anim >= BOTH_T2_BR__R && anim <= BOTH_T2_BL_TL ||
			anim >= BOTH_T3_BR__R && anim <= BOTH_T3_BL_TL ||
			anim >= BOTH_T4_BR__R && anim <= BOTH_T4_BL_TL ||
			anim >= BOTH_T5_BR__R && anim <= BOTH_T5_BL_TL ||
			anim >= BOTH_T6_BR__R && anim <= BOTH_T6_BL_TL ||
			anim >= BOTH_T7_BR__R && anim <= BOTH_T7_BL_TL ||
			anim >= BOTH_S1_S1_T_ && anim <= BOTH_S1_S1_TR ||
			anim >= BOTH_S2_S1_T_ && anim <= BOTH_S2_S1_TR ||
			anim >= BOTH_S3_S1_T_ && anim <= BOTH_S3_S1_TR ||
			anim >= BOTH_S4_S1_T_ && anim <= BOTH_S4_S1_TR ||
			anim >= BOTH_S5_S1_T_ && anim <= BOTH_S5_S1_TR)
		{
			if (g_SerenityJediEngineMode->integer && gent && gent->client && gent->client->ps.saberFatigueChainCount >= MISHAPLEVEL_HUDFLASH)
			{
				if (anim != (BOTH_FORCEWALLRELEASE_FORWARD | BOTH_FORCEWALLRUNFLIP_START | BOTH_FORCEWALLRUNFLIP_END | BOTH_JUMPFLIPSTABDOWN | BOTH_JUMPFLIPSLASHDOWN1 | BOTH_LUNGE2_B__T_))
				{
					constexpr float fatiguedanimscale = 0.75f;
					*animSpeed *= fatiguedanimscale;
				}
			}
			else if (g_SerenityJediEngineMode->integer && fatigued & 1 << FLAG_SLOWBOUNCE)
			{
				//slow animation for slow bounces
				if (PM_BounceAnim(anim))
				{
					*animSpeed *= 0.6f;
				}
				else if (PM_SaberReturnAnim(anim))
				{
					*animSpeed *= 0.8f;
				}
			}
			else if (g_SerenityJediEngineMode->integer && fatigued & 1 << FLAG_OLDSLOWBOUNCE)
			{
				//getting parried slows down your reaction
				if (PM_BounceAnim(anim) || PM_SaberReturnAnim(anim))
				{
					//only apply to bounce and returns since this flag is technically turned off immediately after the animation is set.
					*animSpeed *= 0.6f;
				}
			}
			else if (g_SerenityJediEngineMode->integer && fatigued & 1 << FLAG_PARRIED)
			{
				//getting parried slows down your reaction
				if (PM_BounceAnim(anim) || PM_SaberReturnAnim(anim))
				{
					*animSpeed *= 0.90f;
				}
			}
			else if (g_SerenityJediEngineMode->integer && fatigued & 1 << FLAG_BLOCKED)
			{
				if (PM_BounceAnim(anim) || PM_SaberReturnAnim(anim))
				{
					*animSpeed *= 0.85f;
				}
			}
			else if (g_SerenityJediEngineMode->integer && fatigued & 1 << FLAG_MBLOCKBOUNCE)
			{
				//slow animation for all bounces
				if (PM_SaberInMassiveBounce(anim))
				{
					*animSpeed *= 0.5f;
				}
			}
			else if (gent
				&& gent->client
				&& gent->client->ps.forceUpperAnimSpeed)
			{
				*animSpeed *= gent->client->ps.forceUpperAnimSpeed;
			}
			else if (gent
				&& gent->client
				&& gent->client->NPC_class == CLASS_YODA)
			{
				constexpr float yoda_animscale = 1.25f;
				*animSpeed *= yoda_animscale;
			}
			else
			{
				if (saber_anim_level == SS_DUAL)
				{
					//slow down broken parries
					if (anim >= BOTH_H6_S6_T_ && anim <= BOTH_H6_S6_BR)
					{
						//dual broken parries are 1/3 the frames of the single broken parries
						*animSpeed *= 0.6f;
					}
					else
					{
						constexpr float dualanimscale = 0.90f;
						*animSpeed *= dualanimscale;
					}
				}
				else if (saber_anim_level == SS_STAFF)
				{
					if (anim >= BOTH_H7_S7_T_ && anim <= BOTH_H7_S7_BR)
					{
						//doubles are 1/2 the frames of single broken parries
						*animSpeed *= 0.8f;
					}
					else
					{
						constexpr float staffanimscale = 0.90f;
						*animSpeed *= staffanimscale;
					}
				}
				else if (saber_anim_level == SS_FAST)
				{
					constexpr float blueanimscale = 1.0f;
					*animSpeed *= blueanimscale;
				}
				else if (saber_anim_level == SS_MEDIUM)
				{
					if (gent
						&& gent->client
						&& gent->s.number < MAX_CLIENTS)
					{
						if (g_RealisticBlockingMode->integer)
						{
							if (gent->client->ps.saberFatigueChainCount >= MISHAPLEVEL_LIGHT)
							{//Slow down saber moves...
								constexpr float fatiguedanimscale = 0.96f;
								*animSpeed *= fatiguedanimscale;
							}
							else
							{
								constexpr float realisticanimscale = 0.98f;
								*animSpeed *= realisticanimscale;
							}
						}
						else
						{
							constexpr float mediumanimscale = 1.0f;
							*animSpeed *= mediumanimscale;
						}
					}
					else
					{
						constexpr float npcanimscale = 0.90f;
						*animSpeed *= npcanimscale;
					}
				}
				else if (saber_anim_level == SS_STRONG)
				{
					constexpr float redanimscale = 1.0f;
					*animSpeed *= redanimscale;
				}
				else if (saber_anim_level == SS_DESANN)
				{
					constexpr float heavyanimscale = 1.0f;
					*animSpeed *= heavyanimscale;
				}
				else if (saber_anim_level == SS_TAVION)
				{
					constexpr float tavionanimscale = 0.9f;
					*animSpeed *= tavionanimscale;
				}
				else
				{
					*animSpeed *= saberanimscale;
				}
			}
		}
	}
	else
	{
		if (anim >= BOTH_A1_T__B_ && anim <= BOTH_H7_S7_BR)
		{
			if (gent
				&& gent->client
				&& gent->client->ps.forceUpperAnimSpeed)
			{
				*animSpeed *= gent->client->ps.forceUpperAnimSpeed;
			}
			else
			{
				*animSpeed *= saberanimscale;
			}
		}

		if (anim >= BOTH_T1_BR__R &&
			anim <= BOTH_T1_BL_TL ||
			anim >= BOTH_T3_BR__R &&
			anim <= BOTH_T3_BL_TL ||
			anim >= BOTH_T5_BR__R &&
			anim <= BOTH_T5_BL_TL)
		{
			if (saber_anim_level == FORCE_LEVEL_1 || saber_anim_level == FORCE_LEVEL_5)
			{
				*animSpeed *= 1.5;
			}
			else if (saber_anim_level == FORCE_LEVEL_3)
			{
				*animSpeed *= 0.75;
			}
		}
	}
}

extern qboolean player_locked;
extern qboolean PlayerAffectedByStasis();
extern qboolean MatrixMode;

float pm_get_time_scale_mod(const gentity_t* gent)
{
	if (g_timescale->value)
	{
		if (!MatrixMode
			&& gent->client->ps.legsAnim != BOTH_FORCELONGLEAP_START
			&& gent->client->ps.legsAnim != BOTH_FORCELONGLEAP_ATTACK
			&& gent->client->ps.legsAnim != BOTH_FORCELONGLEAP_LAND)
		{
			if (gent && gent->s.client_num == 0 && !player_locked && !PlayerAffectedByStasis() && gent->client->ps.
				forcePowersActive & 1 << FP_SPEED)
			{
				return 1.0 / g_timescale->value;
			}
			if (gent && gent->client && gent->client->ps.forcePowersActive & 1 << FP_SPEED)
			{
				return 1.0 / g_timescale->value;
			}
		}
	}
	return 1.0f;
}

/*
-------------------------
PM_SetAnimFinal
-------------------------
*/
void PM_SetAnimFinal(int* torso_anim, int* legs_anim,
	const int set_anim_parts, int anim, const int set_anim_flags,
	int* torso_anim_timer, int* legs_anim_timer,
	gentity_t* gent, int blend_time) // default blend_time=350
{
	// BASIC SETUP AND SAFETY CHECKING
	//=================================

	// If It Is A Busted Entity, Don't Do Anything Here.
	//---------------------------------------------------
	if (!gent || !gent->client)
	{
		return;
	}

	// Make Sure This Character Has Such An Anim And A Model
	//-------------------------------------------------------
	if (anim < 0 || anim >= MAX_ANIMATIONS || !ValidAnimFileIndex(gent->client->clientInfo.animFileIndex))
	{
#ifndef FINAL_BUILD
		if (g_AnimWarning->integer)
		{
			if (anim < 0 || anim >= MAX_ANIMATIONS)
			{
				gi.Printf(S_COLOR_RED"PM_SetAnimFinal: Invalid Anim Index (%d)!\n", anim);
			}
			else
			{
				gi.Printf(S_COLOR_RED"PM_SetAnimFinal: Invalid Anim File Index (%d)!\n", gent->client->clientInfo.animFileIndex);
			}
		}
#endif
		return;
	}

	// Get Global Time Properties
	//----------------------------
	float time_scale_mod = pm_get_time_scale_mod(gent);
	const int actual_time = cg.time ? cg.time : level.time;
	const animation_t* animations = level.knownAnimFileSets[gent->client->clientInfo.animFileIndex].animations;
	const animation_t& cur_anim = animations[anim];

	// Make Sure This Character Has Such An Anim And A Model
	//-------------------------------------------------------
	if (animations[anim].numFrames == 0)
	{
#ifndef FINAL_BUILD
		static int	LastAnimWarningNum = 0;
		if (LastAnimWarningNum != anim)
		{
			if ((cg_debugAnim.integer == 3) ||												// 3 = do everyone
				(cg_debugAnim.integer == 1 && gent->s.number == 0) ||							// 1 = only the player
				(cg_debugAnim.integer == 2 && gent->s.number != 0) ||							// 2 = only everyone else
				(cg_debugAnim.integer == 4 && gent->s.number != cg_debugAnimTarget.integer))	// 4 = specific entnum
			{
				gi.Printf(S_COLOR_RED"PM_SetAnimFinal: Anim %s does not exist in this model (%s)!\n", anim_table[anim].name, gent->NPC_type);
			}
		}
		LastAnimWarningNum = anim;
#endif
		return;
	}

	// If It's Not A Ghoul 2 Model, Just Remember The Anims And Stop, Because Everything Beyond This Is Ghoul2
	//---------------------------------------------------------------------------------------------------------
	if (!gi.G2API_HaveWeGhoul2Models(gent->ghoul2))
	{
		if (set_anim_parts & SETANIM_TORSO)
		{
			*torso_anim = anim;
		}
		if (set_anim_parts & SETANIM_LEGS)
		{
			*legs_anim = anim;
		}
		return;
	}

	// Lower Offensive Skill Slows Down The Saber Start Attack Animations
	//--------------------------------------------------------------------

	PM_SaberStartTransAnim(gent->client->ps.saber_anim_level, anim, &time_scale_mod, gent, gent->userInt3);

	// SETUP VALUES FOR INCOMMING ANIMATION
	//======================================
	const bool anim_foot_move = PM_WalkingAnim(anim) || PM_RunningAnim(anim) || anim == BOTH_CROUCH1WALK || anim ==
		BOTH_CROUCH1WALKBACK;
	const bool anim_holdless = (set_anim_flags & SETANIM_FLAG_HOLDLESS) != 0;
	const bool anim_hold = (set_anim_flags & SETANIM_FLAG_HOLD) != 0;
	const bool anim_restart = (set_anim_flags & SETANIM_FLAG_RESTART) != 0;
	const bool anim_override = (set_anim_flags & SETANIM_FLAG_OVERRIDE) != 0;
	const bool anim_sync = g_synchSplitAnims->integer != 0 && !anim_restart;
	float anim_current = -1.0f;
	float animSpeed = 50.0f / cur_anim.frameLerp * time_scale_mod;
	// animSpeed is 1.0 if the frameLerp (ms/frame) is 50 (20 fps).
	const float anim_fps = fabsf(cur_anim.frameLerp);
	const auto anim_dur_m_sec = static_cast<int>((cur_anim.numFrames - 1) * anim_fps / time_scale_mod);
	const int anim_hold_m_sec = anim_holdless && time_scale_mod == 1.0f
		? (anim_dur_m_sec > 1 ? anim_dur_m_sec - 1 : anim_fps)
		: anim_dur_m_sec;
	int anim_flags = cur_anim.loopFrames != -1 ? BONE_ANIM_OVERRIDE_LOOP : BONE_ANIM_OVERRIDE_FREEZE;
	int anim_start = cur_anim.firstFrame;
	int anim_end = cur_anim.firstFrame + animations[anim].numFrames;

	// If We Have A Blend Timer, Add The Blend Flag
	//----------------------------------------------
	if (blend_time > 0)
	{
		anim_flags |= BONE_ANIM_BLEND;
	}

	// If Animation Is Going Backwards, Swap Last And First Frames
	//-------------------------------------------------------------
	if (animSpeed < 0.0f)
	{
#ifndef FINAL_BUILD
		if (g_AnimWarning->integer == 1)
		{
			if (animFlags & BONE_ANIM_OVERRIDE_LOOP)
			{
				gi.Printf(S_COLOR_YELLOW"PM_SetAnimFinal: WARNING: Anim (%s) looping backwards!\n", anim_table[anim].name);
			}
		}
#endif

		const int temp = anim_end;
		anim_end = anim_start;
		anim_start = temp;
		blend_time = 0;
	}

	// If The Animation Is Walking Or Running, Attempt To Scale The Playback Speed To Match
	//--------------------------------------------------------------------------------------
	if (g_noFootSlide->integer
		&& anim_foot_move
		&& animSpeed >= 0.0f
		&& gent->client->NPC_class != CLASS_HOWLER
		&& gent->client->NPC_class != CLASS_WAMPA
		&& gent->client->NPC_class != CLASS_GONK
		&& gent->client->NPC_class != CLASS_HOWLER
		&& gent->client->NPC_class != CLASS_MOUSE
		&& gent->client->NPC_class != CLASS_PROBE
		&& gent->client->NPC_class != CLASS_PROTOCOL
		&& gent->client->NPC_class != CLASS_R2D2
		&& gent->client->NPC_class != CLASS_R5D2
		&& gent->client->NPC_class != CLASS_SEEKER)
	{
		const bool walking = !!PM_WalkingAnim(anim);
		const bool has_dual = gent->client->ps.saber_anim_level == SS_DUAL;
		const bool has_staff = gent->client->ps.saber_anim_level == SS_STAFF;
		float move_speed_of_anim; //g_noFootSlideRunScale->value;

		if (anim == BOTH_CROUCH1WALK || anim == BOTH_CROUCH1WALKBACK)
		{
			move_speed_of_anim = 75.0f;
		}
		else
		{
			if (gent->client->NPC_class == CLASS_HAZARD_TROOPER)
			{
				move_speed_of_anim = 50.0f;
			}
			else if (gent->client->NPC_class == CLASS_RANCOR)
			{
				move_speed_of_anim = 173.0f;
			}
			else
			{
				if (walking)
				{
					if (has_dual || has_staff)
					{
						move_speed_of_anim = 100.0f;
					}
					else
					{
						move_speed_of_anim = 50.0f; // g_noFootSlideWalkScale->value;
					}
				}
				else
				{
					if (has_staff)
					{
						move_speed_of_anim = 250.0f;
					}
					else
					{
						move_speed_of_anim = 150.0f;
					}
				}
			}
		}

		animSpeed *= gent->resultspeed / move_speed_of_anim;
		if (animSpeed < 0.01f)
		{
			animSpeed = 0.01f;
		}

		// Make Sure Not To Play Too Fast An Anim
		//----------------------------------------
		const float max_playback_speed = 1.5f * time_scale_mod;
		if (animSpeed > max_playback_speed)
		{
			animSpeed = max_playback_speed;
		}
	}

	// GET VALUES FOR EXISTING BODY ANIMATION
	//==========================================
	float body_speed = 0.0f;
	float body_current = 0.0f;
	int body_start = 0;
	int body_end = 0;
	int body_flags = 0;
	const int body_anim = *legs_anim;
	const int body_bone = gent->rootBone;
	const bool body_timer_on = *legs_anim_timer > 0 || *legs_anim_timer == -1;
	bool body_play = set_anim_parts & SETANIM_LEGS && body_bone != -1 && (anim_override || !body_timer_on);
	const bool body_animating = !!gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], body_bone, actual_time,
		&body_current, &body_start, &body_end, &body_flags,
		&body_speed,
		nullptr);
	const bool body_on_anim_now = body_animating && body_anim == anim && body_start == anim_start && body_end ==
		anim_end;
	bool body_match_tors_frame = false;

	// GET VALUES FOR EXISTING TORSO ANIMATION
	//===========================================
	float tors_speed = 0.0f;
	float tors_current = 0.0f;
	int tors_start = 0;
	int tors_end = 0;
	int tors_flags = 0;
	const int tors_anim = *torso_anim;
	const int tors_bone = gent->lowerLumbarBone;
	const bool tors_timer_on = *torso_anim_timer > 0 || *torso_anim_timer == -1;
	bool tors_play = gent->client->NPC_class != CLASS_RANCOR && set_anim_parts & SETANIM_TORSO && tors_bone != -1 && (
		anim_override || !tors_timer_on);
	const bool tors_animating = !!gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], tors_bone, actual_time,
		&tors_current, &tors_start, &tors_end, &tors_flags,
		&tors_speed,
		nullptr);
	const bool tors_on_anim_now = tors_animating && tors_anim == anim && tors_start == anim_start && tors_end ==
		anim_end;
	bool tors_match_body_frame = false;

	// APPLY SYNC TO TORSO
	//=====================
	if (anim_sync && tors_play && !body_play && body_on_anim_now && (!tors_on_anim_now || tors_current != body_current))
	{
		tors_match_body_frame = true;
		anim_current = body_current;
	}
	if (anim_sync && body_play && !tors_play && tors_on_anim_now && (!body_on_anim_now || body_current != tors_current))
	{
		body_match_tors_frame = true;
		anim_current = tors_current;
	}

	// If Already Doing These Exact Parameters, Then Don't Play
	//----------------------------------------------------------
	if (!anim_restart)
	{
		tors_play &= !(tors_on_anim_now && tors_speed == animSpeed && !tors_match_body_frame);
		body_play &= !(body_on_anim_now && body_speed == animSpeed && !body_match_tors_frame);
	}

#ifndef FINAL_BUILD
	if ((cg_debugAnim.integer == 3) ||												// 3 = do everyone
		(cg_debugAnim.integer == 1 && gent->s.number == 0) ||							// 1 = only the player
		(cg_debugAnim.integer == 2 && gent->s.number != 0) ||							// 2 = only everyone else
		(cg_debugAnim.integer == 4 && gent->s.number != cg_debugAnimTarget.integer) 	// 4 = specific entnum
		)
	{
		if (bodyPlay || torsPlay)
		{
			char* entName = gent->targetname;
			char* location;

			// Select Entity Name
			//--------------------
			if (!entName || !entName[0])
			{
				entName = gent->NPC_targetname;
			}
			if (!entName || !entName[0])
			{
				entName = gent->NPC_type;
			}
			if (!entName || !entName[0])
			{
				entName = gent->classname;
			}
			if (!entName || !entName[0])
			{
				entName = "UNKNOWN";
			}

			// Select Play Location
			//----------------------
			if (bodyPlay && torsPlay)
			{
				location = "BOTH ";
			}
			else if (bodyPlay)
			{
				location = "LEGS ";
			}
			else
			{
				location = "TORSO";
			}

			// Print It!
			//-----------
			Com_Printf("[%10d] ent[%3d-%18s] %s anim[%3d] - %s\n",
				actualTime,
				gent->s.number,
				entName,
				location,
				anim,
				anim_table[anim].name);
		}
	}
#endif

	// PLAY ON THE TORSO
	//========================
	if (tors_play)
	{
		*torso_anim = anim;
		const float old_anim_current = anim_current;
		if (anim_current != body_current && tors_on_anim_now && !anim_restart && !tors_match_body_frame)
		{
			anim_current = tors_current;
		}

		if (gent->client->ps.forceUpperAnimSpeed)
		{
			animSpeed *= gent->client->ps.forceUpperAnimSpeed;
		}

		gi.G2API_SetAnimIndex(&gent->ghoul2[gent->playerModel], cur_anim.glaIndex);
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], tors_bone,
			anim_start,
			anim_end,
			tors_on_anim_now && !anim_restart ? anim_flags & ~BONE_ANIM_BLEND : anim_flags,
			animSpeed,
			actual_time,
			anim_current,
			blend_time);

		if (gent->motionBone != -1)
		{
			gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone,
				anim_start,
				anim_end,
				tors_on_anim_now && !anim_restart ? anim_flags & ~BONE_ANIM_BLEND : anim_flags,
				animSpeed,
				actual_time,
				anim_current,
				blend_time);
		}

		anim_current = old_anim_current;

		// If This Animation Is To Be Locked And Held, Calculate The Duration And Set The Timer
		//--------------------------------------------------------------------------------------
		if (anim_hold || anim_holdless)
		{
			PM_SetTorsoAnimTimer(gent, torso_anim_timer, anim_hold_m_sec);
		}
	}

	// PLAY ON THE WHOLE BODY
	//========================
	if (body_play)
	{
		*legs_anim = anim;

		if (body_on_anim_now && !anim_restart && !body_match_tors_frame)
		{
			anim_current = body_current;
		}

		if (gent->client->ps.forceLowerAnimSpeed)
		{
			animSpeed *= gent->client->ps.forceLowerAnimSpeed;
		}

		gi.G2API_SetAnimIndex(&gent->ghoul2[gent->playerModel], cur_anim.glaIndex);
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], body_bone,
			anim_start,
			anim_end,
			body_on_anim_now && !anim_restart ? anim_flags & ~BONE_ANIM_BLEND : anim_flags,
			animSpeed,
			actual_time,
			anim_current,
			blend_time);

		// If This Animation Is To Be Locked And Held, Calculate The Duration And Set The Timer
		//--------------------------------------------------------------------------------------
		if (anim_hold || anim_holdless)
		{
			PM_SetLegsAnimTimer(gent, legs_anim_timer, anim_hold_m_sec);
		}
	}

	// PRINT SOME DEBUG TEXT OF EXISTING VALUES
	//==========================================
}

void PM_SetAnim(const pmove_t* pm, int set_anim_parts, const int anim, const int set_anim_flags, const int blend_time)
{
	// FIXME : once torsoAnim and legsAnim are in the same structure for NPC and Players
	// rename PM_SetAnimFinal to PM_SetAnim and have both NPC and Players call PM_SetAnim

	if (pm->ps->pm_type >= PM_DEAD)
	{
		//FIXME: sometimes we'll want to set anims when your dead... twitches, impacts, etc.
		return;
	}

	if (pm->gent == nullptr)
	{
		return;
	}

	if (pm->ps->stasisTime > level.time)
	{
		return;
	}

	if (pm->ps->stasisJediTime > level.time)
	{
		return;
	}

	if (!pm->gent || pm->gent->health > 0)
	{
		//don't lock anims if the guy is dead
		if (pm->ps->torsoAnimTimer
			&& PM_LockedAnim(pm->ps->torsoAnim)
			&& !PM_LockedAnim(anim))
		{
			//nothing can override these special anims
			set_anim_parts &= ~SETANIM_TORSO;
		}

		if (pm->ps->legsAnimTimer
			&& PM_LockedAnim(pm->ps->legsAnim)
			&& !PM_LockedAnim(anim))
		{
			//nothing can override these special anims
			set_anim_parts &= ~SETANIM_LEGS;
		}
	}

	if (!set_anim_parts)
	{
		return;
	}

	if (set_anim_flags & SETANIM_FLAG_OVERRIDE)
	{
		if (set_anim_parts & SETANIM_TORSO)
		{
			if (set_anim_flags & SETANIM_FLAG_RESTART || pm->ps->torsoAnim != anim)
			{
				PM_SetTorsoAnimTimer(pm->gent, &pm->ps->torsoAnimTimer, 0);
			}
		}
		if (set_anim_parts & SETANIM_LEGS)
		{
			if (set_anim_flags & SETANIM_FLAG_RESTART || pm->ps->legsAnim != anim)
			{
				PM_SetLegsAnimTimer(pm->gent, &pm->ps->legsAnimTimer, 0);
			}
		}
	}

	PM_SetAnimFinal(&pm->ps->torsoAnim, &pm->ps->legsAnim, set_anim_parts, anim, set_anim_flags,
		&pm->ps->torsoAnimTimer,
		&pm->ps->legsAnimTimer, &g_entities[pm->ps->client_num], blend_time); //was pm->gent
}

static bool TorsoAgainstWindTest(gentity_t* ent)
{
	if (ent && //valid ent
		ent->client && //a client
		ent->client->ps.groundEntityNum != ENTITYNUM_NONE &&
		(ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent)) &&
		gi.WE_GetWindGusting(ent->currentOrigin) &&
		gi.WE_IsOutside(ent->currentOrigin))
	{
		if (Q_stricmp(level.mapname, "t2_wedge") != 0)
		{
			vec3_t wind_dir;
			if (gi.WE_GetWindVector(wind_dir, ent->currentOrigin))
			{
				vec3_t fwd;
				VectorScale(wind_dir, -1.0f, wind_dir);
				AngleVectors(pm->gent->currentAngles, fwd, nullptr, nullptr);
				if (DotProduct(fwd, wind_dir) > 0.65f)
				{
					if (ent->client->ps.PlayerEffectFlags & 1 << PEF_SPRINTING ||
						ent->client->ps.PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING ||
						ent->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK)
					{
						// Dont do this if sprinting it looks shit
					}
					else
					{
						if (ent->client->ps.weapon != WP_SABER)
						{
							// any other weapon but saber
							if (ent->client && ent->client->ps.torsoAnim != BOTH_WIND)
							{
								NPC_SetAnim(ent, SETANIM_TORSO, BOTH_WIND, SETANIM_FLAG_NORMAL, 400);
							}
						}
						else
						{
							// ok i have a saber
							if (g_SerenityJediEngineMode->integer)
							{
								if (!(ent->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK) &&
									!(ent->client->ps.PlayerEffectFlags & 1 << PEF_SPRINTING) &&
									!(ent->client->ps.PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
								{
									if (ent->client && ent->client->ps.torsoAnim != BOTH_WIND)
									{
										NPC_SetAnim(ent, SETANIM_TORSO, BOTH_WIND, SETANIM_FLAG_NORMAL, 400);
									}
								}
							}
							else
							{
								if (!(ent->client->ps.PlayerEffectFlags & 1 << PEF_SPRINTING) &&
									!(ent->client->ps.PlayerEffectFlags & 1 << PEF_WEAPONSPRINTING))
								{
									if (ent->client && ent->client->ps.torsoAnim != BOTH_WIND)
									{
										NPC_SetAnim(ent, SETANIM_TORSO, BOTH_WIND, SETANIM_FLAG_NORMAL, 400);
									}
								}
							}
						}
					}

					return true;
				}
			}
		}
	}
	return false;
}

qboolean BG_SprintAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_SPRINT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_SaberSprintAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_SPRINT_SINGLE_LIGHTSABER:
	case BOTH_SPRINT_STAFF_LIGHTSABER:
	case BOTH_SPRINT_DUAL_LIGHTSABER:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_WeaponSprintAnim(const int anim)
{
	switch (anim)
	{
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

/*
-------------------------
PM_TorsoAnimLightsaber
-------------------------
*/

// Note that this function is intended to set the animation for the player, but
// only does idle-ish anims.  Anything that has a timer associated, such as attacks and blocks,
// are set by PM_WeaponLightsaber()

extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern qboolean PM_LandingAnim(int anim);
qboolean PM_InCartwheel(int anim);

static void PM_TorsoAnimLightsaber()
{
	// *********************************************************
	// WEAPON_READY
	// *********************************************************

	const qboolean is_holding_block_button = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;
	//Holding Block Button
	const qboolean active_blocking = pm->ps->ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK ? qtrue : qfalse;
	//Active Blocking
	const qboolean walking_blocking = pm->ps->ManualBlockingFlags & 1 << MBF_BLOCKWALKING ? qtrue : qfalse;
	//Walking Blocking

	if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
	{
		//holding an enemy aloft with force-grip
		return;
	}

	if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_1)
	{
		//lightning
		return;
	}

	if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] > FORCE_LEVEL_1)
	{
		//holding an enemy aloft with force-grasp
		return;
	}

	if (pm->ps->forcePowersActive & 1 << FP_DRAIN)
	{
		//drain
		return;
	}

	if (pm->ps->saber[0].blade[0].active
		&& pm->ps->saber[0].blade[0].length < 3
		&& !(pm->ps->saberEventFlags & SEF_HITWALL)
		&& pm->ps->weaponstate == WEAPON_RAISING)
	{
		if (!G_IsRidingVehicle(pm->gent))
		{
			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{
				PM_SetSaberMove(LS_DRAW);
			}
			else
			{
				if (g_SerenityJediEngineMode->integer)
				{
					if (g_SerenityJediEngineMode->integer == 2)
					{
						//AMD Mode
						if (!g_noIgniteTwirl->integer && !active_blocking
							&& !is_holding_block_button && !IsSurrendering(pm->gent)) //twirl on
						{
							PM_SetSaberMove(LS_DRAW);
						}
						else
						{
							if ((pm->ps->legsAnim == BOTH_WALK_STAFF
								|| pm->ps->legsAnim == BOTH_WALK_DUAL
								|| pm->ps->legsAnim == BOTH_WALKBACK_STAFF
								|| pm->ps->legsAnim == BOTH_WALKBACK_DUAL
								|| pm->ps->legsAnim == BOTH_WALK_STAFF_AMD
								|| pm->ps->legsAnim == BOTH_WALK_DUAL_AMD
								|| pm->ps->legsAnim == BOTH_WALK1
								|| pm->ps->legsAnim == BOTH_WALK1_MDA
								|| pm->ps->legsAnim == BOTH_WALKBACK1
								|| pm->ps->legsAnim == BOTH_WALK2
								|| pm->ps->legsAnim == BOTH_WALKBACK2)
								&& pm->ps->saberBlockingTime < cg.time
								&& !active_blocking
								&& !is_holding_block_button
								&& !walking_blocking
								&& !IsSurrendering(pm->gent))
							{
								//running w/1-handed weapon uses full-body anim
								int set_flags = SETANIM_FLAG_NORMAL;
								if (PM_LandingAnim(pm->ps->torsoAnim))
								{
									set_flags = SETANIM_FLAG_OVERRIDE;
								}
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
							}
							else if (PM_RunningAnim(pm->ps->legsAnim) && pm->ps->saberBlockingTime < cg.time && !
								IsSurrendering(pm->gent))
							{
								//running w/1-handed weapon uses full-body anim
								int set_flags = SETANIM_FLAG_NORMAL;
								if (PM_LandingAnim(pm->ps->torsoAnim))
								{
									set_flags = SETANIM_FLAG_OVERRIDE;
								}
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
							}
							else
							{
								if (!IsSurrendering(pm->gent))
								{
									PM_SetSaberMove(LS_READY);
									/*if (pm->ps->saber_anim_level == SS_STAFF)
									{
										PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABER_IDLE_STANCE_STAFF,
											SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
									}
									else if (pm->ps->saber_anim_level == SS_DUAL)
									{
										PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABER_IDLE_STANCE_DUAL,
											SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
									}
									else
									{
										PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABER_IDLE_STANCE,
											SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
									}*/
								}
							}
						}
					}
					else
					{
						//MD Mode
						if (!g_noIgniteTwirl->integer && !active_blocking
							&& !is_holding_block_button
							&& !IsSurrendering(pm->gent)) //twirl on
						{
							PM_SetSaberMove(LS_DRAW);
						}
						else
						{
							if ((pm->ps->legsAnim == BOTH_WALK_STAFF
								|| pm->ps->legsAnim == BOTH_WALK_DUAL
								|| pm->ps->legsAnim == BOTH_WALKBACK_STAFF
								|| pm->ps->legsAnim == BOTH_WALKBACK_DUAL
								|| pm->ps->legsAnim == BOTH_WALK_STAFF_AMD
								|| pm->ps->legsAnim == BOTH_WALK_DUAL_AMD
								|| pm->ps->legsAnim == BOTH_WALK1
								|| pm->ps->legsAnim == BOTH_WALK1_MDA
								|| pm->ps->legsAnim == BOTH_WALKBACK1
								|| pm->ps->legsAnim == BOTH_WALK2
								|| pm->ps->legsAnim == BOTH_WALKBACK2)
								&& pm->ps->saberBlockingTime < cg.time
								&& !active_blocking
								&& !is_holding_block_button
								&& !walking_blocking
								&& !IsSurrendering(pm->gent))
							{
								//running w/1-handed weapon uses full-body anim
								int set_flags = SETANIM_FLAG_NORMAL;
								if (PM_LandingAnim(pm->ps->torsoAnim))
								{
									set_flags = SETANIM_FLAG_OVERRIDE;
								}
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
							}
							else if (PM_RunningAnim(pm->ps->legsAnim) && pm->ps->saberBlockingTime < cg.time && !
								IsSurrendering(pm->gent))
							{
								//running w/1-handed weapon uses full-body anim
								int set_flags = SETANIM_FLAG_NORMAL;
								if (PM_LandingAnim(pm->ps->torsoAnim))
								{
									set_flags = SETANIM_FLAG_OVERRIDE;
								}
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
							}
							else
							{
								if (!IsSurrendering(pm->gent))
								{
									PM_SetSaberMove(LS_READY);
								}
							}
						}
					}
				}
				else
				{
					//JKA Mode
					if (!g_noIgniteTwirl->integer && !active_blocking
						&& !is_holding_block_button
						&& !IsSurrendering(pm->gent)) //twirl on
					{
						PM_SetSaberMove(LS_DRAW);
					}
					else
					{
						if ((pm->ps->legsAnim == BOTH_WALK_STAFF
							|| pm->ps->legsAnim == BOTH_WALK_DUAL
							|| pm->ps->legsAnim == BOTH_WALKBACK_STAFF
							|| pm->ps->legsAnim == BOTH_WALKBACK_DUAL
							|| pm->ps->legsAnim == BOTH_WALK_STAFF_AMD
							|| pm->ps->legsAnim == BOTH_WALK_DUAL_AMD
							|| pm->ps->legsAnim == BOTH_WALK1
							|| pm->ps->legsAnim == BOTH_WALK1_MDA
							|| pm->ps->legsAnim == BOTH_WALKBACK1
							|| pm->ps->legsAnim == BOTH_WALK2
							|| pm->ps->legsAnim == BOTH_WALKBACK2)
							&& pm->ps->saberBlockingTime < cg.time
							&& !active_blocking
							&& !is_holding_block_button
							&& !walking_blocking
							&& !IsSurrendering(pm->gent))
						{
							//running w/1-handed weapon uses full-body anim
							int set_flags = SETANIM_FLAG_NORMAL;
							if (PM_LandingAnim(pm->ps->torsoAnim))
							{
								set_flags = SETANIM_FLAG_OVERRIDE;
							}
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim) && pm->ps->saberBlockingTime < cg.time && !
							IsSurrendering(pm->gent))
						{
							//running w/1-handed weapon uses full-body anim
							int set_flags = SETANIM_FLAG_NORMAL;
							if (PM_LandingAnim(pm->ps->torsoAnim))
							{
								set_flags = SETANIM_FLAG_OVERRIDE;
							}
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
						}
						else
						{
							if (!IsSurrendering(pm->gent))
							{
								PM_SetSaberMove(LS_READY);
							}
						}
					}
				}
			}
		}
		return;
	}
	if (!pm->ps->SaberActive() && pm->ps->SaberLength())
	{
		if (!G_IsRidingVehicle(pm->gent))
		{
			if (pm->ps->client_num >= MAX_CLIENTS && !PM_ControlledByPlayer())
			{
				PM_SetSaberMove(LS_PUTAWAY);
			}
			else
			{
				if (!g_noIgniteTwirl->integer && !active_blocking
					&& !is_holding_block_button
					&& !IsSurrendering(pm->gent)) //twirl on
				{
					PM_SetSaberMove(LS_PUTAWAY);
				}
				else
				{
					if (!IsSurrendering(pm->gent))
					{
						if (g_SerenityJediEngineMode->integer)
						{
							if (g_SerenityJediEngineMode->integer == 2)
							{
								//AMD Mode
								if (!IsSurrendering(pm->gent))
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1IDLE1,
										SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
								}
							}
							else
							{
								//MD Mode
								//
							}
						}
						else
						{
							//JKA Mode
							//
						}
					}
				}
			}
		}
		return;
	}

	if (pm->ps->weaponTime > 0)
	{
		// weapon is already busy.
		if (pm->ps->torsoAnim == BOTH_TOSS1
			|| pm->ps->torsoAnim == BOTH_TOSS2)
		{
			//in toss
			if (!pm->ps->torsoAnimTimer)
			{
				//weird, get out of it, I guess
				PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
			}
		}
		return;
	}

	if (pm->ps->weaponstate == WEAPON_READY ||
		pm->ps->weaponstate == WEAPON_CHARGING ||
		pm->ps->weaponstate == WEAPON_CHARGING_ALT)
	{
		//ready
		if (pm->ps->weapon == WP_SABER && pm->ps->SaberLength() && (pm->ps->SaberActive() || !g_noIgniteTwirl->integer))
		{
			//saber is on
			// Select the proper idle Lightsaber attack move from the chart.
			if (pm->ps->saber_move > LS_READY && pm->ps->saber_move < LS_MOVE_MAX)
			{
				PM_SetSaberMove(saber_moveData[pm->ps->saber_move].chain_idle);
			}
			else
			{
				if (PM_JumpingAnim(pm->ps->legsAnim)
					|| PM_LandingAnim(pm->ps->legsAnim)
					|| PM_InCartwheel(pm->ps->legsAnim)
					|| PM_FlippingAnim(pm->ps->legsAnim))
				{
					PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
				}
				else
				{
					if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim ==
						BOTH_BUTTON_HOLD)
					{
						//using something
						if (!pm->ps->useTime)
						{
							//stopped holding it, release
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_BUTTON_RELEASE,
								SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						} //else still holding, leave it as it is
					}
					else
					{
						//This controls saber movement anims //JaceSolaris
						if ((pm->ps->legsAnim == BOTH_WALK_STAFF
							|| pm->ps->legsAnim == BOTH_WALK_DUAL
							|| pm->ps->legsAnim == BOTH_WALKBACK_STAFF
							|| pm->ps->legsAnim == BOTH_WALKBACK_DUAL
							|| pm->ps->legsAnim == BOTH_WALK_STAFF_AMD
							|| pm->ps->legsAnim == BOTH_WALK_DUAL_AMD
							|| pm->ps->legsAnim == BOTH_WALK1
							|| pm->ps->legsAnim == BOTH_WALK1_MDA
							|| pm->ps->legsAnim == BOTH_WALKBACK1
							|| pm->ps->legsAnim == BOTH_WALK2
							|| pm->ps->legsAnim == BOTH_WALKBACK2)
							&& pm->ps->saberBlockingTime < cg.time
							&& !active_blocking
							&& !is_holding_block_button
							&& !walking_blocking
							&& !IsSurrendering(pm->gent))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim) && pm->ps->saberBlockingTime < cg.time && !
							IsSurrendering(pm->gent))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetSaberMove(LS_READY);
						}
					}
				}
			}
		}
		else if (TorsoAgainstWindTest(pm->gent))
		{
		}
		else if (pm->ps->legsAnim == BOTH_RUN1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN1, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_SINGLE_LIGHTSABER)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_SINGLE_LIGHTSABER, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_STAFF_LIGHTSABER)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_STAFF_LIGHTSABER, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_DUAL_LIGHTSABER)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_DUAL_LIGHTSABER, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_BAZOOKA)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_BAZOOKA, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_BLASTER)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_BLASTER, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_DOUBLE_PISTOL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_DOUBLE_PISTOL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_GRENADE)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_GRENADE, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_HEAVY)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_HEAVY, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_MINIGUN)
		{
			//PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_MINIGUN, SETANIM_FLAG_NORMAL);
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_HEAVY, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_PISTOL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_PISTOL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_RUN2)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN2, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_RUN_STAFF)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN_STAFF, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_RUN_DUAL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN_DUAL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_BAZOOKA)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_BAZOOKA, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_BLASTER)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_BLASTER, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_DOUBLE_PISTOL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_DOUBLE_PISTOL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_GRENADE)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_GRENADE, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_HEAVY)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_HEAVY, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_MINIGUN)
		{
			//PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_MINIGUN, SETANIM_FLAG_NORMAL);
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_HEAVY, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JOG_PISTOL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_PISTOL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK1, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_BAZOOKA)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_BAZOOKA, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_BLASTER)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_BLASTER, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_DOUBLE_PISTOL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_DOUBLE_PISTOL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_GRENADE)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_GRENADE, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_HEAVY)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_HEAVY, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_MINIGUN)
		{
			//PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_MINIGUN, SETANIM_FLAG_NORMAL);
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_HEAVY, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_PISTOL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_PISTOL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK1_STICK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK1_STICK, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_MENUIDLE1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_MENUIDLE1, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_DUELS)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_DUELS, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_STAFF)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_STAFF, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_IDLE, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_DUELS)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_IDLE_DUELS, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_STAFF)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_IDLE_STAFF, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SABERSINGLECROUCH)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERSINGLECROUCH, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_FORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_FORWARD, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_BACK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_BACK, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_RIGHT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_RIGHT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_LEFT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_FORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_FORWARD, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_BACK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_BACK, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_RIGHT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_RIGHT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_LEFT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_FORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_FORWARD, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_BACK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_BACK, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_RIGHT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_RIGHT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_LEFT, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SABERDUALCROUCH)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERDUALCROUCH, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_SABERSTAFFCROUCH)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERSTAFFCROUCH, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK2)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK2, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_STAFF)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_STAFF, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_STAFF_AMD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_STAFF_AMD, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_DUAL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_DUAL, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_WALK_DUAL_AMD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_DUAL_AMD, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_CROUCH1IDLE && pm->ps->client_num != 0)
		{
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_JUMP1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JUMP1, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else
		{
			if (pm->ps->saber_move > LS_READY && pm->ps->saber_move < LS_MOVE_MAX)
			{
				PM_SetSaberMove(saber_moveData[pm->ps->saber_move].chain_idle);
			}
			else
			{
				if (PM_JumpingAnim(pm->ps->legsAnim)
					|| PM_LandingAnim(pm->ps->legsAnim)
					|| PM_InCartwheel(pm->ps->legsAnim)
					|| PM_FlippingAnim(pm->ps->legsAnim))
				{
					PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
				}
				else
				{
					if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim ==
						BOTH_BUTTON_HOLD)
					{
						//using something
						if (!pm->ps->useTime)
						{
							//stopped holding it, release
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_BUTTON_RELEASE,
								SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						} //else still holding, leave it as it is
					}
					else
					{
						PM_SetSaberMove(LS_READY);
					}
				}
			}
		}
	}

	// *********************************************************
	// WEAPON_IDLE
	// *********************************************************

	else if (pm->ps->weaponstate == WEAPON_IDLE)
	{
		if (TorsoAgainstWindTest(pm->gent))
		{
		}
		else if (pm->ps->legsAnim == BOTH_GUARD_LOOKAROUND1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_GUARD_LOOKAROUND1, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_GUARD_IDLE1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_GUARD_IDLE1, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND1IDLE1
			|| pm->ps->legsAnim == BOTH_STAND9IDLE1
			|| pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE
			|| pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_DUELS
			|| pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_STAFF
			|| pm->ps->legsAnim == BOTH_STAND2IDLE1
			|| pm->ps->legsAnim == BOTH_STAND2IDLE2
			|| pm->ps->legsAnim == BOTH_STAND3IDLE1
			|| pm->ps->legsAnim == BOTH_STAND5IDLE1
			|| pm->ps->legsAnim == BOTH_MENUIDLE1
			|| pm->ps->legsAnim == BOTH_STANDYODAIDLE_STICK
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE2
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE2P
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE3
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE4
			|| pm->ps->legsAnim == BOTH_STANCE_MINIGUN
			|| pm->ps->legsAnim == BOTH_STANCE_READY_MINIGUN_IDLE
			|| pm->ps->legsAnim == BOTH_SABERSINGLECROUCH
			|| pm->ps->legsAnim == BOTH_SABERDUALCROUCH
			|| pm->ps->legsAnim == BOTH_SABERSTAFFCROUCH
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_FORWARD
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_BACK
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_RIGHT
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_LEFT
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_FORWARD
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_BACK
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_RIGHT
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_LEFT
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_FORWARD
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_BACK
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_RIGHT
			|| pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND2TO4)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND2TO4, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND4TO2)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND4TO2, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND4)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND4, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else
		{
			qboolean saber_in_air = qtrue;

			if (pm->ps->saberInFlight)
			{
				//guiding saber
				if (PM_SaberInBrokenParry(pm->ps->saber_move) || pm->ps->saberBlocked == BLOCKED_PARRY_BROKEN ||
					PM_DodgeAnim(pm->ps->torsoAnim))
				{
					//we're stuck in a broken parry
					saber_in_air = qfalse;
				}
				if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)
				{
					//
					if (&g_entities[pm->ps->saberEntityNum] != nullptr && g_entities[pm->ps->saberEntityNum].s.pos.trType == TR_STATIONARY)
					{
						//fell to the ground and we're not trying to pull it back
						saber_in_air = qfalse;
					}
				}
			}
			if (pm->ps->saberInFlight
				&& saber_in_air
				&& (!pm->ps->dualSabers || !pm->ps->saber[1].Active()))
			{
				if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300)
				{
					//don't interrupt a force power anim
					if (pm->ps->torsoAnim != BOTH_LOSE_SABER || !pm->ps->torsoAnimTimer)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERPULL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
				}
			}
			else
			{
				//saber is on
				// Idle for Lightsaber
				if (pm->gent && pm->gent->client)
				{
					if (!G_InCinematicSaberAnim(pm->gent))
					{
						pm->gent->client->ps.SaberDeactivateTrail(0);
					}
				}
				// Select the proper idle Lightsaber attack move from the chart.
				if (pm->ps->saber_move > LS_READY && pm->ps->saber_move < LS_MOVE_MAX)
				{
					PM_SetSaberMove(saber_moveData[pm->ps->saber_move].chain_idle);
				}
				else
				{
					if (PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_LandingAnim(pm->ps->legsAnim)
						|| PM_InCartwheel(pm->ps->legsAnim)
						|| PM_FlippingAnim(pm->ps->legsAnim))
					{
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim ==
							BOTH_BUTTON_HOLD)
						{
							//using something
							if (!pm->ps->useTime)
							{
								//stopped holding it, release
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_BUTTON_RELEASE,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
							}
						}
						else
						{
							//This controls saber movement anims //JaceSolaris
							if ((pm->ps->legsAnim == BOTH_WALK_STAFF
								|| pm->ps->legsAnim == BOTH_WALK_DUAL
								|| pm->ps->legsAnim == BOTH_WALKBACK_STAFF
								|| pm->ps->legsAnim == BOTH_WALKBACK_DUAL
								|| pm->ps->legsAnim == BOTH_WALK_STAFF_AMD
								|| pm->ps->legsAnim == BOTH_WALK_DUAL_AMD
								|| pm->ps->legsAnim == BOTH_WALK1
								|| pm->ps->legsAnim == BOTH_WALK1_MDA
								|| pm->ps->legsAnim == BOTH_WALKBACK1
								|| pm->ps->legsAnim == BOTH_WALK2
								|| pm->ps->legsAnim == BOTH_WALKBACK2)
								&& pm->ps->saberBlockingTime < cg.time
								&& !active_blocking
								&& !is_holding_block_button
								&& !walking_blocking
								&& !IsSurrendering(pm->gent))
							{
								//running w/1-handed weapon uses full-body anim
								int set_flags = SETANIM_FLAG_NORMAL;
								if (PM_LandingAnim(pm->ps->torsoAnim))
								{
									set_flags = SETANIM_FLAG_OVERRIDE;
								}
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
							}
							else if (PM_RunningAnim(pm->ps->legsAnim) && pm->ps->saberBlockingTime < cg.time && !
								IsSurrendering(pm->gent))
							{
								//running w/1-handed weapon uses full-body anim
								int set_flags = SETANIM_FLAG_NORMAL;
								if (PM_LandingAnim(pm->ps->torsoAnim))
								{
									set_flags = SETANIM_FLAG_OVERRIDE;
								}
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, set_flags);
							}
							else
							{
								PM_SetSaberMove(LS_READY);
							}
						}
					}
				}
			}
		}
	}
}

static qboolean pm_is_pistoleer()
{
	switch (pm->ps->weapon)
	{
	case WP_BRYAR_PISTOL:
	case WP_SBD_BLASTER:
	case WP_JAWA:
	case WP_REY:
	case WP_JANGO:
	case WP_CLONEPISTOL:
	case WP_REBELBLASTER:
	case WP_BLASTER_PISTOL:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_AllowedDualPistol()
{
	switch (pm->ps->weapon)
	{
	case WP_BLASTER_PISTOL:
	case WP_REY:
	case WP_JANGO:
	case WP_CLONEPISTOL:
	case WP_REBELBLASTER:
		return qtrue;
	default:;
	}
	return qfalse;
}

/*
-------------------------
PM_TorsoAnimation
-------------------------
*/
void PM_TorsoAnimation()
{
	if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps))
	{
		//in knockdown
		return;
	}

	if (pm->ps->eFlags & EF_HELD_BY_WAMPA)
	{
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

	if (pm->gent && pm->gent->NPC && pm->gent->NPC->scriptFlags & SCF_FORCED_MARCH)
	{
		return;
	}

	if (pm->gent != nullptr && pm->gent->client)
	{
		pm->gent->client->renderInfo.torsoFpsMod = 1.0f;
	}

	if (pm->gent && pm->ps && pm->ps->eFlags & EF_LOCKED_TO_WEAPON)
	{
		if (pm->gent->owner && pm->gent->owner->e_UseFunc == useF_emplaced_gun_use) //ugly way to tell, but...
		{
			//full body
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_GUNSIT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD); //SETANIM_FLAG_NORMAL
		}
		else
		{
			//torso
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_GUNSIT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//SETANIM_FLAG_NORMAL
		}
		return;
	}

	if (pm->ps->taunting > level.time)
	{
		if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_ALORA)
		{
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_ALORA_TAUNT, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//SETANIM_FLAG_NORMAL
		}
		else if (pm->ps->weapon == WP_SABER && pm->gent->client->friendlyfaction == FACTION_NEUTRAL && PM_HasAnimation(
			pm->gent, BOTH_ENGAGETAUNT))
		{
			// No force powers so do basic taunt
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_ENGAGETAUNT, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//SETANIM_FLAG_NORMAL
		}
		else if (pm->ps->weapon == WP_SABER && pm->ps->saber_anim_level == SS_DUAL && PM_HasAnimation(
			pm->gent, BOTH_DUAL_TAUNT))
		{
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_DUAL_TAUNT, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//SETANIM_FLAG_NORMAL
		}
		else if (pm->ps->weapon == WP_SABER && pm->ps->saber_anim_level == SS_STAFF)
			//pm->ps->saber[0].type == SABER_STAFF )
		{
			//turn on the blades
			if (PM_HasAnimation(pm->gent, BOTH_STAFF_TAUNT))
			{
				PM_SetAnim(pm, SETANIM_BOTH, BOTH_STAFF_TAUNT, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				//SETANIM_FLAG_NORMAL
			}
		}
		else if (PM_HasAnimation(pm->gent, BOTH_GESTURE1))
		{
			PM_SetAnim(pm, SETANIM_BOTH, BOTH_GESTURE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//SETANIM_FLAG_NORMAL
			pm->gent->client->ps.SaberActivateTrail(100);
		}
		else
		{
			//PM_SetAnim(pm,SETANIM_TORSO,TORSO_WEAPONIDLE1,SETANIM_FLAG_NORMAL);
		}
		return;
	}

	if (pm->ps->weapon == WP_SABER) // WP_LIGHTSABER
	{
		qboolean saber_in_air = qfalse;

		if (pm->ps->SaberLength() && !pm->ps->saberInFlight && (pm->ps->SaberActive() || !g_noIgniteTwirl->integer))
		{
			PM_TorsoAnimLightsaber();
		}
		else
		{
			if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
			{
				//holding an enemy aloft with force-grip
				return;
			}
			if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] > FORCE_LEVEL_1)
			{
				//holding an enemy aloft with force-grasp
				return;
			}
			if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
				FORCE_LEVEL_1)
			{
				//lightning
				return;
			}
			if (pm->ps->forcePowersActive & 1 << FP_DRAIN)
			{
				//drain
				return;
			}

			saber_in_air = qtrue;

			if (PM_SaberInBrokenParry(pm->ps->saber_move) || pm->ps->saberBlocked == BLOCKED_PARRY_BROKEN ||
				PM_DodgeAnim(pm->ps->torsoAnim))
			{
				//we're stuck in a broken parry
				PM_TorsoAnimLightsaber();
			}
			else
			{
				if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0) //player is 0
				{
					//
					if (&g_entities[pm->ps->saberEntityNum] != nullptr && g_entities[pm->ps->saberEntityNum].s.pos.trType == TR_STATIONARY)
					{
						//fell to the ground and we're not trying to pull it back
						saber_in_air = qfalse;
					}
				}

				if (pm->ps->saberInFlight
					&& saber_in_air
					&& (!pm->ps->dualSabers //not using 2 sabers
						|| !pm->ps->saber[1].Active() //left one off
						|| pm->ps->torsoAnim == BOTH_SABERDUAL_STANCE //not attacking
						|| pm->ps->torsoAnim == BOTH_SABERDUAL_STANCE_JKA //not attacking
						|| pm->ps->torsoAnim == BOTH_SABERPULL //not attacking
						|| pm->ps->torsoAnim == BOTH_STAND1 //not attacking
						|| pm->ps->torsoAnim == BOTH_STAND_SABER_ON //not attacking
						|| pm->ps->torsoAnim == BOTH_STAND_SABER_ON_DUELS //not attacking
						|| pm->ps->torsoAnim == BOTH_STAND_SABER_ON_STAFF //not attacking
						|| pm->ps->torsoAnim == BOTH_STANDYODA_STICK //not attacking
						|| PM_RunningAnim(pm->ps->torsoAnim) //not attacking
						|| PM_WalkingAnim(pm->ps->torsoAnim) //not attacking
						|| PM_JumpingAnim(pm->ps->torsoAnim) //not attacking
						|| PM_SwimmingAnim(pm->ps->torsoAnim)))
				{
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
					if (PM_InSlopeAnim(pm->ps->legsAnim))
					{
						//HMM... this probably breaks the saber putaway and select anims
						if (pm->ps->SaberLength() > 0 && (pm->ps->SaberActive() || !g_noIgniteTwirl->integer))
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND2, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
						}
					}
					else
					{
						if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim ==
							BOTH_BUTTON_HOLD)
						{
							//using something
							if (!pm->ps->useTime)
							{
								//stopped holding it, release
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_BUTTON_RELEASE,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
							} //else still holding, leave it as it is
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
					}
				}
			}
		}

		if (pm->ps->weaponTime <= 0 && (pm->ps->saber_move == LS_READY || pm->ps->SaberLength() == 0) && !saber_in_air)
		{
			TorsoAgainstWindTest(pm->gent);
		}
		return;
	}

	if (PM_ForceAnim(pm->ps->torsoAnim)
		&& pm->ps->torsoAnimTimer > 0)
	{
		//in a force anim, don't do a stand anim
		return;
	}

	qboolean weapon_busy = qfalse;

	if (pm->ps->weapon == WP_NONE)
	{
		weapon_busy = qfalse;
	}
	else if (pm->ps->weaponstate == WEAPON_FIRING || pm->ps->weaponstate == WEAPON_CHARGING || pm->ps->weaponstate ==
		WEAPON_CHARGING_ALT)
	{
		weapon_busy = qtrue;
	}
	else if (pm->ps->lastShotTime > level.time - 2500 && cg.renderingThirdPerson && pm_is_pistoleer())
	{
		weapon_busy = qtrue;
	}
	else if (pm->ps->lastShotTime > level.time - 3000 && cg.renderingThirdPerson)
	{
		weapon_busy = qtrue;
	}
	else if (pm->ps->lastShotTime > level.time - 4000 && !cg.renderingThirdPerson)
	{
		weapon_busy = qtrue;
	}
	else if (pm->ps->weaponTime > 0)
	{
		weapon_busy = qtrue;
	}
	else if (pm->gent && pm->gent->client->fireDelay > 0)
	{
		weapon_busy = qtrue;
	}
	else if (TorsoAgainstWindTest(pm->gent))
	{
		return;
	}
	else if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.zoomTime > cg.time - 5000)
	{
		//if we used binoculars recently, aim weapon
		weapon_busy = qtrue;
		pm->ps->weaponstate = WEAPON_IDLE;
	}
	else if (pm->ps->pm_flags & PMF_DUCKED)
	{
		//ducking is considered on alert... plus looks stupid to have arms hanging down when crouched
		weapon_busy = qtrue;
	}

	if (pm->ps->weapon == WP_NONE ||
		pm->ps->weaponstate == WEAPON_READY ||
		pm->ps->weaponstate == WEAPON_CHARGING ||
		pm->ps->weaponstate == WEAPON_CHARGING_ALT
		&& !PM_SaberInBounce(pm->ps->saber_move)
		&& !PM_SaberInReturn(pm->ps->saber_move)
		&& !PM_SaberInMassiveBounce(pm->ps->torsoAnim))
	{
		if (pm->ps->weapon == WP_SABER && pm->ps->SaberLength() && (pm->ps->SaberActive() || !g_noIgniteTwirl->integer))
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK1, SETANIM_FLAG_NORMAL); //TORSO_WEAPONREADY1
		}
		else if (pm->ps->legsAnim == BOTH_RUN1 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_SINGLE_LIGHTSABER && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_SINGLE_LIGHTSABER, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_STAFF_LIGHTSABER && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_STAFF_LIGHTSABER, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_DUAL_LIGHTSABER && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_DUAL_LIGHTSABER, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_BAZOOKA && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_BAZOOKA, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_BLASTER && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_BLASTER, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_DOUBLE_PISTOL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_DOUBLE_PISTOL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_GRENADE && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_GRENADE, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_HEAVY && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_HEAVY, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_MINIGUN && !weapon_busy)
		{
			//PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_MINIGUN, SETANIM_FLAG_NORMAL);
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_HEAVY, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SPRINT_PISTOL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SPRINT_PISTOL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_RUN2 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN2, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_RUN4 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN4, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_RUN_STAFF && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN_STAFF, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_RUN_DUAL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_RUN_DUAL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_BAZOOKA && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_BAZOOKA, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_BLASTER && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_BLASTER, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_DOUBLE_PISTOL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_DOUBLE_PISTOL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_GRENADE && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_GRENADE, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_HEAVY && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_HEAVY, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_MINIGUN && !weapon_busy)
		{
			//PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_MINIGUN, SETANIM_FLAG_NORMAL);
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_HEAVY, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_JOG_PISTOL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JOG_PISTOL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK1 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_BAZOOKA && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_BAZOOKA, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_BLASTER && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_BLASTER, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_DOUBLE_PISTOL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_DOUBLE_PISTOL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_GRENADE && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_GRENADE, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_HEAVY && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_HEAVY, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_MINIGUN && !weapon_busy)
		{
			//PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_MINIGUN, SETANIM_FLAG_NORMAL);
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_HEAVY, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_PISTOL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_PISTOL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK1_STICK && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK1_STICK, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_MENUIDLE1 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_MENUIDLE1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SABERSINGLECROUCH && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERSINGLECROUCH, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_FORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_FORWARD, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_BACK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_BACK, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_RIGHT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_RIGHT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_LEFT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_FORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_FORWARD, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_BACK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_BACK, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_RIGHT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_RIGHT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_DUAL_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_DUAL_LEFT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_FORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_FORWARD, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_BACK)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_BACK, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_RIGHT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_RIGHT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_BLOCKING_ON_STAFF_LEFT)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_BLOCKING_ON_STAFF_LEFT, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SABERDUALCROUCH && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERDUALCROUCH, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SABERSTAFFCROUCH && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SABERSTAFFCROUCH, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_DUELS && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_DUELS, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_STAFF && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_STAFF, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_IDLE, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_DUELS && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_IDLE_DUELS, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_STAFF && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND_SABER_ON_IDLE_STAFF, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK2 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK2, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_STAFF && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_STAFF, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_STAFF_AMD && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_STAFF_AMD, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_DUAL && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_DUAL, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_WALK_DUAL_AMD && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_WALK_DUAL_AMD, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_CROUCH1IDLE && pm->ps->client_num != 0) //player falls through
		{
			//
		}
		else if (pm->ps->legsAnim == BOTH_JUMP1 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_JUMP1, SETANIM_FLAG_NORMAL, 100); // Only blend over 100ms
		}
		else if (pm->ps->legsAnim == BOTH_SWIM_IDLE1 && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SWIM_IDLE1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SWIMFORWARD && !weapon_busy)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SWIMFORWARD, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->weapon == WP_NONE)
		{
			const int legs_anim = pm->ps->legsAnim;
			PM_SetAnim(pm, SETANIM_TORSO, legs_anim, SETANIM_FLAG_NORMAL);
		}
		else
		{
			if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim == BOTH_BUTTON_HOLD)
			{
				//using something
				if (!pm->ps->useTime)
				{
					//stopped holding it, release
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_BUTTON_RELEASE, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				} //else still holding, leave it as it is
			}
			else if (pm->gent != nullptr
				&& (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent))
				&& pm->ps->weaponstate != WEAPON_CHARGING
				&& pm->ps->weaponstate != WEAPON_CHARGING_ALT
				&& !PM_SaberInBounce(pm->ps->saber_move)
				&& !PM_SaberInReturn(pm->ps->saber_move)
				&& !PM_SaberInMassiveBounce(pm->ps->torsoAnim))
			{
				//PLayer- temp hack for weapon frame
				if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR)
				{
					//ignore
				}
				else if (pm->ps->weapon == WP_MELEE)
				{
					//hehe
					if (pm->gent->client->NPC_class == CLASS_YODA)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
					}
					else if (PM_CrouchAnim(pm->ps->legsAnim))
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND6, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_MD_CIN_1, SETANIM_FLAG_NORMAL); // was BOTH_STAND6
					}
				}
				else
				{
					PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
				}
			}
			else if (PM_InSpecialJump(pm->ps->legsAnim))
			{
				//use legs anim
			}
			else
			{
				switch (pm->ps->weapon)
				{
					// ********************************************************
				case WP_SABER:
					if (pm->ps->saber_move > LS_NONE && pm->ps->saber_move < LS_MOVE_MAX)
					{
						PM_SetSaberMove(saber_moveData[pm->ps->saber_move].chain_idle);
					}
					break;
					// ********************************************************

				case WP_BRYAR_PISTOL:
				case WP_SBD_BLASTER:
				case WP_JAWA:
					if (pm->ps->weaponstate == WEAPON_CHARGING_ALT || weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
					}
					else if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
					}
					break;

					// dual capabilities
				case WP_BLASTER_PISTOL:
				case WP_REBELBLASTER:
				case WP_REY:
				case WP_JANGO:
				case WP_CLONEPISTOL:
				case WP_DUAL_PISTOL:
				case WP_DUAL_CLONEPISTOL:
					if (pm->gent && pm->gent->weaponModel[1] > 0)
					{
						//dual pistols
						if (weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_READY_DUAL, SETANIM_FLAG_NORMAL);
						}
					}
					else
					{
						//single pistols
						if (pm->ps->weaponstate == WEAPON_CHARGING_ALT || weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_DROIDEKA:
					if (pm->gent && pm->gent->weaponModel[1] > 0)
					{
						if (weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_READY_DUAL, SETANIM_FLAG_NORMAL);
						}
					}
					else
					{
						if (weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_NONE:
					//NOTE: should never get here
					break;
				case WP_MELEE:
					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR)
						{
							//ignore
						}
						else if (pm->gent && pm->gent->client && !PM_DroidMelee(pm->gent->client->NPC_class))
						{
							if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_YODA)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
							}
							else if (PM_CrouchAnim(pm->ps->legsAnim))
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND6, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_MD_CIN_1, SETANIM_FLAG_NORMAL); // was BOTH_STAND6
							}
						}
						else
						{
							if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_YODA)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
						}
					}
					break;
				case WP_TUSKEN_STAFF:
					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_NOGHRI_STICK:
					PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
					break;

				case WP_BOWCASTER:
					if (pm->ps->weaponstate == WEAPON_CHARGING || weapon_busy)
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					break;
				case WP_FLECHETTE:
					if (weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_CLONERIFLE:
				case WP_ROCKET_LAUNCHER:
				case WP_CONCUSSION:
				case WP_DEMP2:
				case WP_Z6_ROTARY_CANNON:
					if (weapon_busy)
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				/*case WP_Z6_ROTARY_CANNON:
					if (weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANCE_READY_MINIGUN, SETANIM_FLAG_NORMAL);
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANCE_READY_MINIGUN, SETANIM_FLAG_NORMAL);
					}
					break;*/

				case WP_BLASTER:
				case WP_THEFIRSTORDER:
				case WP_BOBA:
				case WP_REBELRIFLE:
				case WP_CLONECARBINE:
				case WP_BATTLEDROID:
				case WP_CLONECOMMANDO:

					if (weapon_busy)
					{
						if (cg.renderingThirdPerson)
						{
							if (pm->gent->alt_fire || pm->gent->client->NPC_class == CLASS_BATTLEDROID)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (cg.renderingThirdPerson)
						{
							if (pm->gent->alt_fire || pm->gent->client->NPC_class == CLASS_BATTLEDROID)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_REPEATER:
					if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
					{
						if (pm->gent->alt_fire)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY1, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_DISRUPTOR:
				case WP_TUSKEN_RIFLE:
					if (pm->ps->weaponstate != WEAPON_FIRING
						&& pm->ps->weaponstate != WEAPON_CHARGING
						&& pm->ps->weaponstate != WEAPON_CHARGING_ALT
						|| PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running sniper weapon uses normal ready
						if (pm->ps->client_num)
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
							}
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
							}
						}
					}
					else
					{
						if (pm->ps->client_num)
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
							}
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
							}
						}
					}
					break;
				case WP_BOT_LASER:
					PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
					break;
				case WP_THERMAL:
					if (pm->ps->weaponstate != WEAPON_FIRING
						&& pm->ps->weaponstate != WEAPON_CHARGING
						&& pm->ps->weaponstate != WEAPON_CHARGING_ALT
						&& (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim)))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && (pm->ps->weaponstate ==
							WEAPON_CHARGING || pm->ps->weaponstate == WEAPON_CHARGING_ALT))
						{
							//player pulling back to throw
							if (PM_StandingAnim(pm->ps->legsAnim))
							{
								PM_SetAnim(pm, SETANIM_LEGS, BOTH_THERMAL_READY,
									SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
							}
							else if (pm->ps->legsAnim == BOTH_THERMAL_READY)
							{
								//sigh... hold it so pm_footsteps doesn't override
								if (pm->ps->legsAnimTimer < 100)
								{
									pm->ps->legsAnimTimer = 100;
								}
							}
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_THERMAL_READY,
								SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						}
						else
						{
							if (weapon_busy)
							{
								if (cg.renderingThirdPerson)
								{
									PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY10, SETANIM_FLAG_NORMAL);
								}
								else
								{
									if (cg_trueguns.integer)
									{
										PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
									}
								}
							}
							else if (PM_RunningAnim(pm->ps->legsAnim)
								|| PM_WalkingAnim(pm->ps->legsAnim)
								|| PM_JumpingAnim(pm->ps->legsAnim)
								|| PM_SwimmingAnim(pm->ps->legsAnim))
							{
								//running w/1-handed weapon uses full-body anim
								PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg.renderingThirdPerson)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND9, SETANIM_FLAG_NORMAL);
								}
								else
								{
									if (cg_trueguns.integer)
									{
										PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
									}
								}
							}
						}
					}
					break;

				case WP_TRIP_MINE:
				case WP_DET_PACK:
					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (weapon_busy)
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg_trueguns.integer)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
								}
							}
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg_trueguns.integer)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
								}
							}
						}
					}
					break;
				default:
					if (pm->gent->alt_fire)
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
					}
					else if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
							PM_ControlledByPlayer()))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
					}
					break;
				}
			}
		}
	}
	else if (pm->ps->weaponstate == WEAPON_IDLE)
	{
		if (pm->ps->legsAnim == BOTH_GUARD_LOOKAROUND1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_GUARD_LOOKAROUND1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_GUARD_IDLE1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_GUARD_IDLE1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND1IDLE1
			|| pm->ps->legsAnim == BOTH_STAND9IDLE1
			|| pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE
			|| pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_DUELS
			|| pm->ps->legsAnim == BOTH_STAND_SABER_ON_IDLE_STAFF
			|| pm->ps->legsAnim == BOTH_STAND2IDLE1
			|| pm->ps->legsAnim == BOTH_STAND2IDLE2
			|| pm->ps->legsAnim == BOTH_STAND3IDLE1
			|| pm->ps->legsAnim == BOTH_STAND5IDLE1
			|| pm->ps->legsAnim == BOTH_MENUIDLE1
			|| pm->ps->legsAnim == BOTH_STANDYODAIDLE_STICK
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE2
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE2P
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE3
			|| pm->ps->legsAnim == TORSO_WEAPONIDLE4
			|| pm->ps->legsAnim == BOTH_STANCE_MINIGUN
			|| pm->ps->legsAnim == BOTH_STANCE_READY_MINIGUN_IDLE)
		{
			PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
			pm->ps->saber_move = LS_READY;
		}
		else if (pm->ps->legsAnim == BOTH_STAND2TO4)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND2TO4, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND4TO2)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND4TO2, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_STAND4)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND4, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SWIM_IDLE1)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SWIM_IDLE1, SETANIM_FLAG_NORMAL);
		}
		else if (pm->ps->legsAnim == BOTH_SWIMFORWARD)
		{
			PM_SetAnim(pm, SETANIM_TORSO, BOTH_SWIMFORWARD, SETANIM_FLAG_NORMAL);
		}
		else if (PM_InSpecialJump(pm->ps->legsAnim))
		{
			//use legs anim
		}
		else if ((pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim == BOTH_BUTTON_HOLD)
		{
			//using something
			if (!pm->ps->useTime)
			{
				//stopped holding it, release
				PM_SetAnim(pm, SETANIM_TORSO, BOTH_BUTTON_RELEASE, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			} //else still holding, leave it as it is
		}
		else
		{
			if (!weapon_busy
				&& (PM_RunningAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS || PM_ControlledByPlayer()))
				|| PM_JumpingAnim(pm->ps->legsAnim)
				|| PM_SwimmingAnim(pm->ps->legsAnim))
			{
				//running w/1-handed or light 2-handed weapon uses full-body anim if you're not using the weapon right now
				PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
			}
			else
			{
				switch (pm->ps->weapon)
				{
					// ********************************************************
				case WP_SABER: // WP_LIGHTSABER
					// Shouldn't get here, should go to TorsoAnimLightsaber
					break;
					// ********************************************************

				case WP_BRYAR_PISTOL:
				case WP_SBD_BLASTER:
				case WP_JAWA:
					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}

					if (pm->ps->weaponstate == WEAPON_CHARGING_ALT || weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
					}
					else if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2, SETANIM_FLAG_NORMAL);
					}
					break;

					// dual capabilities
				case WP_BLASTER_PISTOL:
				case WP_REBELBLASTER:
				case WP_REY:
				case WP_JANGO:
				case WP_CLONEPISTOL:
				case WP_DUAL_PISTOL:
				case WP_DUAL_CLONEPISTOL:
					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->gent && pm->gent->weaponModel[1] > 0)
					{
						//dual pistols
						if (weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_READY_DUAL, SETANIM_FLAG_NORMAL);
						}
					}
					else
					{
						//single pistols
						if (pm->ps->weaponstate == WEAPON_CHARGING_ALT || weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY2, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_DROIDEKA:
					if (pm->gent && pm->gent->weaponModel[1] > 0)
					{
						//dual pistols
						if (weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_READY_DUAL, SETANIM_FLAG_NORMAL);
						}
					}
					else
					{
						//single pistols
						if (weapon_busy)
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_DUAL, SETANIM_FLAG_NORMAL);
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_NONE:
					//NOTE: should never get here
					break;

				case WP_MELEE:
					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_RANCOR)
						{
							//ignore
						}
						else if (pm->gent && pm->gent->client && !PM_DroidMelee(pm->gent->client->NPC_class))
						{
							if (!PM_JumpingAnim(pm->ps->legsAnim)
								&& !PM_LandingAnim(pm->ps->legsAnim)
								&& !PM_InCartwheel(pm->ps->legsAnim)
								&& !PM_FlippingAnim(pm->ps->legsAnim))
							{
								if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_YODA)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
								}
								else if (PM_CrouchAnim(pm->ps->legsAnim))
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND6, SETANIM_FLAG_NORMAL);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_MD_CIN_1, SETANIM_FLAG_NORMAL); // was BOTH_STAND6
								}
							}
						}
						else
						{
							if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_YODA)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANDYODA_STICK, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND1, SETANIM_FLAG_NORMAL);
							}
						}
					}
					break;

				case WP_TUSKEN_STAFF:
					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_NOGHRI_STICK:
					if (weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_BOWCASTER:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->weaponstate == WEAPON_CHARGING || weapon_busy)
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_FLECHETTE:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_CLONERIFLE:
				case WP_ROCKET_LAUNCHER:
				case WP_CONCUSSION:
				case WP_DEMP2:
				case WP_Z6_ROTARY_CANNON:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (weapon_busy)
					{
						if (cg.renderingThirdPerson)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
					}
					break;

				/*case WP_Z6_ROTARY_CANNON:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (weapon_busy)
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANCE_READY_MINIGUN, SETANIM_FLAG_NORMAL);
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, BOTH_STANCE_READY_MINIGUN_IDLE, SETANIM_FLAG_NORMAL);
					}
					break;*/

				case WP_BLASTER:
				case WP_THEFIRSTORDER:
				case WP_BOBA:
				case WP_REBELRIFLE:
				case WP_CLONECARBINE:
				case WP_BATTLEDROID:
				case WP_CLONECOMMANDO:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (weapon_busy)
					{
						if (cg.renderingThirdPerson)
						{
							if (pm->gent->alt_fire || pm->gent->client->NPC_class == CLASS_BATTLEDROID)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
							}
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
						}
					}
					else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
						PM_ControlledByPlayer()))
					{
						//
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
					}
					break;

				case WP_REPEATER:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == CLASS_GALAKMECH)
					{
						if (pm->gent->alt_fire)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE1, SETANIM_FLAG_NORMAL);
						}
					}
					else
					{
						if (weapon_busy)
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
							}
						}
						else if (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
							PM_ControlledByPlayer()))
						{
							//
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
						}
					}
					break;

				case WP_DISRUPTOR:
				case WP_TUSKEN_RIFLE:
					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}

					if (pm->ps->weaponstate != WEAPON_FIRING
						&& pm->ps->weaponstate != WEAPON_CHARGING
						&& pm->ps->weaponstate != WEAPON_CHARGING_ALT
						|| PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running sniper weapon uses normal ready
						if (pm->ps->client_num)
						{
							if (weapon_busy)
							{
								if (cg.renderingThirdPerson)
								{
									PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
								}
							}
							else
							{
								if (cg.renderingThirdPerson)
								{
									PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE4, SETANIM_FLAG_NORMAL);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
								}
							}
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE4, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
							}
						}
					}
					else
					{
						if (pm->ps->client_num)
						{
							if (weapon_busy)
							{
								if (cg.renderingThirdPerson)
								{
									PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY4, SETANIM_FLAG_NORMAL);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
								}
							}
							else
							{
								if (cg.renderingThirdPerson)
								{
									PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE4, SETANIM_FLAG_NORMAL);
								}
								else
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_ATTACK_FP, SETANIM_FLAG_NORMAL);
								}
							}
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE4, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
							}
						}
					}
					break;

				case WP_BOT_LASER:
					PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE2,
						SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
					break;

				case WP_THERMAL:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}

					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (weapon_busy)
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE10, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg_trueguns.integer)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
								}
							}
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND9, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg_trueguns.integer)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
								}
							}
						}
					}
					break;

				case WP_TRIP_MINE:
				case WP_DET_PACK:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (weapon_busy)
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND9, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg_trueguns.integer)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
								}
							}
						}
						else if (PM_RunningAnim(pm->ps->legsAnim)
							|| PM_WalkingAnim(pm->ps->legsAnim)
							|| PM_JumpingAnim(pm->ps->legsAnim)
							|| PM_SwimmingAnim(pm->ps->legsAnim))
						{
							//running w/1-handed weapon uses full-body anim
							PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, BOTH_STAND9, SETANIM_FLAG_NORMAL);
							}
							else
							{
								if (cg_trueguns.integer)
								{
									PM_SetAnim(pm, SETANIM_TORSO, BOTH_PF_GRENADE, SETANIM_FLAG_NORMAL);
								}
							}
						}
					}
					break;

				default:

					if (pm->ps->forcePowersActive & 1 << FP_GRIP && pm->ps->forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_GRASP && pm->ps->forcePowerLevel[FP_GRASP] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (pm->ps->forcePowersActive & 1 << FP_LIGHTNING && pm->ps->forcePowerLevel[FP_LIGHTNING] >
						FORCE_LEVEL_1)
					{
						//holding an enemy aloft with force-grip
						return;
					}
					if (weapon_busy)
					{
						if (pm->gent->alt_fire)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL);
							}
						}
					}
					else if (PM_RunningAnim(pm->ps->legsAnim)
						|| PM_WalkingAnim(pm->ps->legsAnim)
						|| PM_JumpingAnim(pm->ps->legsAnim)
						|| PM_SwimmingAnim(pm->ps->legsAnim) && (pm->ps->client_num < MAX_CLIENTS ||
							PM_ControlledByPlayer()))
					{
						//running w/1-handed weapon uses full-body anim
						PM_SetAnim(pm, SETANIM_TORSO, pm->ps->legsAnim, SETANIM_FLAG_NORMAL);
					}
					else
					{
						if (pm->gent->alt_fire)
						{
							PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
						}
						else
						{
							if (cg.renderingThirdPerson)
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
							}
							else
							{
								PM_SetAnim(pm, SETANIM_TORSO, TORSO_WEAPONIDLE3, SETANIM_FLAG_NORMAL);
							}
						}
					}
					break;
				}
			}
		}
	}
}

//=========================================================================
// Anim checking utils
//=========================================================================

int PM_GetTurnAnim(const gentity_t* gent, const int anim)
{
	if (!gent)
	{
		return -1;
	}

	switch (anim)
	{
	case BOTH_STAND1: //# Standing idle: no weapon: hands down
	case BOTH_STAND9:
	case BOTH_STAND9IDLE1:
	case BOTH_STAND1IDLE1: //# Random standing idle
	case BOTH_STAND2: //# Standing idle with a weapon
	case BOTH_STAND2_JKA: //# Standing idle with a weapon
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
	case BOTH_SABERFAST_STANCE:
	case BOTH_SABERFAST_STANCE_JKA:
	case BOTH_SABERSLOW_STANCE:
	case BOTH_SABERSLOW_STANCE_JKA:
	case BOTH_SABERTAVION_STANCE: //tavion saberstyle
	case BOTH_SABERTAVION_STANCE_JKA: //tavion saberstyle
	case BOTH_SABERDESANN_STANCE: //desann saber style
	case BOTH_SABERDESANN_STANCE_JKA: //desann saber style
	case BOTH_STAND2IDLE1: //# Random standing idle
	case BOTH_STAND2IDLE2: //# Random standing idle
	case BOTH_STAND3: //# Standing hands behind back: at ease: etc.
	case BOTH_STAND3IDLE1: //# Random standing idle
	case BOTH_STAND4: //# two handed: gun down: relaxed stand
	case BOTH_STAND5: //# standing idle, no weapon, hand down, back straight
	case BOTH_STAND5IDLE1:
	case BOTH_MENUIDLE1: //# Random standing idle
	case BOTH_MD_CIN_1:
	case BOTH_STAND6: //# one handed: gun at side: relaxed stand
	case BOTH_STAND2TO4: //# Transition from stand2 to stand4
	case BOTH_STAND4TO2: //# Transition from stand4 to stand2
	case BOTH_GESTURE1: //# Generic gesture: non-specific
	case BOTH_GESTURE2: //# Generic gesture: non-specific
	case BOTH_TALK1: //# Generic talk anim
	case BOTH_TALK2: //# Generic talk anim
	case BOTH_STANDYODA_STICK:
	case BOTH_STANDYODAIDLE_STICK:
	case TORSO_WEAPONIDLE2:
	case TORSO_WEAPONIDLE2P:
	case TORSO_WEAPONIDLE3:
	case TORSO_WEAPONIDLE4:
	case BOTH_STANCE_MINIGUN:
	case BOTH_STANCE_READY_MINIGUN_IDLE:
	case BOTH_STAND_SABER_ON_IDLE:
	case BOTH_STAND_SABER_ON_IDLE_DUELS:
	case BOTH_STAND_SABER_ON_IDLE_STAFF:
	case BOTH_STAND_SABER_ON:
	case BOTH_STAND_SABER_ON_DUELS:
	case BOTH_STAND_SABER_ON_STAFF:
	case BOTH_SABERSINGLECROUCH:
	case BOTH_SABERDUALCROUCH:
	case BOTH_SABERSTAFFCROUCH:
	{
		if (PM_HasAnimation(gent, LEGS_TURN1))
		{
			return LEGS_TURN1;
		}
		return -1;
	}
	case BOTH_ATTACK1: //# Attack with generic 1-handed weapon
	case BOTH_ATTACK2: //# Attack with generic 2-handed weapon
	case BOTH_ATTACK3: //# Attack with heavy 2-handed weapon
	case BOTH_ATTACK4: //# Attack with ???
	case BOTH_MELEE1: //# First melee attack
	case BOTH_MELEE2: //# Second melee attack
	case BOTH_SLAP_L:
	case BOTH_SLAP_R:
	case BOTH_GUARD_LOOKAROUND1: //# Cradling weapon and looking around
	case BOTH_GUARD_IDLE1: //# Cradling weapon and standing
	case BOTH_ATTACK_DUAL:
	case BOTH_ATTACK_FP:
	{
		if (PM_HasAnimation(gent, LEGS_TURN2))
		{
			return LEGS_TURN2;
		}
		return -1;
	}
	default:
		return -1;
	}
}

int PM_TurnAnimForLegsAnim(const gentity_t* gent, const int anim)
{
	if (!gent)
	{
		return -1;
	}

	switch (anim)
	{
	case BOTH_STAND1: //# Standing idle: no weapon: hands down
	case BOTH_STAND9:
	case BOTH_STAND9IDLE1:
	case BOTH_STAND1IDLE1: //# Random standing idle
	case BOTH_STAND_SABER_ON_IDLE:
	case BOTH_STAND_SABER_ON_IDLE_DUELS:
	case BOTH_STAND_SABER_ON_IDLE_STAFF:
	case BOTH_STAND_SABER_ON:
	case BOTH_STAND_SABER_ON_DUELS:
	case BOTH_STAND_SABER_ON_STAFF:
	case BOTH_STANDYODA_STICK:
	case BOTH_STANDYODAIDLE_STICK:
	case TORSO_WEAPONIDLE2:
	case TORSO_WEAPONIDLE2P:
	case TORSO_WEAPONIDLE3:
	case TORSO_WEAPONIDLE4:
	case BOTH_STANCE_MINIGUN:
	case BOTH_STANCE_READY_MINIGUN_IDLE:
	{
		if (PM_HasAnimation(gent, BOTH_TURNSTAND1))
		{
			return BOTH_TURNSTAND1;
		}
		return -1;
	}
	case BOTH_STAND2: //# Standing idle with a weapon
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
	case BOTH_SABERTAVION_STANCE: //tavion saberstyle
	case BOTH_SABERTAVION_STANCE_JKA: //tavion saberstyle
	case BOTH_SABERDESANN_STANCE: //desann saber style
	case BOTH_SABERDESANN_STANCE_JKA: //desann saber style
	case BOTH_STAND2IDLE1: //# Random standing idle
	case BOTH_STAND2IDLE2: //# Random standing idle
	case BOTH_SABERSINGLECROUCH:
	case BOTH_SABERDUALCROUCH:
	case BOTH_SABERSTAFFCROUCH:
	{
		if (PM_HasAnimation(gent, BOTH_TURNSTAND2))
		{
			return BOTH_TURNSTAND2;
		}
		return -1;
	}
	case BOTH_STAND3: //# Standing hands behind back: at ease: etc.
	case BOTH_STAND3IDLE1: //# Random standing idle
	{
		if (PM_HasAnimation(gent, BOTH_TURNSTAND3))
		{
			return BOTH_TURNSTAND3;
		}
		return -1;
	}
	case BOTH_STAND4: //# two handed: gun down: relaxed stand
	{
		if (PM_HasAnimation(gent, BOTH_TURNSTAND4))
		{
			return BOTH_TURNSTAND4;
		}
		return -1;
	}
	case BOTH_STAND5: //# standing idle, no weapon, hand down, back straight
	case BOTH_STAND5IDLE1:
	case BOTH_MENUIDLE1: //# Random standing idle
	{
		if (PM_HasAnimation(gent, BOTH_TURNSTAND5))
		{
			return BOTH_TURNSTAND5;
		}
		return -1;
	}
	case BOTH_CROUCH1: //# Transition from standing to crouch
	case BOTH_CROUCH1IDLE: //# Crouching idle
	{
		if (PM_HasAnimation(gent, BOTH_TURNCROUCH1))
		{
			return BOTH_TURNCROUCH1;
		}
		return -1;
	}
	default:
		return -1;
	}
}

qboolean PM_InOnGroundAnim(playerState_t* ps)
{
	switch (ps->legsAnim)
	{
	case BOTH_DEAD1:
	case BOTH_DEAD2:
	case BOTH_DEAD3:
	case BOTH_DEAD4:
	case BOTH_DEAD5:
	case BOTH_DEADFORWARD1:
	case BOTH_DEADBACKWARD1:
	case BOTH_DEADFORWARD2:
	case BOTH_DEADBACKWARD2:
	case BOTH_LYINGDEATH1:
	case BOTH_LYINGDEAD1:
	case BOTH_SLEEP1: //# laying on back-rknee up-rhand on torso
		return qtrue;
	case BOTH_KNOCKDOWN1: //#
	case BOTH_KNOCKDOWN2: //#
	case BOTH_KNOCKDOWN3: //#
	case BOTH_KNOCKDOWN4: //#
	case BOTH_KNOCKDOWN5: //#
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
	case BOTH_LK_DL_ST_T_SB_1_L:
	case BOTH_RELEASED:
		if (ps->legsAnimTimer < 500)
		{
			//pretty much horizontal by this point
			return qtrue;
		}
		break;
	case BOTH_PLAYER_PA_3_FLY:
		if (ps->legsAnimTimer < 300)
		{
			//pretty much horizontal by this point
			return qtrue;
		}
		/*
		else if ( ps->client_num < MAX_CLIENTS
			&& ps->legsAnimTimer < 300 + PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME )
		{
			return qtrue;
		}
		*/
		break;
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
		if (ps->legsAnimTimer > PM_AnimLength(g_entities[ps->client_num].client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ps->legsAnim)) - 400)
		{
			//still pretty much horizontal at this point
			return qtrue;
		}
		break;
	default:;
	}

	return qfalse;
}

qboolean PM_InOnGroundAnims(const int anim)
{
	switch (anim)
	{
	case BOTH_DEAD1:
	case BOTH_DEAD2:
	case BOTH_DEAD3:
	case BOTH_DEAD4:
	case BOTH_DEAD5:
	case BOTH_DEADFORWARD1:
	case BOTH_DEADBACKWARD1:
	case BOTH_DEADFORWARD2:
	case BOTH_DEADBACKWARD2:
	case BOTH_LYINGDEATH1:
	case BOTH_LYINGDEAD1:
	case BOTH_SLEEP1: //# laying on back-rknee up-rhand on torso
	case BOTH_KNOCKDOWN1: //#
	case BOTH_KNOCKDOWN2: //#
	case BOTH_KNOCKDOWN3: //#
	case BOTH_KNOCKDOWN4: //#
	case BOTH_KNOCKDOWN5: //#
	case BOTH_SLAPDOWNRIGHT:
	case BOTH_SLAPDOWNLEFT:
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

static qboolean PM_InSpecialDeathAnim()
{
	switch (pm->ps->legsAnim)
	{
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

qboolean PM_InDeathAnim()
{
	//Purposely does not cover stumble death and fall death...
	switch (pm->ps->legsAnim)
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
		//# #sep case BOTH_ DEAD POSES # Should be last frame of corresponding previous anims
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
		//# #sep case BOTH_ DEAD TWITCH/FLOP # React to being shot from death poses
	case BOTH_DEADFLOP1: //# React to being shot from First Death finished pose
	case BOTH_DEADFLOP2: //# React to being shot from Second Death finished pose
	case BOTH_DISMEMBER_HEAD1: //#
	case BOTH_DISMEMBER_TORSO1: //#
	case BOTH_DISMEMBER_LLEG: //#
	case BOTH_DISMEMBER_RLEG: //#
	case BOTH_DISMEMBER_RARM: //#
	case BOTH_DISMEMBER_LARM: //#
		return qtrue;
	default:
		return PM_InSpecialDeathAnim();
	}
}

qboolean PM_InCartwheel(const int anim)
{
	switch (anim)
	{
	case BOTH_ARIAL_LEFT:
	case BOTH_ARIAL_RIGHT:
	case BOTH_ARIAL_F1:
	case BOTH_CARTWHEEL_LEFT:
	case BOTH_CARTWHEEL_RIGHT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InButterfly(const int anim)
{
	switch (anim)
	{
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_StandingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_STAND1:
	case BOTH_STAND2:
	case BOTH_STAND3:
	case BOTH_STAND4:
	case BOTH_ATTACK3:
	case BOTH_STANDYODA_STICK:
	case BOTH_STAND_SABER_ON:
	case BOTH_STAND_SABER_ON_DUELS:
	case BOTH_STAND_SABER_ON_STAFF:
	case BOTH_STAND9:
	case BOTH_STAND_BLOCKING_ON:
	case BOTH_STAND_BLOCKING_ON_DUAL:
	case BOTH_STAND_BLOCKING_ON_STAFF:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_StandingidleAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_SABERFAST_STANCE: //single-saber, fast style
	case BOTH_STAND_BLOCKING_ON:
	case BOTH_STAND_BLOCKING_ON_DUAL:
	case BOTH_STAND_BLOCKING_ON_STAFF:
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
	case BOTH_STAND1:
	case BOTH_STAND1IDLE1:
	case BOTH_STAND9IDLE1:
	case BOTH_STAND2:
	case BOTH_STAND2_JKA:
	case BOTH_STAND2IDLE1:
	case BOTH_STAND2IDLE2:
	case BOTH_STAND3:
	case BOTH_STAND3IDLE1:
	case BOTH_STAND4:
	case BOTH_STAND5:
	case BOTH_STAND5IDLE1:
	case BOTH_MENUIDLE1:
	case BOTH_ATTACK3:
	case BOTH_ATTACK5:
	case BOTH_ATTACK6:
	case BOTH_STANDYODA_STICK:
	case BOTH_STANDYODAIDLE_STICK:
	case TORSO_WEAPONIDLE2:
	case TORSO_WEAPONIDLE2P:
	case TORSO_WEAPONIDLE3:
	case TORSO_WEAPONIDLE4:
	case BOTH_STANCE_MINIGUN:
	case BOTH_STANCE_READY_MINIGUN_IDLE:
	case BOTH_STAND_SABER_ON_IDLE:
	case BOTH_STAND_SABER_ON_IDLE_DUELS:
	case BOTH_STAND_SABER_ON_IDLE_STAFF:
	case BOTH_STAND_SABER_ON:
	case BOTH_STAND_SABER_ON_DUELS:
	case BOTH_STAND_SABER_ON_STAFF:
	case BOTH_STAND9:
	case BOTH_SABERSINGLECROUCH:
	case BOTH_SABERDUALCROUCH:
	case BOTH_SABERSTAFFCROUCH:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_StandingAtReadyAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_SABERFAST_STANCE: //single-saber, fast style
	case BOTH_STAND_BLOCKING_ON:
	case BOTH_STAND_BLOCKING_ON_DUAL:
	case BOTH_STAND_BLOCKING_ON_STAFF:
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
	case BOTH_STAND1:
	case BOTH_STAND1IDLE1:
	case BOTH_STAND9IDLE1:
	case BOTH_STAND2:
	case BOTH_STAND2_JKA:
	case BOTH_STAND2IDLE1:
	case BOTH_STAND2IDLE2:
	case BOTH_STAND3:
	case BOTH_STAND3IDLE1:
	case BOTH_STAND4:
	case BOTH_STAND5:
	case BOTH_STAND5IDLE1:
	case BOTH_MENUIDLE1:
	case BOTH_STANDYODA_STICK:
	case BOTH_STANDYODAIDLE_STICK:
	case TORSO_WEAPONIDLE2:
	case TORSO_WEAPONIDLE2P:
	case TORSO_WEAPONIDLE3:
	case TORSO_WEAPONIDLE4:
	case BOTH_STANCE_MINIGUN:
	case BOTH_STANCE_READY_MINIGUN_IDLE:
	case BOTH_STAND9:
	case BOTH_STAND_SABER_ON_IDLE:
	case BOTH_STAND_SABER_ON_IDLE_DUELS:
	case BOTH_STAND_SABER_ON_IDLE_STAFF:
	case BOTH_STAND_SABER_ON:
	case BOTH_STAND_SABER_ON_DUELS:
	case BOTH_STAND_SABER_ON_STAFF:
	case BOTH_SABERSINGLECROUCH:
	case BOTH_SABERDUALCROUCH:
	case BOTH_SABERSTAFFCROUCH:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_InAirKickingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_A7_KICK_F_AIR:
	case BOTH_FLYING_KICK:
	case BOTH_A7_KICK_B_AIR:
	case BOTH_A7_KICK_R_AIR:
	case BOTH_A7_KICK_L_AIR:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_KickingAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_A7_KICK_F:
	case BOTH_KICK_F_MD:
	case BOTH_A7_KICK_B:
	case BOTH_SWEEP_KICK:
	case BOTH_A7_KICK_R:
	case BOTH_A7_KICK_L:
	case BOTH_A7_KICK_S:
	case BOTH_A7_KICK_BF:
	case BOTH_A7_KICK_RL:
		//NOT a kick, but acts like one:
	case BOTH_A7_HILT:
	case BOTH_SLAP_L:
	case BOTH_SLAP_R:
	case BOTH_A7_SLAP_R:
	case BOTH_A7_SLAP_L:
		//NOT kicks, but do kick traces anyway
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_TUSKENLUNGE1:
	case BOTH_TUSKENATTACK1:
	case BOTH_TUSKENATTACK2:
	case BOTH_TUSKENATTACK3:
		return qtrue;
	default:
		return PM_InAirKickingAnim(anim);
	}
	//return qfalse;
}

qboolean PM_StabDownAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_STABDOWN:
	case BOTH_STABDOWN_STAFF:
	case BOTH_STABDOWN_DUAL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_LungRollAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_ROLL_STAB:
	case BOTH_LUNGE2_B__T_:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_GoingToAttackDown(const playerState_t* ps)
{
	if (PM_StabDownAnim(ps->torsoAnim) //stabbing downward
		|| ps->saber_move == LS_A_LUNGE //lunge
		|| ps->saber_move == LS_A_JUMP_T__B_ //death from above
		|| ps->saber_move == LS_A_T2B //attacking top to bottom
		|| ps->saber_move == LS_S_T2B //starting at attack downward
		|| PM_SaberInTransition(ps->saber_move) && saber_moveData[ps->saber_move].endQuad == Q_T)
		//transitioning to a top to bottom attack
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_ForceUsingSaberAnim(const int anim)
{
	//saber/acrobatic anims that should prevent you from recharging force power while you're in them...
	switch (anim)
	{
	case BOTH_JUMPFLIPSLASHDOWN1:
	case BOTH_JUMPFLIPSTABDOWN:
	case BOTH_FORCELEAP2_T__B_:
	case BOTH_JUMPATTACK6:
	case BOTH_JUMPATTACK7:
	case BOTH_FORCELONGLEAP_START:
	case BOTH_FORCELONGLEAP_ATTACK:
	case BOTH_FORCEWALLRUNFLIP_START:
	case BOTH_FORCEWALLRUNFLIP_END:
	case BOTH_FORCEWALLRUNFLIP_ALT:
	case BOTH_FORCEWALLREBOUND_FORWARD:
	case BOTH_FORCEWALLREBOUND_LEFT:
	case BOTH_FORCEWALLREBOUND_BACK:
	case BOTH_FORCEWALLREBOUND_RIGHT:
	case BOTH_FLIP_ATTACK7:
	case BOTH_FLIP_HOLD7:
	case BOTH_FLIP_LAND:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_PULL_IMPALE_SWING:
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case BOTH_ARIAL_LEFT:
	case BOTH_ARIAL_RIGHT:
	case BOTH_CARTWHEEL_LEFT:
	case BOTH_CARTWHEEL_RIGHT:
	case BOTH_FLIP_LEFT:
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_FLIP_BACK3:
	case BOTH_ALORA_FLIP_B:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
	case BOTH_BUTTERFLY_FL1:
	case BOTH_BUTTERFLY_FR1:
	case BOTH_WALL_RUN_RIGHT:
	case BOTH_WALL_RUN_RIGHT_FLIP:
	case BOTH_WALL_RUN_RIGHT_STOP:
	case BOTH_WALL_RUN_LEFT:
	case BOTH_WALL_RUN_LEFT_FLIP:
	case BOTH_WALL_RUN_LEFT_STOP:
	case BOTH_WALL_FLIP_RIGHT:
	case BOTH_WALL_FLIP_LEFT:
	case BOTH_FORCEJUMP1:
	case BOTH_FORCEINAIR1:
	case BOTH_FORCELAND1:
	case BOTH_FORCEJUMPBACK1:
	case BOTH_FORCEINAIRBACK1:
	case BOTH_FORCELANDBACK1:
	case BOTH_FORCEJUMPLEFT1:
	case BOTH_FORCEINAIRLEFT1:
	case BOTH_FORCELANDLEFT1:
	case BOTH_FORCEJUMPRIGHT1:
	case BOTH_FORCEINAIRRIGHT1:
	case BOTH_FORCELANDRIGHT1:
	case BOTH_FLIP_F:
	case BOTH_FLIP_B:
	case BOTH_FLIP_L:
	case BOTH_FLIP_R:
	case BOTH_ALORA_FLIP_1:
	case BOTH_ALORA_FLIP_2:
	case BOTH_ALORA_FLIP_3:
	case BOTH_DODGE_FL:
	case BOTH_DODGE_FR:
	case BOTH_DODGE_BL:
	case BOTH_DODGE_BR:
	case BOTH_DODGE_L:
	case BOTH_DODGE_R:
	case BOTH_DODGE_HOLD_FL:
	case BOTH_DODGE_HOLD_FR:
	case BOTH_DODGE_HOLD_BL:
	case BOTH_DODGE_HOLD_BR:
	case BOTH_DODGE_HOLD_L:
	case BOTH_DODGE_HOLD_R:
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
	case BOTH_WALL_FLIP_BACK1:
	case BOTH_WALL_FLIP_BACK2:
	case BOTH_SPIN1:
	case BOTH_FJSS_TR_BL:
	case BOTH_FJSS_TL_BR:
	case BOTH_DEFLECTSLASH__R__L_FIN:
	case BOTH_ARIAL_F1:
	case BOTH_DASH_L:
	case BOTH_DASH_R:
	case BOTH_GRAPPLE_PULL:
	case BOTH_GRAPPLE_FIRE:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean G_HasKnockdownAnims(const gentity_t* ent)
{
	if (PM_HasAnimation(ent, BOTH_KNOCKDOWN1)
		&& PM_HasAnimation(ent, BOTH_KNOCKDOWN2)
		&& PM_HasAnimation(ent, BOTH_KNOCKDOWN3)
		&& PM_HasAnimation(ent, BOTH_KNOCKDOWN4)
		&& PM_HasAnimation(ent, BOTH_KNOCKDOWN5)
		&& PM_HasAnimation(ent, BOTH_SLAPDOWNRIGHT)
		&& PM_HasAnimation(ent, BOTH_SLAPDOWNLEFT))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean PM_InAttackRoll(const int anim)
{
	switch (anim)
	{
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_LockedAnim(const int anim)
{
	//anims that can *NEVER* be overridden, regardless
	switch (anim)
	{
	case BOTH_KYLE_PA_1:
	case BOTH_KYLE_PA_2:
	case BOTH_KYLE_PA_3:
	case BOTH_PLAYER_PA_1:
	case BOTH_PLAYER_PA_2:
	case BOTH_PLAYER_PA_3:
	case BOTH_PLAYER_PA_3_FLY:
	case BOTH_TAVION_SCEPTERGROUND:
	case BOTH_TAVION_SWORDPOWER:
	case BOTH_SCEPTER_START:
	case BOTH_SCEPTER_HOLD:
	case BOTH_SCEPTER_STOP:
		//grabbed by wampa
	case BOTH_GRABBED: //#
	case BOTH_RELEASED: //#	when Wampa drops player, transitions into fall on back
	case BOTH_HANG_IDLE: //#
	case BOTH_HANG_ATTACK: //#
	case BOTH_HANG_PAIN: //#
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_FaceProtectAnim(const int anim)
{
	//Protect face against lightning and fire
	switch (anim)
	{
	case BOTH_WIND:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SuperBreakLoseAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_LK_S_DL_S_SB_1_L: //super break I lost
	case BOTH_LK_S_DL_T_SB_1_L: //super break I lost
	case BOTH_LK_S_ST_S_SB_1_L: //super break I lost
	case BOTH_LK_S_ST_T_SB_1_L: //super break I lost
	case BOTH_LK_S_S_S_SB_1_L: //super break I lost
	case BOTH_LK_S_S_T_SB_1_L: //super break I lost
	case BOTH_LK_DL_DL_S_SB_1_L: //super break I lost
	case BOTH_LK_DL_DL_T_SB_1_L: //super break I lost
	case BOTH_LK_DL_ST_S_SB_1_L: //super break I lost
	case BOTH_LK_DL_ST_T_SB_1_L: //super break I lost
	case BOTH_LK_DL_S_S_SB_1_L: //super break I lost
	case BOTH_LK_DL_S_T_SB_1_L: //super break I lost
	case BOTH_LK_ST_DL_S_SB_1_L: //super break I lost
	case BOTH_LK_ST_DL_T_SB_1_L: //super break I lost
	case BOTH_LK_ST_ST_S_SB_1_L: //super break I lost
	case BOTH_LK_ST_ST_T_SB_1_L: //super break I lost
	case BOTH_LK_ST_S_S_SB_1_L: //super break I lost
	case BOTH_LK_ST_S_T_SB_1_L: //super break I lost
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SuperBreakWinAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_LK_S_DL_S_SB_1_W: //super break I won
	case BOTH_LK_S_DL_T_SB_1_W: //super break I won
	case BOTH_LK_S_ST_S_SB_1_W: //super break I won
	case BOTH_LK_S_ST_T_SB_1_W: //super break I won
	case BOTH_LK_S_S_S_SB_1_W: //super break I won
	case BOTH_LK_S_S_T_SB_1_W: //super break I won
	case BOTH_LK_DL_DL_S_SB_1_W: //super break I won
	case BOTH_LK_DL_DL_T_SB_1_W: //super break I won
	case BOTH_LK_DL_ST_S_SB_1_W: //super break I won
	case BOTH_LK_DL_ST_T_SB_1_W: //super break I won
	case BOTH_LK_DL_S_S_SB_1_W: //super break I won
	case BOTH_LK_DL_S_T_SB_1_W: //super break I won
	case BOTH_LK_ST_DL_S_SB_1_W: //super break I won
	case BOTH_LK_ST_DL_T_SB_1_W: //super break I won
	case BOTH_LK_ST_ST_S_SB_1_W: //super break I won
	case BOTH_LK_ST_ST_T_SB_1_W: //super break I won
	case BOTH_LK_ST_S_S_SB_1_W: //super break I won
	case BOTH_LK_ST_S_T_SB_1_W: //super break I won
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberLockBreakAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BF1BREAK:
	case BOTH_BF2BREAK:
	case BOTH_CWCIRCLEBREAK:
	case BOTH_CCWCIRCLEBREAK:
	case BOTH_LK_S_DL_S_B_1_L: //normal break I lost
	case BOTH_LK_S_DL_S_B_1_W: //normal break I won
	case BOTH_LK_S_DL_T_B_1_L: //normal break I lost
	case BOTH_LK_S_DL_T_B_1_W: //normal break I won
	case BOTH_LK_S_ST_S_B_1_L: //normal break I lost
	case BOTH_LK_S_ST_S_B_1_W: //normal break I won
	case BOTH_LK_S_ST_T_B_1_L: //normal break I lost
	case BOTH_LK_S_ST_T_B_1_W: //normal break I won
	case BOTH_LK_S_S_S_B_1_L: //normal break I lost
	case BOTH_LK_S_S_S_B_1_W: //normal break I won
	case BOTH_LK_S_S_T_B_1_L: //normal break I lost
	case BOTH_LK_S_S_T_B_1_W: //normal break I won
	case BOTH_LK_DL_DL_S_B_1_L: //normal break I lost
	case BOTH_LK_DL_DL_S_B_1_W: //normal break I won
	case BOTH_LK_DL_DL_T_B_1_L: //normal break I lost
	case BOTH_LK_DL_DL_T_B_1_W: //normal break I won
	case BOTH_LK_DL_ST_S_B_1_L: //normal break I lost
	case BOTH_LK_DL_ST_S_B_1_W: //normal break I won
	case BOTH_LK_DL_ST_T_B_1_L: //normal break I lost
	case BOTH_LK_DL_ST_T_B_1_W: //normal break I won
	case BOTH_LK_DL_S_S_B_1_L: //normal break I lost
	case BOTH_LK_DL_S_S_B_1_W: //normal break I won
	case BOTH_LK_DL_S_T_B_1_L: //normal break I lost
	case BOTH_LK_DL_S_T_B_1_W: //normal break I won
	case BOTH_LK_ST_DL_S_B_1_L: //normal break I lost
	case BOTH_LK_ST_DL_S_B_1_W: //normal break I won
	case BOTH_LK_ST_DL_T_B_1_L: //normal break I lost
	case BOTH_LK_ST_DL_T_B_1_W: //normal break I won
	case BOTH_LK_ST_ST_S_B_1_L: //normal break I lost
	case BOTH_LK_ST_ST_S_B_1_W: //normal break I won
	case BOTH_LK_ST_ST_T_B_1_L: //normal break I lost
	case BOTH_LK_ST_ST_T_B_1_W: //normal break I won
	case BOTH_LK_ST_S_S_B_1_L: //normal break I lost
	case BOTH_LK_ST_S_S_B_1_W: //normal break I won
	case BOTH_LK_ST_S_T_B_1_L: //normal break I lost
	case BOTH_LK_ST_S_T_B_1_W: //normal break I won
		return static_cast<qboolean>(PM_SuperBreakLoseAnim(anim) || PM_SuperBreakWinAnim(anim));
	default:;
	}
	return qfalse;
}

qboolean PM_GetupAnimNoMove(const int legs_anim)
{
	switch (legs_anim)
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
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_KnockDownAnim(const int anim)
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
	default:;
	}
	return qfalse;
}

qboolean PM_DeathCinAnim(const int anim)
{
	switch (anim)
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
		//# #sep case BOTH_ DEAD POSES # Should be last frame of corresponding previous anims
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
		//# #sep case BOTH_ DEAD TWITCH/FLOP # React to being shot from death poses
	case BOTH_DEADFLOP1: //# React to being shot from First Death finished pose
	case BOTH_DEADFLOP2: //# React to being shot from Second Death finished pose
	case BOTH_DISMEMBER_HEAD1: //#
	case BOTH_DISMEMBER_TORSO1: //#
	case BOTH_DISMEMBER_LLEG: //#
	case BOTH_DISMEMBER_RLEG: //#
	case BOTH_DISMEMBER_RARM: //#
	case BOTH_DISMEMBER_LARM: //#
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_StaggerAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BASHED1:
	case BOTH_H1_S1_T_:
	case BOTH_H1_S1_TR:
	case BOTH_H1_S1_TL:
	case BOTH_H1_S1_BL:
	case BOTH_H1_S1_B_:
	case BOTH_H1_S1_BR:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_KnockDownAnimExtended(const int anim)
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
		//special anims:
	case BOTH_RELEASED:
	case BOTH_LK_DL_ST_T_SB_1_L:
	case BOTH_PLAYER_PA_3_FLY:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_SaberInKata(const saber_moveName_t saber_move)
{
	switch (saber_move)
	{
	case LS_A1_SPECIAL:
	case LS_A2_SPECIAL:
	case LS_A3_SPECIAL:
	case LS_DUAL_SPIN_PROTECT:
	case LS_STAFF_SOULCAL:
		return qtrue;
	default:
		break;
	}
	return qfalse;
}

qboolean PM_SaberInBackAttack(const saber_moveName_t saber_move)
{
	switch (saber_move)
	{
	case LS_A_BACK:
	case LS_A_BACK_CR:
	case LS_A_BACKSTAB:
		return qtrue;
	default:;
	}

	return qfalse;
}

qboolean PM_SaberInOverHeadSlash(const saber_moveName_t saber_move)
{
	switch (saber_move)
	{
	case LS_A_FLIP_STAB:
	case LS_A_FLIP_SLASH:
		return qtrue;
	default:;
	}

	return qfalse;
}

qboolean PM_SaberInRollStab(const saber_moveName_t saber_move)
{
	switch (saber_move)
	{
	case LS_ROLL_STAB:
		return qtrue;
	default:;
	}

	return qfalse;
}

qboolean PM_SaberInLungeStab(const saber_moveName_t saber_move)
{
	switch (saber_move)
	{
	case LS_A_LUNGE:
		return qtrue;
	default:;
	}

	return qfalse;
}

qboolean PM_InKataAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_A6_SABERPROTECT:
	case BOTH_A7_SOULCAL:
	case BOTH_A1_SPECIAL:
	case BOTH_A2_SPECIAL:
	case BOTH_A3_SPECIAL:
	case LS_DUAL_SPIN_PROTECT:
	case LS_STAFF_SOULCAL:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_CanRollFromSoulCal(const playerState_t* ps)
{
	if (ps->legsAnim == BOTH_A7_SOULCAL
		&& ps->legsAnimTimer < 700
		&& ps->legsAnimTimer > 250)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean BG_FullBodyTauntAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_GESTURE1:
	case BOTH_DUAL_TAUNT:
	case BOTH_STAFF_TAUNT:
	case BOTH_BOW:
	case BOTH_MEDITATE:
	case BOTH_MEDITATE_SABER:
	case BOTH_SHOWOFF_FAST:
	case BOTH_SHOWOFF_MEDIUM:
	case BOTH_SHOWOFF_STRONG:
	case BOTH_SHOWOFF_DUAL:
	case BOTH_SHOWOFF_STAFF:
	case BOTH_VICTORY_FAST:
	case BOTH_VICTORY_MEDIUM:
	case BOTH_VICTORY_STRONG:
	case BOTH_VICTORY_DUAL:
	case BOTH_VICTORY_STAFF:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_FullBodyEmoteAnim(const int anim)
{
	switch (anim)
	{
	case PLAYER_SURRENDER_START:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_FullBodyRespectAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_BOW:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_FullBodyCowerstartAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_COWER1:
	case BOTH_COWER1_START:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_FullBodyCowerAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_COWER1:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_IsAlreadyinTauntAnim(const int anim)
{
	switch (anim)
	{
	case BOTH_GESTURE1:
	case BOTH_DUAL_TAUNT:
	case BOTH_STAFF_TAUNT:
	case BOTH_BOW:
	case BOTH_MEDITATE:
	case BOTH_MEDITATE_SABER:
	case BOTH_SHOWOFF_FAST:
	case BOTH_SHOWOFF_MEDIUM:
	case BOTH_SHOWOFF_STRONG:
	case BOTH_SHOWOFF_DUAL:
	case BOTH_SHOWOFF_STAFF:
	case BOTH_VICTORY_FAST:
	case BOTH_VICTORY_MEDIUM:
	case BOTH_VICTORY_STRONG:
	case BOTH_VICTORY_DUAL:
	case BOTH_VICTORY_STAFF:
	case PLAYER_SURRENDER_START:
	case BOTH_TUSKENTAUNT1:
	case TORSO_HANDSIGNAL4:
	case TORSO_HANDSIGNAL3:
	case TORSO_HANDSIGNAL2:
	case TORSO_HANDSIGNAL1:
	case BOTH_COWER1:
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
		//
	case BOTH_RELOAD_MINIGUN:
	case BOTH_RECHARGE_MINIGUN:
	case BOTH_RELOAD_FAIL_MINIGUN:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean PM_Bobaspecialanim(const int anim)
{
	switch (anim)
	{
	case BOTH_GESTURE1:
	case BOTH_TUSKENTAUNT1:
	case TORSO_HANDSIGNAL4:
	case TORSO_HANDSIGNAL3:
	case TORSO_HANDSIGNAL2:
	case TORSO_HANDSIGNAL1:
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
		//
	case BOTH_RELOAD_MINIGUN:
	case BOTH_RECHARGE_MINIGUN:
	case BOTH_RELOAD_FAIL_MINIGUN:
		//
	case BOTH_GUNSIT1:
	case BOTH_ATTACK2:
	case BOTH_ATTACK3:
	case BOTH_ATTACK4:
	case BOTH_ATTACK_DUAL:
	case BOTH_ATTACK_FP:
	case BOTH_READY_DUAL:
		//
	case BOTH_FLAMETHROWER:
	case BOTH_PULL_IMPALE_STAB:
	case BOTH_FORCELIGHTNING_HOLD:
	case BOTH_GRAPPLE_FIRE:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_AnimRequiresResponce(const int anim)
{
	switch (anim)
	{
	case BOTH_GESTURE1:
	case BOTH_ENGAGETAUNT:
	case BOTH_DUAL_TAUNT:
	case BOTH_STAFF_TAUNT:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean BG_AnimIsSurrenderingandRequiresResponce(const int anim)
{
	switch (anim)
	{
	case PLAYER_SURRENDER_START:
	case BOTH_COWER1_START:
		return qtrue;
	default:;
	}
	return qfalse;
}

qboolean SaberAttacking(const gentity_t* self)
{
	if (!self || !self->client)
	{
		return qfalse;
	}

	if (PM_SaberInParry(self->client->ps.saber_move))
	{
		return qfalse;
	}
	if (PM_SaberInBrokenParry(self->client->ps.saber_move))
	{
		return qfalse;
	}
	if (PM_SaberInDeflect(self->client->ps.saber_move))
	{
		return qfalse;
	}
	if (PM_SaberInBounce(self->client->ps.saber_move))
	{
		return qfalse;
	}
	if (PM_SaberInKnockaway(self->client->ps.saber_move))
	{
		return qfalse;
	}

	if (PM_SaberInAttack(self->client->ps.saber_move))
	{
		if (self->client->ps.weaponstate == WEAPON_FIRING && self->client->ps.saberBlocked == BLOCKED_ATK_BOUNCE)
		{
			//if we're firing and not blocking, then we're attacking.
			return qtrue;
		}
	}

	if (PM_SaberInSpecial(self->client->ps.saber_move))
	{
		return qtrue;
	}

	return qfalse;
}