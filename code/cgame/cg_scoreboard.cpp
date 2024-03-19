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
#include "../game/b_local.h"

#define	SCOREBOARD_WIDTH	(26*BIGCHAR_WIDTH)

/*
=================
CG_MissionFailed
=================
*/
int statusTextIndex = -1;

void CG_MissionFailed()
{
	if (!cg.missionFailedScreen)
	{
		char* text;
		cgi_UI_SetActive_Menu("missionfailed_menu");
		cg.missionFailedScreen = qtrue;

		switch (statusTextIndex)
		{
		case -1: //Our HERO DIED!!!
			text = "@SP_INGAME_MISSIONFAILED_PLAYER";
			break;
		case MISSIONFAILED_JAN:
			text = "@SP_INGAME_MISSIONFAILED_JAN";
			break;
		case MISSIONFAILED_LUKE:
			text = "@SP_INGAME_MISSIONFAILED_LUKE";
			break;
		case MISSIONFAILED_LANDO:
			text = "@SP_INGAME_MISSIONFAILED_LANDO";
			break;
		case MISSIONFAILED_R5D2:
			text = "@SP_INGAME_MISSIONFAILED_R5D2";
			break;
		case MISSIONFAILED_WARDEN:
			text = "@SP_INGAME_MISSIONFAILED_WARDEN";
			break;
		case MISSIONFAILED_PRISONERS:
			text = "@SP_INGAME_MISSIONFAILED_PRISONERS";
			break;
		case MISSIONFAILED_EMPLACEDGUNS:
			text = "@SP_INGAME_MISSIONFAILED_EMPLACEDGUNS";
			break;
		case MISSIONFAILED_LADYLUCK:
			text = "@SP_INGAME_MISSIONFAILED_LADYLUCK";
			break;
		case MISSIONFAILED_KYLECAPTURE:
			text = "@SP_INGAME_MISSIONFAILED_KYLECAPTURE";
			break;
		case MISSIONFAILED_TOOMANYALLIESDIED:
			text = "@SP_INGAME_MISSIONFAILED_TOOMANYALLIESDIED";
			break;

		case MISSIONFAILED_CHEWIE:
			text = "@SP_INGAME_MISSIONFAILED_CHEWIE";
			break;

		case MISSIONFAILED_KYLE:
			text = "@SP_INGAME_MISSIONFAILED_KYLE";
			break;

		case MISSIONFAILED_ROSH:
			text = "@SP_INGAME_MISSIONFAILED_ROSH";
			break;

		case MISSIONFAILED_WEDGE:
			text = "@SP_INGAME_MISSIONFAILED_WEDGE";
			break;

		case MISSIONFAILED_TURNED:
			text = "@SP_INGAME_MISSIONFAILED_TURNED";
			break;

		default:
			text = "@SP_INGAME_MISSIONFAILED_UNKNOWN";
			break;
		}

		gi.cvar_set("ui_missionfailed_text", text);
	}
}

/*
=================
CG_MissionCompletion
=================
*/
void CG_MissionCompletion()
{
	char text[1024] = { 0 };
	constexpr int pad = 18;

	/*cgi_SP_GetStringTextString("SP_INGAME_MISSIONCOMPLETION", text, sizeof(text));
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontMedium, 1.2f);
	cgi_R_Font_DrawString(320 - w / 2, 53, text, colorTable[CT_LTGOLD1], cgs.media.qhFontMedium, -1, 1.2f);*/

	int x = 75;
	int y = 86;
	cgi_SP_GetStringTextString("SP_INGAME_SECRETAREAS", text, sizeof text);
	int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_SP_GetStringTextString("SP_INGAME_SECRETAREAS_OF", text, sizeof text);
	cgi_R_Font_DrawString(x + w, y, va("%d %s %d",
		cg_entities[0].gent->client->sess.missionStats.secretsFound,
		text,
		cg_entities[0].gent->client->sess.missionStats.totalSecrets
	),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_ENEMIESKILLED", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.enemiesKilled),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_FAVORITEWEAPON", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);

	int wpn = 0;
	int max_wpn = cg_entities[0].gent->client->sess.missionStats.weaponUsed[0];
	for (int i = 1; i < WP_NUM_WEAPONS; i++)
	{
		if (cg_entities[0].gent->client->sess.missionStats.weaponUsed[i] > max_wpn)
		{
			max_wpn = cg_entities[0].gent->client->sess.missionStats.weaponUsed[i];
			wpn = i;
		}
	}

	if (wpn)
	{
		const gitem_t* wItem = FindItemForWeapon(static_cast<weapon_t>(wpn));
		cgi_SP_GetStringTextString(va("SP_INGAME_%s", wItem->classname), text, sizeof text);
		cgi_R_Font_DrawString(x + w, y, text, colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);
	}

	x = 334 + 70;
	y = 86;
	cgi_SP_GetStringTextString("SP_INGAME_SHOTSFIRED", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.shotsFired),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_HITS", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.hits), colorTable[CT_WHITE],
		cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_ACCURACY", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	const float percent = cg_entities[0].gent->client->sess.missionStats.shotsFired
		? 100.0f * static_cast<float>(cg_entities[0].gent->client->sess.missionStats.hits) /
		cg_entities[0].gent->client->sess.missionStats.shotsFired
		: 0;
	cgi_R_Font_DrawString(x + w, y, va("%.2f%%", percent), colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	if (cg_entities[0].gent->client->sess.missionStats.weaponUsed[WP_SABER] <= 0)
	{
		return; //don't have saber yet, so don't print any stats
	}
	//first column, FORCE POWERS
	y = 180;
	cgi_SP_GetStringTextString("SP_INGAME_FORCEUSE", text, sizeof text);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_HEAL", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_HEAL]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_SPEED", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_SPEED]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_PULL", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_PULL]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_PUSH", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_PUSH]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_MINDTRICK", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_TELEPATHY]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_GRIP", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_GRIP]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_LIGHTNING", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.forceUsed[FP_LIGHTNING]),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	//second column, LIGHT SABER
	y = 180;
	x = 140;
	cgi_SP_GetStringTextString("SP_INGAME_LIGHTSABERUSE", text, sizeof text);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_THROWN", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.saberThrownCnt),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_BLOCKS", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.saberBlocksCnt),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_LEGATTACKS", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.legAttacksCnt),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_ARMATTACKS", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.armAttacksCnt),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_BODYATTACKS", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.torsoAttacksCnt),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);

	y += pad;
	cgi_SP_GetStringTextString("SP_INGAME_OTHERATTACKS", text, sizeof text);
	w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 0.8f);
	cgi_R_Font_DrawString(x, y, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 0.8f);
	cgi_R_Font_DrawString(x + w, y, va("%d", cg_entities[0].gent->client->sess.missionStats.otherAttacksCnt),
		colorTable[CT_WHITE], cgs.media.qhFontSmall, -1, 0.8f);
}

/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
return value is bool to NOT draw centerstring
=================
*/
qboolean CG_DrawScoreboard()
{
	// don't draw anything if the menu is up
	if (cg_paused.integer)
	{
		return qfalse;
	}

	// Character is either dead, or a script has brought up the screen
	if (cg.predictedPlayerState.pm_type == PM_DEAD && cg.missionStatusDeadTime < level.time
		|| cg.missionStatusShow)
	{
		CG_MissionFailed();
		return qtrue;
	}

	return qfalse;
}

void ScoreBoardReset()
{
}

//================================================================================