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

#ifndef	__CG_LOCAL_H__
#define	__CG_LOCAL_H__

#include "../qcommon/q_shared.h"

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
#define	GAME_INCLUDE
#include "../game/g_shared.h"
#include "cg_camera.h"

// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

constexpr auto POWERUP_BLINKS = 5;
constexpr auto POWERUP_BLINK_TIME = 1000;
constexpr auto FADE_TIME = 200;
constexpr auto PULSE_TIME = 200;
constexpr auto DAMAGE_DEFLECT_TIME = 100;
constexpr auto DAMAGE_RETURN_TIME = 400;
constexpr auto DAMAGE_TIME = 500;
constexpr auto LAND_DEFLECT_TIME = 150;
constexpr auto LAND_RETURN_TIME = 300;
constexpr auto STEP_TIME = 200;
constexpr auto DUCK_TIME = 100;
constexpr auto PAIN_TWITCH_TIME = 200;
constexpr auto WEAPON_SELECT_TIME = 1400;
constexpr auto ITEM_SCALEUP_TIME = 1000;
// Zoom vars
constexpr auto ZOOM_TIME = 150; // not currently used?;
constexpr auto MAX_ZOOM_FOV = 3.0f;
constexpr auto ZOOM_IN_TIME = 1500.0f;
constexpr auto ZOOM_OUT_TIME = 100.0f;
constexpr auto ZOOM_START_PERCENT = 0.3f;

constexpr auto ITEM_BLOB_TIME = 200;
constexpr auto MUZZLE_FLASH_TIME = 20;
constexpr auto FRAG_FADE_TIME = 1000; // time for fragments to fade away;

constexpr auto PULSE_SCALE = 1.5; // amount to scale up the icons when activating;

constexpr auto MAX_STEP_CHANGE = 32;

constexpr auto MAX_VERTS_ON_POLY = 10;
constexpr auto MAX_MARK_POLYS = 256;

constexpr auto STAT_MINUS = 10; // num frame for '-' stats digit;

constexpr auto ICON_SIZE = 48;
constexpr auto CHAR_WIDTH = 32;
constexpr auto CHAR_HEIGHT = 48;
constexpr auto TEXT_ICON_SPACE = 4;

constexpr auto CHARSMALL_WIDTH = 16;
constexpr auto CHARSMALL_HEIGHT = 32;

// very large characters
constexpr auto GIANT_WIDTH = 32;
constexpr auto GIANT_HEIGHT = 48;

constexpr auto MAX_PRINTTEXT = 128;
constexpr auto MAX_CAPTIONTEXT = 32;
// we don't need 64 now since we don't use this for scroll text, and I needed to change a hardwired 128 to 256, so...;
constexpr auto MAX_LCARSTEXT = 128;

constexpr auto NUM_FONT_BIG = 1;
constexpr auto NUM_FONT_SMALL = 2;
constexpr auto NUM_FONT_CHUNKY = 3;

constexpr auto CS_BASIC = 0;
constexpr auto CS_COMBAT = 1;
constexpr auto CS_EXTRA = 2;
constexpr auto CS_JEDI = 3;
constexpr auto CS_TRY_ALL = 4;

constexpr auto WAVE_AMPLITUDE = 1;
constexpr auto WAVE_FREQUENCY = 0.4;

//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
using lerpFrame_t = struct
{
	int oldFrame;
	int oldFrameTime; // time when ->oldFrame was exactly on

	int frame;
	int frameTime; // time when ->frame will be exactly on

	float backlerp;

	float yawAngle;
	qboolean yawing;
	float pitchAngle;
	qboolean pitching;

	int animationNumber;
	animation_t* animation;
	int animationTime; // time when the first frame of the animation will be exact
};

using playerEntity_t = struct
{
	lerpFrame_t legs, torso;
	int painTime;
	int painDirection; // flip from 0 to 1

	int pushedTime; // when the last push sound occurred

	// For persistent beam weapons, so they don't play their start sound more than once
	qboolean lightningFiring;

	// machinegun spinning
	float barrelAngle;
	int barrelTime;
	bool barrelSpinning;

	qboolean noHead;
};

//=================================================

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
struct centity_s
{
	entityState_t currentState; // from cg.frame
	const entityState_t* nextState; // from cg.nextFrame, if available
	qboolean interpolate; // true if next is valid to interpolate to
	qboolean currentValid; // true if cg.frame holds this entity

	int muzzleFlashTime; // move to playerEntity?
	int muzzleFlashTimeL; // move to playerEntity?
	int muzzleFlashTimeR; // move to playerEntity?
	qboolean alt_fire; // move to playerEntity?

	int previousEvent;
	//	int				teleportFlag;
	Vehicle_t* m_pVehicle; //vehicle data

	int trailTime; // so missile trails can handle dropped initial packets
	int miscTime;

	playerEntity_t pe;

	// exact interpolated position of entity on this frame
	vec3_t lerpOrigin;
	vec3_t lerpAngles;
	vec3_t renderAngles;
	//for ET_PLAYERS, the actual angles it was rendered at- should be used by any getboltmatrix calls after CG_Player

	float rotValue; //rotation increment for repeater effect

	int snapShotTime;

	vec3_t damageAngles;
	int damageTime;

	//Pointer to corresponding gentity
	gentity_t* gent;

	int shieldHitTime;
	int shieldRechargeTime;
	int muzzleOverheatTime;
};

using centity_t = centity_s;

//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independently from all server transmitted entities

using markPoly_t = struct markPoly_s
{
	markPoly_s* prevMark, * nextMark;
	int time;
	qhandle_t markShader;
	qboolean alphaFade; // fade alpha instead of rgb
	float color[4];
	poly_t poly;
	polyVert_t verts[MAX_VERTS_ON_POLY];
};

using leType_t = enum
{
	LE_MARK,
	LE_FADE_MODEL,
	LE_FADE_SCALE_MODEL,
	// currently only for Demp2 shock sphere
	LE_FRAGMENT,
	LE_PUFF,
	LE_FADE_RGB,
	LE_LIGHT,
	LE_LINE,
	LE_QUAD,
	LE_SPRITE,
};

using leFlag_t = enum
{
	LEF_PUFF_DONT_SCALE = 0x0001,
	// do not scale size over time
	LEF_TUMBLE = 0x0002,
	// tumble over time, used for ejecting shells
	LEF_FADE_RGB = 0x0004,
	// explicitly fade
	LEF_NO_RANDOM_ROTATE = 0x0008 // MakeExplosion adds random rotate which could be bad in some cases
};

using leBounceSound_t = enum
{
	LEBS_NONE,
	LEBS_METAL,
	LEBS_ROCK
}; // fragment local entities can make sounds on impacts

using localEntity_t = struct localEntity_s
{
	localEntity_s* prev, * next;
	leType_t leType;
	int leFlags;

	int startTime;
	int endTime;

	float lifeRate; // 1.0 / (endTime - startTime)

	trajectory_t pos;
	trajectory_t angles;

	float bounceFactor; // 0.0 = no bounce, 1.0 = perfect

	float color[4];

	float radius;

	float light;
	vec3_t lightColor;

	leBounceSound_t leBounceSoundType;

	refEntity_t refEntity;
	int ownerGentNum;
};

//======================================================================

// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
using itemInfo_t = struct
{
	qboolean registered;
	qhandle_t models;
	qhandle_t icon;
};

using powerupInfo_t = struct
{
	int itemNum;
};

constexpr auto CG_OVERRIDE_3RD_PERSON_ENT = 0x00000001;
constexpr auto CG_OVERRIDE_3RD_PERSON_RNG = 0x00000002;
constexpr auto CG_OVERRIDE_3RD_PERSON_ANG = 0x00000004;
constexpr auto CG_OVERRIDE_3RD_PERSON_VOF = 0x00000008;
constexpr auto CG_OVERRIDE_3RD_PERSON_POF = 0x00000010;
constexpr auto CG_OVERRIDE_3RD_PERSON_CDP = 0x00000020;
constexpr auto CG_OVERRIDE_3RD_PERSON_APH = 0x00000040;
constexpr auto CG_OVERRIDE_3RD_PERSON_HOF = 0x00000080;
constexpr auto CG_OVERRIDE_FOV = 0x00000100;

using overrides_t = struct
{
	//NOTE: these probably get cleared in save/load!!!
	int active; //bit-flag field of which overrides are active
	int thirdPersonEntity; //who to center on
	float thirdPersonRange; //how far to be from them
	float thirdPersonAngle; //what angle to look at them from
	float thirdPersonVertOffset; //how high to be above them
	float thirdPersonPitchOffset; //what offset pitch to apply the the camera view
	float thirdPersonCameraDamp; //how tightly to move the camera pos behind the player
	float thirdPersonHorzOffset; //NOTE: could put Alpha and HorzOffset and the target & camera damps, but no-one is trying to override those, so... YES WE ARE
	float thirdPersonAlpha; //how tightly to move the camera pos behind the player
	float fov; //what fov to use
};

//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

using cg_t = struct
{
	int clientFrame; // incremented each frame

	qboolean levelShot; // taking a level menu screenshot

	// there are only one or two snapshot_t that are relevent at a time
	int latestSnapshotNum; // the number of snapshots the client system has received
	int latestSnapshotTime; // the time from latestSnapshotNum, so we don't need to read the snapshot yet
	int processedSnapshotNum; // the number of snapshots cgame has requested
	snapshot_t* snap; // cg.snap->serverTime <= cg.time
	snapshot_t* nextSnap; // cg.nextSnap->serverTime > cg.time, or NULL

	float frameInterpolation;
	// (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

	qboolean thisFrameTeleport;
	qboolean nextFrameTeleport;

	int frametime; // cg.time - cg.oldTime

	int time; // this is the time value that the client
	// is rendering at.
	int oldTime; // time at last frame, used for missile trails and prediction checking

	int timelimitWarnings; // 5 min, 1 min, overtime

	qboolean renderingThirdPerson; // during deaths, chasecams, etc

	// prediction state
	qboolean hyperspace; // true if prediction has hit a trigger_teleport
	playerState_t predictedPlayerState;
	playerState_t predictedVehicleState;
	qboolean validPPS; // clear until the first call to CG_PredictPlayerState
	int predictedErrorTime;
	vec3_t predictedError;

	float stepChange; // for stair up smoothing
	int stepTime;

	float duckChange; // for duck viewheight smoothing
	int duckTime;

	float landChange; // for landing hard
	int landTime;

	// input state sent to server
	int weaponSelect;
	int saberAnimLevelPending;

	// auto rotating items
	vec3_t autoAngles;
	vec3_t autoAxis[3];
	vec3_t autoAnglesFast;
	vec3_t autoAxisFast[3];

	// view rendering
	refdef_t refdef;
	vec3_t refdefViewAngles; // will be converted to refdef.viewaxis

	// zoom key
	int zoomMode; // 0 - not zoomed, 1 - binoculars, 2 - disruptor weapon
	int zoomDir; // -1, 1
	int zoomTime;
	qboolean zoomLocked;

	// gonk use
	int batteryChargeTime;

	// FIXME:
	int forceCrosshairStartTime;
	int forceCrosshairEndTime;

	// information screen text during loading
	char infoScreenText[MAX_STRING_CHARS];

	// centerprinting
	int centerPrintTime;
	int centerPrintY;
	char centerPrint[1024];
	int centerPrintLines;

	// Scrolling text, caption text and LCARS text use this
	char printText[MAX_PRINTTEXT][128];
	int printTextY;

	char captionText[MAX_CAPTIONTEXT][256/*128*/]; // bosted for taiwanese squealy radio static speech in kejim post
	int captionTextY;

	int scrollTextLines; // Number of lines being printed
	int scrollTextTime;

	int captionNextTextTime;
	int captionTextCurrentLine;
	int captionTextTime;
	int captionLetterTime;

	// For flashing health armor counter
	int oldhealth;
	int oldHealthTime;
	int oldarmor;
	int oldArmorTime;
	int oldammo;
	int oldAmmoTime;

	// low ammo warning state
	int lowAmmoWarning; // 1 = low, 2 = empty

	// crosshair client ID
	int crosshairclientNum; //who you're looking at
	int crosshairClientTime; //last time you looked at them

	// powerup active flashing
	int powerupActive;
	int powerupTime;

	//==========================
	int creditsStart;

	int itemPickup;
	int itemPickupTime;
	int itemPickupBlendTime; // the pulse around the crosshair is timed seperately

	float iconHUDPercent; // How far into opening sequence the icon HUD is
	int iconSelectTime; // How long the Icon HUD has been active
	qboolean iconHUDActive;

	int DataPadInventorySelect; // Current inventory item chosen on Data Pad
	int DataPadWeaponSelect; // Current weapon item chosen on Data Pad
	int DataPadforcepowerSelect; // Current force power chosen on Data Pad

	qboolean messageLitActive; // Flag to show of message lite is active

	int weaponSelectTime;
	int weaponAnimation;
	int weaponAnimationTime;

	int inventorySelect; // Current inventory item chosen
	int inventorySelectTime;

	int forcepowerSelect; // Current force power chosen
	int forcepowerSelectTime;

	// blend blobs
	float damageTime;
	float damageX, damageY, damageValue;

	// status bar head
	float headYaw;
	float headEndPitch;
	float headEndYaw;
	int headEndTime;
	float headStartPitch;
	float headStartYaw;
	int headStartTime;

	int loadLCARSStage;

	int missionInfoFlashTime;
	qboolean missionStatusShow;
	int missionStatusDeadTime;

	int mishapHUDTotalFlashTime;
	int mishapHUDNextFlashTime;
	qboolean mishapHUDActive;

	int forceHUDTotalFlashTime;
	int forceHUDNextFlashTime;
	qboolean forceHUDActive; // Flag to show force hud is off/on

	int blockHUDTotalFlashTime;
	int blockHUDNextFlashTime;
	qboolean blockHUDActive;

	qboolean missionFailedScreen; // qtrue if opened

	int weaponPickupTextTime;

	int VHUDFlashTime;
	qboolean VHUDTurboFlag;
	int HUDTickFlashTime;
	qboolean HUDArmorFlag;
	qboolean HUDHealthFlag;

	// view movement
	float v_dmg_time;
	float v_dmg_pitch;
	float v_dmg_roll;

	int wonkyTime; // when interrogator gets you, wonky time controls "drugged" camera view.
	int stunnedTime;

	vec3_t kick_angles; // weapon kicks
	int kick_time; // when the kick happened, so it gets reduced over time

	// temp working variables for player view
	float bobfracsin;
	int bobcycle;
	float xyspeed;

	// development tool
	char testModelName[MAX_QPATH];
	/*
	Ghoul2 Insert Start
	*/
	int testModel;
	// had to be moved so we wouldn't wipe these out with the memset - these have STL in them and shouldn't be cleared that way
	snapshot_t activeSnapshots[2];
	refEntity_t testModelEntity;
	/*
	Ghoul2 Insert End
	*/
	overrides_t overrides; //for overriding certain third-person camera properties

	short radarEntityCount;
	short radarEntities[64];

	short bracketedEntityCount;
	short bracketedEntities[64];
};

constexpr auto MAX_SHOWPOWERS = 21;
extern int showPowers[MAX_SHOWPOWERS];
extern const char* showPowersName[MAX_SHOWPOWERS];
extern int force_icons[NUM_FORCE_POWERS];
constexpr auto MAX_DPSHOWPOWERS = 25;

void Weapon_StunFree(gentity_t* ent);
void Weapon_StunThink(gentity_t* ent);

void Weapon_HookThink(gentity_t* ent);
void Weapon_HookFree(gentity_t* ent);

//==============================================================================

constexpr auto SG_OFF = 0;
constexpr auto SG_STRING = 1;
constexpr auto SG_GRAPHIC = 2;
constexpr auto SG_NUMBER = 3;
constexpr auto SG_VAR = 4;

using screengraphics_s = struct
{
	int type; // STRING or GRAPHIC
	float timer; // When it changes
	int x; // X position
	int y; // Y positon
	int width; // Graphic width
	int height; // Graphic height
	char* file; // File name of graphic/ text if STRING
	int ingameEnum; // Index to ingame_text[]
	qhandle_t graphic; // Handle of graphic if GRAPHIC
	int min; //
	int max;
	int target; // Final value
	int inc;
	int style;
	int color; // Normal color
	void* pointer; // To an address
};

extern cg_t cg;
extern centity_t cg_entities[MAX_GENTITIES];

extern centity_t* cg_permanents[MAX_GENTITIES];
extern int cg_numpermanents;

extern weaponInfo_t cg_weapons[MAX_WEAPONS];
extern itemInfo_t cg_items[MAX_ITEMS];
extern markPoly_t cg_markPolys[MAX_MARK_POLYS];

extern vmCvar_t cg_runpitch;
extern vmCvar_t cg_runroll;
extern vmCvar_t cg_bobup;
extern vmCvar_t cg_bobpitch;
extern vmCvar_t cg_bobroll;
extern vmCvar_t cg_shadows;
extern vmCvar_t cg_renderToTextureFX;
extern vmCvar_t cg_shadowCullDistance;
extern vmCvar_t cg_paused;
extern vmCvar_t cg_drawTimer;
extern vmCvar_t cg_drawFPS;
extern vmCvar_t cg_drawSnapshot;
extern vmCvar_t cg_drawAmmoWarning;
extern vmCvar_t cg_drawCrosshair;
extern vmCvar_t cg_dynamicCrosshair;
extern vmCvar_t cg_drawCrosshairNames;
extern vmCvar_t cg_DrawCrosshairItem;
extern vmCvar_t cg_crosshairForceHint;
extern vmCvar_t cg_crosshairIdentifyTarget;
extern vmCvar_t cg_crosshairX;
extern vmCvar_t cg_crosshairY;
extern vmCvar_t cg_crosshairSize;
extern vmCvar_t cg_crosshairDualSize;
extern vmCvar_t cg_weaponcrosshairs;
extern vmCvar_t cg_drawStatus;
extern vmCvar_t cg_drawHUD;
extern vmCvar_t cg_draw2D;
extern vmCvar_t cg_debugAnim;
#ifndef FINAL_BUILD
extern	vmCvar_t		cg_debugAnimTarget;
extern	vmCvar_t		cg_gun_frame;
#endif
extern vmCvar_t cg_gun_x;
extern vmCvar_t cg_gun_y;
extern vmCvar_t cg_gun_z;
extern vmCvar_t cg_debugSaber;
extern vmCvar_t cg_debugEvents;
extern vmCvar_t cg_errorDecay;
extern vmCvar_t cg_footsteps;
extern vmCvar_t cg_addMarks;
extern vmCvar_t cg_drawGun;
extern vmCvar_t cg_autoswitch;
extern vmCvar_t cg_simpleItems;
extern vmCvar_t cg_fov;
extern vmCvar_t cg_fovAspectAdjust;
extern vmCvar_t cg_endcredits;
extern vmCvar_t cg_updatedDataPadForcePower1;
extern vmCvar_t cg_updatedDataPadForcePower2;
extern vmCvar_t cg_updatedDataPadForcePower3;
extern vmCvar_t cg_updatedDataPadObjective;
extern vmCvar_t cg_drawBreath;
extern vmCvar_t cg_roffdebug;
#ifndef FINAL_BUILD
extern	vmCvar_t		cg_roffval1;
extern	vmCvar_t		cg_roffval2;
extern	vmCvar_t		cg_roffval3;
extern	vmCvar_t		cg_roffval4;
#endif
extern vmCvar_t cg_thirdPerson;
extern vmCvar_t cg_thirdPersonRange;
extern vmCvar_t cg_thirdPersonMaxRange;
extern vmCvar_t cg_thirdPersonAngle;
extern vmCvar_t cg_thirdPersonPitchOffset;
extern vmCvar_t cg_thirdPersonHorzOffset;
extern vmCvar_t cg_thirdPersonVertOffset;
extern vmCvar_t cg_thirdPersonCameraDamp;
extern vmCvar_t cg_thirdPersonTargetDamp;
extern vmCvar_t cg_gunAutoFirst;

extern vmCvar_t cg_stereoSeparation;
extern vmCvar_t cg_developer;
extern vmCvar_t cg_timescale;
extern vmCvar_t cg_skippingcin;

extern vmCvar_t cg_pano;
extern vmCvar_t cg_panoNumShots;

extern vmCvar_t fx_freeze;
extern vmCvar_t fx_debug;

extern vmCvar_t cg_missionInfoFlashTime;
extern vmCvar_t cg_hudFiles;

extern vmCvar_t cg_turnAnims;
extern vmCvar_t cg_motionBoneComp;
extern vmCvar_t cg_reliableAnimSounds;

extern vmCvar_t cg_smoothPlayerPos;
extern vmCvar_t cg_smoothPlayerPlat;
extern vmCvar_t cg_smoothPlayerPlatAccel;

extern vmCvar_t cg_smoothCamera;
extern vmCvar_t cg_speedTrail;
extern vmCvar_t cg_fovViewmodel;
extern vmCvar_t cg_fovViewmodelAdjust;

extern vmCvar_t cg_scaleVehicleSensitivity;

extern vmCvar_t cg_drawRadar;

extern vmCvar_t cg_drawSelectionScrollBar;

extern vmCvar_t cg_trueguns;
extern vmCvar_t cg_fpls;

extern vmCvar_t cg_trueroll;
extern vmCvar_t cg_trueflip;
extern vmCvar_t cg_truespin;
extern vmCvar_t cg_truemoveroll;
extern vmCvar_t cg_truesaberonly;
extern vmCvar_t cg_trueeyeposition;
extern vmCvar_t cg_trueinvertsaber;
extern vmCvar_t cg_truefov;
extern vmCvar_t cg_truebobbing;

extern vmCvar_t cg_setVaderBreath;
extern vmCvar_t cg_setVaderBreathdamaged;

extern vmCvar_t cg_hudRatio;
extern vmCvar_t cg_saberLockCinematicCamera;
extern vmCvar_t cg_jumpSounds;
extern vmCvar_t cg_rollSounds;

extern vmCvar_t cg_saberinfo;

extern vmCvar_t cg_com_rend2;

extern vmCvar_t cg_textprintscale;

void CG_NewClientinfo(int clientNum);
//
// cg_main.c
//
const char* CG_ConfigString(int index);
const char* CG_Argv(int arg);

void QDECL CG_Printf(const char* msg, ...);
NORETURN void QDECL CG_Error(const char* msg, ...);

void CG_StartMusic(qboolean bForceStart);

void CG_UpdateCvars();

int CG_CrosshairPlayer();
void CG_LoadMenus(const char* menuFile);

//
// cg_view.c
//
void CG_TestModel_f();
void CG_TestModelNextFrame_f();
void CG_TestModelPrevFrame_f();
void CG_TestModelNextSkin_f();
void CG_TestModelPrevSkin_f();

void CG_DrawActiveFrame(int server_time, stereoFrame_t stereo_view);
/*
Ghoul2 Insert Start
*/

void CG_TestG2Model_f();
void CG_TestModelSurfaceOnOff_f();
void CG_ListModelSurfaces_f();
void CG_ListModelBones_f();
void CG_TestModelSetAnglespre_f();
void CG_TestModelSetAnglespost_f();
void CG_TestModelAnimate_f();
/*
Ghoul2 Insert End
*/

//
// cg_drawtools.c
//

constexpr auto CG_LEFT = 0x00000000; // default;
constexpr auto CG_CENTER = 0x00000001;
constexpr auto CG_RIGHT = 0x00000002;
constexpr auto CG_FORMATMASK = 0x00000007;
constexpr auto CG_SMALLFONT = 0x00000010;
constexpr auto CG_BIGFONT = 0x00000020; // default;

constexpr auto CG_DROPSHADOW = 0x00000800;
constexpr auto CG_BLINK = 0x00001000;
constexpr auto CG_INVERSE = 0x00002000;
constexpr auto CG_PULSE = 0x00004000;

void CG_DrawRect(float x, float y, float width, float height, float size, const float* color);
void CG_FillRect(float x, float y, float width, float height, const float* color);
void CG_Scissor(float x, float y, float width, float height);
void CG_DrawPic(float x, float y, float width, float height, qhandle_t hShader);
void CG_DrawPic2(float x, float y, float width, float height, float s1, float t1, float s2, float t2,
	qhandle_t hShader);
void CG_DrawRotatePic(float x, float y, float width, float height, float angle, qhandle_t hShader,
	float aspect_correction = 1.0f);
void CG_DrawRotatePic2(float x, float y, float width, float height, float angle, qhandle_t hShader,
	float aspect_correction = 1.0f);
void CG_DrawNumField(int x, int y, int width, int value, int char_width, int char_height, int style, qboolean zero_fill);
void CG_DrawProportionalString(int x, int y, const char* str, int style, vec4_t color);

void CG_DrawStringExt(int x, int y, const char* string, const float* set_color, qboolean force_color, qboolean shadow,
	int char_width, int char_height);
void CG_DrawSmallStringColor(int x, int y, const char* s, vec4_t color);

int CG_DrawStrlen(const char* str);

float* CG_FadeColor(int start_msec, int total_msec);
void CG_TileClear();

//
// cg_draw.c
//
void CG_CenterPrint(const char* str, int y);
void CG_DrawActive(stereoFrame_t stereoView);
void CG_ScrollText(const char* str, int iPixelWidth);
void CG_CaptionText(const char* str, int sound);
void CG_CaptionTextStop();

//
// cg_text.c
//
void CG_DrawScrollText();
void CG_DrawCaptionText();
void CG_DrawCenterString();

//
// cg_player.c
//
void CG_AddGhoul2Mark(int type, float size, vec3_t hitloc, vec3_t hitdirection,
	int entnum, vec3_t entposition, float entangle, CGhoul2Info_v& ghoul2, vec3_t model_scale,
	int life_time = 0, int first_model = 0, vec3_t uaxis = nullptr);
void CG_Player(centity_t* cent);
void CG_ResetPlayerEntity(centity_t* cent);
void CG_AddRefEntityWithPowerups(refEntity_t* ent, int powerups, centity_t* cent);
void CG_GetTagWorldPosition(refEntity_t* model, const char* tag, vec3_t pos, vec3_t axis[3]);
void CG_PlayerShieldHit(int entitynum, vec3_t angles, int amount);

//
// cg_predict.c
//
int CG_PointContents(const vec3_t point, int passEntityNum);
void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
	int skip_number, int mask);
void CG_PredictPlayerState();

//
// cg_events.c
//
void CG_CheckEvents(centity_t* cent);
const char* CG_PlaceString(int rank);
void CG_EntityEvent(centity_t* cent, vec3_t position);

//
// cg_ents.c
//
vec3_t* CG_SetEntitySoundPosition(const centity_t* cent);
void CG_AddPacketEntities(qboolean isPortal);
void CG_Beam(const centity_t* cent, int color);
void CG_AdjustPositionForMover(const vec3_t in, int mover_num, int at_time, vec3_t out);

void CG_PositionEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parent_model, const char* tag_name);
void CG_PositionEntityOnTagDual(refEntity_t* entity, const refEntity_t* parent, qhandle_t parent_model,
	const char* tag_name);

void CG_PositionRotatedEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parent_model,
	const char* tag_name,
	orientation_t* tag_orient);

/*
Ghoul2 Insert Start
*/
void ScaleModelAxis(refEntity_t* ent);
/*
Ghoul2 Insert End
*/

//
// cg_weapons.c
//
void CG_NextWeapon_f();
void CG_PrevWeapon_f();
void CG_Weapon_f();
void CG_DPNextWeapon_f();
void CG_DPPrevWeapon_f();
void CG_DPNextInventory_f();
void CG_DPPrevInventory_f();
void CG_DPNextForcePower_f();
void CG_DPPrevForcePower_f();

void CG_RegisterWeapon(int weapon_num);
void CG_RegisterItemVisuals(int item_num);
void CG_RegisterItemSounds(int itemNum);

void CG_FireWeapon(centity_t* cent, qboolean alt_fire);

void CG_AddViewWeapon(playerState_t* ps);
void CG_AddViewWeaponDuals(playerState_t* ps);

void CG_DrawWeaponSelect();
void CG_DrawWeaponSelect_kotor();
void CG_DrawWeaponSelect_text();

void CG_OutOfAmmoChange(); // should this be in pmove?

//
// cg_marks.c
//
void CG_InitMarkPolys();
void CG_AddMarks();
void CG_ImpactMark(qhandle_t mark_shader,
	const vec3_t origin, const vec3_t dir,
	float orientation,
	float r, float g, float b, float a,
	qboolean alpha_fade,
	float radius, qboolean temporary);

//
// cg_localents.c
//
void CG_InitLocalEntities();
localEntity_t* CG_AllocLocalEntity();
void CG_AddLocalEntities();

//
// cg_effects.c
//

/*localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, int numframes, qhandle_t shader, int msec,
								qboolean isSprite, float scale = 1.0f );// Overloaded

localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, int numframes, qhandle_t shader, int msec,
								qboolean isSprite, float scale, int flags );// Overloaded
*/
localEntity_t* CG_AddTempLight(vec3_t origin, float scale, vec3_t color, int msec);

void CG_TestLine(vec3_t start, vec3_t end, int time, unsigned int color, int radius);

void CG_BlockLine(vec3_t start, vec3_t end, int time, unsigned int color, int radius);

void CG_StunStartpoint(vec3_t start_pos);
void CG_GrappleStartpoint(vec3_t start_pos);
void CG_GrappleLine(vec3_t start, vec3_t end, int time, unsigned int color, int radius);

//
// cg_snapshot.c
//
void CG_ProcessSnapshots();

//
// cg_info.c
//
void CG_DrawInformation();

//
// cg_scoreboard.c
//
qboolean CG_DrawScoreboard();
extern void CG_MissionCompletion();

//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand();
void CG_InitConsoleCommands();

//
// cg_servercmds.c
//
void CG_ExecuteNewServerCommands(int latestSequence);
void CG_ParseServerinfo();

//
// cg_playerstate.c
//
void CG_Respawn();
void CG_TransitionPlayerState(playerState_t* ps, playerState_t* ops);

// cg_credits.cpp
//
void CG_Credits_Init(const char* psStripReference, vec4_t* pv4Color);
qboolean CG_Credits_Running();
qboolean CG_Credits_Draw();

//===============================================

//
// system calls
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void cgi_Printf(const char* fmt);

// abort the game
NORETURN void cgi_Error(const char* fmt);

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int cgi_Milliseconds();

// console variable interaction
void cgi_Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags);
void cgi_Cvar_Update(vmCvar_t* vmCvar);
void cgi_Cvar_Set(const char* var_name, const char* value);

// ServerCommand and ConsoleCommand parameter access
int cgi_Argc();
void cgi_Argv(int n, char* buffer, int bufferLength);
void cgi_Args(char* buffer, int bufferLength);

// filesystem access
// returns length of file
int cgi_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode);
int cgi_FS_Read(void* buffer, int len, fileHandle_t f);
int cgi_FS_Write(const void* buffer, int len, fileHandle_t f);
void cgi_FS_FCloseFile(fileHandle_t f);

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void cgi_SendConsoleCommand(const char* text);

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void cgi_AddCommand(const char* cmdName);

// send a string to the server over the network
void cgi_SendClientCommand(const char* s);

// force a screen update, only used during gamestate load
void cgi_UpdateScreen();

//RMG
void cgi_RMG_Init(int terrainID, const char* terrainInfo);
int cgi_CM_RegisterTerrain(const char* terrainInfo);
void cgi_RE_InitRendererTerrain(const char* terrainInfo);

// model collision
void cgi_CM_LoadMap(const char* mapname, qboolean subBSP);
int cgi_CM_NumInlineModels();
clipHandle_t cgi_CM_InlineModel(int index); // 0 = world, 1+ = bmodels
clipHandle_t cgi_CM_TempBoxModel(const vec3_t mins, const vec3_t maxs); //, const int contents );
int cgi_CM_PointContents(const vec3_t p, clipHandle_t model);
int cgi_CM_TransformedPointContents(const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles);
void cgi_CM_BoxTrace(trace_t* results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs,
	clipHandle_t model, int brushmask);
void cgi_CM_TransformedBoxTrace(trace_t* results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs,
	clipHandle_t model, int brushmask,
	const vec3_t origin, const vec3_t angles);

// Returns the projection of a polygon onto the solid brushes in the world
int cgi_CM_MarkFragments(int numPoints, const vec3_t* points,
	const vec3_t projection,
	int max_points, vec3_t point_buffer,
	int max_fragments, markFragment_t* fragment_buffer);

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void cgi_S_StartSound(const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
void cgi_S_StopSounds();

// a local sound is always played full volume
void cgi_S_StartLocalSound(sfxHandle_t sfx, int channelNum);
void cgi_S_ClearLoopingSounds();
void cgi_S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx,
	soundChannel_t chan = CHAN_AUTO);
void cgi_S_UpdateEntityPosition(int entityNum, const vec3_t origin);

// repatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void cgi_S_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], qboolean inwater);
sfxHandle_t cgi_S_RegisterSound(const char* sample); // returns buzz if not found
void cgi_S_StartBackgroundTrack(const char* intro, const char* loop, qboolean bForceStart); // empty name stops music
float cgi_S_GetSampleLength(sfxHandle_t sfx);

void cgi_R_LoadWorldMap(const char* mapname);

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t cgi_R_RegisterModel(const char* name); // returns rgb axis if not found
qhandle_t cgi_R_RegisterSkin(const char* name);
qhandle_t cgi_R_RegisterShader(const char* name); // returns default shader if not found
qhandle_t cgi_R_RegisterShaderNoMip(const char* name); // returns all white if not found
qhandle_t cgi_R_RegisterFont(const char* name);
int cgi_R_Font_StrLenPixels(const char* text, int iFontIndex, float scale = 1.0f);
int cgi_R_Font_StrLenChars(const char* text);
int cgi_R_Font_HeightPixels(int iFontIndex, float scale = 1.0f);
void cgi_R_Font_DrawString(int ox, int oy, const char* text, const float* rgba, int setIndex, int iMaxPixelWidth,
	float scale = 1.0f);
qboolean cgi_Language_IsAsian();
qboolean cgi_Language_UsesSpaces();
unsigned cgi_AnyLanguage_ReadCharFromString(const char* psText, int* pi_advance_count,
	qboolean* pbIsTrailingPunctuation = nullptr);

void cgi_R_SetRefractProp(float alpha, float stretch, qboolean prepost, qboolean negate);

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void cgi_R_ClearScene();
void cgi_R_AddRefEntityToScene(const refEntity_t* re);
void cgi_R_GetLighting(const vec3_t origin, vec3_t ambientLight, vec3_t directedLight, vec3_t ligthDir);

//used by miscents
qboolean cgi_R_inPVS(vec3_t p1, vec3_t p2);

// polys are intended for simple wall marks, not really for doing
// significant construction
void cgi_R_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts);
void cgi_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void cgi_R_RenderScene(const refdef_t* fd);
void cgi_R_SetColor(const float* rgba); // NULL = 1,1,1,1
void cgi_R_DrawStretchPic(float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t hShader);

void cgi_R_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs);
void cgi_R_LerpTag(orientation_t* tag, qhandle_t mod, int startFrame, int endFrame,
	float frac, const char* tagName);
// Does weird, barely controllable rotation behaviour
void cgi_R_DrawRotatePic(float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, float a, qhandle_t hShader, float aspectCorrection);
// rotates image around exact center point of passed in coords
void cgi_R_DrawRotatePic2(float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, float a, qhandle_t hShader, float aspectCorrection);
void cgi_R_SetRangeFog(float range);
void cgi_R_LAGoggles();
void cgi_R_Scissor(float x, float y, float w, float h);

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void cgi_GetGlconfig(glconfig_t* glconfig);

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void cgi_GetGameState(gameState_t* gamestate);

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned seperately so that
// snapshot latency can be calculated.
void cgi_GetCurrentSnapshotNumber(int* snapshotNumber, int* serverTime);

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean cgi_GetSnapshot(int snapshotNumber, snapshot_t* snapshot);

qboolean cgi_GetDefaultState(int entityIndex, entityState_t* state);

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean cgi_GetServerCommand(int serverCommandNumber);

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int cgi_GetCurrentCmdNumber();
qboolean cgi_GetUserCmd(int cmdNumber, usercmd_t* ucmd);

// used for the weapon select and zoom
void cgi_SetUserCmdValue(int stateValue, float sensitivityScale, float mPitchOverride, float mYawOverride);
void cgi_SetUserCmdAngles(float pitchOverride, float yawOverride, float rollOverride);

void cgi_S_UpdateAmbientSet(const char* name, vec3_t origin);
void cgi_AS_ParseSets();
void cgi_AS_AddPrecacheEntry(const char* name);
int cgi_S_AddLocalSet(const char* name, vec3_t listener_origin, vec3_t origin, int entID, int time);
sfxHandle_t cgi_AS_GetBModelSound(const char* name, int stage);

void CG_DrawMiscEnts();

//-----------------------------
// Effects related prototypes
//-----------------------------

void FX_YellowLightningStrike(vec3_t start, vec3_t end);
void FX_LightningStrike(vec3_t start, vec3_t end);

// Weapon prototypes
void FX_Saber(vec3_t start, vec3_t normal, float height, float radius, saber_colors_t color);

void FX_BryarHitWall(vec3_t origin, vec3_t normal);
void FX_BryarAltHitWall(vec3_t origin, vec3_t normal, int power);
void FX_BryarHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);
void FX_BryarAltHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_BlasterProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_BlasterAltFireThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_BlasterWeaponHitWall(vec3_t origin, vec3_t normal);
void FX_BlasterWeaponHitPlayer(gentity_t* hit, vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_WristProjectileThink(centity_t* cent, const weaponInfo_s* weapon);

void FX_DroidekaProjectileThink(centity_t* cent, const weaponInfo_s* weapon);

void FX_CloneProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_CloneAltFireThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_CloneAltHitWall(vec3_t origin, vec3_t normal, int power);
void FX_CloneWeaponHitWall(vec3_t origin, vec3_t normal);
void FX_CloneWeaponHitPlayer(gentity_t* hit, vec3_t origin, vec3_t normal, qboolean humanoid);
void FX_CloneAltHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_DisruptorMainShot(vec3_t start, vec3_t end);
void FX_DisruptorAltShot(vec3_t start, vec3_t end, qboolean full_charge);
void FX_DisruptorAltMiss(vec3_t origin, vec3_t normal);

void FX_BowcasterHitWall(vec3_t origin, vec3_t normal);
void FX_BowcasterHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_RepeaterHitWall(vec3_t origin, vec3_t normal);
void FX_RepeaterAltHitWall(vec3_t origin, vec3_t normal);
void FX_RepeaterHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);
void FX_RepeaterAltHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_DEMP2_HitWall(vec3_t origin, vec3_t normal);
void FX_DEMP2_HitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);
void FX_DEMP2_AltDetonate(vec3_t org, float size);

void FX_FlechetteProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_FlechetteWeaponHitWall(vec3_t origin, vec3_t normal);
void FX_FlechetteWeaponHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_RocketHitWall(vec3_t origin, vec3_t normal);
void FX_RocketHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_ConcProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_ConcHitWall(vec3_t origin, vec3_t normal);
void FX_ConcHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);
void FX_ConcAltShot(vec3_t start, vec3_t end);
void FX_ConcAltMiss(vec3_t origin, vec3_t normal);

void FX_EmplacedHitWall(vec3_t origin, vec3_t normal, qboolean eweb);
void FX_EmplacedHitPlayer(vec3_t origin, vec3_t normal, qboolean eweb);
void FX_EmplacedProjectileThink(centity_t* cent, const weaponInfo_s* weapon);

void FX_ATSTMainHitWall(vec3_t origin, vec3_t normal);
void FX_ATSTMainHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);
void FX_ATSTMainProjectileThink(centity_t* cent, const weaponInfo_s* weapon);

void FX_TuskenShotProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_TuskenShotWeaponHitWall(vec3_t origin, vec3_t normal);
void FX_TuskenShotWeaponHitPlayer(gentity_t* hit, vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_NoghriShotProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_NoghriShotWeaponHitWall(vec3_t origin, vec3_t normal);
void FX_NoghriShotWeaponHitPlayer(gentity_t* hit, vec3_t origin, vec3_t normal, qboolean humanoid);

void CG_BounceEffect(int weapon, vec3_t origin, vec3_t normal);
void CG_MissileStick(const centity_t* cent, int weapon);

void FX_DestructionProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_DestructionHitWall(vec3_t origin, vec3_t normal);
void FX_DestructionHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_BlastProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_BlastHitWall(vec3_t origin, vec3_t normal);
void FX_BlastHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void FX_StrikeProjectileThink(centity_t* cent, const weaponInfo_s* weapon);
void FX_StrikeHitWall(vec3_t origin, vec3_t normal);
void FX_StrikeHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid);

void CG_MissileHitPlayer(const centity_t* cent, int weapon, vec3_t origin, vec3_t dir, qboolean alt_fire);
void CG_MissileHitWall(const centity_t* cent, int weapon, vec3_t origin, vec3_t dir, qboolean alt_fire);

void CG_DrawTargetBeam(vec3_t start, vec3_t end, vec3_t norm, const char* beam_fx, const char* impact_fx);

void FX_stunAltFireThink(centity_t* cent, const weaponInfo_s* weapon);

qboolean CG_VehicleWeaponImpact(centity_t* cent);

/*
Ghoul2 Insert Start
*/
// CG specific API access
void trap_G2_SetGhoul2ModelIndexes(CGhoul2Info_v& ghoul2, qhandle_t* model_list, qhandle_t* skin_list);
void CG_Init_CG();

void CG_SetGhoul2Info(refEntity_t* ent, const centity_t* cent);

/*
Ghoul2 Insert End
*/
void trap_Com_SetOrgAngles(vec3_t org, vec3_t angles);
void trap_R_GetLightStyle(int style, color4ub_t color);
void trap_R_SetLightStyle(int style, int color);

int trap_CIN_PlayCinematic(const char* arg0, int xpos, int ypos, int width, int height, int bits, const char* psAudioFile = nullptr);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic(int handle);
void trap_CIN_DrawCinematic(int handle);
void trap_CIN_SetExtents(int handle, int x, int y, int w, int h);
void* cgi_Z_Malloc(int size, int tag);
void cgi_Z_Free(void* ptr);

int cgi_SP_GetStringTextString(const char* text, char* buffer, int buffer_length);

void cgi_UI_Menu_Reset();
void cgi_UI_Menu_New(char* buf);
void cgi_UI_Menu_OpenByName(char* buf);
void cgi_UI_SetActive_Menu(char* name);
void cgi_UI_Parse_Int(int* value);
void cgi_UI_Parse_String(char* buf);
void cgi_UI_Parse_Float(float* value);
int cgi_UI_StartParseSession(char* menuFile, char** buf);
void cgi_UI_ParseExt(char** token);
void cgi_UI_MenuPaintAll();
void cgi_UI_MenuCloseAll();
void cgi_UI_String_Init();
int cgi_UI_GetMenuItemInfo(const char* menuFile, const char* itemName, int* x, int* y, int* w, int* h, vec4_t color,
	qhandle_t* background);
int cgi_UI_GetMenuInfo(char* menuFile, int* x, int* y, int* w, int* h);
void cgi_UI_Menu_Paint(void* menu, qboolean force);
void* cgi_UI_GetMenuByName(const char* menu);

void SetWeaponSelectTime();

void CG_PlayEffectBolted(const char* fx_name, int modelIndex, int bolt_index, int entNum, vec3_t origin,
	int i_loop_time = 0, bool is_relative = false);
void CG_PlayEffectIDBolted(int fx_id, int modelIndex, int bolt_index, int entNum, vec3_t origin, int i_loop_time = 0,
	bool is_relative = false);
void CG_PlayEffectOnEnt(const char* fx_name, int clientNum, vec3_t origin, const vec3_t fwd);
void CG_PlayEffectIDOnEnt(int fx_id, int clientNum, vec3_t origin, const vec3_t fwd);
void CG_PlayEffect(const char* fx_name, vec3_t origin, const vec3_t fwd);
void CG_PlayEffectID(int fx_id, vec3_t origin, const vec3_t fwd);

void CG_ClearLightStyles();
void CG_RunLightStyles();
void CG_SetLightstyle(int i);

//trueview stuff
void CG_TrueViewInit();
void CG_AdjustEyePos(const char* model_name);

#endif	//__CG_LOCAL_H__
