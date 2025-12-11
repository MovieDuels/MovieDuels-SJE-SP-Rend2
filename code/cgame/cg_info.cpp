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

#include "cg_headers.h"

#include "cg_media.h"
#include "../game/objectives.h"

// For printing objectives
static constexpr short objectiveStartingYpos = 75; // Y starting position for objective text
static constexpr short objectiveStartingXpos = 60; // X starting position for objective text
static constexpr int objectiveTextBoxWidth = 500; // Width (in pixels) of text box
static constexpr int objectiveTextBoxHeight = 300; // Height (in pixels) of text box
static constexpr short missionYpos = 79;
extern vmCvar_t cg_com_kotor;
extern vmCvar_t cg_com_rend2;
extern vmCvar_t cg_com_outcast;

const char* showLoadPowersName[] =
{
	"SP_INGAME_HEAL2",
	"SP_INGAME_JUMP2",
	"SP_INGAME_SPEED2",
	"SP_INGAME_PUSH2",
	"SP_INGAME_PULL2",
	"SP_INGAME_MINDTRICK2",
	"SP_INGAME_GRIP2",
	"SP_INGAME_LIGHTNING2",
	"SP_INGAME_SABER_THROW2",
	"SP_INGAME_SABER_OFFENSE2",
	"SP_INGAME_SABER_DEFENSE2",
	"SP_INGAME_STASIS2",
	"SP_INGAME_DESTRUCTION2",
	"SP_INGAME_GRASP2",
	"SP_INGAME_REPULSE2",
	"SP_INGAME_LIGHTNING_STRIKE2",
	"SP_INGAME_BLAST2",
	"SP_INGAME_FEAR2",
	"SP_INGAME_DEADLYSIGHT2",
	"SP_INGAME_PROJECTION2",
	nullptr,
};

constexpr auto MAX_OBJ_GRAPHICS = 4;
constexpr auto OBJ_GRAPHIC_SIZE = 240;
int obj_graphics[MAX_OBJ_GRAPHICS];

qboolean CG_ForcePower_Valid(int forceKnownBits, int index);

/*
====================
ObjectivePrint_Line

Print a single mission objective
====================
*/
static void ObjectivePrint_Line(const int color, const int objectIndex, int& missionYcnt)
{
	int y;
	char finalText[2048];
	qhandle_t graphic;

	const int iYPixelsPerLine = cgi_R_Font_HeightPixels(cgs.media.qhFontMedium, 1.0f);

	cgi_SP_GetStringTextString(va("OBJECTIVES_%s", objectiveTable[objectIndex].name), finalText, sizeof finalText);

	// A hack to be able to count prisoners
	if (objectIndex == T2_RANCOR_OBJ5)
	{
		char value[64];

		gi.Cvar_VariableStringBuffer("ui_prisonerobj_currtotal", value, sizeof value);
		const int currTotal = atoi(value);
		gi.Cvar_VariableStringBuffer("ui_prisonerobj_maxtotal", value, sizeof value);
		const int minTotal = atoi(value);

		Q_strncpyz(finalText, va(finalText, currTotal, minTotal), sizeof finalText);
	}

	int pixelLen = cgi_R_Font_StrLenPixels(finalText, cgs.media.qhFontMedium, 1.0f);

	char* str = finalText;

	if (cgi_Language_IsAsian())
	{
		// this is execrable, and should NOT have had to've been done now, but...
		//
		extern const char* CG_DisplayBoxedText(int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight,
			const char* psText, int iFontHandle, float fScale,
			const vec4_t v4Color);
		extern int giLinesOutput;
		extern float gfAdvanceHack;

		gfAdvanceHack = 1.0f; // override internal vertical advance
		y = objectiveStartingYpos + iYPixelsPerLine * missionYcnt;

		// Advance line if a graphic has printed
		for (const int obj_graphic : obj_graphics)
		{
			if (obj_graphic)
			{
				y += OBJ_GRAPHIC_SIZE + 4;
			}
		}

		CG_DisplayBoxedText(
			objectiveStartingXpos,
			y,
			objectiveTextBoxWidth,
			objectiveTextBoxHeight,
			finalText, // int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight, const char *psText
			cgs.media.qhFontMedium, // int iFontHandle,
			1.0f, // float fScale,
			colorTable[color] // const vec4_t v4Color
		);

		gfAdvanceHack = 0.0f; // restore
		missionYcnt += giLinesOutput;
	}
	else
	{
		// western...
		//
		if (pixelLen < objectiveTextBoxWidth) // One shot - small enough to print entirely on one line
		{
			y = objectiveStartingYpos + iYPixelsPerLine * missionYcnt;

			cgi_R_Font_DrawString(
				objectiveStartingXpos,
				y,
				str,
				colorTable[color],
				cgs.media.qhFontMedium,
				-1,
				1.0f);

			++missionYcnt;
		}
		// Text is too long, break into lines.
		else
		{
			constexpr int maxHoldText = 1024;
			char holdText[maxHoldText];
			char holdText2[2]{};
			pixelLen = 0;
			int charLen = 0;
			holdText2[1] = '\0';
			const char* strBegin = str;

			while (*str)
			{
				holdText2[0] = *str;
				pixelLen += cgi_R_Font_StrLenPixels(holdText2, cgs.media.qhFontMedium, 1.0f);

				pixelLen += 2; // For kerning
				++charLen;

				if (pixelLen > objectiveTextBoxWidth)
				{
					//Reached max length of this line
					//step back until we find a space
					while (charLen > 10 && *str != ' ')
					{
						--str;
						--charLen;
					}

					if (*str == ' ')
					{
						++str; // To get past space
					}

					assert(charLen < maxHoldText); // Too big?

					Q_strncpyz(holdText, strBegin, charLen);
					holdText[charLen] = '\0';
					strBegin = str;
					pixelLen = 0;
					charLen = 1;

					y = objectiveStartingYpos + iYPixelsPerLine * missionYcnt;

					CG_DrawProportionalString(
						objectiveStartingXpos,
						y,
						holdText,
						CG_SMALLFONT,
						colorTable[color]);

					++missionYcnt;
				}
				else if (*(str + 1) == '\0')
				{
					++charLen;

					assert(charLen < maxHoldText); // Too big?

					y = objectiveStartingYpos + iYPixelsPerLine * missionYcnt;

					Q_strncpyz(holdText, strBegin, charLen);
					CG_DrawProportionalString(
						objectiveStartingXpos,
						y, holdText,
						CG_SMALLFONT,
						colorTable[color]);

					++missionYcnt;
					break;
				}
				++str;
			}
		}
	}

	if (objectIndex == T3_BOUNTY_OBJ1)
	{
		y = objectiveStartingYpos + iYPixelsPerLine * missionYcnt;

		if (obj_graphics[1])
		{
			y += OBJ_GRAPHIC_SIZE + 4;
		}
		if (obj_graphics[2])
		{
			y += OBJ_GRAPHIC_SIZE + 4;
		}
		graphic = cgi_R_RegisterShaderNoMip("textures/system/viewscreen1");
		CG_DrawPic(355, 50, OBJ_GRAPHIC_SIZE, OBJ_GRAPHIC_SIZE, graphic);
		obj_graphics[3] = qtrue;
	} //// Special case hack
	else if (objectIndex == DOOM_COMM_OBJ4)
	{
		y = missionYpos + iYPixelsPerLine * missionYcnt;
		graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode");
		CG_DrawPic(320 - 128 / 2, y + 8, 128, 32, graphic);
		obj_graphics[0] = qtrue;
	}
	else if (objectIndex == KEJIM_POST_OBJ3)
	{
		y = missionYpos + iYPixelsPerLine * missionYcnt;
		graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode_red");
		CG_DrawPic(320 - 32 / 2, y + 8, 32, 32, graphic);
		obj_graphics[1] = qtrue;
	}
	else if (objectIndex == KEJIM_POST_OBJ4)
	{
		y = missionYpos + iYPixelsPerLine * missionYcnt;
		if (obj_graphics[1])
		{
			y += 32 + 4;
		}
		graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode_green");
		CG_DrawPic(320 - 32 / 2, y + 8, 32, 32, graphic);
		obj_graphics[2] = qtrue;
	}
	else if (objectIndex == KEJIM_POST_OBJ5)
	{
		y = missionYpos + iYPixelsPerLine * missionYcnt;
		if (obj_graphics[1])
		{
			y += 32 + 4;
		}
		if (obj_graphics[2])
		{
			y += 32 + 4;
		}
		graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode_blue");
		CG_DrawPic(320 - 32 / 2, y + 8, 32, 32, graphic);
		obj_graphics[3] = qtrue;
	}
}

/*
====================
CG_DrawDataPadObjectives

Draw routine for the objective info screen of the data pad.
====================
*/
void CG_DrawDataPadObjectives(const centity_t* cent)
{
	int i;
	const int iYPixelsPerLine = cgi_R_Font_HeightPixels(cgs.media.qhFontMedium, 1.0f);

	constexpr short titleXPos = objectiveStartingXpos - 22; // X starting position for title text
	constexpr short titleYPos = objectiveStartingYpos - 23; // Y starting position for title text
	constexpr short graphic_size = 16; // Size (width and height) of graphic used to show status of objective
	constexpr short graphicXpos = objectiveStartingXpos - graphic_size - 8;
	// Amount of X to backup from text starting position
	const short graphicYOffset = (iYPixelsPerLine - graphic_size) / 2;
	// Amount of Y to raise graphic so it's in the center of the text line

	missionInfo_Updated = qfalse; // This will stop the text from flashing
	cg.missionInfoFlashTime = 0;

	// zero out objective graphics
	for (i = 0; i < MAX_OBJ_GRAPHICS; i++)
	{
		obj_graphics[i] = qfalse;
	}

	// Title Text at the top
	char text[1024] = { 0 };
	cgi_SP_GetStringTextString("SP_INGAME_OBJECTIVES", text, sizeof text);
	cgi_R_Font_DrawString(titleXPos, titleYPos, text, colorTable[CT_TITLE], cgs.media.qhFontMedium, -1, 1.0f);

	int missionYcnt = 0;

	// Print all active objectives
	for (i = 0; i < MAX_OBJECTIVES; i++)
	{
		// Is there an objective to see?
		if (cent->gent->client->sess.mission_objectives[i].display)
		{
			// Calculate the Y position
			const int totalY = objectiveStartingYpos + iYPixelsPerLine * missionYcnt + iYPixelsPerLine / 2;

			//	Draw graphics that show if mission has been accomplished or not
			cgi_R_SetColor(colorTable[CT_BLUE3]);
			CG_DrawPic(graphicXpos, totalY - graphicYOffset, graphic_size, graphic_size,
				cgs.media.messageObjCircle); // Circle in front
			if (cent->gent->client->sess.mission_objectives[i].status == OBJECTIVE_STAT_SUCCEEDED)
			{
				CG_DrawPic(graphicXpos, totalY - graphicYOffset, graphic_size, graphic_size,
					cgs.media.messageLitOn); // Center Dot
			}

			// Print current objective text
			ObjectivePrint_Line(CT_BLUE3, i, missionYcnt);
		}
	}

	// No mission text?
	if (!missionYcnt)
	{
		// Set the message a quarter of the way down and in the center of the text box
		constexpr int messageYPosition = objectiveStartingYpos + objectiveTextBoxHeight / 4;

		cgi_SP_GetStringTextString("SP_INGAME_OBJNONE", text, sizeof text);
		const int messageXPosition = objectiveStartingXpos + objectiveTextBoxWidth / 2 - cgi_R_Font_StrLenPixels(
			text, cgs.media.qhFontMedium, 1.0f) / 2;

		cgi_R_Font_DrawString(
			messageXPosition,
			messageYPosition,
			text,
			colorTable[CT_LTBLUE1],
			cgs.media.qhFontMedium,
			-1,
			1.0f);
	}
}

constexpr auto LOADBAR_CLIP_WIDTH = 256;
constexpr auto LOADBAR_CLIP_HEIGHT = 64;

static void CG_LoadBar()
{
	constexpr int numticks = 9, tickwidth = 40, tickheight = 8;
	constexpr int tickpadx = 20, tickpady = 12;
	constexpr int capwidth = 8;
	constexpr int barwidth = numticks * tickwidth + tickpadx * 2 + capwidth * 2, barleft = (640 - barwidth) / 2;
	constexpr int barheight = tickheight + tickpady * 2, bartop = 475 - barheight;
	constexpr int capleft = barleft + tickpadx, tickleft = capleft + capwidth, ticktop = bartop + tickpady;

	cgi_R_SetColor(colorTable[CT_WHITE]);
	// Draw background
	CG_DrawPic(barleft, bartop, barwidth, barheight, cgs.media.levelLoad);

	// Draw left cap (backwards)
	CG_DrawPic(tickleft, ticktop, -capwidth, tickheight, cgs.media.loadTickCap);

	// Draw bar
	CG_DrawPic(tickleft, ticktop, tickwidth * cg.loadLCARSStage, tickheight, cgs.media.loadTick);

	// Draw right cap
	CG_DrawPic(tickleft + tickwidth * cg.loadLCARSStage, ticktop, capwidth, tickheight, cgs.media.loadTickCap);

	if (cg.loadLCARSStage >= 3)
	{
		if (cg.loadLCARSStage <= 6)
		{
			if (cg_com_rend2.integer == 1) //rend2 is on
			{
				cgi_R_Font_DrawString(10, 10, va("Longer loading times can be expected with Rend2 enabled."), colorTable[CT_WHITE], cgs.media.qhFontMedium, -1, 0.5f);
			}
		}
		constexpr int x = (640 - LOADBAR_CLIP_WIDTH) / 2;
		constexpr int y = 50;

		//CG_DrawPic(x, y, LOADBAR_CLIP_WIDTH, LOADBAR_CLIP_HEIGHT, cgs.media.load_SerenitySaberSystems);
	}
}

int CG_WeaponCheck(int weapon_index);

// For printing load screen icons
constexpr int MAXLOADICONSPERROW = 8; // Max icons displayed per row
constexpr int MAXLOADWEAPONS = WP_NUM_WEAPONS;
constexpr int maxload_forceiconsize = 40; // Size of force power icons
constexpr int MAXLOAD_FORCEICONPAD = 12; // Padding space between icons

static int CG_DrawLoadWeaponsPrintRow(const char* itemName, const char* weapons, const int rowIconCnt, const int startIndex)
{
	int endIndex = 0, printedIconCnt = 0;
	int x, y;
	int width, height;
	vec4_t color;
	qhandle_t background;
	const gentity_t* cent = &g_entities[g_entities[0].client->ps.client_num];

	if (!cgi_UI_GetMenuItemInfo(
		"loadScreen",
		itemName,
		&x,
		&y,
		&width,
		&height,
		color,
		&background))
	{
		return 0;
	}

	cgi_R_SetColor(color);

	constexpr int iconSize = 60;
	constexpr int pad = 12;

	// calculate placement of weapon icons
	int holdX = x + (width - (iconSize * rowIconCnt + pad * (rowIconCnt - 1))) / 2;

	for (int i = startIndex; i < MAXLOADWEAPONS; i++)
	{
		if (!weapons[i]) // Does he have this weapon?
		{
			continue;
		}

		if (cg_com_kotor.integer == 1) //playing kotor
		{
			if (weaponData[i].alt_weapon_Icon_file[0])
			{
				CG_RegisterWeapon(i);
				const weaponInfo_t* weaponInfo = &cg_weapons[i];
				endIndex = i;

				constexpr int yOffset = 0;
				CG_DrawPic(holdX, y + yOffset, iconSize, iconSize, weaponInfo->alt_weapon_Icon);

				printedIconCnt++;
				if (printedIconCnt == MAXLOADICONSPERROW)
				{
					break;
				}

				holdX += iconSize + pad;
			}
		}
		else
		{
			if (cent->friendlyfaction == FACTION_KOTOR)
			{
				if (weaponData[i].alt_weapon_Icon_file[0])
				{
					CG_RegisterWeapon(i);
					const weaponInfo_t* weaponInfo = &cg_weapons[i];
					endIndex = i;

					constexpr int yOffset = 0;
					CG_DrawPic(holdX, y + yOffset, iconSize, iconSize, weaponInfo->alt_weapon_Icon);

					printedIconCnt++;
					if (printedIconCnt == MAXLOADICONSPERROW)
					{
						break;
					}

					holdX += iconSize + pad;
				}
			}
			else
			{
				if (weaponData[i].weapon_Icon_file[0])
				{
					CG_RegisterWeapon(i);
					const weaponInfo_t* weaponInfo = &cg_weapons[i];
					endIndex = i;

					constexpr int yOffset = 0;
					CG_DrawPic(holdX, y + yOffset, iconSize, iconSize, weaponInfo->weapon_Icon);

					printedIconCnt++;
					if (printedIconCnt == MAXLOADICONSPERROW)
					{
						break;
					}

					holdX += iconSize + pad;
				}
			}
		}
	}

	return endIndex;
}

// Print weapons the player is carrying
// Two rows print if there are too many
static void CG_DrawLoadWeapons(const char* weapons)
{
	// count the number of weapons owned
	int iconCnt = 0;
	for (int i = 1; i < MAXLOADWEAPONS; i++)
	{
		if (weapons[i])
		{
			iconCnt++;
		}
	}

	if (!iconCnt) // If no weapons, don't display
	{
		return;
	}

	//TODO: allow for more rows of icons??
	// Single line of icons
	if (iconCnt <= MAXLOADICONSPERROW)
	{
		CG_DrawLoadWeaponsPrintRow("weaponicons_singlerow", weapons, iconCnt, 0);
	}
	// Two lines of icons
	else
	{
		// Print top row
		const int endIndex = CG_DrawLoadWeaponsPrintRow("weaponicons_row1", weapons, MAXLOADICONSPERROW, 0);

		// Print second row
		const int rowIconCnt = iconCnt - MAXLOADICONSPERROW;
		CG_DrawLoadWeaponsPrintRow("weaponicons_row2", weapons, rowIconCnt, endIndex + 1);
	}

	cgi_R_SetColor(nullptr);
}

static int CG_DrawLoadForcePrintRow(const char* itemName, const int forceBits, const int rowIconCnt,
	const int startIndex)
{
	int endIndex = 0, printedIconCnt = 0;
	int x, y;
	int width, height;
	vec4_t color;
	qhandle_t background;

	if (!cgi_UI_GetMenuItemInfo(
		"loadScreen",
		itemName,
		&x,
		&y,
		&width,
		&height,
		color,
		&background))
	{
		return 0;
	}

	cgi_R_SetColor(color);

	// calculate placement of weapon icons
	int holdX = x + (width - (maxload_forceiconsize * rowIconCnt + MAXLOAD_FORCEICONPAD * (rowIconCnt - 1))) / 2;

	for (int i = startIndex; i < MAX_SHOWPOWERS; i++)
	{
		if (!CG_ForcePower_Valid(forceBits, i)) // Does he have this power?
		{
			continue;
		}

		if (force_icons[showPowers[i]])
		{
			constexpr int yOffset = 0;
			endIndex = i;

			CG_DrawPic(holdX, y + yOffset, maxload_forceiconsize, maxload_forceiconsize, force_icons[showPowers[i]]);

			printedIconCnt++;
			if (printedIconCnt == MAXLOADICONSPERROW)
			{
				break;
			}

			holdX += maxload_forceiconsize + MAXLOAD_FORCEICONPAD;
		}
	}

	return endIndex;
}

int loadForcePowerLevel[NUM_FORCE_POWERS];

/*
===============
ForcePowerDataPad_Valid
===============
*/
qboolean CG_ForcePower_Valid(const int forceKnownBits, const int index)
{
	if (forceKnownBits & 1 << showPowers[index] &&
		loadForcePowerLevel[showPowers[index]]) // Does he have the force power?
	{
		return qtrue;
	}

	return qfalse;
}

// Print force powers the player is using
// Two rows print if there are too many
static void CG_DrawLoadForcePowers(const int forceBits)
{
	int iconCnt = 0;

	// Count the number of force powers known
	for (int i = 0; i < MAX_SHOWPOWERS; ++i)
	{
		if (CG_ForcePower_Valid(forceBits, i))
		{
			iconCnt++;
		}
	}

	if (!iconCnt) // If no force powers, don't display
	{
		return;
	}

	// Single line of icons
	if (iconCnt <= MAXLOADICONSPERROW)
	{
		CG_DrawLoadForcePrintRow("forceicons_singlerow", forceBits, iconCnt, 0);
	}
	// Two lines of icons
	else
	{
		// Print top row
		const int endIndex = CG_DrawLoadForcePrintRow("forceicons_row1", forceBits, MAXLOADICONSPERROW, 0);

		// Print second row
		const int rowIconCnt = iconCnt - MAXLOADICONSPERROW;
		CG_DrawLoadForcePrintRow("forceicons_row2", forceBits, rowIconCnt, endIndex + 1);
	}

	cgi_R_SetColor(nullptr);
}

// Get the player weapons and force power info
static void CG_GetLoadScreenInfo(char* weapons, int* forceBits)
{
	char s[MAX_STRING_CHARS];
	int iDummy;
	float fDummy;
	const char* var;

	gi.Cvar_VariableStringBuffer(sCVARNAME_PLAYERSAVE, s, sizeof s);

	// Get player weapons and force powers known
	if (s[0])
	{
		//				|general info				  |-force powers
		sscanf(s, "%i %i %i %i %i %i %f %f %f %i %i",
			&iDummy, //	&client->ps.stats[STAT_HEALTH],
			&iDummy, //	&client->ps.stats[STAT_ARMOR],
			&iDummy, //	&client->ps.stats[STAT_ITEMS],
			&iDummy, //	&client->ps.weapon,
			&iDummy, //	&client->ps.weaponstate,
			&iDummy, //	&client->ps.batteryCharge,
			&fDummy, //	&client->ps.viewangles[0],
			&fDummy, //	&client->ps.viewangles[1],
			&fDummy, //	&client->ps.viewangles[2],
			//force power data
			&*forceBits, //	&client->ps.forcePowersKnown,
			&iDummy //	&client->ps.forcePower,

		);
	}

	gi.Cvar_VariableStringBuffer("playerweaps", s, sizeof s);
	int i = 0;
	if (s[0])
	{
		var = strtok(s, " ");
		while (var != nullptr)
		{
			/* While there are tokens in "s" */
			weapons[i++] = atoi(var);
			/* Get next token: */
			var = strtok(nullptr, " ");
		}
		assert(i == WP_NUM_WEAPONS);
	}
	else
	{
		weapons[0] = -1;
	}

	// the new JK2 stuff - force powers, etc...
	//
	gi.Cvar_VariableStringBuffer("playerfplvl", s, sizeof s);
	i = 0;
	var = strtok(s, " ");
	while (var != nullptr)
	{
		/* While there are tokens in "s" */
		loadForcePowerLevel[i++] = atoi(var);
		/* Get next token: */
		var = strtok(nullptr, " ");
	}
}

/*
====================
CG_DrawLoadingScreen

Load screen displays the map pic, the mission briefing and weapons/force powers
====================
*/

static void CG_DrawLoadingScreen(const qhandle_t levelshot, const char* map_name)
{
	int x_pos, y_pos, width, height;
	vec4_t color;
	qhandle_t background;
	int forcepowers = 0;
	char weapons[WP_NUM_WEAPONS];

	// Get mission briefing for load screen
	if (cgi_SP_GetStringTextString(va("BRIEFINGS_%s", map_name), nullptr, 0) == 0)
	{
		cgi_Cvar_Set("ui_missionbriefing", "@BRIEFINGS_NONE");
	}
	else
	{
		cgi_Cvar_Set("ui_missionbriefing", va("@BRIEFINGS_%s", map_name));
	}

	// Print background
	if (cgi_UI_GetMenuItemInfo(
		"loadScreen",
		"background",
		&x_pos,
		&y_pos,
		&width,
		&height,
		color,
		&background))
	{
		cgi_R_SetColor(color);
		CG_DrawPic(x_pos, y_pos, width, height, background);
	}

	// Print level pic
	if (cgi_UI_GetMenuItemInfo(
		"loadScreen",
		"mappic",
		&x_pos,
		&y_pos,
		&width,
		&height,
		color,
		&background))
	{
		cgi_R_SetColor(color);
		CG_DrawPic(x_pos, y_pos, width, height, levelshot);
	}

	// Get player weapons and force power info
	CG_GetLoadScreenInfo(weapons, &forcepowers);

	// Print weapon icons
	if (weapons[0] > -1)
	{
		CG_DrawLoadWeapons(weapons);
	}

	// Print force power icons
	if (forcepowers)
	{
		CG_DrawLoadForcePowers(forcepowers);
	}
}

/*
====================
CG_DrawInformation

Draw all the status / pacifier stuff during level loading
====================
*/

int SCREENTIP_NEXT_UPDATE_TIME = 0;

static void LoadTips(void)
{
	const int index = Q_irand(1, 20);
	const int time = cgi_Milliseconds();

	if ((SCREENTIP_NEXT_UPDATE_TIME < time || SCREENTIP_NEXT_UPDATE_TIME == 0))
	{
		cgi_Cvar_Set("ui_tipsbriefing", va("@LOADTIPS_TIP%d", index));
		SCREENTIP_NEXT_UPDATE_TIME = time + 3500;
	}
}

int SCREENSHOT_TOTAL = 8;
int SCREENSHOT_CHOICE = 0;
int SCREENSHOT_NEXT_UPDATE_TIME = 0;
char SCREENSHOT_CURRENT[64] = { 0 };

static char* cg_GetCurrentLevelshot1(const char* s)
{
	const qhandle_t levelshot1 = cgi_R_RegisterShaderNoMip(va("levelshots/%s", s));
	const int time = cgi_Milliseconds();

	if (levelshot1 && SCREENSHOT_NEXT_UPDATE_TIME == 0)
	{
		SCREENSHOT_NEXT_UPDATE_TIME = time + 10000;
		memset(SCREENSHOT_CURRENT, 0, sizeof SCREENSHOT_CURRENT);
		strcpy(SCREENSHOT_CURRENT, va("levelshots/%s", s));
		return SCREENSHOT_CURRENT;
	}

	if (SCREENSHOT_NEXT_UPDATE_TIME < time || SCREENSHOT_NEXT_UPDATE_TIME == 0)
	{
		SCREENSHOT_NEXT_UPDATE_TIME = time + 10000;
		SCREENSHOT_CHOICE = Q_irand(0, SCREENSHOT_TOTAL);
		memset(SCREENSHOT_CURRENT, 0, sizeof SCREENSHOT_CURRENT);
		strcpy(SCREENSHOT_CURRENT, va("menu/art/unknownmap%i", SCREENSHOT_CHOICE));
	}
	return SCREENSHOT_CURRENT;
}

static char* cg_GetCurrentLevelshot2(const char* s)
{
	const qhandle_t levelshot2 = cgi_R_RegisterShaderNoMip(va("levelshots/%s2", s));
	const int time = cgi_Milliseconds();

	if (levelshot2 && SCREENSHOT_NEXT_UPDATE_TIME == 0)
	{
		SCREENSHOT_NEXT_UPDATE_TIME = time + 2500;
		memset(SCREENSHOT_CURRENT, 0, sizeof SCREENSHOT_CURRENT);
		strcpy(SCREENSHOT_CURRENT, va("levelshots/%s2", s));
		return SCREENSHOT_CURRENT;
	}

	if (SCREENSHOT_NEXT_UPDATE_TIME < time || SCREENSHOT_NEXT_UPDATE_TIME == 0)
	{
		SCREENSHOT_NEXT_UPDATE_TIME = time + 2500;
		SCREENSHOT_CHOICE = Q_irand(0, SCREENSHOT_TOTAL);
		memset(SCREENSHOT_CURRENT, 0, sizeof SCREENSHOT_CURRENT);
		strcpy(SCREENSHOT_CURRENT, va("menu/art/unknownmap%i", SCREENSHOT_CHOICE));
	}
	return SCREENSHOT_CURRENT;
}

void CG_DrawInformation()
{
	// draw the dialog background
	const char* info = CG_ConfigString(CS_SERVERINFO);
	const char* s = Info_ValueForKey(info, "mapname");

	extern SavedGameJustLoaded_e g_eSavedGameJustLoaded;

	qhandle_t levelshot = cgi_R_RegisterShaderNoMip(va("levelshots/%s", s));
	qhandle_t levelshot2 = cgi_R_RegisterShaderNoMip(va("levelshots/%s2", s));
	qhandle_t Loadshot = cgi_R_RegisterShaderNoMip("menu/art/loadshot");
	qhandle_t Loadshot2 = cgi_R_RegisterShaderNoMip("menu/art/loadshot2");

	if (!levelshot)
	{
		levelshot = cgi_R_RegisterShaderNoMip(cg_GetCurrentLevelshot1(s));
	}
	if (!levelshot2)
	{
		levelshot2 = cgi_R_RegisterShaderNoMip(cg_GetCurrentLevelshot2(s));
	}
	if (!Loadshot)
	{
		Loadshot = cgi_R_RegisterShaderNoMip(cg_GetCurrentLevelshot1(s));
	}
	if (!Loadshot2)
	{
		Loadshot2 = cgi_R_RegisterShaderNoMip(cg_GetCurrentLevelshot1(s));
	}

	if (g_eSavedGameJustLoaded != eFULL
		&& (strcmp(s, "yavin1") == 0
			|| strcmp(s, "demo") == 0
			|| strcmp(s, "jodemo") == 0
			|| strcmp(s, "01nar") == 0
			|| strcmp(s, "md2_bd_ch") == 0
			|| strcmp(s, "md_sn_intro_jedi") == 0
			|| strcmp(s, "md_sn_intro_jedi2") == 0
			|| strcmp(s, "md_ch_battledroids") == 0
			|| strcmp(s, "md_ep4_intro") == 0
			|| strcmp(s, "secbase") == 0
			|| strcmp(s, "level0") == 0
			|| strcmp(s, "part_1") == 0
			|| strcmp(s, "kejim_post") == 0)) //special case for first map!
	{
		constexpr char text[1024] = { 0 };
		if (cg.loadLCARSStage <= 4 && cg_com_rend2.integer == 1)
		{
			CG_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Loadshot2);
		}
		else
		{
			CG_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Loadshot);

			const int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontMedium, 1.0f);
			cgi_R_Font_DrawString(320 - w / 2, 140, text, colorTable[CT_ICON_BLUE], cgs.media.qhFontMedium, -1, 1.0f);
		}
	}
	else
	{
		if (cg_com_outcast.integer == 1)
		{
			if (cg.loadLCARSStage >= 4)
			{
				CG_DrawLoadingScreen(levelshot2, s);
				if (cg_com_outcast.integer == 1)
				{
					CG_MissionCompletion();
				}
			}
			else
			{
				CG_DrawLoadingScreen(levelshot, s);
			}
		}
		else
		{
			CG_DrawLoadingScreen(levelshot, s);
		}
		cgi_UI_Menu_Paint(cgi_UI_GetMenuByName("loadscreen"), qtrue);
		CG_LoadBar();
		LoadTips();
	}

	// draw info string information

	// map-specific message (long map name)
	s = CG_ConfigString(CS_MESSAGE);

	if (s[0])
	{
		constexpr int y = 20;
		if (s[0] == '@')
		{
			char text[1024] = { 0 };
			cgi_SP_GetStringTextString(s + 1, text, sizeof text);
			cgi_R_Font_DrawString(15, y, va("\"%s\"", text), colorTable[CT_WHITE], cgs.media.qhFontMedium, -1, 1.0f);
		}
		else
		{
			cgi_R_Font_DrawString(15, y, va("\"%s\"", s), colorTable[CT_WHITE], cgs.media.qhFontMedium, -1, 1.0f);
		}
	}
}