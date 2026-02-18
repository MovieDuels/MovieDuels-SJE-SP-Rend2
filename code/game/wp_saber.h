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

#ifndef __WP_SABER_H
#define __WP_SABER_H

constexpr auto ARMOR_EFFECT_TIME = 500;

constexpr auto JSF_AMBUSH = 16;

//saberEventFlags
constexpr auto SEF_HITENEMY = 0x1; //Hit the enemy;
constexpr auto SEF_HITOBJECT = 0x2; //Hit some other object;
constexpr auto SEF_HITWALL = 0x4; //Hit a wall;
constexpr auto SEF_PARRIED = 0x8; //Parried a saber swipe;
constexpr auto SEF_DEFLECTED = 0x10; //Deflected a missile or saberInFlight;
constexpr auto SEF_BLOCKED = 0x20; //Was blocked by a parry;
#define	SEF_EVENTS		(SEF_HITENEMY|SEF_HITOBJECT|SEF_HITWALL|SEF_PARRIED|SEF_DEFLECTED|SEF_BLOCKED)
constexpr auto SEF_LOCKED = 0x40; //Sabers locked with someone else;
constexpr auto SEF_INWATER = 0x80; //Saber is in water;
constexpr auto SEF_LOCK_WON = 0x100; //Won a saberLock;
//saberEntityState
constexpr auto SES_LEAVING = 1;
constexpr auto SES_HOVERING = 2;
constexpr auto SES_RETURNING = 3;
constexpr auto SES_STUCK = 4;
constexpr auto SES_STUCK_RETRIEVING = 5; //retrieving a stuck saber;

constexpr auto SES_STUCK_RETRIEVE_DELAY = 3000;   // 3 seconds. This will leave you nice and open if you lose your saber.
constexpr auto SES_STUCK_TIME = 6000;               // it cant be stuck for more that 6 seconds or it will just be dropped to the ground for retrieval.;

constexpr auto MAX_LEAVE_TIME = 2500;
constexpr auto MAX_RETURN_TIME = 2000;
constexpr auto MAX_DISARM_TIME = 1750;

constexpr auto SABER_EXTRAPOLATE_DIST = 16.0f;

constexpr auto SABER_MAX_DIST = 400.0f;
#define	SABER_MAX_DIST_SQUARED	(SABER_MAX_DIST*SABER_MAX_DIST)

constexpr auto FORCE_POWER_MAX = 100;

constexpr auto SABER_REFLECT_MISSILE_CONE = 0.2f;

constexpr auto SABER_RADIUS_STANDARD = 3.2f;

constexpr auto SABER_LOCK_TIME = 10000;
constexpr auto SABER_LOCK_DELAYED_TIME = 9500;

constexpr auto FORCE_LIGHTNING_RADIUS_WIDE = 512;
constexpr auto FORCE_LIGHTNING_RADIUS_THIN = 256;

constexpr auto BOBA_MISSILE_ROCKET = 0;
constexpr auto BOBA_MISSILE_LASER = 1;
constexpr auto BOBA_MISSILE_DART = 2;
constexpr auto BOBA_MISSILE_VIBROBLADE = 3;

constexpr auto NPC_PARRYRATE = 100;
constexpr auto MPCOST_PARRIED = 1;
constexpr auto MPCOST_MBLOCKED = 3;
constexpr auto MPCOST_PARRIED_ATTACKFAKE = 2;
constexpr auto MPCOST_PARRYING = -2;
constexpr auto MPCOST_PARRYING_ATTACKFAKE = -4;

extern void Boba_FireWristMissile(gentity_t* self, int whichMissile);
extern void Boba_EndWristMissile(const gentity_t* self, int which_missile);

using wristWeapon_t = struct wristWeapon_s
{
	int theMissile;
	int dummyForcePower;
	int whichWeapon;
	qboolean alt_fire;
	int maxShots;
	int animTimer;
	int animDelay;
	int fireAnim;
	qboolean fullyCharged;
	qboolean leftBolt;
	qboolean hold;
};

using saberLockResult_t = enum
{
	LOCK_VICTORY = 0,
	//one side won
	LOCK_STALEMATE,
	//neither side won
	LOCK_DRAW //both people fall back
};

using sabersLockMode_t = enum
{
	LOCK_FIRST = 0,
	LOCK_TOP = LOCK_FIRST,
	LOCK_DIAG_TR,
	LOCK_DIAG_TL,
	LOCK_DIAG_BR,
	LOCK_DIAG_BL,
	LOCK_R,
	LOCK_L,
	LOCK_RANDOM,
	LOCK_KYLE_GRAB1,
	LOCK_KYLE_GRAB2,
	LOCK_KYLE_GRAB3,
	LOCK_FORCE_DRAIN
};

enum
{
	SABERLOCK_TOP,
	SABERLOCK_SIDE,
	SABERLOCK_LOCK,
	SABERLOCK_BREAK,
	SABERLOCK_SUPERBREAK,
	SABERLOCK_WIN,
	SABERLOCK_LOSE
};

enum
{
	DIR_RIGHT,
	DIR_LEFT,
	DIR_FRONT,
	DIR_BACK
};

enum
{
	EVASION_ROLL_DIR_NONE,
	EVASION_ROLL_DIR_BACK,
	EVASION_ROLL_DIR_LEFT,
	EVASION_ROLL_DIR_RIGHT
};

constexpr auto FORCE_LIGHTSIDE = 1;
constexpr auto FORCE_DARKSIDE = 2;

constexpr auto MAX_FORCE_HEAL_HARD = 25;
constexpr auto MAX_FORCE_HEAL_MEDIUM = 50;
constexpr auto MAX_FORCE_HEAL_EASY = 75;
constexpr auto FORCE_HEAL_INTERVAL = 200; //FIXME: maybe level 1 is slower or level 2 is faster?;

constexpr auto FORCE_GRIP_3_MIN_DIST = 128.0f;
constexpr auto FORCE_GRIP_3_MAX_DIST = 256.0f;
constexpr auto FORCE_GRIP_DIST = 512.0f; //FIXME: vary by power level?;
#define	FORCE_GRIP_DIST_SQUARED	(FORCE_GRIP_DIST*FORCE_GRIP_DIST)

constexpr auto FORCE_STASIS_DIST_HIGH = 2048.0f;
constexpr auto FORCE_STASIS_DIST_LOW = 1024.0f;
#define	FORCE_STASIS_DIST_SQUARED_LOW	(FORCE_STASIS_DIST_LOW*FORCE_STASIS_DIST_LOW)
#define	FORCE_STASIS_DIST_SQUARED_HIGH	(FORCE_STASIS_DIST_HIGH*FORCE_STASIS_DIST_HIGH)

constexpr auto FORCE_DRAIN_DIST = 64.0f; //FIXME: vary by power level?;
#define	FORCE_DRAIN_DIST_SQUARED	(FORCE_DRAIN_DIST*FORCE_DRAIN_DIST)

constexpr auto MAX_DRAIN_DISTANCE = 512;

constexpr auto MIN_SABERBLADE_DRAW_LENGTH = 0.5f;

constexpr auto STAFF_KICK_RANGE = 16;

constexpr auto JUMP_OFF_WALL_SPEED = 200.0f;

constexpr auto FORCE_LONG_LEAP_SPEED = 475.0f; //300;

constexpr auto SABER_KATA_ATTACK_POWER = 80; //75?;

constexpr auto SABER_ALT_ATTACK_POWER = 50; //75?;
constexpr auto SABER_ALT_ATTACK_POWER_LR = 10; //30?;
constexpr auto SABER_ALT_ATTACK_POWER_FB = 25; //30/50?;

constexpr auto FORCE_LONGJUMP_POWER = 20;
constexpr auto FORCE_DEFLECT_PUSH = 10;

constexpr auto WALL_RUN_UP_BACKFLIP_SPEED = -150.0f; //was -300.0f;
constexpr auto MAX_WALL_RUN_Z_NORMAL = 0.4f; //was 0.0f;

//KNOCKDOWN HOLD
constexpr auto PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME = 300;
constexpr auto NPC_KNOCKDOWN_HOLD_EXTRA_TIME = 400;
//KNOCKOVERHOLD
constexpr auto PLAYER_KNOCKOVER_HOLD_EXTRA_TIME = 300;
constexpr auto NPC_KNOCKOVER_HOLD_EXTRA_TIME = 400;

constexpr auto MAX_WALL_GRAB_SLOPE = 0.2f;

//"Matrix" effect flags
constexpr auto MEF_NO_TIMESCALE = 0x000001; //no timescale;
constexpr auto MEF_NO_VERTBOB = 0x000002; //no vert bob;
constexpr auto MEF_NO_SPIN = 0x000004; //no spin;
constexpr auto MEF_NO_RANGEVAR = 0x000008; //no camera range variation;
constexpr auto MEF_HIT_GROUND_STOP = 0x000010; //stop effect when subject hits the ground;
constexpr auto MEF_REVERSE_SPIN = 0x000020; //spin counter-clockwise instead of clockwise;
constexpr auto MEF_MULTI_SPIN = 0x000040; //spin once every second, until the effect stops;
constexpr auto MEF_LOOK_AT_ENEMY = 0x000200;

constexpr auto SABER_PITCH_HACK = 90;

extern float forceJumpStrength[];
extern float forceJumpHeight[];
extern float forceJumpHeightMax[];

extern float forcePushPullRadius[];

extern void ForceSpeed(gentity_t* self, int duration = 0);
extern float forceDashValue[];
extern float forceSpeedValue[];
extern float forceSpeedRangeMod[];
extern float forceSpeedFOVMod[];
extern float forceDashRangeMod[];
extern float forceDashFOVMod[];
extern const char* saberColorStringForColor[];

constexpr auto FORCE_SPEED_DURATION_DASH = 500.0f;

constexpr auto FORCE_SPEED_DURATION = 10000.0f;
constexpr auto FORCE_SPEED_DURATION_FORCE_LEVEL_1 = 1000.0f;
constexpr auto FORCE_SPEED_DURATION_FORCE_LEVEL_2 = 2000.0f;
constexpr auto FORCE_SPEED_DURATION_FORCE_LEVEL_3 = 10000.0f;

constexpr auto FORCE_RAGE_DURATION = 10000.0f;

#define MASK_FORCE_PUSH (MASK_OPAQUE|CONTENTS_SOLID)

enum
{
	FORCE_LEVEL_0,
	FORCE_LEVEL_1,
	FORCE_LEVEL_2,
	FORCE_LEVEL_3,
	NUM_FORCE_POWER_LEVELS
};

#define	FORCE_LEVEL_4 (FORCE_LEVEL_3+1)
#define	FORCE_LEVEL_5 (FORCE_LEVEL_4+1)

enum
{
	FJ_FORWARD,
	FJ_BACKWARD,
	FJ_RIGHT,
	FJ_LEFT,
	FJ_UP
};

constexpr auto FORCE_JUMP_CHARGE_TIME = 1000.0f; //Force jump reaches maximum power in one second;

#define FORCE_POWERS_ROSH_FROM_TWINS ((1<<FP_SPEED)|(1<<FP_GRIP)|(1<<FP_RAGE)|(1<<FP_SABERTHROW))

extern void WP_InitForcePowers(const gentity_t* ent);
extern int WP_GetVelocityForForceJump(const gentity_t* self, vec3_t jump_vel, const usercmd_t* ucmd);
extern int WP_SaberInitBladeData(gentity_t* ent);
extern void G_CreateG2HolsteredWeaponModel(gentity_t* ent, const char* ps_weapon_model, int bolt_num, int weapon_num,
	vec3_t angles, vec3_t offset);
extern void G_CreateG2AttachedWeaponModel(gentity_t* ent, const char* ps_weapon_model, int bolt_num, int weapon_num);
extern void WP_SaberAddG2SaberModels(gentity_t* ent, int specific_saber_num = -1);
extern void WP_SaberAddHolsteredG2SaberModels(gentity_t* ent, int specific_saber_num = -1);
extern qboolean WP_SaberParseParms(const char* SaberName, saberInfo_t* saber, qboolean setColors = qtrue);
extern qboolean WP_BreakSaber(gentity_t* ent, const char* surfName, saberType_t saberType = SABER_NONE);
extern void ForceThrow_JKA(gentity_t* self, qboolean pull, qboolean fake = qfalse);
extern void ForceThrow_MD(gentity_t* self, qboolean pull, qboolean fake = qfalse);
extern void ForceRepulse(gentity_t* self, qboolean pull, qboolean fake = qfalse);
extern qboolean G_GetHitLocFromSurfName(gentity_t* ent, const char* surf_name, int* hit_loc, vec3_t point, vec3_t dir,
	vec3_t blade_dir, int mod, saberType_t saber_type = SABER_NONE);
extern qboolean G_CheckEnemyPresence(const gentity_t* ent, int dir, float radius, float tolerance = 0.75f);
extern void WP_SaberFreeStrings(saberInfo_t& saber);
extern qboolean G_EnoughPowerForSpecialMove(int force_power, int cost, qboolean kata_move = qfalse, qboolean play_sound = qtrue);
extern void G_DrainPowerForSpecialMove(const gentity_t* self, forcePowers_t fp, int cost, qboolean kata_move = qfalse);
extern int G_CostForSpecialMove(int cost, qboolean kata_move = qfalse);
extern gentity_t* G_DropSaberItem(const char* saberType, saber_colors_t saberColor, vec3_t saberPos, vec3_t saberVel,
	vec3_t saberAngles, const gentity_t* copySaber = nullptr);

using evasionType_t = enum
{
	EVASION_NONE = 0,
	EVASION_PARRY,
	EVASION_DUCK_PARRY,
	EVASION_JUMP_PARRY,
	EVASION_DODGE,
	EVASION_JUMP,
	EVASION_DUCK,
	EVASION_FJUMP,
	EVASION_CARTWHEEL,
	EVASION_OTHER,
	EVASION_DASH,
	NUM_EVASION_TYPES
};

using swingType_t = enum
{
	SWING_FAST,
	SWING_MEDIUM,
	SWING_STRONG
};
// Okay, here lies the much-dreaded Pat-created FSM movement chart...  Heretic II strikes again!
// Why am I inflicting this on you?  Well, it's better than hardcoded states.
// Ideally this will be replaced with an external file or more sophisticated move-picker
// once the game gets out of prototype stage. <- HAHA!

#ifdef LS_NONE
#undef LS_NONE
#endif

using saber_moveName_t = enum
{
	// Invalid, or saber not armed
	LS_INVALID = -1,
	LS_NONE = 0,

	// General movements with saber
	LS_READY,
	LS_DRAW,
	LS_PUTAWAY,

	// Attacks
	LS_A_TL2BR,
	//4
	LS_A_L2R,
	LS_A_BL2TR,
	LS_A_BR2TL,
	LS_A_R2L,
	LS_A_TR2BL,
	LS_A_T2B,
	LS_A_BACKSTAB,
	LS_A_BACK,
	LS_A_BACK_CR,
	LS_ROLL_STAB,
	LS_A_LUNGE,
	LS_A_JUMP_T__B_,
	LS_A_FLIP_STAB,
	LS_A_FLIP_SLASH,
	LS_JUMPATTACK_DUAL,
	LS_JUMPATTACK_ARIAL_LEFT,
	LS_JUMPATTACK_ARIAL_RIGHT,
	LS_JUMPATTACK_CART_LEFT,
	LS_JUMPATTACK_CART_RIGHT,
	LS_JUMPATTACK_STAFF_LEFT,
	LS_JUMPATTACK_STAFF_RIGHT,
	LS_BUTTERFLY_LEFT,
	LS_BUTTERFLY_RIGHT,
	LS_A_BACKFLIP_ATK,
	LS_SPINATTACK_DUAL,
	LS_SPINATTACK,
	LS_LEAP_ATTACK,
	LS_SWOOP_ATTACK_RIGHT,
	LS_SWOOP_ATTACK_LEFT,
	LS_TAUNTAUN_ATTACK_RIGHT,
	LS_TAUNTAUN_ATTACK_LEFT,
	LS_KICK_F,
	LS_KICK_F_MD,
	LS_KICK_B,
	LS_KICK_SWEEP,
	LS_KICK_R,
	LS_SMACK_R,
	LS_KICK_L,
	LS_SMACK_L,
	LS_KICK_S,
	LS_KICK_BF,
	LS_KICK_RL,
	LS_KICK_F_AIR,
	LS_FLYING_KICK,
	LS_KICK_B_AIR,
	LS_KICK_R_AIR,
	LS_KICK_L_AIR,
	LS_STABDOWN,
	LS_STABDOWN_STAFF,
	LS_STABDOWN_DUAL,
	LS_DUAL_SPIN_PROTECT,
	LS_STAFF_SOULCAL,
	LS_A1_SPECIAL,
	LS_A2_SPECIAL,
	LS_A3_SPECIAL,
	LS_UPSIDE_DOWN_ATTACK,
	LS_PULL_ATTACK_STAB,
	LS_GRAPPLE_FIRE,
	LS_PULL_ATTACK_SWING,
	LS_SPINATTACK_ALORA,
	LS_DUAL_FB,
	LS_DUAL_LR,
	LS_HILT_BASH,
	LS_SLAP_R,
	LS_SLAP_L,

	//starts
	LS_S_TL2BR,
	//26
	LS_S_L2R,
	LS_S_BL2TR,
	//# Start of attack chaining to SLASH LR2UL
	LS_S_BR2TL,
	//# Start of attack chaining to SLASH LR2UL
	LS_S_R2L,
	LS_S_TR2BL,
	LS_S_T2B,

	//returns
	LS_R_TL2BR,
	//33
	LS_R_L2R,
	LS_R_BL2TR,
	LS_R_BR2TL,
	LS_R_R2L,
	LS_R_TR2BL,
	LS_R_T2B,

	//transitions
	LS_T1_BR__R,
	//40
	LS_T1_BR_TR,
	LS_T1_BR_T_,
	LS_T1_BR_TL,
	LS_T1_BR__L,
	LS_T1_BR_BL,
	LS_T1__R_BR,
	//46
	LS_T1__R_TR,
	LS_T1__R_T_,
	LS_T1__R_TL,
	LS_T1__R__L,
	LS_T1__R_BL,
	LS_T1_TR_BR,
	//52
	LS_T1_TR__R,
	LS_T1_TR_T_,
	LS_T1_TR_TL,
	LS_T1_TR__L,
	LS_T1_TR_BL,
	LS_T1_T__BR,
	//58
	LS_T1_T___R,
	LS_T1_T__TR,
	LS_T1_T__TL,
	LS_T1_T___L,
	LS_T1_T__BL,
	LS_T1_TL_BR,
	//64
	LS_T1_TL__R,
	LS_T1_TL_TR,
	LS_T1_TL_T_,
	LS_T1_TL__L,
	LS_T1_TL_BL,
	LS_T1__L_BR,
	//70
	LS_T1__L__R,
	LS_T1__L_TR,
	LS_T1__L_T_,
	LS_T1__L_TL,
	LS_T1__L_BL,
	LS_T1_BL_BR,
	//76
	LS_T1_BL__R,
	LS_T1_BL_TR,
	LS_T1_BL_T_,
	LS_T1_BL_TL,
	LS_T1_BL__L,

	//Bounces
	LS_B1_BR,
	LS_B1__R,
	LS_B1_TR,
	LS_B1_T_,
	LS_B1_TL,
	LS_B1__L,
	LS_B1_BL,

	//Deflected attacks
	LS_D1_BR,
	LS_D1__R,
	LS_D1_TR,
	LS_D1_T_,
	LS_D1_TL,
	LS_D1__L,
	LS_D1_BL,
	LS_D1_B_,

	//Reflected attacks
	LS_V1_BR,
	LS_V1__R,
	LS_V1_TR,
	LS_V1_T_,
	LS_V1_TL,
	LS_V1__L,
	LS_V1_BL,
	LS_V1_B_,

	// Broken parries
	LS_H1_T_,
	//
	LS_H1_TR,
	LS_H1_TL,
	LS_H1_BR,
	LS_H1_B_,
	LS_H1_BL,

	// Knockaways
	LS_K1_T_,
	//
	LS_K1_TR,
	LS_K1_TR_MD,
	LS_K1_TR_PB,
	LS_K1_TL,
	LS_K1_TL_MD,
	LS_K1_TL_PB,
	LS_K1_BR,
	LS_K1_BL,

	// Parries
	LS_PARRY_UP,
	LS_PARRY_UR,
	LS_PARRY_UL,
	LS_PARRY_LR,
	LS_PARRY_LL,
	LS_PARRY_B,

	// Projectile Reflections
	LS_REFLECT_UP,
	LS_REFLECT_UR,
	LS_REFLECT_UL,
	LS_REFLECT_LR,
	LS_REFLECT_LL,
	LS_REFLECT_B,

	LS_MOVE_MAX //
};

void PM_SetSaberMove(saber_moveName_t new_move);

using saberQuadrant_t = enum
{
	Q_BR,
	Q_R,
	Q_TR,
	Q_T,
	Q_TL,
	Q_L,
	Q_BL,
	Q_B,
	Q_NUM_QUADS
};

using saber_moveData_t = struct
{
	const char* name;
	int animToUse;
	int startQuad;
	int endQuad;
	unsigned animSetFlags;
	int blend_time;
	int blocking;
	saber_moveName_t chain_idle; // What move to call if the attack button is not pressed at the end of this anim
	saber_moveName_t chain_attack; // What move to call if the attack button (and nothing else) is pressed
	int trailLength;
};

extern saber_moveData_t saber_moveData[LS_MOVE_MAX];

#endif	// __WP_SABER_H
