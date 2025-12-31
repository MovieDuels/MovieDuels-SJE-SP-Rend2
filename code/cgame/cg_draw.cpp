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

// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_headers.h"

#include "cg_media.h"
#include "../game/objectives.h"
#include "../game/g_vehicles.h"

extern vmCvar_t cg_debugHealthBars;
extern vmCvar_t cg_debugBlockBars;
extern vmCvar_t cg_debugFatigueBars;
extern vmCvar_t cg_saberinfo;
extern vmCvar_t cg_com_kotor;

extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern qboolean G_ControlledByPlayer(const gentity_t* self);

void CG_DrawSJEIconBackground();
void CG_DrawIconBackground();
void CG_DrawInventorySelect();
void cg_draw_inventory_select_side();
void CG_DrawInventorySelect_text();
void CG_DrawForceSelect();
void CG_DrawForceSelect_side();
void CG_DrawForceSelect_text();
qboolean CG_WorldCoordToScreenCoord(vec3_t world_coord, int* x, int* y);
qboolean CG_WorldCoordToScreenCoordFloat(vec3_t world_coord, float* x, float* y);
extern qboolean PM_DeathCinAnim(int anim);
extern qboolean NPC_IsOversized(const gentity_t* self);

extern float g_crosshairEntDist;
extern int g_crosshairSameEntTime;
extern int g_crosshairEntNum;
extern int g_crosshairEntTime;
qboolean cg_forceCrosshair = qfalse;

// bad cheating
extern int g_rocketLockEntNum;
extern int g_rocketLockTime;
extern int g_rocketSlackTime;
extern vmCvar_t cg_SerenityJediEngineMode;
extern vmCvar_t cg_SerenityJediEngineHudMode;

vec3_t vfwd;
vec3_t vright;
vec3_t vup;
vec3_t vfwd_n;
vec3_t vright_n;
vec3_t vup_n;
int infoStringCount;

int cgRageTime = 0;
int cgRageFadeTime = 0;
float cgRageFadeVal = 0;

int cgRageRecTime = 0;
int cgRageRecFadeTime = 0;
float cgRageRecFadeVal = 0;

int cgAbsorbTime = 0;
int cgAbsorbFadeTime = 0;
float cgAbsorbFadeVal = 0;

int cgProtectTime = 0;
int cgProtectFadeTime = 0;
float cgProtectFadeVal = 0;

int cgProjectTime = 0;
int cgProjectFadeTime = 0;
float cgProjectFadeVal = 0;
//===============================================================

/*
================
CG_DrawMessageLit
================
*/
static void CG_DrawMessageLit(const int x, const int y)
{
	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (cg.missionInfoFlashTime > cg.time)
	{
		if (!(cg.time / 600 & 1))
		{
			if (!cg.messageLitActive)
			{
				cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.messageLitSound);
				cg.messageLitActive = qtrue;
			}

			cgi_R_SetColor(colorTable[CT_HUD_RED]);
			CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOn);
		}
		else
		{
			cg.messageLitActive = qfalse;
		}
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOff);
}

static void CG_DrawForcePower(const centity_t* cent, const float hud_ratio)
{
	qboolean flash = qfalse;
	vec4_t calc_color;
	float extra = 0, percent;

	if (!cent->gent->client->ps.forcePowersKnown || G_IsRidingVehicle(cent->gent))
	{
		return;
	}

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.forceHUDTotalFlashTime > cg.time)
	{
		flash = qtrue;
		if (cg.forceHUDNextFlashTime < cg.time)
		{
			cg.forceHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.noforceSound);
			if (cg.forceHUDActive)
			{
				cg.forceHUDActive = qfalse;
			}
			else
			{
				cg.forceHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.forceHUDNextFlashTime = 0;
		cg.forceHUDActive = qtrue;
	}

	const float inc = static_cast<float>(cent->gent->client->ps.forcePowerMax) / MAX_HUD_TICS;
	float value = cent->gent->client->ps.forcePower;
	if (value > cent->gent->client->ps.forcePowerMax)
	{
		//supercharged with force
		extra = value - cent->gent->client->ps.forcePowerMax;
		value = cent->gent->client->ps.forcePowerMax;
	}

	for (int i = MAX_HUD_TICS - 1; i >= 0; i--)
	{
		if (extra)
		{
			//supercharged
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			percent = 0.75f + sin(cg.time * 0.005f) * (extra / cent->gent->client->ps.forcePowerMax * 0.25f);
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else if (value <= 0) // no more
		{
			break;
		}
		else if (value < inc) // partial tic
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}

			percent = value / inc;
			calc_color[3] = percent;
		}
		else
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(SCREEN_WIDTH - (SCREEN_WIDTH - forceTics[i].xPos) * hud_ratio,
			forceTics[i].yPos,
			forceTics[i].width * hud_ratio,
			forceTics[i].height,
			forceTics[i].background);

		value -= inc;
	}

	if (flash)
	{
		cgi_R_SetColor(colorTable[CT_RED]);
	}
	else
	{
		cgi_R_SetColor(otherHUDBits[OHB_FORCEAMOUNT].color);
	}

	// Print force numeric amount
	CG_DrawNumField(
		SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_FORCEAMOUNT].xPos) * hud_ratio,
		otherHUDBits[OHB_FORCEAMOUNT].yPos,
		3,
		cent->gent->client->ps.forcePower,
		otherHUDBits[OHB_FORCEAMOUNT].width * hud_ratio,
		otherHUDBits[OHB_FORCEAMOUNT].height,
		NUM_FONT_SMALL,
		qfalse);
}

static void CG_DrawJK2ForcePower(const centity_t* cent, const int x, const int y)
{
	vec4_t calc_color;
	float extra = 0;
	float percent;

	if (!cent->gent->client->ps.forcePowersKnown)
	{
		return;
	}
	const float inc = static_cast<float>(cent->gent->client->ps.forcePowerMax) / MAX_DATAPADTICS;
	float value = cent->gent->client->ps.forcePower;
	if (value > cent->gent->client->ps.forcePowerMax)
	{
		//supercharged with force
		extra = value - cent->gent->client->ps.forcePowerMax;
		value = cent->gent->client->ps.forcePowerMax;
	}

	for (int i = MAX_DATAPADTICS - 1; i >= 0; i--)
	{
		if (extra)
		{
			//supercharged
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			percent = 0.75f + sin(cg.time * 0.005f) * (extra / cent->gent->client->ps.forcePowerMax * 0.25f);
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else if (value <= 0) // partial tic
		{
			memcpy(calc_color, colorTable[CT_BLACK], sizeof(vec4_t));
		}
		else if (value < inc) // partial tic
		{
			memcpy(calc_color, colorTable[CT_LTGREY], sizeof(vec4_t));
			percent = value / inc;
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else
		{
			memcpy(calc_color, colorTable[CT_LTGREY], sizeof(vec4_t));
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x + JK2forceTicPos[i].x,
			y + JK2forceTicPos[i].y,
			JK2forceTicPos[i].width,
			JK2forceTicPos[i].height,
			JK2forceTicPos[i].tic);

		value -= inc;
	}

	if (!cent->gent->client->ps.saberFatigueChainCount && !cent->gent->client->ps.BlasterAttackChainCount)
	{
		cgi_R_SetColor(otherHUDBits[OHB_JK2FORCEAMOUNT].color);

		// Print force numeric amount
		CG_DrawNumField(
			SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_JK2FORCEAMOUNT].xPos),
			otherHUDBits[OHB_JK2FORCEAMOUNT].yPos,
			3,
			cent->gent->client->ps.forcePower,
			otherHUDBits[OHB_JK2FORCEAMOUNT].width,
			otherHUDBits[OHB_JK2FORCEAMOUNT].height,
			NUM_FONT_SMALL,
			qfalse);
	}
}

static void CG_DrawJK2SaberFatigue(const centity_t* cent, const int x, const int y)
{
	if (cent->gent->health < 1)
	{
		return;
	}

	if (!cent->gent->client->ps.saberFatigueChainCount)
	{
		return;
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_HUDFLASH)
	{
		if (!(cg.time / 600 & 1))
		{
			if (!cg.messageLitActive)
			{
				cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.messageLitSound);
				cg.messageLitActive = qtrue;
			}

			cgi_R_SetColor(colorTable[CT_HUD_RED]);
			CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOn);
		}
		else
		{
			cg.messageLitActive = qfalse;
		}
	}
	else if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_TEN)
	{
		if (!(cg.time / 600 & 1))
		{
			if (!cg.messageLitActive)
			{
				cg.messageLitActive = qtrue;
			}

			cgi_R_SetColor(colorTable[CT_MAGENTA]);
			CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOn);
		}
		else
		{
			cg.messageLitActive = qfalse;
		}
	}
	else if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_SIX)
	{
		if (!(cg.time / 600 & 1))
		{
			if (!cg.messageLitActive)
			{
				cg.messageLitActive = qtrue;
			}

			cgi_R_SetColor(colorTable[CT_BLUE]);
			CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOn);
		}
		else
		{
			cg.messageLitActive = qfalse;
		}
	}
	else if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.saberFatigueChainCount >
		MISHAPLEVEL_TWO)
	{
		if (!(cg.time / 600 & 1))
		{
			if (!cg.messageLitActive)
			{
				cg.messageLitActive = qtrue;
			}

			cgi_R_SetColor(colorTable[CT_GREEN]);
			CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOn);
		}
		else
		{
			cg.messageLitActive = qfalse;
		}
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOff);
}

static void CG_DrawJK2GunFatigue(const centity_t* cent, const int x, const int y)
{
	if (cent->gent->health < 1)
	{
		return;
	}

	if (!cent->gent->client->ps.BlasterAttackChainCount)
	{
		return;
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_FULL)
	{
		if (!(cg.time / 600 & 1))
		{
			if (!cg.messageLitActive)
			{
				cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.messageLitSound);
				cg.messageLitActive = qtrue;
			}

			cgi_R_SetColor(colorTable[CT_HUD_RED]);
			CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOn);
		}
		else
		{
			cg.messageLitActive = qfalse;
		}
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x + 33, y + 41, 16, 16, cgs.media.messageLitOff);
}

static void CG_DrawSaberStyle(const centity_t* cent, const float hud_ratio)
{
	int index;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon != WP_SABER || !cent->gent)
	{
		return;
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (!cg.saberAnimLevelPending && cent->gent->client)
	{
		//uninitialized after a loadgame, cheat across and get it
		cg.saberAnimLevelPending = cent->gent->client->ps.saber_anim_level;
	}

	// don't need to draw ammo, but we will draw the current saber style in this window
	if (cg.saberAnimLevelPending == SS_FAST)
	{
		index = OHB_SABERSTYLE_FAST;
	}
	else if (cg.saberAnimLevelPending == SS_MEDIUM)
	{
		index = OHB_SABERSTYLE_MEDIUM;
	}
	else if (cg.saberAnimLevelPending == SS_TAVION)
	{
		index = OHB_SABERSTYLE_TAVION;
	}
	else if (cg.saberAnimLevelPending == SS_DESANN)
	{
		index = OHB_SABERSTYLE_DESANN;
	}
	else if (cg.saberAnimLevelPending == SS_STAFF)
	{
		index = OHB_SABERSTYLE_STAFF;
	}
	else if (cg.saberAnimLevelPending == SS_DUAL)
	{
		index = OHB_SABERSTYLE_DUAL;
	}
	else
	{
		index = OHB_SABERSTYLE_STRONG;
	}

	cgi_R_SetColor(otherHUDBits[index].color);

	CG_DrawPic(
		SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[index].xPos) * hud_ratio,
		otherHUDBits[index].yPos,
		otherHUDBits[index].width * hud_ratio,
		otherHUDBits[index].height,
		otherHUDBits[index].background
	);
}

static void CG_DrawJK2blockingMode(const centity_t* cent)
{
	int blockindex;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon != WP_SABER || !cent->gent)
	{
		return;
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (cg.predictedPlayerState.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK)
	{
		blockindex = OHB_JK2MBLOCKINGMODE;
	}
	else if (cg.predictedPlayerState.ManualBlockingFlags & 1 << HOLDINGBLOCK)
	{
		blockindex = OHB_JK2BLOCKINGMODE;
	}
	else
	{
		blockindex = OHB_JK2NOTBLOCKING;
	}

	cgi_R_SetColor(otherHUDBits[blockindex].color);

	CG_DrawPic(
		SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[blockindex].xPos),
		otherHUDBits[blockindex].yPos,
		otherHUDBits[blockindex].width,
		otherHUDBits[blockindex].height,
		otherHUDBits[blockindex].background
	);
}

static void cg_drawweapontype(const centity_t* cent)
{
	int wp_index;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon == WP_SABER || !cent->gent)
	{
		return;
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (cent->currentState.weapon == WP_STUN_BATON)
	{
		wp_index = OHB_STUN_BATON;
	}
	else if (cent->currentState.weapon == WP_BRYAR_PISTOL)
	{
		if (cg_com_kotor.integer == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_BPISTOL3;
		}
		else
		{
			if (cent->gent->client->charKOTORWeapons == 1)
			{
				wp_index = OHB_KOTOR_BPISTOL3;
			}
			else
			{
				wp_index = OHB_BRIAR_PISTOL;
			}
		}
	}
	else if (cent->currentState.weapon == WP_BLASTER_PISTOL)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_BPISTOL2;
		}
		else
		{
			wp_index = OHB_BLASTER_PISTOL;
		}
	}
	else if (cent->currentState.weapon == WP_BLASTER)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_BRIFLE2;
		}
		else
		{
			wp_index = OHB_BLASTER;
		}
	}
	else if (cent->currentState.weapon == WP_DISRUPTOR)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_BRIFLE3;
		}
		else
		{
			wp_index = OHB_DISRUPTOR;
		}
	}
	else if (cent->currentState.weapon == WP_BOWCASTER)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_BOWCASTER;
		}
		else
		{
			wp_index = OHB_BOWCASTER;
		}
	}
	else if (cent->currentState.weapon == WP_REPEATER)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_REPEATER;
		}
		else
		{
			wp_index = OHB_REPEATER;
		}
	}
	else if (cent->currentState.weapon == WP_DEMP2)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_IONRIFLE;
		}
		else
		{
			wp_index = OHB_DEMP2;
		}
	}
	else if (cent->currentState.weapon == WP_FLECHETTE)
	{
		wp_index = OHB_FLACHETTE;
	}
	else if (cent->currentState.weapon == WP_ROCKET_LAUNCHER)
	{
		wp_index = OHB_ROCKET;
	}
	else if (cent->currentState.weapon == WP_THERMAL)
	{
		wp_index = OHB_THERMAL;
	}
	else if (cent->currentState.weapon == WP_TRIP_MINE)
	{
		wp_index = OHB_TRIPMINE;
	}
	else if (cent->currentState.weapon == WP_DET_PACK)
	{
		wp_index = OHB_DETPACK;
	}
	else if (cent->currentState.weapon == WP_CONCUSSION)
	{
		wp_index = OHB_CONCUSSION;
	}
	else if (cent->currentState.weapon == WP_MELEE)
	{
		wp_index = OHB_MELEE;
	}
	else if (cent->currentState.weapon == WP_BATTLEDROID)
	{
		wp_index = OHB_BATTLEDROID;
	}
	else if (cent->currentState.weapon == WP_THEFIRSTORDER)
	{
		wp_index = OHB_THEFIRSTORDER;
	}
	else if (cent->currentState.weapon == WP_CLONECARBINE)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_BRIFLE1;
		}
		else
		{
			wp_index = OHB_CLONECARBINE;
		}
	}
	else if (cent->currentState.weapon == WP_REBELBLASTER)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_HPISTOL;
		}
		else
		{
			wp_index = OHB_REBELBLASTER;
		}
	}
	else if (cent->currentState.weapon == WP_CLONERIFLE)
	{
		wp_index = OHB_CLONERIFLE;
	}
	else if (cent->currentState.weapon == WP_CLONECOMMANDO)
	{
		wp_index = OHB_CLONECOMMANDO;
	}
	else if (cent->currentState.weapon == WP_Z6_ROTARY_CANNON)
	{
		wp_index = OHB_Z6_ROTARY_CANNON;
	}
	else if (cent->currentState.weapon == WP_REBELRIFLE)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_DRIFLE;
		}
		else
		{
			wp_index = OHB_REBELRIFLE;
		}
	}
	else if (cent->currentState.weapon == WP_REY)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_HOBPISTOL;
		}
		else
		{
			wp_index = OHB_REY;
		}
	}
	else if (cent->currentState.weapon == WP_JANGO)
	{
		wp_index = OHB_JANGO;
	}
	else if (cent->currentState.weapon == WP_BOBA)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_REPRIFLE;
		}
		else
		{
			wp_index = OHB_BOBA;
		}
	}
	else if (cent->currentState.weapon == WP_CLONEPISTOL)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_PISTOL1;
		}
		else
		{
			wp_index = OHB_CLONEPISTOL;
		}
	}
	else if (cent->currentState.weapon == WP_DUAL_CLONEPISTOL)
	{
		if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
		{
			wp_index = OHB_KOTOR_PISTOL1;
		}
		else
		{
			wp_index = OHB_CLONEPISTOL;
		}
	}
	else if (cent->currentState.weapon == WP_WRIST_BLASTER)
	{
		wp_index = OHB_WRIST;
	}
	else if (cent->currentState.weapon == WP_DUAL_PISTOL)
	{
		wp_index = OHB_DUAL;
	}
	else if (cent->currentState.weapon == WP_NOGHRI_STICK)
	{
		wp_index = OHB_NOGRI;
	}
	else if (cent->currentState.weapon == WP_TUSKEN_RIFLE)
	{
		wp_index = OHB_TUSKEN;
	}
	else if (cent->currentState.weapon == WP_DROIDEKA)
	{
		wp_index = OHB_DEKA;
	}
	else
	{
		wp_index = OHB_MELEE;
	}

	cgi_R_SetColor(otherHUDBits[wp_index].color);

	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawPic(
			SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[wp_index].xPos),
			otherHUDBits[wp_index].yPos,
			otherHUDBits[wp_index].width,
			otherHUDBits[wp_index].height,
			otherHUDBits[wp_index].background
		);
	}
	else // horizontal
	{
		CG_DrawPic(
			SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[wp_index].xPos) - 12,
			otherHUDBits[wp_index].yPos + 18,
			otherHUDBits[wp_index].width,
			otherHUDBits[wp_index].height,
			otherHUDBits[wp_index].background
		);
	}
}

static void CG_DrawClassicsaberfatigue(const centity_t* cent, const float hud_ratio)
{
	vec4_t calc_color;
	constexpr int saber_fatigue_chain_count = MISHAPLEVEL_MAX;
	qboolean flash = qfalse;

	if (cent->gent->health < 1)
	{
		return;
	}

	if (!cent->gent->client->ps.saberFatigueChainCount)
	{
		return;
	}

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_HUDFLASH)
	{
		flash = qtrue;
		if (cg.mishapHUDNextFlashTime < cg.time)
		{
			cg.mishapHUDNextFlashTime = cg.time + 400;

			if (cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_MAX)
			{
				cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.overload);
			}

			if (cg.mishapHUDActive)
			{
				cg.mishapHUDActive = qfalse;
			}
			else
			{
				cg.mishapHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.mishapHUDNextFlashTime = 0;
		cg.mishapHUDActive = qtrue;
	}

	constexpr float inc = static_cast<float>(saber_fatigue_chain_count) / MAX_SJEHUD_TICS;
	float value = cent->gent->client->ps.saberFatigueChainCount;

	for (int i = MAX_SJEHUD_TICS - 1; i >= 0; i--)
	{
		if (value <= 0) // no more
		{
			break;
		}
		if (value < inc) // partial tic
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}

			const float percent = value / inc;
			calc_color[3] = percent;
		}
		else
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(
			SCREEN_WIDTH - (SCREEN_WIDTH - classicsabfatticS[i].xPos) * hud_ratio,
			classicsabfatticS[i].yPos,
			classicsabfatticS[i].width * hud_ratio,
			classicsabfatticS[i].height,
			classicsabfatticS[i].background);

		value -= inc;
	}
}

static void CG_DrawNewClassicsaberfatigue(const centity_t* cent, const float hud_ratio)
{
	vec4_t calc_color;
	constexpr int saber_fatigue_chain_count = MISHAPLEVEL_MAX;
	qboolean flash = qfalse;

	if (cent->gent->health < 1)
	{
		return;
	}

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.mishapHUDTotalFlashTime > cg.time || cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_HUDFLASH)
	{
		flash = qtrue;
		if (cg.mishapHUDNextFlashTime < cg.time)
		{
			cg.mishapHUDNextFlashTime = cg.time + 400;

			if (cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_MAX)
			{
				cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.overload);
			}

			if (cg.mishapHUDActive)
			{
				cg.mishapHUDActive = qfalse;
			}
			else
			{
				cg.mishapHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.mishapHUDNextFlashTime = 0;
		cg.mishapHUDActive = qtrue;
	}

	constexpr float inc = static_cast<float>(saber_fatigue_chain_count) / MAX_SJEHUD_TICS;
	float value = cent->gent->client->ps.saberFatigueChainCount;

	for (int i = MAX_SJEHUD_TICS - 1; i >= 0; i--)
	{
		if (value < inc) // partial tic
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}

			const float percent = value / inc;
			calc_color[3] = percent;
		}
		else
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(
			SCREEN_WIDTH - (SCREEN_WIDTH - newsabfatticS[i].xPos) * hud_ratio,
			newsabfatticS[i].yPos,
			newsabfatticS[i].width * hud_ratio,
			newsabfatticS[i].height,
			newsabfatticS[i].background);

		value -= inc;
	}
}

//draw meter showing blockPoints when it's not full
constexpr auto BPFUELBAR_H = 100.0f;
constexpr auto BPFUELBAR_W = 15.0f;
#define BPFUELBAR_X			(SCREEN_WIDTH-BPFUELBAR_W-8.0f)
constexpr auto BPFUELBAR_Y = 250.0f;

static void CG_DrawoldblockPoints(const centity_t* cent)
{
	vec4_t aColor{};
	vec4_t b_color{};
	vec4_t cColor{};
	float x = BPFUELBAR_X;
	constexpr float y = BPFUELBAR_Y;

	float percent = static_cast<float>(cg.snap->ps.blockPoints) / 100.0f * BPFUELBAR_H;

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.blockHUDTotalFlashTime > cg.time || cg.snap->ps.blockPoints < BLOCKPOINTS_WARNING)
	{
		if (cg.blockHUDNextFlashTime < cg.time)
		{
			cg.blockHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.overload);

			if (cg.blockHUDActive)
			{
				cg.blockHUDActive = qfalse;
			}
			else
			{
				cg.blockHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.blockHUDNextFlashTime = 0;
		cg.blockHUDActive = qtrue;
	}

	if (cent->gent->health < 1)
	{
		return;
	}

	if (percent > BPFUELBAR_H)
	{
		return;
	}

	if (cg.snap->ps.cloakFuel < 100)
	{
		//if drawing jetpack fuel bar too, then move this over...?
		x -= BPFUELBAR_W + 16.0f;
	}

	if (cent->gent->client->ps.saberFatigueChainCount > MISHAPLEVEL_MIN)
	{
		//if drawing then move this over...?
		x -= BPFUELBAR_W + 8.0f;
	}

	if (percent < 0.1f)
	{
		percent = 0.1f;
	}

	if (cg.snap->ps.blockPoints < BLOCKPOINTS_FATIGUE)
	{
		//color of the bar //red
		aColor[0] = 1.0f;
		aColor[1] = 0.0f;
		aColor[2] = 0.0f;
		aColor[3] = 0.4f;
	}
	else if (cg.snap->ps.blockPoints < BLOCKPOINTS_HALF)
	{
		//color of the bar
		aColor[0] = 0.8f;
		aColor[1] = 0.0f;
		aColor[2] = 0.6f;
		aColor[3] = 0.8f;
	}
	else if (cg.snap->ps.blockPoints < BLOCKPOINTS_MISSILE)
	{
		//color of the bar
		aColor[0] = 0.4f;
		aColor[1] = 0.0f;
		aColor[2] = 0.6f;
		aColor[3] = 0.8f;
	}
	else
	{
		//color of the bar
		aColor[0] = 85.0f;
		aColor[1] = 0.0f;
		aColor[2] = 85.0f;
		aColor[3] = 0.4f;
	}

	//color of the border
	b_color[0] = 0.0f;
	b_color[1] = 0.0f;
	b_color[2] = 0.0f;
	b_color[3] = 0.3f;

	//color of greyed out "missing fuel"
	cColor[0] = 0.1f;
	cColor[1] = 0.1f;
	cColor[2] = 0.3f;
	cColor[3] = 0.1f;

	//draw the background (black)
	CG_DrawRect(x, y, BPFUELBAR_W, BPFUELBAR_H, 1.0f, colorTable[CT_BLACK]);

	//now draw the part to show how much fuel there is in the color specified
	CG_FillRect(x + 1.0f, y + 1.0f + (BPFUELBAR_H - percent), BPFUELBAR_W - 1.0f,
		BPFUELBAR_H - 1.0f - (BPFUELBAR_H - percent), aColor);

	//then draw the other part greyed out
	CG_FillRect(x + 1.0f, y + 1.0f, BPFUELBAR_W - 1.0f, BPFUELBAR_H - percent, cColor);
}

static void CG_DrawJK2blockPoints(const int x, const int y)
{
	vec4_t calc_color;

	//	Outer block circular
	//==========================================================================================================//

	if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
	{
		//blockingflag is on
		memcpy(calc_color, colorTable[CT_GREEN], sizeof(vec4_t));
	}
	else
	{
		memcpy(calc_color, colorTable[CT_MAGENTA], sizeof(vec4_t));
	}

	const float hold = cg.snap->ps.blockPoints - BLOCK_POINTS_MAX / 2;
	float block_percent = static_cast<float>(hold) / (static_cast<float>(BLOCK_POINTS_MAX) / 2);

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.blockHUDTotalFlashTime > cg.time || cg.snap->ps.blockPoints < BLOCKPOINTS_WARNING)
	{
		if (cg.blockHUDNextFlashTime < cg.time)
		{
			cg.blockHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.overload);

			if (cg.blockHUDActive)
			{
				cg.blockHUDActive = qfalse;
			}
			else
			{
				cg.blockHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.blockHUDNextFlashTime = 0;
		cg.blockHUDActive = qtrue;
	}

	if (block_percent < 0)
	{
		block_percent = 0;
	}

	calc_color[0] *= block_percent;
	calc_color[1] *= block_percent;
	calc_color[2] *= block_percent;

	cgi_R_SetColor(calc_color);

	if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
	{
		//blockingflag is on
		CG_DrawPic(x, y, 35, 35, cgs.media.HUDblockpointMB1);
	}
	else
	{
		CG_DrawPic(x, y, 35, 35, cgs.media.HUDblockpoint1);
	}

	// Inner block circular
	//==========================================================================================================//
	if (block_percent > 0)
	{
		block_percent = 1;
	}
	else
	{
		block_percent = static_cast<float>(cg.snap->ps.blockPoints) / (static_cast<float>(BLOCK_POINTS_MAX) / 2);
	}

	if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
	{
		//blockingflag is on
		memcpy(calc_color, colorTable[CT_GREEN], sizeof(vec4_t));
	}
	else
	{
		memcpy(calc_color, colorTable[CT_MAGENTA], sizeof(vec4_t));
	}

	calc_color[0] *= block_percent;
	calc_color[1] *= block_percent;
	calc_color[2] *= block_percent;

	cgi_R_SetColor(calc_color);

	if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
	{
		//blockingflag is on
		CG_DrawPic(x, y, 35, 35, cgs.media.HUDblockpointMB2);
	}
	else
	{
		CG_DrawPic(x, y, 35, 35, cgs.media.HUDblockpoint2);
	}

	// Print force health amount
	cgi_R_SetColor(otherHUDBits[OHB_JK2BLOCKAMOUNT].color);

	CG_DrawNumField(
		otherHUDBits[OHB_JK2BLOCKAMOUNT].xPos,
		otherHUDBits[OHB_JK2BLOCKAMOUNT].yPos,
		3,
		cg.snap->ps.blockPoints,
		otherHUDBits[OHB_JK2BLOCKAMOUNT].width,
		otherHUDBits[OHB_JK2BLOCKAMOUNT].height,
		NUM_FONT_SMALL,
		qfalse);
}

static void CG_DrawClassicblockPoints(const centity_t* cent, const float hud_ratio)
{
	vec4_t calc_color;
	float extra = 0, percent;

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.blockHUDTotalFlashTime > cg.time || cg.snap->ps.blockPoints < BLOCKPOINTS_WARNING)
	{
		if (cg.blockHUDNextFlashTime < cg.time)
		{
			cg.blockHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.overload);

			if (cg.blockHUDActive)
			{
				cg.blockHUDActive = qfalse;
			}
			else
			{
				cg.blockHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.blockHUDNextFlashTime = 0;
		cg.blockHUDActive = qtrue;
	}

	constexpr float inc = static_cast<float>(BLOCK_POINTS_MAX) / MAX_HUD_TICS;

	float value = cent->gent->client->ps.blockPoints;
	if (value > BLOCK_POINTS_MAX)
	{
		//supercharged with force
		extra = value - BLOCK_POINTS_MAX;
		value = BLOCK_POINTS_MAX;
	}

	for (int i = MAX_HUD_TICS - 1; i >= 0; i--)
	{
		if (extra)
		{
			//supercharged
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			percent = 0.75f + sin(cg.time * 0.005f) * (extra / BLOCK_POINTS_MAX * 0.25f);
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else if (value <= 0) // no more
		{
			break;
		}
		else if (value < inc) // partial tic
		{
			if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
			{
				//blockingflag is on
				memcpy(calc_color, colorTable[CT_GREEN], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_MAGENTA], sizeof(vec4_t));
			}

			percent = value / inc;
			calc_color[3] = percent;
		}
		else
		{
			if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
			{
				//blockingflag is on
				memcpy(calc_color, colorTable[CT_GREEN], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_MAGENTA], sizeof(vec4_t));
			}
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(SCREEN_WIDTH - (SCREEN_WIDTH - blockpoint_tics[i].xPos) * hud_ratio,
			blockpoint_tics[i].yPos,
			blockpoint_tics[i].width * hud_ratio,
			blockpoint_tics[i].height,
			blockpoint_tics[i].background);

		value -= inc;
	}

	{
		cgi_R_SetColor(otherHUDBits[OHB_CLASSICBLOCKAMOUNT].color);
	}

	CG_DrawNumField(
		otherHUDBits[OHB_CLASSICBLOCKAMOUNT].xPos * hud_ratio,
		otherHUDBits[OHB_CLASSICBLOCKAMOUNT].yPos,
		3,
		cg.snap->ps.blockPoints,
		otherHUDBits[OHB_CLASSICBLOCKAMOUNT].width * hud_ratio,
		otherHUDBits[OHB_CLASSICBLOCKAMOUNT].height,
		NUM_FONT_SMALL,
		qfalse);
}

// new df hud

static void CG_DrawDF_Pic(const int index)
{
	cgi_R_SetColor(otherHUDBits[index].color);
	CG_DrawPic(
		otherHUDBits[index].xPos,
		otherHUDBits[index].yPos,
		otherHUDBits[index].width,
		otherHUDBits[index].height,
		otherHUDBits[index].background
	);
}

static void CG_DrawDF_RotatePic(const int index, const int rotate, const int offset_x, const int offset_y, const float scale_w,
	const float scale_h)
{
	cgi_R_SetColor(otherHUDBits[index].color);
	CG_DrawRotatePic2(
		otherHUDBits[index].xPos + offset_x,
		otherHUDBits[index].yPos + offset_y,
		otherHUDBits[index].width * scale_w,
		otherHUDBits[index].height * scale_h,
		rotate,
		otherHUDBits[index].background
	);
}

// left hud
static void CG_DrawDF_Health()
{
	vec4_t calc_color;
	const playerState_t* ps = &cg.snap->ps;
	int offset_y = 21;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = static_cast<float>(ps->stats[STAT_MAX_HEALTH]) / MAX_DFHUDTICS;
	float cur_value = ps->stats[STAT_HEALTH];
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (cur_value <= 0) // don't show tic
		{
			break;
		}
		if (cur_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_health_tics[i].color, sizeof(vec4_t));
			const float percent = cur_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(
				df_health_tics[i].xPos,
				df_health_tics[i].yPos,
				df_health_tics[i].width,
				df_health_tics[i].height,
				df_health_tics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 455;
			int offset_correction = 0;
			if (i == 0)
			{
				offset_correction = 1;
			}
			offset_y += df_health_tics[i].height;
			CG_DrawRotatePic2(
				df_health_tics[i].xPos - df_health_tics[i].yPos + offset_x - offset_correction,
				df_health_tics[i].yPos + offset_y,
				df_health_tics[i].width,
				df_health_tics[i].height + offset_correction,
				90,
				df_health_tics[i].background
			);
		}
		cur_value -= inc;
	}

	// Print force health amount
	cgi_R_SetColor(otherHUDBits[OHB_HEALTHAMOUNT_DF].color);

	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawNumField(
			otherHUDBits[OHB_HEALTHAMOUNT_DF].xPos,
			otherHUDBits[OHB_HEALTHAMOUNT_DF].yPos,
			3,
			ps->stats[STAT_HEALTH],
			otherHUDBits[OHB_HEALTHAMOUNT_DF].width,
			otherHUDBits[OHB_HEALTHAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_Pic(OHB_HEALTH_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawNumField(
			otherHUDBits[OHB_HEALTHAMOUNT_DF].xPos + 3,
			otherHUDBits[OHB_HEALTHAMOUNT_DF].yPos - 18,
			3,
			ps->stats[STAT_HEALTH],
			otherHUDBits[OHB_HEALTHAMOUNT_DF].width,
			otherHUDBits[OHB_HEALTHAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_RotatePic(OHB_HEALTH_ICON_DF, 0, 52, 36, 1, 1);
	}
}

static void CG_DrawDF_Armor()
{
	vec4_t calc_color;
	const playerState_t* ps = &cg.snap->ps;
	int offset_y = 43;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = static_cast<float>(ps->stats[STAT_MAX_HEALTH]) / MAX_DFHUDTICS;
	float cur_value = ps->stats[STAT_ARMOR];
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (cur_value <= 0) // don't show tic
		{
			break;
		}
		if (cur_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_armorTics[i].color, sizeof(vec4_t));
			const float percent = cur_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);
		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(
				df_armorTics[i].xPos,
				df_armorTics[i].yPos,
				df_armorTics[i].width,
				df_armorTics[i].height,
				df_armorTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 432;
			int offset_correction = 0;
			if (i == 0)
			{
				offset_correction = 1;
			}
			offset_y += df_armorTics[i].height;
			CG_DrawRotatePic2(
				df_armorTics[i].xPos - df_armorTics[i].yPos + offset_x - offset_correction,
				df_armorTics[i].yPos + offset_y,
				df_armorTics[i].width,
				df_armorTics[i].height + offset_correction,
				90,
				df_armorTics[i].background
			);
		}
		cur_value -= inc;
	}

	// Print armor amount
	cgi_R_SetColor(otherHUDBits[OHB_ARMORAMOUNT_DF].color);

	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawNumField(
			otherHUDBits[OHB_ARMORAMOUNT_DF].xPos,
			otherHUDBits[OHB_ARMORAMOUNT_DF].yPos,
			3,
			ps->stats[STAT_ARMOR],
			otherHUDBits[OHB_ARMORAMOUNT_DF].width,
			otherHUDBits[OHB_ARMORAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_Pic(OHB_ARMOR_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawNumField(
			otherHUDBits[OHB_ARMORAMOUNT_DF].xPos - 30,
			otherHUDBits[OHB_ARMORAMOUNT_DF].yPos + 3,
			3,
			ps->stats[STAT_ARMOR],
			otherHUDBits[OHB_ARMORAMOUNT_DF].width,
			otherHUDBits[OHB_ARMORAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_RotatePic(OHB_ARMOR_ICON_DF, 0, 29, 58, 1, 1);
	}

	// If armor is low, flash a graphic to warn the player
	if (ps->stats[STAT_ARMOR]) // Is there armor? Draw the HUD Armor TIC
	{
		const float quarter_armor = ps->stats[STAT_MAX_HEALTH] / 4.0f;

		// Make tic flash if armor is at 25% of full armor
		if (ps->stats[STAT_ARMOR] < quarter_armor) // Do whatever the flash timer says
		{
			if (cg.HUDTickFlashTime < cg.time) // Flip at the same time
			{
				cg.HUDTickFlashTime = cg.time + 400;
				if (cg.HUDArmorFlag)
				{
					cg.HUDArmorFlag = qfalse;
				}
				else
				{
					cg.HUDArmorFlag = qtrue;
				}
			}
		}
		else
		{
			cg.HUDArmorFlag = qtrue;
		}
	}
	else // No armor? Don't show it.
	{
		cg.HUDArmorFlag = qfalse;
	}
}

static void CG_DrawDF_Stamina_Sprint()
{
	vec4_t calc_color;
	int offset_y = 32;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	constexpr float inc = static_cast<float>(PLAYER_FUEL_MAX) / MAX_DFHUDTICS;
	float cur_value = cg.snap->ps.sprintFuel;
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (cur_value <= 0) // don't show tic
		{
			break;
		}
		if (cur_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_staminaTics[i].color, sizeof(vec4_t));
			const float percent = cur_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(
				df_staminaTics[i].xPos,
				df_staminaTics[i].yPos,
				df_staminaTics[i].width,
				df_staminaTics[i].height,
				df_staminaTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 444;
			offset_y += df_staminaTics[i].height;
			CG_DrawRotatePic2(
				df_staminaTics[i].xPos - df_staminaTics[i].yPos + offset_x,
				df_staminaTics[i].yPos + offset_y,
				df_staminaTics[i].width,
				df_staminaTics[i].height,
				90,
				df_staminaTics[i].background
			);
		}
		cur_value -= inc;
	}
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawDF_Pic(OHB_STAMINA_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawDF_RotatePic(OHB_STAMINA_ICON_DF, 0, 41, 47, 1, 1);
	}
}

static void CG_DrawDF_JetPackFuel()
{
	vec4_t calc_color;
	int offset_y = 32;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	constexpr float inc = static_cast<float>(PLAYER_FUEL_MAX) / MAX_DFHUDTICS;
	float cur_value = cg.snap->ps.jetpackFuel;
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (cur_value <= 0) // don't show tic
		{
			break;
		}
		if (cur_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_fuelTics[i].color, sizeof(vec4_t));
			const float percent = cur_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(
				df_fuelTics[i].xPos,
				df_fuelTics[i].yPos,
				df_fuelTics[i].width,
				df_fuelTics[i].height,
				df_fuelTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 444;
			offset_y += df_fuelTics[i].height;
			CG_DrawRotatePic2(
				df_fuelTics[i].xPos - df_fuelTics[i].yPos + offset_x,
				df_fuelTics[i].yPos + offset_y,
				df_fuelTics[i].width,
				df_fuelTics[i].height,
				90,
				df_fuelTics[i].background
			);
		}
		cur_value -= inc;
	}
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawDF_Pic(OHB_FUEL_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawDF_RotatePic(OHB_FUEL_ICON_DF, 0, 41, 47, 1, 1);
	}
}

static void CG_DrawDF_CloakFuel()
{
	vec4_t calc_color;
	int offset_y = 32;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	constexpr float inc = static_cast<float>(PLAYER_FUEL_MAX) / MAX_DFHUDTICS;
	float cur_value = cg.snap->ps.cloakFuel;
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (cur_value <= 0) // don't show tic
		{
			break;
		}
		if (cur_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_cloakTics[i].color, sizeof(vec4_t));
			const float percent = cur_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(
				df_cloakTics[i].xPos,
				df_cloakTics[i].yPos,
				df_cloakTics[i].width,
				df_cloakTics[i].height,
				df_cloakTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 444;
			offset_y += df_cloakTics[i].height;
			CG_DrawRotatePic2(
				df_cloakTics[i].xPos - df_cloakTics[i].yPos + offset_x,
				df_cloakTics[i].yPos + offset_y,
				df_cloakTics[i].width,
				df_cloakTics[i].height,
				90,
				df_cloakTics[i].background
			);
		}
		cur_value -= inc;
	}
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawDF_Pic(OHB_CLOAK_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawDF_RotatePic(OHB_CLOAK_ICON_DF, 0, 41, 47, 1, 1);
	}
}

static void CG_DrawDF_BarrierFuel()
{
	vec4_t calc_color;
	int offset_y = 32;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	constexpr float inc = static_cast<float>(PLAYER_FUEL_MAX) / MAX_DFHUDTICS;
	float cur_value = cg.snap->ps.BarrierFuel;
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (cur_value <= 0) // don't show tic
		{
			break;
		}
		if (cur_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_barrierTics[i].color, sizeof(vec4_t));
			const float percent = cur_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(
				df_barrierTics[i].xPos,
				df_barrierTics[i].yPos,
				df_barrierTics[i].width,
				df_barrierTics[i].height,
				df_barrierTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 444;
			offset_y += df_barrierTics[i].height;
			CG_DrawRotatePic2(
				df_barrierTics[i].xPos - df_barrierTics[i].yPos + offset_x,
				df_barrierTics[i].yPos + offset_y,
				df_barrierTics[i].width,
				df_barrierTics[i].height,
				90,
				df_barrierTics[i].background
			);
		}
		cur_value -= inc;
	}
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawDF_Pic(OHB_BARRIER_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawDF_RotatePic(OHB_BARRIER_ICON_DF, 0, 41, 47, 1, 1);
	}
}

static void CG_DrawDF_GunFatigue(const centity_t* cent)
{
	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	cgi_R_SetColor(otherHUDBits[OHB_BLASTERSTYLE_DF].color);

	// Show weapon fatigue by changing height and position of blaster style image
	float fatigue_percent = static_cast<float>(cent->gent->client->ps.BlasterAttackChainCount) / (BLASTERMISHAPLEVEL_MAX + 1);

	if (fatigue_percent < 0)
	{
		fatigue_percent = 0.0f;
	}
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawPic(
			SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_BLASTERSTYLE_DF].xPos),
			otherHUDBits[OHB_BLASTERSTYLE_DF].yPos + fatigue_percent * 100,
			otherHUDBits[OHB_BLASTERSTYLE_DF].width,
			otherHUDBits[OHB_BLASTERSTYLE_DF].height - fatigue_percent * 100,
			otherHUDBits[OHB_BLASTERSTYLE_DF].background
		);
	}
	else // horizontal
	{
		constexpr int offset_y = 134;
		constexpr int offset_x = 60;
		CG_DrawRotatePic2(
			SCREEN_WIDTH - (SCREEN_WIDTH + offset_x - otherHUDBits[OHB_BLASTERSTYLE_DF].xPos) + fatigue_percent * 60,
			otherHUDBits[OHB_BLASTERSTYLE_DF].yPos + offset_y,
			otherHUDBits[OHB_BLASTERSTYLE_DF].width,
			otherHUDBits[OHB_BLASTERSTYLE_DF].height - fatigue_percent * 100,
			-90,
			otherHUDBits[OHB_BLASTERSTYLE_DF].background,
			cgs.widthRatioCoef
		);
	}
}

// new df hud// right hud

static void CG_DrawDF_Ammo(const centity_t* cent)
{
	vec4_t calc_color;
	int offset_y = 21;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon == WP_STUN_BATON)
	{
		return;
	}

	const playerState_t* ps = &cg.snap->ps;
	float curr_value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	if (curr_value < 0) // No ammo
	{
		return;
	}

	const float inc = static_cast<float>(ammoData[weaponData[cent->currentState.weapon].ammoIndex].max) / MAX_DFHUDTICS;
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));

	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, df_ammoTics[i].color, sizeof(vec4_t));
			const float percent = curr_value / inc;
			calc_color[3] *= percent;
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(df_ammoTics[i].xPos,
				df_ammoTics[i].yPos,
				df_ammoTics[i].width,
				df_ammoTics[i].height,
				df_ammoTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 442;
			int offset_correction = 0;
			if (i == 0)
			{
				offset_correction = 1;
			}
			offset_y += df_ammoTics[i].height;
			CG_DrawRotatePic2(
				df_ammoTics[i].xPos + df_ammoTics[i].yPos - offset_x + offset_correction,
				df_ammoTics[i].yPos + offset_y,
				df_ammoTics[i].width,
				df_ammoTics[i].height + offset_correction,
				-90,
				df_ammoTics[i].background
			);
		}
		curr_value -= inc;
	}

	// Determine the color of the numeric field

	if (cg.oldammo < curr_value)
	{
		cg.oldAmmoTime = cg.time + 200;
	}

	cg.oldammo = curr_value;

	// Firing or reloading?
	if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING
		&& cg.predictedPlayerState.weaponTime > 100)
	{
		memcpy(calc_color, colorTable[CT_LTGREY], sizeof(vec4_t));
	}
	else
	{
		if (curr_value > 0)
		{
			if (cg.oldAmmoTime > cg.time)
			{
				memcpy(calc_color, colorTable[CT_YELLOW], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, otherHUDBits[OHB_AMMOAMOUNT_DF].color, sizeof(vec4_t));
			}
		}
		else
		{
			memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
		}
	}

	// Print number amount
	cgi_R_SetColor(calc_color);

	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawNumField(
			otherHUDBits[OHB_AMMOAMOUNT_DF].xPos,
			otherHUDBits[OHB_AMMOAMOUNT_DF].yPos,
			3,
			ps->ammo[weaponData[cent->currentState.weapon].ammoIndex],
			otherHUDBits[OHB_AMMOAMOUNT_DF].width,
			otherHUDBits[OHB_AMMOAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_Pic(OHB_AMMO_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawNumField(
			otherHUDBits[OHB_AMMOAMOUNT_DF].xPos + 14,
			otherHUDBits[OHB_AMMOAMOUNT_DF].yPos - 8,
			3,
			ps->ammo[weaponData[cent->currentState.weapon].ammoIndex],
			otherHUDBits[OHB_AMMOAMOUNT_DF].width,
			otherHUDBits[OHB_AMMOAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_RotatePic(OHB_AMMO_ICON_DF, 0, -45, 36, 1, 1);
	}
}

static void CG_DrawDF_BlockPoints(const centity_t* cent)
{
	qboolean flash = qfalse;
	vec4_t calc_color;
	float percent;
	const playerState_t* ps = &cg.snap->ps;
	int offset_y = 21;

	if (G_IsRidingVehicle(cent->gent))
	{
		return;
	}

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.forceHUDTotalFlashTime > cg.time)
	{
		flash = qtrue;
		if (cg.forceHUDNextFlashTime < cg.time)
		{
			cg.forceHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.noforceSound);
			if (cg.forceHUDActive)
			{
				cg.forceHUDActive = qfalse;
			}
			else
			{
				cg.forceHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.forceHUDNextFlashTime = 0;
		cg.forceHUDActive = qtrue;
	}

	const float inc = static_cast<float>(BLOCK_POINTS_MAX) / MAX_DFHUDTICS;
	float value = cent->gent->client->ps.blockPoints;

	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		constexpr float extra = 0;
		if (extra)
		{
			//supercharged
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			percent = 0.75f + sin(cg.time * 0.005f) * (extra / BLOCK_POINTS_MAX * 0.25f);
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}

		if (value <= 0) // no more
		{
			break;
		}
		if (value < inc) // partial tic
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}

			percent = value / inc;
			calc_color[3] = percent;
		}
		else
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(df_blockTics[i].xPos,
				df_blockTics[i].yPos,
				df_blockTics[i].width,
				df_blockTics[i].height,
				df_blockTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 442;
			int offset_correction = 0;
			if (i == 0)
			{
				offset_correction = 1;
			}
			offset_y += df_blockTics[i].height;
			CG_DrawRotatePic2(
				df_blockTics[i].xPos + df_blockTics[i].yPos - offset_x + offset_correction,
				df_blockTics[i].yPos + offset_y,
				df_blockTics[i].width,
				df_blockTics[i].height + offset_correction,
				-90,
				df_blockTics[i].background
			);
		}
		value -= inc;
	}

	if (flash)
	{
		cgi_R_SetColor(colorTable[CT_RED]);
	}
	else
	{
		cgi_R_SetColor(otherHUDBits[OHB_BLOCKAMOUNT_DF].color);
	}

	// Print force numeric amount
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawNumField(
			otherHUDBits[OHB_BLOCKAMOUNT_DF].xPos,
			otherHUDBits[OHB_BLOCKAMOUNT_DF].yPos,
			3,
			cent->gent->client->ps.blockPoints,
			otherHUDBits[OHB_BLOCKAMOUNT_DF].width,
			otherHUDBits[OHB_BLOCKAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_Pic(OHB_BLOCK_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawNumField(
			otherHUDBits[OHB_BLOCKAMOUNT_DF].xPos - 3,
			otherHUDBits[OHB_BLOCKAMOUNT_DF].yPos - 18,
			3,
			cent->gent->client->ps.blockPoints,
			otherHUDBits[OHB_BLOCKAMOUNT_DF].width,
			otherHUDBits[OHB_BLOCKAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_RotatePic(OHB_BLOCK_ICON_DF, 0, -45, 36, 1, 1);
	}
}

static void CG_DrawDF_ForcePowers(const centity_t* cent)
{
	qboolean flash = qfalse;
	vec4_t calc_color;
	float extra = 0, percent;
	int offset_y = 43;

	if (!cent->gent->client->ps.forcePowersKnown || G_IsRidingVehicle(cent->gent))
	{
		return;
	}

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.forceHUDTotalFlashTime > cg.time)
	{
		flash = qtrue;
		if (cg.forceHUDNextFlashTime < cg.time)
		{
			cg.forceHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.noforceSound);
			if (cg.forceHUDActive)
			{
				cg.forceHUDActive = qfalse;
			}
			else
			{
				cg.forceHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.forceHUDNextFlashTime = 0;
		cg.forceHUDActive = qtrue;
	}

	const float inc = static_cast<float>(cent->gent->client->ps.forcePowerMax) / MAX_DFHUDTICS;
	float value = cent->gent->client->ps.forcePower;

	if (value > cent->gent->client->ps.forcePowerMax)
	{
		//supercharged with force
		extra = value - cent->gent->client->ps.forcePowerMax;
		value = cent->gent->client->ps.forcePowerMax;
	}

	for (int i = MAX_DFHUDTICS - 1; i >= 0; i--)
	{
		if (extra)
		{
			//supercharged
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			percent = 0.75f + sin(cg.time * 0.005f) * (extra / cent->gent->client->ps.forcePowerMax * 0.25f);
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else if (value <= 0) // no more
		{
			break;
		}
		else if (value < inc) // partial tic
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}

			percent = value / inc;
			calc_color[3] = percent;
		}
		else
		{
			if (flash)
			{
				memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			}
		}

		cgi_R_SetColor(calc_color);

		if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
		{
			CG_DrawPic(df_forceTics[i].xPos,
				df_forceTics[i].yPos,
				df_forceTics[i].width,
				df_forceTics[i].height,
				df_forceTics[i].background
			);
		}
		else // horizontal
		{
			constexpr int offset_x = 419;
			int offset_correction = 0;
			if (i == 0)
			{
				offset_correction = 1;
			}
			offset_y += df_forceTics[i].height;
			CG_DrawRotatePic2(
				df_forceTics[i].xPos + df_forceTics[i].yPos - offset_x + offset_correction,
				df_forceTics[i].yPos + offset_y,
				df_forceTics[i].width,
				df_forceTics[i].height + offset_correction,
				-90,
				df_forceTics[i].background
			);
		}

		value -= inc;
	}

	if (flash)
	{
		cgi_R_SetColor(colorTable[CT_RED]);
	}
	else
	{
		cgi_R_SetColor(otherHUDBits[OHB_FORCEAMOUNT_DF].color);
	}

	// Print force numeric amount
	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawNumField(
			otherHUDBits[OHB_FORCEAMOUNT_DF].xPos,
			otherHUDBits[OHB_FORCEAMOUNT_DF].yPos,
			3,
			cent->gent->client->ps.forcePower,
			otherHUDBits[OHB_FORCEAMOUNT_DF].width,
			otherHUDBits[OHB_FORCEAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_Pic(OHB_FORCE_ICON_DF);
	}
	else // horizontal
	{
		CG_DrawNumField(
			otherHUDBits[OHB_FORCEAMOUNT_DF].xPos + 30,
			otherHUDBits[OHB_FORCEAMOUNT_DF].yPos + 3,
			3,
			cent->gent->client->ps.forcePower,
			otherHUDBits[OHB_FORCEAMOUNT_DF].width,
			otherHUDBits[OHB_FORCEAMOUNT_DF].height,
			NUM_FONT_SMALL,
			qfalse
		);
		CG_DrawDF_RotatePic(OHB_FORCE_ICON_DF, 0, -22, 58, 1, 1);
	}
}

static void CG_DrawDF_SaberStyle_Fatigue(const centity_t* cent)
{
	int index;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon != WP_SABER || !cent->gent)
	{
		return;
	}

	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (!cg.saberAnimLevelPending && cent->gent->client)
	{
		//uninitialized after a load game, cheat across and get it
		cg.saberAnimLevelPending = cent->gent->client->ps.saber_anim_level;
	}

	// don't need to draw ammo, but we will draw the current saber style in this window
	if (cg.saberAnimLevelPending == SS_FAST)
	{
		index = OHB_SABERSTYLE_FAST_DF;
	}
	else if (cg.saberAnimLevelPending == SS_MEDIUM)
	{
		index = OHB_SABERSTYLE_MEDIUM_DF;
	}
	else if (cg.saberAnimLevelPending == SS_TAVION)
	{
		index = OHB_SABERSTYLE_TAVION_DF;
	}
	else if (cg.saberAnimLevelPending == SS_DESANN)
	{
		index = OHB_SABERSTYLE_DESANN_DF;
	}
	else if (cg.saberAnimLevelPending == SS_STAFF)
	{
		index = OHB_SABERSTYLE_STAFF_DF;
	}
	else if (cg.saberAnimLevelPending == SS_DUAL)
	{
		index = OHB_SABERSTYLE_DUAL_DF;
	}
	else
	{
		index = OHB_SABERSTYLE_STRONG_DF;
	}

	cgi_R_SetColor(otherHUDBits[index].color);

	// Show saber fatigue by changing height and position of saber style/blade image
	float fatigue_percent;
	if (cg_SerenityJediEngineMode.integer)
	{
		fatigue_percent = static_cast<float>(cent->gent->client->ps.saberFatigueChainCount) / (MISHAPLEVEL_MAX + 1);
	}
	else
	{
		fatigue_percent = static_cast<float>(cent->gent->client->ps.saberAttackChainCount) / (MISHAPLEVEL_MAX + 1);
	}

	if (fatigue_percent < 0)
	{
		fatigue_percent = 0.0f;
	}

	if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
	{
		CG_DrawPic(
			SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[index].xPos),
			otherHUDBits[index].yPos + fatigue_percent * 100,
			otherHUDBits[index].width,
			otherHUDBits[index].height - fatigue_percent * 100,
			otherHUDBits[index].background
		);
	}
	else // horizontal
	{
		constexpr int offset_y = 132;
		constexpr int offset_x = 55;
		CG_DrawRotatePic2(
			SCREEN_WIDTH - (SCREEN_WIDTH + offset_x - otherHUDBits[index].xPos) + fatigue_percent * 60,
			otherHUDBits[index].yPos + offset_y,
			otherHUDBits[index].width,
			otherHUDBits[index].height - fatigue_percent * 100,
			-90,
			otherHUDBits[index].background
		);
	}
}

//draw meter showing sprint fuel when it's not full
constexpr auto SPFUELBAR_H = 100.0f;
constexpr auto SPFUELBAR_W = 5.0f;
#define SPFUELBAR_X			(SCREEN_WIDTH-SPFUELBAR_W-4.0f)
constexpr auto SPFUELBAR_Y = 240.0f;

static void CG_DrawSprintFuel()
{
	vec4_t aColor{};
	vec4_t b_color{};
	vec4_t cColor{};
	constexpr float x = SPFUELBAR_X;
	constexpr float y = SPFUELBAR_Y;
	float percent = static_cast<float>(cg.snap->ps.sprintFuel) / 100.0f * SPFUELBAR_H;

	if (percent > SPFUELBAR_H)
	{
		return;
	}

	if (percent < 0.1f)
	{
		percent = 0.1f;
	}

	//color of the bar
	aColor[0] = 0.0f;
	aColor[1] = 0.3f;
	aColor[2] = 0.6f;
	aColor[3] = 0.8f;

	//color of the border
	b_color[0] = 0.0f;
	b_color[1] = 0.0f;
	b_color[2] = 0.0f;
	b_color[3] = 0.3f;

	//color of greyed out "missing fuel"
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.1f;

	//draw the background (black)
	CG_DrawRect(x, y + 100, SPFUELBAR_W, SPFUELBAR_H, 0.5f, colorTable[CT_BLACK]);

	//now draw the part to show how much health there is in the color specified
	CG_FillRect(x + 0.5f, y + 0.5f + (SPFUELBAR_H - percent) + 100, SPFUELBAR_W - 0.5f,
		SPFUELBAR_H - 0.5f - (SPFUELBAR_H - percent), aColor);

	//then draw the other part greyed out
	CG_FillRect(x + 0.5f, y + 0.5f + 100, SPFUELBAR_W - 0.5f, SPFUELBAR_H - percent, cColor);
}

//draw meter showing jetpack fuel when it's not full

constexpr auto JPFUELBAR_H = 100.0f;
constexpr auto JPFUELBAR_W = 5.0f;
#define JPFUELBAR_X			(SCREEN_WIDTH-JPFUELBAR_W-4.0f)
constexpr auto JPFUELBAR_Y = 240.0f;

static void CG_DrawJetpackFuel()
{
	vec4_t aColor{};
	vec4_t b_color{};
	vec4_t cColor{};
	float x = JPFUELBAR_X;
	constexpr float y = JPFUELBAR_Y;
	float percent = static_cast<float>(cg.snap->ps.jetpackFuel) / 100.0f * JPFUELBAR_H;

	if (percent > JPFUELBAR_H)
	{
		return;
	}

	if (percent < 0.1f)
	{
		percent = 0.1f;
	}

	if (cg.snap->ps.sprintFuel < 100)
	{
		x -= SPFUELBAR_W + 4.0f;
	}

	//color of the bar
	aColor[0] = 0.5f;
	aColor[1] = 0.0f;
	aColor[2] = 0.0f;
	aColor[3] = 0.8f;

	//color of the border
	b_color[0] = 0.0f;
	b_color[1] = 0.0f;
	b_color[2] = 0.0f;
	b_color[3] = 0.3f;

	//color of greyed out "missing fuel"
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.1f;

	//draw the background (black)
	CG_DrawRect(x, y, JPFUELBAR_W, JPFUELBAR_H, 0.5f, colorTable[CT_BLACK]);

	//now draw the part to show how much health there is in the color specified
	CG_FillRect(x + 0.5f, y + 0.5f + (JPFUELBAR_H - percent), JPFUELBAR_W - 0.5f,
		JPFUELBAR_H - 0.5f - (JPFUELBAR_H - percent), aColor);

	//then draw the other part greyed out
	CG_FillRect(x + 0.5f, y + 0.5f, JPFUELBAR_W - 0.5f, JPFUELBAR_H - percent, cColor);
}

////draw meter showing cloak fuel when it's not full
constexpr auto CLFUELBAR_H = 100.0f;
constexpr auto CLFUELBAR_W = 5.0f;
#define CLFUELBAR_X			(SCREEN_WIDTH-CLFUELBAR_W-4.0f)
constexpr auto CLFUELBAR_Y = 240.0f;

static void CG_DrawCloakFuel()
{
	vec4_t aColor{};
	vec4_t b_color{};
	vec4_t cColor{};
	float x = CLFUELBAR_X;
	constexpr float y = CLFUELBAR_Y;

	float percent = static_cast<float>(cg.snap->ps.cloakFuel) / 100.0f * CLFUELBAR_H;
	if (percent > CLFUELBAR_H)
	{
		return;
	}

	if (percent < 0.1f)
	{
		percent = 0.1f;
	}

	if (cg.snap->ps.sprintFuel < 100)
	{
		x -= SPFUELBAR_W;
	}

	if (cg.snap->ps.jetpackFuel < 100)
	{
		x -= JPFUELBAR_W + 8.0f;
	}

	//color of the bar
	aColor[0] = 0.0f;
	aColor[1] = 0.0f;
	aColor[2] = 0.6f;
	aColor[3] = 0.8f;

	//color of the border
	b_color[0] = 0.0f;
	b_color[1] = 0.0f;
	b_color[2] = 0.0f;
	b_color[3] = 0.3f;

	//color of greyed out "missing fuel"
	cColor[0] = 0.1f;
	cColor[1] = 0.1f;
	cColor[2] = 0.3f;
	cColor[3] = 0.1f;

	//draw the background (black)
	CG_DrawRect(x, y, CLFUELBAR_W, CLFUELBAR_H, 0.5f, colorTable[CT_BLACK]);

	//now draw the part to show how much fuel there is in the color specified
	CG_FillRect(x + 0.5f, y + 0.5f + (CLFUELBAR_H - percent), CLFUELBAR_W - 0.5f,
		CLFUELBAR_H - 0.5f - (CLFUELBAR_H - percent), aColor);

	//then draw the other part greyed out
	CG_FillRect(x + 0.5f, y + 0.5f, CLFUELBAR_W - 0.5f, CLFUELBAR_H - percent, cColor);
}

//draw meter showing cloak fuel when it's not full
constexpr auto BFFUELBAR_H = 100.0f;
constexpr auto BFFUELBAR_W = 5.0f;
#define BFFUELBAR_X			(SCREEN_WIDTH-BFFUELBAR_W-4.0f)
constexpr auto BFFUELBAR_Y = 240.0f;

static void CG_DrawBarrierFuel()
{
	vec4_t aColor{};
	vec4_t b_color{};
	vec4_t cColor{};
	float x = BFFUELBAR_X;
	constexpr float y = BFFUELBAR_Y;

	float percent = static_cast<float>(cg.snap->ps.BarrierFuel) / 100.0f * BFFUELBAR_H;
	if (percent > BFFUELBAR_H)
	{
		return;
	}

	if (percent < 0.1f)
	{
		percent = 0.1f;
	}

	if (cg.snap->ps.sprintFuel < 100)
	{
		x -= SPFUELBAR_W;
	}

	if (cg.snap->ps.jetpackFuel < 100)
	{
		return;
	}

	if (cg.snap->ps.cloakFuel < 100)
	{
		return;
	}

	//color of the bar
	aColor[0] = 0.0f;
	aColor[1] = 0.6f;
	aColor[2] = 0.4f;
	aColor[3] = 0.2f;

	//color of the border
	b_color[0] = 0.0f;
	b_color[1] = 0.0f;
	b_color[2] = 0.0f;
	b_color[3] = 0.3f;

	//color of greyed out "missing fuel"
	cColor[0] = 0.1f;
	cColor[1] = 0.1f;
	cColor[2] = 0.3f;
	cColor[3] = 0.1f;

	//draw the background (black)
	CG_DrawRect(x, y, BFFUELBAR_W, BFFUELBAR_H, 0.5f, colorTable[CT_BLACK]);

	//now draw the part to show how much fuel there is in the color specified
	CG_FillRect(x + 0.5f, y + 0.5f + (BFFUELBAR_H - percent), BFFUELBAR_W - 0.5f,
		BFFUELBAR_H - 0.5f - (BFFUELBAR_H - percent), aColor);

	//then draw the other part greyed out
	CG_FillRect(x + 0.5f, y + 0.5f, BFFUELBAR_W - 0.5f, BFFUELBAR_H - percent, cColor);
}

static void CG_DrawAmmo(const centity_t* cent, const float hud_ratio)
{
	vec4_t calc_color;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon == WP_STUN_BATON)
	{
		return;
	}

	if (cent->currentState.weapon == WP_MELEE)
	{
		return;
	}

	const playerState_t* ps = &cg.snap->ps;

	float curr_value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	if (curr_value < 0) // No ammo
	{
		return;
	}

	//
	// ammo
	//
	if (cg.oldammo < curr_value)
	{
		cg.oldAmmoTime = cg.time + 200;
	}

	cg.oldammo = curr_value;

	// Determine the color of the numeric field

	// Firing or reloading?
	if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING
		&& cg.predictedPlayerState.weaponTime > 100)
	{
		memcpy(calc_color, colorTable[CT_LTGREY], sizeof(vec4_t));
	}
	else
	{
		if (curr_value > 0)
		{
			if (cg.oldAmmoTime > cg.time)
			{
				memcpy(calc_color, colorTable[CT_YELLOW], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, otherHUDBits[OHB_AMMOAMOUNT].color, sizeof(vec4_t));
			}
		}
		else
		{
			memcpy(calc_color, colorTable[CT_RED], sizeof(vec4_t));
		}
	}

	// Print number amount
	cgi_R_SetColor(calc_color);

	CG_DrawNumField(
		SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_AMMOAMOUNT].xPos) * hud_ratio,
		otherHUDBits[OHB_AMMOAMOUNT].yPos,
		3,
		ps->ammo[weaponData[cent->currentState.weapon].ammoIndex],
		otherHUDBits[OHB_AMMOAMOUNT].width * hud_ratio,
		otherHUDBits[OHB_AMMOAMOUNT].height,
		NUM_FONT_SMALL,
		qfalse);

	const float inc = static_cast<float>(ammoData[weaponData[cent->currentState.weapon].ammoIndex].max) / MAX_HUD_TICS;
	curr_value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));

	for (int i = MAX_HUD_TICS - 1; i >= 0; i--)
	{
		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, ammoTics[i].color, sizeof(vec4_t));
			const float percent = curr_value / inc;
			calc_color[3] *= percent;
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(SCREEN_WIDTH - (SCREEN_WIDTH - ammoTics[i].xPos) * hud_ratio,
			ammoTics[i].yPos,
			ammoTics[i].width * hud_ratio,
			ammoTics[i].height,
			ammoTics[i].background);

		curr_value -= inc;
	}
}

static void CG_DrawJK2Ammo(const centity_t* cent, const int x, const int y)
{
	int num_color_i;
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon == WP_STUN_BATON)
	{
		return;
	}

	if (cent->currentState.weapon == WP_SABER && cent->gent)
	{
		cgi_R_SetColor(colorTable[CT_WHITE]);

		if (!cg.saberAnimLevelPending && cent->gent->client)
		{
			//uninitialized after a load game, cheat across and get it
			cg.saberAnimLevelPending = cent->gent->client->ps.saber_anim_level;
		}
		// don't need to draw ammo, but we will draw the current saber style in this window
		switch (cg.saberAnimLevelPending)
		{
		case 1: //FORCE_LEVEL_1:
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleFast);
			break;
		case 2: //FORCE_LEVEL_2:
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleMed);
			break;
		case 3: //FORCE_LEVEL_3:
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleStrong);
			break;
		case 4: //FORCE_LEVEL_4://Desann
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleDesann);
			break;
		case 5: //FORCE_LEVEL_5://Tavion
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleTavion);
			break;
		case 6: //FORCE_LEVEL_5://STAFF
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleStaff);
			break;
		case 7: //FORCE_LEVEL_5://DUELS
			CG_DrawPic(x, y, 80, 40, cgs.media.JK2HUDSaberStyleDuels);
			break;
		default:;
		}
		return;
	}
	float value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	if (value < 0) // No ammo
	{
		return;
	}

	//
	// ammo
	//
	if (cg.oldammo < value)
	{
		cg.oldAmmoTime = cg.time + 200;
	}

	cg.oldammo = value;

	// Firing or reloading?
	if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING
		&& cg.predictedPlayerState.weaponTime > 100)
	{
		num_color_i = CT_LTGREY;
	}
	else
	{
		if (value > 0)
		{
			if (cg.oldAmmoTime > cg.time)
			{
				num_color_i = CT_YELLOW;
			}
			else
			{
				num_color_i = CT_HUD_ORANGE;
			}
		}
		else
		{
			num_color_i = CT_RED;
		}
	}

	cgi_R_SetColor(colorTable[num_color_i]);
	CG_DrawNumField(x + 29, y + 26, 3, value, 6, 12, NUM_FONT_SMALL, qfalse);

	const float inc = static_cast<float>(ammoData[weaponData[cent->currentState.weapon].ammoIndex].max) /
		MAX_DATAPADTICS;
	value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	for (int i = MAX_DATAPADTICS - 1; i >= 0; i--)
	{
		if (value <= 0) // partial tic
		{
			memcpy(calc_color, colorTable[CT_BLACK], sizeof(vec4_t));
		}
		else if (value < inc) // partial tic
		{
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			const float percent = value / inc;
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else
		{
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x + JK2ammoTicPos[i].x,
			y + JK2ammoTicPos[i].y,
			JK2ammoTicPos[i].width,
			JK2ammoTicPos[i].height,
			JK2ammoTicPos[i].tic);

		value -= inc;
	}
}

static void CG_DrawHealth(const float hud_ratio)
{
	vec4_t calc_color;
	const playerState_t* ps = &cg.snap->ps;

	// Print all the tics of the health graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = static_cast<float>(ps->stats[STAT_MAX_HEALTH]) / MAX_HUD_TICS;
	float curr_value = ps->stats[STAT_HEALTH];
	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_HUD_TICS - 1; i >= 0; i--)
	{
		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, healthTics[i].color, sizeof(vec4_t));
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(
			healthTics[i].xPos * hud_ratio,
			healthTics[i].yPos,
			healthTics[i].width * hud_ratio,
			healthTics[i].height,
			healthTics[i].background
		);

		curr_value -= inc;
	}

	// Print force health amount
	cgi_R_SetColor(otherHUDBits[OHB_HEALTHAMOUNT].color);

	CG_DrawNumField(
		otherHUDBits[OHB_HEALTHAMOUNT].xPos * hud_ratio,
		otherHUDBits[OHB_HEALTHAMOUNT].yPos,
		3,
		ps->stats[STAT_HEALTH],
		otherHUDBits[OHB_HEALTHAMOUNT].width * hud_ratio,
		otherHUDBits[OHB_HEALTHAMOUNT].height,
		NUM_FONT_SMALL,
		qfalse);
}

static void CG_DrawArmor(const float hud_ratio)
{
	vec4_t calc_color;
	const playerState_t* ps = &cg.snap->ps;

	// Print all the tics of the armor graphic
	// Look at the amount of armor left and show only as much of the graphic as there is armor.
	// Use alpha to fade out partial section of armor
	// MAX_HEALTH is the same thing as max armor
	const float inc = static_cast<float>(ps->stats[STAT_MAX_HEALTH]) / MAX_HUD_TICS;
	float curr_value = ps->stats[STAT_ARMOR];

	memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
	for (int i = MAX_HUD_TICS - 1; i >= 0; i--)
	{
		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			memcpy(calc_color, armorTics[i].color, sizeof(vec4_t));
			const float percent = curr_value / inc;
			calc_color[3] *= percent;
		}

		cgi_R_SetColor(calc_color);

		if (i == MAX_HUD_TICS - 1 && curr_value < inc)
		{
			if (cg.HUDArmorFlag)
			{
				CG_DrawPic(
					armorTics[i].xPos * hud_ratio,
					armorTics[i].yPos,
					armorTics[i].width * hud_ratio,
					armorTics[i].height,
					armorTics[i].background
				);
			}
		}
		else
		{
			CG_DrawPic(
				armorTics[i].xPos * hud_ratio,
				armorTics[i].yPos,
				armorTics[i].width * hud_ratio,
				armorTics[i].height,
				armorTics[i].background
			);
		}

		curr_value -= inc;
	}

	// Print armor amount
	cgi_R_SetColor(otherHUDBits[OHB_ARMORAMOUNT].color);

	CG_DrawNumField(
		otherHUDBits[OHB_ARMORAMOUNT].xPos * hud_ratio,
		otherHUDBits[OHB_ARMORAMOUNT].yPos,
		3,
		ps->stats[STAT_ARMOR],
		otherHUDBits[OHB_ARMORAMOUNT].width * hud_ratio,
		otherHUDBits[OHB_ARMORAMOUNT].height,
		NUM_FONT_SMALL,
		qfalse);

	// If armor is low, flash a graphic to warn the player
	if (ps->stats[STAT_ARMOR]) // Is there armor? Draw the HUD Armor TIC
	{
		const float quarter_armor = ps->stats[STAT_MAX_HEALTH] / 4.0f;

		// Make tic flash if armor is at 25% of full armor
		if (ps->stats[STAT_ARMOR] < quarter_armor) // Do whatever the flash timer says
		{
			if (cg.HUDTickFlashTime < cg.time) // Flip at the same time
			{
				cg.HUDTickFlashTime = cg.time + 400;
				if (cg.HUDArmorFlag)
				{
					cg.HUDArmorFlag = qfalse;
				}
				else
				{
					cg.HUDArmorFlag = qtrue;
				}
			}
		}
		else
		{
			cg.HUDArmorFlag = qtrue;
		}
	}
	else // No armor? Don't show it.
	{
		cg.HUDArmorFlag = qfalse;
	}
}

static void CG_DrawJK2Armor(const centity_t* cent, const int x, const int y)
{
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	//	Outer Armor circular
	memcpy(calc_color, colorTable[CT_HUD_GREEN], sizeof(vec4_t));

	const float hold = ps->stats[STAT_ARMOR] - ps->stats[STAT_MAX_HEALTH] / static_cast<float>(2);
	float armor_percent = hold / (ps->stats[STAT_MAX_HEALTH] / static_cast<float>(2));
	if (armor_percent < 0)
	{
		armor_percent = 0;
	}
	calc_color[0] *= armor_percent;
	calc_color[1] *= armor_percent;
	calc_color[2] *= armor_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDArmor1);

	// Inner Armor circular
	if (armor_percent > 0)
	{
		armor_percent = 1;
	}
	else
	{
		armor_percent = static_cast<float>(ps->stats[STAT_ARMOR]) / (ps->stats[STAT_MAX_HEALTH] / static_cast<float>(2));
	}
	memcpy(calc_color, colorTable[CT_HUD_GREEN], sizeof(vec4_t));
	calc_color[0] *= armor_percent;
	calc_color[1] *= armor_percent;
	calc_color[2] *= armor_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDArmor2);

	cgi_R_SetColor(colorTable[CT_HUD_GREEN]);

	if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
	{
		CG_DrawNumField(x + 16 + 14, y + 40 + 16, 3, ps->stats[STAT_ARMOR], 4, 7, NUM_FONT_SMALL, qfalse);
	}
	else
	{
		CG_DrawNumField(x + 16 + 14, y + 40 + 14, 3, ps->stats[STAT_ARMOR], 6, 12, NUM_FONT_SMALL, qfalse);
	}
}

static void CG_DrawJK2Health(const centity_t* cent, const int x, const int y)
{
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	memcpy(calc_color, colorTable[CT_HUD_RED], sizeof(vec4_t));
	const float health_percent = static_cast<float>(ps->stats[STAT_HEALTH]) / ps->stats[STAT_MAX_HEALTH];
	calc_color[0] *= health_percent;
	calc_color[1] *= health_percent;
	calc_color[2] *= health_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDHealth);

	// Draw the ticks
	if (cg.HUDHealthFlag)
	{
		cgi_R_SetColor(colorTable[CT_HUD_RED]);
		CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDHealthTic);
	}

	cgi_R_SetColor(colorTable[CT_HUD_RED]);

	if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
	{
		CG_DrawNumField(x + 16, y + 40, 3, ps->stats[STAT_HEALTH], 4, 6, NUM_FONT_SMALL, qtrue);
	}
	else
	{
		CG_DrawNumField(x + 16, y + 40, 3, ps->stats[STAT_HEALTH], 6, 12, NUM_FONT_SMALL, qtrue);
	}
}

static void CG_DrawJK2HealthSJE(const centity_t* cent, const int x, const int y)
{
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	memcpy(calc_color, colorTable[CT_HUD_RED], sizeof(vec4_t));
	const float health_percent = static_cast<float>(ps->stats[STAT_HEALTH]) / ps->stats[STAT_MAX_HEALTH];
	calc_color[0] *= health_percent;
	calc_color[1] *= health_percent;
	calc_color[2] *= health_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 40, 40, cgs.media.JK2HUDHealth);

	// Draw the ticks
	if (cg.HUDHealthFlag)
	{
		cgi_R_SetColor(colorTable[CT_HUD_RED]);
		CG_DrawPic(x, y, 40, 40, cgs.media.JK2HUDHealthTic);
	}

	cgi_R_SetColor(colorTable[CT_HUD_RED]);

	if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
	{
		CG_DrawNumField(x + 2, y + 26, 3, ps->stats[STAT_HEALTH], 4, 7, NUM_FONT_SMALL, qtrue);
	}
	else
	{
		CG_DrawNumField(x + 16, y + 40, 3, ps->stats[STAT_HEALTH], 6, 12, NUM_FONT_SMALL, qtrue);
	}
}

constexpr auto MAX_VHUD_SHIELD_TICS = 12;
constexpr auto MAX_VHUD_SPEED_TICS = 5;
constexpr auto MAX_VHUD_ARMOR_TICS = 5;
constexpr auto MAX_VHUD_AMMO_TICS = 5;

static void CG_DrawVehicleSheild(const Vehicle_t* p_veh)
{
	int x_pos, y_pos, width, height;
	vec4_t color, calc_color;
	qhandle_t background;
	float curr_value, max_health;

	//riding some kind of living creature
	if (p_veh->m_pVehicleInfo->type == VH_ANIMAL || p_veh->m_pVehicleInfo->type == VH_FLIER)
	{
		max_health = 100.0f;
		curr_value = p_veh->m_pParentEntity->health;
	}
	else //normal vehicle
	{
		max_health = p_veh->m_pVehicleInfo->armor;
		curr_value = p_veh->m_iArmor;
	}

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"shieldbackground",
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

	// Print all the tics of the shield graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = max_health / MAX_VHUD_SHIELD_TICS;
	for (int i = 1; i <= MAX_VHUD_SHIELD_TICS; i++)
	{
		char item_name[64];
		Com_sprintf(item_name, sizeof item_name, "shield_tic%d", i);

		if (!cgi_UI_GetMenuItemInfo(
			"swoopvehiclehud",
			item_name,
			&x_pos,
			&y_pos,
			&width,
			&height,
			color,
			&background))
		{
			continue;
		}

		memcpy(calc_color, color, sizeof(vec4_t));

		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x_pos, y_pos, width, height, background);

		curr_value -= inc;
	}
}

// The HUD.menu file has the graphic print with a negative height, so it will print from the bottom up.
static void CG_DrawVehicleTurboRecharge(const Vehicle_t* p_veh)
{
	int x_pos, y_pos, width, height;
	qhandle_t background;
	vec4_t color;

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"turborecharge",
		&x_pos,
		&y_pos,
		&width,
		&height,
		color,
		&background))
	{
		float percent;
		const int diff = cg.time - p_veh->m_iTurboTime;

		// Calc max time

		if (diff > p_veh->m_pVehicleInfo->turboRecharge)
		{
			percent = 1.0f;
			cgi_R_SetColor(colorTable[CT_GREEN]);
		}
		else
		{
			percent = static_cast<float>(diff) / p_veh->m_pVehicleInfo->turboRecharge;
			if (percent < 0.0f)
			{
				percent = 0.0f;
			}
			cgi_R_SetColor(colorTable[CT_RED]);
		}

		height *= percent;

		CG_DrawPic(x_pos, y_pos, width, height, cgs.media.whiteShader); // Top
	}
}

static void CG_DrawVehicleSpeed(const Vehicle_t* p_veh, const char* ent_hud)
{
	int x_pos, y_pos, width, height;
	qhandle_t background;
	const gentity_t* parent = p_veh->m_pParentEntity;
	const playerState_t* parent_ps = &parent->client->ps;
	vec4_t color, calc_color;

	if (cgi_UI_GetMenuItemInfo(
		ent_hud,
		"speedbackground",
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

	const float max_speed = p_veh->m_pVehicleInfo->speedMax;
	float curr_value = parent_ps->speed;

	// Print all the tics of the shield graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = static_cast<float>(max_speed) / MAX_VHUD_SPEED_TICS;
	for (int i = 1; i <= MAX_VHUD_SPEED_TICS; i++)
	{
		char itemName[64];
		Com_sprintf(itemName, sizeof itemName, "speed_tic%d", i);

		if (!cgi_UI_GetMenuItemInfo(
			ent_hud,
			itemName,
			&x_pos,
			&y_pos,
			&width,
			&height,
			color,
			&background))
		{
			continue;
		}

		if (level.time > p_veh->m_iTurboTime)
		{
			memcpy(calc_color, color, sizeof(vec4_t));
		}
		else // In turbo mode
		{
			if (cg.VHUDFlashTime < cg.time)
			{
				cg.VHUDFlashTime = cg.time + 400;
				if (cg.VHUDTurboFlag)
				{
					cg.VHUDTurboFlag = qfalse;
				}
				else
				{
					cg.VHUDTurboFlag = qtrue;
				}
			}

			if (cg.VHUDTurboFlag)
			{
				memcpy(calc_color, colorTable[CT_LTRED1], sizeof(vec4_t));
			}
			else
			{
				memcpy(calc_color, color, sizeof(vec4_t));
			}
		}

		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x_pos, y_pos, width, height, background);

		curr_value -= inc;
	}
}

static void CG_DrawVehicleArmor(const Vehicle_t* p_veh)
{
	int x_pos, y_pos, width, height;
	qhandle_t background;
	vec4_t color, calc_color;

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"armorbackground",
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

	const float max_armor = p_veh->m_iArmor;
	float curr_value = p_veh->m_pVehicleInfo->armor;

	// Print all the tics of the shield graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = static_cast<float>(max_armor) / MAX_VHUD_ARMOR_TICS;
	for (int i = 1; i <= MAX_VHUD_ARMOR_TICS; i++)
	{
		char item_name[64];
		Com_sprintf(item_name, sizeof item_name, "armor_tic%d", i);

		if (!cgi_UI_GetMenuItemInfo(
			"swoopvehiclehud",
			item_name,
			&x_pos,
			&y_pos,
			&width,
			&height,
			color,
			&background))
		{
			continue;
		}

		memcpy(calc_color, color, sizeof(vec4_t));

		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x_pos, y_pos, width, height, background);

		curr_value -= inc;
	}
}

static void CG_DrawVehicleAmmo(const Vehicle_t* p_veh)
{
	int x_pos, y_pos, width, height;
	qhandle_t background;
	vec4_t color, calc_color;

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"ammobackground",
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

	const float max_ammo = p_veh->m_pVehicleInfo->weapon[0].ammoMax;
	float curr_value = p_veh->weaponStatus[0].ammo;
	const float inc = static_cast<float>(max_ammo) / MAX_VHUD_AMMO_TICS;
	for (int i = 1; i <= MAX_VHUD_AMMO_TICS; i++)
	{
		char item_name[64];
		Com_sprintf(item_name, sizeof item_name, "ammo_tic%d", i);

		if (!cgi_UI_GetMenuItemInfo(
			"swoopvehiclehud",
			item_name,
			&x_pos,
			&y_pos,
			&width,
			&height,
			color,
			&background))
		{
			continue;
		}

		memcpy(calc_color, color, sizeof(vec4_t));

		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);
		CG_DrawPic(x_pos, y_pos, width, height, background);

		curr_value -= inc;
	}
}

static void CG_DrawVehicleHud(const centity_t* cent, const Vehicle_t* p_veh)
{
	int x_pos, y_pos, width, height;
	vec4_t color;
	qhandle_t background;

	CG_DrawVehicleTurboRecharge(p_veh);

	// Draw frame
	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"leftframe",
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

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"rightframe",
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

	CG_DrawVehicleSheild(p_veh);

	CG_DrawVehicleSpeed(p_veh, "swoopvehiclehud");

	CG_DrawVehicleArmor(p_veh);

	CG_DrawVehicleAmmo(p_veh);
}

static void CG_DrawTauntaunHud(const Vehicle_t* p_veh)
{
	int x_pos, y_pos, width, height;
	vec4_t color;
	qhandle_t background;

	CG_DrawVehicleTurboRecharge(p_veh);

	// Draw frame
	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"leftframe",
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

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"rightframe",
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

	CG_DrawVehicleSheild(p_veh);

	CG_DrawVehicleSpeed(p_veh, "tauntaunhud");
}

static void CG_DrawEmplacedGunHealth(const centity_t* cent)
{
	int x_pos, y_pos, width, height, health;
	vec4_t color, calc_color;
	qhandle_t background;

	if (cent->gent && cent->gent->owner)
	{
		if (cent->gent->owner->flags & FL_GODMODE)
		{
			// chair is in godmode, so render the health of the player instead
			health = cent->gent->health;
		}
		else
		{
			// render the chair health
			health = cent->gent->owner->health;
		}
	}
	else
	{
		return;
	}
	//riding some kind of living creature
	const auto max_health = static_cast<float>(cent->gent->max_health);
	float curr_value = health;

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"shieldbackground",
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

	// Print all the tics of the shield graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = static_cast<float>(max_health) / MAX_VHUD_SHIELD_TICS;
	for (int i = 1; i <= MAX_VHUD_SHIELD_TICS; i++)
	{
		char item_name[64];
		Com_sprintf(item_name, sizeof item_name, "shield_tic%d", i);

		if (!cgi_UI_GetMenuItemInfo(
			"swoopvehiclehud",
			item_name,
			&x_pos,
			&y_pos,
			&width,
			&height,
			color,
			&background))
		{
			continue;
		}

		memcpy(calc_color, color, sizeof(vec4_t));

		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x_pos, y_pos, width, height, background);

		curr_value -= inc;
	}
}

static void CG_DrawEmplacedGunHud(const centity_t* cent)
{
	int x_pos, y_pos, width, height;
	vec4_t color;
	qhandle_t background;

	// Draw frame
	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"leftframe",
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

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"rightframe",
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

	CG_DrawEmplacedGunHealth(cent);
}

static void CG_DrawItemHealth(float curr_value, const float max_health)
{
	int x_pos, y_pos, width, height;
	vec4_t color, calc_color;
	qhandle_t background;

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"shieldbackground",
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

	// Print all the tics of the shield graphic
	// Look at the amount of health left and show only as much of the graphic as there is health.
	// Use alpha to fade out partial section of health
	const float inc = max_health / MAX_VHUD_SHIELD_TICS;
	for (int i = 1; i <= MAX_VHUD_SHIELD_TICS; i++)
	{
		char item_name[64];
		Com_sprintf(item_name, sizeof item_name, "shield_tic%d", i);

		if (!cgi_UI_GetMenuItemInfo(
			"swoopvehiclehud",
			item_name,
			&x_pos,
			&y_pos,
			&width,
			&height,
			color,
			&background))
		{
			continue;
		}

		memcpy(calc_color, color, sizeof(vec4_t));

		if (curr_value <= 0) // don't show tic
		{
			break;
		}
		if (curr_value < inc) // partial tic (alpha it out)
		{
			const float percent = curr_value / inc;
			calc_color[3] *= percent; // Fade it out
		}

		cgi_R_SetColor(calc_color);

		CG_DrawPic(x_pos, y_pos, width, height, background);

		curr_value -= inc;
	}
}

static void CG_DrawPanelTurretHud()
{
	int x_pos, y_pos, width, height;
	vec4_t color;
	qhandle_t background;

	// Draw frame
	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"leftframe",
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

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"rightframe",
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

	CG_DrawItemHealth(
		g_entities[cg.snap->ps.viewEntity].health,
		static_cast<float>(g_entities[cg.snap->ps.viewEntity].max_health)
	);
}

static void CG_DrawATSTHud()
{
	int x_pos, y_pos, width, height;
	vec4_t color;
	qhandle_t background;
	float health;

	if (!cg.snap
		|| !g_entities[cg.snap->ps.viewEntity].activator)
	{
		return;
	}

	// Draw frame
	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"leftframe",
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

	if (cgi_UI_GetMenuItemInfo(
		"swoopvehiclehud",
		"rightframe",
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

	// we just calc the display value from the sum of health and armor
	if (g_entities[cg.snap->ps.viewEntity].activator)
		// ensure we can look back to the atst_drivable to get the max health
	{
		health = g_entities[cg.snap->ps.viewEntity].health + g_entities[cg.snap->ps.viewEntity].client->ps.stats[
			STAT_ARMOR];
	}
	else
	{
		health = g_entities[cg.snap->ps.viewEntity].health + g_entities[cg.snap->ps.viewEntity].client->ps.stats[
			STAT_ARMOR];
	}

	CG_DrawItemHealth(health, g_entities[cg.snap->ps.viewEntity].activator->max_health);

	if (cgi_UI_GetMenuItemInfo(
		"atsthud",
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

	if (cgi_UI_GetMenuItemInfo(
		"atsthud",
		"outer_frame",
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

	if (cgi_UI_GetMenuItemInfo(
		"atsthud",
		"left_pic",
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
}

//-----------------------------------------------------
static qboolean CG_DrawCustomHealthHud(const centity_t* cent)
{
	Vehicle_t* p_veh;

	// In a Weapon?
	if (cent->currentState.eFlags & EF_LOCKED_TO_WEAPON)
	{
		CG_DrawEmplacedGunHud(cent);
		return qfalse; // drew this hud, so don't draw the player one
	}
	// In an ATST
	if (cent->currentState.eFlags & EF_IN_ATST)
	{
		CG_DrawATSTHud();

		return qfalse; // drew this hud, so don't draw the player one
	}
	// In a vehicle
	if ((p_veh = G_IsRidingVehicle(cent->gent)) != nullptr)
	{
		//riding some kind of living creature
		if (p_veh->m_pVehicleInfo->type == VH_ANIMAL)
		{
			CG_DrawTauntaunHud(p_veh);
		}
		else
		{
			CG_DrawVehicleHud(cent, p_veh);
		}
		return qtrue; // draw this hud AND the player one
	}
	// Other?
	if (cg.snap->ps.viewEntity && g_entities[cg.snap->ps.viewEntity].dflags & DAMAGE_CUSTOM_HUD)
	{
		CG_DrawPanelTurretHud();
		return qfalse; // drew this hud, so don't draw the player one
	}

	return qtrue;
}

//--------------------------------------
static void CG_DrawBatteryCharge()
{
	if (cg.batteryChargeTime > cg.time)
	{
		vec4_t color{};

		// FIXME: drawing it here will overwrite zoom masks...find a better place
		if (cg.batteryChargeTime < cg.time + 1000)
		{
			// fading out for the last second
			color[0] = color[1] = color[2] = 1.0f;
			color[3] = (cg.batteryChargeTime - cg.time) / 1000.0f;
		}
		else
		{
			// draw full
			color[0] = color[1] = color[2] = color[3] = 1.0f;
		}

		cgi_R_SetColor(color);

		// batteries were just charged
		CG_DrawPic(605, 295, 24, 32, cgs.media.batteryChargeShader);
	}
}

#define SimpleHud_DrawString( x, y, str, color ) cgi_R_Font_DrawString( x, y, str, color, (int)0x80000000 | cgs.media.qhFontSmall, -1, 1.0f )

static void CG_DrawSimpleSaberStyle(const centity_t* cent)
{
	uint32_t calc_color;
	char num[7] = { 0 };
	int weap_x = 16;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon != WP_SABER || !cent->gent)
	{
		return;
	}

	if (!cg.saberAnimLevelPending && cent->gent && cent->gent->client)
	{
		//uninitialized after a loadgame, cheat across and get it
		cg.saberAnimLevelPending = cent->gent->client->ps.saber_anim_level;
	}

	switch (cg.saberAnimLevelPending)
	{
	default:
	case SS_FAST:
		Com_sprintf(num, sizeof num, "FAST");
		calc_color = CT_ICON_BLUE;
		weap_x = 0;
		break;
	case SS_MEDIUM:
		Com_sprintf(num, sizeof num, "MEDIUM");
		calc_color = CT_YELLOW;
		break;
	case SS_STRONG:
		Com_sprintf(num, sizeof num, "STRONG");
		calc_color = CT_HUD_RED;
		break;
	case SS_DESANN:
		Com_sprintf(num, sizeof num, "DESANN");
		calc_color = CT_HUD_RED;
		break;
	case SS_TAVION:
		Com_sprintf(num, sizeof num, "TAVION");
		calc_color = CT_ICON_BLUE;
		break;
	case SS_DUAL:
		Com_sprintf(num, sizeof num, "AKIMBO");
		calc_color = CT_HUD_ORANGE;
		break;
	case SS_STAFF:
		Com_sprintf(num, sizeof num, "STAFF");
		calc_color = CT_HUD_ORANGE;
		break;
	}

	SimpleHud_DrawString(SCREEN_WIDTH - cgs.widthRatioCoef * (weap_x + 16 + 32), SCREEN_HEIGHT - 80 + 40, num,
		colorTable[calc_color]);
}

static void CG_DrawSimpleAmmo(const centity_t* cent)
{
	uint32_t calc_color;
	char num[16] = { 0 };

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	const playerState_t* ps = &cg.snap->ps;

	const int curr_value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	// No ammo
	if (curr_value < 0 || weaponData[cent->currentState.weapon].energyPerShot == 0 && weaponData[cent->currentState.
		weapon].altEnergyPerShot == 0)
	{
		SimpleHud_DrawString(SCREEN_WIDTH - cgs.widthRatioCoef * (16 + 32), SCREEN_HEIGHT - 80 + 40, "--",
			colorTable[CT_HUD_ORANGE]);
		return;
	}

	//
	// ammo
	//
	if (cg.oldammo < curr_value)
	{
		cg.oldAmmoTime = cg.time + 200;
	}

	cg.oldammo = curr_value;

	// Determine the color of the numeric field

	// Firing or reloading?
	if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING
		&& cg.predictedPlayerState.weaponTime > 100)
	{
		calc_color = CT_LTGREY;
	}
	else
	{
		if (curr_value > 0)
		{
			if (cg.oldAmmoTime > cg.time)
			{
				calc_color = CT_YELLOW;
			}
			else
			{
				calc_color = CT_HUD_ORANGE;
			}
		}
		else
		{
			calc_color = CT_RED;
		}
	}

	Com_sprintf(num, sizeof num, "%i", curr_value);

	SimpleHud_DrawString(SCREEN_WIDTH - cgs.widthRatioCoef * (16 + 32), SCREEN_HEIGHT - 80 + 40, num,
		colorTable[calc_color]);
}

static void CG_DrawSimpleForcePower(const centity_t* cent)
{
	char num[16] = { 0 };
	qboolean flash = qfalse;

	if (!cent->gent || !cent->gent->client->ps.forcePowersKnown)
	{
		return;
	}

	// Make the hud flash by setting forceHUDTotalFlashTime above cg.time
	if (cg.forceHUDTotalFlashTime > cg.time)
	{
		flash = qtrue;
		if (cg.forceHUDNextFlashTime < cg.time)
		{
			cg.forceHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound(nullptr, 0, CHAN_AUTO, cgs.media.noforceSound);
			if (cg.forceHUDActive)
			{
				cg.forceHUDActive = qfalse;
			}
			else
			{
				cg.forceHUDActive = qtrue;
			}
		}
	}
	else // turn HUD back on if it had just finished flashing time.
	{
		cg.forceHUDNextFlashTime = 0;
		cg.forceHUDActive = qtrue;
	}

	// Determine the color of the numeric field
	const uint32_t calc_color = flash ? CT_RED : CT_ICON_BLUE;

	Com_sprintf(num, sizeof num, "%i", cent->gent->client->ps.forcePower);

	SimpleHud_DrawString(SCREEN_WIDTH - cgs.widthRatioCoef * (16 + 32), SCREEN_HEIGHT - 80 + 40 + 14, num,
		colorTable[calc_color]);
}

/*
================
CG_DrawHUD
================
*/
extern void workshop_draw_clientside_information();

static bool draw_jetpack_fuel_rocket_trooper_player(const centity_t* cent)
{
	if (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent))
	{
		if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ROCKETTROOPER)
		{
			return qtrue;
		}
	}
	return qfalse;
};

static void CG_DrawSJEHUDRightFrame(const int x, const int y)
{
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x - 72, y - 75, 152, 160, cgs.media.SJEHUDRightFrame); // Metal frame
}

static void CG_DrawSJEHUDLeftFrame(const int x, const int y)
{
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x, y - 75, 152, 160, cgs.media.SJEHUDLeftFrame); // Metal frame
}

//jk2 hud
vec4_t bluehudtint = { 0.5, 0.5, 1.0, 1.0 };
vec4_t redhudtint = { 1.0, 0.5, 0.5, 1.0 };
float* hudTintColor;

void CG_DrawHUDJK2LeftFrame1(int x, int y);
void CG_DrawHUDJK2LeftFrame2(int x, int y);
void CG_DrawHUDJK2RightFrame1(int x, int y);
void CG_DrawHUDJK2RightFrame2(int x, int y);

static void CG_DrawHUD(const centity_t* cent)
{
	int section_x_pos, section_y_pos, section_width, section_height;
	const float hud_ratio = cg_hudRatio.integer ? cgs.widthRatioCoef : 1.0f;

	workshop_draw_clientside_information();
	const Vehicle_t* p_veh;

	if (cent->gent->health < 1)
	{
		return;
	}

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return;
	}

	// Are we in zoom mode or the HUD is turned off?
	if (cg.zoomMode != 0 || !cg_drawHUD.integer)
	{
		return;
	}

	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		return;
	}

	if ((p_veh = G_IsRidingVehicle(cg_entities[0].gent)) != nullptr)
	{
		return;
	}

	if (cg_hudFiles.integer)
	{
		constexpr int x = 0;
		constexpr int y = SCREEN_HEIGHT - 80;

		SimpleHud_DrawString(x + cgs.widthRatioCoef * 16, y + 40, va("%i", cg.snap->ps.stats[STAT_HEALTH]),
			colorTable[CT_HUD_RED]);

		SimpleHud_DrawString(x + cgs.widthRatioCoef * (18 + 14), y + 40 + 14, va("%i", cg.snap->ps.stats[STAT_ARMOR]),
			colorTable[CT_HUD_GREEN]);

		CG_DrawSimpleForcePower(cent);

		if (cent->currentState.weapon == WP_SABER)
		{
			CG_DrawSimpleSaberStyle(cent);

			if (cg.snap->ps.blockPoints < BLOCK_POINTS_MAX)
			{
				//draw it as long as it isn't full
				CG_DrawoldblockPoints(cent);
			}
		}
		else
		{
			CG_DrawSimpleAmmo(cent);
		}

		return;
	}

	if (cg_SerenityJediEngineHudMode.integer == 2)
	{
		if (cg.snap->ps.forcePowersActive & 1 << FP_GRIP
			|| cg.snap->ps.forcePowersActive & 1 << FP_DRAIN
			|| cg.snap->ps.forcePowersActive & 1 << FP_LIGHTNING
			|| cg.snap->ps.forcePowersActive & 1 << FP_RAGE
			|| cg.snap->ps.forcePowersActive & 1 << FP_DESTRUCTION
			|| cg.snap->ps.forcePowersActive & 1 << FP_DEADLYSIGHT
			|| cg.snap->ps.forcePowersActive & 1 << FP_LIGHTNING_STRIKE
			|| cg.snap->ps.forcePowersActive & 1 << FP_FEAR)
		{
			hudTintColor = redhudtint;
		}
		else if (cg.snap->ps.forcePowersActive & 1 << FP_ABSORB
			|| cg.snap->ps.forcePowersActive & 1 << FP_HEAL
			|| cg.snap->ps.forcePowersActive & 1 << FP_PROTECT
			|| cg.snap->ps.forcePowersActive & 1 << FP_TELEPATHY
			|| cg.snap->ps.forcePowersActive & 1 << FP_STASIS
			|| cg.snap->ps.forcePowersActive & 1 << FP_REPULSE
			|| cg.snap->ps.forcePowersActive & 1 << FP_GRASP
			|| cg.snap->ps.forcePowersActive & 1 << FP_PROJECTION
			|| cg.snap->ps.forcePowersActive & 1 << FP_BLAST)
		{
			hudTintColor = bluehudtint;
		}
		else
		{
			hudTintColor = colorTable[CT_WHITE];
		}
	}

	// Draw the lower left section of the HUD
	if (cgi_UI_GetMenuInfo("lefthud", &section_x_pos, &section_y_pos, &section_width, &section_height))
	{
		// Draw all the HUD elements
		cgi_UI_Menu_Paint(cgi_UI_GetMenuByName("lefthud"), qtrue);

		// Draw armor & health values
		if (cg_drawStatus.integer == 2)
		{
			if (cg_SerenityJediEngineHudMode.integer == 1)
			{
				CG_DrawSmallStringColor(section_x_pos + 5, section_y_pos - 60,
					va("Armor:%d", cg.snap->ps.stats[STAT_ARMOR]), colorTable[CT_HUD_GREEN]);
				CG_DrawSmallStringColor(section_x_pos + 5, section_y_pos - 40,
					va("Health:%d", cg.snap->ps.stats[STAT_HEALTH]), colorTable[CT_HUD_GREEN]);
			}
			else if (cg_SerenityJediEngineHudMode.integer == 2)
			{
				CG_DrawSmallStringColor(section_x_pos + 5, section_y_pos - 60,
					va("Armor:%d", cg.snap->ps.stats[STAT_ARMOR]), colorTable[CT_HUD_GREEN]);
				CG_DrawSmallStringColor(section_x_pos + 5, section_y_pos - 40,
					va("Health:%d", cg.snap->ps.stats[STAT_HEALTH]), colorTable[CT_HUD_GREEN]);
			}
			else
			{
				CG_DrawSmallStringColor(section_x_pos + 5, section_y_pos - 60,
					va("Armor:%d", cg.snap->ps.stats[STAT_ARMOR]), colorTable[CT_HUD_GREEN]);
				CG_DrawSmallStringColor(section_x_pos + 5, section_y_pos - 40,
					va("Health:%d", cg.snap->ps.stats[STAT_HEALTH]), colorTable[CT_HUD_GREEN]);
			}
		}

		// Print frame

		if (cg_SerenityJediEngineHudMode.integer == 1)
		{// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_LEFT].color);

			CG_DrawPic(
				otherHUDBits[OHB_SCANLINE_LEFT].xPos * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].yPos,
				otherHUDBits[OHB_SCANLINE_LEFT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].height,
				otherHUDBits[OHB_SCANLINE_LEFT].background
			);
			CG_DrawHUDJK2LeftFrame1(0.1, 400);
			CG_DrawHUDJK2LeftFrame2(0.1, 400);

			CG_DrawJK2Armor(cent, 0.1, 400);

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawJK2HealthSJE(cent, 0.1 + 21, 400 + 21);
			}
			else
			{
				CG_DrawJK2Health(cent, 0.1, 400);
			}

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawJK2blockPoints(23.7, 424);
			}
		}
		else if (cg_SerenityJediEngineHudMode.integer == 2)
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_LEFT].color);

			CG_DrawPic(
				otherHUDBits[OHB_SCANLINE_LEFT].xPos * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].yPos,
				otherHUDBits[OHB_SCANLINE_LEFT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].height,
				otherHUDBits[OHB_SCANLINE_LEFT].background
			);
			CG_DrawHUDJK2LeftFrame1(0.1, 400);
			CG_DrawHUDJK2LeftFrame2(0.1, 400);

			CG_DrawJK2Armor(cent, 0.1, 400);

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawJK2HealthSJE(cent, 0.1 + 21, 400 + 21);
			}
			else
			{
				CG_DrawJK2Health(cent, 0.1, 400);
			}

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawJK2blockPoints(23.7, 424);
			}
		}
		else if (cg_SerenityJediEngineHudMode.integer == 3)
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_LEFT].color);

			CG_DrawPic(
				otherHUDBits[OHB_SCANLINE_LEFT].xPos * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].yPos,
				otherHUDBits[OHB_SCANLINE_LEFT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].height,
				otherHUDBits[OHB_SCANLINE_LEFT].background
			);
			CG_DrawHUDJK2LeftFrame1(0.1, 400);
			CG_DrawHUDJK2LeftFrame2(0.1, 400);

			CG_DrawJK2Armor(cent, 0.1, 400);

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawJK2HealthSJE(cent, 0.1 + 21, 400 + 21);
			}
			else
			{
				CG_DrawJK2Health(cent, 0.1, 400);
			}

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawJK2blockPoints(23.7, 424);
			}
		} // new df hud
		else if (cg_SerenityJediEngineHudMode.integer == 4 || cg_SerenityJediEngineHudMode.integer == 5) // left hud
		{
			CG_DrawDF_Health();
			CG_DrawDF_Armor();

			if (cg.snap->ps.jetpackFuel < 100 && cg_SerenityJediEngineMode.integer)
			{
				if (cent->gent->client->NPC_class == CLASS_MANDALORIAN
					|| cent->gent->client->NPC_class == CLASS_JANGO
					|| cent->gent->client->NPC_class == CLASS_JANGODUAL
					|| cent->gent->client->NPC_class == CLASS_BOBAFETT || draw_jetpack_fuel_rocket_trooper_player(cent))
				{
					//draw it as long as it isn't full
					CG_DrawDF_JetPackFuel();
				}
			}
			else if (cg.snap->ps.cloakFuel < 100)
			{
				//draw it as long as it isn't full
				CG_DrawDF_CloakFuel();
			}
			else if (cg.snap->ps.BarrierFuel < 100)
			{
				//draw it as long as it isn't full
				CG_DrawDF_BarrierFuel();
			}
			else
			{
				// Draw stamina by default
				CG_DrawDF_Stamina_Sprint();
			}

			// left hud frame, center, ring
			if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
			{
				CG_DrawDF_Pic(OHB_LEFT_FRAME_DF);
				CG_DrawDF_Pic(OHB_LEFT_CENTER_DF);
				CG_DrawDF_Pic(OHB_LEFT_RING_DF);
			}
			else // horizontal
			{
				CG_DrawDF_Pic(OHB_LEFT_FRAME_DF2);
				CG_DrawDF_RotatePic(OHB_LEFT_CENTER_DF, 90, 37, 42, 1.1f, 0.95f);
				CG_DrawDF_RotatePic(OHB_LEFT_RING_DF, 90, 37, 42, 1.1f, 0.95f);
			}

			CG_DrawForceSelect_side();
			CG_DrawForceSelect_text();

			cg_draw_inventory_select_side();
			CG_DrawInventorySelect_text();

			// this side need blockpoints , armour , health
		}
		else
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_LEFT].color);

			CG_DrawPic(
				otherHUDBits[OHB_SCANLINE_LEFT].xPos * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].yPos,
				otherHUDBits[OHB_SCANLINE_LEFT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_LEFT].height,
				otherHUDBits[OHB_SCANLINE_LEFT].background
			);

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				cgi_R_SetColor(otherHUDBits[OHB_CLASSIC_FRAME_LEFT].color);

				CG_DrawPic(
					otherHUDBits[OHB_CLASSIC_FRAME_LEFT].xPos * hud_ratio,
					otherHUDBits[OHB_CLASSIC_FRAME_LEFT].yPos,
					otherHUDBits[OHB_CLASSIC_FRAME_LEFT].width * hud_ratio,
					otherHUDBits[OHB_CLASSIC_FRAME_LEFT].height,
					otherHUDBits[OHB_CLASSIC_FRAME_LEFT].background
				);
			}
			else
			{
				cgi_R_SetColor(otherHUDBits[OHB_FRAME_LEFT].color);

				CG_DrawPic(
					otherHUDBits[OHB_FRAME_LEFT].xPos * hud_ratio,
					otherHUDBits[OHB_FRAME_LEFT].yPos,
					otherHUDBits[OHB_FRAME_LEFT].width * hud_ratio,
					otherHUDBits[OHB_FRAME_LEFT].height,
					otherHUDBits[OHB_FRAME_LEFT].background
				);
			}
			CG_DrawArmor(hud_ratio);
			CG_DrawHealth(hud_ratio);

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				CG_DrawClassicblockPoints(cent, hud_ratio);
			}
		}
	}

	// Draw the lower right section of the HUD
	if (cgi_UI_GetMenuInfo("righthud", &section_x_pos, &section_y_pos, &section_width, &section_height))
	{
		// Draw all the HUD elements
		cgi_UI_Menu_Paint(cgi_UI_GetMenuByName("righthud"), qtrue);

		// Draw ammo & health values
		if (cg_drawStatus.integer == 2)
		{
			if (cent->currentState.weapon != WP_SABER && cent->currentState.weapon != WP_STUN_BATON && cent->
				currentState.weapon != WP_MELEE && cent->gent)
			{
				const int value = cg.snap->ps.ammo[weaponData[cent->currentState.weapon].ammoIndex];
				CG_DrawSmallStringColor(section_x_pos, section_y_pos - 60, va("Ammo:%d", value),
					colorTable[CT_HUD_GREEN]);
			}
			CG_DrawSmallStringColor(section_x_pos, section_y_pos - 40,
				va("Force:%d", cent->gent->client->ps.forcePower),
				colorTable[CT_HUD_GREEN]);
		}

		// Print frame
		if (cg_SerenityJediEngineHudMode.integer == 1)
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_RIGHT].color);

			CG_DrawPic(
				SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_SCANLINE_RIGHT].xPos) * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].yPos,
				otherHUDBits[OHB_SCANLINE_RIGHT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].height,
				otherHUDBits[OHB_SCANLINE_RIGHT].background
			);

			CG_DrawHUDJK2RightFrame1(560, 400);
			CG_DrawHUDJK2RightFrame2(560, 400);
			CG_DrawJK2ForcePower(cent, 560, 400);

			if (cent->currentState.weapon == WP_MELEE || cent->currentState.weapon == WP_STUN_BATON)
			{
				//
			}
			else
			{
				CG_DrawJK2Ammo(cent, 560, 400);

				if (cg_SerenityJediEngineMode.integer)
				{
					CG_DrawJK2blockingMode(cent);
				}
				if (cent->currentState.weapon == WP_SABER && cg_SerenityJediEngineMode.integer)
				{
					CG_DrawJK2SaberFatigue(cent, 560, 400);
				}
				else
				{
					CG_DrawJK2GunFatigue(cent, 560, 400);
				}
			}
		}
		else if (cg_SerenityJediEngineHudMode.integer == 2)
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_RIGHT].color);

			CG_DrawPic(
				SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_SCANLINE_RIGHT].xPos) * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].yPos,
				otherHUDBits[OHB_SCANLINE_RIGHT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].height,
				otherHUDBits[OHB_SCANLINE_RIGHT].background
			);

			CG_DrawHUDJK2RightFrame1(560, 400);
			CG_DrawHUDJK2RightFrame2(560, 400);
			CG_DrawJK2ForcePower(cent, 560, 400);

			if (cent->currentState.weapon == WP_MELEE || cent->currentState.weapon == WP_STUN_BATON)
			{
				//
			}
			else
			{
				CG_DrawJK2Ammo(cent, 560, 400);

				if (cg_SerenityJediEngineMode.integer)
				{
					CG_DrawJK2blockingMode(cent);
				}
				if (cent->currentState.weapon == WP_SABER && cg_SerenityJediEngineMode.integer)
				{
					CG_DrawJK2SaberFatigue(cent, 560, 400);
				}
				else
				{
					CG_DrawJK2GunFatigue(cent, 560, 400);
				}
			}
		}
		else if (cg_SerenityJediEngineHudMode.integer == 3)
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_RIGHT].color);

			CG_DrawPic(
				SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_SCANLINE_RIGHT].xPos) * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].yPos,
				otherHUDBits[OHB_SCANLINE_RIGHT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].height,
				otherHUDBits[OHB_SCANLINE_RIGHT].background
			);

			CG_DrawHUDJK2RightFrame1(560, 400);
			CG_DrawHUDJK2RightFrame2(560, 400);
			CG_DrawJK2ForcePower(cent, 560, 400);

			if (cent->currentState.weapon == WP_MELEE || cent->currentState.weapon == WP_STUN_BATON)
			{
				//
			}
			else
			{
				CG_DrawJK2Ammo(cent, 560, 400);

				if (cg_SerenityJediEngineMode.integer)
				{
					CG_DrawJK2blockingMode(cent);
				}
				if (cent->currentState.weapon == WP_SABER && cg_SerenityJediEngineMode.integer)
				{
					CG_DrawJK2SaberFatigue(cent, 560, 400);
				}
				else
				{
					CG_DrawJK2GunFatigue(cent, 560, 400);
				}
			}
		} // new df hud
		else if (cg_SerenityJediEngineHudMode.integer == 4 || cg_SerenityJediEngineHudMode.integer == 5) // right hud
		{
			CG_DrawDF_ForcePowers(cent);

			if (cent->currentState.weapon == WP_SABER && cg_SerenityJediEngineMode.integer == 2)
			{
				CG_DrawDF_BlockPoints(cent);
			}

			if (cent->currentState.weapon != WP_SABER && cent->currentState.weapon != WP_MELEE && cent->currentState.weapon != WP_STUN_BATON)
			{
				CG_DrawDF_Ammo(cent);
			}

			// right hud frame
			if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
			{
				CG_DrawDF_Pic(OHB_RIGHT_FRAME_DF);
			}
			else // horizontal
			{
				CG_DrawDF_Pic(OHB_RIGHT_FRAME_DF2);
			}

			if (cent->currentState.weapon == WP_SABER)
			{
				CG_DrawDF_SaberStyle_Fatigue(cent);
			}

			if (cent->currentState.weapon != WP_SABER && cent->currentState.weapon != WP_MELEE)
			{
				CG_DrawDF_GunFatigue(cent);
			}

			// right hud center and ring
			if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
			{
				CG_DrawDF_Pic(OHB_RIGHT_CENTER_DF);
				CG_DrawDF_Pic(OHB_RIGHT_RING_DF);
			}
			else // horizontal
			{
				CG_DrawDF_RotatePic(OHB_RIGHT_CENTER_DF, -90, 11, 42, 1.1f, 0.95f); // 1.32 0.75
				CG_DrawDF_RotatePic(OHB_RIGHT_RING_DF, -90, 11, 42, 1.1f, 0.95f);
			}

			if (cent->currentState.weapon == WP_SABER)
			{
				if (cg.predictedPlayerState.ManualBlockingFlags & 1 << HOLDINGBLOCK)
				{
					// right hud block
					if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
					{
						CG_DrawDF_Pic(OHB_BLOCK_DF);
					}
					else // horizontal
					{
						CG_DrawDF_RotatePic(OHB_BLOCK_DF, -90, 0, 39, 1.1f, 0.95f);
					}
				}

				if (cg.predictedPlayerState.ManualBlockingFlags & 1 << PERFECTBLOCKING)
				{
					// right hud mblock
					if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
					{
						CG_DrawDF_Pic(OHB_MBLOCK_DF);
					}
					else // horizontal
					{
						CG_DrawDF_RotatePic(OHB_MBLOCK_DF, -90, 0, 39, 1.1f, 0.95f);
					}
				}
				else
				{
					// right hud hilt/saber
					if (cg_SerenityJediEngineHudMode.integer == 4) // vertical
					{
						CG_DrawDF_Pic(OHB_HILT_DF);
					}
					else // horizontal
					{
						CG_DrawDF_RotatePic(OHB_HILT_DF, -90, 0, 39, 1.1f, 0.95f);
					}
				}
			}
			else if (cent->currentState.weapon == WP_MELEE)
			{
				cg_drawweapontype(cent);
			}
			else
			{
				cg_drawweapontype(cent);
			}

			CG_DrawWeaponSelect_text();

			// this side needs force powers / ammo /weapon type / gun fatigue / saber style/ blocking mode/ saber fatigue
		}
		else
		{
			// Print scanline
			cgi_R_SetColor(otherHUDBits[OHB_SCANLINE_RIGHT].color);

			CG_DrawPic(
				SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_SCANLINE_RIGHT].xPos) * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].yPos,
				otherHUDBits[OHB_SCANLINE_RIGHT].width * hud_ratio,
				otherHUDBits[OHB_SCANLINE_RIGHT].height,
				otherHUDBits[OHB_SCANLINE_RIGHT].background
			);

			if (cg_SerenityJediEngineMode.integer == 2 && cent->currentState.weapon == WP_SABER)
			{
				cgi_R_SetColor(otherHUDBits[OHB_CLASSIC_FRAME_RIGHT].color);
				CG_DrawPic(
					SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_CLASSIC_FRAME_RIGHT].xPos) * hud_ratio,
					otherHUDBits[OHB_CLASSIC_FRAME_RIGHT].yPos,
					otherHUDBits[OHB_CLASSIC_FRAME_RIGHT].width * hud_ratio,
					otherHUDBits[OHB_CLASSIC_FRAME_RIGHT].height,
					otherHUDBits[OHB_CLASSIC_FRAME_RIGHT].background
				);
			}
			else
			{
				cgi_R_SetColor(otherHUDBits[OHB_FRAME_RIGHT].color);
				CG_DrawPic(
					SCREEN_WIDTH - (SCREEN_WIDTH - otherHUDBits[OHB_FRAME_RIGHT].xPos) * hud_ratio,
					otherHUDBits[OHB_FRAME_RIGHT].yPos,
					otherHUDBits[OHB_FRAME_RIGHT].width * hud_ratio,
					otherHUDBits[OHB_FRAME_RIGHT].height,
					otherHUDBits[OHB_FRAME_RIGHT].background
				);
			}
			CG_DrawForcePower(cent, hud_ratio);

			// Draw ammo tics or saber style
			if (cent->currentState.weapon == WP_SABER)
			{
				CG_DrawSaberStyle(cent, hud_ratio);
			}
			else
			{
				CG_DrawAmmo(cent, hud_ratio);
			}

			if (cent->currentState.weapon == WP_SABER)
			{
				CG_DrawClassicsaberfatigue(cent, hud_ratio);
				CG_DrawNewClassicsaberfatigue(cent, hud_ratio);
			}
			else
			{
			}
		}

		if (!(cg_SerenityJediEngineHudMode.integer == 4 || cg_SerenityJediEngineHudMode.integer == 5))
		{
			if (cg_SerenityJediEngineMode.integer)
			{
				if ((cent->gent->client->NPC_class == CLASS_MANDALORIAN
					|| cent->gent->client->NPC_class == CLASS_JANGO
					|| cent->gent->client->NPC_class == CLASS_JANGODUAL
					|| cent->gent->client->NPC_class == CLASS_BOBAFETT || draw_jetpack_fuel_rocket_trooper_player(
						cent))
					&& cg.snap->ps.jetpackFuel < 100)
				{
					//draw it as long as it isn't full
					CG_DrawJetpackFuel();
				}
			}
			if (cg.snap->ps.cloakFuel < 100)
			{
				//draw it as long as it isn't full
				CG_DrawCloakFuel();
			}
			if (cg.snap->ps.BarrierFuel < 100)
			{
				//draw it as long as it isn't full
				CG_DrawBarrierFuel();
			}

			if (cg.snap->ps.sprintFuel < 100)
			{
				//draw it as long as it isn't full
				CG_DrawSprintFuel();
			}
		}
	}
}

/*
================
CG_ClearDataPadCvars
================
*/
static void CG_ClearDataPadCvars()
{
	cgi_Cvar_Set("cg_updatedDataPadForcePower1", "0");
	cgi_Cvar_Update(&cg_updatedDataPadForcePower1);
	cgi_Cvar_Set("cg_updatedDataPadForcePower2", "0");
	cgi_Cvar_Update(&cg_updatedDataPadForcePower2);
	cgi_Cvar_Set("cg_updatedDataPadForcePower3", "0");
	cgi_Cvar_Update(&cg_updatedDataPadForcePower3);

	cgi_Cvar_Set("cg_updatedDataPadObjective", "0");
	cgi_Cvar_Update(&cg_updatedDataPadObjective);
}

/*
================
CG_DrawHUDRightFrame1
================
*/
static void CG_DrawHUDRightFrame1(const int x, const int y)
{
	cgi_R_SetColor(colorTable[CT_WHITE]);
	// Inner gray wire frame
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDInnerRight); //
}

/*
================
CG_DrawHUDRightFrame2
================
*/
static void CG_DrawHUDRightFrame2(const int x, const int y)
{
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDRightFrame); // Metal frame
}

/*
================
CG_DrawHUDLeftFrame1
================
*/
static void CG_DrawHUDLeftFrame1(const int x, const int y)
{
	// Inner gray wire frame
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDInnerLeft);
}

void CG_DrawHUDJK2LeftFrame1(const int x, const int y)
{
	// Inner gray wire frame
	cgi_R_SetColor(hudTintColor);
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDInnerLeft);
}

void CG_DrawHUDJK2RightFrame1(const int x, const int y)
{
	cgi_R_SetColor(hudTintColor);
	// Inner gray wire frame
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDInnerRight);
}

/*
================
CG_DrawHUDLeftFrame2
================
*/
static void CG_DrawHUDLeftFrame2(const int x, const int y)
{
	// Inner gray wire frame
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDLeftFrame); // Metal frame
}

void CG_DrawHUDJK2LeftFrame2(const int x, const int y)
{
	// Inner gray wire frame
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDLeftFrame); // Metal frame
}

void CG_DrawHUDJK2RightFrame2(const int x, const int y)
{
	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(x, y, 80, 80, cgs.media.JK2HUDRightFrame); // Metal frame
}

/*
================
CG_DrawDatapadHealth
================
*/
static void CG_DrawDatapadHealth(const int x, const int y)
{
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	memcpy(calc_color, colorTable[CT_HUD_RED], sizeof(vec4_t));
	const float health_percent = static_cast<float>(ps->stats[STAT_HEALTH]) / ps->stats[STAT_MAX_HEALTH];
	calc_color[0] *= health_percent;
	calc_color[1] *= health_percent;
	calc_color[2] *= health_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDHealth);

	// Draw the ticks
	if (cg.HUDHealthFlag)
	{
		cgi_R_SetColor(colorTable[CT_HUD_RED]);
		CG_DrawPic(x, y, 80, 80, cgs.media.HUDHealthTic);
	}

	cgi_R_SetColor(colorTable[CT_HUD_RED]);
	CG_DrawNumField(x + 16, y + 40, 3, ps->stats[STAT_HEALTH], 6, 12,
		NUM_FONT_SMALL, qtrue);
}

static void CG_DrawDataPadArmor(const int x, const int y)
{
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	//	Outer Armor circular
	memcpy(calc_color, colorTable[CT_HUD_GREEN], sizeof(vec4_t));

	const float hold = ps->stats[STAT_ARMOR] - ps->stats[STAT_MAX_HEALTH] / static_cast<float>(2);
	float armor_percent = static_cast<float>(hold) / (ps->stats[STAT_MAX_HEALTH] / static_cast<float>(2));
	if (armor_percent < 0)
	{
		armor_percent = 0;
	}
	calc_color[0] *= armor_percent;
	calc_color[1] *= armor_percent;
	calc_color[2] *= armor_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDArmor1);

	// Inner Armor circular
	if (armor_percent > 0)
	{
		armor_percent = 1;
	}
	else
	{
		armor_percent = static_cast<float>(ps->stats[STAT_ARMOR]) / (ps->stats[STAT_MAX_HEALTH] / static_cast<float>(2));
	}
	memcpy(calc_color, colorTable[CT_HUD_GREEN], sizeof(vec4_t));
	calc_color[0] *= armor_percent;
	calc_color[1] *= armor_percent;
	calc_color[2] *= armor_percent;
	cgi_R_SetColor(calc_color);
	CG_DrawPic(x, y, 80, 80, cgs.media.HUDArmor2);

	cgi_R_SetColor(colorTable[CT_HUD_GREEN]);
	CG_DrawNumField(x + 16 + 14, y + 40 + 14, 3, ps->stats[STAT_ARMOR], 6, 12,
		NUM_FONT_SMALL, qfalse);
}

/*
================
CG_DrawForcePower
================
*/
static void CG_DrawDatapadForcePower(const centity_t* cent, const int x, const int y)
{
	vec4_t calc_color;
	float extra = 0, percent;

	if (!cent->gent->client->ps.forcePowersKnown)
	{
		return;
	}
	const float inc = static_cast<float>(cent->gent->client->ps.forcePowerMax) / MAX_DATAPADTICS;
	float value = cent->gent->client->ps.forcePower;

	if (value > cent->gent->client->ps.forcePowerMax)
	{
		//supercharged with force
		extra = value - cent->gent->client->ps.forcePowerMax;
		value = cent->gent->client->ps.forcePowerMax;
	}

	for (int i = MAX_DATAPADTICS - 1; i >= 0; i--)
	{
		if (extra)
		{
			//supercharged
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			percent = 0.75f + sin(cg.time * 0.005f) * (extra / cent->gent->client->ps.forcePowerMax * 0.25f);
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else if (value <= 0) // partial tic
		{
			memcpy(calc_color, colorTable[CT_BLACK], sizeof(vec4_t));
		}
		else if (value < inc) // partial tic
		{
			memcpy(calc_color, colorTable[CT_LTGREY], sizeof(vec4_t));
			percent = value / inc;
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else
		{
			memcpy(calc_color, colorTable[CT_LTGREY], sizeof(vec4_t));
		}

		cgi_R_SetColor(calc_color);
		CG_DrawPic(x + forceTicPos[i].x,
			y + forceTicPos[i].y,
			forceTicPos[i].width,
			forceTicPos[i].height,
			forceTicPos[i].tic);

		value -= inc;
	}
}

/*
================
CG_DrawAmmo
================
*/
static void CG_DrawDataPadAmmo(const centity_t* cent, const int x, const int y)
{
	int num_color_i;
	vec4_t calc_color;

	const playerState_t* ps = &cg.snap->ps;

	if (!cent->currentState.weapon) // We don't have a weapon right now
	{
		return;
	}

	if (cent->currentState.weapon == WP_STUN_BATON || cent->currentState.weapon == WP_MELEE)
	{
		return;
	}

	if (cent->currentState.weapon == WP_SABER && cent->gent)
	{
		cgi_R_SetColor(colorTable[CT_WHITE]);

		if (!cg.saberAnimLevelPending && cent->gent->client)
		{
			//uninitialized after a loadgame, cheat across and get it
			cg.saberAnimLevelPending = cent->gent->client->ps.saber_anim_level;
		}
		// don't need to draw ammo, but we will draw the current saber style in this window
		switch (cg.saberAnimLevelPending)
		{
		case SS_FAST:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleFast);
			break;
		case SS_MEDIUM:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleMed);
			break;
		case SS_STRONG:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleStrong);
			break;
		case SS_DESANN:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleDesann);
			break;
		case SS_TAVION:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleTavion);
			break;
		case SS_DUAL:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleDuels);
			break;
		case SS_STAFF:
			CG_DrawPic(x, y, 80, 40, cgs.media.HUDSaberStyleStaff);
			break;
		default:;
		}
		return;
	}
	float value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	if (value < 0) // No ammo
	{
		return;
	}

	//
	// ammo
	//
	if (cg.oldammo < value)
	{
		cg.oldAmmoTime = cg.time + 200;
	}

	cg.oldammo = value;

	// Firing or reloading?
	if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING
		&& cg.predictedPlayerState.weaponTime > 100)
	{
		num_color_i = CT_LTGREY;
	}
	else
	{
		if (value > 0)
		{
			if (cg.oldAmmoTime > cg.time)
			{
				num_color_i = CT_YELLOW;
			}
			else
			{
				num_color_i = CT_HUD_ORANGE;
			}
		}
		else
		{
			num_color_i = CT_RED;
		}
	}

	cgi_R_SetColor(colorTable[num_color_i]);
	CG_DrawNumField(x + 29, y + 26, 3, value, 6, 12, NUM_FONT_SMALL, qfalse);

	const float inc = static_cast<float>(ammoData[weaponData[cent->currentState.weapon].ammoIndex].max) /
		MAX_DATAPADTICS;
	value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];

	for (int i = MAX_DATAPADTICS - 1; i >= 0; i--)
	{
		if (value <= 0) // partial tic
		{
			memcpy(calc_color, colorTable[CT_BLACK], sizeof(vec4_t));
		}
		else if (value < inc) // partial tic
		{
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
			const float percent = value / inc;
			calc_color[0] *= percent;
			calc_color[1] *= percent;
			calc_color[2] *= percent;
		}
		else
		{
			memcpy(calc_color, colorTable[CT_WHITE], sizeof(vec4_t));
		}

		cgi_R_SetColor(calc_color);
		CG_DrawPic(x + ammoTicPos[i].x,
			y + ammoTicPos[i].y,
			ammoTicPos[i].width,
			ammoTicPos[i].height,
			ammoTicPos[i].tic);

		value -= inc;
	}
}

/*
================
CG_DrawDataPadHUD
================
*/
void CG_DrawDataPadHUD(const centity_t* cent)
{
	int x = 34;
	constexpr int y = 286;

	CG_DrawHUDLeftFrame1(x, y);
	CG_DrawHUDLeftFrame2(x, y);
	CG_DrawDataPadArmor(x, y);
	CG_DrawDatapadHealth(x, y);

	x = 526;

	if (missionInfo_Updated && (cg_updatedDataPadForcePower1.integer || cg_updatedDataPadObjective.integer))
	{
		// Stop flashing light
		cg.missionInfoFlashTime = 0;
		missionInfo_Updated = qfalse;

		// Set which force power to show
		if (cg_updatedDataPadForcePower1.integer)
		{
			cg.DataPadforcepowerSelect = cg_updatedDataPadForcePower1.integer - 1; // Not pretty, I know
			if (cg.DataPadforcepowerSelect >= MAX_DPSHOWPOWERS)
			{
				//duh
				cg.DataPadforcepowerSelect = MAX_DPSHOWPOWERS - 1;
			}
			else if (cg.DataPadforcepowerSelect < 0)
			{
				cg.DataPadforcepowerSelect = 0;
			}
		}
	}

	CG_DrawHUDRightFrame1(x, y);
	CG_DrawHUDRightFrame2(x, y);
	CG_DrawDatapadForcePower(cent, x, y);
	CG_DrawDataPadAmmo(cent, x, y);
	CG_DrawMessageLit(x, y);

	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawPic(0, 0, 640, 480, cgs.media.dataPadFrame);
}

//------------------------
// CG_DrawZoomMask
//------------------------
static void CG_DrawBinocularNumbers(const qboolean power)
{
	cgi_R_SetColor(colorTable[CT_BLACK]);
	CG_DrawPic(212, 367, 200, 40, cgs.media.whiteShader);

	if (power)
	{
		vec4_t color1{};
		// Numbers should be kind of greenish
		color1[0] = 0.2f;
		color1[1] = 0.4f;
		color1[2] = 0.2f;
		color1[3] = 0.3f;
		cgi_R_SetColor(color1);

		// Draw scrolling numbers, use intervals 10 units apart--sorry, this section of code is just kind of hacked
		//	up with a bunch of magic numbers.....
		int val = static_cast<int>((cg.refdefViewAngles[YAW] + 180) / 10) * 10;
		const float off = cg.refdefViewAngles[YAW] + 180 - val;

		for (int i = -10; i < 30; i += 10)
		{
			val -= 10;

			if (val < 0)
			{
				val += 360;
			}

			// we only want to draw the very far left one some of the time, if it's too far to the left it will poke outside the mask.
			if (off > 3.0f && i == -10 || i > -10)
			{
				// draw the value, but add 200 just to bump the range up...arbitrary, so change it if you like
				CG_DrawNumField(155 + i * 10 + off * 10, 374, 3, val + 200, 24, 14, NUM_FONT_CHUNKY, qtrue);
				CG_DrawPic(245 + (i - 1) * 10 + off * 10, 376, 6, 6, cgs.media.whiteShader);
			}
		}

		CG_DrawPic(212, 367, 200, 28, cgs.media.binocularOverlay);
	}
}

/*
================
CG_DrawZoomMask

================
*/
extern float cg_zoomFov; //from cg_view.cpp

static void CG_DrawZoomMask()
{
	vec4_t color1{};
	float level;
	static qboolean flip = qtrue;
	const float charge = cg.snap->ps.batteryCharge / static_cast<float>(MAX_BATTERIES);
	// convert charge to a percentage
	qboolean power = qfalse;

	const centity_t* cent = &cg_entities[0];

	if (charge > 0.0f)
	{
		power = qtrue;
	}

	//-------------
	// Binoculars
	//--------------------------------
	if (cg.zoomMode == 1)
	{
		CG_RegisterItemVisuals(ITM_BINOCULARS_PICKUP);

		// zoom level
		level = (80.0f - cg_zoomFov) / 80.0f;

		// ...so we'll clamp it
		if (level < 0.0f)
		{
			level = 0.0f;
		}
		else if (level > 1.0f)
		{
			level = 1.0f;
		}

		// Using a magic number to convert the zoom level to scale amount
		level *= 162.0f;

		if (power)
		{
			// draw blue tinted distortion mask, trying to make it as small as is necessary to fill in the viewable area
			cgi_R_SetColor(colorTable[CT_WHITE]);
			CG_DrawPic(34, 48, 570, 362, cgs.media.binocularStatic);
		}

		CG_DrawBinocularNumbers(power);

		// Black out the area behind the battery display
		cgi_R_SetColor(colorTable[CT_DKGREY]);
		CG_DrawPic(50, 389, 161, 16, cgs.media.whiteShader);

		if (power)
		{
			color1[0] = sin(cg.time * 0.01f) * 0.5f + 0.5f;
			color1[0] = color1[0] * color1[0];
			color1[1] = color1[0];
			color1[2] = color1[0];
			color1[3] = 1.0f;

			cgi_R_SetColor(color1);

			CG_DrawPic(82, 94, 16, 16, cgs.media.binocularCircle);
		}

		CG_DrawPic(0, 0, 640, 480, cgs.media.binocularMask);

		if (power)
		{
			// Flickery color
			color1[0] = 0.7f + Q_flrand(-1.0f, 1.0f) * 0.1f;
			color1[1] = 0.8f + Q_flrand(-1.0f, 1.0f) * 0.1f;
			color1[2] = 0.7f + Q_flrand(-1.0f, 1.0f) * 0.1f;
			color1[3] = 1.0f;
			cgi_R_SetColor(color1);

			CG_DrawPic(4, 282 - level, 16, 16, cgs.media.binocularArrow);
		}
		else
		{
			// No power color
			color1[0] = 0.15f;
			color1[1] = 0.15f;
			color1[2] = 0.15f;
			color1[3] = 1.0f;
			cgi_R_SetColor(color1);
		}

		// The top triangle bit randomly flips when the power is on
		if (flip && power)
		{
			CG_DrawPic(330, 60, -26, -30, cgs.media.binocularTri);
		}
		else
		{
			CG_DrawPic(307, 40, 26, 30, cgs.media.binocularTri);
		}

		if (Q_flrand(0.0f, 1.0f) > 0.98f && cg.time & 1024)
		{
			flip = static_cast<qboolean>(!flip);
		}

		if (power)
		{
			color1[0] = 1.0f * (charge < 0.2f ? !!(cg.time & 256) : 1);
			color1[1] = charge * color1[0];
			color1[2] = 0.0f;
			color1[3] = 0.2f;

			cgi_R_SetColor(color1);
			CG_DrawPic(60, 394.5f, charge * 141, 5, cgs.media.whiteShader);
		}
	}
	//------------
	// Disruptor
	//--------------------------------
	else if (cg.zoomMode == 2)
	{
		level = (80.0f - cg_zoomFov) / 80.0f;

		// ...so we'll clamp it
		if (level < 0.0f)
		{
			level = 0.0f;
		}
		else if (level > 1.0f)
		{
			level = 1.0f;
		}

		// Using a magic number to convert the zoom level to a rotation amount that correlates more or less with the zoom artwork.
		level *= 103.0f;

		// Draw target mask
		cgi_R_SetColor(colorTable[CT_WHITE]);
		CG_DrawPic(0, 0, 640, 480, cgs.media.disruptorMask);

		// apparently 99.0f is the full zoom level
		if (level >= 99)
		{
			// Fully zoomed, so make the rotating insert pulse
			color1[0] = 1.0f;
			color1[1] = 1.0f;
			color1[2] = 1.0f;
			color1[3] = 0.7f + sin(cg.time * 0.01f) * 0.3f;

			cgi_R_SetColor(color1);
		}

		// Draw rotating insert
		CG_DrawRotatePic2(static_cast<float>(SCREEN_WIDTH) / 2, static_cast<float>(SCREEN_HEIGHT) / 2, SCREEN_WIDTH, SCREEN_HEIGHT, -level,
			cgs.media.disruptorInsert, cgs.widthRatioCoef);

		float max = cg_entities[0].gent->client->ps.ammo[weaponData[WP_DISRUPTOR].ammoIndex] / static_cast<float>(
			ammoData[weaponData[
				WP_DISRUPTOR].ammoIndex].max);

		if (max > 1.0f)
		{
			max = 1.0f;
		}

		color1[0] = (1.0f - max) * 2.0f;
		color1[1] = max * 1.5f;
		color1[2] = 0.0f;
		color1[3] = 1.0f;

		// If we are low on ammo, make us flash
		if (max < 0.15f && cg.time & 512)
		{
			VectorClear(color1);
		}

		if (color1[0] > 1.0f)
		{
			color1[0] = 1.0f;
		}

		if (color1[1] > 1.0f)
		{
			color1[1] = 1.0f;
		}

		cgi_R_SetColor(color1);

		max *= 58.0f;

		for (float i = 18.5f; i <= 18.5f + max; i += 3) // going from 15 to 45 degrees, with 5 degree increments
		{
			const float cx = 320 + sin((i + 90.0f) / 57.296f) * 190;
			const float cy = 240 + cos((i + 90.0f) / 57.296f) * 190;

			CG_DrawRotatePic2(cx, cy, 12, 24, 90 - i, cgs.media.disruptorInsertTick, cgs.widthRatioCoef);
		}

		// FIXME: doesn't know about ammo!! which is bad because it draws charge beyond what ammo you may have..
		if (cg_entities[0].gent->client->ps.weaponstate == WEAPON_CHARGING_ALT)
		{
			cgi_R_SetColor(colorTable[CT_WHITE]);

			// draw the charge level
			max = (cg.time - cg_entities[0].gent->client->ps.weaponChargeTime) / (150.0f * 10.0f);
			// bad hardcodedness 150 is disruptor charge unit and 10 is max charge units allowed.

			if (max > 1.0f)
			{
				max = 1.0f;
			}

			CG_DrawPic2(257, 435, 134 * max, 34, 0, 0, max, 1, cgi_R_RegisterShaderNoMip("gfx/2d/crop_charge"));
		}
	}
	//-----------
	// Light Amp
	//--------------------------------
	else if (cg.zoomMode == 3)
	{
		CG_RegisterItemVisuals(ITM_LA_GOGGLES_PICKUP);

		if (power)
		{
			cgi_R_SetColor(colorTable[CT_WHITE]);
			CG_DrawPic(34, 29, 580, 410, cgs.media.laGogglesStatic);

			CG_DrawPic(570, 140, 12, 160, cgs.media.laGogglesSideBit);

			float light = (128 - cent->gent->lightLevel) * 0.5f;

			if (light < -81) // saber can really jack up local light levels....?magic number??
			{
				light = -81;
			}

			const float pos1 = 220 + light;
			const float pos2 = 220 + cos(cg.time * 0.0004f + light * 0.05f) * 40 + sin(cg.time * 0.0013f + 1) * 20 +
				sin(cg.time * 0.0021f) * 5;

			// Flickery color
			color1[0] = 0.7f + Q_flrand(-1.0f, 1.0f) * 0.2f;
			color1[1] = 0.8f + Q_flrand(-1.0f, 1.0f) * 0.2f;
			color1[2] = 0.7f + Q_flrand(-1.0f, 1.0f) * 0.2f;
			color1[3] = 1.0f;
			cgi_R_SetColor(color1);

			CG_DrawPic(565, pos1, 22, 8, cgs.media.laGogglesBracket);
			CG_DrawPic(558, pos2, 14, 5, cgs.media.laGogglesArrow);
		}

		// Black out the area behind the battery display
		cgi_R_SetColor(colorTable[CT_DKGREY]);
		CG_DrawPic(236, 357, 164, 16, cgs.media.whiteShader);

		if (power)
		{
			// Power bar
			color1[0] = 1.0f * (charge < 0.2f ? !!(cg.time & 256) : 1);
			color1[1] = charge * color1[0];
			color1[2] = 0.0f;
			color1[3] = 0.4f;

			cgi_R_SetColor(color1);
			CG_DrawPic(247.0f, 362.5f, charge * 143.0f, 6, cgs.media.whiteShader);

			// pulsing dot bit
			color1[0] = sin(cg.time * 0.01f) * 0.5f + 0.5f;
			color1[0] = color1[0] * color1[0];
			color1[1] = color1[0];
			color1[2] = color1[0];
			color1[3] = 1.0f;

			cgi_R_SetColor(color1);

			CG_DrawPic(65, 94, 16, 16, cgs.media.binocularCircle);
		}

		CG_DrawPic(0, 0, 640, 480, cgs.media.laGogglesMask);
	}
}

/*
================
CG_DrawStats

================
*/
static void CG_DrawStats()
{
	if (cg_drawStatus.integer == 0)
	{
		return;
	}

	const centity_t* cent = &cg_entities[cg.snap->ps.client_num];

	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		// MIGHT try and draw a custom hud if it wants...
		CG_DrawCustomHealthHud(cent);
		return;
	}

	cgi_UI_MenuPaintAll();

	qboolean draw_hud = qtrue;

	if (cent && cent->gent)
	{
		draw_hud = CG_DrawCustomHealthHud(cent);
	}

	if (draw_hud && cg_drawHUD.integer)
	{
		CG_DrawHUD(cent);
	}
}

/*
===================
CG_DrawPickupItem
===================
*/
constexpr auto PICKUP_ICON_SIZE = 44;
constexpr auto PICKUP_ICON_SMALL = 22;

static void CG_DrawPickupItem()
{
	const int value = cg.itemPickup;
	if (value && cg_items[value].icon != -1)
	{
		const float* fade_color = CG_FadeColor(cg.itemPickupTime, 3000);
		if (fade_color)
		{
			CG_RegisterItemVisuals(value);
			cgi_R_SetColor(fade_color);

			if (cg_SerenityJediEngineHudMode.integer == 4)
			{
				CG_DrawPic(19, 411, PICKUP_ICON_SMALL, PICKUP_ICON_SMALL, cg_items[value].icon);
			}
			else if (cg_SerenityJediEngineHudMode.integer == 5)
			{
				CG_DrawPic(19, 471, PICKUP_ICON_SMALL, PICKUP_ICON_SMALL, cg_items[value].icon);
			}
			else
			{
				CG_DrawPic(290, 20, PICKUP_ICON_SIZE, PICKUP_ICON_SIZE, cg_items[value].icon);
			}
			cgi_R_SetColor(nullptr);
		}
	}
}

void CMD_CGCam_Disable();

/*
===================
CG_DrawPickupItem
===================
*/
static void CG_DrawCredits()
{
	if (!cg.creditsStart)
	{
		//
		cg.creditsStart = qtrue;
		CG_Credits_Init("CREDITS_RAVEN", &colorTable[CT_ICON_BLUE]);
		if (cg_skippingcin.integer)
		{
			//Were skipping a cinematic and it's over now
			gi.cvar_set("timescale", "1");
			gi.cvar_set("skippingCinematic", "0");
		}
	}

	if (cg.creditsStart)
	{
		if (!CG_Credits_Running())
		{
			cgi_Cvar_Set("cg_endcredits", "0");
			CMD_CGCam_Disable();
			cgi_SendConsoleCommand("disconnect\n");
		}
	}
}

//draw the health bar based on current "health" and maxhealth
static void CG_DrawHealthBar(const centity_t* cent, const float ch_x, const float ch_y, const float ch_w, const float ch_h)
{
	vec4_t aColor{};
	vec4_t cColor{};
	const float x = ch_x - ch_w / 2;
	const float y = ch_y - ch_h;

	if (!cent || !cent->gent)
	{
		return;
	}

	if (cent->gent->health < 1)
	{
		return;
	}

	if (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent))
	{
		return;
	}

	if (PM_DeathCinAnim(cent->gent->client->ps.legsAnim))
	{
		return;
	}

	if (cent->gent->client->NPC_class == CLASS_OBJECT
		|| cent->gent->client->NPC_class == CLASS_PROJECTION
		|| cent->gent->client->NPC_class == CLASS_VEHICLE)
	{
		return;
	}

	const float percent = static_cast<float>(cent->gent->health) / static_cast<float>(cent->gent->max_health);

	if (percent <= 0)
	{
		return;
	}

	//color of the bar
	//hostile
	aColor[0] = 1.0f;
	aColor[1] = 0.0f;
	aColor[2] = 0.0f;
	aColor[3] = 0.4f;

	//color of greyed out "missing health"
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.5f;

	//draw the other part greyed out
	CG_FillRect(x - 1.0f + percent * ch_w, y + 1.0f, ch_w - percent * ch_w, ch_h - 2.0f, cColor);

	//draw the border (black)
	CG_DrawRect(x, y, ch_w, ch_h, 1.0f, colorTable[CT_BLACK]);

	//now draw the part to show how much health there is in the color specified
	CG_FillRect(x + 1.0f, y + 1.0f, percent * ch_w - 2.0f, ch_h - 2.0f, aColor);
}

#define HACK_WIDTH		33.5f
#define HACK_HEIGHT		3.5f
//same routine (at least for now), draw progress of a "hack" or whatever
static void CG_DrawHaqrBar(const float chX, const float chY, const float chW, const float chH)
{
	vec4_t aColor{};
	vec4_t cColor{};
	const float x = chX + (chW / 2 - HACK_WIDTH / 2);
	const float y = chY + chH + 8.0f;
	const float percent = ((float)cg.predictedPlayerState.hackingTime - (float)cg.time) / (float)cg.predictedPlayerState.hackingBaseTime * HACK_WIDTH;

	if (percent > HACK_WIDTH || percent < 1.0f)
	{
		return;
	}

	//color of the bar
	aColor[0] = 0.0f;
	aColor[1] = 0.0f;
	aColor[2] = 0.6f;
	aColor[3] = 0.9f;

	//color of greyed out done area
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.1f;

	//draw the background (black)
	CG_DrawRect(x, y, HACK_WIDTH, HACK_HEIGHT, 1.0f, colorTable[CT_BLACK]);

	//now draw the part to show how much health there is in the color specified
	CG_FillRect(x + 1.0f, y + 1.0f, percent - 1.0f, HACK_HEIGHT - 1.0f, aColor);

	//then draw the other part greyed out
	CG_FillRect(x + percent, y + 1.0f, HACK_WIDTH - percent - 1.0f, HACK_HEIGHT - 1.0f, cColor);

	//draw the hacker icon
	CG_DrawPic(x, y - HACK_WIDTH, HACK_WIDTH, HACK_WIDTH, cgs.media.hackerIconShader);
}

static void CG_DrawBlockPointBar(const centity_t* cent, const float ch_x, const float ch_y, const float ch_w, const float ch_h)
{
	vec4_t aColor{};
	vec4_t cColor{};
	const float x = ch_x - ch_w / 2;
	const float y = ch_y - ch_h;

	if (!cent || !cent->gent)
	{
		return;
	}

	if (cent->gent->health < 1)
	{
		return;
	}

	if (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent))
	{
		return;
	}

	if (cent->currentState.weapon != WP_SABER)
	{
		return;
	}

	if (PM_DeathCinAnim(cent->gent->client->ps.legsAnim))
	{
		return;
	}

	if (cent->gent->client->NPC_class == CLASS_OBJECT
		|| cent->gent->client->NPC_class == CLASS_PROJECTION
		|| cent->gent->client->NPC_class == CLASS_VEHICLE)
	{
		return;
	}

	const float block_percent = static_cast<float>(cent->gent->client->ps.blockPoints) / static_cast<float>(
		BLOCK_POINTS_MAX);

	//color of the bar
	//hostile
	aColor[0] = 1.0f;
	aColor[1] = 0.0f;
	aColor[2] = 1.0f;
	aColor[3] = 0.4f;

	//color of greyed out "missing health"
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.5f;

	//draw the other part greyed out
	CG_FillRect(x - 1.0f + block_percent * ch_w, y + 1.0f, ch_w - block_percent * ch_w, ch_h - 2.0f, cColor);

	//draw the border (black)
	CG_DrawRect(x, y, ch_w, ch_h, 1.0f, colorTable[CT_BLACK]);

	//now draw the part to show how much health there is in the color specified
	CG_FillRect(x + 1.0f, y + 1.0f, block_percent * ch_w - 2.0f, ch_h - 2.0f, aColor);
}

static void CG_DrawFatiguePointBar(const centity_t* cent, const float ch_x, const float ch_y, const float ch_w,
	const float ch_h)
{
	vec4_t aColor{};
	vec4_t cColor{};
	const float x = ch_x - ch_w / 2;
	const float y = ch_y - ch_h;

	if (!cent || !cent->gent)
	{
		return;
	}

	if (cent->gent->health < 1)
	{
		return;
	}

	if (!cg_saberinfo.integer)
	{
		if (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent))
		{
			return;
		}
	}

	if (cent->currentState.weapon != WP_SABER)
	{
		return;
	}

	if (PM_DeathCinAnim(cent->gent->client->ps.legsAnim))
	{
		return;
	}

	if (cent->gent->client->NPC_class == CLASS_OBJECT
		|| cent->gent->client->NPC_class == CLASS_PROJECTION
		|| cent->gent->client->NPC_class == CLASS_VEHICLE)
	{
		return;
	}

	const float fatigue_percent = static_cast<float>(cent->gent->client->ps.saberFatigueChainCount) / static_cast<float>
		(MISHAPLEVEL_OVERLOAD);

	//color of the bar
	//hostile
	aColor[0] = 0.8f;
	aColor[1] = 0.8f;
	aColor[2] = 0.8f;
	aColor[3] = 0.8f;

	//color of greyed out "missing health"
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.5f;

	//draw the other part greyed out
	CG_FillRect(x + fatigue_percent * ch_w, y + 1.0f, ch_w - fatigue_percent * ch_w, ch_h - 2.0f, aColor);

	//draw the border (black)
	CG_DrawRect(x, y, ch_w, ch_h, 1.0f, colorTable[CT_BLACK]);

	//now draw the part to show how much fatigue there is in the color specified
	CG_FillRect(x, y + 1.0f, fatigue_percent * ch_w, ch_h - 2.0f, cColor);
}

static void CG_DrawForcePointBar(const centity_t* cent, const float ch_x, const float ch_y, const float ch_w, const float ch_h)
{
	vec4_t aColor{};
	vec4_t cColor{};
	const float x = ch_x - ch_w / 2;
	const float y = ch_y - ch_h;

	if (!cent || !cent->gent)
	{
		return;
	}

	if (cent->gent->health < 1)
	{
		return;
	}

	if (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent))
	{
		return;
	}

	if (cent->currentState.weapon != WP_SABER)
	{
		return;
	}

	if (PM_DeathCinAnim(cent->gent->client->ps.legsAnim))
	{
		return;
	}

	if (cent->gent->client->NPC_class == CLASS_OBJECT
		|| cent->gent->client->NPC_class == CLASS_PROJECTION
		|| cent->gent->client->NPC_class == CLASS_VEHICLE)
	{
		return;
	}

	const float force_percent = static_cast<float>(cent->gent->client->ps.forcePower) / static_cast<float>(cent->gent->
		client->ps.forcePowerMax);

	//color of the bar
	//hostile
	aColor[0] = 0.5f;
	aColor[1] = 0.7f;
	aColor[2] = 1.0f;
	aColor[3] = 0.4f;

	//color of greyed out "missing health"
	cColor[0] = 0.5f;
	cColor[1] = 0.5f;
	cColor[2] = 0.5f;
	cColor[3] = 0.5f;

	//draw the other part greyed out
	CG_FillRect(x - 1.0f + force_percent * ch_w, y + 1.0f, ch_w - force_percent * ch_w, ch_h - 2.0f, cColor);

	//draw the border (black)
	CG_DrawRect(x, y, ch_w, ch_h, 1.0f, colorTable[CT_BLACK]);

	//now draw the part to show how much health there is in the color specified
	CG_FillRect(x + 1.0f, y + 1.0f, force_percent * ch_w - 2.0f, ch_h - 2.0f, aColor);
}

constexpr auto MAX_BLOCKPOINT_BAR_ENTS = 32;
int cg_numBlockPointBarEnts = 0;
int cg_BlockPointBarEnts[MAX_BLOCKPOINT_BAR_ENTS];
constexpr auto BLOCKPOINT_BAR_WIDTH = 50;
constexpr auto BLOCKPOINT_BAR_HEIGHT = 4;
constexpr auto BLOCKPOINT_BAR_RANGE = 200;

static void CG_DrawBlockPointBars()
{
	float ch_x = 0, ch_y = 0;
	vec3_t pos;
	for (int i = 0; i < cg_numBlockPointBarEnts; i++)
	{
		const centity_t* cent = &cg_entities[cg_BlockPointBarEnts[i]];
		if (cent && cent->gent)
		{
			VectorCopy(cent->lerpOrigin, pos);
			pos[2] += cent->gent->maxs[2] + BLOCKPOINT_BAR_HEIGHT + 12;
			if (CG_WorldCoordToScreenCoordFloat(pos, &ch_x, &ch_y))
			{
				if (NPC_IsOversized(cent->gent))
				{
					//on screen
					CG_DrawBlockPointBar(cent, ch_x, ch_y + 10, BLOCKPOINT_BAR_WIDTH, BLOCKPOINT_BAR_HEIGHT);
				}
				else
				{
					//on screen
					CG_DrawBlockPointBar(cent, ch_x, ch_y, BLOCKPOINT_BAR_WIDTH, BLOCKPOINT_BAR_HEIGHT);
				}
			}
		}
	}
}

void CG_AddBlockPointBarEnt(const int entNum)
{
	if (cg_numBlockPointBarEnts >= MAX_BLOCKPOINT_BAR_ENTS)
	{
		return;
	}

	if (DistanceSquared(cg_entities[entNum].lerpOrigin, g_entities[0].client->renderInfo.eyePoint) <
		BLOCKPOINT_BAR_RANGE * BLOCKPOINT_BAR_RANGE)
	{
		cg_BlockPointBarEnts[cg_numBlockPointBarEnts++] = entNum;
	}
}

// saber fatigue begin

constexpr auto MAX_FATIGUEPOINT_BAR_ENTS = 32;
int cg_numFatigueBarEnts = 0;
int cg_FatiguePointBarEnts[MAX_FATIGUEPOINT_BAR_ENTS];
constexpr auto FATIGUE_BAR_WIDTH = 50;
constexpr auto Fatigue_BAR_HEIGHT = 4;
constexpr auto Fatigue_BAR_RANGE = 200;

static void CG_DrawFatiguePointBars()
{
	float ch_x = 0, ch_y = 0;
	vec3_t pos;
	for (int i = 0; i < cg_numFatigueBarEnts; i++)
	{
		const centity_t* cent = &cg_entities[cg_FatiguePointBarEnts[i]];
		if (cent && cent->gent)
		{
			VectorCopy(cent->lerpOrigin, pos);
			pos[2] += cent->gent->maxs[2] + Fatigue_BAR_HEIGHT + 16;
			if (CG_WorldCoordToScreenCoordFloat(pos, &ch_x, &ch_y))
			{
				if (NPC_IsOversized(cent->gent))
				{
					//on screen
					CG_DrawFatiguePointBar(cent, ch_x, ch_y + 10, -FATIGUE_BAR_WIDTH + 2, Fatigue_BAR_HEIGHT);
				}
				else
				{
					//on screen
					CG_DrawFatiguePointBar(cent, ch_x, ch_y, -FATIGUE_BAR_WIDTH + 2, Fatigue_BAR_HEIGHT);
				}
			}
		}
	}
}

void CG_AddFatiguePointBarEnt(const int entNum)
{
	if (cg_numFatigueBarEnts >= MAX_FATIGUEPOINT_BAR_ENTS)
	{
		return;
	}

	if (DistanceSquared(cg_entities[entNum].lerpOrigin, g_entities[0].client->renderInfo.eyePoint) < Fatigue_BAR_RANGE
		* Fatigue_BAR_RANGE)
	{
		cg_FatiguePointBarEnts[cg_numFatigueBarEnts++] = entNum;
	}
}

void CG_ClearFatiguePointBarEnts()
{
	if (cg_numFatigueBarEnts)
	{
		cg_numFatigueBarEnts = 0;
		memset(&cg_FatiguePointBarEnts, 0, MAX_FATIGUEPOINT_BAR_ENTS);
	}
}

// saber fatigue bar end

static void CG_DrawForcePointBars()
{
	float ch_x = 0, ch_y = 0;
	vec3_t pos;
	for (int i = 0; i < cg_numBlockPointBarEnts; i++)
	{
		const centity_t* cent = &cg_entities[cg_BlockPointBarEnts[i]];
		if (cent && cent->gent)
		{
			VectorCopy(cent->lerpOrigin, pos);
			pos[2] += cent->gent->maxs[2] + BLOCKPOINT_BAR_HEIGHT + 12;
			if (CG_WorldCoordToScreenCoordFloat(pos, &ch_x, &ch_y))
			{
				if (NPC_IsOversized(cent->gent))
				{
					//on screen
					CG_DrawForcePointBar(cent, ch_x, ch_y + 10, BLOCKPOINT_BAR_WIDTH, BLOCKPOINT_BAR_HEIGHT);
				}
				else
				{
					//on screen
					CG_DrawForcePointBar(cent, ch_x, ch_y, BLOCKPOINT_BAR_WIDTH, BLOCKPOINT_BAR_HEIGHT);
				}
			}
		}
	}
}

void CG_ClearBlockPointBarEnts()
{
	if (cg_numBlockPointBarEnts)
	{
		cg_numBlockPointBarEnts = 0;
		memset(&cg_BlockPointBarEnts, 0, MAX_BLOCKPOINT_BAR_ENTS);
	}
}

constexpr auto MAX_HEALTH_BAR_ENTS = 32;
int cg_numHealthBarEnts = 0;
int cg_healthBarEnts[MAX_HEALTH_BAR_ENTS];
constexpr auto HEALTH_BAR_WIDTH = 50;
constexpr auto HEALTH_BAR_HEIGHT = 4;

static void CG_DrawHealthBars()
{
	float ch_x = 0, ch_y = 0;
	vec3_t pos;
	for (int i = 0; i < cg_numHealthBarEnts; i++)
	{
		const centity_t* cent = &cg_entities[cg_healthBarEnts[i]];
		if (cent && cent->gent)
		{
			VectorCopy(cent->lerpOrigin, pos);
			pos[2] += cent->gent->maxs[2] + HEALTH_BAR_HEIGHT + 8;

			if (CG_WorldCoordToScreenCoordFloat(pos, &ch_x, &ch_y))
			{
				if (NPC_IsOversized(cent->gent))
				{
					//on screen
					CG_DrawHealthBar(cent, ch_x, ch_y + 10, HEALTH_BAR_WIDTH, HEALTH_BAR_HEIGHT);
				}
				else
				{
					//on screen
					CG_DrawHealthBar(cent, ch_x, ch_y, HEALTH_BAR_WIDTH, HEALTH_BAR_HEIGHT);
				}
			}
		}
	}
}

constexpr auto HEALTHBARRANGE = 200;

void CG_AddHealthBarEnt(const int entNum)
{
	if (cg_numHealthBarEnts >= MAX_HEALTH_BAR_ENTS)
	{
		return;
	}

	if (DistanceSquared(cg_entities[entNum].lerpOrigin, g_entities[0].client->renderInfo.eyePoint) < HEALTHBARRANGE *
		HEALTHBARRANGE)
	{
		cg_healthBarEnts[cg_numHealthBarEnts++] = entNum;
	}
}

void CG_ClearHealthBarEnts()
{
	if (cg_numHealthBarEnts)
	{
		cg_numHealthBarEnts = 0;
		memset(&cg_healthBarEnts, 0, MAX_HEALTH_BAR_ENTS);
	}
}

/*
================================================================================

CROSSHAIR

================================================================================
*/

/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(vec3_t world_point)
{
	float w, h;
	qboolean corona = qfalse;
	vec4_t ecolor{};
	float x, y;
	float		chX, chY;

	if (!cg_drawCrosshair.integer)
	{
		return;
	}

	if (in_camera)
	{
		//no crosshair while in cutscenes
		return;
	}

	if (cg.zoomMode > 0 && cg.zoomMode < 3)
	{
		//not while scoped
		return;
	}

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return;
	}

	//set color based on what kind of ent is under crosshair
	if (g_crosshairEntNum >= ENTITYNUM_WORLD)
	{
		ecolor[0] = ecolor[1] = ecolor[2] = 1.0f;
	}
	else if (cg_forceCrosshair && cg_crosshairForceHint.integer)
	{
		ecolor[0] = 0.2f;
		ecolor[1] = 0.5f;
		ecolor[2] = 1.0f;

		corona = qtrue;
	}
	else if (cg_crosshairIdentifyTarget.integer)
	{
		const gentity_t* crossEnt = &g_entities[g_crosshairEntNum];

		if (crossEnt->client)
		{
			if (crossEnt->client->ps.powerups[PW_CLOAKED])
			{
				//cloaked don't show up
				ecolor[0] = 1.0f; //R
				ecolor[1] = 1.0f; //G
				ecolor[2] = 1.0f; //B
			}
			else if (g_entities[0].client && (g_entities[0].client->playerTeam == TEAM_FREE || g_entities[0].client->
				playerTeam == TEAM_SOLO || g_entities[0].client->playerTeam == TEAM_PROJECTION) ||
				crossEnt->client->playerTeam == TEAM_FREE ||
				crossEnt->client->playerTeam == TEAM_PROJECTION ||
				crossEnt->client->playerTeam == TEAM_SOLO)
			{
				//evil player or target: everyone is red
				ecolor[0] = 1.0f; //R
				ecolor[1] = 0.1f; //G
				ecolor[2] = 0.1f; //B
			}
			else if (g_entities[0].client && g_entities[0].client->playerTeam == crossEnt->client->playerTeam)
			{
				//Allies are green unless we are on Free or Solo
				ecolor[0] = 0.0f; //R
				ecolor[1] = 1.0f; //G
				ecolor[2] = 0.0f; //B
			}
			else if (crossEnt->client->playerTeam == TEAM_NEUTRAL || g_entities[0].client && g_entities[0].client->
				playerTeam == TEAM_NEUTRAL)
			{
				//Neutrals are yellow unless they are on our team or we are on Free or Solo. If we are Neutral everyone is yellow.
				ecolor[0] = 1.0f; //R
				ecolor[1] = 1.0f; //G
				ecolor[2] = 0.1f; //B
			}
			else
			{
				//Enemies are red
				ecolor[0] = 1.0f; //R
				ecolor[1] = 0.1f; //G
				ecolor[2] = 0.1f; //B
			}
		}
		else if (crossEnt->s.weapon == WP_TURRET && crossEnt->svFlags & SVF_NONNPC_ENEMY)
		{
			// a turret
			if (crossEnt->noDamageTeam == TEAM_PLAYER)
			{
				// mine are green
				ecolor[0] = 0.0; //R
				ecolor[1] = 1.0; //G
				ecolor[2] = 0.0; //B
			}
			else
			{
				// hostile ones are red
				ecolor[0] = 1.0; //R
				ecolor[1] = 0.0; //G
				ecolor[2] = 0.0; //B
			}
		}
		else if (crossEnt->s.weapon == WP_TRIP_MINE)
		{
			// tripmines are red
			ecolor[0] = 1.0; //R
			ecolor[1] = 0.0; //G
			ecolor[2] = 0.0; //B
		}
		else if (crossEnt->flags & FL_RED_CROSSHAIR)
		{
			//special case flagged to turn crosshair red
			ecolor[0] = 1.0; //R
			ecolor[1] = 0.0; //G
			ecolor[2] = 0.0; //B
		}
		else
		{
			VectorCopy(crossEnt->startRGBA, ecolor);

			if (!ecolor[0] && !ecolor[1] && !ecolor[2])
			{
				// We don't want a black crosshair, so use white since it will show up better
				ecolor[0] = 1.0f; //R
				ecolor[1] = 1.0f; //G
				ecolor[2] = 1.0f; //B
			}
		}
	}
	else // cg_crosshairIdentifyTarget is not on, so make it white
	{
		ecolor[0] = ecolor[1] = ecolor[2] = 1.0f;
	}

	ecolor[3] = 1.0;
	cgi_R_SetColor(ecolor);

	if (cg.forceCrosshairStartTime)
	{
		// both of these calcs will fade the corona in one direction
		if (cg.forceCrosshairEndTime)
		{
			ecolor[3] = (cg.time - cg.forceCrosshairEndTime) / 500.0f;
		}
		else
		{
			ecolor[3] = (cg.time - cg.forceCrosshairStartTime) / 300.0f;
		}

		// clamp
		if (ecolor[3] < 0)
		{
			ecolor[3] = 0;
		}
		else if (ecolor[3] > 1.0f)
		{
			ecolor[3] = 1.0f;
		}

		if (!cg.forceCrosshairEndTime)
		{
			// but for the other direction, we'll need to reverse it
			ecolor[3] = 1.0f - ecolor[3];
		}
	}

	if (corona) // we are pointing at a crosshair item
	{
		if (!cg.forceCrosshairStartTime)
		{
			// must have just happened because we are not fading in yet...start it now
			cg.forceCrosshairStartTime = cg.time;
			cg.forceCrosshairEndTime = 0;
		}
		if (cg.forceCrosshairEndTime)
		{
			// must have just gone over a force thing again...and we were in the process of fading out.  Set fade in level to the level where the fade left off
			cg.forceCrosshairStartTime = cg.time - (1.0f - ecolor[3]) * 300.0f;
			cg.forceCrosshairEndTime = 0;
		}
	}
	else // not pointing at a crosshair item
	{
		if (cg.forceCrosshairStartTime && !cg.forceCrosshairEndTime) // were currently fading in
		{
			// must fade back out, but we will have to set the fadeout time to be equal to the current level of faded-in-edness
			cg.forceCrosshairEndTime = cg.time - ecolor[3] * 500.0f;
		}
		if (cg.forceCrosshairEndTime && cg.time - cg.forceCrosshairEndTime > 500.0f)
			// not pointing at anything and fade out is totally done
		{
			// reset everything
			cg.forceCrosshairStartTime = 0;
			cg.forceCrosshairEndTime = 0;
		}
	}

	if ((cg.snap->ps.weapon == WP_DUAL_PISTOL ||
		cg.snap->ps.weapon == WP_DUAL_CLONEPISTOL ||
		cg.snap->ps.weapon == WP_BLASTER_PISTOL ||
		cg.snap->ps.weapon == WP_REY ||
		cg.snap->ps.weapon == WP_JANGO ||
		cg.snap->ps.weapon == WP_CLONEPISTOL ||
		cg.snap->ps.weapon == WP_REBELBLASTER) &&
		(cg_entities[0].currentState.eFlags & EF2_JANGO_DUALS || cg_entities[0].currentState.eFlags & EF2_DUAL_CLONE_PISTOLS || cg_entities[0].currentState.eFlags & EF2_DUAL_PISTOLS || cg_entities[0].currentState.eFlags & EF2_DUAL_CALONORD) &&
		cg_trueguns.integer)
	{
		w = h = cg_crosshairDualSize.value;
	}
	else if (cg.snap->ps.weapon == WP_DROIDEKA)
	{
		w = h = cg_crosshairDualSize.value;
	}
	else
	{
		w = h = cg_crosshairSize.value;
	}

	// pulse the size of the crosshair when picking up items
	float f = cg.time - cg.itemPickupBlendTime;

	if (f > 0 && f < ITEM_BLOB_TIME)
	{
		f /= ITEM_BLOB_TIME;
		w *= 1 + f;
		h *= 1 + f;
	}

	if (world_point && VectorLength(world_point))
	{
		if (!CG_WorldCoordToScreenCoordFloat(world_point, &x, &y))
		{
			//off screen, don't draw it
			cgi_R_SetColor(nullptr);
			return;
		}
		x -= 320; //????
		y -= 240; //????
	}
	else
	{
		x = cg_crosshairX.integer;
		y = cg_crosshairY.integer;
	}

	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		if (!Q_stricmp("misc_panel_turret", g_entities[cg.snap->ps.viewEntity].classname))
		{
			// draws a custom crosshair that is twice as large as normal
			cgi_R_DrawStretchPic(x + cg.refdef.x + 320 - w, y + cg.refdef.y + 240 - h, w * 2, h * 2, 0, 0, 1, 1,
				cgs.media.turretCrossHairShader);
		}
	}
	else if ((cg.snap->ps.weapon == WP_DUAL_PISTOL ||
		cg.snap->ps.weapon == WP_DUAL_CLONEPISTOL ||
		cg.snap->ps.weapon == WP_BLASTER_PISTOL ||
		cg.snap->ps.weapon == WP_REY ||
		cg.snap->ps.weapon == WP_JANGO ||
		cg.snap->ps.weapon == WP_CLONEPISTOL ||
		cg.snap->ps.weapon == WP_REBELBLASTER) &&
		(cg_entities[0].currentState.eFlags & EF2_JANGO_DUALS || cg_entities[0].currentState.eFlags & EF2_DUAL_CLONE_PISTOLS || cg_entities[0].currentState.eFlags & EF2_DUAL_PISTOLS || cg_entities[0].currentState.eFlags & EF2_DUAL_CALONORD) &&
		cg_trueguns.integer)
	{
		cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0, 1, 1,
			cgs.media.crosshairShader[4]);
	}
	else if (cg.snap->ps.weapon == WP_DROIDEKA)
	{
		cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0, 1, 1,
			cgs.media.crosshairShader[4]);
	}
	else
	{
		qhandle_t hShader;
		if (cg_weaponcrosshairs.integer)
		{
			if (cg.snap->ps.weapon == WP_SABER ||
				cg.snap->ps.weapon == WP_MELEE)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[1]);
			}
			else if (cg.snap->ps.weapon == WP_REPEATER ||
				cg.snap->ps.weapon == WP_BLASTER ||
				cg.snap->ps.weapon == WP_BATTLEDROID ||
				cg.snap->ps.weapon == WP_THEFIRSTORDER ||
				cg.snap->ps.weapon == WP_CLONECARBINE ||
				cg.snap->ps.weapon == WP_CLONERIFLE ||
				cg.snap->ps.weapon == WP_CLONECOMMANDO ||
				cg.snap->ps.weapon == WP_Z6_ROTARY_CANNON ||
				cg.snap->ps.weapon == WP_BOBA ||
				cg.snap->ps.weapon == WP_REBELRIFLE)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[2]);
			}
			else if (cg.snap->ps.weapon == WP_FLECHETTE ||
				cg.snap->ps.weapon == WP_CONCUSSION ||
				cg.snap->ps.weapon == WP_BOWCASTER ||
				cg.snap->ps.weapon == WP_DEMP2)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[3]);
			}
			else if ((cg.snap->ps.weapon == WP_DUAL_PISTOL ||
				cg.snap->ps.weapon == WP_DUAL_CLONEPISTOL ||
				cg.snap->ps.weapon == WP_BLASTER_PISTOL ||
				cg.snap->ps.weapon == WP_REY ||
				cg.snap->ps.weapon == WP_JANGO ||
				cg.snap->ps.weapon == WP_CLONEPISTOL ||
				cg.snap->ps.weapon == WP_REBELBLASTER) &&
				(cg_entities[0].currentState.eFlags & EF2_JANGO_DUALS || cg_entities[0].currentState.eFlags & EF2_DUAL_CLONE_PISTOLS || cg_entities[0].currentState.eFlags & EF2_DUAL_PISTOLS || cg_entities[0].currentState.eFlags & EF2_DUAL_CALONORD) &&
				cg_trueguns.integer)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[4]);
			}
			else if (cg.snap->ps.weapon == WP_DROIDEKA)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[4]);
			}
			else if (cg.snap->ps.weapon == WP_THERMAL ||
				cg.snap->ps.weapon == WP_DET_PACK ||
				cg.snap->ps.weapon == WP_TRIP_MINE)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[5]);
			}
			else if (cg.snap->ps.weapon == WP_DISRUPTOR ||
				cg.snap->ps.weapon == WP_TUSKEN_RIFLE)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[6]);
			}
			else if (cg.snap->ps.weapon == WP_BLASTER_PISTOL ||
				cg.snap->ps.weapon == WP_BRYAR_PISTOL ||
				cg.snap->ps.weapon == WP_REY ||
				cg.snap->ps.weapon == WP_CLONEPISTOL ||
				cg.snap->ps.weapon == WP_REBELBLASTER ||
				cg.snap->ps.weapon == WP_JANGO ||
				cg.snap->ps.weapon == WP_STUN_BATON ||
				cg.snap->ps.weapon == WP_SBD_BLASTER)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[7]);
			}
			else if (cg.snap->ps.weapon == WP_ROCKET_LAUNCHER)
			{
				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, cgs.media.crosshairShader[8]);
			}
			else
			{
				hShader = cgs.media.crosshairShader[cg_drawCrosshair.integer % NUM_CROSSHAIRS];

				cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0,
					1, 1, hShader);
			}
		}
		else
		{
			hShader = cgs.media.crosshairShader[cg_drawCrosshair.integer % NUM_CROSSHAIRS];

			cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (640 - w), y + cg.refdef.y + 0.5 * (480 - h), w, h, 0, 0, 1, 1,
				hShader);
		}
	}

	if (cg.forceCrosshairStartTime && cg_crosshairForceHint.integer) // drawing extra bits
	{
		ecolor[0] = ecolor[1] = ecolor[2] = (1 - ecolor[3]) * (sin(cg.time * 0.001f) * 0.08f + 0.35f);
		// don't draw full color
		ecolor[3] = 1.0f;

		cgi_R_SetColor(ecolor);

		w *= 2.0f;
		h *= 2.0f;

		cgi_R_DrawStretchPic(x + cg.refdef.x + 0.5f * (640 - w), y + cg.refdef.y + 0.5f * (480 - h),
			w, h,
			0, 0, 1, 1,
			cgs.media.forceCoronaShader);
	}

	chX = x + cg.refdef.x + 0.5 * (640 - w);
	chY = y + cg.refdef.y + 0.5 * (480 - h);

	if (cg.predictedPlayerState.hackingTime)
	{ //hacking something
		CG_DrawHaqrBar(chX, chY - 50, w, h);
	}

	cgi_R_SetColor(nullptr);
}

/*
qboolean CG_WorldCoordToScreenCoord(vec3_t worldCoord, int *x, int *y)

  Take any world coord and convert it to a 2D virtual 640x480 screen coord
*/
qboolean CG_WorldCoordToScreenCoordFloat(vec3_t world_coord, float* x, float* y)
{
	vec3_t trans;

	const float px = tan(cg.refdef.fov_x * (M_PI / 360));
	const float py = tan(cg.refdef.fov_y * (M_PI / 360));

	VectorSubtract(world_coord, cg.refdef.vieworg, trans);

	constexpr float xc = 640 / 2.0;
	constexpr float yc = 480 / 2.0;

	// z = how far is the object in our forward direction
	const float z = DotProduct(trans, cg.refdef.viewaxis[0]);
	if (z <= 0.001)
		return qfalse;

	*x = xc - DotProduct(trans, cg.refdef.viewaxis[1]) * xc / (z * px);
	*y = yc - DotProduct(trans, cg.refdef.viewaxis[2]) * yc / (z * py);

	return qtrue;
}

qboolean CG_WorldCoordToScreenCoord(vec3_t world_coord, int* x, int* y)
{
	float x_f, y_f;

	if (CG_WorldCoordToScreenCoordFloat(world_coord, &x_f, &y_f))
	{
		*x = static_cast<int>(x_f);
		*y = static_cast<int>(y_f);
		return qtrue;
	}

	return qfalse;
}

qboolean CG_InFighter()
{
	if (cg.predictedPlayerState.m_iVehicleNum)
	{
		//I'm in a vehicle
		const centity_t* veh_cent = &cg_entities[cg.predictedPlayerState.m_iVehicleNum];

		if (veh_cent
			&& veh_cent->m_pVehicle
			&& veh_cent->m_pVehicle->m_pVehicleInfo
			&& veh_cent->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
		{
			//I'm in a fighter
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean CG_InATST()
{
	if (cg.predictedPlayerState.m_iVehicleNum)
	{
		//I'm in a vehicle
		const centity_t* veh_cent = &cg_entities[cg.predictedPlayerState.m_iVehicleNum];
		if (veh_cent
			&& veh_cent->m_pVehicle
			&& veh_cent->m_pVehicle->m_pVehicleInfo
			&& veh_cent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
		{
			//I'm in an atst
			return qtrue;
		}
	}
	return qfalse;
}

// I'm keeping the rocket tracking code separate for now since I may want to do different logic...but it still uses trace info from scanCrosshairEnt
//-----------------------------------------
static void CG_ScanForRocketLock()
//-----------------------------------------
{
	static qboolean temp_lock = qfalse;
	// this will break if anything else uses this locking code ( other than the player )

	const gentity_t* traceEnt = &g_entities[g_crosshairEntNum];

	if (!traceEnt || g_crosshairEntNum <= 0 || g_crosshairEntNum >= ENTITYNUM_WORLD || !traceEnt->client && traceEnt
		->s
		.weapon != WP_TURRET || !traceEnt->health
		|| traceEnt && traceEnt->client && traceEnt->client->ps.powerups[PW_CLOAKED])
	{
		// see how much locking we have
		const int dif = (cg.time - g_rocketLockTime) / (1200.0f / 8.0f);

		// 8 is full locking....also if we just traced onto the world,
		//	give them 1/2 second of slop before dropping the lock
		if (dif < 8 && g_rocketSlackTime + 500 < cg.time)
		{
			// didn't have a full lock and not in grace period, so drop the lock
			g_rocketLockTime = 0;
			g_rocketSlackTime = 0;
			temp_lock = qfalse;
		}

		if (g_rocketSlackTime + 500 >= cg.time && g_rocketLockEntNum < ENTITYNUM_WORLD)
		{
			// were locked onto an ent, aren't right now.....but still within the slop grace period
			//	keep the current lock amount
			g_rocketLockTime += cg.frametime;
		}

		if (!temp_lock && g_rocketLockEntNum < ENTITYNUM_WORLD && dif >= 8)
		{
			temp_lock = qtrue;

			if (g_rocketLockTime + 1200 < cg.time)
			{
				g_rocketLockTime = cg.time - 1200; // doh, hacking the time so the targetting still gets drawn full
			}
		}

		// keep locking to this thing for one second after it gets out of view
		if (g_rocketLockTime + 2000.0f < cg.time)
			// since time was hacked above, I'm compensating so that 2000ms is really only 1000ms
		{
			// too bad, you had your chance
			g_rocketLockEntNum = ENTITYNUM_NONE;
			g_rocketSlackTime = 0;
			g_rocketLockTime = 0;
		}
	}
	else
	{
		temp_lock = qfalse;

		if (g_rocketLockEntNum >= ENTITYNUM_WORLD)
		{
			if (g_rocketSlackTime + 500 < cg.time)
			{
				// we just locked onto something, start the lock at the current time
				g_rocketLockEntNum = g_crosshairEntNum;
				g_rocketLockTime = cg.time;
				g_rocketSlackTime = cg.time;
			}
		}
		else
		{
			if (g_rocketLockEntNum != g_crosshairEntNum)
			{
				g_rocketLockTime = cg.time;
			}

			// may as well always set this when we can potentially lock to something
			g_rocketSlackTime = cg.time;
			g_rocketLockEntNum = g_crosshairEntNum;
		}
	}
}

/*
=================
CG_ScanForCrosshairEntity
=================
*/
extern float forcePushPullRadius[];

static void CG_ScanForCrosshairEntity(const qboolean scan_all)
{
	trace_t trace;
	const gentity_t* traceEnt = nullptr;
	vec3_t start, end;
	int ignoreEnt = cg.snap->ps.client_num;
	const Vehicle_t* p_veh;

	//FIXME: debounce this to about 10fps?

	cg_forceCrosshair = qfalse;
	if (cg_entities[0].gent && cg_entities[0].gent->client)
		// <-Mike said it should always do this   //if (cg_crosshairForceHint.integer &&
	{
		//try to check for force-affectable stuff first
		vec3_t d_f, d_rt, d_up;

		// If you're riding a vehicle and not being drawn.
		if ((p_veh = G_IsRidingVehicle(cg_entities[0].gent)) != nullptr && cg_entities[0].currentState.eFlags &
			EF_NODRAW)
		{
			VectorCopy(cg_entities[p_veh->m_pParentEntity->s.number].lerpOrigin, start);
			AngleVectors(cg_entities[p_veh->m_pParentEntity->s.number].lerpAngles, d_f, d_rt, d_up);
		}
		else
		{
			VectorCopy(g_entities[0].client->renderInfo.eyePoint, start);
			AngleVectors(cg_entities[0].lerpAngles, d_f, d_rt, d_up);
		}

		VectorMA(start, 2048, d_f, end); //4028 is max for mind trick

		//YES!  This is very very bad... but it works!  James made me do it.  Really, he did.  Blame James.
		gi.trace(&trace, start, vec3_origin, vec3_origin, end,
			ignoreEnt, MASK_OPAQUE | CONTENTS_SHOTCLIP | CONTENTS_BODY | CONTENTS_ITEM | CONTENTS_TERRAIN,
			G2_NOCOLLIDE, 10);
		// ); took out CONTENTS_SOLID| so you can target people through glass.... took out CONTENTS_CORPSE so disintegrated guys aren't shown, could just remove their body earlier too...

		if (trace.entityNum < ENTITYNUM_WORLD)
		{
			//hit something
			traceEnt = &g_entities[trace.entityNum];
			if (traceEnt)
			{
				// Check for mind trickable-guys
				if (traceEnt->client)
				{
					//is a client
					if (cg_entities[0].gent->client->ps.forcePowerLevel[FP_TELEPATHY] && traceEnt->health > 0 &&
						VALIDSTRING(traceEnt->behaviorSet[BSET_MINDTRICK]))
					{
						//I have the ability to mind-trick and he is alive and he has a mind trick script
						//NOTE: no need to check range since it's always 2048
						cg_forceCrosshair = qtrue;
					}
				}
				// No?  Check for force-push/pullable doors and func_statics
				else if (traceEnt->s.eType == ET_MOVER)
				{
					//hit a mover
					if (!Q_stricmp("func_door", traceEnt->classname))
					{
						//it's a func_door
						if (traceEnt->spawnflags & 2/*MOVER_FORCE_ACTIVATE*/)
						{
							//it's force-usable
							if (cg_entities[0].gent->client->ps.forcePowerLevel[FP_PULL] || cg_entities[0].gent->client
								->ps.forcePowerLevel[FP_PUSH])
							{
								//player has push or pull
								float max_range;
								if (cg_entities[0].gent->client->ps.forcePowerLevel[FP_PULL] > cg_entities[0].gent->
									client->ps.forcePowerLevel[FP_PUSH])
								{
									//use the better range
									max_range = forcePushPullRadius[cg_entities[0].gent->client->ps.forcePowerLevel[
										FP_PULL]];
								}
								else
								{
									//use the better range
									max_range = forcePushPullRadius[cg_entities[0].gent->client->ps.forcePowerLevel[
										FP_PUSH]];
								}
								if (max_range >= trace.fraction * 2048)
								{
									//actually close enough to use one of our force powers on it
									cg_forceCrosshair = qtrue;
								}
							}
						}
					}
					else if (!Q_stricmp("func_static", traceEnt->classname))
					{
						//it's a func_static
						if (traceEnt->spawnflags & 1/*F_PUSH*/ && traceEnt->spawnflags & 2/*F_PULL*/)
						{
							//push or pullable
							float max_range;
							if (cg_entities[0].gent->client->ps.forcePowerLevel[FP_PULL] > cg_entities[0].gent->client->
								ps.forcePowerLevel[FP_PUSH])
							{
								//use the better range
								max_range = forcePushPullRadius[cg_entities[0].gent->client->ps.forcePowerLevel[
									FP_PULL]];
							}
							else
							{
								//use the better range
								max_range = forcePushPullRadius[cg_entities[0].gent->client->ps.forcePowerLevel[
									FP_PUSH]];
							}
							if (max_range >= trace.fraction * 2048)
							{
								//actually close enough to use one of our force powers on it
								cg_forceCrosshair = qtrue;
							}
						}
						else if (traceEnt->spawnflags & 1/*F_PUSH*/)
						{
							//pushable only
							if (forcePushPullRadius[cg_entities[0].gent->client->ps.forcePowerLevel[FP_PUSH]] >= trace.
								fraction * 2048)
							{
								//actually close enough to use force push on it
								cg_forceCrosshair = qtrue;
							}
						}
						else if (traceEnt->spawnflags & 2/*F_PULL*/)
						{
							//pullable only
							if (forcePushPullRadius[cg_entities[0].gent->client->ps.forcePowerLevel[FP_PULL]] >= trace.
								fraction * 2048)
							{
								//actually close enough to use force pull on it
								cg_forceCrosshair = qtrue;
							}
						}
					}
				}
			}
		}
	}
	if (!cg_forceCrosshair)
	{
		if (cg_dynamicCrosshair.integer)
		{
			//100% accurate
			vec3_t d_f, d_rt, d_up;
			// If you're riding a vehicle and not being drawn.
			if ((p_veh = G_IsRidingVehicle(cg_entities[0].gent)) != nullptr && cg_entities[0].currentState.eFlags &
				EF_NODRAW)
			{
				VectorCopy(cg_entities[p_veh->m_pParentEntity->s.number].lerpOrigin, start);
				AngleVectors(cg_entities[p_veh->m_pParentEntity->s.number].lerpAngles, d_f, d_rt, d_up);
			}
			else if (cg.snap->ps.weapon == WP_NONE || cg.snap->ps.weapon == WP_SABER || cg.snap->ps.weapon ==
				WP_STUN_BATON)
			{
				if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
				{
					//in camera ent view
					ignoreEnt = cg.snap->ps.viewEntity;
					if (g_entities[cg.snap->ps.viewEntity].client)
					{
						VectorCopy(g_entities[cg.snap->ps.viewEntity].client->renderInfo.eyePoint, start);
					}
					else
					{
						VectorCopy(cg_entities[cg.snap->ps.viewEntity].lerpOrigin, start);
					}
					AngleVectors(cg_entities[cg.snap->ps.viewEntity].lerpAngles, d_f, d_rt, d_up);
				}
				else
				{
					VectorCopy(g_entities[0].client->renderInfo.eyePoint, start);
					AngleVectors(cg_entities[0].lerpAngles, d_f, d_rt, d_up);
				}
			}
			else
			{
				AngleVectors(cg_entities[0].lerpAngles, d_f, d_rt, d_up);
				CalcMuzzlePoint(&g_entities[0], d_f, start, 0);
			}
			//VectorCopy( g_entities[0].client->renderInfo.muzzlePoint, start );
			//FIXME: increase this?  Increase when zoom in?
			VectorMA(start, 4096, d_f, end); //was 8192
		}
		else
		{
			//old way
			VectorCopy(cg.refdef.vieworg, start);
			//FIXME: increase this?  Increase when zoom in?
			VectorMA(start, 131072, cg.refdef.viewaxis[0], end); //was 8192
		}
		//YES!  This is very very bad... but it works!  James made me do it.  Really, he did.  Blame James.
		gi.trace(&trace, start, vec3_origin, vec3_origin, end,
			ignoreEnt, MASK_OPAQUE | CONTENTS_TERRAIN | CONTENTS_SHOTCLIP | CONTENTS_BODY | CONTENTS_ITEM,
			G2_NOCOLLIDE, 10);
		// ); took out CONTENTS_SOLID| so you can target people through glass.... took out CONTENTS_CORPSE so disintegrated guys aren't shown, could just remove their body earlier too...

		/*
		CG_Trace( &trace, start, vec3_origin, vec3_origin, end,
			cg.snap->ps.client_num, MASK_PLAYERSOLID|CONTENTS_CORPSE|CONTENTS_ITEM );
		*/
		//FIXME: pick up corpses
		if (trace.startsolid || trace.allsolid)
		{
			// trace should not be allowed to pick up anything if it started solid.  I tried actually moving the trace start back, which also worked,
			//	but the dynamic cursor drawing caused it to render around the clip of the gun when I pushed the blaster all the way into a wall.
			//	It looked quite horrible...but, if this is bad for some reason that I don't know
			trace.entityNum = ENTITYNUM_NONE;
		}

		traceEnt = &g_entities[trace.entityNum];
	}

	// if the object is "dead", don't show it
	/*	if ( cg.crosshairclient_num && g_entities[cg.crosshairclient_num].health <= 0 )
		{
			cg.crosshairclient_num = 0;
			return;
		}
	*/
	//draw crosshair at endpoint
	CG_DrawCrosshair(trace.endpos);

	g_crosshairEntNum = trace.entityNum;
	g_crosshairEntDist = 4096 * trace.fraction;

	if (!traceEnt)
	{
		//not looking at anything
		g_crosshairSameEntTime = 0;
		g_crosshairEntTime = 0;
	}
	else
	{
		//looking at a valid ent
		//store the distance
		if (trace.entityNum != g_crosshairEntNum)
		{
			//new crosshair ent
			g_crosshairSameEntTime = 0;
		}
		else if (g_crosshairEntDist < 256)
		{
			//close enough to start counting how long you've been looking
			g_crosshairSameEntTime += cg.frametime;
		}
		//remember the last time you looked at the person
		g_crosshairEntTime = cg.time;
	}

	if (!traceEnt)
	{
		if (traceEnt && scan_all)
		{
		}
		else
		{
			return;
		}
	}

	// if the player is in fog, don't show it
	const int content = cgi_CM_PointContents(trace.endpos, 0);
	if (content & CONTENTS_FOG)
	{
		return;
	}

	// if the player is cloaked, don't show it
	if (cg_entities[trace.entityNum].currentState.powerups & 1 << PW_CLOAKED)
	{
		return;
	}

	// update the fade timer
	if (cg.crosshairclient_num != trace.entityNum)
	{
		infoStringCount = 0;
	}

	cg.crosshairclient_num = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}

/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames()
{
	constexpr qboolean scan_all = qfalse;
	const centity_t* player = &cg_entities[0];

	if (cg_dynamicCrosshair.integer)
	{
		// still need to scan for dynamic crosshair
		CG_ScanForCrosshairEntity(scan_all);
		return;
	}

	if (!player->gent)
	{
		return;
	}

	if (!player->gent->client)
	{
		return;
	}

	// scan the known entities to see if the crosshair is sighted on one
	// This is currently being called by the rocket tracking code, so we don't necessarily want to do duplicate traces :)
	CG_ScanForCrosshairEntity(scan_all);
}

static void CG_DrawCrosshairItem()
{
	constexpr qboolean scan_all = qfalse;

	if (in_camera)
	{
		return;
	}

	if (!cg_DrawCrosshairItem.integer)
	{
		return;
	}

	CG_ScanForCrosshairEntity(scan_all);

	if (cg_entities[cg.crosshairclient_num].currentState.eType == ET_ITEM && cg.snap->ps.weapon != WP_DROIDEKA)
	{
		if (cg_SerenityJediEngineHudMode.integer == 4)
		{
			CG_DrawPic(20, 285, 26, 26, cgs.media.useableHint);
		}
		else if (cg_SerenityJediEngineHudMode.integer == 5)
		{
			CG_DrawPic(157, 429, 26, 26, cgs.media.useableHint);
		}
		else
		{
			CG_DrawPic(50, 285, 32, 32, cgs.media.useableHint);
		}
		return;
	}

	cgi_R_SetColor(nullptr);
}

//--------------------------------------------------------------
static void CG_DrawActivePowers()
//--------------------------------------------------------------
{
	constexpr int icon_size = 20;
	constexpr int icon_size2 = 10;
	constexpr int icon_size3 = 60;
	constexpr int startx = icon_size2 * 2 + 16;
	constexpr int starty = SCREEN_HEIGHT - icon_size3 * 2;

	constexpr int endx = icon_size;
	constexpr int endy = icon_size;

	if (cg.zoomMode)
	{
		//don't display over zoom mask
		return;
	}

	//additionally, draw an icon force force rage recovery
	if (cg.snap->ps.forceRageRecoveryTime > cg.time)
	{
		CG_DrawPic(startx, starty, endx, endy, cgs.media.rageRecShader);
	}
}

//--------------------------------------------------------------
static void CG_DrawRocketLocking(const int lock_ent_num)
//--------------------------------------------------------------
{
	const gentity_t* gent = &g_entities[lock_ent_num];

	if (!gent)
	{
		return;
	}

	int cx, cy;
	vec3_t org;

	VectorCopy(gent->currentOrigin, org);
	org[2] += (gent->mins[2] + gent->maxs[2]) * 0.5f;

	if (CG_WorldCoordToScreenCoord(org, &cx, &cy))
	{
		static int old_dif = 0;
		// we care about distance from enemy to eye, so this is good enough
		float sz = Distance(gent->currentOrigin, cg.refdef.vieworg) / 1024.0f;

		if (cg.zoomMode > 0)
		{
			if (cg.overrides.active & CG_OVERRIDE_FOV)
			{
				sz -= (cg.overrides.fov - cg_zoomFov) / 80.0f;
			}
			else if (!cg.renderingThirdPerson && (cg_trueguns.integer || cg.snap->ps.weapon == WP_SABER
				|| cg.snap->ps.weapon == WP_MELEE) && cg_truefov.value)
			{
				sz -= (cg_truefov.value - cg_zoomFov) / 80.0f;
			}
			else
			{
				sz -= (cg_fov.value - cg_zoomFov) / 80.0f;
			}
		}

		if (sz > 1.0f)
		{
			sz = 1.0f;
		}
		else if (sz < 0.0f)
		{
			sz = 0.0f;
		}

		sz = (1.0f - sz) * (1.0f - sz) * 32 + 6;

		vec4_t color = { 0.0f, 0.0f, 0.0f, 0.0f };

		cy += sz * 0.5f;

		// well now, take our current lock time and divide that by 8 wedge slices to get the current lock amount
		int dif = (cg.time - g_rocketLockTime) / (1200.0f / 8.0f);

		if (dif < 0)
		{
			old_dif = 0;
			return;
		}
		if (dif > 8)
		{
			dif = 8;
		}

		// do sounds
		if (old_dif != dif)
		{
			if (dif == 8)
			{
				cgi_S_StartSound(org, 0, CHAN_AUTO, cgi_S_RegisterSound("sound/weapons/rocket/lock.wav"));
			}
			else
			{
				cgi_S_StartSound(org, 0, CHAN_AUTO, cgi_S_RegisterSound("sound/weapons/rocket/tick.wav"));
			}
		}

		old_dif = dif;

		for (int i = 0; i < dif; i++)
		{
			color[0] = 1.0f;
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = 0.1f * i + 0.2f;

			cgi_R_SetColor(color);

			// our slices are offset by about 45 degrees.
			CG_DrawRotatePic(cx - sz, cy - sz, sz, sz, i * 45.0f, cgi_R_RegisterShaderNoMip("gfx/2d/wedge"),
				cgs.widthRatioCoef);
		}

		// we are locked and loaded baby
		if (dif == 8)
		{
			color[0] = color[1] = color[2] = sin(cg.time * 0.05f) * 0.5f + 0.5f;
			color[3] = 1.0f; // this art is additive, so the alpha value does nothing

			cgi_R_SetColor(color);

			CG_DrawPic(cx - sz, cy - sz * 2, sz * 2, sz * 2, cgi_R_RegisterShaderNoMip("gfx/2d/lock"));
		}
	}
}

//------------------------------------
static void CG_RunRocketLocking()
//------------------------------------
{
	const centity_t* player = &cg_entities[0];

	// Only bother with this when the player is holding down the alt-fire button of the rocket launcher
	if (player->currentState.weapon == WP_ROCKET_LAUNCHER)
	{
		if (player->currentState.eFlags & EF_ALT_FIRING)
		{
			CG_ScanForRocketLock();

			if (g_rocketLockEntNum > 0 && g_rocketLockEntNum < ENTITYNUM_WORLD && g_rocketLockTime > 0)
			{
				CG_DrawRocketLocking(g_rocketLockEntNum);
			}
		}
		else
		{
			// disengage any residual locking
			g_rocketLockEntNum = ENTITYNUM_WORLD;
			g_rocketLockTime = 0;
		}
	}
}

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission()
{
	CG_DrawScoreboard();
}

/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot(const float y)
{
	const char* s = va("time:%i snap:%i cmd:%i", cg.snap->serverTime,
		cg.latestSnapshotNum, cgs.serverCommandSequence);

	const int w = cgi_R_Font_StrLenPixels(s, cgs.media.qhFontMedium, 1.0f);
	cgi_R_Font_DrawString(635 - w, y + 2, s, colorTable[CT_LTGOLD1], cgs.media.qhFontMedium, -1, 1.0f);

	return y + BIGCHAR_HEIGHT + 10;
}

/*
==================
CG_DrawFPS
==================
*/
constexpr auto FPS_FRAMES = 16;

static float CG_DrawFPS(const float y)
{
	static unsigned short previous_times[FPS_FRAMES];
	static int previous, lastupdate;
	constexpr int x_offset = 0;

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return y;
	}

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	const int t = cgi_Milliseconds();
	const unsigned short frame_time = t - previous;
	previous = t;
	if (t - lastupdate > 50) //don't sample faster than this
	{
		static unsigned short index;
		lastupdate = t;
		previous_times[index % FPS_FRAMES] = frame_time;
		index++;
	}
	// average multiple frames together to smooth changes out a bit
	int total = 0;
	for (const unsigned short previousTime : previous_times)
	{
		total += previousTime;
	}
	if (!total)
	{
		total = 1;
	}
	const int fps = 1000 * FPS_FRAMES / total;

	const char* s = va("%ifps", fps);
	const int w = cgi_R_Font_StrLenPixels(s, cgs.media.qhFontMedium, 0.5f);
	cgi_R_Font_DrawString(635 - x_offset - w, y + 2, s, colorTable[CT_LTGOLD1], cgs.media.qhFontMedium, -1, 0.5f);

	return y + BIGCHAR_HEIGHT + 10;
}

/*
=================
CG_DrawTimer
=================
*/
static float CG_DrawTimer(const float y)
{
	int seconds = cg.time / 1000;
	const int mins = seconds / 60;
	seconds -= mins * 60;
	const int tens = seconds / 10;
	seconds -= tens * 10;

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return y;
	}

	const char* s = va("%i:%i%i", mins, tens, seconds);

	const int w = cgi_R_Font_StrLenPixels(s, cgs.media.qhFontMedium, 1.0f);
	cgi_R_Font_DrawString(635 - w, y + 2, s, colorTable[CT_LTGOLD1], cgs.media.qhFontMedium, -1, 1.0f);

	return y + BIGCHAR_HEIGHT + 10;
}

/*
=====================
CG_DrawRadar
=====================
*/
float cg_radarRange = 2500.0f;

constexpr auto RADAR_RADIUS = 30;
#define RADAR_RADIUS_X			30 * cgs.widthRatioCoef
#define RADAR_X					SCREEN_WIDTH - 2 * RADAR_RADIUS_X
static int radarLockSoundDebounceTime = 0;
constexpr auto RADAR_MISSILE_RANGE = 3000.0f;
constexpr auto RADAR_ASTEROID_RANGE = 10000.0f;
constexpr auto RADAR_MIN_ASTEROID_SURF_WARN_DIST = 1200.0f;

static float cg_draw_radar(const float y)
{
	vec4_t color{};
	float arrow_w;
	float arrow_h;
	float arrow_base_scale;
	float z_scale;
	constexpr int x_offset = 0;

	if (!cg.snap)
	{
		return y;
	}

	// Make sure the radar should be showing
	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return y;
	}

	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		return y;
	}

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return y;
	}

	// Draw the radar background image
	color[0] = color[1] = color[2] = 1.0f;
	color[3] = 0.6f;
	cgi_R_SetColor(color);

	if (cg_SerenityJediEngineHudMode.integer == 4 || cg_SerenityJediEngineHudMode.integer == 5)
	{
		CG_DrawPic(RADAR_X + x_offset + 5, y + 5, RADAR_RADIUS_X * 2 - 11, RADAR_RADIUS * 2 - 11, cgs.media.radarScanShader);
		CG_DrawPic(RADAR_X + x_offset, y, RADAR_RADIUS_X * 2, RADAR_RADIUS * 2, cgs.media.radarShader);
	}
	else
	{
		CG_DrawPic(RADAR_X + x_offset + 5, y + 5, RADAR_RADIUS_X * 2 - 11, RADAR_RADIUS * 2 - 11, cgs.media.radarScanShader);
		CG_DrawPic(RADAR_X + x_offset, y, RADAR_RADIUS_X * 2, RADAR_RADIUS * 2, cgs.media.radarShader);
	}

	// Draw all of the radar entities.  Draw them backwards so players are drawn last
	for (int i = cg.radarEntityCount - 1; i >= 0; i--)
	{
		vec3_t dir_look;
		vec3_t dir_player;
		qboolean far_away = qfalse;

		const centity_t* cent = &cg_entities[cg.radarEntities[i]];

		// Get the distances first
		VectorSubtract(cg.predictedPlayerState.origin, cent->lerpOrigin, dir_player);
		dir_player[2] = 0;
		float distance;
		float actual_dist = distance = VectorNormalize(dir_player);

		if (distance > cg_radarRange * 0.8f)
		{
			if (cent->currentState.eFlags2 & EF2_RADAROBJECT) //still want to draw the direction
			{
				distance = cg_radarRange * 0.8f;
				far_away = qtrue;
			}
			else
			{
				continue;
			}
		}

		distance = distance / cg_radarRange;
		distance *= RADAR_RADIUS;

		AngleVectors(cg.predictedPlayerState.viewangles, dir_look, nullptr, nullptr);

		dir_look[2] = 0;
		const float angle_player = atan2(dir_player[0], dir_player[1]);
		VectorNormalize(dir_look);
		const float angle_look = atan2(dir_look[0], dir_look[1]);
		const float angle = angle_look - angle_player;

		const gentity_t* radar_ent = &g_entities[cent->currentState.number];

		switch (cent->currentState.eType)
		{
		default:
		{
			vec4_t rgba{};

			const float x = (float)RADAR_X + (float)RADAR_RADIUS_X + sin(angle) * distance * cgs.widthRatioCoef;
			const float ly = y + static_cast<float>(RADAR_RADIUS) + cos(angle) * distance;

			arrow_base_scale = 6.0f;
			qhandle_t shader;
			z_scale = 1.0f;

			if (!far_away)
			{
				//we want to scale the thing up/down based on the relative Z (up/down) positioning
				if (cent->lerpOrigin[2] > cg.predictedPlayerState.origin[2])
				{
					//higher, scale up (between 16 and 24)
					float dif = cent->lerpOrigin[2] - cg.predictedPlayerState.origin[2];

					//max out to 1.5x scale at 512 units above local player's height
					dif /= 1024.0f;
					if (dif > 0.5f)
					{
						dif = 0.5f;
					}
					z_scale += dif;
				}
				else if (cent->lerpOrigin[2] < cg.predictedPlayerState.origin[2])
				{
					//lower, scale down (between 16 and 8)
					float dif = cg.predictedPlayerState.origin[2] - cent->lerpOrigin[2];

					//half scale at 512 units below local player's height
					dif /= 1024.0f;
					if (dif > 0.5f)
					{
						dif = 0.5f;
					}
					z_scale -= dif;
				}
			}

			arrow_base_scale *= z_scale;

			rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0f;

			// generic enemy index specifies a shader to use for the radar entity.
			if (cent->currentState.genericenemyindex && cent->currentState.genericenemyindex < MAX_ICONS)
			{
				shader = cgs.media.radarIcons[cent->currentState.genericenemyindex];
			}
			else if (cent->currentState.radarIcon)
			{
				shader = cgs.media.radarIcons[cent->currentState.radarIcon];
			}
			else
			{
				shader = cgs.media.siegeItemShader;
			}

			if (shader)
			{
				// Pulse the alpha if time2 is set.  time2 gets set when the entity takes pain
				if (cent->currentState.time2 && cg.time - cent->currentState.time2 < 5000 ||
					cent->currentState.time2 == 0xFFFFFFFF)
				{
					if (cg.time / 200 & 1)
					{
						rgba[3] = 0.1f + 0.9f * static_cast<float>(cg.time % 200) / 200.0f;
					}
					else
					{
						rgba[3] = 1.0f - 0.9f * static_cast<float>(cg.time % 200) / 200.0f;
					}
				}

				cgi_R_SetColor(rgba);
				CG_DrawPic(x - 4 + x_offset, ly - 4, arrow_base_scale, arrow_base_scale, shader);
			}
		}
		break;

		case ET_PLAYER:
		{
			qhandle_t shader;
			vec4_t rgba;

			if (radar_ent->client->ps.stats[STAT_HEALTH] <= 0)
			{
				continue;
			}

			switch (radar_ent->client->playerTeam)
			{
			case TEAM_ENEMY:
				VectorCopy(colorTable[CT_RED], rgba);
				break;
			case TEAM_NEUTRAL:
				VectorCopy(colorTable[CT_YELLOW], rgba);
				break;
			case TEAM_PLAYER:
				VectorCopy(colorTable[CT_GREEN], rgba);
				break;
			case TEAM_SOLO:
				VectorCopy(colorTable[CT_MAGENTA], rgba);
				break;
			case TEAM_PROJECTION:
				VectorCopy(colorTable[CT_BLUE], rgba);
				break;
			case TEAM_FREE:
				VectorCopy(colorTable[CT_CYAN], rgba);
				break;
			default:
				VectorCopy(colorTable[CT_WHITE], rgba);
				break;
			}

			rgba[3] = 1.0f;

			arrow_base_scale = 8.0f;
			z_scale = 1.0f;

			cgi_R_SetColor(rgba);

			if (cent->currentState.radarIcon)
			{
				shader = cgs.media.radarIcons[cent->currentState.radarIcon];
			}
			else
			{
				shader = cgs.media.mAutomapPlayerIcon;
			}

			if (!far_away)
			{
				//we want to scale the thing up/down based on the relative Z (up/down) positioning
				if (cent->lerpOrigin[2] > cg.predictedPlayerState.origin[2])
				{
					//higher, scale up (between 16 and 32)
					float dif = cent->lerpOrigin[2] - cg.predictedPlayerState.origin[2];

					//max out to 2x scale at 1024 units above local player's height
					dif /= 1024.0f;
					if (dif > 1.0f)
					{
						dif = 1.0f;
					}
					z_scale += dif;
				}
				else if (cent->lerpOrigin[2] < cg.predictedPlayerState.origin[2])
				{
					//lower, scale down (between 16 and 8)
					float dif = cg.predictedPlayerState.origin[2] - cent->lerpOrigin[2];

					//half scale at 512 units below local player's height
					dif /= 1024.0f;
					if (dif > 0.5f)
					{
						dif = 0.5f;
					}
					z_scale -= dif;
				}
			}

			arrow_base_scale *= z_scale;

			arrow_w = arrow_base_scale * RADAR_RADIUS / 128;
			arrow_h = arrow_base_scale * RADAR_RADIUS / 128;

			if (cent->currentState.radarIcon)
			{
				arrow_w *= 2.0f;
				arrow_h *= 2.0f;
				CG_DrawPic(0, 0, 0, 0, cgs.media.whiteShader);
				CG_DrawRotatePic2(RADAR_X + RADAR_RADIUS_X + sin(angle) * distance * cgs.widthRatioCoef + x_offset,
					y + RADAR_RADIUS + cos(angle) * distance,
					arrow_w, arrow_h,
					0, shader, cgs.widthRatioCoef);
			}
			else
			{
				CG_DrawPic(0, 0, 0, 0, cgs.media.whiteShader);
				CG_DrawRotatePic2(RADAR_X + RADAR_RADIUS_X + sin(angle) * distance * cgs.widthRatioCoef + x_offset,
					y + RADAR_RADIUS + cos(angle) * distance,
					arrow_w, arrow_h,
					360 - cent->lerpAngles[YAW] + cg.predictedPlayerState.viewangles[YAW], shader,
					cgs.widthRatioCoef);
			}
			break;

		case ET_MISSILE:

			if (cent->currentState.client_num > MAX_CLIENTS //belongs to an NPC
				&& cg_entities[cent->currentState.client_num].currentState.NPC_class == CLASS_VEHICLE)
			{
				//a rocket belonging to an NPC, FIXME: only tracking rockets!

				// ReSharper disable once CppCStyleCast
				const float x = (float)RADAR_X + static_cast<float>(RADAR_RADIUS) + sin(angle) * distance;
				const float ly = y + static_cast<float>(RADAR_RADIUS) + cos(angle) * distance;

				arrow_base_scale = 3.0f;
				if (cg.predictedPlayerState.m_iVehicleNum)
				{
					//I'm in a vehicle
					//if it's targeting me, then play an alarm sound if I'm in a vehicle
					if (cent->currentState.otherentity_num == cg.predictedPlayerState.client_num || cent->
						currentState.otherentity_num == cg.predictedPlayerState.m_iVehicleNum)
					{
						if (radarLockSoundDebounceTime < cg.time)
						{
							vec3_t sound_org;
							int alarm_sound;
							if (actual_dist > RADAR_MISSILE_RANGE * 0.66f)
							{
								radarLockSoundDebounceTime = cg.time + 1000;
								arrow_base_scale = 3.0f;
								alarm_sound = cgi_S_RegisterSound("sound/vehicles/common/lockalarm1.wav");
							}
							else if (actual_dist > RADAR_MISSILE_RANGE / 3.0f)
							{
								radarLockSoundDebounceTime = cg.time + 500;
								arrow_base_scale = 6.0f;
								alarm_sound = cgi_S_RegisterSound("sound/vehicles/common/lockalarm2.wav");
							}
							else
							{
								radarLockSoundDebounceTime = cg.time + 250;
								arrow_base_scale = 9.0f;
								alarm_sound = cgi_S_RegisterSound("sound/vehicles/common/lockalarm3.wav");
							}
							if (actual_dist > RADAR_MISSILE_RANGE)
							{
								actual_dist = RADAR_MISSILE_RANGE;
							}
							VectorMA(cg.refdef.vieworg, -500.0f * (actual_dist / RADAR_MISSILE_RANGE), dir_player,
								sound_org);
							cgi_S_StartSound(sound_org, ENTITYNUM_WORLD, CHAN_AUTO, alarm_sound);
						}
					}
				}
				z_scale = 1.0f;

				//we want to scale the thing up/down based on the relative Z (up/down) positioning
				if (cent->lerpOrigin[2] > cg.predictedPlayerState.origin[2])
				{
					//higher, scale up (between 16 and 24)
					float dif = cent->lerpOrigin[2] - cg.predictedPlayerState.origin[2];

					//max out to 1.5x scale at 512 units above local player's height
					dif /= 1024.0f;
					if (dif > 0.5f)
					{
						dif = 0.5f;
					}
					z_scale += dif;
				}
				else if (cent->lerpOrigin[2] < cg.predictedPlayerState.origin[2])
				{
					//lower, scale down (between 16 and 8)
					float dif = cg.predictedPlayerState.origin[2] - cent->lerpOrigin[2];

					//half scale at 512 units below local player's height
					dif /= 1024.0f;
					if (dif > 0.5f)
					{
						dif = 0.5f;
					}
					z_scale -= dif;
				}

				arrow_base_scale *= z_scale;

				if (cent->currentState.client_num >= MAX_CLIENTS //missile owned by an NPC
					&& cg_entities[cent->currentState.client_num].currentState.NPC_class == CLASS_VEHICLE
					//NPC is a vehicle
					&& cg_entities[cent->currentState.client_num].currentState.m_iVehicleNum <= MAX_CLIENTS
					//Vehicle has a player driver
					&& cgs.clientinfo[cg_entities[cent->currentState.client_num].currentState.m_iVehicleNum - 1].
					infoValid) //player driver is valid
				{
					cgi_R_SetColor(colorTable[CT_RED]);
				}
				else
				{
					cgi_R_SetColor(nullptr);
				}
				CG_DrawPic(x - 4 + x_offset, ly - 4, arrow_base_scale, arrow_base_scale,
					cgs.media.mAutomapRocketIcon);
			}
			break;
		}
		}
	}

	arrow_base_scale = 8.0f;

	arrow_w = arrow_base_scale * RADAR_RADIUS / 128;
	arrow_h = arrow_base_scale * RADAR_RADIUS / 128;

	cgi_R_SetColor(colorTable[CT_WHITE]);
	CG_DrawRotatePic2(RADAR_X + RADAR_RADIUS_X + x_offset, y + RADAR_RADIUS, arrow_w, arrow_h,
		0, cgs.media.mAutomapPlayerIcon, cgs.widthRatioCoef);

	return y + RADAR_RADIUS * 2;
}

/*
=================
CG_DrawAmmoWarning
=================
*/
static void CG_DrawAmmoWarning()
{
	char text[1024] = { 0 };

	if (cg_drawAmmoWarning.integer == 0)
	{
		return;
	}

	if (!cg.lowAmmoWarning)
	{
		return;
	}

	if (weaponData[cg.snap->ps.weapon].ammoIndex == AMMO_NONE)
	{
		//doesn't use ammo, so no warning
		return;
	}

	if (cg.lowAmmoWarning == 2)
	{
		cgi_SP_GetStringTextString("SP_INGAME_INSUFFICIENTENERGY", text, sizeof text);
	}
	else
	{
		return;
		//s = "LOW AMMO WARNING";
	}

	const int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 1.0f);
	cgi_R_Font_DrawString(320 - w / 2, 64, text, colorTable[CT_LTGOLD1], cgs.media.qhFontSmall, -1, 1.0f);
}

//---------------------------------------
static qboolean CG_RenderingFromMiscCamera()
{
	if (cg.snap->ps.viewEntity > 0 &&
		cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		// Only check viewEntities
		if (!Q_stricmp("misc_camera", g_entities[cg.snap->ps.viewEntity].classname))
		{
			// Only doing a misc_camera, so check health.
			if (g_entities[cg.snap->ps.viewEntity].health > 0)
			{
				CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/workingCamera"));
			}
			else
			{
				CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/brokenCamera"));
			}
			// don't render other 2d stuff
			return qtrue;
		}
		if (!Q_stricmp("misc_panel_turret", g_entities[cg.snap->ps.viewEntity].classname))
		{
			// could do a panel turret screen overlay...this is a cheesy placeholder
			CG_DrawPic(30, 90, 128, 300, cgs.media.turretComputerOverlayShader);
			CG_DrawPic(610, 90, -128, 300, cgs.media.turretComputerOverlayShader);
		}
		else
		{
			// FIXME: make sure that this assumption is correct...because I'm assuming that I must be a droid.
			CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/droid_view"));
		}
	}

	// not in misc_camera, render other stuff.
	return qfalse;
}

qboolean cg_usingInFrontOf = qfalse;
qboolean CanUseInfrontOf(const gentity_t*);

static void CG_UseIcon()
{
	cg_usingInFrontOf = CanUseInfrontOf(cg_entities[cg.snap->ps.client_num].gent);
	if (cg_usingInFrontOf)
	{
		cgi_R_SetColor(nullptr);

		if (cg_SerenityJediEngineHudMode.integer == 4)
		{
			CG_DrawPic(20, 285, 26, 26, cgs.media.useableHint);
		}
		else if (cg_SerenityJediEngineHudMode.integer == 5)
		{
			CG_DrawPic(157, 429, 26, 26, cgs.media.useableHint);
		}
		else
		{
			CG_DrawPic(50, 285, 32, 32, cgs.media.useableHint);
		}
	}
}

static void CG_Draw2DScreenTints()
{
	float rage_time, rage_rec_time, absorb_time, protect_time, project_time;
	vec4_t hcolor{};
	//force effects
	if (cg.snap->ps.forcePowersActive & 1 << FP_RAGE)
	{
		if (!cgRageTime)
		{
			cgRageTime = cg.time;
		}

		rage_time = static_cast<float>(cg.time - cgRageTime);

		rage_time /= 9000;

		if (rage_time < 0)
		{
			rage_time = 0;
		}
		if (rage_time > 0.15f)
		{
			rage_time = 0.15f;
		}

		hcolor[3] = rage_time;
		hcolor[0] = 0.7f;
		hcolor[1] = 0;
		hcolor[2] = 0;

		if (!cg.renderingThirdPerson)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}

		cgRageFadeTime = 0;
		cgRageFadeVal = 0;
	}
	else if (cgRageTime)
	{
		if (!cgRageFadeTime)
		{
			cgRageFadeTime = cg.time;
			cgRageFadeVal = 0.15f;
		}

		rage_time = cgRageFadeVal;

		cgRageFadeVal -= (cg.time - cgRageFadeTime) * 0.000005;

		if (rage_time < 0)
		{
			rage_time = 0;
		}
		if (rage_time > 0.15f)
		{
			rage_time = 0.15f;
		}

		if (cg.snap->ps.forceRageRecoveryTime > cg.time)
		{
			float check_rage_rec_time = rage_time;

			if (check_rage_rec_time < 0.15f)
			{
				check_rage_rec_time = 0.15f;
			}

			hcolor[3] = check_rage_rec_time;
			hcolor[0] = rage_time * 4;
			if (hcolor[0] < 0.2f)
			{
				hcolor[0] = 0.2f;
			}
			hcolor[1] = 0.2f;
			hcolor[2] = 0.2f;
		}
		else
		{
			hcolor[3] = rage_time;
			hcolor[0] = 0.7f;
			hcolor[1] = 0;
			hcolor[2] = 0;
		}

		if (!cg.renderingThirdPerson && rage_time)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}
		else
		{
			if (cg.snap->ps.forceRageRecoveryTime > cg.time)
			{
				hcolor[3] = 0.15f;
				hcolor[0] = 0.2f;
				hcolor[1] = 0.2f;
				hcolor[2] = 0.2f;
				CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
			}
			cgRageTime = 0;
		}
	}
	else if (cg.snap->ps.forceRageRecoveryTime > cg.time)
	{
		if (!cgRageRecTime)
		{
			cgRageRecTime = cg.time;
		}

		rage_rec_time = static_cast<float>(cg.time - cgRageRecTime);

		rage_rec_time /= 9000;

		if (rage_rec_time < 0.15f) //0)
		{
			rage_rec_time = 0.15f; //0;
		}
		if (rage_rec_time > 0.15f)
		{
			rage_rec_time = 0.15f;
		}

		hcolor[3] = rage_rec_time;
		hcolor[0] = 0.2f;
		hcolor[1] = 0.2f;
		hcolor[2] = 0.2f;

		if (!cg.renderingThirdPerson)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}

		cgRageRecFadeTime = 0;
		cgRageRecFadeVal = 0;
	}
	else if (cgRageRecTime)
	{
		if (!cgRageRecFadeTime)
		{
			cgRageRecFadeTime = cg.time;
			cgRageRecFadeVal = 0.15f;
		}

		rage_rec_time = cgRageRecFadeVal;

		cgRageRecFadeVal -= (cg.time - cgRageRecFadeTime) * 0.000005;

		if (rage_rec_time < 0)
		{
			rage_rec_time = 0;
		}
		if (rage_rec_time > 0.15f)
		{
			rage_rec_time = 0.15f;
		}

		hcolor[3] = rage_rec_time;
		hcolor[0] = 0.2f;
		hcolor[1] = 0.2f;
		hcolor[2] = 0.2f;

		if (!cg.renderingThirdPerson && rage_rec_time)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}
		else
		{
			cgRageRecTime = 0;
		}
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_ABSORB)
	{
		if (!cgAbsorbTime)
		{
			cgAbsorbTime = cg.time;
		}

		absorb_time = static_cast<float>(cg.time - cgAbsorbTime);

		absorb_time /= 9000;

		if (absorb_time < 0)
		{
			absorb_time = 0;
		}
		if (absorb_time > 0.15f)
		{
			absorb_time = 0.15f;
		}

		hcolor[3] = absorb_time / 2;
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 0.7f;

		if (!cg.renderingThirdPerson)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}

		cgAbsorbFadeTime = 0;
		cgAbsorbFadeVal = 0;
	}
	else if (cgAbsorbTime)
	{
		if (!cgAbsorbFadeTime)
		{
			cgAbsorbFadeTime = cg.time;
			cgAbsorbFadeVal = 0.15f;
		}

		absorb_time = cgAbsorbFadeVal;

		cgAbsorbFadeVal -= (cg.time - cgAbsorbFadeTime) * 0.000005;

		if (absorb_time < 0)
		{
			absorb_time = 0;
		}
		if (absorb_time > 0.15f)
		{
			absorb_time = 0.15f;
		}

		hcolor[3] = absorb_time / 2;
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 0.7f;

		if (!cg.renderingThirdPerson && absorb_time)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}
		else
		{
			cgAbsorbTime = 0;
		}
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_PROTECT)
	{
		if (!cgProtectTime)
		{
			cgProtectTime = cg.time;
		}

		protect_time = static_cast<float>(cg.time - cgProtectTime);

		protect_time /= 9000;

		if (protect_time < 0)
		{
			protect_time = 0;
		}
		if (protect_time > 0.15f)
		{
			protect_time = 0.15f;
		}

		hcolor[3] = protect_time / 2;
		hcolor[0] = 0;
		hcolor[1] = 0.7f;
		hcolor[2] = 0;

		if (!cg.renderingThirdPerson)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}

		cgProtectFadeTime = 0;
		cgProtectFadeVal = 0;
	}
	else if (cgProtectTime)
	{
		if (!cgProtectFadeTime)
		{
			cgProtectFadeTime = cg.time;
			cgProtectFadeVal = 0.15f;
		}

		protect_time = cgProtectFadeVal;

		cgProtectFadeVal -= (cg.time - cgProtectFadeTime) * 0.000005;

		if (protect_time < 0)
		{
			protect_time = 0;
		}
		if (protect_time > 0.15f)
		{
			protect_time = 0.15f;
		}

		hcolor[3] = protect_time / 2;
		hcolor[0] = 0;
		hcolor[1] = 0.7f;
		hcolor[2] = 0;

		if (!cg.renderingThirdPerson && protect_time)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}
		else
		{
			cgProtectTime = 0;
		}
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_PROJECTION)
	{
		if (!cgProjectTime)
		{
			cgProjectTime = cg.time;
		}

		project_time = static_cast<float>(cg.time - cgProjectTime);

		project_time /= 9000;

		if (project_time < 0)
		{
			project_time = 0;
		}
		if (project_time > 0.15f)
		{
			project_time = 0.15f;
		}

		hcolor[3] = project_time / 2;
		hcolor[0] = 0;
		hcolor[1] = 0.7f;
		hcolor[2] = 0;

		if (!cg.renderingThirdPerson)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}

		cgProjectFadeTime = 0;
		cgProjectFadeVal = 0;
	}
	else if (cgProjectTime)
	{
		if (!cgProjectFadeTime)
		{
			cgProjectFadeTime = cg.time;
			cgProjectFadeVal = 0.15f;
		}

		project_time = cgProjectFadeVal;

		cgProjectFadeVal -= (cg.time - cgProjectFadeTime) * 0.000005;

		if (project_time < 0)
		{
			project_time = 0;
		}
		if (project_time > 0.15f)
		{
			project_time = 0.15f;
		}

		hcolor[3] = project_time / 2;
		hcolor[0] = 0;
		hcolor[1] = 0.7f;
		hcolor[2] = 0;

		if (!cg.renderingThirdPerson && project_time)
		{
			CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
		}
		else
		{
			cgProtectTime = 0;
		}
	}

	if (cg.refdef.viewContents & CONTENTS_LAVA)
	{
		//tint screen red
		const float phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		hcolor[3] = 0.5 + 0.15f * sin(phase);
		hcolor[0] = 0.7f;
		hcolor[1] = 0;
		hcolor[2] = 0;

		CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
	}
	else if (cg.refdef.viewContents & CONTENTS_SLIME)
	{
		//tint screen green
		const float phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		hcolor[3] = 0.4 + 0.1f * sin(phase);
		hcolor[0] = 0;
		hcolor[1] = 0.7f;
		hcolor[2] = 0;

		CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
	}
	else if (cg.refdef.viewContents & CONTENTS_WATER)
	{
		//tint screen light blue -- FIXME: check to see if in fog?
		const float phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		hcolor[3] = 0.3 + 0.05f * sin(phase);
		hcolor[0] = 0;
		hcolor[1] = 0.2f;
		hcolor[2] = 0.8f;

		CG_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
	}
}

/*
=================
CG_Draw2D
=================
*/
extern void CG_SaberClashFlare();

static void CG_Draw2D()
{
	char text[1024] = { 0 };
	int w, y_pos;
	const float scale = cg_textprintscale.value;
	const centity_t* cent = &cg_entities[cg.snap->ps.client_num];

	// if we are taking a levelshot for the menu, don't draw anything
	if (cg.levelShot)
	{
		return;
	}

	if (cg_draw2D.integer == 0)
	{
		return;
	}

	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		CG_DrawIntermission();
		return;
	}

	CG_Draw2DScreenTints();

	//end credits
	if (cg_endcredits.integer)
	{
		if (!CG_Credits_Draw())
		{
			CG_DrawCredits(); // will probably get rid of this soon
		}
	}

	CGCam_DrawWideScreen();

	CG_DrawBatteryCharge();

	if (cg.snap->ps.forcePowersActive || cg.snap->ps.forceRageRecoveryTime > cg.time)
	{
		CG_DrawActivePowers();
	}

	// Draw this before the text so that any text won't get clipped off
	if (!in_camera)
	{
		CG_DrawZoomMask();
	}

	CG_DrawScrollText();
	CG_DrawCaptionText();

	if (in_camera)
	{
		//still draw the saber clash flare, but nothing else
		CG_SaberClashFlare();
		return;
	}

	if (CG_RenderingFromMiscCamera())
	{
		// purposely doing an early out when in a misc_camera, change it if needed.

		// allowing center print when in camera mode, probably just an alpha thing - dmv
		CG_DrawCenterString();
		return;
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_SEE)
	{
		//force sight is on
		//indicate this with sight cone thingy
		CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/jsense"));

		if (cg_debugHealthBars.integer < 1)
		{
			CG_DrawHealthBars();
		}
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_PROJECTION)
	{
		CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/droid_view"));
	}

	//if (cg.predictedPlayerState.communicatingflags & (1 << DASHING))
		//if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING))
		//if (cg.predictedPlayerState.communicatingflags & (1 << PROJECTING))
		//if (cg.predictedPlayerState.pm_flags & PMF_DASH_HELD)
		//if (cg.predictedPlayerState.ManualBlockingFlags & (1 << HOLDINGBLOCKANDATTACK))
		//if (cg_entities[cg.snap->ps.client_num].currentState.userInt3 & (1 << FLAG_ATTACKFAKE))
		//if (cent->currentState.eFlags & EF2_DUAL_WEAPONS)
		//if (cent->currentState.eFlags & EF2_DUAL_PISTOLS)
		//if (cg.predictedPlayerState.ManualBlockingFlags & 1 << MBF_ACCURATEMISSILEBLOCKING)
		//if (cg.predictedPlayerState.ManualBlockingFlags & 1 << MBF_NPCBLOCKSTANCE)
		//if (cg.predictedPlayerState.ManualBlockingFlags & 1 << MBF_MISSILESTASIS)
	//{//test for all sorts of shit... does it work? show me.
	//	CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/jsense"));
	//	CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/droid_view"));
	//}

	if (cg_debugHealthBars.integer)
	{
		CG_DrawHealthBars();
	}

	if (cg_SerenityJediEngineMode.integer)
	{
		if (cg.snap->ps.powerups[PW_CLOAKED])
		{
			//draw it as long as it isn't full
			CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/cloaksense"));
		}
		else if (cg.predictedPlayerState.PlayerEffectFlags & 1 << PEF_BURNING)
		{
			//draw it as long as it isn't full
			CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/firesense"));
		}
		else if (cg.predictedPlayerState.PlayerEffectFlags & 1 << PEF_FREEZING)
		{
			//draw it as long as it isn't full
			CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/stasissense"));
		}
		else if ((cg.snap->ps.stats[STAT_HEALTH] <= 25 && cg.snap->ps.stats[STAT_MAX_HEALTH] >= 100) ||
			(cg.snap->ps.stats[STAT_HEALTH] <= (cg.snap->ps.stats[STAT_MAX_HEALTH] / 3) && cg.snap->ps.stats[STAT_MAX_HEALTH] < 100))
		{
			//tint screen red
			CG_DrawPic(0, 0, 640, 480, cgi_R_RegisterShader("gfx/2d/dmgsense"));
		}

		if (cg_debugBlockBars.integer)
		{
			if (cg_SerenityJediEngineMode.integer)
			{
				if (cg_SerenityJediEngineMode.integer == 2)
				{
					CG_DrawBlockPointBars();
				}
				else
				{
					CG_DrawForcePointBars();
				}
			}
			else
			{
				//Dont draw in JKA Mode
			}
		}

		if (cg_debugFatigueBars.integer || cg_saberinfo.integer)
		{
			if (cg_SerenityJediEngineMode.integer)
			{
				CG_DrawFatiguePointBars();
			}
			else
			{
				//Dont draw in JKA Mode
			}
		}
	}

	// don't draw any status if dead
	if (cg.snap->ps.stats[STAT_HEALTH] > 0)
	{
		if (!(cent->gent && cent->gent->s.eFlags & (EF_LOCKED_TO_WEAPON | EF_IN_ATST)) && !
			G_IsRidingVehicle(cent->gent))
		{
			if (cg_SerenityJediEngineMode.integer)
			{
				if (cg_SerenityJediEngineMode.integer == 2 && cg_SerenityJediEngineHudMode.integer == 1)
				{
					CG_DrawSJEIconBackground();
				}
				else if (cg_SerenityJediEngineHudMode.integer == 2)
				{
					CG_DrawSJEIconBackground();
				}
				else if (cg_SerenityJediEngineMode.integer == 2 && cg_SerenityJediEngineHudMode.integer == 3)
				{
					CG_DrawSJEIconBackground();
				}
				else
				{
					CG_DrawIconBackground();
				}
			}
			CG_DrawInventorySelect();

			if ((cg_SerenityJediEngineHudMode.integer == 4 || cg_SerenityJediEngineHudMode.integer == 5) && !
				cg_drawSelectionScrollBar.integer)
			{
				//
			}
			else
			{
				CG_DrawForceSelect();
			}
		}

		if (cg_com_kotor.integer == 1) //playing kotor
		{
			CG_DrawWeaponSelect_kotor();
		}
		else
		{
			if (cent->gent->client->charKOTORWeapons == 1)
			{
				CG_DrawWeaponSelect_kotor();
			}
			else
			{
				CG_DrawWeaponSelect();
			}
		}

		if (cg.zoomMode == 0)
		{
			CG_DrawStats();
		}
		CG_DrawAmmoWarning();

		CG_DrawCrosshairNames();

		CG_DrawCrosshairItem();

		CG_RunRocketLocking();

		CG_DrawPickupItem();

		CG_UseIcon();
	}
	CG_SaberClashFlare();

	float y = 0;
	if (cg_drawSnapshot.integer)
	{
		y = CG_DrawSnapshot(y);
	}
	if (cg_drawFPS.integer)
	{
		y = CG_DrawFPS(y);
	}
	if (cg_drawTimer.integer)
	{
		y = CG_DrawTimer(y);
	}

	// don't draw center string if scoreboard is up
	if (!CG_DrawScoreboard())
	{
		CG_DrawCenterString();
	}

	if (cg_drawRadar.integer)
	{
		cg_draw_radar(y);
	}

	if (missionInfo_Updated)
	{
		if (cg.predictedPlayerState.pm_type != PM_DEAD)
		{
			if (!cg.missionInfoFlashTime)
			{
				cg.missionInfoFlashTime = cg.time + cg_missionInfoFlashTime.integer;
			}

			if (cg.missionInfoFlashTime < cg.time) // Time's up.  They didn't read it.
			{
				cg.missionInfoFlashTime = 0;
				missionInfo_Updated = qfalse;

				CG_ClearDataPadCvars();
			}

			cgi_SP_GetStringTextString("SP_INGAME_NEW_OBJECTIVE_INFO", text, sizeof text);

			y_pos = 20;
			w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontMedium, scale);
			const int x_pos = SCREEN_WIDTH / 2 - w / 2;
			cgi_R_Font_DrawString(x_pos, y_pos, text, colorTable[CT_LTRED1], cgs.media.qhFontMedium, -1, scale);
		}
	}

	if (cg.weaponPickupTextTime > cg.time)
	{
		y_pos = 5;
		gi.Cvar_VariableStringBuffer("cg_WeaponPickupText", text, sizeof text);

		w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontMedium, scale);
		const int x_pos = SCREEN_WIDTH / 2 - w / 2;

		cgi_R_Font_DrawString(x_pos, y_pos, text, colorTable[CT_WHITE], cgs.media.qhFontMedium, -1, scale);
	}
}

/*
===================
CG_DrawIconBackground

Choose the proper background for the icons, scale it depending on if your opening or
closing the icon section of the HU
===================
*/
void CG_DrawIconBackground()
{
	int background_x_pos, background_y_pos;
	int background_width, background_height;
	qhandle_t background;
	constexpr float shutdown_time = 130.0f;

	const bool is_on_veh = G_IsRidingVehicle(cg_entities[0].gent) != nullptr;

	if (cg_SerenityJediEngineHudMode.integer == 0 || cg_SerenityJediEngineHudMode.integer == 4 ||
		cg_SerenityJediEngineHudMode.integer == 5)
	{
		return;
	}

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return;
	}

	if (cg_hudFiles.integer)
	{
		//simple hud
		return;
	}

	// Are we in zoom mode or the HUD is turned off?
	if (cg.zoomMode != 0 || !cg_drawHUD.integer)
	{
		return;
	}

	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		return;
	}

	if (is_on_veh)
	{
		return;
	}

	// Get size and location of bakcround specified in the HUD.MENU file
	if (!cgi_UI_GetMenuInfo("iconbackground", &background_x_pos, &background_y_pos, &background_width,
		&background_height))
	{
		return;
	}

	// Use inventory background?
	if (cg.inventorySelectTime + WEAPON_SELECT_TIME > cg.time || cgs.media.currentBackground == ICON_INVENTORY)
	{
		background = cgs.media.inventoryIconBackground;
	}
	// Use weapon background?
	else if (cg.weaponSelectTime + WEAPON_SELECT_TIME > cg.time || cgs.media.currentBackground == ICON_WEAPONS)
	{
		background = cgs.media.weaponIconBackground;
	}
	// Use force background?
	else
	{
		background = cgs.media.forceIconBackground;
	}

	// Time is up, shutdown the icon section of the HUD
	if (cg.iconSelectTime + WEAPON_SELECT_TIME < cg.time)
	{
		// Scale background down as it goes away
		if (background && cg.iconHUDActive)
		{
			cg.iconHUDPercent = (cg.time - (cg.iconSelectTime + WEAPON_SELECT_TIME)) / shutdown_time;
			cg.iconHUDPercent = 1.0f - cg.iconHUDPercent;

			if (cg.iconHUDPercent < 0.0f)
			{
				cg.iconHUDActive = qfalse;
				cg.iconHUDPercent = 0.f;
			}

			const auto hold_float = static_cast<float>(background_height);
			background_height = static_cast<int>(hold_float * cg.iconHUDPercent);
			CG_DrawPic(background_x_pos, background_y_pos, background_width, -background_height, background);
			// Top half
			CG_DrawPic(background_x_pos, background_y_pos, background_width, background_height, background);
			// Bottom half
		}
		return;
	}

	// Scale background up as it comes up
	if (!cg.iconHUDActive)
	{
		cg.iconHUDPercent = (cg.time - cg.iconSelectTime) / shutdown_time;

		// Calc how far into opening sequence we are
		if (cg.iconHUDPercent > 1.0f)
		{
			cg.iconHUDActive = qtrue;
			cg.iconHUDPercent = 1.0f;
		}
		else if (cg.iconHUDPercent < 0.0f)
		{
			cg.iconHUDPercent = 0.0f;
		}
	}
	else
	{
		cg.iconHUDPercent = 1.0f;
	}

	// Print the background
	if (background)
	{
		cgi_R_SetColor(colorTable[CT_WHITE]);
		const auto hold_float = static_cast<float>(background_height);
		background_height = static_cast<int>(hold_float * cg.iconHUDPercent);
		CG_DrawPic(background_x_pos, background_y_pos, background_width, -background_height, background); // Top half
		CG_DrawPic(background_x_pos, background_y_pos, background_width, background_height, background); // Bottom half
	}
	if (cg.inventorySelectTime + WEAPON_SELECT_TIME > cg.time)
	{
		cgs.media.currentBackground = ICON_INVENTORY;
	}
	else if (cg.weaponSelectTime + WEAPON_SELECT_TIME > cg.time)
	{
		cgs.media.currentBackground = ICON_WEAPONS;
	}
	else
	{
		cgs.media.currentBackground = ICON_FORCE;
	}
}

void CG_DrawSJEIconBackground()
{
	int background_x_pos, background_y_pos;
	int background_width, background_height, x_add;
	qhandle_t background;
	constexpr float shutdown_time = 130.0f;
	const float hud_ratio = cg_hudRatio.integer ? cgs.widthRatioCoef : 1.0f;

	const bool is_on_veh = G_IsRidingVehicle(cg_entities[0].gent) != nullptr;

	if (cg_hudFiles.integer)
	{
		//simple hud
		return;
	}

	if (cg.predictedPlayerState.communicatingflags & (1 << CF_SABERLOCKING) && cg_saberLockCinematicCamera.integer)
	{
		return;
	}

	// Are we in zoom mode or the HUD is turned off?
	if (cg.zoomMode != 0 || !cg_drawHUD.integer)
	{
		return;
	}

	if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD)
	{
		return;
	}

	if (is_on_veh)
	{
		return;
	}

	// Get size and location of bakcround specified in the HUD.MENU file
	if (!cgi_UI_GetMenuInfo("iconbackground", &background_x_pos, &background_y_pos, &background_width,
		&background_height))
	{
		return;
	}

	int prong_left_x = background_x_pos - 60;
	int prong_right_x = background_x_pos + 430;

	// Use inventory background?
	if (cg.inventorySelectTime + WEAPON_SELECT_TIME > cg.time || cgs.media.currentBackground == ICON_INVENTORY)
	{
		background = cgs.media.inventoryIconBackground;
	}
	// Use weapon background?
	else if (cg.weaponSelectTime + WEAPON_SELECT_TIME > cg.time || cgs.media.currentBackground == ICON_WEAPONS)
	{
		background = cgs.media.weaponIconBackground;
	}
	// Use force background?
	else
	{
		background = cgs.media.forceIconBackground;
	}

	// Time is up, shutdown the icon section of the HUD
	if (cg.iconSelectTime + WEAPON_SELECT_TIME < cg.time)
	{
		// Scale background down as it goes away
		if (background && cg.iconHUDActive)
		{
			cg.iconHUDPercent = (cg.time - (cg.iconSelectTime + WEAPON_SELECT_TIME)) / shutdown_time;
			cg.iconHUDPercent = 1.0f - cg.iconHUDPercent;

			if (cg.iconHUDPercent < 0.0f)
			{
				cg.iconHUDActive = qfalse;
				cg.iconHUDPercent = 0.f;
			}

			x_add = 40 * cg.iconHUDPercent;

			const auto hold_float = static_cast<float>(background_height);
			background_height = static_cast<int>(hold_float * cg.iconHUDPercent);
			CG_DrawPic(background_x_pos, background_y_pos, background_width, -background_height, background);
			// Top half
			CG_DrawPic(background_x_pos, background_y_pos, background_width, background_height, background);
			// Bottom half
		}
		else
		{
			x_add = 0;
		}
		cgi_R_SetColor(colorTable[CT_WHITE]);
		CG_DrawPic(prong_left_x + x_add * hud_ratio, background_y_pos - 45, 40 * hud_ratio, 80,
			cgs.media.weaponProngsOff);
		CG_DrawPic(prong_right_x - x_add * hud_ratio, background_y_pos - 45, -40 * hud_ratio, 80,
			cgs.media.weaponProngsOff);
		return;
	}

	prong_left_x = background_x_pos - 60;
	prong_right_x = background_x_pos + 430;

	// Scale background up as it comes up
	if (!cg.iconHUDActive)
	{
		cg.iconHUDPercent = (cg.time - cg.iconSelectTime) / shutdown_time;

		// Calc how far into opening sequence we are
		if (cg.iconHUDPercent > 1.0f)
		{
			cg.iconHUDActive = qtrue;
			cg.iconHUDPercent = 1.0f;
		}
		else if (cg.iconHUDPercent < 0.0f)
		{
			cg.iconHUDPercent = 0.0f;
		}
	}
	else
	{
		cg.iconHUDPercent = 1.0f;
	}

	// Print the background
	if (background)
	{
		cgi_R_SetColor(colorTable[CT_WHITE]);
		const auto hold_float = static_cast<float>(background_height);
		background_height = static_cast<int>(hold_float * cg.iconHUDPercent);
		CG_DrawPic(background_x_pos, background_y_pos, background_width, -background_height, background); // Top half
		CG_DrawPic(background_x_pos, background_y_pos, background_width, background_height, background); // Bottom half
	}

	if (cg.inventorySelectTime + WEAPON_SELECT_TIME > cg.time)
	{
		cgs.media.currentBackground = ICON_INVENTORY;
		background = cgs.media.inventoryProngsOn;
	}
	else if (cg.weaponSelectTime + WEAPON_SELECT_TIME > cg.time)
	{
		cgs.media.currentBackground = ICON_WEAPONS;
		background = cgs.media.weaponProngsOn;
	}
	else
	{
		cgs.media.currentBackground = ICON_FORCE;
		background = cgs.media.forceProngsOn;
	}

	// Side Prongs
	cgi_R_SetColor(colorTable[CT_WHITE]);
	x_add = 40 * cg.iconHUDPercent;
	CG_DrawPic(prong_left_x + x_add * hud_ratio, background_y_pos - 45, 40 * hud_ratio, 80, background);
	CG_DrawPic(prong_right_x - x_add * hud_ratio, background_y_pos - 45, -40 * hud_ratio, 80, background);
}

/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive(const stereoFrame_t stereoView)
{
	float separation;
	vec3_t base_org;

	// optionally draw the info screen instead
	if (!cg.snap)
	{
		CG_DrawInformation();
		return;
	}

	//FIXME: these globals done once at start of frame for various funcs
	AngleVectors(cg.refdefViewAngles, vfwd, vright, vup);
	VectorCopy(vfwd, vfwd_n);
	VectorCopy(vright, vright_n);
	VectorCopy(vup, vup_n);
	VectorNormalize(vfwd_n);
	VectorNormalize(vright_n);
	VectorNormalize(vup_n);

	switch (stereoView)
	{
	case STEREO_CENTER:
		separation = 0;
		break;
	case STEREO_LEFT:
		separation = -cg_stereoSeparation.value / 2;
		break;
	case STEREO_RIGHT:
		separation = cg_stereoSeparation.value / 2;
		break;
	default:
		CG_Error("CG_DrawActive: Undefined stereoView");
	}

	// clear around the rendered view if sized down
	CG_TileClear();

	// offset vieworg appropriately if we're doing stereo separation
	VectorCopy(cg.refdef.vieworg, base_org);
	if (separation != 0)
	{
		VectorMA(cg.refdef.vieworg, -separation, cg.refdef.viewaxis[1], cg.refdef.vieworg);
	}

	if (cg.zoomMode == 3 && cg.snap->ps.batteryCharge) // doing the Light amp goggles thing
	{
		cgi_R_LAGoggles();
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_SEE)
	{
		cg.refdef.rdflags |= RDF_ForceSightOn;
	}

	cg.refdef.rdflags |= RDF_DRAWSKYBOX;

	// draw 3D view
	cgi_R_RenderScene(&cg.refdef);

	// restore original viewpoint if running stereo
	if (separation != 0)
	{
		VectorCopy(base_org, cg.refdef.vieworg);
	}

	// draw status bar and other floating elements
	CG_Draw2D();
}