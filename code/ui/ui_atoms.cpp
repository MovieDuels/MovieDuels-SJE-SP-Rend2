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

/**********************************************************************
	UI_ATOMS.C

	User interface building blocks and support functions.
**********************************************************************/

#include "../server/exe_headers.h"

#include "ui_local.h"
#include "gameinfo.h"
#include "../qcommon/stv_version.h"

uiimport_t ui;
uiStatic_t uis;

//externs
static void UI_LoadMenu_f();
static void UI_SaveMenu_f();
extern vmCvar_t ui_com_outcast;
extern vmCvar_t ui_com_rend2;

//locals

/*
=================
UI_ForceMenuOff
=================
*/
void UI_ForceMenuOff()
{
	ui.Key_SetCatcher(ui.Key_GetCatcher() & ~KEYCATCH_UI);
	ui.Key_ClearStates();
	ui.Cvar_Set("cl_paused", "0");
}

/*
=================
UI_SetActiveMenu -
	this should be the ONLY way the menu system is brought up

=================
*/
void UI_SetActiveMenu(const char* menuname, const char* menuID)
{
	// this should be the ONLY way the menu system is brought up (besides the UI_ConsoleCommand below)

	if (cls.state != CA_DISCONNECTED && !ui.SG_GameAllowedToSaveHere(qtrue)) //don't check full sytem, only if incamera
	{
		return;
	}

	if (!menuname)
	{
		UI_ForceMenuOff();
		return;
	}

	//make sure force-speed and slowmodeath doesn't slow down menus - NOTE: they should reset the timescale when the game un-pauses
	Cvar_SetValue("timescale", 1.0f);

	UI_Cursor_Show(qtrue);

	// enusure minumum menu data is cached
	Menu_Cache();

	if (Q_stricmp(menuname, "mainMenu") == 0)
	{
		UI_MainMenu();
		return;
	}

	if (Q_stricmp(menuname, "ingame") == 0)
	{
		ui.Cvar_Set("cl_paused", "1");
		UI_InGameMenu(menuID);
		return;
	}

	if (Q_stricmp(menuname, "datapad") == 0)
	{
		ui.Cvar_Set("cl_paused", "1");
		UI_DataPadMenu();
		return;
	}
#ifndef JK2_MODE
	if (Q_stricmp(menuname, "missionfailed_menu") == 0)
	{
		Menus_CloseAll();
		Menus_ActivateByName("missionfailed_menu");
		ui.Key_SetCatcher(KEYCATCH_UI);
	}
#endif
}

/*
=================
UI_Argv
=================
*/
static char* UI_Argv(const int arg)
{
	static char buffer[MAX_STRING_CHARS];

	ui.Argv(arg, buffer, sizeof(buffer));

	return buffer;
}

/*
=================
UI_Cvar_VariableString
=================
*/
char* UI_Cvar_VariableString(const char* var_name)
{
	static char buffer[MAX_STRING_CHARS];

	ui.Cvar_VariableStringBuffer(var_name, buffer, sizeof(buffer));

	return buffer;
}

/*
=================
UI_Cache
=================
*/
static void UI_Cache_f()
{
	Menu_Cache();
#ifndef JK2_MODE
	for (int index = 0; index < 5; index++)
	{
		extern const char* kyleForceStatusSounds[];
		extern const char* lukeForceStatusSounds[];
		DC->registerSound(lukeForceStatusSounds[index], qfalse);
		DC->registerSound(kyleForceStatusSounds[index], qfalse);
	}
	for (int index = 1; index <= 18; index++)
	{
		DC->registerSound(va("sound/chars/storyinfo/%d", index), qfalse);
	}
	trap_S_RegisterSound("sound/chars/kyle/04kyk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/kyle/05kyk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/kyle/07kyk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/kyle/14kyk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/kyle/21kyk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/kyle/24kyk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/kyle/25kyk001.mp3", qfalse);

	trap_S_RegisterSound("sound/chars/luke/06luk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/luke/08luk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/luke/22luk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/luke/23luk001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/protocol/12pro001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/protocol/15pro001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/protocol/16pro001.mp3", qfalse);
	trap_S_RegisterSound("sound/chars/wedge/13wea001.mp3", qfalse);

	Menus_ActivateByName("ingameMissionSelect1");
	Menus_ActivateByName("ingameMissionSelect2");
	Menus_ActivateByName("ingameMissionSelect3");
#endif
}

/*
=================
UI_ConsoleCommand
=================
*/
#ifndef JK2_MODE
void UI_Load(); //in UI_main.cpp
#endif

#ifdef NEW_FEEDER
void UI_OpenMenu_f(void) {
	Menus_CloseAll();
	if (Menus_ActivateByName(UI_Argv(1)))
		trap_Key_SetCatcher(KEYCATCH_UI);
}
#endif

qboolean UI_ConsoleCommand()
{
	if (!ui.SG_GameAllowedToSaveHere(qtrue)) //only check if incamera
	{
		return qfalse;
	}

	const char* cmd = UI_Argv(0);

	// ensure minimum menu data is available
	Menu_Cache();

	if (Q_stricmp(cmd, "ui_cache") == 0)
	{
		UI_Cache_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "levelselect") == 0)
	{
		UI_LoadMenu_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "ui_teamOrders") == 0)
	{
		UI_SaveMenu_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "ui_report") == 0)
	{
		UI_Report();
		return qtrue;
	}

#ifndef JK2_MODE
	if (Q_stricmp(cmd, "ui_load") == 0)
	{
		UI_Load();
		return qtrue;
	}
#endif
#ifdef NEW_FEEDER
	if (Q_stricmp(cmd, "ui_openmenu") == 0)
	{
		UI_OpenMenu_f();
		return qtrue;
	}
#endif

	return qfalse;
}

/*
=================
UI_Init
=================
*/
void UI_Init(const int apiVersion, const uiimport_t* uiimport, const qboolean inGameLoad)
{
	ui = *uiimport;

	if (apiVersion != UI_API_VERSION)
	{
		ui.Error(ERR_FATAL, "Bad UI_API_VERSION: expected %i, got %i\n", UI_API_VERSION, apiVersion);
	}

	// get static data (glconfig, media)
	ui.GetGlconfig(&uis.glconfig);

	uis.scaley = uis.glconfig.vidHeight * (1.0 / 480.0);
	uis.scalex = uis.glconfig.vidWidth * (1.0 / 640.0);

	Menu_Cache();

	ui.Cvar_Create("cg_drawCrosshair", "1", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_marks", "1", CVAR_ARCHIVE);
	ui.Cvar_Create("s_language", "english", CVAR_ARCHIVE | CVAR_NORESTART);
#ifndef JK2_MODE
	ui.Cvar_Create("g_char_model", "jedi_hm", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_char_skin_head", "head_a1", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_char_skin_torso", "torso_a1", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_char_skin_legs", "lower_a1", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_char_color_red", "255", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_char_color_green", "255", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_char_color_blue", "255", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_saber_type", "single", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_saber", "single_1", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_saber2", "", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_saber_color", "yellow", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_saber2_color", "yellow", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	ui.Cvar_Create("g_NPCsaber", "single_1", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_NPCsabertwo", "", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_NPCsabercolor", "red", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("g_NPCsabertwocolor", "red", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	ui.Cvar_Create("ui_forcepower_inc", "0", CVAR_ROM | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("tier_storyinfo", "0", CVAR_ROM | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("tiers_complete", "", CVAR_ROM | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("ui_prisonerobj_currtotal", "0", CVAR_ROM | CVAR_SAVEGAME | CVAR_NORESTART);
	ui.Cvar_Create("ui_prisonerobj_mintotal", "0", CVAR_ROM | CVAR_SAVEGAME | CVAR_NORESTART);

	ui.Cvar_Create("g_dismemberment", "3", CVAR_ARCHIVE); //0 = none, 1 = arms and hands, 2 = legs, 3 = waist and head
	ui.Cvar_Create("cg_gunAutoFirst", "0", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_crosshairIdentifyTarget", "1", CVAR_ARCHIVE);
	ui.Cvar_Create("g_subtitles", "0", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_marks", "1", CVAR_ARCHIVE);
	ui.Cvar_Create("d_slowmodeath", "2", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_shadows", "1", CVAR_ARCHIVE);

	ui.Cvar_Create("cg_runpitch", "0.002", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_runroll", "0.005", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_bobup", "0.005", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_bobpitch", "0.002", CVAR_ARCHIVE);
	ui.Cvar_Create("cg_bobroll", "0.002", CVAR_ARCHIVE);

	ui.Cvar_Create("ui_disableWeaponSway", "0", CVAR_ARCHIVE);

	ui.Cvar_Create("d_slowmoaction", "1", CVAR_ARCHIVE);

#endif

	_UI_Init(inGameLoad);
}

/*
================
UI_DrawNamedPic
=================
*/
void UI_DrawNamedPic(const float x, const float y, const float width, const float height, const char* picname)
{
	const qhandle_t hShader = ui.R_RegisterShaderNoMip(picname);
	ui.R_DrawStretchPic(x, y, width, height, 0, 0, 1, 1, hShader);
}

/*
================
UI_DrawHandlePic
=================
*/
void UI_DrawHandlePic(const float x, const float y, float w, float h, const qhandle_t hShader)
{
	float s0;
	float s1;
	float t0;
	float t1;

	if (w < 0)
	{
		// flip about horizontal
		w = -w;
		s0 = 1;
		s1 = 0;
	}
	else
	{
		s0 = 0;
		s1 = 1;
	}

	if (h < 0)
	{
		// flip about vertical
		h = -h;
		t0 = 1;
		t1 = 0;
	}
	else
	{
		t0 = 0;
		t1 = 1;
	}

	ui.R_DrawStretchPic(x, y, w, h, s0, t0, s1, t1, hShader);
}

/*
================
UI_FillRect

Coordinates are 640*480 virtual values
=================
*/
void UI_FillRect(const float x, const float y, const float width, const float height, const float* color)
{
	ui.R_SetColor(color);

	ui.R_DrawStretchPic(x, y, width, height, 0, 0, 0, 0, uis.whiteShader);

	ui.R_SetColor(nullptr);
}

/*
=================
UI_UpdateScreen
=================
*/
void UI_UpdateScreen()
{
	ui.UpdateScreen();
}

/*
===============
UI_LoadMenu_f
===============
*/
static void UI_LoadMenu_f()
{
	trap_Key_SetCatcher(KEYCATCH_UI);
	Menus_ActivateByName("ingameloadMenu");
}

/*
===============
UI_SaveMenu_f
===============
*/
static void UI_SaveMenu_f()
{
#ifdef JK2_MODE
	ui.PrecacheScreenshot();
#endif

	trap_Key_SetCatcher(KEYCATCH_UI);
	Menus_ActivateByName("ingamesaveMenu");
}

//--------------------------------------------

/*
=================
UI_SetColor
=================
*/
void UI_SetColor(const float* rgba)
{
	trap_R_SetColor(rgba);
}

/*int registeredFontCount = 0;
#define MAX_FONTS 6
static fontInfo_t registeredFont[MAX_FONTS];
*/

/*
=================
UI_RegisterFont
=================
*/

int UI_RegisterFont(const char* fontName)
{
	int iFontIndex = ui.R_RegisterFont(fontName);
	if (iFontIndex == 0)
	{
		iFontIndex = ui.R_RegisterFont("ergoec"); // fall back
	}

	return iFontIndex;
}