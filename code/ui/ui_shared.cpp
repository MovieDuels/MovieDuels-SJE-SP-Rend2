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

//
// string allocation/managment

// leave this at the top of all UI_xxxx files for PCH reasons...
//
#include "../server/exe_headers.h"

#include "ui_local.h"

//rww - added for ui ghoul2 models
#define UI_SHARED_CPP

#include "../game/anims.h"
#include "../cgame/animtable.h"

#include "ui_shared.h"
#include "menudef.h"

#include "qcommon/stringed_ingame.h"

void UI_LoadMenus(const char* menuFile, qboolean reset);

extern vmCvar_t ui_char_color_red;
extern vmCvar_t ui_char_color_green;
extern vmCvar_t ui_char_color_blue;

extern vmCvar_t ui_char_model_angle;
extern void UI_SaberLoadParms();
extern void UI_CacheSaberGlowGraphics();

bool initialLoad = true;
bool AlsoLoad = true;

void* UI_Alloc(int size);

void Controls_GetConfig();
void Fade(int* flags, float* f, float clamp, int* nextTime, int offsetTime, qboolean bFlags, float fadeAmount);
void Item_Init(itemDef_t* item);
void Item_InitControls(itemDef_t* item);
qboolean Item_Parse(itemDef_t* item);
void Item_RunScript(itemDef_t* item, const char* s);
void Item_SetupKeywordHash();
void Item_Text_AutoWrapped_Paint(itemDef_t* item);
void Item_UpdatePosition(itemDef_t* item);
void Item_ValidateTypeData(itemDef_t* item);
void LerpColor(vec4_t a, vec4_t b, vec4_t c, float t);
itemDef_t* Menu_FindItemByName(const menuDef_t* menu, const char* p);
itemDef_t* Menu_GetMatchingItemByNumber(const menuDef_t* menu, int index, const char* name);
void Menu_Paint(menuDef_t* menu, qboolean forcePaint);
void Menu_SetupKeywordHash();
void Menus_ShowItems(const char* menuName);
qboolean ParseRect(const char** p, rectDef_t* r);
const char* String_Alloc(const char* p);
void ToWindowCoords(float* x, float* y, const windowDef_t* window);
void Window_Paint(Window* w, float fadeAmount, float fadeClamp, float fadeCycle);
int Item_ListBox_ThumbDrawPosition(itemDef_t* item);
int Item_ListBox_ThumbPosition(itemDef_t* item);
int Item_ListBox_MaxScroll(const itemDef_t* item);
static qboolean Rect_ContainsPoint(const rectDef_t* rect, float x, float y);
static qboolean Item_Paint(itemDef_t* item, qboolean bDraw);
int Item_TextScroll_ThumbDrawPosition(itemDef_t* item);
static void Item_TextScroll_BuildLines(const itemDef_t* item);

//static qboolean debugMode = qfalse;
static qboolean g_waitingForKey = qfalse;
static qboolean g_editingField = qfalse;

static itemDef_t* g_bindItem = nullptr;
static itemDef_t* g_editItem = nullptr;
static itemDef_t* itemCapture = nullptr; // item that has the mouse captured ( if any )

constexpr auto DOUBLE_CLICK_DELAY = 300;
static int lastListBoxClickTime = 0;

static void (*captureFunc)(void* p) = nullptr;
static void* captureData = nullptr;

//const char defaultString[10] = {"default"};
#ifdef CGAME
#define MEM_POOL_SIZE  (256 * 1024)
#else
constexpr auto MEM_POOL_SIZE = 8 * 1024 * 1024;
#endif

constexpr auto SCROLL_TIME_START = 500;
constexpr auto SCROLL_TIME_ADJUST = 150;
constexpr auto SCROLL_TIME_ADJUSTOFFSET = 40;
constexpr auto SCROLL_TIME_FLOOR = 20;

using scrollInfo_t = struct scrollInfo_s
{
	int nextScrollTime;
	int nextAdjustTime;
	int adjustValue;
	int scrollKey;
	float xStart;
	float yStart;
	itemDef_t* item;
	qboolean scrollDir;
};

static scrollInfo_t scrollInfo;

static char memoryPool[MEM_POOL_SIZE];
static int allocPoint, outOfMemory;

displayContextDef_t* DC = nullptr;

menuDef_t Menus[MAX_MENUS]; // defined menus
int menuCount = 0; // how many

menuDef_t* menuStack[MAX_OPEN_MENUS];
int openMenuCount = 0;

static int strPoolIndex = 0;
static char strPool[STRING_POOL_SIZE];

using stringDef_t = struct stringDef_s
{
	stringDef_s* next;
	const char* str;
};

constexpr auto HASH_TABLE_SIZE = 2048;

static int strHandleCount = 0;
static stringDef_t* strHandle[HASH_TABLE_SIZE];

using itemFlagsDef_t = struct itemFlagsDef_s
{
	const char* string;
	int value;
};

itemFlagsDef_t itemFlags[] = {
	{"WINDOW_INACTIVE", WINDOW_INACTIVE},
	{nullptr, 0}
};

const char* styles[] = {
	"WINDOW_STYLE_EMPTY",
	"WINDOW_STYLE_FILLED",
	"WINDOW_STYLE_GRADIENT",
	"WINDOW_STYLE_SHADER",
	"WINDOW_STYLE_TEAMCOLOR",
	"WINDOW_STYLE_CINEMATIC",
	nullptr
};

const char* types[] = {
	"ITEM_TYPE_TEXT",
	"ITEM_TYPE_BUTTON",
	"ITEM_TYPE_RADIOBUTTON",
	"ITEM_TYPE_CHECKBOX",
	"ITEM_TYPE_EDITFIELD",
	"ITEM_TYPE_COMBO",
	"ITEM_TYPE_LISTBOX",
	"ITEM_TYPE_MODEL",
	"ITEM_TYPE_OWNERDRAW",
	"ITEM_TYPE_NUMERICFIELD",
	"ITEM_TYPE_SLIDER",
	"ITEM_TYPE_YESNO",
	"ITEM_TYPE_MULTI",
	"ITEM_TYPE_BIND",
	"ITEM_TYPE_TEXTSCROLL",
	"ITEM_TYPE_SLIDER_INTEGER",
	"ITEM_TYPE_SLIDER_ROTATE",
	nullptr
};

const char* alignment[] = {
	"ITEM_ALIGN_LEFT",
	"ITEM_ALIGN_CENTER",
	"ITEM_ALIGN_RIGHT",
	nullptr
};

/*
==================
Init_Display

Initializes the display with a structure to all the drawing routines
 ==================
*/
void Init_Display(displayContextDef_t* dc)
{
	DC = dc;
}

/*
==================
Window_Init

Initializes a window structure ( windowDef_t ) with defaults

==================
*/
static void Window_Init(Window* w)
{
	memset(w, 0, sizeof(windowDef_t));
	w->borderSize = 1;
	w->foreColor[0] = w->foreColor[1] = w->foreColor[2] = w->foreColor[3] = 1.0;
	w->cinematic = -1;
}

/*
=================
PC_SourceError
=================
*/
static void PC_SourceError(int handle, char* format, ...)
{
	char filename[128]{};
	va_list argptr;
	static char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof string, format, argptr);
	va_end(argptr);

	filename[0] = '\0';
	constexpr int line = 0;

	Com_Printf(S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string);
}

/*
=================
PC_ParseStringMem
=================
*/
qboolean PC_ParseStringMem(const char** out)
{
	const char* temp;

	if (PC_ParseString(&temp))
	{
		return qfalse;
	}

	*out = String_Alloc(temp);

	return qtrue;
}

/*
=================
PC_ParseRect
=================
*/
static qboolean PC_ParseRect(rectDef_t* r)
{
	if (!PC_ParseFloat(&r->x))
	{
		if (!PC_ParseFloat(&r->y))
		{
			if (!PC_ParseFloat(&r->w))
			{
				if (!PC_ParseFloat(&r->h))
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
=================
PC_Script_Parse
=================
*/
qboolean PC_Script_Parse(const char** out)
{
	char script[4096]{};

	script[0] = 0;
	// scripts start with { and have ; separated command lists.. commands are command, arg..
	// basically we want everything between the { } as it will be interpreted at run time

	char* token2 = PC_ParseExt();
	if (!token2)
	{
		return qfalse;
	}

	if (*token2 != '{')
	{
		return qfalse;
	}

	while (true)
	{
		token2 = PC_ParseExt();
		if (!token2)
		{
			return qfalse;
		}

		if (*token2 == '}') // End of the script?
		{
			*out = String_Alloc(script);
			return qtrue;
		}

		if (*(token2 + 1) != '\0')
		{
			Q_strcat(script, sizeof script, va("\"%s\"", token2));
		}
		else
		{
			Q_strcat(script, sizeof script, token2);
		}
		Q_strcat(script, sizeof script, " ");
	}
}

//--------------------------------------------------------------------------------------------
//	Menu Keyword Parse functions
//--------------------------------------------------------------------------------------------

/*
=================
MenuParse_font
=================
*/
static qboolean MenuParse_font(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_ParseStringMem(&menu->font))
	{
		return qfalse;
	}

	if (!DC->Assets.fontRegistered)
	{
		DC->Assets.qhMediumFont = DC->registerFont(menu->font);
		DC->Assets.fontRegistered = qtrue;
	}
	return qtrue;
}

/*
=================
MenuParse_name
=================
*/
static qboolean MenuParse_name(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_ParseStringMem((const char**)&menu->window.name))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_fullscreen
=================
*/

static qboolean MenuParse_fullscreen(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt((int*)&menu->fullScreen))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_rect
=================
*/

static qboolean MenuParse_rect(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_ParseRect(&menu->window.rect))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_style
=================
*/
static qboolean MenuParse_style(itemDef_t* item)
{
	const char* tempStr;
	const auto menu = (menuDef_t*)item;

	if (PC_ParseString(&tempStr))
	{
		return qfalse;
	}

	int i = 0;
	while (styles[i])
	{
		if (Q_stricmp(tempStr, styles[i]) == 0)
		{
			menu->window.style = i;
			break;
		}
		i++;
	}

	if (styles[i] == nullptr)
	{
		PC_ParseWarning(va("Unknown menu style value '%s'", tempStr));
	}
	return qtrue;
}

/*
=================
MenuParse_visible
=================
*/
static qboolean MenuParse_visible(itemDef_t* item)
{
	int i;
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&i))
	{
		return qfalse;
	}

	if (i)
	{
		menu->window.flags |= WINDOW_VISIBLE;
	}
	return qtrue;
}

/*
=================
MenuParse_ignoreescape
=================
*/
static qboolean MenuParse_ignoreescape(itemDef_t* item)
{
	int i;
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&i))
	{
		return qfalse;
	}

	if (i)
	{
		menu->window.flags |= WINDOW_IGNORE_ESCAPE;
	}
	return qtrue;
}

/*
=================
MenuParse_onOpen
=================
*/
static qboolean MenuParse_onOpen(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_Script_Parse(&menu->onOpen))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_onClose
=================
*/
static qboolean MenuParse_onClose(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_Script_Parse(&menu->onClose))
	{
		return qfalse;
	}
	return qtrue;
}

//JLFACCEPT MPMOVED
/*
=================
MenuParse_onAccept
=================
*/
static qboolean MenuParse_onAccept(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_Script_Parse(&menu->onAccept))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_onESC
=================
*/
static qboolean MenuParse_onESC(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_Script_Parse(&menu->onESC))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_border
=================
*/
static qboolean MenuParse_border(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&menu->window.border))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_borderSize
=================
*/
static qboolean MenuParse_borderSize(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseFloat(&menu->window.borderSize))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_backcolor
=================
*/
static qboolean MenuParse_backcolor(itemDef_t* item)
{
	float f;
	const auto menu = (menuDef_t*)item;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		menu->window.backColor[i] = f;
	}
	return qtrue;
}

/*
=================
MenuParse_forecolor
=================
*/
static qboolean MenuParse_forecolor(itemDef_t* item)
{
	float f;
	const auto menu = (menuDef_t*)item;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		if (f < 0)
		{
			//special case for player color
			menu->window.flags |= WINDOW_PLAYERCOLOR;
			return qtrue;
		}
		menu->window.foreColor[i] = f;
		menu->window.flags |= WINDOW_FORECOLORSET;
	}
	return qtrue;
}

/*
=================
MenuParse_bordercolor
=================
*/
static qboolean MenuParse_bordercolor(itemDef_t* item)
{
	float f;
	const auto menu = (menuDef_t*)item;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		menu->window.borderColor[i] = f;
	}
	return qtrue;
}

/*
=================
MenuParse_focuscolor
=================
*/
static qboolean MenuParse_focuscolor(itemDef_t* item)
{
	float f;
	const auto menu = (menuDef_t*)item;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		menu->focusColor[i] = f;
	}
	return qtrue;
}

/*
=================
MenuParse_focuscolor
=================
*/
static qboolean MenuParse_appearanceIncrement(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseFloat(&menu->appearanceIncrement))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_descAlignment
=================
*/
static qboolean MenuParse_descAlignment(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;
	const char* tempStr;

	if (PC_ParseString(&tempStr))
	{
		return qfalse;
	}

	int i = 0;
	while (alignment[i])
	{
		if (Q_stricmp(tempStr, alignment[i]) == 0)
		{
			menu->descAlignment = i;
			break;
		}
		i++;
	}

	if (alignment[i] == nullptr)
	{
		PC_ParseWarning(va("Unknown desc alignment value '%s'", tempStr));
	}

	return qtrue;
}

/*
=================
MenuParse_descTextStyle
=================
*/
static qboolean MenuParse_descTextStyle(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&menu->descTextStyle))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_descX
=================
*/
static qboolean MenuParse_descX(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&menu->descX))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_descY
=================
*/
static qboolean MenuParse_descY(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&menu->descY))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_descScale
=================
*/
static qboolean MenuParse_descScale(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseFloat(&menu->descScale))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_descColor
=================
*/
static qboolean MenuParse_descColor(itemDef_t* item)
{
	float f;
	const auto menu = (menuDef_t*)item;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		menu->descColor[i] = f;
	}
	return qtrue;
}

/*
=================
MenuParse_disablecolor
=================
*/
static qboolean MenuParse_disablecolor(itemDef_t* item)
{
	float f;
	const auto menu = (menuDef_t*)item;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		menu->disableColor[i] = f;
	}
	return qtrue;
}

/*
=================
MenuParse_outlinecolor
=================
*/
static qboolean MenuParse_outlinecolor(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseColor(&menu->window.outlineColor))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_background
=================
*/
static qboolean MenuParse_background(itemDef_t* item)
{
	const char* buff;
	const auto menu = (menuDef_t*)item;

	if (PC_ParseString(&buff))
	{
		return qfalse;
	}

	menu->window.background = ui.R_RegisterShaderNoMip(buff);
	return qtrue;
}

/*
=================
MenuParse_cinematic
=================
*/
static qboolean MenuParse_cinematic(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_ParseStringMem(&menu->window.cinematicName))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_ownerdrawFlag
=================
*/
static qboolean MenuParse_ownerdrawFlag(itemDef_t* item)
{
	int i;
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&i))
	{
		return qfalse;
	}
	menu->window.ownerDrawFlags |= i;
	return qtrue;
}

/*
=================
MenuParse_ownerdraw
=================
*/
static qboolean MenuParse_ownerdraw(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&menu->window.ownerDraw))
	{
		return qfalse;
	}
	return qtrue;
}

/*
=================
MenuParse_popup
=================
*/
static qboolean MenuParse_popup(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;
	menu->window.flags |= WINDOW_POPUP;
	return qtrue;
}

/*
=================
MenuParse_outOfBounds
=================
*/
static qboolean MenuParse_outOfBounds(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	menu->window.flags |= WINDOW_OOB_CLICK;
	return qtrue;
}

/*
=================
MenuParse_soundLoop
=================
*/
static qboolean MenuParse_soundLoop(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (!PC_ParseStringMem(&menu->soundName))
	{
		return qfalse;
	}
	return qtrue;
}

/*
================
MenuParse_fadeClamp
================
*/
static qboolean MenuParse_fadeClamp(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseFloat(&menu->fadeClamp))
	{
		return qfalse;
	}
	return qtrue;
}

/*
================
MenuParse_fadeAmount
================
*/
static qboolean MenuParse_fadeAmount(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseFloat(&menu->fadeAmount))
	{
		return qfalse;
	}
	return qtrue;
}

/*
================
MenuParse_fadeCycle
================
*/
static qboolean MenuParse_fadeCycle(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;

	if (PC_ParseInt(&menu->fadeCycle))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
Item_ApplyHacks
Hacks to fix issues with Team Arena menu scripts
===============
*/
static void Item_ApplyHacks(itemDef_t* item)
{
#if !defined(_WIN32) || ( defined(_WIN32) && defined(idx64) )
	// Fix length of favorite address in createfavorite.menu
	if (item->type == ITEM_TYPE_MULTI && item->cvar && !Q_stricmp(item->cvar, "s_UseOpenAL")) {
		if (item->parent)
		{
			menuDef_t* parent = (menuDef_t*)item->parent;
			VectorSet4(parent->disableColor, 0.5f, 0.5f, 0.5f, 1.0f);
			item->disabled = qtrue;
			// Just in case it had focus
			item->window.flags &= ~WINDOW_MOUSEOVER;
			Com_Printf("Disabling eax field because current platform does not support EAX.\n");
		}
	}

	if (item->type == ITEM_TYPE_TEXT && item->window.name && !Q_stricmp(item->window.name, "eax_icon") && item->cvarTest && !Q_stricmp(item->cvarTest, "s_UseOpenAL") && item->enableCvar && (item->cvarFlags & CVAR_HIDE)) {
		if (item->parent)
		{
			menuDef_t* parent = (menuDef_t*)item->parent;
			VectorSet4(parent->disableColor, 0.5f, 0.5f, 0.5f, 1.0f);
			item->disabled = item->disabledHidden = qtrue;
			// Just in case it had focus
			item->window.flags &= ~WINDOW_MOUSEOVER;
			Com_Printf("Hiding eax_icon object because current platform does not support EAX.\n");
		}
	}
#endif

	if (item->type == ITEM_TYPE_MULTI && item->window.name && !Q_stricmp(item->window.name, "sound_quality"))
	{
		auto multiPtr = static_cast<multiDef_t*>(item->typeData);
		qboolean found = qfalse;
		for (int i = 0; i < multiPtr->count; i++)
		{
			if (multiPtr->cvarValue[i] == 44)
			{
				found = qtrue;
				break;
			}
		}
		if (!found && multiPtr->count < MAX_MULTI_CVARS)
		{
#ifdef JK2_MODE
			multiPtr->cvarList[multiPtr->count] = String_Alloc("@MENUS0_VERY_HIGH");
#else
			multiPtr->cvarList[multiPtr->count] = String_Alloc("@MENUS_VERY_HIGH");
#endif
			multiPtr->cvarValue[multiPtr->count] = 44;
			multiPtr->count++;
			Com_Printf("Extended sound quality field to contain very high option.\n");
		}
	}

	if (item->type == ITEM_TYPE_MULTI && item->window.name && !Q_stricmp(item->window.name, "voice") && item->cvar && !
		Q_stricmp(item->cvar, "g_subtitles"))
	{
		auto multiPtr = static_cast<multiDef_t*>(item->typeData);
		qboolean found = qfalse;
		for (int i = 0; i < multiPtr->count; i++)
		{
			if (multiPtr->cvarValue[i] == 1)
			{
				found = qtrue;
				break;
			}
		}
		if (!found && multiPtr->count < MAX_MULTI_CVARS)
		{
#ifdef JK2_MODE
			multiPtr->cvarList[multiPtr->count] = String_Alloc("@MENUS3_ALL_VOICEOVERS");
#else
			multiPtr->cvarList[multiPtr->count] = String_Alloc("@MENUS_ALL_VOICEOVERS");
#endif
			multiPtr->cvarValue[multiPtr->count] = 1;
			multiPtr->count++;
			Com_Printf("Extended subtitles field to contain all voiceovers option.\n");
		}
	}

#ifdef JK2_MODE
	if (item->type == ITEM_TYPE_MULTI && item->window.name && !Q_stricmp(item->window.name, "video_mode") && item->cvar && !Q_stricmp(item->cvar, "r_ext_texture_filter_anisotropic")) {
		{
			memset(item->typeData, 0, sizeof(multiDef_t));
		}
		editFieldDef_t* editPtr = nullptr;

		item->cvarFlags = CVAR_DISABLE;
		item->type = ITEM_TYPE_SLIDER;

		Item_ValidateTypeData(item);

		editPtr = (editFieldDef_t*)item->typeData;
		editPtr->minVal = 0.5f;
		editPtr->maxVal = cls.glconfig.maxTextureFilterAnisotropy;
		editPtr->defVal = 1.0f;
		Com_Printf("Converted anisotropic filter field to slider.\n");
	}
#endif
}

/*
================
MenuParse_itemDef
================
*/
static qboolean MenuParse_itemDef(itemDef_t* item)
{
	const auto menu = (menuDef_t*)item;
	if (menu->itemCount < MAX_MENUITEMS)
	{
		itemDef_t* newItem = menu->items[menu->itemCount] = static_cast<itemDef_s*>(UI_Alloc(sizeof(itemDef_t)));
		Item_Init(newItem);
		if (!Item_Parse(newItem))
		{
			return qfalse;
		}
		Item_InitControls(newItem);
		newItem->parent = menu->items[menu->itemCount]->parent = menu;
		menu->itemCount++;
		Item_ApplyHacks(newItem);
	}
	else
	{
		PC_ParseWarning(va("Exceeded item/menu max of %d", MAX_MENUITEMS));
	}
	return qtrue;
}

constexpr auto KEYWORDHASH_SIZE = 512;

using keywordHash_t = struct keywordHash_s
{
	const char* keyword;
	qboolean(*func)(itemDef_t* item);
	keywordHash_s* next;
};

keywordHash_t menuParseKeywords[] = {
	{"appearanceIncrement", MenuParse_appearanceIncrement},
	{"backcolor", MenuParse_backcolor,},
	{"background", MenuParse_background,},
	{"border", MenuParse_border,},
	{"bordercolor", MenuParse_bordercolor,},
	{"borderSize", MenuParse_borderSize,},
	{"cinematic", MenuParse_cinematic,},
	{"descAlignment", MenuParse_descAlignment},
	{"descTextStyle", MenuParse_descTextStyle},
	{"desccolor", MenuParse_descColor},
	{"descX", MenuParse_descX},
	{"descY", MenuParse_descY},
	{"descScale", MenuParse_descScale},
	{"disablecolor", MenuParse_disablecolor,},
	{"fadeClamp", MenuParse_fadeClamp,},
	{"fadeCycle", MenuParse_fadeCycle,},
	{"fadeAmount", MenuParse_fadeAmount,},
	{"focuscolor", MenuParse_focuscolor,},
	{"font", MenuParse_font,},
	{"forecolor", MenuParse_forecolor,},
	{"fullscreen", MenuParse_fullscreen,},
	{"itemDef", MenuParse_itemDef,},
	{"name", MenuParse_name,},
	{"onClose", MenuParse_onClose,},
	//JLFACCEPT MPMOVED
	{"onAccept", MenuParse_onAccept,},
	{"onESC", MenuParse_onESC,},
	{"onOpen", MenuParse_onOpen,},
	{"outlinecolor", MenuParse_outlinecolor,},
	{"outOfBoundsClick", MenuParse_outOfBounds,},
	{"ownerdraw", MenuParse_ownerdraw,},
	{"ownerdrawFlag", MenuParse_ownerdrawFlag,},
	{"popup", MenuParse_popup,},
	{"rect", MenuParse_rect,},
	{"soundLoop", MenuParse_soundLoop,},
	{"style", MenuParse_style,},
	{"visible", MenuParse_visible,},
	{"ignoreescape", MenuParse_ignoreescape,},
	{nullptr, nullptr,}
};

keywordHash_t* menuParseKeywordHash[KEYWORDHASH_SIZE];

/*
================
KeywordHash_Key
================
*/
static int KeywordHash_Key(const char* keyword)
{
	int hash = 0;
	for (int i = 0; keyword[i] != '\0'; i++)
	{
		if (keyword[i] >= 'A' && keyword[i] <= 'Z')
			hash += (keyword[i] + ('a' - 'A')) * (119 + i);
		else
			hash += keyword[i] * (119 + i);
	}
	hash = (hash ^ hash >> 10 ^ hash >> 20) & KEYWORDHASH_SIZE - 1;
	return hash;
}

/*
================
KeywordHash_Add
================
*/
static void KeywordHash_Add(keywordHash_t* table[], keywordHash_t* key)
{
	const int hash = KeywordHash_Key(key->keyword);
	key->next = table[hash];
	table[hash] = key;
}

/*
===============
KeywordHash_Find
===============
*/
static keywordHash_t* KeywordHash_Find(keywordHash_t* table[], const char* keyword)
{
	const int hash = KeywordHash_Key(keyword);
	for (keywordHash_t* key = table[hash]; key; key = key->next)
	{
		if (!Q_stricmp(key->keyword, keyword))
			return key;
	}
	return nullptr;
}

/*
================
hashForString

return a hash value for the string
================
*/
static unsigned hashForString(const char* str)
{
	unsigned hash = 0;
	int i = 0;
	while (str[i] != '\0')
	{
		const char letter = tolower(static_cast<unsigned char>(str[i]));
		hash += static_cast<unsigned>(letter) * (i + 119);
		i++;
	}
	hash &= HASH_TABLE_SIZE - 1;
	return hash;
}

/*
=================
String_Alloc
=================
*/
const char* String_Alloc(const char* p)
{
	if (p == nullptr)
	{
		return nullptr;
	}

	if (*p == 0)
	{
		static auto staticNULL = "";
		return staticNULL;
	}

	const unsigned hash = hashForString(p);

	stringDef_t* str = strHandle[hash];
	while (str)
	{
		if (strcmp(p, str->str) == 0)
		{
			return str->str;
		}
		str = str->next;
	}

	const int len = strlen(p);
	if (len + strPoolIndex + 1 < STRING_POOL_SIZE)
	{
		const int ph = strPoolIndex;
		strcpy(&strPool[strPoolIndex], p);
		strPoolIndex += len + 1;

		str = strHandle[hash];
		stringDef_t* last = str;
		while (last && last->next)
		{
			last = last->next;
		}

		str = static_cast<stringDef_s*>(UI_Alloc(sizeof(stringDef_t)));
		str->next = nullptr;
		str->str = &strPool[ph];
		if (last)
		{
			last->next = str;
		}
		else
		{
			strHandle[hash] = str;
		}

		return &strPool[ph];
	}
	Com_Printf("WARNING: Ran out of strPool space\n");

	return nullptr;
}

/*
=================
String_Report
=================
*/
void String_Report()
{
	Com_Printf("Memory/String Pool Info\n");
	Com_Printf("----------------\n");
	float f = strPoolIndex;
	f /= STRING_POOL_SIZE;
	f *= 100;
	Com_Printf("String Pool is %.1f%% full, %i bytes out of %i used.\n", f, strPoolIndex, STRING_POOL_SIZE);
	f = allocPoint;
	f /= MEM_POOL_SIZE;
	f *= 100;
	Com_Printf("Memory Pool is %.1f%% full, %i bytes out of %i used.\n", f, allocPoint, MEM_POOL_SIZE);
}

/*
=================
String_Init
=================
*/
void String_Init()
{
	for (int i = 0; i < HASH_TABLE_SIZE; i++)
	{
		strHandle[i] = nullptr;
	}

	strHandleCount = 0;
	strPoolIndex = 0;
	UI_InitMemory();
	Item_SetupKeywordHash();
	Menu_SetupKeywordHash();

	if (DC && DC->getBindingBuf)
	{
		Controls_GetConfig();
	}
}

//---------------------------------------------------------------------------------------------------------
//		Memory
//---------------------------------------------------------------------------------------------------------

/*
===============
UI_Alloc
===============
*/
void* UI_Alloc(const int size)
{
	if (allocPoint + size > MEM_POOL_SIZE)
	{
		outOfMemory = qtrue;
		if (DC->Print)
		{
			DC->Print("UI_Alloc: Failure. Out of memory!\n");
		}
		return nullptr;
	}

	char* p = &memoryPool[allocPoint];

	allocPoint += size + 15 & ~15;

	return p;
}

/*
===============
UI_InitMemory
===============
*/
void UI_InitMemory()
{
	allocPoint = 0;
	outOfMemory = qfalse;
}

/*
===============
Menu_ItemsMatchingGroup
===============
*/
int Menu_ItemsMatchingGroup(const menuDef_t* menu, const char* name)
{
	int count = 0;

	for (int i = 0; i < menu->itemCount; i++)
	{
		if (!menu->items[i]->window.name && !menu->items[i]->window.group)
		{
			Com_Printf(S_COLOR_YELLOW"WARNING: item has neither name or group\n");
			continue;
		}

		if (Q_stricmp(menu->items[i]->window.name, name) == 0 || menu->items[i]->window.group && Q_stricmp(
			menu->items[i]->window.group, name) == 0)
		{
			count++;
		}
	}

	return count;
}

/*
===============
Menu_GetMatchingItemByNumber
===============
*/
itemDef_t* Menu_GetMatchingItemByNumber(const menuDef_t* menu, const int index, const char* name)
{
	int count = 0;
	for (int i = 0; i < menu->itemCount; i++)
	{
		if (Q_stricmp(menu->items[i]->window.name, name) == 0 || menu->items[i]->window.group && Q_stricmp(
			menu->items[i]->window.group, name) == 0)
		{
			if (count == index)
			{
				return menu->items[i];
			}
			count++;
		}
	}
	return nullptr;
}

/*
===============
Menu_FadeItemByName
===============
*/
static void Menu_FadeItemByName(const menuDef_t* menu, const char* p, const qboolean fadeOut)
{
	const int count = Menu_ItemsMatchingGroup(menu, p);
	for (int i = 0; i < count; i++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != nullptr)
		{
			if (fadeOut)
			{
				item->window.flags |= WINDOW_FADINGOUT | WINDOW_VISIBLE;
				item->window.flags &= ~WINDOW_FADINGIN;
			}
			else
			{
				item->window.flags |= WINDOW_VISIBLE | WINDOW_FADINGIN;
				item->window.flags &= ~WINDOW_FADINGOUT;
			}
		}
	}
}

static void Menu_FadeOutItemOnly(const menuDef_t* menu, const char* p)
{
	const int count = Menu_ItemsMatchingGroup(menu, p);
	for (int i = 0; i < count; i++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != nullptr)
		{
			item->window.flags |= WINDOW_FADINGOUT | WINDOW_VISIBLE;
			item->window.flags &= ~WINDOW_FADINGIN;
		}
	}
}

/*
===============
Menu_ShowItemByName
===============
*/
static void Menu_ShowItemByName(const menuDef_t* menu, const char* p, const qboolean bShow)
{
	const int count = Menu_ItemsMatchingGroup(menu, p);

	if (!count)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: Menu_ShowItemByName - unable to locate any items named: \"%s\"\n", p);
	}

	for (int i = 0; i < count; i++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != nullptr)
		{
			if (bShow)
			{
				item->window.flags |= WINDOW_VISIBLE;
			}
			else
			{
				item->window.flags &= ~(WINDOW_VISIBLE | WINDOW_HASFOCUS);
				// stop cinematics playing in the window
				if (item->window.cinematic >= 0)
				{
					DC->stopCinematic(item->window.cinematic);
					item->window.cinematic = -1;
				}
			}
		}
	}
}

/*
===============
Menu_GetFocused
===============
*/
menuDef_t* Menu_GetFocused()
{
	for (int i = 0; i < menuCount; i++)
	{
		if (Menus[i].window.flags & WINDOW_HASFOCUS && Menus[i].window.flags & WINDOW_VISIBLE)
		{
			return &Menus[i];
		}
	}

	return nullptr;
}

/*
===============
Menus_OpenByName
===============
*/
void Menus_OpenByName(const char* p)
{
	Menus_ActivateByName(p);
}

/*
===============
Menus_FindByName
===============
*/
menuDef_t* Menus_FindByName(const char* p)
{
	for (int i = 0; i < menuCount; i++)
	{
		if (Q_stricmp(Menus[i].window.name, p) == 0)
		{
			return &Menus[i];
		}
	}
	return nullptr;
}

/*
===============
Menu_RunCloseScript
===============
*/
static void Menu_RunCloseScript(menuDef_t* menu)
{
	if (menu && menu->window.flags & WINDOW_VISIBLE && menu->onClose)
	{
		itemDef_t item;
		item.parent = menu;
		Item_RunScript(&item, menu->onClose);
	}
}

/*
===============
Item_ActivateByName
===============
*/
static void Item_ActivateByName(const char* menuName, const char* itemName)
{
	const menuDef_t* menu = Menus_FindByName(menuName);

	const auto item = Menu_FindItemByName(menu, itemName);

	if (item != nullptr)
	{
		item->window.flags &= ~WINDOW_INACTIVE;
	}
}

/*
===============
Menus_CloseByName
===============
*/
void Menus_CloseByName(const char* p)
{
	menuDef_t* menu = Menus_FindByName(p);

	// If the menu wasnt found just exit
	if (menu == nullptr)
	{
		return;
	}

	// Run the close script for the menu
	Menu_RunCloseScript(menu);

	// If this window had the focus then take it away
	if (menu->window.flags & WINDOW_HASFOCUS)
	{
		// If there is something still in the open menu list then
		// set it to have focus now
		if (openMenuCount)
		{
			// Subtract one from the open menu count to prepare to
			// remove the top menu from the list
			openMenuCount -= 1;

			// Set the top menu to have focus now
			menuStack[openMenuCount]->window.flags |= WINDOW_HASFOCUS;

			// Remove the top menu from the list
			menuStack[openMenuCount] = nullptr;
		}
	}

	// Window is now invisible and doenst have focus
	menu->window.flags &= ~(WINDOW_VISIBLE | WINDOW_HASFOCUS);
}

/*
===============
Menu_FindItemByName
===============
*/
itemDef_t* Menu_FindItemByName(const menuDef_t* menu, const char* p)
{
	if (menu == nullptr || p == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < menu->itemCount; i++)
	{
		if (Q_stricmp(p, menu->items[i]->window.name) == 0)
		{
			return menu->items[i];
		}
	}

	return nullptr;
}

/*
=================
Menu_ClearFocus
=================
*/
static itemDef_t* Menu_ClearFocus(const menuDef_t* menu)
{
	itemDef_t* ret = nullptr;

	if (menu == nullptr)
	{
		return nullptr;
	}
	for (int i = 0; i < menu->itemCount; i++)
	{
		if (menu->items[i]->window.flags & WINDOW_HASFOCUS)
		{
			ret = menu->items[i];
			menu->items[i]->window.flags &= ~WINDOW_HASFOCUS;
			if (menu->items[i]->leaveFocus)
			{
				Item_RunScript(menu->items[i], menu->items[i]->leaveFocus);
			}
		}
	}
	return ret;
}

// Set all the items within a given menu, with the given itemName, to the given shader
static void Menu_SetItemBackground(const menuDef_t* menu, const char* itemName, const char* background)
{
	if (!menu) // No menu???
	{
		return;
	}

	const int count = Menu_ItemsMatchingGroup(menu, itemName);

	for (int j = 0; j < count; j++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, j, itemName);
		if (item != nullptr)
		{
			//			item->window.background = DC->registerShaderNoMip(background);
			item->window.background = ui.R_RegisterShaderNoMip(background);
		}
	}
}

// Change the exec command for a button.
static void Menu_SetItemExec(const menuDef_t* menu, const char* itemName, const char* text)
{
	try
	{
		if (!menu) // No menu???
		{
			return;
		}

		const int count = Menu_ItemsMatchingGroup(menu, itemName);

		for (int j = 0; j < count; j++)
		{
			itemDef_t* item = Menu_GetMatchingItemByNumber(menu, j, itemName);
			if (item != nullptr)
			{
				if (text[0] == 'n' && text[1] == 'p' && text[2] == 'c')
				{
					item->exec = (char*)text;
					return;
				}
			}
		}
	}
	catch (...)
	{
	}
}

// Set all the items within a given menu, with the given itemName, to the given text
static void Menu_SetItemText(const menuDef_t* menu, const char* itemName, const char* text)
{
	if (!menu) // No menu???
	{
		return;
	}

	const int count = Menu_ItemsMatchingGroup(menu, itemName);

	for (int j = 0; j < count; j++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, j, itemName);
		if (item != nullptr)
		{
			if (text[0] == '*')
			{
				item->cvar = text + 1;
				// Just copying what was in ItemParse_cvar()
				if (item->typeData)
				{
					const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);
					editPtr->minVal = -1;
					editPtr->maxVal = -1;
					editPtr->defVal = -1;
				}
			}
			else
			{
				if (item->type == ITEM_TYPE_TEXTSCROLL)
				{
					const auto scrollPtr = static_cast<textScrollDef_t*>(item->typeData);
					if (scrollPtr)
					{
						scrollPtr->startPos = 0;
						scrollPtr->endPos = 0;
					}

					if (item->cvar)
					{
						char cvartext[2048];
						DC->getCVarString(item->cvar, cvartext, sizeof cvartext);
						item->text = cvartext;
					}
					else
					{
						item->text = (char*)text;
					}

					Item_TextScroll_BuildLines(item);
				}
				else
				{
					item->text = (char*)text;
				}
			}
		}
	}
}

/*
=================
Menu_TransitionItemByName
=================
*/
static void Menu_TransitionItemByName(const menuDef_t* menu, const char* p, const rectDef_t* rectFrom, const rectDef_t* rectTo,
	const int time, const float amt)
{
	const int count = Menu_ItemsMatchingGroup(menu, p);
	for (int i = 0; i < count; i++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != nullptr)
		{
			if (!rectFrom)
			{
				rectFrom = &item->window.rect;
				//if there are more than one of these with the same name, they'll all use the FIRST one's FROM.
			}
			item->window.flags |= WINDOW_INTRANSITION | WINDOW_VISIBLE;
			item->window.offsetTime = time;
			memcpy(&item->window.rectClient, rectFrom, sizeof(rectDef_t));
			memcpy(&item->window.rectEffects, rectTo, sizeof(rectDef_t));
			item->window.rectEffects2.x = abs(rectTo->x - rectFrom->x) / amt;
			item->window.rectEffects2.y = abs(rectTo->y - rectFrom->y) / amt;
			item->window.rectEffects2.w = abs(rectTo->w - rectFrom->w) / amt;
			item->window.rectEffects2.h = abs(rectTo->h - rectFrom->h) / amt;
			Item_UpdatePosition(item);
		}
	}
}

/*
=================
Menu_TransitionItemByName
=================
*/
//JLF MOVED
#define _TRANS3
#ifdef _TRANS3
static void Menu_Transition3ItemByName(const menuDef_t* menu, const char* p, const float minx, const float miny,
	const float minz,
	const float maxx, const float maxy, const float maxz, const float fovtx,
	const float fovty,
	const int time, const float amt)
{
	const int count = Menu_ItemsMatchingGroup(menu, p);
	for (int i = 0; i < count; i++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != nullptr)
		{
			if (item->type == ITEM_TYPE_MODEL)
			{
				const auto modelptr = static_cast<modelDef_t*>(item->typeData);

				item->window.flags |= WINDOW_INTRANSITIONMODEL | WINDOW_VISIBLE;
				item->window.offsetTime = time;
				modelptr->fov_x2 = fovtx;
				modelptr->fov_y2 = fovty;
				VectorSet(modelptr->g2maxs2, maxx, maxy, maxz);
				VectorSet(modelptr->g2mins2, minx, miny, minz);

				//				//modelptr->g2maxs2.x= maxx;
				//				modelptr->g2maxs2.y= maxy;
				//				modelptr->g2maxs2.z= maxz;
				//				modelptr->g2mins2.x= minx;
				//				modelptr->g2mins2.y= miny;
				//				modelptr->g2mins2.z= minz;

				//				VectorSet(modelptr->g2maxs2, maxx, maxy, maxz);

				modelptr->g2maxsEffect[0] = abs(modelptr->g2maxs2[0] - modelptr->g2maxs[0]) / amt;
				modelptr->g2maxsEffect[1] = abs(modelptr->g2maxs2[1] - modelptr->g2maxs[1]) / amt;
				modelptr->g2maxsEffect[2] = abs(modelptr->g2maxs2[2] - modelptr->g2maxs[2]) / amt;

				modelptr->g2minsEffect[0] = abs(modelptr->g2mins2[0] - modelptr->g2mins[0]) / amt;
				modelptr->g2minsEffect[1] = abs(modelptr->g2mins2[1] - modelptr->g2mins[1]) / amt;
				modelptr->g2minsEffect[2] = abs(modelptr->g2mins2[2] - modelptr->g2mins[2]) / amt;

				modelptr->fov_Effectx = abs(modelptr->fov_x2 - modelptr->fov_x) / amt;
				modelptr->fov_Effecty = abs(modelptr->fov_y2 - modelptr->fov_y) / amt;
			}
		}
	}
}

#endif

/*
=================
Menu_OrbitItemByName
=================
*/
static void Menu_OrbitItemByName(const menuDef_t* menu, const char* p, const float x, const float y, const float cx, const float cy, const int time)
{
	const int count = Menu_ItemsMatchingGroup(menu, p);
	for (int i = 0; i < count; i++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != nullptr)
		{
			item->window.flags |= WINDOW_ORBITING | WINDOW_VISIBLE;
			item->window.offsetTime = time;
			item->window.rectEffects.x = cx;
			item->window.rectEffects.y = cy;
			item->window.rectClient.x = x;
			item->window.rectClient.y = y;
			Item_UpdatePosition(item);
		}
	}
}

void Menu_ItemDisable(const menuDef_t* menu, const char* name, const qboolean disableFlag)
{
	const int count = Menu_ItemsMatchingGroup(menu, name);
	// Loop through all items that have this name
	for (int j = 0; j < count; j++)
	{
		itemDef_t* itemFound = Menu_GetMatchingItemByNumber(menu, j, name);
		if (itemFound != nullptr)
		{
			itemFound->disabled = disableFlag;
			// Just in case it had focus
			itemFound->window.flags &= ~WINDOW_MOUSEOVER;
		}
	}
}

/*
=================
Rect_Parse
=================
*/
static qboolean Rect_Parse(const char** p, rectDef_t* r)
{
	if (!COM_ParseFloat(p, &r->x))
	{
		if (!COM_ParseFloat(p, &r->y))
		{
			if (!COM_ParseFloat(p, &r->w))
			{
				if (!COM_ParseFloat(p, &r->h))
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

static qboolean Script_SetItemRect(itemDef_t* item, const char** args)
{
	const char* itemname;
	rectDef_t rect;

	// expecting type of color to set and 4 args for the color
	if (String_Parse(args, &itemname))
	{
		const int count = Menu_ItemsMatchingGroup(static_cast<menuDef_t*>(item->parent), itemname);

		if (!Rect_Parse(args, &rect))
		{
			return qtrue;
		}

		for (int j = 0; j < count; j++)
		{
			itemDef_t* item2 = Menu_GetMatchingItemByNumber(static_cast<menuDef_t*>(item->parent), j, itemname);
			if (item2 != nullptr)
			{
				const rectDef_t* out = &item2->window.rect;

				if (out)
				{
					item2->window.rect.x = rect.x;
					item2->window.rect.y = rect.y;
					item2->window.rect.w = rect.w;
					item2->window.rect.h = rect.h;
				}
			}
		}
	}
	return qtrue;
}

/*
=================
Script_SetItemBackground
=================
*/
static qboolean Script_SetItemBackground(itemDef_t* item, const char** args)
{
	const char* itemName;
	const char* name;

	// expecting name of shader
	if (String_Parse(args, &itemName) && String_Parse(args, &name))
	{
		Menu_SetItemBackground(static_cast<menuDef_t*>(item->parent), itemName, name);
	}
	return qtrue;
}

/*
=================
Script_SetItemText
=================
*/
static qboolean Script_SetItemText(itemDef_t* item, const char** args)
{
	const char* itemName;
	const char* text;

	// expecting text
	if (String_Parse(args, &itemName) && String_Parse(args, &text))
	{
		Menu_SetItemText(static_cast<menuDef_t*>(item->parent), itemName, text);
	}
	return qtrue;
}

/*
=================
Script_SetItemExec
=================
*/
static qboolean Script_SetItemExec(itemDef_t* item, const char** args)
{
	try
	{
		// expecting text
		const char* itemName;
		const char* text;

		// expecting text
		if (String_Parse(args, &itemName) && String_Parse(args, &text))
		{
			Menu_SetItemExec(static_cast<menuDef_t*>(item->parent), itemName, text);
		}
	}

	catch (...)
	{
	}
	return qtrue;
}

/*
=================
Script_FadeIn
=================
*/
static qboolean Script_FadeIn(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		Menu_FadeItemByName(static_cast<menuDef_t*>(item->parent), name, qfalse);
	}

	return qtrue;
}

/*
=================
Script_FadeOut
=================
*/
static qboolean Script_FadeOut(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		Menu_FadeItemByName(static_cast<menuDef_t*>(item->parent), name, qtrue);
	}

	return qtrue;
}

/*
=================
Script_Show
=================
*/
static qboolean Script_Show(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		Menu_ShowItemByName(static_cast<menuDef_t*>(item->parent), name, qtrue);
	}

	return qtrue;
}

/*
=================
Script_ShowMenu
=================
*/
static qboolean Script_ShowMenu(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		Menus_ShowItems(name);
	}

	return qtrue;
}

/*
=================
Script_Hide
=================
*/
static qboolean Script_Hide(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		Menu_ShowItemByName(static_cast<menuDef_t*>(item->parent), name, qfalse);
	}

	return qtrue;
}

/*
=================
Script_SetColor
=================
*/
static qboolean Script_SetColor(itemDef_t* item, const char** args)
{
	const char* name;
	float f;

	// expecting type of color to set and 4 args for the color
	if (String_Parse(args, &name))
	{
		vec4_t* out = nullptr;
		if (Q_stricmp(name, "backcolor") == 0)
		{
			out = &item->window.backColor;
			item->window.flags |= WINDOW_BACKCOLORSET;
		}
		else if (Q_stricmp(name, "forecolor") == 0)
		{
			out = &item->window.foreColor;
			item->window.flags |= WINDOW_FORECOLORSET;
		}
		else if (Q_stricmp(name, "bordercolor") == 0)
		{
			out = &item->window.borderColor;
		}

		if (out)
		{
			for (int i = 0; i < 4; i++)
			{
				//				if (!Float_Parse(args, &f))
				if (COM_ParseFloat(args, &f))
				{
					return qtrue;
				}
				(*out)[i] = f;
			}
		}
	}

	return qtrue;
}

/*
=================
Script_Open
=================
*/
static qboolean Script_Open(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		Menus_OpenByName(name);
	}

	return qtrue;
}

static qboolean Script_OpenGoToMenu(itemDef_t* item, const char** args)
{
	Menus_OpenByName(GoToMenu); // Give warning
	return qtrue;
}

/*
=================
Script_Close
=================
*/
static qboolean Script_Close(itemDef_t* item, const char** args)
{
	const char* name;
	if (String_Parse(args, &name))
	{
		if (Q_stricmp(name, "all") == 0)
		{
			Menus_CloseAll();
		}
		else
		{
			Menus_CloseByName(name);
		}
	}

	return qtrue;
}

/*
=================
Script_Activate
=================
*/
static qboolean Script_Activate(itemDef_t* item, const char** args)
{
	const char* name, * menu;

	if (String_Parse(args, &menu))
	{
		if (String_Parse(args, &name))
		{
			Item_ActivateByName(menu, name);
		}
	}

	return qtrue;
}

/*
=================
Script_SetBackground
=================
*/
static qboolean Script_SetBackground(itemDef_t* item, const char** args)
{
	const char* name;
	// expecting name to set asset to
	if (String_Parse(args, &name))
	{
		item->window.background = DC->registerShaderNoMip(name);
	}

	return qtrue;
}

/*
=================
Script_SetAsset
=================
*/
static qboolean Script_SetAsset(itemDef_t* item, const char** args)
{
	const char* name;
	// expecting name to set asset to
	if (String_Parse(args, &name))
	{
		// check for a model
		if (item->type == ITEM_TYPE_MODEL)
		{
		}
	}

	return qtrue;
}

/*
=================
Script_SetFocus
=================
*/
static qboolean Script_SetFocus(itemDef_t* item, const char** args)
{
	const char* name;

	if (String_Parse(args, &name))
	{
		const auto focusItem = Menu_FindItemByName(static_cast<menuDef_t*>(item->parent), name);
		if (focusItem && !(focusItem->window.flags & WINDOW_DECORATION) && !(focusItem->window.flags & WINDOW_HASFOCUS))
		{
			Menu_ClearFocus(static_cast<menuDef_t*>(item->parent));
			//JLF
			focusItem->window.flags |= WINDOW_HASFOCUS;
			//END JLF

			if (focusItem->onFocus)
			{
				Item_RunScript(focusItem, focusItem->onFocus);
			}
			if (DC->Assets.itemFocusSound)
			{
				DC->startLocalSound(DC->Assets.itemFocusSound, CHAN_LOCAL_SOUND);
			}
		}
	}

	return qtrue;
}

/*
=================
Script_SetItemFlag
=================
*/
static qboolean Script_SetItemFlag(itemDef_t* item, const char** args)
{
	const char* itemName, * number;

	if (String_Parse(args, &itemName))
	{
		item = Menu_FindItemByName(static_cast<menuDef_t*>(item->parent), itemName);

		if (String_Parse(args, &number))
		{
			const int amount = atoi(number);
			item->window.flags |= amount;
		}
	}

	return qtrue;
}

void UI_SetItemVisible(const menuDef_t* menu, const char* itemname, const qboolean visible)
{
	const int count = Menu_ItemsMatchingGroup(menu, itemname);

	for (int j = 0; j < count; j++)
	{
		itemDef_t* item = Menu_GetMatchingItemByNumber(menu, j, itemname);
		if (item != nullptr)
		{
			if (visible == qtrue)
			{
				item->window.flags |= WINDOW_VISIBLE;
			}
			else
			{
				item->window.flags &= ~WINDOW_VISIBLE;
			}
		}
	}
}

void UI_SetItemColor(const itemDef_t* item, const char* itemname, const char* name, vec4_t color)
{
	const int count = Menu_ItemsMatchingGroup(static_cast<menuDef_t*>(item->parent), itemname);

	for (int j = 0; j < count; j++)
	{
		itemDef_t* item2 = Menu_GetMatchingItemByNumber(static_cast<menuDef_t*>(item->parent), j, itemname);
		if (item2 != nullptr)
		{
			vec4_t* out = nullptr;
			if (Q_stricmp(name, "backcolor") == 0)
			{
				out = &item2->window.backColor;
			}
			else if (Q_stricmp(name, "forecolor") == 0)
			{
				out = &item2->window.foreColor;
				item2->window.flags |= WINDOW_FORECOLORSET;
			}
			else if (Q_stricmp(name, "bordercolor") == 0)
			{
				out = &item2->window.borderColor;
			}

			if (out)
			{
				for (int i = 0; i < 4; i++)
				{
					(*out)[i] = color[i];
				}
			}
		}
	}
}

/*
=================
Script_SetItemColor
=================
*/
static qboolean Script_SetItemColor(itemDef_t* item, const char** args)
{
	const char* itemname;
	const char* name;
	vec4_t color;

	// expecting type of color to set and 4 args for the color
	if (String_Parse(args, &itemname) && String_Parse(args, &name))
	{
		if (COM_ParseVec4(args, &color))
		{
			return qtrue;
		}

		UI_SetItemColor(item, itemname, name, color);
	}

	return qtrue;
}

/*
=================
Script_Defer

Defers the rest of the script based on the defer condition.  The deferred
portion of the script can later be run with the "rundeferred"
=================
*/
static qboolean Script_Defer(itemDef_t* item, const char** args)
{
	// Should the script be deferred?
	if (DC->deferScript(args))
	{
		// Need the item the script was being run on
		uiInfo.deferredScriptItem = item;

		// Save the rest of the script
		Q_strncpyz(uiInfo.deferredScript, *args, MAX_DEFERRED_SCRIPT);

		// No more running
		return qfalse;
	}

	// Keep running the script, its ok
	return qtrue;
}

/*
=================
Script_RunDeferred

Runs the last deferred script, there can only be one script deferred at a
time so be careful of recursion
=================
*/
static qboolean Script_RunDeferred(itemDef_t* item, const char** args)
{
	// Make sure there is something to run.
	if (!uiInfo.deferredScript[0] || !uiInfo.deferredScriptItem)
	{
		return qtrue;
	}

	// Run the deferred script now
	Item_RunScript(uiInfo.deferredScriptItem, uiInfo.deferredScript);

	return qtrue;
}

/*
=================
Script_Delay

Delays the rest of the script for the specified amount of time
=================
*/
static qboolean Script_Delay(itemDef_t* item, const char** args)
{
	int time;

	if (Int_Parse(args, &time))
	{
		item->window.flags |= WINDOW_SCRIPTWAITING;
		item->window.delayTime = DC->realTime + time; // Flag to set delay time on next paint
		item->window.delayedScript = (char*)*args; // Copy current location, we'll resume executing here later
	}
	else
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: Script_Delay: error parsing\n");
	}

	// Stop running
	return qfalse;
}

/*
=================
Script_Transition

transition		rtvscr		321 0 202 264  415 0 202 264  20 25
=================
*/
static qboolean Script_Transition(itemDef_t* item, const char** args)
{
	const char* name;
	rectDef_t rectFrom, rectTo;
	int time;
	float amt;

	if (String_Parse(args, &name))
	{
		if (ParseRect(args, &rectFrom) && ParseRect(args, &rectTo) && Int_Parse(args, &time) && !
			COM_ParseFloat(args, &amt))
		{
			Menu_TransitionItemByName(static_cast<menuDef_t*>(item->parent), name, &rectFrom, &rectTo, time, amt);
		}
		else
		{
			Com_Printf(S_COLOR_YELLOW"WARNING: Script_Transition: error parsing '%s'\n", name);
		}
	}

	return qtrue;
}

/*
=================
Script_Transition2
uses current origin instead of specifing a starting origin

transition2		lfvscr		25 0 202 264  20 25
=================
*/
static qboolean Script_Transition2(itemDef_t* item, const char** args)
{
	const char* name;
	rectDef_t rectTo;
	int time;
	float amt;

	if (String_Parse(args, &name))
	{
		if (ParseRect(args, &rectTo) && Int_Parse(args, &time) && !COM_ParseFloat(args, &amt))
		{
			Menu_TransitionItemByName(static_cast<menuDef_t*>(item->parent), name, nullptr, &rectTo, time, amt);
		}
		else
		{
			Com_Printf(S_COLOR_YELLOW"WARNING: Script_Transition2: error parsing '%s'\n", name);
		}
	}

	return qtrue;
}

#ifdef _TRANS3
/*
JLF MPMOVED
=================
Script_Transition3

used exclusively with model views
uses current origin instead of specifing a starting origin

transition3		lfvscr		(min extent) (max extent) (fovx,y)  20 25
=================
*/
static qboolean Script_Transition3(itemDef_t* item, const char** args)
{
	const char* name = nullptr;
	const char* value = nullptr;

	if (String_Parse(args, &name))
	{
		if (String_Parse(args, &value))
		{
			const float minx = atof(value);
			if (String_Parse(args, &value))
			{
				const float miny = atof(value);
				if (String_Parse(args, &value))
				{
					const float minz = atof(value);
					if (String_Parse(args, &value))
					{
						const float maxx = atof(value);
						if (String_Parse(args, &value))
						{
							const float maxy = atof(value);
							if (String_Parse(args, &value))
							{
								const float maxz = atof(value);
								if (String_Parse(args, &value))
								{
									const float fovtx = atof(value);
									if (String_Parse(args, &value))
									{
										const float fovty = atof(value);

										if (String_Parse(args, &value))
										{
											const int time = atoi(value);
											if (String_Parse(args, &value))
											{
												const float amt = atof(value);
												//set up the variables
												Menu_Transition3ItemByName(static_cast<menuDef_t*>(item->parent),
													name,
													minx, miny, minz,
													maxx, maxy, maxz,
													fovtx, fovty,
													time, amt);

												return qtrue;
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
	if (name)
	{
		Com_Printf(S_COLOR_YELLOW "WARNING: Script_Transition2: error parsing '%s'\n", name);
	}
	return qtrue;
}
#endif

//only works on some feeders
static int GetCurrentFeederIndex(itemDef_t* item)
{
	const float feederID = item->special;
	const char* name;
	int i, max;

	if (feederID == FEEDER_PLAYER_SPECIES)
	{
		return uiInfo.playerSpeciesIndex;
	}
	if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		name = Cvar_VariableString("ui_char_skin_head");
		max = uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount;
		for (i = 0; i < max; i++)
		{
			if (!Q_stricmp(name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[i].name))
			{
				return i;
			}

			//	Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadNames[index]);
		}
		return -1;
	}
	if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		name = Cvar_VariableString("ui_char_skin_torso");
		max = uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount;
		for (i = 0; i < max; i++)
		{
			if (!Q_stricmp(name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[i].name))
			{
				return i;
			}
			//	Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadNames[index]);
		}
		return -1;
	}
	if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		name = Cvar_VariableString("ui_char_skin_legs");
		max = uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount;
		for (i = 0; i < max; i++)
		{
			if (!Q_stricmp(name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[i].name))
			{
				return i;
			}
			//	Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadNames[index]);
		}
		return -1;

		//	if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		//	{
		//		Cvar_Set("ui_char_skin_legs", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegNames[index]);
		//	}
	}
	if (feederID == FEEDER_COLORCHOICES)
	{
		extern void Item_RunScript(itemDef_t * item_item, const char* s); //from ui_shared;
		const int currR = Cvar_VariableIntegerValue("ui_char_color_red");
		const int currG = Cvar_VariableIntegerValue("ui_char_color_green");
		const int currB = Cvar_VariableIntegerValue("ui_char_color_blue");

		max = uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount;
		for (i = 0; i < max; i++)
		{
			Item_RunScript(item, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[i].actionText);
			const int newR = Cvar_VariableIntegerValue("ui_char_color_red");
			const int newG = Cvar_VariableIntegerValue("ui_char_color_green");
			const int newB = Cvar_VariableIntegerValue("ui_char_color_blue");
			if (currR == newR && currG == newG && currB == newB)
				return i;
		}
		return -1;
	}

	return -1;
}

static qboolean Script_IncrementFeeder(itemDef_t* item, const char** args)
{
	const int feedercount = uiInfo.uiDC.feederCount(item->special);
	int value = GetCurrentFeederIndex(item);
	value++;
	if (value >= feedercount)
		value = 0;
	DC->feederSelection(item->special, value, item);
	return qtrue;
}

static qboolean Script_DecrementFeeder(itemDef_t* item, const char** args)
{
	const int feedercount = uiInfo.uiDC.feederCount(item->special);
	int value = GetCurrentFeederIndex(item);
	value--;
	if (value < 0)
		value = feedercount - 1;
	DC->feederSelection(item->special, value, item);
	return qtrue;
}

/*
=================
Script_SetCvar
=================
*/
static qboolean Script_SetCvar(itemDef_t* item, const char** args)
{
	const char* cvar, * val;
	if (String_Parse(args, &cvar) && String_Parse(args, &val))
	{
		if (!Q_stricmp(val, "(NULL)"))
		{
			DC->setCVar(cvar, "");
		}
		else
		{
			DC->setCVar(cvar, val);
		}
	}

	return qtrue;
}

/*
=================
Script_Exec
=================
*/
static qboolean Script_Exec(itemDef_t* item, const char** args)
{
	const char* val;

	if (initialLoad)
	{
		item->exec = "menumusic";
		initialLoad = false;
	}

	if (AlsoLoad)
	{
		item->exec = "totgmapmusic";
		AlsoLoad = false;
	}

	if (String_Parse(args, &val))
	{
		DC->executeText(EXEC_APPEND, va("%s ; ", val));
	}

	return qtrue;
}

/*
=================
Script_Play
=================
*/
static qboolean Script_Play(itemDef_t* item, const char** args)
{
	const char* val;
	if (String_Parse(args, &val))
	{
		DC->startLocalSound(DC->registerSound(val, qfalse), CHAN_AUTO);
	}

	return qtrue;
}

/*
=================
Script_PlayVoice
=================
*/
static qboolean Script_PlayVoice(itemDef_t* item, const char** args)
{
	const char* val;
	if (String_Parse(args, &val))
	{
		DC->startLocalSound(DC->registerSound(val, qfalse), CHAN_VOICE);
	}

	return qtrue;
}

/*
=================
Script_StopVoice
=================
*/
static qboolean Script_StopVoice(itemDef_t* item, const char** args)
{
	DC->startLocalSound(uiInfo.uiDC.Assets.nullSound, CHAN_VOICE);

	return qtrue;
}

/*
=================
Script_Orbit
=================
*/
static qboolean Script_Orbit(itemDef_t* item, const char** args)
{
	const char* name;
	float cx, cy, x, y;
	int time;

	if (String_Parse(args, &name))
	{
		//		if ( Float_Parse(args, &x) && Float_Parse(args, &y) && Float_Parse(args, &cx) && Float_Parse(args, &cy) && Int_Parse(args, &time) )
		if (!COM_ParseFloat(args, &x) && !COM_ParseFloat(args, &y) && !COM_ParseFloat(args, &cx) && !
			COM_ParseFloat(args, &cy) && Int_Parse(args, &time))
		{
			Menu_OrbitItemByName(static_cast<menuDef_t*>(item->parent), name, x, y, cx, cy, time);
		}
	}

	return qtrue;
}

commandDef_t commandList[] =
{
	{"activate", &Script_Activate}, // menu
	{"close", &Script_Close}, // menu
	{"exec", &Script_Exec}, // group/name
	{"fadein", &Script_FadeIn}, // group/name
	{"fadeout", &Script_FadeOut}, // group/name
	{"hide", &Script_Hide}, // group/name
	{"open", &Script_Open}, // menu
	{"openGoToMenu", &Script_OpenGoToMenu}, //
	{"orbit", &Script_Orbit}, // group/name
	{"play", &Script_Play}, // group/name
	{"playVoice", &Script_PlayVoice}, // group/name
	{"stopVoice", &Script_StopVoice}, // group/name
	{"setasset", &Script_SetAsset}, // works on this
	{"setbackground", &Script_SetBackground}, // works on this
	{"setcolor", &Script_SetColor}, // works on this
	{"setcvar", &Script_SetCvar}, // group/name
	{"setfocus", &Script_SetFocus}, // sets this background color to team color
	{"setitemcolor", &Script_SetItemColor}, // group/name
	{"setitemflag", &Script_SetItemFlag}, // name
	{"show", &Script_Show}, // group/name
	{"showMenu", &Script_ShowMenu}, // menu
	{"transition", &Script_Transition}, // group/name
	{"transition2", &Script_Transition2}, // group/name
	{"setitembackground", &Script_SetItemBackground}, // group/name
	{"setitemtext", &Script_SetItemText}, // group/name
	{"setitemrect", &Script_SetItemRect}, // group/name
	{"setitemexec", &Script_SetItemExec}, // group/name
	{"defer", &Script_Defer}, //
	{"rundeferred", &Script_RunDeferred}, //
	{"delay", &Script_Delay}, // works on this (script)
	{"transition3", &Script_Transition3}, // model exclusive transition
	{"incrementfeeder", &Script_IncrementFeeder},
	{"decrementfeeder", &Script_DecrementFeeder}
};

int scriptCommandCount = sizeof commandList / sizeof(commandDef_t);

/*
===============
Item_Init
===============
*/
void Item_Init(itemDef_t* item)
{
	memset(item, 0, sizeof(itemDef_t));
	item->textscale = 0.55f;
	Window_Init(&item->window);
}

/*
===============
Item_Multi_Setting
===============
*/
static const char* Item_Multi_Setting(const itemDef_t* item)
{
	const multiDef_t* multiPtr = static_cast<multiDef_t*>(item->typeData);
	if (multiPtr)
	{
		float value = 0;
		char buff[1024];
		if (multiPtr->strDef)
		{
			if (item->cvar)
			{
				DC->getCVarString(item->cvar, buff, sizeof buff);
			}
			else
			{
			}
		}
		else
		{
			if (item->cvar) // Was a cvar given?
			{
				value = DC->getCVarValue(item->cvar);
			}
			else
			{
				value = item->value;
			}
		}

		for (int i = 0; i < multiPtr->count; i++)
		{
			if (multiPtr->strDef)
			{
				if (Q_stricmp(buff, multiPtr->cvarStr[i]) == 0)
				{
					return multiPtr->cvarList[i];
				}
			}
			else
			{
				if (multiPtr->cvarValue[i] == value)
				{
					return multiPtr->cvarList[i];
				}
			}
		}
	}

#ifdef JK2_MODE
	return "@MENUS1_CUSTOM";
#else
	return "@MENUS_CUSTOM";
#endif
}

//---------------------------------------------------------------------------------------------------------
//		Item Keyword Parse functions
//---------------------------------------------------------------------------------------------------------

/*
===============
ItemParse_name
	name <string>
===============
*/
static qboolean ItemParse_name(itemDef_t* item)
{
	if (!PC_ParseStringMem((const char**)&item->window.name))
	{
		return qfalse;
	}

	return qtrue;
}

static qboolean ItemParse_font(itemDef_t* item)
{
	if (PC_ParseInt(&item->font))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_focusSound
	name <string>
===============
*/
static qboolean ItemParse_focusSound(itemDef_t* item)
{
	const char* temp;

	if (PC_ParseString(&temp))
	{
		return qfalse;
	}
	item->focusSound = DC->registerSound(temp, qfalse);
	return qtrue;
}

/*
===============
ItemParse_text
	text <string>
===============
*/
static qboolean ItemParse_text(itemDef_t* item)
{
	if (!PC_ParseStringMem((const char**)&item->text))
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_descText
	text <string>
===============
*/
static qboolean ItemParse_descText(itemDef_t* item)
{
	if (!PC_ParseStringMem(&item->descText))
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_text
	text <string>
===============
*/
static qboolean ItemParse_text2(itemDef_t* item)
{
	if (!PC_ParseStringMem((const char**)&item->text2))
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_group
	group <string>
===============
*/
static qboolean ItemParse_group(itemDef_t* item)
{
	if (!PC_ParseStringMem((const char**)&item->window.group))
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_asset_model
	asset_model <string>
===============
*/
qboolean ItemParse_asset_model_go(itemDef_t* item, const char* name)
{
	Item_ValidateTypeData(item);
	const modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (!Q_stricmp(&name[strlen(name) - 4], ".glm"))
	{
		//it's a ghoul2 model then
		if (item->ghoul2.size() && item->ghoul2[0].mModelindex >= 0)
		{
			DC->g2_RemoveGhoul2Model(item->ghoul2, 0);
			item->flags &= ~ITF_G2VALID;
		}
		const int g2Model = DC->g2_InitGhoul2Model(item->ghoul2, name, 0, 0, 0, 0, 0);
		if (g2Model >= 0)
		{
			item->flags |= ITF_G2VALID;

			if (modelPtr->g2anim)
			{
				//does the menu request this model be playing an animation?
				DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qfalse);
			}
			if (modelPtr->g2skin)
			{
				DC->g2_SetSkin(&item->ghoul2[0], 0, modelPtr->g2skin);
				//this is going to set the surfs on/off matching the skin file
			}
		}
	}
	else if (!item->asset)
	{
		//guess it's just an md3
		item->asset = DC->registerModel(name);
		item->flags &= ~ITF_G2VALID;
	}
	return qtrue;
}

static qboolean ItemParse_asset_model(itemDef_t* item)
{
	const char* temp;
	Item_ValidateTypeData(item);

	if (PC_ParseString(&temp))
	{
		return qfalse;
	}
	char modelPath[MAX_QPATH];

	if (!Q_stricmp(temp, "ui_char_model"))
	{
		Com_sprintf(modelPath, sizeof modelPath, "models/players/%s/model.glm", Cvar_VariableString("g_char_model"));
	}
	else
	{
		Com_sprintf(modelPath, sizeof modelPath, temp);
	}
	return ItemParse_asset_model_go(item, modelPath);
}

/*
===============
ItemParse_asset_model
	asset_shader <string>
===============
*/
static qboolean ItemParse_asset_shader(itemDef_t* item)
{
	const char* temp;

	if (PC_ParseString(&temp))
	{
		return qfalse;
	}
	item->asset = DC->registerShaderNoMip(temp);
	return qtrue;
}

/*
===============
ItemParse_asset_model
	model_origin <number> <number> <number>
===============
*/
static qboolean ItemParse_model_origin(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (PC_ParseFloat(&modelPtr->origin[0]))
	{
		if (PC_ParseFloat(&modelPtr->origin[1]))
		{
			if (PC_ParseFloat(&modelPtr->origin[2]))
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

/*
===============
ItemParse_model_fovx
	model_fovx <number>
===============
*/
static qboolean ItemParse_model_fovx(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (PC_ParseFloat(&modelPtr->fov_x))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_model_fovy
	model_fovy <number>
===============
*/
static qboolean ItemParse_model_fovy(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (PC_ParseFloat(&modelPtr->fov_y))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_model_rotation
	model_rotation <integer>
===============
*/
static qboolean ItemParse_model_rotation(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (PC_ParseInt(&modelPtr->rotationSpeed))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_model_angle
	model_angle <integer>
===============
*/
static qboolean ItemParse_model_angle(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (PC_ParseInt(&modelPtr->angle))
	{
		return qfalse;
	}
	return qtrue;
}

// model_g2mins <number> <number> <number>
static qboolean ItemParse_model_g2mins(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (!PC_ParseFloat(&modelPtr->g2mins[0]))
	{
		if (!PC_ParseFloat(&modelPtr->g2mins[1]))
		{
			if (!PC_ParseFloat(&modelPtr->g2mins[2]))
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

// model_g2maxs <number> <number> <number>
static qboolean ItemParse_model_g2maxs(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (!PC_ParseFloat(&modelPtr->g2maxs[0]))
	{
		if (!PC_ParseFloat(&modelPtr->g2maxs[1]))
		{
			if (!PC_ParseFloat(&modelPtr->g2maxs[2]))
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

// model_g2skin <string>
qboolean ItemParse_model_g2skin_go(itemDef_t* item, const char* skinName)
{
	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (!skinName || !skinName[0])
	{
		//it was parsed cor~rectly so still return true.
		modelPtr->g2skin = 0;
		DC->g2_SetSkin(&item->ghoul2[0], -1, 0); //turn off custom skin
		return qtrue;
	}

	modelPtr->g2skin = DC->registerSkin(skinName);
	if (item->ghoul2.IsValid())
	{
		DC->g2_SetSkin(&item->ghoul2[0], 0, modelPtr->g2skin);
		//this is going to set the surfs on/off matching the skin file
	}

	return qtrue;
}

static qboolean ItemParse_model_g2skin(itemDef_t* item)
{
	const char* skinName;

	if (PC_ParseString(&skinName))
	{
		return qfalse;
	}

	return ItemParse_model_g2skin_go(item, skinName);
}

// model_g2anim <number>
qboolean ItemParse_model_g2anim_go(itemDef_t* item, const char* animName)
{
	int i = 0;

	Item_ValidateTypeData(item);
	const auto modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (!animName || !animName[0])
	{
		//it was parsed correctly so still return true.
		return qtrue;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(animName, anim_table[i].name))
		{
			//found it
			modelPtr->g2anim = anim_table[i].id;
			return qtrue;
		}
		i++;
	}

	Com_Printf("Could not find '%s' in the anim table\n", animName);
	return qtrue;
}

static qboolean ItemParse_model_g2anim(itemDef_t* item)
{
	const char* animName;

	if (PC_ParseString(&animName))
	{
		return qfalse;
	}

	return ItemParse_model_g2anim_go(item, animName);
}

/*
===============
ItemParse_rect
	rect <rectangle>
===============
*/
static qboolean ItemParse_rect(itemDef_t* item)
{
	if (!PC_ParseRect(&item->window.rectClient))
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_flag
	flag <integer>
===============
*/
static qboolean ItemParse_flag(itemDef_t* item)
{
	const char* tempStr;

	if (PC_ParseString(&tempStr))
	{
		return qfalse;
	}

	int i = 0;
	while (itemFlags[i].string)
	{
		if (Q_stricmp(tempStr, itemFlags[i].string) == 0)
		{
			item->window.flags |= itemFlags[i].value;
			break;
		}
		i++;
	}

	if (itemFlags[i].string == nullptr)
	{
		PC_ParseWarning(va("Unknown item flag value '%s'", tempStr));
	}

	return qtrue;
}

/*
===============
ItemParse_style
	style <integer>
===============
*/
static qboolean ItemParse_style(itemDef_t* item)
{
	const char* tempStr;

	if (PC_ParseString(&tempStr))
	{
		return qfalse;
	}

	int i = 0;
	while (styles[i])
	{
		if (Q_stricmp(tempStr, styles[i]) == 0)
		{
			item->window.style = i;
			break;
		}
		i++;
	}

	if (styles[i] == nullptr)
	{
		PC_ParseWarning(va("Unknown item style value '%s'", tempStr));
	}

	return qtrue;
}

/*
===============
ItemParse_decoration
	decoration
===============
*/
static qboolean ItemParse_decoration(itemDef_t* item)
{
	item->window.flags |= WINDOW_DECORATION;
	return qtrue;
}

/*
===============
ItemParse_notselectable
	notselectable
===============
*/
static qboolean ItemParse_notselectable(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);

	if (item->type == ITEM_TYPE_LISTBOX && listPtr)
	{
		listPtr->notselectable = qtrue;
	}
	return qtrue;
}

/*
===============
ItemParse_scrollhidden
	scrollhidden
===============
*/
static qboolean ItemParse_scrollhidden(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);

	if (item->type == ITEM_TYPE_LISTBOX && listPtr)
	{
		listPtr->scrollhidden = qtrue;
	}
	return qtrue;
}

/*
===============
ItemParse_wrapped
	manually wrapped
===============
*/
static qboolean ItemParse_wrapped(itemDef_t* item)
{
	item->window.flags |= WINDOW_WRAPPED;
	return qtrue;
}

/*
===============
ItemParse_autowrapped
	auto wrapped
===============
*/
static qboolean ItemParse_autowrapped(itemDef_t* item)
{
	item->window.flags |= WINDOW_AUTOWRAPPED;
	return qtrue;
}

/*
===============
ItemParse_horizontalscroll
	horizontalscroll
===============
*/
static qboolean ItemParse_horizontalscroll(itemDef_t* item)
{
	item->window.flags |= WINDOW_HORIZONTAL;
	return qtrue;
}

/*
===============
ItemParse_type
	type <integer>
===============
*/
static qboolean ItemParse_type(itemDef_t* item)
{
	const char* tempStr;

	if (PC_ParseString(&tempStr))
	{
		return qfalse;
	}

	int i = 0;
	while (types[i])
	{
		if (Q_stricmp(tempStr, types[i]) == 0)
		{
			item->type = i;
			break;
		}
		i++;
	}

	if (types[i] == nullptr)
	{
		PC_ParseWarning(va("Unknown item type value '%s'", tempStr));
	}
	else
	{
		Item_ValidateTypeData(item);
	}
	return qtrue;
}

/*
===============
ItemParse_elementwidth
	 elementwidth, used for listbox image elements
	 uses textalignx for storage
===============
*/
static qboolean ItemParse_elementwidth(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
	if (PC_ParseFloat(&listPtr->elementWidth))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_elementheight
	elementheight, used for listbox image elements
	uses textaligny for storage
===============
*/
static qboolean ItemParse_elementheight(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
	if (PC_ParseFloat(&listPtr->elementHeight))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_feeder
	feeder <float>
===============
*/
static qboolean ItemParse_feeder(itemDef_t* item)
{
	if (PC_ParseFloat(&item->special))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_elementtype
	elementtype, used to specify what type of elements a listbox contains
	uses textstyle for storage
===============
*/
static qboolean ItemParse_elementtype(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}

	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
	if (PC_ParseInt(&listPtr->elementStyle))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_columns
	columns sets a number of columns and an x pos and width per..
===============
*/
static qboolean ItemParse_columns(itemDef_t* item)
{
	int num;

	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}

	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
	if (!PC_ParseInt(&num))
	{
		if (num > MAX_LB_COLUMNS)
		{
			num = MAX_LB_COLUMNS;
		}
		listPtr->numColumns = num;
		for (int i = 0; i < num; i++)
		{
			int pos, width, maxChars;

			if (!PC_ParseInt(&pos) && !PC_ParseInt(&width) && !PC_ParseInt(&maxChars))
			{
				listPtr->columnInfo[i].pos = pos;
				listPtr->columnInfo[i].width = width;
				listPtr->columnInfo[i].maxChars = maxChars;
			}
			else
			{
				return qfalse;
			}
		}
	}
	else
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_border
===============
*/
static qboolean ItemParse_border(itemDef_t* item)
{
	if (PC_ParseInt(&item->window.border))
	{
		return qfalse;
	}

	return qtrue;
}

/*
===============
ItemParse_bordersize
===============
*/
static qboolean ItemParse_bordersize(itemDef_t* item)
{
	if (PC_ParseFloat(&item->window.borderSize))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_visible
===============
*/
static qboolean ItemParse_visible(itemDef_t* item)
{
	int i;

	if (PC_ParseInt(&i))
	{
		return qfalse;
	}
	if (i)
	{
		item->window.flags |= WINDOW_VISIBLE;
	}
	return qtrue;
}

/*
===============
ItemParse_ownerdraw
===============
*/
static qboolean ItemParse_ownerdraw(itemDef_t* item)
{
	if (PC_ParseInt(&item->window.ownerDraw))
	{
		return qfalse;
	}
	item->type = ITEM_TYPE_OWNERDRAW;
	return qtrue;
}

/*
===============
ItemParse_align
===============
*/
static qboolean ItemParse_align(itemDef_t* item)
{
	if (PC_ParseInt(&item->alignment))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_align
===============
*/
static qboolean ItemParse_Appearance_slot(itemDef_t* item)
{
	if (PC_ParseInt(&item->appearanceSlot))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_textalign
===============
*/
static qboolean ItemParse_textalign(itemDef_t* item)
{
	const char* tempStr;

	if (PC_ParseString(&tempStr))
	{
		return qfalse;
	}

	int i = 0;
	while (alignment[i])
	{
		if (Q_stricmp(tempStr, alignment[i]) == 0)
		{
			item->textalignment = i;
			break;
		}
		i++;
	}

	if (alignment[i] == nullptr)
	{
		PC_ParseWarning(va("Unknown text alignment value '%s'", tempStr));
	}

	return qtrue;
}

/*
===============
ItemParse_text2alignx
===============
*/
static qboolean ItemParse_text2alignx(itemDef_t* item)
{
	if (PC_ParseFloat(&item->text2alignx))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_text2aligny
===============
*/
static qboolean ItemParse_text2aligny(itemDef_t* item)
{
	if (PC_ParseFloat(&item->text2aligny))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_textalignx
===============
*/
static qboolean ItemParse_textalignx(itemDef_t* item)
{
	if (PC_ParseFloat(&item->textalignx))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_textaligny
===============
*/
static qboolean ItemParse_textaligny(itemDef_t* item)
{
	if (PC_ParseFloat(&item->textaligny))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_textscale
===============
*/
static qboolean ItemParse_textscale(itemDef_t* item)
{
	if (PC_ParseFloat(&item->textscale))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_textstyle
===============
*/
static qboolean ItemParse_textstyle(itemDef_t* item)
{
	if (PC_ParseInt(&item->textStyle))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_invertyesno
===============
*/
static qboolean ItemParse_invertyesno(itemDef_t* item)
{
	if (PC_ParseInt(&item->invertYesNo))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_xoffset (used for yes/no and multi)
===============
*/
static qboolean ItemParse_xoffset(itemDef_t* item)
{
	if (PC_ParseInt(&item->xoffset))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_backcolor
===============
*/
static qboolean ItemParse_backcolor(itemDef_t* item)
{
	float f;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		item->window.backColor[i] = f;
	}
	return qtrue;
}

/*
===============
ItemParse_forecolor
===============
*/
static qboolean ItemParse_forecolor(itemDef_t* item)
{
	float f;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		if (f < 0)
		{
			//special case for player color
			item->window.flags |= WINDOW_PLAYERCOLOR;
			return qtrue;
		}
		item->window.foreColor[i] = f;
		item->window.flags |= WINDOW_FORECOLORSET;
	}
	return qtrue;
}

/*
===============
ItemParse_bordercolor
===============
*/
static qboolean ItemParse_bordercolor(itemDef_t* item)
{
	float f;

	for (int i = 0; i < 4; i++)
	{
		if (PC_ParseFloat(&f))
		{
			return qfalse;
		}
		item->window.borderColor[i] = f;
	}
	return qtrue;
}

/*
===============
ItemParse_outlinecolor
===============
*/
static qboolean ItemParse_outlinecolor(itemDef_t* item)
{
	if (PC_ParseColor(&item->window.outlineColor))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_background
===============
*/
static qboolean ItemParse_background(itemDef_t* item)
{
	const char* temp;

	if (PC_ParseString(&temp))
	{
		return qfalse;
	}
	item->window.background = ui.R_RegisterShaderNoMip(temp);
	return qtrue;
}

/*
===============
ItemParse_cinematic
===============
*/
static qboolean ItemParse_cinematic(itemDef_t* item)
{
	if (!PC_ParseStringMem(&item->window.cinematicName))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_doubleClick
===============
*/
static qboolean ItemParse_doubleClick(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}

	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);

	if (!PC_Script_Parse(&listPtr->doubleClick))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_onFocus
===============
*/
static qboolean ItemParse_onFocus(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->onFocus))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_leaveFocus
===============
*/
static qboolean ItemParse_leaveFocus(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->leaveFocus))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_mouseEnter
===============
*/
static qboolean ItemParse_mouseEnter(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->mouseEnter))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_mouseExit
===============
*/
static qboolean ItemParse_mouseExit(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->mouseExit))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_mouseEnterText
===============
*/
static qboolean ItemParse_mouseEnterText(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->mouseEnterText))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_mouseExitText
===============
*/
static qboolean ItemParse_mouseExitText(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->mouseExitText))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_accept
===============
*/
static qboolean ItemParse_accept(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->accept))
	{
		return qfalse;
	}
	return qtrue;
}

//JLFDPADSCRIPT
/*
===============
ItemParse_selectionNext
===============
*/
static qboolean ItemParse_selectionNext(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->selectionNext))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_selectionPrev
===============
*/
static qboolean ItemParse_selectionPrev(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->selectionPrev))
	{
		return qfalse;
	}
	return qtrue;
}

// END JLFDPADSCRIPT

/*
===============
ItemParse_action
===============
*/
static qboolean ItemParse_action(itemDef_t* item)
{
	if (!PC_Script_Parse(&item->action))
	{
		return qfalse;
	}
	return qtrue;
}

static qboolean ItemParse_exec(itemDef_t* item)
{
	try
	{
		if (!PC_Script_Parse(&item->exec))
		{
			return qfalse;
		}
	}
	catch (...)
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_special
===============
*/
static qboolean ItemParse_special(itemDef_t* item)
{
	if (PC_ParseFloat(&item->special))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_cvarTest
===============
*/
static qboolean ItemParse_cvarTest(itemDef_t* item)
{
	if (!PC_ParseStringMem(&item->cvarTest))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
ItemParse_cvar
===============
*/
static qboolean ItemParse_cvar(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	if (!PC_ParseStringMem(&item->cvar))
	{
		return qfalse;
	}

	if (item->typeData)
	{
		editFieldDef_t* editPtr;
		switch (item->type)
		{
		case ITEM_TYPE_EDITFIELD:
		case ITEM_TYPE_NUMERICFIELD:
		case ITEM_TYPE_YESNO:
		case ITEM_TYPE_BIND:
		case ITEM_TYPE_SLIDER:
		case ITEM_TYPE_SLIDER_INTEGER:
		case ITEM_TYPE_TEXT:
		case ITEM_TYPE_TEXTSCROLL:
			editPtr = static_cast<editFieldDef_t*>(item->typeData);
			editPtr->minVal = -1;
			editPtr->maxVal = -1;
			editPtr->defVal = -1;
			break;
		case ITEM_TYPE_SLIDER_ROTATE:
			editPtr = static_cast<editFieldDef_t*>(item->typeData);
			editPtr->range = 720;
			editPtr->minVal = -1;
			editPtr->maxVal = -1;
			editPtr->defVal = -1;
			break;
		default:;
		}
	}
	return qtrue;
}

/*
===============
ItemParse_maxChars
===============
*/
static qboolean ItemParse_maxChars(itemDef_t* item)
{
	int maxChars;

	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}

	if (PC_ParseInt(&maxChars))
	{
		return qfalse;
	}
	const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);
	editPtr->maxChars = maxChars;
	return qtrue;
}

/*
===============
ItemParse_maxPaintChars
===============
*/
static qboolean ItemParse_maxPaintChars(itemDef_t* item)
{
	int maxChars;

	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}

	if (PC_ParseInt(&maxChars))
	{
		return qfalse;
	}
	const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);
	editPtr->maxPaintChars = maxChars;
	return qtrue;
}

static qboolean ItemParse_lineHeight(itemDef_t* item)
{
	int height;

	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}

	if (PC_ParseInt(&height))
	{
		return qfalse;
	}

	const auto scrollPtr = static_cast<textScrollDef_t*>(item->typeData);
	scrollPtr->lineHeight = height;

	return qtrue;
}

/*
===============
ItemParse_cvarFloat
===============
*/
static qboolean ItemParse_cvarInt(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}
	const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);

	if (PC_ParseStringMem(&item->cvar) &&
		!PC_ParseFloat(&editPtr->defVal) &&
		!PC_ParseFloat(&editPtr->minVal) &&
		!PC_ParseFloat(&editPtr->maxVal))
	{
		return qtrue;
	}
	return qfalse;
}

/*
===============
ItemParse_cvarFloat
===============
*/
static qboolean ItemParse_cvarFloat(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}
	const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);
	if (PC_ParseStringMem(&item->cvar) &&
		!PC_ParseFloat(&editPtr->defVal) &&
		!PC_ParseFloat(&editPtr->minVal) &&
		!PC_ParseFloat(&editPtr->maxVal))
	{
		if (!Q_stricmp(item->cvar, "r_ext_texture_filter_anisotropic"))
		{
			//hehe, hook up the correct max value here.
			editPtr->maxVal = cls.glconfig.maxTextureFilterAnisotropy;
		}
		return qtrue;
	}

	return qfalse;
}

/*
 ===============
 ItemParse_cvarRotateScale
 ===============
 */
static qboolean ItemParse_cvarRotateScale(itemDef_t* item)
{
	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}
	const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);
	if (PC_ParseStringMem(&item->cvar) &&
		!PC_ParseFloat(&editPtr->range))
	{
		return qtrue;
	}

	return qfalse;
}

/*
===============
ItemParse_cvarStrList
===============
*/
static qboolean ItemParse_cvarStrList(itemDef_t* item)
{
	const char* token;

	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}
	const auto multiPtr = static_cast<multiDef_t*>(item->typeData);
	multiPtr->count = 0;
	multiPtr->strDef = qtrue;

	if (PC_ParseString(&token))
	{
		return qfalse;
	}

	if (!Q_stricmp(token, "feeder") && item->special == FEEDER_PLAYER_SPECIES)
	{
		for (; multiPtr->count < uiInfo.playerSpeciesCount; multiPtr->count++)
		{
			multiPtr->cvarList[multiPtr->count] = String_Alloc(
				Q_strupr(va("@MENUS_%s", uiInfo.playerSpecies[multiPtr->count].Name))); //look up translation
			multiPtr->cvarStr[multiPtr->count] = uiInfo.playerSpecies[multiPtr->count].Name; //value
		}
		return qtrue;
	}
	// languages
	if (!Q_stricmp(token, "feeder") && item->special == FEEDER_LANGUAGES)
	{
		for (; multiPtr->count < uiInfo.languageCount; multiPtr->count++)
		{
			// The displayed text
			multiPtr->cvarList[multiPtr->count] = "@MENUS_MYLANGUAGE";
			// The cvar value that goes into se_language

#ifndef JK2_MODE // FIXME
			multiPtr->cvarStr[multiPtr->count] = SE_GetLanguageName(multiPtr->count);
#endif
		}
		return qtrue;
	}
	if (*token != '{')
	{
		return qfalse;
	}

	int pass = 0;
	while (true)
	{
		if (!PC_ParseStringMem(&token))
		{
			PC_ParseWarning("end of file inside menu item\n");
			return qfalse;
		}
		if (*token == '}')
		{
			return qtrue;
		}

		if (*token == ',' || *token == ';')
		{
			continue;
		}

		if (pass == 0)
		{
			multiPtr->cvarList[multiPtr->count] = token;
			pass = 1;
		}
		else
		{
			multiPtr->cvarStr[multiPtr->count] = token;
			pass = 0;
			multiPtr->count++;
			if (multiPtr->count >= MAX_MULTI_CVARS)
			{
				return qfalse;
			}
		}
	}
}

/*
===============
ItemParse_cvarFloatList
===============
*/
static qboolean ItemParse_cvarFloatList(itemDef_t* item)
{
	const char* token;

	Item_ValidateTypeData(item);
	if (!item->typeData)
	{
		return qfalse;
	}
	const auto multiPtr = static_cast<multiDef_t*>(item->typeData);
	multiPtr->count = 0;
	multiPtr->strDef = qfalse;

	if (PC_ParseString(&token))
	{
		return qfalse;
	}

	if (*token != '{')
	{
		return qfalse;
	}

	while (true)
	{
		if (!PC_ParseStringMem(&token))
		{
			PC_ParseWarning("end of file inside menu item\n");
			return qfalse;
		}
		if (*token == '}')
		{
			return qtrue;
		}

		if (*token == ',' || *token == ';')
		{
			continue;
		}

		multiPtr->cvarList[multiPtr->count] = token; //a StringAlloc ptr
		if (PC_ParseFloat(&multiPtr->cvarValue[multiPtr->count]))
		{
			return qfalse;
		}

		multiPtr->count++;
		if (multiPtr->count >= MAX_MULTI_CVARS)
		{
			return qfalse;
		}
	}
}

/*
===============
ItemParse_addColorRange
===============
*/
static qboolean ItemParse_addColorRange(itemDef_t* item)
{
	colorRangeDef_t color{};

	if (PC_ParseFloat(&color.low) &&
		PC_ParseFloat(&color.high) &&
		PC_ParseColor(&color.color))
	{
		if (item->numColors < MAX_COLOR_RANGES)
		{
			memcpy(&item->colorRanges[item->numColors], &color, sizeof color);
			item->numColors++;
		}
		return qtrue;
	}
	return qfalse;
}

/*
===============
ItemParse_ownerdrawFlag
===============
*/
static qboolean ItemParse_ownerdrawFlag(itemDef_t* item)
{
	int i;
	if (PC_ParseInt(&i))
	{
		return qfalse;
	}
	item->window.ownerDrawFlags |= i;
	return qtrue;
}

/*
===============
ItemParse_enableCvar
===============
*/
static qboolean ItemParse_enableCvar(itemDef_t* item)
{
	if (PC_Script_Parse(&item->enableCvar))
	{
		item->cvarFlags = CVAR_ENABLE;
		return qtrue;
	}
	return qfalse;
}

/*
===============
ItemParse_disableCvar
===============
*/
static qboolean ItemParse_disableCvar(itemDef_t* item)
{
	if (PC_Script_Parse(&item->enableCvar))
	{
		item->cvarFlags = CVAR_DISABLE;
		return qtrue;
	}
	return qfalse;
}

/*
===============
ItemParse_showCvar
===============
*/
static qboolean ItemParse_showCvar(itemDef_t* item)
{
	if (PC_Script_Parse(&item->enableCvar))
	{
		item->cvarFlags = CVAR_SHOW;
		return qtrue;
	}
	return qfalse;
}

/*
===============
ItemParse_hideCvar
===============
*/
static qboolean ItemParse_hideCvar(itemDef_t* item)
{
	if (PC_Script_Parse(&item->enableCvar))
	{
		item->cvarFlags = CVAR_HIDE;
		return qtrue;
	}
	return qfalse;
}

/*
===============
ItemParse_cvarsubstring
===============
*/
static qboolean ItemParse_cvarsubstring(itemDef_t* item)
{
	assert(item->cvarFlags); //need something set first, then we or in our flag.
	item->cvarFlags |= CVAR_SUBSTRING;
	return qtrue;
}

/*
===============
Item_ValidateTypeData
===============
*/
void Item_ValidateTypeData(itemDef_t* item)
{
	if (item->typeData)
	{
		return;
	}

	if (item->type == ITEM_TYPE_LISTBOX)
	{
		item->typeData = UI_Alloc(sizeof(listBoxDef_t));
		memset(item->typeData, 0, sizeof(listBoxDef_t));
	}
	else if (item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD || item->type == ITEM_TYPE_YESNO
		|| item->type == ITEM_TYPE_BIND || item->type == ITEM_TYPE_SLIDER || item->type == ITEM_TYPE_SLIDER_INTEGER ||
		item->type == ITEM_TYPE_TEXT || item->type == ITEM_TYPE_SLIDER_ROTATE)
	{
		item->typeData = UI_Alloc(sizeof(editFieldDef_t));
		memset(item->typeData, 0, sizeof(editFieldDef_t));
		if (item->type == ITEM_TYPE_EDITFIELD)
		{
			if (!static_cast<editFieldDef_t*>(item->typeData)->maxPaintChars)
			{
				static_cast<editFieldDef_t*>(item->typeData)->maxPaintChars = MAX_EDITFIELD;
			}
		}
	}
	else if (item->type == ITEM_TYPE_MULTI)
	{
		item->typeData = UI_Alloc(sizeof(multiDef_t));
	}
	else if (item->type == ITEM_TYPE_MODEL)
	{
		item->typeData = UI_Alloc(sizeof(modelDef_t));
		memset(item->typeData, 0, sizeof(modelDef_t));
	}
	else if (item->type == ITEM_TYPE_TEXTSCROLL)
	{
		item->typeData = UI_Alloc(sizeof(textScrollDef_t));
	}
}

static qboolean ItemParse_isCharacter(itemDef_t* item)
{
	int i;
	if (!PC_ParseInt(&i))
	{
		if (i)
		{
			item->flags |= ITF_ISCHARACTER;
		}
		else
		{
			item->flags &= ~ITF_ISCHARACTER;
		}
		return qtrue;
	}
	return qfalse;
}

static qboolean ItemParse_isSaber(itemDef_t* item)
{
	int i;
	if (!PC_ParseInt(&i))
	{
		if (i)
		{
			extern qboolean ui_saber_parms_parsed;
			item->flags |= ITF_ISSABER;
			UI_CacheSaberGlowGraphics();
			if (!ui_saber_parms_parsed)
			{
				UI_SaberLoadParms();
			}
		}
		else
		{
			item->flags &= ~ITF_ISSABER;
		}
		return qtrue;
	}
	return qfalse;
}

static qboolean ItemParse_isSaber2(itemDef_t* item)
{
	int i;
	if (!PC_ParseInt(&i))
	{
		if (i)
		{
			extern qboolean ui_saber_parms_parsed;
			item->flags |= ITF_ISSABER2;
			UI_CacheSaberGlowGraphics();
			if (!ui_saber_parms_parsed)
			{
				UI_SaberLoadParms();
			}
		}
		else
		{
			item->flags &= ~ITF_ISSABER2;
		}
		return qtrue;
	}
	return qfalse;
}

keywordHash_t itemParseKeywords[] = {
	{"accept", ItemParse_accept,},
	{"selectNext", ItemParse_selectionNext,},
	{"selectPrev", ItemParse_selectionPrev,},
	{"action", ItemParse_action,},
	{"addColorRange", ItemParse_addColorRange,},
	{"align", ItemParse_align,},
	{"appearance_slot", ItemParse_Appearance_slot,},
	{"asset_model", ItemParse_asset_model,},
	{"asset_shader", ItemParse_asset_shader,},
	{"isCharacter", ItemParse_isCharacter,},
	{"isSaber", ItemParse_isSaber,},
	{"isSaber2", ItemParse_isSaber2,},
	{"autowrapped", ItemParse_autowrapped,},
	{"backcolor", ItemParse_backcolor,},
	{"background", ItemParse_background,},
	{"border", ItemParse_border,},
	{"bordercolor", ItemParse_bordercolor,},
	{"bordersize", ItemParse_bordersize,},
	{"cinematic", ItemParse_cinematic,},
	{"columns", ItemParse_columns,},
	{"cvar", ItemParse_cvar,},
	{"cvarFloat", ItemParse_cvarFloat,},
	{"cvarInt", ItemParse_cvarInt,},
	{"cvarRotateScale", ItemParse_cvarRotateScale,},
	{"cvarFloatList", ItemParse_cvarFloatList,},
	{"cvarSubString", ItemParse_cvarsubstring},
	{"cvarStrList", ItemParse_cvarStrList,},
	{"cvarTest", ItemParse_cvarTest,},
	{"decoration", ItemParse_decoration,},
	{"desctext", ItemParse_descText},
	{"disableCvar", ItemParse_disableCvar,},
	{"doubleclick", ItemParse_doubleClick,},
	{"elementheight", ItemParse_elementheight,},
	{"elementtype", ItemParse_elementtype,},
	{"elementwidth", ItemParse_elementwidth,},
	{"enableCvar", ItemParse_enableCvar,},
	{"exec", ItemParse_exec,},
	{"feeder", ItemParse_feeder,},
	{"flag", ItemParse_flag,},
	{"focusSound", ItemParse_focusSound,},
	{"font", ItemParse_font,},
	{"forecolor", ItemParse_forecolor,},
	{"group", ItemParse_group,},
	{"hideCvar", ItemParse_hideCvar,},
	{"horizontalscroll", ItemParse_horizontalscroll,},
	{"leaveFocus", ItemParse_leaveFocus,},
	{"maxChars", ItemParse_maxChars,},
	{"maxPaintChars", ItemParse_maxPaintChars,},
	{"model_angle", ItemParse_model_angle,},
	{"model_fovx", ItemParse_model_fovx,},
	{"model_fovy", ItemParse_model_fovy,},
	{"model_origin", ItemParse_model_origin,},
	{"model_rotation", ItemParse_model_rotation,},
	//rww - g2 begin
	{"model_g2mins", ItemParse_model_g2mins,},
	{"model_g2maxs", ItemParse_model_g2maxs,},
	{"model_g2skin", ItemParse_model_g2skin,},
	{"model_g2anim", ItemParse_model_g2anim,},
	//rww - g2 end
	{"mouseEnter", ItemParse_mouseEnter,},
	{"mouseEnterText", ItemParse_mouseEnterText,},
	{"mouseExit", ItemParse_mouseExit,},
	{"mouseExitText", ItemParse_mouseExitText,},
	{"name", ItemParse_name},
	{"notselectable", ItemParse_notselectable,},
	//JLF
	{"scrollhidden", ItemParse_scrollhidden,},
	//JLF END
	{"onFocus", ItemParse_onFocus,},
	{"outlinecolor", ItemParse_outlinecolor,},
	{"ownerdraw", ItemParse_ownerdraw,},
	{"ownerdrawFlag", ItemParse_ownerdrawFlag,},
	{"rect", ItemParse_rect,},
	{"showCvar", ItemParse_showCvar,},
	{"special", ItemParse_special,},
	{"style", ItemParse_style,},
	{"text", ItemParse_text},
	{"text2", ItemParse_text2},
	{"text2alignx", ItemParse_text2alignx,},
	{"text2aligny", ItemParse_text2aligny,},
	{"textalign", ItemParse_textalign,},
	{"textalignx", ItemParse_textalignx,},
	{"textaligny", ItemParse_textaligny,},
	{"textscale", ItemParse_textscale,},
	{"textstyle", ItemParse_textstyle,},
	{"type", ItemParse_type,},
	{"visible", ItemParse_visible,},
	{"wrapped", ItemParse_wrapped,},
	{"invertyesno", ItemParse_invertyesno},
	{"xoffset", ItemParse_xoffset}, //for yes/no and multi

	// Text scroll specific
	{"lineHeight", ItemParse_lineHeight, nullptr},

	{nullptr, nullptr,}
};

keywordHash_t* itemParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Item_SetupKeywordHash
===============
*/
void Item_SetupKeywordHash()
{
	memset(itemParseKeywordHash, 0, sizeof itemParseKeywordHash);
	for (int i = 0; itemParseKeywords[i].keyword; i++)
	{
		KeywordHash_Add(itemParseKeywordHash, &itemParseKeywords[i]);
	}
}

/*
===============
Item_Parse
===============
*/
qboolean Item_Parse(itemDef_t* item)
{
	const char* token;

	if (PC_ParseString(&token))
	{
		return qfalse;
	}

	if (*token != '{')
	{
		return qfalse;
	}

	while (true)
	{
		if (PC_ParseString(&token))
		{
			PC_ParseWarning("End of file inside menu item");
			return qfalse;
		}

		if (*token == '}')
		{
			return qtrue;
		}

		const keywordHash_t* key = KeywordHash_Find(itemParseKeywordHash, token);
		if (!key)
		{
			PC_ParseWarning(va("Unknown item keyword '%s'", token));
			continue;
		}

		if (!key->func(item))
		{
			PC_ParseWarning(va("Couldn't parse item keyword '%s'", token));
			return qfalse;
		}
	}
}

static void Item_TextScroll_BuildLines(const itemDef_t* item)
{
	// new asian-aware line breaker...  (pasted from elsewhere late @ night, hence aliasing-vars ;-)
	//
	const auto scrollPtr = static_cast<textScrollDef_t*>(item->typeData);
	const char* psText = item->text; // for copy/paste ease
	const int iBoxWidth = item->window.rect.w - SCROLLBAR_SIZE - 10;

	qboolean bIsTrailingPunctuation;

	if (!psText)
	{
		return;
	}

	if (*psText == '@') // string reference
	{
		//		trap_SP_GetStringTextString( &psText[1], text, sizeof(text));
		psText = SE_GetString(&psText[1]);
	}

	const char* psCurrentTextReadPos = psText;
	const char* psReadPosAtLineStart = psCurrentTextReadPos;
	const char* psBestLineBreakSrcPos = psCurrentTextReadPos;

	scrollPtr->iLineCount = 0;
	memset(scrollPtr->pLines, 0, sizeof scrollPtr->pLines);

	while (*psCurrentTextReadPos && scrollPtr->iLineCount < MAX_TEXTSCROLL_LINES)
	{
		char sLineForDisplay[2048]{}; // ott

		// construct a line...
		//
		psCurrentTextReadPos = psReadPosAtLineStart;
		sLineForDisplay[0] = '\0';
		while (*psCurrentTextReadPos)
		{
			int iAdvanceCount;
			const char* psLastGood_s = psCurrentTextReadPos;

			// read letter...
			//
			const unsigned int uiLetter = ui.AnyLanguage_ReadCharFromString((char*)psCurrentTextReadPos, &iAdvanceCount,
				&bIsTrailingPunctuation);
			psCurrentTextReadPos += iAdvanceCount;

			// concat onto string so far...
			//
			if (uiLetter == 32 && sLineForDisplay[0] == '\0')
			{
				psReadPosAtLineStart++;
				continue; // unless it's a space at the start of a line, in which case ignore it.
			}

			if (uiLetter > 255)
			{
				Q_strcat(sLineForDisplay, sizeof sLineForDisplay, va("%c%c", uiLetter >> 8, uiLetter & 0xFF));
			}
			else
			{
				Q_strcat(sLineForDisplay, sizeof sLineForDisplay, va("%c", uiLetter & 0xFF));
			}

			if (uiLetter == '\n')
			{
				// explicit new line...
				//
				sLineForDisplay[strlen(sLineForDisplay) - 1] = '\0'; // kill the CR
				psReadPosAtLineStart = psCurrentTextReadPos;
				psBestLineBreakSrcPos = psCurrentTextReadPos;

				// hack it to fit in with this code...
				//
				scrollPtr->pLines[scrollPtr->iLineCount] = String_Alloc(sLineForDisplay);
				break; // print this line
			}
			if (DC->textWidth(sLineForDisplay, item->textscale, item->font) >= iBoxWidth)
			{
				// reached screen edge, so cap off string at bytepos after last good position...
				//
				if (uiLetter > 255 && bIsTrailingPunctuation && !ui.Language_UsesSpaces())
				{
					// Special case, don't consider line breaking if you're on an asian punctuation char of
					//	a language that doesn't use spaces...
					//
				}
				else
				{
					if (psBestLineBreakSrcPos == psReadPosAtLineStart)
					{
						//  aarrrggh!!!!!   we'll only get here is someone has fed in a (probably) garbage string,
						//		since it doesn't have a single space or punctuation mark right the way across one line
						//		of the screen.  So far, this has only happened in testing when I hardwired a taiwanese
						//		string into this function while the game was running in english (which should NEVER happen
						//		normally).  On the other hand I suppose it's entirely possible that some taiwanese string
						//		might have no punctuation at all, so...
						//
						psBestLineBreakSrcPos = psLastGood_s; // force a break after last good letter
					}

					sLineForDisplay[psBestLineBreakSrcPos - psReadPosAtLineStart] = '\0';
					psReadPosAtLineStart = psCurrentTextReadPos = psBestLineBreakSrcPos;

					// hack it to fit in with this code...
					//
					scrollPtr->pLines[scrollPtr->iLineCount] = String_Alloc(sLineForDisplay);
					break; // print this line
				}
			}

			// record last-good linebreak pos...  (ie if we've just concat'd a punctuation point (western or asian) or space)
			//
			if (bIsTrailingPunctuation || uiLetter == ' ' || uiLetter > 255 && !ui.Language_UsesSpaces())
			{
				psBestLineBreakSrcPos = psCurrentTextReadPos;
			}
		}

		/// arrgghh, this is gettng horrible now...
		//
		if (scrollPtr->pLines[scrollPtr->iLineCount] == nullptr && strlen(sLineForDisplay))
		{
			// then this is the last line and we've just run out of text, no CR, no overflow etc...
			//
			scrollPtr->pLines[scrollPtr->iLineCount] = String_Alloc(sLineForDisplay);
		}

		scrollPtr->iLineCount++;
	}
}

/*
===============
Item_InitControls
	init's special control types
===============
*/
void Item_InitControls(itemDef_t* item)
{
	if (item == nullptr)
	{
		return;
	}
	if (item->type == ITEM_TYPE_LISTBOX)
	{
		const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
		item->cursorPos = 0;
		if (listPtr)
		{
			listPtr->cursorPos = 0;
			listPtr->startPos = 0;
			listPtr->endPos = 0;
			listPtr->cursorPos = 0;
		}
	}
}

/*
=================
Int_Parse
=================
*/
qboolean Int_Parse(const char** p, int* i)
{
	const char* token = COM_ParseExt(p, qfalse);

	if (token && token[0] != 0)
	{
		*i = atoi(token);
		return qtrue;
	}
	return qfalse;
}

/*
=================
String_Parse
=================
*/
qboolean String_Parse(const char** p, const char** out)
{
	const char* token = COM_ParseExt(p, qfalse);
	if (token && token[0] != 0)
	{
		*out = String_Alloc(token);
		return static_cast<qboolean>(*out != nullptr);
	}
	return qfalse;
}

/*
===============
Item_RunScript
===============
*/
void Item_RunScript(itemDef_t* item, const char* s)
{
	const char* p;

	uiInfo.runScriptItem = item;

	if (item && s && s[0])
	{
		p = s;
		COM_BeginParseSession();
		while (true)
		{
			const char* command;
			// expect command then arguments, ; ends command, NULL ends script
			if (!String_Parse(&p, &command))
			{
				break;
			}

			if (command[0] == ';' && command[1] == '\0')
			{
				continue;
			}

			qboolean bRan = qfalse;
			for (int i = 0; i < scriptCommandCount; i++)
			{
				if (Q_stricmp(command, commandList[i].name) == 0)
				{
					if (!commandList[i].handler(item, &p))
					{
						COM_EndParseSession();
						return;
					}

					bRan = qtrue;
					break;
				}
			}
			// not in our auto list, pass to handler
			if (!bRan)
			{
				// Allow any script command to fail
				if (!DC->runScript(&p))
				{
					break;
				}
			}
		}
		COM_EndParseSession();
	}
}

/*
===============
Menu_SetupKeywordHash
===============
*/
void Menu_SetupKeywordHash()
{
	memset(menuParseKeywordHash, 0, sizeof menuParseKeywordHash);
	for (int i = 0; menuParseKeywords[i].keyword; i++)
	{
		KeywordHash_Add(menuParseKeywordHash, &menuParseKeywords[i]);
	}
}

/*
===============
Menus_ActivateByName
===============
*/
void Menu_HandleMouseMove(const menuDef_t* menu, float x, float y);

menuDef_t* Menus_ActivateByName(const char* p)
{
	menuDef_t* m = nullptr;
	menuDef_t* focus = Menu_GetFocused();

	for (int i = 0; i < menuCount; i++)
	{
		// Look for the name in the current list of windows
		if (Q_stricmp(Menus[i].window.name, p) == 0)
		{
			m = &Menus[i];
			Menus_Activate(m);
			if (openMenuCount < MAX_OPEN_MENUS && focus != nullptr)
			{
				menuStack[openMenuCount++] = focus;
			}
		}
		else
		{
			Menus[i].window.flags &= ~WINDOW_HASFOCUS;
		}
	}

	if (!m)
	{
		// A hack so we don't have to load all three mission menus before we know what tier we're on
		if (!Q_stricmp(p, "ingameMissionSelect1"))
		{
			UI_LoadMenus("ui/tier1.txt", qfalse);
			Menus_CloseAll();
			Menus_OpenByName("ingameMissionSelect1");
		}
		else if (!Q_stricmp(p, "ingameMissionSelect2"))
		{
			UI_LoadMenus("ui/tier2.txt", qfalse);
			Menus_CloseAll();
			Menus_OpenByName("ingameMissionSelect2");
		}
		else if (!Q_stricmp(p, "ingameMissionSelect3"))
		{
			UI_LoadMenus("ui/tier3.txt", qfalse);
			Menus_CloseAll();
			Menus_OpenByName("ingameMissionSelect3");
		}
		else
		{
			Com_Printf(S_COLOR_YELLOW"WARNING: Menus_ActivateByName: Unable to find menu \"%s\"\n", p);
		}
	}

	// First time, show force select instructions
	if (!Q_stricmp(p, "ingameForceSelect"))
	{
		const int tier_storyinfo = Cvar_VariableIntegerValue("tier_storyinfo");

		if (tier_storyinfo == 1)
		{
			Menus_OpenByName("ingameForceHelp");
		}
	}

	// First time, show weapons select instructions
	if (!Q_stricmp(p, "ingameWpnSelect"))
	{
		const int tier_storyinfo = Cvar_VariableIntegerValue("tier_storyinfo");

		if (tier_storyinfo == 1)
		{
			Menus_OpenByName("ingameWpnSelectHelp");
		}
	}

	// Want to handle a mouse move on the new menu in case your already over an item
	Menu_HandleMouseMove(m, DC->cursorx, DC->cursory);

	return m;
}

/*
===============
Menus_Activate
===============
*/
void Menus_Activate(menuDef_t* menu)
{
	menu->window.flags |= WINDOW_HASFOCUS | WINDOW_VISIBLE;

	if (menu->onOpen)
	{
		itemDef_t item;
		item.parent = menu;
		item.window.flags = 0; //err, item is fake here, but we want a valid flag before calling runscript
		Item_RunScript(&item, menu->onOpen);

		if (item.window.flags & WINDOW_SCRIPTWAITING) //in case runscript set waiting, copy it up to the menu
		{
			menu->window.flags |= WINDOW_SCRIPTWAITING;
			menu->window.delayedScript = item.window.delayedScript;
			menu->window.delayTime = item.window.delayTime;
		}
	}

	//	menu->appearanceTime = DC->realTime + 1000;
	menu->appearanceTime = 0;
	menu->appearanceCnt = 0;
}

static const char* g_bindCommands[] =
{
	"+altattack",
	"+attack",
	"+back",
#ifndef JK2_MODE
	"+force_drain",
#endif
	"+force_grip",
	"+force_lightning",
	"+forward",
	"+left",
	"+lookdown",
	"+lookup",
	"+mlook",
	"+movedown",
	"+moveleft",
	"+moveright",
	"+moveup",
	"+right",
	"+speed",
	"+strafe",
	"+use",
	"+useforce",
	"centerview",
	"cg_thirdperson !",
	"datapad",
	"exitview",
#ifndef JK2_MODE
	"force_absorb",
#endif
	"force_distract",
	"force_heal",
#ifndef JK2_MODE
	"force_protect",
#endif
	"force_pull",
#ifndef JK2_MODE
	"force_rage",
	"force_sight",
#endif
	"force_speed",
#ifndef JK2_MODE
	"force_destruction",
	"force_stasis",
	"force_grasp",
	"force_repulse",
	"force_strike",
	"force_fear",
	"force_deadlysight",
	"force_projection",
	"force_blast",
#endif
	"force_throw",
	"forcenext",
	"forceprev",
	"invnext",
	"invprev",
	"invuse",
	"load auto",
#ifdef JK2_MODE
	"load quik",
#else
	"load quick",
#endif
	"saberAttackCycle",
#ifdef JK2_MODE
	"save quik*",
#else
	"save quick",
#endif
	"taunt",
	"uimenu ingameloadmenu",
	"uimenu ingamesavemenu",
	"use_bacta",
	"use_electrobinoculars",
	"use_lightamp_goggles",
	"use_seeker",
	"use_sentry",
	"weapnext",
	"weapon 0",
	"weapon 1",
	"weapon 10",
	"weapon 11",
	"weapon 12",
	"weapon 13",
	"weapon 2",
	"weapon 3",
	"weapon 4",
	"weapon 5",
	"weapon 6",
	"weapon 7",
	"weapon 8",
	"weapon 9",
	"weapprev",
	"zoom",
	"bow",
	"meditate",
	"flourish",
	"gloat",
	"+button9",
	"+button10",
	"+button11",
	"+button12",
	"+button13",
	"+button14",
	"+button15",
	"+button16",
	"+button17",
	"+button18",
	"+button19",
	"+button20",
	"use_Cloak",
	"use_barrier",
	"weather",
	"saberdown",
	//emotes
	"myhead",
	"cower",
	"smack",
	"enraged",
	"victory2",
	"victory3",
	"swirl",
	"dance",
	"dance2",
	"dance3",
	"sit2",
	"point",
	"kneel2",
	"kneel",
	"laydown",
	"breakdance",
	"cheer",
	"comeon",
	"headshake",
	"headnod",
	"surrender",
	"combatstance",
	"reload",
	"atease",
	"punch",
	"intimidate",
	"slash",
	"sit",
	"harlem",
	"hello",
	"hips",
	"won",
	"emote",
	"vstr npc_spawn_recent",
	"vstr npc_recent_command",
	"vstr opennpcmenu",
	"d_npcfreeze !",
	"g_allowdualpistols !",
	"cl_run !",
	"vstr opencharactermenu",
	"uimenu ingamesabercustomizationMenu",
	"uimenu ingamespawncommandsMenu",
	"uimenu ingameweaponwheelMenu",
	"scale",
	"r_weather"
};

#define g_bindCount ARRAY_LEN(g_bindCommands)

static int g_bindKeys[g_bindCount][2];

/*
=================
Controls_GetKeyAssignment
=================
*/
static void Controls_GetKeyAssignment(const char* command, int* twokeys)
{
	twokeys[0] = twokeys[1] = -1;
	int count = 0;

	for (int j = 0; j < MAX_KEYS; j++)
	{
		char b[256];
		DC->getBindingBuf(j, b, sizeof b);
		if (*b && !Q_stricmp(b, command))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

/*
=================
Controls_GetConfig
=================
*/
void Controls_GetConfig()
{
	// iterate each command, get its numeric binding
	for (size_t i = 0; i < g_bindCount; i++)
		Controls_GetKeyAssignment(g_bindCommands[i], g_bindKeys[i]);
}

/*
===============
Item_SetScreenCoords
===============
*/
static void Item_SetScreenCoords(itemDef_t* item, float x, float y)
{
	if (item == nullptr)
	{
		return;
	}

	if (item->window.border != 0)
	{
		x += item->window.borderSize;
		y += item->window.borderSize;
	}

	item->window.rect.x = x + item->window.rectClient.x;
	item->window.rect.y = y + item->window.rectClient.y;
	item->window.rect.w = item->window.rectClient.w;
	item->window.rect.h = item->window.rectClient.h;

	// force the text rects to recompute
	item->textRect.w = 0;
	item->textRect.h = 0;

	switch (item->type)
	{
	case ITEM_TYPE_TEXTSCROLL:
	{
		const auto scrollPtr = static_cast<textScrollDef_t*>(item->typeData);
		if (scrollPtr)
		{
			scrollPtr->startPos = 0;
			scrollPtr->endPos = 0;
		}

		Item_TextScroll_BuildLines(item);

		break;
	}
	default:;
	}
}

static void Menu_FreeGhoulItems(const menuDef_t* menu)
{
	for (int i = 0; i < menu->itemCount; i++)
	{
		for (int j = 0; j < menu->items[i]->ghoul2.size(); j++)
		{
			if (menu->items[i]->ghoul2[j].mModelindex >= 0)
			{
				DC->g2_RemoveGhoul2Model(menu->items[i]->ghoul2, j);
			}
		}
		menu->items[i]->ghoul2.clear(); //clear is the public Free so i can actually remove this slot
	}
}

/*
===============
Menu_Reset
===============
*/
void Menu_Reset()
{
	for (int i = 0; i < menuCount; i++)
	{
		Menu_FreeGhoulItems(&Menus[i]);
	}

	menuCount = 0;
}

/*
===============
Menu_UpdatePosition
===============
*/
static void Menu_UpdatePosition(const menuDef_t* menu)
{
	if (menu == nullptr)
	{
		return;
	}

	float x = menu->window.rect.x;
	float y = menu->window.rect.y;
	if (menu->window.border != 0)
	{
		x += menu->window.borderSize;
		y += menu->window.borderSize;
	}

	for (int i = 0; i < menu->itemCount; i++)
	{
		Item_SetScreenCoords(menu->items[i], x, y);
	}
}

/*
===============
Menu_PostParse
===============
*/
static void Menu_PostParse(menuDef_t* menu)
{
	if (menu == nullptr)
	{
		return;
	}

	if (menu->fullScreen)
	{
		menu->window.rect.x = 0;
		menu->window.rect.y = 0;
		menu->window.rect.w = 640;
		menu->window.rect.h = 480;
	}
	Menu_UpdatePosition(menu);
}

/*
===============
Menu_Init
===============
*/
static void Menu_Init(menuDef_t* menu)
{
	memset(menu, 0, sizeof(menuDef_t));
	menu->cursorItem = -1;

	UI_Cursor_Show(qtrue);

	if (DC)
	{
		menu->fadeAmount = DC->Assets.fadeAmount;
		menu->fadeClamp = DC->Assets.fadeClamp;
		menu->fadeCycle = DC->Assets.fadeCycle;
	}

	Window_Init(&menu->window);
}

/*
===============
Menu_Parse
===============
*/
static qboolean Menu_Parse(char* inbuffer, menuDef_t* menu)
{
	bool nest = false;
	char* buffer = inbuffer;

	char* token2 = PC_ParseExt();

	if (!token2)
	{
		return qfalse;
	}

	if (*token2 != '{')
	{
		PC_ParseWarning("Misplaced {");
		return qfalse;
	}
	while (true)
	{
		token2 = PC_ParseExt();
		if (!token2)
		{
			PC_ParseWarning("End of file inside menu.");
			return qfalse;
		}

		if (*token2 == '}')
		{
			return qtrue;
		}

		if (nest && *token2 == 0)
		{
			PC_EndParseSession(buffer);

			nest = false;
			continue;
		}
		const keywordHash_t* key = KeywordHash_Find(menuParseKeywordHash, token2);

		if (!key)
		{
			PC_ParseWarning(va("Unknown menu keyword %s", token2));
			continue;
		}

		if (!key->func((itemDef_t*)menu))
		{
			PC_ParseWarning(va("Couldn't parse menu keyword %s as %s", token2, key->keyword));
			return qfalse;
		}
	}
}

/*
===============
Menu_New
===============
*/
void Menu_New(char* buffer)
{
	menuDef_t* menu = &Menus[menuCount];

	if (menuCount < MAX_MENUS)
	{
		Menu_Init(menu);
		if (Menu_Parse(buffer, menu))
		{
			Menu_PostParse(menu);
			menuCount++;
		}
	}
}

/*
===============
Menus_CloseAll
===============
*/
void Menus_CloseAll()
{
	for (int i = 0; i < menuCount; i++)
	{
		Menu_RunCloseScript(&Menus[i]);
		Menus[i].window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE);
	}

	// Clear the menu stack
	openMenuCount = 0;
}

/*
===============
PC_StartParseSession
===============
*/

int PC_StartParseSession(const char* fileName, char** buffer)
{
	// Try to open file and read it in.
	const int len = ui.FS_ReadFile(fileName, (void**)buffer);

	// Not there?
	if (len > 0)
	{
		COM_BeginParseSession();

		Q_strncpyz(parseData[parseDataCount].fileName, fileName, sizeof parseData[0].fileName);
		parseData[parseDataCount].bufferStart = *buffer;
		parseData[parseDataCount].bufferCurrent = *buffer;
	}

	return len;
}

/*
===============
PC_EndParseSession
===============
*/
void PC_EndParseSession(char* buffer)
{
	COM_EndParseSession();
	ui.FS_FreeFile(buffer); //let go of the buffer
}

/*
===============
PC_ParseWarning
===============
*/
void PC_ParseWarning(const char* message)
{
	ui.Printf(S_COLOR_YELLOW "WARNING: %s Line #%d of File '%s'\n", message, parseData[parseDataCount].com_lines,
		parseData[parseDataCount].fileName);
}

char* PC_ParseExt()
{
	if (parseDataCount < 0)
		Com_Error(ERR_FATAL, "PC_ParseExt: parseDataCount < 0 (be sure to call PC_StartParseSession!)");
	return COM_ParseExt(&parseData[parseDataCount].bufferCurrent, qtrue);
}

qboolean PC_ParseString(const char** string)
{
	if (parseDataCount < 0)
		Com_Error(ERR_FATAL, "PC_ParseString: parseDataCount < 0 (be sure to call PC_StartParseSession!)");

	int hold = COM_ParseString(&parseData[parseDataCount].bufferCurrent, string);

	while (hold == 0 && **string == 0)
	{
		hold = COM_ParseString(&parseData[parseDataCount].bufferCurrent, string);
	}

	return static_cast<qboolean>(hold != 0);
}

qboolean PC_ParseInt(int* number)
{
	if (parseDataCount < 0)
		Com_Error(ERR_FATAL, "PC_ParseInt: parseDataCount < 0 (be sure to call PC_StartParseSession!)");

	return COM_ParseInt(&parseData[parseDataCount].bufferCurrent, number);
}

qboolean PC_ParseFloat(float* number)
{
	if (parseDataCount < 0)
		Com_Error(ERR_FATAL, "PC_ParseFloat: parseDataCount < 0 (be sure to call PC_StartParseSession!)");

	return COM_ParseFloat(&parseData[parseDataCount].bufferCurrent, number);
}

qboolean PC_ParseColor(vec4_t* color)
{
	if (parseDataCount < 0)
		Com_Error(ERR_FATAL, "PC_ParseColor: parseDataCount < 0 (be sure to call PC_StartParseSession!)");

	return COM_ParseVec4(&parseData[parseDataCount].bufferCurrent, color);
}

/*
=================
Menu_Count
=================
*/
int Menu_Count()
{
	return menuCount;
}

/*
=================
Menu_PaintAll
=================
*/
void Menu_PaintAll()
{
	if (captureFunc)
	{
		captureFunc(captureData);
	}

	for (int i = 0; i < menuCount; i++)
	{
		Menu_Paint(&Menus[i], qfalse);
	}

	if (uis.debugMode)
	{
		vec4_t v = { 1, 1, 1, 1 };
		DC->drawText(5, 25, .75, v, va("(%d,%d)", DC->cursorx, DC->cursory), 0, 0, DC->Assets.qhMediumFont);
		DC->drawText(5, 10, .75, v, va("fps: %f", DC->FPS), 0, 0, DC->Assets.qhMediumFont);
	}
}

/*
=================
Menu_Paint
=================
*/
void Menu_Paint(menuDef_t* menu, qboolean forcePaint)
{
	int i;

	if (menu == nullptr)
	{
		return;
	}

	if (menu->window.flags & WINDOW_SCRIPTWAITING)
	{
		if (DC->realTime > menu->window.delayTime)
		{
			// Time has elapsed, resume running whatever script we saved
			itemDef_t item;
			item.parent = menu;
			item.window.flags = 0; //clear before calling RunScript
			menu->window.flags &= ~WINDOW_SCRIPTWAITING;
			Item_RunScript(&item, menu->window.delayedScript);

			// Could have hit another delay. Need to hoist from fake item
			if (item.window.flags & WINDOW_SCRIPTWAITING)
			{
				menu->window.flags |= WINDOW_SCRIPTWAITING;
				menu->window.delayedScript = item.window.delayedScript;
				menu->window.delayTime = item.window.delayTime;
			}
		}
	}

	if (!(menu->window.flags & WINDOW_VISIBLE) && !forcePaint)
	{
		return;
	}

	//	if (menu->window.ownerDrawFlags && DC->ownerDrawVisible && !DC->ownerDrawVisible(menu->window.ownerDrawFlags))
	//	{
	//		return;
	//	}

	if (forcePaint)
	{
		menu->window.flags |= WINDOW_FORCED;
	}

	// draw the background if necessary
	if (menu->fullScreen)
	{
		vec4_t color{};
		color[0] = menu->window.backColor[0];
		color[1] = menu->window.backColor[1];
		color[2] = menu->window.backColor[2];
		color[3] = menu->window.backColor[3];

		ui.R_SetColor(color);

		if (menu->window.background == 0) // No background shader given? Make it blank
		{
			menu->window.background = uis.whiteShader;
		}

		DC->drawHandlePic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, menu->window.background);
	}
	else if (menu->window.background)
	{
		// this allows a background shader without being full screen
		//UI_DrawHandlePic(menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, menu->backgroundShader);
	}

	// paint the background and or border
	Window_Paint(&menu->window, menu->fadeAmount, menu->fadeClamp, menu->fadeCycle);

	// Loop through all items for the menu and paint them
	int iSlotsVisible = 0;
	for (i = 0; i < menu->itemCount; i++)
	{
		if (!menu->items[i]->appearanceSlot)
		{
			Item_Paint(menu->items[i], qtrue);
		}
		else // Timed order of appearance
		{
			if (Item_Paint(menu->items[i], qfalse))
			{
				iSlotsVisible++; //It would paint
			}
			if (menu->items[i]->appearanceSlot <= menu->appearanceCnt)
			{
				Item_Paint(menu->items[i], qtrue);
			}
		}
	}
	if (iSlotsVisible && menu->appearanceTime < DC->realTime && menu->appearanceCnt < menu->itemCount)
		// Time to show another item
	{
		menu->appearanceTime = DC->realTime + menu->appearanceIncrement;
		menu->appearanceCnt++;
	}

	if (uis.debugMode)
	{
		vec4_t color{};
		color[0] = color[2] = color[3] = 1;
		color[1] = 0;
		DC->drawRect(menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, 1, color);
	}
}

/*
=================
Item_EnableShowViaCvar
=================
*/
static qboolean Item_EnableShowViaCvar(const itemDef_t* item, const int flag)
{
	if (item && item->enableCvar && *item->enableCvar && item->cvarTest && *item->cvarTest)
	{
		char script[1024];
		const char* p;
		char buff[1024];
		if (item->cvarFlags & CVAR_SUBSTRING)
		{
			const char* val;
			p = item->enableCvar;
			COM_BeginParseSession();
			if (!String_Parse(&p, &val))
			{
				//strip the quotes off
				COM_EndParseSession();
				return item->cvarFlags & flag ? qfalse : qtrue;
			}

			COM_EndParseSession();
			Q_strncpyz(buff, val, sizeof buff);
			DC->getCVarString(item->cvarTest, script, sizeof script);
			p = script;
		}
		else
		{
			DC->getCVarString(item->cvarTest, buff, sizeof buff);
			Q_strncpyz(script, item->enableCvar, sizeof script);
			p = script;
		}
		COM_BeginParseSession();
		while (true)
		{
			const char* val;
			// expect value then ; or NULL, NULL ends list
			if (!String_Parse(&p, &val))
			{
				COM_EndParseSession();
				return item->cvarFlags & flag ? qfalse : qtrue;
			}

			if (val[0] == ';' && val[1] == '\0')
			{
				continue;
			}

			// enable it if any of the values are true
			if (item->cvarFlags & flag)
			{
				if (Q_stricmp(buff, val) == 0)
				{
					COM_EndParseSession();
					return qtrue;
				}
			}
			else
			{
				// disable it if any of the values are true
				if (Q_stricmp(buff, val) == 0)
				{
					COM_EndParseSession();
					return qfalse;
				}
			}
		}
	}
	return qtrue;
}

static bool HasStringLanguageChanged(const itemDef_t* item)
{
	if (!item->text || item->text[0] == '\0')
	{
		return false;
	}

	int modificationCount;
#ifdef JK2_MODE
	modificationCount = sp_language->modificationCount;
#else
	modificationCount = se_language->modificationCount;
#endif

	return item->asset != modificationCount;
}

/*
=================
Item_SetTextExtents
=================
*/
static void Item_SetTextExtents(itemDef_t* item, int* width, int* height, const char* text)
{
	const char* textPtr = text ? text : item->text;

	if (textPtr == nullptr)
	{
		return;
	}

	*width = item->textRect.w;
	*height = item->textRect.h;

	// keeps us from computing the widths and heights more than once
	if (*width == 0 ||
		item->type == ITEM_TYPE_OWNERDRAW && item->textalignment == ITEM_ALIGN_CENTER ||
		HasStringLanguageChanged(item))
	{
		int originalWidth = DC->textWidth(textPtr, item->textscale, item->font);

		if (item->type == ITEM_TYPE_OWNERDRAW && (item->textalignment == ITEM_ALIGN_CENTER || item->textalignment ==
			ITEM_ALIGN_RIGHT))
		{
			originalWidth += DC->ownerDrawWidth(item->window.ownerDraw, item->textscale);
		}
		else if (item->type == ITEM_TYPE_EDITFIELD && item->textalignment == ITEM_ALIGN_CENTER && item->cvar)
		{
			char buff[256];
			DC->getCVarString(item->cvar, buff, 256);
			originalWidth += DC->textWidth(buff, item->textscale, item->font);
		}

		*width = DC->textWidth(textPtr, item->textscale, item->font);
		*height = DC->textHeight(textPtr, item->textscale, item->font);

		item->textRect.w = *width;
		item->textRect.h = *height;
		item->textRect.x = item->textalignx;
		item->textRect.y = item->textaligny;
		if (item->textalignment == ITEM_ALIGN_RIGHT)
		{
			item->textRect.x = item->textalignx - originalWidth;
		}
		else if (item->textalignment == ITEM_ALIGN_CENTER)
		{
			item->textRect.x = item->textalignx - originalWidth / 2;
		}

		ToWindowCoords(&item->textRect.x, &item->textRect.y, &item->window);
#ifdef JK2_MODE
		if (item->text && item->text[0] == '@')
			item->asset = sp_language->modificationCount;
#else
		if (item->text && item->text[0] == '@') //string package
			item->asset = se_language->modificationCount; //mark language
#endif
	}
}

/*
=================
Item_TextColor
=================
*/
static void Item_TextColor(itemDef_t* item, vec4_t* newColor)
{
	constexpr vec4_t greyColor = { .5, .5, .5, 1 };
	const auto parent = static_cast<menuDef_t*>(item->parent);

	Fade(&item->window.flags, &item->window.foreColor[3], parent->fadeClamp, &item->window.nextTime, parent->fadeCycle,
		qtrue, parent->fadeAmount);

	if (!(item->type == ITEM_TYPE_TEXT && item->window.flags & WINDOW_AUTOWRAPPED) && item->window.flags &
		WINDOW_HASFOCUS)
	{
		vec4_t lowLight{};
		lowLight[0] = 0.8 * parent->focusColor[0];
		lowLight[1] = 0.8 * parent->focusColor[1];
		lowLight[2] = 0.8 * parent->focusColor[2];
		lowLight[3] = 0.8 * parent->focusColor[3];
		LerpColor(parent->focusColor, lowLight, *newColor,
			0.5 + 0.5 * sin(static_cast<float>(DC->realTime / static_cast<float>(PULSE_DIVISOR))));
	}
	else
	{
		memcpy(newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	if (item->disabled)
	{
		memcpy(newColor, &parent->disableColor, sizeof(vec4_t));
	}

	// items can be enabled and disabled based on cvars
	if (item->enableCvar && *item->enableCvar && item->cvarTest && *item->cvarTest)
	{
		if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE))
		{
			memcpy(newColor, &parent->disableColor, sizeof(vec4_t));
		}
	}

	if (item->window.flags & WINDOW_INACTIVE)
	{
		memcpy(newColor, &greyColor, sizeof(vec4_t));
	}
}

/*
=================
Item_Text_Wrapped_Paint
=================
*/
static void Item_Text_Wrapped_Paint(itemDef_t* item)
{
	const char* textPtr;
	char buff[1024];
	int width, height;
	vec4_t color;

	// now paint the text and/or any optional images
	// default to left

	if (item->text == nullptr)
	{
		char text[1024];
		if (item->cvar == nullptr)
		{
			return;
		}
		DC->getCVarString(item->cvar, text, sizeof text);
		textPtr = text;
	}
	else
	{
		textPtr = item->text;
	}
	if (*textPtr == '@') // string reference
	{
		textPtr = SE_GetString(&textPtr[1]);
	}

	if (*textPtr == '\0')
	{
		return;
	}

	Item_TextColor(item, &color);
	Item_SetTextExtents(item, &width, &height, textPtr);

	const float x = item->textRect.x;
	float y = item->textRect.y;
	const char* start = textPtr;
	const char* p = strchr(textPtr, '\r');
	while (p && *p)
	{
		strncpy(buff, start, p - start + 1);
		buff[p - start] = '\0';
		DC->drawText(x, y, item->textscale, color, buff, 0, item->textStyle, item->font);
		y += height + 5;
		start += p - start + 1;
		p = strchr(p + 1, '\r');
	}
	DC->drawText(x, y, item->textscale, color, start, 0, item->textStyle, item->font);
}

/*
=================
Menu_Paint
=================
*/
static void Item_Text_Paint(itemDef_t* item)
{
	const char* textPtr;
	int height, width;
	vec4_t color;

	if (item->window.flags & WINDOW_WRAPPED)
	{
		Item_Text_Wrapped_Paint(item);
		return;
	}

	if (item->window.flags & WINDOW_AUTOWRAPPED)
	{
		Item_Text_AutoWrapped_Paint(item);
		return;
	}

	if (item->text == nullptr)
	{
		char text[1024];
		if (item->cvar == nullptr)
		{
			return;
		}
		DC->getCVarString(item->cvar, text, sizeof text);
		textPtr = text;
	}
	else
	{
		textPtr = item->text;
	}
#ifdef JK2_MODE
	if (*textPtr == '@')
	{
		textPtr = JK2SP_GetStringTextString(&textPtr[1]);
	}
#else
	if (*textPtr == '@') // string reference
	{
		textPtr = SE_GetString(&textPtr[1]);
	}
#endif

	// this needs to go here as it sets extents for cvar types as well
	Item_SetTextExtents(item, &width, &height, textPtr);

	if (*textPtr == '\0')
	{
		return;
	}

	Item_TextColor(item, &color);
	DC->drawText(item->textRect.x, item->textRect.y, item->textscale, color, textPtr, 0, item->textStyle, item->font);

	if (item->text2) // Is there a second line of text?
	{
		textPtr = item->text2;
		if (*textPtr == '@') // string reference
		{
			textPtr = SE_GetString(&textPtr[1]);
		}

		Item_TextColor(item, &color);
		DC->drawText(item->textRect.x + item->text2alignx, item->textRect.y + item->text2aligny, item->textscale, color,
			textPtr, 0, item->textStyle, item->font);
	}
}

/*
=================
Item_UpdatePosition
=================
*/
// FIXME: consolidate this with nearby stuff
void Item_UpdatePosition(itemDef_t* item)
{
	if (item == nullptr || item->parent == nullptr)
	{
		return;
	}

	const menuDef_t* menu = static_cast<menuDef_t*>(item->parent);

	float x = menu->window.rect.x;
	float y = menu->window.rect.y;

	if (menu->window.border != 0)
	{
		x += menu->window.borderSize;
		y += menu->window.borderSize;
	}

	Item_SetScreenCoords(item, x, y);
}

/*
=================
Item_TextField_Paint
=================
*/
static void Item_TextField_Paint(itemDef_t* item)
{
	char buff[1024]{};
	vec4_t newColor;
	const auto parent = static_cast<menuDef_t*>(item->parent);
	const editFieldDef_t* editPtr = static_cast<editFieldDef_t*>(item->typeData);

	Item_Text_Paint(item);

	buff[0] = '\0';

	if (item->cvar)
	{
		DC->getCVarString(item->cvar, buff, sizeof buff);
	}

	if (item->window.flags & WINDOW_HASFOCUS)
	{
		vec4_t lowLight{};
		lowLight[0] = 0.8 * parent->focusColor[0];
		lowLight[1] = 0.8 * parent->focusColor[1];
		lowLight[2] = 0.8 * parent->focusColor[2];
		lowLight[3] = 0.8 * parent->focusColor[3];
		LerpColor(parent->focusColor, lowLight, newColor,
			0.5 + 0.5 * sin(static_cast<float>(DC->realTime / static_cast<float>(PULSE_DIVISOR))));
	}
	else
	{
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	constexpr int offset = 8; //(item->text && *item->text) ? 8 : 0;
	if (item->window.flags & WINDOW_HASFOCUS && g_editingField)
	{
		const char cursor = DC->getOverstrikeMode() ? '_' : '|';
		DC->drawTextWithCursor(item->textRect.x + item->textRect.w + offset, item->textRect.y, item->textscale,
			newColor, buff + editPtr->paintOffset, item->cursorPos - editPtr->paintOffset, cursor,
			/*editPtr->maxPaintChars*/ item->window.rect.w, item->textStyle, item->font);
	}
	else
	{
		DC->drawText(item->textRect.x + item->textRect.w + offset, item->textRect.y, item->textscale, newColor,
			buff + editPtr->paintOffset, /*editPtr->maxPaintChars*/ item->window.rect.w, item->textStyle,
			item->font);
	}
}

static void Item_TextScroll_Paint(itemDef_t* item)
{
	float x, y;
	const auto scrollPtr = static_cast<textScrollDef_t*>(item->typeData);

	float size = item->window.rect.h - 2;

	// Re-arranged this function. Previous version had a plethora of bugs.
	// Still a little iffy - BTO (VV)
	if (item->cvar)
	{
		char cvartext[1024];
		DC->getCVarString(item->cvar, cvartext, sizeof cvartext);
		item->text = cvartext;
	}

	Item_TextScroll_BuildLines(item);
	const float count = scrollPtr->iLineCount;

	// Draw scroll bar if text goes beyond bottom
	if (
#ifdef NEW_FEEDER_V2
		Q_stricmp(item->window.name, "char_desc") && // Dont show this...
#endif
		scrollPtr->iLineCount * scrollPtr->lineHeight > size)
	{
		// draw scrollbar to right side of the window
		x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE - 1;
		y = item->window.rect.y + 1;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowUp);
		y += SCROLLBAR_SIZE - 1;

		scrollPtr->endPos = scrollPtr->startPos;
		size = item->window.rect.h - SCROLLBAR_SIZE * 2;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, size + 1, DC->Assets.scrollBar);
		y += size - 1;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowDown);

		// thumb
		float thumb = Item_TextScroll_ThumbDrawPosition(item);
		if (thumb > y - SCROLLBAR_SIZE - 1)
		{
			thumb = y - SCROLLBAR_SIZE - 1;
		}
		DC->drawHandlePic(x, thumb, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);
	}

	// adjust size for item painting
	size = item->window.rect.h - 2;
	x = item->window.rect.x + item->textalignx + 1;
	y = item->window.rect.y + item->textaligny + 1;

	for (int i = scrollPtr->startPos; i < count; i++)
	{
		const char* text = scrollPtr->pLines[i];
		if (!text)
		{
			continue;
		}

		DC->drawText(x + 4, y, item->textscale, item->window.foreColor, text, 0, item->textStyle, item->font);

		size -= scrollPtr->lineHeight;
		if (size < scrollPtr->lineHeight)
		{
			scrollPtr->drawPadding = scrollPtr->lineHeight - size;
			break;
		}

		scrollPtr->endPos++;
		y += scrollPtr->lineHeight;
	}
}

/*
=================
Item_ListBox_Paint
=================
*/
#ifdef NEW_FEEDER
qhandle_t	mdBorder;
qhandle_t	mdBorderSel;
qhandle_t	mdBackground;
#endif
static void Item_ListBox_Paint(itemDef_t* item)
{
#ifdef NEW_FEEDER_V5
	if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS)) 
	{
	float x, y;
	int count = DC->feederCount(item->special);
	auto* listPtr = static_cast<listBoxDef_t*>(item->typeData);

	// Clamp startPos and cursorPos
	if (listPtr->startPos > (count > 0 ? count - 1 : 0))
		listPtr->startPos = 0;
	if (item->cursorPos > (count > 0 ? count - 1 : 0)) {
		item->cursorPos = (count > 0 ? count - 1 : 0);
		DC->feederSelection(item->special, item->cursorPos, nullptr);
	}

	// Draw scrollbar if needed
	if (!listPtr->scrollhidden && Item_ListBox_MaxScroll(item) > 0) {
		if (item->window.flags & WINDOW_HORIZONTAL) {
			x = item->window.rect.x + 1;
			y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE - 1;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowLeft);
			x += SCROLLBAR_SIZE - 1;
			float sizeWidth = item->window.rect.w - (SCROLLBAR_SIZE * 2);
			DC->drawHandlePic(x, y, sizeWidth + 1, SCROLLBAR_SIZE, DC->Assets.scrollBar);
			x += sizeWidth - 1;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowRight);
			float thumb = Item_ListBox_ThumbDrawPosition(item);
			if (thumb > x - SCROLLBAR_SIZE - 1)
				thumb = x - SCROLLBAR_SIZE - 1;
			DC->drawHandlePic(thumb, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);
		} else {
			x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE - 1;
			y = item->window.rect.y + 1;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowUp);
			y += SCROLLBAR_SIZE - 1;
			float sizeHeight = item->window.rect.h - (SCROLLBAR_SIZE * 2);
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, sizeHeight + 1, DC->Assets.scrollBar);
			y += sizeHeight - 1;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowDown);
			float thumb = Item_ListBox_ThumbDrawPosition(item);
			if (thumb > y - SCROLLBAR_SIZE - 1)
				thumb = y - SCROLLBAR_SIZE - 1;
			DC->drawHandlePic(x, thumb, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);
		}
	}

	// Calculate available area for items
	float availW = item->window.rect.w - 2;
	float availH = item->window.rect.h - 2;
	if (!(item->window.flags & WINDOW_HORIZONTAL))
		availW -= SCROLLBAR_SIZE;
	else
		availH -= SCROLLBAR_SIZE;

	// Determine how many columns and rows fit
	int cols = 1, rows = 1;
	if (listPtr->elementStyle == LISTBOX_IMAGE) {
		cols = std::max(1, static_cast<int>(availW / listPtr->elementWidth));
		rows = std::max(1, static_cast<int>(availH / listPtr->elementHeight));
	}

	// Paint items
	int drawn = 0;
	listPtr->endPos = listPtr->startPos;
	x = item->window.rect.x + 1;
	y = item->window.rect.y + 1;

	for (int idx = listPtr->startPos; idx < count; ) {
		float curX = x, curY = y;
		for (int r = 0; r < rows && idx < count; ++r) {
			curX = x;
			for (int c = 0; c < cols && idx < count; ++c, ++idx) {
				qhandle_t image = DC->feederItemImage(item->special, idx);
#ifdef NEW_FEEDER
#ifdef NEW_FEEDER_V4
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS)) {
					DC->setColor(colorTable[CT_WHITE]);
					DC->drawHandlePic(curX + 1, curY + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, mdBackground);
				}
#else
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS))
					DC->drawHandlePic(curX + 1, curY + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, mdBackground);
#endif
#endif		
				if (image) {
					if (item->window.flags & WINDOW_PLAYERCOLOR) {
						vec4_t color;
						color[0] = ui_char_color_red.integer / 255.0f;
						color[1] = ui_char_color_green.integer / 255.0f;
						color[2] = ui_char_color_blue.integer / 255.0f;
						color[3] = 1.0f;
						DC->setColor(color);
					}
					DC->drawHandlePic(curX + 1, curY + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image);
				}
#ifdef NEW_FEEDER
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS))
					DC->drawHandlePic(curX + 1, curY + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, (idx == item->cursorPos) ? mdBorderSel : mdBorder);
#endif		
				if (idx == item->cursorPos) {
					DC->drawRect(curX, curY, listPtr->elementWidth - 1, listPtr->elementHeight - 1, item->window.borderSize, item->window.borderColor);
				}
				curX += listPtr->elementWidth;
				listPtr->endPos++;
				drawn++;
			}
			curY += listPtr->elementHeight;
		}
		break; // Only one page of items is drawn
	}

	// Calculate drawPadding for the last row/column
	if (listPtr->elementStyle == LISTBOX_IMAGE) {
		if (item->window.flags & WINDOW_HORIZONTAL) {
			int itemsInRow = std::min(cols, count - listPtr->startPos);
			listPtr->drawPadding = availW - (itemsInRow * listPtr->elementWidth);
		} else {
			int itemsInCol = std::min(rows, count - listPtr->startPos);
			listPtr->drawPadding = availH - (itemsInCol * listPtr->elementHeight);
		}
	}
	}
	else
	{
#endif
	float x, y, size;
	int i, thumb;
	qhandle_t image;
	qhandle_t optionalImage;
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);

	// the listbox is horizontal or vertical and has a fixed size scroll bar going either direction
	// elements are enumerated from the DC and either text or image handles are acquired from the DC as well
	// textscale is used to size the text, textalignx and textaligny are used to size image elements
	// there is no clipping available so only the last completely visible item is painted
	const int count = DC->feederCount(item->special);

	if (listPtr->startPos > (count ? count - 1 : count))
	{
		//probably changed feeders, so reset
		listPtr->startPos = 0;
	}

	if (item->cursorPos > (count ? count - 1 : count))
	{
		//probably changed feeders, so reset
		item->cursorPos = 0;
	}
	// default is vertical if horizontal flag is not here
	if (item->window.flags & WINDOW_HORIZONTAL)
	{
		//JLF new variable (code just indented)
		if (!listPtr->scrollhidden)
		{
			// draw scrollbar in bottom of the window
			// bar
			if (Item_ListBox_MaxScroll(item) > 0)
			{
				x = item->window.rect.x + 1;
				y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE - 1;
				DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowLeft);
				x += SCROLLBAR_SIZE - 1;
				size = item->window.rect.w - SCROLLBAR_SIZE * 2;
				DC->drawHandlePic(x, y, size + 1, SCROLLBAR_SIZE, DC->Assets.scrollBar);
				x += size - 1;
				DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowRight);
				// thumb
				thumb = Item_ListBox_ThumbDrawPosition(item); //Item_ListBox_ThumbPosition(item);
				if (thumb > x - SCROLLBAR_SIZE - 1)
				{
					thumb = x - SCROLLBAR_SIZE - 1;
				}
				DC->drawHandlePic(thumb, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);
			}
			else if (listPtr->startPos > 0)
			{
				listPtr->startPos = 0;
			}
		}
		//JLF end
		//
		listPtr->endPos = listPtr->startPos;
		size = item->window.rect.w - 2;
		// items
		// size contains max available space
		if (listPtr->elementStyle == LISTBOX_IMAGE)
		{
			// fit = 0;
			x = item->window.rect.x + 1;
			y = item->window.rect.y + 1;
			for (i = listPtr->startPos; i < count; i++)
			{
				// always draw at least one
				// which may overdraw the box if it is too small for the element
				image = DC->feederItemImage(item->special, i);
#ifdef NEW_FEEDER
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS)) {
					DC->drawHandlePic(x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, mdBackground);
				}
#endif		
				if (image)
				{
					if (item->window.flags & WINDOW_PLAYERCOLOR)
					{
						vec4_t color{};
						color[0] = ui_char_color_red.integer / 255.0f;
						color[1] = ui_char_color_green.integer / 255.0f;
						color[2] = ui_char_color_blue.integer / 255.0f;
						color[3] = 1;
						ui.R_SetColor(color);
					}
					DC->drawHandlePic(x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image);
				}

#ifdef NEW_FEEDER
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS)) {
					DC->drawHandlePic(x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, (i == item->cursorPos) ? mdBorderSel : mdBorder);
				}
#endif		

				if (i == item->cursorPos)
				{
					DC->drawRect(x, y, listPtr->elementWidth - 1, listPtr->elementHeight - 1, item->window.borderSize,
						item->window.borderColor);
				}

				size -= listPtr->elementWidth;
				if (size < listPtr->elementWidth)
				{
					listPtr->drawPadding = size; //listPtr->elementWidth - size;
					break;
				}
				x += listPtr->elementWidth;
				listPtr->endPos++;
				// fit++;
			}
		}
		else
		{
			//
		}
	}
	else
	{
		//JLF new variable (code idented with if)
		if (!listPtr->scrollhidden)
		{
			// draw scrollbar to right side of the window
			x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE - 1;
			y = item->window.rect.y + 1;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowUp);
			y += SCROLLBAR_SIZE - 1;

			listPtr->endPos = listPtr->startPos;
			size = item->window.rect.h - SCROLLBAR_SIZE * 2;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, size + 1, DC->Assets.scrollBar);
			y += size - 1;
			DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowDown);
			// thumb
			thumb = Item_ListBox_ThumbDrawPosition(item); //Item_ListBox_ThumbPosition(item);
			if (thumb > y - SCROLLBAR_SIZE - 1)
			{
				thumb = y - SCROLLBAR_SIZE - 1;
			}
			DC->drawHandlePic(x, thumb, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);
		}
		//JLF end
		// adjust size for item painting
		size = item->window.rect.h - 2;
		if (listPtr->elementStyle == LISTBOX_IMAGE)
		{
			// fit = 0;
			x = item->window.rect.x + 1;
			y = item->window.rect.y + 1;

			for (i = listPtr->startPos; i < count; i++)
			{
				// always draw at least one
				// which may overdraw the box if it is too small for the element
				image = DC->feederItemImage(item->special, i);

#ifdef NEW_FEEDER
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS)) {
					DC->drawHandlePic(x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, mdBackground);
				}
#endif		
				if (image)
				{
					DC->drawHandlePic(x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image);
				}

#ifdef NEW_FEEDER
				if ((item->special == FEEDER_MD_MODELS) || (item->special == FEEDER_MD_VARIANTS)) {
					DC->drawHandlePic(x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, (i == item->cursorPos) ? mdBorderSel : mdBorder);
				}
#endif		

				if (i == item->cursorPos)
				{
					DC->drawRect(x, y, listPtr->elementWidth - 1, listPtr->elementHeight - 1, item->window.borderSize,
						item->window.borderColor);
				}

				listPtr->endPos++;
				size -= listPtr->elementHeight;
				if (size < listPtr->elementHeight)
				{
					listPtr->drawPadding = listPtr->elementHeight - size;
					break;
				}
				y += listPtr->elementHeight;
				// fit++;
			}
		}
		else
		{
			x = item->window.rect.x + 1;
			y = item->window.rect.y + 1 - listPtr->elementHeight;
			i = listPtr->startPos;

			for (; i < count; i++)
			{
				const char* text;
				// always draw at least one
				// which may overdraw the box if it is too small for the element

				if (listPtr->numColumns > 0)
				{
					for (int j = 0; j < listPtr->numColumns; j++)
					{
						text = DC->feederItemText(item->special, i, j, &optionalImage);
						const bool textname = text[0] == '@';
						if (textname)
						{
							text = SE_GetString(&text[1]);
						}

						if (optionalImage >= 0)
						{
							DC->drawHandlePic(x + 4 + listPtr->columnInfo[j].pos, y - 1 + listPtr->elementHeight / 2,
								listPtr->columnInfo[j].width, listPtr->columnInfo[j].width,
								optionalImage);
						}
						else if (text)
						{
							vec4_t* color;
							const auto parent = static_cast<menuDef_t*>(item->parent);

							// Use focus color is it has focus.
							if (i == item->cursorPos)
							{
								color = &parent->focusColor;
							}
							else
							{
								color = &item->window.foreColor;
							}

							constexpr int textyOffset = 0;

							DC->drawText(x + 4 + listPtr->columnInfo[j].pos, y + listPtr->elementHeight + textyOffset,
								item->textscale, *color, text, listPtr->columnInfo[j].maxChars,
								item->textStyle, item->font);
						}
					}
				}
				else
				{
					text = DC->feederItemText(item->special, i, 0, &optionalImage);
					if (optionalImage >= 0)
					{
						//DC->drawHandlePic(x + 4 + listPtr->elementHeight, y, listPtr->columnInfo[j].width, listPtr->columnInfo[j].width, optionalImage);
					}
					else if (text)
					{
						DC->drawText(x + 4, y + listPtr->elementHeight, item->textscale, item->window.foreColor, text,
							0, item->textStyle, item->font);
					}
				}

				// The chosen text
				if (i == item->cursorPos)
				{
					DC->fillRect(x + 2, y + listPtr->elementHeight + 2, item->window.rect.w - SCROLLBAR_SIZE - 4,
						listPtr->elementHeight + 2, item->window.outlineColor);
				}

				size -= listPtr->elementHeight;
				if (size < listPtr->elementHeight)
				{
					listPtr->drawPadding = listPtr->elementHeight - size;
					break;
				}
				listPtr->endPos++;
				y += listPtr->elementHeight;
				// fit++;
			}
		}
	}
#ifdef NEW_FEEDER_V5
	}
#endif
}

/*
=================
BindingIDFromName
=================
*/
static int BindingIDFromName(const char* name)
{
	// iterate each command, set its default binding
	for (size_t i = 0; i < g_bindCount; i++)
	{
		if (!Q_stricmp(name, g_bindCommands[i]))
			return i;
	}
	return -1;
}

char g_nameBind[96];

/*
=================
BindingFromName
=================
*/
static void BindingFromName(const char* cvar)
{
	// iterate each command, set its default binding
	for (size_t i = 0; i < g_bindCount; i++)
	{
		if (!Q_stricmp(cvar, g_bindCommands[i]))
		{
			const int b2 = g_bindKeys[i][1];
			const int b1 = g_bindKeys[i][0];
			if (b1 == -1)
				break;

			if (b2 != -1)
			{
				char sOR[32];
				char keyname[2][32];

				DC->keynumToStringBuf(b1, keyname[0], sizeof keyname[0]);
				// do NOT do this or it corrupts asian text!!!					Q_strupr(keyname[0]);
				DC->keynumToStringBuf(b2, keyname[1], sizeof keyname[1]);
				// do NOT do this or it corrupts asian text!!!					Q_strupr(keyname[1]);

#ifdef JK2_MODE
				Q_strncpyz(sOR, ui.SP_GetStringTextString("MENUS3_KEYBIND_OR"), sizeof(sOR));
#else
				Q_strncpyz(sOR, SE_GetString("MENUS_KEYBIND_OR"), sizeof sOR);
#endif

				Com_sprintf(g_nameBind, sizeof g_nameBind, "%s %s %s", keyname[0], sOR, keyname[1]);
			}
			else
			{
				DC->keynumToStringBuf(b1, g_nameBind, sizeof g_nameBind);
				// do NOT do this or it corrupts asian text!!!					Q_strupr(g_nameBind);
			}
			return;
		}
	}
	Q_strncpyz(g_nameBind, "???", sizeof g_nameBind);
}

/*
=================
Item_Bind_Paint
=================
*/
static void Item_Bind_Paint(itemDef_t* item)
{
	vec4_t newColor;
	int maxChars = 0;

	const auto parent = static_cast<menuDef_t*>(item->parent);
	const editFieldDef_t* editPtr = static_cast<editFieldDef_t*>(item->typeData);

	if (editPtr)
	{
		maxChars = editPtr->maxPaintChars;
	}

	const float value = item->cvar ? DC->getCVarValue(item->cvar) : 0;

	if (item->window.flags & WINDOW_HASFOCUS)
	{
		vec4_t lowLight{};
		if (g_bindItem == item)
		{
			lowLight[0] = 0.8f * 1.0f;
			lowLight[1] = 0.8f * 0.0f;
			lowLight[2] = 0.8f * 0.0f;
			lowLight[3] = 0.8f * 1.0f;
		}
		else
		{
			lowLight[0] = 0.8f * parent->focusColor[0];
			lowLight[1] = 0.8f * parent->focusColor[1];
			lowLight[2] = 0.8f * parent->focusColor[2];
			lowLight[3] = 0.8f * parent->focusColor[3];
		}
		LerpColor(parent->focusColor, lowLight, newColor,
			0.5 + 0.5 * sin(static_cast<float>(DC->realTime / static_cast<float>(PULSE_DIVISOR))));
	}
	else
	{
		Item_TextColor(item, &newColor);
	}

	if (item->text)
	{
		Item_Text_Paint(item);
		BindingFromName(item->cvar);

		// If the text runs past the limit bring the scale down until it fits.
		float textScale = item->textscale;
		float textWidth = DC->textWidth(g_nameBind, textScale, uiInfo.uiDC.Assets.qhMediumFont);

		const int startingXPos = item->textRect.x + item->textRect.w + 8;

		while (startingXPos + textWidth >= SCREEN_WIDTH)
		{
			textScale -= .05f;
			textWidth = DC->textWidth(g_nameBind, textScale, uiInfo.uiDC.Assets.qhMediumFont);
		}

		// Try to adjust it's y placement if the scale has changed.
		int yAdj = 0;
		if (textScale != item->textscale)
		{
			const int textHeight = DC->textHeight(g_nameBind, item->textscale, uiInfo.uiDC.Assets.qhMediumFont);
			yAdj = textHeight - DC->textHeight(g_nameBind, textScale, uiInfo.uiDC.Assets.qhMediumFont);
		}

		DC->drawText(startingXPos, item->textRect.y + yAdj, textScale, newColor, g_nameBind,
			maxChars/*item->textRect.w*/, item->textStyle, item->font);
	}
	else
	{
		DC->drawText(item->textRect.x, item->textRect.y, item->textscale, newColor, value != 0 ? "FIXME 1" : "FIXME 0",
			maxChars/*item->textRect.w*/, item->textStyle, item->font);
	}
}

static void UI_ScaleModelAxis(refEntity_t* ent)

{
	// scale the model should we need to
	if (ent->modelScale[0] && ent->modelScale[0] != 1.0f)
	{
		VectorScale(ent->axis[0], ent->modelScale[0], ent->axis[0]);
		ent->nonNormalizedAxes = qtrue;
	}
	if (ent->modelScale[1] && ent->modelScale[1] != 1.0f)
	{
		VectorScale(ent->axis[1], ent->modelScale[1], ent->axis[1]);
		ent->nonNormalizedAxes = qtrue;
	}
	if (ent->modelScale[2] && ent->modelScale[2] != 1.0f)
	{
		VectorScale(ent->axis[2], ent->modelScale[2], ent->axis[2]);
		ent->nonNormalizedAxes = qtrue;
	}
}

/*
=================
Item_Model_Paint
=================
*/

extern int s_entityWavVol[MAX_GENTITIES]; //from snd_dma.cpp
static void UI_TalkingHead(itemDef_t* item)
{
	//	static int facial_blink = DC->realTime + Q_flrand(4000.0, 8000.0);
	static int facial_timer = DC->realTime + Q_flrand(10000.0, 30000.0);
	//	static animNumber_t facial_anim = FACE_ALERT;
	int anim = -1;

	if (s_entityWavVol[0] > 0) // if we aren't talking, then it will be 0, -1 for talking but paused
	{
		anim = FACE_TALK1 + s_entityWavVol[0] - 1;
		if (anim > FACE_TALK4)
		{
			anim = FACE_TALK4;
		}
		// reset timers so we don't start right away after talking
		facial_timer = DC->realTime + Q_flrand(2000.0, 7000.0);
	}
	else if (s_entityWavVol[0] == -1)
	{
		// talking, but silent
		anim = FACE_TALK0;
		// reset timers so we don't start right away after talking
		facial_timer = DC->realTime + Q_flrand(2000.0, 7000.0);
	}
	if (facial_timer < DC->realTime)
	{
		//restart the base anim
	}
	if (anim != -1)
	{
		DC->g2hilev_SetAnim(&item->ghoul2[0], "face", anim, qfalse);
	}
}

/*
=================
Item_Model_Paint
=================
*/
extern void UI_SaberDrawBlades(itemDef_t* item, vec3_t origin, float cur_yaw);

static void Item_Model_Paint(itemDef_t* item)
{
	float x, y, w, h;
	refdef_t refdef;
	refEntity_t ent;
	vec3_t mins, maxs, origin{};
	vec3_t angles;
	const modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);

	if (modelPtr == nullptr)
	{
		return;
	}

	// Fuck all the logic
#ifdef JK2_MODE
	// setup the refdef
	memset(&refdef, 0, sizeof(refdef));
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear(refdef.viewaxis);
	x = item->window.rect.x + 1;
	y = item->window.rect.y + 1;
	w = item->window.rect.w - 2;
	h = item->window.rect.h - 2;

	refdef.x = x * DC->xscale;
	refdef.y = y * DC->yscale;
	refdef.width = w * DC->xscale;
	refdef.height = h * DC->yscale;

	DC->modelBounds(item->asset, mins, maxs);

	origin[2] = -0.5 * (mins[2] + maxs[2]);
	origin[1] = 0.5 * (mins[1] + maxs[1]);

	// calculate distance so the model nearly fills the box
	if (qtrue)
	{
		float len = 0.5 * (maxs[2] - mins[2]);
		origin[0] = len / 0.268;	// len / tan( fov/2 )
		//origin[0] = len / tan(w/2);
	}
	else
	{
		origin[0] = item->textscale;
	}

	refdef.fov_x = 45;
	refdef.fov_y = 45;

	DC->clearScene();

	refdef.time = DC->realTime;

	// add the model

	memset(&ent, 0, sizeof(ent));

	//adjust = 5.0 * sin( (float)uis.realtime / 500 );
	//adjust = 360 % (int)((float)uis.realtime / 1000);
	//VectorSet( angles, 0, 0, 1 );

	// use item storage to track
/*
	if (modelPtr->rotationSpeed)
	{
		if (DC->realTime > item->window.nextTime)
		{
			item->window.nextTime = DC->realTime + modelPtr->rotationSpeed;
			modelPtr->angle = (int)(modelPtr->angle + 1) % 360;
		}
	}
	VectorSet( angles, 0, modelPtr->angle, 0 );
*/
	VectorSet(angles, 0, (float)(refdef.time / 20.0f), 0);

	AnglesToAxis(angles, ent.axis);

	ent.hModel = item->asset;
	VectorCopy(origin, ent.origin);
	VectorCopy(ent.origin, ent.oldorigin);

	// Set up lighting
	VectorCopy(refdef.vieworg, ent.lightingOrigin);
	ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	DC->addRefEntityToScene(&ent);
	DC->renderScene(&refdef);
#else
	// a moves datapad anim is playing
	if (uiInfo.moveAnimTime && uiInfo.moveAnimTime < uiInfo.uiDC.realTime)
	{
		const modelDef_t* model_ptr = static_cast<modelDef_t*>(item->typeData);
		if (model_ptr)
		{
			//HACKHACKHACK: check for any multi-part anim sequences, and play the next anim, if needbe
			switch (model_ptr->g2anim)
			{
			case BOTH_FORCEWALLREBOUND_FORWARD:
			case BOTH_FORCEJUMP1:
				ItemParse_model_g2anim_go(item, anim_table[BOTH_FORCEINAIR1].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				if (!uiInfo.moveAnimTime)
				{
					uiInfo.moveAnimTime = 500;
				}
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			case BOTH_FORCEINAIR1:
				ItemParse_model_g2anim_go(item, anim_table[BOTH_FORCELAND1].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			case BOTH_FORCEWALLRUNFLIP_START:
				ItemParse_model_g2anim_go(item, anim_table[BOTH_FORCEWALLRUNFLIP_END].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			case BOTH_FORCELONGLEAP_START:
				ItemParse_model_g2anim_go(item, anim_table[BOTH_FORCELONGLEAP_LAND].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			case BOTH_KNOCKDOWN3: //on front - into force getup
				DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveJumpSound, CHAN_LOCAL);
				ItemParse_model_g2anim_go(item, anim_table[BOTH_FORCE_GETUP_F1].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			case BOTH_KNOCKDOWN2: //on back - kick forward getup
				DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveJumpSound, CHAN_LOCAL);
				ItemParse_model_g2anim_go(item, anim_table[BOTH_GETUP_BROLL_F].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			case BOTH_KNOCKDOWN1: //on back - roll-away
				DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveRollSound, CHAN_LOCAL);
				ItemParse_model_g2anim_go(item, anim_table[BOTH_GETUP_BROLL_R].name);
				uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				break;
			default:
				ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);
				DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", model_ptr->g2anim, qtrue);
				uiInfo.moveAnimTime = 0;
				break;
			}
		}
	}

	// setup the refdef
	memset(&refdef, 0, sizeof refdef);
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear(refdef.viewaxis);
	x = item->window.rect.x + 1;
	y = item->window.rect.y + 1;
	w = item->window.rect.w - 2;
	h = item->window.rect.h - 2;

	refdef.x = x * DC->xscale;
	refdef.y = y * DC->yscale;
	refdef.width = w * DC->xscale;
	refdef.height = h * DC->yscale;

	if (item->flags & ITF_G2VALID)
	{
		//ghoul2 models don't have bounds, so we have to parse them.
		VectorCopy(modelPtr->g2mins, mins);
		VectorCopy(modelPtr->g2maxs, maxs);

		if (!mins[0] && !mins[1] && !mins[2] &&
			!maxs[0] && !maxs[1] && !maxs[2])
		{
			//we'll use defaults then I suppose.
			VectorSet(mins, -16, -16, -24);
			VectorSet(maxs, 16, 16, 32);
		}
	}
	else
	{
		DC->modelBounds(item->asset, mins, maxs);
	}

	origin[2] = -0.5 * (mins[2] + maxs[2]);
	origin[1] = 0.5 * (mins[1] + maxs[1]);

	refdef.fov_x = modelPtr->fov_x
		? modelPtr->fov_x
		: static_cast<int>(static_cast<float>(refdef.width) / 640.0f * 90.0f);
	refdef.fov_y = modelPtr->fov_y
		? modelPtr->fov_y
		: atan2(refdef.height, refdef.width / tan(refdef.fov_x / 360 * M_PI)) * (360 / M_PI);

	//	refdef.fov_x = (modelPtr->fov_x) ? modelPtr->fov_x : refdef.width;
	//	refdef.fov_y = (modelPtr->fov_y) ? modelPtr->fov_y : refdef.height;

	// calculate distance so the model nearly fills the box
	float len = 0.5 * (maxs[2] - mins[2]);
	origin[0] = len / 0.268;

	DC->clearScene();

	refdef.time = DC->realTime;

	// add the model

	memset(&ent, 0, sizeof ent);

	// use item storage to track
	float curYaw = modelPtr->angle;
	if (item->cvar)
	{
		curYaw = DC->getCVarValue(item->cvar);
	}
	if (modelPtr->rotationSpeed)
	{
		curYaw += static_cast<float>(refdef.time) / modelPtr->rotationSpeed;
	}
	if (item->flags & ITF_ISANYSABER && !(item->flags & ITF_ISCHARACTER))
	{
		//hack to put saber on it's side
		VectorSet(angles, curYaw, 0, 90);
	}
	else
	{
		VectorSet(angles, 0, curYaw, 0);
	}

	AnglesToAxis(angles, ent.axis);

	if (item->flags & ITF_G2VALID)
	{
		ent.ghoul2 = &item->ghoul2;
		ent.radius = 1000;
		ent.customSkin = modelPtr->g2skin;

		if (item->flags & ITF_ISCHARACTER)
		{
			ent.shaderRGBA[0] = ui_char_color_red.integer;
			ent.shaderRGBA[1] = ui_char_color_green.integer;
			ent.shaderRGBA[2] = ui_char_color_blue.integer;
			ent.shaderRGBA[3] = 255;
			UI_TalkingHead(item);
		}
		if (item->flags & ITF_ISANYSABER)
		{
			//UGH, draw the saber blade!
			UI_SaberDrawBlades(item, origin, curYaw);
		}
	}
	else
	{
		ent.hModel = item->asset;
	}
	VectorCopy(origin, ent.origin);
	VectorCopy(ent.origin, ent.oldorigin);

	// Set up lighting
	//VectorCopy( refdef.vieworg, ent.lightingOrigin );
	ent.renderfx = RF_NOSHADOW;

	ui.R_AddLightToScene(refdef.vieworg, 500, 1, 1, 1); //fixme: specify in menu file!

	DC->addRefEntityToScene(&ent);
	DC->renderScene(&refdef);
#endif
}

/*
=================
Item_OwnerDraw_Paint
=================
*/
static void Item_OwnerDraw_Paint(itemDef_t* item)
{
	if (item == nullptr)
	{
		return;
	}

	const auto parent = static_cast<menuDef_t*>(item->parent);

	if (DC->ownerDrawItem)
	{
		vec4_t color, lowLight{};
		Fade(&item->window.flags, &item->window.foreColor[3], parent->fadeClamp, &item->window.nextTime,
			parent->fadeCycle, qtrue, parent->fadeAmount);
		memcpy(&color, &item->window.foreColor, sizeof color);
		if (item->numColors > 0 && DC->getValue)
		{
			const float f = DC->getValue(item->window.ownerDraw);
			for (int i = 0; i < item->numColors; i++)
			{
				if (f >= item->colorRanges[i].low && f <= item->colorRanges[i].high)
				{
					memcpy(&color, &item->colorRanges[i].color, sizeof color);
					break;
				}
			}
		}

		if (item->window.flags & WINDOW_HASFOCUS)
		{
			lowLight[0] = 0.8 * parent->focusColor[0];
			lowLight[1] = 0.8 * parent->focusColor[1];
			lowLight[2] = 0.8 * parent->focusColor[2];
			lowLight[3] = 0.8 * parent->focusColor[3];
			LerpColor(parent->focusColor, lowLight, color,
				0.5 + 0.5 * sin(static_cast<float>(DC->realTime / static_cast<float>(PULSE_DIVISOR))));
		}
		else if (item->textStyle == ITEM_TEXTSTYLE_BLINK && !(DC->realTime / BLINK_DIVISOR & 1))
		{
			lowLight[0] = 0.8 * item->window.foreColor[0];
			lowLight[1] = 0.8 * item->window.foreColor[1];
			lowLight[2] = 0.8 * item->window.foreColor[2];
			lowLight[3] = 0.8 * item->window.foreColor[3];
			LerpColor(item->window.foreColor, lowLight, color,
				0.5 + 0.5 * sin(static_cast<float>(DC->realTime / static_cast<float>(PULSE_DIVISOR))));
		}

		if (item->disabled)
			memcpy(color, parent->disableColor, sizeof(vec4_t));

		if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE))
		{
			memcpy(color, parent->disableColor, sizeof(vec4_t));
		}

		if (item->text)
		{
			Item_Text_Paint(item);

			// +8 is an offset kludge to properly align owner draw items that have text combined with them
			DC->ownerDrawItem(item->textRect.x + item->textRect.w + 8, item->window.rect.y, item->window.rect.w,
				item->window.rect.h, 0, item->textaligny, item->window.ownerDraw,
				item->window.ownerDrawFlags, item->alignment, item->special, item->textscale, color,
				item->window.background, item->textStyle, item->font);
		}
		else
		{
			DC->ownerDrawItem(item->window.rect.x, item->window.rect.y, item->window.rect.w, item->window.rect.h,
				item->textalignx, item->textaligny, item->window.ownerDraw, item->window.ownerDrawFlags,
				item->alignment, item->special, item->textscale, color, item->window.background,
				item->textStyle, item->font);
		}
	}
}

static void Item_YesNo_Paint(itemDef_t* item)
{
	vec4_t color;

	const float value = item->cvar ? DC->getCVarValue(item->cvar) : 0;

#ifdef JK2_MODE
	const char* psYes = ui.SP_GetStringTextString("MENUS0_YES");
	const char* psNo = ui.SP_GetStringTextString("MENUS0_NO");
#else
	const char* psYes = SE_GetString("MENUS_YES");
	const char* psNo = SE_GetString("MENUS_NO");
#endif
	const char* yesnovalue;

	if (item->invertYesNo)
		yesnovalue = value == 0 ? psYes : psNo;
	else
		yesnovalue = value != 0 ? psYes : psNo;

	Item_TextColor(item, &color);
	if (item->text)
	{
		Item_Text_Paint(item);
		DC->drawText(item->textRect.x + item->textRect.w + 8, item->textRect.y, item->textscale, color, yesnovalue, 0,
			item->textStyle, item->font);
	}
	else
	{
		DC->drawText(item->textRect.x, item->textRect.y, item->textscale, color, yesnovalue, 0, item->textStyle,
			item->font);
	}
}

/*
=================
Item_Multi_Paint
=================
*/
static void Item_Multi_Paint(itemDef_t* item)
{
	vec4_t color;

	const char* text = Item_Multi_Setting(item);
	if (*text == '@') // string reference
	{
		text = SE_GetString(&text[1]);
	}

	Item_TextColor(item, &color);
	if (item->text)
	{
		Item_Text_Paint(item);
		DC->drawText(item->textRect.x + item->textRect.w + 8, item->textRect.y, item->textscale, color, text, 0,
			item->textStyle, item->font);
	}
	else
	{
		//JLF added xoffset
		DC->drawText(item->textRect.x + item->xoffset, item->textRect.y, item->textscale, color, text, 0,
			item->textStyle, item->font);
	}
}

static int Item_TextScroll_MaxScroll(const itemDef_t* item)
{
	const textScrollDef_t* scrollPtr = static_cast<textScrollDef_t*>(item->typeData);

	const int count = scrollPtr->iLineCount;
	const int max = count - static_cast<int>(item->window.rect.h / scrollPtr->lineHeight) + 1;

	if (max < 0)
	{
		return 0;
	}

	return max;
}

static int Item_TextScroll_ThumbPosition(const itemDef_t* item)
{
	float pos;
	const textScrollDef_t* scrollPtr = static_cast<textScrollDef_t*>(item->typeData);

	const float max = Item_TextScroll_MaxScroll(item);
	const float size = item->window.rect.h - SCROLLBAR_SIZE * 2 - 2;

	if (max > 0)
	{
		pos = (size - SCROLLBAR_SIZE) / static_cast<float>(max);
	}
	else
	{
		pos = 0;
	}

	pos *= scrollPtr->startPos;

	return item->window.rect.y + 1 + SCROLLBAR_SIZE + pos;
}

int Item_TextScroll_ThumbDrawPosition(itemDef_t* item)
{
	if (itemCapture == item)
	{
		const int min = item->window.rect.y + SCROLLBAR_SIZE + 1;
		const int max = item->window.rect.y + item->window.rect.h - 2 * SCROLLBAR_SIZE - 1;

		if (DC->cursory >= min + SCROLLBAR_SIZE / 2 && DC->cursory <= max + SCROLLBAR_SIZE / 2)
		{
			return DC->cursory - SCROLLBAR_SIZE / 2;
		}

		return Item_TextScroll_ThumbPosition(item);
	}

	return Item_TextScroll_ThumbPosition(item);
}

static int Item_TextScroll_OverLB(itemDef_t* item, const float x, const float y)
{
	rectDef_t r{};

	const textScrollDef_t* scrollPtr = static_cast<textScrollDef_t*>(item->typeData);

	// Scroll bar isn't drawing so ignore this input
	if (scrollPtr->iLineCount * scrollPtr->lineHeight <= item->window.rect.h - 2 && item->type == ITEM_TYPE_TEXTSCROLL)
	{
		return 0;
	}

	r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
	r.y = item->window.rect.y;
	r.h = r.w = SCROLLBAR_SIZE;
	if (Rect_ContainsPoint(&r, x, y))
	{
		return WINDOW_LB_LEFTARROW;
	}

	r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
	if (Rect_ContainsPoint(&r, x, y))
	{
		return WINDOW_LB_RIGHTARROW;
	}

	const int thumbstart = Item_TextScroll_ThumbPosition(item);
	r.y = thumbstart;
	if (Rect_ContainsPoint(&r, x, y))
	{
		return WINDOW_LB_THUMB;
	}

	r.y = item->window.rect.y + SCROLLBAR_SIZE;
	r.h = thumbstart - r.y;
	if (Rect_ContainsPoint(&r, x, y))
	{
		return WINDOW_LB_PGUP;
	}

	r.y = thumbstart + SCROLLBAR_SIZE;
	r.h = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
	if (Rect_ContainsPoint(&r, x, y))
	{
		return WINDOW_LB_PGDN;
	}

	return 0;
}

static void Item_TextScroll_MouseEnter(itemDef_t* item, const float x, const float y)
{
	item->window.flags &= ~(WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP |
		WINDOW_LB_PGDN);
	item->window.flags |= Item_TextScroll_OverLB(item, x, y);
}

/*
=================
Item_Slider_ThumbPosition
=================
*/
int Item_ListBox_ThumbDrawPosition(itemDef_t* item)
{
	if (itemCapture == item)
	{
		int max;
		int min;
		if (item->window.flags & WINDOW_HORIZONTAL)
		{
			min = item->window.rect.x + SCROLLBAR_SIZE + 1;
			max = item->window.rect.x + item->window.rect.w - 2 * SCROLLBAR_SIZE - 1;
			if (DC->cursorx >= min + SCROLLBAR_SIZE / 2 && DC->cursorx <= max + SCROLLBAR_SIZE / 2)
			{
				return DC->cursorx - SCROLLBAR_SIZE / 2;
			}
			return Item_ListBox_ThumbPosition(item);
		}
		min = item->window.rect.y + SCROLLBAR_SIZE + 1;
		max = item->window.rect.y + item->window.rect.h - 2 * SCROLLBAR_SIZE - 1;
		if (DC->cursory >= min + SCROLLBAR_SIZE / 2 && DC->cursory <= max + SCROLLBAR_SIZE / 2)
		{
			return DC->cursory - SCROLLBAR_SIZE / 2;
		}
		return Item_ListBox_ThumbPosition(item);
	}
	return Item_ListBox_ThumbPosition(item);
}

/*
=================
Item_Slider_ThumbPosition
=================
*/
static float Item_Slider_ThumbPosition(const itemDef_t* item)
{
	float x;
	const editFieldDef_t* editDef = static_cast<editFieldDef_t*>(item->typeData);

	if (item->text)
	{
		x = item->textRect.x + item->textRect.w + 8;
	}
	else
	{
		x = item->window.rect.x;
	}

	if (!editDef || !item->cvar)
	{
		return x;
	}

	float value = DC->getCVarValue(item->cvar);

	if (value < editDef->minVal)
	{
		value = editDef->minVal;
	}
	else if (value > editDef->maxVal)
	{
		value = editDef->maxVal;
	}

	const float range = editDef->maxVal - editDef->minVal;
	value -= editDef->minVal;
	value /= range;
	//value /= (editDef->maxVal - editDef->minVal);
	value *= SLIDER_WIDTH;
	x += value;
	// vm fuckage
	//x = x + (((float)value / editDef->maxVal) * SLIDER_WIDTH);
	return x;
}

/*
=================
Item_Slider_Paint
=================
*/
static void Item_Slider_Paint(itemDef_t* item)
{
	vec4_t newColor;
	float x;
	const auto parent = static_cast<menuDef_t*>(item->parent);

	if (item->window.flags & WINDOW_HASFOCUS)
	{
		vec4_t lowLight{};
		lowLight[0] = 0.8 * parent->focusColor[0];
		lowLight[1] = 0.8 * parent->focusColor[1];
		lowLight[2] = 0.8 * parent->focusColor[2];
		lowLight[3] = 0.8 * parent->focusColor[3];
		LerpColor(parent->focusColor, lowLight, newColor,
			0.5 + 0.5 * sin(static_cast<float>(DC->realTime / static_cast<float>(PULSE_DIVISOR))));
	}
	else
	{
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	const float y = item->window.rect.y;
	if (item->text)
	{
		Item_Text_Paint(item);
		x = item->textRect.x + item->textRect.w + 8;
	}
	else
	{
		x = item->window.rect.x;
	}
	DC->setColor(newColor);
	DC->drawHandlePic(x, y + 2, SLIDER_WIDTH, SLIDER_HEIGHT, DC->Assets.sliderBar);

	x = Item_Slider_ThumbPosition(item);
	//	DC->drawHandlePic( x - (SLIDER_THUMB_WIDTH / 2), y - 2, SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT, DC->Assets.sliderThumb );
	DC->drawHandlePic(x - SLIDER_THUMB_WIDTH / 2, y + 2, SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT,
		DC->Assets.sliderThumb);
}

/*
=================
Item_Paint
=================
*/
static qboolean Item_Paint(itemDef_t* item, const qboolean bDraw)
{
	vec4_t red{};
	red[0] = red[3] = 1;
	red[1] = red[2] = 0;

	if (item == nullptr)
	{
		return qfalse;
	}

	const auto parent = static_cast<menuDef_t*>(item->parent);

	if (item->window.flags & WINDOW_SCRIPTWAITING)
	{
		if (DC->realTime > item->window.delayTime)
		{
			// Time has elapsed, resume running whatever script we saved
			item->window.flags &= ~WINDOW_SCRIPTWAITING;
			Item_RunScript(item, item->window.delayedScript);
		}
	}

	if (item->window.flags & WINDOW_ORBITING)
	{
		if (DC->realTime > item->window.nextTime)
		{
			item->window.nextTime = DC->realTime + item->window.offsetTime;
			// translate
			const float w = item->window.rectClient.w / 2;
			const float h = item->window.rectClient.h / 2;
			const float rx = item->window.rectClient.x + w - item->window.rectEffects.x;
			const float ry = item->window.rectClient.y + h - item->window.rectEffects.y;
			constexpr float a = 3 * M_PI / 180;
			const float c = cos(a);
			const float s = sin(a);
			item->window.rectClient.x = rx * c - ry * s + item->window.rectEffects.x - w;
			item->window.rectClient.y = rx * s + ry * c + item->window.rectEffects.y - h;
			Item_UpdatePosition(item);
		}
	}

	if (item->window.flags & WINDOW_INTRANSITION)
	{
		if (DC->realTime > item->window.nextTime)
		{
			int done = 0;
			item->window.nextTime = DC->realTime + item->window.offsetTime;

			// transition the x,y
			if (item->window.rectClient.x == item->window.rectEffects.x)
			{
				done++;
			}
			else
			{
				if (item->window.rectClient.x < item->window.rectEffects.x)
				{
					item->window.rectClient.x += item->window.rectEffects2.x;
					if (item->window.rectClient.x > item->window.rectEffects.x)
					{
						item->window.rectClient.x = item->window.rectEffects.x;
						done++;
					}
				}
				else
				{
					item->window.rectClient.x -= item->window.rectEffects2.x;
					if (item->window.rectClient.x < item->window.rectEffects.x)
					{
						item->window.rectClient.x = item->window.rectEffects.x;
						done++;
					}
				}
			}

			if (item->window.rectClient.y == item->window.rectEffects.y)
			{
				done++;
			}
			else
			{
				if (item->window.rectClient.y < item->window.rectEffects.y)
				{
					item->window.rectClient.y += item->window.rectEffects2.y;
					if (item->window.rectClient.y > item->window.rectEffects.y)
					{
						item->window.rectClient.y = item->window.rectEffects.y;
						done++;
					}
				}
				else
				{
					item->window.rectClient.y -= item->window.rectEffects2.y;
					if (item->window.rectClient.y < item->window.rectEffects.y)
					{
						item->window.rectClient.y = item->window.rectEffects.y;
						done++;
					}
				}
			}

			if (item->window.rectClient.w == item->window.rectEffects.w)
			{
				done++;
			}
			else
			{
				if (item->window.rectClient.w < item->window.rectEffects.w)
				{
					item->window.rectClient.w += item->window.rectEffects2.w;
					if (item->window.rectClient.w > item->window.rectEffects.w)
					{
						item->window.rectClient.w = item->window.rectEffects.w;
						done++;
					}
				}
				else
				{
					item->window.rectClient.w -= item->window.rectEffects2.w;
					if (item->window.rectClient.w < item->window.rectEffects.w)
					{
						item->window.rectClient.w = item->window.rectEffects.w;
						done++;
					}
				}
			}

			if (item->window.rectClient.h == item->window.rectEffects.h)
			{
				done++;
			}
			else
			{
				if (item->window.rectClient.h < item->window.rectEffects.h)
				{
					item->window.rectClient.h += item->window.rectEffects2.h;
					if (item->window.rectClient.h > item->window.rectEffects.h)
					{
						item->window.rectClient.h = item->window.rectEffects.h;
						done++;
					}
				}
				else
				{
					item->window.rectClient.h -= item->window.rectEffects2.h;
					if (item->window.rectClient.h < item->window.rectEffects.h)
					{
						item->window.rectClient.h = item->window.rectEffects.h;
						done++;
					}
				}
			}

			Item_UpdatePosition(item);

			if (done == 4)
			{
				item->window.flags &= ~WINDOW_INTRANSITION;
			}
		}
	}

#ifdef _TRANS3

	//JLF begin model transition stuff
	if (item->window.flags & WINDOW_INTRANSITIONMODEL)
	{
		if (item->type == ITEM_TYPE_MODEL)
		{
			//fields ing modelptr
			//				vec3_t g2mins2, g2maxs2, g2minsEffect, g2maxsEffect;
			//	float fov_x2, fov_y2, fov_Effectx, fov_Effecty;

			const auto modelptr = static_cast<modelDef_t*>(item->typeData);

			if (DC->realTime > item->window.nextTime)
			{
				int done = 0;
				item->window.nextTime = DC->realTime + item->window.offsetTime;

				// transition the x,y,z max
				if (modelptr->g2maxs[0] == modelptr->g2maxs2[0])
				{
					done++;
				}
				else
				{
					if (modelptr->g2maxs[0] < modelptr->g2maxs2[0])
					{
						modelptr->g2maxs[0] += modelptr->g2maxsEffect[0];
						if (modelptr->g2maxs[0] > modelptr->g2maxs2[0])
						{
							modelptr->g2maxs[0] = modelptr->g2maxs2[0];
							done++;
						}
					}
					else
					{
						modelptr->g2maxs[0] -= modelptr->g2maxsEffect[0];
						if (modelptr->g2maxs[0] < modelptr->g2maxs2[0])
						{
							modelptr->g2maxs[0] = modelptr->g2maxs2[0];
							done++;
						}
					}
				}
				//y
				if (modelptr->g2maxs[1] == modelptr->g2maxs2[1])
				{
					done++;
				}
				else
				{
					if (modelptr->g2maxs[1] < modelptr->g2maxs2[1])
					{
						modelptr->g2maxs[1] += modelptr->g2maxsEffect[1];
						if (modelptr->g2maxs[1] > modelptr->g2maxs2[1])
						{
							modelptr->g2maxs[1] = modelptr->g2maxs2[1];
							done++;
						}
					}
					else
					{
						modelptr->g2maxs[1] -= modelptr->g2maxsEffect[1];
						if (modelptr->g2maxs[1] < modelptr->g2maxs2[1])
						{
							modelptr->g2maxs[1] = modelptr->g2maxs2[1];
							done++;
						}
					}
				}

				//z

				if (modelptr->g2maxs[2] == modelptr->g2maxs2[2])
				{
					done++;
				}
				else
				{
					if (modelptr->g2maxs[2] < modelptr->g2maxs2[2])
					{
						modelptr->g2maxs[2] += modelptr->g2maxsEffect[2];
						if (modelptr->g2maxs[2] > modelptr->g2maxs2[2])
						{
							modelptr->g2maxs[2] = modelptr->g2maxs2[2];
							done++;
						}
					}
					else
					{
						modelptr->g2maxs[2] -= modelptr->g2maxsEffect[2];
						if (modelptr->g2maxs[2] < modelptr->g2maxs2[2])
						{
							modelptr->g2maxs[2] = modelptr->g2maxs2[2];
							done++;
						}
					}
				}

				// transition the x,y,z min
				if (modelptr->g2mins[0] == modelptr->g2mins2[0])
				{
					done++;
				}
				else
				{
					if (modelptr->g2mins[0] < modelptr->g2mins2[0])
					{
						modelptr->g2mins[0] += modelptr->g2minsEffect[0];
						if (modelptr->g2mins[0] > modelptr->g2mins2[0])
						{
							modelptr->g2mins[0] = modelptr->g2mins2[0];
							done++;
						}
					}
					else
					{
						modelptr->g2mins[0] -= modelptr->g2minsEffect[0];
						if (modelptr->g2mins[0] < modelptr->g2mins2[0])
						{
							modelptr->g2mins[0] = modelptr->g2mins2[0];
							done++;
						}
					}
				}
				//y
				if (modelptr->g2mins[1] == modelptr->g2mins2[1])
				{
					done++;
				}
				else
				{
					if (modelptr->g2mins[1] < modelptr->g2mins2[1])
					{
						modelptr->g2mins[1] += modelptr->g2minsEffect[1];
						if (modelptr->g2mins[1] > modelptr->g2mins2[1])
						{
							modelptr->g2mins[1] = modelptr->g2mins2[1];
							done++;
						}
					}
					else
					{
						modelptr->g2mins[1] -= modelptr->g2minsEffect[1];
						if (modelptr->g2mins[1] < modelptr->g2mins2[1])
						{
							modelptr->g2mins[1] = modelptr->g2mins2[1];
							done++;
						}
					}
				}

				//z

				if (modelptr->g2mins[2] == modelptr->g2mins2[2])
				{
					done++;
				}
				else
				{
					if (modelptr->g2mins[2] < modelptr->g2mins2[2])
					{
						modelptr->g2mins[2] += modelptr->g2minsEffect[2];
						if (modelptr->g2mins[2] > modelptr->g2mins2[2])
						{
							modelptr->g2mins[2] = modelptr->g2mins2[2];
							done++;
						}
					}
					else
					{
						modelptr->g2mins[2] -= modelptr->g2minsEffect[2];
						if (modelptr->g2mins[2] < modelptr->g2mins2[2])
						{
							modelptr->g2mins[2] = modelptr->g2mins2[2];
							done++;
						}
					}
				}

				//fovx
				if (modelptr->fov_x == modelptr->fov_x2)
				{
					done++;
				}
				else
				{
					if (modelptr->fov_x < modelptr->fov_x2)
					{
						modelptr->fov_x += modelptr->fov_Effectx;
						if (modelptr->fov_x > modelptr->fov_x2)
						{
							modelptr->fov_x = modelptr->fov_x2;
							done++;
						}
					}
					else
					{
						modelptr->fov_x -= modelptr->fov_Effectx;
						if (modelptr->fov_x < modelptr->fov_x2)
						{
							modelptr->fov_x = modelptr->fov_x2;
							done++;
						}
					}
				}

				//fovy
				if (modelptr->fov_y == modelptr->fov_y2)
				{
					done++;
				}
				else
				{
					if (modelptr->fov_y < modelptr->fov_y2)
					{
						modelptr->fov_y += modelptr->fov_Effecty;
						if (modelptr->fov_y > modelptr->fov_y2)
						{
							modelptr->fov_y = modelptr->fov_y2;
							done++;
						}
					}
					else
					{
						modelptr->fov_y -= modelptr->fov_Effecty;
						if (modelptr->fov_y < modelptr->fov_y2)
						{
							modelptr->fov_y = modelptr->fov_y2;
							done++;
						}
					}
				}

				if (done == 5)
				{
					item->window.flags &= ~WINDOW_INTRANSITIONMODEL;
				}
			}
		}
	}
#endif
	//JLF end transition stuff for models

	if (item->window.ownerDrawFlags && DC->ownerDrawVisible)
	{
		if (!DC->ownerDrawVisible(item->window.ownerDrawFlags))
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}
		else
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	if (item->disabled && item->disabledHidden)
	{
		return qfalse;
	}

	if (item->cvarFlags & (CVAR_SHOW | CVAR_HIDE))
	{
		if (!Item_EnableShowViaCvar(item, CVAR_SHOW))
		{
			return qfalse;
		}
	}

	if (item->window.flags & WINDOW_TIMEDVISIBLE)
	{
	}

	if (!(item->window.flags & WINDOW_VISIBLE))
	{
		return qfalse;
	}

	if (!bDraw)
	{
		return qtrue;
	}
	//okay to paint
	//JLFMOUSE

	if (item->window.flags & WINDOW_MOUSEOVER)
	{
		if (item->descText && !Display_KeyBindPending())
		{
			// Make DOUBLY sure that this item should have desctext.
			// NOTE : we can't just check the mouse position on this, what if we TABBED
			// to the current menu item -- in that case our mouse isn't over the item.
			// Removing the WINDOW_MOUSEOVER flag just prevents the item's OnExit script from running
			//	if (!Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory))
			//	{	// It isn't something that should, because it isn't live anymore.
			//		item->window.flags &= ~WINDOW_MOUSEOVER;
			//	}
			//	else

			//END JLFMOUSE

			// items can be enabled and disabled based on cvars
			if (!(item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE)))
			{
				int xPos;
				// Draw the desctext
				const char* textPtr = item->descText;
				if (*textPtr == '@') // string reference
				{
					textPtr = SE_GetString(&textPtr[1]);
				}

				vec4_t color = { 1, 1, 1, 1 };
				Item_TextColor(item, &color);

				float fDescScale = parent->descScale ? parent->descScale : 1;
				const float fDescScaleCopy = fDescScale;
				while (true)
				{
					// FIXME - add some type of parameter in the menu file like descfont to specify the font for the descriptions for this menu.
					const int textWidth = DC->textWidth(textPtr, fDescScale, 4); //  item->font);

					if (parent->descAlignment == ITEM_ALIGN_RIGHT)
					{
						xPos = parent->descX - textWidth; // Right justify
					}
					else if (parent->descAlignment == ITEM_ALIGN_CENTER)
					{
						xPos = parent->descX - textWidth / 2; // Center justify
					}
					else // Left justify
					{
						xPos = parent->descX;
					}

					if (parent->descAlignment == ITEM_ALIGN_CENTER)
					{
						// only this one will auto-shrink the scale until we eventually fit...
						//
						if (xPos + textWidth > SCREEN_WIDTH - 4)
						{
							fDescScale -= 0.001f;
							continue;
						}
					}

					// Try to adjust it's y placement if the scale has changed...
					//
					int iYadj = 0;
					if (fDescScale != fDescScaleCopy)
					{
						const int iOriginalTextHeight = DC->textHeight(textPtr, fDescScaleCopy,
							uiInfo.uiDC.Assets.qhMediumFont);
						iYadj = iOriginalTextHeight - DC->textHeight(textPtr, fDescScale,
							uiInfo.uiDC.Assets.qhMediumFont);
					}

					// FIXME - add some type of parameter in the menu file like descfont to specify the font for the descriptions for this menu.
					DC->drawText(xPos, parent->descY + iYadj, fDescScale, parent->descColor, textPtr, 0,
						parent->descTextStyle, 4); //item->font);
					break;
				}
			}
		}
	}

	// paint the rect first..
	Window_Paint(&item->window, parent->fadeAmount, parent->fadeClamp, parent->fadeCycle);

	// Print a box showing the extents of the rectangle, when in debug mode
	if (uis.debugMode)
	{
		vec4_t color{};
		color[1] = color[3] = 1;
		color[0] = color[2] = 0;
		DC->drawRect(
			item->window.rect.x,
			item->window.rect.y,
			item->window.rect.w,
			item->window.rect.h,
			1,
			color);
	}

	//DC->drawRect(item->window.rect.x, item->window.rect.y, item->window.rect.w, item->window.rect.h, 1, red);

	switch (item->type)
	{
	case ITEM_TYPE_OWNERDRAW:
		Item_OwnerDraw_Paint(item);
		break;

	case ITEM_TYPE_TEXT:
	case ITEM_TYPE_BUTTON:
		Item_Text_Paint(item);
		break;
	case ITEM_TYPE_RADIOBUTTON:
		break;
	case ITEM_TYPE_CHECKBOX:
		break;
	case ITEM_TYPE_EDITFIELD:
	case ITEM_TYPE_NUMERICFIELD:
		Item_TextField_Paint(item);
		break;
	case ITEM_TYPE_COMBO:
		break;
	case ITEM_TYPE_LISTBOX:
		Item_ListBox_Paint(item);
		break;
	case ITEM_TYPE_TEXTSCROLL:
		Item_TextScroll_Paint(item);
		break;
	case ITEM_TYPE_MODEL:
		Item_Model_Paint(item);
		break;
	case ITEM_TYPE_YESNO:
		Item_YesNo_Paint(item);
		break;
	case ITEM_TYPE_MULTI:
		Item_Multi_Paint(item);
		break;
	case ITEM_TYPE_BIND:
		Item_Bind_Paint(item);
		break;
	case ITEM_TYPE_SLIDER:
	case ITEM_TYPE_SLIDER_INTEGER:
		Item_Slider_Paint(item);
		break;
	case ITEM_TYPE_SLIDER_ROTATE:
		//Don't bother drawing anything at all for now!
		break;
	default:
		break;
	}
	return qtrue;
}

/*
=================
LerpColor
=================
*/
void LerpColor(vec4_t a, vec4_t b, vec4_t c, const float t)
{
	// lerp and clamp each component
	for (int i = 0; i < 4; i++)
	{
		c[i] = a[i] + t * (b[i] - a[i]);
		if (c[i] < 0)
		{
			c[i] = 0;
		}
		else if (c[i] > 1.0)
		{
			c[i] = 1.0;
		}
	}
}

/*
=================
Fade
=================
*/
void Fade(int* flags, float* f, const float clamp, int* nextTime, const int offsetTime, const qboolean bFlags, const float fadeAmount)
{
	if (*flags & (WINDOW_FADINGOUT | WINDOW_FADINGIN))
	{
		if (DC->realTime > *nextTime)
		{
			*nextTime = DC->realTime + offsetTime;
			if (*flags & WINDOW_FADINGOUT)
			{
				*f -= fadeAmount;
				if (bFlags && *f <= 0.0)
				{
					*flags &= ~(WINDOW_FADINGOUT | WINDOW_VISIBLE);
				}
			}
			else
			{
				*f += fadeAmount;
				if (*f >= clamp)
				{
					*f = clamp;
					if (bFlags)
					{
						*flags &= ~WINDOW_FADINGIN;
					}
				}
			}
		}
	}
}

/*
=================
GradientBar_Paint
=================
*/
static void GradientBar_Paint(const rectDef_t* rect, vec4_t color)
{
	// gradient bar takes two paints
	DC->setColor(color);
	DC->drawHandlePic(rect->x, rect->y, rect->w, rect->h, DC->Assets.gradientBar);
	DC->setColor(nullptr);
}

/*
=================
Window_Paint
=================
*/
void Window_Paint(Window* w, const float fadeAmount, const float fadeClamp, const float fadeCycle)
{
	//float bordersize = 0;
	vec4_t color{};
	rectDef_t fillRect = w->rect;

	if (uis.debugMode)
	{
		color[0] = color[1] = color[2] = color[3] = 1;
		DC->drawRect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, 1, color);
	}

	if (w == nullptr || w->style == 0 && w->border == 0)
	{
		return;
	}

	if (w->border != 0)
	{
		fillRect.x += w->borderSize;
		fillRect.y += w->borderSize;
		fillRect.w -= w->borderSize + 1;
		fillRect.h -= w->borderSize + 1;
	}

	if (w->style == WINDOW_STYLE_FILLED)
	{
		// box, but possible a shader that needs filled
		if (w->background)
		{
			Fade(&w->flags, &w->backColor[3], fadeClamp, &w->nextTime, fadeCycle, qtrue, fadeAmount);
			DC->setColor(w->backColor);
			DC->drawHandlePic(fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background);
			DC->setColor(nullptr);
		}
		else
		{
			DC->fillRect(fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->backColor);
		}
	}
	else if (w->style == WINDOW_STYLE_GRADIENT)
	{
		GradientBar_Paint(&fillRect, w->backColor);
		// gradient bar
	}
	else if (w->style == WINDOW_STYLE_SHADER)
	{
		if (w->flags & WINDOW_PLAYERCOLOR)
		{
			vec4_t colour{};
			colour[0] = ui_char_color_red.integer / 255.0f;
			colour[1] = ui_char_color_green.integer / 255.0f;
			colour[2] = ui_char_color_blue.integer / 255.0f;
			colour[3] = 1;
			ui.R_SetColor(colour);
		}
		else if (w->flags & WINDOW_FORECOLORSET)
		{
			DC->setColor(w->foreColor);
		}
		DC->drawHandlePic(fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background);
		DC->setColor(nullptr);
	}

	if (w->border == WINDOW_BORDER_FULL)
	{
		// full
		// HACK HACK HACK
		if (w->style == WINDOW_STYLE_TEAMCOLOR)
		{
			if (color[0] > 0)
			{
				// red
				color[0] = 1;
				color[1] = color[2] = .5;
			}
			else
			{
				color[2] = 1;
				color[0] = color[1] = .5;
			}
			color[3] = 1;
			DC->drawRect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, color);
		}
		else
		{
			DC->drawRect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, w->borderColor);
		}
	}
	else if (w->border == WINDOW_BORDER_HORZ)
	{
		// top/bottom
		DC->setColor(w->borderColor);
		DC->drawTopBottom(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize);
		DC->setColor(nullptr);
	}
	else if (w->border == WINDOW_BORDER_VERT)
	{
		// left right
		DC->setColor(w->borderColor);
		DC->drawSides(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize);
		DC->setColor(nullptr);
	}
	else if (w->border == WINDOW_BORDER_KCGRADIENT)
	{
		// this is just two gradient bars along each horz edge
		rectDef_t r = w->rect;
		r.h = w->borderSize;
		GradientBar_Paint(&r, w->borderColor);
		r.y = w->rect.y + w->rect.h - 1;
		GradientBar_Paint(&r, w->borderColor);
	}
}

/*
=================
Display_KeyBindPending
=================
*/
qboolean Display_KeyBindPending()
{
	return g_waitingForKey;
}

/*
=================
ToWindowCoords
=================
*/
void ToWindowCoords(float* x, float* y, const windowDef_t* window)
{
	if (window->border != 0)
	{
		*x += window->borderSize;
		*y += window->borderSize;
	}
	*x += window->rect.x;
	*y += window->rect.y;
}

/*
=================
Item_Text_AutoWrapped_Paint
=================
*/
void Item_Text_AutoWrapped_Paint(itemDef_t* item)
{
	const char* textPtr;
	char buff[1024]{};
	vec4_t color;

	int textWidth = 0;
	const char* newLinePtr = nullptr;

	if (item->text == nullptr)
	{
		char text[1024];
		if (item->cvar == nullptr)
		{
			return;
		}
		DC->getCVarString(item->cvar, text, sizeof text);
		textPtr = text;
	}
	else
	{
		textPtr = item->text;
	}
	if (*textPtr == '@') // string reference
	{
		textPtr = SE_GetString(&textPtr[1]);
	}

	if (*textPtr == '\0')
	{
		return;
	}
	Item_TextColor(item, &color);
	if (item->value == 0)
	{
		item->value = static_cast<int>(0.5 + static_cast<float>(DC->textWidth(textPtr, item->textscale, item->font)) /
			item->window.rect.w);
	}
	const int height = DC->textHeight(textPtr, item->textscale, item->font);
	item->special = 0;

	float y = item->textaligny;
	int len = 0;
	buff[0] = '\0';
	int newLine = 0;
	int newLineWidth = 0;
	const char* p = textPtr;
	int line = 1;
	while (true) //findmeste (this will break widechar languages)!
	{
		if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\0')
		{
			newLine = len;
			newLinePtr = p + 1;
			newLineWidth = textWidth;
		}
		textWidth = DC->textWidth(buff, item->textscale, 0);
		if (newLine && textWidth >= item->window.rect.w - item->textalignx || *p == '\n' || *p == '\0')
		{
			if (line > item->cursorPos) //scroll
			{
				if (len)
				{
					if (item->textalignment == ITEM_ALIGN_LEFT)
					{
						item->textRect.x = item->textalignx;
					}
					else if (item->textalignment == ITEM_ALIGN_RIGHT)
					{
						item->textRect.x = item->textalignx - newLineWidth;
					}
					else if (item->textalignment == ITEM_ALIGN_CENTER)
					{
						item->textRect.x = item->textalignx - newLineWidth / 2;
					}
					item->textRect.y = y;
					ToWindowCoords(&item->textRect.x, &item->textRect.y, &item->window);
					//
					buff[newLine] = '\0';

					if (*p && y + height + 4 > item->window.rect.h - height)
					{
						item->special = 1;
						strcat(buff, "..."); //uhh, let's render some ellipses
					}
					DC->drawText(item->textRect.x, item->textRect.y, item->textscale, color, buff, 0, item->textStyle,
						item->font);
				}
				y += height + 4;
				if (y > item->window.rect.h - height)
				{
					//reached the bottom of the box, so stop
					break;
				}
				len = 0;
			}
			else
			{
				strcpy(buff, "...");
				len = 3;
			}
			if (*p == '\0')
			{
				//end of string
				break;
			}
			p = newLinePtr;
			newLine = 0;
			newLineWidth = 0;
			line++;
		}
		buff[len++] = *p++;
		buff[len] = '\0';
	}
	item->textRect = item->window.rect;
}

/*
=================
Rect_ContainsPoint
=================
*/
static qboolean Rect_ContainsPoint(const rectDef_t* rect, const float x, const float y)
{
	//JLF  ignore mouse pointer location
	//	return true;
	// END JLF
	if (rect)
	{
		//		if ((x > rect->x) && (x < (rect->x + rect->w)) && (y > rect->y) && (y < (rect->y + rect->h)))
		if (x > rect->x && x < rect->x + rect->w)
		{
			if (y > rect->y && y < rect->y + rect->h)
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

static qboolean Item_TextScroll_HandleKey(itemDef_t* item, const int key, qboolean down, const qboolean force)
{
	const auto scrollPtr = static_cast<textScrollDef_t*>(item->typeData);

	if (force || Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags &
		WINDOW_HASFOCUS)
	{
		const int max = Item_TextScroll_MaxScroll(item);

		const int viewmax = item->window.rect.h / scrollPtr->lineHeight;
		if (key == A_CURSOR_UP || key == A_KP_8)
		{
			scrollPtr->startPos--;
			if (scrollPtr->startPos < 0)
			{
				scrollPtr->startPos = 0;
			}
			return qtrue;
		}

		if (key == A_CURSOR_DOWN || key == A_KP_2)
		{
			scrollPtr->startPos++;
			if (scrollPtr->startPos > max)
			{
				scrollPtr->startPos = max;
			}

			return qtrue;
		}

		//Raz: Added
		if (key == A_MWHEELUP)
		{
			scrollPtr->startPos--;
			if (scrollPtr->startPos < 0)
			{
				scrollPtr->startPos = 0;
				Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
				return qfalse;
			}
			Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
			return qtrue;
		}
		if (key == A_MWHEELDOWN)
		{
			scrollPtr->startPos++;
			if (scrollPtr->startPos > max)
			{
				scrollPtr->startPos = max;
				Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
				return qfalse;
			}
			Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
			return qtrue;
		}

		// mouse hit
		if (key == A_MOUSE1 || key == A_MOUSE2)
		{
			if (item->window.flags & WINDOW_LB_LEFTARROW)
			{
				scrollPtr->startPos--;
				if (scrollPtr->startPos < 0)
				{
					scrollPtr->startPos = 0;
				}
			}
			else if (item->window.flags & WINDOW_LB_RIGHTARROW)
			{
				// one down
				scrollPtr->startPos++;
				if (scrollPtr->startPos > max)
				{
					scrollPtr->startPos = max;
				}
			}
			else if (item->window.flags & WINDOW_LB_PGUP)
			{
				// page up
				scrollPtr->startPos -= viewmax;
				if (scrollPtr->startPos < 0)
				{
					scrollPtr->startPos = 0;
				}
			}
			else if (item->window.flags & WINDOW_LB_PGDN)
			{
				// page down
				scrollPtr->startPos += viewmax;
				if (scrollPtr->startPos > max)
				{
					scrollPtr->startPos = max;
				}
			}
			else if (item->window.flags & WINDOW_LB_THUMB)
			{
				// Display_SetCaptureItem(item);
			}

			return qtrue;
		}

		if (key == A_HOME || key == A_KP_7)
		{
			// home
			scrollPtr->startPos = 0;
			return qtrue;
		}
		if (key == A_END || key == A_KP_1)
		{
			// end
			scrollPtr->startPos = max;
			return qtrue;
		}
		if (key == A_PAGE_UP || key == A_KP_9)
		{
			scrollPtr->startPos -= viewmax;
			if (scrollPtr->startPos < 0)
			{
				scrollPtr->startPos = 0;
			}

			return qtrue;
		}
		if (key == A_PAGE_DOWN || key == A_KP_3)
		{
			scrollPtr->startPos += viewmax;
			if (scrollPtr->startPos > max)
			{
				scrollPtr->startPos = max;
			}
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
Item_ListBox_MaxScroll
=================
*/
int Item_ListBox_MaxScroll(const itemDef_t* item)
{
	const listBoxDef_t* listPtr = static_cast<listBoxDef_t*>(item->typeData);
	const int count = DC->feederCount(item->special);
	int max;

#ifdef NEW_FEEDER_V3
	if (item->special == FEEDER_MD_MODELS)
	{
		max = count - item->window.rect.w / listPtr->elementWidth;
	}
	else
#endif
	if (item->window.flags & WINDOW_HORIZONTAL)
	{
		max = count - item->window.rect.w / listPtr->elementWidth + 1;
	}
	else
	{
		max = count - item->window.rect.h / listPtr->elementHeight + 1;
	}

	if (max < 0)
	{
		return 0;
	}
	return max;
}

/*
=================
Item_ListBox_ThumbPosition
=================
*/
int Item_ListBox_ThumbPosition(itemDef_t* item)
{
	float pos, size;
	const listBoxDef_t* listPtr = static_cast<listBoxDef_t*>(item->typeData);

	const float max = Item_ListBox_MaxScroll(item);
	if (item->window.flags & WINDOW_HORIZONTAL)
	{
		size = item->window.rect.w - SCROLLBAR_SIZE * 2 - 2;
		if (max > 0)
		{
			pos = (size - SCROLLBAR_SIZE) / static_cast<float>(max);
		}
		else
		{
			pos = 0;
		}
		pos *= listPtr->startPos;
		return item->window.rect.x + 1 + SCROLLBAR_SIZE + pos;
	}
	size = item->window.rect.h - SCROLLBAR_SIZE * 2 - 2;
	if (max > 0)
	{
		pos = (size - SCROLLBAR_SIZE) / static_cast<float>(max);
	}
	else
	{
		pos = 0;
	}
	pos *= listPtr->startPos;
	return item->window.rect.y + 1 + SCROLLBAR_SIZE + pos;
}

/*
=================
Item_ListBox_OverLB
=================
*/
static int Item_ListBox_OverLB(itemDef_t* item, const float x, const float y)
{
	rectDef_t r{};
	int thumbstart;

	if (item->window.flags & WINDOW_HORIZONTAL)
	{
		// check if on left arrow
		r.x = item->window.rect.x;
		r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
		r.h = r.w = SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_LEFTARROW;
		}
		// check if on right arrow
		r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_RIGHTARROW;
		}
		// check if on thumb
		thumbstart = Item_ListBox_ThumbPosition(item);
		r.x = thumbstart;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_THUMB;
		}
		r.x = item->window.rect.x + SCROLLBAR_SIZE;
		r.w = thumbstart - r.x;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_PGUP;
		}
		r.x = thumbstart + SCROLLBAR_SIZE;
		r.w = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_PGDN;
		}
	}
	else
	{
		r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
		r.y = item->window.rect.y;
		r.h = r.w = SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_LEFTARROW;
		}
		r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_RIGHTARROW;
		}
		thumbstart = Item_ListBox_ThumbPosition(item);
		r.y = thumbstart;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_THUMB;
		}
		r.y = item->window.rect.y + SCROLLBAR_SIZE;
		r.h = thumbstart - r.y;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_PGUP;
		}
		r.y = thumbstart + SCROLLBAR_SIZE;
		r.h = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y))
		{
			return WINDOW_LB_PGDN;
		}
	}
	return 0;
}

/*
=================
Item_ListBox_MouseEnter
=================
*/
static void Item_ListBox_MouseEnter(itemDef_t* item, const float x, const float y)
{
#ifdef NEW_FEEDER_V1
	rectDef_t r{};
	auto* listPtr = static_cast<listBoxDef_t*>(item->typeData);

	item->window.flags &= ~(WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN);
	item->window.flags |= Item_ListBox_OverLB(item, x, y);

	// Only process if not over arrows/thumb/page buttons
	if (item->window.flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN))
		return;

	// Set up the selection rectangle
	r.x = item->window.rect.x;
	r.y = item->window.rect.y;
	r.w = item->window.rect.w - ((item->window.flags & WINDOW_HORIZONTAL) ? listPtr->drawPadding : SCROLLBAR_SIZE);
	r.h = item->window.rect.h - ((item->window.flags & WINDOW_HORIZONTAL) ? SCROLLBAR_SIZE : listPtr->drawPadding);

	if (!Rect_ContainsPoint(&r, x, y))
		return;

	// Multiple rows and columns for image listboxes
	if (listPtr->elementStyle == LISTBOX_IMAGE &&
		item->window.rect.w > (listPtr->elementWidth * 2) &&
		item->window.rect.h > (listPtr->elementHeight * 2))
	{
		int row = static_cast<int>((y - r.y) / listPtr->elementHeight);
		int col = static_cast<int>((x - r.x) / listPtr->elementWidth);
		int rowLength = static_cast<int>(r.w / listPtr->elementWidth);

		listPtr->cursorPos = row * rowLength + col + listPtr->startPos;
#ifdef NEW_FEEDER_V2
		if (listPtr->cursorPos >= listPtr->endPos)
			listPtr->cursorPos = item->cursorPos;
#else
		if (listPtr->cursorPos > listPtr->endPos)
			listPtr->cursorPos = listPtr->endPos;
#endif
		return;
	}

	// Single row/column logic
	if (item->window.flags & WINDOW_HORIZONTAL) {
		listPtr->cursorPos = static_cast<int>((x - r.x) / listPtr->elementWidth) + listPtr->startPos;
	} else {
		listPtr->cursorPos = static_cast<int>((y - r.y) / listPtr->elementHeight) + listPtr->startPos;
	}
#ifdef NEW_FEEDER_V2
	if (listPtr->cursorPos >= listPtr->endPos)
		listPtr->cursorPos = item->cursorPos;
#else
	if (listPtr->cursorPos > listPtr->endPos)
		listPtr->cursorPos = listPtr->endPos;
#endif
#else
	rectDef_t r{};
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);

	item->window.flags &= ~(WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP |
		WINDOW_LB_PGDN);
	item->window.flags |= Item_ListBox_OverLB(item, x, y);

	if (item->window.flags & WINDOW_HORIZONTAL)
	{
		if (!(item->window.flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP |
			WINDOW_LB_PGDN)))
		{
			// check for selection hit as we have exausted buttons and thumb
			if (listPtr->elementStyle == LISTBOX_IMAGE)
			{
				r.x = item->window.rect.x;
				r.y = item->window.rect.y;
				r.h = item->window.rect.h - SCROLLBAR_SIZE;
				r.w = item->window.rect.w - listPtr->drawPadding;
				if (Rect_ContainsPoint(&r, x, y))
				{
					listPtr->cursorPos = static_cast<int>((x - r.x) / listPtr->elementWidth) + listPtr->startPos;
					if (listPtr->cursorPos > listPtr->endPos)
					{
						listPtr->cursorPos = listPtr->endPos;
					}
				}
			}
			else
			{
				// text hit..
			}
		}
	}
	else if (!(item->window.flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP |
		WINDOW_LB_PGDN)))
	{
		r.x = item->window.rect.x;
		r.y = item->window.rect.y;
		r.w = item->window.rect.w - SCROLLBAR_SIZE;
		r.h = item->window.rect.h - listPtr->drawPadding;
		if (Rect_ContainsPoint(&r, x, y))
		{
			listPtr->cursorPos = static_cast<int>((y - 2 - r.y) / listPtr->elementHeight) + listPtr->startPos;
			if (listPtr->cursorPos > listPtr->endPos)
			{
				listPtr->cursorPos = listPtr->endPos;
			}
		}
	}
#endif
}

/*
=================
Item_MouseEnter
=================
*/
void Item_MouseEnter(itemDef_t* item, const float x, const float y)
{
	rectDef_t r;
	//JLFMOUSE
	if (item)
	{
		r = item->textRect;
		//		r.y -= r.h;			// NOt sure why this is here, but I commented it out.
		// in the text rect?

		// items can be enabled and disabled
		if (item->disabled)
		{
			return;
		}

		// items can be enabled and disabled based on cvars
		if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE))
		{
			return;
		}

		if (item->cvarFlags & (CVAR_SHOW | CVAR_HIDE) && !Item_EnableShowViaCvar(item, CVAR_SHOW))
		{
			return;
		}

		//JLFMOUSE
		if (Rect_ContainsPoint(&r, x, y))
		{
			if (!(item->window.flags & WINDOW_MOUSEOVERTEXT))
			{
				Item_RunScript(item, item->mouseEnterText);
				item->window.flags |= WINDOW_MOUSEOVERTEXT;
			}

			if (!(item->window.flags & WINDOW_MOUSEOVER))
			{
				Item_RunScript(item, item->mouseEnter);
				item->window.flags |= WINDOW_MOUSEOVER;
			}
		}
		else
		{
			// not in the text rect
			if (item->window.flags & WINDOW_MOUSEOVERTEXT)
			{
				// if we were
				Item_RunScript(item, item->mouseExitText);
				item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
			}

			if (!(item->window.flags & WINDOW_MOUSEOVER))
			{
				Item_RunScript(item, item->mouseEnter);
				item->window.flags |= WINDOW_MOUSEOVER;
			}

			if (item->type == ITEM_TYPE_LISTBOX)
			{
				Item_ListBox_MouseEnter(item, x, y);
			}
			else if (item->type == ITEM_TYPE_TEXTSCROLL)
			{
				Item_TextScroll_MouseEnter(item, x, y);
			}
		}
	}
}

/*
=================
Item_SetFocus
=================
*/
// will optionaly set focus to this item
qboolean Item_SetFocus(itemDef_t* item, const float x, const float y)
{
	const sfxHandle_t* sfx = &DC->Assets.itemFocusSound;
	qboolean playSound = qfalse;
	// sanity check, non-null, not a decoration and does not already have the focus
	if (item == nullptr || item->window.flags & WINDOW_DECORATION || item->window.flags & WINDOW_HASFOCUS || !(item->
		window.flags & WINDOW_VISIBLE) || item->window.flags & WINDOW_INACTIVE)
	{
		return qfalse;
	}
	const auto parent = static_cast<menuDef_t*>(item->parent);

	// items can be enabled and disabled
	if (item->disabled)
	{
		return qfalse;
	}

	// items can be enabled and disabled based on cvars
	if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE))
	{
		return qfalse;
	}

	if (item->cvarFlags & (CVAR_SHOW | CVAR_HIDE) && !Item_EnableShowViaCvar(item, CVAR_SHOW))
	{
		return qfalse;
	}

	itemDef_t* oldFocus = Menu_ClearFocus(static_cast<menuDef_t*>(item->parent));

	if (item->type == ITEM_TYPE_TEXT)
	{
		rectDef_t r;
		r = item->textRect;
		//r.y -= r.h;
		//JLFMOUSE
		if (Rect_ContainsPoint(&r, x, y))
		{
			item->window.flags |= WINDOW_HASFOCUS;
			if (item->focusSound)
			{
				sfx = &item->focusSound;
			}
			playSound = qtrue;
		}
		else

		{
			if (oldFocus)
			{
				oldFocus->window.flags |= WINDOW_HASFOCUS;
				if (oldFocus->onFocus)
				{
					Item_RunScript(oldFocus, oldFocus->onFocus);
				}
			}
		}
	}
	else
	{
		item->window.flags |= WINDOW_HASFOCUS;
		if (item->onFocus)
		{
			Item_RunScript(item, item->onFocus);
		}
		if (item->focusSound)
		{
			sfx = &item->focusSound;
		}
		playSound = qtrue;
	}

	if (playSound && sfx)
	{
		DC->startLocalSound(*sfx, CHAN_LOCAL_SOUND);
	}

	for (int i = 0; i < parent->itemCount; i++)
	{
		if (parent->items[i] == item)
		{
			parent->cursorItem = i;
			break;
		}
	}

	return qtrue;
}

/*
=================
IsVisible
=================
*/
static qboolean IsVisible(const int flags)
{
	return static_cast<qboolean>((flags & WINDOW_VISIBLE & ~(flags & WINDOW_FADINGOUT)) != 0);
}

/*
=================
Item_MouseLeave
=================
*/
static void Item_MouseLeave(itemDef_t* item)
{
	if (item)
	{
		if (item->window.flags & WINDOW_MOUSEOVERTEXT)
		{
			Item_RunScript(item, item->mouseExitText);
			item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
		}
		Item_RunScript(item, item->mouseExit);
		item->window.flags &= ~(WINDOW_LB_RIGHTARROW | WINDOW_LB_LEFTARROW);
	}
}

/*
=================
Item_SetMouseOver
=================
*/
static void Item_SetMouseOver(itemDef_t* item, const qboolean focus)
{
	if (item)
	{
		if (focus)
		{
			item->window.flags |= WINDOW_MOUSEOVER;
		}
		else
		{
			item->window.flags &= ~WINDOW_MOUSEOVER;
		}
	}
}

/*
=================
Menu_HandleMouseMove
=================
*/
void Menu_HandleMouseMove(const menuDef_t* menu, const float x, const float y)
{
	qboolean focusSet = qfalse;

	if (menu == nullptr)
	{
		return;
	}

	if (!(menu->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED)))
	{
		return;
	}

	if (itemCapture)
	{
		//Item_MouseMove(itemCapture, x, y);
		return;
	}

	if (g_waitingForKey || g_editingField)
	{
		return;
	}

	// FIXME: this is the whole issue of focus vs. mouse over..
	// need a better overall solution as i don't like going through everything twice
	for (int pass = 0; pass < 2; pass++)
	{
		for (int i = 0; i < menu->itemCount; i++)
		{
			// turn off focus each item
			// menu->items[i].window.flags &= ~WINDOW_HASFOCUS;

			if (!(menu->items[i]->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED)))
			{
				continue;
			}

			if (menu->items[i]->window.flags & WINDOW_INACTIVE)
			{
				continue;
			}

			if (menu->items[i]->disabled)
			{
				continue;
			}

			// items can be enabled and disabled based on cvars
			if (menu->items[i]->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(
				menu->items[i], CVAR_ENABLE))
			{
				continue;
			}

			if (menu->items[i]->cvarFlags & (CVAR_SHOW | CVAR_HIDE) && !Item_EnableShowViaCvar(
				menu->items[i], CVAR_SHOW))
			{
				continue;
			}

			if (Rect_ContainsPoint(&menu->items[i]->window.rect, x, y))
			{
				if (pass == 1)
				{
					itemDef_t* overItem = menu->items[i];
					if (overItem->type == ITEM_TYPE_TEXT && overItem->text)
					{
						if (!Rect_ContainsPoint(&overItem->window.rect, x, y))
						{
							continue;
						}
					}

					// if we are over an item
					if (IsVisible(overItem->window.flags))
					{
						// different one
						Item_MouseEnter(overItem, x, y);
						// Item_SetMouseOver(overItem, qtrue);

						// if item is not a decoration see if it can take focus
						if (!focusSet)
						{
							focusSet = Item_SetFocus(overItem, x, y);
						}
					}
				}
			}
			else if (menu->items[i]->window.flags & WINDOW_MOUSEOVER)
			{
				Item_MouseLeave(menu->items[i]);
				Item_SetMouseOver(menu->items[i], qfalse);
			}
		}
	}
}

/*
=================
Display_MouseMove
=================
*/
qboolean Display_MouseMove(void* p, const int x, const int y)
{
	auto menu = static_cast<menuDef_t*>(p);

	if (menu == nullptr)
	{
		menu = Menu_GetFocused();
		if (menu)
		{
			if (menu->window.flags & WINDOW_POPUP)
			{
				Menu_HandleMouseMove(menu, x, y);
				return qtrue;
			}
		}

		for (int i = 0; i < menuCount; i++)
		{
			Menu_HandleMouseMove(&Menus[i], x, y);
		}
	}
	else
	{
		menu->window.rect.x += x;
		menu->window.rect.y += y;
		Menu_UpdatePosition(menu);
	}
	return qtrue;
}

/*
=================
Menus_AnyFullScreenVisible
=================
*/
qboolean Menus_AnyFullScreenVisible()
{
	for (int i = 0; i < menuCount; i++)
	{
		if (Menus[i].window.flags & WINDOW_VISIBLE && Menus[i].fullScreen)
		{
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
Controls_SetConfig
=================
*/
void Controls_SetConfig()
{
	// iterate each command, get its numeric binding
	for (size_t i = 0; i < g_bindCount; i++)
	{
		if (g_bindKeys[i][0] != -1)
		{
			DC->setBinding(g_bindKeys[i][0], g_bindCommands[i]);

			if (g_bindKeys[i][1] != -1)
				DC->setBinding(g_bindKeys[i][1], g_bindCommands[i]);
		}
	}
}

void Controls_SetDefaults()
{
	for (size_t i = 0; i < g_bindCount; i++)
	{
		g_bindKeys[i][0] = -1;
		g_bindKeys[i][1] = -1;
	}
}

static void Item_Bind_Ungrey(const itemDef_t* item)
{
	const menuDef_t* menu = static_cast<menuDef_t*>(item->parent);
	for (int i = 0; i < menu->itemCount; ++i)
	{
		if (menu->items[i] == item)
		{
			continue;
		}

		menu->items[i]->window.flags &= ~WINDOW_INACTIVE;
	}
}

/*
=================
Item_Bind_HandleKey
=================
*/
static qboolean Item_Bind_HandleKey(itemDef_t* item, const int key, const qboolean down)
{
	int id;
	int i;
	menuDef_t* menu;

	if (key == A_MOUSE1 && Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && !g_waitingForKey)
	{
		if (down)
		{
			g_waitingForKey = qtrue;
			g_bindItem = item;

			// Set all others in the menu to grey
			menu = static_cast<menuDef_t*>(item->parent);
			for (i = 0; i < menu->itemCount; ++i)
			{
				if (menu->items[i] == item)
				{
					continue;
				}
				menu->items[i]->window.flags |= WINDOW_INACTIVE;
			}
		}
		return qtrue;
	}
	if (key == A_ENTER && !g_waitingForKey)
	{
		if (down)
		{
			g_waitingForKey = qtrue;
			g_bindItem = item;

			// Set all others in the menu to grey
			menu = static_cast<menuDef_t*>(item->parent);
			for (i = 0; i < menu->itemCount; ++i)
			{
				if (menu->items[i] == item)
				{
					continue;
				}
				menu->items[i]->window.flags |= WINDOW_INACTIVE;
			}
		}
		return qtrue;
	}
	if (!g_waitingForKey || g_bindItem == nullptr)
	{
		return qfalse;
	}

	if (key & K_CHAR_FLAG)
	{
		return qtrue;
	}

	switch (key)
	{
	case A_ESCAPE:
		g_waitingForKey = qfalse;
		Item_Bind_Ungrey(item);
		return qtrue;

	case A_BACKSPACE:
		id = BindingIDFromName(item->cvar);
		if (id != -1)
		{
			if (g_bindKeys[id][0] != -1)
				DC->setBinding(g_bindKeys[id][0], "");

			if (g_bindKeys[id][1] != -1)
				DC->setBinding(g_bindKeys[id][1], "");

			g_bindKeys[id][0] = -1;
			g_bindKeys[id][1] = -1;
		}
		Controls_SetConfig();
		g_waitingForKey = qfalse;
		g_bindItem = nullptr;
		Item_Bind_Ungrey(item);
		return qtrue;
	case '`':
		return qtrue;
	default:;
	}

	// Is the same key being bound to something else?
	if (key != -1)
	{
		for (size_t b = 0; b < g_bindCount; b++)
		{
			if (g_bindKeys[b][1] == key)
				g_bindKeys[b][1] = -1;

			if (g_bindKeys[b][0] == key)
			{
				g_bindKeys[b][0] = g_bindKeys[b][1];
				g_bindKeys[b][1] = -1;
			}
		}
	}

	id = BindingIDFromName(item->cvar);

	if (id != -1)
	{
		if (key == -1)
		{
			if (g_bindKeys[id][0] != -1)
			{
				DC->setBinding(g_bindKeys[id][0], "");
				g_bindKeys[id][0] = -1;
			}
			if (g_bindKeys[id][1] != -1)
			{
				DC->setBinding(g_bindKeys[id][1], "");
				g_bindKeys[id][1] = -1;
			}
		}
		else if (g_bindKeys[id][0] == -1)
			g_bindKeys[id][0] = key;
		else if (g_bindKeys[id][0] != key && g_bindKeys[id][1] == -1)
			g_bindKeys[id][1] = key;
		else
		{
			DC->setBinding(g_bindKeys[id][0], "");
			DC->setBinding(g_bindKeys[id][1], "");
			g_bindKeys[id][0] = key;
			g_bindKeys[id][1] = -1;
		}
	}

	Controls_SetConfig();
	g_waitingForKey = qfalse;
	Item_Bind_Ungrey(item);

	return qtrue;
}

/*
=================
Menu_SetNextCursorItem
=================
*/
static itemDef_t* Menu_SetNextCursorItem(menuDef_t* menu)
{
	qboolean wrapped = qfalse;
	const int oldCursor = menu->cursorItem;

	if (menu->cursorItem == -1)
	{
		menu->cursorItem = 0;
		wrapped = qtrue;
	}

	while (menu->cursorItem < menu->itemCount)
	{
		menu->cursorItem++;
		if (menu->cursorItem >= menu->itemCount && !wrapped)
		{
			wrapped = qtrue;
			menu->cursorItem = 0;
		}

		if (Item_SetFocus(menu->items[menu->cursorItem], DC->cursorx, DC->cursory))
		{
			Menu_HandleMouseMove(menu, menu->items[menu->cursorItem]->window.rect.x + 1,
				menu->items[menu->cursorItem]->window.rect.y + 1);
			return menu->items[menu->cursorItem];
		}
	}

	menu->cursorItem = oldCursor;
	return nullptr;
}

/*
=================
Menu_SetPrevCursorItem
=================
*/
static itemDef_t* Menu_SetPrevCursorItem(menuDef_t* menu)
{
	qboolean wrapped = qfalse;
	const int oldCursor = menu->cursorItem;

	if (menu->cursorItem < 0)
	{
		menu->cursorItem = menu->itemCount - 1;
		wrapped = qtrue;
	}

	while (menu->cursorItem > -1)
	{
		menu->cursorItem--;
		if (menu->cursorItem < 0)
		{
			if (wrapped)
			{
				break;
			}
			wrapped = qtrue;
			menu->cursorItem = menu->itemCount - 1;
		}

		if (Item_SetFocus(menu->items[menu->cursorItem], DC->cursorx, DC->cursory))
		{
			Menu_HandleMouseMove(menu, menu->items[menu->cursorItem]->window.rect.x + 1,
				menu->items[menu->cursorItem]->window.rect.y + 1);
			return menu->items[menu->cursorItem];
		}
	}
	menu->cursorItem = oldCursor;
	return nullptr;
}

/*
=================
Item_TextField_HandleKey
=================
*/
static qboolean Item_TextField_HandleKey(itemDef_t* item, int key)
{
	const auto editPtr = static_cast<editFieldDef_t*>(item->typeData);

	if (item->cvar)
	{
		itemDef_t* newItem;
		char buff[1024] = {};
		DC->getCVarString(item->cvar, buff, sizeof buff);
		int len = strlen(buff);
		if (editPtr->maxChars && len > editPtr->maxChars)
		{
			len = editPtr->maxChars;
		}

		if (key & K_CHAR_FLAG)
		{
			key &= ~K_CHAR_FLAG;

			if (key == 'h' - 'a' + 1)
			{
				// ctrl-h is backspace
				if (item->cursorPos > 0)
				{
					memmove(&buff[item->cursorPos - 1], &buff[item->cursorPos], len + 1 - item->cursorPos);
					item->cursorPos--;
					if (item->cursorPos < editPtr->paintOffset)
					{
						editPtr->paintOffset--;
					}
				}
				DC->setCVar(item->cvar, buff);
				return qtrue;
			}

			//
			// ignore any non printable chars
			//
			if (key < 32 || !item->cvar)
			{
				return qtrue;
			}

			if (item->type == ITEM_TYPE_NUMERICFIELD)
			{
				if (key < '0' || key > '9')
				{
					return qfalse;
				}
			}

			if (!DC->getOverstrikeMode())
			{
				if (len == MAX_EDITFIELD - 1 || editPtr->maxChars && len >= editPtr->maxChars)
				{
					return qtrue;
				}
				memmove(&buff[item->cursorPos + 1], &buff[item->cursorPos], len + 1 - item->cursorPos);
			}
			else
			{
				if (editPtr->maxChars && item->cursorPos >= editPtr->maxChars)
				{
					return qtrue;
				}
			}

			buff[item->cursorPos] = key;

			DC->setCVar(item->cvar, buff);

			if (item->cursorPos < len + 1)
			{
				item->cursorPos++;
				if (editPtr->maxPaintChars && item->cursorPos > editPtr->maxPaintChars)
				{
					editPtr->paintOffset++;
				}
			}
		}
		else
		{
			if (key == A_DELETE || key == A_KP_PERIOD)
			{
				if (item->cursorPos < len)
				{
					memmove(buff + item->cursorPos, buff + item->cursorPos + 1, len - item->cursorPos);
					DC->setCVar(item->cvar, buff);
				}
				return qtrue;
			}

			if (key == A_CURSOR_RIGHT || key == A_KP_6)
			{
				if (editPtr->maxPaintChars && item->cursorPos >= editPtr->maxPaintChars && item->cursorPos < len)
				{
					item->cursorPos++;
					editPtr->paintOffset++;
					return qtrue;
				}

				if (item->cursorPos < len)
				{
					item->cursorPos++;
				}
				return qtrue;
			}

			if (key == A_CURSOR_LEFT || key == A_KP_4)
			{
				if (item->cursorPos > 0)
				{
					item->cursorPos--;
				}
				if (item->cursorPos < editPtr->paintOffset)
				{
					editPtr->paintOffset--;
				}
				return qtrue;
			}

			if (key == A_HOME || key == A_KP_7)
			{
				item->cursorPos = 0;
				editPtr->paintOffset = 0;
				return qtrue;
			}

			if (key == A_END || key == A_KP_1)
			{
				item->cursorPos = len;
				if (item->cursorPos > editPtr->maxPaintChars)
				{
					editPtr->paintOffset = len - editPtr->maxPaintChars;
				}
				return qtrue;
			}

			if (key == A_INSERT || key == A_KP_0)
			{
				DC->setOverstrikeMode(static_cast<qboolean>(!DC->getOverstrikeMode()));
				return qtrue;
			}
		}

		if (key == A_TAB || key == A_CURSOR_DOWN || key == A_KP_2)
		{
			newItem = Menu_SetNextCursorItem(static_cast<menuDef_t*>(item->parent));
			if (newItem && (newItem->type == ITEM_TYPE_EDITFIELD || newItem->type == ITEM_TYPE_NUMERICFIELD))
			{
				g_editItem = newItem;
			}
		}

		if (key == A_CURSOR_UP || key == A_KP_8)
		{
			newItem = Menu_SetPrevCursorItem(static_cast<menuDef_t*>(item->parent));
			if (newItem && (newItem->type == ITEM_TYPE_EDITFIELD || newItem->type == ITEM_TYPE_NUMERICFIELD))
			{
				g_editItem = newItem;
			}
		}

		if (key == A_ENTER || key == A_KP_ENTER || key == A_ESCAPE)
		{
			return qfalse;
		}

		return qtrue;
	}
	return qfalse;
}

static void Scroll_TextScroll_AutoFunc(void* p)
{
	const auto si = static_cast<scrollInfo_t*>(p);

	if (DC->realTime > si->nextScrollTime)
	{
		// need to scroll which is done by simulating a click to the item
		// this is done a bit sideways as the autoscroll "knows" that the item is a listbox
		// so it calls it directly
		Item_TextScroll_HandleKey(si->item, si->scrollKey, qtrue, qfalse);
		si->nextScrollTime = DC->realTime + si->adjustValue;
	}

	if (DC->realTime > si->nextAdjustTime)
	{
		si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
		if (si->adjustValue > SCROLL_TIME_FLOOR)
		{
			si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
		}
	}
}

static void Scroll_TextScroll_ThumbFunc(void* p)
{
	const auto si = static_cast<scrollInfo_t*>(p);

	const auto scrollPtr = static_cast<textScrollDef_t*>(si->item->typeData);

	if (DC->cursory != si->yStart)
	{
		rectDef_t r{};
		r.x = si->item->window.rect.x + si->item->window.rect.w - SCROLLBAR_SIZE - 1;
		r.y = si->item->window.rect.y + SCROLLBAR_SIZE + 1;
		r.h = si->item->window.rect.h - SCROLLBAR_SIZE * 2 - 2;
		r.w = SCROLLBAR_SIZE;
		const int max = Item_TextScroll_MaxScroll(si->item);
		//
		int pos = (DC->cursory - r.y - SCROLLBAR_SIZE / 2) * max / (r.h - SCROLLBAR_SIZE);
		if (pos < 0)
		{
			pos = 0;
		}
		else if (pos > max)
		{
			pos = max;
		}

		scrollPtr->startPos = pos;
		si->yStart = DC->cursory;
	}

	if (DC->realTime > si->nextScrollTime)
	{
		// need to scroll which is done by simulating a click to the item
		// this is done a bit sideways as the autoscroll "knows" that the item is a listbox
		// so it calls it directly
		Item_TextScroll_HandleKey(si->item, si->scrollKey, qtrue, qfalse);
		si->nextScrollTime = DC->realTime + si->adjustValue;
	}

	if (DC->realTime > si->nextAdjustTime)
	{
		si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
		if (si->adjustValue > SCROLL_TIME_FLOOR)
		{
			si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
		}
	}
}

/*
=================
Menu_OverActiveItem
=================
*/
static qboolean Menu_OverActiveItem(menuDef_t* menu, const float x, const float y)
{
	if (menu && menu->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED))
	{
		if (Rect_ContainsPoint(&menu->window.rect, x, y))
		{
			for (int i = 0; i < menu->itemCount; i++)
			{
				// turn off focus each item
				// menu->items[i].window.flags &= ~WINDOW_HASFOCUS;

				if (!(menu->items[i]->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED)))
				{
					continue;
				}

				if (menu->items[i]->window.flags & WINDOW_DECORATION)
				{
					continue;
				}

				if (Rect_ContainsPoint(&menu->items[i]->window.rect, x, y))
				{
					const itemDef_t* overItem = menu->items[i];
					if (overItem->type == ITEM_TYPE_TEXT && overItem->text)
					{
						if (Rect_ContainsPoint(&overItem->window.rect, x, y))
						{
							return qtrue;
						}
						continue;
					}
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
=================
Display_VisibleMenuCount
=================
*/
int Display_VisibleMenuCount()
{
	int count = 0;
	for (int i = 0; i < menuCount; i++)
	{
		if (Menus[i].window.flags & (WINDOW_FORCED | WINDOW_VISIBLE))
		{
			count++;
		}
	}
	return count;
}

/*
=================
Window_CloseCinematic
=================
*/
static void Window_CloseCinematic(windowDef_t* window)
{
	if (window->style == WINDOW_STYLE_CINEMATIC && window->cinematic >= 0)
	{
		DC->stopCinematic(window->cinematic);
		window->cinematic = -1;
	}
}

/*
=================
Menu_CloseCinematics
=================
*/
static void Menu_CloseCinematics(menuDef_t* menu)
{
	if (menu)
	{
		Window_CloseCinematic(&menu->window);
		for (int i = 0; i < menu->itemCount; i++)
		{
			Window_CloseCinematic(&menu->items[i]->window);
			if (menu->items[i]->type == ITEM_TYPE_OWNERDRAW)
			{
				DC->stopCinematic(0 - menu->items[i]->window.ownerDraw);
			}
		}
	}
}

/*
=================
Display_CloseCinematics
=================
*/
static void Display_CloseCinematics()
{
	for (int i = 0; i < menuCount; i++)
	{
		Menu_CloseCinematics(&Menus[i]);
	}
}

/*
=================
Menus_HandleOOBClick
=================
*/
static void Menus_HandleOOBClick(menuDef_t* menu, const int key, const qboolean down)
{
	if (menu)
	{
		// basically the behaviour we are looking for is if there are windows in the stack.. see if
		// the cursor is within any of them.. if not close them otherwise activate them and pass the
		// key on.. force a mouse move to activate focus and script stuff
		if (down && menu->window.flags & WINDOW_OOB_CLICK)
		{
			Menu_RunCloseScript(menu);
			menu->window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE);
		}

		for (int i = 0; i < menuCount; i++)
		{
			if (Menu_OverActiveItem(&Menus[i], DC->cursorx, DC->cursory))
			{
				Menu_RunCloseScript(menu);
				menu->window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE);
				//	Menus_Activate(&Menus[i]);
				Menu_HandleMouseMove(&Menus[i], DC->cursorx, DC->cursory);
				Menu_HandleKey(&Menus[i], key, down);
			}
		}

		if (Display_VisibleMenuCount() == 0)
		{
			if (DC->Pause)
			{
				DC->Pause(qfalse);
			}
		}
		Display_CloseCinematics();
	}
}

/*
=================
Item_StopCapture
=================
*/
static void Item_StopCapture(itemDef_t* item)
{
}

/*
=================
Item_ListBox_HandleKey
=================
*/
static qboolean Item_ListBox_HandleKey(itemDef_t* item, const int key, qboolean down, const qboolean force)
{
	const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
	const int count = DC->feederCount(item->special);
	if (force || Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags &
		WINDOW_HASFOCUS)
	{
		int viewmax;
		const int max = Item_ListBox_MaxScroll(item);
		if (item->window.flags & WINDOW_HORIZONTAL)
		{
			viewmax = item->window.rect.w / listPtr->elementWidth;
#ifdef NEW_FEEDER_V1
			if (key == A_CURSOR_LEFT || key == A_KP_4 || key == A_MWHEELUP)
#else
			if (key == A_CURSOR_LEFT || key == A_KP_4)
#endif
			{
				if (!listPtr->notselectable)
				{
					listPtr->cursorPos--;
					if (listPtr->cursorPos < 0)
					{
						listPtr->cursorPos = 0;
					}
					if (listPtr->cursorPos < listPtr->startPos)
					{
						listPtr->startPos = listPtr->cursorPos;
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax)
					{
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;

					DC->feederSelection(item->special, item->cursorPos, item);
				}
				else
				{
					listPtr->startPos--;
					if (listPtr->startPos < 0)
					{
						listPtr->startPos = 0;
					}
				}
				return qtrue;
			}
#ifdef NEW_FEEDER_V1
			if (key == A_CURSOR_RIGHT || key == A_KP_6 || key == A_MWHEELDOWN)
#else
			if (key == A_CURSOR_RIGHT || key == A_KP_6)
#endif
			{
				if (!listPtr->notselectable)
				{
					listPtr->cursorPos++;

					if (listPtr->cursorPos < listPtr->startPos)
					{
						listPtr->startPos = listPtr->cursorPos;
					}
					if (listPtr->cursorPos >= count)
					{
						listPtr->cursorPos = count - 1;
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax)
					{
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;

					DC->feederSelection(item->special, item->cursorPos, item);
				}
				else
				{
					listPtr->startPos++;

					if (listPtr->startPos >= count)
					{
						listPtr->startPos = count - 1;
					}
				}
				return qtrue;
			}
		}
		else
		{
#ifdef NEW_FEEDER_V3
			// Multiple rows and columns (since it's more than twice as wide as an element)
			if ((item->window.rect.w > (listPtr->elementWidth * 2)) && (listPtr->elementStyle == LISTBOX_IMAGE)) {
				viewmax = (item->window.rect.w / listPtr->elementWidth);
			} else {
				viewmax = (item->window.rect.h / listPtr->elementHeight);
			}
#else
			viewmax = item->window.rect.h / listPtr->elementHeight;
#endif
			if (key == A_CURSOR_UP || key == A_KP_8)
			{
				if (!listPtr->notselectable)
				{
					listPtr->cursorPos--;

					if (listPtr->cursorPos < 0)
					{
						listPtr->cursorPos = 0;
#ifdef NEW_FEEDER_V3
						return qfalse;
#endif
					}
					if (listPtr->cursorPos < listPtr->startPos)
					{
						listPtr->startPos = listPtr->cursorPos;
#ifdef NEW_FEEDER_V3
						return qfalse;
#endif
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax)
					{
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;

					DC->feederSelection(item->special, item->cursorPos, item);
				}
				else
				{
					listPtr->startPos--;

					if (listPtr->startPos < 0)
					{
						listPtr->startPos = 0;
					}
				}
				return qtrue;
			}
			if (key == A_CURSOR_DOWN || key == A_KP_2)
			{
				if (!listPtr->notselectable)
				{
					listPtr->cursorPos++;
					if (listPtr->cursorPos < listPtr->startPos)
					{
						listPtr->startPos = listPtr->cursorPos;
#ifdef NEW_FEEDER_V3
						return qfalse;
#endif
					}
					if (listPtr->cursorPos >= count)
					{
						listPtr->cursorPos = count - 1;
#ifdef NEW_FEEDER_V3
						return qfalse;
#endif
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax)
					{
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;

					DC->feederSelection(item->special, item->cursorPos, item);
				}
				else
				{
					listPtr->startPos++;

					if (listPtr->startPos > max)
					{
						listPtr->startPos = max;
					}
				}
				return qtrue;
			}

			//Raz: Added
			if (key == A_MWHEELUP)
			{
#ifdef NEW_FEEDER_V3
				listPtr->startPos -= static_cast<int>(item->special) == FEEDER_MD_MODELS ? viewmax : 1;
#else
				listPtr->startPos -= static_cast<int>(item->special) == FEEDER_Q3HEADS ? viewmax : 1;
#endif
				if (listPtr->startPos < 0)
				{
					listPtr->startPos = 0;
					Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
					return qfalse;
				}
				Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
				return qtrue;
			}
			if (key == A_MWHEELDOWN)
			{
#ifdef NEW_FEEDER_V3
#ifdef NEW_FEEDER_V4
				int lengthMax = (item->window.rect.h / listPtr->elementHeight);
				int viewChunkSize = lengthMax * viewmax;
				int removeRemaining = count % viewmax;

				if (count <= viewChunkSize) // Entire List fits in chunk, prevent scrolling all together
					return qfalse;

				listPtr->startPos += static_cast<int>(item->special) == FEEDER_MD_MODELS ? viewmax : 1;
				int maxStartPos = (count > viewmax) ? (count - viewmax) : 0;
				if (listPtr->startPos > maxStartPos - removeRemaining) {
					listPtr->startPos = maxStartPos - removeRemaining;
					Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
					return qfalse;
				}
#else
				listPtr->startPos += static_cast<int>(item->special) == FEEDER_MD_MODELS ? viewmax : 1;
				int maxStartPos = (count > viewmax) ? (count - viewmax) : 0;
				if (listPtr->startPos > maxStartPos)
				{
					listPtr->startPos = maxStartPos;
					Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
					return qfalse;
				}
#endif
#else
				listPtr->startPos += static_cast<int>(item->special) == FEEDER_Q3HEADS ? viewmax : 1;

				if (listPtr->startPos > max)
				{
					listPtr->startPos = max;
					Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
					return qfalse;
				}
#endif
				Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
				return qtrue;
			}
		}
		// mouse hit
		if (key == A_MOUSE1 || key == A_MOUSE2)
		{
			if (item->window.flags & WINDOW_LB_LEFTARROW)
			{
				listPtr->startPos--;
				if (listPtr->startPos < 0)
				{
					listPtr->startPos = 0;
				}
			}
			else if (item->window.flags & WINDOW_LB_RIGHTARROW)
			{
				// one down
				listPtr->startPos++;
				if (listPtr->startPos > max)
				{
					listPtr->startPos = max;
				}
			}
			else if (item->window.flags & WINDOW_LB_PGUP)
			{
				// page up
				listPtr->startPos -= viewmax;
				if (listPtr->startPos < 0)
				{
					listPtr->startPos = 0;
				}
			}
			else if (item->window.flags & WINDOW_LB_PGDN)
			{
				// page down
				listPtr->startPos += viewmax;
				if (listPtr->startPos > max)
				{
					listPtr->startPos = max;
				}
			}
			else if (item->window.flags & WINDOW_LB_THUMB)
			{
				// Display_SetCaptureItem(item);
			}
			else
			{
				// select an item
				if (DC->realTime < lastListBoxClickTime && listPtr->doubleClick)
				{
					Item_RunScript(item, listPtr->doubleClick);
				}
				lastListBoxClickTime = DC->realTime + DOUBLE_CLICK_DELAY;
				item->cursorPos = listPtr->cursorPos;
				DC->feederSelection(item->special, item->cursorPos, item);
			}
			return qtrue;
		}
		if (key == A_HOME || key == A_KP_7)
		{
			// home
			listPtr->startPos = 0;
			return qtrue;
		}
		if (key == A_END || key == A_KP_1)
		{
			// end
			listPtr->startPos = max;
			return qtrue;
		}
		if (key == A_PAGE_UP || key == A_KP_9)
		{
			// page up
			if (!listPtr->notselectable)
			{
				listPtr->cursorPos -= viewmax;
				if (listPtr->cursorPos < 0)
				{
					listPtr->cursorPos = 0;
				}
				if (listPtr->cursorPos < listPtr->startPos)
				{
					listPtr->startPos = listPtr->cursorPos;
				}
				if (listPtr->cursorPos >= listPtr->startPos + viewmax)
				{
					listPtr->startPos = listPtr->cursorPos - viewmax + 1;
				}
				item->cursorPos = listPtr->cursorPos;
				DC->feederSelection(item->special, item->cursorPos, item);
			}
			else
			{
				listPtr->startPos -= viewmax;
				if (listPtr->startPos < 0)
				{
					listPtr->startPos = 0;
				}
			}
			return qtrue;
		}
		if (key == A_PAGE_DOWN || key == A_KP_3)
		{
			// page down
			if (!listPtr->notselectable)
			{
				listPtr->cursorPos += viewmax;
				if (listPtr->cursorPos < listPtr->startPos)
				{
					listPtr->startPos = listPtr->cursorPos;
				}
				if (listPtr->cursorPos >= count)
				{
					listPtr->cursorPos = count - 1;
				}
				if (listPtr->cursorPos >= listPtr->startPos + viewmax)
				{
					listPtr->startPos = listPtr->cursorPos - viewmax + 1;
				}
				item->cursorPos = listPtr->cursorPos;
				DC->feederSelection(item->special, item->cursorPos, item);
			}
			else
			{
				listPtr->startPos += viewmax;
				if (listPtr->startPos > max)
				{
					listPtr->startPos = max;
				}
			}
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
Scroll_ListBox_AutoFunc
=================
*/
static void Scroll_ListBox_AutoFunc(void* p)
{
	const auto si = static_cast<scrollInfo_t*>(p);
	if (DC->realTime > si->nextScrollTime)
	{
		// need to scroll which is done by simulating a click to the item
		// this is done a bit sideways as the autoscroll "knows" that the item is a listbox
		// so it calls it directly
		Item_ListBox_HandleKey(si->item, si->scrollKey, qtrue, qfalse);
		si->nextScrollTime = DC->realTime + si->adjustValue;
	}

	if (DC->realTime > si->nextAdjustTime)
	{
		si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
		if (si->adjustValue > SCROLL_TIME_FLOOR)
		{
			si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
		}
	}
}

/*
=================
Scroll_ListBox_ThumbFunc
=================
*/
static void Scroll_ListBox_ThumbFunc(void* p)
{
	const auto si = static_cast<scrollInfo_t*>(p);
	rectDef_t r{};
	int pos, max;

	const auto listPtr = static_cast<listBoxDef_t*>(si->item->typeData);
	if (si->item->window.flags & WINDOW_HORIZONTAL)
	{
		if (DC->cursorx == si->xStart)
		{
			return;
		}
		r.x = si->item->window.rect.x + SCROLLBAR_SIZE + 1;
		r.y = si->item->window.rect.y + si->item->window.rect.h - SCROLLBAR_SIZE - 1;
		r.h = SCROLLBAR_SIZE;
		r.w = si->item->window.rect.w - SCROLLBAR_SIZE * 2 - 2;
		max = Item_ListBox_MaxScroll(si->item);
		//
		pos = (DC->cursorx - r.x - SCROLLBAR_SIZE / 2) * max / (r.w - SCROLLBAR_SIZE);
		if (pos < 0)
		{
			pos = 0;
		}
		else if (pos > max)
		{
			pos = max;
		}
		listPtr->startPos = pos;
		si->xStart = DC->cursorx;
	}
	else if (DC->cursory != si->yStart)
	{
		r.x = si->item->window.rect.x + si->item->window.rect.w - SCROLLBAR_SIZE - 1;
		r.y = si->item->window.rect.y + SCROLLBAR_SIZE + 1;
		r.h = si->item->window.rect.h - SCROLLBAR_SIZE * 2 - 2;
		r.w = SCROLLBAR_SIZE;
		max = Item_ListBox_MaxScroll(si->item);
		//
		pos = (DC->cursory - r.y - SCROLLBAR_SIZE / 2) * max / (r.h - SCROLLBAR_SIZE);
		if (pos < 0)
		{
			pos = 0;
		}
		else if (pos > max)
		{
			pos = max;
		}
		listPtr->startPos = pos;
		si->yStart = DC->cursory;
	}

	if (DC->realTime > si->nextScrollTime)
	{
		// need to scroll which is done by simulating a click to the item
		// this is done a bit sideways as the autoscroll "knows" that the item is a listbox
		// so it calls it directly
		Item_ListBox_HandleKey(si->item, si->scrollKey, qtrue, qfalse);
		si->nextScrollTime = DC->realTime + si->adjustValue;
	}

	if (DC->realTime > si->nextAdjustTime)
	{
		si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
		if (si->adjustValue > SCROLL_TIME_FLOOR)
		{
			si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
		}
	}
}

/*
=================
Item_Slider_OverSlider
=================
*/
static int Item_Slider_OverSlider(itemDef_t* item, const float x, const float y)
{
	rectDef_t r{};

	r.x = Item_Slider_ThumbPosition(item) - SLIDER_THUMB_WIDTH / 2;
	r.y = item->window.rect.y - 2;
	r.w = SLIDER_THUMB_WIDTH;
	r.h = SLIDER_THUMB_HEIGHT;

	if (Rect_ContainsPoint(&r, x, y))
	{
		return WINDOW_LB_THUMB;
	}
	return 0;
}

/*
 =================
 Scroll_Slider_Integer_ThumbFunc
 =================
 */
static void Scroll_Slider_Integer_ThumbFunc(void* p)
{
	float x;
	const scrollInfo_t* si = static_cast<scrollInfo_t*>(p);
	const editFieldDef_t* editDef = static_cast<editFieldDef_s*>(si->item->typeData);

	if (si->item->text)
	{
		x = si->item->textRect.x + si->item->textRect.w + 8;
	}
	else
	{
		x = si->item->window.rect.x;
	}

	float cursorx = DC->cursorx;

	if (cursorx < x)
	{
		cursorx = x;
	}
	else if (cursorx > x + SLIDER_WIDTH)
	{
		cursorx = x + SLIDER_WIDTH;
	}
	float value = cursorx - x;
	value /= SLIDER_WIDTH;
	value *= editDef->maxVal - editDef->minVal;
	value += editDef->minVal;
	const int intValue = static_cast<int>(value);
	DC->setCVar(si->item->cvar, va("%d", intValue));
}

/*
=================
Scroll_Slider_ThumbFunc
=================
*/
static void Scroll_Slider_ThumbFunc(void* p)
{
	float x;
	const scrollInfo_t* si = static_cast<scrollInfo_t*>(p);
	const editFieldDef_t* editDef = static_cast<editFieldDef_s*>(si->item->typeData);

	if (si->item->text)
	{
		x = si->item->textRect.x + si->item->textRect.w + 8;
	}
	else
	{
		x = si->item->window.rect.x;
	}

	float cursorx = DC->cursorx;

	if (cursorx < x)
	{
		cursorx = x;
	}
	else if (cursorx > x + SLIDER_WIDTH)
	{
		cursorx = x + SLIDER_WIDTH;
	}
	float value = cursorx - x;
	value /= SLIDER_WIDTH;
	value *= editDef->maxVal - editDef->minVal;
	value += editDef->minVal;
	DC->setCVar(si->item->cvar, va("%f", value));
}

/*
 =================
 Scroll_Rotate
 =================
 */
static void Scroll_Rotate(void* p)
{
	//This maps a scroll to a rotation. That rotation is then added to the current cvar value.
	//It reads off editDef->range to give the correct angle range for the item area.
	float start, size, cursorpos, cursorpos_old;
	const scrollInfo_t* si = static_cast<scrollInfo_t*>(p);
	const editFieldDef_t* editDef = static_cast<editFieldDef_s*>(si->item->typeData);

	const auto useYAxis = static_cast<qboolean>(si->item->flags & ITF_ISANYSABER && !(si->item->flags &
		ITF_ISCHARACTER));

	if (useYAxis)
	{
		start = si->item->window.rect.y;
		size = si->item->window.rect.h;

		cursorpos = DC->cursory;
		cursorpos_old = si->yStart;
	}
	else
	{
		start = si->item->window.rect.x;
		size = si->item->window.rect.w;

		cursorpos = DC->cursorx;
		cursorpos_old = si->xStart;
	}

	if (cursorpos < start)
	{
		cursorpos = start;
	}
	else if (cursorpos > start + size)
	{
		cursorpos = start + size;
	}
	//moving across the whole model area should allow for 720 degree rotation
	const float angleDiff = editDef->range * (cursorpos - cursorpos_old) / size;
	const int intValue = static_cast<int>(si->adjustValue + angleDiff) % 360;
	DC->setCVar(si->item->cvar, va("%d", intValue));
}

/*
=================
Item_StartCapture
=================
*/
static void Item_StartCapture(itemDef_t* item, const int key)
{
	int flags;
	switch (item->type)
	{
	case ITEM_TYPE_EDITFIELD:
	case ITEM_TYPE_NUMERICFIELD:

	case ITEM_TYPE_LISTBOX:
	{
		flags = Item_ListBox_OverLB(item, DC->cursorx, DC->cursory);
		if (flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW))
		{
			scrollInfo.nextScrollTime = DC->realTime + SCROLL_TIME_START;
			scrollInfo.nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
			scrollInfo.adjustValue = SCROLL_TIME_START;
			scrollInfo.scrollKey = key;
			scrollInfo.scrollDir = flags & WINDOW_LB_LEFTARROW ? qtrue : qfalse;
			scrollInfo.item = item;
			captureData = &scrollInfo;
			captureFunc = &Scroll_ListBox_AutoFunc;
			itemCapture = item;
		}
		else if (flags & WINDOW_LB_THUMB)
		{
			scrollInfo.scrollKey = key;
			scrollInfo.item = item;
			scrollInfo.xStart = DC->cursorx;
			scrollInfo.yStart = DC->cursory;
			captureData = &scrollInfo;
			captureFunc = &Scroll_ListBox_ThumbFunc;
			itemCapture = item;
		}
		break;
	}

	case ITEM_TYPE_TEXTSCROLL:
		flags = Item_TextScroll_OverLB(item, DC->cursorx, DC->cursory);
		if (flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW))
		{
			scrollInfo.nextScrollTime = DC->realTime + SCROLL_TIME_START;
			scrollInfo.nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
			scrollInfo.adjustValue = SCROLL_TIME_START;
			scrollInfo.scrollKey = key;
			scrollInfo.scrollDir = flags & WINDOW_LB_LEFTARROW ? qtrue : qfalse;
			scrollInfo.item = item;
			captureData = &scrollInfo;
			captureFunc = &Scroll_TextScroll_AutoFunc;
			itemCapture = item;
		}
		else if (flags & WINDOW_LB_THUMB)
		{
			scrollInfo.scrollKey = key;
			scrollInfo.item = item;
			scrollInfo.xStart = DC->cursorx;
			scrollInfo.yStart = DC->cursory;
			captureData = &scrollInfo;
			captureFunc = &Scroll_TextScroll_ThumbFunc;
			itemCapture = item;
		}
		break;

	case ITEM_TYPE_SLIDER:
	{
		flags = Item_Slider_OverSlider(item, DC->cursorx, DC->cursory);
		if (flags & WINDOW_LB_THUMB)
		{
			scrollInfo.scrollKey = key;
			scrollInfo.item = item;
			scrollInfo.xStart = DC->cursorx;
			scrollInfo.yStart = DC->cursory;
			captureData = &scrollInfo;
			captureFunc = &Scroll_Slider_ThumbFunc;
			itemCapture = item;
		}
		break;
	}
	case ITEM_TYPE_SLIDER_INTEGER:
	{
		flags = Item_Slider_OverSlider(item, DC->cursorx, DC->cursory);
		if (flags & WINDOW_LB_THUMB)
		{
			scrollInfo.scrollKey = key;
			scrollInfo.item = item;
			scrollInfo.xStart = DC->cursorx;
			scrollInfo.yStart = DC->cursory;
			captureData = &scrollInfo;
			captureFunc = &Scroll_Slider_Integer_ThumbFunc;
			itemCapture = item;
		}
		break;
	}
	case ITEM_TYPE_SLIDER_ROTATE:
	{
		if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory))
		{
			scrollInfo.scrollKey = key;
			scrollInfo.item = item;
			scrollInfo.xStart = DC->cursorx;
			scrollInfo.yStart = DC->cursory;
			scrollInfo.adjustValue = static_cast<int>(DC->getCVarValue(item->cvar));
			captureData = &scrollInfo;
			captureFunc = &Scroll_Rotate;
			itemCapture = item;
		}
		break;
	}
	default:;
	}
}

/*
=================
Item_YesNo_HandleKey
=================
*/
static qboolean Item_YesNo_HandleKey(itemDef_t* item, const int key)
{
	if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags & WINDOW_HASFOCUS && item
		->cvar)
	{
		if (key == A_MOUSE1 || key == A_ENTER || key == A_MOUSE2 || key == A_MOUSE3)
		{
			DC->setCVar(item->cvar, va("%i", !DC->getCVarValue(item->cvar)));
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
Item_Multi_FindCvarByValue
=================
*/
static int Item_Multi_FindCvarByValue(const itemDef_t* item)
{
	const multiDef_t* multiPtr = static_cast<multiDef_t*>(item->typeData);
	if (multiPtr)
	{
		float value = 0;
		char buff[1024];
		if (multiPtr->strDef)
		{
			DC->getCVarString(item->cvar, buff, sizeof buff);
		}
		else
		{
			value = DC->getCVarValue(item->cvar);
		}
		for (int i = 0; i < multiPtr->count; i++)
		{
			if (multiPtr->strDef)
			{
				if (Q_stricmp(buff, multiPtr->cvarStr[i]) == 0)
				{
					return i;
				}
			}
			else
			{
				if (multiPtr->cvarValue[i] == value)
				{
					return i;
				}
			}
		}
	}
	return 0;
}

/*
=================
Item_Multi_CountSettings
=================
*/
static int Item_Multi_CountSettings(const itemDef_t* item)
{
	const multiDef_t* multiPtr = static_cast<multiDef_t*>(item->typeData);
	if (multiPtr == nullptr)
	{
		return 0;
	}
	return multiPtr->count;
}

/*
=================
Item_OwnerDraw_HandleKey
=================
*/
static qboolean Item_OwnerDraw_HandleKey(itemDef_t* item, const int key)
{
	if (item && DC->ownerDrawHandleKey)
	{
		return DC->ownerDrawHandleKey(item->window.ownerDraw, item->window.ownerDrawFlags, &item->special, key);
	}
	return qfalse;
}

/*
=================
Item_Text_HandleKey
=================
*/
static qboolean Item_Text_HandleKey(itemDef_t* item, const int key)
{
	if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags & WINDOW_AUTOWRAPPED)

	{
		if (key == A_MOUSE1 || key == A_ENTER || key == A_MOUSE2 || key == A_MOUSE3)
		{
			if (key == A_MOUSE2)
			{
				item->cursorPos--;
				if (item->cursorPos < 0)
				{
					item->cursorPos = 0;
				}
			}
			else if (item->special)
			{
				item->cursorPos++;
				if (item->cursorPos >= item->value)
				{
					item->cursorPos = 0;
				}
			}
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
Item_Multi_HandleKey
=================
*/
static qboolean Item_Multi_HandleKey(itemDef_t* item, const int key)
{
	const multiDef_t* multiPtr = static_cast<multiDef_t*>(item->typeData);
	if (multiPtr)
	{
		if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags & WINDOW_HASFOCUS)
		{
			//Raz: Scroll on multi buttons!
			if (key == A_MOUSE1 || key == A_ENTER || key == A_MOUSE2 || key == A_MOUSE3 || key == A_MWHEELDOWN || key ==
				A_MWHEELUP)
				//if (key == A_MOUSE1 || key == A_ENTER || key == A_MOUSE2 || key == A_MOUSE3)
			{
				if (item->cvar)
				{
					int current = Item_Multi_FindCvarByValue(item);
					const int max = Item_Multi_CountSettings(item);

					if (key == A_MOUSE2 || key == A_MWHEELDOWN)
					{
						current--;
						if (current < 0)
						{
							current = max - 1;
						}
					}
					else
					{
						current++;
						if (current >= max)
						{
							current = 0;
						}
					}

					if (multiPtr->strDef)
					{
						DC->setCVar(item->cvar, multiPtr->cvarStr[current]);
					}
					else
					{
						const float value = multiPtr->cvarValue[current];
						if (static_cast<float>(static_cast<int>(value)) == value)
						{
							DC->setCVar(item->cvar, va("%i", static_cast<int>(value)));
						}
						else
						{
							DC->setCVar(item->cvar, va("%f", value));
						}
					}
					if (item->special)
					{
						//its a feeder?
						DC->feederSelection(item->special, current, item);
					}
				}
				else
				{
					const int max = Item_Multi_CountSettings(item);

					if (key == A_MOUSE2 || key == A_MWHEELDOWN)
					{
						item->value--;
						if (item->value < 0)
						{
							item->value = max;
						}
					}
					else
					{
						item->value++;
						if (item->value >= max)
						{
							item->value = 0;
						}
					}
					if (item->special)
					{
						//its a feeder?
						DC->feederSelection(item->special, item->value, item);
					}
				}

				return qtrue;
			}
		}
	}
	return qfalse;
}

/*
=================
Item_Slider_HandleKey
=================
*/
static qboolean Item_Slider_HandleKey(itemDef_t* item, const int key, qboolean down)
{
	//DC->Print("slider handle key\n");
	//JLF MPMOVED

	if (item->window.flags & WINDOW_HASFOCUS && item->cvar && Rect_ContainsPoint(
		&item->window.rect, DC->cursorx, DC->cursory))
	{
		if (key == A_MOUSE1 || key == A_ENTER || key == A_MOUSE2 || key == A_MOUSE3)
		{
			const editFieldDef_t* editDef = static_cast<editFieldDef_s*>(item->typeData);
			if (editDef)
			{
				float x;
				rectDef_t testRect;
				constexpr float width = SLIDER_WIDTH;
				if (item->text)
				{
					x = item->textRect.x + item->textRect.w + 8;
				}
				else
				{
					x = item->window.rect.x;
				}

				testRect = item->window.rect;
				testRect.x = x;
				float value = static_cast<float>(SLIDER_THUMB_WIDTH) / 2;
				testRect.x -= value;
				//DC->Print("slider x: %f\n", testRect.x);
				testRect.w = SLIDER_WIDTH + static_cast<float>(SLIDER_THUMB_WIDTH) / 2;
				//DC->Print("slider w: %f\n", testRect.w);
				if (Rect_ContainsPoint(&testRect, DC->cursorx, DC->cursory))
				{
					const float work = DC->cursorx - x;
					value = work / width;
					value *= editDef->maxVal - editDef->minVal;
					// vm fuckage
					// value = (((float)(DC->cursorx - x)/ SLIDER_WIDTH) * (editDef->maxVal - editDef->minVal));
					value += editDef->minVal;
					DC->setCVar(item->cvar, va("%f", value));
					return qtrue;
				}
			}
		}
	}

	//DC->Print("slider handle key exit\n");
	return qfalse;
}

/*
 =================
 Item_Slider_HandleKey
 =================
 */
static qboolean Item_Slider_Integer_HandleKey(itemDef_t* item, const int key, qboolean down)
{
	if (item->window.flags & WINDOW_HASFOCUS && item->cvar && Rect_ContainsPoint(
		&item->window.rect, DC->cursorx, DC->cursory))
	{
		if (key == A_MOUSE1 || key == A_ENTER || key == A_MOUSE2 || key == A_MOUSE3)
		{
			const editFieldDef_t* editDef = static_cast<editFieldDef_s*>(item->typeData);
			if (editDef)
			{
				float x;
				rectDef_t testRect;
				constexpr float width = SLIDER_WIDTH;
				if (item->text)
				{
					x = item->textRect.x + item->textRect.w + 8;
				}
				else
				{
					x = item->window.rect.x;
				}

				testRect = item->window.rect;
				testRect.x = x;
				float value = static_cast<float>(SLIDER_THUMB_WIDTH) / 2;
				testRect.x -= value;
				//DC->Print("slider x: %f\n", testRect.x);
				testRect.w = SLIDER_WIDTH + static_cast<float>(SLIDER_THUMB_WIDTH) / 2;
				//DC->Print("slider w: %f\n", testRect.w);
				if (Rect_ContainsPoint(&testRect, DC->cursorx, DC->cursory))
				{
					const float work = DC->cursorx - x;
					value = work / width;
					value *= editDef->maxVal - editDef->minVal;
					// vm fuckage
					// value = (((float)(DC->cursorx - x)/ SLIDER_WIDTH) * (editDef->maxVal - editDef->minVal));
					value += editDef->minVal;
					const int intValue = static_cast<int>(value);
					DC->setCVar(item->cvar, va("%d", intValue));
					return qtrue;
				}
			}
		}
	}

	//DC->Print("slider handle key exit\n");
	return qfalse;
}

/*
=================
Item_HandleKey
=================
*/
static qboolean Item_HandleKey(itemDef_t* item, const int key, const qboolean down)
{
	if (itemCapture)
	{
		Item_StopCapture(itemCapture);
		itemCapture = nullptr;
		captureFunc = nullptr;
		captureData = nullptr;
	}
	else
	{
		if (down && (key == A_MOUSE1 || key == A_MOUSE2 || key == A_MOUSE3))
		{
			Item_StartCapture(item, key);
		}
	}

	if (!down)
	{
		return qfalse;
	}

	switch (item->type)
	{
	case ITEM_TYPE_BUTTON:
		return qfalse;
	case ITEM_TYPE_RADIOBUTTON:
		return qfalse;
	case ITEM_TYPE_CHECKBOX:
		return qfalse;
	case ITEM_TYPE_EDITFIELD:
	case ITEM_TYPE_NUMERICFIELD:
		//return Item_TextField_HandleKey(item, key);
		return qfalse;
	case ITEM_TYPE_COMBO:
		return qfalse;
	case ITEM_TYPE_LISTBOX:
		return Item_ListBox_HandleKey(item, key, down, qfalse);
	case ITEM_TYPE_TEXTSCROLL:
		return Item_TextScroll_HandleKey(item, key, down, qfalse);
	case ITEM_TYPE_YESNO:
		return Item_YesNo_HandleKey(item, key);
	case ITEM_TYPE_MULTI:
		return Item_Multi_HandleKey(item, key);
	case ITEM_TYPE_OWNERDRAW:
		return Item_OwnerDraw_HandleKey(item, key);
	case ITEM_TYPE_BIND:
		return Item_Bind_HandleKey(item, key, down);
	case ITEM_TYPE_SLIDER:
		return Item_Slider_HandleKey(item, key, down);
	case ITEM_TYPE_SLIDER_INTEGER:
		return Item_Slider_Integer_HandleKey(item, key, down);
	case ITEM_TYPE_SLIDER_ROTATE:
		//don't bother with this!
		return qfalse;
	case ITEM_TYPE_TEXT:
		return Item_Text_HandleKey(item, key);
	default:
		return qfalse;
	}

	//return qfalse;
}

//JLFACCEPT MPMOVED
/*
-----------------------------------------
Item_HandleAccept
	If Item has an accept script, run it.
-------------------------------------------
*/
static qboolean Item_HandleAccept(itemDef_t* item)
{
	if (item->accept)
	{
		Item_RunScript(item, item->accept);
		return qtrue;
	}
	return qfalse;
}

//JLFDPADSCRIPT MPMOVED
/*
-----------------------------------------
Item_HandleSelectionNext
	If Item has an selectionNext script, run it.
-------------------------------------------
*/
static qboolean Item_HandleSelectionNext(itemDef_t* item)
{
	if (item->selectionNext)
	{
		Item_RunScript(item, item->selectionNext);
		return qtrue;
	}
	return qfalse;
}

//JLFDPADSCRIPT MPMOVED
/*
-----------------------------------------
Item_HandleSelectionPrev
	If Item has an selectionPrev script, run it.
-------------------------------------------
*/
static qboolean Item_HandleSelectionPrev(itemDef_t* item)
{
	if (item->selectionPrev)
	{
		Item_RunScript(item, item->selectionPrev);
		return qtrue;
	}
	return qfalse;
}

/*
=================
Item_Action
=================
*/
static void Item_Action(itemDef_t* item)
{
	if (item)
	{
		Item_RunScript(item, item->action);
	}
}

/*
=================
Menu_HandleKey
=================
*/
void Menu_HandleKey(menuDef_t* menu, int key, qboolean down)
{
	int i;
	itemDef_t* item = nullptr;
	qboolean inHandler = qfalse;

	if (inHandler)
	{
		return;
	}

	inHandler = qtrue;
	if (g_waitingForKey && down)
	{
		Item_Bind_HandleKey(g_bindItem, key, down);
		inHandler = qfalse;
		return;
	}

	if (g_editingField && down)
	{
		if (!Item_TextField_HandleKey(g_editItem, key))
		{
			g_editingField = qfalse;
			g_editItem = nullptr;
			inHandler = qfalse;
			return;
		}
		if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_MOUSE3)
		{
			g_editingField = qfalse;
			g_editItem = nullptr;
			Display_MouseMove(nullptr, DC->cursorx, DC->cursory);
		}
		else if (key == A_TAB || key == A_CURSOR_UP || key == A_CURSOR_DOWN)
		{
			return;
		}
	}

	if (menu == nullptr)
	{
		inHandler = qfalse;
		return;
	}

	//JLFMOUSE  MPMOVED
	// see if the mouse is within the window bounds and if so is this a mouse click
	if (down && !(menu->window.flags & WINDOW_POPUP) && !Rect_ContainsPoint(
		&menu->window.rect, DC->cursorx, DC->cursory))
	{
		static qboolean inHandleKey = qfalse;
		if (!inHandleKey && (key == A_MOUSE1 || key == A_MOUSE2 || key == A_MOUSE3))
		{
			inHandleKey = qtrue;
			Menus_HandleOOBClick(menu, key, down);
			inHandleKey = qfalse;
			inHandler = qfalse;
			return;
		}
	}

	// get the item with focus
	for (i = 0; i < menu->itemCount; i++)
	{
		if (menu->items[i]->window.flags & WINDOW_HASFOCUS)
		{
			item = menu->items[i];
			break;
		}
	}

	// Ignore if disabled
	if (item && item->disabled)
	{
		return;
	}

	if (item != nullptr)
	{
		if (Item_HandleKey(item, key, down))
			//JLFLISTBOX
		{
			// It is possible for an item to be disable after Item_HandleKey is run (like in Voice Chat)
			if (!item->disabled)
			{
				Item_Action(item);
			}
			inHandler = qfalse;
			return;
		}
	}

	if (!down)
	{
		inHandler = qfalse;
		return;
	}

	// Special Data Pad key handling (gotta love the datapad)
	if (!(key & K_CHAR_FLAG))
	{
		//only check keys not chars
		char b[256];
		DC->getBindingBuf(key, b, 256);
		if (Q_stricmp(b, "datapad") == 0) // They hit the datapad key again.
		{
			if (Q_stricmp(menu->window.name, "datapadMissionMenu") == 0 ||
				Q_stricmp(menu->window.name, "datapadWeaponsMenu") == 0 ||
				Q_stricmp(menu->window.name, "datapadForcePowersMenu") == 0 ||
				Q_stricmp(menu->window.name, "datapadInventoryMenu") == 0)
			{
				key = A_ESCAPE; //pop on outta here
			}
		}
	}
	// default handling
	switch (key)
	{
	case A_F11:
		if (DC->getCVarValue("developer"))
		{
			uis.debugMode = static_cast<qboolean>(!uis.debugMode);
		}
		break;

	case A_F12:
		if (DC->getCVarValue("developer"))
		{
			switch (DC->screenshotFormat)
			{
			case SSF_JPEG:
				DC->executeText(EXEC_APPEND, "screenshot\n");
				break;
			case SSF_TGA:
				DC->executeText(EXEC_APPEND, "screenshot_tga\n");
				break;
			case SSF_PNG:
				DC->executeText(EXEC_APPEND, "screenshot_png\n");
				break;
			default:
				if (DC->Print)
				{
					DC->Print(
						S_COLOR_YELLOW
						"Menu_HandleKey[F12]: Unknown screenshot format assigned! This should not happen.\n");
				}
				break;
			}
		}
		break;
	case A_KP_8:
	case A_CURSOR_UP:
		Menu_SetPrevCursorItem(menu);
		break;

	case A_PAD0_GUIDE:
	case A_ESCAPE:
		if (!g_waitingForKey && menu->onESC)
		{
			itemDef_t it;
			it.parent = menu;
			Item_RunScript(&it, menu->onESC);
		}
		break;
	case A_TAB:
	case A_KP_2:
	case A_CURSOR_DOWN:
		Menu_SetNextCursorItem(menu);
		break;

	case A_MOUSE1:
	case A_MOUSE2:
		if (item)
		{
			if (item->type == ITEM_TYPE_TEXT)
			{
				if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory))
				{
					if (item->action)
						Item_Action(item);
					else
					{
						if (menu->onAccept)
						{
							itemDef_t it;
							it.parent = menu;
							Item_RunScript(&it, menu->onAccept);
						}
					}
				}
			}
			else if (item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD)
			{
				if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory))
				{
					item->cursorPos = 0;
					g_editingField = qtrue;
					g_editItem = item;
					//DC->setOverstrikeMode(qtrue);
				}
			}
			//JLFACCEPT
			// add new types here as needed
			/* Notes:
				Most controls will use the dpad to move through the selection possibilies.  Buttons are the only exception.
				Buttons will be assumed to all be on one menu together.  If the start or A button is pressed on a control focus, that
				means that the menu is accepted and move onto the next menu.  If the start or A button is pressed on a button focus it
				should just process the action and not support the accept functionality.
			*/
			//JLFACCEPT
			else if (item->type == ITEM_TYPE_MULTI || item->type == ITEM_TYPE_YESNO || item->type == ITEM_TYPE_SLIDER ||
				item->type == ITEM_TYPE_SLIDER_INTEGER || item->type == ITEM_TYPE_SLIDER_ROTATE)
			{
				if (Item_HandleAccept(item))
				{
					//Item processed it overriding the menu processing
					return;
				}
				if (menu->onAccept)
				{
					itemDef_t it;
					it.parent = menu;
					Item_RunScript(&it, menu->onAccept);
				}
			}

			//END JLFACCEPT
			else
			{
				if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory))
				{
					Item_Action(item);
				}
			}
		}
		else if (menu->onAccept)
		{
			itemDef_t it;
			it.parent = menu;
			Item_RunScript(&it, menu->onAccept);
		}
		break;

	case A_JOY0:
	case A_JOY1:
	case A_JOY2:
	case A_JOY3:
	case A_JOY4:
	case A_AUX0:
	case A_AUX1:
	case A_AUX2:
	case A_AUX3:
	case A_AUX4:
	case A_AUX5:
	case A_AUX6:
	case A_AUX7:
	case A_AUX8:
	case A_AUX9:
	case A_AUX10:
	case A_AUX11:
	case A_AUX12:
	case A_AUX13:
	case A_AUX14:
	case A_AUX15:
	case A_AUX16:
		break;
	case A_KP_ENTER:
	case A_ENTER:
		if (item)
		{
			if (item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD)
			{
				item->cursorPos = 0;
				g_editingField = qtrue;
				g_editItem = item;
				//DC->setOverstrikeMode(qtrue);
			}
			else
			{
				Item_Action(item);
			}
		}
		break;
	default:;
	}
	inHandler = qfalse;
}

/*
=================
ParseRect
=================
*/
qboolean ParseRect(const char** p, rectDef_t* r)
{
	if (!COM_ParseFloat(p, &r->x))
	{
		if (!COM_ParseFloat(p, &r->y))
		{
			if (!COM_ParseFloat(p, &r->w))
			{
				if (!COM_ParseFloat(p, &r->h))
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
=================
Menus_HideItems
=================
*/
void Menus_HideItems(const char* menuName)
{
	menuDef_t* menu = Menus_FindByName(menuName); // Get menu

	if (!menu)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: No menu was found. Could not hide items.\n");
		return;
	}

	menu->window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE);

	for (int i = 0; i < menu->itemCount; i++)
	{
		menu->items[i]->cvarFlags = CVAR_HIDE;
	}
}

/*
=================
Menus_ShowItems
=================
*/
void Menus_ShowItems(const char* menuName)
{
	menuDef_t* menu = Menus_FindByName(menuName); // Get menu

	if (!menu)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: No menu was found. Could not show items.\n");
		return;
	}

	menu->window.flags |= WINDOW_HASFOCUS | WINDOW_VISIBLE;

	for (int i = 0; i < menu->itemCount; i++)
	{
		menu->items[i]->cvarFlags = CVAR_SHOW;
	}
}

/*
=================
UI_Cursor_Show
=================
*/
void UI_Cursor_Show(const qboolean flag)
{
	DC->cursorShow = flag;

	if (DC->cursorShow != qtrue && DC->cursorShow != qfalse)
	{
		DC->cursorShow = qtrue;
	}
}