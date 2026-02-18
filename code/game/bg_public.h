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

#ifndef __BG_PUBLIC_H__
#define __BG_PUBLIC_H__
// bg_public.h -- definitions shared by both the server game and client game modules
#include "weapons.h"
#include "g_items.h"
#include "teams.h"
#include "statindex.h"

#define DEFAULT_SABER			"Kyle"
#define DEFAULT_SABER_NAME		"lightsaber"
#define DEFAULT_SABER_STAFF		"dual_1"
#define DEFAULT_SABER_MODEL		"models/weapons2/saber_1/saber_1.glm"
#define	DEFAULT_MODEL			"kyle"
#define DEFAULT_MODEL_FEMALE	"jan"
#define DEFAULT_BACKHANDSABER_MODEL		"models/weapons2/saber_plasma/saber_ahsoka3.glm"

constexpr auto DEFAULT_GRAVITY = 800;
constexpr auto GIB_HEALTH = -40;
constexpr auto ARMOR_PROTECTION = 0.40;

constexpr auto MAX_ITEMS = 256;

constexpr auto RANK_TIED_FLAG = 0x4000;

constexpr auto DEFAULT_SHOTGUN_SPREAD = 700;
constexpr auto DEFAULT_SHOTGUN_COUNT = 11;

constexpr auto ITEM_RADIUS = 15; // item sizes are needed for client side pickup detection;

//Player sizes
extern float DEFAULT_MINS_0;
extern float DEFAULT_MINS_1;
extern float DEFAULT_MAXS_0;
extern float DEFAULT_MAXS_1;
extern float DEFAULT_PLAYER_RADIUS;

constexpr auto DEFAULT_MINS_2 = -24;
constexpr auto DEFAULT_MAXS_2 = 40; // was 32, but too short for player;
constexpr auto CROUCH_MAXS_2 = 16;

constexpr auto ATST_MINS0 = -40;
constexpr auto ATST_MINS1 = -40;
constexpr auto ATST_MINS2 = -24;
constexpr auto ATST_MAXS0 = 40;
constexpr auto ATST_MAXS1 = 40;
constexpr auto ATST_MAXS2 = 248;

//Player viewheights
constexpr auto STANDARD_VIEWHEIGHT_OFFSET = -4;
constexpr auto DEAD_VIEWHEIGHT = -16;
//Player movement values
constexpr auto MIN_WALK_NORMAL = 0.7; // can't walk on very steep slopes;
constexpr auto JUMP_VELOCITY = 225; // 270;
#define	STEPSIZE			18

constexpr auto FATIGUEDTHRESHHOLD = .1;

constexpr auto FATIGUE_JUMP = 1;
constexpr auto FATIGUE_SABERBLOCK = 1;
constexpr auto FATIGUE_BOLTBLOCK = 1;
constexpr auto FATIGUE_MELEE = 2;
constexpr auto FATIGUE_SABERATTACK = 1;
constexpr auto FATIGUE_BLOCKED = 1;
constexpr auto FATIGUE_SABERTRANS = 2;
constexpr auto FATIGUE_BACKBLOCK = 2;
constexpr auto FATIGUE_DUALSABERTRANS = 2;
constexpr auto FATIGUE_BACKFLIP = 3;
constexpr auto FATIGUE_KICKHIT = 10;
constexpr auto FATIGUE_KICKHIT_ABSORB = 5;
#define FATIGUE_CARTWHEEL		-FATIGUE_JUMP + 3
#define FATIGUE_CARTWHEEL_ATARU -FATIGUE_JUMP + 1
constexpr auto FATIGUE_GROUNDATTACK = 3;
#define FATIGUE_JUMPATTACK		-FATIGUE_JUMP + 4
constexpr auto FATIGUE_AUTOSABERDEFENSE = 2;
constexpr auto FATIGUE_SPECIALMOVE = 2;
constexpr auto FATIGUE_AUTOBOLTBLOCK = 7;
constexpr auto FATIGUE_ATTACKFAKE = 2;
constexpr auto FATIGUE_DODGE = 50;
constexpr auto FATIGUE_DODGEING = 35;
constexpr auto FATIGUE_DODGEINGBOT = 70;
constexpr auto FATIGUE_AUTOSABERBOLTBLOCK = 2;
constexpr auto FATIGUE_BLOCKPOINTDRAIN = 5;
constexpr auto FATIGUE_BP_ABSORB = 2;

constexpr auto FLAG_FATIGUED = 1;
constexpr auto FLAG_ATTACKFEINT = 2; //Unused
constexpr auto FLAG_ATTACKFAKE = 3;
constexpr auto FLAG_SLOWBOUNCE = 4;
constexpr auto FLAG_OLDSLOWBOUNCE = 5;
constexpr auto FLAG_SABERLOCK_ATTACKER = 6;
constexpr auto FLAG_FLAMETHROWER = 7;
constexpr auto FLAG_PARRIED = 8;
constexpr auto FLAG_PREBLOCK = 9;
constexpr auto FLAG_BLOCKED = 10;
constexpr auto FLAG_BLOCKING = 11;
constexpr auto FLAG_BLOCKDRAINED = 12;
constexpr auto FLAG_FROZEN = 13;
constexpr auto FLAG_BLOCKEDBOUNCE = 14; //Unused
constexpr auto FLAG_DASHING = 15;
constexpr auto FLAG_ATTACKFATIGUE = 16; //unused
constexpr auto FLAG_NPCBLOCKING = 17;
constexpr auto FLAG_MBLOCKBOUNCE = 18;
constexpr auto FLAG_PERFECTBLOCK = 19;

constexpr auto DODGE_BOLTBLOCK = 2; //standard DP cost to block a missile bolt;
constexpr auto DODGE_BOWCASTERBLOCK = 5;
constexpr auto DODGE_TUSKENBLOCK = 4;
constexpr auto DODGE_REPEATERBLOCK = 2;
//the cost of blocking repeater shots is lower since the repeater shoots much faster.;

constexpr auto BLASTERMISHAPLEVEL_MAX = 16;
constexpr auto BLASTERMISHAPLEVEL_FIFTEEN = 15;
constexpr auto BLASTERMISHAPLEVEL_FOURTEEN = 14;
constexpr auto BLASTERMISHAPLEVEL_OVERLOAD = 13;
constexpr auto BLASTERMISHAPLEVEL_TWELVE = 12;
constexpr auto BLASTERMISHAPLEVEL_FULL = 11;
constexpr auto BLASTERMISHAPLEVEL_HEAVYER = 10;
constexpr auto BLASTERMISHAPLEVEL_HALF = 9;
constexpr auto BLASTERMISHAPLEVEL_HEAVY = 8;
constexpr auto BLASTERMISHAPLEVEL_MEDIUM = 7;
constexpr auto BLASTERMISHAPLEVEL_LIGHT = 5;
constexpr auto BLASTERMISHAPLEVEL_THREE = 3;
constexpr auto BLASTERMISHAPLEVEL_TWO = 2;
constexpr auto BLASTERMISHAPLEVEL_MIN = 1;
constexpr auto BLASTERMISHAPLEVEL_NONE = 0;

constexpr auto MISHAPLEVEL_OVERLOAD = 16;
constexpr auto MISHAPLEVEL_MAX = 15;
constexpr auto MISHAPLEVEL_FULL = 14;
constexpr auto MISHAPLEVEL_THIRTEEN = 13;
constexpr auto MISHAPLEVEL_HUDFLASH = 12;
constexpr auto MISHAPLEVEL_ELEVEN = 11;
constexpr auto MISHAPLEVEL_TEN = 10;
constexpr auto MISHAPLEVEL_NINE = 9;
constexpr auto MISHAPLEVEL_HEAVY = 8;
constexpr auto MISHAPLEVEL_SEVEN = 7;
constexpr auto MISHAPLEVEL_SIX = 6;
constexpr auto MISHAPLEVEL_LIGHT = 5;
constexpr auto MISHAPLEVEL_FOUR = 4;
constexpr auto MISHAPLEVEL_SNIPER = 3;
constexpr auto MISHAPLEVEL_TWO = 2;
constexpr auto MISHAPLEVEL_MIN = 1;
constexpr auto MISHAPLEVEL_NONE = 0;

constexpr auto BLOCK_POINTS_MAX = 100;
constexpr auto BLOCKPOINTS_FULL = 90;
constexpr auto BLOCKPOINTS_MISSILE = 75;
constexpr auto BLOCKPOINTS_KNOCKAWAY = 65;
constexpr auto BLOCKPOINTS_HALF = 50;
constexpr auto BLOCKPOINTS_FOURTY = 40;
constexpr auto BLOCKPOINTS_THIRTY = 30;
constexpr auto BLOCKPOINTS_TWENTYFIVE = 25;
constexpr auto BLOCKPOINTS_FATIGUE = 20;
constexpr auto BLOCKPOINTS_FIFTEEN = 15;
constexpr auto BLOCKPOINTS_TWELVE = 12;
constexpr auto BLOCKPOINTS_TEN = 10;
constexpr auto BLOCKPOINTS_WARNING = 7;
constexpr auto BLOCKPOINTS_FIVE = 5;
constexpr auto BLOCKPOINTS_DANGER = 4;
constexpr auto BLOCKPOINTS_THREE = 3;
constexpr auto BLOCKPOINTS_FAIL = 2;
constexpr auto BLOCK_POINTS_MIN = 1;

constexpr auto PLAYER_FUEL_MAX = 100;

constexpr auto DEFAULT_BLOCK_TIME_MAX_MILLISECONDS = 300;

#define BRYAR_PISTOL_ALT_DPDAMAGE			DODGE_BOLTBLOCK			//minimum DP damage of bryar secondary
#define BRYAR_PISTOL_ALT_DPMAXDAMAGE		DODGE_BOLTBLOCK*1.5		//maximum DP damage of bryar secondary
constexpr auto BRYAR_MAX_CHARGE = 5;

constexpr auto FATIGUE_KILLBONUS = 30; //the bonus you get for killing another player;
constexpr auto FATIGUE_NPCDAMAGEBONUS = 30; //the bonus you get for slapping another player;
constexpr auto FATIGUE_DAMAGEBONUS = 20; //the bonus you get for slapping another player;
constexpr auto FATIGUE_HURTBONUSMAX = 15; //the bonus you get for hurting another player alot;
constexpr auto FATIGUE_HURTBONUS = 10; //the bonus you get for hurting another player;
constexpr auto FATIGUE_HURTBONUSMIN = 5; //the bonus you get for hurting another player a small amount;

/*
===================================================================================

PMOVE MODULE

The pmove code takes a player_state_t and a usercmd_t and generates a new player_state_t
and some other output data.  Used for local prediction on the client game and true
movement on the server game.
===================================================================================
*/

using pmtype_t = enum
{
	PM_NORMAL,
	// can accelerate and turn
	PM_NOCLIP,
	// noclip movement
	PM_SPECTATOR,
	// still run into walls
	PM_DEAD,
	// no acceleration or turning, but free falling
	PM_FREEZE,
	// stuck in place with no control
	PM_INTERMISSION // no movement or status bar
};

using weaponstate_t = enum
{
	WEAPON_READY,
	WEAPON_RAISING,
	WEAPON_DROPPING,
	WEAPON_FIRING,
	WEAPON_CHARGING,
	WEAPON_CHARGING_ALT,
	WEAPON_IDLE,
	//lowered
	WEAPON_RELOADING,
	// added for reloading weapon
};

// pmove->pm_flags
constexpr auto PMF_DUCKED = 1 << 0; //1;
constexpr auto PMF_JUMP_HELD = 1 << 1; //2;
constexpr auto PMF_JUMPING = 1 << 2;
//4		// yes, I really am in a jump -- Mike, you may want to come up with something better here since this is really a temp fix.;
constexpr auto PMF_BACKWARDS_JUMP = 1 << 3; //8		// go into backwards land;
constexpr auto PMF_BACKWARDS_RUN = 1 << 4; //16		// coast down to backwards run;
constexpr auto PMF_TIME_LAND = 1 << 5; //32		// pm_time is time before rejump;
constexpr auto PMF_TIME_KNOCKBACK = 1 << 6; //64		// pm_time is an air-accelerate only time;
constexpr auto PMF_TIME_NOFRICTION = 1 << 7; //128		// pm_time is a no-friction time;
constexpr auto PMF_TIME_WATERJUMP = 1 << 8; //256		// pm_time is waterjump;
constexpr auto PMF_RESPAWNED = 1 << 9; //512		// clear after attack and jump buttons come up;
constexpr auto PMF_USEFORCE_HELD = 1 << 10; //1024	// for debouncing the button;
constexpr auto PMF_JUMP_DUCKED = 1 << 11; //2048	// viewheight changes in mid-air;
constexpr auto PMF_TRIGGER_PUSHED = 1 << 12;
//4096	// pushed by a trigger_push or other such thing - cannot force jump and will not take impact damage;
constexpr auto PMF_STUCK_TO_WALL = 1 << 13; //8192	// grabbing a wall;
constexpr auto PMF_SLOW_MO_FALL = 1 << 14; //16384	// Fall slower until hit ground;
constexpr auto PMF_ATTACK_HELD = 1 << 15; //32768	// Holding down the attack button;
constexpr auto PMF_ALT_ATTACK_HELD = 1 << 16; //65536	// Holding down the alt-attack button;
constexpr auto PMF_BUMPED = 1 << 17; //131072	// Bumped into something;
constexpr auto PMF_FORCE_FOCUS_HELD = 1 << 18; //262144	// Holding down the saberthrow/kick button;
constexpr auto PMF_FIX_MINS = 1 << 19; //524288	// Mins raised for dual forward jump, fix them;
constexpr auto PMF_USE_HELD = 1 << 20; //32768	// Holding down the attack button;
constexpr auto PMF_BLOCK_HELD = 1 << 21; //32768	// Holding down the attack button;
constexpr auto PMF_GRAPPLE_PULL = 1 << 22;
constexpr auto PMF_KICK_HELD = 1 << 23;
constexpr auto PMF_DASH_HELD = 1 << 24; // Holding down the DASH button;
constexpr auto PMF_ACCURATE_MISSILE_BLOCK_HELD = 1 << 24;

#define	PMF_ALL_TIMES	(PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_KNOCKBACK|PMF_TIME_NOFRICTION)

constexpr auto MAXTOUCH = 32;

using gentity_t = struct gentity_s;
using pmove_t = struct
{
	// state (in / out)
	playerState_t* ps;

	// command (in)
	usercmd_t cmd;
	int tracemask; // collide against these types of surfaces
	int debugLevel; // if set, diagnostic output will be printed
	qboolean noFootsteps; // if the game is setup for no footsteps by the server

	// results (out)
	int numtouch;
	int touchents[MAXTOUCH];

	int useEvent;

	vec3_t mins, maxs; // bounding box size

	int watertype;
	int waterlevel;

	float xyspeed;
	gentity_s* gent; // Pointer to entity in g_entities[]

	// callbacks to test the world
	// these will be different functions during game and cgame
	void (*trace)(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
		int passEntityNum, int content_mask, EG2_Collision eG2TraceType, int useLod);
	int (*pointcontents)(const vec3_t point, int passEntityNum);
};

// if a full pmove isn't done on the client, you can just update the angles
void PM_UpdateViewAngles(int saberAnimLevel, playerState_t* ps, usercmd_t* cmd, gentity_t* gent);
void Pmove(pmove_t* pmove);

constexpr auto SETANIM_TORSO = 1;
constexpr auto SETANIM_LEGS = 2;
constexpr auto SETANIM_BOTH = SETANIM_TORSO | SETANIM_LEGS; //3

constexpr auto SETANIM_FLAG_NORMAL = 0; //Only set if timer is 0;
constexpr auto SETANIM_FLAG_OVERRIDE = 1; //Override previous;
constexpr auto SETANIM_FLAG_HOLD = 2; //Set the new timer;
constexpr auto SETANIM_FLAG_RESTART = 4; //Allow restarting the anim if playing the same one (weapon fires);
constexpr auto SETANIM_FLAG_HOLDLESS = 8; //Set the new timer;
constexpr auto SETANIM_FLAG_PACE = 16;
//Switch to this animation and keep repeating this animation while updating its timers
constexpr auto SETANIM_AFLAG_BLOCKPACE = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART;
constexpr auto SETANIM_AFLAG_PACE = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD;
constexpr auto AFLAG_LEDGE = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS | SETANIM_FLAG_PACE;
constexpr auto SETANIM_BLEND_DEFAULT = 100;

void PM_SetAnim(const pmove_t* pm, int set_anim_parts, int anim, int set_anim_flags,
	int blend_time = SETANIM_BLEND_DEFAULT);
void PM_SetAnimFinal(int* torso_anim, int* legs_anim, int set_anim_parts, int anim, int set_anim_flags, int* torso_anim_timer,
	int* legs_anim_timer, gentity_t* gent, int blend_time = SETANIM_BLEND_DEFAULT);

//===================================================================================

// player_state->persistant[] indexes
// these fields are the only part of player_state that isn't
// cleared on respawn
//
//  NOTE!!! Even though this is an enum, the array that contains these uses #define MAX_PERSISTANT 16 in q_shared.h,
//		so be careful how many you add since it'll just overflow without telling you -slc
//
using persEnum_t = enum
{
	PERS_SCORE,
	// !!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!
	PERS_HITS,
	// total points damage inflicted so damage beeps can sound on change
	PERS_TEAM,
	PERS_SPAWN_COUNT,
	// incremented every respawn
	//	PERS_REWARD_COUNT,				// incremented for each reward sound
	PERS_ATTACKER,
	// clientNum of last damage inflicter
	PERS_KILLED,
	// count of the number of times you died

	PERS_ACCURACY_SHOTS,
	// scoreboard - number of player shots
	PERS_ACCURACY_HITS,
	// scoreboard - number of player shots that hit an enemy
	PERS_ENEMIES_KILLED,
	// scoreboard - number of enemies player killed
	PERS_TEAMMATES_KILLED // scoreboard - number of teammates killed
};

// entityState_t->eFlags
constexpr auto EF_HELD_BY_SAND_CREATURE = 0x00000001; // In a sand creature's mouth;
constexpr auto EF_HELD_BY_RANCOR = 0x00000002; // Being held by Rancor;
constexpr auto EF_TELEPORT_BIT = 0x00000004; // toggled every time the origin abruptly changes;
constexpr auto EF_SHADER_ANIM = 0x00000008; // Animating shader (by s.frame);
constexpr auto EF_BOUNCE = 0x00000010; // for missiles;
constexpr auto EF_BOUNCE_HALF = 0x00000020; // for missiles;
constexpr auto EF_MISSILE_STICK = 0x00000040; // missiles that stick to the wall.;
constexpr auto EF_NODRAW = 0x00000080; // may have an event, but no model (unspawned items);
constexpr auto EF_FIRING = 0x00000100; // for lightning gun;
constexpr auto EF_ALT_FIRING = 0x00000200; // for alt-fires, mostly for lightning guns though;
constexpr auto EF_VEH_BOARDING = 0x00000400; // Whether a vehicle is being boarded or not.;
constexpr auto EF_AUTO_SIZE = 0x00000800; // CG_Ents will create the mins & max itself based on model bounds;
constexpr auto EF_BOUNCE_SHRAPNEL = 0x00001000; // special shrapnel flag;
constexpr auto EF_USE_ANGLEDELTA = 0x00002000; // Not used.;
constexpr auto EF_ANIM_ALLFAST = 0x00004000; // automatically cycle through all frames at 10hz;
constexpr auto EF_ANIM_ONCE = 0x00008000; // cycle through all frames just once then stop;
constexpr auto EF_HELD_BY_WAMPA = 0x00010000; // being held by the Wampa;
constexpr auto EF_PROX_TRIP = 0x00020000; // Proximity trip mine has been activated;
constexpr auto EF_LOCKED_TO_WEAPON = 0x00040000;
// When we use an emplaced weapon, we turn this on to lock us to that weapon;

//rest not sent over net?

constexpr auto EF_PERMANENT = 0x00080000;
// this entity is permanent and is never updated (sent only in the game state);
constexpr auto EF_SPOTLIGHT = 0x00100000; // Your lights are on...;
constexpr auto EF_PLANTED_CHARGE = 0x00200000; // For detpack charge;
constexpr auto EF_POWERING_ROSH = 0x00400000; // Only for Twins powering up Rosh;
constexpr auto EF_FORCE_VISIBLE = 0x00800000; // Always visible with force sight;
constexpr auto EF_IN_ATST = 0x01000000; // Driving an ATST;
constexpr auto EF_DISINTEGRATION = 0x02000000; // Disruptor effect;
constexpr auto EF_LESS_ATTEN = 0x04000000; // Use less sound attenuation (louder even when farther).;
constexpr auto EF_JETPACK_ACTIVE = 0x08000000; // Not used;
constexpr auto EF_DISABLE_SHADER_ANIM = 0x10000000;
// Normally shader animation chugs along, but movers can force shader animation to be on frame 1;
constexpr auto EF_FORCE_GRIPPED = 0x20000000; // Force gripped effect;
constexpr auto EF_FORCE_DRAINED = 0x40000000; // Force drained effect;
constexpr auto EF_BLOCKED_MOVER = 0x80000000; // for movers that are blocked - shared with previous;
constexpr auto EF_WALK = 0x100000000;
constexpr auto EF_FORCE_GRASPED = 0x200000000;

constexpr auto EF_MEDITATING = 0x00000008; // draw an excellent sprite;
constexpr auto EF_AWARD_GAUNTLET = 0x00000040; // draw a gauntlet sprite;
constexpr auto EF_AWARD_CAP = 0x00000800; // draw the capture sprite;
constexpr auto EF_AWARD_IMPRESSIVE = 0x00008000; // draw an impressive sprite;
constexpr auto EF_AWARD_DEFEND = 0x00010000; // draw a defend sprite;
constexpr auto EF_AWARD_ASSIST = 0x00020000; // draw a assist sprite;
constexpr auto EF_AWARD_DENIED = 0x00040000; // denied;

constexpr auto EF2_RADAROBJECT = 1 << 0; // Being held by something, like a Rancor or a Wampa;
constexpr auto EF2_USE_ALT_ANIM = 1 << 1;
// For certain special runs/stands for creatures like the Rancor and Wampa whose runs/stands are conditional;
constexpr auto EF2_ALERTED = 1 << 2;
// For certain special anims, for Rancor: means you've had an enemy, so use the more alert stand;
constexpr auto EF2_GENERIC_NPC_FLAG = 1 << 3; // So far, used for Rancor...;
constexpr auto EF2_FLYING = 1 << 4;
// Flying FIXME: only used on NPCs doesn't *really* have to be passed over, does it?;
constexpr auto EF2_HYPERSPACE = 1 << 5;
// Used to both start the hyperspace effect on the predicted client and to let the vehicle know it can now jump into hyperspace (after turning to face the proper angle);
constexpr auto EF2_BRACKET_ENTITY = 1 << 6; // Draw as bracketed;
constexpr auto EF2_SHIP_DEATH = 1 << 7; // "died in ship" mode;
constexpr auto EF2_NOT_USED_1 = 1 << 8; // not used;
constexpr auto EF2_BOWCASTERSCOPE = 1 << 9; //[BowcasterScope];
constexpr auto EF2_PLAYERHIT = 1 << 10; //[SPShield];
constexpr auto EF2_JANGO_DUALS = 1 << 11;
constexpr auto EF2_DUAL_PISTOLS = 1 << 12;
constexpr auto EF2_DUAL_CLONE_PISTOLS = 1 << 13;
constexpr auto EF2_DUAL_CALONORD = 1 << 14;

using powerup_t = enum
{
	PW_DISINT_2,
	PW_MEDITATE,
	PW_BATTLESUIT,
	PW_STUNNED,
	PW_CLOAKED,
	PW_UNCLOAKING,
	PW_DISRUPTION,
	PW_GALAK_SHIELD,
	PW_SEEKER,
	PW_SHOCKED,
	//electricity effect
	PW_DRAINED,
	//drain effect
	PW_FORCE_REPULSE,
	//ghost
	PW_INVINCIBLE,
	PW_FORCE_PUSH,
	PW_FORCE_PUSH_RHAND,
	PW_FORCE_PROJECTILE,

	PW_NUM_POWERUPS
};

#define PW_REMOVE_AT_DEATH ((1<<PW_MEDITATE)|(1<<PW_BATTLESUIT)|(1<<PW_CLOAKED)|(1<<PW_UNCLOAKING)|(1<<PW_UNCLOAKING)|(1<<PW_GALAK_SHIELD)|(1<<PW_DISINT_2)|(1<<PW_INVINCIBLE)|(1<<PW_SEEKER))
// entityState_t->event values
// entity events are for effects that take place relative
// to an existing entities origin.  Very network efficient.

// two bits at the top of the entityState->event field
// will be incremented with each change in the event so
// that an identical event started twice in a row can
// be distinguished.  And off the value with ~EV_EVENT_BITS
// to retrieve the actual event number
constexpr auto EV_EVENT_BIT1 = 0x00000100;
constexpr auto EV_EVENT_BIT2 = 0x00000200;
#define	EV_EVENT_BITS		(EV_EVENT_BIT1|EV_EVENT_BIT2)

using entity_event_t = enum
{
	EV_NONE,

	EV_FOOTSTEP,
	EV_FOOTSTEP_METAL,
	EV_FOOTSPLASH,
	EV_FOOTWADE,
	EV_SWIM,

	EV_STEP_4,
	EV_STEP_8,
	EV_STEP_12,
	EV_STEP_16,

	EV_FALL_SHORT,
	EV_FALL_MEDIUM,
	EV_FALL_FAR,

	EV_JUMP,
	EV_ROLL,
	EV_WATER_TOUCH,
	// foot touches
	EV_WATER_LEAVE,
	// foot leaves
	EV_WATER_UNDER,
	// head touches
	EV_WATER_CLEAR,
	// head leaves
	EV_WATER_GURP1,
	// need air 1
	EV_WATER_GURP2,
	// need air 2
	EV_WATER_DROWN,
	// drowned
	EV_LAVA_TOUCH,
	// foot touches
	EV_LAVA_LEAVE,
	// foot leaves
	EV_LAVA_UNDER,
	// head touches

	EV_ITEM_PICKUP,

	EV_NOAMMO,
	EV_CHANGE_WEAPON,
	EV_FIRE_WEAPON,
	EV_ALT_FIRE,
	EV_POWERUP_SEEKER_FIRE,
	EV_POWERUP_BATTLESUIT,
	EV_USE,

	EV_REPLICATOR,

	EV_BATTERIES_CHARGED,

	EV_GRENADE_BOUNCE,
	// eventParm will be the soundindex
	EV_MISSILE_STICK,
	// eventParm will be the soundindex

	EV_BMODEL_SOUND,
	EV_GENERAL_SOUND,
	EV_GLOBAL_SOUND,
	// no attenuation

	EV_PLAY_EFFECT,
	EV_PLAY_MUZZLE_EFFECT,
	EV_STOP_EFFECT,

	EV_TARGET_BEAM_DRAW,

	EV_DISRUPTOR_MAIN_SHOT,
	EV_DISRUPTOR_SNIPER_SHOT,
	EV_DISRUPTOR_SNIPER_MISS,

	EV_DEMP2_ALT_IMPACT,
	//NEW for JKA weapons:
	EV_CONC_ALT_SHOT,
	EV_CONC_ALT_MISS,
	//END JKA weapons
	EV_PAIN,
	EV_DEATH1,
	EV_DEATH2,
	EV_DEATH3,

	EV_MISSILE_HIT,
	EV_MISSILE_MISS,

	EV_DISINTEGRATION,

	EV_ANGER1,
	//Say when acquire an enemy when didn't have one before
	EV_ANGER2,
	EV_ANGER3,

	EV_VICTORY1,
	//Say when killed an enemy
	EV_VICTORY2,
	EV_VICTORY3,

	EV_CONFUSE1,
	//Say when confused
	EV_CONFUSE2,
	EV_CONFUSE3,

	EV_PUSHED1,
	//Say when pushed
	EV_PUSHED2,
	EV_PUSHED3,

	EV_CHOKE1,
	//Say when choking
	EV_CHOKE2,
	EV_CHOKE3,

	EV_FFWARN,
	//ffire founds
	EV_FFTURN,
	//extra sounds for ST
	EV_CHASE1,
	EV_CHASE2,
	EV_CHASE3,
	EV_COVER1,
	EV_COVER2,
	EV_COVER3,
	EV_COVER4,
	EV_COVER5,
	EV_DETECTED1,
	EV_DETECTED2,
	EV_DETECTED3,
	EV_DETECTED4,
	EV_DETECTED5,
	EV_LOST1,
	EV_OUTFLANK1,
	EV_OUTFLANK2,
	EV_ESCAPING1,
	EV_ESCAPING2,
	EV_ESCAPING3,
	EV_GIVEUP1,
	EV_GIVEUP2,
	EV_GIVEUP3,
	EV_GIVEUP4,
	EV_LOOK1,
	EV_LOOK2,
	EV_SIGHT1,
	EV_SIGHT2,
	EV_SIGHT3,
	EV_SOUND1,
	EV_SOUND2,
	EV_SOUND3,
	EV_SUSPICIOUS1,
	EV_SUSPICIOUS2,
	EV_SUSPICIOUS3,
	EV_SUSPICIOUS4,
	EV_SUSPICIOUS5,
	//extra sounds for Jedi
	EV_COMBAT1,
	EV_COMBAT2,
	EV_COMBAT3,
	EV_JDETECTED1,
	EV_JDETECTED2,
	EV_JDETECTED3,
	EV_TAUNT1,
	EV_TAUNT2,
	EV_TAUNT3,
	EV_TAUNT4,
	EV_TAUNT5,
	EV_JCHASE1,
	EV_JCHASE2,
	EV_JCHASE3,
	EV_JLOST1,
	EV_JLOST2,
	EV_JLOST3,
	EV_DEFLECT1,
	EV_DEFLECT2,
	EV_DEFLECT3,
	EV_GLOAT1,
	EV_GLOAT2,
	EV_GLOAT3,
	EV_PUSHFAIL,

	EV_USE_ITEM,

	EV_USE_INV_BINOCULARS,
	EV_USE_INV_BACTA,
	EV_USE_INV_SEEKER,
	EV_USE_INV_LIGHTAMP_GOGGLES,
	EV_USE_INV_SENTRY,
	EV_USE_INV_CLOAK,
	EV_USE_INV_BARRIER,
	EV_USE_INV_GRAPPLEHOOK,

	EV_USE_FORCE,

	EV_DRUGGED,
	// hit by an interrogator

	EV_DEBUG_LINE,
	EV_KOTHOS_BEAM,

	EV_STUNNED,

	EV_SHIELD_HIT,

	EV_GIB_PLAYER_HEADSHOT,

	EV_BODY_NOHEAD,

	EV_GIB_PLAYER,

	EV_LIGHTNING_STRIKE,

	EV_JEDISPAWN,
};

class animation_t
{
public:
	unsigned short firstFrame;
	unsigned short numFrames;
	short frameLerp; // msec between frames
	//initial lerp is abs(frameLerp)
	signed char loopFrames; // 0 to numFrames, -1 = no loop
	unsigned char glaIndex;

	void sg_export(
		ojk::SavedGameHelper& saved_game) const
	{
		saved_game.write<uint16_t>(firstFrame);
		saved_game.write<uint16_t>(numFrames);
		saved_game.write<int16_t>(frameLerp);
		saved_game.write<int8_t>(loopFrames);
		saved_game.write<uint8_t>(glaIndex);
	}

	void sg_import(
		ojk::SavedGameHelper& saved_game)
	{
		saved_game.read<uint16_t>(firstFrame);
		saved_game.read<uint16_t>(numFrames);
		saved_game.read<int16_t>(frameLerp);
		saved_game.read<int8_t>(loopFrames);
		saved_game.read<uint8_t>(glaIndex);
	}
}; // animation_t

#define MAX_ANIM_FILES	32
constexpr auto MAX_ANIM_EVENTS = 600;

//size of Anim eventData array...
constexpr auto MAX_RANDOM_ANIM_SOUNDS = 8;
#define	AED_ARRAY_SIZE				(MAX_RANDOM_ANIM_SOUNDS+3)
//indices for AEV_SOUND data
constexpr auto AED_SOUNDINDEX_START = 0;
#define	AED_SOUNDINDEX_END			(MAX_RANDOM_ANIM_SOUNDS-1)
#define	AED_SOUND_NUMRANDOMSNDS		(MAX_RANDOM_ANIM_SOUNDS)
#define	AED_SOUND_PROBABILITY		(MAX_RANDOM_ANIM_SOUNDS+1)
//indices for AEV_SOUNDCHAN data
#define	AED_SOUNDCHANNEL			(MAX_RANDOM_ANIM_SOUNDS+2)
//indices for AEV_FOOTSTEP data
constexpr auto AED_FOOTSTEP_TYPE = 0;
constexpr auto AED_FOOTSTEP_PROBABILITY = 1;
//indices for AEV_EFFECT data
constexpr auto AED_EFFECTINDEX = 0;
constexpr auto AED_BOLTINDEX = 1;
constexpr auto AED_EFFECT_PROBABILITY = 2;
constexpr auto AED_MODELINDEX = 3;
//indices for AEV_FIRE data
constexpr auto AED_FIRE_ALT = 0;
constexpr auto AED_FIRE_PROBABILITY = 1;
//indices for AEV_MOVE data
constexpr auto AED_MOVE_FWD = 0;
constexpr auto AED_MOVE_RT = 1;
constexpr auto AED_MOVE_UP = 2;
//indices for AEV_SABER_SWING data
constexpr auto AED_SABER_SWING_saber_num = 0;
constexpr auto AED_SABER_SWING_TYPE = 1;
constexpr auto AED_SABER_SWING_PROBABILITY = 2;
//indices for AEV_SABER_SPIN data
constexpr auto AED_SABER_SPIN_saber_num = 0;
constexpr auto AED_SABER_SPIN_TYPE = 1; //0 = saberspinoff, 1 = saberspin, 2-4 = saberspin1-saberspin3;
constexpr auto AED_SABER_SPIN_PROBABILITY = 2;

using animEventType_t = enum
{
	//NOTENOTE:  Be sure to update animEventTypeTable and ParseAnimationEvtBlock(...) if you change this enum list!
	AEV_NONE,
	AEV_SOUND,
	//# animID AEV_SOUND framenum soundpath randomlow randomhi chancetoplay
	AEV_FOOTSTEP,
	//# animID AEV_FOOTSTEP framenum footstepType chancetoplay
	AEV_EFFECT,
	//# animID AEV_EFFECT framenum effectpath boltName chancetoplay
	AEV_FIRE,
	//# animID AEV_FIRE framenum altfire chancetofire
	AEV_MOVE,
	//# animID AEV_MOVE framenum forwardpush rightpush uppush
	AEV_SOUNDCHAN,
	//# animID AEV_SOUNDCHAN framenum CHANNEL soundpath randomlow randomhi chancetoplay
	AEV_SABER_SWING,
	//# animID AEV_SABER_SWING framenum CHANNEL randomlow randomhi chancetoplay
	AEV_SABER_SPIN,
	//# animID AEV_SABER_SPIN framenum CHANNEL chancetoplay
	AEV_NUM_AEV
};

using animevent_t = struct animevent_s
{
	animEventType_t eventType;
	signed short modelOnly; //event is specific to a modelname to skeleton
	unsigned short glaIndex;
	unsigned short keyFrame; //Frame to play event on
	signed short eventData[AED_ARRAY_SIZE];
	//Unique IDs, can be soundIndex of sound file to play OR effect index or footstep type, etc.
	char* stringData;
	//we allow storage of one string, temporarily (in case we have to look up an index later, then make sure to set stringData to NULL so we only do the look-up once)

	void sg_export(
		ojk::SavedGameHelper& saved_game) const
	{
		saved_game.write<int32_t>(eventType);
		saved_game.write<int16_t>(modelOnly);
		saved_game.write<uint16_t>(glaIndex);
		saved_game.write<uint16_t>(keyFrame);
		saved_game.write<int16_t>(eventData);
		saved_game.write<int32_t>(stringData);
	}

	void sg_import(
		ojk::SavedGameHelper& saved_game)
	{
		saved_game.read<int32_t>(eventType);
		saved_game.read<int16_t>(modelOnly);
		saved_game.read<uint16_t>(glaIndex);
		saved_game.read<uint16_t>(keyFrame);
		saved_game.read<int16_t>(eventData);
		saved_game.read<int32_t>(stringData);
	}
};

using footstepType_t = enum
{
	FOOTSTEP_R,
	FOOTSTEP_L,
	FOOTSTEP_HEAVY_R,
	FOOTSTEP_HEAVY_L,
	NUM_FOOTSTEP_TYPES
};

// means of death
using meansOfDeath_t = enum
{
	MOD_UNKNOWN,

	// weapons
	MOD_SABER,
	MOD_BRYAR,
	MOD_BRYAR_ALT,
	MOD_BLASTER,
	MOD_BLASTER_ALT,
	MOD_CLONERIFLE,
	MOD_CLONERIFLE_ALT,
	MOD_REBELBLASTER,
	MOD_REBELBLASTER_ALT,
	MOD_CLONECOMMANDO,
	MOD_CLONECOMMANDO_ALT,
	MOD_Z6_ROTARY_CANNON,
	MOD_REBELRIFLE,
	MOD_REBELRIFLE_ALT,
	MOD_REY,
	MOD_REY_ALT,
	MOD_CLONEPISTOL,
	MOD_CLONEPISTOL_ALT,
	MOD_JANGO,
	MOD_JANGO_ALT,
	MOD_BOBA,
	MOD_BOBA_ALT,
	MOD_DISRUPTOR,
	MOD_SNIPER,
	MOD_BOWCASTER,
	MOD_BOWCASTER_ALT,
	MOD_REPEATER,
	MOD_REPEATER_ALT,
	MOD_DEMP2,
	MOD_DEMP2_ALT,
	MOD_FLECHETTE,
	MOD_FLECHETTE_ALT,
	MOD_ROCKET,
	MOD_ROCKET_ALT,
	//NEW for JKA weapons:
	MOD_CONC,
	MOD_CONC_ALT,
	//END JKA weapons.
	MOD_THERMAL,
	MOD_THERMAL_ALT,
	MOD_DETPACK,
	MOD_LASERTRIP,
	MOD_LASERTRIP_ALT,
	MOD_MELEE,
	MOD_SEEKER,
	MOD_FORCE_GRIP,
	MOD_FORCE_LIGHTNING,
	MOD_FORCE_DRAIN,
	MOD_EMPLACED,

	// world / generic
	MOD_ELECTROCUTE,
	MOD_EXPLOSIVE,
	MOD_EXPLOSIVE_SPLASH,
	MOD_KNOCKOUT,
	MOD_ENERGY,
	MOD_ENERGY_SPLASH,
	MOD_WATER,
	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH,
	MOD_IMPACT,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_TRIGGER_HURT,
	MOD_GAS,

	MOD_DESTRUCTION,

	MOD_HEADSHOT,
	MOD_BODYSHOT,

	MOD_LIGHTNING_STRIKE,

	MOD_PROJECTION_END,

	NUM_MODS,
};

constexpr auto LOCATION_NONE = 0x00000000;

// Height layers
constexpr auto LOCATION_HEAD = 0x00000001; // [F,B,L,R] Top of head;
constexpr auto LOCATION_FACE = 0x00000002; // [F] Face [B,L,R] Head;
constexpr auto LOCATION_SHOULDER = 0x00000004; // [L,R] Shoulder [F] Throat, [B] Neck;
constexpr auto LOCATION_CHEST = 0x00000008; // [F] Chest [B] Back [L,R] Arm;
constexpr auto LOCATION_STOMACH = 0x00000010; // [L,R] Sides [F] Stomach [B] Lower Back;
constexpr auto LOCATION_GROIN = 0x00000020; // [F] Groin [B] Butt [L,R] Hip;
constexpr auto LOCATION_LEG = 0x00000040; // [F,B,L,R] Legs;
constexpr auto LOCATION_FOOT = 0x00000080; // [F,B,L,R] Bottom of Feet;
// Relative direction strike came from
constexpr auto LOCATION_LEFT = 0x00000100;
constexpr auto LOCATION_RIGHT = 0x00000200;
constexpr auto LOCATION_FRONT = 0x00000400;
constexpr auto LOCATION_BACK = 0x00000800;

//---------------------------------------------------------

// gitem_t->type
using itemType_t = enum
{
	IT_BAD,
	IT_WEAPON,
	IT_AMMO,
	IT_ARMOR,
	IT_HEALTH,
	IT_HOLDABLE,
	IT_BATTERY,
	IT_HOLOCRON,
};

using gitem_t = struct gitem_s
{
	const char* classname; // spawning name
	const char* pickup_sound;
	const char* world_model;

	const char* icon;

	int quantity; // for ammo how much, or duration of powerup
	itemType_t giType; // IT_* flags

	int giTag;

	const char* precaches; // string of all models and images this item will use
	const char* sounds; // string of all sounds this item will use
	vec3_t mins; // Bbox
	vec3_t maxs; // Bbox
};

// included in both the game dll and the client
extern gitem_t bg_itemlist[];
extern const int bg_numItems;
extern weaponData_t weaponData[WP_NUM_WEAPONS];
extern ammoData_t ammoData[AMMO_MAX];

//==============================================================================

gitem_t* FindItem(const char* className);
gitem_t* FindItemForWeapon(weapon_t weapon);
gitem_t* FindItemForInventory(int inv);

#define	ITEM_INDEX(x) ((x)-bg_itemlist)

qboolean BG_CanItemBeGrabbed(const entityState_t* ent, const playerState_t* ps);

// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID|CONTENTS_TERRAIN)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY|CONTENTS_TERRAIN)
#define	MASK_NPCSOLID			(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BODY|CONTENTS_TERRAIN)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_TERRAIN)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_OPAQUE|CONTENTS_SLIME|CONTENTS_LAVA)//was CONTENTS_SOLID, not CONTENTS_OPAQUE...?
/*
Ghoul2 Insert Start
*/
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE|CONTENTS_SHOTCLIP|CONTENTS_TERRAIN)
/*
Ghoul2 Insert End
*/

//
// entityState_t->eType
//
using entityType_t = enum
{
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER,
	ET_BEAM,
	ET_PORTAL,
	ET_SPEAKER,
	ET_PUSH_TRIGGER,
	ET_TELEPORT_TRIGGER,
	ET_INVISIBLE,
	ET_THINKER,
	ET_CLOUD,
	// dumb
	ET_TERRAIN,
	ET_GRAPPLE,
	// grapple hooked on wall

	ET_EVENTS // any of the EV_* events can be added freestanding
	// by setting eType to ET_EVENTS + eventNum
	// this avoids having to set eFlags and eventNum
};

void EvaluateTrajectory(const trajectory_t* tr, int at_time, vec3_t result);
void EvaluateTrajectoryDelta(const trajectory_t* tr, int at_time, vec3_t result);

void AddEventToPlayerstate(int new_event, int event_parm, playerState_t* ps);
int CurrentPlayerstateEvent(const playerState_t* ps);

void PlayerStateToEntityState(playerState_t* ps, entityState_t* s);

qboolean BG_PlayerTouchesItem(const playerState_t* ps, const entityState_t* item, int at_time);

constexpr auto HYPERSPACE_TIME = 4000; //For hyperspace triggers;
constexpr auto HYPERSPACE_TELEPORT_FRAC = 0.75f;
constexpr auto HYPERSPACE_SPEED = 10000.0f; //was 30000;
constexpr auto HYPERSPACE_TURN_RATE = 45.0f;

qboolean PM_InLedgeMove(int anim);
qboolean In_LedgeIdle(int anim);
extern cvar_t* g_sentryexplode;

#endif//#ifndef __BG_PUBLIC_H__
