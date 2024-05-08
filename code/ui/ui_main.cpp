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

/*
=======================================================================

USER INTERFACE MAIN

=======================================================================
*/

#include <algorithm>
#include <vector>

#include "../server/exe_headers.h"

#include "ui_local.h"

#include "menudef.h"

#include "ui_shared.h"

#include "../game/bg_public.h"
#include "../game/anims.h"
extern stringID_table_t anim_table[MAX_ANIMATIONS + 1];

#include "../qcommon/stringed_ingame.h"
#include "../qcommon/stv_version.h"
#include "../qcommon/q_shared.h"

extern qboolean ItemParse_model_g2anim_go(itemDef_t* item, const char* animName);
extern qboolean ItemParse_asset_model_go(itemDef_t* item, const char* name);
extern qboolean ItemParse_model_g2skin_go(itemDef_t* item, const char* skinName);
extern qboolean UI_SaberModelForSaber(const char* saber_name, char* saber_model);
extern qboolean UI_SaberSkinForSaber(const char* saber_name, char* saber_skin);
extern void UI_SaberAttachToChar(itemDef_t* item);
extern void FS_Restart();

extern qboolean PC_Script_Parse(const char** out);

constexpr auto LISTBUFSIZE = 10240;

static struct
{
	char listBuf[LISTBUFSIZE]; //	The list of file names read in

	// For scrolling through file names
	int currentLine; //	Index to currentSaveFileComments[] currently highlighted
	int saveFileCnt; //	Number of save files read in

	int awaitingSave; //	Flag to see if user wants to overwrite a game.

	char* savegameMap;
	int savegameFromFlag;
} s_savegame;

constexpr auto MAX_SAVELOADFILES = 100;
constexpr auto MAX_SAVELOADNAME = 32;

#ifdef JK2_MODE
byte screenShotBuf[SG_SCR_WIDTH * SG_SCR_HEIGHT * 4];
#endif

using savedata_t = struct
{
	char* currentSaveFileName; // file name of savegame
	char currentSaveFileComments[iSG_COMMENT_SIZE]; // file comment
	char currentSaveFileDateTimeString[iSG_COMMENT_SIZE]; // file time and date
	time_t currentSaveFileDateTime;
	char currentSaveFileMap[MAX_TOKEN_CHARS]; // map save game is from
};

static savedata_t s_savedata[MAX_SAVELOADFILES];
void UI_SetActiveMenu(const char* menuname, const char* menuID);
void ReadSaveDirectory();
void Item_RunScript(itemDef_t* item, const char* s);
qboolean Item_SetFocus(itemDef_t* item, float x, float y);

qboolean Asset_Parse(char** buffer);
menuDef_t* Menus_FindByName(const char* p);
void Menus_HideItems(const char* menuName);
int Text_Height(const char* text, float scale, int iFontIndex);
int Text_Width(const char* text, float scale, int iFontIndex);
void _UI_DrawTopBottom(float x, float y, float w, float h, float size);
void _UI_DrawSides(float x, float y, float w, float h, float size);
void UI_CheckVid1Data(const char* menuTo, const char* warningMenuName);
void UI_GetVideoSetup();
void UI_UpdateVideoSetup();
static void UI_UpdateCharacterCvars();
static void UI_GetCharacterCvars();
static void UI_UpdateSaberCvars();
static void UI_GetSaberCvars();
static void UI_ResetSaberCvars();
static void UI_InitAllocForcePowers(const char* forceName);
static void UI_AffectForcePowerLevel(const char* forceName);
static void UI_ShowForceLevelDesc(const char* forceName);
static void UI_ResetForceLevels();
static void UI_ClearWeapons();
static void UI_clearsabers();
static void UI_GiveAmmo(int ammoIndex, int ammoAmount, const char* soundfile);
static void UI_GiveWeapon(int weaponIndex);
static void UI_EquipWeapon(int weaponIndex);
static void UI_LoadMissionSelectMenu(const char* cvarName);
static void UI_AddWeaponSelection(int weaponIndex, int ammoIndex, int ammoAmount, const char* iconItemName,
	const char* litIconItemName, const char* hexBackground, const char* soundfile);
static void UI_AddPistolSelection(int weaponIndex, int ammoIndex, int ammoAmount, const char* iconItemName,
	const char* litIconItemName, const char* hexBackground, const char* soundfile);
static void UI_AddThrowWeaponSelection(int weaponIndex, int ammoIndex, int ammoAmount, const char* iconItemName,
	const char* litIconItemName, const char* hexBackground, const char* soundfile);
static void UI_RemoveWeaponSelection(int weapon_selection_index);
static void UI_Removepistolselection();
static void UI_RemoveThrowWeaponSelection();
static void UI_HighLightWeaponSelection(int selectionslot);
static void UI_Highlightpistolselection();
static void UI_NormalWeaponSelection(int selectionslot);
static void UI_Normalpistolselection();
static void UI_NormalThrowSelection();
static void UI_HighLightThrowSelection();
static void UI_ClearInventory();
static void UI_GiveInventory(int itemIndex, int amount);
static void UI_ForcePowerWeaponsButton(qboolean activeFlag);
static void UI_UpdateCharacterSkin();
static void UI_UpdateCharacter(qboolean changedModel);
static void UI_UpdateSaberType();
static void UI_UpdateSaberHilt(qboolean secondSaber);
//static void		UI_UpdateSaberColor( qboolean secondSaber );
static void UI_InitWeaponSelect();
static void UI_WeaponHelpActive();

#ifndef JK2_MODE
static void UI_UpdateFightingStyle();
static void UI_UpdateFightingStyleChoices();
static void UI_CalcForceStatus();
#endif // !JK2_MODE

static void UI_DecrementForcePowerLevel();
static void UI_DecrementCurrentForcePower();
static void UI_ShutdownForceHelp();
static void UI_ForceHelpActive();

#ifndef JK2_MODE
static void UI_DemoSetForceLevels();
#endif // !JK2_MODE

static void UI_RecordForceLevels();
static void UI_RecordWeapons();
static void UI_ResetCharacterListBoxes();

void UI_LoadMenus(const char* menuFile, qboolean reset);
static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw,
	int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader,
	int textStyle, int iFontIndex);
static qboolean UI_OwnerDrawVisible(int flags);
int UI_OwnerDrawWidth(int ownerDraw, float scale);
static void UI_Update(const char* name);
void UI_UpdateCvars();
void UI_ResetDefaults();
void UI_AdjustSaveGameListBox(int currentLine);

void Menus_CloseByName(const char* p);

// Movedata Sounds
enum
{
	MDS_NONE = 0,
	MDS_FORCE_JUMP,
	MDS_ROLL,
	MDS_SABER,
	MDS_MOVE_SOUNDS_MAX
};

enum
{
	MD_ACROBATICS = 0,
	MD_SINGLE_FAST,
	MD_SINGLE_MEDIUM,
	MD_SINGLE_STRONG,
	MD_DUAL_SABERS,
	MD_SABER_STAFF,
	MD_MOVE_TITLE_MAX
};

// Some hard coded badness
// At some point maybe this should be externalized to a .dat file
const char* datapadMoveTitleData[MD_MOVE_TITLE_MAX] =
{
	"@MENUS_ACROBATICS",
	"@MENUS_SINGLE_FAST",
	"@MENUS_SINGLE_MEDIUM",
	"@MENUS_SINGLE_STRONG",
	"@MENUS_DUAL_SABERS",
	"@MENUS_SABER_STAFF",
};

const char* datapadMoveTitleBaseAnims[MD_MOVE_TITLE_MAX] =
{
	"BOTH_RUN1",
	"BOTH_SABERFAST_STANCE",
	"BOTH_STAND2",
	"BOTH_SABERSLOW_STANCE",
	"BOTH_SABERDUAL_STANCE",
	"BOTH_SABERSTAFF_STANCE",
};

constexpr auto MAX_MOVES = 16;

using datpadmovedata_t = struct
{
	const char* title;
	const char* desc;
	const char* anim;
	short sound;
};

static datpadmovedata_t datapadMoveData[MD_MOVE_TITLE_MAX][MAX_MOVES] =
{
	{
		// Acrobatics
		{"@MENUS_FORCE_JUMP1", "@MENUS_FORCE_JUMP1_DESC", "BOTH_FORCEJUMP1", MDS_FORCE_JUMP},
		{"@MENUS_FORCE_FLIP", "@MENUS_FORCE_FLIP_DESC", "BOTH_FLIP_F", MDS_FORCE_JUMP},
		{"@MENUS_ROLL", "@MENUS_ROLL_DESC", "BOTH_ROLL_F", MDS_ROLL},
		{"@MENUS_BACKFLIP_OFF_WALL", "@MENUS_BACKFLIP_OFF_WALL_DESC", "BOTH_WALL_FLIP_BACK1", MDS_FORCE_JUMP},
		{"@MENUS_SIDEFLIP_OFF_WALL", "@MENUS_SIDEFLIP_OFF_WALL_DESC", "BOTH_WALL_FLIP_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_WALL_RUN", "@MENUS_WALL_RUN_DESC", "BOTH_WALL_RUN_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_LONG_JUMP", "@MENUS_LONG_JUMP_DESC", "BOTH_FORCELONGLEAP_START", MDS_FORCE_JUMP},
		{"@MENUS_WALL_GRAB_JUMP", "@MENUS_WALL_GRAB_JUMP_DESC", "BOTH_FORCEWALLREBOUND_FORWARD", MDS_FORCE_JUMP},
		{
			"@MENUS_RUN_UP_WALL_BACKFLIP", "@MENUS_RUN_UP_WALL_BACKFLIP_DESC", "BOTH_FORCEWALLRUNFLIP_START",
			MDS_FORCE_JUMP
		},
		{"@MENUS_JUMPUP_FROM_KNOCKDOWN", "@MENUS_JUMPUP_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN3", MDS_NONE},
		{"@MENUS_JUMPKICK_FROM_KNOCKDOWN", "@MENUS_JUMPKICK_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN2", MDS_NONE},
		{"@MENUS_ROLL_FROM_KNOCKDOWN", "@MENUS_ROLL_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN1", MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Single Saber, Fast Style
		{"@MENUS_STAB_BACK", "@MENUS_STAB_BACK_DESC", "BOTH_A2_STABBACK1", MDS_SABER},
		{"@MENUS_LUNGE_ATTACK", "@MENUS_LUNGE_ATTACK_DESC", "BOTH_LUNGE2_B__T_", MDS_SABER},
		{"@MENUS_FORCE_PULL_IMPALE", "@MENUS_FORCE_PULL_IMPALE_DESC", "BOTH_PULL_IMPALE_STAB", MDS_SABER},
		{"@MENUS_FAST_ATTACK_KATA", "@MENUS_FAST_ATTACK_KATA_DESC", "BOTH_A1_SPECIAL", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", MDS_FORCE_JUMP},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Single Saber, Medium Style
		{"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", MDS_SABER},
		{"@MENUS_FLIP_ATTACK", "@MENUS_FLIP_ATTACK_DESC", "BOTH_JUMPFLIPSLASHDOWN1", MDS_FORCE_JUMP},
		{"@MENUS_FORCE_PULL_SLASH", "@MENUS_FORCE_PULL_SLASH_DESC", "BOTH_PULL_IMPALE_SWING", MDS_SABER},
		{"@MENUS_MEDIUM_ATTACK_KATA", "@MENUS_MEDIUM_ATTACK_KATA_DESC", "BOTH_A2_SPECIAL", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", MDS_FORCE_JUMP},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Single Saber, Strong Style
		{"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", MDS_SABER},
		{"@MENUS_JUMP_ATTACK", "@MENUS_JUMP_ATTACK_DESC", "BOTH_FORCELEAP2_T__B_", MDS_FORCE_JUMP},
		{"@MENUS_FORCE_PULL_SLASH", "@MENUS_FORCE_PULL_SLASH_DESC", "BOTH_PULL_IMPALE_SWING", MDS_SABER},
		{"@MENUS_STRONG_ATTACK_KATA", "@MENUS_STRONG_ATTACK_KATA_DESC", "BOTH_A3_SPECIAL", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", MDS_FORCE_JUMP},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Dual Sabers
		{"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", MDS_SABER},
		{"@MENUS_FLIP_FORWARD_ATTACK", "@MENUS_FLIP_FORWARD_ATTACK_DESC", "BOTH_JUMPATTACK6", MDS_FORCE_JUMP},
		{"@MENUS_DUAL_SABERS_TWIRL", "@MENUS_DUAL_SABERS_TWIRL_DESC", "BOTH_SPINATTACK6", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_DUAL", MDS_FORCE_JUMP},
		{"@MENUS_DUAL_SABER_BARRIER", "@MENUS_DUAL_SABER_BARRIER_DESC", "BOTH_A6_SABERPROTECT", MDS_SABER},
		{"@MENUS_DUAL_STAB_FRONT_BACK", "@MENUS_DUAL_STAB_FRONT_BACK_DESC", "BOTH_A6_FB", MDS_SABER},
		{"@MENUS_DUAL_STAB_LEFT_RIGHT", "@MENUS_DUAL_STAB_LEFT_RIGHT_DESC", "BOTH_A6_LR", MDS_SABER},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		// Saber Staff
		{"@MENUS_STAB_BACK", "@MENUS_STAB_BACK_DESC", "BOTH_A2_STABBACK1", MDS_SABER},
		{"@MENUS_BACK_FLIP_ATTACK", "@MENUS_BACK_FLIP_ATTACK_DESC", "BOTH_JUMPATTACK7", MDS_FORCE_JUMP},
		{"@MENUS_SABER_STAFF_TWIRL", "@MENUS_SABER_STAFF_TWIRL_DESC", "BOTH_SPINATTACK7", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_STAFF", MDS_FORCE_JUMP},
		{"@MENUS_SPINNING_KATA", "@MENUS_SPINNING_KATA_DESC", "BOTH_A7_SOULCAL", MDS_SABER},
		{"@MENUS_KICK1", "@MENUS_KICK1_DESC", "BOTH_A7_KICK_F", MDS_FORCE_JUMP},
		{"@MENUS_JUMP_KICK", "@MENUS_JUMP_KICK_DESC", "BOTH_A7_KICK_F_AIR", MDS_FORCE_JUMP},
		{"@MENUS_SPLIT_KICK", "@MENUS_SPLIT_KICK_DESC", "BOTH_A7_KICK_RL", MDS_FORCE_JUMP},
		{"@MENUS_SPIN_KICK", "@MENUS_SPIN_KICK_DESC", "BOTH_A7_KICK_S", MDS_FORCE_JUMP},
		{"@MENUS_FLIP_KICK", "@MENUS_FLIP_KICK_DESC", "BOTH_A7_KICK_BF", MDS_FORCE_JUMP},
		{"@MENUS_BUTTERFLY_ATTACK", "@MENUS_BUTTERFLY_ATTACK_DESC", "BOTH_BUTTERFLY_FR1", MDS_SABER},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	}
};

static int gamecodetoui[] = { 4, 2, 3, 0, 5, 1, 6 };

uiInfo_t uiInfo;

static void UI_RegisterCvars();
void UI_Load();

static int UI_GetScreenshotFormatForString(const char* str)
{
	if (!Q_stricmp(str, "jpg") || !Q_stricmp(str, "jpeg"))
		return SSF_JPEG;
	if (!Q_stricmp(str, "tga"))
		return SSF_TGA;
	if (!Q_stricmp(str, "png"))
		return SSF_PNG;
	return -1;
}

static const char* UI_GetScreenshotFormatString(const int format)
{
	switch (format)
	{
	default:
	case SSF_JPEG:
		return "jpg";
	case SSF_TGA:
		return "tga";
	case SSF_PNG:
		return "png";
	}
}

using cvarTable_t = struct cvarTable_s
{
	vmCvar_t* vmCvar;
	const char* cvarName;
	const char* defaultString;
	void (*update)();
	uint32_t cvarFlags;
};

vmCvar_t ui_menuFiles;
vmCvar_t ui_hudFiles;

vmCvar_t ui_char_anim;
vmCvar_t ui_char_model;
vmCvar_t ui_char_skin_head;
vmCvar_t ui_char_skin_torso;
vmCvar_t ui_char_skin_legs;
vmCvar_t ui_saber_type;
vmCvar_t ui_saber;
vmCvar_t ui_saber2;
vmCvar_t ui_saber_color;
vmCvar_t ui_saber2_color;
vmCvar_t ui_char_color_red;
vmCvar_t ui_char_color_green;
vmCvar_t ui_char_color_blue;
vmCvar_t ui_PrecacheModels;
vmCvar_t ui_screenshotType;

vmCvar_t ui_rgb_saber_red;
vmCvar_t ui_rgb_saber_green;
vmCvar_t ui_rgb_saber_blue;

vmCvar_t ui_rgb_saber2_red;
vmCvar_t ui_rgb_saber2_green;
vmCvar_t ui_rgb_saber2_blue;

vmCvar_t ui_SFXSabers;
vmCvar_t ui_SFXSabersGlowSize;
vmCvar_t ui_SFXSabersCoreSize;
vmCvar_t ui_SFXLerpOrigin;
//
vmCvar_t ui_SFXSabersGlowSizeEP1;
vmCvar_t ui_SFXSabersCoreSizeEP1;
vmCvar_t ui_SFXSabersGlowSizeEP2;
vmCvar_t ui_SFXSabersCoreSizeEP2;
vmCvar_t ui_SFXSabersGlowSizeEP3;
vmCvar_t ui_SFXSabersCoreSizeEP3;
vmCvar_t ui_SFXSabersGlowSizeSFX;
vmCvar_t ui_SFXSabersCoreSizeSFX;
vmCvar_t ui_SFXSabersGlowSizeOT;
vmCvar_t ui_SFXSabersCoreSizeOT;
vmCvar_t ui_SFXSabersGlowSizeROTJ;
vmCvar_t ui_SFXSabersCoreSizeROTJ;
vmCvar_t ui_SFXSabersGlowSizeTFA;
vmCvar_t ui_SFXSabersCoreSizeTFA;
vmCvar_t ui_SFXSabersGlowSizeUSB;
vmCvar_t ui_SFXSabersCoreSizeUSB;
//
vmCvar_t ui_SFXSabersGlowSizeRebels;
vmCvar_t ui_SFXSabersCoreSizeRebels;

vmCvar_t ui_SerenityJediEngineMode;

vmCvar_t ui_char_model_angle;

vmCvar_t ui_com_outcast;
vmCvar_t ui_update6firststartup;
vmCvar_t ui_totgfirststartup;
vmCvar_t ui_cursor;

vmCvar_t ui_npc_saber;
vmCvar_t ui_npc_sabertwo;

vmCvar_t ui_npc_sabercolor;
vmCvar_t ui_npc_sabertwocolor;

cvar_t* g_NPCsabercolor;
cvar_t* g_NPCsabertwo;
cvar_t* g_NPCsabertwocolor;
vmCvar_t ui_com_kotor;
vmCvar_t ui_com_rend2;

static void UI_UpdateScreenshot()
{
	qboolean changed = qfalse;
	// check some things
	if (ui_screenshotType.string[0] && isalpha(ui_screenshotType.string[0]))
	{
		const int ssf = UI_GetScreenshotFormatForString(ui_screenshotType.string);
		if (ssf == -1)
		{
			ui.Printf("UI Screenshot Format Type '%s' unrecognised, defaulting to JPEG\n", ui_screenshotType.string);
			uiInfo.uiDC.screenshotFormat = SSF_JPEG;
			changed = qtrue;
		}
		else
			uiInfo.uiDC.screenshotFormat = ssf;
	}
	else if (ui_screenshotType.integer < SSF_JPEG || ui_screenshotType.integer > SSF_PNG)
	{
		ui.Printf("ui_screenshotType %i is out of range, defaulting to 0 (JPEG)\n", ui_screenshotType.integer);
		uiInfo.uiDC.screenshotFormat = SSF_JPEG;
		changed = qtrue;
	}
	else
	{
		uiInfo.uiDC.screenshotFormat = atoi(ui_screenshotType.string);
		changed = qtrue;
	}

	if (changed)
	{
		Cvar_Set("ui_screenshotType", UI_GetScreenshotFormatString(uiInfo.uiDC.screenshotFormat));
		Cvar_Update(&ui_screenshotType);
	}
}

vmCvar_t r_ratiofix;

static void UI_Set2DRatio()
{
	if (r_ratiofix.integer)
	{
		uiInfo.uiDC.widthRatioCoef = static_cast<float>(SCREEN_WIDTH * uiInfo.uiDC.glconfig.vidHeight) / static_cast<
			float>(SCREEN_HEIGHT * uiInfo.uiDC.glconfig.vidWidth);
	}
	else
	{
		uiInfo.uiDC.widthRatioCoef = 1.0f;
	}
}

static cvarTable_t cvarTable[] =
{
	{&ui_menuFiles, "ui_menuFiles", "ui/menus.txt", nullptr, CVAR_ARCHIVE},
#ifdef JK2_MODE
	{ &ui_hudFiles,				"cg_hudFiles",			"ui/jk2hud.txt", nullptr, CVAR_ARCHIVE},
#else
	{&ui_hudFiles, "cg_hudFiles", "ui/jahud.txt", nullptr, CVAR_ARCHIVE},
#endif

	{&ui_char_anim, "ui_char_anim", "BOTH_MENUIDLE1", nullptr, 0},

	{&ui_char_model, "ui_char_model", "", nullptr, 0}, //these are filled in by the "g_*" versions on load
	{&ui_char_skin_head, "ui_char_skin_head", "", nullptr, 0},
	//the "g_*" versions are initialized in UI_Init, ui_atoms.cpp
	{&ui_char_skin_torso, "ui_char_skin_torso", "", nullptr, 0},
	{&ui_char_skin_legs, "ui_char_skin_legs", "", nullptr, 0},

	{&ui_saber_type, "ui_saber_type", "", nullptr, 0},
	{&ui_saber, "ui_saber", "", nullptr, 0},
	{&ui_saber2, "ui_saber2", "", nullptr, 0},
	{&ui_saber_color, "ui_saber_color", "", nullptr, 0},
	{&ui_saber2_color, "ui_saber2_color", "", nullptr, 0},

	{&ui_char_color_red, "ui_char_color_red", "", nullptr, 0},
	{&ui_char_color_green, "ui_char_color_green", "", nullptr, 0},
	{&ui_char_color_blue, "ui_char_color_blue", "", nullptr, 0},

	{&ui_PrecacheModels, "ui_PrecacheModels", "1", nullptr, CVAR_ARCHIVE},

	{&ui_screenshotType, "ui_screenshotType", "jpg", UI_UpdateScreenshot, CVAR_ARCHIVE},

	{&ui_rgb_saber_red, "ui_rgb_saber_red", "", nullptr, 0},
	{&ui_rgb_saber_blue, "ui_rgb_saber_blue", "", nullptr, 0},
	{&ui_rgb_saber_green, "ui_rgb_saber_green", "", nullptr, 0},
	{&ui_rgb_saber2_red, "ui_rgb_saber2_red", "", nullptr, 0},
	{&ui_rgb_saber2_blue, "ui_rgb_saber2_blue", "", nullptr, 0},
	{&ui_rgb_saber2_green, "ui_rgb_saber2_green", "", nullptr, 0},

	{&ui_SFXSabers, "cg_SFXSabers", "4", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSize, "cg_SFXSabersGlowSize", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSize, "cg_SFXSabersCoreSize", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXLerpOrigin, "cg_SFXLerpOrigin", "0", nullptr, CVAR_ARCHIVE},
	//
	{&ui_SFXSabersGlowSizeEP1, "cg_SFXSabersGlowSizeEP1", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeEP1, "cg_SFXSabersCoreSizeEP1", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeEP2, "cg_SFXSabersGlowSizeEP2", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeEP2, "cg_SFXSabersCoreSizeEP2", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeEP3, "cg_SFXSabersGlowSizeEP3", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeEP3, "cg_SFXSabersCoreSizeEP3", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeSFX, "cg_SFXSabersGlowSizeSFX", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeSFX, "cg_SFXSabersCoreSizeSFX", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeOT, "cg_SFXSabersGlowSizeOT", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeOT, "cg_SFXSabersCoreSizeOT", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeROTJ, "cg_SFXSabersGlowSizeROTJ", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeROTJ, "cg_SFXSabersCoreSizeROTJ", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeTFA, "cg_SFXSabersGlowSizeTFA", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeTFA, "cg_SFXSabersCoreSizeTFA", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeUSB, "cg_SFXSabersGlowSizeUSB", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeUSB, "cg_SFXSabersCoreSizeUSB", "1.0", nullptr, CVAR_ARCHIVE},
	//
	{&ui_SFXSabersGlowSizeRebels, "cg_SFXSabersGlowSizeRebels", "0.6", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeRebels, "cg_SFXSabersCoreSizeRebels", "0.6", nullptr, CVAR_ARCHIVE},

	{&ui_SerenityJediEngineMode, "g_SerenityJediEngineMode", "1", nullptr, CVAR_ARCHIVE},

	{&ui_char_model_angle, "ui_char_model_angle", "175", nullptr, 0},

	{&ui_com_outcast, "com_outcast", "0", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART},

	{&ui_update6firststartup, "g_update6firststartup", "1", nullptr, 0},

	{&ui_totgfirststartup, "g_totgfirststartup", "1", nullptr, 0},

	{&ui_cursor, "g_cursor", "4", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART}, //11 or 12 with base cursor

	{&r_ratiofix, "r_ratiofix", "0", UI_Set2DRatio, CVAR_ARCHIVE},

	{&ui_npc_saber, "ui_npc_saber", "", nullptr},
	{&ui_npc_sabertwo, "ui_npc_sabertwo", "", nullptr},

	{&ui_npc_sabercolor, "ui_npc_sabercolor", "", nullptr},
	{&ui_npc_sabertwocolor, "ui_npc_sabertwocolor", "", nullptr},

	{&ui_com_kotor, "com_kotor", "0", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART},

	{&ui_com_rend2, "com_rend2", "0", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART},
};

constexpr auto FP_UPDATED_NONE = -1;
constexpr auto NOWEAPON = -1;

static constexpr size_t cvarTableSize = std::size(cvarTable);

extern void SE_CheckForLanguageUpdates();

void Text_Paint(float x, float y, float scale, vec4_t color, const char* text, int iMaxPixelWidth, int style,
	int iFontIndex);
int Key_GetCatcher();

constexpr auto UI_FPS_FRAMES = 4;

void _UI_Refresh(const int realtime)
{
	static int index;
	static int previousTimes[UI_FPS_FRAMES];

	if (!(Key_GetCatcher() & KEYCATCH_UI))
	{
		return;
	}
	SE_CheckForLanguageUpdates();

	if (Menus_AnyFullScreenVisible())
	{
		//if not in full screen, don't mess with ghoul2
		//rww - ghoul2 needs to know what time it is even if the client/server are not running
		//FIXME: this screws up the game when you go back to the game...
		re.G2API_SetTime(realtime, 0);
		re.G2API_SetTime(realtime, 1);
	}

	uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime = realtime;

	previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
	index++;
	if (index > UI_FPS_FRAMES)
	{
		// average multiple frames together to smooth changes out a bit
		int total = 0;
		for (const int previousTime : previousTimes)
		{
			total += previousTime;
		}
		if (!total)
		{
			total = 1;
		}
		uiInfo.uiDC.FPS = static_cast<float>(1000 * UI_FPS_FRAMES) / total;
	}

	UI_UpdateCvars();

	if (Menu_Count() > 0)
	{
		// paint all the menus
		Menu_PaintAll();
	}

	// draw cursor
	UI_SetColor(nullptr);

	if (Menu_Count() > 0)
	{
		if (uiInfo.uiDC.cursorShow == qtrue)
		{
			if (ui_cursor.integer == 0)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor);
			}
			else if (ui_cursor.integer == 1)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_anakin);
			}
			else if (ui_cursor.integer == 2)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_jk);
			}
			else if (ui_cursor.integer == 3)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_katarn);
			}
			else if (ui_cursor.integer == 4)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_kylo);
			}
			else if (ui_cursor.integer == 5)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_luke);
			}
			else if (ui_cursor.integer == 6)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_obiwan);
			}
			else if (ui_cursor.integer == 7)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_oldrepublic);
			}
			else if (ui_cursor.integer == 8)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_quigon);
			}
			else if (ui_cursor.integer == 9)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_rey);
			}
			else if (ui_cursor.integer == 10)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_vader);
			}
			else if (ui_cursor.integer == 11)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_windu);
			}
			else
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_kylo);
			}
		}
	}
}

#define MODSBUFSIZE (MAX_MODS * MAX_QPATH)

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods()
{
	char dirlist[MODSBUFSIZE];

	uiInfo.modCount = 0;

	const int numdirs = FS_GetFileList("$modlist", "", dirlist, sizeof dirlist);
	char* dirptr = dirlist;
	for (int i = 0; i < numdirs; i++)
	{
		const int dirlen = strlen(dirptr) + 1;
		const char* descptr = dirptr + dirlen;
		uiInfo.modList[uiInfo.modCount].mod_name = String_Alloc(dirptr);
		uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
		dirptr += dirlen + strlen(descptr) + 1;
		uiInfo.modCount++;
		if (uiInfo.modCount >= MAX_MODS)
		{
			break;
		}
	}
}

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .qvm file
================
*/
extern "C" Q_EXPORT intptr_t QDECL vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5,
	int arg6, int arg7, int arg8, int arg9, int arg10, int arg11)
{
	return 0;
}

/*
================
Text_PaintChar
================
*/
/*
static void Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader)
{
	float w, h;

	w = width * scale;
	h = height * scale;
	ui.R_DrawStretchPic((int)x, (int)y, w, h, s, t, s2, t2, hShader );	//make the coords (int) or else the chars bleed
}
*/

/*
================
Text_Paint
================
*/
// iMaxPixelWidth is 0 here for no limit (but gets converted to -1), else max printable pixel width relative to start pos
//
void Text_Paint(const float x, const float y, const float scale, vec4_t color, const char* text, const int iMaxPixelWidth, const int style,
	int iFontIndex)
{
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	// kludge.. convert JK2 menu styles to SOF2 printstring ctrl codes...
	//
	int iStyleOR = 0;
	switch (style)
	{
		//	case  ITEM_TEXTSTYLE_NORMAL:			iStyleOR = 0;break;					// JK2 normal text
		//	case  ITEM_TEXTSTYLE_BLINK:				iStyleOR = STYLE_BLINK;break;		// JK2 fast blinking
	case ITEM_TEXTSTYLE_PULSE: iStyleOR = STYLE_BLINK;
		break; // JK2 slow pulsing
	case ITEM_TEXTSTYLE_SHADOWED: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	case ITEM_TEXTSTYLE_OUTLINED: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	case ITEM_TEXTSTYLE_OUTLINESHADOWED: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	case ITEM_TEXTSTYLE_SHADOWEDMORE: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	default:;
	}

	ui.R_Font_DrawString(x, // int ox
		y, // int oy
		text, // const char *text
		color, // paletteRGBA_c c
		iStyleOR | iFontIndex, // const int iFontHandle
		!iMaxPixelWidth ? -1 : iMaxPixelWidth, // iMaxPixelWidth (-1 = none)
		scale // const float scale = 1.0f
	);
}

/*
================
Text_PaintWithCursor
================
*/
// iMaxPixelWidth is 0 here for no-limit
static void Text_PaintWithCursor(const float x, const float y, const float scale, vec4_t color, const char* text, const int cursorPos, const char cursor,
	const int iMaxPixelWidth, const int style, const int iFontIndex)
{
	Text_Paint(x, y, scale, color, text, iMaxPixelWidth, style, iFontIndex);

	// now print the cursor as well...
	//
	char sTemp[1024];
	int iCopyCount = iMaxPixelWidth > 0 ? Q_min((int)strlen(text), iMaxPixelWidth) : static_cast<int>(strlen(text));
	iCopyCount = Q_min(iCopyCount, cursorPos);
	iCopyCount = Q_min(iCopyCount, (int)sizeof(sTemp));

	// copy text into temp buffer for pixel measure...
	//
	strncpy(sTemp, text, iCopyCount);
	sTemp[iCopyCount] = '\0';

	const int iNextXpos = ui.R_Font_StrLenPixels(sTemp, iFontIndex, scale);

	Text_Paint(x + iNextXpos, y, scale, color, va("%c", cursor), iMaxPixelWidth, style | ITEM_TEXTSTYLE_BLINK,
		iFontIndex);
}

static const char* UI_FeederItemText(const float feederID, const int index, const int column, qhandle_t* handle)
{
	*handle = -1;

	if (feederID == FEEDER_SAVEGAMES)
	{
		if (column == 0)
		{
			return s_savedata[index].currentSaveFileComments;
		}
		return s_savedata[index].currentSaveFileDateTimeString;
	}
	if (feederID == FEEDER_MOVES)
	{
		return datapadMoveData[uiInfo.movesTitleIndex][index].title;
	}
	if (feederID == FEEDER_MOVES_TITLES)
	{
		return datapadMoveTitleData[index];
	}
	if (feederID == FEEDER_PLAYER_SPECIES)
	{
		if (index >= 0 && index < uiInfo.playerSpeciesCount)
		{
			return uiInfo.playerSpecies[index].Name;
		}
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
#ifdef JK2_MODE
		// FIXME
		return nullptr;
#else
		return SE_GetLanguageName(index);
#endif
	}
	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].
				name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name;
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].
				name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name;
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name;
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			*handle = ui.R_RegisterShaderNoMip(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader;
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		if (index >= 0 && index < uiInfo.modCount)
		{
			if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr)
			{
				return uiInfo.modList[index].modDescr;
			}
			return uiInfo.modList[index].mod_name;
		}
	}

	return "";
}

static qhandle_t UI_FeederItemImage(const float feederID, const int index)
{
	if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name));
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name));
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name));
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			return ui.R_RegisterShaderNoMip(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
		}
	}
	/*	else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS)
		{
			int actual;
			UI_SelectedMap(index, &actual);
			index = actual;
			if (index >= 0 && index < uiInfo.mapCount)
			{
				if (uiInfo.mapList[index].levelShot == -1)
				{
					uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
				}
				return uiInfo.mapList[index].levelShot;
			}
		}
	*/
	return 0;
}

/*
=================
CreateNextSaveName
=================
*/
static int CreateNextSaveName(char* fileName)
{
	// Loop through all the save games and look for the first open name
	for (int i = 0; i < MAX_SAVELOADFILES; i++)
	{
#ifdef JK2_MODE
		Com_sprintf(fileName, MAX_SAVELOADNAME, "jkii%02d", i);
#else
		Com_sprintf(fileName, MAX_SAVELOADNAME, "jedi_%02d", i);
#endif

		if (!ui.SG_GetSaveGameComment(fileName, nullptr, nullptr))
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
===============
UI_DeferMenuScript

Return true if the menu script should be deferred for later
===============
*/
static qboolean UI_DeferMenuScript(const char** args)
{
	const char* name;

	// Whats the reason for being deferred?
	if (!String_Parse(args, &name))
	{
		return qfalse;
	}

	// Handle the custom cases
	if (!Q_stricmp(name, "VideoSetup"))
	{
		const char* warningMenuName;

		// No warning menu specified
		if (!String_Parse(args, &warningMenuName))
		{
			return qfalse;
		}

		// Defer if the video options were modified
		const qboolean deferred = Cvar_VariableIntegerValue("ui_r_modified") ? qtrue : qfalse;

		if (deferred)
		{
			// Open the warning menu
			Menus_OpenByName(warningMenuName);
		}

		return deferred;
	}

	return qfalse;
}

/*
===============
UI_RunMenuScript
===============
*/
static qboolean UI_RunMenuScript(const char** args)
{
	const char* name, * name2, * mapName, * menuName, * warningMenuName;

	if (String_Parse(args, &name))
	{
		if (Q_stricmp(name, "resetdefaults") == 0)
		{
			UI_ResetDefaults();
		}
		else if (Q_stricmp(name, "saveControls") == 0)
		{
			Controls_SetConfig();
		}
		else if (Q_stricmp(name, "loadControls") == 0)
		{
			Controls_GetConfig();
		}
		else if (Q_stricmp(name, "clearError") == 0)
		{
			Cvar_Set("com_errorMessage", "");
		}
		else if (Q_stricmp(name, "ReadSaveDirectory") == 0)
		{
			s_savegame.saveFileCnt = -1; //force a refresh at drawtime
			//			ReadSaveDirectory();
		}
		else if (Q_stricmp(name, "loadAuto") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "load auto\n"); //load game menu
		}
		else if (Q_stricmp(name, "loadgame") == 0)
		{
			if (s_savedata[s_savegame.currentLine].currentSaveFileName) // && (*s_file_desc_field.field.buffer))
			{
				Menus_CloseAll();
				ui.Cmd_ExecuteText(EXEC_APPEND,
					va("load %s\n", s_savedata[s_savegame.currentLine].currentSaveFileName));
			}
			// after loading a game, the list box (and it's highlight) get's reset back to 0, but currentLine sticks around, so set it to 0 here
			s_savegame.currentLine = 0;
		}
		else if (Q_stricmp(name, "deletegame") == 0)
		{
			if (s_savedata[s_savegame.currentLine].currentSaveFileName) // A line was chosen
			{
#ifndef FINAL_BUILD
				ui.Printf(va("%s\n", "Attempting to delete game"));
#endif

				ui.Cmd_ExecuteText(EXEC_NOW, va("wipe %s\n", s_savedata[s_savegame.currentLine].currentSaveFileName));

				if (s_savegame.currentLine > 0 && s_savegame.currentLine + 1 == s_savegame.saveFileCnt)
				{
					s_savegame.currentLine--;
					// yeah this is a pretty bad hack
					// adjust cursor position of listbox so correct item is highlighted
					UI_AdjustSaveGameListBox(s_savegame.currentLine);
				}

				//				ReadSaveDirectory();	//refresh
				s_savegame.saveFileCnt = -1; //force a refresh at drawtime
			}
		}
		else if (Q_stricmp(name, "savegame") == 0)
		{
			char fileName[MAX_SAVELOADNAME];
			char description[64];
			// Create a new save game
			//			if ( !s_savedata[s_savegame.currentLine].currentSaveFileName)	// No line was chosen
			{
				CreateNextSaveName(fileName); // Get a name to save to
			}
			//			else	// Overwrite a current save game? Ask first.
			{
				//				s_savegame.yes.generic.flags	= QMF_HIGHLIGHT_IF_FOCUS;
				//				s_savegame.no.generic.flags		= QMF_HIGHLIGHT_IF_FOCUS;

				//				strcpy(fileName,s_savedata[s_savegame.currentLine].currentSaveFileName);
				//				s_savegame.awaitingSave = qtrue;
				//				s_savegame.deletegame.generic.flags	= QMF_GRAYED;	// Turn off delete button
				//				break;
			}

			// Save description line
			ui.Cvar_VariableStringBuffer("ui_gameDesc", description, sizeof description);
			ui.SG_StoreSaveGameComment(description);

			ui.Cmd_ExecuteText(EXEC_APPEND, va("save %s\n", fileName));
			s_savegame.saveFileCnt = -1; //force a refresh the next time around
		}
		else if (Q_stricmp(name, "LoadMods") == 0)
		{
			UI_LoadMods();
		}
		else if (Q_stricmp(name, "RunMod") == 0)
		{
			if (uiInfo.modList[uiInfo.modIndex].mod_name)
			{
				Cvar_Set("fs_game", uiInfo.modList[uiInfo.modIndex].mod_name);
				FS_Restart();
				Cbuf_ExecuteText(EXEC_APPEND, "vid_restart;");
			}
		}
		else if (Q_stricmp(name, "Quit") == 0)
		{
			Cbuf_ExecuteText(EXEC_NOW, "quit");
		}
		else if (Q_stricmp(name, "Controls") == 0)
		{
			Cvar_Set("cl_paused", "1");
			trap_Key_SetCatcher(KEYCATCH_UI);
			Menus_CloseAll();
			Menus_ActivateByName("setup_menu2");
		}
		else if (Q_stricmp(name, "Leave") == 0)
		{
			Cbuf_ExecuteText(EXEC_APPEND, "disconnect\n");
			trap_Key_SetCatcher(KEYCATCH_UI);
			Menus_CloseAll();
		}
		else if (Q_stricmp(name, "getvideosetup") == 0)
		{
			UI_GetVideoSetup();
		}
		else if (Q_stricmp(name, "updatevideosetup") == 0)
		{
			UI_UpdateVideoSetup();
		}
		else if (Q_stricmp(name, "nextDataPadForcePower") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpforcenext\n");
		}
		else if (Q_stricmp(name, "prevDataPadForcePower") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpforceprev\n");
		}
		else if (Q_stricmp(name, "nextDataPadWeapon") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpweapnext\n");
		}
		else if (Q_stricmp(name, "prevDataPadWeapon") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpweapprev\n");
		}
		else if (Q_stricmp(name, "nextDataPadInventory") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpinvnext\n");
		}
		else if (Q_stricmp(name, "prevDataPadInventory") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpinvprev\n");
		}
		else if (Q_stricmp(name, "checkvid1data") == 0) // Warn user data has changed before leaving screen?
		{
			String_Parse(args, &menuName);

			String_Parse(args, &warningMenuName);

			UI_CheckVid1Data(menuName, warningMenuName);
		}
		else if (Q_stricmp(name, "startgame") == 0)
		{
			Menus_CloseAll();
#ifdef JK2_MODE
			ui.Cmd_ExecuteText(EXEC_APPEND, "map kejim_post\n");
#else
			if (Cvar_VariableIntegerValue("com_demo"))
			{
				ui.Cmd_ExecuteText(EXEC_APPEND, "map demo\n");
			}
			else
			{
				ui.Cmd_ExecuteText(EXEC_APPEND, "map yavin1\n");
			}
#endif
		}
		else if (Q_stricmp(name, "startoutcast") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map kejim_post\n");
		}
		else if (Q_stricmp(name, "startdemo") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map jodemo\n");
		}
		else if (Q_stricmp(name, "startdarkforces") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map secbase\n");
		}
		else if (Q_stricmp(name, "startdarkforces2") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map 01nar\n");
		}
		else if (Q_stricmp(name, "startdeception") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map yavindec\n");
		}
		else if (Q_stricmp(name, "starthoth") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map imphoth_a\n");
		}
		else if (Q_stricmp(name, "startkessel") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map kessel\n");
		}
		else if (Q_stricmp(name, "starttournament") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map tournament_ruins\n");
		}
		else if (Q_stricmp(name, "startprivateer") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map intro\n");
		}
		else if (Q_stricmp(name, "startredemption") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map redemption1\n");
		}
		else if (Q_stricmp(name, "startrendar") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map drlevel1\n");
		}
		else if (Q_stricmp(name, "startvalley") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map sith_valley\n");
		}
		else if (Q_stricmp(name, "starttatooine") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map cot\n");
		}
		else if (Q_stricmp(name, "startvengance") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map part_1\n");
		}
		else if (Q_stricmp(name, "startpuzzle") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map puzzler\n");
		}
		else if (Q_stricmp(name, "startnina") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map nina01\n");
		}
		else if (Q_stricmp(name, "startyav") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map level0\n");
		}
		else if (Q_stricmp(name, "startmap") == 0)
		{
			Menus_CloseAll();

			String_Parse(args, &mapName);

			ui.Cmd_ExecuteText(EXEC_APPEND, va("maptransition %s\n", mapName));
		}
		else if (Q_stricmp(name, "closeingame") == 0)
		{
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Cvar_Set("cl_paused", "0");
			Menus_CloseAll();

			if (1 == Cvar_VariableIntegerValue("ui_missionfailed"))
			{
				Menus_ActivateByName("missionfailed_menu");
				ui.Key_SetCatcher(KEYCATCH_UI);
			}
			else
			{
				Menus_ActivateByName("mainhud");
			}
		}
		else if (Q_stricmp(name, "closedatapad") == 0)
		{
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Cvar_Set("cl_paused", "0");
			Menus_CloseAll();
			Menus_ActivateByName("mainhud");

			Cvar_Set("cg_updatedDataPadForcePower1", "0");
			Cvar_Set("cg_updatedDataPadForcePower2", "0");
			Cvar_Set("cg_updatedDataPadForcePower3", "0");
			Cvar_Set("cg_updatedDataPadObjective", "0");
		}
		else if (Q_stricmp(name, "closesabermenu") == 0)
		{
			// if we're in the saber menu when creating a character, close this down
			if (!Cvar_VariableIntegerValue("saber_menu"))
			{
				Menus_CloseByName("saberMenu");
				Menus_OpenByName("characterMenu");
			}
		}
		else if (Q_stricmp(name, "clearmouseover") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();

			if (menu)
			{
				const char* itemName;
				String_Parse(args, &itemName);
				const auto item = Menu_FindItemByName(menu, itemName);
				if (item)
				{
					item->window.flags &= ~WINDOW_MOUSEOVER;
				}
			}
		}
		else if (Q_stricmp(name, "setMovesListDefault") == 0)
		{
			uiInfo.movesTitleIndex = 2;
		}
		else if (Q_stricmp(name, "resetMovesDesc") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();

			if (menu)
			{
				const auto item = Menu_FindItemByName(menu, "item_desc");
				if (item)
				{
					const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
					if (listPtr)
					{
						listPtr->cursorPos = 0;
						listPtr->startPos = 0;
					}
					item->cursorPos = 0;
				}
			}
		}
		else if (Q_stricmp(name, "resetMovesList") == 0)
		{
			const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");
			//update saber models
			if (menu)
			{
				const auto item = Menu_FindItemByName(menu, "character");
				if (item)
				{
					UI_SaberAttachToChar(item);
				}
			}

			Cvar_Set("ui_move_desc", " ");
		}
		else if (Q_stricmp(name, "setMoveCharacter") == 0)
		{
			UI_GetCharacterCvars();
			UI_GetSaberCvars();

			uiInfo.movesTitleIndex = 0;

			const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");

			if (menu)
			{
				const auto item = Menu_FindItemByName(menu, "character");
				if (item)
				{
					const modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);
					if (modelPtr)
					{
						char skin[MAX_QPATH];
						uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
						ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);

						uiInfo.moveAnimTime = 0;
						DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue);
						Com_sprintf(skin, sizeof skin, "models/players/%s/|%s|%s|%s",
							Cvar_VariableString("g_char_model"),
							Cvar_VariableString("g_char_skin_head"),
							Cvar_VariableString("g_char_skin_torso"),
							Cvar_VariableString("g_char_skin_legs")
						);

						ItemParse_model_g2skin_go(item, skin);
						UI_SaberAttachToChar(item);
					}
				}
			}
		}
		else if (Q_stricmp(name, "glCustom") == 0)
		{
			Cvar_Set("ui_r_glCustom", "4");
		}
		else if (Q_stricmp(name, "character") == 0)
		{
			UI_UpdateCharacter(qfalse);
		}
		else if (Q_stricmp(name, "characterchanged") == 0)
		{
			UI_UpdateCharacter(qtrue);
		}
		else if (Q_stricmp(name, "char_skin") == 0)
		{
			UI_UpdateCharacterSkin();
		}
		else if (Q_stricmp(name, "saber_type") == 0)
		{
			UI_UpdateSaberType();
		}
		else if (Q_stricmp(name, "saber_hilt") == 0)
		{
			UI_UpdateSaberHilt(qfalse);
		}
		else if (Q_stricmp(name, "saber_color") == 0)
		{
			//			UI_UpdateSaberColor( qfalse );
		}
		else if (Q_stricmp(name, "saber2_hilt") == 0)
		{
			UI_UpdateSaberHilt(qtrue);
		}
		else if (Q_stricmp(name, "saber2_color") == 0)
		{
			//			UI_UpdateSaberColor( qtrue );
		}
		else if (Q_stricmp(name, "updatecharcvars") == 0)
		{
			UI_UpdateCharacterCvars();
		}
		else if (Q_stricmp(name, "getcharcvars") == 0)
		{
			UI_GetCharacterCvars();
		}
		else if (Q_stricmp(name, "updatesabercvars") == 0)
		{
			UI_UpdateSaberCvars();
		}
		else if (Q_stricmp(name, "getsabercvars") == 0)
		{
			UI_GetSaberCvars();
		}
		else if (Q_stricmp(name, "resetsabercvardefaults") == 0)
		{
			// NOTE : ONLY do this if saber menu is set properly (ie. first time we enter this menu)
			if (!Cvar_VariableIntegerValue("saber_menu"))
			{
				UI_ResetSaberCvars();
			}
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "updatefightingstylechoices") == 0)
		{
			UI_UpdateFightingStyleChoices();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "initallocforcepower") == 0)
		{
			const char* forceName;
			String_Parse(args, &forceName);

			UI_InitAllocForcePowers(forceName);
		}
		else if (Q_stricmp(name, "affectforcepowerlevel") == 0)
		{
			const char* forceName;
			String_Parse(args, &forceName);

			UI_AffectForcePowerLevel(forceName);
		}
		else if (Q_stricmp(name, "decrementcurrentforcepower") == 0)
		{
			UI_DecrementCurrentForcePower();
		}
		else if (Q_stricmp(name, "shutdownforcehelp") == 0)
		{
			UI_ShutdownForceHelp();
		}
		else if (Q_stricmp(name, "forcehelpactive") == 0)
		{
			UI_ForceHelpActive();
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "demosetforcelevels") == 0)
		{
			UI_DemoSetForceLevels();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "recordforcelevels") == 0)
		{
			UI_RecordForceLevels();
		}
		else if (Q_stricmp(name, "recordweapons") == 0)
		{
			UI_RecordWeapons();
		}
		else if (Q_stricmp(name, "showforceleveldesc") == 0)
		{
			const char* forceName;
			String_Parse(args, &forceName);

			UI_ShowForceLevelDesc(forceName);
		}
		else if (Q_stricmp(name, "resetforcelevels") == 0)
		{
			UI_ResetForceLevels();
		}
		else if (Q_stricmp(name, "weaponhelpactive") == 0)
		{
			UI_WeaponHelpActive();
		}
		// initialize weapon selection screen
		else if (Q_stricmp(name, "initweaponselect") == 0)
		{
			UI_InitWeaponSelect();
		}
		else if (Q_stricmp(name, "clearweapons") == 0)
		{
			UI_ClearWeapons();
		}
		else if (Q_stricmp(name, "clearsabers") == 0)
		{
			UI_clearsabers();
		}
		else if (Q_stricmp(name, "stopgamesounds") == 0)
		{
			trap_S_StopSounds();
		}
		else if (Q_stricmp(name, "loadmissionselectmenu") == 0)
		{
			const char* cvarName;
			String_Parse(args, &cvarName);

			if (cvarName)
			{
				UI_LoadMissionSelectMenu(cvarName);
			}
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "calcforcestatus") == 0)
		{
			UI_CalcForceStatus();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "giveweapon") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			UI_GiveWeapon(atoi(weaponIndex));
		}
		else if (Q_stricmp(name, "giveammo") == 0)
		{
			const char* ammoIndex;
			String_Parse(args, &ammoIndex);

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);

			const char* soundfile = nullptr;
			String_Parse(args, &soundfile);

			UI_GiveAmmo(atoi(ammoIndex), atoi(ammoAmount), soundfile);
		}
		else if (Q_stricmp(name, "equipweapon") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			UI_EquipWeapon(atoi(weaponIndex));
		}
		else if (Q_stricmp(name, "addweaponselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char* ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char* itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char* litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char* backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char* soundfile = nullptr;
			String_Parse(args, &soundfile);

			UI_AddWeaponSelection(atoi(weaponIndex), atoi(ammoIndex), atoi(ammoAmount), itemName, litItemName,
				backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "addpistolselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char* ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char* itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char* litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char* backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char* soundfile = nullptr;
			String_Parse(args, &soundfile);

			UI_AddPistolSelection(atoi(weaponIndex), atoi(ammoIndex), atoi(ammoAmount), itemName, litItemName,
				backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "addthrowweaponselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char* ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char* itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char* litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char* backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char* soundfile;
			String_Parse(args, &soundfile);

			UI_AddThrowWeaponSelection(atoi(weaponIndex), atoi(ammoIndex), atoi(ammoAmount), itemName, litItemName,
				backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "removeweaponselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (weaponIndex)
			{
				UI_RemoveWeaponSelection(atoi(weaponIndex));
			}
		}
		else if (Q_stricmp(name, "removepistolselection") == 0)
		{
			UI_Removepistolselection();
		}
		else if (Q_stricmp(name, "removethrowweaponselection") == 0)
		{
			UI_RemoveThrowWeaponSelection();
		}
		else if (Q_stricmp(name, "normalthrowselection") == 0)
		{
			UI_NormalThrowSelection();
		}
		else if (Q_stricmp(name, "highlightthrowselection") == 0)
		{
			UI_HighLightThrowSelection();
		}
		else if (Q_stricmp(name, "normalweaponselection") == 0)
		{
			const char* slotIndex;
			String_Parse(args, &slotIndex);
			if (!slotIndex)
			{
				return qfalse;
			}

			UI_NormalWeaponSelection(atoi(slotIndex));
		}
		else if (Q_stricmp(name, "normalpistolselection") == 0)
		{
			UI_Normalpistolselection();
		}
		else if (Q_stricmp(name, "highlightweaponselection") == 0)
		{
			const char* slotIndex;
			String_Parse(args, &slotIndex);
			if (!slotIndex)
			{
				return qfalse;
			}

			UI_HighLightWeaponSelection(atoi(slotIndex));
		}
		else if (Q_stricmp(name, "highlightpistolselection") == 0)
		{
			UI_Highlightpistolselection();
		}
		else if (Q_stricmp(name, "clearinventory") == 0)
		{
			UI_ClearInventory();
		}
		else if (Q_stricmp(name, "giveinventory") == 0)
		{
			const char* inventoryIndex, * amount;
			String_Parse(args, &inventoryIndex);
			String_Parse(args, &amount);
			UI_GiveInventory(atoi(inventoryIndex), atoi(amount));
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "updatefightingstyle") == 0)
		{
			UI_UpdateFightingStyle();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "update") == 0)
		{
			if (String_Parse(args, &name2))
			{
				UI_Update(name2);
			}
			else
			{
				Com_Printf("update missing cmd\n");
			}
		}
		else if (Q_stricmp(name, "load_quick") == 0)
		{
#ifdef JK2_MODE
			ui.Cmd_ExecuteText(EXEC_APPEND, "load quik\n");
#else
			ui.Cmd_ExecuteText(EXEC_APPEND, "load quick\n");
#endif
		}
		else if (Q_stricmp(name, "load_auto") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "load *respawn\n");
			//death menu, might load a saved game instead if they just loaded on this map
		}
		else if (Q_stricmp(name, "decrementforcepowerlevel") == 0)
		{
			UI_DecrementForcePowerLevel();
		}
		else if (Q_stricmp(name, "getmousepitch") == 0)
		{
			Cvar_Set("ui_mousePitch", trap_Cvar_VariableValue("m_pitch") >= 0 ? "0" : "1");
		}
		else if (Q_stricmp(name, "resetcharacterlistboxes") == 0)
		{
			UI_ResetCharacterListBoxes();
		}
		else if (Q_stricmp(name, "LaunchMP") == 0)
		{
			// TODO for MAC_PORT, will only be valid for non-JK2 mode
		}
		else
		{
			Com_Printf("unknown UI script %s\n", name);
		}
	}

	return qtrue;
}

/*
=================
UI_GetValue
=================
*/
static float UI_GetValue(int ownerDraw)
{
	return 0;
}

//Force Warnings
enum
{
	FW_VERY_LIGHT = 0,
	FW_SEMI_LIGHT,
	FW_NEUTRAL,
	FW_SEMI_DARK,
	FW_VERY_DARK
};

const char* lukeForceStatusSounds[] =
{
	"sound/chars/luke/misc/MLUK_03.mp3", // Very Light
	"sound/chars/luke/misc/MLUK_04.mp3", // Semi Light
	"sound/chars/luke/misc/MLUK_05.mp3", // Neutral
	"sound/chars/luke/misc/MLUK_01.mp3", // Semi dark
	"sound/chars/luke/misc/MLUK_02.mp3", // Very dark
};

const char* kyleForceStatusSounds[] =
{
	"sound/chars/kyle/misc/MKYK_05.mp3", // Very Light
	"sound/chars/kyle/misc/MKYK_04.mp3", // Semi Light
	"sound/chars/kyle/misc/MKYK_03.mp3", // Neutral
	"sound/chars/kyle/misc/MKYK_01.mp3", // Semi dark
	"sound/chars/kyle/misc/MKYK_02.mp3", // Very dark
};

#ifndef JK2_MODE
static void UI_CalcForceStatus()
{
	short index;
	qboolean lukeFlag = qtrue;
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	char value[256];

	if (!cl)
	{
		return;
	}
	const playerState_t* pState = cl->gentity->client;

	if (!cl->gentity || !cl->gentity->client)
	{
		return;
	}

	memset(value, 0, sizeof value);

	const float lightSide = pState->forcePowerLevel[FP_HEAL] +
		pState->forcePowerLevel[FP_TELEPATHY] +
		pState->forcePowerLevel[FP_PROTECT] +
		pState->forcePowerLevel[FP_ABSORB] +
		pState->forcePowerLevel[FP_STASIS] +
		pState->forcePowerLevel[FP_GRASP] +
		pState->forcePowerLevel[FP_REPULSE] +
		pState->forcePowerLevel[FP_PROJECTION] +
		pState->forcePowerLevel[FP_BLAST];

	const float darkSide = pState->forcePowerLevel[FP_GRIP] +
		pState->forcePowerLevel[FP_LIGHTNING] +
		pState->forcePowerLevel[FP_RAGE] +
		pState->forcePowerLevel[FP_DRAIN] +
		pState->forcePowerLevel[FP_DESTRUCTION] +
		pState->forcePowerLevel[FP_LIGHTNING_STRIKE] +
		pState->forcePowerLevel[FP_FEAR] +
		pState->forcePowerLevel[FP_DEADLYSIGHT];

	const float total = lightSide + darkSide;

	const float percent = lightSide / total;

	const short who = Q_irand(0, 100);
	if (percent >= 0.90f) //  90 - 100%
	{
		index = FW_VERY_LIGHT;
		if (who < 50)
		{
			strcpy(value, "vlk"); // Very light Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "vll"); // Very light Luke
		}
	}
	else if (percent > 0.60f)
	{
		index = FW_SEMI_LIGHT;
		if (who < 50)
		{
			strcpy(value, "slk"); // Semi-light Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "sll"); // Semi-light light Luke
		}
	}
	else if (percent > 0.40f)
	{
		index = FW_NEUTRAL;
		if (who < 50)
		{
			strcpy(value, "ntk"); // Neutral Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "ntl"); // Netural Luke
		}
	}
	else if (percent > 0.10f)
	{
		index = FW_SEMI_DARK;
		if (who < 50)
		{
			strcpy(value, "sdk"); // Semi-dark Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "sdl"); // Semi-Dark Luke
		}
	}
	else
	{
		index = FW_VERY_DARK;
		if (who < 50)
		{
			strcpy(value, "vdk"); // Very dark Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "vdl"); // Very Dark Luke
		}
	}

	Cvar_Set("ui_forcestatus", value);

	if (lukeFlag)
	{
		DC->startLocalSound(DC->registerSound(lukeForceStatusSounds[index], qfalse), CHAN_VOICE);
	}
	else
	{
		DC->startLocalSound(DC->registerSound(kyleForceStatusSounds[index], qfalse), CHAN_VOICE);
	}
}
#endif // !JK2_MODE

/*
=================
UI_StopCinematic
=================
*/
static void UI_StopCinematic(int handle)
{
	if (handle >= 0)
	{
		trap_CIN_StopCinematic(handle);
	}
	else
	{
		handle = abs(handle);
		if (handle == UI_MAPCINEMATIC)
		{
			// FIXME - BOB do we need this?
			//			if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0)
			//			{
			//				trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
			//				uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
			//			}
		}
		else if (handle == UI_NETMAPCINEMATIC)
		{
			// FIXME - BOB do we need this?
			//			if (uiInfo.serverStatus.currentServerCinematic >= 0)
			//			{
			//				trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
			//				uiInfo.serverStatus.currentServerCinematic = -1;
			//			}
		}
		else if (handle == UI_CLANCINEMATIC)
		{
			// FIXME - BOB do we need this?
			//			int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
			//			if (i >= 0 && i < uiInfo.teamCount)
			//			{
			//				if (uiInfo.teamList[i].cinematic >= 0)
			//				{
			//					trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
			//					uiInfo.teamList[i].cinematic = -1;
			//				}
			//			}
		}
	}
}

static void UI_HandleLoadSelection()
{
	Cvar_Set("ui_SelectionOK", va("%d", s_savegame.currentLine < s_savegame.saveFileCnt));
	if (s_savegame.currentLine >= s_savegame.saveFileCnt)
		return;
#ifdef JK2_MODE
	Cvar_Set("ui_gameDesc", s_savedata[s_savegame.currentLine].currentSaveFileComments);	// set comment

	if (!ui.SG_GetSaveImage(s_savedata[s_savegame.currentLine].currentSaveFileName, &screenShotBuf))
	{
		memset(screenShotBuf, 0, (SG_SCR_WIDTH * SG_SCR_HEIGHT * 4));
	}
#endif
}

/*
=================
UI_FeederCount
=================
*/
static int UI_FeederCount(const float feederID)
{
	if (feederID == FEEDER_SAVEGAMES)
	{
		if (s_savegame.saveFileCnt == -1)
		{
			ReadSaveDirectory(); //refresh
			UI_HandleLoadSelection();
#ifndef JK2_MODE
			UI_AdjustSaveGameListBox(s_savegame.currentLine);
#endif
		}
		return s_savegame.saveFileCnt;
	}
	// count number of moves for the current title
	if (feederID == FEEDER_MOVES)
	{
		int count = 0;

		for (int i = 0; i < MAX_MOVES; i++)
		{
			if (datapadMoveData[uiInfo.movesTitleIndex][i].title)
			{
				count++;
			}
		}

		return count;
	}
	if (feederID == FEEDER_MOVES_TITLES)
	{
		return MD_MOVE_TITLE_MAX;
	}
	if (feederID == FEEDER_MODS)
	{
		return uiInfo.modCount;
	}
	if (feederID == FEEDER_LANGUAGES)
	{
		return uiInfo.languageCount;
	}
	if (feederID == FEEDER_PLAYER_SPECIES)
	{
		return uiInfo.playerSpeciesCount;
	}
	if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount;
	}
	if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount;
	}
	if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount;
	}
	if (feederID == FEEDER_COLORCHOICES)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount;
	}

	return 0;
}

/*
=================
UI_FeederSelection
=================
*/
static void UI_FeederSelection(const float feederID, const int index, itemDef_t* item)
{
	if (feederID == FEEDER_SAVEGAMES)
	{
		s_savegame.currentLine = index;
		UI_HandleLoadSelection();
	}
	else if (feederID == FEEDER_MOVES)
	{
		const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");

		if (menu)
		{
			const auto item_def_s = Menu_FindItemByName(menu, "character");
			if (item_def_s)
			{
				const modelDef_t* modelPtr = static_cast<modelDef_t*>(item_def_s->typeData);
				if (modelPtr)
				{
					if (datapadMoveData[uiInfo.movesTitleIndex][index].anim)
					{
						char skin[MAX_QPATH];
						ItemParse_model_g2anim_go(item_def_s, datapadMoveData[uiInfo.movesTitleIndex][index].anim);
						uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item_def_s->ghoul2[0], "model_root",
							modelPtr->g2anim, qtrue);

						uiInfo.moveAnimTime += uiInfo.uiDC.realTime;

						// Play sound for anim
						if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_FORCE_JUMP)
						{
							DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveJumpSound, CHAN_LOCAL);
						}
						else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_ROLL)
						{
							DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveRollSound, CHAN_LOCAL);
						}
						else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_SABER)
						{
							// Randomly choose one sound
							const int soundI = Q_irand(1, 6);
							const sfxHandle_t* soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound1;
							if (soundI == 2)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound2;
							}
							else if (soundI == 3)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound3;
							}
							else if (soundI == 4)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound4;
							}
							else if (soundI == 5)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound5;
							}
							else if (soundI == 6)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound6;
							}

							DC->startLocalSound(*soundPtr, CHAN_LOCAL);
						}

						if (datapadMoveData[uiInfo.movesTitleIndex][index].desc)
						{
							Cvar_Set("ui_move_desc", datapadMoveData[uiInfo.movesTitleIndex][index].desc);
						}

						Com_sprintf(skin, sizeof skin, "models/players/%s/|%s|%s|%s",
							Cvar_VariableString("g_char_model"),
							Cvar_VariableString("g_char_skin_head"),
							Cvar_VariableString("g_char_skin_torso"),
							Cvar_VariableString("g_char_skin_legs")
						);

						ItemParse_model_g2skin_go(item_def_s, skin);
					}
				}
			}
		}
	}
	else if (feederID == FEEDER_MOVES_TITLES)
	{
		uiInfo.movesTitleIndex = index;
		uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
		const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");

		if (menu)
		{
			const auto item_def_s = Menu_FindItemByName(menu, "character");
			if (item_def_s)
			{
				const modelDef_t* modelPtr = static_cast<modelDef_t*>(item_def_s->typeData);
				if (modelPtr)
				{
					ItemParse_model_g2anim_go(item_def_s, uiInfo.movesBaseAnim);
					uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item_def_s->ghoul2[0], "model_root", modelPtr->g2anim,
						qtrue);
				}
			}
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		uiInfo.modIndex = index;
	}
	else if (feederID == FEEDER_PLAYER_SPECIES)
	{
		if (index >= 0 && index < uiInfo.playerSpeciesCount)
		{
			uiInfo.playerSpeciesIndex = index;
		}
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
		uiInfo.languageCountIndex = index;
	}
	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name);
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			Cvar_Set("ui_char_skin_torso", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name);
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			Cvar_Set("ui_char_skin_legs", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name);
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		extern void Item_RunScript(itemDef_t * item_def_item, const char* s); //from ui_shared;
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			Item_RunScript(item, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].actionText);
		}
	}
	/*	else if (feederID == FEEDER_CINEMATICS)
		{
			uiInfo.movieIndex = index;
			if (uiInfo.previewMovie >= 0)
			{
				trap_CIN_StopCinematic(uiInfo.previewMovie);
			}
			uiInfo.previewMovie = -1;
		}
		else if (feederID == FEEDER_DEMOS)
		{
			uiInfo.demoIndex = index;
		}
	*/
}

void Key_KeynumToStringBuf(int keynum, char* buf, int buflen);
void Key_GetBindingBuf(int keynum, char* buf, int buflen);

static qboolean UI_Crosshair_HandleKey(int flags, float* special, const int key)
{
	if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER)
	{
		if (key == A_MOUSE2)
		{
			uiInfo.currentCrosshair--;
		}
		else
		{
			uiInfo.currentCrosshair++;
		}

		if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS)
		{
			uiInfo.currentCrosshair = 0;
		}
		else if (uiInfo.currentCrosshair < 0)
		{
			uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
		}
		Cvar_Set("cg_drawCrosshair", va("%d", uiInfo.currentCrosshair));
		return qtrue;
	}
	return qfalse;
}

static qboolean UI_OwnerDrawHandleKey(const int ownerDraw, const int flags, float* special, const int key)
{
	switch (ownerDraw)
	{
	case UI_CROSSHAIR:
		UI_Crosshair_HandleKey(flags, special, key);
		break;
	default:
		break;
	}

	return qfalse;
}

//unfortunately we cannot rely on any game/cgame module code to do our animation stuff,
//because the ui can be loaded while the game/cgame are not loaded. So we're going to recreate what we need here.
#undef MAX_ANIM_FILES
#define MAX_ANIM_FILES 32

class ui_animFileSet_t
{
public:
	char filename[MAX_QPATH];
	animation_t animations[MAX_ANIMATIONS];
}; // ui_animFileSet_t
static ui_animFileSet_t ui_knownAnimFileSets[MAX_ANIM_FILES];

int ui_numKnownAnimFileSets;

static qboolean UI_ParseAnimationFile(const char* af_filename)
{
	const char* text_p;
	char text[80000];
	animation_t* animations = ui_knownAnimFileSets[ui_numKnownAnimFileSets].animations;

	const int len = re.GetAnimationCFG(af_filename, text, sizeof text);
	if (len <= 0)
	{
		return qfalse;
	}
	if (len >= static_cast<int>(sizeof text - 1))
	{
		Com_Error(ERR_FATAL, "UI_ParseAnimationFile: File %s too long\n (%d > %d)", af_filename, len, sizeof text - 1);
	}

	// parse the text
	text_p = text;

	//FIXME: have some way of playing anims backwards... negative numFrames?

	//initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
	for (int i = 0; i < MAX_ANIMATIONS; i++)
	{
		animations[i].firstFrame = 0;
		animations[i].numFrames = 0;
		animations[i].loopFrames = -1;
		animations[i].frameLerp = 100;
		//		animations[i].initialLerp = 100;
	}

	// read information for each frame
	COM_BeginParseSession();
	while (true)
	{
		const char* token = COM_Parse(&text_p);

		if (!token || !token[0])
		{
			break;
		}

		const int animNum = GetIDForString(anim_table, token);
		if (animNum == -1)
		{
			//#ifndef FINAL_BUILD
#ifdef _DEBUG
			if (strcmp(token, "ROOT"))
			{
				Com_Printf(S_COLOR_RED"WARNING: Unknown token %s in %s\n", token, af_filename);
			}
#endif
			while (token[0])
			{
				token = COM_ParseExt(&text_p, qfalse); //returns empty string when next token is EOL
			}
			continue;
		}

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		animations[animNum].firstFrame = atoi(token);

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		animations[animNum].numFrames = atoi(token);

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		animations[animNum].loopFrames = atoi(token);

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		float fps = atof(token);
		if (fps == 0)
		{
			fps = 1; //Don't allow divide by zero error
		}
		if (fps < 0)
		{
			//backwards
			animations[animNum].frameLerp = floor(1000.0f / fps);
		}
		else
		{
			animations[animNum].frameLerp = ceil(1000.0f / fps);
		}

		//		animations[animNum].initialLerp = ceil(1000.0f / fabs(fps));
	}
	COM_EndParseSession();

	return qtrue;
}

static qboolean UI_ParseAnimFileSet(const char* animCFG, int* animFileIndex)
{
	//Not going to bother parsing the sound config here.
	char afilename[MAX_QPATH];
	char strippedName[MAX_QPATH];
	int i;

	Q_strncpyz(strippedName, animCFG, sizeof strippedName);
	char* slash = strrchr(strippedName, '/');
	if (slash)
	{
		// truncate modelName to find just the dir not the extension
		*slash = 0;
	}

	//if this anims file was loaded before, don't parse it again, just point to the correct table of info
	for (i = 0; i < ui_numKnownAnimFileSets; i++)
	{
		if (Q_stricmp(ui_knownAnimFileSets[i].filename, strippedName) == 0)
		{
			*animFileIndex = i;
			return qtrue;
		}
	}

	if (ui_numKnownAnimFileSets == MAX_ANIM_FILES)
	{
		//TOO MANY!
		for (i = 0; i < MAX_ANIM_FILES; i++)
		{
			Com_Printf("animfile[%d]: %s\n", i, ui_knownAnimFileSets[i].filename);
		}
		Com_Error(ERR_FATAL, "UI_ParseAnimFileSet: %d == MAX_ANIM_FILES == %d", ui_numKnownAnimFileSets,
			MAX_ANIM_FILES);
	}

	//Okay, time to parse in a new one
	Q_strncpyz(ui_knownAnimFileSets[ui_numKnownAnimFileSets].filename, strippedName,
		sizeof ui_knownAnimFileSets[ui_numKnownAnimFileSets].filename);

	// Load and parse animations.cfg file
	Com_sprintf(afilename, sizeof afilename, "%s/animation.cfg", strippedName);
	if (!UI_ParseAnimationFile(afilename))
	{
		*animFileIndex = -1;
		return qfalse;
	}

	//set index and increment
	*animFileIndex = ui_numKnownAnimFileSets++;

	return qtrue;
}

static int UI_G2SetAnim(CGhoul2Info* ghlInfo, const char* boneName, const int animNum, const qboolean freeze)
{
	int animIndex;

	const char* GLAName = re.G2API_GetGLAName(ghlInfo);

	if (!GLAName || !GLAName[0])
	{
		return 0;
	}

	UI_ParseAnimFileSet(GLAName, &animIndex);

	if (animIndex != -1)
	{
		const animation_t* anim = &ui_knownAnimFileSets[animIndex].animations[animNum];
		if (anim->numFrames <= 0)
		{
			return 0;
		}
		const int sFrame = anim->firstFrame;
		const int eFrame = anim->firstFrame + anim->numFrames;
		int flags = BONE_ANIM_OVERRIDE;
		const int time = uiInfo.uiDC.realTime;
		const float animSpeed = 50.0f / anim->frameLerp;

		// Freeze anim if it's not looping, special hack for datapad moves menu
		if (freeze)
		{
			if (anim->loopFrames == -1)
			{
				flags = BONE_ANIM_OVERRIDE_FREEZE;
			}
			else
			{
				flags = BONE_ANIM_OVERRIDE_LOOP;
			}
		}
		else if (anim->loopFrames != -1)
		{
			flags = BONE_ANIM_OVERRIDE_LOOP;
		}
		flags |= BONE_ANIM_BLEND;
		constexpr int blend_time = 150;

		re.G2API_SetBoneAnim(ghlInfo, boneName, sFrame, eFrame, flags, animSpeed, time, -1, blend_time);

		return anim->frameLerp * (anim->numFrames - 2);
	}

	return 0;
}

static qboolean UI_ParseColorData(const char* buf, playerSpeciesInfo_t& species)
{
	const char* p;

	p = buf;
	COM_BeginParseSession();
	species.ColorCount = 0;
	species.ColorMax = 16;
	species.Color = static_cast<playerColor_t*>(malloc(species.ColorMax * sizeof(playerColor_t)));

	while (p)
	{
		const char* token = COM_ParseExt(&p, qtrue); //looking for the shader
		if (token[0] == 0)
		{
			COM_EndParseSession();
			return static_cast<qboolean>(species.ColorCount != 0);
		}

		if (species.ColorCount >= species.ColorMax)
		{
			species.ColorMax *= 2;
			species.Color = static_cast<playerColor_t*>(
				realloc(species.Color, species.ColorMax * sizeof(playerColor_t)));
		}

		memset(&species.Color[species.ColorCount], 0, sizeof(playerColor_t));

		Q_strncpyz(species.Color[species.ColorCount].shader, token, MAX_QPATH);

		token = COM_ParseExt(&p, qtrue); //looking for action block {
		if (token[0] != '{')
		{
			COM_EndParseSession();
			return qfalse;
		}

		token = COM_ParseExt(&p, qtrue); //looking for action commands
		while (token[0] != '}')
		{
			if (token[0] == 0)
			{
				//EOF
				COM_EndParseSession();
				return qfalse;
			}
			Q_strcat(species.Color[species.ColorCount].actionText, ACTION_BUFFER_SIZE, token);
			Q_strcat(species.Color[species.ColorCount].actionText, ACTION_BUFFER_SIZE, " ");
			token = COM_ParseExt(&p, qtrue); //looking for action commands or final }
		}
		species.ColorCount++; //next color please
	}
	COM_EndParseSession();
	return qtrue; //never get here
}

/*
=================
bIsImageFile
builds path and scans for valid image extentions
=================
*/
static qboolean IsImageFile(const char* dirptr, const char* skinname, const qboolean building)
{
	char fpath[MAX_QPATH];
	int f;

	Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.jpg", dirptr, skinname);
	ui.FS_FOpenFile(fpath, &f, FS_READ);
	if (!f)
	{
		//not there, try png
		Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.png", dirptr, skinname);
		ui.FS_FOpenFile(fpath, &f, FS_READ);
	}
	if (!f)
	{
		//not there, try tga
		Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.tga", dirptr, skinname);
		ui.FS_FOpenFile(fpath, &f, FS_READ);
	}
	if (f)
	{
		ui.FS_FCloseFile(f);
		if (building) ui.R_RegisterShaderNoMip(fpath);
		return qtrue;
	}

	return qfalse;
}

static void UI_FreeSpecies(playerSpeciesInfo_t* species)
{
	free(species->SkinHead);
	free(species->SkinTorso);
	free(species->SkinLeg);
	free(species->Color);
	memset(species, 0, sizeof(playerSpeciesInfo_t));
}

static void UI_FreeAllSpecies()
{
	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		UI_FreeSpecies(&uiInfo.playerSpecies[i]);
	}
	free(uiInfo.playerSpecies);

	uiInfo.playerSpeciesCount = 0;
	uiInfo.playerSpecies = nullptr;
}

/*
=================
PlayerModel_BuildList
=================
*/
static void UI_BuildPlayerModel_List(const qboolean inGameLoad)
{
	static constexpr size_t DIR_LIST_SIZE = 16384;

	size_t dirListSize = DIR_LIST_SIZE;
	char stackDirList[8192]{};
	int dirlen = 0;
	const int building = Cvar_VariableIntegerValue("com_buildscript");

	auto dirlist = static_cast<char*>(malloc(DIR_LIST_SIZE));
	if (!dirlist)
	{
		Com_Printf(S_COLOR_YELLOW "WARNING: Failed to allocate %u bytes of memory for player model "
			"directory list. Using stack allocated buffer of %u bytes instead.",
			DIR_LIST_SIZE, sizeof stackDirList);

		dirlist = stackDirList;
		dirListSize = sizeof stackDirList;
	}

	uiInfo.playerSpeciesCount = 0;
	uiInfo.playerSpeciesIndex = 0;
	uiInfo.playerSpeciesMax = 8;
	uiInfo.playerSpecies = static_cast<playerSpeciesInfo_t*>(malloc(
		uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t)));

	// iterate directory of all player models
	const int numdirs = ui.FS_GetFileList("models/players", "/", dirlist, dirListSize);
	char* dirptr = dirlist;
	for (int i = 0; i < numdirs; i++, dirptr += dirlen + 1)
	{
		int f = 0;
		char fpath[MAX_QPATH];

		dirlen = strlen(dirptr);

		if (dirlen)
		{
			if (dirptr[dirlen - 1] == '/')
				dirptr[dirlen - 1] = '\0';
		}
		else
		{
			continue;
		}

		if (strcmp(dirptr, ".") == 0 || strcmp(dirptr, "..") == 0)
			continue;

		Com_sprintf(fpath, sizeof fpath, "models/players/%s/PlayerChoice.txt", dirptr);
		int filelen = ui.FS_FOpenFile(fpath, &f, FS_READ);

		if (f)
		{
			char filelist[2048];

			std::vector<char> buffer(filelen + 1);
			ui.FS_Read(&buffer[0], filelen, f);
			ui.FS_FCloseFile(f);

			buffer[filelen] = 0;

			//record this species
			if (uiInfo.playerSpeciesCount >= uiInfo.playerSpeciesMax)
			{
				uiInfo.playerSpeciesMax *= 2;
				uiInfo.playerSpecies = static_cast<playerSpeciesInfo_t*>(realloc(
					uiInfo.playerSpecies, uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t)));
			}
			playerSpeciesInfo_t* species = &uiInfo.playerSpecies[uiInfo.playerSpeciesCount];
			memset(species, 0, sizeof(playerSpeciesInfo_t));
			Q_strncpyz(species->Name, dirptr, MAX_QPATH);

			if (!UI_ParseColorData(buffer.data(), *species))
			{
				ui.Printf("UI_BuildPlayerModel_List: Errors parsing '%s'\n", fpath);
			}

			species->SkinHeadMax = 8;
			species->SkinTorsoMax = 8;
			species->SkinLegMax = 8;

			species->SkinHead = static_cast<skinName_t*>(malloc(species->SkinHeadMax * sizeof(skinName_t)));
			species->SkinTorso = static_cast<skinName_t*>(malloc(species->SkinTorsoMax * sizeof(skinName_t)));
			species->SkinLeg = static_cast<skinName_t*>(malloc(species->SkinLegMax * sizeof(skinName_t)));

			int iSkinParts = 0;

			const int numfiles = ui.
				FS_GetFileList(va("models/players/%s", dirptr), ".skin", filelist, sizeof filelist);
			char* fileptr = filelist;
			for (int j = 0; j < numfiles; j++, fileptr += filelen + 1)
			{
				char skinname[64];
				if (building)
				{
					ui.FS_FOpenFile(va("models/players/%s/%s", dirptr, fileptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/animevents.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
				}

				filelen = strlen(fileptr);
				COM_StripExtension(fileptr, skinname, sizeof skinname);

				if (IsImageFile(dirptr, skinname, static_cast<qboolean>(building != 0)))
				{
					//if it exists
					if (Q_stricmpn(skinname, "head_", 5) == 0)
					{
						if (species->SkinHeadCount >= species->SkinHeadMax)
						{
							species->SkinHeadMax *= 2;
							species->SkinHead = static_cast<skinName_t*>(realloc(
								species->SkinHead, species->SkinHeadMax * sizeof(skinName_t)));
						}
						Q_strncpyz(species->SkinHead[species->SkinHeadCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 0;
					}
					else if (Q_stricmpn(skinname, "torso_", 6) == 0)
					{
						if (species->SkinTorsoCount >= species->SkinTorsoMax)
						{
							species->SkinTorsoMax *= 2;
							species->SkinTorso = static_cast<skinName_t*>(realloc(
								species->SkinTorso, species->SkinTorsoMax * sizeof(skinName_t)));
						}
						Q_strncpyz(species->SkinTorso[species->SkinTorsoCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 1;
					}
					else if (Q_stricmpn(skinname, "lower_", 6) == 0)
					{
						if (species->SkinLegCount >= species->SkinLegMax)
						{
							species->SkinLegMax *= 2;
							species->SkinLeg = static_cast<skinName_t*>(realloc(
								species->SkinLeg, species->SkinLegMax * sizeof(skinName_t)));
						}
						Q_strncpyz(species->SkinLeg[species->SkinLegCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 2;
					}
				}
			}
			if (iSkinParts < 7)
			{
				//didn't get a skin for each, then skip this model.
				UI_FreeSpecies(species);
				continue;
			}
			uiInfo.playerSpeciesCount++;

			if (ui_com_rend2.integer == 0) //rend2 is off
			{
				if (!inGameLoad && ui_PrecacheModels.integer)
				{
					CGhoul2Info_v ghoul2;
					Com_sprintf(fpath, sizeof fpath, "models/players/%s/model.glm", dirptr);
					const int g2Model = DC->g2_InitGhoul2Model(ghoul2, fpath, 0, 0, 0, 0, 0);
					if (g2Model >= 0)
					{
						DC->g2_RemoveGhoul2Model(ghoul2, 0);
					}
				}
			}
		}
	}

	if (dirlist != stackDirList)
	{
		free(dirlist);
	}
}

/*
================
UI_Shutdown
=================
*/
void UI_Shutdown()
{
	UI_FreeAllSpecies();
}

/*
=================
UI_Init
=================
*/
void _UI_Init(const qboolean inGameLoad)
{
	// Get the list of possible languages
#ifndef JK2_MODE
	uiInfo.languageCount = SE_GetNumLanguages(); // this does a dir scan, so use carefully
#else
	// sod it, parse every menu strip file until we find a gap in the sequence...
	//
	for (int i = 0; i < 10; i++)
	{
		if (!ui.SP_Register(va("menus%d", i), /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			break;
	}
#endif

	uiInfo.inGameLoad = inGameLoad;

	UI_RegisterCvars();

	UI_InitMemory();

	// cache redundant calulations
	trap_GetGlconfig(&uiInfo.uiDC.glconfig);

	// for 640x480 virtualized screen
	uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0 / 480.0);
	uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0 / 640.0);
	if (uiInfo.uiDC.glconfig.vidWidth * 480 > uiInfo.uiDC.glconfig.vidHeight * 640)
	{
		// wide screen
		uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - uiInfo.uiDC.glconfig.vidHeight * (640.0 / 480.0));
	}
	else
	{
		// no wide screen
		uiInfo.uiDC.bias = 0;
	}

	Init_Display(&uiInfo.uiDC);

	uiInfo.uiDC.drawText = &Text_Paint;
	uiInfo.uiDC.drawHandlePic = &UI_DrawHandlePic;
	uiInfo.uiDC.drawRect = &_UI_DrawRect;
	uiInfo.uiDC.drawSides = &_UI_DrawSides;
	uiInfo.uiDC.drawTextWithCursor = &Text_PaintWithCursor;
	uiInfo.uiDC.executeText = &Cbuf_ExecuteText;
	uiInfo.uiDC.drawTopBottom = &_UI_DrawTopBottom;
	uiInfo.uiDC.feederCount = &UI_FeederCount;
	uiInfo.uiDC.feederSelection = &UI_FeederSelection;
	uiInfo.uiDC.fillRect = &UI_FillRect;
	uiInfo.uiDC.getBindingBuf = &Key_GetBindingBuf;
	uiInfo.uiDC.getCVarString = Cvar_VariableStringBuffer;
	uiInfo.uiDC.getCVarValue = trap_Cvar_VariableValue;
	uiInfo.uiDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
	uiInfo.uiDC.getValue = &UI_GetValue;
	uiInfo.uiDC.keynumToStringBuf = &Key_KeynumToStringBuf;
	uiInfo.uiDC.modelBounds = &trap_R_ModelBounds;
	uiInfo.uiDC.ownerDrawVisible = &UI_OwnerDrawVisible;
	uiInfo.uiDC.ownerDrawWidth = &UI_OwnerDrawWidth;
	uiInfo.uiDC.ownerDrawItem = &UI_OwnerDraw;
	uiInfo.uiDC.Print = &Com_Printf;
	uiInfo.uiDC.registerSound = &trap_S_RegisterSound;
	uiInfo.uiDC.registerModel = ui.R_RegisterModel;
	uiInfo.uiDC.clearScene = &trap_R_ClearScene;
	uiInfo.uiDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
	uiInfo.uiDC.renderScene = &trap_R_RenderScene;
	uiInfo.uiDC.runScript = &UI_RunMenuScript;
	uiInfo.uiDC.deferScript = &UI_DeferMenuScript;
	uiInfo.uiDC.setBinding = &trap_Key_SetBinding;
	uiInfo.uiDC.setColor = &UI_SetColor;
	uiInfo.uiDC.setCVar = Cvar_Set;
	uiInfo.uiDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
	uiInfo.uiDC.startLocalSound = &trap_S_StartLocalSound;
	uiInfo.uiDC.stopCinematic = &UI_StopCinematic;
	uiInfo.uiDC.textHeight = &Text_Height;
	uiInfo.uiDC.textWidth = &Text_Width;
	uiInfo.uiDC.feederItemImage = &UI_FeederItemImage;
	uiInfo.uiDC.feederItemText = &UI_FeederItemText;
	uiInfo.uiDC.ownerDrawHandleKey = &UI_OwnerDrawHandleKey;

	uiInfo.uiDC.registerSkin = re.RegisterSkin;

	uiInfo.uiDC.g2_SetSkin = re.G2API_SetSkin;
	uiInfo.uiDC.g2_SetBoneAnim = re.G2API_SetBoneAnim;
	uiInfo.uiDC.g2_RemoveGhoul2Model = re.G2API_RemoveGhoul2Model;
	uiInfo.uiDC.g2_InitGhoul2Model = re.G2API_InitGhoul2Model;
	uiInfo.uiDC.g2_CleanGhoul2Models = re.G2API_CleanGhoul2Models;
	uiInfo.uiDC.g2_AddBolt = re.G2API_AddBolt;
	uiInfo.uiDC.g2_GetBoltMatrix = re.G2API_GetBoltMatrix;
	uiInfo.uiDC.g2_GiveMeVectorFromMatrix = re.G2API_GiveMeVectorFromMatrix;

	uiInfo.uiDC.g2hilev_SetAnim = UI_G2SetAnim;

	UI_BuildPlayerModel_List(inGameLoad);

	String_Init();

	const char* menuSet = UI_Cvar_VariableString("ui_menuFiles");

	if (menuSet == nullptr || menuSet[0] == '\0')
	{
		menuSet = "ui/menus.txt";
	}

#ifndef JK2_MODE
	if (inGameLoad)
	{
		UI_LoadMenus("ui/ingame.txt", qtrue);
	}
	else
#endif
	{
		UI_LoadMenus(menuSet, qtrue);
	}

	Menus_CloseAll();

	uiInfo.uiDC.whiteShader = ui.R_RegisterShaderNoMip("white");

	AssetCache();

	uis.debugMode = qfalse;

	// sets defaults for ui temp cvars
	uiInfo.effectsColor = static_cast<int>(trap_Cvar_VariableValue("color")) - 1;
	if (uiInfo.effectsColor < 0)
	{
		uiInfo.effectsColor = 0;
	}
	uiInfo.effectsColor = gamecodetoui[uiInfo.effectsColor];
	uiInfo.currentCrosshair = static_cast<int>(trap_Cvar_VariableValue("cg_drawCrosshair"));
	Cvar_Set("ui_mousePitch", trap_Cvar_VariableValue("m_pitch") >= 0 ? "0" : "1");

	Cvar_Set("cg_endcredits", "0"); // Reset value
	Cvar_Set("ui_missionfailed", "0"); // reset

	uiInfo.forcePowerUpdated = FP_UPDATED_NONE;
	uiInfo.selectedWeapon1 = NOWEAPON;
	uiInfo.selectedWeapon2 = NOWEAPON;
	uiInfo.selectedThrowWeapon = NOWEAPON;
	uiInfo.selectedPistolWeapon = NOWEAPON;

	uiInfo.uiDC.Assets.nullSound = trap_S_RegisterSound("sound/null", qfalse);

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	trap_S_RegisterSound("sound/interface/weapon_deselect", qfalse);
#endif
}

/*
=================
UI_RegisterCvars
=================
*/
static void UI_RegisterCvars()
{
	size_t i;
	const cvarTable_t* cv;

	for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++)
	{
		Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
		if (cv->update)
			cv->update();
	}
}

/*
=================
UI_ParseMenu
=================
*/
static void UI_ParseMenu(const char* menuFile)
{
	char* buffer, * holdBuffer;
	//	pc_token_t token;

	//Com_DPrintf("Parsing menu file: %s\n", menuFile);
	const int len = PC_StartParseSession(menuFile, &buffer);

	holdBuffer = buffer;

	if (len <= 0)
	{
		Com_Printf("UI_ParseMenu: Unable to load menu %s\n", menuFile);
		return;
	}

	while (true)
	{
		char* token2 = PC_ParseExt();

		if (!*token2)
		{
			break;
		}
		/*
				if ( menuCount == MAX_MENUS )
				{
					PC_ParseWarning("Too many menus!");
					break;
				}
		*/
		if (*token2 == '{')
		{
			continue;
		}
		if (*token2 == '}')
		{
			break;
		}
		if (Q_stricmp(token2, "assetGlobalDef") == 0)
		{
			if (Asset_Parse(&holdBuffer))
			{
				continue;
			}
			break;
		}
		if (Q_stricmp(token2, "menudef") == 0)
		{
			// start a new menu
			Menu_New(holdBuffer);
			continue;
		}

		PC_ParseWarning(va("Invalid keyword '%s'", token2));
	}

	PC_EndParseSession(buffer);
}

/*
=================
Load_Menu
	Load current menu file
=================
*/
static qboolean Load_Menu(const char** holdBuffer)
{
	const char* token2 = COM_ParseExt(holdBuffer, qtrue);

	if (!token2[0])
	{
		return qfalse;
	}

	if (*token2 != '{')
	{
		return qfalse;
	}

	while (true)
	{
		token2 = COM_ParseExt(holdBuffer, qtrue);

		if (!token2 || token2 == nullptr)
		{
			return qfalse;
		}

		if (*token2 == '}')
		{
			return qtrue;
		}

		//#ifdef _DEBUG
		//		extern void UI_Debug_AddMenuFilePath(const char *);
		//		UI_Debug_AddMenuFilePath(token2);
		//#endif
		UI_ParseMenu(token2);
	}
}

/*
=================
UI_LoadMenus
	Load all menus based on the files listed in the data file in menuFile (default "ui/menus.txt")
=================
*/
void UI_LoadMenus(const char* menuFile, const qboolean reset)
{
	char* buffer = nullptr;
	const char* holdBuffer;

	const int start = Sys_Milliseconds();

	int len = ui.FS_ReadFile(menuFile, reinterpret_cast<void**>(&buffer));

	if (len < 1)
	{
		Com_Printf(va(S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile));
		len = ui.FS_ReadFile("ui/menus.txt", reinterpret_cast<void**>(&buffer));

		if (len < 1)
		{
			Com_Error(ERR_FATAL, "%s",
				va("default menu file not found: ui/menus.txt, unable to continue!\n", menuFile));
		}
	}

	if (reset)
	{
		Menu_Reset();
	}

	holdBuffer = buffer;
	COM_BeginParseSession();
	while (true)
	{
		const char* token2 = COM_ParseExt(&holdBuffer, qtrue);
		if (!*token2)
		{
			break;
		}

		if (*token2 == 0 || *token2 == '}') // End of the menus file
		{
			break;
		}

		if (*token2 == '{')
		{
			continue;
		}
		if (Q_stricmp(token2, "loadmenu") == 0)
		{
			if (Load_Menu(&holdBuffer))
			{
				continue;
			}
			break;
		}
		Com_Printf("Unknown keyword '%s' in menus file %s\n", token2, menuFile);
	}
	COM_EndParseSession();

	Com_Printf("UI menu load time = %d milli seconds\n", Sys_Milliseconds() - start);

	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("----- Genuine MovieDuels SerenityJediEngine(Solaris Edition)-----\n");
	Com_Printf("---------------- MovieDuels-SJE-1.0-SP---------------------------\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("------------------------Update 9---------------------------------\n");
	Com_Printf("------------------Build Date 08/05/2024--------------------------\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("------------------------LightSaber-------------------------------\n");
	Com_Printf("-----------An elegant weapon for a more civilized age------------\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("This mod improves the hit detection and alters the saber traces.\n");
	Com_Printf("Additionally,the first part of a swing will now do damage.\n");
	Com_Printf("This allows the beginning of the swing to be blocked/parried.\n");
	Com_Printf("Because of this new early-block system, saber spam or heavy\n");
	Com_Printf("exploits will be less effective.\n");
	Com_Printf("A player can 'deny' an enemy who wildly swings their saber\n");
	Com_Printf("and will force them to make cleaner, more precise hits.\n");
	Com_Printf("To make up for the beginning of the swing's shortcomings\n");
	Com_Printf("I added a 50 percent increase to end-phase damage.\n");
	Com_Printf("More damage at the end of the swing should maintain the fast-paced feel.\n");
	Com_Printf("Well-timed and well-aimed slashes are more effective than on basejka.\n");
	Com_Printf("This could potentially improve the way we play MovieDuels quite a lot.\n");
	Com_Printf("Less spam-friendly, and more accurate hit detection could make the game more movie like.\n");
	Com_Printf("-----------------------------------------------------------------\n");

	ui.FS_FreeFile(buffer); //let go of the buffer
}

/*
=================
UI_Load
=================
*/
void UI_Load()
{
	const char* menuSet;
	char lastName[1024];
	const menuDef_t* menu = Menu_GetFocused();

	if (menu && menu->window.name)
	{
		strcpy(lastName, menu->window.name);
	}
	else
	{
		lastName[0] = 0;
	}

#ifndef JK2_MODE
	if (uiInfo.inGameLoad)
	{
		menuSet = "ui/ingame.txt";
	}
	else
#endif
	{
		menuSet = UI_Cvar_VariableString("ui_menuFiles");
	}
	if (menuSet == nullptr || menuSet[0] == '\0')
	{
		menuSet = "ui/menus.txt";
	}

	String_Init();

	UI_LoadMenus(menuSet, qtrue);
	Menus_CloseAll();
	Menus_ActivateByName(lastName);
}

/*
=================
Asset_Parse
=================
*/
qboolean Asset_Parse(char** buffer)
{
	const char* tempStr;
	int pointSize;

	char* token = PC_ParseExt();

	if (!token)
	{
		return qfalse;
	}

	if (*token != '{')
	{
		return qfalse;
	}

	while (true)
	{
		token = PC_ParseExt();

		if (!token)
		{
			return qfalse;
		}

		if (*token == '}')
		{
			return qtrue;
		}

		// fonts
		if (Q_stricmp(token, "smallFont") == 0) //legacy, really it only matters which order they are registered
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'smallFont'");
				return qfalse;
			}

			UI_RegisterFont(tempStr);

			//not used anymore
			if (PC_ParseInt(&pointSize))
			{
				//				PC_ParseWarning("Bad 2nd parameter for keyword 'smallFont'");
			}

			continue;
		}

		if (Q_stricmp(token, "mediumFont") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'font'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.qhMediumFont = UI_RegisterFont(tempStr);
			uiInfo.uiDC.Assets.fontRegistered = qtrue;

			//not used
			if (PC_ParseInt(&pointSize))
			{
				//				PC_ParseWarning("Bad 2nd parameter for keyword 'font'");
			}
			continue;
		}

		if (Q_stricmp(token, "bigFont") == 0) //legacy
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'bigFont'");
				return qfalse;
			}

			UI_RegisterFont(tempStr);

			if (PC_ParseInt(&pointSize))
			{
				//				PC_ParseWarning("Bad 2nd parameter for keyword 'bigFont'");
			}

			continue;
		}

#ifdef JK2_MODE
		if (Q_stricmp(token, "stripedFile") == 0)
		{
			if (!PC_ParseStringMem((const char**)&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'stripedFile'");
				return qfalse;
			}

			char sTemp[1024];
			Q_strncpyz(sTemp, tempStr, sizeof(sTemp));
			if (!ui.SP_Register(sTemp, /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			{
				PC_ParseWarning(va("(.SP file \"%s\" not found)", sTemp));
				//return qfalse;	// hmmm... dunno about this, don't want to break scripts for just missing subtitles
			}
			else
			{
				//				extern void AddMenuPackageRetryKey(const char *);
				//				AddMenuPackageRetryKey(sTemp);
			}

			continue;
		}
#endif

		// gradientbar
		if (Q_stricmp(token, "gradientbar") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'gradientbar'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.gradientBar = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		// enterMenuSound
		if (Q_stricmp(token, "menuEnterSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuEnterSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// exitMenuSound
		if (Q_stricmp(token, "menuExitSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuExitSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// itemFocusSound
		if (Q_stricmp(token, "itemFocusSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'itemFocusSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// menuBuzzSound
		if (Q_stricmp(token, "menuBuzzSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuBuzzSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// Chose a force power from the ingame force allocation screen (the one where you get to allocate a force power point)
		if (Q_stricmp(token, "forceChosenSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'forceChosenSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.forceChosenSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// Unchose a force power from the ingame force allocation screen (the one where you get to allocate a force power point)
		if (Q_stricmp(token, "forceUnchosenSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'forceUnchosenSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.forceUnchosenSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveRollSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveRollSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveRollSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveJumpSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveRoll'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveJumpSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound1") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound1'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound1 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound2") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound2'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound2 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound3") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound3'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound3 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound4") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound4'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound4 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound5") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound5'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound5 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound6") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound6'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound6 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		////////////////////////////////////////////////////////////////////////////////////

		if (Q_stricmp(token, "cursor") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_anakin") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_anakin'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_anakin = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_jk") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_jk3'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_jk = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_katarn") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_katarn'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_katarn = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_kylo") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_kylo'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_kylo = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_luke") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_luke'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_luke = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_obiwan") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_obiwan'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_obiwan = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_oldrepublic") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_oldrepublic'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_oldrepublic = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_quigon") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_quigon'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_quigon = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_rey") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_rey'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_rey = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_vader") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_vader'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_vader = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_windu") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_windu'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_windu = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////////

		if (Q_stricmp(token, "fadeClamp") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.fadeClamp))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeClamp'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "fadeCycle") == 0)
		{
			if (PC_ParseInt(&uiInfo.uiDC.Assets.fadeCycle))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeCycle'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "fadeAmount") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.fadeAmount))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeAmount'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowX") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.shadowX))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowX'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowY") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.shadowY))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowY'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowColor") == 0)
		{
			if (PC_ParseColor(&uiInfo.uiDC.Assets.shadowColor))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowColor'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
			continue;
		}

		// precaching various sound files used in the menus
		if (Q_stricmp(token, "precacheSound") == 0)
		{
			if (PC_Script_Parse(&tempStr))
			{
				char* soundFile;
				do
				{
					soundFile = COM_ParseExt(&tempStr, qfalse);
					if (soundFile[0] != 0 && soundFile[0] != ';')
					{
						if (!trap_S_RegisterSound(soundFile, qfalse))
						{
							PC_ParseWarning("Can't locate precache sound");
						}
					}
				} while (soundFile[0]);
			}
		}
	}
}

/*
=================
UI_Update
=================
*/
static void UI_Update(const char* name)
{
	const int val = trap_Cvar_VariableValue(name);

	if (Q_stricmp(name, "s_khz") == 0)
	{
		ui.Cmd_ExecuteText(EXEC_APPEND, "snd_restart\n");
		return;
	}

	if (Q_stricmp(name, "ui_SetName") == 0)
	{
		Cvar_Set("name", UI_Cvar_VariableString("ui_Name"));
	}
	else if (Q_stricmp(name, "ui_GetName") == 0)
	{
		Cvar_Set("ui_Name", UI_Cvar_VariableString("name"));
	}
	else if (Q_stricmp(name, "ui_r_colorbits") == 0)
	{
		switch (val)
		{
		case 0:
			Cvar_SetValue("ui_r_depthbits", 0);
			break;

		case 16:
			Cvar_SetValue("ui_r_depthbits", 16);
			break;

		case 32:
			Cvar_SetValue("ui_r_depthbits", 24);
			break;
		default:;
		}
	}
	else if (Q_stricmp(name, "ui_r_lodbias") == 0)
	{
		switch (val)
		{
		case 0:
			Cvar_SetValue("ui_r_subdivisions", 4);
			break;
		case 1:
			Cvar_SetValue("ui_r_subdivisions", 12);
			break;

		case 2:
			Cvar_SetValue("ui_r_subdivisions", 20);
			break;
		default:;
		}
	}
	else if (Q_stricmp(name, "ui_r_glCustom") == 0)
	{
		switch (val)
		{
		case 0: // high quality

			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 4);
			Cvar_SetValue("ui_r_lodbias", 0);
			Cvar_SetValue("ui_r_colorbits", 32);
			Cvar_SetValue("ui_r_depthbits", 24);
			Cvar_SetValue("ui_r_picmip", 0);
			Cvar_SetValue("ui_r_mode", 4);
			Cvar_SetValue("ui_r_texturebits", 32);
			Cvar_SetValue("ui_r_fastSky", 0);
			Cvar_SetValue("ui_r_inGameVideo", 1);
			Cvar_SetValue("ui_cg_shadows", 3);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
			break;

		case 1: // normal
			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 4);
			Cvar_SetValue("ui_r_lodbias", 0);
			Cvar_SetValue("ui_r_colorbits", 0);
			Cvar_SetValue("ui_r_depthbits", 24);
			Cvar_SetValue("ui_r_picmip", 1);
			Cvar_SetValue("ui_r_mode", 3);
			Cvar_SetValue("ui_r_texturebits", 0);
			Cvar_SetValue("ui_r_fastSky", 0);
			Cvar_SetValue("ui_r_inGameVideo", 1);
			Cvar_SetValue("ui_cg_shadows", 3);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
			break;

		case 2: // fast

			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 12);
			Cvar_SetValue("ui_r_lodbias", 1);
			Cvar_SetValue("ui_r_colorbits", 0);
			Cvar_SetValue("ui_r_depthbits", 0);
			Cvar_SetValue("ui_r_picmip", 2);
			Cvar_SetValue("ui_r_mode", 3);
			Cvar_SetValue("ui_r_texturebits", 0);
			Cvar_SetValue("ui_r_fastSky", 1);
			Cvar_SetValue("ui_r_inGameVideo", 0);
			Cvar_SetValue("ui_cg_shadows", 1);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
			break;

		case 3: // fastest

			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 20);
			Cvar_SetValue("ui_r_lodbias", 2);
			Cvar_SetValue("ui_r_colorbits", 16);
			Cvar_SetValue("ui_r_depthbits", 16);
			Cvar_SetValue("ui_r_mode", 3);
			Cvar_SetValue("ui_r_picmip", 3);
			Cvar_SetValue("ui_r_texturebits", 16);
			Cvar_SetValue("ui_r_fastSky", 1);
			Cvar_SetValue("ui_r_inGameVideo", 0);
			Cvar_SetValue("ui_cg_shadows", 0);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
			break;
		default:;
		}
	}
	else if (Q_stricmp(name, "ui_mousePitch") == 0)
	{
		if (val == 0)
		{
			Cvar_SetValue("m_pitch", 0.022f);
		}
		else
		{
			Cvar_SetValue("m_pitch", -0.022f);
		}
	}
	else
	{
		//failure!!
		Com_Printf("unknown UI script UPDATE %s\n", name);
	}
}

constexpr auto ASSET_SCROLLBAR = "gfx/menus/scrollbar.tga";
constexpr auto ASSET_SCROLLBAR_ARROWDOWN = "gfx/menus/scrollbar_arrow_dwn_a.tga";
constexpr auto ASSET_SCROLLBAR_ARROWUP = "gfx/menus/scrollbar_arrow_up_a.tga";
constexpr auto ASSET_SCROLLBAR_ARROWLEFT = "gfx/menus/scrollbar_arrow_left.tga";
constexpr auto ASSET_SCROLLBAR_ARROWRIGHT = "gfx/menus/scrollbar_arrow_right.tga";
constexpr auto ASSET_SCROLL_THUMB = "gfx/menus/scrollbar_thumb.tga";
//
constexpr auto ASSET_ANAKIN = "gfx/menus/cursor_anakin.tga";
constexpr auto ASSET_JK = "gfx/menus/cursor_jk.tga";
constexpr auto ASSET_KATARN = "gfx/menus/cursor_katarn.tga";
constexpr auto ASSET_KYLO = "gfx/menus/cursor_kylo.tga";
constexpr auto ASSET_LUKE = "gfx/menus/cursor_luke.tga";
constexpr auto ASSET_OBIWAN = "gfx/menus/cursor_obiwan.tga";
constexpr auto ASSET_OLDREPUBLIC = "gfx/menus/cursor_oldrepublic.tga";
constexpr auto ASSET_QUIGON = "gfx/menus/cursor_quigon.tga";
constexpr auto ASSET_RAY = "gfx/menus/cursor_rey.tga";
constexpr auto ASSET_VADER = "gfx/menus/cursor_vader.tga";
constexpr auto ASSET_WINDU = "gfx/menus/cursor_windu.tga";

/*
=================
AssetCache
=================
*/
void AssetCache()
{
	//	int n;
	uiInfo.uiDC.Assets.scrollBar = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR);
	uiInfo.uiDC.Assets.scrollBarArrowDown = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
	uiInfo.uiDC.Assets.scrollBarArrowUp = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
	uiInfo.uiDC.Assets.scrollBarArrowLeft = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
	uiInfo.uiDC.Assets.scrollBarArrowRight = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
	uiInfo.uiDC.Assets.scrollBarThumb = ui.R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);

	uiInfo.uiDC.Assets.sliderBar = ui.R_RegisterShaderNoMip("menu/new/slider");
	uiInfo.uiDC.Assets.sliderThumb = ui.R_RegisterShaderNoMip("menu/new/sliderthumb");

	uiInfo.uiDC.Assets.cursor_anakin = ui.R_RegisterShaderNoMip(ASSET_ANAKIN);
	uiInfo.uiDC.Assets.cursor_jk = ui.R_RegisterShaderNoMip(ASSET_JK);
	uiInfo.uiDC.Assets.cursor_katarn = ui.R_RegisterShaderNoMip(ASSET_KATARN);
	uiInfo.uiDC.Assets.cursor_kylo = ui.R_RegisterShaderNoMip(ASSET_KYLO);
	uiInfo.uiDC.Assets.cursor_luke = ui.R_RegisterShaderNoMip(ASSET_LUKE);
	uiInfo.uiDC.Assets.cursor_obiwan = ui.R_RegisterShaderNoMip(ASSET_OBIWAN);
	uiInfo.uiDC.Assets.cursor_oldrepublic = ui.R_RegisterShaderNoMip(ASSET_OLDREPUBLIC);
	uiInfo.uiDC.Assets.cursor_quigon = ui.R_RegisterShaderNoMip(ASSET_QUIGON);
	uiInfo.uiDC.Assets.cursor_rey = ui.R_RegisterShaderNoMip(ASSET_RAY);
	uiInfo.uiDC.Assets.cursor_vader = ui.R_RegisterShaderNoMip(ASSET_VADER);
	uiInfo.uiDC.Assets.cursor_windu = ui.R_RegisterShaderNoMip(ASSET_WINDU);
}

/*
================
_UI_DrawSides
=================
*/
void _UI_DrawSides(const float x, const float y, const float w, const float h, float size)
{
	size *= uiInfo.uiDC.xscale;
	trap_R_DrawStretchPic(x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
	trap_R_DrawStretchPic(x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

/*
================
_UI_DrawTopBottom
=================
*/
void _UI_DrawTopBottom(const float x, const float y, const float w, const float h, float size)
{
	size *= uiInfo.uiDC.yscale;
	trap_R_DrawStretchPic(x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
	trap_R_DrawStretchPic(x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void _UI_DrawRect(const float x, const float y, const float width, const float height, const float size, const float* color)
{
	trap_R_SetColor(color);

	_UI_DrawTopBottom(x, y, width, height, size);
	_UI_DrawSides(x, y, width, height, size);

	trap_R_SetColor(nullptr);
}

/*
=================
UI_UpdateCvars
=================
*/
void UI_UpdateCvars()
{
	size_t i;
	const cvarTable_t* cv;

	for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++)
	{
		if (cv->vmCvar)
		{
			const int modCount = cv->vmCvar->modificationCount;
			Cvar_Update(cv->vmCvar);
			if (cv->vmCvar->modificationCount != modCount)
			{
				if (cv->update)
					cv->update();
			}
		}
	}
}

/*
=================
UI_DrawEffects
=================
*/
static void UI_DrawEffects(const rectDef_t* rect, float scale, vec4_t color)
{
	UI_DrawHandlePic(rect->x, rect->y - 14, 128, 8, 0/*uiInfo.uiDC.Assets.fxBasePic*/);
	UI_DrawHandlePic(rect->x + uiInfo.effectsColor * 16 + 8, rect->y - 16, 16, 12,
		0/*uiInfo.uiDC.Assets.fxPic[uiInfo.effectsColor]*/);
}

/*
=================
UI_Version
=================
*/
static void UI_Version(const rectDef_t* rect, const float scale, vec4_t color, const int iFontIndex)
{
	const int width = DC->textWidth(Q3_VERSION, scale, 0);

	DC->drawText(rect->x - width, rect->y, scale, color, Q3_VERSION, 0, ITEM_TEXTSTYLE_SHADOWED, iFontIndex);
}

/*
=================
UI_DrawKeyBindStatus
=================
*/
static void UI_DrawKeyBindStatus(rectDef_t* rect, float scale, vec4_t color, int textStyle, int iFontIndex)
{
	if (Display_KeyBindPending())
	{
#ifdef JK2_MODE
		Text_Paint(rect->x, rect->y, scale, color, ui.SP_GetStringTextString("MENUS_WAITINGFORKEY"), 0, textStyle, iFontIndex);
#else
		Text_Paint(rect->x, rect->y, scale, color, SE_GetString("MENUS_WAITINGFORKEY"), 0, textStyle, iFontIndex);
#endif
	}
	else
	{
		//		Text_Paint(rect->x, rect->y, scale, color, ui.SP_GetStringTextString("MENUS_ENTERTOCHANGE"), 0, textStyle, iFontIndex);
	}
}

/*
=================
UI_DrawKeyBindStatus
=================
*/
static void UI_DrawGLInfo(const rectDef_t* rect, const float scale, vec4_t color, const int textStyle, const int iFontIndex)
{
	constexpr auto MAX_LINES = 64;
	char buff[4096]{};
	char* eptr = buff;
	const char* lines[MAX_LINES]{};
	int numLines = 0, i = 0;

	int y = rect->y;
	Text_Paint(rect->x, y, scale, color, va("GL_VENDOR: %s", uiInfo.uiDC.glconfig.vendor_string), rect->w, textStyle,
		iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color,
		va("GL_VERSION: %s: %s", uiInfo.uiDC.glconfig.version_string, uiInfo.uiDC.glconfig.renderer_string),
		rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color, "GL_PIXELFORMAT:", rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color,
		va("Color(%d-bits) Z(%d-bits) stencil(%d-bits)", uiInfo.uiDC.glconfig.colorBits,
			uiInfo.uiDC.glconfig.depthBits, uiInfo.uiDC.glconfig.stencilBits), rect->w, textStyle, iFontIndex);
	y += 15;
	// build null terminated extension strings
	Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, sizeof buff);
	int testy = y - 16;
	while (testy <= rect->y + rect->h && *eptr && numLines < MAX_LINES)
	{
		while (*eptr && *eptr == ' ')
			*eptr++ = '\0';

		// track start of valid string
		if (*eptr && *eptr != ' ')
		{
			lines[numLines++] = eptr;
			testy += 16;
		}

		while (*eptr && *eptr != ' ')
			eptr++;
	}

	numLines--;
	while (i < numLines)
	{
		Text_Paint(rect->x, y, scale, color, lines[i++], rect->w, textStyle, iFontIndex);
		y += 16;
	}
}

/*
=================
UI_DataPad_Inventory
=================
*/
/*
static void UI_DataPad_Inventory(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	Text_Paint(rect->x, rect->y, scale, color, "INVENTORY", 0, 1, iFontIndex);
}
*/
/*
=================
UI_DataPad_ForcePowers
=================
*/
/*
static void UI_DataPad_ForcePowers(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	Text_Paint(rect->x, rect->y, scale, color, "FORCE POWERS", 0, 1, iFontIndex);
}
*/

static void UI_DrawCrosshair(const rectDef_t* rect, float scale, vec4_t color)
{
	trap_R_SetColor(color);
	if (uiInfo.currentCrosshair < 0 || uiInfo.currentCrosshair >= NUM_CROSSHAIRS)
	{
		uiInfo.currentCrosshair = 0;
	}
	UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
	trap_R_SetColor(nullptr);
}

/*
=================
UI_OwnerDraw
=================
*/
static void UI_OwnerDraw(float x, float y, float w, float h, const float text_x, const float text_y, const int ownerDraw,
	int ownerDrawFlags, int align, float special, const float scale, vec4_t color, qhandle_t shader,
	const int textStyle, const int iFontIndex)
{
	rectDef_t rect{};

	rect.x = x + text_x;
	rect.y = y + text_y;
	rect.w = w;
	rect.h = h;

	switch (ownerDraw)
	{
	case UI_EFFECTS:
		UI_DrawEffects(&rect, scale, color);
		break;
	case UI_VERSION:
		UI_Version(&rect, scale, color, iFontIndex);
		break;

	case UI_DATAPAD_MISSION:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_OBJECTIVES);
		break;

	case UI_DATAPAD_WEAPONS:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_WEAPONS);
		break;

	case UI_DATAPAD_INVENTORY:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_INVENTORY);
		break;

	case UI_DATAPAD_FORCEPOWERS:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_FORCEPOWERS);
		break;

	case UI_ALLMAPS_SELECTION: //saved game thumbnail

		int levelshot;
		levelshot = ui.
			R_RegisterShaderNoMip(va("levelshots/%s", s_savedata[s_savegame.currentLine].currentSaveFileMap));
#ifdef JK2_MODE
		if (screenShotBuf[0])
		{
			ui.DrawStretchRaw(x, y, w, h, SG_SCR_WIDTH, SG_SCR_HEIGHT, screenShotBuf, 0, qtrue);
		}
		else
#endif
			if (levelshot)
			{
				ui.R_DrawStretchPic(x, y, w, h, 0, 0, 1, 1, levelshot);
			}
			else
			{
				UI_DrawHandlePic(x, y, w, h, uis.menuBackShader);
			}

		ui.R_Font_DrawString(x, // int ox
			y + h, // int oy
			s_savedata[s_savegame.currentLine].currentSaveFileMap, // const char *text
			color, // paletteRGBA_c c
			iFontIndex, // const int iFontHandle
			w, //-1,		// iMaxPixelWidth (-1 = none)
			scale // const float scale = 1.0f
		);
		break;
	case UI_PREVIEWCINEMATIC:
		// FIXME BOB - make this work?
		//			UI_DrawPreviewCinematic(&rect, scale, color);
		break;
	case UI_CROSSHAIR:
		UI_DrawCrosshair(&rect, scale, color);
		break;
	case UI_GLINFO:
		UI_DrawGLInfo(&rect, scale, color, textStyle, iFontIndex);
		break;
	case UI_KEYBINDSTATUS:
		UI_DrawKeyBindStatus(&rect, scale, color, textStyle, iFontIndex);
		break;
	default:
		break;
	}
}

/*
=================
UI_OwnerDrawVisible
=================
*/
static qboolean UI_OwnerDrawVisible(int flags)
{
	constexpr qboolean vis = qtrue;

	while (flags)
	{
		/*		if (flags & UI_SHOW_DEMOAVAILABLE)
				{
					if (!uiInfo.demoAvailable)
					{
						vis = qfalse;
					}
					flags &= ~UI_SHOW_DEMOAVAILABLE;
				}
				else
		*/
		{
			flags = 0;
		}
	}
	return vis;
}

/*
=================
Text_Width
=================
*/
int Text_Width(const char* text, const float scale, int iFontIndex)
{
	// temp code until Bob retro-fits all menus to have font specifiers...
	//
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	return ui.R_Font_StrLenPixels(text, iFontIndex, scale);
}

/*
=================
UI_OwnerDrawWidth
=================
*/
int UI_OwnerDrawWidth(const int ownerDraw, const float scale)
{
	//	int i, h, value;
	//	const char *text;
	const char* s = nullptr;

	switch (ownerDraw)
	{
	case UI_KEYBINDSTATUS:
		if (Display_KeyBindPending())
		{
#ifdef JK2_MODE
			s = ui.SP_GetStringTextString("MENUS_WAITINGFORKEY");
#else
			s = SE_GetString("MENUS_WAITINGFORKEY");
#endif
		}
		else
		{
			//			s = ui.SP_GetStringTextString("MENUS_ENTERTOCHANGE");
		}
		break;

		// FIXME BOB
		//	case UI_SERVERREFRESHDATE:
		//		s = UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer));
		//		break;
	default:
		break;
	}

	if (s)
	{
		return Text_Width(s, scale, 0);
	}
	return 0;
}

/*
=================
Text_Height
=================
*/
int Text_Height(const char* text, const float scale, int iFontIndex)
{
	// temp until Bob retro-fits all menu files with font specifiers...
	//
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	return ui.R_Font_HeightPixels(iFontIndex, scale);
}

/*
=================
UI_MouseEvent
=================
*/
//JLFMOUSE  CALLED EACH FRAME IN UI
void _UI_MouseEvent(const int dx, const int dy)
{
	// update mouse screen position
	uiInfo.uiDC.cursorx += dx;
	if (uiInfo.uiDC.cursorx < 0)
	{
		uiInfo.uiDC.cursorx = 0;
	}
	else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH)
	{
		uiInfo.uiDC.cursorx = SCREEN_WIDTH;
	}

	uiInfo.uiDC.cursory += dy;
	if (uiInfo.uiDC.cursory < 0)
	{
		uiInfo.uiDC.cursory = 0;
	}
	else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
	{
		uiInfo.uiDC.cursory = SCREEN_HEIGHT;
	}

	if (Menu_Count() > 0)
	{
		Display_MouseMove(nullptr, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
	}
}

/*
=================
UI_KeyEvent
=================
*/
void _UI_KeyEvent(const int key, const qboolean down)
{
	/*	extern qboolean SwallowBadNumLockedKPKey( int iKey );
		if (SwallowBadNumLockedKPKey(key)){
			return;
		}
	*/

	if (Menu_Count() > 0)
	{
		menuDef_t* menu = Menu_GetFocused();
		if (menu)
		{
			//DemoEnd();
			if (key == A_ESCAPE && down && !Menus_AnyFullScreenVisible() && !(menu->window.flags & WINDOW_IGNORE_ESCAPE))
			{
				Menus_CloseAll();
			}
			else
			{
				Menu_HandleKey(menu, key, down);
			}
		}
		else
		{
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Cvar_Set("cl_paused", "0");
		}
	}
}

/*
=================
UI_Report
=================
*/
void UI_Report()
{
	String_Report();
}

/*
=================
UI_DataPadMenu
=================
*/
void UI_DataPadMenu()
{
	Menus_CloseByName("mainhud");

	const int newForcePower = static_cast<int>(trap_Cvar_VariableValue("cg_updatedDataPadForcePower1"));
	const int newObjective = static_cast<int>(trap_Cvar_VariableValue("cg_updatedDataPadObjective"));

	if (newForcePower)
	{
		Menus_ActivateByName("datapadForcePowersMenu");
	}
	else if (newObjective)
	{
		Menus_ActivateByName("datapadMissionMenu");
	}
	else
	{
		Menus_ActivateByName("datapadMissionMenu");
	}
	ui.Key_SetCatcher(KEYCATCH_UI);
}

/*
=================
UI_InGameMenu
=================
*/
void UI_InGameMenu(const char* menuID)
{
#ifdef JK2_MODE
	ui.PrecacheScreenshot();
#endif
	Menus_CloseByName("mainhud");

	if (menuID)
	{
		Menus_ActivateByName(menuID);
	}
	else
	{
		Menus_ActivateByName("ingameMainMenu");
	}
	ui.Key_SetCatcher(KEYCATCH_UI);
}

qboolean _UI_IsFullscreen()
{
	return Menus_AnyFullScreenVisible();
}

/*
=======================================================================

MAIN MENU

=======================================================================
*/

/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu()
{
	char buf[256];
	ui.Cvar_Set("sv_killserver", "1"); // let the demo server know it should shut down

	ui.Key_SetCatcher(KEYCATCH_UI);

	const menuDef_t* m = Menus_ActivateByName("mainMenu");
	if (!m)
	{
		//wha? try again
		UI_LoadMenus("ui/menus.txt", qfalse);
	}
	ui.Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof buf);
	if (strlen(buf))
	{
		Menus_ActivateByName("error_popmenu");
	}
}

int SCREENSHOT_TOTAL = -1;
int SCREENSHOT_CHOICE = 0;
int SCREENSHOT_NEXT_UPDATE_TIME = 0;

static char* UI_GetCurrentLevelshot()
{
	const int time = Sys_Milliseconds();

	if (SCREENSHOT_NEXT_UPDATE_TIME < time)
	{
		if (SCREENSHOT_TOTAL < 0)
		{
			// Count and register them...
			SCREENSHOT_TOTAL = 0;

			while (true)
			{
				char screenShot[128] = { 0 };

				strcpy(screenShot, va("menu/art/unknownmap%i", SCREENSHOT_TOTAL));

				if (!ui.R_RegisterShaderNoMip(screenShot))
				{
					break;
				}
				SCREENSHOT_TOTAL++;
			}
			SCREENSHOT_TOTAL--;
		}
		SCREENSHOT_NEXT_UPDATE_TIME = time + 5000;
		SCREENSHOT_CHOICE++;

		if (SCREENSHOT_CHOICE > SCREENSHOT_TOTAL)
		{
			SCREENSHOT_CHOICE = 0;
		}
	}

	return va("menu/art/unknownmap%i", SCREENSHOT_CHOICE);
}

/*
=================
Menu_Cache
=================
*/
void Menu_Cache()
{
	uis.cursor = ui.R_RegisterShaderNoMip("menu/new/crosshairb");
	// Common menu graphics
	uis.whiteShader = ui.R_RegisterShader("white");
	//uis.menuBackShader = ui.R_RegisterShaderNoMip( "menu/art/unknownmap" );
	//uis.menuBackShader = ui.R_RegisterShaderNoMip(UI_GetCurrentLevelshot());
	uis.menuBackShader = ui.R_RegisterShaderNoMip(va("menu/art/unknownmap%d", Q_irand(0, 10)));
}

/*
=================
UI_UpdateVideoSetup

Copies the temporary user interface version of the video cvars into
their real counterparts.  This is to create a interface which allows
you to discard your changes if you did something you didnt want
=================
*/
void UI_UpdateVideoSetup()
{
	Cvar_Set("r_mode", Cvar_VariableString("ui_r_mode"));
	Cvar_Set("r_fullscreen", Cvar_VariableString("ui_r_fullscreen"));
	Cvar_Set("r_colorbits", Cvar_VariableString("ui_r_colorbits"));
	Cvar_Set("r_lodbias", Cvar_VariableString("ui_r_lodbias"));
	Cvar_Set("r_picmip", Cvar_VariableString("ui_r_picmip"));
	Cvar_Set("r_texturebits", Cvar_VariableString("ui_r_texturebits"));
	Cvar_Set("r_texturemode", Cvar_VariableString("ui_r_texturemode"));
	Cvar_Set("r_detailtextures", Cvar_VariableString("ui_r_detailtextures"));
	Cvar_Set("r_ext_compress_textures", Cvar_VariableString("ui_r_ext_compress_textures"));
	Cvar_Set("r_depthbits", Cvar_VariableString("ui_r_depthbits"));
	Cvar_Set("r_subdivisions", Cvar_VariableString("ui_r_subdivisions"));
	Cvar_Set("r_fastSky", Cvar_VariableString("ui_r_fastSky"));
	Cvar_Set("r_inGameVideo", Cvar_VariableString("ui_r_inGameVideo"));
	Cvar_Set("r_allowExtensions", Cvar_VariableString("ui_r_allowExtensions"));
	Cvar_Set("cg_shadows", Cvar_VariableString("ui_cg_shadows"));
	Cvar_Set("ui_r_modified", "0");

	Cbuf_ExecuteText(EXEC_APPEND, "vid_restart;");
}

/*
=================
UI_GetVideoSetup

Retrieves the current actual video settings into the temporary user
interface versions of the cvars.
=================
*/
void UI_GetVideoSetup()
{
	Cvar_Register(nullptr, "ui_r_glCustom", "4", CVAR_ARCHIVE);

	// Make sure the cvars are registered as read only.
	Cvar_Register(nullptr, "ui_r_mode", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_fullscreen", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_colorbits", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_lodbias", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_picmip", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_texturebits", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_texturemode", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_detailtextures", "1", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_ext_compress_textures", "1", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_depthbits", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_subdivisions", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_fastSky", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_inGameVideo", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_allowExtensions", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_cg_shadows", "3", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_modified", "0", CVAR_ROM);

	// Copy over the real video cvars into their temporary counterparts
	Cvar_Set("ui_r_mode", Cvar_VariableString("r_mode"));
	Cvar_Set("ui_r_colorbits", Cvar_VariableString("r_colorbits"));
	Cvar_Set("ui_r_fullscreen", Cvar_VariableString("r_fullscreen"));
	Cvar_Set("ui_r_lodbias", Cvar_VariableString("r_lodbias"));
	Cvar_Set("ui_r_picmip", Cvar_VariableString("r_picmip"));
	Cvar_Set("ui_r_texturebits", Cvar_VariableString("r_texturebits"));
	Cvar_Set("ui_r_texturemode", Cvar_VariableString("r_texturemode"));
	Cvar_Set("ui_r_detailtextures", Cvar_VariableString("r_detailtextures"));
	Cvar_Set("ui_r_ext_compress_textures", Cvar_VariableString("r_ext_compress_textures"));
	Cvar_Set("ui_r_depthbits", Cvar_VariableString("r_depthbits"));
	Cvar_Set("ui_r_subdivisions", Cvar_VariableString("r_subdivisions"));
	Cvar_Set("ui_r_fastSky", Cvar_VariableString("r_fastSky"));
	Cvar_Set("ui_r_inGameVideo", Cvar_VariableString("r_inGameVideo"));
	Cvar_Set("ui_r_allowExtensions", Cvar_VariableString("r_allowExtensions"));
	Cvar_Set("ui_cg_shadows", Cvar_VariableString("cg_shadows"));
	Cvar_Set("ui_r_modified", "0");
}

static void UI_SetSexandSoundForModel(const char* char_model)
{
	int f;
	char soundpath[MAX_QPATH]{};
	qboolean isFemale = qfalse;

	int i = ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", char_model), &f, FS_READ);
	if (!f)
	{
		//no?  oh bother.
		Cvar_Reset("snd");
		Cvar_Reset("sex");
		return;
	}

	soundpath[0] = 0;

	ui.FS_Read(&soundpath, i, f);

	while (i >= 0 && soundpath[i] != '\n')
	{
		if (soundpath[i] == 'f')
		{
			isFemale = qtrue;
			soundpath[i] = 0;
		}

		i--;
	}

	i = 0;

	while (soundpath[i] && soundpath[i] != '\r' && soundpath[i] != '\n')
	{
		i++;
	}
	soundpath[i] = 0;

	ui.FS_FCloseFile(f);

	Cvar_Set("snd", soundpath);
	if (isFemale)
	{
		Cvar_Set("sex", "f");
	}
	else
	{
		Cvar_Set("sex", "m");
	}
}

static void UI_UpdateCharacterCvars()
{
	const char* char_model = Cvar_VariableString("ui_char_model");
	UI_SetSexandSoundForModel(char_model);
	Cvar_Set("g_char_model", char_model);
	Cvar_Set("g_char_skin_head", Cvar_VariableString("ui_char_skin_head"));
	Cvar_Set("g_char_skin_torso", Cvar_VariableString("ui_char_skin_torso"));
	Cvar_Set("g_char_skin_legs", Cvar_VariableString("ui_char_skin_legs"));
	Cvar_Set("g_char_color_red", Cvar_VariableString("ui_char_color_red"));
	Cvar_Set("g_char_color_green", Cvar_VariableString("ui_char_color_green"));
	Cvar_Set("g_char_color_blue", Cvar_VariableString("ui_char_color_blue"));
}

static void UI_GetCharacterCvars()
{
	Cvar_Set("ui_char_skin_head", Cvar_VariableString("g_char_skin_head"));
	Cvar_Set("ui_char_skin_torso", Cvar_VariableString("g_char_skin_torso"));
	Cvar_Set("ui_char_skin_legs", Cvar_VariableString("g_char_skin_legs"));
	Cvar_Set("ui_char_color_red", Cvar_VariableString("g_char_color_red"));
	Cvar_Set("ui_char_color_green", Cvar_VariableString("g_char_color_green"));
	Cvar_Set("ui_char_color_blue", Cvar_VariableString("g_char_color_blue"));

	const char* model = Cvar_VariableString("g_char_model");
	Cvar_Set("ui_char_model", model);

	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		if (!Q_stricmp(model, uiInfo.playerSpecies[i].Name))
		{
			uiInfo.playerSpeciesIndex = i;
		}
	}
}

extern saber_colors_t TranslateSaberColor(const char* name);

static void UI_UpdateSaberCvars()
{
	if (!Cvar_VariableIntegerValue("g_NPCsaber") && !Cvar_VariableIntegerValue("g_NPCsabertwo"))
	{
		Cvar_Set("g_saber_type", Cvar_VariableString("ui_saber_type"));
		Cvar_Set("g_saber", Cvar_VariableString("ui_saber"));
		Cvar_Set("g_saber2", Cvar_VariableString("ui_saber2"));
		Cvar_Set("g_saber_color", Cvar_VariableString("ui_saber_color"));
		Cvar_Set("g_saber2_color", Cvar_VariableString("ui_saber2_color"));
	}
	else
	{
		Cvar_Set("g_NPCsaber", Cvar_VariableString("ui_npc_saber"));
		Cvar_Set("g_NPCsabertwo", Cvar_VariableString("ui_npc_sabertwo"));

		Cvar_Set("g_NPCsabercolor", Cvar_VariableString("ui_npc_sabercolor"));
		Cvar_Set("g_NPCsabertwocolor", Cvar_VariableString("ui_npc_sabertwocolor"));
	}

	if (TranslateSaberColor(Cvar_VariableString("ui_saber_color")) >= SABER_RGB)
	{
		char rgbColor[8];
		Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber_red"),
			Cvar_VariableIntegerValue("ui_rgb_saber_green"),
			Cvar_VariableIntegerValue("ui_rgb_saber_blue"));
		if (!Cvar_VariableIntegerValue("g_NPCsaber") && !Cvar_VariableIntegerValue("g_NPCsabertwo"))
		{
			Cvar_Set("g_saber_color", rgbColor);
		}
		else
		{
			Cvar_Set("g_NPCsabercolor", rgbColor);
		}
	}

	if (TranslateSaberColor(Cvar_VariableString("ui_saber2_color")) >= SABER_RGB)
	{
		char rgbColor[8];
		Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber2_red"),
			Cvar_VariableIntegerValue("ui_rgb_saber2_green"),
			Cvar_VariableIntegerValue("ui_rgb_saber2_blue"));
		if (!Cvar_VariableIntegerValue("g_NPCsaber") && !Cvar_VariableIntegerValue("g_NPCsabertwo"))
		{
			Cvar_Set("g_saber2_color", rgbColor);
		}
		else
		{
			Cvar_Set("g_NPCsabertwocolor", rgbColor);
		}
	}
}

#ifndef JK2_MODE
static void UI_UpdateFightingStyleChoices()
{
	//
	if (!Q_stricmp("staff", Cvar_VariableString("ui_saber_type")))
	{
		Cvar_Set("ui_fightingstylesallowed", "0");
		Cvar_Set("ui_newfightingstyle", "4"); // SS_STAFF
	}
	else if (!Q_stricmp("dual", Cvar_VariableString("ui_saber_type")))
	{
		Cvar_Set("ui_fightingstylesallowed", "0");
		Cvar_Set("ui_newfightingstyle", "3"); // SS_DUAL
	}
	else
	{
		// Get player state
		const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

		if (cl && cl->gentity && cl->gentity->client)
		{
			const playerState_t* pState = cl->gentity->client;

			// Knows Fast style?
			if (pState->saberStylesKnown & 1 << SS_FAST)
			{
				// And Medium?
				if (pState->saberStylesKnown & 1 << SS_MEDIUM)
				{
					Cvar_Set("ui_fightingstylesallowed", "6"); // Has FAST and MEDIUM, so can only choose STRONG
					Cvar_Set("ui_newfightingstyle", "2"); // STRONG
				}
				else
				{
					Cvar_Set("ui_fightingstylesallowed", "1"); // Has FAST, so can choose from MEDIUM and STRONG
					Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
				}
			}
			// Knows Medium style?
			else if (pState->saberStylesKnown & 1 << SS_MEDIUM)
			{
				// And Strong?
				if (pState->saberStylesKnown & 1 << SS_STRONG)
				{
					Cvar_Set("ui_fightingstylesallowed", "4"); // Has MEDIUM and STRONG, so can only choose FAST
					Cvar_Set("ui_newfightingstyle", "0"); // FAST
				}
				else
				{
					Cvar_Set("ui_fightingstylesallowed", "2"); // Has MEDIUM, so can choose from FAST and STRONG
					Cvar_Set("ui_newfightingstyle", "0"); // FAST
				}
			}
			// Knows Strong style?
			else if (pState->saberStylesKnown & 1 << SS_STRONG)
			{
				// And Fast
				if (pState->saberStylesKnown & 1 << SS_FAST)
				{
					Cvar_Set("ui_fightingstylesallowed", "5"); // Has STRONG and FAST, so can only take MEDIUM
					Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
				}
				else
				{
					Cvar_Set("ui_fightingstylesallowed", "3"); // Has STRONG, so can choose from FAST and MEDIUM
					Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
				}
			}
			else // They have nothing, which should not happen
			{
				Cvar_Set("ui_currentfightingstyle", "1"); // Default MEDIUM
				Cvar_Set("ui_newfightingstyle", "0"); // FAST??
				Cvar_Set("ui_fightingstylesallowed", "0"); // Default to no new styles allowed
			}

			// Determine current style
			if (pState->saber_anim_level == SS_FAST)
			{
				Cvar_Set("ui_currentfightingstyle", "0"); // FAST
			}
			else if (pState->saber_anim_level == SS_STRONG)
			{
				Cvar_Set("ui_currentfightingstyle", "2"); // STRONG
			}
			else
			{
				Cvar_Set("ui_currentfightingstyle", "1"); // default MEDIUM
			}
		}
		else // No client so this must be first time
		{
			Cvar_Set("ui_currentfightingstyle", "1"); // Default to MEDIUM
			Cvar_Set("ui_fightingstylesallowed", "0"); // Default to no new styles allowed
			Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
		}
	}
}
#endif // !JK2_MODE

constexpr auto MAX_POWER_ENUMS = 25;

using powerEnum_t = struct
{
	const char* title;
	short powerEnum;
};

static powerEnum_t powerEnums[MAX_POWER_ENUMS] =
{
#ifndef JK2_MODE
	{"absorb", FP_ABSORB},
#endif // !JK2_MODE

	{"heal", FP_HEAL},
	{"mindtrick", FP_TELEPATHY},

#ifndef JK2_MODE
	{"protect", FP_PROTECT},
	{"stasis", FP_STASIS},
#endif // !JK2_MODE

	// Core powers
	{"jump", FP_LEVITATION},
	{"pull", FP_PULL},
	{"push", FP_PUSH},

#ifndef JK2_MODE
	{"sense", FP_SEE},
#endif // !JK2_MODE

	{"speed", FP_SPEED},
	{"sabdef", FP_SABER_DEFENSE},
	{"saboff", FP_SABER_OFFENSE},
	{"sabthrow", FP_SABERTHROW},

	// Dark powers
#ifndef JK2_MODE
	{"drain", FP_DRAIN},
#endif // !JK2_MODE

	{"grip", FP_GRIP},
	{"lightning", FP_LIGHTNING},

#ifndef JK2_MODE
	{"rage", FP_RAGE},
	{"destruction", FP_DESTRUCTION},

	{"grasp", FP_GRASP},
	{"repulse", FP_REPULSE},
	{"strike", FP_LIGHTNING_STRIKE},
	{"fear", FP_FEAR},
	{"deadlysight", FP_DEADLYSIGHT},
	{"projection", FP_PROJECTION},
	{"blast", FP_BLAST},
#endif // !JK2_MODE
};

// Find the index to the Force Power in powerEnum array
static qboolean UI_GetForcePowerIndex(const char* forceName, short* forcePowerI)
{
	// Find a match for the forceName passed in
	for (int i = 0; i < MAX_POWER_ENUMS; i++)
	{
		if (!Q_stricmp(forceName, powerEnums[i].title))
		{
			*forcePowerI = i;
			return qtrue;
		}
	}

	*forcePowerI = FP_UPDATED_NONE; // Didn't find it

	return qfalse;
}

// Set the fields for the allocation of force powers (Used by Force Power Allocation screen)
static void UI_InitAllocForcePowers(const char* forceName)
{
	short forcePowerI = 0;
	int forcelevel;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex(forceName, &forcePowerI))
	{
		return;
	}

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE: this UIScript can be called outside the running game now, so handle that case
	// by getting info frim UIInfo instead of PlayerState
	if (cl)
	{
		const playerState_t* pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	char itemName[128];
	Com_sprintf(itemName, sizeof itemName, "%s_hexpic", powerEnums[forcePowerI].title);
	auto item = Menu_FindItemByName(menu, itemName);

	if (item)
	{
		char itemGraphic[128];
		Com_sprintf(itemGraphic, sizeof itemGraphic, "gfx/menus/hex_pattern_%d", forcelevel >= 4 ? 3 : forcelevel);
		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);

		// If maxed out on power - don't allow update
		if (forcelevel >= 3)
		{
			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[forcePowerI].title);
			item = Menu_FindItemByName(menu, itemName);
			if (item)
			{
				item->action = nullptr; //you are bad, no action for you!
				item->descText = nullptr; //no desc either!
			}
		}
	}

	// Set weapons button to inactive
	UI_ForcePowerWeaponsButton(qfalse);
}

// Flip flop between being able to see the text showing the Force Point has or hasn't been allocated (Used by Force Power Allocation screen)
static void UI_SetPowerTitleText(const qboolean showAllocated)
{
	itemDef_t* item;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (showAllocated)
	{
		// Show the text saying the force point has been allocated
		item = Menu_FindItemByName(menu, "allocated_text");
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}

		// Hide text saying the force point needs to be allocated
		item = Menu_FindItemByName(menu, "allocate_text");
		if (item)
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}
	}
	else
	{
		// Hide the text saying the force point has been allocated
		item = Menu_FindItemByName(menu, "allocated_text");
		if (item)
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}

		// Show text saying the force point needs to be allocated
		item = Menu_FindItemByName(menu, "allocate_text");
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}
}

static int UI_CountForcePowers(void)
{
	const client_t* cl = &svs.clients[0];

	if (cl && cl->gentity)
	{
		const playerState_t* ps = cl->gentity->client;

		return ps->forcePowerLevel[FP_HEAL] +
			ps->forcePowerLevel[FP_TELEPATHY] +
			ps->forcePowerLevel[FP_PROTECT] +
			ps->forcePowerLevel[FP_ABSORB] +
			ps->forcePowerLevel[FP_GRIP] +
			ps->forcePowerLevel[FP_LIGHTNING] +
			ps->forcePowerLevel[FP_RAGE] +
			ps->forcePowerLevel[FP_DRAIN];
	}
	else
	{
		return uiInfo.forcePowerLevel[FP_HEAL] +
			uiInfo.forcePowerLevel[FP_TELEPATHY] +
			uiInfo.forcePowerLevel[FP_PROTECT] +
			uiInfo.forcePowerLevel[FP_ABSORB] +
			uiInfo.forcePowerLevel[FP_GRIP] +
			uiInfo.forcePowerLevel[FP_LIGHTNING] +
			uiInfo.forcePowerLevel[FP_RAGE] +
			uiInfo.forcePowerLevel[FP_DRAIN];
	}
}

//. Find weapons button and make active/inactive  (Used by Force Power Allocation screen)
static void UI_ForcePowerWeaponsButton(qboolean activeFlag)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// Cheats are on so lets always let us pass
	if ((trap_Cvar_VariableValue("helpUsObi") != 0) || (UI_CountForcePowers() >= 24))
	{
		activeFlag = qtrue;
	}

	const auto item = Menu_FindItemByName(menu, "weaponbutton");
	if (item)
	{
		// Make it active
		if (activeFlag)
		{
			item->window.flags &= ~WINDOW_INACTIVE;
		}
		else
		{
			item->window.flags |= WINDOW_INACTIVE;
		}
	}
}

void UI_SetItemColor(const itemDef_t* item, const char* itemname, const char* name, vec4_t color);

static void UI_SetHexPicLevel(const menuDef_t* menu, const int forcePowerI, const int powerLevel,
	const qboolean goldFlag)
{
	char itemName[128];

	// Find proper hex picture on menu
	Com_sprintf(itemName, sizeof itemName, "%s_hexpic", powerEnums[forcePowerI].title);
	auto item = Menu_FindItemByName(menu, itemName);

	// Now give it the proper hex graphic
	if (item)
	{
		char itemGraphic[128];
		if (goldFlag)
		{
			Com_sprintf(itemGraphic, sizeof itemGraphic, "gfx/menus/hex_pattern_%d_gold",
				powerLevel >= 4 ? 3 : powerLevel);
		}
		else
		{
			Com_sprintf(itemGraphic, sizeof itemGraphic, "gfx/menus/hex_pattern_%d", powerLevel >= 4 ? 3 : powerLevel);
		}

		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);

		Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[forcePowerI].title);
		item = Menu_FindItemByName(menu, itemName);
		if (item)
		{
			if (goldFlag)
			{
				// Change description text to tell player they can decrement the force point
				item->descText = "@MENUS_REMOVEFP";
			}
			else
			{
				// Change description text to tell player they can increment the force point
				item->descText = "@MENUS_ADDFP";
			}
		}
	}
}

void UI_SetItemVisible(const menuDef_t* menu, const char* itemname, qboolean visible);

// if this is the first time into the force power allocation screen, show the INSTRUCTION screen
static void UI_ForceHelpActive()
{
	const int tier_storyinfo = Cvar_VariableIntegerValue("tier_storyinfo");

	// First time, show instructions
	if (tier_storyinfo == 1)
	{
		//		Menus_OpenByName("ingameForceHelp");
		Menus_ActivateByName("ingameForceHelp");
	}
}

#ifndef JK2_MODE
// Set the force levels depending on the level chosen
static void UI_DemoSetForceLevels()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	char buffer[MAX_STRING_CHARS];

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	const playerState_t* pState = nullptr;
	if (cl)
	{
		pState = cl->gentity->client;
	}

	ui.Cvar_VariableStringBuffer("ui_demo_level", buffer, sizeof buffer);
	if (Q_stricmp(buffer, "t1_sour") == 0)
	{
		// NOTE : always set the uiInfo powers
		// level 1 in all core powers
		uiInfo.forcePowerLevel[FP_LEVITATION] = 1;
		uiInfo.forcePowerLevel[FP_SPEED] = 1;
		uiInfo.forcePowerLevel[FP_PUSH] = 1;
		uiInfo.forcePowerLevel[FP_PULL] = 1;
		uiInfo.forcePowerLevel[FP_SEE] = 1;
		uiInfo.forcePowerLevel[FP_SABER_OFFENSE] = 1;
		uiInfo.forcePowerLevel[FP_SABER_DEFENSE] = 1;
		uiInfo.forcePowerLevel[FP_SABERTHROW] = 1;
		// plus these extras
		uiInfo.forcePowerLevel[FP_HEAL] = 1;
		uiInfo.forcePowerLevel[FP_TELEPATHY] = 1;
		uiInfo.forcePowerLevel[FP_GRIP] = 1;

		// and set the rest to zero
		uiInfo.forcePowerLevel[FP_ABSORB] = 0;
		uiInfo.forcePowerLevel[FP_PROTECT] = 0;
		uiInfo.forcePowerLevel[FP_DRAIN] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING] = 0;
		uiInfo.forcePowerLevel[FP_RAGE] = 0;

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = 0;
		uiInfo.forcePowerLevel[FP_STASIS] = 0;

		uiInfo.forcePowerLevel[FP_GRASP] = 0;
		uiInfo.forcePowerLevel[FP_REPULSE] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = 0;
		uiInfo.forcePowerLevel[FP_FEAR] = 0;
		uiInfo.forcePowerLevel[FP_DEADLYSIGHT] = 0;
		uiInfo.forcePowerLevel[FP_BLAST] = 0;
		uiInfo.forcePowerLevel[FP_PROJECTION] = 0;
	}
	else
	{
		// level 3 in all core powers
		uiInfo.forcePowerLevel[FP_LEVITATION] = 3;
		uiInfo.forcePowerLevel[FP_SPEED] = 3;
		uiInfo.forcePowerLevel[FP_PUSH] = 3;
		uiInfo.forcePowerLevel[FP_PULL] = 3;
		uiInfo.forcePowerLevel[FP_SEE] = 3;
		uiInfo.forcePowerLevel[FP_SABER_OFFENSE] = 3;
		uiInfo.forcePowerLevel[FP_SABER_DEFENSE] = 3;
		uiInfo.forcePowerLevel[FP_SABERTHROW] = 3;

		// plus these extras
		uiInfo.forcePowerLevel[FP_HEAL] = 1;
		uiInfo.forcePowerLevel[FP_TELEPATHY] = 1;
		uiInfo.forcePowerLevel[FP_GRIP] = 2;
		uiInfo.forcePowerLevel[FP_LIGHTNING] = 1;
		uiInfo.forcePowerLevel[FP_PROTECT] = 1;

		// and set the rest to zero

		uiInfo.forcePowerLevel[FP_ABSORB] = 0;
		uiInfo.forcePowerLevel[FP_DRAIN] = 0;
		uiInfo.forcePowerLevel[FP_RAGE] = 0;

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = 0;
		uiInfo.forcePowerLevel[FP_STASIS] = 0;

		uiInfo.forcePowerLevel[FP_GRASP] = 0;
		uiInfo.forcePowerLevel[FP_REPULSE] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = 0;
		uiInfo.forcePowerLevel[FP_FEAR] = 0;
		uiInfo.forcePowerLevel[FP_DEADLYSIGHT] = 0;
		uiInfo.forcePowerLevel[FP_BLAST] = 0;
		uiInfo.forcePowerLevel[FP_PROJECTION] = 0;
	}

	if (pState)
	{
		//i am carrying over from a previous level, so get the increased power! (non-core only)
		uiInfo.forcePowerLevel[FP_HEAL] = Q_max(pState->forcePowerLevel[FP_HEAL], uiInfo.forcePowerLevel[FP_HEAL]);
		uiInfo.forcePowerLevel[FP_TELEPATHY] = Q_max(pState->forcePowerLevel[FP_TELEPATHY],
			uiInfo.forcePowerLevel[FP_TELEPATHY]);
		uiInfo.forcePowerLevel[FP_GRIP] = Q_max(pState->forcePowerLevel[FP_GRIP], uiInfo.forcePowerLevel[FP_GRIP]);
		uiInfo.forcePowerLevel[FP_LIGHTNING] = Q_max(pState->forcePowerLevel[FP_LIGHTNING],
			uiInfo.forcePowerLevel[FP_LIGHTNING]);
		uiInfo.forcePowerLevel[FP_PROTECT] = Q_max(pState->forcePowerLevel[FP_PROTECT],
			uiInfo.forcePowerLevel[FP_PROTECT]);

		uiInfo.forcePowerLevel[FP_ABSORB] =
			Q_max(pState->forcePowerLevel[FP_ABSORB], uiInfo.forcePowerLevel[FP_ABSORB]);
		uiInfo.forcePowerLevel[FP_DRAIN] = Q_max(pState->forcePowerLevel[FP_DRAIN], uiInfo.forcePowerLevel[FP_DRAIN]);
		uiInfo.forcePowerLevel[FP_RAGE] = Q_max(pState->forcePowerLevel[FP_RAGE], uiInfo.forcePowerLevel[FP_RAGE]);

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = Q_max(pState->forcePowerLevel[FP_DESTRUCTION],
			uiInfo.forcePowerLevel[FP_DESTRUCTION]);
		uiInfo.forcePowerLevel[FP_STASIS] =
			Q_max(pState->forcePowerLevel[FP_STASIS], uiInfo.forcePowerLevel[FP_STASIS]);

		uiInfo.forcePowerLevel[FP_GRASP] = Q_max(pState->forcePowerLevel[FP_GRASP], uiInfo.forcePowerLevel[FP_GRASP]);
		uiInfo.forcePowerLevel[FP_REPULSE] = Q_max(pState->forcePowerLevel[FP_REPULSE],
			uiInfo.forcePowerLevel[FP_REPULSE]);
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = Q_max(pState->forcePowerLevel[FP_LIGHTNING_STRIKE],
			uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE]);
		uiInfo.forcePowerLevel[FP_FEAR] = Q_max(pState->forcePowerLevel[FP_FEAR], uiInfo.forcePowerLevel[FP_FEAR]);
		uiInfo.forcePowerLevel[FP_DEADLYSIGHT] = Q_max(pState->forcePowerLevel[FP_DEADLYSIGHT],
			uiInfo.forcePowerLevel[FP_DEADLYSIGHT]);
		uiInfo.forcePowerLevel[FP_PROJECTION] = Q_max(pState->forcePowerLevel[FP_PROJECTION],
			uiInfo.forcePowerLevel[FP_PROJECTION]);
		uiInfo.forcePowerLevel[FP_BLAST] = Q_max(pState->forcePowerLevel[FP_BLAST], uiInfo.forcePowerLevel[FP_BLAST]);
	}
}
#endif // !JK2_MODE

// record the force levels into a cvar so when restoring player from map transition
// the force levels are set up correctly
static void UI_RecordForceLevels()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	auto s2 = "";
	for (const int i : uiInfo.forcePowerLevel)
	{
		s2 = va("%s %i", s2, i);
	}
	Cvar_Set("demo_playerfplvl", s2);
}

// record the weapons into a cvar so when restoring player from map transition
// the force levels are set up correctly
static void UI_RecordWeapons()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	int wpns = 0;
	// always add blaster and saber
	wpns |= 1 << WP_SABER;
	wpns |= 1 << WP_BLASTER_PISTOL;
	wpns |= 1 << uiInfo.selectedWeapon1;
	wpns |= 1 << uiInfo.selectedWeapon2;
	wpns |= 1 << uiInfo.selectedThrowWeapon;
	wpns |= 1 << uiInfo.selectedPistolWeapon;
	const char* s2 = va("%i", wpns);

	Cvar_Set("demo_playerwpns", s2);
}

// Shut down the help screen in the force power allocation screen
static void UI_ShutdownForceHelp()
{
	char itemName[128];
	itemDef_t* item;
	vec4_t color = { 0.65f, 0.65f, 0.65f, 1.0f };

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// Not in upgrade mode so turn on all the force buttons, the big forceicon and description text
	if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
	{
		// We just decremented a field so turn all buttons back on
		// Make it so all  buttons can be clicked
		for (const auto& powerEnum : powerEnums)
		{
			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnum.title);

			UI_SetItemVisible(menu, itemName, qtrue);
		}

		UI_SetItemVisible(menu, "force_icon", qtrue);
		UI_SetItemVisible(menu, "force_desc", qtrue);
		UI_SetItemVisible(menu, "leveltext", qtrue);

		// Set focus on the top left button
		item = Menu_FindItemByName(menu, "absorb_fbutton");
		item->window.flags |= WINDOW_HASFOCUS;

		if (item->onFocus)
		{
			Item_RunScript(item, item->onFocus);
		}
	}
	// In upgrade mode so just turn the deallocate button on
	else
	{
		UI_SetItemVisible(menu, "force_icon", qtrue);
		UI_SetItemVisible(menu, "force_desc", qtrue);
		UI_SetItemVisible(menu, "deallocate_fbutton", qtrue);

		item = Menu_FindItemByName(menu, va("%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title));
		if (item)
		{
			item->window.flags |= WINDOW_HASFOCUS;
		}

		// Get player state
		const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

		if (!cl) // No client, get out
		{
			return;
		}

		const playerState_t* pState = cl->gentity->client;

		if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
		{
			return;
		}

		// Update level description
		Com_sprintf(
			itemName,
			sizeof itemName,
			"%s_level%ddesc",
			powerEnums[uiInfo.forcePowerUpdated].title,
			pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]
		);

		item = Menu_FindItemByName(menu, itemName);
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	// If one was a chosen force power, high light it again.
	if (uiInfo.forcePowerUpdated > FP_UPDATED_NONE)
	{
		char itemhexName[128];
		char itemiconName[128];
		vec4_t color2 = { 1.0f, 1.0f, 1.0f, 1.0f };

		Com_sprintf(itemhexName, sizeof itemhexName, "%s_hexpic", powerEnums[uiInfo.forcePowerUpdated].title);
		Com_sprintf(itemiconName, sizeof itemiconName, "%s_iconpic", powerEnums[uiInfo.forcePowerUpdated].title);

		UI_SetItemColor(item, itemhexName, "forecolor", color2);
		UI_SetItemColor(item, itemiconName, "forecolor", color2);
	}
	else
	{
		// Un-grey-out all icons
		UI_SetItemColor(item, "hexpic", "forecolor", color);
		UI_SetItemColor(item, "iconpic", "forecolor", color);
	}
}

// Decrement force power level (Used by Force Power Allocation screen)
static void UI_DecrementCurrentForcePower()
{
	itemDef_t* item;
	vec4_t color = { 0.65f, 0.65f, 0.65f, 1.0f };

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	playerState_t* pState = nullptr;
	int forcelevel;

	if (cl)
	{
		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
	}

	if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
	{
		return;
	}

	DC->startLocalSound(uiInfo.uiDC.Assets.forceUnchosenSound, CHAN_AUTO);

	if (forcelevel > 0)
	{
		if (pState)
		{
			pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--; // Decrement it
			forcelevel = pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
			// Turn off power if level is 0
			if (pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum] < 1)
			{
				pState->forcePowersKnown &= ~(1 << powerEnums[uiInfo.forcePowerUpdated].powerEnum);
			}
		}
		else
		{
			uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--; // Decrement it
			forcelevel = uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
		}
	}

	UI_SetHexPicLevel(menu, uiInfo.forcePowerUpdated, forcelevel, qfalse);

	UI_ShowForceLevelDesc(powerEnums[uiInfo.forcePowerUpdated].title);

	// We just decremented a field so turn all buttons back on
	// Make it so all  buttons can be clicked
	for (const auto& powerEnum : powerEnums)
	{
		char itemName[128];
		Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnum.title);
		item = Menu_FindItemByName(menu, itemName);
		if (item) // This is okay, because core powers don't have a hex button
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	// Show point has not been allocated
	UI_SetPowerTitleText(qfalse);

	// Make weapons button inactive
	UI_ForcePowerWeaponsButton(qfalse);

	// Hide the deallocate button
	item = Menu_FindItemByName(menu, "deallocate_fbutton");
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE; //

		// Un-grey-out all icons
		UI_SetItemColor(item, "hexpic", "forecolor", color);
		UI_SetItemColor(item, "iconpic", "forecolor", color);
	}

	item = Menu_FindItemByName(menu, va("%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title));
	if (item)
	{
		item->window.flags |= WINDOW_HASFOCUS;
	}

	uiInfo.forcePowerUpdated = FP_UPDATED_NONE; // It's as if nothing happened.
}

void Item_MouseEnter(itemDef_t* item, float x, float y);

// Try to increment force power level (Used by Force Power Allocation screen)
static void UI_AffectForcePowerLevel(const char* forceName)
{
	short forcePowerI = 0;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex(forceName, &forcePowerI))
	{
		return;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	playerState_t* pState = nullptr;
	int forcelevel;
	if (cl)
	{
		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	if (forcelevel > 2)
	{
		// Too big, can't be incremented
		return;
	}

	// Increment power level.
	DC->startLocalSound(uiInfo.uiDC.Assets.forceChosenSound, CHAN_AUTO);

	uiInfo.forcePowerUpdated = forcePowerI; // Remember which power was updated

	if (pState)
	{
		pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]++; // Increment it
		pState->forcePowersKnown |= 1 << powerEnums[forcePowerI].powerEnum;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum]++; // Increment it
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	UI_SetHexPicLevel(menu, uiInfo.forcePowerUpdated, forcelevel, qtrue);

	UI_ShowForceLevelDesc(forceName);

	// A field was updated, so make it so others can't be
	if (uiInfo.forcePowerUpdated > FP_UPDATED_NONE)
	{
		itemDef_t* item;
		vec4_t color = { 0.25f, 0.25f, 0.25f, 1.0f };

		// Make it so none of the other buttons can be clicked
		for (short i = 0; i < MAX_POWER_ENUMS; i++)
		{
			char itemName[128];
			if (i == uiInfo.forcePowerUpdated)
			{
				continue;
			}

			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[i].title);
			item = Menu_FindItemByName(menu, itemName);
			if (item) // This is okay, because core powers don't have a hex button
			{
				item->window.flags &= ~WINDOW_VISIBLE;
			}
		}

		// Show point has been allocated
		UI_SetPowerTitleText(qtrue);

		// Make weapons button active
		UI_ForcePowerWeaponsButton(qtrue);

		// Make user_info
		Cvar_Set("ui_forcepower_inc", va("%d", uiInfo.forcePowerUpdated));

		// Just grab an item to hand it to the function.
		item = Menu_FindItemByName(menu, "deallocate_fbutton");
		if (item)
		{
			// Show all icons as greyed-out
			UI_SetItemColor(item, "hexpic", "forecolor", color);
			UI_SetItemColor(item, "iconpic", "forecolor", color);

			item->window.flags |= WINDOW_HASFOCUS;
		}
	}
}

static void UI_DecrementForcePowerLevel()
{
	const int forcePowerI = Cvar_VariableIntegerValue("ui_forcepower_inc");
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	playerState_t* pState = cl->gentity->client;

	pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]--; // Decrement it
}

// Show force level description that matches current player level (Used by Force Power Allocation screen)
static void UI_ShowForceLevelDesc(const char* forceName)
{
	short forcePowerI = 0;
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex(forceName, &forcePowerI))
	{
		return;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}
	const playerState_t* pState = cl->gentity->client;

	char itemName[128];

	// Update level description
	Com_sprintf(
		itemName,
		sizeof itemName,
		"%s_level%ddesc",
		powerEnums[forcePowerI].title,
		pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]
	);

	const auto item = Menu_FindItemByName(menu, itemName);
	if (item)
	{
		item->window.flags |= WINDOW_VISIBLE;
	}
}

// Reset force level powers screen to what it was before player upgraded them (Used by Force Power Allocation screen)
static void UI_ResetForceLevels()
{
	// What force ppower had the point added to it?
	if (uiInfo.forcePowerUpdated != FP_UPDATED_NONE)
	{
		// Get player state
		const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

		if (!cl) // No client, get out
		{
			return;
		}
		playerState_t* pState = cl->gentity->client;

		// Decrement that power
		pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--;

		itemDef_t* item;

		const menuDef_t* menu = Menu_GetFocused(); // Get current menu

		if (!menu)
		{
			return;
		}

		char itemName[128];

		// Make it so all  buttons can be clicked
		for (const auto& powerEnum : powerEnums)
		{
			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnum.title);
			item = Menu_FindItemByName(menu, itemName);
			if (item) // This is okay, because core powers don't have a hex button
			{
				item->window.flags |= WINDOW_VISIBLE;
			}
		}

		UI_SetPowerTitleText(qfalse);

		Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title);
		item = Menu_FindItemByName(menu, itemName);
		if (item)
		{
			// Change description text to tell player they can increment the force point
			item->descText = "@MENUS_ADDFP";
		}

		uiInfo.forcePowerUpdated = FP_UPDATED_NONE;
	}

	UI_ForcePowerWeaponsButton(qfalse);
}

#ifndef JK2_MODE
// Set the Players known saber style
static void UI_UpdateFightingStyle()
{
	int saberStyle;

	const int fightingStyle = Cvar_VariableIntegerValue("ui_newfightingstyle");

	if (fightingStyle == 1)
	{
		saberStyle = SS_MEDIUM;
	}
	else if (fightingStyle == 2)
	{
		saberStyle = SS_STRONG;
	}
	else if (fightingStyle == 3)
	{
		saberStyle = SS_DUAL;
	}
	else if (fightingStyle == 4)
	{
		saberStyle = SS_STAFF;
	}
	else // 0 is Fast
	{
		saberStyle = SS_FAST;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// No client, get out
	if (cl && cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;
		pState->saberStylesKnown |= 1 << saberStyle;
	}
	else // Must be at the beginning of the game so the client hasn't been created, shove data in a cvar
	{
		Cvar_Set("g_fighting_style", va("%d", saberStyle));
	}
}
#endif // !JK2_MODE

static void UI_ResetCharacterListBoxes()
{
	const menuDef_t* menu = Menus_FindByName("characterMenu");

	if (menu)
	{
		listBoxDef_t* listPtr;
		auto item = Menu_FindItemByName(menu, "headlistbox");
		if (item)
		{
			const auto list_box_def_s = static_cast<listBoxDef_t*>(item->typeData);
			if (list_box_def_s)
			{
				list_box_def_s->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = Menu_FindItemByName(menu, "torsolistbox");
		if (item)
		{
			listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = Menu_FindItemByName(menu, "lowerlistbox");
		if (item)
		{
			listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = Menu_FindItemByName(menu, "colorbox");
		if (item)
		{
			listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}
	}
}

static void UI_ClearInventory()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		for (int& i : pState->inventory)
		{
			i = 0;
		}
	}
}

static void UI_GiveInventory(const int itemIndex, const int amount)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (itemIndex < MAX_INVENTORY)
		{
			pState->inventory[itemIndex] = amount;
		}
	}
}

//. Find weapons allocation screen BEGIN button and make active/inactive
static void UI_WeaponAllocBeginButton(const qboolean activeFlag)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const int weap = Cvar_VariableIntegerValue("weapon_menu");

	itemDef_t* item = Menu_GetMatchingItemByNumber(menu, weap, "beginmission");

	if (item)
	{
		// Make it active
		if (activeFlag)
		{
			item->window.flags &= ~WINDOW_INACTIVE;
		}
		else
		{
			item->window.flags |= WINDOW_INACTIVE;
		}
	}
}

// If we have both weapons and the throwable weapon, turn on the begin mission button,
// otherwise, turn it off
static void UI_WeaponsSelectionsComplete()
{
	// We need two weapons and one throwable
	if (uiInfo.selectedWeapon1 != NOWEAPON &&
		uiInfo.selectedWeapon2 != NOWEAPON &&
		uiInfo.selectedThrowWeapon != NOWEAPON &&
		uiInfo.selectedPistolWeapon != NOWEAPON)
	{
		UI_WeaponAllocBeginButton(qtrue); // Turn it on
	}
	else
	{
		UI_WeaponAllocBeginButton(qfalse); // Turn it off
	}
}

// if this is the first time into the weapon allocation screen, show the INSTRUCTION screen
static void UI_WeaponHelpActive()
{
	const int tier_storyinfo = Cvar_VariableIntegerValue("tier_storyinfo");

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// First time, show instructions
	if (tier_storyinfo == 1)
	{
		UI_SetItemVisible(menu, "weapon_button", qfalse);

		UI_SetItemVisible(menu, "inst_stuff", qtrue);
	}
	// just act like normal
	else
	{
		UI_SetItemVisible(menu, "weapon_button", qtrue);

		UI_SetItemVisible(menu, "inst_stuff", qfalse);
	}
}

static void UI_InitWeaponSelect()
{
	UI_WeaponAllocBeginButton(qfalse);
	uiInfo.selectedWeapon1 = NOWEAPON;
	uiInfo.selectedWeapon2 = NOWEAPON;
	uiInfo.selectedThrowWeapon = NOWEAPON;
	uiInfo.selectedPistolWeapon = NOWEAPON;
}

static void UI_clearsabers()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		// Clear out any weapons for the player
		if (pState->stats[STAT_WEAPONS] |= 1 << WP_SABER)
		{
			pState->stats[STAT_WEAPONS] |= 1 << WP_MELEE;

			pState->weapon = WP_MELEE;
		}
	}
}

static void UI_ClearWeapons()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		// Clear out any weapons for the player
		for (char& weapon : pState->weapons)
		{
			weapon = 0;
		}

		pState->weapon = WP_NONE;
	}
}

static void UI_GiveAmmo(const int ammoIndex, const int ammoAmount, const char* soundfile)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (ammoIndex < AMMO_MAX)
		{
			pState->ammo[ammoIndex] = ammoAmount;
		}

		if (soundfile)
		{
			DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
		}
	}
}

static void UI_GiveWeapon(const int weaponIndex)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (weaponIndex < WP_NUM_WEAPONS)
		{
			pState->weapons[weaponIndex] = 1;
		}
	}
}

static void UI_EquipWeapon(const int weaponIndex)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (weaponIndex < WP_NUM_WEAPONS)
		{
			pState->weapon = weaponIndex;
			//force it to change
			//CG_ChangeWeapon( wp );
		}
	}
}

static void UI_LoadMissionSelectMenu(const char* cvarName)
{
	const int holdLevel = static_cast<int>(trap_Cvar_VariableValue(cvarName));

	// Figure out which tier menu to load
	if (holdLevel > 0 && holdLevel < 5)
	{
		UI_LoadMenus("ui/tier1.txt", qfalse);

		Menus_CloseByName("ingameMissionSelect1");
	}
	else if (holdLevel > 6 && holdLevel < 10)
	{
		UI_LoadMenus("ui/tier2.txt", qfalse);

		Menus_CloseByName("ingameMissionSelect2");
	}
	else if (holdLevel > 11 && holdLevel < 15)
	{
		UI_LoadMenus("ui/tier3.txt", qfalse);

		Menus_CloseByName("ingameMissionSelect3");
	}
}

// Update the player weapons with the chosen weapon
static void UI_AddWeaponSelection(const int weaponIndex, const int ammoIndex, const int ammoAmount,
	const char* iconItemName, const char* litIconItemName, const char* hexBackground,
	const char* soundfile)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const itemDef_s* iconItem = Menu_FindItemByName(menu, iconItemName);
	const itemDef_s* litIconItem = Menu_FindItemByName(menu, litIconItemName);

	const char* chosenItemName, * chosenButtonName;

	// has this weapon already been chosen?
	if (weaponIndex == uiInfo.selectedWeapon1)
	{
		UI_RemoveWeaponSelection(1);
		return;
	}
	if (weaponIndex == uiInfo.selectedWeapon2)
	{
		UI_RemoveWeaponSelection(2);
		return;
	}

	// See if either slot is empty
	if (uiInfo.selectedWeapon1 == NOWEAPON)
	{
		chosenItemName = "chosenweapon1_icon";
		chosenButtonName = "chosenweapon1_button";
		uiInfo.selectedWeapon1 = weaponIndex;
		uiInfo.selectedWeapon1AmmoIndex = ammoIndex;

		memcpy(uiInfo.selectedWeapon1ItemName, hexBackground, sizeof uiInfo.selectedWeapon1ItemName);

		//Save the lit and unlit icons for the selected weapon slot
		uiInfo.litWeapon1Icon = litIconItem->window.background;
		uiInfo.unlitWeapon1Icon = iconItem->window.background;

		uiInfo.weapon1ItemButton = uiInfo.runScriptItem;
		uiInfo.weapon1ItemButton->descText = "@MENUS_CLICKREMOVE";
	}
	else if (uiInfo.selectedWeapon2 == NOWEAPON)
	{
		chosenItemName = "chosenweapon2_icon";
		chosenButtonName = "chosenweapon2_button";
		uiInfo.selectedWeapon2 = weaponIndex;
		uiInfo.selectedWeapon2AmmoIndex = ammoIndex;

		memcpy(uiInfo.selectedWeapon2ItemName, hexBackground, sizeof uiInfo.selectedWeapon2ItemName);

		//Save the lit and unlit icons for the selected weapon slot
		uiInfo.litWeapon2Icon = litIconItem->window.background;
		uiInfo.unlitWeapon2Icon = iconItem->window.background;

		uiInfo.weapon2ItemButton = uiInfo.runScriptItem;
		uiInfo.weapon2ItemButton->descText = "@MENUS_CLICKREMOVE";
	}
	else // Both slots are used, can't add it.
	{
		return;
	}

	auto item = Menu_FindItemByName(menu, chosenItemName);
	if (item && iconItem)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on chosenweapon button so player can unchoose the weapon
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = Menu_FindItemByName(menu, hexBackground);
	if (item)
	{
		item->window.foreColor[0] = 0;
		item->window.foreColor[1] = 1;
		item->window.foreColor[2] = 0;
		item->window.foreColor[3] = 1;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl)
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weaponIndex > 0 && weaponIndex < WP_NUM_WEAPONS)
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if (ammoIndex > 0 && ammoIndex < AMMO_MAX)
			{
				pState->ammo[ammoIndex] = ammoAmount;
			}
		}
	}

	if (soundfile)
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
	}

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_AddPistolSelection(const int weaponIndex, const int ammoIndex, const int ammoAmount,
	const char* iconItemName, const char* litIconItemName, const char* hexBackground,
	const char* soundfile)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const itemDef_s* iconItem = Menu_FindItemByName(menu, iconItemName);
	const itemDef_s* litIconItem = Menu_FindItemByName(menu, litIconItemName);

	// Has a throw weapon already been chosen?
	if (uiInfo.selectedPistolWeapon != NOWEAPON)
	{
		// Clicked on the selected throwable weapon
		if (uiInfo.selectedPistolWeapon == weaponIndex)
		{
			// Deselect it
			UI_Removepistolselection();
		}
		return;
	}

	const auto chosenItemName = "chosenpistol1_icon";
	const auto chosenButtonName = "chosenpistol1_button";
	uiInfo.selectedPistolWeapon = weaponIndex;
	uiInfo.selectedPistolWeaponAmmoIndex = ammoIndex;
	uiInfo.PistolArmButton = uiInfo.runScriptItem;

	if (uiInfo.PistolArmButton)
	{
		uiInfo.PistolArmButton->descText = "@MENUS_CLICKREMOVE";
	}

	memcpy(uiInfo.selectedPistolWeaponItemName, hexBackground, sizeof uiInfo.selectedWeapon1ItemName);

	//Save the lit and unlit icons for the selected weapon slot
	uiInfo.litPistolIcon = litIconItem->window.background;
	uiInfo.unlitPistolIcon = iconItem->window.background;

	auto item = Menu_FindItemByName(menu, chosenItemName);
	if (item && iconItem)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on throwchosenweapon button so player can unchoose the weapon
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = Menu_FindItemByName(menu, hexBackground);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 1.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weaponIndex > 0 && weaponIndex < WP_NUM_WEAPONS)
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if (ammoIndex > 0 && ammoIndex < AMMO_MAX)
			{
				pState->ammo[ammoIndex] = ammoAmount;
			}
		}
	}

	if (soundfile)
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
	}

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

// Update the player weapons with the chosen weapon
static void UI_RemoveWeaponSelection(const int weapon_selection_index)
{
	const char* chosen_item_name, * chosen_button_name, * background;
	int ammo_index, weapon_index;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	// Which item has it?
	if (weapon_selection_index == 1)
	{
		chosen_item_name = "chosenweapon1_icon";
		chosen_button_name = "chosenweapon1_button";
		background = uiInfo.selectedWeapon1ItemName;
		ammo_index = uiInfo.selectedWeapon1AmmoIndex;
		weapon_index = uiInfo.selectedWeapon1;

		if (uiInfo.weapon1ItemButton)
		{
			uiInfo.weapon1ItemButton->descText = "@MENUS_CLICKSELECT";
			uiInfo.weapon1ItemButton = nullptr;
		}
	}
	else if (weapon_selection_index == 2)
	{
		chosen_item_name = "chosenweapon2_icon";
		chosen_button_name = "chosenweapon2_button";
		background = uiInfo.selectedWeapon2ItemName;
		ammo_index = uiInfo.selectedWeapon2AmmoIndex;
		weapon_index = uiInfo.selectedWeapon2;

		if (uiInfo.weapon2ItemButton)
		{
			uiInfo.weapon2ItemButton->descText = "@MENUS_CLICKSELECT";
			uiInfo.weapon2ItemButton = nullptr;
		}
	}
	else
	{
		return;
	}

	// Reset background of upper icon
	auto item = Menu_FindItemByName(menu, background);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.5f;
		item->window.foreColor[2] = 0.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = Menu_FindItemByName(menu, chosen_item_name);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = Menu_FindItemByName(menu, chosen_button_name);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weapon_index > 0 && weapon_index < WP_NUM_WEAPONS)
			{
				pState->weapons[weapon_index] = 0;
			}

			// Remove ammo too
			if (ammo_index > 0 && ammo_index < AMMO_MAX)
			{
				// But don't take it away if the other weapon is using that ammo
				if (uiInfo.selectedWeapon1AmmoIndex != uiInfo.selectedWeapon2AmmoIndex)
				{
					pState->ammo[ammo_index] = 0;
				}
			}
		}
	}

	// Now do a little clean up
	if (weapon_selection_index == 1)
	{
		uiInfo.selectedWeapon1 = NOWEAPON;
		memset(uiInfo.selectedWeapon1ItemName, 0, sizeof uiInfo.selectedWeapon1ItemName);
		uiInfo.selectedWeapon1AmmoIndex = 0;
	}
	else if (weapon_selection_index == 2)
	{
		uiInfo.selectedWeapon2 = NOWEAPON;
		memset(uiInfo.selectedWeapon2ItemName, 0, sizeof uiInfo.selectedWeapon2ItemName);
		uiInfo.selectedWeapon2AmmoIndex = 0;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL);
#endif

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_Removepistolselection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	// Weapon not chosen
	if (uiInfo.selectedPistolWeapon == NOWEAPON)
	{
		return;
	}

	const auto chosenItemName = "chosenpistol1_icon";
	const auto chosenButtonName = "chosenpistol1_button";
	const char* background = uiInfo.selectedPistolWeaponItemName;

	// Reset background of upper icon
	auto item = Menu_FindItemByName(menu, background);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 0.5f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = Menu_FindItemByName(menu, chosenItemName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (uiInfo.selectedPistolWeapon > 0 && uiInfo.selectedPistolWeapon < WP_NUM_WEAPONS)
			{
				pState->weapons[uiInfo.selectedPistolWeapon] = 0;
			}

			// Remove ammo too
			if (uiInfo.selectedPistolWeaponAmmoIndex > 0 && uiInfo.selectedPistolWeaponAmmoIndex < AMMO_MAX)
			{
				pState->ammo[uiInfo.selectedPistolWeaponAmmoIndex] = 0;
			}
		}
	}

	// Now do a little clean up
	uiInfo.selectedPistolWeapon = NOWEAPON;
	memset(uiInfo.selectedPistolWeaponItemName, 0, sizeof uiInfo.selectedPistolWeaponItemName);
	uiInfo.selectedPistolWeaponAmmoIndex = 0;

	if (uiInfo.PistolArmButton)
	{
		uiInfo.PistolArmButton->descText = "@MENUS_CLICKSELECT";
		uiInfo.PistolArmButton = nullptr;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL);
#endif

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_NormalWeaponSelection(const int selectionslot)
{
	itemDef_s* item;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	if (selectionslot == 1)
	{
		item = Menu_FindItemByName(menu, "chosenweapon1_icon");
		if (item)
		{
			item->window.background = uiInfo.unlitWeapon1Icon;
		}
	}

	if (selectionslot == 2)
	{
		item = Menu_FindItemByName(menu, "chosenweapon2_icon");
		if (item)
		{
			item->window.background = uiInfo.unlitWeapon2Icon;
		}
	}
}

static void UI_Normalpistolselection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenpistol1_icon");
	item->window.background = uiInfo.unlitPistolIcon;
}

static void UI_HighLightWeaponSelection(const int selectionslot)
{
	itemDef_s* item;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	if (selectionslot == 1)
	{
		item = Menu_FindItemByName(menu, "chosenweapon1_icon");
		if (item)
		{
			item->window.background = uiInfo.litWeapon1Icon;
		}
	}

	if (selectionslot == 2)
	{
		item = Menu_FindItemByName(menu, "chosenweapon2_icon");
		if (item)
		{
			item->window.background = uiInfo.litWeapon2Icon;
		}
	}
}

static void UI_Highlightpistolselection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenpistol1_icon");
	item->window.background = uiInfo.litPistolIcon;
}

// Update the player throwable weapons (okay it's a bad description) with the chosen weapon
static void UI_AddThrowWeaponSelection(const int weaponIndex, const int ammoIndex, const int ammoAmount,
	const char* iconItemName, const char* litIconItemName, const char* hexBackground,
	const char* soundfile)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const itemDef_s* iconItem = Menu_FindItemByName(menu, iconItemName);
	const itemDef_s* litIconItem = Menu_FindItemByName(menu, litIconItemName);

	// Has a throw weapon already been chosen?
	if (uiInfo.selectedThrowWeapon != NOWEAPON)
	{
		// Clicked on the selected throwable weapon
		if (uiInfo.selectedThrowWeapon == weaponIndex)
		{
			// Deselect it
			UI_RemoveThrowWeaponSelection();
		}
		return;
	}

	const auto chosenItemName = "chosenthrowweapon_icon";
	const auto chosenButtonName = "chosenthrowweapon_button";
	uiInfo.selectedThrowWeapon = weaponIndex;
	uiInfo.selectedThrowWeaponAmmoIndex = ammoIndex;
	uiInfo.weaponThrowButton = uiInfo.runScriptItem;

	if (uiInfo.weaponThrowButton)
	{
		uiInfo.weaponThrowButton->descText = "@MENUS_CLICKREMOVE";
	}

	memcpy(uiInfo.selectedThrowWeaponItemName, hexBackground, sizeof uiInfo.selectedWeapon1ItemName);

	//Save the lit and unlit icons for the selected weapon slot
	uiInfo.litThrowableIcon = litIconItem->window.background;
	uiInfo.unlitThrowableIcon = iconItem->window.background;

	auto item = Menu_FindItemByName(menu, chosenItemName);
	if (item && iconItem)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on throwchosenweapon button so player can unchoose the weapon
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = Menu_FindItemByName(menu, hexBackground);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 1.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weaponIndex > 0 && weaponIndex < WP_NUM_WEAPONS)
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if (ammoIndex > 0 && ammoIndex < AMMO_MAX)
			{
				pState->ammo[ammoIndex] = ammoAmount;
			}
		}
	}

	if (soundfile)
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
	}

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

// Update the player weapons with the chosen throw weapon
static void UI_RemoveThrowWeaponSelection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	// Weapon not chosen
	if (uiInfo.selectedThrowWeapon == NOWEAPON)
	{
		return;
	}

	const auto chosenItemName = "chosenthrowweapon_icon";
	const auto chosenButtonName = "chosenthrowweapon_button";
	const char* background = uiInfo.selectedThrowWeaponItemName;

	// Reset background of upper icon
	auto item = Menu_FindItemByName(menu, background);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 0.5f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = Menu_FindItemByName(menu, chosenItemName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (uiInfo.selectedThrowWeapon > 0 && uiInfo.selectedThrowWeapon < WP_NUM_WEAPONS)
			{
				pState->weapons[uiInfo.selectedThrowWeapon] = 0;
			}

			// Remove ammo too
			if (uiInfo.selectedThrowWeaponAmmoIndex > 0 && uiInfo.selectedThrowWeaponAmmoIndex < AMMO_MAX)
			{
				pState->ammo[uiInfo.selectedThrowWeaponAmmoIndex] = 0;
			}
		}
	}

	// Now do a little clean up
	uiInfo.selectedThrowWeapon = NOWEAPON;
	memset(uiInfo.selectedThrowWeaponItemName, 0, sizeof uiInfo.selectedThrowWeaponItemName);
	uiInfo.selectedThrowWeaponAmmoIndex = 0;

	if (uiInfo.weaponThrowButton)
	{
		uiInfo.weaponThrowButton->descText = "@MENUS_CLICKSELECT";
		uiInfo.weaponThrowButton = nullptr;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL);
#endif

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_NormalThrowSelection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenthrowweapon_icon");
	item->window.background = uiInfo.unlitThrowableIcon;
}

static void UI_HighLightThrowSelection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenthrowweapon_icon");
	item->window.background = uiInfo.litThrowableIcon;
}

static void UI_GetSaberCvars()
{
	Cvar_Set("ui_saber_type", Cvar_VariableString("g_saber_type"));
	if (!Cvar_VariableIntegerValue("ui_npc_saber") && !Cvar_VariableIntegerValue("ui_npc_sabertwo"))
	{
		Cvar_Set("ui_saber", Cvar_VariableString("g_saber"));
		Cvar_Set("ui_saber2", Cvar_VariableString("g_saber2"));
		Cvar_Set("ui_saber_color", Cvar_VariableString("g_saber_color"));
		Cvar_Set("ui_saber2_color", Cvar_VariableString("g_saber2_color"));
	}
	else
	{
		Cvar_Set("ui_npc_saber", Cvar_VariableString("g_NPCsaber"));
		Cvar_Set("ui_npc_sabertwo", Cvar_VariableString("g_NPCsabertwo"));
		Cvar_Set("ui_npc_sabercolor", Cvar_VariableString("g_NPCsabercolor"));
		Cvar_Set("ui_npc_sabertwocolor", Cvar_VariableString("g_NPCsabertwocolor"));
	}

	const saber_colors_t saberColour = TranslateSaberColor(Cvar_VariableString("ui_saber_color"));

	if (saberColour >= SABER_RGB)
	{
		Cvar_SetValue("ui_rgb_saber_red", saberColour & 0xff);
		Cvar_SetValue("ui_rgb_saber_green", saberColour >> 8 & 0xff);
		Cvar_SetValue("ui_rgb_saber_blue", saberColour >> 16 & 0xff);
	}

	const saber_colors_t saber2Colour = TranslateSaberColor(Cvar_VariableString("ui_saber2_color"));

	if (saber2Colour >= SABER_RGB)
	{
		Cvar_SetValue("ui_rgb_saber2_red", saber2Colour & 0xff);
		Cvar_SetValue("ui_rgb_saber2_green", saber2Colour >> 8 & 0xff);
		Cvar_SetValue("ui_rgb_saber2_blue", saber2Colour >> 16 & 0xff);
	}

	Cvar_Set("ui_newfightingstyle", "0");
}

static void UI_ResetSaberCvars()
{
	Cvar_Set("g_saber_type", "single");
	Cvar_Set("g_saber", "single_1");
	Cvar_Set("g_saber2", "");

	Cvar_Set("ui_saber_type", "single");
	Cvar_Set("ui_saber", "single_1");
	Cvar_Set("ui_saber2", "");
}

extern qboolean ItemParse_asset_model_go(itemDef_t* item, const char* name);
extern qboolean ItemParse_model_g2skin_go(itemDef_t* item, const char* skinName);

static void UI_UpdateCharacterSkin()
{
	char skin[MAX_QPATH];

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "character");

	if (!item)
	{
		Com_Error(ERR_FATAL, "UI_UpdateCharacterSkin: Could not find item (character) in menu (%s)", menu->window.name);
	}

	Com_sprintf(skin, sizeof skin, "models/players/%s/|%s|%s|%s",
		Cvar_VariableString("ui_char_model"),
		Cvar_VariableString("ui_char_skin_head"),
		Cvar_VariableString("ui_char_skin_torso"),
		Cvar_VariableString("ui_char_skin_legs")
	);

	ItemParse_model_g2skin_go(item, skin);
}

static void UI_UpdateCharacter(const qboolean changedModel)
{
	char modelPath[MAX_QPATH];

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "character");

	if (!item)
	{
		Com_Error(ERR_FATAL, "UI_UpdateCharacter: Could not find item (character) in menu (%s)", menu->window.name);
	}

	ItemParse_model_g2anim_go(item, ui_char_anim.string);

	Com_sprintf(modelPath, sizeof modelPath, "models/players/%s/model.glm", Cvar_VariableString("ui_char_model"));
	ItemParse_asset_model_go(item, modelPath);

	if (changedModel)
	{
		//set all skins to first skin since we don't know you always have all skins
		//FIXME: could try to keep the same spot in each list as you swtich models
		UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, 0, item); //fixme, this is not really the right item!!
		UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, 0, item);
		UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, 0, item);
		UI_FeederSelection(FEEDER_COLORCHOICES, 0, item);
	}
	UI_UpdateCharacterSkin();
}

void UI_UpdateSaberType()
{
	char sType[MAX_QPATH];
	DC->getCVarString("ui_saber_type", sType, sizeof sType);
	if (Q_stricmp("single", sType) == 0 ||
		Q_stricmp("staff", sType) == 0)
	{
		DC->setCVar("ui_saber2", "");
	}
}

static void UI_UpdateSaberHilt(const qboolean secondSaber)
{
	char model[MAX_QPATH];
	char modelPath[MAX_QPATH];
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

	if (!menu)
	{
		return;
	}

	const char* itemName;
	const char* saberCvarName;
	if (secondSaber)
	{
		itemName = "saber2";
		saberCvarName = "ui_saber2";
	}
	else
	{
		itemName = "saber";
		saberCvarName = "ui_saber";
	}

	const auto item = Menu_FindItemByName(menu, itemName);

	if (!item)
	{
		Com_Error(ERR_FATAL, "UI_UpdateSaberHilt: Could not find item (%s) in menu (%s)", itemName, menu->window.name);
	}
	DC->getCVarString(saberCvarName, model, sizeof model);
	//read this from the sabers.cfg
	if (UI_SaberModelForSaber(model, modelPath))
	{
		char skinPath[MAX_QPATH];
		//successfully found a model
		ItemParse_asset_model_go(item, modelPath); //set the model
		//get the customSkin, if any
		//COM_StripExtension( modelPath, skinPath, sizeof(skinPath) );
		//COM_DefaultExtension( skinPath, sizeof( skinPath ), ".skin" );
		if (UI_SaberSkinForSaber(model, skinPath))
		{
			ItemParse_model_g2skin_go(item, skinPath); //apply the skin
		}
		else
		{
			ItemParse_model_g2skin_go(item, nullptr); //apply the skin
		}
	}
}

char GoToMenu[1024];

/*
=================
Menus_SaveGoToMenu
=================
*/
static void Menus_SaveGoToMenu(const char* menuTo)
{
	memcpy(GoToMenu, menuTo, sizeof GoToMenu);
}

/*
=================
UI_CheckVid1Data
=================
*/
void UI_CheckVid1Data(const char* menuTo, const char* warningMenuName)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

	if (!menu)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: No videoMenu was found. Video data could not be checked\n");
		return;
	}

	const itemDef_t* applyChanges = Menu_FindItemByName(menu, "applyChanges");

	if (!applyChanges)
	{
		//		Menus_CloseAll();
		Menus_OpenByName(menuTo);
		return;
	}

	if (applyChanges->window.flags & WINDOW_VISIBLE) // Is the APPLY CHANGES button active?
	{
		Menus_OpenByName(warningMenuName); // Give warning
	}
	else
	{
		//		Menus_CloseAll();
		//		Menus_OpenByName(menuTo);
	}
}

/*
=================
UI_ResetDefaults
=================
*/
void UI_ResetDefaults()
{
	ui.Cmd_ExecuteText(EXEC_APPEND, "cvar_restart\n");
	Controls_SetDefaults();
	ui.Cmd_ExecuteText(EXEC_APPEND, "exec MD-SP-default.cfg\n");
	ui.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
}

/*
=======================
UI_SortSaveGames
=======================
*/
static int UI_SortSaveGames(const void* A, const void* B)
{
	const int& a = ((savedata_t*)A)->currentSaveFileDateTime;
	const int& b = ((savedata_t*)B)->currentSaveFileDateTime;

	if (a > b)
	{
		return -1;
	}
	return a < b;
}

/*
=======================
UI_AdjustSaveGameListBox
=======================
*/
// Yeah I could get fired for this... in a world of good and bad, this is bad
// I wish we passed in the menu item to RunScript(), oh well...
void UI_AdjustSaveGameListBox(const int currentLine)
{
	// could be in either the ingame or shell load menu (I know, I know it's bad)
	const menuDef_t* menu = Menus_FindByName("loadgameMenu");
	if (!menu)
	{
		menu = Menus_FindByName("ingameloadMenu");
	}

	if (menu)
	{
		const auto item = Menu_FindItemByName(menu, "loadgamelist");
		if (item)
		{
			const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = currentLine;
			}

			item->cursorPos = currentLine;
		}
	}
}

/*
=================
ReadSaveDirectory
=================
*/
//JLFSAVEGAME MPNOTUSED
void ReadSaveDirectory()
{
	char* holdChar;
	int len;
	int fileCnt;
	// Clear out save data
	memset(s_savedata, 0, sizeof s_savedata);
	s_savegame.saveFileCnt = 0;
	Cvar_Set("ui_gameDesc", ""); // Blank out comment
	Cvar_Set("ui_SelectionOK", "0");
#ifdef JK2_MODE
	memset(screenShotBuf, 0, (SG_SCR_WIDTH * SG_SCR_HEIGHT * 4)); //blank out sshot
#endif

	// Get everything in saves directory
	fileCnt = ui.FS_GetFileList("Account/Saved-Missions-MovieDuels/", ".sav", s_savegame.listBuf, LISTBUFSIZE);

	Cvar_Set("ui_ResumeOK", "0");
	holdChar = s_savegame.listBuf;
	for (int i = 0; i < fileCnt; i++)
	{
		// strip extension
		len = strlen(holdChar);
		holdChar[len - 4] = '\0';

		if (Q_stricmp("current", holdChar) != 0)
		{
			time_t result;
			if (Q_stricmp("auto", holdChar) == 0)
			{
				Cvar_Set("ui_ResumeOK", "1");
			}
			else
			{
				// Is this a valid file??? & Get comment of file
				result = ui.SG_GetSaveGameComment(holdChar, s_savedata[s_savegame.saveFileCnt].currentSaveFileComments,
					s_savedata[s_savegame.saveFileCnt].currentSaveFileMap);
				if (result != 0) // ignore Bad save game
				{
					s_savedata[s_savegame.saveFileCnt].currentSaveFileName = holdChar;
					s_savedata[s_savegame.saveFileCnt].currentSaveFileDateTime = result;

					const tm* localTime = localtime(&result);
					strcpy(s_savedata[s_savegame.saveFileCnt].currentSaveFileDateTimeString, asctime(localTime));
					s_savegame.saveFileCnt++;
					if (s_savegame.saveFileCnt == MAX_SAVELOADFILES)
					{
						break;
					}
				}
			}
		}

		holdChar += len + 1; //move to next item
	}

	qsort(s_savedata, s_savegame.saveFileCnt, sizeof(savedata_t), UI_SortSaveGames);
}