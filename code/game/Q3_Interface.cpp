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

// ICARUS Engine Interface File
//
//	This file is the only section of the ICARUS systems that
//	is not directly portable from engine to engine.
//
//	-- jweier
#include "g_local.h"
#include "g_functions.h"
#include "Q3_Interface.h"
#include "anims.h"
#include "b_local.h"
#include "events.h"
#include "g_nav.h"
#include "../cgame/cg_camera.h"
#include "../game/objectives.h"
#include "g_roff.h"
#include "../cgame/cg_local.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "g_navigator.h"
#include "qcommon/ojk_saved_game_helper.h"

extern cvar_t* com_buildScript;

extern void InitMover(gentity_t* ent);
extern void MatchTeam(gentity_t* teamLeader, int moverState, int time);
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern char* G_GetLocationForEnt(const gentity_t* ent);
extern void NPC_BSSearchStart(int home_wp, bState_t b_state);
extern void InitMoverTrData(gentity_t* ent);
extern qboolean SpotWouldTelefrag2(const gentity_t* mover, vec3_t dest);
extern cvar_t* g_sex;
extern cvar_t* g_timescale;
extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
static void Q3_SetWeapon(int entID, const char* wp_name);
static void Q3_SetItem(int entID, const char* item_name);
extern void CG_ChangeWeapon(int num);
extern int TAG_GetOrigin2(const char* owner, const char* name, vec3_t origin);
extern cvar_t* g_SerenityJediEngineMode;
extern void G_PlayDoorLoopSound(gentity_t* ent);
extern void G_PlayDoorSound(gentity_t* ent, int type);
extern void NPC_SetLookTarget(const gentity_t* self, int entNum, int clearTime);
extern void NPC_ClearLookTarget(const gentity_t* self);
extern void WP_SaberSetColor(const gentity_t* ent, int saber_num, int blade_num, const char* colorName);
extern void WP_SetSaber(gentity_t* ent, int saber_num, const char* saber_name);
extern void WP_SetNPCSaber(gentity_t* ent, int saber_num, const char* saberName);
extern qboolean PM_HasAnimation(const gentity_t* ent, int animation);
extern void G_ChangePlayerModel(gentity_t* ent, const char* new_model);
extern void G_ChangeModel(const char* new_model);
extern void WP_RemoveSaber(gentity_t* ent, int saber_num);
extern void WP_RemoveSecondSaber(gentity_t* ent, int saber_num);
extern void WP_SetSaberOrigin(gentity_t* self, vec3_t new_org);
extern void Rail_LockCenterOfTrack(const char* trackName);
extern void Rail_UnLockCenterOfTrack(const char* trackName);
extern void G_GetBoltPosition(gentity_t* self, int bolt_index, vec3_t pos, int modelIndex = 0);
extern qboolean G_DoDismembermentcin(gentity_t* self, vec3_t point, int mod, int hit_loc,
	qboolean force = qfalse);
extern void G_ChangeScale(const char* data);
extern void UnLockDoors(const gentity_t* ent);
extern void LockDoors(const gentity_t* ent);
extern cvar_t* com_kotor;

const char* GetSaberColor(int color);

extern int BMS_START;
extern int BMS_MID;
extern int BMS_END;
extern cvar_t* g_skippingcin;

extern cvar_t* g_darkkorriban;

extern qboolean stop_icarus;

#define stringIDExpand(str, strEnum)	{ str, strEnum }, ENUM2STRING(strEnum)

stringID_table_t BSTable[] =
{
	ENUM2STRING(BS_DEFAULT), //# default behavior for that NPC
	ENUM2STRING(BS_ADVANCE_FIGHT), //# Advance to captureGoal and shoot enemies if you can
	ENUM2STRING(BS_SLEEP), //# Play awake script when startled by sound
	ENUM2STRING(BS_FOLLOW_LEADER), //# Follow your leader and shoot any enemies you come across
	ENUM2STRING(BS_JUMP), //# Face navgoal and jump to it.
	ENUM2STRING(BS_SEARCH),
	//# Using current waypoint as a base), search the immediate branches of waypoints for enemies
	ENUM2STRING(BS_WANDER), //# Wander down random waypoint paths
	ENUM2STRING(BS_NOCLIP), //# Moves through walls), etc.
	ENUM2STRING(BS_REMOVE), //# Waits for player to leave PVS then removes itself
	ENUM2STRING(BS_CINEMATIC), //# Does nothing but face it's angles and move to a goal if it has one
	ENUM2STRING(BS_FLEE), //# Run toward the nav goal, avoiding danger
	//the rest are internal only
	// ... but we should list them anyway, otherwise workshop behavior will screw up
	ENUM2STRING(BS_WAIT),
	ENUM2STRING(BS_STAND_GUARD),
	ENUM2STRING(BS_PATROL),
	ENUM2STRING(BS_INVESTIGATE),
	ENUM2STRING(BS_HUNT_AND_KILL),
	ENUM2STRING(BS_FOLLOW_OVERRIDE), //# Follow your leader and shoot any enemies you come across
	{"", -1}
};

stringID_table_t NPCClassTable[] =
{
	ENUM2STRING(CLASS_NONE), // hopefully this will never be used by an npc), just covering all bases
	ENUM2STRING(CLASS_ATST), // technically droid...
	ENUM2STRING(CLASS_BARTENDER),
	ENUM2STRING(CLASS_BESPIN_COP),
	ENUM2STRING(CLASS_CLAW),
	ENUM2STRING(CLASS_COMMANDO),
	ENUM2STRING(CLASS_DESANN),
	ENUM2STRING(CLASS_FISH),
	ENUM2STRING(CLASS_FLIER2),
	ENUM2STRING(CLASS_GALAK),
	ENUM2STRING(CLASS_GLIDER),
	ENUM2STRING(CLASS_GONK), // droid
	ENUM2STRING(CLASS_GRAN),
	ENUM2STRING(CLASS_HOWLER),
	ENUM2STRING(CLASS_RANCOR),
	ENUM2STRING(CLASS_SAND_CREATURE),
	ENUM2STRING(CLASS_WAMPA),
	ENUM2STRING(CLASS_IMPERIAL),
	ENUM2STRING(CLASS_IMPWORKER),
	ENUM2STRING(CLASS_INTERROGATOR), // droid
	ENUM2STRING(CLASS_JAN),
	ENUM2STRING(CLASS_JEDI),
	ENUM2STRING(CLASS_KYLE),
	ENUM2STRING(CLASS_LANDO),
	ENUM2STRING(CLASS_LIZARD),
	ENUM2STRING(CLASS_LUKE),
	ENUM2STRING(CLASS_MARK1), // droid
	ENUM2STRING(CLASS_MARK2), // droid
	ENUM2STRING(CLASS_GALAKMECH), // droid
	ENUM2STRING(CLASS_MINEMONSTER),
	ENUM2STRING(CLASS_MONMOTHA),
	ENUM2STRING(CLASS_MORGANKATARN),
	ENUM2STRING(CLASS_MOUSE), // droid
	ENUM2STRING(CLASS_MURJJ),
	ENUM2STRING(CLASS_PRISONER),
	ENUM2STRING(CLASS_PROBE), // droid
	ENUM2STRING(CLASS_PROTOCOL), // droid
	ENUM2STRING(CLASS_R2D2), // droid
	ENUM2STRING(CLASS_R5D2), // droid
	ENUM2STRING(CLASS_REBEL),
	ENUM2STRING(CLASS_REBORN),
	ENUM2STRING(CLASS_REELO),
	ENUM2STRING(CLASS_REMOTE),
	ENUM2STRING(CLASS_RODIAN),
	ENUM2STRING(CLASS_SEEKER), // droid
	ENUM2STRING(CLASS_SENTRY),
	ENUM2STRING(CLASS_SHADOWTROOPER),
	ENUM2STRING(CLASS_SABOTEUR),
	ENUM2STRING(CLASS_STORMTROOPER),
	ENUM2STRING(CLASS_SWAMP),
	ENUM2STRING(CLASS_SWAMPTROOPER),
	ENUM2STRING(CLASS_NOGHRI),
	ENUM2STRING(CLASS_TAVION),
	ENUM2STRING(CLASS_ALORA),
	ENUM2STRING(CLASS_TRANDOSHAN),
	ENUM2STRING(CLASS_UGNAUGHT),
	ENUM2STRING(CLASS_JAWA),
	ENUM2STRING(CLASS_WEEQUAY),
	ENUM2STRING(CLASS_TUSKEN),
	ENUM2STRING(CLASS_BOBAFETT),
	ENUM2STRING(CLASS_ROCKETTROOPER),
	ENUM2STRING(CLASS_SABER_DROID),
	ENUM2STRING(CLASS_ASSASSIN_DROID),
	ENUM2STRING(CLASS_HAZARD_TROOPER),
	ENUM2STRING(CLASS_PLAYER),
	ENUM2STRING(CLASS_VEHICLE),
	ENUM2STRING(CLASS_MANDALORIAN),
	ENUM2STRING(CLASS_JANGO),
	ENUM2STRING(CLASS_SBD),
	ENUM2STRING(CLASS_BATTLEDROID),
	ENUM2STRING(CLASS_DROIDEKA),
	ENUM2STRING(CLASS_WOOKIE),
	ENUM2STRING(CLASS_CLONETROOPER),
	ENUM2STRING(CLASS_STORMCOMMANDO),
	ENUM2STRING(CLASS_VADER),
	ENUM2STRING(CLASS_SITHLORD),
	ENUM2STRING(CLASS_GALEN),
	ENUM2STRING(CLASS_GUARD),
	ENUM2STRING(CLASS_YODA),
	ENUM2STRING(CLASS_OBJECT),
	ENUM2STRING(CLASS_AHSOKA),
	ENUM2STRING(CLASS_JANGODUAL),
	ENUM2STRING(CLASS_BOC),
	ENUM2STRING(CLASS_PROJECTION),
	ENUM2STRING(CLASS_JEDIMASTER),
	ENUM2STRING(CLASS_GROGU),
	ENUM2STRING(CLASS_CALONORD),
	{"CLASS_GALAK_MECH", CLASS_GALAKMECH},

	{"", -1}
};

stringID_table_t RankTable[] =
{
	ENUM2STRING(RANK_CIVILIAN),
	ENUM2STRING(RANK_CREWMAN),
	ENUM2STRING(RANK_ENSIGN),
	ENUM2STRING(RANK_LT_JG),
	ENUM2STRING(RANK_LT),
	ENUM2STRING(RANK_LT_COMM),
	ENUM2STRING(RANK_COMMANDER),
	ENUM2STRING(RANK_CAPTAIN),

	{"", -1},
};

stringID_table_t MoveTypeTable[] =
{
	ENUM2STRING(MT_STATIC),
	ENUM2STRING(MT_WALK),
	ENUM2STRING(MT_RUNJUMP),
	ENUM2STRING(MT_FLYSWIM),
	{"", -1}
};

stringID_table_t BSETTable[] =
{
	ENUM2STRING(BSET_SPAWN), //# script to use when first spawned
	ENUM2STRING(BSET_USE), //# script to use when used
	ENUM2STRING(BSET_AWAKE), //# script to use when awoken/startled
	ENUM2STRING(BSET_ANGER), //# script to use when aquire an enemy
	ENUM2STRING(BSET_ATTACK), //# script to run when you attack
	ENUM2STRING(BSET_VICTORY), //# script to run when you kill someone
	ENUM2STRING(BSET_LOSTENEMY), //# script to run when you can't find your enemy
	ENUM2STRING(BSET_PAIN), //# script to use when take pain
	ENUM2STRING(BSET_FLEE), //# script to use when take pain below 50% of health
	ENUM2STRING(BSET_DEATH), //# script to use when killed
	ENUM2STRING(BSET_DELAYED), //# script to run when self->delayScriptTime is reached
	ENUM2STRING(BSET_BLOCKED), //# script to run when blocked by a friendly NPC or player
	ENUM2STRING(BSET_BUMPED), //# script to run when bumped into a friendly NPC or player (can set bumpRadius)
	ENUM2STRING(BSET_STUCK), //# script to run when blocked by a wall
	ENUM2STRING(BSET_FFIRE), //# script to run when player shoots their own teammates
	ENUM2STRING(BSET_FFDEATH), //# script to run when player kills a teammate
	ENUM2STRING(UNDYINGPLAYERVICTORY), //# script to run when you kill someone
	stringIDExpand("", BSET_INVALID),
	{"", -1}
};

stringID_table_t WPTable[] =
{
	{"NULL", WP_NONE},
	ENUM2STRING(WP_NONE),
	// Player weapons
	ENUM2STRING(WP_SABER),
	// NOTE: lots of code assumes this is the first weapon (... which is crap) so be careful -Ste.
	ENUM2STRING(WP_BLASTER_PISTOL), // apparently some enemy only version of the blaster
	ENUM2STRING(WP_BLASTER),
	ENUM2STRING(WP_DISRUPTOR),
	ENUM2STRING(WP_BOWCASTER),
	ENUM2STRING(WP_REPEATER),
	ENUM2STRING(WP_DEMP2),
	ENUM2STRING(WP_FLECHETTE),
	ENUM2STRING(WP_ROCKET_LAUNCHER),
	ENUM2STRING(WP_THERMAL),
	ENUM2STRING(WP_TRIP_MINE),
	ENUM2STRING(WP_DET_PACK),
	ENUM2STRING(WP_CONCUSSION),
	ENUM2STRING(WP_MELEE), // Any ol' melee attack
	ENUM2STRING(WP_ATST_MAIN),
	ENUM2STRING(WP_ATST_SIDE),
	//NOTE: player can only have up to 16 weapons), anything after that is enemy only
	ENUM2STRING(WP_STUN_BATON),
	// NPC enemy weapons
	ENUM2STRING(WP_BRYAR_PISTOL),
	ENUM2STRING(WP_EMPLACED_GUN),
	ENUM2STRING(WP_DROIDEKA),
	ENUM2STRING(WP_SBD_BLASTER),
	ENUM2STRING(WP_WRIST_BLASTER),
	ENUM2STRING(WP_DUAL_PISTOL),
	ENUM2STRING(WP_DUAL_CLONEPISTOL),
	ENUM2STRING(WP_BOT_LASER), // Probe droid	- Laser blast
	ENUM2STRING(WP_TURRET), // turret guns
	ENUM2STRING(WP_TIE_FIGHTER),
	ENUM2STRING(WP_RAPID_FIRE_CONC),
	ENUM2STRING(WP_JAWA),
	ENUM2STRING(WP_TUSKEN_RIFLE),
	ENUM2STRING(WP_TUSKEN_STAFF),
	ENUM2STRING(WP_SCEPTER),
	ENUM2STRING(WP_NOGHRI_STICK),
	ENUM2STRING(WP_BATTLEDROID),
	ENUM2STRING(WP_THEFIRSTORDER),
	ENUM2STRING(WP_CLONECARBINE),
	ENUM2STRING(WP_REBELBLASTER),
	ENUM2STRING(WP_CLONERIFLE),
	ENUM2STRING(WP_CLONECOMMANDO),
	ENUM2STRING(WP_Z6_ROTARY_CANNON),
	ENUM2STRING(WP_REBELRIFLE),
	ENUM2STRING(WP_REY),
	ENUM2STRING(WP_JANGO),
	ENUM2STRING(WP_BOBA),
	ENUM2STRING(WP_CLONEPISTOL),
	{"", 0}
};

stringID_table_t INVTable[] =
{
	ENUM2STRING(INV_ELECTROBINOCULARS),
	ENUM2STRING(INV_BACTA_CANISTER),
	ENUM2STRING(INV_SEEKER),
	ENUM2STRING(INV_LIGHTAMP_GOGGLES),
	ENUM2STRING(INV_SENTRY),
	ENUM2STRING(INV_CLOAK),
	ENUM2STRING(INV_BARRIER),
	ENUM2STRING(INV_GRAPPLEHOOK),
	{"", 0}
};

stringID_table_t eventTable[] =
{
	//BOTH_h
	//END
	{"", EV_BAD}
};

stringID_table_t DMSTable[] =
{
	{"NULL", -1},
	ENUM2STRING(DM_AUTO), //# let the game determine the dynamic music as normal
	ENUM2STRING(DM_SILENCE), //# stop the music
	ENUM2STRING(DM_EXPLORE), //# force the exploration music to play
	ENUM2STRING(DM_ACTION), //# force the action music to play
	ENUM2STRING(DM_BOSS), //# force the boss battle music to play (if there is any)
	ENUM2STRING(DM_DEATH), //# force the "player dead" music to play
	{"", -1}
};

stringID_table_t HLTable[] =
{
	{"NULL", -1},
	ENUM2STRING(HL_FOOT_RT),
	ENUM2STRING(HL_FOOT_LT),
	ENUM2STRING(HL_LEG_RT),
	ENUM2STRING(HL_LEG_LT),
	ENUM2STRING(HL_WAIST),
	ENUM2STRING(HL_BACK_RT),
	ENUM2STRING(HL_BACK_LT),
	ENUM2STRING(HL_BACK),
	ENUM2STRING(HL_CHEST_RT),
	ENUM2STRING(HL_CHEST_LT),
	ENUM2STRING(HL_CHEST),
	ENUM2STRING(HL_ARM_RT),
	ENUM2STRING(HL_ARM_LT),
	ENUM2STRING(HL_HAND_RT),
	ENUM2STRING(HL_HAND_LT),
	ENUM2STRING(HL_HEAD),
	ENUM2STRING(HL_GENERIC1),
	ENUM2STRING(HL_GENERIC2),
	ENUM2STRING(HL_GENERIC3),
	ENUM2STRING(HL_GENERIC4),
	ENUM2STRING(HL_GENERIC5),
	ENUM2STRING(HL_GENERIC6),
	{"", -1}
};

stringID_table_t setTable[] =
{
	ENUM2STRING(SET_SPAWNSCRIPT), //0
	ENUM2STRING(SET_USESCRIPT),
	ENUM2STRING(SET_AWAKESCRIPT),
	ENUM2STRING(SET_ANGERSCRIPT),
	ENUM2STRING(SET_ATTACKSCRIPT),
	ENUM2STRING(SET_VICTORYSCRIPT),
	ENUM2STRING(SET_PAINSCRIPT),
	ENUM2STRING(SET_FLEESCRIPT),
	ENUM2STRING(SET_DEATHSCRIPT),
	ENUM2STRING(SET_DELAYEDSCRIPT),
	ENUM2STRING(SET_BLOCKEDSCRIPT),
	ENUM2STRING(SET_FFIRESCRIPT),
	ENUM2STRING(SET_FFDEATHSCRIPT),
	ENUM2STRING(SET_MINDTRICKSCRIPT),
	ENUM2STRING(SET_NO_MINDTRICK),
	ENUM2STRING(SET_ORIGIN),
	ENUM2STRING(SET_TELEPORT_DEST),
	ENUM2STRING(SET_ANGLES),
	ENUM2STRING(SET_XVELOCITY),
	ENUM2STRING(SET_YVELOCITY),
	ENUM2STRING(SET_ZVELOCITY),
	ENUM2STRING(SET_Z_OFFSET),
	ENUM2STRING(SET_ENEMY),
	ENUM2STRING(SET_LEADER),
	ENUM2STRING(SET_NAVGOAL),
	ENUM2STRING(SET_ANIM_UPPER),
	ENUM2STRING(SET_ANIM_LOWER),
	ENUM2STRING(SET_ANIM_BOTH),
	ENUM2STRING(SET_ANIM_HOLDTIME_LOWER),
	ENUM2STRING(SET_ANIM_HOLDTIME_UPPER),
	ENUM2STRING(SET_ANIM_HOLDTIME_BOTH),
	ENUM2STRING(SET_PLAYER_TEAM),
	ENUM2STRING(SET_ENEMY_TEAM),
	ENUM2STRING(SET_BEHAVIOR_STATE),
	ENUM2STRING(SET_BEHAVIOR_STATE),
	ENUM2STRING(SET_HEALTH),
	ENUM2STRING(SET_ARMOR),
	ENUM2STRING(SET_DEFAULT_BSTATE),
	ENUM2STRING(SET_CAPTURE),
	ENUM2STRING(SET_DPITCH),
	ENUM2STRING(SET_DYAW),
	ENUM2STRING(SET_EVENT),
	ENUM2STRING(SET_TEMP_BSTATE),
	ENUM2STRING(SET_COPY_ORIGIN),
	ENUM2STRING(SET_VIEWTARGET),
	ENUM2STRING(SET_WEAPON),
	ENUM2STRING(SET_ITEM),
	ENUM2STRING(SET_WALKSPEED),
	ENUM2STRING(SET_RUNSPEED),
	ENUM2STRING(SET_YAWSPEED),
	ENUM2STRING(SET_AGGRESSION),
	ENUM2STRING(SET_AIM),
	ENUM2STRING(SET_FRICTION),
	ENUM2STRING(SET_GRAVITY),
	ENUM2STRING(SET_IGNOREPAIN),
	ENUM2STRING(SET_IGNOREENEMIES),
	ENUM2STRING(SET_IGNOREALERTS),
	ENUM2STRING(SET_DONTSHOOT),
	ENUM2STRING(SET_DONTFIRE),
	ENUM2STRING(SET_LOCKED_ENEMY),
	ENUM2STRING(SET_NOTARGET),
	ENUM2STRING(SET_LEAN),
	ENUM2STRING(SET_CROUCHED),
	ENUM2STRING(SET_WALKING),
	ENUM2STRING(SET_RUNNING),
	ENUM2STRING(SET_CHASE_ENEMIES),
	ENUM2STRING(SET_LOOK_FOR_ENEMIES),
	ENUM2STRING(SET_FACE_MOVE_DIR),
	ENUM2STRING(SET_ALT_FIRE),
	ENUM2STRING(SET_DONT_FLEE),
	ENUM2STRING(SET_FORCED_MARCH),
	ENUM2STRING(SET_NO_RESPONSE),
	ENUM2STRING(SET_NO_COMBAT_TALK),
	ENUM2STRING(SET_NO_ALERT_TALK),
	ENUM2STRING(SET_UNDYING),
	ENUM2STRING(SET_TREASONED),
	ENUM2STRING(SET_DISABLE_SHADER_ANIM),
	ENUM2STRING(SET_SHADER_ANIM),
	ENUM2STRING(SET_INVINCIBLE),
	ENUM2STRING(SET_NOAVOID),
	ENUM2STRING(SET_SHOOTDIST),
	ENUM2STRING(SET_TARGETNAME),
	ENUM2STRING(SET_TARGET),
	ENUM2STRING(SET_TARGET2),
	ENUM2STRING(SET_LOCATION),
	ENUM2STRING(SET_PAINTARGET),
	ENUM2STRING(SET_TIMESCALE),
	ENUM2STRING(SET_VISRANGE),
	ENUM2STRING(SET_EARSHOT),
	ENUM2STRING(SET_VIGILANCE),
	ENUM2STRING(SET_HFOV),
	ENUM2STRING(SET_VFOV),
	ENUM2STRING(SET_DELAYSCRIPTTIME),
	ENUM2STRING(SET_FORWARDMOVE),
	ENUM2STRING(SET_RIGHTMOVE),
	ENUM2STRING(SET_LOCKYAW),
	ENUM2STRING(SET_SOLID),
	ENUM2STRING(SET_CAMERA_GROUP),
	ENUM2STRING(SET_CAMERA_GROUP_Z_OFS),
	ENUM2STRING(SET_CAMERA_GROUP_TAG),
	ENUM2STRING(SET_LOOK_TARGET),
	ENUM2STRING(SET_ADDRHANDBOLT_MODEL),
	ENUM2STRING(SET_REMOVERHANDBOLT_MODEL),
	ENUM2STRING(SET_ADDLHANDBOLT_MODEL),
	ENUM2STRING(SET_REMOVELHANDBOLT_MODEL),
	ENUM2STRING(SET_FACEAUX),
	ENUM2STRING(SET_FACEBLINK),
	ENUM2STRING(SET_FACEBLINKFROWN),
	ENUM2STRING(SET_FACEFROWN),
	ENUM2STRING(SET_FACESMILE),
	ENUM2STRING(SET_FACEGLAD),
	ENUM2STRING(SET_FACEHAPPY),
	ENUM2STRING(SET_FACESHOCKED),
	ENUM2STRING(SET_FACENORMAL),
	ENUM2STRING(SET_FACEEYESCLOSED),
	ENUM2STRING(SET_FACEEYESOPENED),
	ENUM2STRING(SET_SCROLLTEXT),
	ENUM2STRING(SET_LCARSTEXT),
	ENUM2STRING(SET_CENTERTEXT),
	ENUM2STRING(SET_SCROLLTEXTCOLOR),
	ENUM2STRING(SET_CAPTIONTEXTCOLOR),
	ENUM2STRING(SET_CENTERTEXTCOLOR),
	ENUM2STRING(SET_PLAYER_USABLE),
	ENUM2STRING(SET_STARTFRAME),
	ENUM2STRING(SET_ENDFRAME),
	ENUM2STRING(SET_ANIMFRAME),
	ENUM2STRING(SET_LOOP_ANIM),
	ENUM2STRING(SET_INTERFACE),
	ENUM2STRING(SET_SHIELDS),
	ENUM2STRING(SET_NO_KNOCKBACK),
	ENUM2STRING(SET_INVISIBLE),
	ENUM2STRING(SET_VAMPIRE),
	ENUM2STRING(SET_FORCE_INVINCIBLE),
	ENUM2STRING(SET_GREET_ALLIES),
	ENUM2STRING(SET_PLAYER_LOCKED),
	ENUM2STRING(SET_LOCK_PLAYER_WEAPONS),
	ENUM2STRING(SET_NO_IMPACT_DAMAGE),
	ENUM2STRING(SET_PARM1),
	ENUM2STRING(SET_PARM2),
	ENUM2STRING(SET_PARM3),
	ENUM2STRING(SET_PARM4),
	ENUM2STRING(SET_PARM5),
	ENUM2STRING(SET_PARM6),
	ENUM2STRING(SET_PARM7),
	ENUM2STRING(SET_PARM8),
	ENUM2STRING(SET_PARM9),
	ENUM2STRING(SET_PARM10),
	ENUM2STRING(SET_PARM11),
	ENUM2STRING(SET_PARM12),
	ENUM2STRING(SET_PARM13),
	ENUM2STRING(SET_PARM14),
	ENUM2STRING(SET_PARM15),
	ENUM2STRING(SET_PARM16),
	ENUM2STRING(SET_DEFEND_TARGET),
	ENUM2STRING(SET_WAIT),
	ENUM2STRING(SET_COUNT),
	ENUM2STRING(SET_SHOT_SPACING),
	ENUM2STRING(SET_VIDEO_PLAY),
	ENUM2STRING(SET_VIDEO_FADE_IN),
	ENUM2STRING(SET_VIDEO_FADE_OUT),
	ENUM2STRING(SET_REMOVE_TARGET),
	ENUM2STRING(SET_LOADGAME),
	ENUM2STRING(SET_MENU_SCREEN),
	ENUM2STRING(SET_OBJECTIVE_SHOW),
	ENUM2STRING(SET_OBJECTIVE_HIDE),
	ENUM2STRING(SET_OBJECTIVE_SUCCEEDED),
	ENUM2STRING(SET_OBJECTIVE_SUCCEEDED_NO_UPDATE),
	ENUM2STRING(SET_OBJECTIVE_FAILED),
	ENUM2STRING(SET_MISSIONFAILED),
	ENUM2STRING(SET_TACTICAL_SHOW),
	ENUM2STRING(SET_TACTICAL_HIDE),
	ENUM2STRING(SET_FOLLOWDIST),
	ENUM2STRING(SET_SCALE),
	ENUM2STRING(SET_NPC_SCALE),
	ENUM2STRING(SET_KOTOR_MODE),
	ENUM2STRING(SET_OBJECTIVE_CLEARALL),
	ENUM2STRING(SET_OBJECTIVE_LIGHTSIDE),
	ENUM2STRING(SET_MISSIONSTATUSTEXT),
	ENUM2STRING(SET_WIDTH),
	ENUM2STRING(SET_CLOSINGCREDITS),
	ENUM2STRING(SET_SKILL),
	ENUM2STRING(SET_MISSIONSTATUSTIME),
	ENUM2STRING(SET_FORCE_HEAL_LEVEL),
	ENUM2STRING(SET_FORCE_JUMP_LEVEL),
	ENUM2STRING(SET_FORCE_SPEED_LEVEL),
	ENUM2STRING(SET_FORCE_PUSH_LEVEL),
	ENUM2STRING(SET_FORCE_PULL_LEVEL),
	ENUM2STRING(SET_FORCE_MINDTRICK_LEVEL),
	ENUM2STRING(SET_FORCE_GRIP_LEVEL),
	ENUM2STRING(SET_FORCE_LIGHTNING_LEVEL),
	ENUM2STRING(SET_SABER_THROW),
	ENUM2STRING(SET_SABER_DEFENSE),
	ENUM2STRING(SET_SABER_OFFENSE),
	ENUM2STRING(SET_FORCE_RAGE_LEVEL),
	ENUM2STRING(SET_FORCE_PROTECT_LEVEL),
	ENUM2STRING(SET_FORCE_ABSORB_LEVEL),
	ENUM2STRING(SET_FORCE_DRAIN_LEVEL),
	ENUM2STRING(SET_FORCE_SIGHT_LEVEL),
	ENUM2STRING(SET_FORCE_DESTRUCTION_LEVEL),
	ENUM2STRING(SET_FORCE_STASIS_LEVEL),
	ENUM2STRING(SET_FORCE_GRASP_LEVEL),
	ENUM2STRING(SET_FORCE_REPULSE_LEVEL),
	ENUM2STRING(SET_FORCE_LIGHTNING_STRIKE_LEVEL),
	ENUM2STRING(SET_FORCE_FEAR_LEVEL),
	ENUM2STRING(SET_FORCE_DEADLYSIGHT_LEVEL),
	ENUM2STRING(SET_FORCE_PROJECTION_LEVEL),
	ENUM2STRING(SET_FORCE_BLAST_LEVEL),
	ENUM2STRING(SET_SABER1_COLOR1),
	ENUM2STRING(SET_SABER1_COLOR2),
	ENUM2STRING(SET_SABER2_COLOR1),
	ENUM2STRING(SET_SABER2_COLOR2),
	ENUM2STRING(SET_DISMEMBER_LIMB),

	ENUM2STRING(SET_VIEWENTITY),
	ENUM2STRING(SET_WATCHTARGET),
	ENUM2STRING(SET_SABERACTIVE),

	ENUM2STRING(SET_SABER1BLADEON),
	ENUM2STRING(SET_SABER1BLADEOFF),
	ENUM2STRING(SET_SABER2BLADEON),
	ENUM2STRING(SET_SABER2BLADEOFF),

	ENUM2STRING(SET_ADJUST_AREA_PORTALS),
	ENUM2STRING(SET_DMG_BY_HEAVY_WEAP_ONLY),
	ENUM2STRING(SET_SHIELDED),
	ENUM2STRING(SET_NO_GROUPS),
	ENUM2STRING(SET_FIRE_WEAPON),
	ENUM2STRING(SET_FIRE_WEAPON_NO_ANIM),
	ENUM2STRING(SET_SAFE_REMOVE),
	ENUM2STRING(SET_BOBA_JET_PACK),
	ENUM2STRING(SET_INACTIVE),
	ENUM2STRING(SET_FUNC_USABLE_VISIBLE),
	ENUM2STRING(SET_END_SCREENDISSOLVE),
	ENUM2STRING(SET_LOOPSOUND),
	ENUM2STRING(SET_ICARUS_FREEZE),
	ENUM2STRING(SET_ICARUS_UNFREEZE),
	ENUM2STRING(SET_SABER1),
	ENUM2STRING(SET_SABER2),
	ENUM2STRING(SET_PLAYERMODEL),
	ENUM2STRING(SET_VEHICLE),
	ENUM2STRING(SET_SECURITY_KEY),
	ENUM2STRING(SET_DAMAGEENTITY),
	ENUM2STRING(SET_USE_CP_NEAREST),
	ENUM2STRING(SET_MORELIGHT),
	ENUM2STRING(SET_CINEMATIC_SKIPSCRIPT),
	ENUM2STRING(SET_RAILCENTERTRACKLOCKED),
	ENUM2STRING(SET_RAILCENTERTRACKUNLOCKED),
	ENUM2STRING(SET_NO_FORCE),
	ENUM2STRING(SET_NO_FALLTODEATH),
	ENUM2STRING(SET_DISMEMBERABLE),
	ENUM2STRING(SET_NO_ACROBATICS),
	ENUM2STRING(SET_MUSIC_STATE),
	ENUM2STRING(SET_USE_SUBTITLES),
	ENUM2STRING(SET_CLEAN_DAMAGING_ENTS),
	ENUM2STRING(SET_HUD),
	//JKA
	ENUM2STRING(SET_NO_PVS_CULL),
	ENUM2STRING(SET_CLOAK),
	ENUM2STRING(SET_RENDER_CULL_RADIUS),
	ENUM2STRING(SET_DISTSQRD_TO_PLAYER),
	ENUM2STRING(SET_FORCE_HEAL),
	ENUM2STRING(SET_FORCE_SPEED),
	ENUM2STRING(SET_FORCE_PUSH),
	ENUM2STRING(SET_FORCE_PUSH_FAKE),
	ENUM2STRING(SET_FORCE_PULL),
	ENUM2STRING(SET_FORCE_MIND_TRICK),
	ENUM2STRING(SET_FORCE_GRIP),
	ENUM2STRING(SET_FORCE_LIGHTNING),
	ENUM2STRING(SET_FORCE_SABERTHROW),
	ENUM2STRING(SET_FORCE_RAGE),
	ENUM2STRING(SET_FORCE_PROTECT),
	ENUM2STRING(SET_FORCE_ABSORB),
	ENUM2STRING(SET_FORCE_DRAIN),
	ENUM2STRING(SET_FORCE_DESTRUCTION),
	ENUM2STRING(SET_FORCE_STASIS),
	ENUM2STRING(SET_FORCE_GRASP),
	ENUM2STRING(SET_FORCE_REPULSE),
	ENUM2STRING(SET_FORCE_LIGHTNING_STRIKE),
	ENUM2STRING(SET_FORCE_FEAR),
	ENUM2STRING(SET_FORCE_DEADLYSIGHT),
	ENUM2STRING(SET_FORCE_PROJECTION),
	ENUM2STRING(SET_FORCE_BLAST),
	ENUM2STRING(SET_WINTER_GEAR),
	ENUM2STRING(SET_NO_ANGLES),
	ENUM2STRING(SET_SABER_ORIGIN),
	ENUM2STRING(SET_SKIN),
	ENUM2STRING(SET_RADAR_ICON),
	ENUM2STRING(SET_RADAR_OBJECT),
	ENUM2STRING(SET_VADERBREATH),
	ENUM2STRING(SET_VADERDAMAGEDBREATH),
	ENUM2STRING(SET_SABERCOLLISIONS),

	ENUM2STRING(SET_ANIM_LOWER_FORCED),
	ENUM2STRING(SET_ANIM_UPPER_FORCED),
	ENUM2STRING(SET_ANIM_BOTH_FORCED),
	ENUM2STRING(SET_ANIM_HOLDTIME_LOWER_FORCED),
	ENUM2STRING(SET_ANIM_HOLDTIME_UPPER_FORCED),
	ENUM2STRING(SET_ANIM_HOLDTIME_BOTH_FORCED),
	ENUM2STRING(SET_ANIM_SPEED_LOWER_FORCED),
	ENUM2STRING(SET_ANIM_SPEED_UPPER_FORCED),
	ENUM2STRING(SET_ANIM_SPEED_BOTH_FORCED),
	ENUM2STRING(SET_NO_SABER_RETURN),

	ENUM2STRING(SET_MODEL),
	ENUM2STRING(SET_PLAYERSKIN),
	ENUM2STRING(SET_PLAYERSCALE),
	ENUM2STRING(SET_SOUNDSET),
	ENUM2STRING(SET_GENDER),

	ENUM2STRING(SET_NPCSABER1),
	ENUM2STRING(SET_NPCSABER2),

	ENUM2STRING(SET_ENDLESS_BP),

	ENUM2STRING(SET_MORELIGHT_PLAYER),

	ENUM2STRING(SET_CONSOLE_COMMAND),

	ENUM2STRING(SET_UNDYINGPLAYERVICTORYSCRIPT),

	ENUM2STRING(SET_FFAMODE),

	ENUM2STRING(SET_ENDLESS_ST),

	{"", SET_}
};

qboolean COM_ParseString(char** data, char** s);

//=======================================================================

vec4_t textcolor_caption;
vec4_t textcolor_center;
vec4_t textcolor_scroll;

/*
-------------------------
void Q3_SetTaskID( gentity_t *ent, taskID_t taskType, int taskID )
-------------------------
*/

static void Q3_TaskIDSet(gentity_t* ent, const taskID_t taskType, const int taskID)
{
	if (taskType < TID_CHAN_VOICE || taskType >= NUM_TIDS)
	{
		return;
	}

	//Might be stomping an old task, so complete and clear previous task if there was one
	Q3_TaskIDComplete(ent, taskType);

	ent->taskID[taskType] = taskID;
}

/*
-------------------------
SetTextColor
-------------------------
*/

static void SetTextColor(vec4_t textcolor, const char* color)
{
	if (Q_stricmp(color, "BLACK") == 0)
	{
		VectorCopy4(colorTable[CT_BLACK], textcolor);
	}
	else if (Q_stricmp(color, "RED") == 0)
	{
		VectorCopy4(colorTable[CT_RED], textcolor);
	}
	else if (Q_stricmp(color, "GREEN") == 0)
	{
		VectorCopy4(colorTable[CT_GREEN], textcolor);
	}
	else if (Q_stricmp(color, "YELLOW") == 0)
	{
		VectorCopy4(colorTable[CT_YELLOW], textcolor);
	}
	else if (Q_stricmp(color, "BLUE") == 0)
	{
		VectorCopy4(colorTable[CT_BLUE], textcolor);
	}
	else if (Q_stricmp(color, "CYAN") == 0)
	{
		VectorCopy4(colorTable[CT_CYAN], textcolor);
	}
	else if (Q_stricmp(color, "MAGENTA") == 0)
	{
		VectorCopy4(colorTable[CT_MAGENTA], textcolor);
	}
	else if (Q_stricmp(color, "WHITE") == 0)
	{
		VectorCopy4(colorTable[CT_WHITE], textcolor);
	}
	else
	{
		VectorCopy4(colorTable[CT_WHITE], textcolor);
	}
}

/*
-------------------------
void Q3_ClearTaskID( int *taskID )

WARNING: Clearing a taskID will make that task never finish unless you intend to
			return the same taskID from somewhere else.
-------------------------
*/
void Q3_TaskIDClear(int* taskID)
{
	*taskID = -1;
}

/*
-------------------------
qboolean Q3_TaskIDPending( gentity_t *ent, taskID_t taskType )
-------------------------
*/
qboolean Q3_TaskIDPending(const gentity_t* ent, const taskID_t taskType)
{
	if (ent->m_iIcarusID == IIcarusInterface::ICARUS_INVALID /*!ent->sequencer || !ent->taskManager*/)
	{
		return qfalse;
	}

	if (taskType < TID_CHAN_VOICE || taskType >= NUM_TIDS)
	{
		return qfalse;
	}

	if (ent->taskID[taskType] >= 0) //-1 is none
	{
		return qtrue;
	}

	return qfalse;
}

/*
-------------------------
void Q3_TaskIDComplete( gentity_t *ent, taskID_t taskType )
-------------------------
*/
void Q3_TaskIDComplete(gentity_t* ent, const taskID_t taskType)
{
	if (taskType < TID_CHAN_VOICE || taskType >= NUM_TIDS)
	{
		return;
	}

	if (ent->m_iIcarusID != IIcarusInterface::ICARUS_INVALID /*ent->taskManager*/ && Q3_TaskIDPending(ent, taskType))
	{
		//Complete it
		IIcarusInterface::GetIcarus()->Completed(ent->m_iIcarusID, ent->taskID[taskType]);

		//See if any other tasks have the name number and clear them so we don't complete more than once
		const int clearTask = ent->taskID[taskType];
		for (int& tid : ent->taskID)
		{
			if (tid == clearTask)
			{
				Q3_TaskIDClear(&tid);
			}
		}

		//clear it - should be cleared in for loop above
		//Q3_TaskIDClear( &ent->taskID[taskType] );
	}
	//otherwise, wasn't waiting for a task to complete anyway
}

/*
============
Q3_CheckStringCounterIncrement
  Description	:
  Return type	: static float
  Argument		: const char *string
============
*/
static float Q3_CheckStringCounterIncrement(const char* string)
{
	char* numString;
	float val = 0.0f;

	if (string[0] == '+')
	{
		//We want to increment whatever the value is by whatever follows the +
		if (string[1])
		{
			numString = const_cast<char*>(&string[1]);
			val = atof(numString);
		}
	}
	else if (string[0] == '-')
	{
		//we want to decrement
		if (string[1])
		{
			numString = const_cast<char*>(&string[1]);
			val = atof(numString) * -1;
		}
	}

	return val;
}

/*
-------------------------
Q3_GetAnimLower
-------------------------
*/
static char* Q3_GetAnimLower(const gentity_t* ent)
{
	if (ent->client == nullptr)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_GetAnimLower: attempted to read animation state off non-client!\n");
		return nullptr;
	}

	const int anim = ent->client->ps.legsAnim;

	return const_cast<char*>(GetStringForID(anim_table, anim));
}

/*
-------------------------
Q3_GetAnimUpper
-------------------------
*/
static char* Q3_GetAnimUpper(const gentity_t* ent)
{
	if (ent->client == nullptr)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_GetAnimUpper: attempted to read animation state off non-client!\n");
		return nullptr;
	}

	const int anim = ent->client->ps.torsoAnim;

	return const_cast<char*>(GetStringForID(anim_table, anim));
}

/*
-------------------------
Q3_GetAnimBoth
-------------------------
*/
static char* Q3_GetAnimBoth(const gentity_t* ent)
{
	char* lowerName = Q3_GetAnimLower(ent);
	const char* upperName = Q3_GetAnimUpper(ent);

	if (VALIDSTRING(lowerName) == false)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_GetAnimBoth: NULL legs animation string found!\n");
		return nullptr;
	}

	if (VALIDSTRING(upperName) == false)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_GetAnimBoth: NULL torso animation string found!\n");
		return nullptr;
	}

	if (Q_stricmp(lowerName, upperName))
	{
#ifdef _DEBUG	// sigh, cut down on tester reports that aren't important
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_GetAnimBoth: legs and torso animations did not match : returning legs\n");
#endif
	}

	return lowerName;
}

/*
-------------------------
Q3_SetObjective
-------------------------
*/
extern qboolean G_CheckPlayerDarkSide();

static void Q3_SetObjective(const char* ObjEnum, const int status)
{
	gclient_t* client = &level.clients[0];

	const int objectiveID = GetIDForString(objectiveTable, ObjEnum);
	objectives_t* objective = &client->sess.mission_objectives[objectiveID];
	int* objectivesShown = &client->sess.missionObjectivesShown;

	switch (status)
	{
	case SET_OBJ_HIDE:
		objective->display = static_cast<qboolean>(OBJECTIVE_HIDE != 0);
		break;
	case SET_OBJ_SHOW:
		objective->display = static_cast<qboolean>(OBJECTIVE_SHOW != 0);
		objectivesShown++;
		missionInfo_Updated = qtrue; // Activate flashing text
		break;
	case SET_OBJ_PENDING:
		objective->status = OBJECTIVE_STAT_PENDING;
		if (objective->display != OBJECTIVE_HIDE)
		{
			objectivesShown++;
			missionInfo_Updated = qtrue; // Activate flashing text
		}
		break;
	case SET_OBJ_SUCCEEDED:
		objective->status = OBJECTIVE_STAT_SUCCEEDED;
		if (objective->display != OBJECTIVE_HIDE)
		{
			objectivesShown++;
			missionInfo_Updated = qtrue; // Activate flashing text
		}
		break;
	case SET_OBJ_FAILED:
		objective->status = OBJECTIVE_STAT_FAILED;
		if (objective->display != OBJECTIVE_HIDE)
		{
			objectivesShown++;
			missionInfo_Updated = qtrue; // Activate flashing text
		}
		break;
	default:;
	}

	if (objectiveID == LIGHTSIDE_OBJ
		&& status == SET_OBJ_FAILED)
	{
		//don't do this unless we just set that specific objective (just turned to the dark side) with this SET_OBJECTIVE command
		G_CheckPlayerDarkSide();
	}
}

/*
-------------------------
Q3_SetMissionFailed
-------------------------
*/
extern void G_PlayerGuiltDeath();

static void Q3_SetMissionFailed(const char* TextEnum)
{
	gentity_t* ent = &g_entities[0];

	if (ent->health > 0)
	{
		G_PlayerGuiltDeath();
	}
	ent->health = 0;
	//FIXME: what about other NPCs?  Scripts?

	// statusTextIndex is looked at on the client side.
	statusTextIndex = GetIDForString(missionFailedTable, TextEnum);
	cg.missionStatusShow = qtrue;
	if (ent->client)
	{
		//hold this screen up for at least 2 seconds
		ent->client->respawnTime = level.time + 2000;
	}
}

/*
-------------------------
Q3_SetStatusText
-------------------------
*/
static void Q3_SetStatusText(const char* StatusTextEnum)
{
	const int statusTextID = GetIDForString(statusTextTable, StatusTextEnum);

	switch (statusTextID)
	{
	case STAT_INSUBORDINATION:
	case STAT_YOUCAUSEDDEATHOFTEAMMATE:
	case STAT_DIDNTPROTECTTECH:
	case STAT_DIDNTPROTECT7OF9:
	case STAT_NOTSTEALTHYENOUGH:
	case STAT_STEALTHTACTICSNECESSARY:
	case STAT_WATCHYOURSTEP:
	case STAT_JUDGEMENTMUCHDESIRED:
		statusTextIndex = statusTextID;
		break;
	default:
		assert(0);
		break;
	}
}

/*
-------------------------
Q3_ObjectiveClearAll
-------------------------
*/
static void Q3_ObjectiveClearAll()
{
	client = &level.clients[0];
	memset(client->sess.mission_objectives, 0, sizeof client->sess.mission_objectives);
}

/*
=============
Q3_SetCaptionTextColor

Change color text prints in
=============
*/
static void Q3_SetCaptionTextColor(const char* color)
{
	SetTextColor(textcolor_caption, color);
}

/*
=============
Q3_SetCenterTextColor

Change color text prints in
=============
*/
static void Q3_SetCenterTextColor(const char* color)
{
	SetTextColor(textcolor_center, color);
}

/*
=============
Q3_SetScrollTextColor

Change color text prints in
=============
*/
static void Q3_SetScrollTextColor(const char* color)
{
	SetTextColor(textcolor_scroll, color);
}

/*
=============
Q3_ScrollText

Prints a message in the center of the screen
=============
*/
static void Q3_ScrollText(const char* id)
{
	gi.SendServerCommand(0, "st \"%s\"", id);
}

/*
=============
Q3_LCARSText

Prints a message in the center of the screen giving it an LCARS frame around it
=============
*/
static void Q3_LCARSText(const char* id)
{
	gi.SendServerCommand(0, "lt \"%s\"", id);
}

/*
=============
`

Returns the sequencer of the entity by the given name
=============
*/
/*static gentity_t *Q3_GetEntityByName( const char *name )
{
	gentity_t				*ent;
	entitylist_t::iterator		ei;
	char					temp[1024];

	if ( name == NULL || name[0] == NULL )
		return NULL;

	strncpy( (char *) temp, name, sizeof(temp) );
	temp[sizeof(temp)-1] = 0;

	ei = ICARUS_EntList.find( strupr( (char *) temp ) );

	if ( ei == ICARUS_EntList.end() )
		return NULL;

	ent = &g_entities[(*ei).second];

	return ent;
	// this now returns the ent instead of the sequencer -- dmv 06/27/01
//	if (ent == NULL)
//		return NULL;
//	return ent->sequencer;
}*/

/*
=============
Q3_GetTime

Get the current game time
=============
*/
/*static DWORD Q3_GetTime( void )
{
	return level.time;
}*/

/*
=============
G_AddSexToPlayerString

Take any string, look for "jaden_male/" replace with "jaden_fmle/" based on "sex"
And: Take any string, look for "/mr_" replace with "/ms_" based on "sex"
returns qtrue if changed to ms
=============
*/
#define JADEN_VOICES_CUSTOM
static qboolean G_AddSexToPlayerString(char* string, const qboolean qDoBoth)
{
	char* start;
	bool bUseCustomDirectory = false;

	if VALIDSTRING(string) {
		char snddir[MAX_QPATH];
#ifdef JADEN_VOICES_CUSTOM
		gi.Cvar_VariableStringBuffer("snd", snddir, MAX_QPATH);
		bUseCustomDirectory = (strlen(snddir) == 10 && !Q_stricmpn(snddir, "jaden_", 6));
#endif
		if (!bUseCustomDirectory)
		{
			Q_strncpyz(snddir, "jaden_fmle", MAX_QPATH);
		}

		//Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "bUseCustomDirectory: %d snddir: %s\n", (int)bUseCustomDirectory, snddir);

		if (bUseCustomDirectory || g_sex->string[0] == 'f')
		{
			char* start = strstr(string, "jaden_male/");

			qboolean bChanged = qfalse;
			if (start != NULL) {
				strncpy(start, snddir, 10);
				bChanged = qtrue;
			}
			if (qDoBoth && g_sex->string[0] == 'f')
			{
				start = strrchr(string, '/');		//get the last slash before the wav
				if (start != NULL) {
					if (!strncmp(start, "/mr_", 4)) {
						if (qDoBoth) {	//we want to change mr to ms
							start[2] = 's';	//change mr to ms
							return qtrue;
						}
						else {	//IF qDoBoth
							return bChanged;	//don't want this one
						}
					}
				}	//IF found slash
			}
			return bChanged;
		}	//IF Female
		else if (!bUseCustomDirectory)
		{
			//i'm male
			start = strrchr(string, '/');		//get the last slash before the wav
			if (start != NULL) {
				if (!strncmp(start, "/ms_", 4)) {
					return qfalse;	//don't want this one
				}
			}	//IF found slash
		}
	}	//if VALIDSTRING
	return qtrue;
}


/*
=============
Q3_SetAngles

Sets the angles of an entity directly
=============
*/
static void Q3_SetDYaw(int entID, float data);

static void Q3_SetAngles(const int entID, vec3_t angles)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAngles: bad ent %d\n", entID);
		return;
	}

	if (ent->client)
	{
		SetClientViewAngle(ent, angles);
		if (ent->NPC)
		{
			Q3_SetDYaw(entID, angles[YAW]);
		}
	}
	else
	{
		VectorCopy(angles, ent->s.angles);
		VectorCopy(angles, ent->s.apos.trBase);
		VectorCopy(angles, ent->currentAngles);
	}
	gi.linkentity(ent);
}

/*
=============
Q3_SetOrigin

Sets the origin of an entity directly
=============
*/
static void Q3_SetOrigin(const int entID, vec3_t origin)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetOrigin: bad ent %d\n", entID);
		return;
	}

	gi.unlinkentity(ent);

	if (ent->client)
	{
		VectorCopy(origin, ent->client->ps.origin);
		VectorCopy(origin, ent->currentOrigin);
		ent->client->ps.origin[2] += 1;

		VectorClear(ent->client->ps.velocity);
		ent->client->ps.pm_time = 160; // hold time
		ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;

		ent->client->ps.eFlags ^= EF_TELEPORT_BIT;

		//		G_KillBox (ent);
	}
	else
	{
		G_SetOrigin(ent, origin);
	}

	gi.linkentity(ent);
}

/*
============
MoveOwner
  Description	:
  Return type	: void
  Argument		: gentity_t *self
============
*/
void MoveOwner(gentity_t* self)
{
	self->nextthink = level.time + FRAMETIME;
	self->e_ThinkFunc = thinkF_G_FreeEntity;

	if (!self->owner || !self->owner->inuse)
	{
		return;
	}

	if (SpotWouldTelefrag2(self->owner, self->currentOrigin))
	{
		self->e_ThinkFunc = thinkF_MoveOwner;
	}
	else
	{
		G_SetOrigin(self->owner, self->currentOrigin);
		gi.linkentity(self->owner);
		Q3_TaskIDComplete(self->owner, TID_MOVE_NAV);
	}
}

/*
=============
Q3_SetTeleportDest

Copies passed origin to ent running script once there is nothing there blocking the spot
=============
*/
static qboolean Q3_SetTeleportDest(const int entID, vec3_t org)
{
	gentity_t* teleEnt = &g_entities[entID];

	if (teleEnt)
	{
		if (SpotWouldTelefrag2(teleEnt, org))
		{
			gentity_t* teleporter = G_Spawn();

			G_SetOrigin(teleporter, org);
			gi.linkentity(teleporter);
			teleporter->owner = teleEnt;

			teleporter->e_ThinkFunc = thinkF_MoveOwner;
			teleporter->nextthink = level.time + FRAMETIME;

			return qfalse;
		}
		G_SetOrigin(teleEnt, org);
		gi.linkentity(teleEnt);
	}

	return qtrue;
}

/*
=============
Q3_SetCopyOrigin

Copies origin of found ent into ent running script
=============`
*/
static void Q3_SetCopyOrigin(const int entID, const char* name)
{
	gentity_t* found = G_Find(nullptr, FOFS(targetname), name);

	if (found)
	{
		Q3_SetOrigin(entID, found->currentOrigin);
		SetClientViewAngle(&g_entities[entID], found->s.angles);
	}
	else
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetCopyOrigin: ent %s not found!\n", name);
	}
}

/*
=============
Q3_SetVelocity

Set the velocity of an entity directly
=============
*/
static void Q3_SetVelocity(const int entID, const int axis, const float speed)
{
	const gentity_t* found = &g_entities[entID];
	//FIXME: Not supported
	if (!found)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetVelocity invalid entID %d\n", entID);
		return;
	}

	if (!found->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetVelocity: not a client %d\n", entID);
		return;
	}

	//FIXME: add or set?
	found->client->ps.velocity[axis] += speed;

	found->client->ps.pm_time = 500;
	found->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
}

/*
============
Q3_SetAdjustAreaPortals
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean shields
============
*/
static void Q3_SetAdjustAreaPortals(const int entID, const qboolean adjust)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAdjustAreaPortals: invalid entID %d\n", entID);
		return;
	}

	ent->svFlags = adjust
		? ent->svFlags | SVF_MOVER_ADJ_AREA_PORTALS
		: ent->svFlags & ~SVF_MOVER_ADJ_AREA_PORTALS;
}

/*
============
Q3_SetDmgByHeavyWeapOnly
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean dmg
============
*/
static void Q3_SetDmgByHeavyWeapOnly(const int entID, const qboolean dmg)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDmgByHeavyWeapOnly: invalid entID %d\n", entID);
		return;
	}

	ent->flags = dmg ? ent->flags | FL_DMG_BY_HEAVY_WEAP_ONLY : ent->flags & ~FL_DMG_BY_HEAVY_WEAP_ONLY;
}

/*
============
Q3_SetShielded
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean dmg
============
*/
static void Q3_SetShielded(const int entID, const qboolean dmg)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetShielded: invalid entID %d\n", entID);
		return;
	}

	ent->flags = dmg ? ent->flags | FL_SHIELDED : ent->flags & ~FL_SHIELDED;
}

/*
============
Q3_SetNoGroups
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean dmg
============
*/
static void Q3_SetNoGroups(const int entID, const qboolean noGroups)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoGroups: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoGroups: ent %s is not an NPC!\n",
			ent->targetname);
		return;
	}

	ent->NPC->scriptFlags = noGroups
		? ent->NPC->scriptFlags | SCF_NO_GROUPS
		: ent->NPC->scriptFlags & ~SCF_NO_GROUPS;
}

/*
=============
moverCallback

Utility function
=============
*/
extern void misc_model_breakable_gravity_init(gentity_t* ent, qboolean drop_to_floor);

void moverCallback(gentity_t* ent)
{
	//complete the task
	Q3_TaskIDComplete(ent, TID_MOVE_NAV);

	// play sound
	ent->s.loopSound = 0; //stop looping sound
	G_PlayDoorSound(ent, BMS_END); //play end sound

	if (ent->moverState == MOVER_1TO2)
	{
		//reached open
		// reached pos2
		MatchTeam(ent, MOVER_POS2, level.time);
		//SetMoverState( ent, MOVER_POS2, level.time );
	}
	else if (ent->moverState == MOVER_2TO1)
	{
		//reached closed
		MatchTeam(ent, MOVER_POS1, level.time);
		//SetMoverState( ent, MOVER_POS1, level.time );
		//close the portal
		if (ent->svFlags & SVF_MOVER_ADJ_AREA_PORTALS)
		{
			gi.AdjustAreaPortalState(ent, qfalse);
		}
	}

	if (ent->e_BlockedFunc == blockedF_Blocked_Mover)
	{
		ent->e_BlockedFunc = blockedF_NULL;
	}

	if (!Q_stricmp("misc_model_breakable", ent->classname) && ent->physicsBounce)
	{
		//a gravity-affected model
		misc_model_breakable_gravity_init(ent, qfalse);
	}
}

/*
=============
anglerCallback

Utility function
=============
*/
void anglerCallback(gentity_t* ent)
{
	//Complete the task
	Q3_TaskIDComplete(ent, TID_ANGLE_FACE);

	// play sound
	ent->s.loopSound = 0; //stop looping sound
	G_PlayDoorSound(ent, BMS_END); //play end sound

	//Set the currentAngles, clear all movement
	VectorMA(ent->s.apos.trBase, ent->s.apos.trDuration * 0.001f, ent->s.apos.trDelta, ent->currentAngles);
	VectorCopy(ent->currentAngles, ent->s.apos.trBase);
	VectorClear(ent->s.apos.trDelta);
	ent->s.apos.trDuration = 1;
	ent->s.apos.trType = TR_STATIONARY;
	ent->s.apos.trTime = level.time;

	//Stop thinking
	ent->e_ReachedFunc = reachedF_NULL;
	if (ent->e_ThinkFunc == thinkF_anglerCallback)
	{
		ent->e_ThinkFunc = thinkF_NULL;
	}

	//link
	gi.linkentity(ent);
}

/*
=============
moveAndRotateCallback

Utility function
=============
*/
void moveAndRotateCallback(gentity_t* ent)
{
	//stop turning
	anglerCallback(ent);
	//stop moving
	moverCallback(ent);
}

void Blocked_Mover(gentity_t* ent, gentity_t* other)
{
	// remove anything other than a client -- no longer the case

	// don't remove security keys or goodie keys
	if (other->s.eType == ET_ITEM && (other->item->giTag >= INV_GOODIE_KEY && other->item->giTag <= INV_SECURITY_KEY))
	{
		// should we be doing anything special if a key blocks it... move it somehow..?
	}
	// if your not a client, or your a dead client remove yourself...
	else if (other->s.number && (!other->client || other->client && other->health <= 0 && other->contents ==
		CONTENTS_CORPSE && !other->message))
	{
		if (!IIcarusInterface::GetIcarus()->IsRunning(other->m_iIcarusID)
			/*!other->taskManager || !other->taskManager->IsRunning()*/)
		{
			// if an item or weapon can we do a little explosion..?
			G_FreeEntity(other);
			return;
		}
	}

	if (ent->damage)
	{
		G_Damage(other, ent, ent, nullptr, nullptr, ent->damage, 0, MOD_CRUSH);
	}
}

/*
=============
Q3_Lerp2Start

Lerps the origin of an entity to its starting position
=============
// NEEDED???
*/
/*static void Q3_Lerp2Start( int entID, int taskID, float duration )
{
	gentity_t	*ent = &g_entities[entID];

	if(!ent)
	{
		Quake3Game()->DebugPrint( WL_WARNING, "Q3_Lerp2Start: invalid entID %d\n", entID);
		return;
	}

	if ( ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0 )
	{
		Quake3Game()->DebugPrint( WL_ERROR, "Q3_Lerp2Start: ent %d is NOT a mover!\n", entID);
		return;
	}

	if ( ent->s.eType != ET_MOVER )
	{
		ent->s.eType = ET_MOVER;
	}

	//FIXME: set up correctly!!!
	ent->moverState = MOVER_2TO1;
	ent->s.eType = ET_MOVER;
	ent->e_ReachedFunc = reachedF_moverCallback;		//Callsback the the completion of the move
	if ( ent->damage )
	{
		ent->e_BlockedFunc = blockedF_Blocked_Mover;
	}

	ent->s.pos.trDuration = duration * 10;	//In seconds
	ent->s.pos.trTime = level.time;

	Q3_TaskIDSet( ent, TID_MOVE_NAV, taskID );
	// starting sound
	G_PlayDoorLoopSound( ent );
	G_PlayDoorSound( ent, BMS_START );	//??

	gi.linkentity( ent );
}*/

/*
=============
Q3_Lerp2End

Lerps the origin of an entity to its ending position
=============
// NEEDED???
*/
/*static void Q3_Lerp2End( int entID, int taskID, float duration )
{
	gentity_t	*ent = &g_entities[entID];

	if(!ent)
	{
		Quake3Game()->DebugPrint( WL_WARNING, "Q3_Lerp2End: invalid entID %d\n", entID);
		return;
	}

	if ( ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0 )
	{
		Quake3Game()->DebugPrint( WL_ERROR, "Q3_Lerp2End: ent %d is NOT a mover!\n", entID);
		return;
	}

	if ( ent->s.eType != ET_MOVER )
	{
		ent->s.eType = ET_MOVER;
	}

	if ( ent->moverState == MOVER_POS1 )
	{//open the portal
		if ( ent->svFlags & SVF_MOVER_ADJ_AREA_PORTALS )
		{
			gi.AdjustAreaPortalState( ent, qtrue );
		}
	}

	//FIXME: set up correctly!!!
	ent->moverState = MOVER_1TO2;
	ent->s.eType = ET_MOVER;
	ent->e_ReachedFunc = reachedF_moverCallback;		//Callsback the the completion of the move
	if ( ent->damage )
	{
		ent->e_BlockedFunc = blockedF_Blocked_Mover;
	}

	ent->s.pos.trDuration = duration * 10;	//In seconds
	ent->s.time = level.time;

	Q3_TaskIDSet( ent, TID_MOVE_NAV, taskID );
	// starting sound
	G_PlayDoorLoopSound( ent );
	G_PlayDoorSound( ent, BMS_START );	//??

	gi.linkentity( ent );
}*/

/*
=============
Q3_Lerp2Pos

Lerps the origin and angles of an entity to the destination values

=============
*/
/*static void Q3_Lerp2Pos( int taskID, int entID, vec3_t origin, vec3_t angles, float duration )
{
	gentity_t	*ent = &g_entities[entID];
	vec3_t		ang;
	int			i;

	if(!ent)
	{
		Quake3Game()->DebugPrint( WL_WARNING, "Q3_Lerp2Pos: invalid entID %d\n", entID);
		return;
	}

	if ( ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0 )
	{
		Quake3Game()->DebugPrint( WL_ERROR, "Q3_Lerp2Pos: ent %d is NOT a mover!\n", entID);
		return;
	}

	if ( ent->s.eType != ET_MOVER )
	{
		ent->s.eType = ET_MOVER;
	}

	//Don't allow a zero duration
	if ( duration == 0 )
		duration = 1;

	//
	// Movement

	moverState_t moverState = ent->moverState;

	if ( moverState == MOVER_POS1 || moverState == MOVER_2TO1 )
	{
		VectorCopy( ent->currentOrigin, ent->pos1 );
		VectorCopy( origin, ent->pos2 );

		if ( moverState == MOVER_POS1 )
		{//open the portal
			if ( ent->svFlags & SVF_MOVER_ADJ_AREA_PORTALS )
			{
				gi.AdjustAreaPortalState( ent, qtrue );
			}
		}

		moverState = MOVER_1TO2;
	}
	else / *if ( moverState == MOVER_POS2 || moverState == MOVER_1TO2 )*/
	/*	{
			VectorCopy( ent->currentOrigin, ent->pos2 );
			VectorCopy( origin, ent->pos1 );

			moverState = MOVER_2TO1;
		}

		InitMoverTrData( ent );

		ent->s.pos.trDuration = duration;

		// start it going
		MatchTeam( ent, moverState, level.time );
		//SetMoverState( ent, moverState, level.time );

		//Only do the angles if specified
		if ( angles != NULL )
		{
			//
			// Rotation

			for ( i = 0; i < 3; i++ )
			{
				ang[i] = AngleDelta( angles[i], ent->currentAngles[i] );
				ent->s.apos.trDelta[i] = ( ang[i] / ( duration * 0.001f ) );
			}

			VectorCopy( ent->currentAngles, ent->s.apos.trBase );

			if ( ent->alt_fire )
			{
				ent->s.apos.trType = TR_LINEAR_STOP;
			}
			else
			{
				ent->s.apos.trType = TR_NONLINEAR_STOP;
			}
			ent->s.apos.trDuration = duration;

			ent->s.apos.trTime = level.time;

			ent->e_ReachedFunc = reachedF_moveAndRotateCallback;
			Q3_TaskIDSet( ent, TID_ANGLE_FACE, taskID );
		}
		else
		{
			//Setup the last bits of information
			ent->e_ReachedFunc  = reachedF_moverCallback;
		}

		if ( ent->damage )
		{
			ent->e_BlockedFunc = blockedF_Blocked_Mover;
		}

		Q3_TaskIDSet( ent, TID_MOVE_NAV, taskID );
		// starting sound
		G_PlayDoorLoopSound( ent );
		G_PlayDoorSound( ent, BMS_START );	//??

		gi.linkentity( ent );
	}*/

	/*
	=============
	Q3_Lerp2Origin

	Lerps the origin to the destination value
	=============
	*/
void Q3_Lerp2Origin(const int taskID, const int entID, vec3_t origin, const float duration)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_Lerp2Origin: invalid entID %d\n", entID);
		return;
	}

	if (ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_Lerp2Origin: ent %d is NOT a mover!\n", entID);
		return;
	}

	if (ent->s.eType != ET_MOVER)
	{
		ent->s.eType = ET_MOVER;
	}

	moverState_t moverState = ent->moverState;

	if (moverState == MOVER_POS1 || moverState == MOVER_2TO1)
	{
		VectorCopy(ent->currentOrigin, ent->pos1);
		VectorCopy(origin, ent->pos2);

		if (moverState == MOVER_POS1)
		{
			//open the portal
			if (ent->svFlags & SVF_MOVER_ADJ_AREA_PORTALS)
			{
				gi.AdjustAreaPortalState(ent, qtrue);
			}
		}

		moverState = MOVER_1TO2;
	}
	else if (moverState == MOVER_POS2 || moverState == MOVER_1TO2)
	{
		VectorCopy(ent->currentOrigin, ent->pos2);
		VectorCopy(origin, ent->pos1);

		moverState = MOVER_2TO1;
	}

	InitMoverTrData(ent); //FIXME: This will probably break normal things that are being moved...

	ent->s.pos.trDuration = duration;

	// start it going
	MatchTeam(ent, moverState, level.time);
	//SetMoverState( ent, moverState, level.time );

	ent->e_ReachedFunc = reachedF_moverCallback;
	if (ent->damage)
	{
		ent->e_BlockedFunc = blockedF_Blocked_Mover;
	}
	if (taskID != -1)
	{
		Q3_TaskIDSet(ent, TID_MOVE_NAV, taskID);
	}
	// starting sound
	G_PlayDoorLoopSound(ent); //start looping sound
	G_PlayDoorSound(ent, BMS_START); //play start sound

	gi.linkentity(ent);
}

static void Q3_SetOriginOffset(const int entID, const int axis, const float offset)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetOriginOffset: invalid entID %d\n", entID);
		return;
	}

	if (ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetOriginOffset: ent %d is NOT a mover!\n", entID);
		return;
	}

	vec3_t origin;
	VectorCopy(ent->s.origin, origin);
	origin[axis] += offset;
	float duration = 0;
	if (ent->speed)
	{
		duration = fabs(offset) / fabs(ent->speed) * 1000.0f;
	}
	Q3_Lerp2Origin(-1, entID, origin, duration);
}

/*
=============
Q3_LerpAngles

Lerps the angles to the destination value
=============
*/
/*static void Q3_Lerp2Angles( int taskID, int entID, vec3_t angles, float duration )
{
	gentity_t	*ent = &g_entities[entID];
	vec3_t		ang;
	int			i;

	if(!ent)
	{
		Quake3Game()->DebugPrint( WL_WARNING, "Q3_Lerp2Angles: invalid entID %d\n", entID);
		return;
	}

	if ( ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0 )
	{
		Quake3Game()->DebugPrint( WL_ERROR, "Q3_Lerp2Angles: ent %d is NOT a mover!\n", entID);
		return;
	}

	//If we want an instant move, don't send 0...
	ent->s.apos.trDuration = (duration>0) ? duration : 1;

	for ( i = 0; i < 3; i++ )
	{
		ang [i] = AngleSubtract( angles[i], ent->currentAngles[i]);
		ent->s.apos.trDelta[i] = ( ang[i] / ( ent->s.apos.trDuration * 0.001f ) );
	}

	VectorCopy( ent->currentAngles, ent->s.apos.trBase );

	if ( ent->alt_fire )
	{
		ent->s.apos.trType = TR_LINEAR_STOP;
	}
	else
	{
		ent->s.apos.trType = TR_NONLINEAR_STOP;
	}

	ent->s.apos.trTime = level.time;

	Q3_TaskIDSet( ent, TID_ANGLE_FACE, taskID );

	//ent->e_ReachedFunc = reachedF_NULL;
	ent->e_ThinkFunc = thinkF_anglerCallback;
	ent->nextthink = level.time + duration;

	gi.linkentity( ent );
}*/

/*
=============
Q3_GetTag

Gets the value of a tag by the give name
=============
*/
/*static int	Q3_GetTag( int entID, const char *name, int lookup, vec3_t info )
{
	gentity_t	*ent = &g_entities[ entID ];

	VALIDATEB( ent );

	switch ( lookup )
	{
	case TYPE_ORIGIN:
		//return TAG_GetOrigin( ent->targetname, name, info );
		return TAG_GetOrigin( ent->ownername, name, info );
		break;

	case TYPE_ANGLES:
		//return TAG_GetAngles( ent->targetname, name, info );
		return TAG_GetAngles( ent->ownername, name, info );
		break;
	}

	return false;
}*/

/*
=============
Q3_SetNavGoal

Sets the navigational goal of an entity
=============
*/
static qboolean Q3_SetNavGoal(const int entID, const char* name)
{
	gentity_t* ent = &g_entities[entID];
	vec3_t goalPos;

	if (!ent->health)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
			"Q3_SetNavGoal: tried to set a navgoal (\"%s\") on a corpse! \"%s\"\n", name,
			ent->script_targetname);
		return qfalse;
	}
	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
			"Q3_SetNavGoal: tried to set a navgoal (\"%s\") on a non-NPC: \"%s\"\n", name,
			ent->script_targetname);
		return qfalse;
	}
	if (!ent->NPC->tempGoal)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
			"Q3_SetNavGoal: tried to set a navgoal (\"%s\") on a dead NPC: \"%s\"\n", name,
			ent->script_targetname);
		return qfalse;
	}
	if (!ent->NPC->tempGoal->inuse)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNavGoal: NPC's (\"%s\") navgoal is freed: \"%s\"\n",
			name, ent->script_targetname);
		return qfalse;
	}
	if (Q_stricmp("null", name) == 0
		|| Q_stricmp("NULL", name) == 0)
	{
		ent->NPC->goalEntity = nullptr;
		Q3_TaskIDComplete(ent, TID_MOVE_NAV);
		return qfalse;
	}
	//Get the position of the goal
	if (TAG_GetOrigin2(nullptr, name, goalPos) == false)
	{
		gentity_t* targ = G_Find(nullptr, FOFS(targetname), name);
		if (!targ)
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNavGoal: can't find NAVGOAL \"%s\"\n", name);
			return qfalse;
		}
		ent->NPC->goalEntity = targ;
		ent->NPC->goalRadius = sqrt(ent->maxs[0] + ent->maxs[0]) + sqrt(targ->maxs[0] + targ->maxs[0]);
		ent->NPC->aiFlags &= ~NPCAI_TOUCHED_GOAL;
	}
	else
	{
		const int goalRadius = TAG_GetRadius(nullptr, name);
		NPC_SetMoveGoal(ent, goalPos, goalRadius, qtrue);
		//We know we want to clear the lastWaypoint here
		ent->NPC->goalEntity->lastWaypoint = WAYPOINT_NONE;
		ent->NPC->aiFlags &= ~NPCAI_TOUCHED_GOAL;
#ifdef _DEBUG
		//this is *only* for debugging navigation
		ent->NPC->tempGoal->target = G_NewString(name);
#endif// _DEBUG
		return qtrue;
	}
	return qfalse;
}

//-----------------------------------------------

/*
============
SetLowerAnim
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int animID
============
*/
static void SetLowerAnim(const int entID, const int animID)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "SetLowerAnim: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "SetLowerAnim: ent %d is NOT a player or NPC!\n", entID);
		return;
	}

	NPC_SetAnim(ent, SETANIM_LEGS, animID, SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
}

/*
============
SetUpperAnim
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int animID
============
*/
static void SetUpperAnim(const int entID, const int animID)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "SetUpperAnim: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "SetLowerAnim: ent %d is NOT a player or NPC!\n", entID);
		return;
	}

	NPC_SetAnim(ent, SETANIM_TORSO, animID, SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
}

//-----------------------------------------------

/*
=============
Q3_SetAnimUpper

Sets the upper animation of an entity
=============
*/
qboolean Q3_SetAnimUpper(const int entID, const char* anim_name)
{
	const int animID = GetIDForString(anim_table, anim_name);

	if (animID == -1)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAnimUpper: unknown animation sequence '%s'\n",
			anim_name);
		return qfalse;
	}

	if (!PM_HasAnimation(&g_entities[entID], animID))
	{
		return qfalse;
	}

	SetUpperAnim(entID, animID);
	return qtrue;
}

/*
=============
Q3_SetAnimLower

Sets the lower animation of an entity
=============
*/
qboolean Q3_SetAnimLower(const int entID, const char* anim_name)
{
	//FIXME: Setting duck anim does not actually duck!

	const int animID = GetIDForString(anim_table, anim_name);

	if (animID == -1)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAnimLower: unknown animation sequence '%s'\n",
			anim_name);
		return qfalse;
	}

	if (!PM_HasAnimation(&g_entities[entID], animID))
	{
		return qfalse;
	}

	SetLowerAnim(entID, animID);
	return qtrue;
}

/*
============
Q3_SetAnimHoldTime
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int int_data
  Argument		: qboolean lower
============
*/
extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torso_anim_timer, int time);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legs_anim_timer, int time);

void Q3_SetAnimHoldTime(const int entID, const int int_data, const qboolean lower)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAnimHoldTime: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetAnimHoldTime: ent %d is NOT a player or NPC!\n",
			entID);
		return;
	}

	if (lower)
	{
		PM_SetLegsAnimTimer(ent, &ent->client->ps.legsAnimTimer, int_data);
	}
	else
	{
		PM_SetTorsoAnimTimer(ent, &ent->client->ps.torsoAnimTimer, int_data);
	}
}

/*
=============
Q3_SetEnemy

Sets the enemy of an entity
=============
*/
static void Q3_SetEnemy(const int entID, const char* name)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEnemy: invalid entID %d\n", entID);
		return;
	}

	if (!Q_stricmp("NONE", name) || !Q_stricmp("NULL", name))
	{
		if (ent->NPC)
		{
			G_ClearEnemy(ent);
		}
		else
		{
			ent->enemy = nullptr;
		}
	}
	else
	{
		gentity_t* enemy = G_Find(nullptr, FOFS(targetname), name);

		if (enemy == nullptr)
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetEnemy: no such enemy: '%s'\n", name);
			return;
		}
		/*else if(enemy->health <= 0)
		{
			//Quake3Game()->DebugPrint( WL_ERROR, "Q3_SetEnemy: ERROR - desired enemy has health %d\n", enemy->health );
			return;
		}*/
		if (ent->NPC)
		{
			G_SetEnemy(ent, enemy);
			ent->cantHitEnemyCounter = 0;
		}
		else
		{
			G_SetEnemy(ent, enemy);
		}
	}
}

/*
=============
Q3_SetLeader

Sets the leader of an NPC
=============
*/
static void Q3_SetLeader(const int entID, const char* name)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLeader: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetLeader: ent %d is NOT a player or NPC!\n", entID);
		return;
	}

	if (!Q_stricmp("NONE", name) || !Q_stricmp("NULL", name))
	{
		ent->client->leader = nullptr;
	}
	else
	{
		gentity_t* leader = G_Find(nullptr, FOFS(targetname), name);

		if (leader == nullptr)
		{
			//Quake3Game()->DebugPrint( WL_ERROR,"Q3_SetEnemy: unable to locate enemy: '%s'\n", name );
			return;
		}
		if (leader->health <= 0)
		{
			//Quake3Game()->DebugPrint( WL_ERROR,"Q3_SetEnemy: ERROR - desired enemy has health %d\n", enemy->health );
			return;
		}
		ent->client->leader = leader;
	}
}

stringID_table_t teamTable[] =
{
	ENUM2STRING(TEAM_FREE),
	ENUM2STRING(TEAM_PLAYER),
	ENUM2STRING(TEAM_ENEMY),
	ENUM2STRING(TEAM_NEUTRAL),
	ENUM2STRING(TEAM_SOLO),
	ENUM2STRING(TEAM_PROJECTION),
	{"", TEAM_FREE},
};

/*
============
Q3_SetPlayerTeam
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: const char *teamName
============
*/
static void Q3_SetPlayerTeam(const int entID, const char* teamName)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetPlayerTeam: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetPlayerTeam: ent %d is NOT a player or NPC!\n", entID);
		return;
	}

	const auto newTeam = static_cast<team_t>(GetIDForString(teamTable, teamName));
	ent->client->playerTeam = newTeam;
}

/*
============
Q3_SetEnemyTeam
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: const char *teamName
============
*/
static void Q3_SetEnemyTeam(const int entID, const char* teamName)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEnemyTeam: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetEnemyTeam: ent %d is NOT a player or NPC!\n", entID);
		return;
	}

	ent->client->enemyTeam = static_cast<team_t>(GetIDForString(teamTable, teamName));
}

/*
============
Q3_SetHealth
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int data
============
*/
void Q3_SetHealth(const int entID, int data)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetHealth: invalid entID %d\n", entID);
		return;
	}

	// FIXME : should we really let you set health on a dead guy?
	// this close to gold I won't change it, but warn you about it
	if (ent->health <= 0)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetHealth: trying to set health on a dead guy! %d\n",
			entID);
	}

	if (data < 0)
	{
		data = 0;
	}

	ent->health = data;

	// should adjust max if new health is higher than max
	if (ent->health > ent->max_health)
	{
		ent->max_health = ent->health;
	}

	if (!ent->client)
	{
		return;
	}

	ent->client->ps.stats[STAT_HEALTH] = data;
	if (ent->s.number == 0)
	{
		//clamp health to max
		if (ent->client->ps.stats[STAT_HEALTH] > ent->client->ps.stats[STAT_MAX_HEALTH])
		{
			ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
		if (data == 0)
		{
			//artificially "killing" the player", don't let him respawn right away
			ent->client->ps.pm_type = PM_DEAD;
			//delay respawn for 2 seconds
			ent->client->respawnTime = level.time + 2000;
			//stop all scripts
			stop_icarus = qtrue;
		}
	}
}

/*
============
Q3_SetArmor
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int data
============
*/
void Q3_SetArmor(const int entID, const int data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetArmor: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		return;
	}

	ent->client->ps.stats[STAT_ARMOR] = data;
	if (ent->s.number == 0)
	{
		//clamp armor to max_health
		if (ent->client->ps.stats[STAT_ARMOR] > ent->client->ps.stats[STAT_MAX_HEALTH])
		{
			ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
	}
}

/*
============
Q3_SetBState
  Description	:
  Return type	: static qboolean
  Argument		:  int entID
  Argument		: const char *bs_name
FIXME: this should be a general NPC wrapper function
	that is called ANY time	a b_state is changed...
============
*/
static qboolean Q3_SetBState(const int entID, const char* bs_name)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetBState: invalid entID %d\n", entID);
		return qtrue;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetBState: '%s' is not an NPC\n", ent->targetname);
		return qtrue; //ok to complete
	}

	const auto bSID = static_cast<bState_t>(GetIDForString(BSTable, bs_name));
	if (bSID != static_cast<bState_t>(-1))
	{
		if (bSID == BS_SEARCH || bSID == BS_WANDER)
		{
			//FIXME: Reimplement

			if (ent->waypoint != WAYPOINT_NONE)
			{
				NPC_BSSearchStart(ent->waypoint, bSID);
			}
			else
			{
				ent->waypoint = NAV::GetNearestNode(ent);

				if (ent->waypoint != WAYPOINT_NONE)
				{
					NPC_BSSearchStart(ent->waypoint, bSID);
				}
				else
				{
					Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
						"Q3_SetBState: '%s' is not in a valid waypoint to search from!\n",
						ent->targetname);
					return qtrue;
				}
			}
		}

		ent->NPC->tempBehavior = BS_DEFAULT; //need to clear any temp behaviour
		if (ent->NPC->behaviorState == BS_NOCLIP && bSID != BS_NOCLIP)
		{
			//need to rise up out of the floor after noclipping
			ent->currentOrigin[2] += 0.125;
			G_SetOrigin(ent, ent->currentOrigin);
			gi.linkentity(ent);
		}
		ent->NPC->behaviorState = bSID;
		if (bSID == BS_DEFAULT)
		{
			ent->NPC->defaultBehavior = bSID;
		}
	}

	ent->NPC->aiFlags &= ~NPCAI_TOUCHED_GOAL;

	if (bSID == BS_NOCLIP)
	{
		ent->client->noclip = true;
	}
	else
	{
		ent->client->noclip = false;
	}

	if (bSID == BS_ADVANCE_FIGHT)
	{
		return qfalse; //need to wait for task complete message
	}

	if (bSID == BS_JUMP)
	{
		ent->NPC->jumpState = JS_FACING;
	}

	return qtrue; //ok to complete
}

/*
============
Q3_SetTempBState
  Description	:
  Return type	: static qboolean
  Argument		:  int entID
  Argument		: const char *bs_name
============
*/
static qboolean Q3_SetTempBState(const int entID, const char* bs_name)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetTempBState: invalid entID %d\n", entID);
		return qtrue;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetTempBState: '%s' is not an NPC\n", ent->targetname);
		return qtrue; //ok to complete
	}

	const auto bSID = static_cast<bState_t>(GetIDForString(BSTable, bs_name));
	if (bSID != static_cast<bState_t>(-1))
	{
		ent->NPC->tempBehavior = bSID;
	}

	/*
		if ( bSID == BS_FACE || bSID == BS_POINT_AND_SHOOT || bSID == BS_FACE_ENEMY )
		{
			ent->NPC->aimTime = level.time + 5 * 1000;//try for 5 seconds
			return qfalse;//need to wait for task complete message
		}
	*/

	/*
		if ( bSID == BS_SHOOT || bSID == BS_POINT_AND_SHOOT )
		{//Let them shoot right NOW
			ent->NPC->shotTime = ent->attackDebounceTime = level.time;
		}
	*/
	return qtrue; //ok to complete
}

/*
============
Q3_SetDefaultBState
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: const char *bs_name
============
*/
static void Q3_SetDefaultBState(const int entID, const char* bs_name)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDefaultBState: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetDefaultBState: '%s' is not an NPC\n",
			ent->targetname);
		return;
	}

	const auto bSID = static_cast<bState_t>(GetIDForString(BSTable, bs_name));
	if (bSID != static_cast<bState_t>(-1))
	{
		ent->NPC->defaultBehavior = bSID;
	}
}

/*
============
Q3_SetDPitch
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetDPitch(const int entID, float data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDPitch: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC || !ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetDPitch: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	const int pitchMin = -ent->client->renderInfo.headPitchRangeUp + 1;
	const int pitchMax = ent->client->renderInfo.headPitchRangeDown - 1;

	//clamp angle to -180 -> 180
	data = AngleNormalize180(data);

	//Clamp it to my valid range
	if (data < -1)
	{
		if (data < pitchMin)
		{
			data = pitchMin;
		}
	}
	else if (data > 1)
	{
		if (data > pitchMax)
		{
			data = pitchMax;
		}
	}

	ent->NPC->lockedDesiredPitch = ent->NPC->desiredPitch = data;
}

/*
============
Q3_SetDYaw
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetDYaw(const int entID, const float data)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDYaw: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetDYaw: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	if (!ent->enemy)
	{
		//don't mess with this if they're aiming at someone
		ent->NPC->lockedDesiredYaw = ent->NPC->desiredYaw = ent->s.angles[1] = data;
	}
	else
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Could not set DYAW: '%s' has an enemy (%s)!\n",
			ent->targetname, ent->enemy->targetname);
	}
}

/*
============
Q3_SetShootDist
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetShootDist(const int entID, const float data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetShootDist: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetShootDist: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->NPC->stats.shootDistance = data;
}

/*
============
Q3_SetVisrange
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetVisrange(const int entID, const float data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetVisrange: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetVisrange: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->NPC->stats.visrange = data;
}

/*
============
Q3_SetEarshot
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetEarshot(const int entID, const float data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEarshot: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetEarshot: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->NPC->stats.earshot = data;
}

/*
============
Q3_SetVigilance
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetVigilance(const int entID, const float data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetVigilance: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetVigilance: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->NPC->stats.vigilance = data;
}

/*
============
Q3_SetVFOV
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int data
============
*/
static void Q3_SetVFOV(const int entID, const int data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetVFOV: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetVFOV: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->NPC->stats.vfov = data;
}

/*
============
Q3_SetHFOV
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int data
============
*/
static void Q3_SetHFOV(const int entID, const int data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetHFOV: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetHFOV: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->NPC->stats.hfov = data;
}

/*
============
Q3_SetWidth
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: float data
============
*/
static void Q3_SetWidth(const int entID, const int data)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWidth: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWidth: '%s' is not an NPC\n", ent->targetname);
		return;
	}

	ent->maxs[0] = ent->maxs[1] = data;
	ent->mins[0] = ent->mins[1] = -data;
}

/*
============
Q3_GetTimeScale
  Description	:
  Return type	: static DWORD
  Argument		: void
============
*/
// NEEDED???
/*static DWORD Q3_GetTimeScale( void )
{
	//return	Q3_TIME_SCALE;
	return g_timescale->value;
}*/

/*
============
Q3_SetTimeScale
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: const char *data
============
*/
static void Q3_SetTimeScale(int entID, const char* data)
{
	// if we're skipping the script DO NOT allow timescale to be set (skipping needs it at 100)
	if (g_skippingcin->integer)
	{
		return;
	}

	gi.cvar_set("timescale", data);
}

/*
============
Q3_SetInvisible
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: qboolean invisible
============
*/
static void Q3_SetInvisible(const int entID, const qboolean invisible)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetInvisible: invalid entID %d\n", entID);
		return;
	}

	if (invisible)
	{
		self->s.eFlags |= EF_NODRAW;
		if (self->client)
		{
			self->client->ps.eFlags |= EF_NODRAW;
		}
		self->contents = 0;
	}
	else
	{
		self->s.eFlags &= ~EF_NODRAW;
		if (self->client)
		{
			self->client->ps.eFlags &= ~EF_NODRAW;
		}
	}
}

/*
============
Q3_SetVampire
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: qboolean vampire
============
*/
static void Q3_SetVampire(const int entID, const qboolean vampire)
{
	const gentity_t* self = &g_entities[entID];

	if (!self || !self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetVampire: entID %d not a client\n", entID);
		return;
	}

	if (vampire)
	{
		self->client->ps.powerups[PW_DISINT_2] = Q3_INFINITE;
	}
	else
	{
		self->client->ps.powerups[PW_DISINT_2] = 0;
	}
}

/*
============
Q3_SetGreetAllies
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: qboolean greet
============
*/
static void Q3_SetGreetAllies(const int entID, const qboolean greet)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetGreetAllies: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetGreetAllies: ent %s is not an NPC!\n",
			self->targetname);
		return;
	}

	if (greet)
	{
		self->NPC->aiFlags |= NPCAI_GREET_ALLIES;
	}
	else
	{
		self->NPC->aiFlags &= ~NPCAI_GREET_ALLIES;
	}
}

/*
============
Q3_SetViewTarget
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *name
============
*/
static void Q3_SetViewTarget(const int entID, const char* name)
{
	const gentity_t* self = &g_entities[entID];
	const gentity_t* viewtarget = G_Find(nullptr, FOFS(targetname), name);
	vec3_t viewspot, selfspot, viewvec, viewangles;

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetViewTarget: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetViewTarget: '%s' is not a player/NPC!\n",
			self->targetname);
		return;
	}

	//FIXME: Exception handle here
	if (viewtarget == nullptr)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetViewTarget: can't find ViewTarget: '%s'\n", name);
		return;
	}

	//FIXME: should we set behavior to BS_FACE and keep facing this ent as it moves
	//around for a script-specified length of time...?
	VectorCopy(self->currentOrigin, selfspot);
	selfspot[2] += self->client->ps.viewheight;

	if (viewtarget->client && (!g_skippingcin || !g_skippingcin->integer))
	{
		VectorCopy(viewtarget->client->renderInfo.eyePoint, viewspot);
	}
	else
	{
		VectorCopy(viewtarget->currentOrigin, viewspot);
	}

	VectorSubtract(viewspot, selfspot, viewvec);

	vectoangles(viewvec, viewangles);

	Q3_SetDYaw(entID, viewangles[YAW]);
	if (!g_skippingcin || !g_skippingcin->integer)
	{
		Q3_SetDPitch(entID, viewangles[PITCH]);
	}
}

/*
============
Q3_SetWatchTarget
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *name
============
*/
static void Q3_SetWatchTarget(const int entID, const char* name)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWatchTarget: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWatchTarget: '%s' is not an NPC!\n",
			self->targetname);
		return;
	}

	if (Q_stricmp("NULL", name) == 0 || Q_stricmp("NONE", name) == 0 || self->targetname && Q_stricmp(
		self->targetname, name) == 0)
	{
		//clearing watchTarget
		self->NPC->watchTarget = nullptr;
	}

	gentity_t* watchTarget = G_Find(nullptr, FOFS(targetname), name);
	if (watchTarget == nullptr)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWatchTarget: can't find WatchTarget: '%s'\n", name);
		return;
	}

	self->NPC->watchTarget = watchTarget;
}

static void Q3_SetLoopSound(const int entID, const char* name)
{
	sfxHandle_t index;
	gentity_t* self = &g_entities[entID];

	if (Q_stricmp("NULL", name) == 0 || Q_stricmp("NONE", name) == 0)
	{
		self->s.loopSound = 0;
		return;
	}

	if (self->s.eType == ET_MOVER)
	{
		index = cgi_S_RegisterSound(name);
	}
	else
	{
		index = G_SoundIndex(name);
	}

	if (index)
	{
		self->s.loopSound = index;
	}
	else
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLoopSound: can't find sound file: '%s'\n", name);
	}
}

static void Q3_SetICARUSFreeze(int entID, const char* name, const qboolean freeze)
{
	gentity_t* self = G_Find(nullptr, FOFS(targetname), name);
	if (!self)
	{
		//hmm, targetname failed, try script_targetname?
		self = G_Find(nullptr, FOFS(script_targetname), name);
	}

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetICARUSFreeze: invalid ent %s\n", name);
		return;
	}

	if (freeze)
	{
		self->svFlags |= SVF_ICARUS_FREEZE;
	}
	else
	{
		self->svFlags &= ~SVF_ICARUS_FREEZE;
	}
}

/*
============
Q3_SetViewEntity
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *name
============
*/
extern qboolean G_ClearViewEntity(gentity_t* ent);
extern void G_SetViewEntity(gentity_t* self, gentity_t* view_entity);

void Q3_SetViewEntity(const int entID, const char* name)
{
	gentity_t* self = &g_entities[entID];
	gentity_t* viewtarget = G_Find(nullptr, FOFS(targetname), name);

	if (entID != 0)
	{
		//only valid on player
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetViewEntity: only valid on player\n", entID);
		return;
	}

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetViewEntity: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetViewEntity: '%s' is not a player!\n",
			self->targetname);
		return;
	}

	if (!name)
	{
		G_ClearViewEntity(self);
		return;
	}

	if (viewtarget == nullptr)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetViewEntity: can't find ViewEntity: '%s'\n", name);
		return;
	}

	G_SetViewEntity(self, viewtarget);
}

/*
============
Q3_SetWeapon
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *wp_name
============
*/
extern gentity_t* TossClientItems(gentity_t* self);

void G_SetWeapon(gentity_t* self, int wp)
{
	qboolean hadWeapon = qfalse;

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWeapon: '%s' is not a player/NPC!\n",
			self->targetname);
		return;
	}

	if (self->NPC)
	{
		//since a script sets a weapon, we presume we don't want to auto-match the player's weapon anymore
		self->NPC->aiFlags &= ~NPCAI_MATCHPLAYERWEAPON;
	}

	if (wp == WP_NONE)
	{
		//no weapon
		self->client->ps.weapon = WP_NONE;
		G_RemoveWeaponModels(self);
		if (self->s.number < MAX_CLIENTS)
		{
			//make sure the cgame-side knows this
			CG_ChangeWeapon(wp);
		}
		return;
	}

	const gitem_t* item = FindItemForWeapon(static_cast<weapon_t>(wp));
	RegisterItem(item); //make sure the weapon is cached in case this runs at startup

	if (self->client->ps.weapons[wp])
	{
		hadWeapon = qtrue;
	}
	if (self->NPC)
	{
		//Should NPCs have only 1 weapon at a time?
		for (char& weapon : self->client->ps.weapons)
		{
			weapon = 0;
		}
		self->client->ps.weapons[wp] = 1;
		self->client->ps.ammo[weaponData[wp].ammoIndex] = 999;

		ChangeWeapon(self, wp);
		self->client->ps.weapon = wp;
		self->client->ps.weaponstate = WEAPON_READY; //WEAPON_RAISING;
		G_AddEvent(self, EV_GENERAL_SOUND, G_SoundIndex("sound/weapons/change.wav"));
	}
	else
	{
		self->client->ps.weapons[wp] = 1;
		self->client->ps.ammo[weaponData[wp].ammoIndex] = ammoData[weaponData[wp].ammoIndex].max;

		G_AddEvent(self, EV_ITEM_PICKUP, item - bg_itemlist);
		//force it to change
		CG_ChangeWeapon(wp);
		G_AddEvent(self, EV_GENERAL_SOUND, G_SoundIndex("sound/weapons/change.wav"));
	}
	G_RemoveWeaponModels(self);

	if (wp == WP_SABER)
	{
		if (!hadWeapon)
		{
			WP_SaberInitBladeData(self);
		}
		WP_SaberAddG2SaberModels(self);
		G_RemoveHolsterModels(self);
	}
	else
	{
		if (com_kotor->integer == 1) //playing kotor
		{
			G_CreateG2AttachedWeaponModel(self, weaponData[wp].altweaponMdl, self->handRBolt, 0);
		}
		else
		{
			if (self->client->charKOTORWeapons == 1)
			{
				G_CreateG2AttachedWeaponModel(self, weaponData[wp].altweaponMdl, self->handRBolt, 0);
			}
			else
			{
				G_CreateG2AttachedWeaponModel(self, weaponData[wp].weaponMdl, self->handRBolt, 0);
			}
		}
		WP_SaberAddHolsteredG2SaberModels(self);
	}
}

static void Q3_SetWeapon(const int entID, const char* wp_name)
{
	gentity_t* self = &g_entities[entID];
	const int wp = GetIDForString(WPTable, wp_name);

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWeapon: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWeapon: '%s' is not a player/NPC!\n",
			self->targetname);
		return;
	}

	if (self->NPC)
	{
		//since a script sets a weapon, we presume we don't want to auto-match the player's weapon anymore
		self->NPC->aiFlags &= ~NPCAI_MATCHPLAYERWEAPON;
	}

	if (!Q_stricmp("drop", wp_name))
	{
		//no weapon, drop it
		TossClientItems(self);
		self->client->ps.weapon = WP_NONE;
		G_RemoveWeaponModels(self);
		return;
	}

	G_SetWeapon(self, wp);
}

/*
============
Q3_SetItem
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *wp_name
============
*/
static void Q3_SetItem(const int entID, const char* item_name)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWeapon: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWeapon: '%s' is not a player/NPC!\n",
			self->targetname);
		return;
	}

	const int inv = GetIDForString(INVTable, item_name);

	const gitem_t* item = FindItemForInventory(inv);
	RegisterItem(item); //make sure the item is cached in case this runs at startup

	//		G_AddEvent( self, EV_ITEM_PICKUP, (item - bg_itemlist) );
	//		G_AddEvent( self, EV_GENERAL_SOUND, G_SoundIndex( "sound/weapons/change.wav" ));

	self->client->ps.stats[STAT_ITEMS] |= 1 << item->giTag;

	if (inv == INV_ELECTROBINOCULARS || inv == INV_LIGHTAMP_GOGGLES)
	{
		self->client->ps.inventory[inv] = 1;
		return;
	}
	// else Bacta, seeker, sentry
	if (self->client->ps.inventory[inv] < 5)
	{
		self->client->ps.inventory[inv]++;
	}
}

/*
============
Q3_SetWalkSpeed
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: int int_data
============
*/
static void Q3_SetWalkSpeed(const int entID, const int int_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWalkSpeed: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWalkSpeed: '%s' is not an NPC!\n", self->targetname);
		return;
	}

	if (int_data == 0)
	{
		self->NPC->stats.walkSpeed = self->client->ps.speed = 1;
	}

	self->NPC->stats.walkSpeed = self->client->ps.speed = int_data;
}

/*
============
Q3_SetRunSpeed
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: int int_data
============
*/
static void Q3_SetRunSpeed(const int entID, const int int_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetRunSpeed: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetRunSpeed: '%s' is not an NPC!\n", self->targetname);
		return;
	}

	if (int_data == 0)
	{
		self->NPC->stats.runSpeed = self->client->ps.speed = 1;
	}

	self->NPC->stats.runSpeed = self->client->ps.speed = int_data;
}

/*
============
Q3_SetYawSpeed
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: float float_data
============
*/
static void Q3_SetYawSpeed(const int entID, const float float_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetYawSpeed: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetYawSpeed: '%s' is not an NPC!\n", self->targetname);
		return;
	}

	self->NPC->stats.yawSpeed = float_data;
}

/*
============
Q3_SetAggression
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: int int_data
============
*/
static void Q3_SetAggression(const int entID, const int int_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAggression: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetAggression: '%s' is not an NPC!\n", self->targetname);
		return;
	}

	if (int_data < 1 || int_data > 5)
		return;

	self->NPC->stats.aggression = int_data;
}

/*
============
Q3_SetAim
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: int int_data
============
*/
static void Q3_SetAim(const int entID, const int int_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAim: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetAim: '%s' is not an NPC!\n", self->targetname);
		return;
	}

	if (int_data < 1 || int_data > 5)
		return;

	self->NPC->stats.aim = int_data;
}

/*
============
Q3_SetFriction
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: int int_data
============
*/
static void Q3_SetFriction(const int entID, const int int_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetFriction: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetFriction: '%s' is not an NPC/player!\n",
			self->targetname);
		return;
	}

	self->client->ps.friction = int_data;
}

/*
============
Q3_SetGravity
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: float float_data
============
*/
void Q3_SetGravity(const int entID, const float float_data)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetGravity: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetGravity: '%s' is not an NPC/player!\n",
			self->targetname);
		return;
	}

	//FIXME: what if we want to return them to normal global gravity?
	self->svFlags |= SVF_CUSTOM_GRAVITY;
	self->client->ps.gravity = float_data;
}

/*
============
Q3_SetWait
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: float float_data
============
*/
static void Q3_SetWait(const int entID, const float float_data)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWait: invalid entID %d\n", entID);
		return;
	}

	self->wait = float_data;
}

static void Q3_SetShotSpacing(const int entID, const int int_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetShotSpacing: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetShotSpacing: '%s' is not an NPC!\n",
			self->targetname);
		return;
	}

	self->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
	self->NPC->burstSpacing = int_data;
}

/*
============
Q3_SetFollowDist
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: float float_data
============
*/
static void Q3_SetFollowDist(const int entID, const float float_data)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetFollowDist: invalid entID %d\n", entID);
		return;
	}

	if (!self->client || !self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetFollowDist: '%s' is not an NPC!\n", self->targetname);
		return;
	}

	self->NPC->followDist = float_data;
}

/*
============
Q3_SetScale
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: float float_data
============
*/
void Q3_SetScale(const int entID, const float float_data)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetScale: invalid entID %d\n", entID);
		return;
	}

	self->s.scale = float_data;
}

static void Q3_Set_NPC_Scale(const int entID, const float float_data)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_Set_NPC_Scale: invalid entID %d\n", entID);
		return;
	}

	self->s.scale = float_data;

	// Adjust the model scale and the collision (mins/maxs) in one pass
	const vec_t prevMins = self->mins[2];
	for (int i = 0; i < 3; i++)
	{
		const float correctScale = self->s.modelScale[i] < 0.001f ? 1.0f : self->s.modelScale[i];
		const float maxNormalizedValue = self->maxs[i] / correctScale;
		const float minNormalizedValue = self->mins[i] / correctScale;
		self->maxs[i] = maxNormalizedValue * float_data;
		self->mins[i] = minNormalizedValue * float_data;
		self->s.modelScale[i] = float_data;
	}
	const vec_t afterMins = self->mins[2];
	const vec_t diff = afterMins - prevMins;

	// Do the below so the NPC doesn't get stuck in the floor
	self->client->ps.origin[2] += fabs(diff * 1.1f);
	self->currentOrigin[2] += fabs(diff);
	self->s.radius = sqrt(self->maxs[0] * self->maxs[0] + self->maxs[1] * self->maxs[1]);
}

/*
============
Q3_SetRenderCullRadius
  Description	: allows NPCs to be drawn even when their origin is very far away from their model
  Return type	: static void
  Argument		: int entID
  Argument		: float float_data (the new radius for render culling)
============
*/
static void Q3_SetRenderCullRadius(const int entID, const float float_data)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetRenderCullRadius: invalid entID %d\n", entID);
		return;
	}

	self->s.radius = float_data;
}

/*
============
Q3_SetCount
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *data
============
*/
static void Q3_SetCount(const int entID, const char* data)
{
	gentity_t* self = &g_entities[entID];
	float val;

	//FIXME: use FOFS() stuff here to make a generic entity field setting?
	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetCount: invalid entID %d\n", entID);
		return;
	}

	if ((val = Q3_CheckStringCounterIncrement(data)))
	{
		self->count += static_cast<int>(val);
	}
	else
	{
		self->count = atoi(data);
	}
}

/*
============
Q3_SetSquadName
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *squadname
============
*/
/*
static void Q3_SetSquadName (int entID, const char *squadname)
{
	gentity_t	*self  = &g_entities[entID];

	if ( !self )
	{
		Quake3Game()->DebugPrint( IGameInterface::WL_WARNING, "Q3_SetSquadName: invalid entID %d\n", entID);
		return;
	}

	if ( !self->client )
	{
		Quake3Game()->DebugPrint( IGameInterface::WL_ERROR, "Q3_SetSquadName: '%s' is not an NPC/player!\n", self->targetname );
		return;
	}

	if(!Q_stricmp("NULL", ((char *)squadname)))
	{
		self->client->squadname = NULL;
	}
	else
	{
		self->client->squadname = G_NewString(squadname);
	}
}
*/

/*
============
Q3_SetTargetName
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *targetname
============
*/
static void Q3_SetTargetName(const int entID, const char* targetname)
{
	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetTargetName: invalid entID %d\n", entID);
		return;
	}

	if (!Q_stricmp("NULL", targetname))
	{
		self->targetname = nullptr;
	}
	else
	{
		self->targetname = G_NewString(targetname);
	}
}

/*
============
Q3_SetTarget
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *target
============
*/
static void Q3_SetTarget(const int entID, const char* target)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetTarget: invalid entID %d\n", entID);
		return;
	}

	if (!Q_stricmp("NULL", target))
	{
		self->target = nullptr;
	}
	else
	{
		self->target = G_NewString(target);
	}
}

/*
============
Q3_SetTarget2
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *target
============
*/
static void Q3_SetTarget2(const int entID, const char* target2)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetTarget2: invalid entID %d\n", entID);
		return;
	}

	if (!Q_stricmp("NULL", target2))
	{
		self->target2 = nullptr;
	}
	else
	{
		self->target2 = G_NewString(target2);
	}
}

/*
============
Q3_SetRemoveTarget
  Description	:
  Return type	: static void
  Argument		: int entID
  Argument		: const char *target
============
*/
static void Q3_SetRemoveTarget(const int entID, const char* target)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetRemoveTarget: invalid entID %d\n", entID);
		return;
	}

	if (!self->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetRemoveTarget: '%s' is not an NPC!\n",
			self->targetname);
		return;
	}

	if (!Q_stricmp("NULL", target))
	{
		self->target3 = nullptr;
	}
	else
	{
		self->target3 = G_NewString(target);
	}
}

/*
============
Q3_SetPainTarget
  Description	:
  Return type	: void
  Argument		: int entID
  Argument		: const char *targetname
============
*/
static void Q3_SetPainTarget(const int entID, const char* targetname)
{
	gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetPainTarget: invalid entID %d\n", entID);
		return;
	}

	if (Q_stricmp("NULL", targetname) == 0)
	{
		self->paintarget = nullptr;
	}
	else
	{
		self->paintarget = G_NewString(targetname);
	}
}

static void Q3_SetMusicState(const char* dms)
{
	const int newDMS = GetIDForString(DMSTable, dms);
	if (newDMS != -1)
	{
		level.dmState = newDMS;
	}
}

static void Q3_SetForcePowerLevel(const int entID, const int forcePower, const int forceLevel)
{
	if (forcePower < FP_FIRST || forceLevel >= NUM_FORCE_POWERS)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
			"Q3_SetForcePowerLevel: Force Power index %d out of range (%d-%d)\n", forcePower,
			FP_FIRST, NUM_FORCE_POWERS - 1);
		return;
	}

	if (forceLevel < 0 || forceLevel >= NUM_FORCE_POWER_LEVELS)
	{
		if (forcePower != FP_SABER_OFFENSE || forceLevel >= SS_NUM_SABER_STYLES)
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
				"Q3_SetForcePowerLevel: Force power setting %d out of range (0-3)\n", forceLevel);
			return;
		}
	}

	const gentity_t* self = &g_entities[entID];

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetForcePowerLevel: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetForcePowerLevel: ent %s is not a player or NPC\n",
			self->targetname);
		return;
	}

	if (forceLevel > self->client->ps.forcePowerLevel[forcePower] && entID == 0 && forceLevel > 0)
	{
	}

	self->client->ps.forcePowerLevel[forcePower] = forceLevel;
	if (forceLevel)
	{
		self->client->ps.forcePowersKnown |= 1 << forcePower;
	}
	else
	{
		self->client->ps.forcePowersKnown &= ~(1 << forcePower);
	}
}

extern qboolean G_InventorySelectable(int index, const gentity_t* other);

static void Q3_GiveSecurityKey(const int entID, const char* keyname)
{
	const gentity_t* other = &g_entities[entID];

	if (!other)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_GiveSecurityKey: invalid entID %d\n", entID);
		return;
	}

	if (!other->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_GiveSecurityKey: ent %s is not a player or NPC\n",
			other->targetname);
		return;
	}

	if (!keyname || !keyname[0] || !Q_stricmp("none", keyname) || !Q_stricmp("null", keyname))
	{
		//remove the key
		if (other->message)
		{
			//remove it
			INV_SecurityKeyTake(other, other->message);
		}
		return;
	}

	other->client->ps.stats[STAT_ITEMS] |= 1 << INV_SECURITY_KEY;

	//give the key
	gi.SendServerCommand(0, "cp @SP_INGAME_YOU_TOOK_SECURITY_KEY");
	INV_SecurityKeyGive(other, keyname);
	// Got a security key

	// Set the inventory select, just in case it hasn't
	const int original = cg.inventorySelect;
	for (int i = 0; i < INV_MAX; i++)
	{
		if (cg.inventorySelect < INV_ELECTROBINOCULARS || cg.inventorySelect >= INV_MAX)
		{
			cg.inventorySelect = INV_MAX - 1;
		}

		if (G_InventorySelectable(cg.inventorySelect, other))
		{
			return;
		}
		cg.inventorySelect++;
	}

	cg.inventorySelect = original;
}

/*
============
Q3_SetParm
  Description	:
  Return type	: void
  Argument		: int entID
  Argument		: int parmNum
  Argument		: const char *parmValue
============
*/
void Q3_SetParm(const int entID, const int parmNum, const char* parmValue)
{
	gentity_t* ent = &g_entities[entID];
	float val;

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetParm: invalid entID %d\n", entID);
		return;
	}

	if (parmNum < 0 || parmNum >= MAX_PARMS)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "SET_PARM: parmNum %d out of range!\n", parmNum);
		return;
	}

	if (!ent->parms)
	{
		ent->parms = static_cast<parms_t*>(G_Alloc(sizeof(parms_t)));
		memset(ent->parms, 0, sizeof(parms_t));
	}

	if ((val = Q3_CheckStringCounterIncrement(parmValue)))
	{
		val += atof(ent->parms->parm[parmNum]);
		Com_sprintf(ent->parms->parm[parmNum], sizeof ent->parms->parm[parmNum], "%f", val);
	}
	else
	{
		//Just copy the string
		//copy only 16 characters
		strncpy(ent->parms->parm[parmNum], parmValue, sizeof ent->parms->parm[parmNum]);
		//set the last character to null in case we had to truncate their passed string
		if (ent->parms->parm[parmNum][sizeof ent->parms->parm[parmNum] - 1] != 0)
		{
			//Tried to set a string that is too long
			ent->parms->parm[parmNum][sizeof ent->parms->parm[parmNum] - 1] = 0;
			Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
				"SET_PARM: parm%d string too long, truncated to '%s'!\n", parmNum,
				ent->parms->parm[parmNum]);
		}
	}
}

/*
=============
Q3_SetCaptureGoal

Sets the capture spot goal of an entity
=============
*/
static void Q3_SetCaptureGoal(const int entID, const char* name)
{
	const gentity_t* ent = &g_entities[entID];
	gentity_t* goal = G_Find(nullptr, FOFS(targetname), name);

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetCaptureGoal: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetCaptureGoal: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	//FIXME: Exception handle here
	if (goal == nullptr)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetCaptureGoal: can't find CaptureGoal target: '%s'\n",
			name);
		return;
	}

	if (ent->NPC)
	{
		ent->NPC->captureGoal = goal;
		ent->NPC->goalEntity = goal;
		ent->NPC->goalTime = level.time + 100000;
	}
}

/*
=============
Q3_SetEvent

?
=============
*/
static void Q3_SetEvent(const int entID, const char* event_name)
{
	const gentity_t* ent = &g_entities[entID];
	//	vec3_t		spot;

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEvent: invalid entID %d\n", entID);
		return;
	}

	const int event = GetIDForString(eventTable, event_name);
	switch (event)
	{
		/*
			case EV_DISINTEGRATE:
				if( VectorCompare( ent->currentOrigin, vec3_origin ) )
				{//Brush with no origin
					VectorSubtract( ent->absmax, ent->absmin, spot );
					VectorMA( ent->absmin, 0.5, spot, spot );
				}
				else
				{
					VectorCopy( ent->currentOrigin, spot );
					spot[2] += ent->maxs[2]/2;
				}
				tent = G_TempEntity( spot, EV_DISINTEGRATION );
				tent->s.eventParm = PW_REGEN;
				tent->owner = ent;
				break;

		*/
	case EV_BAD:
	default:
		//Quake3Game()->DebugPrint( IGameInterface::WL_ERROR,"Q3_SetEvent: Invalid Event %d\n", event );
		return;
	}
}

/*
============
Q3_Use

Uses an entity
============
*/
/*static void Q3_Use( int entID, const char *target )
{
	gentity_t	*ent  = &g_entities[entID];

	if ( !ent )
	{
		Quake3Game()->DebugPrint( IGameInterface::WL_WARNING, "Q3_Use: invalid entID %d\n", entID);
		return;
	}

	if( !target || !target[0] )
	{
		Quake3Game()->DebugPrint( IGameInterface::WL_WARNING, "Q3_Use: string is NULL!\n" );
		return;
	}

	G_UseTargets2(ent, ent, target);
}*/

/*
============
Q3_SetBehaviorSet

?
============
*/
static qboolean Q3_SetBehaviorSet(const int entID, const int toSet, const char* scriptname)
{
	gentity_t* ent = &g_entities[entID];
	bSet_t bSet = BSET_INVALID;

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetBehaviorSet: invalid entID %d\n", entID);
		return qfalse;
	}

	switch (toSet)
	{
	case SET_SPAWNSCRIPT:
		bSet = BSET_SPAWN;
		break;
	case SET_USESCRIPT:
		bSet = BSET_USE;
		break;
	case SET_AWAKESCRIPT:
		bSet = BSET_AWAKE;
		break;
	case SET_ANGERSCRIPT:
		bSet = BSET_ANGER;
		break;
	case SET_ATTACKSCRIPT:
		bSet = BSET_ATTACK;
		break;
	case SET_VICTORYSCRIPT:
		bSet = BSET_VICTORY;
		break;
	case SET_LOSTENEMYSCRIPT:
		bSet = BSET_LOSTENEMY;
		break;
	case SET_PAINSCRIPT:
		bSet = BSET_PAIN;
		break;
	case SET_FLEESCRIPT:
		bSet = BSET_FLEE;
		break;
	case SET_DEATHSCRIPT:
		bSet = BSET_DEATH;
		break;
	case SET_DELAYEDSCRIPT:
		bSet = BSET_DELAYED;
		break;
	case SET_BLOCKEDSCRIPT:
		bSet = BSET_BLOCKED;
		break;
	case SET_FFIRESCRIPT:
		bSet = BSET_FFIRE;
		break;
	case SET_FFDEATHSCRIPT:
		bSet = BSET_FFDEATH;
		break;
	case SET_MINDTRICKSCRIPT:
		bSet = BSET_MINDTRICK;
		break;
	case SET_UNDYINGPLAYERVICTORYSCRIPT:
		bSet = UNDYINGPLAYERVICTORY;
		break;
	default:;
	}

	if (bSet < BSET_SPAWN || bSet >= NUM_BSETS)
	{
		return qfalse;
	}

	if (!Q_stricmp("NULL", scriptname))
	{
		if (ent->behaviorSet[bSet] != nullptr)
		{
			//			gi.TagFree( ent->behaviorSet[bSet] );
		}

		ent->behaviorSet[bSet] = nullptr;
		//memset( &ent->behaviorSet[bSet], 0, sizeof(ent->behaviorSet[bSet]) );
	}
	else
	{
		if (scriptname)
		{
			if (ent->behaviorSet[bSet] != nullptr)
			{
				//				gi.TagFree( ent->behaviorSet[bSet] );
			}

			ent->behaviorSet[bSet] = G_NewString(scriptname); //FIXME: This really isn't good...
		}

		//ent->behaviorSet[bSet] = scriptname;
		//strncpy( (char *) &ent->behaviorSet[bSet], scriptname, MAX_BSET_LENGTH );
	}
	return qtrue;
}

/*
============
Q3_SetDelayScriptTime

?
============
*/
static void Q3_SetDelayScriptTime(const int entID, const int delayTime)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDelayScriptTime: invalid entID %d\n", entID);
		return;
	}

	ent->delayScriptTime = level.time + delayTime;
}

/*
============
Q3_SetIgnorePain

?
============
*/
static void Q3_SetIgnorePain(const int entID, const qboolean data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetIgnorePain: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetIgnorePain: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	ent->NPC->ignorePain = data;
}

/*
============
Q3_SetIgnoreEnemies

?
============
*/
static void Q3_SetIgnoreEnemies(const int entID, const qboolean data)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetIgnoreEnemies: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetIgnoreEnemies: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (data)
	{
		ent->svFlags |= SVF_IGNORE_ENEMIES;
	}
	else
	{
		ent->svFlags &= ~SVF_IGNORE_ENEMIES;
	}
}

/*
============
Q3_SetIgnoreAlerts

?
============
*/
static void Q3_SetIgnoreAlerts(const int entID, const qboolean data)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetIgnoreAlerts: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetIgnoreAlerts: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (data)
	{
		ent->NPC->scriptFlags |= SCF_IGNORE_ALERTS;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_IGNORE_ALERTS;
	}
}

/*
============
Q3_SetNoTarget

?
============
*/
static void Q3_SetNoTarget(const int entID, const qboolean data)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoTarget: invalid entID %d\n", entID);
		return;
	}

	if (data)
		ent->flags |= FL_NOTARGET;
	else
		ent->flags &= ~FL_NOTARGET;
}

/*
============
Q3_SetDontShoot

?
============
*/
static void Q3_SetDontShoot(const int entID, const qboolean add)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDontShoot: invalid entID %d\n", entID);
		return;
	}

	if (add)
	{
		ent->flags |= FL_DONT_SHOOT;
	}
	else
	{
		ent->flags &= ~FL_DONT_SHOOT;
	}
}

/*
============
Q3_SetDontFire

?
============
*/
static void Q3_SetDontFire(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDontFire: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetDontFire: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_DONT_FIRE;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_DONT_FIRE;
	}
}

/*
============
Q3_SetFireWeapon

?
============
*/
static void Q3_SetFireWeapon(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_FireWeapon: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetFireWeapon: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_FIRE_WEAPON;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_FIRE_WEAPON;
	}
}

/*
============
Q3_SetFireWeaponNoAnim

?
============
*/
static void Q3_SetFireWeaponNoAnim(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_FireWeaponNoAnim: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetFireWeaponNoAnim: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_FIRE_WEAPON_NO_ANIM;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_FIRE_WEAPON_NO_ANIM;
	}
}

/*
============
Q3_SetSafeRemove

If true, NPC will remove itself once player is not in PVS
============
*/
static void Q3_SetSafeRemove(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetSafeRemove: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetSafeRemove: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_SAFE_REMOVE;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_SAFE_REMOVE;
	}
}

/*
============
Q3_SetBobaJetPack

Turn on/off Boba's jet pack
============
*/
static void Q3_SetBobaJetPack(const int entID, const qboolean add)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetBobaJetPack: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetBobaJetPack: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	// make sure we this is Boba Fett
	if (ent->client && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN &&
		ent->client->NPC_class != CLASS_JANGO && ent->client->NPC_class != CLASS_JANGODUAL))
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetBobaJetPack: '%s' is not Boba Fett!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		if (ent->genericBolt1 != -1)
		{
			if (ent->client && ent->client->NPC_class == CLASS_MANDALORIAN)
			{
				G_PlayEffect(G_EffectIndex("rockettrooper/flameNEW"), ent->playerModel, ent->genericBolt1,
					ent->s.number, ent->currentOrigin, qtrue, qtrue);
			}
			else
			{
				G_PlayEffect(G_EffectIndex("boba/jetSP"), ent->playerModel, ent->genericBolt1, ent->s.number,
					ent->currentOrigin, qtrue, qtrue);
			}
		}
		if (ent->genericBolt2 != -1)
		{
			if (ent->client && ent->client->NPC_class == CLASS_MANDALORIAN)
			{
				G_PlayEffect(G_EffectIndex("rockettrooper/flameNEW"), ent->playerModel, ent->genericBolt2,
					ent->s.number, ent->currentOrigin, qtrue, qtrue);
			}
			else
			{
				G_PlayEffect(G_EffectIndex("boba/jetSP"), ent->playerModel, ent->genericBolt2, ent->s.number,
					ent->currentOrigin, qtrue, qtrue);
			}
		}
		//take-off sound
		G_SoundOnEnt(ent, CHAN_ITEM, "sound/chars/boba/bf_blast-off.wav");
		//jet loop sound
		ent->s.loopSound = G_SoundIndex("sound/chars/boba/bf_jetpack_lp.wav");
	}
	else
	{
		if (ent->genericBolt1 != -1)
		{
			if (ent->client && ent->client->NPC_class == CLASS_MANDALORIAN)
			{
				G_StopEffect("rockettrooper/flameNEW", ent->playerModel, ent->genericBolt1, ent->s.number);
			}
			else
			{
				G_StopEffect("boba/jetSP", ent->playerModel, ent->genericBolt1, ent->s.number);
			}
		}
		if (ent->genericBolt2 != -1)
		{
			if (ent->client && ent->client->NPC_class == CLASS_MANDALORIAN)
			{
				G_StopEffect("rockettrooper/flameNEW", ent->playerModel, ent->genericBolt2, ent->s.number);
			}
			else
			{
				G_StopEffect("boba/jetSP", ent->playerModel, ent->genericBolt2, ent->s.number);
			}
		}
		//stop jet loop sound
		ent->s.loopSound = 0;
		G_SoundOnEnt(ent, CHAN_ITEM, "sound/chars/boba/bf_land.wav");
	}
}

/*
============
Q3_SetInactive

?
============
*/
static void Q3_SetInactive(const int entID, const qboolean add)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetInactive: invalid entID %d\n", entID);
		return;
	}

	if (add)
	{
		ent->svFlags |= SVF_INACTIVE;
	}
	else
	{
		ent->svFlags &= ~SVF_INACTIVE;
	}
}

/*
============
Q3_SetFuncUsableVisible

?
============
*/
static void Q3_SetFuncUsableVisible(const int entID, const qboolean visible)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetFuncUsableVisible: invalid entID %d\n", entID);
		return;
	}

	// Yeah, I know that this doesn't even do half of what the func_usable use code does, but if I've got two things on top of each other...and only
	//	one is visible at a time....and neither can ever be used......and finally, the shader on it has the shader_anim stuff going on....It doesn't seem
	//	like I can easily use the other version without nasty side effects.
	if (visible)
	{
		ent->svFlags &= ~SVF_NOCLIENT;
		ent->s.eFlags &= ~EF_NODRAW;
	}
	else
	{
		ent->svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
	}
}

/*
============
Q3_SetLockedEnemy

?
============
*/
static void Q3_SetLockedEnemy(const int entID, const qboolean locked)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLockedEnemy: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetLockedEnemy: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	//FIXME: make an NPCAI_FLAG
	if (locked)
	{
		ent->svFlags |= SVF_LOCKEDENEMY;
	}
	else
	{
		ent->svFlags &= ~SVF_LOCKEDENEMY;
	}
}

char cinematicSkipScript[64];

/*
============
Q3_SetCinematicSkipScript

============
*/
static void Q3_SetCinematicSkipScript(const char* scriptname)
{
	if (Q_stricmp("none", scriptname) == 0 || Q_stricmp("NULL", scriptname) == 0)
	{
		cinematicSkipScript[0] = 0;
	}
	else
	{
		Q_strncpyz(cinematicSkipScript, scriptname, sizeof cinematicSkipScript);
	}
}

/*
============
Q3_SetNoMindTrick

?
============
*/
static void Q3_SetNoMindTrick(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoMindTrick: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNoMindTrick: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_MIND_TRICK;
		ent->NPC->confusionTime = 0;
		if (ent->ghoul2.size() && ent->headBolt != -1)
		{
			G_StopEffect("force/confusion", ent->playerModel, ent->headBolt, ent->s.number);
		}
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_MIND_TRICK;
	}
}

/*
============
Q3_SetCrouched

?
============
*/
static void Q3_SetCrouched(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetCrouched: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetCrouched: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_CROUCHED;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_CROUCHED;
	}
}

/*
============
Q3_SetWalking

?
============
*/
static void Q3_SetWalking(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetWalking: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetWalking: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_WALKING;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_WALKING;
	}
}

/*
============
Q3_SetRunning

?
============
*/
static void Q3_SetRunning(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetRunning: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetRunning: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_RUNNING;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_RUNNING;
	}
}

/*
============
Q3_SetForcedMarch

?
============
*/
static void Q3_SetForcedMarch(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetForcedMarch: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetForcedMarch: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_FORCED_MARCH;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_FORCED_MARCH;
	}
}

/*
============
Q3_SetChaseEnemies

indicates whether the npc should chase after an enemy
============
*/
static void Q3_SetChaseEnemies(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetChaseEnemies: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetChaseEnemies: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_CHASE_ENEMIES;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_CHASE_ENEMIES;
	}
}

/*
============
Q3_SetLookForEnemies

if set npc will be on the look out for potential enemies
if not set, npc will ignore enemies
============
*/
static void Q3_SetLookForEnemies(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLookForEnemies: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetLookForEnemies: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_LOOK_FOR_ENEMIES;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_LOOK_FOR_ENEMIES;
	}
}

/*
============
Q3_SetFaceMoveDir

============
*/
static void Q3_SetFaceMoveDir(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetFaceMoveDir: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetFaceMoveDir: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_FACE_MOVE_DIR;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_FACE_MOVE_DIR;
	}
}

/*
============
Q3_SetAltFire

?
============
*/
static void Q3_SetAltFire(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAltFire: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetAltFire: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_ALT_FIRE;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_ALT_FIRE;
	}

	ChangeWeapon(ent, ent->client->ps.weapon);
}

/*
============
Q3_SetDontFlee

?
============
*/
static void Q3_SetDontFlee(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDontFlee: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetDontFlee: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_DONT_FLEE;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_DONT_FLEE;
	}
}

/*
============
Q3_SetNoResponse

?
============
*/
static void Q3_SetNoResponse(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoResponse: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNoResponse: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_RESPONSE;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_RESPONSE;
	}
}

/*
============
Q3_SetCombatTalk

?
============
*/
static void Q3_SetCombatTalk(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetCombatTalk: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetCombatTalk: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_COMBAT_TALK;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_COMBAT_TALK;
	}
}

/*
============
Q3_SetAlertTalk

?
============
*/
static void Q3_SetAlertTalk(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAlertTalk: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetAlertTalk: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_ALERT_TALK;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_ALERT_TALK;
	}
}

/*
============
Q3_SetUseCpNearest

?
============
*/
static void Q3_SetUseCpNearest(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetUseCpNearest: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetUseCpNearest: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_USE_CP_NEAREST;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_USE_CP_NEAREST;
	}
}

/*
============
Q3_SetNoForce

?
============
*/
static void Q3_SetNoForce(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoForce: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNoForce: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_FORCE;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_FORCE;
	}
}

/*
============
Q3_SetNoAcrobatics

?
============
*/
static void Q3_SetNoAcrobatics(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoAcrobatics: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNoAcrobatics: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_ACROBATICS;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_ACROBATICS;
	}
}

/*
============
Q3_SetUseSubtitles

?
============
*/
static void Q3_SetUseSubtitles(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetUseSubtitles: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetUseSubtitles: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_USE_SUBTITLES;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_USE_SUBTITLES;
	}
}

/*
============
Q3_SetNoFallToDeath

?
============
*/
static void Q3_SetNoFallToDeath(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoFallToDeath: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNoFallToDeath: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_NO_FALLTODEATH;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_NO_FALLTODEATH;
	}
}

/*
============
Q3_SetDismemberable

?
============
*/
static void Q3_SetDismemberable(const int entID, const qboolean dismemberable)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDismemberable: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetDismemberable: '%s' is not an client!\n",
			ent->targetname);
		return;
	}

	ent->client->dismembered = !dismemberable;
}

/*
============
Q3_SetMoreLight

?
============
*/
static void Q3_SetMoreLight(const int entID, const qboolean add)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetMoreLight: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetMoreLight: '%s' is not an NPC!\n",
			ent->targetname);
		return;
	}

	if (add)
	{
		ent->NPC->scriptFlags |= SCF_MORELIGHT;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_MORELIGHT;
	}
}

static void Q3_SetMoreLightnpc(const int entID, const qboolean add)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetMoreLightnpc: invalid entID %d\n", entID);
		return;
	}

	if (add)
	{
		ent->flags |= FL_MORELIGHPLAYER;
	}
	else
	{
		ent->flags &= ~FL_MORELIGHPLAYER;
	}
}

/*
============
Q3_SetUndying

?
============
*/
static void Q3_SetUndying(const int entID, const qboolean undying)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetUndying: invalid entID %d\n", entID);
		return;
	}

	if (undying)
	{
		ent->flags |= FL_UNDYING;
	}
	else
	{
		ent->flags &= ~FL_UNDYING;
	}
}

/*
============
Q3_SetInvincible

?
============
*/
static void Q3_SetInvincible(const int entID, const qboolean invincible)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetInvincible: invalid entID %d\n", entID);
		return;
	}

	if (!Q_stricmp("func_breakable", ent->classname))
	{
		if (invincible)
		{
			ent->spawnflags |= 1;
		}
		else
		{
			ent->spawnflags &= ~1;
		}
		return;
	}

	if (invincible)
	{
		ent->flags |= FL_GODMODE;
	}
	else
	{
		ent->flags &= ~FL_GODMODE;
	}
}

/*
============
Q3_SetEndlessBP

?
============
*/
static void Q3_SetEndlessBP(const int entID, const qboolean endlessbp)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEndlessBP: invalid entID %d\n", entID);
		return;
	}

	if (endlessbp)
	{
		ent->flags |= FL_BLOCKPOINTMODE;
	}
	else
	{
		ent->flags &= ~FL_BLOCKPOINTMODE;
	}
}

/*
============
Q3_SetEndlessST

?
============
*/
static void Q3_SetEndlessST(const int entID, const qboolean endlessst)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEndlessST: invalid entID %d\n", entID);
		return;
	}

	if (endlessst)
	{
		ent->flags |= FL_STAMINA_MODE;
	}
	else
	{
		ent->flags &= ~FL_STAMINA_MODE;
	}
}

/*
============
Q3_SetForceInvincible
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: qboolean forceInv
============
*/
static void Q3_SetForceInvincible(const int entID, const qboolean forceInv)
{
	const gentity_t* self = &g_entities[entID];

	if (!self || !self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetForceInvincible: entID %d not a client\n",
			entID);
		return;
	}

	Q3_SetInvincible(entID, forceInv);
	if (forceInv)
	{
		self->client->ps.powerups[PW_INVINCIBLE] = Q3_INFINITE;
	}
	else
	{
		self->client->ps.powerups[PW_INVINCIBLE] = 0;
	}
}

/*
============
Q3_SetNoAvoid

?
============
*/
static void Q3_SetNoAvoid(const int entID, const qboolean noAvoid)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoAvoid: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetNoAvoid: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (noAvoid)
	{
		ent->NPC->aiFlags |= NPCAI_NO_COLL_AVOID;
	}
	else
	{
		ent->NPC->aiFlags &= ~NPCAI_NO_COLL_AVOID;
	}
}

/*
============
SolidifyOwner
  Description	:
  Return type	: void
  Argument		: gentity_t *self
============
*/
void SolidifyOwner(gentity_t* self)
{
	self->nextthink = level.time + FRAMETIME;
	self->e_ThinkFunc = thinkF_G_FreeEntity;

	if (!self->owner || !self->owner->inuse)
	{
		return;
	}

	const int oldContents = self->owner->contents;
	self->owner->contents = CONTENTS_BODY;
	if (SpotWouldTelefrag2(self->owner, self->owner->currentOrigin))
	{
		self->owner->contents = oldContents;
		self->e_ThinkFunc = thinkF_SolidifyOwner;
	}
	else
	{
		if (self->owner->NPC && !(self->owner->spawnflags & SFB_NOTSOLID))
		{
			self->owner->clipmask |= CONTENTS_BODY;
		}
		Q3_TaskIDComplete(self->owner, TID_RESIZE);
	}
}

/*
============
Q3_SetSolid

?
============
*/
static qboolean Q3_SetSolid(const int entID, const qboolean solid)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetSolid: invalid entID %d\n", entID);
		return qtrue;
	}

	if (solid)
	{
		//FIXME: Presumption
		const int oldContents = ent->contents;
		ent->contents = CONTENTS_BODY;
		if (SpotWouldTelefrag2(ent, ent->currentOrigin))
		{
			gentity_t* solidifier = G_Spawn();

			solidifier->owner = ent;

			solidifier->e_ThinkFunc = thinkF_SolidifyOwner;
			solidifier->nextthink = level.time + FRAMETIME;

			ent->contents = oldContents;
			return qfalse;
		}
		ent->clipmask |= CONTENTS_BODY;
	}
	else
	{
		//FIXME: Presumption
		if (ent->s.eFlags & EF_NODRAW)
		{
			//We're invisible too, so set contents to none
			ent->contents = 0;
		}
		else
		{
			ent->contents = CONTENTS_CORPSE;
		}
		if (ent->NPC)
		{
			if (!(ent->spawnflags & SFB_NOTSOLID))
			{
				ent->clipmask &= ~CONTENTS_BODY;
			}
		}
	}
	return qtrue;
}

/*
============
Q3_SetLean

?
============
*/
#define LEAN_NONE	0
#define LEAN_RIGHT	1
#define LEAN_LEFT	2

static void Q3_SetLean(const int entID, const int lean)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLean: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetLean: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (lean == LEAN_RIGHT)
	{
		ent->NPC->scriptFlags |= SCF_LEAN_RIGHT;
		ent->NPC->scriptFlags &= ~SCF_LEAN_LEFT;
	}
	else if (lean == LEAN_LEFT)
	{
		ent->NPC->scriptFlags |= SCF_LEAN_LEFT;
		ent->NPC->scriptFlags &= ~SCF_LEAN_RIGHT;
	}
	else
	{
		ent->NPC->scriptFlags &= ~SCF_LEAN_LEFT;
		ent->NPC->scriptFlags &= ~SCF_LEAN_RIGHT;
	}
}

/*
============
Q3_SetForwardMove

?
============
*/
static void Q3_SetForwardMove(const int entID, const int fmoveVal)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetForwardMove: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetForwardMove: '%s' is not an NPC/player!\n",
			ent->targetname);
		return;
	}

	ent->client->forced_forwardmove = fmoveVal;
}

/*
============
Q3_SetRightMove

?
============
*/
static void Q3_SetRightMove(const int entID, const int rmoveVal)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetRightMove: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetRightMove: '%s' is not an NPC/player!\n",
			ent->targetname);
		return;
	}

	ent->client->forced_rightmove = rmoveVal;
}

/*
============
Q3_SetLockAngle

?
============
*/
static void Q3_SetLockAngle(const int entID, const char* lockAngle)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLockAngle: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetLockAngle: '%s' is not an NPC/player!\n",
			ent->targetname);
		return;
	}

	if (Q_stricmp("off", lockAngle) == 0)
	{
		//free it
		ent->client->renderInfo.renderFlags &= ~RF_LOCKEDANGLE;
	}
	else
	{
		ent->client->renderInfo.renderFlags |= RF_LOCKEDANGLE;

		if (Q_stricmp("auto", lockAngle) == 0)
		{
			//use current yaw
			if (ent->NPC) // I need this to work on NPCs, so their locked value
			{
				ent->NPC->lockedDesiredYaw = NPC->client->ps.viewangles[YAW];
				// could also set s.angles[1] and desiredYaw to this value...
			}
			else
			{
				ent->client->renderInfo.lockYaw = ent->client->ps.viewangles[YAW];
			}
		}
		else
		{
			//specified yaw
			if (ent->NPC) // I need this to work on NPCs, so their locked value
			{
				ent->NPC->lockedDesiredYaw = atof(lockAngle);
				// could also set s.angles[1] and desiredYaw to this value...
			}
			else
			{
				ent->client->renderInfo.lockYaw = atof(lockAngle);
			}
		}
	}
}

/*
============
Q3_CameraGroup

?
============
*/
static void Q3_CameraGroup(const int entID, const char* camG)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_CameraGroup: invalid entID %d\n", entID);
		return;
	}

	ent->cameraGroup = G_NewString(camG);
}

extern camera_t client_camera;
/*
============
Q3_CameraGroupZOfs

?
============
*/
static void Q3_CameraGroupZOfs(const float camGZOfs)
{
	client_camera.cameraGroupZOfs = camGZOfs;
}

/*
============
Q3_CameraGroup

?
============
*/
static void Q3_CameraGroupTag(const char* camGTag)
{
	Q_strncpyz(client_camera.cameraGroupTag, camGTag, sizeof client_camera.cameraGroupTag);
}

/*
============
Q3_RemoveRHandModel
============
*/
static void Q3_RemoveRHandModel(const int entID, char* addModel)
{
	gentity_t* ent = &g_entities[entID];

	if (ent->cinematicModel >= 0)
	{
		gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->cinematicModel);
	}
}

/*
============
Q3_AddRHandModel
============
*/
static void Q3_AddRHandModel(const int entID, const char* addModel)
{
	gentity_t* ent = &g_entities[entID];

	ent->cinematicModel = gi.G2API_InitGhoul2Model(ent->ghoul2, addModel, G_ModelIndex(addModel), 0, 0, 0, 0);
	if (ent->cinematicModel != -1)
	{
		// attach it to the hand
		gi.G2API_AttachG2Model(&ent->ghoul2[ent->cinematicModel], &ent->ghoul2[ent->playerModel],
			ent->handRBolt, ent->playerModel);
	}
}

/*
============
Q3_AddLHandModel
============
*/
static void Q3_AddLHandModel(const int entID, const char* addModel)
{
	gentity_t* ent = &g_entities[entID];

	ent->cinematicModel = gi.G2API_InitGhoul2Model(ent->ghoul2, addModel, G_ModelIndex(addModel), 0, 0, 0, 0);
	if (ent->cinematicModel != -1)
	{
		// attach it to the hand
		gi.G2API_AttachG2Model(&ent->ghoul2[ent->cinematicModel], &ent->ghoul2[ent->playerModel],
			ent->handLBolt, ent->playerModel);
	}
}

/*
============
Q3_RemoveLHandModel
============
*/
static void Q3_RemoveLHandModel(const int entID, char* addModel)
{
	gentity_t* ent = &g_entities[entID];

	if (ent->cinematicModel >= 0)
	{
		gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->cinematicModel);
	}
}

/*
============
Q3_LookTarget

?
============
*/
static void Q3_LookTarget(const int entID, char* targetName)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_LookTarget: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_LookTarget: '%s' is not an NPC/player!\n",
			ent->targetname);
		return;
	}

	if (Q_stricmp("none", targetName) == 0 || Q_stricmp("NULL", targetName) == 0)
	{
		//clearing look target
		NPC_ClearLookTarget(ent);
		return;
	}

	const gentity_t* targ = G_Find(nullptr, FOFS(targetname), targetName);
	if (!targ)
	{
		targ = G_Find(nullptr, FOFS(script_targetname), targetName);
		if (!targ)
		{
			targ = G_Find(nullptr, FOFS(NPC_targetname), targetName);
			if (!targ)
			{
				Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_LookTarget: Can't find ent %s\n",
					targetName);
				return;
			}
		}
	}

	NPC_SetLookTarget(ent, targ->s.number, 0);
}

/*
============
Q3_Face

?
============
*/
static void Q3_Face(const int entID, const int expression, float holdtime)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_Face: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_Face: '%s' is not an NPC/player!\n",
			ent->targetname);
		return;
	}

	//FIXME: change to milliseconds to be consistant!
	holdtime *= 1000;

	switch (expression)
	{
	case SET_FACEEYESCLOSED:
		ent->client->facial_blink = 1;
		break;
	case SET_FACEEYESOPENED:
		ent->client->facial_blink = -1;
		break;
	case SET_FACEBLINK:
		ent->client->facial_timer = -(level.time + holdtime);
		break;
	case SET_FACEAUX:
		ent->client->facial_timer = -(level.time + holdtime);
		ent->client->facial_anim = FACE_ALERT;
		break;
	case SET_FACEBLINKFROWN:
		ent->client->facial_blink = -(level.time + holdtime);
		//fall through
	case SET_FACEFROWN:
		ent->client->facial_timer = -(level.time + holdtime);
		ent->client->facial_anim = FACE_FROWN;
		break;

		//Extra facial expressions:
	case SET_FACESMILE:
		ent->client->facial_blink = -(level.time + holdtime);
		ent->client->facial_timer = -(level.time + holdtime);
		ent->client->facial_anim = FACE_SMILE;
		break;
	case SET_FACEGLAD:
		ent->client->facial_blink = 1;
		ent->client->facial_timer = -(level.time + holdtime);
		ent->client->facial_anim = FACE_TALK1;
		break;
	case SET_FACEHAPPY:
		ent->client->facial_blink = -(level.time + holdtime);
		ent->client->facial_timer = -(level.time + holdtime);
		ent->client->facial_anim = FACE_TALK1;
		break;
	case SET_FACESHOCKED:
		ent->client->facial_blink = -1;
		ent->client->facial_timer = -(level.time + holdtime);
		ent->client->facial_anim = FACE_TALK3;
		break;

	case SET_FACENORMAL:
		ent->client->facial_timer = level.time + Q_flrand(6000.0, 10000.0);
		ent->client->facial_blink = level.time + Q_flrand(3000.0, 5000.0);
		break;
	default:;
	}
}

/*
============
Q3_SetPlayerUsable
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean usable
============
*/
static void Q3_SetPlayerUsable(const int entID, const qboolean usable)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetPlayerUsable: invalid entID %d\n", entID);
		return;
	}

	if (usable)
	{
		ent->svFlags |= SVF_PLAYER_USABLE;
	}
	else
	{
		ent->svFlags &= ~SVF_PLAYER_USABLE;
	}
}

/*
============
Q3_SetDisableShaderAnims
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int disabled
============
*/
static void Q3_SetDisableShaderAnims(const int entID, const int disabled)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetDisableShaderAnims: invalid entID %d\n", entID);
		return;
	}

	if (disabled)
	{
		ent->s.eFlags |= EF_DISABLE_SHADER_ANIM;
	}
	else
	{
		ent->s.eFlags &= ~EF_DISABLE_SHADER_ANIM;
	}
}

/*
============
Q3_SetShaderAnim
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int disabled
============
*/
static void Q3_SetShaderAnim(const int entID, const int disabled)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetShaderAnim: invalid entID %d\n", entID);
		return;
	}

	if (disabled)
	{
		ent->s.eFlags |= EF_SHADER_ANIM;
	}
	else
	{
		ent->s.eFlags &= ~EF_SHADER_ANIM;
	}
}

static void Q3_SetBroadcast(const int entID, const qboolean broadcast)
{
	gentity_t* ent = &g_entities[entID];
	if (broadcast)
	{
		ent->svFlags |= SVF_BROADCAST;
	}
	else
	{
		ent->svFlags &= ~SVF_BROADCAST;
	}
}

static void Q3_SetForcePower(const int entID, const int forcePower, const qboolean powerOn)
{
	const gentity_t* ent = &g_entities[entID];
	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetForcePower: invalid entID %d\n", entID);
		return;
	}
	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetForcePower: ent # %d not a client!\n", entID);
		return;
	}
	if (powerOn)
	{
		ent->client->ps.forcePowersForced |= 1 << forcePower;
	}
	else
	{
		ent->client->ps.forcePowersForced &= ~(1 << forcePower);
	}
}

/*
============
Q3_SetStartFrame
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int startFrame
============
*/
static void Q3_SetStartFrame(const int entID, const int startFrame)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetStartFrame: invalid entID %d\n", entID);
		return;
	}

	if (ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_SetLoopAnim: command not valid on players/NPCs!\n");
		return;
	}

	if (startFrame >= 0)
	{
		ent->s.frame = startFrame;
		ent->startFrame = startFrame;
	}
}

/*
============
Q3_SetEndFrame
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int endFrame
============
*/
static void Q3_SetEndFrame(const int entID, const int endFrame)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetEndFrame: invalid entID %d\n", entID);
		return;
	}

	if (ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_SetEndFrame: command not valid on players/NPCs!\n");
		return;
	}

	if (endFrame >= 0)
	{
		ent->endFrame = endFrame;
	}
}

/*
============
Q3_SetAnimFrame
  Description	:
  Return type	: static void
  Argument		:  int entID
  Argument		: int startFrame
============
*/
static void Q3_SetAnimFrame(const int entID, const int animFrame)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetAnimFrame: invalid entID %d\n", entID);
		return;
	}

	if (ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_SetAnimFrame: command not valid on players/NPCs!\n");
		return;
	}

	if (animFrame >= ent->endFrame)
	{
		ent->s.frame = ent->endFrame;
	}
	else if (animFrame >= ent->startFrame)
	{
		ent->s.frame = animFrame;
	}
	else
	{
		// FIXME/NOTE: Set s.frame anyway??
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_SetAnimFrame: value must be valid number between StartFrame and EndFrame.\n");
	}
}

void InflateOwner(gentity_t* self)
{
	self->nextthink = level.time + FRAMETIME;
	self->e_ThinkFunc = thinkF_G_FreeEntity;

	if (!self->owner || !self->owner->inuse)
	{
		return;
	}

	trace_t trace;

	gi.trace(&trace, self->currentOrigin, self->mins, self->maxs, self->currentOrigin, self->owner->s.number,
		self->owner->clipmask & ~(CONTENTS_SOLID | CONTENTS_MONSTERCLIP), static_cast<EG2_Collision>(0), 0);
	if (trace.allsolid || trace.startsolid)
	{
		self->e_ThinkFunc = thinkF_InflateOwner;
		return;
	}

	if (Q3_TaskIDPending(self->owner, TID_RESIZE))
	{
		Q3_TaskIDComplete(self->owner, TID_RESIZE);

		VectorCopy(self->mins, self->owner->mins);
		VectorCopy(self->maxs, self->owner->maxs);
		gi.linkentity(self->owner);
	}
}

/*
============
Q3_SetLoopAnim
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean loopAnim
============
*/
static void Q3_SetLoopAnim(const int entID, const qboolean loopAnim)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetLoopAnim: invalid entID %d\n", entID);
		return;
	}

	if (ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING,
			"Q3_SetLoopAnim: command not valid on players/NPCs!\n");
		return;
	}

	ent->loopAnim = loopAnim;
}

/*
============
Q3_SetShields
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean shields
============
*/
static void Q3_SetShields(const int entID, const qboolean shields)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetShields: invalid entID %d\n", entID);
		return;
	}

	if (!ent->NPC)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetShields: '%s' is not an NPC!\n", ent->targetname);
		return;
	}

	if (shields)
	{
		ent->NPC->aiFlags |= NPCAI_SHIELDS;
	}
	else
	{
		ent->NPC->aiFlags &= ~NPCAI_SHIELDS;
	}
}

/*
============
Q3_SetSaberActive
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean shields
============
*/
static void Q3_SetSaberActive(const int entID, const qboolean active)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetSaberActive: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetSaberActive: '%s' is not an player/NPC!\n",
			ent->targetname);
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		if (ent->client->ps.weapons[WP_SABER])
		{
			//change to it right now
			if (ent->NPC)
			{
				ChangeWeapon(ent, WP_SABER);
			}
			else
			{
				const gitem_t* item = FindItemForWeapon(WP_SABER);
				RegisterItem(item); //make sure the weapon is cached in case this runs at startup
				G_AddEvent(ent, EV_ITEM_PICKUP, item - bg_itemlist);
				CG_ChangeWeapon(WP_SABER);
			}
			ent->client->ps.weapon = WP_SABER;
			ent->client->ps.weaponstate = WEAPON_READY;
			G_AddEvent(ent, EV_GENERAL_SOUND, G_SoundIndex("sound/weapons/change.wav"));
		}
		else
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetSaberActive: '%s' is not using a saber!\n",
				ent->targetname);
			return;
		}
	}

	if (active)
	{
		ent->client->ps.SaberActivate();
	}
	else
	{
		ent->client->ps.SaberDeactivate();
	}
}

/*
============
	Name: Q3_SetSaberBladeActive
	Description: Make a specific blade of a specific Saber active or inactive.
	Created: 10/02/02 by Aurelio Reis, Modified: 10/02/02 by Aurelio Reis
	[in]	int entID		The ID of the Entity to modify.
	[in]	int iSaber		Which Saber to modify.
	[in]	int iBlade		Which blade to modify (0 - (NUM_BLADES - 1)).
	[in]	bool bActive	Whether to make it active (default true) or inactive (false).
	[return]	void
============
*/
static void Q3_SetSaberBladeActive(const int entID, const int iSaber, const int iBlade,
	const qboolean bActive = qtrue)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetSaberBladeActive: invalid entID %d\n", entID);
		return;
	}

	if (!ent->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_SetSaberBladeActive: '%s' is not an player/NPC!\n",
			ent->targetname);
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		if (ent->client->ps.weapons[WP_SABER])
		{
			//change to it right now
			if (ent->NPC)
			{
				ChangeWeapon(ent, WP_SABER);
			}
			else
			{
				const gitem_t* item = FindItemForWeapon(WP_SABER);
				RegisterItem(item); //make sure the weapon is cached in case this runs at startup
				G_AddEvent(ent, EV_ITEM_PICKUP, item - bg_itemlist);
				CG_ChangeWeapon(WP_SABER);
			}
			ent->client->ps.weapon = WP_SABER;
			ent->client->ps.weaponstate = WEAPON_READY;
			G_AddEvent(ent, EV_GENERAL_SOUND, G_SoundIndex("sound/weapons/change.wav"));
		}
		else
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_ERROR,
				"Q3_SetSaberBladeActive: '%s' is not using a saber!\n", ent->targetname);
			return;
		}
	}

	// Activate the Blade.
	ent->client->ps.SaberBladeActivate(iSaber, iBlade, bActive);
}

/*
============
Q3_SetNoKnockback
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean noKnockback
============
*/
static void Q3_SetNoKnockback(const int entID, const qboolean noKnockback)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoKnockback: invalid entID %d\n", entID);
		return;
	}

	if (noKnockback)
	{
		ent->flags |= FL_NO_KNOCKBACK;
	}
	else
	{
		ent->flags &= ~FL_NO_KNOCKBACK;
	}
}

static void Q3_SetNoSaberReturn(const int entID, const qboolean NoSaberReturn)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoSaberReturn: invalid entID %d\n", entID);
		return;
	}

	if (NoSaberReturn)
	{
		ent->flags |= FL_NO_SABER_RETURN;
	}
	else
	{
		ent->flags &= ~FL_NO_SABER_RETURN;
	}
}

/*
============
Q3_SetCleanDamagingEnts
  Description	:
  Return type	: void
============
*/
static void Q3_SetCleanDamagingEnts()
{
	for (int i = 0; i < ENTITYNUM_WORLD; i++)
	{
		if (!PInUse(i))
		{
			continue;
		}

		gentity_t* ent = &g_entities[i];

		if (ent)
		{
			if (!ent->client && (ent->s.weapon == WP_DET_PACK || ent->s.weapon == WP_TRIP_MINE || ent->s.weapon ==
				WP_THERMAL))
			{
				// check for a client, otherwise we could remove someone holding this weapon
				G_FreeEntity(ent);
			}
			else if (ent->s.weapon == WP_TURRET && ent->activator && ent->activator->s.number == 0 && !Q_stricmp(
				"PAS", ent->classname))
			{
				// is a player owner personal assault sentry gun.
				G_FreeEntity(ent);
			}
			else if (ent->client && ent->client->NPC_class == CLASS_SEEKER)
			{
				// they blow up when they run out of ammo, so this may as well just do the same.
				G_Damage(ent, ent, ent, nullptr, nullptr, 999, 0, MOD_UNKNOWN);
			}
		}
	}
}

/*
============
Q3_SetInterface
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: const char *data
============
*/
static void Q3_SetInterface(int entID, const char* data)
{
	gi.cvar_set("cg_drawStatus", data);
}

/*
============
Q3_SetLocation
  Description	:
  Return type	: qboolean
  Argument		:  int entID
  Argument		: const char *location
============
*/
static qboolean Q3_SetLocation(const int entID, const char* location)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		return qtrue;
	}

	const char* currentLoc = G_GetLocationForEnt(ent);
	if (currentLoc && currentLoc[0] && Q_stricmp(location, currentLoc) == 0)
	{
		return qtrue;
	}

	ent->message = G_NewString(location);
	return qfalse;
}

/*
============
Q3_SetPlayerLocked
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean locked
============
*/
qboolean player_locked = qfalse;

static void Q3_SetPlayerLocked(int entID, const qboolean locked)
{
	const gentity_t* ent = &g_entities[0];

	player_locked = locked;
	if (ent && ent->client)
	{
		//stop him too
		VectorClear(ent->client->ps.velocity);
	}
}

/*
============
Q3_SetLockPlayerWeapons
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean locked
============
*/
static void Q3_SetLockPlayerWeapons(int entID, const qboolean locked)
{
	gentity_t* ent = &g_entities[0];

	ent->flags &= ~FL_LOCK_PLAYER_WEAPONS;

	if (locked)
	{
		ent->flags |= FL_LOCK_PLAYER_WEAPONS;
	}
}

/*
============
Q3_SetNoImpactDamage
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: qboolean locked
============
*/
static void Q3_SetNoImpactDamage(const int entID, const qboolean noImp)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_SetNoImpactDamage: invalid entID %d\n", entID);
		return;
	}

	ent->flags &= ~FL_NO_IMPACT_DMG;

	if (noImp)
	{
		ent->flags |= FL_NO_IMPACT_DMG;
	}
}

/*
============
Q3_RemoveEnt
  Description	:
  Return type	: void
  Argument		: gentity_t *victim
============
*/
static void Q3_RemoveEnt(gentity_t* victim)
{
	if (!victim || !victim->inuse)
	{
		return;
	}

	if (victim->client)
	{
		if (victim->client->NPC_class == CLASS_VEHICLE)
		{
			//eject everyone out of a vehicle that's about to remove itself
			Vehicle_t* p_veh = victim->m_pVehicle;
			if (p_veh && p_veh->m_pVehicleInfo)
			{
				p_veh->m_pVehicleInfo->EjectAll(p_veh);
			}
		}
		//ClientDisconnect(ent);
		victim->s.eFlags |= EF_NODRAW;
		victim->svFlags &= ~SVF_NPC;
		victim->s.eType = ET_INVISIBLE;
		victim->contents = 0;
		victim->health = 0;
		victim->targetname = nullptr;

		if (victim->NPC && victim->NPC->tempGoal != nullptr)
		{
			G_FreeEntity(victim->NPC->tempGoal);
			victim->NPC->tempGoal = nullptr;
		}
		if (victim->client->ps.saberEntityNum != ENTITYNUM_NONE && victim->client->ps.saberEntityNum > 0)
		{
			if (g_entities[victim->client->ps.saberEntityNum].inuse)
			{
				G_FreeEntity(&g_entities[victim->client->ps.saberEntityNum]);
			}
			victim->client->ps.saberEntityNum = ENTITYNUM_NONE;
		}
		//Disappear in half a second
		victim->e_ThinkFunc = thinkF_G_FreeEntity;
		victim->nextthink = level.time + 500;
		return;
	}
	victim->e_ThinkFunc = thinkF_G_FreeEntity;
	victim->nextthink = level.time + 100;
}

/*
============
MakeOwnerInvis
  Description	:
  Return type	: void
  Argument		: gentity_t *self
============
*/
void MakeOwnerInvis(gentity_t* self)
{
	if (self->owner && self->owner->client)
	{
		self->owner->client->ps.powerups[PW_CLOAKED] = level.time + 500;
	}

	//HACKHGACLHACK!! - MCG
	self->e_ThinkFunc = thinkF_RemoveOwner;
	self->nextthink = level.time + 400;
}

/*
============
MakeOwnerEnergy
  Description	:
  Return type	: void
  Argument		: gentity_t *self
============
*/
void MakeOwnerEnergy(gentity_t* self)
{
	if (self->owner && self->owner->client)
	{
		//
	}

	G_FreeEntity(self);
}

// NOTE! RemoveOwner() is a function used within the entity (a pointer to the function is
// contained). This leads to a "funny" predicament: why is RemoveOwner() here???
// NOTE NOTE! This is also true of Q3_Remove, which should be eliminated as soon as possible.
/*
============
Q3_Remove
  Description	:
  Return type	: void
  Argument		:  int entID
  Argument		: const char *name
============
*/
static void Q3_Remove(const int entID, const char* name)
{
	gentity_t* ent = &g_entities[entID];
	gentity_t* victim;

	if (!Q_stricmp("self", name))
	{
		victim = ent;
		if (!victim)
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_Remove: can't find %s\n", name);
			return;
		}
		Q3_RemoveEnt(victim);
	}
	else if (!Q_stricmp("enemy", name))
	{
		victim = ent->enemy;
		if (!victim)
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_Remove: can't find %s\n", name);
			return;
		}
		Q3_RemoveEnt(victim);
	}
	else
	{
		victim = G_Find(nullptr, FOFS(targetname), name);
		if (!victim)
		{
			Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_Remove: can't find %s\n", name);
			return;
		}

		while (victim)
		{
			Q3_RemoveEnt(victim);
			victim = G_Find(victim, FOFS(targetname), name);
		}
	}
}

static void Q3_setVaderBreath(const int entID, const qboolean usable)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_setVaderBreath: invalid entID %d\n", entID);
		return;
	}

	if (usable)
	{
		gi.cvar_set("cg_setVaderBreath", "1");
	}
	else
	{
		gi.cvar_set("cg_setVaderBreath", "0");
	}
}

static void Q3_setFFAMode(const int entID, const qboolean usable)
{
	const gentity_t* ent = &g_entities[entID];

	// MESSAGE TO MIKE - We need some sort of CVAR to be activated when playing this FFA Mode
	//  If we dont have a cvar it will effect other SP game stuff possibly ???
	// Can you activate g_ffamode in scripts somehow and then put everything in the cvar bracket to avoid any conflictions

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "g_ffamode: invalid entID %d\n", entID);
		return;
	}

	if (usable)
	{
		gi.cvar_set("g_ffamode", "1");
	}
	else
	{
		gi.cvar_set("g_ffamode", "0");
	}
}

static void Q3_setKotorMode(const int entID, const qboolean usable)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_setKotorMode: invalid entID %d\n", entID);
		return;
	}

	if (usable)
	{
		gi.cvar_set("com_kotor", "1");
	}
	else
	{
		gi.cvar_set("com_kotor", "0");
	}
}

static void Q3_setVaderBreathdamaged(const int entID, const qboolean usable)
{
	const gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_setVaderBreathdamaged: invalid entID %d\n", entID);
		return;
	}

	if (usable)
	{
		gi.cvar_set("cg_setVaderBreathdamaged", "1");
	}
	else
	{
		gi.cvar_set("cg_setVaderBreathdamaged", "0");
	}
}

/*
============
RemoveOwner
  Description	:
  Return type	: void
  Argument		: gentity_t *self
============
*/
void RemoveOwner(gentity_t* self)
{
	if (self->owner && self->owner->inuse)
	{
		//I have an owner and they heavn't been freed yet
		Q3_Remove(self->owner->s.number, "self");
	}

	G_FreeEntity(self);
}

static void Q3_DismemberLimb(const int entID, char* hitLocName)
{
	gentity_t* self = &g_entities[entID];
	const int hit_loc = GetIDForString(HLTable, hitLocName);
	vec3_t point;

	if (!self)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "Q3_DismemberLimb: invalid entID %d\n", entID);
		return;
	}

	if (!self->client)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_DismemberLimb: '%s' is not a player/NPC!\n",
			self->targetname);
		return;
	}

	if (!self->ghoul2.size())
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_DismemberLimb: '%s' is not a ghoul model!\n",
			self->targetname);
		return;
	}

	if (hit_loc <= HL_NONE || hit_loc >= HL_MAX)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_ERROR, "Q3_DismemberLimb: '%s' is not a valid hit location!\n",
			hitLocName);
		return;
	}

	switch (hit_loc)
	{
	case HL_FOOT_RT:
		VectorCopy(self->client->renderInfo.footRPoint, point);
		break;
	case HL_FOOT_LT:
		VectorCopy(self->client->renderInfo.footLPoint, point);
		break;
	case HL_LEG_RT:
		G_GetBoltPosition(self, self->kneeRBolt, point);
		break;
	case HL_LEG_LT:
		G_GetBoltPosition(self, self->kneeLBolt, point);
		break;
	case HL_ARM_RT:
	case HL_CHEST_RT:
	case HL_BACK_LT:
		G_GetBoltPosition(self, self->elbowRBolt, point);
		break;
	case HL_ARM_LT:
	case HL_CHEST_LT:
	case HL_BACK_RT:
		G_GetBoltPosition(self, self->elbowLBolt, point);
		break;
	case HL_WAIST:
	case HL_BACK:
	case HL_CHEST:
		VectorCopy(self->client->renderInfo.torsoPoint, point);
		break;
	case HL_HAND_RT:
		VectorCopy(self->client->renderInfo.handRPoint, point);
		break;
	case HL_HAND_LT:
		VectorCopy(self->client->renderInfo.handLPoint, point);
		break;
	case HL_HEAD:
		VectorCopy(self->client->renderInfo.headPoint, point);
		break;
	case HL_GENERIC1:
	case HL_GENERIC2:
	case HL_GENERIC3:
	case HL_GENERIC4:
	case HL_GENERIC5:
	case HL_GENERIC6:
		VectorCopy(self->currentOrigin, point);
		break;
	default:;
	}
	G_DoDismembermentcin(self, point, MOD_SABER, hit_loc, qtrue);
}

/*
-------------------------
VariableDeclared
-------------------------
*/
int CQuake3GameInterface::VariableDeclared(const char* name)
{
	//Check the strings
	const auto vsi = m_varStrings.find(name);

	if (vsi != m_varStrings.end())
		return VTYPE_STRING;

	//Check the floats
	const auto vfi = m_varFloats.find(name);

	if (vfi != m_varFloats.end())
		return VTYPE_FLOAT;

	//Check the vectors
	const auto vvi = m_varVectors.find(name);

	if (vvi != m_varVectors.end())
		return VTYPE_VECTOR;

	return VTYPE_NONE;
}

/*
-------------------------
SetVar
-------------------------
*/

void CQuake3GameInterface::SetVar(int taskID, int entID, const char* type_name, const char* data)
{
	const int vret = VariableDeclared(type_name);
	float float_data = 0.0f;

	if (vret != VTYPE_NONE)
	{
		float val;
		switch (vret)
		{
		case VTYPE_FLOAT:
			//Check to see if increment command
			if ((val = Q3_CheckStringCounterIncrement(data)))
			{
				GetFloatVariable(type_name, &float_data);
				float_data += val;
			}
			else
			{
				float_data = atof(data);
			}
			SetFloatVariable(type_name, float_data);
			break;

		case VTYPE_STRING:
			SetStringVariable(type_name, data);
			break;

		case VTYPE_VECTOR:
			SetVectorVariable(type_name, data);
			break;
		default:;
		}

		return;
	}

	DebugPrint(WL_ERROR, "%s variable or field not found!\n", type_name);
}

/*
-------------------------
GetFloatVariable
-------------------------
*/

int CQuake3GameInterface::GetFloatVariable(const char* name, float* value)
{
	//Check the floats
	const auto vfi = m_varFloats.find(name);

	if (vfi != m_varFloats.end())
	{
		*value = (*vfi).second;
		return true;
	}

	return false;
}

/*
-------------------------
GetStringVariable
-------------------------
*/

int CQuake3GameInterface::GetStringVariable(const char* name, const char** value)
{
	//Check the strings
	const auto vsi = m_varStrings.find(name);

	if (vsi != m_varStrings.end())
	{
		*value = (*vsi).second.c_str();
		return true;
	}

	return false;
}

/*
-------------------------
GetVectorVariable
-------------------------
*/

int CQuake3GameInterface::GetVectorVariable(const char* name, vec3_t value)
{
	//Check the strings
	const auto vvi = m_varVectors.find(name);

	if (vvi != m_varVectors.end())
	{
		const char* str = (*vvi).second.c_str();

		sscanf(str, "%f %f %f", &value[0], &value[1], &value[2]);
		return true;
	}

	return false;
}

/*
-------------------------
InitVariables
-------------------------
*/

void CQuake3GameInterface::InitVariables()
{
	m_varStrings.clear();
	m_varFloats.clear();
	m_varVectors.clear();

	if (m_numVariables > 0)
		DebugPrint(WL_WARNING, "%d residual variables found!\n", m_numVariables);

	m_numVariables = 0;
}

/*
-------------------------
SetVariable_Float
-------------------------
*/

int CQuake3GameInterface::SetFloatVariable(const char* name, const float value)
{
	//Check the floats
	const auto vfi = m_varFloats.find(name);

	if (vfi == m_varFloats.end())
		return VTYPE_FLOAT;

	(*vfi).second = value;

	return true;
}

/*
-------------------------
SetVariable_String
-------------------------
*/

int CQuake3GameInterface::SetStringVariable(const char* name, const char* value)
{
	//Check the strings
	const auto vsi = m_varStrings.find(name);

	if (vsi == m_varStrings.end())
		return false;

	(*vsi).second = value;

	return true;
}

/*
-------------------------
SetVariable_Vector
-------------------------
*/

int CQuake3GameInterface::SetVectorVariable(const char* name, const char* value)
{
	//Check the strings
	const auto vvi = m_varVectors.find(name);

	if (vvi == m_varVectors.end())
		return false;

	(*vvi).second = value;

	return true;
}

/*
-------------------------
VariableSaveFloats
-------------------------
*/

void CQuake3GameInterface::VariableSaveFloats(varFloat_m& fmap)
{
	const int numFloats = fmap.size();

	ojk::SavedGameHelper saved_game(
		gi.saved_game);

	saved_game.write_chunk<int32_t>(
		INT_ID('F', 'V', 'A', 'R'),
		numFloats);

	varFloat_m::iterator vfi;
	STL_ITERATE(vfi, fmap)
	{
		//Save out the map id
		int idSize = strlen((*vfi).first.c_str());

		//Save out the real data
		saved_game.write_chunk<int32_t>(
			INT_ID('F', 'I', 'D', 'L'),
			idSize);

		saved_game.write_chunk(
			INT_ID('F', 'I', 'D', 'S'),
			(*vfi).first.c_str(),
			idSize);

		//Save out the float value
		saved_game.write_chunk<float>(
			INT_ID('F', 'V', 'A', 'L'),
			(*vfi).second);
	}
}

/*
-------------------------
VariableSaveStrings
-------------------------
*/

void CQuake3GameInterface::VariableSaveStrings(varString_m& smap)
{
	const int numStrings = smap.size();

	ojk::SavedGameHelper saved_game(
		gi.saved_game);

	saved_game.write_chunk<int32_t>(
		INT_ID('S', 'V', 'A', 'R'),
		numStrings);

	varString_m::iterator vsi;
	STL_ITERATE(vsi, smap)
	{
		//Save out the map id
		int idSize = strlen((*vsi).first.c_str());

		//Save out the real data
		saved_game.write_chunk<int32_t>(
			INT_ID('S', 'I', 'D', 'L'),
			idSize);

		saved_game.write_chunk(
			INT_ID('S', 'I', 'D', 'S'),
			(*vsi).first.c_str(),
			idSize);

		//Save out the string value
		idSize = strlen((*vsi).second.c_str());

		saved_game.write_chunk<int32_t>(
			INT_ID('S', 'V', 'S', 'Z'),
			idSize);

		saved_game.write_chunk(
			INT_ID('S', 'V', 'A', 'L'),
			(*vsi).second.c_str(),
			idSize);
	}
}

/*
-------------------------
VariableSave
-------------------------
*/

int CQuake3GameInterface::VariableSave()
{
	VariableSaveFloats(m_varFloats);
	VariableSaveStrings(m_varStrings);
	VariableSaveStrings(m_varVectors);

	return qtrue;
}

/*
-------------------------
VariableLoadFloats
-------------------------
*/

void CQuake3GameInterface::VariableLoadFloats(varFloat_m& fmap)
{
	int numFloats = 0;
	char tempBuffer[1024];

	ojk::SavedGameHelper saved_game(
		gi.saved_game);

	saved_game.read_chunk<int32_t>(
		INT_ID('F', 'V', 'A', 'R'),
		numFloats);

	for (int i = 0; i < numFloats; i++)
	{
		int idSize = 0;

		saved_game.read_chunk<int32_t>(
			INT_ID('F', 'I', 'D', 'L'),
			idSize);

		if (idSize < 0 || static_cast<size_t>(idSize) >= sizeof tempBuffer)
		{
			G_Error("invalid length for FIDS string in save game: %d bytes\n", idSize);
		}

		saved_game.read_chunk(
			INT_ID('F', 'I', 'D', 'S'),
			tempBuffer,
			idSize);

		tempBuffer[idSize] = 0;

		float val = 0.0F;

		saved_game.read_chunk<float>(
			INT_ID('F', 'V', 'A', 'L'),
			val);

		DeclareVariable(TK_FLOAT, reinterpret_cast<const char*>(&tempBuffer));
		SetFloatVariable(reinterpret_cast<const char*>(&tempBuffer), val);
	}
}

/*
-------------------------
VariableLoadStrings
-------------------------
*/

void CQuake3GameInterface::VariableLoadStrings(const int type, varString_m& fmap)
{
	int numFloats = 0;
	char tempBuffer[1024];
	char tempBuffer2[1024];

	ojk::SavedGameHelper saved_game(
		gi.saved_game);

	saved_game.read_chunk<int32_t>(
		INT_ID('S', 'V', 'A', 'R'),
		numFloats);

	for (int i = 0; i < numFloats; i++)
	{
		int idSize = 0;

		saved_game.read_chunk<int32_t>(
			INT_ID('S', 'I', 'D', 'L'),
			idSize);

		if (idSize < 0 || static_cast<size_t>(idSize) >= sizeof tempBuffer)
		{
			G_Error("invalid length for SIDS string in save game: %d bytes\n", idSize);
		}

		saved_game.read_chunk(
			INT_ID('S', 'I', 'D', 'S'),
			tempBuffer,
			idSize);

		tempBuffer[idSize] = 0;

		saved_game.read_chunk<int32_t>(
			INT_ID('S', 'V', 'S', 'Z'),
			idSize);

		if (idSize < 0 || static_cast<size_t>(idSize) >= sizeof tempBuffer2)
		{
			G_Error("invalid length for SVAL string in save game: %d bytes\n", idSize);
		}

		saved_game.read_chunk(
			INT_ID('S', 'V', 'A', 'L'),
			tempBuffer2,
			idSize);

		tempBuffer2[idSize] = 0;

		switch (type)
		{
		case TK_STRING:
			DeclareVariable(TK_STRING, reinterpret_cast<const char*>(&tempBuffer));
			SetStringVariable(reinterpret_cast<const char*>(&tempBuffer),
				reinterpret_cast<const char*>(&tempBuffer2));
			break;

		case TK_VECTOR:
			DeclareVariable(TK_VECTOR, reinterpret_cast<const char*>(&tempBuffer));
			SetVectorVariable(reinterpret_cast<const char*>(&tempBuffer),
				reinterpret_cast<const char*>(&tempBuffer2));
			break;
		default:;
		}
	}
}

/*
-------------------------
VariableLoad
-------------------------
*/

int CQuake3GameInterface::VariableLoad()
{
	InitVariables();

	VariableLoadFloats(m_varFloats);

	VariableLoadStrings(TK_STRING, m_varStrings);

	VariableLoadStrings(TK_VECTOR, m_varVectors);

	return qfalse;
}

// Static Singleton Instance.
CQuake3GameInterface* CQuake3GameInterface::m_pInstance = nullptr;

// Destructor.
IGameInterface::~IGameInterface() = default;

int IGameInterface::s_IcarusFlavorsNeeded = 1;

// Get this Interface Instance.
IGameInterface* IGameInterface::GetGame(int flavor)
{
	// If no instance exists, create one...
	if (!CQuake3GameInterface::m_pInstance)
	{
		CQuake3GameInterface::m_pInstance = new CQuake3GameInterface();
	}

	return CQuake3GameInterface::m_pInstance;
}

// Destroy the game Instance.
void IGameInterface::Destroy()
{
	if (CQuake3GameInterface::m_pInstance)
	{
		delete CQuake3GameInterface::m_pInstance;
		CQuake3GameInterface::m_pInstance = nullptr;
	}
}

// Constructor.
CQuake3GameInterface::CQuake3GameInterface() : IGameInterface()
{
	m_ScriptList.clear();
	m_EntityList.clear();

	m_numVariables = 0;

	m_entFilter = -1;

	player_locked = qfalse;

	gclient_t* client = &level.clients[0];
	memset(&client->sess, 0, sizeof client->sess);
}

// Destructor.
CQuake3GameInterface::~CQuake3GameInterface()
{
	gentity_t* ent = &g_entities[0];

	// Release all entities Icarus resources.
	for (int i = 0; i < globals.num_entities; i++, ent++)
	{
		if (!ent->inuse)
			continue;

		FreeEntity(ent);
	}

	// Clear out all precached script's.
	for (const auto& iterScript : m_ScriptList)
	{
		CQuake3GameInterface::Free(iterScript.second->buffer);
		delete iterScript.second;
	}

	m_ScriptList.clear();
	m_EntityList.clear();
}

// Initialize an Entity by ID.
bool CQuake3GameInterface::InitEntity(gentity_t* pEntity)
{
	//Make sure this is a fresh entity.
	assert(pEntity->m_iIcarusID == IIcarusInterface::ICARUS_INVALID);

	if (pEntity->m_iIcarusID != IIcarusInterface::ICARUS_INVALID)
		return false;

	// Get an Icarus ID.
	pEntity->m_iIcarusID = IIcarusInterface::GetIcarus()->GetIcarusID(pEntity->s.number);

	// Initialize all taskIDs to -1
	memset(&pEntity->taskID, -1, sizeof pEntity->taskID);

	// Add this entity to a map of valid associated entity's for quick retrieval later.
	AssociateEntity(pEntity);

	//Precache all the entity's scripts.
	PrecacheEntity(pEntity);

	return false;
}

// Free an Entity by ID.
void CQuake3GameInterface::FreeEntity(gentity_t* pEntity)
{
	//Make sure the entity is valid
	if (pEntity->m_iIcarusID == IIcarusInterface::ICARUS_INVALID)
		return;

	// Remove the Entity from the Entity map so that when their g_entity index is reused,
	// ICARUS doesn't try to affect the new (incorrect) pEntity.
	if VALIDSTRING(pEntity->script_targetname)
	{
		char temp[1024];

		strncpy(temp, pEntity->script_targetname, 1023);
		temp[1023] = 0;

		const auto it = m_EntityList.find(Q_strupr(temp));

		if (it != m_EntityList.end())
		{
			m_EntityList.erase(it);
		}
	}

	// Delete the Icarus ID, but lets not construct icarus to do it
	if (IIcarusInterface::GetIcarus(0, false))
	{
		IIcarusInterface::GetIcarus()->DeleteIcarusID(pEntity->m_iIcarusID);
	}
}

// Determines whether or not a Game Element needs ICARUS information.
bool CQuake3GameInterface::ValidEntity(gentity_t* pEntity)
{
	// Targeted by a script.
	if (pEntity->script_targetname && pEntity->script_targetname[0])
		return true;

	// Potentially able to call a script.
	for (const auto& i : pEntity->behaviorSet)
	{
		if VALIDSTRING(i)
		{
			//Com_Printf( "WARNING: Entity %d (%s) has behaviorSet but no script_targetname -- using targetname\n", pEntity->s.number, pEntity->targetname );
			pEntity->script_targetname = G_NewString(pEntity->targetname);
			return true;
		}
	}

	return false;
}

// Associate the entity's id and name so that it can be referenced later.
void CQuake3GameInterface::AssociateEntity(const gentity_t* pEntity)
{
	char temp[1024];

	if (VALIDSTRING(pEntity->script_targetname) == false)
		return;

	strncpy(temp, pEntity->script_targetname, 1023);
	temp[1023] = 0;

	m_EntityList[Q_strupr(temp)] = pEntity->s.number;
}

// Make a valid script name.
int CQuake3GameInterface::MakeValidScriptName(char** strScriptName)
{
	if (!Q_stricmp(*strScriptName, "NULL") || !Q_stricmp(*strScriptName, "default"))
		return 0;

	// ensure "scripts" (Q3_SCRIPT_DIR), which will be missing if this was called recursively...
	// MAX_FILENAME_LENGTH should really be MAX_QPATH (and 64 bytes instead of 1024), but this fits the rest of the code
	char sFilename[MAX_FILENAME_LENGTH];

	if (!Q_stricmpn(*strScriptName, Q3_SCRIPT_DIR, strlen(Q3_SCRIPT_DIR)))
	{
		Q_strncpyz(sFilename, *strScriptName, sizeof sFilename);
	}
	else
	{
		Q_strncpyz(sFilename, va("%s/%s", Q3_SCRIPT_DIR, *strScriptName), sizeof sFilename);
	}

	return 1;
}

// First looks to see if a script has already been loaded, if so, return SCRIPT_ALREADYREGISTERED. If a script has
// NOT been already cached, that script is loaded and the return is SCRIPT_REGISTERED. If a script could not
// be found cached and could not be loaded we return SCRIPT_COULDNOTREGISTER.
int CQuake3GameInterface::RegisterScript(const char* strFileName, void** ppBuf, int& iLength)
{
	// If the file name is invalid, leave.
	if (!strFileName || strFileName[0] == '\0' || !Q_stricmp(strFileName, "NULL") || !Q_stricmp(
		strFileName, "default"))
		return SCRIPT_COULDNOTREGISTER;

	// Ensure "scripts" (Q3_SCRIPT_DIR), which will be missing if this was called recursively...
	// MAX_FILENAME_LENGTH should really be MAX_QPATH (and 64 bytes instead of 1024), but this fits the rest of the code
	char sFilename[MAX_FILENAME_LENGTH];

	if (!Q_stricmpn(strFileName, Q3_SCRIPT_DIR, strlen(Q3_SCRIPT_DIR)))
	{
		Q_strncpyz(sFilename, strFileName, sizeof sFilename);
	}
	else
	{
		Q_strncpyz(sFilename, va("%s/%s", Q3_SCRIPT_DIR, strFileName), sizeof sFilename);
	}

	// Make sure this isn't already cached
	const auto ei = m_ScriptList.find(strFileName);

	// If we found the script (didn't get to the end of the list), return true.
	if (ei != m_ScriptList.end())
	{
		// Send back the script buffer info.
		*ppBuf = (*ei).second->buffer;
		iLength = (*ei).second->length;

		return SCRIPT_ALREADYREGISTERED;
	}

	// Prepare the name with the extension.
	char newname[MAX_FILENAME_LENGTH]{};
	sprintf(static_cast<char*>(newname), "%s%s", sFilename, IBI_EXT);

	// Read the Script File into the Buffer.
	char* pBuf = nullptr;
	iLength = gi.FS_ReadFile(newname, reinterpret_cast<void**>(&pBuf));

	if (iLength <= 0)
	{
		// File not found, but keep quiet during interrogate stage, because of stuff like BS_RUN_AND_SHOOT as scriptname
		//
		/*		if (!bCalledDuringInterrogate)
				{
					Com_Printf(S_COLOR_RED"Could not open file '%s'\n", newname );
				}*/
		return SCRIPT_COULDNOTREGISTER;
	}

	// Allocate a new pscript (Script Buffer).
	const auto pscript = new pscript_t;

	// Allocate a buffer of the size we need then mem copy over the buffer.
	pscript->buffer = static_cast<char*>(Malloc(iLength));
	memcpy(pscript->buffer, pBuf, iLength);
	pscript->length = iLength;

	// We (mem)copied the data over so release the original buffer.
	gi.FS_FreeFile(pBuf);

	// Keep track of the buffer still.
	*ppBuf = pscript->buffer;

	// Add the Script Buffer to the STL map.
	m_ScriptList[strFileName] = pscript;

	return SCRIPT_REGISTERED;
}

// Precache all the resources needed by a Script and it's Entity (or vice-versa).
int CQuake3GameInterface::PrecacheEntity(const gentity_t* pEntity)
{
	for (const auto i : pEntity->behaviorSet)
	{
		if (i == nullptr)
			continue;

		if (GetIDForString(BSTable, i) == -1)
		{
			//not a behavior set
			char* pBuf = nullptr;
			int iLength = 0;

			// Try to register this script.
			if (RegisterScript(i, reinterpret_cast<void**>(&pBuf), iLength) != SCRIPT_COULDNOTREGISTER)
			{
				assert(pBuf);
				assert(iLength);

				if (pBuf != nullptr && iLength > 0)
					// Tell Icarus to precache the script data.
					IIcarusInterface::GetIcarus()->Precache(pBuf, iLength);
			}
			else
				// otherwise try the next guy, maybe It will work.
				continue;
		}
	}

	return 0;
}

// Run the script.
void CQuake3GameInterface::RunScript(const gentity_t* pEntity, const char* strScriptName)
{
	char* pBuf = nullptr;
	int iLength = 0;

	switch (RegisterScript(strScriptName, reinterpret_cast<void**>(&pBuf), iLength))
	{
		// If could not be loaded, leave!
	case SCRIPT_COULDNOTREGISTER:
		DebugPrint(WL_WARNING, "RunScript: Script was not found and could not be loaded!!! %s\n", strScriptName);
		return;

		// We loaded the script and registered it, so run it!
	case SCRIPT_REGISTERED:
	case SCRIPT_ALREADYREGISTERED:
		assert(pBuf);
		assert(iLength);
		if (IIcarusInterface::GetIcarus()->Run(pEntity->m_iIcarusID, pBuf, iLength) !=
			IIcarusInterface::ICARUS_INVALID)
			DebugPrint(WL_VERBOSE, "%d Script %s executed by %s %s\n", level.time, strScriptName,
				pEntity->classname, pEntity->targetname);
	default:;
	}
}

void CQuake3GameInterface::Svcmd()
{
	const char* cmd = gi.argv(1);

	if (Q_stricmp(cmd, "log") == 0)
	{
		g_ICARUSDebug->integer = WL_DEBUG;
		if (VALIDSTRING(gi.argv(2)))
		{
			const gentity_t* ent = G_Find(nullptr, FOFS(script_targetname), gi.argv(2));

			if (ent == nullptr)
			{
				Com_Printf("Entity \"%s\" not found!\n", gi.argv(2));
				return;
			}

			//Start logging
			Com_Printf("Logging ICARUS info for entity %s\n", gi.argv(2));

			m_entFilter = ent->s.number == m_entFilter ? -1 : ent->s.number;
		}

		Com_Printf("Logging ICARUS info for all entities\n");
	}
}

// Get the current Game flavor.
int CQuake3GameInterface::GetFlavor()
{
	return 0;
}

// Reads in a file and attaches the script directory properly.
int CQuake3GameInterface::LoadFile(const char* name, void** buf)
{
	int iLength = 0;

	// Register the Script (or retrieve it).
	RegisterScript(name, buf, iLength);

	return iLength;
}

// Prints a message in the center of the screen.
void CQuake3GameInterface::CenterPrint(const char* format, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, format);
	Q_vsnprintf(text, sizeof text, format, argptr);
	va_end(argptr);

	// FIXME: added '!' so you can print something that's hasn't been precached, '@' searches only for precache text
	// this is just a TEMPORARY placeholder until objectives are in!!!  -- dmv 11/26/01

	if (text[0] == '@' || text[0] == '!') // It's a key
	{
		if (text[0] == '!')
		{
			gi.SendServerCommand(0, "cp \"%s\"", text + 1);
			return;
		}

		gi.SendServerCommand(0, "cp \"%s\"", text);
	}

	DebugPrint(WL_VERBOSE, "%s\n", text); // Just a developers note
}

// Print a debug message.
void CQuake3GameInterface::DebugPrint(const e_DebugPrintLevel level, const char* format, ...)
{
	//Don't print messages they don't want to see
	if (g_ICARUSDebug->integer < level)
		return;

	va_list argptr;
	char text[1024];

	va_start(argptr, format);
	Q_vsnprintf(text, sizeof text, format, argptr);
	va_end(argptr);

	//Add the color formatting
	switch (level)
	{
	case WL_ERROR:
		Com_Printf(S_COLOR_RED"ERROR: %s", text);
		break;

	case WL_WARNING:
		Com_Printf(S_COLOR_YELLOW"WARNING: %s", text);
		break;

	case WL_DEBUG:
	{
		int entNum;

		sscanf(text, "%d", &entNum);

		if (m_entFilter >= 0 && m_entFilter != entNum)
			return;

		auto buffer = text;
		buffer += 5;

		if (entNum < 0 || entNum >= MAX_GENTITIES)
			entNum = 0;

		Com_Printf(S_COLOR_BLUE"DEBUG: %s(%d): %s\n", g_entities[entNum].script_targetname, entNum, buffer);
		break;
	}
	default:
	case WL_VERBOSE:
		Com_Printf(S_COLOR_GREEN"INFO: %s", text);
		break;
	}
}

//Gets the current time
unsigned int CQuake3GameInterface::GetTime()
{
	return level.time;
}

//	 DWORD	CQuake3GameInterface::GetTimeScale(void ) {}

// NOTE: This extern does not really fit here, fix later please...
extern void G_SoundBroadcast(const gentity_t* ent, int sound_index);
// Plays a sound from an entity.
int CQuake3GameInterface::PlayIcarusSound(const int taskID, const int entID, const char* name, const char* channel)
{
	gentity_t* ent = &g_entities[entID];
	char finalName[MAX_QPATH];
	soundChannel_t voice_chan = CHAN_VOICE; // set a default so the compiler doesn't bitch
	qboolean type_voice = qfalse;

	Q_strncpyz(finalName, name, MAX_QPATH);
	Q_strlwr(finalName);
	G_AddSexToPlayerString(finalName, qtrue);

	COM_StripExtension(finalName, finalName, sizeof finalName);

	const int soundHandle = G_SoundIndex(finalName);
	bool bBroadcast = false;

	if (Q_stricmp(channel, "CHAN_ANNOUNCER") == 0 || ent->classname && Q_stricmp(
		"target_scriptrunner", ent->classname) == 0)
	{
		bBroadcast = true;
	}

	// moved here from further down so I can easily check channel-type without code dup...
	//
	if (Q_stricmp(channel, "CHAN_VOICE") == 0)
	{
		voice_chan = CHAN_VOICE;
		type_voice = qtrue;
	}
	else if (Q_stricmp(channel, "CHAN_VOICE_ATTEN") == 0)
	{
		voice_chan = CHAN_VOICE_ATTEN;
		type_voice = qtrue;
	}
	else if (Q_stricmp(channel, "CHAN_VOICE_GLOBAL") == 0)
		// this should broadcast to everyone, put only casue animation on G_SoundOnEnt...
	{
		voice_chan = CHAN_VOICE_GLOBAL;
		type_voice = qtrue;
		bBroadcast = true;
	}

	// if we're in-camera, check for skipping cinematic and ifso, no subtitle print (since screen is not being
	//	updated anyway during skipping). This stops leftover subtitles being left onscreen after unskipping.
	//
	if (!in_camera ||
		(!g_skippingcin || !g_skippingcin->integer)
		) // paranoia towards project end <g>
	{
		// Text on
		// certain NPC's we always want to use subtitles regardless of subtitle setting
		if (g_subtitles->integer == 1 || ent->NPC && ent->NPC->scriptFlags & SCF_USE_SUBTITLES) // Show all text
		{
			if (in_camera) // Cinematic
			{
				gi.SendServerCommand(0, "ct \"%s\" %i", finalName, soundHandle);
			}
			else //if (precacheWav[i].speaker==SP_NONE)	//  lower screen text
			{
				const gentity_t* ent2 = &g_entities[0];
				// the numbers in here were either the original ones Bob entered (350), or one arrived at from checking the distance Chell stands at in stasis2 by the computer core that was submitted as a bug report...
				//
				if (bBroadcast || DistanceSquared(ent->currentOrigin, ent2->currentOrigin) < (
					voice_chan == CHAN_VOICE_ATTEN ? 350 * 350 : 1200 * 1200))
				{
					gi.SendServerCommand(0, "ct \"%s\" %i", finalName, soundHandle);
				}
			}
		}
		// Cinematic only
		else if (g_subtitles->integer == 2) // Show only talking head text and CINEMATIC
		{
			if (in_camera) // Cinematic text
			{
				gi.SendServerCommand(0, "ct \"%s\" %i", finalName, soundHandle);
			}
		}
	}

	if (type_voice)
	{
		if (g_timescale->value > 1.0f)
		{
			//Skip the damn sound!
			return qtrue;
		}
		//This the voice channel
		G_SoundOnEnt(ent, voice_chan, finalName);
		//Remember we're waiting for this
		Q3_TaskIDSet(ent, TID_CHAN_VOICE, taskID);
		//do not task_return complete
		//	if( voice_chan == CHAN_VOICE_GLOBAL )
		//	{
		//		G_SoundBroadcast( ent, soundHandle );
		//	}
		return qfalse;
	}

	if (bBroadcast)
	{
		//Broadcast the sound
		G_SoundBroadcast(ent, soundHandle);
	}
	else
	{
		G_Sound(ent, soundHandle);
	}

	return qtrue;
}

// Lerps the origin and angles of an entity to the destination values.
void CQuake3GameInterface::Lerp2Pos(const int taskID, const int entID, vec3_t origin, vec3_t angles, float duration)
{
	gentity_t* ent = &g_entities[entID];

	if (!ent)
	{
		DebugPrint(WL_WARNING, "Lerp2Pos: invalid entID %d\n", entID);
		return;
	}

	if (ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0)
	{
		DebugPrint(WL_ERROR, "Lerp2Pos: ent %d is NOT a mover!\n", entID);
		return;
	}

	if (ent->s.eType != ET_MOVER)
	{
		ent->s.eType = ET_MOVER;
	}

	//Don't allow a zero duration
	if (duration == 0)
		duration = 1;

	//
	// Movement

	moverState_t moverState = ent->moverState;

	if (moverState == MOVER_POS1 || moverState == MOVER_2TO1)
	{
		VectorCopy(ent->currentOrigin, ent->pos1);
		VectorCopy(origin, ent->pos2);

		if (moverState == MOVER_POS1)
		{
			//open the portal
			if (ent->svFlags & SVF_MOVER_ADJ_AREA_PORTALS)
			{
				gi.AdjustAreaPortalState(ent, qtrue);
			}
		}

		moverState = MOVER_1TO2;
	}
	else /*if ( moverState == MOVER_POS2 || moverState == MOVER_1TO2 )*/
	{
		VectorCopy(ent->currentOrigin, ent->pos2);
		VectorCopy(origin, ent->pos1);

		moverState = MOVER_2TO1;
	}

	InitMoverTrData(ent);

	ent->s.pos.trDuration = duration;

	// start it going
	MatchTeam(ent, moverState, level.time);
	//SetMoverState( ent, moverState, level.time );

	//Only do the angles if specified
	if (angles != nullptr)
	{
		vec3_t ang{};
		//
		// Rotation

		for (int i = 0; i < 3; i++)
		{
			ang[i] = AngleDelta(angles[i], ent->currentAngles[i]);
			ent->s.apos.trDelta[i] = ang[i] / (duration * 0.001f);
		}

		VectorCopy(ent->currentAngles, ent->s.apos.trBase);

		if (ent->alt_fire)
		{
			ent->s.apos.trType = TR_LINEAR_STOP;
		}
		else
		{
			ent->s.apos.trType = TR_NONLINEAR_STOP;
		}
		ent->s.apos.trDuration = duration;

		ent->s.apos.trTime = level.time;

		ent->e_ReachedFunc = reachedF_moveAndRotateCallback;
		Q3_TaskIDSet(ent, TID_ANGLE_FACE, taskID);
	}
	else
	{
		//Setup the last bits of information
		ent->e_ReachedFunc = reachedF_moverCallback;
	}

	if (ent->damage)
	{
		ent->e_BlockedFunc = blockedF_Blocked_Mover;
	}

	Q3_TaskIDSet(ent, TID_MOVE_NAV, taskID);
	// starting sound
	G_PlayDoorLoopSound(ent);
	G_PlayDoorSound(ent, BMS_START); //??

	gi.linkentity(ent);
}

//	 void	CQuake3GameInterface::Lerp2Origin( int taskID, int entID, vec3_t origin, float duration ) {}

// Lerps the angles to the destination value.
void CQuake3GameInterface::Lerp2Angles(const int taskID, const int entID, vec3_t angles, const float duration)
{
	gentity_t* ent = &g_entities[entID];
	vec3_t ang{};

	if (!ent)
	{
		DebugPrint(WL_WARNING, "Lerp2Angles: invalid entID %d\n", entID);
		return;
	}

	if (ent->client || ent->NPC || Q_stricmp(ent->classname, "target_scriptrunner") == 0)
	{
		DebugPrint(WL_ERROR, "Lerp2Angles: ent %d is NOT a mover!\n", entID);
		return;
	}

	//If we want an instant move, don't send 0...
	ent->s.apos.trDuration = duration > 0 ? duration : 1;

	for (int i = 0; i < 3; i++)
	{
		ang[i] = AngleSubtract(angles[i], ent->currentAngles[i]);
		ent->s.apos.trDelta[i] = ang[i] / (ent->s.apos.trDuration * 0.001f);
	}

	VectorCopy(ent->currentAngles, ent->s.apos.trBase);

	if (ent->alt_fire)
	{
		ent->s.apos.trType = TR_LINEAR_STOP;
	}
	else
	{
		ent->s.apos.trType = TR_NONLINEAR_STOP;
	}

	ent->s.apos.trTime = level.time;

	Q3_TaskIDSet(ent, TID_ANGLE_FACE, taskID);

	//ent->e_ReachedFunc = reachedF_NULL;
	ent->e_ThinkFunc = thinkF_anglerCallback;
	ent->nextthink = level.time + duration;

	// starting sound
	G_PlayDoorLoopSound(ent);
	G_PlayDoorSound(ent, BMS_START); //??

	gi.linkentity(ent);
}

// Gets the value of a tag by the give name.
int CQuake3GameInterface::GetTag(const int entID, const char* name, const int lookup, vec3_t info)
{
	const gentity_t* ent = &g_entities[entID];

	VALIDATEB(ent);

	switch (lookup)
	{
	case TYPE_ORIGIN:
		//return TAG_GetOrigin( ent->targetname, name, info );
		return TAG_GetOrigin(ent->ownername, name, info);

	case TYPE_ANGLES:
		//return TAG_GetAngles( ent->targetname, name, info );
		return TAG_GetAngles(ent->ownername, name, info);
	default:;
	}

	return false;
}

void CQuake3GameInterface::Set(int taskID, int entID, const char* type_name, const char* data)
{
	gentity_t* ent = &g_entities[entID];
	float float_data;
	int int_data;
	vec3_t vector_data{};

	// eezstreet: Add support for cvars getting modified thru ICARUS script
	if (!Q_stricmpn(type_name, "cvar_", 5) &&
		strlen(type_name) > 5)
	{
		gi.cvar_set(type_name + 5, data);
		return;
	}

	//Set this for callbacks
	const int toSet = GetIDForString(setTable, type_name);

	//TODO: Throw in a showscript command that will list each command and what they're doing...
	//		maybe as simple as printing that line of the script to the console preceeded by the person's name?
	//		showscript can take any number of targetnames or "all"?  Groupname?
	switch (toSet)
	{
	case SET_ORIGIN:
		sscanf(data, "%f %f %f", &vector_data[0], &vector_data[1], &vector_data[2]);
		G_SetOrigin(ent, vector_data);
		if (Q_strncmp("NPC_", ent->classname, 4) == 0)
		{
			//hack for moving spawners
			VectorCopy(vector_data, ent->s.origin);
		}
		if (ent->client)
		{
			//clear jump start positions so we don't take damage when we land...
			ent->client->ps.jumpZStart = ent->client->ps.forceJumpZStart = vector_data[2];
		}
		gi.linkentity(ent);
		break;

	case SET_TELEPORT_DEST:
		sscanf(data, "%f %f %f", &vector_data[0], &vector_data[1], &vector_data[2]);
		if (!Q3_SetTeleportDest(entID, vector_data))
		{
			Q3_TaskIDSet(ent, TID_MOVE_NAV, taskID);
			return;
		}
		break;

	case SET_COPY_ORIGIN:
		Q3_SetCopyOrigin(entID, data);
		break;

	case SET_ANGLES:
		//Q3_SetAngles( entID, *(vec3_t *) data);
		sscanf(data, "%f %f %f", &vector_data[0], &vector_data[1], &vector_data[2]);
		Q3_SetAngles(entID, vector_data);
		break;

	case SET_XVELOCITY:
		float_data = atof(data);
		Q3_SetVelocity(entID, 0, float_data);
		break;

	case SET_YVELOCITY:
		float_data = atof(data);
		Q3_SetVelocity(entID, 1, float_data);
		break;

	case SET_ZVELOCITY:
		float_data = atof(data);
		Q3_SetVelocity(entID, 2, float_data);
		break;

	case SET_Z_OFFSET:
		float_data = atof(data);
		Q3_SetOriginOffset(entID, 2, float_data);
		break;

	case SET_ENEMY:
		Q3_SetEnemy(entID, data);
		break;

	case SET_LEADER:
		Q3_SetLeader(entID, data);
		break;

	case SET_NAVGOAL:
		if (Q3_SetNavGoal(entID, data))
		{
			Q3_TaskIDSet(ent, TID_MOVE_NAV, taskID);
			return; //Don't call it back
		}
		break;

	case SET_ANIM_UPPER:
		if (Q3_SetAnimUpper(entID, data))
		{
			Q3_TaskIDClear(&ent->taskID[TID_ANIM_BOTH]); //We only want to wait for the top
			Q3_TaskIDSet(ent, TID_ANIM_UPPER, taskID);
			return; //Don't call it back
		}
		break;

	case SET_ANIM_LOWER:
		if (Q3_SetAnimLower(entID, data))
		{
			Q3_TaskIDClear(&ent->taskID[TID_ANIM_BOTH]); //We only want to wait for the bottom
			Q3_TaskIDSet(ent, TID_ANIM_LOWER, taskID);
			return; //Don't call it back
		}
		break;

	case SET_ANIM_BOTH:
	{
		int both = 0;
		if (Q3_SetAnimUpper(entID, data))
		{
			Q3_TaskIDSet(ent, TID_ANIM_UPPER, taskID);
			both++;
		}
		else
		{
			DebugPrint(WL_ERROR, "SetAnimUpper: %s does not have anim %s!\n", ent->targetname, data);
		}
		if (Q3_SetAnimLower(entID, data))
		{
			Q3_TaskIDSet(ent, TID_ANIM_LOWER, taskID);
			both++;
		}
		else
		{
			DebugPrint(WL_ERROR, "SetAnimLower: %s does not have anim %s!\n", ent->targetname, data);
		}
		if (both >= 2)
		{
			Q3_TaskIDSet(ent, TID_ANIM_BOTH, taskID);
		}
		if (both)
		{
			return; //Don't call it back
		}
	}
	break;

	case SET_ANIM_HOLDTIME_LOWER:
		int_data = atoi(data);
		Q3_SetAnimHoldTime(entID, int_data, qtrue);
		Q3_TaskIDClear(&ent->taskID[TID_ANIM_BOTH]); //We only want to wait for the bottom
		Q3_TaskIDSet(ent, TID_ANIM_LOWER, taskID);
		return; //Don't call it back

	case SET_ANIM_HOLDTIME_UPPER:
		int_data = atoi(data);
		Q3_SetAnimHoldTime(entID, int_data, qfalse);
		Q3_TaskIDClear(&ent->taskID[TID_ANIM_BOTH]); //We only want to wait for the top
		Q3_TaskIDSet(ent, TID_ANIM_UPPER, taskID);
		return; //Don't call it back

	case SET_ANIM_HOLDTIME_BOTH:
		int_data = atoi(data);
		Q3_SetAnimHoldTime(entID, int_data, qfalse);
		Q3_SetAnimHoldTime(entID, int_data, qtrue);
		Q3_TaskIDSet(ent, TID_ANIM_BOTH, taskID);
		Q3_TaskIDSet(ent, TID_ANIM_UPPER, taskID);
		Q3_TaskIDSet(ent, TID_ANIM_LOWER, taskID);
		return; //Don't call it back

	case SET_PLAYER_TEAM:
		Q3_SetPlayerTeam(entID, data);
		break;

	case SET_ENEMY_TEAM:
		Q3_SetEnemyTeam(entID, data);
		break;

	case SET_HEALTH:
		int_data = atoi(data);
		Q3_SetHealth(entID, int_data);
		break;

	case SET_ARMOR:
		int_data = atoi(data);
		Q3_SetArmor(entID, int_data);
		break;

	case SET_BEHAVIOR_STATE:
		if (!Q3_SetBState(entID, data))
		{
			Q3_TaskIDSet(ent, TID_BSTATE, taskID);
			return; //don't complete
		}
		break;

	case SET_DEFAULT_BSTATE:
		Q3_SetDefaultBState(entID, data);
		break;

	case SET_TEMP_BSTATE:
		if (!Q3_SetTempBState(entID, data))
		{
			Q3_TaskIDSet(ent, TID_BSTATE, taskID);
			return; //don't complete
		}
		break;

	case SET_CAPTURE:
		Q3_SetCaptureGoal(entID, data);
		break;

	case SET_DPITCH:
		//FIXME: make these set tempBehavior to BS_FACE and await completion?  Or set lockedDesiredPitch/Yaw and aimTime?
		float_data = atof(data);
		Q3_SetDPitch(entID, float_data);
		Q3_TaskIDSet(ent, TID_ANGLE_FACE, taskID);
		return;

	case SET_DYAW:
		float_data = atof(data);
		Q3_SetDYaw(entID, float_data);
		Q3_TaskIDSet(ent, TID_ANGLE_FACE, taskID);
		return;

	case SET_EVENT:
		Q3_SetEvent(entID, data);
		break;

	case SET_VIEWTARGET:
		Q3_SetViewTarget(entID, data);
		Q3_TaskIDSet(ent, TID_ANGLE_FACE, taskID);
		return;

	case SET_WATCHTARGET:
		Q3_SetWatchTarget(entID, data);
		break;

	case SET_VIEWENTITY:
		Q3_SetViewEntity(entID, data);
		break;

	case SET_LOOPSOUND:
		Q3_SetLoopSound(entID, data);
		break;

	case SET_ICARUS_FREEZE:
	case SET_ICARUS_UNFREEZE:
		Q3_SetICARUSFreeze(entID, data, static_cast<qboolean>(toSet == SET_ICARUS_FREEZE));
		break;

	case SET_WEAPON:
		Q3_SetWeapon(entID, data);
		break;

	case SET_ITEM:
		Q3_SetItem(entID, data);
		break;

	case SET_WALKSPEED:
		int_data = atoi(data);
		Q3_SetWalkSpeed(entID, int_data);
		break;

	case SET_RUNSPEED:
		int_data = atoi(data);
		Q3_SetRunSpeed(entID, int_data);
		break;

	case SET_WIDTH:
		int_data = atoi(data);
		Q3_SetWidth(entID, int_data);
		return;

	case SET_YAWSPEED:
		float_data = atof(data);
		Q3_SetYawSpeed(entID, float_data);
		break;

	case SET_AGGRESSION:
		int_data = atoi(data);
		Q3_SetAggression(entID, int_data);
		break;

	case SET_AIM:
		int_data = atoi(data);
		Q3_SetAim(entID, int_data);
		break;

	case SET_FRICTION:
		int_data = atoi(data);
		Q3_SetFriction(entID, int_data);
		break;

	case SET_GRAVITY:
		float_data = atof(data);
		Q3_SetGravity(entID, float_data);
		break;

	case SET_WAIT:
		float_data = atof(data);
		Q3_SetWait(entID, float_data);
		break;

	case SET_FOLLOWDIST:
		float_data = atof(data);
		Q3_SetFollowDist(entID, float_data);
		break;

	case SET_SCALE:
		float_data = atof(data);
		Q3_SetScale(entID, float_data);
		break;

	case SET_NPC_SCALE:
		float_data = atof(data);
		Q3_Set_NPC_Scale(entID, float_data);
		break;

	case SET_KOTOR_MODE:
		if (!Q_stricmp("true", data))
		{
			Q3_setKotorMode(entID, qtrue);
		}
		else
		{
			Q3_setKotorMode(entID, qfalse);
		}
		break;

	case SET_RENDER_CULL_RADIUS:
		float_data = atof(data);
		Q3_SetRenderCullRadius(entID, float_data);
		break;

	case SET_COUNT:
		Q3_SetCount(entID, data);
		break;

	case SET_SHOT_SPACING:
		int_data = atoi(data);
		Q3_SetShotSpacing(entID, int_data);
		break;

	case SET_IGNOREPAIN:
		if (!Q_stricmp("true", data))
			Q3_SetIgnorePain(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetIgnorePain(entID, qfalse);
		break;

	case SET_IGNOREENEMIES:
		if (!Q_stricmp("true", data))
			Q3_SetIgnoreEnemies(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetIgnoreEnemies(entID, qfalse);
		break;

	case SET_IGNOREALERTS:
		if (!Q_stricmp("true", data))
			Q3_SetIgnoreAlerts(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetIgnoreAlerts(entID, qfalse);
		break;

	case SET_DONTSHOOT:
		if (!Q_stricmp("true", data))
			Q3_SetDontShoot(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetDontShoot(entID, qfalse);
		break;

	case SET_DONTFIRE:
		if (!Q_stricmp("true", data))
			Q3_SetDontFire(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetDontFire(entID, qfalse);
		break;

	case SET_LOCKED_ENEMY:
		if (!Q_stricmp("true", data))
			Q3_SetLockedEnemy(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetLockedEnemy(entID, qfalse);
		break;

	case SET_NOTARGET:
		if (!Q_stricmp("true", data))
			Q3_SetNoTarget(entID, qtrue);
		else if (!Q_stricmp("false", data))
			Q3_SetNoTarget(entID, qfalse);
		break;

	case SET_LEAN:
		if (!Q_stricmp("right", data))
			Q3_SetLean(entID, LEAN_RIGHT);
		else if (!Q_stricmp("left", data))
			Q3_SetLean(entID, LEAN_LEFT);
		else
			Q3_SetLean(entID, LEAN_NONE);
		break;

	case SET_SHOOTDIST:
		float_data = atof(data);
		Q3_SetShootDist(entID, float_data);
		break;

	case SET_TIMESCALE:
		Q3_SetTimeScale(entID, data);
		break;

	case SET_VISRANGE:
		float_data = atof(data);
		Q3_SetVisrange(entID, float_data);
		break;

	case SET_EARSHOT:
		float_data = atof(data);
		Q3_SetEarshot(entID, float_data);
		break;

	case SET_VIGILANCE:
		float_data = atof(data);
		Q3_SetVigilance(entID, float_data);
		break;

	case SET_VFOV:
		int_data = atoi(data);
		Q3_SetVFOV(entID, int_data);
		break;

	case SET_HFOV:
		int_data = atoi(data);
		Q3_SetHFOV(entID, int_data);
		break;

	case SET_TARGETNAME:
		Q3_SetTargetName(entID, data);
		break;

	case SET_TARGET:
		Q3_SetTarget(entID, data);
		break;

	case SET_TARGET2:
		Q3_SetTarget2(entID, data);
		break;

	case SET_LOCATION:
		if (!Q3_SetLocation(entID, data))
		{
			Q3_TaskIDSet(ent, TID_LOCATION, taskID);
			return;
		}
		break;

	case SET_PAINTARGET:
		Q3_SetPainTarget(entID, data);
		break;

	case SET_DEFEND_TARGET:
		DebugPrint(WL_WARNING, "SetDefendTarget unimplemented\n", entID);
		//Q3_SetEnemy( entID, (char *) data);
		break;

	case SET_PARM1:
	case SET_PARM2:
	case SET_PARM3:
	case SET_PARM4:
	case SET_PARM5:
	case SET_PARM6:
	case SET_PARM7:
	case SET_PARM8:
	case SET_PARM9:
	case SET_PARM10:
	case SET_PARM11:
	case SET_PARM12:
	case SET_PARM13:
	case SET_PARM14:
	case SET_PARM15:
	case SET_PARM16:
		Q3_SetParm(entID, toSet - SET_PARM1, data);
		break;

	case SET_SPAWNSCRIPT:
	case SET_USESCRIPT:
	case SET_AWAKESCRIPT:
	case SET_ANGERSCRIPT:
	case SET_ATTACKSCRIPT:
	case SET_VICTORYSCRIPT:
	case SET_PAINSCRIPT:
	case SET_FLEESCRIPT:
	case SET_DEATHSCRIPT:
	case SET_DELAYEDSCRIPT:
	case SET_BLOCKEDSCRIPT:
	case SET_FFIRESCRIPT:
	case SET_FFDEATHSCRIPT:
	case SET_MINDTRICKSCRIPT:
	case SET_UNDYINGPLAYERVICTORYSCRIPT:
		if (!Q3_SetBehaviorSet(entID, toSet, data))
			DebugPrint(WL_ERROR, "SetBehaviorSet: Invalid bSet %s\n", type_name);
		break;

	case SET_NO_MINDTRICK:
		if (!Q_stricmp("true", data))
			Q3_SetNoMindTrick(entID, qtrue);
		else
			Q3_SetNoMindTrick(entID, qfalse);
		break;

	case SET_CINEMATIC_SKIPSCRIPT:
		Q3_SetCinematicSkipScript(data);
		break;

	case SET_RAILCENTERTRACKLOCKED:
		Rail_LockCenterOfTrack(data);
		break;

	case SET_RAILCENTERTRACKUNLOCKED:
		Rail_UnLockCenterOfTrack(data);
		break;

	case SET_DELAYSCRIPTTIME:
		int_data = atoi(data);
		Q3_SetDelayScriptTime(entID, int_data);
		break;

	case SET_CROUCHED:
		if (!Q_stricmp("true", data))
			Q3_SetCrouched(entID, qtrue);
		else
			Q3_SetCrouched(entID, qfalse);
		break;

	case SET_WALKING:
		if (!Q_stricmp("true", data))
			Q3_SetWalking(entID, qtrue);
		else
			Q3_SetWalking(entID, qfalse);
		break;

	case SET_RUNNING:
		if (!Q_stricmp("true", data))
			Q3_SetRunning(entID, qtrue);
		else
			Q3_SetRunning(entID, qfalse);
		break;

	case SET_CHASE_ENEMIES:
		if (!Q_stricmp("true", data))
			Q3_SetChaseEnemies(entID, qtrue);
		else
			Q3_SetChaseEnemies(entID, qfalse);
		break;

	case SET_LOOK_FOR_ENEMIES:
		if (!Q_stricmp("true", data))
			Q3_SetLookForEnemies(entID, qtrue);
		else
			Q3_SetLookForEnemies(entID, qfalse);
		break;

	case SET_FACE_MOVE_DIR:
		if (!Q_stricmp("true", data))
			Q3_SetFaceMoveDir(entID, qtrue);
		else
			Q3_SetFaceMoveDir(entID, qfalse);
		break;

	case SET_ALT_FIRE:
		if (!Q_stricmp("true", data))
			Q3_SetAltFire(entID, qtrue);
		else
			Q3_SetAltFire(entID, qfalse);
		break;

	case SET_DONT_FLEE:
		if (!Q_stricmp("true", data))
			Q3_SetDontFlee(entID, qtrue);
		else
			Q3_SetDontFlee(entID, qfalse);
		break;

	case SET_FORCED_MARCH:
		if (!Q_stricmp("true", data))
			Q3_SetForcedMarch(entID, qtrue);
		else
			Q3_SetForcedMarch(entID, qfalse);
		break;

	case SET_NO_RESPONSE:
		if (!Q_stricmp("true", data))
			Q3_SetNoResponse(entID, qtrue);
		else
			Q3_SetNoResponse(entID, qfalse);
		break;

	case SET_NO_COMBAT_TALK:
		if (!Q_stricmp("true", data))
			Q3_SetCombatTalk(entID, qtrue);
		else
			Q3_SetCombatTalk(entID, qfalse);
		break;

	case SET_NO_ALERT_TALK:
		if (!Q_stricmp("true", data))
			Q3_SetAlertTalk(entID, qtrue);
		else
			Q3_SetAlertTalk(entID, qfalse);
		break;

	case SET_USE_CP_NEAREST:
		if (!Q_stricmp("true", data))
			Q3_SetUseCpNearest(entID, qtrue);
		else
			Q3_SetUseCpNearest(entID, qfalse);
		break;

	case SET_NO_FORCE:
		if (!Q_stricmp("true", data))
			Q3_SetNoForce(entID, qtrue);
		else
			Q3_SetNoForce(entID, qfalse);
		break;

	case SET_NO_ACROBATICS:
		if (!Q_stricmp("true", data))
			Q3_SetNoAcrobatics(entID, qtrue);
		else
			Q3_SetNoAcrobatics(entID, qfalse);
		break;

	case SET_USE_SUBTITLES:
		if (!Q_stricmp("true", data))
			Q3_SetUseSubtitles(entID, qtrue);
		else
			Q3_SetUseSubtitles(entID, qfalse);
		break;

	case SET_NO_FALLTODEATH:
		if (!Q_stricmp("true", data))
			Q3_SetNoFallToDeath(entID, qtrue);
		else
			Q3_SetNoFallToDeath(entID, qfalse);
		break;

	case SET_DISMEMBERABLE:
		if (!Q_stricmp("true", data))
			Q3_SetDismemberable(entID, qtrue);
		else
			Q3_SetDismemberable(entID, qfalse);
		break;

	case SET_MORELIGHT:
		if (!Q_stricmp("true", data))
			Q3_SetMoreLight(entID, qtrue);
		else
			Q3_SetMoreLight(entID, qfalse);
		break;

	case SET_TREASONED:
		DebugPrint(WL_VERBOSE, "SET_TREASONED is disabled, do not use\n");
		/*
		G_TeamRetaliation( NULL, &g_entities[0], qfalse );
		ffireLevel = FFIRE_LEVEL_RETALIATION;
		*/
		break;

	case SET_UNDYING:
		if (!Q_stricmp("true", data))
			Q3_SetUndying(entID, qtrue);
		else
			Q3_SetUndying(entID, qfalse);
		break;

	case SET_INVINCIBLE:
		if (!Q_stricmp("true", data))
			Q3_SetInvincible(entID, qtrue);
		else
			Q3_SetInvincible(entID, qfalse);
		break;

	case SET_NOAVOID:
		if (!Q_stricmp("true", data))
			Q3_SetNoAvoid(entID, qtrue);
		else
			Q3_SetNoAvoid(entID, qfalse);
		break;

	case SET_SOLID:
		if (!Q_stricmp("true", data))
		{
			if (!Q3_SetSolid(entID, qtrue))
			{
				Q3_TaskIDSet(ent, TID_RESIZE, taskID);
				return;
			}
		}
		else
		{
			Q3_SetSolid(entID, qfalse);
		}
		break;

	case SET_INVISIBLE:
		if (!Q_stricmp("true", data))
			Q3_SetInvisible(entID, qtrue);
		else
			Q3_SetInvisible(entID, qfalse);
		break;

	case SET_VAMPIRE:
		if (!Q_stricmp("true", data))
			Q3_SetVampire(entID, qtrue);
		else
			Q3_SetVampire(entID, qfalse);
		break;

	case SET_FORCE_INVINCIBLE:
		if (!Q_stricmp("true", data))
			Q3_SetForceInvincible(entID, qtrue);
		else
			Q3_SetForceInvincible(entID, qfalse);
		break;

	case SET_GREET_ALLIES:
		if (!Q_stricmp("true", data))
			Q3_SetGreetAllies(entID, qtrue);
		else
			Q3_SetGreetAllies(entID, qfalse);
		break;

	case SET_PLAYER_LOCKED:
		if (!Q_stricmp("true", data))
			Q3_SetPlayerLocked(entID, qtrue);
		else
			Q3_SetPlayerLocked(entID, qfalse);
		break;

	case SET_LOCK_PLAYER_WEAPONS:
		if (!Q_stricmp("true", data))
			Q3_SetLockPlayerWeapons(entID, qtrue);
		else
			Q3_SetLockPlayerWeapons(entID, qfalse);
		break;

	case SET_NO_IMPACT_DAMAGE:
		if (!Q_stricmp("true", data))
			Q3_SetNoImpactDamage(entID, qtrue);
		else
			Q3_SetNoImpactDamage(entID, qfalse);
		break;

	case SET_FORWARDMOVE:
		int_data = atoi(data);
		Q3_SetForwardMove(entID, int_data);
		break;

	case SET_RIGHTMOVE:
		int_data = atoi(data);
		Q3_SetRightMove(entID, int_data);
		break;

	case SET_LOCKYAW:
		Q3_SetLockAngle(entID, data);
		break;

	case SET_CAMERA_GROUP:
		Q3_CameraGroup(entID, data);
		break;
	case SET_CAMERA_GROUP_Z_OFS:
		float_data = atof(data);
		Q3_CameraGroupZOfs(float_data);
		break;
	case SET_CAMERA_GROUP_TAG:
		Q3_CameraGroupTag(data);
		break;

		//FIXME: put these into camera commands
	case SET_LOOK_TARGET:
		Q3_LookTarget(entID, const_cast<char*>(data));
		break;

	case SET_ADDRHANDBOLT_MODEL:
		Q3_AddRHandModel(entID, data);
		break;

	case SET_REMOVERHANDBOLT_MODEL:
		Q3_RemoveRHandModel(entID, const_cast<char*>(data));
		break;

	case SET_ADDLHANDBOLT_MODEL:
		Q3_AddLHandModel(entID, data);
		break;

	case SET_REMOVELHANDBOLT_MODEL:
		Q3_RemoveLHandModel(entID, const_cast<char*>(data));
		break;

	case SET_FACEEYESCLOSED:
	case SET_FACEEYESOPENED:
	case SET_FACEAUX:
	case SET_FACEBLINK:
	case SET_FACEBLINKFROWN:
	case SET_FACEFROWN:
	case SET_FACESMILE:
	case SET_FACEGLAD:
	case SET_FACEHAPPY:
	case SET_FACESHOCKED:
	case SET_FACENORMAL:
		float_data = atof(data);
		Q3_Face(entID, toSet, float_data);
		break;

	case SET_SCROLLTEXT:
		Q3_ScrollText(data);
		break;

	case SET_LCARSTEXT:
		Q3_LCARSText(data);
		break;

	case SET_CENTERTEXT:
		CenterPrint(data);
		break;

	case SET_CAPTIONTEXTCOLOR:
		Q3_SetCaptionTextColor(data);
		break;
	case SET_CENTERTEXTCOLOR:
		Q3_SetCenterTextColor(data);
		break;
	case SET_SCROLLTEXTCOLOR:
		Q3_SetScrollTextColor(data);
		break;

	case SET_PLAYER_USABLE:
		if (!Q_stricmp("true", data))
		{
			Q3_SetPlayerUsable(entID, qtrue);
		}
		else
		{
			Q3_SetPlayerUsable(entID, qfalse);
		}
		break;

	case SET_STARTFRAME:
		int_data = atoi(data);
		Q3_SetStartFrame(entID, int_data);
		break;

	case SET_ENDFRAME:
		int_data = atoi(data);
		Q3_SetEndFrame(entID, int_data);

		Q3_TaskIDSet(ent, TID_ANIM_BOTH, taskID);
		return;

	case SET_ANIMFRAME:
		int_data = atoi(data);
		Q3_SetAnimFrame(entID, int_data);
		return;

	case SET_LOOP_ANIM:
		if (!Q_stricmp("true", data))
		{
			Q3_SetLoopAnim(entID, qtrue);
		}
		else
		{
			Q3_SetLoopAnim(entID, qfalse);
		}
		break;

	case SET_INTERFACE:
		if (!Q_stricmp("true", data))
		{
			Q3_SetInterface(entID, "1");
		}
		else
		{
			Q3_SetInterface(entID, "0");
		}

		break;

	case SET_SHIELDS:
		if (!Q_stricmp("true", data))
		{
			Q3_SetShields(entID, qtrue);
		}
		else
		{
			Q3_SetShields(entID, qfalse);
		}
		break;

	case SET_SABERACTIVE:
		if (!Q_stricmp("true", data))
		{
			Q3_SetSaberActive(entID, qtrue);
		}
		else
		{
			Q3_SetSaberActive(entID, qfalse);
		}
		break;

		// Created: 10/02/02 by Aurelio Reis, Modified: 10/02/02
		// Make a specific Saber 1 Blade active.
	case SET_SABER1BLADEON:
		// Get which Blade to activate.
		int_data = atoi(data);
		Q3_SetSaberBladeActive(entID, 0, int_data, qtrue);
		break;

		// Make a specific Saber 1 Blade inactive.
	case SET_SABER1BLADEOFF:
		// Get which Blade to deactivate.
		int_data = atoi(data);
		Q3_SetSaberBladeActive(entID, 0, int_data, qfalse);
		break;

		// Make a specific Saber 2 Blade active.
	case SET_SABER2BLADEON:
		// Get which Blade to activate.
		int_data = atoi(data);
		Q3_SetSaberBladeActive(entID, 1, int_data, qtrue);
		break;

		// Make a specific Saber 2 Blade inactive.
	case SET_SABER2BLADEOFF:
		// Get which Blade to deactivate.
		int_data = atoi(data);
		Q3_SetSaberBladeActive(entID, 1, int_data, qfalse);
		break;

	case SET_DAMAGEENTITY:
		int_data = atoi(data);
		G_Damage(ent, ent, ent, nullptr, nullptr, int_data, 0, MOD_UNKNOWN);
		break;

	case SET_SABER_ORIGIN:
		sscanf(data, "%f %f %f", &vector_data[0], &vector_data[1], &vector_data[2]);
		WP_SetSaberOrigin(ent, vector_data);
		break;

	case SET_ADJUST_AREA_PORTALS:
		if (!Q_stricmp("true", data))
		{
			Q3_SetAdjustAreaPortals(entID, qtrue);
		}
		else
		{
			Q3_SetAdjustAreaPortals(entID, qfalse);
		}
		break;

	case SET_DMG_BY_HEAVY_WEAP_ONLY:
		if (!Q_stricmp("true", data))
		{
			Q3_SetDmgByHeavyWeapOnly(entID, qtrue);
		}
		else
		{
			Q3_SetDmgByHeavyWeapOnly(entID, qfalse);
		}
		break;

	case SET_SHIELDED:
		if (!Q_stricmp("true", data))
		{
			Q3_SetShielded(entID, qtrue);
		}
		else
		{
			Q3_SetShielded(entID, qfalse);
		}
		break;

	case SET_NO_GROUPS:
		if (!Q_stricmp("true", data))
		{
			Q3_SetNoGroups(entID, qtrue);
		}
		else
		{
			Q3_SetNoGroups(entID, qfalse);
		}
		break;

	case SET_FIRE_WEAPON:
		if (!Q_stricmp("true", data))
		{
			Q3_SetFireWeapon(entID, qtrue);
		}
		else if (!Q_stricmp("false", data))
		{
			Q3_SetFireWeapon(entID, qfalse);
		}
		break;

	case SET_FIRE_WEAPON_NO_ANIM:
		if (!Q_stricmp("true", data))
		{
			Q3_SetFireWeaponNoAnim(entID, qtrue);
		}
		else if (!Q_stricmp("false", data))
		{
			Q3_SetFireWeaponNoAnim(entID, qfalse);
		}
		break;
	case SET_SAFE_REMOVE:
		if (!Q_stricmp("true", data))
		{
			Q3_SetSafeRemove(entID, qtrue);
		}
		else if (!Q_stricmp("false", data))
		{
			Q3_SetSafeRemove(entID, qfalse);
		}
		break;

	case SET_BOBA_JET_PACK:
		if (!Q_stricmp("true", data))
		{
			Q3_SetBobaJetPack(entID, qtrue);
		}
		else if (!Q_stricmp("false", data))
		{
			Q3_SetBobaJetPack(entID, qfalse);
		}
		break;

	case SET_INACTIVE:
		if (!Q_stricmp("true", data))
		{
			Q3_SetInactive(entID, qtrue);
		}
		else if (!Q_stricmp("false", data))
		{
			Q3_SetInactive(entID, qfalse);
		}
		else if (!Q_stricmp("unlocked", data))
		{
			UnLockDoors(&g_entities[entID]);
		}
		else if (!Q_stricmp("locked", data))
		{
			LockDoors(&g_entities[entID]);
		}
		break;

	case SET_END_SCREENDISSOLVE:
		gi.SendConsoleCommand("endscreendissolve\n");
		break;

	case SET_FUNC_USABLE_VISIBLE:
		if (!Q_stricmp("true", data))
		{
			Q3_SetFuncUsableVisible(entID, qtrue);
		}
		else if (!Q_stricmp("false", data))
		{
			Q3_SetFuncUsableVisible(entID, qfalse);
		}
		break;

	case SET_NO_KNOCKBACK:
		if (!Q_stricmp("true", data))
		{
			Q3_SetNoKnockback(entID, qtrue);
		}
		else
		{
			Q3_SetNoKnockback(entID, qfalse);
		}
		break;

	case SET_VIDEO_PLAY:
		gi.SendConsoleCommand(va("inGameCinematic %s\n", const_cast<char*>(data)));
		break;

	case SET_VIDEO_FADE_IN:
		if (!Q_stricmp("true", data))
		{
			gi.cvar_set("cl_VidFadeUp", "1");
		}
		else
		{
			gi.cvar_set("cl_VidFadeUp", "0");
		}
		break;

	case SET_VIDEO_FADE_OUT:
		if (!Q_stricmp("true", data))
		{
			gi.cvar_set("cl_VidFadeDown", "1");
		}
		else
		{
			gi.cvar_set("cl_VidFadeDown", "0");
		}
		break;
	case SET_REMOVE_TARGET:
		Q3_SetRemoveTarget(entID, data);
		break;

	case SET_LOADGAME:
		gi.SendConsoleCommand(va("load %s\n", data));
		break;

	case SET_MENU_SCREEN:
		gi.SendConsoleCommand(va("uimenu %s\n", const_cast<char*>(data)));
		break;

	case SET_OBJECTIVE_SHOW:
		missionInfo_Updated = qtrue; // Activate flashing text
		gi.cvar_set("cg_updatedDataPadObjective", "1");

		Q3_SetObjective(data, SET_OBJ_SHOW);
		Q3_SetObjective(data, SET_OBJ_PENDING);
		break;
	case SET_OBJECTIVE_HIDE:
		Q3_SetObjective(data, SET_OBJ_HIDE);
		break;
	case SET_OBJECTIVE_SUCCEEDED:
		missionInfo_Updated = qtrue; // Activate flashing text
		gi.cvar_set("cg_updatedDataPadObjective", "1");
		Q3_SetObjective(data, SET_OBJ_SUCCEEDED);
		break;
	case SET_OBJECTIVE_SUCCEEDED_NO_UPDATE:
		Q3_SetObjective(data, SET_OBJ_SUCCEEDED);
		break;
	case SET_OBJECTIVE_FAILED:
		Q3_SetObjective(data, SET_OBJ_FAILED);
		break;

	case SET_OBJECTIVE_CLEARALL:
		Q3_ObjectiveClearAll();
		break;

	case SET_MISSIONFAILED:
		Q3_SetMissionFailed(data);
		break;

	case SET_MISSIONSTATUSTEXT:
		Q3_SetStatusText(data);
		break;

	case SET_MISSIONSTATUSTIME:
		int_data = atoi(data);
		cg.missionStatusDeadTime = level.time + int_data;
		break;

	case SET_CLOSINGCREDITS:
		gi.cvar_set("cg_endcredits", "1");
		break;

	case SET_SKILL:
		//		//can never be set
		break;

	case SET_DISABLE_SHADER_ANIM:
		if (!Q_stricmp("true", data))
		{
			Q3_SetDisableShaderAnims(entID, qtrue);
		}
		else
		{
			Q3_SetDisableShaderAnims(entID, qfalse);
		}
		break;

	case SET_SHADER_ANIM:
		if (!Q_stricmp("true", data))
		{
			Q3_SetShaderAnim(entID, qtrue);
		}
		else
		{
			Q3_SetShaderAnim(entID, qfalse);
		}
		break;

	case SET_MUSIC_STATE:
		Q3_SetMusicState(data);
		break;

	case SET_CLEAN_DAMAGING_ENTS:
		Q3_SetCleanDamagingEnts();
		break;

	case SET_HUD:
		if (!Q_stricmp("true", data))
		{
			gi.cvar_set("cg_drawHUD", "1");
		}
		else
		{
			gi.cvar_set("cg_drawHUD", "0");
		}
		break;

	case SET_FORCE_HEAL_LEVEL:
	case SET_FORCE_JUMP_LEVEL:
	case SET_FORCE_SPEED_LEVEL:
	case SET_FORCE_PUSH_LEVEL:
	case SET_FORCE_PULL_LEVEL:
	case SET_FORCE_MINDTRICK_LEVEL:
	case SET_FORCE_GRIP_LEVEL:
	case SET_FORCE_LIGHTNING_LEVEL:
	case SET_SABER_THROW:
	case SET_SABER_DEFENSE:
	case SET_SABER_OFFENSE:
	case SET_FORCE_RAGE_LEVEL:
	case SET_FORCE_PROTECT_LEVEL:
	case SET_FORCE_ABSORB_LEVEL:
	case SET_FORCE_DRAIN_LEVEL:
	case SET_FORCE_SIGHT_LEVEL:
	case SET_FORCE_DESTRUCTION_LEVEL:
	case SET_FORCE_STASIS_LEVEL:
	case SET_FORCE_GRASP_LEVEL:
	case SET_FORCE_REPULSE_LEVEL:
	case SET_FORCE_LIGHTNING_STRIKE_LEVEL:
	case SET_FORCE_FEAR_LEVEL:
	case SET_FORCE_DEADLYSIGHT_LEVEL:
	case SET_FORCE_PROJECTION_LEVEL:
	case SET_FORCE_BLAST_LEVEL:
		int_data = atoi(data);
		Q3_SetForcePowerLevel(entID, toSet - SET_FORCE_HEAL_LEVEL, int_data);
		break;

	case SET_SABER1:
	case SET_SABER2:
		WP_SetSaber(&g_entities[entID], toSet - SET_SABER1, data);
		break;

	case SET_PLAYERMODEL:
		WP_RemoveSaber(&g_entities[entID], 0);
		WP_RemoveSecondSaber(&g_entities[entID], 1);
		G_ChangePlayerModel(&g_entities[entID], data);
		break;

	case SET_VEHICLE:
		Use(entID, data);
		break;

	case SET_SECURITY_KEY:
		Q3_GiveSecurityKey(entID, data);
		break;

	case SET_SABER1_COLOR1:
	case SET_SABER1_COLOR2:
		WP_SaberSetColor(&g_entities[entID], 0, toSet - SET_SABER1_COLOR1, data);
		if (&g_entities[entID] == player)
		{
			gi.cvar_set("g_saber_color", const_cast<char*>(data));
		}
		break;
	case SET_SABER2_COLOR1:
	case SET_SABER2_COLOR2:
		WP_SaberSetColor(&g_entities[entID], 1, toSet - SET_SABER2_COLOR1, data);
		if (&g_entities[entID] == player)
		{
			gi.cvar_set("g_saber2_color", const_cast<char*>(data));
		}
		break;
	case SET_DISMEMBER_LIMB:
		Q3_DismemberLimb(entID, const_cast<char*>(data));
		break;
	case SET_NO_PVS_CULL:
		Q3_SetBroadcast(entID, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
		// Set a Saboteur to cloak (true) or un-cloak (false).
	case SET_CLOAK: // Created: 01/08/03 by AReis.
		extern void Saboteur_Cloak(gentity_t * self);
		if (Q_stricmp("true", data) == 0)
		{
			Saboteur_Cloak(&g_entities[entID]);
		}
		else
		{
			Saboteur_Decloak(&g_entities[entID]);
		}
		break;
	case SET_FORCE_HEAL:
		Q3_SetForcePower(entID, FP_HEAL, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_SPEED:
		Q3_SetForcePower(entID, FP_SPEED, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_PUSH:
		Q3_SetForcePower(entID, FP_PUSH, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_PUSH_FAKE:
		if (g_SerenityJediEngineMode->integer)
		{
			if (ent->client->NPC_class == CLASS_GALEN
				&& (ent->s.weapon == WP_MELEE || ent->s.weapon == WP_NONE || ent->s.weapon == WP_SABER && !ent->
					client->ps.SaberActive())
				&& ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				ForceRepulse(&g_entities[entID], qfalse, qtrue);
			}
			else
			{
				ForceThrow_MD(&g_entities[entID], qfalse, qtrue);
			}
		}
		else
		{
			ForceThrow_JKA(&g_entities[entID], qfalse, qtrue);
		}
		break;
	case SET_FORCE_PULL:
		Q3_SetForcePower(entID, FP_PULL, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_MIND_TRICK:
		Q3_SetForcePower(entID, FP_TELEPATHY, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_GRIP:
		Q3_SetForcePower(entID, FP_GRIP, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_LIGHTNING:
		Q3_SetForcePower(entID, FP_LIGHTNING, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_SABERTHROW:
		Q3_SetForcePower(entID, FP_SABERTHROW, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_RAGE:
		Q3_SetForcePower(entID, FP_RAGE, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_PROTECT:
		Q3_SetForcePower(entID, FP_PROTECT, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_ABSORB:
		Q3_SetForcePower(entID, FP_ABSORB, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_DRAIN:
		Q3_SetForcePower(entID, FP_DRAIN, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_DESTRUCTION:
		Q3_SetForcePower(entID, FP_DESTRUCTION, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_STASIS:
		Q3_SetForcePower(entID, FP_STASIS, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_GRASP:
		Q3_SetForcePower(entID, FP_GRASP, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_REPULSE:
		Q3_SetForcePower(entID, FP_REPULSE, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_LIGHTNING_STRIKE:
		Q3_SetForcePower(entID, FP_LIGHTNING_STRIKE, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_FEAR:
		Q3_SetForcePower(entID, FP_FEAR, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_DEADLYSIGHT:
		Q3_SetForcePower(entID, FP_DEADLYSIGHT, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_PROJECTION:
		Q3_SetForcePower(entID, FP_PROJECTION, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;
	case SET_FORCE_BLAST:
		Q3_SetForcePower(entID, FP_BLAST, static_cast<qboolean>(Q_stricmp("true", data) == 0));
		break;

	case SET_WINTER_GEAR: // Created: 03/26/03 by AReis.
	{
		// If this is a (fake) Player NPC or this IS the Player...
		if (entID == 0 || ent->NPC_type && Q_stricmp(ent->NPC_type, "player") == 0)
		{
			extern cvar_t* g_char_skin_legs;
			extern cvar_t* g_char_skin_torso;
			extern cvar_t* g_char_skin_head;
			extern cvar_t* g_char_model;
			char strSkin[MAX_QPATH];
			// Set the Winter Gear Skin if true, otherwise set back to normal configuration.
			if (Q_stricmp("true", data) == 0)
			{
				Com_sprintf(strSkin, sizeof strSkin, "models/players/%s/|%s|%s|%s", g_char_model->string,
					g_char_skin_head->string, "torso_g1", "lower_e1");
			}
			else if (Q_stricmp(g_char_skin_head->string, "model_default") == 0 &&
				Q_stricmp(g_char_skin_torso->string, "model_default") == 0 && Q_stricmp(
					g_char_skin_legs->string, "model_default") == 0)
			{
				Com_sprintf(strSkin, sizeof strSkin, "models/players/%s/model_default.skin",
					g_char_model->string);
			}
			else
			{
				Com_sprintf(strSkin, sizeof strSkin, "models/players/%s/|%s|%s|%s", g_char_model->string,
					g_char_skin_head->string, g_char_skin_torso->string, g_char_skin_legs->string);
			}
			const int iSkinID = gi.RE_RegisterSkin(strSkin);
			if (iSkinID)
			{
				gi.G2API_SetSkin(&ent->ghoul2[ent->playerModel], G_SkinIndex(strSkin), iSkinID);
			}
		}
		break;
	}
	case SET_NO_ANGLES:
		if (entID >= 0 && entID < ENTITYNUM_WORLD)
		{
			if (Q_stricmp("true", data) == 0)
			{
				g_entities[entID].flags |= FL_NO_ANGLES;
			}
			else
			{
				g_entities[entID].flags &= ~FL_NO_ANGLES;
			}
		}
		break;
	case SET_SKIN:
		// If this is a (fake) Player NPC or this IS the Player...
	{
		//just blindly sets whatever skin you set!  include full path after "base/"... eg: "models/players/tavion_new/model_possessed.skin"
		gentity_t* gentity_s = &g_entities[entID];
		if (gentity_s && gentity_s->inuse && gentity_s->ghoul2.size())
		{
			const int iSkinID = gi.RE_RegisterSkin(const_cast<char*>(data));
			if (iSkinID)
			{
				gi.G2API_SetSkin(&gentity_s->ghoul2[gentity_s->playerModel], G_SkinIndex(data), iSkinID);
			}
		}
	}
	break;
	case SET_RADAR_OBJECT:
		if (entID >= 0 && entID < ENTITYNUM_WORLD)
		{
			if (Q_stricmp("true", data) == 0)
			{
				g_entities[entID].s.eFlags2 |= EF2_RADAROBJECT;
			}
			else
			{
				g_entities[entID].s.eFlags2 &= ~EF2_RADAROBJECT;
			}
		}
		break;
	case SET_RADAR_ICON:
		if (entID >= 0 && entID < ENTITYNUM_WORLD)
		{
			ent->s.radarIcon = G_IconIndex(data);
		}
		break;

	case SET_VADERBREATH:
		if (!Q_stricmp("true", data))
		{
			Q3_setVaderBreath(entID, qtrue);
		}
		else
		{
			Q3_setVaderBreath(entID, qfalse);
		}
		break;

	case SET_VADERDAMAGEDBREATH:
		if (!Q_stricmp("true", data))
		{
			Q3_setVaderBreathdamaged(entID, qtrue);
		}
		else
		{
			Q3_setVaderBreathdamaged(entID, qfalse);
		}
		break;
	case SET_SABERCOLLISIONS:
		if (entID >= 0 && entID < ENTITYNUM_WORLD)
		{
			if (Q_stricmp("true", data) == 0)
			{
				if (g_entities[entID].client)
				{
					g_entities[entID].client->saberCollisions = qtrue;
				}
			}
			else
			{
				if (g_entities[entID].client)
				{
					g_entities[entID].client->saberCollisions = qfalse;
				}
			}
		}
		break;

		//////////
	case SET_ANIM_LOWER_FORCED:
		ent->client->ps.forceLowerAnim = GetIDForString(anim_table, data);
		return; //Don't call it back

	case SET_ANIM_UPPER_FORCED:
		ent->client->ps.forceUpperAnim = GetIDForString(anim_table, data);
		return; //Don't call it back

	case SET_ANIM_BOTH_FORCED:
		ent->client->ps.forceUpperAnim = GetIDForString(anim_table, data);
		ent->client->ps.forceLowerAnim = GetIDForString(anim_table, data);
		return; //Don't call it back

	case SET_ANIM_HOLDTIME_LOWER_FORCED:
		int_data = atoi(data);
		if (int_data == -1)
			int_data = Q3_INFINITE;

		ent->client->ps.forceLowerAnimTimer = level.time + int_data;
		return; //Don't call it back

	case SET_ANIM_HOLDTIME_UPPER_FORCED:
		int_data = atoi(data);
		if (int_data == -1)
			int_data = Q3_INFINITE;

		ent->client->ps.forceUpperAnimTimer = level.time + int_data;
		return; //Don't call it back

	case SET_ANIM_HOLDTIME_BOTH_FORCED:
		int_data = atoi(data);
		if (int_data == -1)
			int_data = Q3_INFINITE;

		ent->client->ps.forceUpperAnimTimer = level.time + int_data;
		ent->client->ps.forceLowerAnimTimer = level.time + int_data;
		return; //Don't call it back

	case SET_ANIM_SPEED_LOWER_FORCED:
		float_data = atof(data);
		ent->client->ps.forceLowerAnimSpeed = float_data;
		return; //Don't call it back

	case SET_ANIM_SPEED_UPPER_FORCED:
		float_data = atof(data);
		ent->client->ps.forceUpperAnimSpeed = float_data;
		return; //Don't call it back

	case SET_ANIM_SPEED_BOTH_FORCED:
		float_data = atof(data);
		if (float_data == -1)
			float_data = Q3_INFINITE;

		ent->client->ps.forceUpperAnimSpeed = float_data;
		ent->client->ps.forceLowerAnimSpeed = float_data;
		return; //Don't call it back
		//////////////////////////////

	case SET_NO_SABER_RETURN:
		if (!Q_stricmp("true", data))
		{
			Q3_SetNoSaberReturn(entID, qtrue);
		}
		else
		{
			Q3_SetNoSaberReturn(entID, qfalse);
		}
		break;

	case SET_MODEL:
		G_ChangeModel(data);
		break;

	case SET_PLAYERSKIN:
		// If this is a (fake) Player NPC or this IS the Player...
	{
		//just blindly sets whatever skin you set!  include full path after "base/"... eg: "models/players/tavion_new/model_possessed.skin"
		gentity_t* gentity_s = &g_entities[entID];
		if (gentity_s && gentity_s->inuse && gentity_s->ghoul2.size())
		{
			const int iSkinID = gi.RE_RegisterSkin(const_cast<char*>(data));
			if (iSkinID)
			{
				gi.G2API_SetSkin(&gentity_s->ghoul2[gentity_s->playerModel], G_SkinIndex(data), iSkinID);
			}
		}
		if (gentity_s == player)
		{
			std::string str = const_cast<char*>(data);

			const int modelFind = str.find("players/");

			const int skinFind = str.find("/model_");

			const int skinFileFind = str.find(".skin");

			const std::string model = str.substr(modelFind + 8, skinFind - modelFind - 8);
			const std::string skin = str.substr(skinFind + 1, skinFileFind - skinFind - 1);

			const char* finalModel = model.c_str();
			const char* finalSkin = skin.c_str();

			gi.cvar_set("g_char_model", finalModel);
			gi.cvar_set("g_char_skin_head", finalSkin);
			gi.cvar_set("g_char_skin_torso", finalSkin);
			gi.cvar_set("g_char_skin_legs", finalSkin);

			G_ChangePlayerModel(player, finalModel);
		}
	}
	break;

	case SET_PLAYERSCALE:
		G_ChangeScale(data);
		break;

	case SET_SOUNDSET:
		gi.cvar_set("snd", data);
		break;

	case SET_GENDER:
		gi.cvar_set("sex", data);
		break;

	case SET_NPCSABER1:
	case SET_NPCSABER2:
		WP_SetNPCSaber(&g_entities[entID], toSet - SET_SABER1, data);
		break;

	case SET_ENDLESS_BP:
		if (!Q_stricmp("true", data))
			Q3_SetEndlessBP(entID, qtrue);
		else
			Q3_SetEndlessBP(entID, qfalse);
		break;

	case SET_MORELIGHT_PLAYER:
		if (!Q_stricmp("true", data))
			Q3_SetMoreLightnpc(entID, qtrue);
		else
			Q3_SetMoreLightnpc(entID, qfalse);
		break;

	case SET_CONSOLE_COMMAND:
		gi.SendConsoleCommand(va("cl_noprint 1; helpusobi 1; %s ; cl_noprint 0\n", const_cast<char*>(data)));
		break;

	case SET_FFAMODE:
		if (!Q_stricmp("true", data))
		{
			Q3_setFFAMode(entID, qtrue);
		}
		else
		{
			Q3_setFFAMode(entID, qfalse);
		}
		break;

	case SET_ENDLESS_ST:
		if (!Q_stricmp("true", data))
			Q3_SetEndlessST(entID, qtrue);
		else
			Q3_SetEndlessST(entID, qfalse);
		break;

	default:
		SetVar(taskID, entID, type_name, data);
		PrisonerObjCheck(type_name, data);
		break;
	}

	IIcarusInterface::GetIcarus()->Completed(ent->m_iIcarusID, taskID);
}

const char* GetSaberColor(const int color)
{
	switch (color)
	{
	case SABER_RED:
		return "red";
	case SABER_ORANGE:
		return "orange";
	case SABER_YELLOW:
		return "yellow";
	case SABER_GREEN:
		return "green";
	case SABER_BLUE:
		return "blue";
	case SABER_PURPLE:
		return "purple";
	case SABER_UNSTABLE_RED:
		return "unstable_red";
	case SABER_RGB:
		return "rgb";
	case SABER_BLACK:
		return "black";
	default:;
	}

	return "blue";
}

void CQuake3GameInterface::PrisonerObjCheck(const char* name, const char* data)
{
	float float_data = 0.0f;

	if (!Q_stricmp("ui_prisonerobj_currtotal", name))
	{
		GetFloatVariable(name, &float_data);
		const int holdData = static_cast<int>(float_data);
		gi.cvar_set("ui_prisonerobj_currtotal", va("%d", holdData));
	}
	else if (!Q_stricmp("ui_prisonerobj_maxtotal", name))
	{
		gi.cvar_set("ui_prisonerobj_maxtotal", data);
	}
}

// Uses an entity.
void CQuake3GameInterface::Use(const int ent_id, const char* name)
{
	gentity_t* ent = &g_entities[ent_id];

	if (!ent)
	{
		DebugPrint(WL_WARNING, "Use: invalid entID %d\n", ent_id);
		return;
	}

	if (!name || !name[0])
	{
		DebugPrint(WL_WARNING, "Use: string is NULL!\n");
		return;
	}

	if (ent->s.number == 0 && ent->client->NPC_class == CLASS_ATST)
	{
		//a player trying to get out of his ATST
		GEntity_UseFunc(ent->activator, ent, ent);
		return;
	}
	/*
	if ( ent->client )
	{//put the button on him, too, since some other things (like getting out of a vehicle?) check for the use button directly...
		ent->client->usercmd.buttons |= BUTTON_USE;
	}
	*/
	G_UseTargets2(ent, ent, name);
}

void CQuake3GameInterface::Activate(const int entID, const char* name)
{
	Q3_SetInactive(entID, qtrue);
}

void CQuake3GameInterface::Deactivate(const int entID, const char* name)
{
	Q3_SetInactive(entID, qfalse);
}

// Kill an entity.
void CQuake3GameInterface::Kill(const int entID, const char* name)
{
	gentity_t* ent = &g_entities[entID];
	gentity_t* victim;

	if (!Q_stricmp(name, "self"))
	{
		victim = ent;
	}
	else if (!Q_stricmp(name, "enemy"))
	{
		victim = ent->enemy;
	}
	else
	{
		victim = G_Find(nullptr, FOFS(targetname), name);
	}

	if (!victim)
	{
		DebugPrint(WL_WARNING, "Kill: can't find %s\n", name);
		return;
	}

	if (victim == ent)
	{
		//don't ICARUS_FreeEnt me, I'm in the middle of a script!  (FIXME: shouldn't ICARUS handle this internally?)
		victim->svFlags |= SVF_KILLED_SELF;
	}

	const int o_health = victim->health;
	victim->health = 0;
	if (victim->client)
	{
		victim->flags |= FL_NO_KNOCKBACK;
	}
	//G_SetEnemy(victim, ent);
	if (victim->e_DieFunc != dieF_NULL) // check can be omitted
	{
		GEntity_DieFunc(victim, nullptr, nullptr, o_health, MOD_UNKNOWN);
	}
}

// Remove an entity.
void CQuake3GameInterface::Remove(const int entID, const char* name)
{
	gentity_t* ent = &g_entities[entID];
	gentity_t* victim;

	if (!Q_stricmp("self", name))
	{
		victim = ent;
		if (!victim)
		{
			DebugPrint(WL_WARNING, "Remove: can't find %s\n", name);
			return;
		}
		Q3_RemoveEnt(victim);
	}
	else if (!Q_stricmp("enemy", name))
	{
		victim = ent->enemy;
		if (!victim)
		{
			DebugPrint(WL_WARNING, "Remove: can't find %s\n", name);
			return;
		}
		Q3_RemoveEnt(victim);
	}
	else
	{
		victim = G_Find(nullptr, FOFS(targetname), name);
		if (!victim)
		{
			DebugPrint(WL_WARNING, "Remove: can't find %s\n", name);
			return;
		}

		while (victim)
		{
			Q3_RemoveEnt(victim);
			victim = G_Find(victim, FOFS(targetname), name);
		}
	}
}

// Get a random (float) number.
float CQuake3GameInterface::Random(const float min, const float max)
{
	return rand() * (max - min) / static_cast<float>(RAND_MAX) + min;
}

void CQuake3GameInterface::Play(const int taskID, const int entID, const char* type, const char* name)
{
	gentity_t* ent = &g_entities[entID];

	if (!Q_stricmp(type, "PLAY_ROFF"))
	{
		// Try to load the requested ROFF
		if (G_LoadRoff(name))
		{
			ent->roff = G_NewString(name);

			// Start the roff from the beginning
			ent->roff_ctr = 0;

			//Save this off for later
			Q3_TaskIDSet(ent, TID_MOVE_NAV, taskID);

			// Let the ROFF playing start.
			ent->next_roff_time = level.time;

			// These need to be initialised up front...
			VectorCopy(ent->currentOrigin, ent->pos1);
			VectorCopy(ent->currentAngles, ent->pos2);
			gi.linkentity(ent);
		}
	}
}

//Camera functions
void CQuake3GameInterface::CameraPan(vec3_t angles, vec3_t dir, const float duration)
{
	CGCam_Pan(angles, dir, duration);
}

void CQuake3GameInterface::CameraMove(vec3_t origin, const float duration)
{
	CGCam_Move(origin, duration);
}

void CQuake3GameInterface::CameraZoom(const float fov, const float duration)
{
	CGCam_Zoom(fov, duration);
}

void CQuake3GameInterface::CameraRoll(const float angle, const float duration)
{
	CGCam_Roll(angle, duration);
}

void CQuake3GameInterface::CameraFollow(const char* name, const float speed, const float initLerp)
{
	CGCam_Follow(name, speed, initLerp);
}

void CQuake3GameInterface::CameraTrack(const char* name, const float speed, const float initLerp)
{
	CGCam_Track(name, speed, initLerp);
}

void CQuake3GameInterface::CameraDistance(const float dist, const float initLerp)
{
	CGCam_Distance(dist, initLerp);
}

void CQuake3GameInterface::CameraFade(const float sr, const float sg, const float sb, const float sa,
	const float dr, const float dg, const float db,
	const float da, const float duration)
{
	vec4_t src{}, dst{};

	src[0] = sr;
	src[1] = sg;
	src[2] = sb;
	src[3] = sa;

	dst[0] = dr;
	dst[1] = dg;
	dst[2] = db;
	dst[3] = da;

	CGCam_Fade(src, dst, duration);
}

void CQuake3GameInterface::CameraPath(const char* name)
{
	CGCam_StartRoff(G_NewString(name));
}

void CQuake3GameInterface::CameraEnable()
{
	CGCam_Enable();
}

void CQuake3GameInterface::CameraDisable()
{
	CGCam_Disable();
}

void CQuake3GameInterface::CameraShake(const float intensity, const int duration)
{
	CGCam_Shake(intensity, duration);
}

int CQuake3GameInterface::GetFloat(const int entID, const char* name, float* value)
{
	const gentity_t* ent = &g_entities[entID];
	//	gclient_t	*client;

	if (!ent)
	{
		return false;
	}

	if (!Q_stricmpn(name, "cvar_", 5) &&
		strlen(name) > 5)
	{
		*value = static_cast<float>(gi.Cvar_VariableIntegerValue(name + 5));
		return true;
	}

	const int toGet = GetIDForString(setTable, name); //FIXME: May want to make a "getTable" as well
	//FIXME: I'm getting really sick of these huge switch statements!

	//NOTENOTE: return true if the value was correctly obtained
	switch (toGet)
	{
	case SET_PARM1:
	case SET_PARM2:
	case SET_PARM3:
	case SET_PARM4:
	case SET_PARM5:
	case SET_PARM6:
	case SET_PARM7:
	case SET_PARM8:
	case SET_PARM9:
	case SET_PARM10:
	case SET_PARM11:
	case SET_PARM12:
	case SET_PARM13:
	case SET_PARM14:
	case SET_PARM15:
	case SET_PARM16:
		if (ent->parms == nullptr)
		{
			DebugPrint(WL_ERROR, "GET_PARM: %s %s did not have any parms set!\n", ent->classname, ent->targetname);
			return false; // would prefer qfalse, but I'm fitting in with what's here <sigh>
		}
		*value = atof(ent->parms->parm[toGet - SET_PARM1]);
		break;

	case SET_COUNT:
		*value = ent->count;
		break;

	case SET_HEALTH:
		*value = ent->health;
		break;

	case SET_SKILL:
		*value = g_spskill->integer;
		break;

	case SET_XVELOCITY: //## %f="0.0" # Velocity along X axis
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_XVELOCITY, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.velocity[0];
		break;

	case SET_YVELOCITY: //## %f="0.0" # Velocity along Y axis
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_YVELOCITY, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.velocity[1];
		break;

	case SET_ZVELOCITY: //## %f="0.0" # Velocity along Z axis
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_ZVELOCITY, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.velocity[2];
		break;

	case SET_Z_OFFSET:
		*value = ent->currentOrigin[2] - ent->s.origin[2];
		break;

	case SET_DPITCH: //## %f="0.0" # Pitch for NPC to turn to
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_DPITCH, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->desiredPitch;
		break;

	case SET_DYAW: //## %f="0.0" # Yaw for NPC to turn to
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_DYAW, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->desiredPitch;
		break;

	case SET_WIDTH: //## %f="0.0" # Width of NPC bounding box
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_WIDTH, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->mins[0];
		break;
	case SET_TIMESCALE: //## %f="0.0" # Speed-up slow down game (0 - 1.0)
		*value = g_timescale->value;
		break;
	case SET_CAMERA_GROUP_Z_OFS: //## %s="NULL" # all ents with this cameraGroup will be focused on
		return false;

	case SET_VISRANGE: //## %f="0.0" # How far away NPC can see
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_VISRANGE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.visrange;
		break;

	case SET_EARSHOT: //## %f="0.0" # How far an NPC can hear
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_EARSHOT, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.earshot;
		break;

	case SET_VIGILANCE: //## %f="0.0" # How often to look for enemies (0 - 1.0)
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_VIGILANCE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.vigilance;
		break;

	case SET_GRAVITY: //## %f="0.0" # Change this ent's gravity - 800 default
		if (ent->svFlags & SVF_CUSTOM_GRAVITY && ent->client)
		{
			*value = ent->client->ps.gravity;
		}
		else
		{
			*value = g_gravity->value;
		}
		break;

	case SET_FACEEYESCLOSED:
	case SET_FACEEYESOPENED:
	case SET_FACEAUX: //## %f="0.0" # Set face to Aux expression for number of seconds
	case SET_FACEBLINK: //## %f="0.0" # Set face to Blink expression for number of seconds
	case SET_FACEBLINKFROWN: //## %f="0.0" # Set face to Blinkfrown expression for number of seconds
	case SET_FACEFROWN: //## %f="0.0" # Set face to Frown expression for number of seconds
	case SET_FACESMILE: //## %f="0.0" # Set face to Smile expression for number of seconds
	case SET_FACEGLAD: //## %f="0.0" # Set face to Glad expression for number of seconds
	case SET_FACEHAPPY: //## %f="0.0" # Set face to Happy expression for number of seconds
	case SET_FACESHOCKED: //## %f="0.0" # Set face to Shocked expression for number of seconds
	case SET_FACENORMAL: //## %f="0.0" # Set face to Normal expression for number of seconds
		DebugPrint(WL_WARNING, "GetFloat: SET_FACE___ not implemented\n");
		return false;
	case SET_WAIT: //## %f="0.0" # Change an entity's wait field
		*value = ent->wait;
		break;
	case SET_FOLLOWDIST: //## %f="0.0" # How far away to stay from leader in BS_FOLLOW_LEADER
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_FOLLOWDIST, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->followDist;
		break;
	case SET_SCALE: //## %f="0.0" # Scale the entity model
		*value = ent->s.scale;
		break;
	case SET_NPC_SCALE: //## %f="0.0" # Scale the npc model
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NPC_SCALE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->s.scale;
		break;
		//# #sep ints
	case SET_ANIM_HOLDTIME_LOWER: //## %d="0" # Hold lower anim for number of milliseconds
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_ANIM_HOLDTIME_LOWER, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.legsAnimTimer;
		break;
	case SET_ANIM_HOLDTIME_UPPER: //## %d="0" # Hold upper anim for number of milliseconds
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_ANIM_HOLDTIME_UPPER, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.torsoAnimTimer;
		break;
	case SET_ANIM_HOLDTIME_BOTH: //## %d="0" # Hold lower and upper anims for number of milliseconds
		DebugPrint(WL_WARNING, "GetFloat: SET_ANIM_HOLDTIME_BOTH not implemented\n");
		return false;
	case SET_ARMOR: //## %d="0" # Change armor
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_ARMOR, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.stats[STAT_ARMOR];
		break;
	case SET_WALKSPEED: //## %d="0" # Change walkSpeed
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_WALKSPEED, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.walkSpeed;
		break;
	case SET_RUNSPEED: //## %d="0" # Change runSpeed
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_RUNSPEED, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.runSpeed;
		break;
	case SET_YAWSPEED: //## %d="0" # Change yawSpeed
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_YAWSPEED, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.yawSpeed;
		break;
	case SET_AGGRESSION: //## %d="0" # Change aggression 1-5
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_AGGRESSION, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.aggression;
		break;
	case SET_AIM: //## %d="0" # Change aim 1-5
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_AIM, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.aim;
		break;
	case SET_FRICTION: //## %d="0" # Change ent's friction - 6 default
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_FRICTION, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.friction;
		break;
	case SET_SHOOTDIST: //## %d="0" # How far the ent can shoot - 0 uses weapon
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_SHOOTDIST, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.shootDistance;
		break;
	case SET_HFOV: //## %d="0" # Horizontal field of view
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_HFOV, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.hfov;
		break;
	case SET_VFOV: //## %d="0" # Vertical field of view
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_VFOV, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->stats.vfov;
		break;
	case SET_DELAYSCRIPTTIME: //## %d="0" # How many seconds to wait before running delayscript
		*value = ent->delayScriptTime - level.time;
		break;
	case SET_FORWARDMOVE: //## %d="0" # NPC move forward -127(back) to 127
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_FORWARDMOVE, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->forced_forwardmove;
		break;
	case SET_RIGHTMOVE: //## %d="0" # NPC move right -127(left) to 127
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_RIGHTMOVE, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->forced_rightmove;
		break;
	case SET_STARTFRAME: //## %d="0" # frame to start animation sequence on
		*value = ent->startFrame;
		break;
	case SET_ENDFRAME: //## %d="0" # frame to end animation sequence on
		*value = ent->endFrame;
		break;
	case SET_ANIMFRAME: //## %d="0" # of current frame
		*value = ent->s.frame;
		break;

	case SET_SHOT_SPACING: //## %d="1000" # Time between shots for an NPC - reset to defaults when changes weapon
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_SHOT_SPACING, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->burstSpacing;
		break;
	case SET_MISSIONSTATUSTIME: //## %d="0" # Amount of time until Mission Status should be shown after death
		*value = cg.missionStatusDeadTime - level.time;
		break;
		//# #sep booleans
	case SET_IGNOREPAIN: //## %t="BOOL_TYPES" # Do not react to pain
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_IGNOREPAIN, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->ignorePain;
		break;
	case SET_IGNOREENEMIES: //## %t="BOOL_TYPES" # Do not acquire enemies
		*value = ent->svFlags & SVF_IGNORE_ENEMIES;
		break;
	case SET_IGNOREALERTS: //## Do not get enemy set by allies in area(ambush)
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_IGNOREALERTS, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_IGNORE_ALERTS;
		break;

	case SET_DONTSHOOT: //## %t="BOOL_TYPES" # Others won't shoot you
		*value = ent->flags & FL_DONT_SHOOT;
		break;
	case SET_NOTARGET: //## %t="BOOL_TYPES" # Others won't pick you as enemy
		*value = ent->flags & FL_NOTARGET;
		break;
	case SET_DONTFIRE: //## %t="BOOL_TYPES" # Don't fire your weapon
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_DONTFIRE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_DONT_FIRE;
		break;

	case SET_LOCKED_ENEMY: //## %t="BOOL_TYPES" # Keep current enemy until dead
		*value = ent->svFlags & SVF_LOCKEDENEMY;
		break;
	case SET_CROUCHED: //## %t="BOOL_TYPES" # Force NPC to crouch
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_CROUCHED, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_CROUCHED;
		break;
	case SET_WALKING: //## %t="BOOL_TYPES" # Force NPC to move at walkSpeed
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_WALKING, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_WALKING;
		break;
	case SET_RUNNING: //## %t="BOOL_TYPES" # Force NPC to move at runSpeed
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_RUNNING, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_RUNNING;
		break;
	case SET_CHASE_ENEMIES: //## %t="BOOL_TYPES" # NPC will chase after enemies
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_CHASE_ENEMIES, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_CHASE_ENEMIES;
		break;
	case SET_LOOK_FOR_ENEMIES: //## %t="BOOL_TYPES" # NPC will be on the lookout for enemies
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_LOOK_FOR_ENEMIES, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_LOOK_FOR_ENEMIES;
		break;
	case SET_FACE_MOVE_DIR: //## %t="BOOL_TYPES" # NPC will face in the direction it's moving
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_FACE_MOVE_DIR, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_FACE_MOVE_DIR;
		break;
	case SET_FORCED_MARCH: //## %t="BOOL_TYPES" # Force NPC to move at runSpeed
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_FORCED_MARCH, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SET_FORCED_MARCH;
		break;
	case SET_UNDYING: //## %t="BOOL_TYPES" # Can take damage down to 1 but not die
		*value = ent->flags & FL_UNDYING;
		break;
	case SET_NOAVOID: //## %t="BOOL_TYPES" # Will not avoid other NPCs or architecture
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NOAVOID, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->aiFlags & NPCAI_NO_COLL_AVOID;
		break;

	case SET_SOLID: //## %t="BOOL_TYPES" # Make yourself notsolid or solid
		*value = ent->contents;
		break;
	case SET_PLAYER_USABLE: //## %t="BOOL_TYPES" # Can be activateby the player's "use" button
		*value = ent->svFlags & SVF_PLAYER_USABLE;
		break;
	case SET_LOOP_ANIM: //## %t="BOOL_TYPES" # For non-NPCs: loop your animation sequence
		*value = ent->loopAnim;
		break;
	case SET_INTERFACE: //## %t="BOOL_TYPES" # Player interface on/off
		DebugPrint(WL_WARNING, "GetFloat: SET_INTERFACE not implemented\n");
		return false;
	case SET_SHIELDS: //## %t="BOOL_TYPES" # NPC has no shields (Borg do not adapt)
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_SHIELDS, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->aiFlags & NPCAI_SHIELDS;
		break;
	case SET_SABERACTIVE:
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_SABERACTIVE, %s not a client\n", ent->targetname);
			return false;
		}
		*value = ent->client->ps.SaberActive();
		break;
	case SET_INVISIBLE: //## %t="BOOL_TYPES" # Makes an NPC not solid and not visible
		*value = ent->s.eFlags & EF_NODRAW;
		break;
	case SET_VAMPIRE: //## %t="BOOL_TYPES" # Makes an NPC not solid and not visible
	{
		if (!ent->client)
		{
			return false;
		}
		*value = ent->client->ps.powerups[PW_DISINT_2] > level.time;
	}
	break;
	case SET_FORCE_INVINCIBLE: //## %t="BOOL_TYPES" # Makes an NPC not solid and not visible
	{
		if (!ent->client)
		{
			return false;
		}
		*value = ent->client->ps.powerups[PW_INVINCIBLE] > level.time;
	}
	break;
	case SET_GREET_ALLIES: //## %t="BOOL_TYPES" # Makes an NPC greet teammates
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_GREET_ALLIES, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->aiFlags & NPCAI_GREET_ALLIES;
		break;
	case SET_VIDEO_FADE_IN: //## %t="BOOL_TYPES" # Makes video playback fade in
		DebugPrint(WL_WARNING, "GetFloat: SET_VIDEO_FADE_IN not implemented\n");
		return false;
	case SET_VIDEO_FADE_OUT: //## %t="BOOL_TYPES" # Makes video playback fade out
		DebugPrint(WL_WARNING, "GetFloat: SET_VIDEO_FADE_OUT not implemented\n");
		return false;
	case SET_PLAYER_LOCKED: //## %t="BOOL_TYPES" # Makes it so player cannot move
		*value = player_locked;
		break;
	case SET_LOCK_PLAYER_WEAPONS: //## %t="BOOL_TYPES" # Makes it so player cannot switch weapons
		*value = ent->flags & FL_LOCK_PLAYER_WEAPONS;
		break;
	case SET_NO_IMPACT_DAMAGE: //## %t="BOOL_TYPES" # Makes it so player cannot switch weapons
		*value = ent->flags & FL_NO_IMPACT_DMG;
		break;
	case SET_NO_KNOCKBACK: //## %t="BOOL_TYPES" # Stops this ent from taking knockback from weapons
		*value = ent->flags & FL_NO_KNOCKBACK;
		break;
	case SET_ALT_FIRE: //## %t="BOOL_TYPES" # Force NPC to use altfire when shooting
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_ALT_FIRE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_ALT_FIRE;
		break;
	case SET_NO_RESPONSE:
		//## %t="BOOL_TYPES" # NPCs will do generic responses when this is on (usescripts override generic responses as well)
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NO_RESPONSE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_NO_RESPONSE;
		break;
	case SET_INVINCIBLE: //## %t="BOOL_TYPES" # Completely unkillable
		*value = ent->flags & FL_GODMODE;
		break;
	case SET_MISSIONSTATUSACTIVE: //# Turns on Mission Status Screen
		*value = cg.missionStatusShow;
		break;
	case SET_NO_COMBAT_TALK: //## %t="BOOL_TYPES" # NPCs will not do their combat talking noises when this is on
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NO_COMBAT_TALK, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_NO_COMBAT_TALK;
		break;
	case SET_NO_ALERT_TALK: //## %t="BOOL_TYPES" # NPCs will not do their combat talking noises when this is on
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NO_ALERT_TALK, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_NO_ALERT_TALK;
		break;
	case SET_USE_CP_NEAREST:
		//## %t="BOOL_TYPES" # NPCs will use their closest combat points, not try and find ones next to the player, or flank player
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_USE_CP_NEAREST, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_USE_CP_NEAREST;
		break;
	case SET_DISMEMBERABLE: //## %t="BOOL_TYPES" # NPC will not be affected by force powers
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_DISMEMBERABLE, %s not a client\n", ent->targetname);
			return false;
		}
		*value = !ent->client->dismembered;
		break;
	case SET_NO_FORCE:
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NO_FORCE, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_NO_FORCE;
		break;
	case SET_NO_ACROBATICS:
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NO_ACROBATICS, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_NO_ACROBATICS;
		break;
	case SET_USE_SUBTITLES:
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_USE_SUBTITLES, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_USE_SUBTITLES;
		break;
	case SET_NO_FALLTODEATH: //## %t="BOOL_TYPES" # NPC will not be affected by force powers
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_NO_FALLTODEATH, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_NO_FALLTODEATH;
		break;
	case SET_MORELIGHT:
		//## %t="BOOL_TYPES" # NPCs will use their closest combat points, not try and find ones next to the player, or flank player
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetFloat: SET_MORELIGHT, %s not an NPC\n", ent->targetname);
			return false;
		}
		*value = ent->NPC->scriptFlags & SCF_MORELIGHT;
		break;
	case SET_TREASONED:
		//## %t="BOOL_TYPES" # Player has turned on his own- scripts will stop: NPCs will turn on him and level changes load the brig
		DebugPrint(WL_VERBOSE, "SET_TREASONED is disabled, do not use\n");
		*value = 0; //(ffireLevel>=FFIRE_LEVEL_RETALIATION);
		break;
	case SET_DISABLE_SHADER_ANIM: //## %t="BOOL_TYPES" # Shaders won't animate
		*value = ent->s.eFlags & EF_DISABLE_SHADER_ANIM;
		break;
	case SET_SHADER_ANIM: //## %t="BOOL_TYPES" # Shader will be under frame control
		*value = ent->s.eFlags & EF_SHADER_ANIM;
		break;
	case SET_ENDLESS_BP: //## %t="BOOL_TYPES" # ENLESS BLOCK POINTS
		*value = ent->flags & FL_BLOCKPOINTMODE;
		break;
	case SET_NO_SABER_RETURN://## %t="BOOL_TYPES" # Stops this ent from taking knockback from weapons
		*value = ent->flags & FL_NO_SABER_RETURN;
		break;

	case SET_OBJECTIVE_LIGHTSIDE:
	{
		if (g_darkkorriban->integer)
		{
			*value = 2;
			player->client->playerTeam = TEAM_FREE;
			player->client->enemyTeam = TEAM_FREE;
		}
		else
		{
			*value = level.clients[0].sess.mission_objectives[LIGHTSIDE_OBJ].status;
		}
		break;
	}

	// kef 4/16/03 -- just trying to put together some scripted meta-AI for swoop riders
	case SET_DISTSQRD_TO_PLAYER:
	{
		vec3_t distSquared;

		VectorSubtract(player->currentOrigin, ent->s.origin, distSquared);

		*value = VectorLengthSquared(distSquared);
		break;
	}
	case SET_MORELIGHT_PLAYER:
		*value = ent->flags & FL_MORELIGHPLAYER;
		break;

	case SET_ENDLESS_ST: //## %t="BOOL_TYPES" # ENLESS STAMINA POINTS
		*value = ent->flags & FL_STAMINA_MODE;
		break;

	default:
		if (VariableDeclared(name) != VTYPE_FLOAT)
			return false;

		return GetFloatVariable(name, value);
	}

	return true;
}

int CQuake3GameInterface::GetVector(const int entID, const char* name, vec3_t value)
{
	const gentity_t* ent = &g_entities[entID];
	if (!ent)
	{
		return false;
	}

	const int toGet = GetIDForString(setTable, name); //FIXME: May want to make a "getTable" as well
	//FIXME: I'm getting really sick of these huge switch statements!

	//NOTENOTE: return true if the value was correctly obtained
	switch (toGet)
	{
	case SET_PARM1:
	case SET_PARM2:
	case SET_PARM3:
	case SET_PARM4:
	case SET_PARM5:
	case SET_PARM6:
	case SET_PARM7:
	case SET_PARM8:
	case SET_PARM9:
	case SET_PARM10:
	case SET_PARM11:
	case SET_PARM12:
	case SET_PARM13:
	case SET_PARM14:
	case SET_PARM15:
	case SET_PARM16:
		sscanf(ent->parms->parm[toGet - SET_PARM1], "%f %f %f", &value[0], &value[1], &value[2]);
		break;

	case SET_ORIGIN:
		VectorCopy(ent->currentOrigin, value);
		break;

	case SET_ANGLES:
		VectorCopy(ent->currentAngles, value);
		break;

	case SET_TELEPORT_DEST: //## %v="0.0 0.0 0.0" # Set origin here as soon as the area is clear
		DebugPrint(WL_WARNING, "GetVector: SET_TELEPORT_DEST not implemented\n");
		return false;

	default:

		if (VariableDeclared(name) != VTYPE_VECTOR)
			return false;

		return GetVectorVariable(name, value);
	}

	return true;
}

int CQuake3GameInterface::GetString(const int entID, const char* name, char** value)
{
	const gentity_t* ent = &g_entities[entID];
	if (!ent)
	{
		return false;
	}

	if (!Q_stricmpn(name, "cvar_", 5) &&
		strlen(name) > 5)
	{
		gi.Cvar_VariableStringBuffer(name + 5, *value, strlen(*value));
		return true;
	}

	const int toGet = GetIDForString(setTable, name); //FIXME: May want to make a "getTable" as well

	switch (toGet)
	{
	case SET_ANIM_BOTH:
		*value = Q3_GetAnimBoth(ent);

		if (VALIDSTRING(*value) == false)
			return false;

		break;

	case SET_PARM1:
	case SET_PARM2:
	case SET_PARM3:
	case SET_PARM4:
	case SET_PARM5:
	case SET_PARM6:
	case SET_PARM7:
	case SET_PARM8:
	case SET_PARM9:
	case SET_PARM10:
	case SET_PARM11:
	case SET_PARM12:
	case SET_PARM13:
	case SET_PARM14:
	case SET_PARM15:
	case SET_PARM16:
		if (ent->parms)
		{
			*value = static_cast<char*>(ent->parms->parm[toGet - SET_PARM1]);
		}
		else
		{
			DebugPrint(WL_WARNING, "GetString: invalid ent %s has no parms!\n", ent->targetname);
			return false;
		}
		break;

	case SET_TARGET:
		*value = ent->target;
		break;

	case SET_LOCATION:
		*value = G_GetLocationForEnt(ent);
		if (!value || !value[0])
		{
			return false;
		}
		break;

		//# #sep Scripts and other file paths
	case SET_SPAWNSCRIPT:
		//## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when spawned //0 - do not change these, these are equal to BSET_SPAWN, etc
		*value = ent->behaviorSet[BSET_SPAWN];
		break;
	case SET_USESCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when used
		*value = ent->behaviorSet[BSET_USE];
		break;
	case SET_AWAKESCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when startled
		*value = ent->behaviorSet[BSET_AWAKE];
		break;
	case SET_ANGERSCRIPT:
		//## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script run when find an enemy for the first time
		*value = ent->behaviorSet[BSET_ANGER];
		break;
	case SET_ATTACKSCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when you shoot
		*value = ent->behaviorSet[BSET_ATTACK];
		break;
	case SET_VICTORYSCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when killed someone
		*value = ent->behaviorSet[BSET_VICTORY];
		break;
	case SET_LOSTENEMYSCRIPT:
		//## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when you can't find your enemy
		*value = ent->behaviorSet[BSET_LOSTENEMY];
		break;
	case SET_PAINSCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when hit
		*value = ent->behaviorSet[BSET_PAIN];
		break;
	case SET_FLEESCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when hit and low health
		*value = ent->behaviorSet[BSET_FLEE];
		break;
	case SET_DEATHSCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when killed
		*value = ent->behaviorSet[BSET_DEATH];
		break;
	case SET_DELAYEDSCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run after a delay
		*value = ent->behaviorSet[BSET_DELAYED];
		break;
	case SET_BLOCKEDSCRIPT:
		//## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when blocked by teammate
		*value = ent->behaviorSet[BSET_BLOCKED];
		break;
	case SET_FFIRESCRIPT:
		//## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when player has shot own team repeatedly
		*value = ent->behaviorSet[BSET_FFIRE];
		break;
	case SET_FFDEATHSCRIPT:
		//## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when player kills a teammate
		*value = ent->behaviorSet[BSET_FFDEATH];
		break;

		//# #sep Standard strings
	case SET_ENEMY: //## %s="NULL" # Set enemy by targetname
		if (ent->enemy != nullptr)
		{
			*value = ent->enemy->targetname;
		}
		else return false;
		break;
	case SET_LEADER: //## %s="NULL" # Set for BS_FOLLOW_LEADER
	{
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetString: SET_LEADER, %s not a client\n", ent->targetname);
			return false;
		}
		if (ent->client->leader)
		{
			*value = ent->client->leader->targetname;
		}
		else return false;
	}
	break;
	case SET_CAPTURE: //## %s="NULL" # Set captureGoal by targetname
	{
		if (ent->NPC == nullptr)
		{
			DebugPrint(WL_WARNING, "GetString: SET_CAPTURE, %s not an NPC\n", ent->targetname);
			return false;
		}
		if (ent->NPC->captureGoal != nullptr)
		{
			*value = ent->NPC->captureGoal->targetname;
		}
		else return false;
	}
	break;

	case SET_TARGETNAME: //## %s="NULL" # Set/change your targetname
		*value = ent->targetname;
		break;
	case SET_PAINTARGET: //## %s="NULL" # Set/change what to use when hit
		*value = ent->paintarget;
		break;
	case SET_PLAYERMODEL:
		*value = ent->NPC_type;
		break;
	case SET_CAMERA_GROUP: //## %s="NULL" # all ents with this cameraGroup will be focused on
		*value = ent->cameraGroup;
		break;
	case SET_CAMERA_GROUP_TAG: //## %s="NULL" # all ents with this cameraGroup will be focused on
		return false;
	case SET_LOOK_TARGET: //## %s="NULL" # object for NPC to look at
	{
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetString: SET_LOOK_TARGET, %s not a client\n", ent->targetname);
			return false;
		}
		const gentity_t* lookTarg = &g_entities[ent->client->renderInfo.lookTarget];
		if (lookTarg != nullptr)
		{
			*value = lookTarg->targetname;
		}
		else return false;
	}
	break;
	case SET_TARGET2:
		//## %s="NULL" # Set/change your target2: on NPC's: this fires when they're knocked out by the red hypo
		*value = ent->target2;
		break;

	case SET_REMOVE_TARGET: //## %s="NULL" # Target that is fired when someone completes the BS_REMOVE behaviorState
		*value = ent->target3;
		break;
	case SET_WEAPON:
	{
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetString: SET_WEAPON, %s not a client\n", ent->targetname);
			return false;
		}
		*value = const_cast<char*>(GetStringForID(WPTable, ent->client->ps.weapon));
	}
	break;

	case SET_ITEM:
	{
		if (ent->client == nullptr)
		{
			DebugPrint(WL_WARNING, "GetString: SET_ITEM, %s not a client\n", ent->targetname);
			return false;
		}
		//	*value = (char *)GetStringForID( WPTable, ent->client->ps.weapon );
	}
	break;
	case SET_MUSIC_STATE:
		*value = const_cast<char*>(GetStringForID(DMSTable, level.dmState));
		break;
		//The below cannot be gotten
	case SET_NAVGOAL: //## %s="NULL" # *Move to this navgoal then continue script
		DebugPrint(WL_WARNING, "GetString: SET_NAVGOAL not implemented\n");
		return false;
	case SET_VIEWTARGET: //## %s="NULL" # Set angles toward ent by targetname
		DebugPrint(WL_WARNING, "GetString: SET_VIEWTARGET not implemented\n");
		return false;
	case SET_WATCHTARGET: //## %s="NULL" # Set angles toward ent by targetname
		if (ent && ent->NPC && ent->NPC->watchTarget)
		{
			*value = ent->NPC->watchTarget->targetname;
		}
		else
		{
			DebugPrint(WL_WARNING, "GetString: SET_WATCHTARGET no watchTarget!\n");
			return false;
		}
		break;
	case SET_VIEWENTITY:
		DebugPrint(WL_WARNING, "GetString: SET_VIEWENTITY not implemented\n");
		return false;
	case SET_CAPTIONTEXTCOLOR: //## %s=""  # Color of text RED:WHITE:BLUE: YELLOW
		DebugPrint(WL_WARNING, "GetString: SET_CAPTIONTEXTCOLOR not implemented\n");
		return false;
	case SET_CENTERTEXTCOLOR: //## %s=""  # Color of text RED:WHITE:BLUE: YELLOW
		DebugPrint(WL_WARNING, "GetString: SET_CENTERTEXTCOLOR not implemented\n");
		return false;
	case SET_SCROLLTEXTCOLOR: //## %s=""  # Color of text RED:WHITE:BLUE: YELLOW
		DebugPrint(WL_WARNING, "GetString: SET_SCROLLTEXTCOLOR not implemented\n");
		return false;
	case SET_COPY_ORIGIN: //## %s="targetname"  # Copy the origin of the ent with targetname to your origin
		DebugPrint(WL_WARNING, "GetString: SET_COPY_ORIGIN not implemented\n");
		return false;
	case SET_DEFEND_TARGET: //## %s="targetname"  # This NPC will attack the target NPC's enemies
		DebugPrint(WL_WARNING, "GetString: SET_COPY_ORIGIN not implemented\n");
		return false;
	case SET_VIDEO_PLAY: //## %s="filename" !!"W:\game\base\video\!!#*.roq" # Play a Video (inGame)
		DebugPrint(WL_WARNING, "GetString: SET_VIDEO_PLAY not implemented\n");
		return false;
	case SET_LOADGAME: //## %s="exitholodeck" # Load the savegame that was auto-saved when you started the holodeck
		DebugPrint(WL_WARNING, "GetString: SET_LOADGAME not implemented\n");
		return false;
	case SET_LOCKYAW: //## %s="off"  # Lock legs to a certain yaw angle (or "off" or "auto" uses current)
		DebugPrint(WL_WARNING, "GetString: SET_LOCKYAW not implemented\n");
		return false;
	case SET_SCROLLTEXT: //## %s="" # key of text string to print
		DebugPrint(WL_WARNING, "GetString: SET_SCROLLTEXT not implemented\n");
		return false;
	case SET_LCARSTEXT: //## %s="" # key of text string to print in LCARS frame
		DebugPrint(WL_WARNING, "GetString: SET_LCARSTEXT not implemented\n");
		return false;

	case SET_CENTERTEXT:
		DebugPrint(WL_WARNING, "GetString: SET_CENTERTEXT not implemented\n");
		return false;

	case SET_UNDYINGPLAYERVICTORYSCRIPT: //## %s="NULL" !!"W:\game\base\scripts\!!#*.txt" # Script to run when killed someone
		*value = ent->behaviorSet[UNDYINGPLAYERVICTORY];
		break;

	default:
		if (VariableDeclared(name) != VTYPE_STRING)
			return false;

		return GetStringVariable(name, const_cast<const char**>(value));
	}

	return true;
}

int CQuake3GameInterface::Evaluate(int p1Type, const char* p1, int p2Type, const char* p2, const int operatorType)
{
	float f1 = 0, f2 = 0;
	vec3_t v1{}, v2{};
	const char* c1 = nullptr, * c2 = nullptr;
	int i1 = 0, i2 = 0;

	//Always demote to int on float to integer comparisons
	if (p1Type == TK_FLOAT && p2Type == TK_INT || p1Type == TK_INT && p2Type == TK_FLOAT)
	{
		p1Type = TK_INT;
		p2Type = TK_INT;
	}

	//Cannot compare two disimilar types
	if (p1Type != p2Type)
	{
		DebugPrint(WL_ERROR, "Evaluate comparing two disimilar types!\n");
		return false;
	}

	//Format the parameters
	switch (p1Type)
	{
	case TK_FLOAT:
		sscanf(p1, "%f", &f1);
		sscanf(p2, "%f", &f2);
		break;

	case TK_INT:
		sscanf(p1, "%d", &i1);
		sscanf(p2, "%d", &i2);
		break;

	case TK_VECTOR:
		sscanf(p1, "%f %f %f", &v1[0], &v1[1], &v1[2]);
		sscanf(p2, "%f %f %f", &v2[0], &v2[1], &v2[2]);
		break;

	case TK_STRING:
	case TK_IDENTIFIER:
		c1 = const_cast<char*>(p1);
		c2 = const_cast<char*>(p2);
		break;

	default:
		DebugPrint(WL_WARNING, "Evaluate unknown type used!\n");
		return false;
	}

	//Compare them and return the result

	//FIXME: YUCK!!!  Better way to do this?

	switch (operatorType)
	{
		//
		//	EQUAL TO
		//
	case TK_EQUALS:

		switch (p1Type)
		{
		case TK_FLOAT:
			return f1 == f2;

		case TK_INT:
			return i1 == i2;

		case TK_VECTOR:
			return VectorCompare(v1, v2);

		case TK_STRING:
		case TK_IDENTIFIER:
			return !Q_stricmp(c1, c2);
			//NOTENOTE: The script uses proper string comparison logic (ex. ( a == a ) == true )

		default:
			DebugPrint(WL_ERROR, "Evaluate unknown type used!\n");
			return false;
		}

		//
		//	GREATER THAN
		//

	case TK_GREATER_THAN:

		switch (p1Type)
		{
		case TK_FLOAT:
			return f1 > f2;

		case TK_INT:
			return i1 > i2;

		case TK_VECTOR:
			DebugPrint(WL_ERROR, "Evaluate vector comparisons of type GREATER THAN cannot be performed!");
			return false;

		case TK_STRING:
		case TK_IDENTIFIER:
			DebugPrint(WL_ERROR, "Evaluate string comparisons of type GREATER THAN cannot be performed!");
			return false;

		default:
			DebugPrint(WL_ERROR, "Evaluate unknown type used!\n");
			return false;
		}

		//
		//	LESS THAN
		//

	case TK_LESS_THAN:

		switch (p1Type)
		{
		case TK_FLOAT:
			return f1 < f2;

		case TK_INT:
			return i1 < i2;

		case TK_VECTOR:
			DebugPrint(WL_ERROR, "Evaluate vector comparisons of type LESS THAN cannot be performed!");
			return false;

		case TK_STRING:
		case TK_IDENTIFIER:
			DebugPrint(WL_ERROR, "Evaluate string comparisons of type LESS THAN cannot be performed!");
			return false;

		default:
			DebugPrint(WL_ERROR, "Evaluate unknown type used!\n");
			return false;
		}

		//
		//	NOT
		//

	case TK_NOT: //NOTENOTE: Implied "NOT EQUAL TO"

		switch (p1Type)
		{
		case TK_FLOAT:
			return f1 != f2;

		case TK_INT:
			return i1 != i2;

		case TK_VECTOR:
			return !VectorCompare(v1, v2);

		case TK_STRING:
		case TK_IDENTIFIER:
			return Q_stricmp(c1, c2);

		default:
			DebugPrint(WL_ERROR, "Evaluate unknown type used!\n");
			return false;
		}

		//
		//	GREATER THAN OR EQUAL TO
		//

	case TK_GE:

		switch (p1Type)
		{
		case TK_FLOAT:
			return f1 >= f2;

		case TK_INT:
			return i1 >= i2;

		case TK_VECTOR:
			DebugPrint(WL_ERROR,
				"Evaluate vector comparisons of type GREATER THAN OR EQUAL TO cannot be performed!");
			return false;

		case TK_STRING:
		case TK_IDENTIFIER:
			DebugPrint(WL_ERROR,
				"Evaluate string comparisons of type GREATER THAN OR EQUAL TO cannot be performed!");
			return false;

		default:
			DebugPrint(WL_ERROR, "Evaluate unknown type used!\n");
			return false;
		}

		//
		//	LESS THAN OR EQUAL TO
		//

	case TK_LE:

		switch (p1Type)
		{
		case TK_FLOAT:
			return f1 <= f2;

		case TK_INT:
			return i1 <= i2;

		case TK_VECTOR:
			DebugPrint(WL_ERROR, "Evaluate vector comparisons of type LESS THAN OR EQUAL TO cannot be performed!");
			return false;

		case TK_STRING:
		case TK_IDENTIFIER:
			DebugPrint(WL_ERROR, "Evaluate string comparisons of type LESS THAN OR EQUAL TO cannot be performed!");
			return false;

		default:
			DebugPrint(WL_ERROR, "Evaluate unknown type used!\n");
			return false;
		}

	default:
		DebugPrint(WL_ERROR, "Evaluate unknown operator used!\n");
		break;
	}

	return false;
}

void CQuake3GameInterface::DeclareVariable(const int type, const char* name)
{
	//Cannot declare the same variable twice
	if (VariableDeclared(name) != VTYPE_NONE)
		return;

	if (m_numVariables > MAX_VARIABLES)
	{
		DebugPrint(WL_ERROR, "too many variables already declared, maximum is %d\n", MAX_VARIABLES);
		return;
	}

	switch (type)
	{
	case TK_FLOAT:
		m_varFloats[name] = 0.0f;
		break;

	case TK_STRING:
		m_varStrings[name] = "NULL";
		break;

	case TK_VECTOR:
		m_varVectors[name] = "0.0 0.0 0.0";
		break;

	default:
		DebugPrint(WL_ERROR, "unknown INT_ID('t','y','p','e') for declare() function!\n");
		return;
	}

	m_numVariables++;
}

void CQuake3GameInterface::FreeVariable(const char* name)
{
	//Check the strings
	const auto vsi = m_varStrings.find(name);

	if (vsi != m_varStrings.end())
	{
		m_varStrings.erase(vsi);
		m_numVariables--;
		return;
	}

	//Check the floats
	const auto vfi = m_varFloats.find(name);

	if (vfi != m_varFloats.end())
	{
		m_varFloats.erase(vfi);
		m_numVariables--;
		return;
	}

	//Check the strings
	const auto vvi = m_varVectors.find(name);

	if (vvi != m_varVectors.end())
	{
		m_varVectors.erase(vvi);
		m_numVariables--;
	}
}

//Save / Load functions
ojk::ISavedGame* CQuake3GameInterface::get_saved_game_file()
{
	return gi.saved_game;
}

int CQuake3GameInterface::LinkGame(const int entID, const int icarusID)
{
	gentity_t* pEntity = &g_entities[entID];

	if (pEntity == nullptr)
		return false;

	// Set the icarus ID.
	pEntity->m_iIcarusID = icarusID;

	// Associate this Entity with the entity list.
	AssociateEntity(pEntity);

	return true;
}

// Access functions
int CQuake3GameInterface::CreateIcarus(const int entID)
{
	gentity_t* pEntity = &g_entities[entID];

	// If the entity doesn't have an Icarus ID, obtain and assign a new one.
	if (pEntity->m_iIcarusID == IIcarusInterface::ICARUS_INVALID)
		pEntity->m_iIcarusID = IIcarusInterface::GetIcarus()->GetIcarusID(entID);

	return pEntity->m_iIcarusID;
}

//Polls the engine for the sequencer of the entity matching the name passed
int CQuake3GameInterface::GetByName(const char* name)
{
	char temp[1024];

	if (name == nullptr || name[0] == '\0')
		return -1;

	strncpy(temp, name, sizeof temp);
	temp[sizeof temp - 1] = 0;

	const auto ei = m_EntityList.find(Q_strupr(temp));

	if (ei == m_EntityList.end())
		return -1;

	const gentity_t* ent = &g_entities[(*ei).second];

	return ent->s.number;

	// this now returns the ent instead of the sequencer -- dmv 06/27/01
	//	if (ent == NULL)
	//		return NULL;
	//	return ent->sequencer;
}

// (g_entities[m_ownerID].svFlags&SVF_ICARUS_FREEZE)
// return -1 indicates invalid
int CQuake3GameInterface::IsFrozen(const int entID)
{
	return g_entities[entID].svFlags & SVF_ICARUS_FREEZE;
}

void CQuake3GameInterface::Free(void* data)
{
	gi.Free(data);
}

void* CQuake3GameInterface::Malloc(const int size)
{
	return gi.Malloc(size, TAG_ICARUS, qtrue);
}

float CQuake3GameInterface::MaxFloat()
{
	// CHANGE!
	return 34000000;
}

// Script Precache functions.
void CQuake3GameInterface::PrecacheRoff(const char* name)
{
	G_LoadRoff(name);
}

void CQuake3GameInterface::PrecacheScript(const char* name)
{
	char newname[1024]; //static char newname[1024];
	// Strip the extension since we want the real name of the script.
	COM_StripExtension(name, newname, sizeof newname);

	char* pBuf = nullptr;
	int iLength = 0;

	// Try to Register the Script.
	switch (RegisterScript(newname, reinterpret_cast<void**>(&pBuf), iLength))
	{
		// If the script has already been registered (or could not be loaded), leave!
	case SCRIPT_COULDNOTREGISTER:
		if (!Q_stricmp(newname, "NULL") || !Q_stricmp(newname, "default"))
		{
			//these are not real errors, suppress warning
			return;
		}
		Quake3Game()->DebugPrint(WL_ERROR, "PrecacheScript: Failed to load %s!\n", newname);
		assert(SCRIPT_COULDNOTREGISTER);
		return;
	case SCRIPT_ALREADYREGISTERED:
		return;

		// We loaded the script and registered it, so precache it through Icarus now.
	case SCRIPT_REGISTERED:
		IIcarusInterface::GetIcarus()->Precache(pBuf, iLength);
	default:;
	}
}

void CQuake3GameInterface::PrecacheSound(const char* name)
{
	char finalName[MAX_QPATH];

	Q_strncpyz(finalName, name, MAX_QPATH);
	Q_strlwr(finalName);
	if (com_buildScript->integer)
	{
		//get the male sound first
		G_SoundIndex(finalName);
	}
	G_AddSexToPlayerString(finalName, qtrue); //now get female

	G_SoundIndex(finalName);
}

void CQuake3GameInterface::PrecacheFromSet(const char* setname, const char* filename)
{
	//JW NOTENOTE: This will not catch special case get() inlines! (There's not really a good way to do that)

	// Get the id for this set identifier then check against valid types.
	switch (GetIDForString(setTable, setname))
	{
	case SET_SPAWNSCRIPT:
	case SET_USESCRIPT:
	case SET_AWAKESCRIPT:
	case SET_ANGERSCRIPT:
	case SET_ATTACKSCRIPT:
	case SET_VICTORYSCRIPT:
	case SET_LOSTENEMYSCRIPT:
	case SET_PAINSCRIPT:
	case SET_FLEESCRIPT:
	case SET_DEATHSCRIPT:
	case SET_DELAYEDSCRIPT:
	case SET_BLOCKEDSCRIPT:
	case SET_FFIRESCRIPT:
	case SET_FFDEATHSCRIPT:
	case SET_MINDTRICKSCRIPT:
	case SET_CINEMATIC_SKIPSCRIPT:
	case SET_UNDYINGPLAYERVICTORYSCRIPT:
		PrecacheScript(filename);
		break;

	case SET_LOOPSOUND: //like ID_SOUND, but set's looping
		G_SoundIndex(filename);
		break;

	case SET_VIDEO_PLAY: //in game cinematic
		if (com_buildScript->integer)
		{
			fileHandle_t file;
			char name[MAX_OSPATH];

			if (strstr(filename, "/") == nullptr && strstr(filename, "\\") == nullptr)
			{
				Com_sprintf(name, sizeof name, "video/%s", filename);
			}
			else
			{
				Com_sprintf(name, sizeof name, "%s", filename);
			}
			COM_StripExtension(name, name, sizeof name);
			COM_DefaultExtension(name, sizeof name, ".roq");

			gi.FS_FOpenFile(name, &file, FS_READ); // trigger the file copy
			if (file)
			{
				gi.FS_FCloseFile(file);
			}
		}
		break;

	case SET_ADDRHANDBOLT_MODEL:
	case SET_ADDLHANDBOLT_MODEL:
	{
		gi.G2API_PrecacheGhoul2Model(filename);
	}
	break;
	case SET_WEAPON:
	{
		const int wp = GetIDForString(WPTable, filename);
		if (wp > 0)
		{
			const gitem_t* item = FindItemForWeapon(static_cast<weapon_t>(wp));
			RegisterItem(item); //make sure the weapon is cached in case this runs at startup
		}
	}
	break;

	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
/*				END Quake 3 Interface Declarations END					*/
//////////////////////////////////////////////////////////////////////////