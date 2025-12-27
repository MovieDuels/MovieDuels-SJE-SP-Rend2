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

#define	CG_PLAYERS_CPP
#include "cg_media.h"
#include "FxScheduler.h"
#include "../game/ghoul2_shared.h"
#include "../game/anims.h"
#include "../game/wp_saber.h"
#include "../game/g_vehicles.h"
#include "../Rufl/hstring.h"

constexpr auto LOOK_SWING_SCALE = 0.5f;
constexpr auto CG_SWINGSPEED = 0.3f;

#include "animtable.h"

extern qboolean WP_SaberBladeUseSecondBladeStyle(const saberInfo_t* saber, int blade_num);
extern void WP_SaberSwingSound(const gentity_t* ent, int saber_num, swingType_t swing_type);
extern vmCvar_t cg_SaberInnonblockableAttackWarning;
extern vmCvar_t cg_IsSaberDoingAttackDamage;
extern qboolean PM_InLedgeMove(int anim);
extern void WP_SabersDamageTrace(gentity_t* ent, qboolean no_effects);
extern void WP_SaberUpdateOldBladeData(gentity_t* ent);

extern vmCvar_t cg_allowcallout;
extern vmCvar_t cg_allowcalloutmarker;

extern vmCvar_t cg_debugHealthBars;
extern vmCvar_t cg_debugBlockBars;
extern vmCvar_t cg_debugFatigueBars;
extern vmCvar_t cg_saberinfo;

extern vmCvar_t cg_ignitionSpeed;
extern vmCvar_t cg_ignitionSpeedstaff;

extern vmCvar_t cg_SFXSabers;
extern vmCvar_t cg_SFXSabersGlowSize;
extern vmCvar_t cg_SFXSabersCoreSize;
extern vmCvar_t cg_SFXLerpOrigin;
//
//
extern vmCvar_t cg_SFXSabersGlowSizeEP1;
extern vmCvar_t cg_SFXSabersCoreSizeEP1;
extern vmCvar_t cg_SFXSabersGlowSizeEP2;
extern vmCvar_t cg_SFXSabersCoreSizeEP2;
extern vmCvar_t cg_SFXSabersGlowSizeEP3;
extern vmCvar_t cg_SFXSabersCoreSizeEP3;
extern vmCvar_t cg_SFXSabersGlowSizeSFX;
extern vmCvar_t cg_SFXSabersCoreSizeSFX;
extern vmCvar_t cg_SFXSabersGlowSizeOT;
extern vmCvar_t cg_SFXSabersCoreSizeOT;
extern vmCvar_t cg_SFXSabersGlowSizeROTJ;
extern vmCvar_t cg_SFXSabersCoreSizeROTJ;
extern vmCvar_t cg_SFXSabersGlowSizeTFA;
extern vmCvar_t cg_SFXSabersCoreSizeTFA;
extern vmCvar_t cg_SFXSabersGlowSizeUSB;
extern vmCvar_t cg_SFXSabersCoreSizeUSB;
//
extern vmCvar_t cg_SFXSabersGlowSizeRebels;
extern vmCvar_t cg_SFXSabersCoreSizeRebels;

extern vmCvar_t cg_jkoeffects;
extern vmCvar_t cg_DebugSaberCombat;

extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern qboolean PM_WindAnim(int anim);
extern qboolean PM_StandingAtReadyAnim(int anim);
extern qboolean PM_InKataAnim(int anim);
extern vmCvar_t cg_SerenityJediEngineMode;
extern qboolean pm_saber_innonblockable_attack(int anim);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern void CheckCameraLocation(vec3_t oldeye_origin);
extern qboolean PM_CrouchAnim(int anim);
extern void CG_CubeOutline(vec3_t mins, vec3_t maxs, int time, unsigned int color);
extern vmCvar_t cg_com_kotor;

/*

player entities generate a great deal of information from implicit ques
taken from the entityState_t

*/

//rww - generic function for applying a shader to the skin.
extern vmCvar_t cg_g2Marks;

void CG_AddGhoul2Mark(const int type, const float size, vec3_t hitloc, vec3_t hitdirection,
	const int entnum, vec3_t entposition, const float entangle, CGhoul2Info_v& ghoul2,
	vec3_t model_scale,
	const int life_time, const int first_model, vec3_t uaxis)
{
	if (!cg_g2Marks.integer)
	{
		//don't want these
		return;
	}

	static SSkinGoreData gore_skin = {};

	gore_skin.growDuration = -1; // do not grow
	gore_skin.goreScaleStartFraction = 1.0; // default start scale
	gore_skin.frontFaces = true; // yes front
	gore_skin.backFaces = false; // no back
	gore_skin.lifeTime = life_time;
	gore_skin.firstModel = first_model;
	gore_skin.current_time = cg.time;
	gore_skin.entNum = entnum;
	gore_skin.SSize = size;
	gore_skin.TSize = size;
	gore_skin.shader = type;
	gore_skin.theta = flrand(0.0f, 6.28f);

	if (uaxis)
	{
		gore_skin.backFaces = true;
		gore_skin.SSize = 6;
		gore_skin.TSize = 3;
		gore_skin.depthStart = -10; //arbitrary depths, just limiting marks to near hit loc
		gore_skin.depthEnd = 15;
		gore_skin.useTheta = false;
		VectorCopy(uaxis, gore_skin.uaxis);
		if (VectorNormalize(gore_skin.uaxis) < 0.001f)
		{
			//too short to make a mark
			return;
		}
	}
	else
	{
		gore_skin.depthStart = -1000;
		gore_skin.depthEnd = 1000;
		gore_skin.useTheta = true;
	}
	VectorCopy(model_scale, gore_skin.scale);

	if (VectorCompare(hitdirection, vec3_origin))
	{
		//wtf, no dir?  Make one up
		VectorSubtract(entposition, hitloc, gore_skin.rayDirection);
		VectorNormalize(gore_skin.rayDirection);
	}
	else
	{
		//use passed in value
		VectorCopy(hitdirection, gore_skin.rayDirection);
	}

	VectorCopy(hitloc, gore_skin.hitLocation);
	VectorCopy(entposition, gore_skin.position);
	gore_skin.angles[YAW] = entangle;

	gi.G2API_AddSkinGore(ghoul2, gore_skin);
}

qboolean CG_RegisterClientModelname(clientInfo_t* ci, const char* headModelName, const char* headSkinName,
	const char* torsoModelName, const char* torsoSkinName,
	const char* legsModelName, const char* legsSkinName);

static void CG_PlayerFootsteps(const centity_t* cent, footstepType_t foot_step_type);
static void CG_PlayerAnimEvents(int anim_file_index, qboolean torso, int old_frame, int frame, int entNum);
extern void BG_G2SetBoneAngles(const centity_t* cent, int bone_index, const vec3_t angles, int flags,
	Eorientations up, Eorientations left, Eorientations forward, qhandle_t* model_list);
extern qboolean pm_saber_in_special_attack(int anim);
extern qboolean PM_SaberInAttack(int move);
extern qboolean PM_SaberInTransitionAny(int move);
extern int PM_GetTurnAnim(const gentity_t* gent, int anim);
extern int PM_AnimLength(int index, animNumber_t anim);
extern qboolean PM_InRoll(const playerState_t* ps);
extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern qboolean PM_SuperBreakWinAnim(int anim);
extern qboolean BG_SaberInPartialDamageMove(gentity_t* self);
extern qboolean BG_SaberInTransitionDamageMove(const playerState_t* ps);
extern qboolean BG_SaberInNonIdleDamageMove(const playerState_t* ps);

//Basic set of custom sounds that everyone needs
// (keep numbers in ascending order in order for variant-capping to work)
const char* cg_customBasicSoundNames[MAX_CUSTOM_BASIC_SOUNDS] =
{
	"*death1.wav",
	"*death2.wav",
	"*death3.wav",
	"*jump1.wav",
	"*pain25.wav",
	"*pain50.wav",
	"*pain75.wav",
	"*pain100.wav",
	"*gurp1.wav",
	"*gurp2.wav",
	"*drown.wav",
	"*gasp.wav",
	"*land1.wav",
	"*falling1.wav",
};

//Used as a supplement to the basic set for enemies and hazard team
// (keep numbers in ascending order in order for variant-capping to work)
const char* cg_customCombatSoundNames[MAX_CUSTOM_COMBAT_SOUNDS] =
{
	"*anger1.wav", //Say when acquire an enemy when didn't have one before
	"*anger2.wav",
	"*anger3.wav",
	"*victory1.wav", //Say when killed an enemy
	"*victory2.wav",
	"*victory3.wav",
	"*confuse1.wav", //Say when confused
	"*confuse2.wav",
	"*confuse3.wav",
	"*pushed1.wav", //Say when force-pushed
	"*pushed2.wav",
	"*pushed3.wav",
	"*choke1.wav",
	"*choke2.wav",
	"*choke3.wav",
	"*ffwarn.wav",
	"*ffturn.wav",
};

//Used as a supplement to the basic set for stormtroopers
// (keep numbers in ascending order in order for variant-capping to work)
const char* cg_customExtraSoundNames[MAX_CUSTOM_EXTRA_SOUNDS] =
{
	"*chase1.wav",
	"*chase2.wav",
	"*chase3.wav",
	"*cover1.wav",
	"*cover2.wav",
	"*cover3.wav",
	"*cover4.wav",
	"*cover5.wav",
	"*detected1.wav",
	"*detected2.wav",
	"*detected3.wav",
	"*detected4.wav",
	"*detected5.wav",
	"*lost1.wav",
	"*outflank1.wav",
	"*outflank2.wav",
	"*escaping1.wav",
	"*escaping2.wav",
	"*escaping3.wav",
	"*giveup1.wav",
	"*giveup2.wav",
	"*giveup3.wav",
	"*giveup4.wav",
	"*look1.wav",
	"*look2.wav",
	"*sight1.wav",
	"*sight2.wav",
	"*sight3.wav",
	"*sound1.wav",
	"*sound2.wav",
	"*sound3.wav",
	"*suspicious1.wav",
	"*suspicious2.wav",
	"*suspicious3.wav",
	"*suspicious4.wav",
	"*suspicious5.wav",
};

//Used as a supplement to the basic set for jedi
// (keep numbers in ascending order in order for variant-capping to work)
const char* cg_customJediSoundNames[MAX_CUSTOM_JEDI_SOUNDS] =
{
	"*combat1.wav",
	"*combat2.wav",
	"*combat3.wav",
	"*jdetected1.wav",
	"*jdetected2.wav",
	"*jdetected3.wav",
	"*taunt1.wav",
	"*taunt2.wav",
	"*taunt3.wav",
	"*taunt4.wav",
	"*taunt5.wav",
	"*jchase1.wav",
	"*jchase2.wav",
	"*jchase3.wav",
	"*jlost1.wav",
	"*jlost2.wav",
	"*jlost3.wav",
	"*deflect1.wav",
	"*deflect2.wav",
	"*deflect3.wav",
	"*gloat1.wav",
	"*gloat2.wav",
	"*gloat3.wav",
	"*pushfail.wav",
};

// done at registration time only...
//
// cuts down on sound-variant registration for low end machines,
//		eg *gloat1.wav (plus...2,...3) can be capped to all be just *gloat1.wav
//
static const char* GetCustomSound_VariantCapped(const char* pps_table[], const int i_entry_num,
	const qboolean b_force_variant1)
{
	extern vmCvar_t cg_VariantSoundCap;

	//	const int iVariantCap = 2;	// test
	const int& i_variant_cap = cg_VariantSoundCap.integer;

	if (i_variant_cap || b_force_variant1)
	{
		auto p = const_cast<char*>(strchr(pps_table[i_entry_num], '.'));
		if (p && p - 2 > pps_table[i_entry_num] && isdigit(p[-1]) && !isdigit(p[-2]))
		{
			const int i_this_variant = p[-1] - '0';

			if (i_this_variant > i_variant_cap || b_force_variant1)
			{
				// ok, let's not load this variant, so pick a random one below the cap value...
				//
				for (int i = 0; i < 2; i++)
					// 1st pass, choose random, 2nd pass (if random not in list), choose xxx1, else fall through...
				{
					char s_name[MAX_QPATH];

					Q_strncpyz(s_name, pps_table[i_entry_num], sizeof s_name);
					p = strchr(s_name, '.');
					if (p)
					{
						*p = '\0';
						s_name[strlen(s_name) - 1] = '\0'; // strip the digit

						const int i_random = b_force_variant1 ? 1 : !i ? Q_irand(1, i_variant_cap) : 1;

						strcat(s_name, va("%d", i_random));

						// does this exist in the entries before the original one?...
						//
						for (int i_scan_num = 0; i_scan_num < i_entry_num; i_scan_num++)
						{
							if (!Q_stricmp(pps_table[i_scan_num], s_name))
							{
								// yeah, this entry is also present in the table, so ok to return it
								//
								return pps_table[i_scan_num];
							}
						}
					}
				}

				// didn't find an entry corresponding to either the random name, or the xxxx1 version,
				//	so give up and drop through to return the original...
				//
			}
		}
	}

	return pps_table[i_entry_num];
}

extern cvar_t* g_sex;
extern cvar_t* com_buildScript;

static void CG_RegisterCustomSounds(clientInfo_t* ci, const int i_sound_entry_base, const int i_table_entries,
	const char* pps_table[], const char* ps_dir)
{
	for (int i = 0; i < i_table_entries; i++)
	{
		char s[MAX_QPATH] = { 0 };
		const char* p_s = GetCustomSound_VariantCapped(pps_table, i, qfalse);
		COM_StripExtension(p_s, s, sizeof s);

		sfxHandle_t h_sfx = 0;
		if (g_sex->string[0] == 'f')
		{
			h_sfx = cgi_S_RegisterSound(va("sound/chars/%s/misc/%s_f.wav", ps_dir, s + 1));
		}
		if (h_sfx == 0 || com_buildScript->integer)
		{
			h_sfx = cgi_S_RegisterSound(va("sound/chars/%s/misc/%s.wav", ps_dir, s + 1));
		}
		if (h_sfx == 0)
		{
			// hmmm... variant in table was missing, so forcibly-retry with %1 version (which we may have just tried, but wtf?)...
			//
			p_s = GetCustomSound_VariantCapped(pps_table, i, qtrue);
			COM_StripExtension(p_s, s, sizeof s);
			if (g_sex->string[0] == 'f')
			{
				h_sfx = cgi_S_RegisterSound(va("sound/chars/%s/misc/%s_f.wav", ps_dir, s + 1));
			}
			if (h_sfx == 0 || com_buildScript->integer)
			{
				h_sfx = cgi_S_RegisterSound(va("sound/chars/%s/misc/%s.wav", ps_dir, s + 1));
			}
			//
			// and fall through regardless...
			//
		}

		ci->sounds[i + i_sound_entry_base] = h_sfx;
	}
}

/*
================
CG_CustomSound

  NOTE: when you call this, check the value.  If zero, do not try to play the sound.
		Either that or when a sound that doesn't exist is played, don't play the null
		sound honk and don't display the error message

================
*/
static sfxHandle_t CG_CustomSound(const int entityNum, const char* sound_name, const int custom_sound_set)
{
	int i;

	if (sound_name[0] != '*')
	{
		return cgi_S_RegisterSound(sound_name);
	}

	if (!g_entities[entityNum].client)
	{
		// No client, this should never happen, so just don't
#ifndef FINAL_BUILD
		//		CG_Printf( "custom sound not on client: %s", soundName );
#endif
		return 0;
	}
	const clientInfo_t* ci = &g_entities[entityNum].client->clientInfo;

	//FIXME: if the sound you want to play could not be found, pick another from the same
	//general grouping?  ie: if you want ff_2c and there is none, try ff_2b or ff_2a...
	switch (custom_sound_set)
	{
	case CS_BASIC:
		// There should always be a clientInfo structure if there is a client, but just make sure...
		if (ci)
		{
			for (i = 0; i < MAX_CUSTOM_BASIC_SOUNDS && cg_customBasicSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customBasicSoundNames[i]))
				{
					return ci->sounds[i];
				}
			}
		}
		break;
	case CS_COMBAT:
		// There should always be a clientInfo structure if there is a client, but just make sure...
		if (ci)
		{
			for (i = 0; i < MAX_CUSTOM_COMBAT_SOUNDS && cg_customCombatSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customCombatSoundNames[i]))
				{
					return ci->sounds[i + MAX_CUSTOM_BASIC_SOUNDS];
				}
			}
		}
		break;
	case CS_EXTRA:
		// There should always be a clientInfo structure if there is a client, but just make sure...
		if (ci)
		{
			for (i = 0; i < MAX_CUSTOM_EXTRA_SOUNDS && cg_customExtraSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customExtraSoundNames[i]))
				{
					return ci->sounds[i + MAX_CUSTOM_BASIC_SOUNDS + MAX_CUSTOM_COMBAT_SOUNDS];
				}
			}
		}
		break;
	case CS_JEDI:
		// There should always be a clientInfo structure if there is a client, but just make sure...
		if (ci)
		{
			for (i = 0; i < MAX_CUSTOM_JEDI_SOUNDS && cg_customJediSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customJediSoundNames[i]))
				{
					return ci->sounds[i + MAX_CUSTOM_BASIC_SOUNDS + MAX_CUSTOM_COMBAT_SOUNDS + MAX_CUSTOM_EXTRA_SOUNDS];
				}
			}
		}
		break;
	case CS_TRY_ALL:
	default:
		//no set specified, search all
		if (ci)
		{
			for (i = 0; i < MAX_CUSTOM_BASIC_SOUNDS && cg_customBasicSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customBasicSoundNames[i]))
				{
					return ci->sounds[i];
				}
			}
			for (i = 0; i < MAX_CUSTOM_COMBAT_SOUNDS && cg_customCombatSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customCombatSoundNames[i]))
				{
					return ci->sounds[i + MAX_CUSTOM_BASIC_SOUNDS];
				}
			}
			for (i = 0; i < MAX_CUSTOM_EXTRA_SOUNDS && cg_customExtraSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customExtraSoundNames[i]))
				{
					return ci->sounds[i + MAX_CUSTOM_BASIC_SOUNDS + MAX_CUSTOM_COMBAT_SOUNDS];
				}
			}
			for (i = 0; i < MAX_CUSTOM_JEDI_SOUNDS && cg_customJediSoundNames[i]; i++)
			{
				if (!Q_stricmp(sound_name, cg_customJediSoundNames[i]))
				{
					return ci->sounds[i + MAX_CUSTOM_BASIC_SOUNDS + MAX_CUSTOM_COMBAT_SOUNDS + MAX_CUSTOM_EXTRA_SOUNDS];
				}
			}
		}
		break;
	}

#ifdef FINAL_BUILD
	CG_Printf("Unknown custom sound: %s", sound_name);
#else
	CG_Error("Unknown custom sound: %s", soundName);
#endif
	return 0;
}

qboolean CG_TryPlayCustomSound(vec3_t origin, const int entityNum, const soundChannel_t channel,
	const char* sound_name, const int custom_sound_set)
{
	const sfxHandle_t sound_index = CG_CustomSound(entityNum, sound_name, custom_sound_set);
	if (!sound_index)
	{
		return qfalse;
	}

	cgi_S_StartSound(origin, entityNum, channel, sound_index);
	return qtrue;
}

/*
======================
CG_NewClientinfo

  For player only, NPCs get them through NPC_stats and G_ModelIndex
======================
*/
void CG_NewClientinfo(const int client_num)
{
	const char* configstring = CG_ConfigString(client_num + CS_PLAYERS);

	if (!configstring[0])
	{
		return; // player just left
	}
	//ci = &cgs.clientinfo[client_num];
	if (!g_entities[client_num].client)
	{
		return;
	}
	clientInfo_t* ci = &g_entities[client_num].client->clientInfo;

	// isolate the player's name
	const char* v = Info_ValueForKey(configstring, "n");
	Q_strncpyz(ci->name, v, sizeof ci->name);

	// handicap
	v = Info_ValueForKey(configstring, "hc");
	ci->handicap = atoi(v);

	// team
	v = Info_ValueForKey(configstring, "t");
	ci->team = static_cast<team_t>(atoi(v));

	// faction
	v = Info_ValueForKey(configstring, "f");
	ci->faction = static_cast<faction_t>(atoi(v));

	// legsModel
	v = Info_ValueForKey(configstring, "legsModel");

	Q_strncpyz(g_entities[client_num].client->renderInfo.legsModelName, v,
		sizeof g_entities[client_num].client->renderInfo.legsModelName);

	// torsoModel
	v = Info_ValueForKey(configstring, "torsoModel");

	Q_strncpyz(g_entities[client_num].client->renderInfo.torsoModelName, v,
		sizeof g_entities[client_num].client->renderInfo.torsoModelName);

	// headModel
	v = Info_ValueForKey(configstring, "headModel");

	Q_strncpyz(g_entities[client_num].client->renderInfo.headModelName, v,
		sizeof g_entities[client_num].client->renderInfo.headModelName);

	// sounds
	v = Info_ValueForKey(configstring, "snd");

	ci->customBasicSoundDir = G_NewString(v);

	//player uses only the basic custom and combat sound sets, not the extra or jedi
	CG_RegisterCustomSounds(ci,
		0, // int iSoundEntryBase,
		MAX_CUSTOM_BASIC_SOUNDS, // int iTableEntries,
		cg_customBasicSoundNames, // const char *ppsTable[],
		ci->customBasicSoundDir // const char *psDir
	);

	CG_RegisterCustomSounds(ci,
		MAX_CUSTOM_BASIC_SOUNDS, // int iSoundEntryBase,
		MAX_CUSTOM_COMBAT_SOUNDS, // int iTableEntries,
		cg_customCombatSoundNames, // const char *ppsTable[],
		ci->customBasicSoundDir // const char *psDir
	);
	ci->infoValid = qfalse;
}

/*
CG_RegisterNPCCustomSounds
*/
void CG_RegisterNPCCustomSounds(clientInfo_t* ci)
{
	if (ci->customBasicSoundDir && ci->customBasicSoundDir[0])
	{
		CG_RegisterCustomSounds(ci,
			0, // int iSoundEntryBase,
			MAX_CUSTOM_BASIC_SOUNDS, // int iTableEntries,
			cg_customBasicSoundNames, // const char *ppsTable[],
			ci->customBasicSoundDir // const char *psDir
		);
	}

	if (ci->customCombatSoundDir && ci->customCombatSoundDir[0])
	{
		CG_RegisterCustomSounds(ci,
			MAX_CUSTOM_BASIC_SOUNDS, // int iSoundEntryBase,
			MAX_CUSTOM_COMBAT_SOUNDS, // int iTableEntries,
			cg_customCombatSoundNames, // const char *ppsTable[],
			ci->customCombatSoundDir // const char *psDir
		);
	}

	if (ci->customExtraSoundDir && ci->customExtraSoundDir[0])
	{
		CG_RegisterCustomSounds(ci,
			MAX_CUSTOM_BASIC_SOUNDS + MAX_CUSTOM_COMBAT_SOUNDS, // int iSoundEntryBase,
			MAX_CUSTOM_EXTRA_SOUNDS, // int iTableEntries,
			cg_customExtraSoundNames, // const char *ppsTable[],
			ci->customExtraSoundDir // const char *psDir
		);
	}

	if (ci->customJediSoundDir && ci->customJediSoundDir[0])
	{
		CG_RegisterCustomSounds(ci,
			MAX_CUSTOM_BASIC_SOUNDS + MAX_CUSTOM_COMBAT_SOUNDS + MAX_CUSTOM_EXTRA_SOUNDS,
			// int iSoundEntryBase,
			MAX_CUSTOM_JEDI_SOUNDS, // int iTableEntries,
			cg_customJediSoundNames, // const char *ppsTable[],
			ci->customJediSoundDir // const char *psDir
		);
	}
}

/*
=============================================================================

PLAYER ANIMATION

=============================================================================
*/

qboolean ValidAnimFileIndex(const int index)
{
	if (index < 0 || index >= level.numKnownAnimFileSets)
	{
		Com_Printf(S_COLOR_RED "Bad animFileIndex: %d\n", index);
		return qfalse;
	}

	return qtrue;
}

/*
======================
CG_ClearAnimEvtCache

resets all the event cache so that a vid restart will recache them
======================
*/
void CG_ClearAnimEvtCache()
{
	for (int i = 0; i < level.numKnownAnimFileSets; i++)
	{
		//
	}
}

/*
===============
CG_SetLerpFrameAnimation
===============
*/
static void CG_SetLerpFrameAnimation(clientInfo_t* ci, lerpFrame_t* lf, int new_animation)
{
	if (new_animation < 0 || new_animation >= MAX_ANIMATIONS)
	{
#ifdef FINAL_BUILD
		new_animation = 0;
#else
		CG_Error("Bad animation number: %i for ", newAnimation, ci->name);
#endif
	}

	lf->animationNumber = new_animation;

	if (!ValidAnimFileIndex(ci->animFileIndex))
	{
#ifdef FINAL_BUILD
		ci->animFileIndex = 0;
#else
		CG_Error("Bad animFileIndex: %i for %s", ci->animFileIndex, ci->name);
#endif
	}

	animation_t* anim = &level.knownAnimFileSets[ci->animFileIndex].animations[new_animation];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + abs(anim->frameLerp);
}

/*
===============
CG_RunLerpFrame

Sets cg.snap, cg.oldFrame, and cg.backlerp
cg.time should be between oldFrameTime and frameTime after exit
===============
*/
static qboolean CG_RunLerpFrame(clientInfo_t* ci, lerpFrame_t* lf, const int new_animation, const int entNum)
{
	qboolean newFrame = qfalse;

	// see if the animation sequence is switching
	//FIXME: allow multiple-frame overlapped lerping between sequences? - Possibly last 3 of last seq and first 3 of next seq?
	if (new_animation != lf->animationNumber || !lf->animation)
	{
		CG_SetLerpFrameAnimation(ci, lf, new_animation);
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if (cg.time >= lf->frameTime)
	{
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		// get the next frame based on the animation
		const animation_t* anim = lf->animation;
		//Do we need to speed up or slow down the anim?
		int anim_frame_time = abs(anim->frameLerp);

		//special hack for player to ensure quick weapon change
		if (entNum == 0)
		{
			if (lf->animationNumber == TORSO_DROPWEAP1 || lf->animationNumber == TORSO_RAISEWEAP1)
			{
				anim_frame_time = 50;
			}
		}

		if (cg.time < lf->animationTime)
		{
			lf->frameTime = lf->animationTime; // initial lerp
		}
		else
		{
			lf->frameTime = lf->oldFrameTime + anim_frame_time;
		}

		int f = (lf->frameTime - lf->animationTime) / anim_frame_time;
		if (f >= anim->numFrames)
		{
			//Reached the end of the anim
			//FIXME: Need to set a flag here to TASK_COMPLETE
			f -= anim->numFrames;
			if (anim->loopFrames != -1) //Before 0 meant no loop
			{
				if (anim->numFrames - anim->loopFrames == 0)
				{
					f %= anim->numFrames;
				}
				else
				{
					f %= anim->numFrames - anim->loopFrames;
				}
				f += anim->loopFrames;
			}
			else
			{
				f = anim->numFrames - 1;
				if (f < 0)
				{
					f = 0;
				}
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = cg.time;
			}
		}

		if (anim->frameLerp < 0)
		{
			lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
		}
		else
		{
			lf->frame = anim->firstFrame + f;
		}

		if (cg.time > lf->frameTime)
		{
			lf->frameTime = cg.time;
		}

		newFrame = qtrue;
	}

	if (lf->frameTime > cg.time + 200)
	{
		lf->frameTime = cg.time;
	}

	if (lf->oldFrameTime > cg.time)
	{
		lf->oldFrameTime = cg.time;
	}
	// calculate current lerp value
	if (lf->frameTime == lf->oldFrameTime)
	{
		lf->backlerp = 0;
	}
	else
	{
		lf->backlerp = 1.0 - static_cast<float>(cg.time - lf->oldFrameTime) / (lf->frameTime - lf->oldFrameTime);
	}

	return newFrame;
}

/*
===============
CG_ClearLerpFrame
===============
*/
static void CG_ClearLerpFrame(clientInfo_t* ci, lerpFrame_t* lf, const int animation_number)
{
	lf->frameTime = lf->oldFrameTime = cg.time;
	CG_SetLerpFrameAnimation(ci, lf, animation_number);
	if (lf->animation->frameLerp < 0)
	{
		//Plays backwards
		lf->oldFrame = lf->frame = lf->animation->firstFrame + lf->animation->numFrames;
	}
	else
	{
		lf->oldFrame = lf->frame = lf->animation->firstFrame;
	}
}

/*
===============
CG_PlayerAnimation
===============
*/

static void CG_PlayerAnimation(centity_t* cent, int* legs_old, int* legs, float* legs_back_lerp, int* torso_old,
	int* torso, float* torso_back_lerp)
{
	int legs_turn_anim = -1;
	qboolean new_legs_frame;

	clientInfo_t* ci = &cent->gent->client->clientInfo;
	//Changed this from cent->currentState.legsAnim to cent->gent->client->ps.legsAnim because it was screwing up our timers when we've just changed anims while turning
	const int legs_anim = cent->gent->client->ps.legsAnim;

	// do the shuffle turn frames locally (MAN this is an Fugly-ass hack!)

	if (cent->pe.legs.yawing)
	{
		legs_turn_anim = PM_GetTurnAnim(cent->gent, legs_anim);
	}

	if (legs_turn_anim != -1)
	{
		new_legs_frame = CG_RunLerpFrame(ci, &cent->pe.legs, legs_turn_anim, cent->gent->s.number);
	}
	else
	{
		new_legs_frame = CG_RunLerpFrame(ci, &cent->pe.legs, legs_anim, cent->gent->s.number);
	}

	*legs_old = cent->pe.legs.oldFrame;
	*legs = cent->pe.legs.frame;
	*legs_back_lerp = cent->pe.legs.backlerp;

	if (new_legs_frame)
	{
		if (ValidAnimFileIndex(ci->animFileIndex))
		{
			CG_PlayerAnimEvents(ci->animFileIndex, qfalse, cent->pe.legs.frame, cent->pe.legs.frame,
				cent->currentState.number);
		}
	}

	const qboolean new_torso_frame = CG_RunLerpFrame(ci, &cent->pe.torso, cent->gent->client->ps.torsoAnim,
		cent->gent->s.number);

	*torso_old = cent->pe.torso.oldFrame;
	*torso = cent->pe.torso.frame;
	*torso_back_lerp = cent->pe.torso.backlerp;

	if (new_torso_frame)
	{
		if (ValidAnimFileIndex(ci->animFileIndex))
		{
			CG_PlayerAnimEvents(ci->animFileIndex, qtrue, cent->pe.torso.frame, cent->pe.torso.frame,
				cent->currentState.number);
		}
	}
}

extern int PM_LegsAnimForFrame(gentity_t* ent, int legs_frame);
extern int PM_TorsoAnimForFrame(gentity_t* ent, int torso_frame);

static void CG_PlayerAnimEventDo(centity_t* cent, animevent_t* anim_event)
{
	//FIXME: pass in event, switch off the type
	if (cent == nullptr || anim_event == nullptr)
	{
		return;
	}

	soundChannel_t channel = CHAN_AUTO;
	switch (anim_event->eventType)
	{
	case AEV_SOUNDCHAN:
		channel = static_cast<soundChannel_t>(anim_event->eventData[AED_SOUNDCHANNEL]);
	case AEV_SOUND:
		// are there variations on the sound?
	{
		const int hold_snd = anim_event->eventData[AED_SOUNDINDEX_START + Q_irand(
			0, anim_event->eventData[AED_SOUND_NUMRANDOMSNDS])];
		if (hold_snd > 0)
		{
			if (cgs.sound_precache[hold_snd])
			{
				cgi_S_StartSound(nullptr, cent->currentState.client_num, channel, cgs.sound_precache[hold_snd]);
			}
			else
			{
				//try a custom sound
				const char* s = CG_ConfigString(CS_SOUNDS + hold_snd);
				CG_TryPlayCustomSound(nullptr, cent->currentState.client_num, channel, va("%s.wav", s), CS_TRY_ALL);
			}
		}
	}
	break;
	case AEV_SABER_SWING:
		if (cent->gent)
		{
			//cheat over to game side and play sound from there...
			WP_SaberSwingSound(cent->gent, anim_event->eventData[AED_SABER_SWING_saber_num],
				static_cast<swingType_t>(anim_event->eventData[AED_SABER_SWING_TYPE]));
		}
		break;
	case AEV_SABER_SPIN:
		if (cent->gent
			&& cent->gent->client)
		{
			const saberInfo_t* saber = &cent->gent->client->ps.saber[anim_event->eventData[AED_SABER_SPIN_saber_num]];
			if (saber)
			{
				int spinSound;
				if (saber->spinSound
					&& cgs.sound_precache[saber->spinSound])
				{
					//use override
					spinSound = cgs.sound_precache[saber->spinSound];
				}
				else
				{
					switch (anim_event->eventData[AED_SABER_SPIN_TYPE])
					{
					case 0: //saberspinoff
						spinSound = cgi_S_RegisterSound("sound/weapons/saber/saberspinoff.wav");
						break;
					case 1: //saberspin
						spinSound = cgi_S_RegisterSound("sound/weapons/saber/saberspin.wav");
						break;
					case 2: //saberspin1
						spinSound = cgi_S_RegisterSound("sound/weapons/saber/saberspin1.wav");
						break;
					case 3: //saberspin2
						spinSound = cgi_S_RegisterSound("sound/weapons/saber/saberspin2.wav");
						break;
					case 4: //saberspin3
						spinSound = cgi_S_RegisterSound("sound/weapons/saber/saberspin3.wav");
						break;
					default: //random saberspin1-3
						spinSound = cgi_S_RegisterSound(va("sound/weapons/saber/saberspin%d.wav", Q_irand(1, 3)));
						break;
					}
				}
				if (spinSound)
				{
					cgi_S_StartSound(nullptr, cent->currentState.client_num, CHAN_AUTO, spinSound);
				}
			}
		}
		break;
	case AEV_FOOTSTEP:
		CG_PlayerFootsteps(cent, static_cast<footstepType_t>(anim_event->eventData[AED_FOOTSTEP_TYPE]));
		break;
	case AEV_EFFECT:
		if (anim_event->eventData[AED_EFFECTINDEX] == -1)
		{
			//invalid effect
			if (anim_event->stringData != nullptr
				&& anim_event->stringData[0])
			{
				//some sort of hard-coded effect
				if (Q_stricmp("push_l", anim_event->stringData) == 0)
				{
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/push_md.mp3"));
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/push.mp3"));
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/pushlow.mp3"));
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/pushhard.mp3"));
					cent->gent->client->ps.powerups[PW_FORCE_PUSH] = cg.time + anim_event->eventData[
						AED_EFFECT_PROBABILITY];
					//AED_EFFECT_PROBABILITY in this case is the number of ms for the effect to last
					cent->gent->client->pushEffectFadeTime = 0;
				}
				else if (Q_stricmp("push_r", anim_event->stringData) == 0)
				{
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/push_md.mp3"));
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/push.mp3"));
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/pushlow.mp3"));
					cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num, CHAN_AUTO,
						cgi_S_RegisterSound("sound/weapons/force/pushhard.mp3"));
					cent->gent->client->ps.powerups[PW_FORCE_PUSH_RHAND] = cg.time + anim_event->eventData[
						AED_EFFECT_PROBABILITY];
					//AED_EFFECT_PROBABILITY in this case is the number of ms for the effect to last
					cent->gent->client->pushEffectFadeTime = 0;
				}
				else if (Q_stricmp("scepter_beam", anim_event->stringData) == 0)
				{
					int modelIndex = cent->gent->weaponModel[1];
					if (modelIndex <= 0)
					{
						modelIndex = cent->gent->cinematicModel;
					}
					if (modelIndex > 0)
					{
						//we have a cinematic model
						const int bolt_index = gi.G2API_AddBolt(&cent->gent->ghoul2[modelIndex], "*flash");
						if (bolt_index > -1)
						{
							//cinematic model has a flash bolt
							CG_PlayEffectBolted("scepter/beam.efx", modelIndex, bolt_index,
								cent->currentState.client_num, cent->lerpOrigin,
								anim_event->eventData[AED_EFFECT_PROBABILITY], qtrue);
						}
					}
				}
				//FIXME: add more
			}
		}
		else
		{
			//add bolt, play effect
			if (anim_event->stringData != nullptr && cent && cent->gent && cent->gent->ghoul2.size())
			{
				//have a bolt name we want to use
				anim_event->eventData[AED_MODELINDEX] = cent->gent->playerModel;
				if ((Q_stricmpn("*blade", anim_event->stringData, 6) == 0
					|| Q_stricmp("*flash", anim_event->stringData) == 0)
					&& cent->gent->weaponModel[0] > 0)
				{
					//must be a weapon, try weapon 0?
					anim_event->eventData[AED_BOLTINDEX] = gi.G2API_AddBolt(
						&cent->gent->ghoul2[cent->gent->weaponModel[0]], anim_event->stringData);
					if (anim_event->eventData[AED_BOLTINDEX] != -1)
					{
						//found it!
						anim_event->eventData[AED_MODELINDEX] = cent->gent->weaponModel[0];
					}
					else
					{
						//hmm, just try on the player model, then?
						anim_event->eventData[AED_BOLTINDEX] = gi.G2API_AddBolt(
							&cent->gent->ghoul2[cent->gent->playerModel], anim_event->stringData);
					}
				}
				else
				{
					anim_event->eventData[AED_BOLTINDEX] = gi.G2API_AddBolt(
						&cent->gent->ghoul2[cent->gent->playerModel], anim_event->stringData);
				}
				anim_event->stringData = nullptr; //so we don't try to do this again
			}
			if (anim_event->eventData[AED_BOLTINDEX] != -1)
			{
				//have a bolt we want to play the effect on
				CG_PlayEffectIDBolted(anim_event->eventData[AED_EFFECTINDEX],
					anim_event->eventData[AED_MODELINDEX],
					anim_event->eventData[AED_BOLTINDEX],
					cent->currentState.client_num,
					cent->lerpOrigin);
			}
			else
			{
				//play at origin?  FIXME: maybe allow a fwd/rt/up offset?
				constexpr vec3_t up = { 0, 0, 1 };
				CG_PlayEffectID(anim_event->eventData[AED_EFFECTINDEX], cent->lerpOrigin, up);
			}
		}
		break;
	case AEV_FIRE:
		//add fire event
		if (anim_event->eventData[AED_FIRE_ALT])
		{
			G_AddEvent(cent->gent, EV_ALT_FIRE, 0);
		}
		else
		{
			G_AddEvent(cent->gent, EV_FIRE_WEAPON, 0);
		}
		break;
	case AEV_MOVE:
		//make him jump
		if (cent && cent->gent && cent->gent->client)
		{
			if (cent->gent->client->ps.groundEntityNum != ENTITYNUM_NONE)
			{
				//on something
				vec3_t fwd, rt, up;
				const vec3_t angles = { 0, cent->gent->client->ps.viewangles[YAW], 0 };
				AngleVectors(angles, fwd, rt, up);
				//FIXME: set or add to velocity?
				VectorScale(fwd, anim_event->eventData[AED_MOVE_FWD], cent->gent->client->ps.velocity);
				VectorMA(cent->gent->client->ps.velocity, anim_event->eventData[AED_MOVE_RT], rt,
					cent->gent->client->ps.velocity);
				VectorMA(cent->gent->client->ps.velocity, anim_event->eventData[AED_MOVE_UP], up,
					cent->gent->client->ps.velocity);

				if (anim_event->eventData[AED_MOVE_UP] > 0)
				{
					//a jump
					cent->gent->client->ps.pm_flags |= PMF_JUMPING;

					G_AddEvent(cent->gent, EV_JUMP, 0);
				}
			}
		}
		break;
	default:;
	}
}

static void CG_PlayerAnimEvents(const int anim_file_index, const qboolean torso, const int old_frame, const int frame,
	const int entNum)
{
	int first_frame = 0, lastFrame = 0;
	qboolean do_event = qfalse;
	qboolean in_same_anim = qfalse;
	qboolean loop_anim = qfalse;
	qboolean anim_backward = qfalse;
	animevent_t* anim_events;
	int gla_index = -1;

	if (g_entities[entNum].ghoul2.size())
	{
		gla_index = gi.G2API_GetAnimIndex(&g_entities[entNum].ghoul2[0]);
	}

	if (torso)
	{
		anim_events = level.knownAnimFileSets[anim_file_index].torsoAnimEvents;
	}
	else
	{
		anim_events = level.knownAnimFileSets[anim_file_index].legsAnimEvents;
	}
	if (abs(old_frame - frame) > 1)
	{
		//given a range, see if keyFrame falls in that range
		int old_anim, anim;
		if (torso)
		{
			//more precise, slower
			old_anim = PM_TorsoAnimForFrame(&g_entities[entNum], old_frame);
			anim = PM_TorsoAnimForFrame(&g_entities[entNum], frame);
		}
		else
		{
			//more precise, slower
			old_anim = PM_LegsAnimForFrame(&g_entities[entNum], old_frame);
			anim = PM_LegsAnimForFrame(&g_entities[entNum], frame);
		}

		if (anim != old_anim)
		{
			//not in same anim
			in_same_anim = qfalse;
			//FIXME: we *could* see if the oldFrame was *just about* to play the keyframed sound...
		}
		else
		{
			//still in same anim, check for looping anim
			in_same_anim = qtrue;
			const animation_t* animation = &level.knownAnimFileSets[anim_file_index].animations[anim];
			anim_backward = static_cast<qboolean>(animation->frameLerp < 0);
			if (animation->loopFrames != -1)
			{
				//a looping anim!
				loop_anim = qtrue;
				first_frame = animation->firstFrame;
				lastFrame = animation->firstFrame + animation->numFrames;
			}
		}
	}

	const hstring my_model = g_entities[entNum].NPC_type; //apparently NPC_type is always the same as the model name???

	// Check for anim event
	for (int i = 0; i < MAX_ANIM_EVENTS; ++i)
	{
		if (anim_events[i].eventType == AEV_NONE) // No event, end of list
		{
			break;
		}

		if (gla_index != -1 && anim_events[i].glaIndex != gla_index)
		{
			continue;
		}

		qboolean match = qfalse;
		if (anim_events[i].modelOnly == 0 || anim_events[i].modelOnly == my_model.handle())
		{
			if (anim_events[i].keyFrame == frame)
			{
				//exact match
				match = qtrue;
			}
			else if (abs(old_frame - frame) > 1) //&& cg_reliableAnimEvents.integer )
			{
				//given a range, see if keyFrame falls in that range
				if (in_same_anim)
				{
					//if changed anims altogether, sorry, the sound is lost
					if (abs(old_frame - anim_events[i].keyFrame) <= 3
						|| abs(frame - anim_events[i].keyFrame) <= 3)
					{
						//must be at least close to the keyframe
						if (anim_backward)
						{
							//animation plays backwards
							if (old_frame > anim_events[i].keyFrame && frame < anim_events[i].keyFrame)
							{
								//old to new passed through keyframe
								match = qtrue;
							}
							else if (loop_anim)
							{
								//hmm, didn't pass through it linearally, see if we looped
								if (anim_events[i].keyFrame >= first_frame && anim_events[i].keyFrame < lastFrame)
								{
									//keyframe is in this anim
									if (old_frame > anim_events[i].keyFrame
										&& frame > old_frame)
									{
										//old to new passed through keyframe
										match = qtrue;
									}
								}
							}
						}
						else
						{
							//anim plays forwards
							if (old_frame < anim_events[i].keyFrame && frame > anim_events[i].keyFrame)
							{
								//old to new passed through keyframe
								match = qtrue;
							}
							else if (loop_anim)
							{
								//hmm, didn't pass through it linearally, see if we looped
								if (anim_events[i].keyFrame >= first_frame && anim_events[i].keyFrame < lastFrame)
								{
									//keyframe is in this anim
									if (old_frame < anim_events[i].keyFrame
										&& frame < old_frame)
									{
										//old to new passed through keyframe
										match = qtrue;
									}
								}
							}
						}
					}
				}
			}
			if (match)
			{
				switch (anim_events[i].eventType)
				{
				case AEV_SOUNDCHAN:
				case AEV_SOUND:
					// Determine probability of playing sound
					if (!anim_events[i].eventData[AED_SOUND_PROBABILITY]) // 100%
					{
						do_event = qtrue;
					}
					else if (anim_events[i].eventData[AED_SOUND_PROBABILITY] > Q_irand(0, 99))
					{
						do_event = qtrue;
					}
					break;
				case AEV_SABER_SWING:
					// Determine probability of playing sound
					if (!anim_events[i].eventData[AED_SABER_SWING_PROBABILITY]) // 100%
					{
						do_event = qtrue;
					}
					else if (anim_events[i].eventData[AED_SABER_SWING_PROBABILITY] > Q_irand(0, 99))
					{
						do_event = qtrue;
					}
					break;
				case AEV_SABER_SPIN:
					// Determine probability of playing sound
					if (!anim_events[i].eventData[AED_SABER_SPIN_PROBABILITY]) // 100%
					{
						do_event = qtrue;
					}
					else if (anim_events[i].eventData[AED_SABER_SPIN_PROBABILITY] > Q_irand(0, 99))
					{
						do_event = qtrue;
					}
					break;
				case AEV_FOOTSTEP:
					// Determine probability of playing sound
					//Com_Printf( "Footstep event on frame %d, even should be on frame %d, off by %d\n", frame, animEvents[i].keyFrame, frame-animEvents[i].keyFrame );
					if (!anim_events[i].eventData[AED_FOOTSTEP_PROBABILITY]) // 100%
					{
						do_event = qtrue;
					}
					else if (anim_events[i].eventData[AED_FOOTSTEP_PROBABILITY] > Q_irand(0, 99))
					{
						do_event = qtrue;
					}
					break;
				case AEV_EFFECT:
					// Determine probability of playing sound
					if (!anim_events[i].eventData[AED_EFFECT_PROBABILITY]) // 100%
					{
						do_event = qtrue;
					}
					else if (anim_events[i].eventData[AED_EFFECT_PROBABILITY] > Q_irand(0, 99))
					{
						do_event = qtrue;
					}
					break;
				case AEV_FIRE:
					// Determine probability of playing sound
					if (!anim_events[i].eventData[AED_FIRE_PROBABILITY]) // 100%
					{
						do_event = qtrue;
					}
					else if (anim_events[i].eventData[AED_FIRE_PROBABILITY] > Q_irand(0, 99))
					{
						do_event = qtrue;
					}
					break;
				case AEV_MOVE:
					do_event = qtrue;
					break;
				default:
					//doEvent = qfalse;//implicit
					break;
				}
				// do event
				if (do_event)
				{
					CG_PlayerAnimEventDo(&cg_entities[entNum], &anim_events[i]);
				}
			} // end if event matches
		} // end if model matches
	} // end for
}

static void CGG2_AnimEvents(centity_t* cent)
{
	if (!cent || !cent->gent || !cent->gent->client)
	{
		return;
	}
	if (!cent->gent->ghoul2.size())
	{
		//sorry, ghoul2 models only
		return;
	}
	assert(cent->gent->playerModel >= 0 && cent->gent->playerModel < cent->gent->ghoul2.size());
	if (ValidAnimFileIndex(cent->gent->client->clientInfo.animFileIndex))
	{
		int junk, cur_frame = 0;
		float current_frame = 0, animSpeed;

		if (cent->gent->rootBone >= 0 && gi.G2API_GetBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel],
			cent->gent->rootBone, cg.time, &current_frame, &junk,
			&junk, &junk, &animSpeed, cgs.model_draw))
		{
			// the above may have failed, not sure what to do about it, current frame will be zero in that case
			cur_frame = floor(current_frame);
		}
		if (cur_frame != cent->gent->client->renderInfo.legsFrame)
		{
			CG_PlayerAnimEvents(cent->gent->client->clientInfo.animFileIndex, qfalse,
				cent->gent->client->renderInfo.legsFrame, cur_frame, cent->currentState.client_num);
		}
		cent->gent->client->renderInfo.legsFrame = cur_frame;
		cent->pe.legs.frame = cur_frame;

		if (cent->gent->lowerLumbarBone >= 0 && gi.G2API_GetBoneAnimIndex(
			&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->lowerLumbarBone, cg.time, &current_frame, &junk,
			&junk, &junk, &animSpeed, cgs.model_draw))
		{
			cur_frame = floor(current_frame);
		}
		if (cur_frame != cent->gent->client->renderInfo.torsoFrame)
		{
			CG_PlayerAnimEvents(cent->gent->client->clientInfo.animFileIndex, qtrue,
				cent->gent->client->renderInfo.torsoFrame, cur_frame, cent->currentState.client_num);
		}
		cent->gent->client->renderInfo.torsoFrame = cur_frame;
		cent->pe.torso.frame = cur_frame;
	}
}

/*
=============================================================================

PLAYER ANGLES

=============================================================================
*/

/*
==================
CG_UpdateAngleClamp
Turn curAngle toward destAngle at angleSpeed, but stay within clampMin and Max
==================
*/
static void CG_UpdateAngleClamp(const float dest_angle, const float clamp_min, const float clamp_max,
	const float angle_speed, float* cur_angle, const float normal_angle)
{
	float move;

	float swing = AngleSubtract(dest_angle, *cur_angle);

	if (swing == 0)
	{
		//Don't have to turn
		return;
	}

	// modify the angleSpeed depending on the delta
	// so it doesn't seem so linear
	float scale;
	if (swing > 0)
	{
		if (swing < clamp_max * 0.25)
		{
			//Pretty small way to go
			scale = 0.25;
		}
		else if (swing > clamp_max * 2.0)
		{
			//Way out of our range
			scale = 2.0;
		}
		else
		{
			//Scale it smoothly
			scale = swing / clamp_max;
		}
	}
	else // if (swing < 0)
	{
		if (swing > clamp_min * 0.25)
		{
			//Pretty small way to go
			scale = 0.5;
		}
		else if (swing < clamp_min * 2.0)
		{
			//Way out of our range
			scale = 2.0;
		}
		else
		{
			//Scale it smoothly
			scale = swing / clamp_min;
		}
	}

	const float actual_speed = scale * angle_speed;
	// swing towards the destination angle
	if (swing >= 0)
	{
		move = cg.frametime * actual_speed;
		if (move >= swing)
		{
			//our turnspeed is so fast, no need to swing, just match
			*cur_angle = dest_angle;
		}
		else
		{
			*cur_angle = AngleNormalize360(*cur_angle + move);
		}
	}
	else if (swing < 0)
	{
		move = cg.frametime * -actual_speed;
		if (move <= swing)
		{
			//our turnspeed is so fast, no need to swing, just match
			*cur_angle = dest_angle;
		}
		else
		{
			*cur_angle = AngleNormalize180(*cur_angle + move);
		}
	}

	swing = AngleSubtract(*cur_angle, normal_angle);

	// clamp to no more than normalAngle + tolerance
	if (swing > clamp_max)
	{
		*cur_angle = AngleNormalize180(normal_angle + clamp_max);
	}
	else if (swing < clamp_min)
	{
		*cur_angle = AngleNormalize180(normal_angle + clamp_min);
	}
}

/*
==================
CG_SwingAngles

  If the body is not locked OR if the upper part is trying to swing beyond it's
	range, turn the lower body part to catch up.

  Parms:	desired angle,		(Our eventual goal angle
			min swing tolerance,(Lower angle value threshold at which to start turning)
			max swing tolerance,(Upper angle value threshold at which to start turning)
			min clamp tolerance,(Lower angle value threshold to clamp output angle to)
			max clamp tolerance,(Upper angle value threshold to clamp output angle to)
			angle speed,		(How fast to turn)
			current angle,		(Current angle to modify)
			locked mode			(Don't turn unless you exceed the swing/clamp tolerance)
==================
*/
static void CG_SwingAngles(const float dest_angle,
	const float swing_tol_min, const float swing_tol_max,
	const float clamp_min, const float clamp_max,
	const float angle_speed, float* cur_angle,
	qboolean* turning)
{
	float move;

	const float swing = AngleSubtract(dest_angle, *cur_angle);

	if (swing == 0)
	{
		//Don't have to turn
		*turning = qfalse;
	}
	else
	{
		*turning = qtrue;
	}

	//If we're not turning, then we're done
	if (*turning == qfalse)
		return;

	// modify the angleSpeed depending on the delta
	// so it doesn't seem so linear
	float scale = fabs(swing);

	if (swing > 0)
	{
		if (clamp_max <= 0)
		{
			*cur_angle = dest_angle;
			return;
		}

		if (swing < swing_tol_max * 0.5)
		{
			//Pretty small way to go
			scale = 0.5;
		}
		else if (scale < swing_tol_max)
		{
			//More than halfway to go
			scale = 1.0;
		}
		else
		{
			//Way out of our range
			scale = 2.0;
		}
	}
	else // if (swing < 0)
	{
		if (clamp_min >= 0)
		{
			*cur_angle = dest_angle;
			return;
		}

		if (swing > swing_tol_min * 0.5)
		{
			//Pretty small way to go
			scale = 0.5;
		}
		else if (scale > swing_tol_min)
		{
			//More than halfway to go
			scale = 1.0;
		}
		else
		{
			//Way out of our range
			scale = 2.0;
		}
	}

	// swing towards the destination angle
	if (swing >= 0)
	{
		move = cg.frametime * scale * angle_speed;
		if (move >= swing)
		{
			//our turnspeed is so fast, no need to swing, just match
			move = swing;
		}
		*cur_angle = AngleNormalize360(*cur_angle + move);
	}
	else if (swing < 0)
	{
		move = cg.frametime * scale * -angle_speed;
		if (move <= swing)
		{
			//our turnspeed is so fast, no need to swing, just match
			move = swing;
		}
		*cur_angle = AngleNormalize360(*cur_angle + move);
	}

	// clamp to no more than tolerance
	if (swing > clamp_max)
	{
		*cur_angle = AngleNormalize360(dest_angle - (clamp_max - 1));
	}
	else if (swing < clamp_min)
	{
		*cur_angle = AngleNormalize360(dest_angle + (-clamp_min - 1));
	}
}

/*
===============
CG_BreathPuffs
===============
Description: Makes the player appear to have breath puffs (from the cold).
Added 11/06/02 by Aurelio Reis.
*/
extern vmCvar_t cg_drawBreath;

static void CG_BreathPuffs(const centity_t* cent, vec3_t angles, vec3_t origin)
{
	gclient_t* client = cent->gent->client;

	/*	cg_drawBreath.integer	== 0 - Don't draw at all.
								== 1 - Draw both (but bubbles only when under water).
								== 2 - Draw only cold breath.
								== 3 - Draw only under water bubbles (when under water)	*/

	if (!client
		|| cg_drawBreath.integer == 0
		|| !cg.renderingThirdPerson && !(cg_trueguns.integer || client->ps.weapon == WP_MELEE || client->ps.weapon ==
			WP_SABER)
		|| client->ps.pm_type == PM_DEAD
		|| client->breathPuffTime > cg.time)
	{
		return;
	}

	// Get the head-front bolt/tag.
	const int bolt = gi.G2API_AddBolt(&cent->gent->ghoul2[cent->gent->playerModel], "*head_front");
	if (bolt == -1)
	{
		return;
	}

	vec3_t v_effect_origin;
	mdxaBone_t bolt_matrix;
	gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, bolt, &bolt_matrix, angles, origin, cg.time,
		cgs.model_draw, cent->currentState.modelScale);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, v_effect_origin);

	const int contents = cgi_CM_PointContents(v_effect_origin, 0);
	if (contents & (CONTENTS_SLIME | CONTENTS_LAVA)) // If they're submerged in something bad, leave.
	{
		return;
	}

	// Show bubbles effect if we're under water.
	if (contents & CONTENTS_WATER && (cg_drawBreath.integer == 1 || cg_drawBreath.integer == 3))
	{
		CG_PlayEffectBolted("misc/waterbreath", cent->gent->playerModel, bolt, cent->currentState.client_num,
			v_effect_origin);
	}
	// Draw cold breath effect.
	else if (cg_drawBreath.integer == 1 || cg_drawBreath.integer == 2)
	{
		CG_PlayEffectBolted("misc/breath", cent->gent->playerModel, bolt, cent->currentState.client_num,
			v_effect_origin);
	}
	if (gi.VoiceVolume[cent->currentState.number] > 0)
	{
		//make breath when talking
		client->breathPuffTime = cg.time + 300; // every 200 ms
	}
	else
	{
		client->breathPuffTime = cg.time + 3000; // every 3 seconds.
	}
}

constexpr auto LOOK_DEFAULT_SPEED = 0.15f;
constexpr auto LOOK_TALKING_SPEED = 0.15f;

static qboolean CG_CheckLookTarget(centity_t* cent, vec3_t look_angles, float* looking_speed)
{
	if (!cent->gent->ghoul2.size())
	{
		if (!cent->gent->client->clientInfo.torsoModel || !cent->gent->client->clientInfo.headModel)
		{
			return qfalse;
		}
	}

	//FIXME: also clamp the lookAngles based on the clamp + the existing difference between
	//		headAngles and torsoAngles?  But often the tag_torso is straight but the torso itself
	//		is deformed to not face straight... sigh...

	//Now calc head angle to lookTarget, if any
	if (cent->gent->client->renderInfo.lookTarget >= 0 && cent->gent->client->renderInfo.lookTarget < ENTITYNUM_WORLD)
	{
		vec3_t look_dir, look_org = { 0.0f }, eye_org;
		if (cent->gent->client->renderInfo.lookMode == LM_ENT)
		{
			const centity_t* look_cent = &cg_entities[cent->gent->client->renderInfo.lookTarget];
			if (look_cent && look_cent->gent)
			{
				if (look_cent->gent != cent->gent->enemy)
				{
					//We turn heads faster than headbob speed, but not as fast as if watching an enemy
					if (cent->gent->client->NPC_class == CLASS_ROCKETTROOPER)
					{
						//they look around slowly and deliberately
						*looking_speed = LOOK_DEFAULT_SPEED * 0.25f;
					}
					else
					{
						*looking_speed = LOOK_DEFAULT_SPEED;
					}
				}

				//FIXME: Ignore small deltas from current angles so we don't bob our head in synch with theirs?

				if (cent->gent->client->renderInfo.lookTarget == 0 && !cg.renderingThirdPerson)
					//!cg_thirdPerson.integer )
				{
					//Special case- use cg.refdef.vieworg if looking at player and not in third person view
					VectorCopy(cg.refdef.vieworg, look_org);
				}
				else if (look_cent->gent->client)
				{
					VectorCopy(look_cent->gent->client->renderInfo.eyePoint, look_org);
				}
				else if (look_cent->gent->s.pos.trType == TR_INTERPOLATE)
				{
					VectorCopy(look_cent->lerpOrigin, look_org);
				}
				else if (look_cent->gent->inuse && !VectorCompare(look_cent->gent->currentOrigin, vec3_origin))
				{
					VectorCopy(look_cent->gent->currentOrigin, look_org);
				}
				else
				{
					//at origin of world
					return qfalse;
				}
				//Look in dir of lookTarget
			}
		}
		else if (cent->gent->client->renderInfo.lookMode == LM_INTEREST && cent->gent->client->renderInfo.lookTarget > -
			1 && cent->gent->client->renderInfo.lookTarget < MAX_INTEREST_POINTS)
		{
			VectorCopy(level.interestPoints[cent->gent->client->renderInfo.lookTarget].origin, look_org);
		}
		else
		{
			return qfalse;
		}

		VectorCopy(cent->gent->client->renderInfo.eyePoint, eye_org);

		VectorSubtract(look_org, eye_org, look_dir);
#if 1
		vectoangles(look_dir, look_angles);
#else
		//FIXME: get the angle of the head tag and account for that when finding the lookAngles-
		//		so if they're lying on their back we get an accurate lookAngle...
		vec3_t	headDirs[3];
		vec3_t	finalDir;

		AnglesToAxis(cent->gent->client->renderInfo.headAngles, headDirs);
		VectorRotate(lookDir, headDirs, finalDir);
		vectoangles(finalDir, lookAngles);
#endif
		for (int i = 0; i < 3; i++)
		{
			look_angles[i] = AngleNormalize180(look_angles[i]);
			cent->gent->client->renderInfo.eyeAngles[i] =
				AngleNormalize180(cent->gent->client->renderInfo.eyeAngles[i]);
		}
		AnglesSubtract(look_angles, cent->gent->client->renderInfo.eyeAngles, look_angles);
		return qtrue;
	}

	return qfalse;
}

/*
=================
CG_AddHeadBob
=================
*/
static qboolean CG_AddHeadBob(const centity_t* cent, vec3_t add_to)
{
	renderInfo_t* render_info = &cent->gent->client->renderInfo;
	const int volume = gi.VoiceVolume[cent->gent->s.client_num];
	const int vol_change = volume - render_info->lastVoiceVolume; //was *3 because voice fromLA was too low
	int i;

	render_info->lastVoiceVolume = volume;

	if (!volume)
	{
		// Not talking, set our target to be the normal head position
		VectorClear(render_info->targetHeadBobAngles);

		if (VectorLengthSquared(render_info->headBobAngles) < 1.0f)
		{
			// We are close enough to being back to our normal head position, so we are done for now
			return qfalse;
		}
	}
	else if (vol_change > 2)
	{
		// a big positive change in volume
		for (i = 0; i < 3; i++)
		{
			// Move our head angle target a bit
			render_info->targetHeadBobAngles[i] += Q_flrand(-1.0 * vol_change, 1.0 * vol_change);

			// Clamp so we don't get too out of hand
			if (render_info->targetHeadBobAngles[i] > 7.0f)
				render_info->targetHeadBobAngles[i] = 7.0f;

			if (render_info->targetHeadBobAngles[i] < -7.0f)
				render_info->targetHeadBobAngles[i] = -7.0f;
		}
	}

	for (i = 0; i < 3; i++)
	{
		// Always try to move head angles towards our target
		render_info->headBobAngles[i] += (render_info->targetHeadBobAngles[i] - render_info->headBobAngles[i]) * (cg.
			frametime / 150.0f);
		if (add_to)
		{
			add_to[i] = AngleNormalize180(add_to[i] + AngleNormalize180(render_info->headBobAngles[i]));
		}
	}

	// We aren't back to our normal position yet, so we still have to apply headBobAngles
	return qtrue;
}

extern float vectoyaw(const vec3_t vec);

static qboolean CG_PlayerLegsYawFromMovement(const centity_t* cent, const vec3_t velocity, float* yaw,
	const float fwd_angle,
	const float swing_tol_min, const float swing_tol_max,
	const qboolean always_face)
{
	float turn_rate = 10, add_angle = 0;

	//figure out what the offset, if any, should be
	if (velocity[0] || velocity[1])
	{
		const float move_yaw = vectoyaw(velocity);
		add_angle = AngleDelta(cent->lerpAngles[YAW], move_yaw) * -1;
		if (add_angle > 150 || add_angle < -150)
		{
			add_angle = 0;
		}
		else
		{
			//FIXME: use actual swing/clamp tolerances
			if (add_angle > swing_tol_max)
			{
				add_angle = swing_tol_max;
			}
			else if (add_angle < swing_tol_min)
			{
				add_angle = swing_tol_min;
			}
			if (cent->gent->client->ps.pm_flags & PMF_BACKWARDS_RUN)
			{
				add_angle *= -1;
			}
			turn_rate = 5;
		}
	}
	else if (!always_face)
	{
		return qfalse;
	}
	if (cent->gent && cent->gent->client && cent->gent->client->ps.forcePowersActive & 1 << FP_SPEED)
	{
		//using force speed
		//scale up the turning speed
		turn_rate /= cg_timescale.value;
	}
	//lerp the legs angle to the new angle
	float angle_diff = AngleDelta(cent->pe.legs.yawAngle, *yaw + add_angle);
	float new_add_angle = angle_diff * cg.frameInterpolation * -1;
	if (fabs(new_add_angle) > fabs(angle_diff))
	{
		new_add_angle = angle_diff * -1;
	}
	if (new_add_angle > turn_rate)
	{
		new_add_angle = turn_rate;
	}
	else if (new_add_angle < -turn_rate)
	{
		new_add_angle = -turn_rate;
	}
	*yaw = cent->pe.legs.yawAngle + new_add_angle;
	//Now clamp
	angle_diff = AngleDelta(fwd_angle, *yaw);
	if (angle_diff > swing_tol_max)
	{
		*yaw = fwd_angle - swing_tol_max;
	}
	else if (angle_diff < swing_tol_min)
	{
		*yaw = fwd_angle - swing_tol_min;
	}
	return qtrue;
}

static void CG_ATSTLegsYaw(centity_t* cent, vec3_t trailing_legs_angles)
{
	float atst_legs_yaw = cent->lerpAngles[YAW];

	CG_PlayerLegsYawFromMovement(cent, cent->gent->client->ps.velocity, &atst_legs_yaw, cent->lerpAngles[YAW], -60, 60,
		qtrue);

	float leg_angle_diff = AngleNormalize180(atst_legs_yaw) - AngleNormalize180(cent->pe.legs.yawAngle);
	int legs_anim = cent->currentState.legsAnim;
	const auto moving = static_cast<qboolean>(!VectorCompare(cent->gent->client->ps.velocity, vec3_origin));
	if (moving || legs_anim == BOTH_TURN_LEFT1 || legs_anim == BOTH_TURN_RIGHT1 || fabs(leg_angle_diff) > 45)
	{
		//moving or turning or beyond the turn allowance
		if (legs_anim == BOTH_STAND1 && !moving)
		{
			//standing
			if (leg_angle_diff > 0)
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_LEFT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_RIGHT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		else if (legs_anim == BOTH_STAND9 && !moving)
		{
			//standing
			if (leg_angle_diff > 0)
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_LEFT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_RIGHT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		else if (legs_anim == BOTH_STANDYODA_STICK && !moving)
		{
			//standing
			if (leg_angle_diff > 0)
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_LEFT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_RIGHT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		else if (legs_anim == BOTH_STAND_SABER_ON && !moving)
		{
			//standing
			if (leg_angle_diff > 0)
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_LEFT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_RIGHT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		else if (legs_anim == BOTH_STAND_SABER_ON_DUELS && !moving)
		{
			//standing
			if (leg_angle_diff > 0)
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_LEFT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_RIGHT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		else if (legs_anim == BOTH_STAND_SABER_ON_STAFF && !moving)
		{
			//standing
			if (leg_angle_diff > 0)
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_LEFT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else
			{
				NPC_SetAnim(cent->gent, SETANIM_LEGS, BOTH_TURN_RIGHT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		else if (legs_anim == BOTH_TURN_LEFT1 || legs_anim == BOTH_TURN_RIGHT1)
		{
			//turning
			leg_angle_diff = AngleSubtract(atst_legs_yaw, cent->gent->client->renderInfo.legsYaw);
			constexpr float add = 0;
			if (leg_angle_diff > 50)
			{
				cent->pe.legs.yawAngle += leg_angle_diff - 50;
			}
			else if (leg_angle_diff < -50)
			{
				cent->pe.legs.yawAngle += leg_angle_diff + 50;
			}
			const float anim_length = PM_AnimLength(cent->gent->client->clientInfo.animFileIndex,
				static_cast<animNumber_t>(legs_anim));
			leg_angle_diff *= (anim_length - cent->gent->client->ps.legsAnimTimer) / anim_length;
			VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle + leg_angle_diff + add, 0);
			if (!cent->gent->client->ps.legsAnimTimer)
			{
				//FIXME: if start turning in the middle of this, our legs pop back to the old cent->pe.legs.yawAngle...
				cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
			}
		}
		else
		{
			//moving
			leg_angle_diff = AngleSubtract(atst_legs_yaw, cent->pe.legs.yawAngle);
			//FIXME: frame rate dependent!!!
			if (leg_angle_diff > 50)
			{
				leg_angle_diff -= 50;
			}
			else if (leg_angle_diff > 5)
			{
				leg_angle_diff = 5;
			}
			else if (leg_angle_diff < -50)
			{
				leg_angle_diff += 50;
			}
			else if (leg_angle_diff < -5)
			{
				leg_angle_diff = -5;
			}
			leg_angle_diff *= cg.frameInterpolation;
			VectorSet(trailing_legs_angles, 0, AngleNormalize180(cent->pe.legs.yawAngle + leg_angle_diff), 0);
			cent->gent->client->renderInfo.legsYaw = trailing_legs_angles[YAW];
		}
		cent->gent->client->renderInfo.legsYaw = cent->pe.legs.yawAngle = trailing_legs_angles[YAW];
		cent->pe.legs.yawing = qtrue;
	}
	else
	{
		VectorSet(trailing_legs_angles, 0, cent->pe.legs.yawAngle, 0);
		cent->gent->client->renderInfo.legsYaw = cent->pe.legs.yawAngle = trailing_legs_angles[YAW];
		cent->pe.legs.yawing = qfalse;
	}
}

extern qboolean G_ClassHasBadBones(int npc_class);
extern void G_BoneOrientationsForClass(int npc_class, const char* boneName, Eorientations* o_up, Eorientations* o_rt,
	Eorientations* o_fwd);
extern qboolean PM_FlippingAnim(int anim);
extern qboolean PM_SpinningSaberAnim(int anim);
static CGhoul2Info_v dummyGhoul2;
static int dummyRootBone;
static int dummyHipsBolt;

static void CG_G2ClientSpineAngles(centity_t* cent, vec3_t view_angles, const vec3_t angles, vec3_t thoracic_angles,
	vec3_t ul_angles, vec3_t ll_angles)
{
	vec3_t motion_bone_correct_angles = { 0 };
	cent->pe.torso.pitchAngle = view_angles[PITCH];
	view_angles[YAW] = AngleDelta(cent->lerpAngles[YAW], angles[YAW]);
	cent->pe.torso.yawAngle = view_angles[YAW];

	if (cent->gent->client->NPC_class == CLASS_SABER_DROID)
	{
		//don't use lower bones
		VectorClear(thoracic_angles);
		VectorClear(ul_angles);
		VectorClear(ll_angles);
		return;
	}

	if (cg_motionBoneComp.integer
		&& !PM_FlippingAnim(cent->currentState.legsAnim)
		&& !PM_SpinningSaberAnim(cent->currentState.legsAnim)
		&& !PM_SpinningSaberAnim(cent->currentState.torsoAnim)
		&& cent->currentState.legsAnim != cent->currentState.torsoAnim
		//NOTE: presumes your legs & torso are on the same frame, though they *should* be because PM_SetAnimFinal tries to keep them in synch
		&& !G_ClassHasBadBones(cent->gent->client->NPC_class))
		//these guys' bones are so fucked up we shouldn't even bother with this motion bone comp...
	{
		//FIXME: no need to do this if legs and torso on are same frame
		mdxaBone_t bolt_matrix;

		if (cg_motionBoneComp.integer > 2 && cent->gent->rootBone >= 0 && cent->gent->lowerLumbarBone >= 0)
		{
			//expensive version
			//have a local ghoul2 instance to mess with for this stuff... :/
			//remember the frame the lower is on
			float upper_frame, animSpeed;
			int junk;
			vec3_t ll_fwd, ll_rt, dest_p_angles, cur_p_angles, temp_ang;

			if (!dummyGhoul2.size())
			{
				//set it up
				const int dummy_h_model = cgi_R_RegisterModel("models/players/_humanoid/_humanoid.glm");

				gi.G2API_InitGhoul2Model(dummyGhoul2, "models/players/_humanoid/_humanoid.glm", dummy_h_model,
					NULL_HANDLE, NULL_HANDLE, 0, 0);
				dummyRootBone = gi.G2API_GetBoneIndex(&dummyGhoul2[0], "model_root", qtrue);
				dummyHipsBolt = gi.G2API_AddBolt(&dummyGhoul2[0], "pelvis");
			}

			gi.G2API_GetBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->lowerLumbarBone,
				cg.time, &upper_frame, &junk, &junk, &junk, &animSpeed, cgs.model_draw);
			//set the dummyGhoul2 lower body to same frame as upper
			gi.G2API_SetBoneAnimIndex(&dummyGhoul2[0], dummyRootBone, upper_frame, upper_frame,
				BONE_ANIM_OVERRIDE_FREEZE,
				1, cg.time, upper_frame, 0);
			//get the dummyGhoul2 lower_lumbar orientation
			gi.G2API_GetBoltMatrix(dummyGhoul2, 0, dummyHipsBolt, &bolt_matrix, vec3_origin, vec3_origin, cg.time,
				cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Z, ll_fwd);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ll_rt);
			vectoangles(ll_fwd, dest_p_angles);
			vectoangles(ll_rt, temp_ang);
			dest_p_angles[ROLL] = -temp_ang[PITCH];
			//get my lower_lumbar
			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->crotchBolt, &bolt_matrix,
				vec3_origin, vec3_origin, cg.time, cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Z, ll_fwd);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ll_rt);
			vectoangles(ll_fwd, cur_p_angles);
			vectoangles(ll_rt, temp_ang);
			cur_p_angles[ROLL] = -temp_ang[PITCH];

			//get the difference
			for (int ang = 0; ang < 3; ang++)
			{
				motion_bone_correct_angles[ang] = AngleNormalize180(
					AngleDelta(AngleNormalize180(dest_p_angles[ang]), AngleNormalize180(cur_p_angles[ang])));
			}
#ifdef _DEBUG
			Com_Printf("motion bone correction:  %4.2f %4.2f %4.2f\n", motion_bone_correct_angles[PITCH],
				motion_bone_correct_angles[YAW], motion_bone_correct_angles[ROLL]);
#endif// _DEBUG
		}
		else
		{
			//adjust for motion offset
			vec3_t motion_fwd, motion_angles;

			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->motionBolt, &bolt_matrix,
				vec3_origin, cent->lerpOrigin, cg.time, cgs.model_draw,
				cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, motion_fwd);
			vectoangles(motion_fwd, motion_angles);
			if (cg_motionBoneComp.integer > 1)
			{
				//do roll, too
				vec3_t motion_rt, temp_ang;
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_X, motion_rt);
				vectoangles(motion_rt, temp_ang);
				motion_angles[ROLL] = -temp_ang[PITCH];
			}

			for (int ang = 0; ang < 3; ang++)
			{
				view_angles[ang] = AngleNormalize180(view_angles[ang] - AngleNormalize180(motion_angles[ang]));
			}
		}
	}
	//distribute the angles differently up the spine
	//NOTE: each of these distributions must add up to 1.0f
	if (cent->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
	{
		//only uses lower_lumbar and upper_lumbar to look around
		VectorClear(thoracic_angles);
		ul_angles[PITCH] = view_angles[PITCH] * 0.50f;
		ll_angles[PITCH] = view_angles[PITCH] * 0.50f + motion_bone_correct_angles[PITCH];

		ul_angles[YAW] = view_angles[YAW] * 0.45f;
		ll_angles[YAW] = view_angles[YAW] * 0.55f + motion_bone_correct_angles[YAW];

		ul_angles[ROLL] = view_angles[ROLL] * 0.45f;
		ll_angles[ROLL] = view_angles[ROLL] * 0.55f + motion_bone_correct_angles[ROLL];
	}
	else if (cent->gent->client->NPC_class == CLASS_ASSASSIN_DROID)
	{
		//each bone has only 1 axis of rotation!
		//upper lumbar does not pitch
		thoracic_angles[PITCH] = view_angles[PITCH] * 0.40f;
		ul_angles[PITCH] = 0.0f;
		ll_angles[PITCH] = view_angles[PITCH] * 0.60f + motion_bone_correct_angles[PITCH];
		//only upper lumbar yaws
		thoracic_angles[YAW] = 0.0f;
		ul_angles[YAW] = view_angles[YAW];
		ll_angles[YAW] = motion_bone_correct_angles[YAW];
		//no bone is capable of rolling
		thoracic_angles[ROLL] = 0.0f;
		ul_angles[ROLL] = 0.0f;
		ll_angles[ROLL] = motion_bone_correct_angles[ROLL];
	}
	else if (cent->gent->client->NPC_class == CLASS_DROIDEKA)
	{
		//each bone has only 1 axis of rotation!
		//upper lumbar does not pitch
		thoracic_angles[PITCH] = view_angles[PITCH] * 0.40f;
		ul_angles[PITCH] = 0.0f;
		ll_angles[PITCH] = view_angles[PITCH] * 0.60f + motion_bone_correct_angles[PITCH];
		//only upper lumbar yaws
		thoracic_angles[YAW] = 0.0f;
		ul_angles[YAW] = view_angles[YAW];
		ll_angles[YAW] = motion_bone_correct_angles[YAW];
		//no bone is capable of rolling
		thoracic_angles[ROLL] = 0.0f;
		ul_angles[ROLL] = 0.0f;
		ll_angles[ROLL] = motion_bone_correct_angles[ROLL];
	}
	else
	{
		//use all 3 bones
		if (PM_InLedgeMove(cent->gent->client->ps.legsAnim))
		{
			//lock spine to animation
			thoracic_angles[PITCH] = 0;
			ll_angles[PITCH] = 0;
			ul_angles[PITCH] = 0;
		}
		else
		{
			thoracic_angles[PITCH] = view_angles[PITCH] * 0.20f;
			ul_angles[PITCH] = view_angles[PITCH] * 0.40f;
			ll_angles[PITCH] = view_angles[PITCH] * 0.40f + motion_bone_correct_angles[PITCH];
		}

		thoracic_angles[YAW] = view_angles[YAW] * 0.20f;
		ul_angles[YAW] = view_angles[YAW] * 0.35f;
		ll_angles[YAW] = view_angles[YAW] * 0.45f + motion_bone_correct_angles[YAW];

		thoracic_angles[ROLL] = view_angles[ROLL] * 0.20f;
		ul_angles[ROLL] = view_angles[ROLL] * 0.35f;
		ll_angles[ROLL] = view_angles[ROLL] * 0.45f + motion_bone_correct_angles[ROLL];
	}

	if (G_IsRidingVehicle(cent->gent)) // && type == VH_SPEEDER ?
	{
		//aim torso forward too
		ul_angles[YAW] = ll_angles[YAW] = 0;

		// Only if they have weapon can they pitch forward/back.
		if (cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_SABER)
		{
			ul_angles[PITCH] = ll_angles[PITCH] = 0;
		}
	}
	//thoracic is added modified again by neckAngle calculations, so don't set it until then
	if (G_ClassHasBadBones(cent->gent->client->NPC_class))
	{
		Eorientations o_up, o_rt, o_fwd;
		if (cent->gent->client->NPC_class == CLASS_RANCOR)
		{
			ll_angles[YAW] = ll_angles[ROLL] = 0.0f;
			ul_angles[YAW] = ul_angles[ROLL] = 0.0f;
		}
		G_BoneOrientationsForClass(cent->gent->client->NPC_class, "upper_lumbar", &o_up, &o_rt, &o_fwd);
		BG_G2SetBoneAngles(cent, cent->gent->upperLumbarBone, ul_angles, BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd,
			cgs.model_draw);
		G_BoneOrientationsForClass(cent->gent->client->NPC_class, "lower_lumbar", &o_up, &o_rt, &o_fwd);
		BG_G2SetBoneAngles(cent, cent->gent->lowerLumbarBone, ll_angles, BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd,
			cgs.model_draw);
	}
	else
	{
		BG_G2SetBoneAngles(cent, cent->gent->upperLumbarBone, ul_angles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->lowerLumbarBone, ll_angles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
	}
}

static void CG_G2ClientNeckAngles(const centity_t* cent, const vec3_t look_angles, vec3_t head_angles,
	vec3_t neck_angles,
	vec3_t thoracic_angles, vec3_t head_clamp_min_angles, vec3_t head_clamp_max_angles)
{
	if (cent->gent->client->NPC_class == CLASS_HAZARD_TROOPER)
	{
		//don't use upper bones
		return;
	}
	vec3_t l_a;
	VectorCopy(look_angles, l_a);
	//clamp the headangles (which should now be relative to the cervical (neck) angles
	if (l_a[PITCH] < head_clamp_min_angles[PITCH])
	{
		l_a[PITCH] = head_clamp_min_angles[PITCH];
	}
	else if (l_a[PITCH] > head_clamp_max_angles[PITCH])
	{
		l_a[PITCH] = head_clamp_max_angles[PITCH];
	}

	if (l_a[YAW] < head_clamp_min_angles[YAW])
	{
		l_a[YAW] = head_clamp_min_angles[YAW];
	}
	else if (l_a[YAW] > head_clamp_max_angles[YAW])
	{
		l_a[YAW] = head_clamp_max_angles[YAW];
	}

	if (l_a[ROLL] < head_clamp_min_angles[ROLL])
	{
		l_a[ROLL] = head_clamp_min_angles[ROLL];
	}
	else if (l_a[ROLL] > head_clamp_max_angles[ROLL])
	{
		l_a[ROLL] = head_clamp_max_angles[ROLL];
	}

	//split it up between the neck and cranium
	if (cent->gent->client->NPC_class == CLASS_ASSASSIN_DROID)
	{
		//each bone has only 1 axis of rotation!
		//thoracic only pitches, split with cervical
		if (thoracic_angles[PITCH])
		{
			//already been set above, blend them
			thoracic_angles[PITCH] = (thoracic_angles[PITCH] + l_a[PITCH] * 0.5f) * 0.5f;
		}
		else
		{
			thoracic_angles[PITCH] = l_a[PITCH] * 0.5f;
		}
		thoracic_angles[YAW] = thoracic_angles[ROLL] = 0.0f;
		//cervical only pitches, split with thoracis
		neck_angles[PITCH] = l_a[PITCH] * 0.5f;
		neck_angles[YAW] = 0.0f;
		neck_angles[ROLL] = 0.0f;
		//cranium only yaws
		head_angles[PITCH] = 0.0f;
		head_angles[YAW] = l_a[YAW];
		head_angles[ROLL] = 0.0f;
		//no bones roll
	}
	else if (cent->gent->client->NPC_class == CLASS_DROIDEKA)
	{
		//each bone has only 1 axis of rotation!
		//thoracic only pitches, split with cervical
		if (thoracic_angles[PITCH])
		{
			//already been set above, blend them
			thoracic_angles[PITCH] = (thoracic_angles[PITCH] + l_a[PITCH] * 0.5f) * 0.5f;
		}
		else
		{
			thoracic_angles[PITCH] = l_a[PITCH] * 0.5f;
		}
		thoracic_angles[YAW] = thoracic_angles[ROLL] = 0.0f;
		//cervical only pitches, split with thoracis
		neck_angles[PITCH] = l_a[PITCH] * 0.5f;
		neck_angles[YAW] = 0.0f;
		neck_angles[ROLL] = 0.0f;
		//cranium only yaws
		head_angles[PITCH] = 0.0f;
		head_angles[YAW] = l_a[YAW];
		head_angles[ROLL] = 0.0f;
		//no bones roll
	}
	else if (cent->gent->client->NPC_class == CLASS_SABER_DROID)
	{
		//each bone has only 1 axis of rotation!
		//no thoracic
		VectorClear(thoracic_angles);
		//cervical only yaws
		neck_angles[PITCH] = 0.0f;
		neck_angles[YAW] = l_a[YAW];
		neck_angles[ROLL] = 0.0f;
		//cranium only pitches
		head_angles[PITCH] = l_a[PITCH];
		head_angles[YAW] = 0.0f;
		head_angles[ROLL] = 0.0f;
		//none of the bones roll
	}
	else
	{
		if (PM_InLedgeMove(cent->gent->client->ps.legsAnim))
		{
			//lock arm parent bone to animation
			thoracic_angles[PITCH] = 0;
		}
		else if (thoracic_angles[PITCH])
		{
			//already been set above, blend them
			thoracic_angles[PITCH] = (thoracic_angles[PITCH] + l_a[PITCH] * 0.4f) * 0.5f;
		}
		else
		{
			thoracic_angles[PITCH] = l_a[PITCH] * 0.4f;
		}
		if (thoracic_angles[YAW])
		{
			//already been set above, blend them
			thoracic_angles[YAW] = (thoracic_angles[YAW] + l_a[YAW] * 0.1f) * 0.5f;
		}
		else
		{
			thoracic_angles[YAW] = l_a[YAW] * 0.1f;
		}
		if (thoracic_angles[ROLL])
		{
			//already been set above, blend them
			thoracic_angles[ROLL] = (thoracic_angles[ROLL] + l_a[ROLL] * 0.1f) * 0.5f;
		}
		else
		{
			thoracic_angles[ROLL] = l_a[ROLL] * 0.1f;
		}

		if (PM_InLedgeMove(cent->gent->client->ps.legsAnim))
		{
			//lock the neckAngles to prevent the head from acting weird
			VectorClear(neck_angles);
			VectorClear(head_angles);
		}
		else
		{
			neck_angles[PITCH] = l_a[PITCH] * 0.2f;
			neck_angles[YAW] = l_a[YAW] * 0.3f;
			neck_angles[ROLL] = l_a[ROLL] * 0.3f;

			head_angles[PITCH] = l_a[PITCH] * 0.4f;
			head_angles[YAW] = l_a[YAW] * 0.6f;
			head_angles[ROLL] = l_a[ROLL] * 0.6f;
		}
	}

	if (G_IsRidingVehicle(cent->gent)) // && type == VH_SPEEDER ?
	{
		//aim torso forward too
		head_angles[YAW] = neck_angles[YAW] = thoracic_angles[YAW] = 0;

		// Only if they have weapon can they pitch forward/back.
		if (cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_SABER)
		{
			thoracic_angles[PITCH] = 0;
		}
	}
	if (G_ClassHasBadBones(cent->gent->client->NPC_class))
	{
		Eorientations o_up, o_rt, o_fwd;
		if (cent->gent->client->NPC_class != CLASS_RANCOR)
		{
			//Rancor doesn't use cranium and cervical
			G_BoneOrientationsForClass(cent->gent->client->NPC_class, "cranium", &o_up, &o_rt, &o_fwd);
			BG_G2SetBoneAngles(cent, cent->gent->craniumBone, head_angles, BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd,
				cgs.model_draw);
			G_BoneOrientationsForClass(cent->gent->client->NPC_class, "cervical", &o_up, &o_rt, &o_fwd);
			BG_G2SetBoneAngles(cent, cent->gent->cervicalBone, neck_angles, BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd,
				cgs.model_draw);
		}
		if (cent->gent->client->NPC_class != CLASS_SABER_DROID)
		{
			//saber droid doesn't use thoracic
			if (cent->gent->client->NPC_class == CLASS_RANCOR)
			{
				thoracic_angles[YAW] = thoracic_angles[ROLL] = 0.0f;
			}
			G_BoneOrientationsForClass(cent->gent->client->NPC_class, "thoracic", &o_up, &o_rt, &o_fwd);
			BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, thoracic_angles, BONE_ANGLES_POSTMULT, o_up, o_rt,
				o_fwd, cgs.model_draw);
		}
	}
	else
	{
		BG_G2SetBoneAngles(cent, cent->gent->craniumBone, head_angles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->cervicalBone, neck_angles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, thoracic_angles, BONE_ANGLES_POSTMULT, POSITIVE_X,
			NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
	}
}

static void CG_UpdateLookAngles(const centity_t* cent, vec3_t look_angles, const float look_speed,
	const float min_pitch, const float max_pitch,
	const float min_yaw, const float max_yaw, const float min_roll, const float max_roll)
{
	if (!cent || !cent->gent || !cent->gent->client)
	{
		return;
	}
	if (cent->gent->client->renderInfo.lookingDebounceTime > cg.time)
	{
		//clamp so don't get "Exorcist" effect
		if (look_angles[PITCH] > max_pitch)
		{
			look_angles[PITCH] = max_pitch;
		}
		else if (look_angles[PITCH] < min_pitch)
		{
			look_angles[PITCH] = min_pitch;
		}
		if (look_angles[YAW] > max_yaw)
		{
			look_angles[YAW] = max_yaw;
		}
		else if (look_angles[YAW] < min_yaw)
		{
			look_angles[YAW] = min_yaw;
		}
		if (look_angles[ROLL] > max_roll)
		{
			look_angles[ROLL] = max_roll;
		}
		else if (look_angles[ROLL] < min_roll)
		{
			look_angles[ROLL] = min_roll;
		}

		//slowly lerp to this new value
		//Remember last headAngles
		vec3_t old_look_angles;
		VectorCopy(cent->gent->client->renderInfo.lastHeadAngles, old_look_angles);
		vec3_t look_angles_diff;
		VectorSubtract(look_angles, old_look_angles, look_angles_diff);

		for (float& ang : look_angles_diff)
		{
			ang = AngleNormalize180(ang);
		}

		if (VectorLengthSquared(look_angles_diff))
		{
			look_angles[PITCH] = AngleNormalize180(
				old_look_angles[PITCH] + look_angles_diff[PITCH] * cg.frameInterpolation * look_speed);
			look_angles[YAW] = AngleNormalize180(
				old_look_angles[YAW] + look_angles_diff[YAW] * cg.frameInterpolation * look_speed);
			look_angles[ROLL] = AngleNormalize180(
				old_look_angles[ROLL] + look_angles_diff[ROLL] * cg.frameInterpolation * look_speed);
		}
	}
	//Remember current lookAngles next time
	VectorCopy(look_angles, cent->gent->client->renderInfo.lastHeadAngles);
}

/*
===============
CG_PlayerAngles

Handles seperate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso

===============
*/
extern int PM_TurnAnimForLegsAnim(const gentity_t* gent, int anim);
extern float pm_get_time_scale_mod(const gentity_t* gent);

static void CG_G2PlayerAngles(centity_t* cent, vec3_t legs[], vec3_t angles)
{
	vec3_t thoracic_angles = { 0, 0, 0 };
	vec3_t look_angles, view_angles;
	float look_angle_speed = LOOK_TALKING_SPEED; //shut up the compiler
	qboolean looking;

	if (cent->gent
		&& (cent->gent->flags & FL_NO_ANGLES
			|| cent->gent->client && cent->gent->client->ps.stasisTime > cg.time))
	{
		//flatten out all bone angles we might have been overriding
		cent->lerpAngles[PITCH] = cent->lerpAngles[ROLL] = 0;
		VectorCopy(cent->lerpAngles, angles);

		BG_G2SetBoneAngles(cent, cent->gent->craniumBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->cervicalBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);

		cent->pe.torso.pitchAngle = 0;
		cent->pe.torso.yawAngle = 0;
		BG_G2SetBoneAngles(cent, cent->gent->upperLumbarBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->lowerLumbarBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);

		cent->pe.legs.pitchAngle = angles[0];
		cent->pe.legs.yawAngle = angles[1];
		if (cent->gent->client)
		{
			cent->gent->client->renderInfo.legsYaw = angles[1];
		}
		AnglesToAxis(angles, legs);
		return;
	}
	if (cent->gent
		&& (cent->gent->flags & FL_NO_ANGLES
			|| cent->gent->client && cent->gent->client->ps.stasisJediTime > cg.time))
	{
		//flatten out all bone angles we might have been overriding
		cent->lerpAngles[PITCH] = cent->lerpAngles[ROLL] = 0;
		VectorCopy(cent->lerpAngles, angles);

		BG_G2SetBoneAngles(cent, cent->gent->craniumBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->cervicalBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);

		cent->pe.torso.pitchAngle = 0;
		cent->pe.torso.yawAngle = 0;
		BG_G2SetBoneAngles(cent, cent->gent->upperLumbarBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->lowerLumbarBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);

		cent->pe.legs.pitchAngle = angles[0];
		cent->pe.legs.yawAngle = angles[1];
		if (cent->gent->client)
		{
			cent->gent->client->renderInfo.legsYaw = angles[1];
		}
		AnglesToAxis(angles, legs);
		return;
	}
	// Dead entity
	if (cent->gent && cent->gent->health <= 0)
	{
		if (cent->gent->hipsBone != -1)
		{
			gi.G2API_StopBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->hipsBone);
		}

		VectorCopy(cent->lerpAngles, angles);

		BG_G2SetBoneAngles(cent, cent->gent->craniumBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->cervicalBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);

		cent->pe.torso.pitchAngle = 0;
		cent->pe.torso.yawAngle = 0;
		BG_G2SetBoneAngles(cent, cent->gent->upperLumbarBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);
		BG_G2SetBoneAngles(cent, cent->gent->lowerLumbarBone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, cgs.model_draw);

		cent->pe.legs.pitchAngle = angles[0];
		cent->pe.legs.yawAngle = angles[1];
		if (cent->gent->client)
		{
			cent->gent->client->renderInfo.legsYaw = angles[1];
		}
		AnglesToAxis(angles, legs);
		return;
	}

	if (cent->gent && cent->gent->client
		&& cent->gent->client->NPC_class != CLASS_GONK
		&& cent->gent->client->NPC_class != CLASS_INTERROGATOR
		&& cent->gent->client->NPC_class != CLASS_SENTRY
		&& cent->gent->client->NPC_class != CLASS_PROBE
		&& cent->gent->client->NPC_class != CLASS_R2D2
		&& cent->gent->client->NPC_class != CLASS_R5D2
		&& (cent->gent->client->NPC_class != CLASS_ATST || !cent->gent->s.number))
	{
		// If we are rendering third person, we should just force the player body to always fully face
		//	whatever way they are looking, otherwise, you can end up with gun shots coming off of the
		//	gun at angles that just look really wrong.

		//NOTENOTE: shots are coming out of the gun at ridiculous angles. The head & torso
		//should pitch *some* when looking up and down...
		VectorCopy(cent->lerpAngles, angles);
		angles[PITCH] = 0;

		if (cent->gent->client)
		{
			if (cent->gent->client->NPC_class != CLASS_ATST)
			{
				if (!PM_SpinningSaberAnim(cent->currentState.legsAnim))
				{
					//don't turn legs if in a spinning saber transition
					//FIXME: use actual swing/clamp tolerances?
					if (cent->gent->client->ps.groundEntityNum != ENTITYNUM_NONE && !PM_InRoll(&cent->gent->client->ps))
					{
						//on the ground
						CG_PlayerLegsYawFromMovement(cent, cent->gent->client->ps.velocity, &angles[YAW],
							cent->lerpAngles[YAW], -60, 60, qtrue);
					}
					else
					{
						//face legs to front
						CG_PlayerLegsYawFromMovement(cent, vec3_origin, &angles[YAW], cent->lerpAngles[YAW], -60, 60,
							qtrue);
					}
				}
			}
		}

		//VectorClear( viewAngles );
		VectorCopy(cent->lerpAngles, view_angles);
		view_angles[YAW] = view_angles[ROLL] = 0;
		if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_RANCOR)
		{
			//rancor uses full pitch
			if (cent->gent->count)
			{
				//don't look up or down at enemy when he's in your hand...
				view_angles[PITCH] = 0.0f;
			}
			else if (cent->gent->enemy)
			{
				if (cent->gent->enemy->s.solid == SOLID_BMODEL)
				{
					//don't look up or down at architecture?
					view_angles[PITCH] = 0.0f;
				}
				else if (cent->gent->client->ps.legsAnim == BOTH_MELEE1)
				{
					//don't look up or down when smashing the ground
					view_angles[PITCH] = 0.0f;
				}
				else
				{
					vec3_t e_dir, e_angles, look_from;
					VectorCopy(cent->lerpOrigin, look_from);
					look_from[2] += cent->gent->maxs[2] * 0.6f;
					VectorSubtract(cg_entities[cent->gent->enemy->s.number].lerpOrigin, look_from, e_dir);
					vectoangles(e_dir, e_angles);
					view_angles[PITCH] = AngleNormalize180(e_angles[0]);
					if (cent->gent->client->ps.legsAnim == BOTH_ATTACK2)
					{
						//swinging at something on the ground
						if (view_angles[PITCH] > 0.0f)
						{
							//don't look down
							view_angles[PITCH] = 0.0f;
						}
					}
					else if (cent->gent->client->ps.legsAnim == BOTH_ATTACK4)
					{
						//in breath attack anim
						if (view_angles[PITCH] > 0.0f)
						{
							//don't look down
							view_angles[PITCH] = 0.0f;
						}
						else
						{
							//exaggerate looking up
							view_angles[PITCH] *= 2.0f;
						}
					}
					else if (view_angles[PITCH] > 0.0f)
					{
						//reduce looking down
						view_angles[PITCH] *= 0.5f;
					}
				}
			}
			else
			{
				view_angles[PITCH] = 0.0f;
			}
		}
		else
		{
			view_angles[PITCH] *= 0.5;
		}
		VectorCopy(view_angles, look_angles);

		if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST)
		{
			look_angles[YAW] = 0;
			BG_G2SetBoneAngles(cent, cent->gent->craniumBone, look_angles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
				NEGATIVE_Z, cgs.model_draw);
			VectorCopy(view_angles, look_angles);
		}
		else
		{
			vec3_t ll_angles;
			vec3_t ul_angles;
			if (cg_turnAnims.integer && !in_camera && cent->gent->hipsBone >= 0)
			{
				//override the hips bone with a turn anim when turning
				//and clear it when we're not... does blend from and to parent actually work?
				int startFrame, endFrame;
				const qboolean animating_hips = gi.G2API_GetAnimRangeIndex(
					&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->hipsBone, &startFrame, &endFrame);

				//FIXME: make legs lag behind when turning in place, only play turn anim when legs have to catch up
				if (angles[YAW] == cent->pe.legs.yawAngle)
				{
					gi.G2API_StopBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->hipsBone);
				}
				else if (VectorCompare(vec3_origin, cent->gent->client->ps.velocity))
				{
					//FIXME: because of LegsYawFromMovement, we play the turnAnims when we stop running, which looks really bad.
					const int turn_anim = PM_TurnAnimForLegsAnim(cent->gent, cent->gent->client->ps.legsAnim);
					if (turn_anim != -1 && cent->gent->health > 0)
					{
						const animation_t* animations = level.knownAnimFileSets[cent->gent->client->clientInfo.
							animFileIndex].animations;

						if (!animating_hips || animations[turn_anim].firstFrame != startFrame)
							// only set the anim if we aren't going to do the same animation again
						{
							const float animSpeed = 50.0f / animations[turn_anim].frameLerp * pm_get_time_scale_mod(
								cent->gent);

							gi.G2API_SetBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel],
								cent->gent->hipsBone,
								animations[turn_anim].firstFrame,
								animations[turn_anim].firstFrame + animations[turn_anim].
								numFrames,
								BONE_ANIM_OVERRIDE_LOOP
								/*|BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND*/, animSpeed,
								cg.time, -1, 100);
						}
					}
					else
					{
						gi.G2API_StopBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->hipsBone);
					}
				}
				else
				{
					gi.G2API_StopBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->hipsBone);
				}
			}

			CG_G2ClientSpineAngles(cent, view_angles, angles, thoracic_angles, ul_angles, ll_angles);
		}

		vec3_t trailing_legs_angles;
		if (cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST)
		{
			CG_ATSTLegsYaw(cent, trailing_legs_angles);
			AnglesToAxis(trailing_legs_angles, legs);
			angles[YAW] = trailing_legs_angles[YAW];
		}
		// either riding a vehicle or we are a vehicle
		if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_VEHICLE)
		{
			//you are a vehicle, just use your lerpAngles which comes from m_vOrientation
			cent->pe.legs.yawing = qfalse;
			cent->pe.legs.yawAngle = cent->lerpAngles[YAW];
			if (cent->gent->client)
			{
				cent->gent->client->renderInfo.legsYaw = cent->lerpAngles[YAW];
			}
			AnglesToAxis(cent->lerpAngles, legs);
			if (cent->gent->m_pVehicle)
			{
				if (cent->gent->m_pVehicle->m_pVehicleInfo)
				{
					if (cent->gent->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER
						|| cent->gent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER)
					{
						VectorCopy(cent->lerpAngles, angles);
					}
				}
			}
		}
		else if (G_IsRidingVehicle(cent->gent))
		{
			//riding a vehicle, get the vehicle's lerpAngles (which comes from m_vOrientation)
			cent->pe.legs.yawing = qfalse;
			cent->pe.legs.yawAngle = cg_entities[cent->gent->owner->s.number].lerpAngles[YAW];
			if (cent->gent->client)
			{
				cent->gent->client->renderInfo.legsYaw = cg_entities[cent->gent->owner->s.number].lerpAngles[YAW];
			}
			AnglesToAxis(cg_entities[cent->gent->owner->s.number].lerpAngles, legs);
		}
		else
		{
			//set the legs.yawing field so we play the turning anim when turning in place
			if (angles[YAW] == cent->pe.legs.yawAngle)
			{
				cent->pe.legs.yawing = qfalse;
			}
			else
			{
				cent->pe.legs.yawing = qtrue;
			}
			cent->pe.legs.yawAngle = angles[YAW];
			if (cent->gent->client)
			{
				cent->gent->client->renderInfo.legsYaw = angles[YAW];
			}
			if ((cent->gent->client->ps.eFlags & EF_FORCE_GRIPPED || cent->gent->client->ps.eFlags & EF_FORCE_GRASPED
				|| (cent->gent->client->NPC_class == CLASS_BOBAFETT || cent->gent->client->NPC_class ==
					CLASS_MANDALORIAN || cent->gent->client->NPC_class == CLASS_JANGO || cent->gent->client->
					NPC_class == CLASS_JANGODUAL || cent->gent->client->NPC_class == CLASS_ROCKETTROOPER) &&
				cent->
				gent->client->moveType == MT_FLYSWIM)
				&& cent->gent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				vec3_t cent_fwd, cent_rt;
				float div_factor = 1.0f;
				if ((cent->gent->client->NPC_class == CLASS_BOBAFETT || cent->gent->client->NPC_class ==
					CLASS_MANDALORIAN || cent->gent->client->NPC_class == CLASS_JANGO || cent->gent->client->
					NPC_class == CLASS_JANGODUAL || cent->gent->client->NPC_class == CLASS_ROCKETTROOPER)
					&& cent->gent->client->moveType == MT_FLYSWIM)
				{
					div_factor = 3.0f;
				}

				AngleVectors(cent->lerpAngles, cent_fwd, cent_rt, nullptr);
				angles[PITCH] = AngleNormalize180(
					DotProduct(cent->gent->client->ps.velocity, cent_fwd) / (2 * div_factor));
				if (angles[PITCH] > 90)
				{
					angles[PITCH] = 90;
				}
				else if (angles[PITCH] < -90)
				{
					angles[PITCH] = -90;
				}
				angles[ROLL] =
					AngleNormalize180(DotProduct(cent->gent->client->ps.velocity, cent_rt) / (10 * div_factor));
				if (angles[ROLL] > 90)
				{
					angles[ROLL] = 90;
				}
				else if (angles[ROLL] < -90)
				{
					angles[ROLL] = -90;
				}
			}
			AnglesToAxis(angles, legs);
		}

		//clamp relative to forward of cervical bone!
		if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST)
		{
			vec3_t chest_angles;
			VectorCopy(vec3_origin, chest_angles);
		}
		else
		{
			//look at lookTarget!
			float looking_speed = 0.3f;
			looking = CG_CheckLookTarget(cent, look_angles, &looking_speed);
			//Now add head bob when talking
			const qboolean talking = CG_AddHeadBob(cent, look_angles);

			//NOTE: previously, lookAngleSpeed was always 0.25f for the player
			//Figure out how fast head should be turning
			if (cent->pe.torso.yawing || cent->pe.torso.pitching)
			{
				//If torso is turning, we want to turn head just as fast
				if (cent->gent->NPC)
				{
					look_angle_speed = cent->gent->NPC->stats.yawSpeed / 150; //about 0.33 normally
				}
				else
				{
					look_angle_speed = CG_SWINGSPEED;
				}
			}
			else if (talking)
			{
				//Slow for head bobbing
				look_angle_speed = LOOK_TALKING_SPEED;
			}
			else if (looking)
			{
				//Not talking, set it up for looking at enemy, CheckLookTarget will scale it down if neccessary
				look_angle_speed = looking_speed;
			}
			else if (cent->gent->client->renderInfo.lookingDebounceTime > cg.time)
			{
				//Not looking, not talking, head is returning from a talking head bob, use talking speed
				look_angle_speed = LOOK_TALKING_SPEED;
			}

			if (looking || talking)
			{
				//want to keep doing this lerp behavior for a full second after stopped looking (so don't snap)
				//we should have a relative look angle, normalized to 180
				cent->gent->client->renderInfo.lookingDebounceTime = cg.time + 1000;
			}
			else
			{
				//still have a relative look angle from above
			}

			if (cent->gent->client->NPC_class == CLASS_RANCOR)
			{
				//always use the viewAngles we calced
				VectorCopy(view_angles, look_angles);
			}
			CG_UpdateLookAngles(cent, look_angles, look_angle_speed, -50.0f, 50.0f, -70.0f, 70.0f, -30.0f, 30.0f);
		}

		if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST)
		{
			VectorCopy(cent->lerpAngles, look_angles);
			look_angles[0] = look_angles[2] = 0;
			look_angles[YAW] -= trailing_legs_angles[YAW];
			BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, look_angles, BONE_ANGLES_POSTMULT, POSITIVE_X,
				NEGATIVE_Y,
				NEGATIVE_Z, cgs.model_draw);
		}
		else
		{
			vec3_t neck_angles;
			vec3_t head_angles;
			vec3_t head_clamp_min_angles = { -25, -55, -10 }, head_clamp_max_angles = { 50, 50, 10 };
			CG_G2ClientNeckAngles(cent, look_angles, head_angles, neck_angles, thoracic_angles, head_clamp_min_angles,
				head_clamp_max_angles);
		}
		return;
	}
	// All other entities
	if (cent->gent && cent->gent->client)
	{
		if (cent->gent->client->NPC_class == CLASS_PROBE
			|| cent->gent->client->NPC_class == CLASS_R2D2
			|| cent->gent->client->NPC_class == CLASS_R5D2
			|| cent->gent->client->NPC_class == CLASS_RANCOR
			|| cent->gent->client->NPC_class == CLASS_WAMPA
			|| cent->gent->client->NPC_class == CLASS_ATST)
		{
			VectorCopy(cent->lerpAngles, angles);
			angles[PITCH] = 0;

			//FIXME: use actual swing/clamp tolerances?
			if (cent->gent->client->ps.groundEntityNum != ENTITYNUM_NONE)
			{
				//on the ground
				CG_PlayerLegsYawFromMovement(cent, cent->gent->client->ps.velocity, &angles[YAW], cent->lerpAngles[YAW],
					-60, 60, qtrue);
			}
			else
			{
				//face legs to front
				CG_PlayerLegsYawFromMovement(cent, vec3_origin, &angles[YAW], cent->lerpAngles[YAW], -60, 60, qtrue);
			}

			VectorCopy(cent->lerpAngles, view_angles);
			//			viewAngles[YAW] = viewAngles[ROLL] = 0;
			view_angles[PITCH] *= 0.5;
			VectorCopy(view_angles, look_angles);

			look_angles[1] = 0;

			if (cent->gent->client->NPC_class == CLASS_ATST)
			{
				//body pitch
				BG_G2SetBoneAngles(cent, cent->gent->thoracicBone, look_angles, BONE_ANGLES_POSTMULT, POSITIVE_X,
					NEGATIVE_Y, NEGATIVE_Z, cgs.model_draw);
			}

			VectorCopy(view_angles, look_angles);

			vec3_t trailing_legs_angles;
			if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST)
			{
				CG_ATSTLegsYaw(cent, trailing_legs_angles);
				AnglesToAxis(trailing_legs_angles, legs);
			}
			else
			{
				//FIXME: this needs to properly set the legs.yawing field so we don't erroneously play the turning anim, but we do play it when turning in place
				if (angles[YAW] == cent->pe.legs.yawAngle)
				{
					cent->pe.legs.yawing = qfalse;
				}
				else
				{
					cent->pe.legs.yawing = qtrue;
				}

				cent->pe.legs.yawAngle = angles[YAW];
				if (cent->gent->client)
				{
					cent->gent->client->renderInfo.legsYaw = angles[YAW];
				}
				AnglesToAxis(angles, legs);
			}

			//			if ( cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST )
			//			{
			//				looking = qfalse;
			//			}
			//			else
			{
				//look at lookTarget!
				//FIXME: snaps to side when lets go of lookTarget... ?
				float looking_speed = 0.3f;
				looking = CG_CheckLookTarget(cent, look_angles, &looking_speed);
				look_angles[PITCH] = look_angles[ROLL] = 0; //droids can't pitch or roll their heads
				if (looking)
				{
					//want to keep doing this lerp behavior for a full second after stopped looking (so don't snap)
					cent->gent->client->renderInfo.lookingDebounceTime = cg.time + 1000;
				}
			}
			if (cent->gent->client->renderInfo.lookingDebounceTime > cg.time)
			{
				//adjust for current body orientation
				look_angles[YAW] -= cent->pe.torso.yawAngle;
				look_angles[YAW] -= cent->pe.legs.yawAngle;

				//normalize
				look_angles[YAW] = AngleNormalize180(look_angles[YAW]);

				//slowly lerp to this new value
				//Remember last headAngles
				vec3_t old_look_angles;
				VectorCopy(cent->gent->client->renderInfo.lastHeadAngles, old_look_angles);
				if (VectorCompare(old_look_angles, look_angles) == qfalse)
				{
					//FIXME: This clamp goes off viewAngles,
					//but really should go off the tag_torso's axis[0] angles, no?
					look_angles[YAW] = old_look_angles[YAW] + (look_angles[YAW] - old_look_angles[YAW]) * cg.
						frameInterpolation * 0.25;
				}
				//Remember current lookAngles next time
				VectorCopy(look_angles, cent->gent->client->renderInfo.lastHeadAngles);
			}
			else
			{
				//Remember current lookAngles next time
				VectorCopy(look_angles, cent->gent->client->renderInfo.lastHeadAngles);
			}
			if (cent->gent->client->NPC_class == CLASS_ATST)
			{
				VectorCopy(cent->lerpAngles, look_angles);
				look_angles[0] = look_angles[2] = 0;
				look_angles[YAW] -= trailing_legs_angles[YAW];
			}
			else
			{
				look_angles[PITCH] = look_angles[ROLL] = 0;
				look_angles[YAW] -= cent->pe.legs.yawAngle;
			}
			if (cent->gent->client->NPC_class == CLASS_WAMPA)
			{
				Eorientations o_up, o_rt, o_fwd;
				G_BoneOrientationsForClass(cent->gent->client->NPC_class, "cranium", &o_up, &o_rt, &o_fwd);
				BG_G2SetBoneAngles(cent, cent->gent->craniumBone, look_angles, BONE_ANGLES_POSTMULT, o_up, o_rt,
					o_fwd, cgs.model_draw);
			}
			else
			{
				BG_G2SetBoneAngles(cent, cent->gent->craniumBone, look_angles, BONE_ANGLES_POSTMULT, POSITIVE_X,
					NEGATIVE_Y, NEGATIVE_Z, cgs.model_draw);
			}
			//return;
		}
		else
			//if ( (cent->gent->client->NPC_class == CLASS_GONK ) || (cent->gent->client->NPC_class == CLASS_INTERROGATOR) || (cent->gent->client->NPC_class == CLASS_SENTRY) )
		{
			VectorCopy(cent->lerpAngles, angles);
			cent->pe.torso.pitchAngle = 0;
			cent->pe.torso.yawAngle = 0;
			cent->pe.legs.pitchAngle = angles[0];
			cent->gent->client->renderInfo.legsYaw = cent->pe.legs.yawAngle = angles[1];
			AnglesToAxis(angles, legs);
			//return;
		}
	}
}

static void CG_PlayerAngles(centity_t* cent, vec3_t legs[3], vec3_t torso[3], vec3_t head[3])
{
	vec3_t legs_angles, torso_angles, head_angles;
	vec3_t look_angles, view_angles;
	float head_yaw_clamp_min, head_yaw_clamp_max;
	float head_pitch_clamp_min, head_pitch_clamp_max;
	float torso_yaw_swing_tol_min, torso_yaw_swing_tol_max;
	float torso_yaw_clamp_min, torso_yaw_clamp_max;
	float torso_pitch_swing_tol_min, torso_pitch_swing_tol_max;
	float torso_pitch_clamp_min, torso_pitch_clamp_max;
	float legs_yaw_swing_tol_min, legs_yaw_swing_tol_max;
	float max_yaw_speed, yaw_speed, looking_speed;
	float look_angle_speed = LOOK_TALKING_SPEED; //shut up the compiler

	if ((cg.renderingThirdPerson || cg_trueguns.integer && !cg.zoomMode || cent->gent->client->ps.weapon == WP_SABER
		|| cent->gent->client->ps.weapon == WP_MELEE) && cent->gent && cent->gent->s.number == 0)
	{
		// If we are rendering third person, we should just force the player body to always fully face
		//	whatever way they are looking, otherwise, you can end up with gun shots coming off of the
		//	gun at angles that just look really wrong.

		//NOTENOTE: shots are coming out of the gun at ridiculous angles. The head & torso
		//should pitch *some* when looking up and down...

		//VectorClear( viewAngles );
		VectorCopy(cent->lerpAngles, view_angles);

		view_angles[YAW] = view_angles[ROLL] = 0;
		view_angles[PITCH] *= 0.5;
		AnglesToAxis(view_angles, head);

		view_angles[PITCH] *= 0.75;
		cent->pe.torso.pitchAngle = view_angles[PITCH];
		cent->pe.torso.yawAngle = view_angles[YAW];
		AnglesToAxis(view_angles, torso);

		VectorCopy(cent->lerpAngles, look_angles);
		look_angles[PITCH] = 0;

		//FIXME: this needs to properly set the legs.yawing field so we don't erroneously play the turning anim, but we do play it when turning in place
		if (look_angles[YAW] == cent->pe.legs.yawAngle)
		{
			cent->pe.legs.yawing = qfalse;
		}
		else
		{
			cent->pe.legs.yawing = qtrue;
		}

		if (cent->gent->client->ps.velocity[0] || cent->gent->client->ps.velocity[1])
		{
			const float move_yaw = vectoyaw(cent->gent->client->ps.velocity);
			look_angles[YAW] = cent->lerpAngles[YAW] + AngleDelta(cent->lerpAngles[YAW], move_yaw);
		}

		cent->pe.legs.yawAngle = look_angles[YAW];
		if (cent->gent->client)
		{
			cent->gent->client->renderInfo.legsYaw = look_angles[YAW];
		}
		AnglesToAxis(look_angles, legs);

		return;
	}

	if (cent->currentState.client_num != 0)
	{
		head_yaw_clamp_min = -cent->gent->client->renderInfo.headYawRangeLeft;
		head_yaw_clamp_max = cent->gent->client->renderInfo.headYawRangeRight;
		//These next two are only used for a calc below- this clamp is done in PM_UpdateViewAngles
		head_pitch_clamp_min = -cent->gent->client->renderInfo.headPitchRangeUp;
		head_pitch_clamp_max = cent->gent->client->renderInfo.headPitchRangeDown;

		torso_yaw_swing_tol_min = head_yaw_clamp_min * 0.3;
		torso_yaw_swing_tol_max = head_yaw_clamp_max * 0.3;
		torso_pitch_swing_tol_min = head_pitch_clamp_min * 0.5;
		torso_pitch_swing_tol_max = head_pitch_clamp_max * 0.5;
		torso_yaw_clamp_min = -cent->gent->client->renderInfo.torsoYawRangeLeft;
		torso_yaw_clamp_max = cent->gent->client->renderInfo.torsoYawRangeRight;
		torso_pitch_clamp_min = -cent->gent->client->renderInfo.torsoPitchRangeUp;
		torso_pitch_clamp_max = cent->gent->client->renderInfo.torsoPitchRangeDown;

		legs_yaw_swing_tol_min = torso_yaw_clamp_min * 0.5;
		legs_yaw_swing_tol_max = torso_yaw_clamp_max * 0.5;

		if (cent->gent && cent->gent->next_roff_time && cent->gent->next_roff_time >= cg.time)
		{
			//Following a roff, body must keep up with head, yaw-wise
			head_yaw_clamp_min =
				head_yaw_clamp_max =
				torso_yaw_swing_tol_min =
				torso_yaw_swing_tol_max =
				torso_yaw_clamp_min =
				torso_yaw_clamp_max =
				legs_yaw_swing_tol_min =
				legs_yaw_swing_tol_max = 0;
		}

		yaw_speed = max_yaw_speed = cent->gent->NPC->stats.yawSpeed / 150; //about 0.33 normally
	}
	else
	{
		head_yaw_clamp_min = -70;
		head_yaw_clamp_max = 70;

		//These next two are only used for a calc below- this clamp is done in PM_UpdateViewAngles
		head_pitch_clamp_min = -90;
		head_pitch_clamp_max = 90;

		torso_yaw_swing_tol_min = -90;
		torso_yaw_swing_tol_max = 90;
		torso_pitch_swing_tol_min = -90;
		torso_pitch_swing_tol_max = 90;
		torso_yaw_clamp_min = -90;
		torso_yaw_clamp_max = 90;
		torso_pitch_clamp_min = -90;
		torso_pitch_clamp_max = 90;

		legs_yaw_swing_tol_min = -90;
		legs_yaw_swing_tol_max = 90;

		yaw_speed = max_yaw_speed = CG_SWINGSPEED;
	}

	if (yaw_speed <= 0)
	{
		//Just in case
		yaw_speed = 0.5f; //was 0.33
	}

	looking_speed = yaw_speed;

	VectorCopy(cent->lerpAngles, head_angles);
	head_angles[YAW] = AngleNormalize360(head_angles[YAW]);
	VectorClear(legs_angles);
	VectorClear(torso_angles);

	// --------- yaw -------------

	//Clamp and swing the legs
	legs_angles[YAW] = head_angles[YAW];

	if (cent->gent->client->renderInfo.renderFlags & RF_LOCKEDANGLE)
	{
		cent->gent->client->renderInfo.legsYaw = cent->pe.legs.yawAngle = cent->gent->client->renderInfo.lockYaw;
		cent->pe.legs.yawing = qfalse;
		legs_angles[YAW] = cent->pe.legs.yawAngle;
	}
	else
	{
		qboolean always_face = qfalse;
		if (cent->gent && cent->gent->health > 0)
		{
			if (cent->gent->enemy)
			{
				always_face = qtrue;
			}
			if (CG_PlayerLegsYawFromMovement(cent, cent->gent->client->ps.velocity, &legs_angles[YAW], head_angles[YAW],
				torso_yaw_clamp_min, torso_yaw_clamp_max, always_face))
			{
				if (legs_angles[YAW] == cent->pe.legs.yawAngle)
				{
					cent->pe.legs.yawing = qfalse;
				}
				else
				{
					cent->pe.legs.yawing = qtrue;
				}
				cent->pe.legs.yawAngle = legs_angles[YAW];
				if (cent->gent->client)
				{
					cent->gent->client->renderInfo.legsYaw = legs_angles[YAW];
				}
			}
			else
			{
				CG_SwingAngles(legs_angles[YAW], legs_yaw_swing_tol_min, legs_yaw_swing_tol_max, torso_yaw_clamp_min,
					torso_yaw_clamp_max, max_yaw_speed, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing);
				legs_angles[YAW] = cent->pe.legs.yawAngle;
				if (cent->gent->client)
				{
					cent->gent->client->renderInfo.legsYaw = legs_angles[YAW];
				}
			}
		}
		else
		{
			CG_SwingAngles(legs_angles[YAW], legs_yaw_swing_tol_min, legs_yaw_swing_tol_max, torso_yaw_clamp_min,
				torso_yaw_clamp_max,
				max_yaw_speed, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing);
			legs_angles[YAW] = cent->pe.legs.yawAngle;
			if (cent->gent && cent->gent->client)
			{
				cent->gent->client->renderInfo.legsYaw = legs_angles[YAW];
			}
		}
	}

	// torso
	// If applicable, swing the lower parts to catch up with the head
	CG_SwingAngles(head_angles[YAW], torso_yaw_swing_tol_min, torso_yaw_swing_tol_max, head_yaw_clamp_min,
		head_yaw_clamp_max,
		yaw_speed, &cent->pe.torso.yawAngle, &cent->pe.torso.yawing);
	torso_angles[YAW] = cent->pe.torso.yawAngle;

	// ---------- pitch -----------

	//As the body twists to its extents, the back tends to arch backwards

	float dest;
	// only show a fraction of the pitch angle in the torso
	if (head_angles[PITCH] > 180)
	{
		dest = (-360 + head_angles[PITCH]) * 0.75;
	}
	else
	{
		dest = head_angles[PITCH] * 0.75;
	}

	CG_SwingAngles(dest, torso_pitch_swing_tol_min, torso_pitch_swing_tol_max, torso_pitch_clamp_min,
		torso_pitch_clamp_max, 0.1f,
		&cent->pe.torso.pitchAngle, &cent->pe.torso.pitching);
	torso_angles[PITCH] = cent->pe.torso.pitchAngle;
	// --------- roll -------------

	// pain twitch - FIXME: don't do this if you have no head (like droids?)
	// Maybe need to have clamp angles for roll as well as pitch and yaw?
	//CG_AddPainTwitch( cent, torsoAngles );

	//----------- Special head looking ---------------

	//FIXME: to clamp the head angles, figure out tag_head's offset from tag_torso and add
	//	that to whatever offset we're getting here... so turning the head in an
	//	anim that also turns the head doesn't allow the head to turn out of range.

	//Start with straight ahead
	VectorCopy(head_angles, view_angles);
	VectorCopy(head_angles, look_angles);

	//Remember last headAngles
	VectorCopy(cent->gent->client->renderInfo.lastHeadAngles, head_angles);

	//See if we're looking at someone/thing
	const qboolean looking = CG_CheckLookTarget(cent, look_angles, &looking_speed);

	//Now add head bob when talking
	/*	if ( cent->gent->client->clientInfo.extensions )
		{
			talking = CG_AddHeadBob( cent, lookAngles );
		}
	*/
	//Figure out how fast head should be turning
	if (cent->pe.torso.yawing || cent->pe.torso.pitching)
	{
		//If torso is turning, we want to turn head just as fast
		look_angle_speed = yaw_speed;
	}
	else if (looking)
	{
		//Not talking, set it up for looking at enemy, CheckLookTarget will scale it down if neccessary
		look_angle_speed = looking_speed;
	}
	else if (cent->gent->client->renderInfo.lookingDebounceTime > cg.time)
	{
		//Not looking, not talking, head is returning from a talking head bob, use talking speed
		look_angle_speed = LOOK_TALKING_SPEED;
	}

	if (looking)
	{
		//Keep this type of looking for a second after stopped looking
		cent->gent->client->renderInfo.lookingDebounceTime = cg.time + 1000;
	}

	if (cent->gent->client->renderInfo.lookingDebounceTime > cg.time)
	{
		//Calc our actual desired head angles
		for (int i = 0; i < 3; i++)
		{
			look_angles[i] = AngleNormalize360(cent->gent->client->renderInfo.headBobAngles[i] + look_angles[i]);
		}

		if (VectorCompare(head_angles, look_angles) == qfalse)
		{
			//FIXME: This clamp goes off viewAngles,
			//but really should go off the tag_torso's axis[0] angles, no?
			CG_UpdateAngleClamp(look_angles[PITCH], head_pitch_clamp_min / 1.25, head_pitch_clamp_max / 1.25,
				look_angle_speed,
				&head_angles[PITCH], view_angles[PITCH]);
			CG_UpdateAngleClamp(look_angles[YAW], head_yaw_clamp_min / 1.25, head_yaw_clamp_max / 1.25,
				look_angle_speed,
				&head_angles[YAW], view_angles[YAW]);
			CG_UpdateAngleClamp(look_angles[ROLL], -10, 10, look_angle_speed, &head_angles[ROLL], view_angles[ROLL]);
		}

		if (!cent->gent->enemy || cent->gent->enemy->s.number != cent->gent->client->renderInfo.lookTarget)
		{
			//Yaw change
			float swing = AngleSubtract(legs_angles[YAW], head_angles[YAW]);
			float scale = fabs(swing) / (torso_yaw_clamp_max + 0.01);
			//NOTENOTE: Some ents have a clamp of 0, which is bad for division

			scale *= LOOK_SWING_SCALE;
			torso_angles[YAW] = legs_angles[YAW] - swing * scale;

			//Pitch change
			swing = AngleSubtract(legs_angles[PITCH], head_angles[PITCH]);
			scale = fabs(swing) / (torso_pitch_clamp_max + 0.01);
			//NOTENOTE: Some ents have a clamp of 0, which is bad for division

			scale *= LOOK_SWING_SCALE;
			torso_angles[PITCH] = legs_angles[PITCH] - swing * scale;
		}
	}
	else
	{
		//Look straight ahead
		VectorCopy(view_angles, head_angles);
	}

	//Remember current headAngles next time
	VectorCopy(head_angles, cent->gent->client->renderInfo.lastHeadAngles);

	//-------------------------------------------------------------

	// pull the angles back out of the hierarchial chain
	AnglesSubtract(head_angles, torso_angles, head_angles);
	AnglesSubtract(torso_angles, legs_angles, torso_angles);
	AnglesToAxis(legs_angles, legs);
	AnglesToAxis(torso_angles, torso);
	AnglesToAxis(head_angles, head);
}

//==========================================================================

/*
===============
CG_TrailItem
===============
*/
/*
static void CG_TrailItem( centity_t *cent, qhandle_t hModel ) {
	refEntity_t		ent;
	vec3_t			angles;
	vec3_t			axis[3];

	VectorCopy( cent->lerpAngles, angles );
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	AnglesToAxis( angles, axis );

	memset( &ent, 0, sizeof( ent ) );
	VectorMA( cent->lerpOrigin, -24, axis[0], ent.origin );
	ent.origin[2] += 20;
	VectorScale( cg.autoAxis[0], 0.75, ent.axis[0] );
	VectorScale( cg.autoAxis[1], 0.75, ent.axis[1] );
	VectorScale( cg.autoAxis[2], 0.75, ent.axis[2] );
	ent.hModel = hModel;
	cgi_R_AddRefEntityToScene( &ent );
}
*/

/*
===============
CG_PlayerPowerups
===============
*/
//extern void CG_Seeker( centity_t *cent );
static void CG_PlayerPowerups(const centity_t* cent)
{
	faction_t friendlyfaction;

	if (cent->gent && cent->gent->client)
	{
		friendlyfaction = cent->gent->client->friendlyfaction;
	}
	else if (cent->gent && cent->gent->owner)
	{
		if (cent->gent->owner->client)
		{
			friendlyfaction = cent->gent->owner->client->friendlyfaction;
		}
		else
		{
			friendlyfaction = cent->gent->owner->noDamageFaction;
		}
	}
	if (!cent->currentState.powerups)
	{
		return;
	}

	if (cent->gent && cent->gent->health < 1)
	{
		return;
	}

	//if (cg_SerenityJediEngineMode.integer == 2 && cg_SaberInnonblockableAttackWarning.integer == 1
	//	&& cent->gent->client->ps.forcePower > 80
	//	&& cent->gent->health > 1
	//	&& (cent->currentState.powerups & 1 << PW_FORCE_PUSH
	//		|| cent->currentState.powerups & 1 << PW_FORCE_PUSH_RHAND
	//		|| cent->currentState.powerups & 1 << PW_MEDITATE))
	//{
	//	switch (cent->gent->friendlyfaction)
	//	{
	//	case FACTION_DARK:
	//		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 1, 0.2f, 0.2f); //red
	//		break;
	//	case FACTION_LIGHT:
	//		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 0.2f, 0.2f, 1); //blue
	//		break;
	//	case FACTION_SOLO:
	//	case FACTION_KOTOR:
	//		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 0.9f, 0.9f, 0.9f); //white
	//		break;
	//	case FACTION_NEUTRAL:
	//		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 0.0f, 0.0f, 0.0f); //clear
	//		break;
	//	default:
	//		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 0.0f, 0.0f, 0.0f); //clear
	//		break;
	//	}
	//}

	if (cent->currentState.powerups & 1 << PW_FORCE_PROJECTILE)
	{
		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 0.9f, 0.9f, 0.9f); //white
	}
}

constexpr auto SHADOW_DISTANCE = 128;

static qboolean player_shadow(const vec3_t origin, const float orientation, float* const shadow_plane,
	const float radius)
{
	vec3_t end;
	constexpr vec3_t mins = { -7, -7, 0 };
	constexpr vec3_t maxs = { 7, 7, 2 };
	trace_t trace;

	// send a trace down from the player to the ground
	VectorCopy(origin, end);
	end[2] -= SHADOW_DISTANCE;

	cgi_CM_BoxTrace(&trace, origin, end, mins, maxs, 0, MASK_PLAYERSOLID);

	// no shadow if too high
	if (trace.fraction == 1.0 || trace.startsolid && trace.allsolid)
	{
		return qfalse;
	}

	*shadow_plane = trace.endpos[2] + 1;

	if (cg_shadows.integer != 1)
	{
		// no mark for stencil or projection shadows
		return qtrue;
	}

	// fade the shadow out with height
	const float alpha = 1.0 - trace.fraction;

	// add the mark as a temporary, so it goes directly to the renderer
	// without taking a spot in the cg_marks array
	CG_ImpactMark(cgs.media.shadowMarkShader, trace.endpos, trace.plane.normal, orientation, 1, 1, 1, alpha, qfalse,
		radius, qtrue);

	return qtrue;
}

/*
===============
CG_PlayerShadow

Returns the Z component of the surface being shadowed

  should it return a full plane instead of a Z?
===============
*/
static qboolean CG_PlayerShadow(const centity_t* cent, float* const shadow_plane)
{
	*shadow_plane = 0;

	if (cg_shadows.integer == 0)
	{
		return qfalse;
	}

	// no shadows when cloaked
	if (cent->currentState.powerups & 1 << PW_CLOAKED)
	{
		return qfalse;
	}

	if (cent->gent->client->NPC_class == CLASS_SAND_CREATURE)
	{
		//sand creatures have no shadow
		return qfalse;
	}

	vec3_t root_origin;
	vec3_t temp_angles{};
	temp_angles[PITCH] = 0;
	temp_angles[YAW] = cent->pe.legs.yawAngle;
	temp_angles[ROLL] = 0;
	if (cent->gent->rootBone >= 0 && cent->gent->ghoul2.IsValid() && cent->gent->ghoul2[0].animModelIndexOffset)
		//If it has an animOffset it's a cinematic anim
	{
		//i might be running out of my bounding box, so get my root origin
		mdxaBone_t bolt_matrix;
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->rootBone,
			&bolt_matrix, temp_angles, cent->lerpOrigin,
			cg.time, cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, root_origin);
	}
	else
	{
		VectorCopy(cent->lerpOrigin, root_origin);
	}

	if (DistanceSquared(cg.refdef.vieworg, root_origin) > cg_shadowCullDistance.value * cg_shadowCullDistance.value)
	{
		// Shadow is too far away, don't do any traces, don't do any marks...blah
		return qfalse;
	}

	if (cent->gent->client->NPC_class == CLASS_ATST)
	{
		mdxaBone_t bolt_matrix;
		vec3_t side_origin;

		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->footLBolt,
			&bolt_matrix, temp_angles, cent->lerpOrigin,
			cg.time, cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, side_origin);
		side_origin[2] += 30; //fudge up a bit for coplaner
		qboolean b_shadowed = player_shadow(side_origin, 0, shadow_plane, 28);

		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->footRBolt,
			&bolt_matrix, temp_angles, cent->lerpOrigin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, side_origin);
		side_origin[2] += 30; //fudge up a bit for coplaner
		b_shadowed = static_cast<qboolean>(player_shadow(side_origin, 0, shadow_plane, 28) ||
			b_shadowed);

		b_shadowed = static_cast<qboolean>(player_shadow(root_origin, cent->pe.legs.yawAngle, shadow_plane, 64) ||
			b_shadowed);
		return b_shadowed;
	}
	if (cent->gent->client->NPC_class == CLASS_RANCOR)
	{
		return player_shadow(root_origin, cent->pe.legs.yawAngle, shadow_plane, 64);
	}
	return player_shadow(root_origin, cent->pe.legs.yawAngle, shadow_plane, 16);
}

void CG_LandingEffect(vec3_t origin, vec3_t normal, const int material)
{
	int effectID = -1;
	switch (material)
	{
	case MATERIAL_MUD:
		effectID = cgs.effects.landingMud;
		break;
	case MATERIAL_DIRT:
		effectID = cgs.effects.landingDirt;
		break;
	case MATERIAL_SAND:
		effectID = cgs.effects.landingSand;
		break;
	case MATERIAL_SNOW:
		effectID = cgs.effects.landingSnow;
		break;
	case MATERIAL_GRAVEL:
		effectID = cgs.effects.landingGravel;
		break;
	case MATERIAL_LAVA:
		effectID = cgs.effects.landingLava;
		break;
	default:;
	}

	if (effectID != -1)
	{
		theFxScheduler.PlayEffect(effectID, origin, normal);
	}
}

constexpr auto FOOTSTEP_DISTANCE = 32;

static void player_foot_step(const vec3_t origin,
	const vec3_t trace_dir,
	const float orientation,
	const float radius,
	const centity_t* cent, const footstepType_t foot_step_type)
{
	vec3_t end;
	constexpr vec3_t mins = { -7, -7, 0 };
	constexpr vec3_t maxs = { 7, 7, 2 };
	trace_t trace;
	footstep_t sound_type;
	bool b_mark = false;
	int effect_id = -1;
	//float		alpha;

	// send a trace down from the player to the ground
	VectorCopy(origin, end);
	VectorMA(origin, FOOTSTEP_DISTANCE, trace_dir, end); //was end[2] -= FOOTSTEP_DISTANCE;

	cgi_CM_BoxTrace(&trace, origin, end, mins, maxs, 0, MASK_PLAYERSOLID);

	// no shadow if too high
	if (trace.fraction >= 1.0f)
	{
		return;
	}

	//check for foot-steppable surface flag
	switch (trace.surfaceFlags & MATERIAL_MASK)
	{
	case MATERIAL_MUD:
		b_mark = true;
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_MUDRUN;
		}
		else
		{
			sound_type = FOOTSTEP_MUDWALK;
		}
		effect_id = cgs.effects.footstepMud;
		break;
	case MATERIAL_DIRT:
		b_mark = true;
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_DIRTRUN;
		}
		else
		{
			sound_type = FOOTSTEP_DIRTWALK;
		}
		effect_id = cgs.effects.footstepSand;
		break;
	case MATERIAL_SAND:
		b_mark = true;
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_SANDRUN;
		}
		else
		{
			sound_type = FOOTSTEP_SANDWALK;
		}
		effect_id = cgs.effects.footstepSand;
		break;
	case MATERIAL_SNOW:
		b_mark = true;
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_SNOWRUN;
		}
		else
		{
			sound_type = FOOTSTEP_SNOWWALK;
		}
		effect_id = cgs.effects.footstepSnow;
		break;
	case MATERIAL_LAVA:
		b_mark = true;
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_DIRTRUN;
		}
		else
		{
			sound_type = FOOTSTEP_DIRTWALK;
		}
		effect_id = cgs.effects.footstepSand;
		break;
	case MATERIAL_SHORTGRASS:
	case MATERIAL_LONGGRASS:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_GRASSRUN;
		}
		else
		{
			sound_type = FOOTSTEP_GRASSWALK;
		}
		break;
	case MATERIAL_SOLIDMETAL:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_METALRUN;
		}
		else
		{
			sound_type = FOOTSTEP_METALWALK;
		}
		break;
	case MATERIAL_HOLLOWMETAL:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_PIPERUN;
		}
		else
		{
			sound_type = FOOTSTEP_PIPEWALK;
		}
		break;
	case MATERIAL_GRAVEL:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_GRAVELRUN;
		}
		else
		{
			sound_type = FOOTSTEP_GRAVELWALK;
		}
		effect_id = cgs.effects.footstepGravel;
		break;
	case MATERIAL_CARPET:
	case MATERIAL_FABRIC:
	case MATERIAL_CANVAS:
	case MATERIAL_RUBBER:
	case MATERIAL_PLASTIC:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_RUGRUN;
		}
		else
		{
			sound_type = FOOTSTEP_RUGWALK;
		}
		break;
	case MATERIAL_SOLIDWOOD:
	case MATERIAL_HOLLOWWOOD:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_WOODRUN;
		}
		else
		{
			sound_type = FOOTSTEP_WOODWALK;
		}
		break;

	default:
		//fall through
	case MATERIAL_GLASS:
	case MATERIAL_WATER:
	case MATERIAL_FLESH:
	case MATERIAL_BPGLASS:
	case MATERIAL_DRYLEAVES:
	case MATERIAL_GREENLEAVES:
	case MATERIAL_TILES:
	case MATERIAL_PLASTER:
	case MATERIAL_SHATTERGLASS:
	case MATERIAL_ARMOR:
	case MATERIAL_COMPUTER:
	case MATERIAL_CONCRETE:
	case MATERIAL_ROCK:
	case MATERIAL_ICE:
	case MATERIAL_MARBLE:
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_HEAVY_L)
		{
			sound_type = FOOTSTEP_STONERUN;
		}
		else
		{
			sound_type = FOOTSTEP_STONEWALK;
		}
		break;
	}

	if (sound_type < FOOTSTEP_TOTAL)
	{
		cgi_S_StartSound(nullptr, cent->currentState.client_num, CHAN_BODY,
			cgs.media.footsteps[sound_type][Q_irand(0, 3)]);
	}

	if (cg_footsteps.integer < 4)
	{
		//debugging - 4 always does footstep effect
		if (cg_footsteps.integer < 2) //1 for sounds, 2 for effects, 3 for marks
		{
			return;
		}
	}

	if (effect_id != -1)
	{
		theFxScheduler.PlayEffect(effect_id, trace.endpos, trace.plane.normal);
	}

	if (cg_footsteps.integer < 4)
	{
		//debugging - 4 always does footprint decal
		if (!b_mark || cg_footsteps.integer < 3) //1 for sounds, 2 for effects, 3 for marks
		{
			return;
		}
	}
	qhandle_t footMarkShader;
	switch (foot_step_type)
	{
	case FOOTSTEP_HEAVY_R:
		footMarkShader = cgs.media.fshrMarkShader;
		break;
	case FOOTSTEP_HEAVY_L:
		footMarkShader = cgs.media.fshlMarkShader;
		break;
	case FOOTSTEP_R:
		footMarkShader = cgs.media.fsrMarkShader;
		break;
	default:
	case FOOTSTEP_L:
		footMarkShader = cgs.media.fslMarkShader;
		break;
	}

	// fade the shadow out with height
	//	alpha = 1.0 - trace.fraction;

	// add the mark as a temporary, so it goes directly to the renderer
	// without taking a spot in the cg_marks array

	vec3_t proj_normal;
	VectorCopy(trace.plane.normal, proj_normal);
	if (proj_normal[2] > 0.5f)
	{
		// footsteps will not have the correct orientation for all surfaces, so punt and set the projection to Z
		proj_normal[0] = 0.0f;
		proj_normal[1] = 0.0f;
		proj_normal[2] = 1.0f;
	}
	CG_ImpactMark(footMarkShader, trace.endpos, proj_normal,
		orientation, 1, 1, 1, 1.0f, qfalse, radius, qfalse);
}

extern vmCvar_t cg_footsteps;

static void CG_PlayerFootsteps(const centity_t* cent, const footstepType_t foot_step_type)
{
	if (cg_footsteps.integer == 0)
	{
		return;
	}

	//FIXME: make this a feature of NPCs in the NPCs.cfg? Specify a footstep shader, if any?
	if (cent->gent->client->NPC_class != CLASS_ATST
		&& cent->gent->client->NPC_class != CLASS_CLAW
		&& cent->gent->client->NPC_class != CLASS_FISH
		&& cent->gent->client->NPC_class != CLASS_FLIER2
		&& cent->gent->client->NPC_class != CLASS_GLIDER
		&& cent->gent->client->NPC_class != CLASS_INTERROGATOR
		&& cent->gent->client->NPC_class != CLASS_MURJJ
		&& cent->gent->client->NPC_class != CLASS_PROBE
		&& cent->gent->client->NPC_class != CLASS_R2D2
		&& cent->gent->client->NPC_class != CLASS_R5D2
		&& cent->gent->client->NPC_class != CLASS_REMOTE
		&& cent->gent->client->NPC_class != CLASS_SEEKER
		&& cent->gent->client->NPC_class != CLASS_SENTRY
		&& cent->gent->client->NPC_class != CLASS_SWAMP)
	{
		mdxaBone_t bolt_matrix;
		vec3_t temp_angles{}, side_origin, foot_down_dir;

		temp_angles[PITCH] = 0;
		temp_angles[YAW] = cent->pe.legs.yawAngle;
		temp_angles[ROLL] = 0;

		int foot_bolt = cent->gent->footLBolt;
		if (foot_step_type == FOOTSTEP_HEAVY_R || foot_step_type == FOOTSTEP_R)
		{
			foot_bolt = cent->gent->footRBolt;
		}
		//FIXME: get yaw orientation of the foot and use on decal
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, foot_bolt,
			&bolt_matrix, temp_angles, cent->lerpOrigin,
			cg.time, cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, side_origin);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, foot_down_dir);
		VectorMA(side_origin, -8.0f, foot_down_dir, side_origin); //was [2] += 15;	//fudge up a bit for coplanar
		player_foot_step(side_origin, foot_down_dir, cent->pe.legs.yawAngle, 6, cent, foot_step_type);
	}
}

static void player_splash(const vec3_t origin, const vec3_t velocity, const float radius, const int max_up)
{
	static vec3_t WHITE = { 1, 1, 1 };
	vec3_t start, end;
	trace_t trace;

	VectorCopy(origin, end);
	end[2] -= 24;

	// if the feet aren't in liquid, don't make a mark
	// this won't handle moving water brushes, but they wouldn't draw right anyway...
	int contents = cgi_CM_PointContents(end, 0);
	if (!(contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)))
	{
		return;
	}

	VectorCopy(origin, start);
	if (max_up < 32)
	{
		//our head may actually be lower than 32 above our origin
		start[2] += max_up;
	}
	else
	{
		start[2] += 32;
	}

	// if the head isn't out of liquid, don't make a mark
	contents = cgi_CM_PointContents(start, 0);
	if (contents & (CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
	{
		return;
	}

	// trace down to find the surface
	cgi_CM_BoxTrace(&trace, start, end, nullptr, nullptr, 0, CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);

	if (trace.fraction == 1.0)
	{
		return;
	}

	VectorCopy(trace.endpos, end);

	end[0] += Q_flrand(-1.0f, 1.0f) * 3.0f;
	end[1] += Q_flrand(-1.0f, 1.0f) * 3.0f;
	end[2] += 1.0f; //fudge up

	int t = VectorLengthSquared(velocity);

	if (t > 8192) // oh, magic number
	{
		t = 8192;
	}

	const float alpha = t / 8192.0f * 0.6f + 0.2f;

	FX_AddOrientedParticle(-1, end, trace.plane.normal, nullptr, nullptr,
		6.0f, radius + Q_flrand(0.0f, 1.0f) * 48.0f, 0,
		alpha, 0.0f, 0.0f,
		WHITE, WHITE, 0.0f,
		Q_flrand(0.0f, 1.0f) * 360, Q_flrand(-1.0f, 1.0f) * 6.0f, nullptr, nullptr, 0.0f, 0, 0, 1200,
		cgs.media.wakeMarkShader, FX_ALPHA_LINEAR | FX_SIZE_LINEAR);
}

/*
===============
CG_PlayerSplash

Draw a mark at the water surface
===============
*/
static void CG_PlayerSplash(const centity_t* cent)
{
	if (!cg_shadows.integer)
	{
		return;
	}

	if (cent->gent && cent->gent->client)
	{
		const gclient_t* cl = cent->gent->client;

		if (cent->gent->disconnectDebounceTime < cg.time) // can't do these expanding ripples all the time
		{
			if (cl->NPC_class == CLASS_ATST)
			{
				mdxaBone_t bolt_matrix;
				vec3_t temp_angles{}, side_origin;

				temp_angles[PITCH] = 0;
				temp_angles[YAW] = cent->pe.legs.yawAngle;
				temp_angles[ROLL] = 0;

				gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->footLBolt,
					&bolt_matrix, temp_angles, cent->lerpOrigin,
					cg.time, cgs.model_draw, cent->currentState.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, side_origin);
				side_origin[2] += 22; //fudge up a bit for coplaner
				player_splash(side_origin, cl->ps.velocity, 42, cent->gent->maxs[2]);

				gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->footRBolt,
					&bolt_matrix, temp_angles, cent->lerpOrigin, cg.time,
					cgs.model_draw, cent->currentState.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, side_origin);
				side_origin[2] += 22; //fudge up a bit for coplaner

				player_splash(side_origin, cl->ps.velocity, 42, cent->gent->maxs[2]);
			}
			else
			{
				// player splash mark
				player_splash(cent->lerpOrigin, cl->ps.velocity, 36,
					cl->renderInfo.eyePoint[2] - cent->lerpOrigin[2] + 5);
			}

			cent->gent->disconnectDebounceTime = cg.time + 125 + Q_flrand(0.0f, 1.0f) * 50.0f;
		}
	}
}

/*
===============
CG_StrikeBolt
===============
*/

extern void FX_Strike_Beam(vec3_t start, vec3_t end, vec3_t normal, vec3_t targ1, vec3_t targ2);

void CG_StrikeBolt(const centity_t* cent, vec3_t origin)
{
	trace_t trace;
	vec3_t end, forward, org, angs;

	// for lightning weapons coming from the player, it had better hit the crosshairs or else..
	if (cent->gent->s.number)
	{
		VectorCopy(origin, org);
	}
	else
	{
		VectorCopy(cent->gent->client->renderInfo.handLPoint, org);
	}

	// Find the impact point of the beam
	VectorCopy(cent->lerpAngles, angs);

	AngleVectors(angs, forward, nullptr, nullptr);

	VectorMA(org, 8192, forward, end);

	CG_Trace(&trace, org, vec3_origin, vec3_origin, end, cent->currentState.number, MASK_SHOT);

	// Make sparking be a bit less frame-rate dependent..also never add sparking when we hit a surface with a NOIMPACT flag
	if (cent->gent->fx_time < cg.time && !(trace.surfaceFlags & SURF_NOIMPACT))
	{
		cent->gent->fx_time = cg.time + Q_flrand(0.0f, 1.0f) * 100 + 100;
	}
	// Add in the effect
	FX_Strike_Beam(origin, trace.endpos, trace.plane.normal, cent->gent->pos1, cent->gent->pos2);
}

//-------------------------------------------
constexpr auto REFRACT_EFFECT_DURATION = 500;
constexpr auto REPULSE_EFFECT_DURATION = 1000;
void CG_ForcePushBlur(const vec3_t org, qboolean dark_side = qfalse);

static void CG_ForcePushRefraction(vec3_t org, const centity_t* cent, const float scale_factor, vec3_t colour)
{
	refEntity_t ent;
	vec3_t ang;
	float scale;
	float alpha;

	if (!cg_renderToTextureFX.integer)
	{
		CG_ForcePushBlur(org);
		return;
	}

	if (!cent->gent ||
		!cent->gent->client)
	{
		//can only do this for player/npc's
		return;
	}

	if (!cent->gent->client->pushEffectFadeTime)
	{
		//the duration for the expansion and fade
		cent->gent->client->pushEffectFadeTime = cg.time + REFRACT_EFFECT_DURATION;
	}

	//closer tDif is to 0, the closer we are to
	//being "done"
	const int t_dif = cent->gent->client->pushEffectFadeTime - cg.time;
	if (REFRACT_EFFECT_DURATION - t_dif < 200)
	{
		//stop following the hand after a little and stay in a fixed spot
		//save the initial spot of the effect
		VectorCopy(org, cent->gent->client->pushEffectOrigin);
	}

	//scale from 1.0f to 0.1f then hold at 0.1 for the rest of the duration
	if (cent->gent->client->ps.forcePowersActive & 1 << FP_REPULSE)
	{
		scale = 1.5f;
	}
	else if ((cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_MELEE)
		&& cent->gent->client->ps.forcePowersActive & 1 << FP_PUSH && cent->gent->client->ps.groundEntityNum ==
		ENTITYNUM_NONE)
	{
		scale = 1.0f;
	}
	else if (cent->gent->client->ps.forcePowersActive & 1 << FP_PULL)
	{
		scale = static_cast<float>(REFRACT_EFFECT_DURATION - t_dif) * 0.003f;
	}
	else
	{
		scale = static_cast<float>(t_dif) * 0.003f;
	}
	if (scale > 1.0f)
	{
		scale = 1.0f;
	}
	else if (scale < 0.2f)
	{
		scale = 0.2f;
	}

	//start alpha at 244, fade to 10
	if ((cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_MELEE)
		&& cent->gent->client->ps.forcePowersActive & 1 << FP_PUSH && cent->gent->client->ps.groundEntityNum ==
		ENTITYNUM_NONE)
	{
		alpha = 244.0f;
	}
	else if (cent->gent->client->ps.forcePowersActive & 1 << FP_REPULSE)
	{
		alpha = 244.0f;
	}
	else
	{
		alpha = static_cast<float>(t_dif) * 0.488f;
	}

	if (alpha > 244.0f)
	{
		alpha = 244.0f;
	}
	else if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}

	memset(&ent, 0, sizeof ent);
	ent.shaderTime = (cent->gent->client->pushEffectFadeTime - REFRACT_EFFECT_DURATION) / 1000.0f;

	VectorCopy(cent->gent->client->pushEffectOrigin, ent.origin);

	VectorSubtract(ent.origin, cg.refdef.vieworg, ent.axis[0]);
	const float v_len = VectorLength(ent.axis[0]);
	if (v_len <= 0.1f)
	{
		// Entity is right on vieworg.  quit.
		return;
	}

	vectoangles(ent.axis[0], ang);
	ang[ROLL] += 180.0f;

	AnglesToAxis(ang, ent.axis);

	//radius must be a power of 2, and is the actual captured texture size
	if (v_len < 128)
	{
		ent.radius = 256;
	}
	else if (v_len < 256)
	{
		ent.radius = 128;
	}
	else if (v_len < 512)
	{
		ent.radius = 64;
	}
	else
	{
		ent.radius = 32;
	}

	if (scale_factor)
	{
		scale *= scale_factor;
	}

	VectorScale(ent.axis[0], scale, ent.axis[0]);
	VectorScale(ent.axis[1], scale, ent.axis[1]);
	VectorScale(ent.axis[2], scale, ent.axis[2]);

	ent.hModel = cgs.media.halfShieldModel;
	ent.customShader = cgs.media.refractShader;
	ent.nonNormalizedAxes = qtrue;

	//make it partially transparent so it blends with the background
	ent.renderfx = RF_DISTORTION | RF_ALPHA_FADE;
	ent.shaderRGBA[0] = 255.0f;
	ent.shaderRGBA[1] = 255.0f;
	ent.shaderRGBA[2] = 255.0f;
	ent.shaderRGBA[3] = alpha;

	if (colour)
	{
		ent.shaderRGBA[0] = colour[0];
		ent.shaderRGBA[1] = colour[1];
		ent.shaderRGBA[2] = colour[2];
	}

	cgi_R_AddRefEntityToScene(&ent);
}

static void CG_ForceRepulseRefraction(vec3_t org, const centity_t* cent, const float scale_factor, vec3_t colour)
{
	refEntity_t ent;
	vec3_t ang;
	float scale;
	float alpha;

	if (!cg_renderToTextureFX.integer)
	{
		CG_ForcePushBlur(org);
		return;
	}

	if (!cent->gent ||
		!cent->gent->client)
	{
		//can only do this for player/npc's
		return;
	}

	if (!cent->gent->client->pushEffectFadeTime)
	{
		//the duration for the expansion and fade
		cent->gent->client->pushEffectFadeTime = cg.time + REFRACT_EFFECT_DURATION;
	}

	//closer tDif is to 0, the closer we are to
	//being "done"
	const int t_dif = cent->gent->client->pushEffectFadeTime - cg.time;
	if (REFRACT_EFFECT_DURATION - t_dif < 200)
	{
		//stop following the hand after a little and stay in a fixed spot
		//save the initial spot of the effect
		VectorCopy(org, cent->gent->client->pushEffectOrigin);
	}

	//scale from 1.0f to 0.1f then hold at 0.1 for the rest of the duration
	if ((cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_MELEE)
		&& cent->gent->client->ps.forcePowersActive & 1 << FP_PUSH && cent->gent->client->ps.groundEntityNum ==
		ENTITYNUM_NONE)
	{
		scale = 1.0f;
	}
	else if (cent->gent->client->ps.forcePowersActive & 1 << FP_PULL)
	{
		scale = static_cast<float>(REFRACT_EFFECT_DURATION - t_dif) * 0.003f;
	}
	else
	{
		scale = static_cast<float>(t_dif) * 0.003f;
	}
	if (scale > 1.0f)
	{
		scale = 1.0f;
	}
	else if (scale < 0.2f)
	{
		scale = 0.2f;
	}

	//start alpha at 244, fade to 10
	if ((cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_MELEE)
		&& cent->gent->client->ps.forcePowersActive & 1 << FP_PUSH && cent->gent->client->ps.groundEntityNum ==
		ENTITYNUM_NONE)
	{
		alpha = 244.0f;
	}
	else
	{
		alpha = static_cast<float>(t_dif) * 0.488f;
	}

	if (alpha > 244.0f)
	{
		alpha = 244.0f;
	}
	else if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}

	memset(&ent, 0, sizeof ent);
	ent.shaderTime = (cent->gent->client->pushEffectFadeTime - REFRACT_EFFECT_DURATION) / 1000.0f;

	VectorCopy(cent->gent->client->pushEffectOrigin, ent.origin);

	VectorSubtract(ent.origin, cg.refdef.vieworg, ent.axis[0]);
	const float v_len = VectorLength(ent.axis[0]);
	if (v_len <= 0.1f)
	{
		// Entity is right on vieworg.  quit.
		return;
	}

	vectoangles(ent.axis[0], ang);
	ang[ROLL] += 180.0f;

	AnglesToAxis(ang, ent.axis);

	//radius must be a power of 2, and is the actual captured texture size
	if (v_len < 128)
	{
		ent.radius = 256;
	}
	else if (v_len < 256)
	{
		ent.radius = 128;
	}
	else if (v_len < 512)
	{
		ent.radius = 64;
	}
	else
	{
		ent.radius = 32;
	}

	if (scale_factor)
	{
		scale *= scale_factor;
	}

	VectorScale(ent.axis[0], scale, ent.axis[0]);
	VectorScale(ent.axis[1], scale, ent.axis[1]);
	VectorScale(ent.axis[2], scale, ent.axis[2]);

	ent.hModel = cgs.media.halfShieldModel;
	ent.customShader = cgs.media.refractShader;
	ent.nonNormalizedAxes = qtrue;

	//make it partially transparent so it blends with the background
	ent.renderfx = RF_DISTORTION | RF_ALPHA_FADE;
	ent.shaderRGBA[0] = 255.0f;
	ent.shaderRGBA[1] = 255.0f;
	ent.shaderRGBA[2] = 255.0f;
	ent.shaderRGBA[3] = alpha;

	if (colour)
	{
		ent.shaderRGBA[0] = colour[0];
		ent.shaderRGBA[1] = colour[1];
		ent.shaderRGBA[2] = colour[2];
	}

	cgi_R_AddRefEntityToScene(&ent);
}

void CG_ForcePushBlur(const vec3_t org, const qboolean dark_side)
{
	localEntity_t* ex = CG_AllocLocalEntity();
	ex->leType = LE_PUFF;
	ex->refEntity.reType = RT_SPRITE;
	ex->radius = 2.0f;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 120;
	VectorCopy(org, ex->pos.trBase);
	ex->pos.trTime = cg.time;
	ex->pos.trType = TR_LINEAR;
	VectorScale(cg.refdef.viewaxis[1], 55, ex->pos.trDelta);

	if (dark_side)
	{
		//make it red
		ex->color[0] = 60;
		ex->color[1] = 8;
		ex->color[2] = 8;
	}
	else
	{
		//blue
		ex->color[0] = 24;
		ex->color[1] = 32;
		ex->color[2] = 40;
	}
	ex->refEntity.customShader = cgi_R_RegisterShader("gfx/effects/forcePush");

	ex = CG_AllocLocalEntity();
	ex->leType = LE_PUFF;
	ex->refEntity.reType = RT_SPRITE;
	ex->refEntity.rotation = 180.0f;
	ex->radius = 2.0f;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 120;
	VectorCopy(org, ex->pos.trBase);
	ex->pos.trTime = cg.time;
	ex->pos.trType = TR_LINEAR;
	VectorScale(cg.refdef.viewaxis[1], -55, ex->pos.trDelta);

	if (dark_side)
	{
		//make it red
		ex->color[0] = 60;
		ex->color[1] = 8;
		ex->color[2] = 8;
	}
	else
	{
		//blue
		ex->color[0] = 24;
		ex->color[1] = 32;
		ex->color[2] = 40;
	}
	ex->refEntity.customShader = cgi_R_RegisterShader("gfx/effects/forcePush");
}

//-------------------------------------------
static void CG_GraspBlur(const vec3_t org)
{
	localEntity_t* ex = CG_AllocLocalEntity();
	ex->leType = LE_PUFF;
	ex->refEntity.reType = RT_SPRITE;
	ex->radius = 2.0f;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 120;
	VectorCopy(org, ex->pos.trBase);
	ex->pos.trTime = cg.time;
	ex->pos.trType = TR_LINEAR;
	VectorScale(cg.refdef.viewaxis[1], 55, ex->pos.trDelta);

	ex->color[0] = 24;
	ex->color[1] = 32;
	ex->color[2] = 40;
	ex->refEntity.customShader = cgi_R_RegisterShader("gfx/effects/forcePush");

	ex = CG_AllocLocalEntity();
	ex->leType = LE_PUFF;
	ex->refEntity.reType = RT_SPRITE;
	ex->refEntity.rotation = 180.0f;
	ex->radius = 2.0f;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 120;
	VectorCopy(org, ex->pos.trBase);
	ex->pos.trTime = cg.time;
	ex->pos.trType = TR_LINEAR;
	VectorScale(cg.refdef.viewaxis[1], -55, ex->pos.trDelta);

	ex->color[0] = 24;
	ex->color[1] = 32;
	ex->color[2] = 40;
	ex->refEntity.customShader = cgi_R_RegisterShader("gfx/effects/forcePush");
}

static void CG_ForceDeadlySightBlur(const vec3_t org)
{
	localEntity_t* ex = CG_AllocLocalEntity();
	ex->leType = LE_PUFF;
	ex->refEntity.reType = RT_SPRITE;
	ex->radius = 2.0f;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 120;
	VectorCopy(org, ex->pos.trBase);
	ex->pos.trTime = cg.time;
	ex->pos.trType = TR_LINEAR;
	VectorScale(cg.refdef.viewaxis[1], 55, ex->pos.trDelta);

	ex->color[0] = 60;
	ex->color[1] = 8;
	ex->color[2] = 8;
	ex->refEntity.customShader = cgi_R_RegisterShader("gfx/effects/forcePush");

	ex = CG_AllocLocalEntity();
	ex->leType = LE_PUFF;
	ex->refEntity.reType = RT_SPRITE;
	ex->refEntity.rotation = 180.0f;
	ex->radius = 2.0f;
	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 120;
	VectorCopy(org, ex->pos.trBase);
	ex->pos.trTime = cg.time;
	ex->pos.trType = TR_LINEAR;
	VectorScale(cg.refdef.viewaxis[1], -55, ex->pos.trDelta);

	ex->color[0] = 60;
	ex->color[1] = 8;
	ex->color[2] = 8;
	ex->refEntity.customShader = cgi_R_RegisterShader("gfx/effects/forcePush");
}

static void CG_ForcePushBodyBlur(const centity_t* cent, const vec3_t origin, vec3_t temp_angles)
{
	vec3_t fx_org;
	mdxaBone_t bolt_matrix;

	// Head blur
	CG_ForcePushBlur(cent->gent->client->renderInfo.eyePoint);

	// Do a torso based blur
	if (cent->gent->torsoBolt >= 0)
	{
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->torsoBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}

	if (cent->gent->handRBolt >= 0)
	{
		// Do a right-hand based blur
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->handRBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}

	if (cent->gent->handLBolt >= 0)
	{
		// Do a left-hand based blur
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->handLBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}

	// Do the knees
	if (cent->gent->kneeLBolt >= 0)
	{
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->kneeLBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}

	if (cent->gent->kneeRBolt >= 0)
	{
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->kneeRBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}

	if (cent->gent->elbowLBolt >= 0)
	{
		// Do the elbows
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->elbowLBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}
	if (cent->gent->elbowRBolt >= 0)
	{
		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->elbowRBolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		CG_ForcePushBlur(fx_org);
	}
}

static void CG_ForceElectrocution(const centity_t* cent, const vec3_t origin, vec3_t temp_angles,
	const qhandle_t shader, const qboolean always_do = qfalse)
{
	// Undoing for now, at least this code should compile if I ( or anyone else ) decides to work on this effect
	qboolean found = qfalse;
	vec3_t fx_org, fx_org2, dir;
	vec3_t rgb = { 1.0f, 1.0f, 1.0f };
	mdxaBone_t bolt_matrix;

	int bolt = -1;
	int iter = 0;
	// Pick a random start point
	while (bolt < 0)
	{
		int test;
		if (iter > 5)
		{
			test = iter - 5;
		}
		else
		{
			test = Q_irand(0, 6);
		}
		switch (test)
		{
		case 0:
			// Right Elbow
			bolt = cent->gent->elbowRBolt;
			break;
		case 1:
			// Left Hand
			bolt = cent->gent->handLBolt;
			break;
		case 2:
			// Right hand
			bolt = cent->gent->handRBolt;
			break;
		case 3:
			// Left Foot
			bolt = cent->gent->footLBolt;
			break;
		case 4:
			// Right foot
			bolt = cent->gent->footRBolt;
			break;
		case 5:
			// Torso
			bolt = cent->gent->torsoBolt;
			break;
		case 6:
		default:
			// Left Elbow
			bolt = cent->gent->elbowLBolt;
			break;
		}
		if (++iter == 20)
			break;
	}
	if (bolt >= 0)
	{
		found = gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, bolt,
			&bolt_matrix, temp_angles, origin, cg.time,
			cgs.model_draw, cent->currentState.modelScale);
	}
	// Make sure that it's safe to even try and get these values out of the Matrix, otherwise the values could be garbage
	if (found)
	{
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);
		if (Q_flrand(0.0f, 1.0f) > 0.5f)
		{
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_X, dir);
		}
		else
		{
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, dir);
		}

		// Add some fudge, makes us not normalized, but that isn't really important
		dir[0] += Q_flrand(-1.0f, 1.0f) * 0.4f;
		dir[1] += Q_flrand(-1.0f, 1.0f) * 0.4f;
		dir[2] += Q_flrand(-1.0f, 1.0f) * 0.4f;
	}
	else
	{
		// Just use the lerp Origin and a random direction
		VectorCopy(cent->lerpOrigin, fx_org);
		VectorSet(dir, Q_flrand(-1.0f, 1.0f), Q_flrand(-1.0f, 1.0f), Q_flrand(-1.0f, 1.0f));
		// Not normalized, but who cares.
		if (cent->gent && cent->gent->client)
		{
			switch (cent->gent->client->NPC_class)
			{
			case CLASS_PROBE:
				fx_org[2] += 50;
				break;
			case CLASS_MARK1:
				fx_org[2] += 50;
				break;
			case CLASS_ATST:
				fx_org[2] += 120;
				break;
			default:
				break;
			}
		}
	}

	VectorMA(fx_org, Q_flrand(0.0f, 1.0f) * 40 + 40, dir, fx_org2);

	trace_t tr;

	CG_Trace(&tr, fx_org, nullptr, nullptr, fx_org2, -1, CONTENTS_SOLID);

	if (tr.fraction < 1.0f || Q_flrand(0.0f, 1.0f) > 0.94f || always_do)
	{
		FX_AddElectricity(-1, fx_org, tr.endpos,
			1.5f, 4.0f, 0.0f,
			1.0f, 0.5f, 0.0f,
			rgb, rgb, 0.0f,
			5.5f, Q_flrand(0.0f, 1.0f) * 50 + 100, shader,
			FX_ALPHA_LINEAR | FX_SIZE_LINEAR | FX_BRANCH | FX_GROW | FX_TAPER, -1, -1);
	}
}

int cg_lastHyperSpaceEffectTime = 0;
constexpr auto FLYBYSOUNDTIME = 2000;
constexpr auto TURN_ON = 0x00000000;
constexpr auto TURN_OFF = 0x00000100;

const char* bgToggleableSurfaces[BG_NUM_TOGGLEABLE_SURFACES] =
{
	"l_arm_key", //0
	"torso_canister1",
	"torso_canister2",
	"torso_canister3",
	"torso_tube1",
	"torso_tube2", //5
	"torso_tube3",
	"torso_tube4",
	"torso_tube5",
	"torso_tube6",
	"r_arm", //10
	"l_arm",
	"torso_shield",
	"torso_galaktorso",
	"torso_collar",
	"r_wing1", //15
	"r_wing2",
	"l_wing1",
	"l_wing2",
	"r_gear",
	"l_gear", //20
	"nose",
	"blah4",
	"blah5",
	"l_hand",
	"r_hand", //25
	"helmet",
	"head",
	"head_concussion_charger",
	"head_light_blaster_cann", //29
	nullptr
};

constexpr int bgToggleableSurfaceDebris[BG_NUM_TOGGLEABLE_SURFACES] =
{
	0, //0
	0,
	0,
	0,
	0,
	0, //5
	0,
	0,
	0,
	0,
	0, //10
	0,
	0,
	0,
	0, //>= 2 means it should create a flame trail when destroyed (for vehicles)
	3, //15
	5, //rwing2
	4,
	6, //lwing2
	0, //rgear
	0, //lgear			//20
	7, //nose
	0, //blah
	0, //blah
	0,
	0, //25
	0,
	0,
	0,
	0, //29
	-1
};

static void CG_BoltedEffects(const centity_t* cent)
{
	if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_VEHICLE)
	{
		Vehicle_t* p_veh = cent->gent->m_pVehicle;
		gentity_t* parent = cent->gent;

		if (p_veh->m_ulFlags & VEH_ARMORLOW && p_veh->m_iLastFXTime <= cg.time && Q_irand(0, 1) == 0)
		{
			p_veh->m_iLastFXTime = cg.time + 50;
			CG_PlayEffectIDBolted(p_veh->m_pVehicleInfo->iArmorLowFX, parent->playerModel, parent->crotchBolt,
				parent->s.number, parent->currentOrigin);
		}
	}
}

/*
===============
CG_PlayerCanSeeCent

tests force sight level
===============
*/
qboolean CG_PlayerCanSeeCent(const centity_t* cent)
{
	//return true if this cent is in view
	//NOTE: this is similar to the func SV_PlayerCanSeeEnt in sv_snapshot
	if (cent->currentState.eFlags & EF_FORCE_VISIBLE)
	{
		//can always be seen
		return qtrue;
	}

	if (g_entities[0].client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2
		&& cent->currentState.eType != ET_PLAYER)
	{
		//TEST: level 1 only sees force hints and enemies
		return qfalse;
	}

	float dot = 0.25f; //1.0f;
	float range = 512.0f;

	switch (g_entities[0].client->ps.forcePowerLevel[FP_SEE])
	{
	case FORCE_LEVEL_1:
		//dot = 0.95f;
		range = 1024.0f;
		break;
	case FORCE_LEVEL_2:
		//dot = 0.7f;
		range = 2048.0f;
		break;
	case FORCE_LEVEL_3:
	case FORCE_LEVEL_4:
	case FORCE_LEVEL_5:
		//dot = 0.4f;
		range = 4096.0f;
		break;
	default:;
	}

	vec3_t cent_dir, look_dir;
	VectorSubtract(cent->lerpOrigin, cg.refdef.vieworg, cent_dir);
	const float cent_dist = VectorNormalize(cent_dir);

	if (cent_dist < 128.0f)
	{
		//can always see them if they're really close
		return qtrue;
	}

	if (cent_dist > range)
	{
		//too far away to see them
		return qfalse;
	}

	dot += (0.99f - dot) * cent_dist / range; //the farther away they are, the more in front they have to be

	AngleVectors(cg.refdefViewAngles, look_dir, nullptr, nullptr);

	if (DotProduct(cent_dir, look_dir) < dot)
	{
		//not in force sight cone
		return qfalse;
	}

	return qtrue;
}

/*
===============
CG_AddForceSightShell

Adds the special effect
===============
*/
extern void CG_AddHealthBarEnt(int entNum);
extern void CG_AddBlockPointBarEnt(int entNum);
extern void CG_AddFatiguePointBarEnt(int entNum);

void CG_AddForceSightShell(refEntity_t* ent, const centity_t* cent)
{
	if (cg_SerenityJediEngineMode.integer == 2)
	{
		ent->customShader = cgs.media.forceShell_md;
	}
	else
	{
		ent->customShader = cgs.media.forceShell;
	}
	ent->renderfx &= ~RF_RGB_TINT;
	// See through walls.
	ent->renderfx |= RF_MINLIGHT | RF_NODEPTH;

	if (cent->currentState.eFlags & EF_FORCE_VISIBLE
		|| cent->currentState.eType == ET_PLAYER && cent->gent && cent->gent->message)
	{
		ent->shaderRGBA[0] = 0;
		ent->shaderRGBA[1] = 0;
		ent->shaderRGBA[2] = 255;
		ent->shaderRGBA[3] = 254;

		cgi_R_AddRefEntityToScene(ent);
		return;
	}

	ent->shaderRGBA[0] = 255;
	ent->shaderRGBA[1] = 255;
	ent->shaderRGBA[2] = 0;

	team_t team = TEAM_NEUTRAL;
	if (cent->gent && cent->gent->client)
	{
		team = cent->gent->client->playerTeam;
	}
	else if (cent->gent && cent->gent->owner)
	{
		if (cent->gent->owner->client)
		{
			team = cent->gent->owner->client->playerTeam;
		}
		else
		{
			team = cent->gent->owner->noDamageTeam;
		}
	}
	switch (team)
	{
	case TEAM_ENEMY:
		if (cent->gent && cent->gent->client)
		{
			if (cent->gent->client->NPC_class == CLASS_DESANN
				|| cent->gent->client->NPC_class == CLASS_SITHLORD
				|| cent->gent->client->NPC_class == CLASS_VADER
				|| cent->gent->client->NPC_class == CLASS_SHADOWTROOPER
				|| cent->gent->client->NPC_class == CLASS_TAVION
				|| cent->gent->client->NPC_class == CLASS_ALORA
				|| cent->gent->client->NPC_class == CLASS_REBORN
				|| cent->gent->client->NPC_class == CLASS_BOC)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 120;
				ent->shaderRGBA[2] = 255;
			}
			else if (cent->gent->client->NPC_class == CLASS_SBD
				|| cent->gent->client->NPC_class == CLASS_ASSASSIN_DROID
				|| cent->gent->client->NPC_class == CLASS_BATTLEDROID
				|| cent->gent->client->NPC_class == CLASS_DROIDEKA
				|| cent->gent->client->NPC_class == CLASS_SABER_DROID)
			{
				ent->shaderRGBA[0] = 100;
				ent->shaderRGBA[1] = 90;
				ent->shaderRGBA[2] = 160;
			}
			else if (cent->gent->client->NPC_class == CLASS_BOBAFETT
				|| cent->gent->client->NPC_class == CLASS_ROCKETTROOPER
				|| cent->gent->client->NPC_class == CLASS_MANDALORIAN
				|| cent->gent->client->NPC_class == CLASS_JANGO
				|| cent->gent->client->NPC_class == CLASS_JANGODUAL)
			{
				ent->shaderRGBA[0] = 85;
				ent->shaderRGBA[1] = 155;
				ent->shaderRGBA[2] = 210;
			}
			else if (cent->gent->client->NPC_class == CLASS_TUSKEN)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 120;
				ent->shaderRGBA[2] = 0;
			}
			else if (cent->gent->client->NPC_class == CLASS_MARK1
				|| cent->gent->client->NPC_class == CLASS_MARK2
				|| cent->gent->client->NPC_class == CLASS_GALAKMECH)
			{
				ent->shaderRGBA[0] = 10;
				ent->shaderRGBA[1] = 10;
				ent->shaderRGBA[2] = 10;
			}
			else if (cent->gent->client->NPC_class == CLASS_IMPERIAL)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 255;
				ent->shaderRGBA[2] = 0;
			}
			else
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 0;
				ent->shaderRGBA[2] = 0;
			}
		}
		break;
	case TEAM_PLAYER:
		if (cent->gent && cent->gent->client)
		{
			if (cent->gent->client->NPC_class == CLASS_JEDI
				|| cent->gent->client->NPC_class == CLASS_GROGU
				|| cent->gent->client->NPC_class == CLASS_LUKE
				|| cent->gent->client->NPC_class == CLASS_AHSOKA
				|| cent->gent->client->NPC_class == CLASS_YODA
				|| cent->gent->client->NPC_class == CLASS_KYLE
				|| cent->gent->client->NPC_class == CLASS_JEDIMASTER)
			{
				ent->shaderRGBA[0] = 0;
				ent->shaderRGBA[1] = 120;
				ent->shaderRGBA[2] = 0;
			}
			else if (cent->gent->client->NPC_class == CLASS_R2D2
				|| cent->gent->client->NPC_class == CLASS_R5D2
				|| cent->gent->client->NPC_class == CLASS_PROTOCOL)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 255;
				ent->shaderRGBA[2] = 230;
			}
			else if (cent->gent->client->NPC_class == CLASS_JAN)
			{
				ent->shaderRGBA[0] = 125;
				ent->shaderRGBA[1] = 255;
				ent->shaderRGBA[2] = 230;
			}
			else
			{
				ent->shaderRGBA[0] = 0;
				ent->shaderRGBA[1] = 255;
				ent->shaderRGBA[2] = 0;
			}
		}
		break;
	case TEAM_FREE:
		if (cent->gent && cent->gent->client)
		{
			if (cent->gent->client->NPC_class == CLASS_TUSKEN)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 120;
				ent->shaderRGBA[2] = 0;
			}
			else if (cent->gent->client->NPC_class == CLASS_RANCOR
				|| cent->gent->client->NPC_class == CLASS_WAMPA
				|| cent->gent->client->NPC_class == CLASS_SAND_CREATURE)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 0;
				ent->shaderRGBA[2] = 0;
			}
			else
			{
				ent->shaderRGBA[0] = 75;
				ent->shaderRGBA[1] = 75;
				ent->shaderRGBA[2] = 255;
			}
		}
		break;
	case TEAM_SOLO:
		if (cent->gent && cent->gent->client)
		{
			if (cent->gent->client->NPC_class == CLASS_TUSKEN)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 120;
				ent->shaderRGBA[2] = 0;
			}
			else if (cent->gent->client->NPC_class == CLASS_RANCOR
				|| cent->gent->client->NPC_class == CLASS_WAMPA
				|| cent->gent->client->NPC_class == CLASS_SAND_CREATURE)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 0;
				ent->shaderRGBA[2] = 0;
			}
			else
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 255;
				ent->shaderRGBA[2] = 255;
			}
		}
		break;
	case TEAM_PROJECTION:
		if (cent->gent && cent->gent->client)
		{
			if (cent->gent->client->NPC_class == CLASS_TUSKEN)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 120;
				ent->shaderRGBA[2] = 0;
			}
			else if (cent->gent->client->NPC_class == CLASS_RANCOR
				|| cent->gent->client->NPC_class == CLASS_WAMPA
				|| cent->gent->client->NPC_class == CLASS_SAND_CREATURE)
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 0;
				ent->shaderRGBA[2] = 0;
			}
			else
			{
				ent->shaderRGBA[0] = 255;
				ent->shaderRGBA[1] = 255;
				ent->shaderRGBA[2] = 255;
			}
		}
		break;
	default:
		break;
	}

	if (g_entities[0].client->ps.forcePowerLevel[FP_SEE] > FORCE_LEVEL_2)
	{
		if (cent->gent && cent->gent->health > 0 && cent->gent->max_health > 0)
		{
			//draw a health bar over them
			CG_AddHealthBarEnt(cent->currentState.client_num);
		}
	}

	if (g_entities[0].client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{
		//only level 2+ can see players through walls
		ent->renderfx &= ~RF_NODEPTH;
	}

	cgi_R_AddRefEntityToScene(ent);
}

/*
===============
CG_AddRefEntityWithPowerups

Adds a piece with modifications or duplications for powerups
===============
*/
extern vmCvar_t cg_thirdPersonAlpha;
extern qboolean NPC_IsMando(const gentity_t* self);
extern qboolean NPC_IsOversized(const gentity_t* self);

void CG_AddRefEntityWithPowerups(refEntity_t* ent, int powerups, centity_t* cent)
{
	const qboolean doing_dash_action = cg.predictedPlayerState.communicatingflags & 1 << DASHING ? qtrue : qfalse;

	if (!cent)
	{
		cgi_R_AddRefEntityToScene(ent);
		return;
	}
	gentity_t* gent = cent->gent;
	if (!gent)
	{
		cgi_R_AddRefEntityToScene(ent);
		return;
	}
	if (gent->client->ps.powerups[PW_DISRUPTION] < cg.time)
	{
		//disruptor
		if (powerups & 1 << PW_DISRUPTION)
		{
			//stop drawing him after this effect
			gent->client->ps.eFlags |= EF_NODRAW;
			return;
		}
	}

	if (gent->markTime > level.time && cg_allowcalloutmarker.integer && cg_allowcallout.integer)
	{
		ent->renderfx &= ~RF_RGB_TINT;
		ent->customShader = 0;
		cgi_R_AddRefEntityToScene(ent);

		ent->customShader = cgs.media.playerShieldDamage;
		ent->renderfx |= RF_RGB_TINT;

		ent->shaderRGBA[0] = 255;
		ent->shaderRGBA[1] = 20;
		ent->shaderRGBA[2] = 255;

		ent->shaderRGBA[3] = 0;

		ent->renderfx &= ~RF_ALPHA_FADE;
		cgi_R_AddRefEntityToScene(ent);
		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 1, 0.2f, 0.2f); //red
	}

	//get the dude's color choice in
	ent->shaderRGBA[0] = gent->client->renderInfo.customRGBA[0];
	ent->shaderRGBA[1] = gent->client->renderInfo.customRGBA[1];
	ent->shaderRGBA[2] = gent->client->renderInfo.customRGBA[2];
	ent->shaderRGBA[3] = gent->client->renderInfo.customRGBA[3];

	if (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent))
	{
		float alpha = 1.0f;
		if (cg.overrides.active & CG_OVERRIDE_3RD_PERSON_APH)
		{
			alpha = cg.overrides.thirdPersonAlpha;
		}
		else
		{
			alpha = cg_thirdPersonAlpha.value;
		}

		if (alpha < 1.0f)
		{
			ent->renderfx |= RF_ALPHA_FADE;
			ent->shaderRGBA[3] *= alpha;
		}
	}

	// If certain states are active, we don't want to add in the regular body
	if (!gent->client->ps.powerups[PW_CLOAKED] &&
		!gent->client->ps.powerups[PW_UNCLOAKING] &&
		!gent->client->ps.powerups[PW_DISRUPTION])
	{
		cgi_R_AddRefEntityToScene(ent);
	}

	// Disruptor Gun Alt-fire
	if (gent->client->ps.powerups[PW_DISRUPTION])
	{
		// I guess when something dies, it looks like pos1 gets set to the impact point on death, we can do fun stuff with this
		vec3_t temp_ang;
		VectorSubtract(gent->pos1, ent->origin, ent->oldorigin);
		//er, adjust this to get the proper position in model space... account for yaw
		float temp_length = VectorNormalize(ent->oldorigin);
		vectoangles(ent->oldorigin, temp_ang);
		temp_ang[YAW] -= gent->client->ps.viewangles[YAW];
		AngleVectors(temp_ang, ent->oldorigin, nullptr, nullptr);
		VectorScale(ent->oldorigin, temp_length, ent->oldorigin);

		ent->endTime = gent->fx_time;
		ent->renderfx |= RF_DISINTEGRATE2;

		ent->customShader = cgi_R_RegisterShader("gfx/effects/burn");
		cgi_R_AddRefEntityToScene(ent);

		ent->renderfx &= ~RF_DISINTEGRATE2;
		ent->renderfx |= RF_DISINTEGRATE1;
		ent->customShader = 0;
		cgi_R_AddRefEntityToScene(ent);

		if (cg.time - ent->endTime < 1000 && cg_timescale.value * cg_timescale.value * Q_flrand(0.0f, 1.0f) > 0.05f)
		{
			vec3_t fx_org;
			mdxaBone_t bolt_matrix;

			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, gent->playerModel, gent->torsoBolt,
				&bolt_matrix, gent->currentAngles, ent->origin, cg.time,
				cgs.model_draw, gent->s.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, fx_org);

			VectorMA(fx_org, -18, cg.refdef.viewaxis[0], fx_org);
			fx_org[2] += Q_flrand(-1.0f, 1.0f) * 20;
			theFxScheduler.PlayEffect("disruptor/death_smoke", fx_org);

			if (Q_flrand(0.0f, 1.0f) > 0.5f)
			{
				theFxScheduler.PlayEffect("disruptor/death_smoke", fx_org);
			}
		}
	}

	// Cloaking & Uncloaking Technology
	//----------------------------------------

	if (powerups & 1 << PW_UNCLOAKING)
	{
		//in the middle of cloaking
		if (cg.snap->ps.forcePowersActive & 1 << FP_SEE
			&& cg.snap->ps.client_num != cent->currentState.number
			&& CG_PlayerCanSeeCent(cent))
		{
			//just draw him
			cgi_R_AddRefEntityToScene(ent);
		}
		else
		{
			float perc = static_cast<float>(gent->client->ps.powerups[PW_UNCLOAKING] - cg.time) / 2000.0f;
			if (powerups & 1 << PW_CLOAKED)
			{
				//actually cloaking, so reverse it
				perc = 1.0f - perc;
			}

			if (perc >= 0.0f && perc <= 1.0f)
			{
				ent->renderfx &= ~RF_ALPHA_FADE;
				ent->renderfx |= RF_RGB_TINT;
				ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = 255.0f * perc;
				ent->shaderRGBA[3] = 0;
				ent->customShader = cgs.media.cloakedShader;
				cgi_R_AddRefEntityToScene(ent);

				ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = 255;
				ent->shaderRGBA[3] = 255 * (1.0f - perc); // let model alpha in
				ent->customShader = 0; // use regular skin
				ent->renderfx &= ~RF_RGB_TINT;
				ent->renderfx |= RF_ALPHA_FADE;
				cgi_R_AddRefEntityToScene(ent);
			}
		}
	}
	else if (powerups & 1 << PW_CLOAKED)
	{
		//fully cloaked
		if (cg.snap->ps.forcePowersActive & 1 << FP_SEE
			&& cg.snap->ps.client_num != cent->currentState.number
			&& CG_PlayerCanSeeCent(cent))
		{
			//just draw him
			cgi_R_AddRefEntityToScene(ent);
		}
		else
		{
			if (cg_renderToTextureFX.integer && cg_shadows.integer != 2 && cgs.glconfig.stencilBits >= 4)
			{
				cgi_R_SetRefractProp(1.0f, 0.0f, qfalse, qfalse); //don't need to do this every frame.. but..
				ent->customShader = 2; //crazy "refractive" shader
				cgi_R_AddRefEntityToScene(ent);
				ent->customShader = 0;
			}
			else
			{
				//stencil buffer's in use, sorry
				ent->renderfx = 0; //&= ~(RF_RGB_TINT|RF_ALPHA_FADE);
				ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = ent->shaderRGBA[3] = 255;
				ent->customShader = cgs.media.cloakedShader;
				cgi_R_AddRefEntityToScene(ent);
			}
		}
	}

	// Electricity
	//------------------------------------------------
	if (powerups & 1 << PW_SHOCKED)
	{
		int dif = gent->client->ps.powerups[PW_SHOCKED] - cg.time;

		if (dif > 0 && Q_flrand(0.0f, 1.0f) > 0.4f)
		{
			// fade out over the last 500 ms
			int brightness = 255;

			if (dif < 500)
			{
				brightness = floor((dif - 500.0f) / 500.0f * 255.0f);
			}

			ent->renderfx |= RF_RGB_TINT;
			ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = brightness;
			ent->shaderRGBA[3] = 255;

			if (rand() & 1)
			{
				ent->customShader = cgs.media.electricBodyShader;
			}
			else
			{
				ent->customShader = cgs.media.electricBody2Shader;
			}

			cgi_R_AddRefEntityToScene(ent);

			if (Q_flrand(0.0f, 1.0f) > 0.9f)
				cgi_S_StartSound(ent->origin, gent->s.number, CHAN_AUTO,
					cgi_S_RegisterSound("sound/effects/energy_crackle.wav"));
		}
	}

	if (powerups & 1 << PW_STUNNED)
	{
		int dif = gent->client->ps.powerups[PW_STUNNED] - cg.time;

		if (dif > 0 && Q_flrand(0.0f, 1.0f) > 0.4f)
		{
			// fade out over the last 500 ms
			int brightness = 255;

			if (dif < 500)
			{
				brightness = floor((dif - 500.0f) / 500.0f * 255.0f);
			}

			ent->renderfx |= RF_RGB_TINT;
			ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = brightness;
			ent->shaderRGBA[3] = 255;

			if (rand() & 1)
			{
				ent->customShader = cgs.media.shockBodyShader;
			}
			else
			{
				ent->customShader = cgs.media.shockBody2Shader;
			}

			cgi_R_AddRefEntityToScene(ent);

			if (Q_flrand(0.0f, 1.0f) > 0.9f)
			{
				cgi_S_StartSound(ent->origin, gent->s.number, CHAN_AUTO,
					cgi_S_RegisterSound("sound/effects/energy_crackle.wav"));
			}
		}
	}

	// FORCE speed does blur trails
	//------------------------------------------------------
	if (!doing_dash_action
		&& cg_speedTrail.integer
		&& (gent->client->ps.forcePowersActive & 1 << FP_SPEED //in force speed
			|| cent->gent->client->ps.legsAnim == BOTH_FORCELONGLEAP_START
			//or force long jump - FIXME: only 1st half of that anim?
			|| cent->gent->client->ps.legsAnim == BOTH_FORCELONGLEAP_ATTACK) //or force long jump attack
		&& (gent->s.number || cg.renderingThirdPerson)) // looks dumb doing this with first peron mode on
	{
		//FIXME: debounce this
		localEntity_t* ex;

		ex = CG_AllocLocalEntity();
		ex->leType = LE_FADE_MODEL;
		memcpy(&ex->refEntity, ent, sizeof(refEntity_t));

		ex->refEntity.renderfx |= RF_ALPHA_FADE | RF_NOSHADOW | RF_G2MINLOD;
		ex->startTime = cg.time;
		ex->endTime = ex->startTime + 75;
		VectorCopy(ex->refEntity.origin, ex->pos.trBase);
		VectorClear(ex->pos.trDelta);

		if (gent->client->renderInfo.customRGBA[0]
			|| gent->client->renderInfo.customRGBA[1]
			|| gent->client->renderInfo.customRGBA[2])
		{
			ex->color[0] = gent->client->renderInfo.customRGBA[0];
			ex->color[1] = gent->client->renderInfo.customRGBA[1];
			ex->color[2] = gent->client->renderInfo.customRGBA[2];
		}
		else
		{
			ex->color[0] = ex->color[1] = ex->color[2] = 255.0f;
		}
		ex->color[3] = 50.0f;
	}

	// Personal Shields
	//------------------------
	if (powerups & 1 << PW_BATTLESUIT)
	{
		float diff = gent->client->ps.powerups[PW_BATTLESUIT] - cg.time;

		if (cent->gent->client->ps.weapon != WP_SABER)
		{
			if (diff > 0)
			{
				float t;
				t = 1.0f - diff / (ARMOR_EFFECT_TIME * 2.0f);
				// Only display when we have damage
				if (t < 0.0f || t > 1.0f)
				{
				}
				else
				{
					ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = 255.0f * t;
					ent->shaderRGBA[3] = 255;
					ent->renderfx &= ~RF_ALPHA_FADE;
					ent->renderfx |= RF_RGB_TINT;
					ent->customShader = cgs.media.personalShieldShader;
					cgi_R_AddRefEntityToScene(ent);
				}
			}
		}
	}

	// Galak Mech shield bubble
	//------------------------------------------------------

	if (powerups & 1 << PW_GALAK_SHIELD && (cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent)))
	{
		if (NPC_IsMando(cent->gent) || NPC_IsOversized(cent->gent))
		{
			refEntity_t tent = {};

			tent.reType = RT_LATHE;

			// Setting up the 2d control points, these get swept around to make a 3D lathed model
			Vector2Set(tent.axis[0], 0.5, 0); // start point of curve
			Vector2Set(tent.axis[1], 50, 85); // control point 1
			Vector2Set(tent.axis[2], 135, -100); // control point 2
			Vector2Set(tent.oldorigin, 0, -90); // end point of curve

			if (gent->client->poisonTime && gent->client->poisonTime + 1000 > cg.time)
			{
				VectorCopy(gent->pos4, tent.lightingOrigin);
				tent.frame = gent->client->poisonTime;
			}

			if (gent->client->stunTime && gent->client->stunTime + 1000 > cg.time)
			{
				VectorCopy(gent->pos4, tent.lightingOrigin);
				tent.frame = gent->client->stunTime;
			}

			if (gent->client->AmputateTime && gent->client->AmputateTime + 1000 > cg.time)
			{
				VectorCopy(gent->pos4, tent.lightingOrigin);
				tent.frame = gent->client->AmputateTime;
			}

			mdxaBone_t bolt_matrix;
			vec3_t angles = { 0, gent->client->ps.legsYaw, 0 };

			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, gent->playerModel, gent->headModel, &bolt_matrix, angles,
				cent->lerpOrigin, cg.time, cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tent.origin); // pass in the emitter origin here

			tent.endTime = gent->fx_time + 1000;
			// if you want the shell to build around the guy, pass in a time that is 1000ms after the start of the turn-on-effect
			tent.customShader = cgi_R_RegisterShader("gfx/effects/barrier_shield");

			cgi_R_AddRefEntityToScene(&tent);
		}
		else if (cent->gent->client->NPC_class == CLASS_DROIDEKA && cent->gent->client->ps.weapon == WP_DROIDEKA)
		{
			//
		}
		else
		{
			refEntity_t tent = {};

			tent.reType = RT_LATHE;

			// Setting up the 2d control points, these get swept around to make a 3D lathed model
			Vector2Set(tent.axis[0], 0.5, 0); // start point of curve
			Vector2Set(tent.axis[1], 30, 50); // control point 1
			Vector2Set(tent.axis[2], 70, -55); // control point 2
			Vector2Set(tent.oldorigin, 0, -50); // end point of curve

			if (gent->client->poisonTime && gent->client->poisonTime + 1000 > cg.time)
			{
				VectorCopy(gent->pos4, tent.lightingOrigin);
				tent.frame = gent->client->poisonTime;
			}

			if (gent->client->stunTime && gent->client->stunTime + 1000 > cg.time)
			{
				VectorCopy(gent->pos4, tent.lightingOrigin);
				tent.frame = gent->client->stunTime;
			}

			if (gent->client->AmputateTime && gent->client->AmputateTime + 1000 > cg.time)
			{
				VectorCopy(gent->pos4, tent.lightingOrigin);
				tent.frame = gent->client->AmputateTime;
			}

			mdxaBone_t bolt_matrix;
			vec3_t angles = { 0, gent->client->ps.legsYaw, 0 };

			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, gent->playerModel, gent->headModel, &bolt_matrix, angles,
				cent->lerpOrigin, cg.time, cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tent.origin); // pass in the emitter origin here

			tent.endTime = gent->fx_time + 1000;
			// if you want the shell to build around the guy, pass in a time that is 1000ms after the start of the turn-on-effect
			tent.customShader = cgi_R_RegisterShader("gfx/effects/barrier_shield");

			cgi_R_AddRefEntityToScene(&tent);
		}
	}

	if (powerups & 1 << PW_GALAK_SHIELD && cent->gent->client->NPC_class == CLASS_GALAKMECH)
	{
		refEntity_t tent = {};

		tent.reType = RT_LATHE;

		// Setting up the 2d control points, these get swept around to make a 3D lathed model
		Vector2Set(tent.axis[0], 0.5, 0); // start point of curve
		Vector2Set(tent.axis[1], 50, 85); // control point 1
		Vector2Set(tent.axis[2], 135, -100); // control point 2
		Vector2Set(tent.oldorigin, 0, -90); // end point of curve

		if (gent->client->poisonTime && gent->client->poisonTime + 1000 > cg.time)
		{
			VectorCopy(gent->pos4, tent.lightingOrigin);
			tent.frame = gent->client->poisonTime;
		}

		if (gent->client->stunTime && gent->client->stunTime + 1000 > cg.time)
		{
			VectorCopy(gent->pos4, tent.lightingOrigin);
			tent.frame = gent->client->stunTime;
		}

		if (gent->client->AmputateTime && gent->client->AmputateTime + 1000 > cg.time)
		{
			VectorCopy(gent->pos4, tent.lightingOrigin);
			tent.frame = gent->client->AmputateTime;
		}

		mdxaBone_t bolt_matrix;
		vec3_t angles = { 0, gent->client->ps.legsYaw, 0 };

		gi.G2API_GetBoltMatrix(cent->gent->ghoul2, gent->playerModel, gent->genericBolt1, &bolt_matrix, angles,
			cent->lerpOrigin, cg.time, cgs.model_draw, cent->currentState.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tent.origin); // pass in the emitter origin here

		tent.endTime = gent->fx_time + 1000;
		// if you want the shell to build around the guy, pass in a time that is 1000ms after the start of the turn-on-effect
		tent.customShader = cgi_R_RegisterShader("gfx/effects/irid_shield");

		cgi_R_AddRefEntityToScene(&tent);
	}

	// Invincibility -- effect needs work
	//------------------------------------------------------

	if (cg_SerenityJediEngineMode.integer == 2
		&& cent->gent->client->ps.forcePower > 80
		&& cent->gent->health > 1
		&& cent->gent->client->ps.groundEntityNum != ENTITYNUM_NONE
		&& powerups & 1 << PW_INVINCIBLE)
	{
		theFxScheduler.PlayEffect(cgs.effects.forceInvincibility, cent->lerpOrigin);
	}

	if (cg_SerenityJediEngineMode.integer == 2
		&& cent->gent->client->ps.forcePower > 80
		&& cent->gent->health > 1
		&& powerups & 1 << PW_MEDITATE && !in_camera)
	{
		theFxScheduler.PlayEffect(cgs.effects.forceInvincibility, cent->lerpOrigin);
		cgi_R_AddLightToScene(cent->lerpOrigin, 60 + (rand() & 20), 0.9f, 0.9f, 0.9f); //white
	}

	// Healing -- could use some work....maybe also make it NOT be framerate dependant
	//------------------------------------------------------
	/*	if ( powerups & ( 1 << PW_HEALING ))
		{
			vec3_t axis[3];

			AngleVectors( cent->gent->client->renderInfo.eyeAngles, axis[0], axis[1], axis[2] );

			theFxScheduler.PlayEffect( cgs.effects.forceHeal, cent->gent->client->renderInfo.eyePoint, axis );
		}
	*/
	// Push Blur
	if (gent->forcePushTime > cg.time && gi.G2API_HaveWeGhoul2Models(cent->gent->ghoul2))
	{
		CG_ForcePushBlur(ent->origin);
	}

	//new Jedi Academy force powers
	//Rage effect
	if (cent->gent->client->ps.forcePowersActive & 1 << FP_RAGE &&
		(cg.renderingThirdPerson || cent->currentState.number != cg.snap->ps.client_num))
	{
		//ent->renderfx &= ~RF_FORCE_ENT_ALPHA;
		//ent->renderfx &= ~RF_MINLIGHT;

		ent->renderfx |= RF_RGB_TINT;
		ent->shaderRGBA[0] = 255;
		ent->shaderRGBA[1] = ent->shaderRGBA[2] = 0;
		ent->shaderRGBA[3] = 255;

		if (rand() & 1)
		{
			ent->customShader = cgs.media.electricBodyShader;
		}
		else
		{
			ent->customShader = cgs.media.electricBody2Shader;
		}

		cgi_R_AddRefEntityToScene(ent);
	}

	if (cg_SerenityJediEngineMode.integer && !in_camera)
	{
		if (cg_SaberInnonblockableAttackWarning.integer == 1
			|| d_SaberactionInfo->integer
			|| cg_DebugSaberCombat.integer)
		{
			if (cent->gent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(cent->gent))
			{
				if (pm_saber_innonblockable_attack(cent->currentState.torsoAnim))
				{
					ent->renderfx |= RF_RGB_TINT;
					ent->shaderRGBA[0] = 255;
					ent->shaderRGBA[1] = ent->shaderRGBA[2] = 0;
					ent->shaderRGBA[3] = 255;

					cgi_R_AddRefEntityToScene(ent);
				}
			}
		}
	}

	if (cg_SerenityJediEngineMode.integer && !in_camera)
	{
		//test for all sorts of shit... does it work? show me.
		if (cg_IsSaberDoingAttackDamage.integer == 1 || d_SaberactionInfo->integer || cg_DebugSaberCombat.integer)
		{
			if (BG_SaberInTransitionDamageMove(&cent->gent->client->ps)) //if in a transition dont do damage turn green
			{
				ent->renderfx |= RF_RGB_TINT;
				ent->shaderRGBA[0] = 0;
				ent->shaderRGBA[1] = ent->shaderRGBA[2] = 255;
				ent->shaderRGBA[3] = 0;

				cgi_R_AddRefEntityToScene(ent);
			}
			else if (BG_SaberInNonIdleDamageMove(&cent->gent->client->ps)) //doing damage make red
			{
				if (BG_SaberInPartialDamageMove(cent->gent)) //turn off damage in the move turn green
				{
					ent->renderfx |= RF_RGB_TINT;
					ent->shaderRGBA[0] = 0;
					ent->shaderRGBA[1] = ent->shaderRGBA[2] = 255;
					ent->shaderRGBA[3] = 0;

					cgi_R_AddRefEntityToScene(ent);
				}
				else
				{
					ent->renderfx |= RF_RGB_TINT; //doing damage make red
					ent->shaderRGBA[0] = 255;
					ent->shaderRGBA[1] = ent->shaderRGBA[2] = 0;
					ent->shaderRGBA[3] = 255;

					cgi_R_AddRefEntityToScene(ent);
				}
			}
		}
	}

	//FIXME: Tavion possessed effect?  White?
	//For now, these two are using the old shield shader. This is just so that you
	//can tell it apart from the JM/duel shaders, but it's still very obvious.
	if (cent->gent->client->ps.forcePowersActive & 1 << FP_PROTECT
		&& cent->gent->client->ps.forcePowersActive & 1 << FP_ABSORB)
	{
		//using both at once, save ourselves some rendering
		//protect+absorb is represented by cyan..

		if (cent->gent->client->ps.forcePowerLevel[FP_PROTECT] > FORCE_LEVEL_1
			|| cent->gent->client->ps.forcePowerLevel[FP_ABSORB] > FORCE_LEVEL_1)
		{
			ent->shaderRGBA[0] = 0;
			ent->shaderRGBA[1] = 255;
			ent->shaderRGBA[2] = 255;
			ent->shaderRGBA[3] = 254;
		}
		else
		{
			ent->shaderRGBA[0] = 0;
			ent->shaderRGBA[1] = 0;
			ent->shaderRGBA[2] = 255;
			ent->shaderRGBA[3] = 254;
		}

		ent->renderfx &= ~RF_RGB_TINT;

		if (cent->gent->client->ps.forcePowerLevel[FP_PROTECT] > FORCE_LEVEL_1
			|| cent->gent->client->ps.forcePowerLevel[FP_ABSORB] > FORCE_LEVEL_1)
		{
			if (cg_SerenityJediEngineMode.integer == 2)
			{
				ent->customShader = cgs.media.forceShell_md;
			}
			else
			{
				ent->customShader = cgs.media.forceShell;
			}
		}
		else
		{
			ent->customShader = cgs.media.playerShieldDamage;
		}

		cgi_R_AddRefEntityToScene(ent);
	}
	else if (cent->gent->client->ps.forcePowersActive & 1 << FP_PROTECT)
	{
		//protect is represented by green..
		if (cent->gent->client->ps.forcePowerLevel[FP_PROTECT] > FORCE_LEVEL_2) // if i have fp 3 i get this color
		{
			ent->shaderRGBA[0] = 120;
			ent->shaderRGBA[1] = 190;
			ent->shaderRGBA[2] = 0;
			ent->shaderRGBA[3] = 254;
		}
		else //if i have less than FP 3 i get this color
		{
			ent->shaderRGBA[0] = 80;
			ent->shaderRGBA[1] = 75;
			ent->shaderRGBA[2] = 140;
			ent->shaderRGBA[3] = 254;
		}

		ent->renderfx &= ~RF_RGB_TINT;

		if (cent->gent->client->ps.forcePowerLevel[FP_PROTECT] > FORCE_LEVEL_1)
		{
			if (cg_SerenityJediEngineMode.integer == 2)
			{
				ent->customShader = cgs.media.forceShell_md;
			}
			else
			{
				ent->customShader = cgs.media.forceShell;
			}
		}
		else
		{
			ent->customShader = cgs.media.playerShieldDamage;
		}

		cgi_R_AddRefEntityToScene(ent);
	}
	else if (cent->gent->client->ps.forcePowersActive & 1 << FP_PROJECTION)
	{
		//projection is represented by grey..
		if (cg_renderToTextureFX.integer && cg_shadows.integer != 2 && cgs.glconfig.stencilBits >= 4)
		{
			cgi_R_SetRefractProp(1.0f, 0.0f, qfalse, qfalse); //don't need to do this every frame.. but..
			ent->customShader = 2; //crazy "refractive" shader
			cgi_R_AddRefEntityToScene(ent);
			ent->customShader = 0;
		}
		else
		{
			//stencil buffer's in use, sorry
			ent->renderfx = 0;
			ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = ent->shaderRGBA[3] = 255;
			ent->customShader = cgs.media.forceShell;
			cgi_R_AddRefEntityToScene(ent);
		}
	}
	else if (cent->gent->client->ps.forcePowersActive & 1 << FP_ABSORB)
	{
		//absorb is represented by blue..
		if (cent->gent->client->ps.forcePowerLevel[FP_ABSORB] > FORCE_LEVEL_2) // if i have fp 3 i get this color
		{
			ent->shaderRGBA[0] = 90;
			ent->shaderRGBA[1] = 30;
			ent->shaderRGBA[2] = 185;
			ent->shaderRGBA[3] = 254;
		}
		else //if i have less than FP 3 i get this color
		{
			ent->shaderRGBA[0] = 255;
			ent->shaderRGBA[1] = 115;
			ent->shaderRGBA[2] = 0;
			ent->shaderRGBA[3] = 254;
		}

		ent->renderfx &= ~RF_RGB_TINT;

		if (cent->gent->client->ps.forcePowerLevel[FP_ABSORB] > FORCE_LEVEL_1)
		{
			if (cg_SerenityJediEngineMode.integer == 2)
			{
				ent->customShader = cgs.media.forceShell_md;
			}
			else
			{
				ent->customShader = cgs.media.forceShell;
			}
		}
		else
		{
			ent->customShader = cgs.media.playerShieldDamage;
		}

		cgi_R_AddRefEntityToScene(ent);
	}

	// Render the shield effect if they've been hit recently
	if (cent->shieldHitTime > cg.time)
	{
		float t = (cent->shieldHitTime - cg.time) / 250.0f;

		ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = t * 255.0f;
		ent->shaderRGBA[3] = 255.0f;

		ent->renderfx &= ~RF_RGB_TINT;
		ent->renderfx &= ~RF_FORCE_ENT_ALPHA;

		ent->customShader = cgs.media.playerShieldDamage;
		cgi_R_AddRefEntityToScene(ent);
	}

	// ...or have started recharging their shield
	if (cent->shieldRechargeTime > cg.time)
	{
		float t = 1.0f - (cent->shieldRechargeTime - cg.time) / 1000.0f;

		ent->shaderRGBA[0] = ent->shaderRGBA[1] = ent->shaderRGBA[2] = t * 255.0f;
		ent->shaderRGBA[3] = 255.0f;

		ent->renderfx &= ~RF_RGB_TINT;
		ent->renderfx &= ~RF_FORCE_ENT_ALPHA;

		ent->customShader = cgs.media.playerShieldDamage;
		cgi_R_AddRefEntityToScene(ent);
	}

	if (cg.snap->ps.forcePowersActive & 1 << FP_SEE
		&& cg.snap->ps.client_num != cent->currentState.number
		&& (cent->currentState.eFlags & EF_FORCE_VISIBLE
			|| (cent->gent->health > 0 || cent->gent->message)
			&& cent->currentState.eType == ET_PLAYER //other things handle this in their own render funcs
			&& CG_PlayerCanSeeCent(cent)))
	{
		//force sight draws auras around living things
		CG_AddForceSightShell(ent, cent);
	}

	//temp stuff for drain
	if ((cent->gent->client->ps.eFlags & EF_FORCE_DRAINED || cent->gent->client->ps.forcePowersActive & 1 <<
		FP_DRAIN) &&
		(cg.renderingThirdPerson || cent->currentState.number != cg.snap->ps.client_num))
	{
		//draining or being drained
		ent->renderfx |= RF_RGB_TINT;
		ent->shaderRGBA[0] = 255;
		ent->shaderRGBA[1] = ent->shaderRGBA[2] = 0;
		ent->shaderRGBA[3] = 255;

		if (rand() & 1)
		{
			ent->customShader = cgs.media.electricBodyShader;
		}
		else
		{
			ent->customShader = cgs.media.electricBody2Shader;
		}

		cgi_R_AddRefEntityToScene(ent);
	}

	if (cg_SerenityJediEngineMode.integer == 2)
	{
		if (cent->gent->client->ps.stasisTime > (cg.time ? cg.time : level.time))
		{
			//stasis is represented by grey..
			ent->shaderRGBA[0] = 10;
			ent->shaderRGBA[1] = 100;
			ent->shaderRGBA[2] = 100;
			ent->shaderRGBA[3] = 254;

			ent->renderfx &= ~RF_RGB_TINT;
			ent->customShader = cgs.media.playerShieldDamage;

			cgi_R_AddRefEntityToScene(ent);
		}

		if (cent->gent->client->ps.stasisJediTime > (cg.time ? cg.time : level.time))
		{
			//stasis is represented by grey..
			ent->shaderRGBA[0] = 255;
			ent->shaderRGBA[1] = 100;
			ent->shaderRGBA[2] = 10;
			ent->shaderRGBA[3] = 10;

			ent->renderfx &= ~RF_RGB_TINT;
			ent->customShader = cgs.media.playerShieldDamage;

			cgi_R_AddRefEntityToScene(ent);
		}
	}
	else
	{
		if (cent->gent->client->ps.stasisTime > (cg.time ? cg.time : level.time))
		{
			//stasis is represented by grey..
			ent->shaderRGBA[0] = 100;
			ent->shaderRGBA[1] = 100;
			ent->shaderRGBA[2] = 100;
			ent->shaderRGBA[3] = 254;

			ent->renderfx &= ~RF_RGB_TINT;
			ent->customShader = cgs.media.playerShieldDamage;

			cgi_R_AddRefEntityToScene(ent);
		}

		if (cent->gent->client->ps.stasisJediTime > (cg.time ? cg.time : level.time))
		{
			//stasis is represented by grey..
			ent->shaderRGBA[0] = 255;
			ent->shaderRGBA[1] = 100;
			ent->shaderRGBA[2] = 10;
			ent->shaderRGBA[3] = 10;

			ent->renderfx &= ~RF_RGB_TINT;
			ent->customShader = cgs.media.playerShieldDamage;

			cgi_R_AddRefEntityToScene(ent);
		}
	}
}

constexpr auto MAX_SHIELD_TIME = 2000.0;
constexpr auto MIN_SHIELD_TIME = 2000.0;

void CG_PlayerShieldHit(const int entitynum, vec3_t angles, const int amount)
{
	int time;

	if (entitynum < 0 || entitynum >= MAX_GENTITIES)
	{
		return;
	}

	centity_t* cent = &cg_entities[entitynum];

	if (cent->currentState.weapon == WP_SABER)
	{
		return;
	}

	if (amount > 100)
	{
		time = cg.time + MAX_SHIELD_TIME; // 2 sec.
	}
	else
	{
		time = cg.time + 500 + amount * 15;
	}

	if (time > cent->damageTime)
	{
		cent->damageTime = time;
		VectorScale(angles, -1, angles);
		vectoangles(angles, cent->damageAngles);
	}
	cent->shieldHitTime = cg.time + 250;
}

static void CG_PlayerShieldRecharging(const int entitynum)
{
	if (entitynum < 0 || entitynum >= MAX_GENTITIES)
	{
		return;
	}

	centity_t* cent = &cg_entities[entitynum];
	cent->shieldRechargeTime = cg.time + 1000;
}

static void CG_DrawPlayerShield(const centity_t* cent, vec3_t origin)
{
	refEntity_t ent;

	// Don't draw the shield when the player is dead.
	if (cent->gent->client->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	memset(&ent, 0, sizeof ent);

	VectorCopy(origin, ent.origin);
	ent.origin[2] += 10.0;
	AnglesToAxis(cent->damageAngles, ent.axis);

	int alpha = 255.0f * ((cent->damageTime - cg.time) / MIN_SHIELD_TIME) + Q_flrand(0.1f, 1.0f) * 16;
	if (alpha > 255)
		alpha = 255;

	// Make it bigger, but tighter if more solid
	const float scale = 1.4 - static_cast<float>(alpha) * (0.4 / 255.0); // Range from 1.0 to 1.4
	VectorScale(ent.axis[0], scale, ent.axis[0]);
	VectorScale(ent.axis[1], scale, ent.axis[1]);
	VectorScale(ent.axis[2], scale, ent.axis[2]);

	ent.hModel = cgs.media.halfShieldModel;
	ent.customShader = cgs.media.halfShieldShader;
	ent.shaderRGBA[0] = alpha;
	ent.shaderRGBA[1] = alpha;
	ent.shaderRGBA[2] = alpha;
	ent.shaderRGBA[3] = 255;
	cgi_R_AddRefEntityToScene(&ent);
}

static void CG_PlayerHitFX(centity_t* cent)
{
	// only do the below fx if the cent in question is...uh...me, and it's first person.
	if (cent->currentState.client_num == cg.snap->ps.client_num || cg.renderingThirdPerson)
	{
		if (cent->damageTime > cg.time && cent->gent->client->NPC_class != CLASS_VEHICLE
			&& cent->gent->client->NPC_class != CLASS_SEEKER
			&& cent->gent->client->NPC_class != CLASS_REMOTE)
		{
			CG_DrawPlayerShield(cent, cent->lerpOrigin);
		}
	}
}

/*
-------------------------
CG_G2SetHeadBlink
-------------------------
*/
static void CG_G2SetHeadBlink(const centity_t* cent, const qboolean b_start)
{
	if (!cent)
	{
		return;
	}
	gentity_t* gent = cent->gent;
	//FIXME: get these boneIndices game-side and pass it down?
	//FIXME: need a version of this that *doesn't* need the mFileName in the ghoul2
	const int h_leye = gi.G2API_GetBoneIndex(&gent->ghoul2[0], "leye", qtrue);
	if (h_leye == -1)
	{
		return;
	}

	vec3_t desired_angles = { 0 };
	int blend_time = 80;
	qboolean b_wink = qfalse;

	if (b_start)
	{
		desired_angles[YAW] = -38;
		if (!in_camera && Q_flrand(0.0f, 1.0f) > 0.95f)
		{
			b_wink = qtrue;
			blend_time /= 3;
		}
	}
	gi.G2API_SetBoneAnglesIndex(&gent->ghoul2[gent->playerModel], h_leye, desired_angles,
		BONE_ANGLES_POSTMULT, POSITIVE_Y, POSITIVE_Z, POSITIVE_X, nullptr, blend_time, cg.time);
	const int h_reye = gi.G2API_GetBoneIndex(&gent->ghoul2[0], "reye", qtrue);
	if (h_reye == -1)
	{
		return;
	}

	if (!b_wink)
		gi.G2API_SetBoneAnglesIndex(&gent->ghoul2[gent->playerModel], h_reye, desired_angles,
			BONE_ANGLES_POSTMULT, POSITIVE_Y, POSITIVE_Z, POSITIVE_X, nullptr, blend_time,
			cg.time);
}

/*
-------------------------
CG_G2SetHeadAnims
-------------------------
*/
static void CG_G2SetHeadAnim(const centity_t* cent, const int anim)
{
	gentity_t* gent = cent->gent;
	const animation_t* animations = level.knownAnimFileSets[gent->client->clientInfo.animFileIndex].animations;
	int anim_flags = BONE_ANIM_OVERRIDE;
	const float time_scale_mod = cg_timescale.value ? 1.0 / cg_timescale.value : 1.0;
	const float animSpeed = 50.0f / animations[anim].frameLerp * time_scale_mod;

	if (animations[anim].numFrames <= 0)
	{
		return;
	}
	if (anim == FACE_DEAD)
	{
		anim_flags |= BONE_ANIM_OVERRIDE_FREEZE;
	}
	// animSpeed is 1.0 if the frameLerp (ms/frame) is 50 (20 fps).
	int first_frame;
	int last_frame;
	if (animSpeed < 0)
	{
		//play anim backwards
		last_frame = animations[anim].firstFrame - 1;
		first_frame = animations[anim].numFrames - 1 + animations[anim].firstFrame;
	}
	else
	{
		first_frame = animations[anim].firstFrame;
		last_frame = animations[anim].numFrames + animations[anim].firstFrame;
	}

	// first decide if we are doing an animation on the head already
	//	int startFrame, endFrame;
	//	const qboolean animatingHead =  gi.G2API_GetAnimRangeIndex(&gent->ghoul2[gent->playerModel], cent->gent->faceBone, &startFrame, &endFrame);

	//	if (!animatingHead || ( animations[anim].firstFrame != startFrame ) )// only set the anim if we aren't going to do the same animation again
	{
		constexpr int blend_time = 50;
		gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], cent->gent->faceBone,
			first_frame, last_frame, anim_flags, animSpeed, cg.time, -1, blend_time);
	}
}

static qboolean CG_G2PlayerHeadAnims(const centity_t* cent)
{
	if (!ValidAnimFileIndex(cent->gent->client->clientInfo.animFileIndex))
	{
		return qfalse;
	}

	if (cent->gent->faceBone == BONE_INDEX_INVALID)
	{
		// i don't have a face
		return qfalse;
	}

	int anim = -1;

	if (cent->gent->health <= 0)
	{
		//Dead people close their eyes and don't make faces!
		anim = FACE_DEAD;
	}
	else
	{
		if (!cent->gent->client->facial_blink)
		{
			// set the timers
			cent->gent->client->facial_blink = cg.time + Q_flrand(4000.0, 8000.0);
			cent->gent->client->facial_timer = cg.time + Q_flrand(6000.0, 10000.0);
		}

		//are we blinking?
		if (cent->gent->client->facial_blink < 0)
		{
			// yes, check if we are we done blinking ?
			if (-cent->gent->client->facial_blink < cg.time)
			{
				// yes, so reset blink timer
				cent->gent->client->facial_blink = cg.time + Q_flrand(4000.0, 8000.0);
				CG_G2SetHeadBlink(cent, qfalse); //stop the blink
			}
		}
		else // no we aren't blinking
		{
			if (cent->gent->client->facial_blink < cg.time) // but should we start ?
			{
				CG_G2SetHeadBlink(cent, qtrue);
				if (cent->gent->client->facial_blink == 1)
				{
					//requested to stay shut by SET_FACEEYESCLOSED
					cent->gent->client->facial_blink = -(cg.time + 99999999.0f); // set blink timer
				}
				else
				{
					cent->gent->client->facial_blink = -(cg.time + 300.0f); // set blink timer
				}
			}
		}

		if (gi.VoiceVolume[cent->gent->s.client_num] > 0)
			// if we aren't talking, then it will be 0, -1 for talking but paused
		{
			anim = FACE_TALK1 + gi.VoiceVolume[cent->gent->s.client_num] - 1;
			cent->gent->client->facial_timer = cg.time + Q_flrand(2000.0, 7000.0);
			if (cent->gent->client->breathPuffTime > cg.time + 300)
			{
				//when talking, do breath puff
				cent->gent->client->breathPuffTime = cg.time;
			}
		}
		else if (gi.VoiceVolume[cent->gent->s.client_num] == -1)
		{
			//talking but silent
			anim = FACE_TALK0;
			cent->gent->client->facial_timer = cg.time + Q_flrand(2000.0, 7000.0);
		}
		else if (gi.VoiceVolume[cent->gent->s.client_num] == 0) //don't do aux if in a slient part of speech
		{
			//not talking
			if (cent->gent->client->facial_timer < 0) // are we auxing ?
			{
				//yes
				if (-cent->gent->client->facial_timer < cg.time) // are we done auxing ?
				{
					// yes, reset aux timer
					cent->gent->client->facial_timer = cg.time + Q_flrand(7000.0, 10000.0);
				}
				else
				{
					// not yet, so choose anim
					anim = cent->gent->client->facial_anim;
				}
			}
			else // no we aren't auxing
			{
				// but should we start ?
				if (cent->gent->client->facial_timer < cg.time)
				{
					//yes
					cent->gent->client->facial_anim = FACE_ALERT + Q_irand(0, 2); //alert, smile, frown
					// set aux timer
					cent->gent->client->facial_timer = -(cg.time + 2000.0);
					anim = cent->gent->client->facial_anim;
				}
			}
		} //talking
	} //dead
	if (anim != -1)
	{
		CG_G2SetHeadAnim(cent, anim);
		return qtrue;
	}
	return qfalse;
}

/*
-------------------------
CG_PlayerHeadExtension
-------------------------
*/
/*
int CG_PlayerHeadExtension( centity_t *cent, refEntity_t *head )
{
	clientInfo_t	*ci = &cent->gent->client->clientInfo;

	// if we have facial texture extensions, go get the sound override and add it to the face skin
	// if we aren't talking, then it will be 0
	if (ci->extensions && (gi.VoiceVolume[cent->gent->s.client_num] > 0))
	{//FIXME: When talking, look at talkTarget, if any
		//ALSO: When talking, add a head bob/movement on syllables - when gi.VoiceVolume[] changes drastically

		if ( cent->gent->health <= 0 )
		{//Dead people close their eyes and don't make faces!  They also tell no tales...  BUM BUM BAHHHHHHH!
			//Make them always blink and frown
			head->customSkin = ci->headSkin + 3;
			return qtrue;
		}

		head->customSkin = ci->headSkin + 4+gi.VoiceVolume[cent->gent->s.client_num];
		//reset the frown and blink timers
	}
	else
	// ok, we have facial extensions, but we aren't speaking. Lets decide if we need to frown or blink
	if (ci->extensions)
	{
		int	add_in = 0;

		// deal with blink first

		//Dead people close their eyes and don't make faces!  They also tell no tales...  BUM BUM BAHHHHHHH!
		if ( cent->gent->health <= 0 )
		{
			//Make them always blink and frown
			head->customSkin = ci->headSkin + 3;
			return qtrue;
		}

		if (!cent->gent->client->facial_blink)
		{	// reset blink timer
			cent->gent->client->facial_blink = cg.time + Q_flrand(3000.0, 5000.0);
			cent->gent->client->facial_frown = cg.time + Q_flrand(6000.0, 10000.0);
			cent->gent->client->facial_aux = cg.time + Q_flrand(6000.0, 10000.0);
		}

		// now deal with auxing
		// are we frowning ?
		if (cent->gent->client->facial_aux < 0)
		{
			// are we done frowning ?
			if (-(cent->gent->client->facial_aux) < cg.time)
			{
				// reset frown timer
				cent->gent->client->facial_aux = cg.time + Q_flrand(6000.0, 10000.0);
			}
			else
			{
				// yes so set offset to frown
				add_in = 4;
			}
		}
		// no we aren't frowning
		else
		{
			// but should we start ?
			if (cent->gent->client->facial_aux < cg.time)
			{
				add_in = 4;
				// set blink timer
				cent->gent->client->facial_aux = -(cg.time + 3000.0);
			}
		}

		// now, if we aren't auxing - lets see if we should be blinking or frowning
		if (!add_in)
		{
			if( gi.VoiceVolume[cent->gent->s.client_num] == -1 )
			{//then we're talking and don't want to use blinking normal frames, force open eyes.
				add_in = 0;
				// reset blink timer
				cent->gent->client->facial_blink = cg.time + Q_flrand(3000.0, 5000.0);
			}
			// are we blinking ?
			else if (cent->gent->client->facial_blink < 0)
			{
				// yes so set offset to blink
				add_in = 1;

				// are we done blinking ?
				if (-(cent->gent->client->facial_blink) < cg.time)
				{
					add_in = 0;
					// reset blink timer
					cent->gent->client->facial_blink = cg.time + Q_flrand(3000.0, 5000.0);
				}
			}
			// no we aren't blinking
			else
			{
				// but should we start ?
				if (cent->gent->client->facial_blink < cg.time)
				{
					add_in = 1;
					// set blink timer
					cent->gent->client->facial_blink = -(cg.time + 200.0);
				}
			}

			// now deal with frowning
			// are we frowning ?
			if (cent->gent->client->facial_frown < 0)
			{
				// yes so set offset to frown
				add_in += 2;

				// are we done frowning ?
				if (-(cent->gent->client->facial_frown) < cg.time)
				{
					add_in -= 2;
					// reset frown timer
					cent->gent->client->facial_frown = cg.time + Q_flrand(6000.0, 10000.0);
				}
			}
			// no we aren't frowning
			else
			{
				// but should we start ?
				if (cent->gent->client->facial_frown < cg.time)
				{
					add_in += 2;
					// set blink timer
					cent->gent->client->facial_frown = -(cg.time + 3000.0);
				}
			}
		}

		// add in whatever we should
		head->customSkin = ci->headSkin + add_in;
	}
	// at this point, we don't have any facial extensions, so who cares ?
	else
	{
		head->customSkin = ci->headSkin;
	}

	return qtrue;
}
*/

//--------------------------------------------------------------
// CG_GetTagWorldPosition
//
// Can pass in NULL for the axis
//--------------------------------------------------------------
void CG_GetTagWorldPosition(refEntity_t* model, const char* tag, vec3_t pos, vec3_t axis[3])
{
	orientation_t orientation;

	// Get the requested tag
	cgi_R_LerpTag(&orientation, model->hModel, model->oldframe, model->frame,
		1.0f - model->backlerp, tag);

	VectorCopy(model->origin, pos);
	for (int i = 0; i < 3; i++)
	{
		VectorMA(pos, orientation.origin[i], model->axis[i], pos);
	}

	if (axis)
	{
		MatrixMultiply(orientation.axis, model->axis, axis);
	}
}

static qboolean calcedMp = qfalse;

/*
-------------------------
CG_GetPlayerLightLevel
-------------------------
*/

static void CG_GetPlayerLightLevel(const centity_t* cent)
{
	vec3_t ambient = { 0 }, directed, lightDir;

	//Poll the renderer for the light level
	if (cent->currentState.client_num == cg.snap->ps.client_num)
	{
		//hAX0R
		ambient[0] = 666;
	}
	cgi_R_GetLighting(cent->lerpOrigin, ambient, directed, lightDir);

	//Get the maximum value for the player
	cent->gent->lightLevel = directed[0];

	if (directed[1] > cent->gent->lightLevel)
		cent->gent->lightLevel = directed[1];

	if (directed[2] > cent->gent->lightLevel)
		cent->gent->lightLevel = directed[2];

	if (cent->gent->client->ps.weapon == WP_SABER && cent->gent->client->ps.SaberLength() > 0)
	{
		cent->gent->lightLevel += cent->gent->client->ps.SaberLength() / cent->gent->client->ps.SaberLengthMax() *
			200;
	}
}

/*
===============
CG_StopWeaponSounds

Stops any weapon sounds as needed
===============
*/
static void CG_StopWeaponSounds(centity_t* cent)
{
	const weaponInfo_t* weapon = &cg_weapons[cent->currentState.weapon];

	if (cent->currentState.weapon == WP_SABER)
	{
		if (cent->gent && cent->gent->client)
		{
			if (!cent->gent->client->ps.SaberActive())
			{
				//neither saber is on
				return;
			}
			if (cent->gent->client->ps.saberInFlight)
			{
				//throwing saber
				if (!cent->gent->client->ps.dualSabers || !cent->gent->client->ps.saber[1].Active())
				{
					//don't have a second saber or it's not on
					return;
				}
			}
		}

		cgi_S_AddLoopingSound(cent->currentState.number,
			cent->lerpOrigin,
			vec3_origin,
			cgs.sound_precache[g_entities[cent->currentState.client_num].client->ps.saber[0].
			soundLoop]);
		return;
	}

	if (cent->currentState.weapon == WP_STUN_BATON || cent->currentState.weapon == WP_CONCUSSION)
	{
		//idling sounds
		if (cent->currentState.weapon == WP_STUN_BATON && cent->currentState.eFlags & EF_ALT_FIRING)
		{
			cgi_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->altFiringSound);
		}
		else
		{
			cgi_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->firingSound);
		}
		return;
	}

	if (!(cent->currentState.eFlags & (EF_FIRING | EF_ALT_FIRING)))
	{
		if (cent->pe.lightningFiring)
		{
			if (weapon->stopSound)
			{
				cgi_S_StartSound(cent->lerpOrigin, cent->currentState.number, CHAN_WEAPON, weapon->stopSound);
			}

			cent->pe.lightningFiring = qfalse;
		}
		return;
	}

	if (cent->currentState.eFlags & EF_ALT_FIRING)
	{
		if (weapon->altFiringSound)
		{
			cgi_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->altFiringSound);
		}
		cent->pe.lightningFiring = qtrue;
	}
	else if (cent->currentState.eFlags & EF_FIRING)
	{
		cent->pe.lightningFiring = qtrue;
	}
}

//--------------- SABER STUFF --------
extern void CG_Smoke(vec3_t origin, vec3_t dir, float radius, float speed, qhandle_t shader, int flags);

void CG_SaberDoWeaponHitMarks(const gclient_t* client, const gentity_t* saber_ent, gentity_t* hit_ent,
	const int saber_num, const int blade_num, vec3_t
	hit_pos, vec3_t hit_dir, vec3_t uaxis, const float size_time_scale)
{
	if (client
		&& size_time_scale > 0.0f
		&& hit_ent
		&& hit_ent->client
		&& hit_ent->ghoul2.size())
	{
		//burn mark with glow
		int life_time = (1.01 - static_cast<float>(hit_ent->health) / hit_ent->max_health) * static_cast<float>(
			Q_irand(5000, 10000));
		float size;
		int weapon_mark_shader = 0, mark_shader = cgs.media.bdecal_saberglowmark;

		//First: do mark decal on hit_ent
		if (WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num))
		{
			if (client->ps.saber[saber_num].g2MarksShader2[0])
			{
				//we have a shader to use instead of the standard mark shader
				mark_shader = cgi_R_RegisterShader(client->ps.saber[saber_num].g2MarksShader2);
				life_time = Q_irand(20000, 30000); //last longer if overridden
			}
		}
		else
		{
			if (client->ps.saber[saber_num].g2MarksShader[0])
			{
				//we have a shader to use instead of the standard mark shader
				mark_shader = cgi_R_RegisterShader(client->ps.saber[saber_num].g2MarksShader);
				life_time = Q_irand(20000, 30000); //last longer if overridden
			}
		}

		if (mark_shader)
		{
			life_time = ceil(static_cast<float>(life_time) * size_time_scale);
			size = Q_flrand(2.0f, 3.0f) * size_time_scale;
			CG_AddGhoul2Mark(mark_shader, size, hit_pos, hit_dir, hit_ent->s.number,
				hit_ent->client->ps.origin, hit_ent->client->renderInfo.legsYaw, hit_ent->ghoul2,
				hit_ent->s.modelScale,
				life_time, 0, uaxis);
		}

		//now do weaponMarkShader - splashback decal on weapon
		if (WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num))
		{
			if (client->ps.saber[saber_num].g2WeaponMarkShader2[0])
			{
				//we have a shader to use instead of the standard mark shader
				weapon_mark_shader = cgi_R_RegisterShader(client->ps.saber[saber_num].g2WeaponMarkShader2);
				life_time = Q_irand(7000, 12000); //last longer if overridden
			}
		}
		else
		{
			if (client->ps.saber[saber_num].g2WeaponMarkShader[0])
			{
				//we have a shader to use instead of the standard mark shader
				weapon_mark_shader = cgi_R_RegisterShader(client->ps.saber[saber_num].g2WeaponMarkShader);
				life_time = Q_irand(7000, 12000); //last longer if overridden
			}
		}

		if (weapon_mark_shader)
		{
			centity_t* splatter_on_cent = saber_ent && client->ps.saberInFlight
				? &cg_entities[saber_ent->s.number]
				: &cg_entities[client->ps.client_num];
			float yaw_angle;
			vec3_t back_dir;
			VectorScale(hit_dir, -1, back_dir);
			if (!splatter_on_cent->gent->client)
			{
				yaw_angle = splatter_on_cent->lerpAngles[YAW];
			}
			else
			{
				yaw_angle = splatter_on_cent->gent->client->renderInfo.legsYaw;
			}
			life_time = ceil(static_cast<float>(life_time) * size_time_scale);
			size = Q_flrand(1.0f, 3.0f) * size_time_scale;
			if (splatter_on_cent->gent->ghoul2.size() > saber_num + 1)
			{
				CG_AddGhoul2Mark(weapon_mark_shader, size, hit_pos, back_dir, splatter_on_cent->currentState.number,
					splatter_on_cent->lerpOrigin, yaw_angle, splatter_on_cent->gent->ghoul2,
					splatter_on_cent->currentState.modelScale,
					life_time, saber_num + 1, uaxis/*splashBackDir*/);
			}
		}
	}
}

static void CG_RGBForSaberColor(const saber_colors_t color, vec3_t rgb)
{
	constexpr int saber_num = 0;
	const saberInfo_t* saber_info = &cg_entities->gent->client->ps.saber[saber_num];

	switch (color)
	{
	case SABER_RED:
		VectorSet(rgb, 1.0f, 0.2f, 0.2f);
		break;
	case SABER_ORANGE:
		VectorSet(rgb, 1.0f, 0.5f, 0.1f);
		break;
	case SABER_YELLOW:
		VectorSet(rgb, 1.0f, 1.0f, 0.2f);
		break;
	case SABER_GREEN:
		VectorSet(rgb, 0.2f, 1.0f, 0.2f);
		break;
	case SABER_BLUE:
		VectorSet(rgb, 0.2f, 0.4f, 1.0f);
		break;
	case SABER_PURPLE:
		VectorSet(rgb, 0.9f, 0.2f, 1.0f);
		break;
	case SABER_UNSTABLE_RED:
		VectorSet(rgb, 1.0f, 0.2f, 0.2f);
		break;
	case SABER_RGB:
		VectorSet(rgb, (color & 0xff) / 255.0f, (color >> 8 & 0xff) / 255.0f, (color >> 16 & 0xff) / 255.0f);
		break;
	case SABER_BLACK:
		VectorSet(rgb, 1.0f, 1.0f, 1.0f);
		break;
	case SABER_WHITE:
		VectorSet(rgb, 1.0f, 1.0f, 1.0f);
		break;
		// Custom saber glow, blade & dlight color code
	case SABER_CUSTOM:
		VectorSet(rgb, saber_info->saberDLightColor[0], saber_info->saberDLightColor[1],
			saber_info->saberDLightColor[2]);
		break;
	default:;
		VectorSet(rgb, (color & 0xff) / 255.0f, (color >> 8 & 0xff) / 255.0f, (color >> 16 & 0xff) / 255.0f);
		break;
	}
}

static void CG_DoSaberLight(const saberInfo_t* saber)
{
	int first_blade = 0;
	//RGB combine all the colors of the sabers you're using into one averaged color!
	if (!saber)
	{
		return;
	}

	int last_blade = saber->numBlades - 1;

	if (saber->saberFlags2 & SFL2_NO_DLIGHT)
	{
		if (saber->bladeStyle2Start > 0)
		{
			if (saber->saberFlags2 & SFL2_NO_DLIGHT2)
			{
				return;
			}
			first_blade = saber->bladeStyle2Start;
		}
		else
		{
			return;
		}
	}
	else if (saber->bladeStyle2Start > 0)
	{
		if (saber->saberFlags2 & SFL2_NO_DLIGHT2)
		{
			last_blade = saber->bladeStyle2Start;
		}
	}

	vec3_t positions[MAX_BLADES * 2]{}, mid = { 0 }, rgbs[MAX_BLADES * 2]{}, rgb = { 0 };
	float lengths[MAX_BLADES * 2] = { 0 };
	float totallength = 0;
	float numpositions = 0;
	float diameter = 0;
	int i;

	if (saber)
	{
		for (i = first_blade; i <= last_blade; i++)
		{
			if (saber->blade[i].length >= MIN_SABERBLADE_DRAW_LENGTH)
			{
				//FIXME: make RGB sabers
				CG_RGBForSaberColor(saber->blade[i].color, rgbs[i]);
				lengths[i] = saber->blade[i].length;
				if (saber->blade[i].length * 2.0f > diameter)
				{
					diameter = saber->blade[i].length * 2.0f;
				}
				totallength += saber->blade[i].length;
				VectorMA(saber->blade[i].muzzlePoint, saber->blade[i].length, saber->blade[i].muzzleDir, positions[i]);
				if (!numpositions)
				{
					//first blade, store middle of that as midpoint
					VectorMA(saber->blade[i].muzzlePoint, saber->blade[i].length * 0.5, saber->blade[i].muzzleDir, mid);
					VectorCopy(rgbs[i], rgb);
				}
				numpositions++;
			}
		}
	}

	if (totallength)
	{
		//actually have something to do
		if (numpositions == 1)
		{
			//only 1 blade, midpoint is already set (halfway between the start and end of that blade), rgb is already set, so it diameter
		}
		else
		{
			//multiple blades, calc averages
			VectorClear(mid);
			VectorClear(rgb);
			//now go through all the data and get the average RGB and middle position and the radius
			for (i = 0; i < MAX_BLADES * 2; i++)
			{
				if (lengths[i])
				{
					VectorMA(rgb, lengths[i], rgbs[i], rgb);
					VectorAdd(mid, positions[i], mid);
				}
			}

			//get middle rgb
			VectorScale(rgb, 1 / totallength, rgb); //get the average, normalized RGB
			//get mid position
			VectorScale(mid, 1 / numpositions, mid);
			//find the farthest distance between the blade tips, this will be our diameter
			for (i = 0; i < MAX_BLADES * 2; i++)
			{
				if (lengths[i])
				{
					for (int j = 0; j < MAX_BLADES * 2; j++)
					{
						if (lengths[j])
						{
							const float dist = Distance(positions[i], positions[j]);
							if (dist > diameter)
							{
								diameter = dist;
							}
						}
					}
				}
			}
		}

		cgi_R_AddLightToScene(mid, diameter + Q_flrand(0.0f, 1.0f) * 8.0f, rgb[0], rgb[1], rgb[2]);
	}
}

//Movie Sabers
static void CG_DoEp1Saber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	refEntity_t saber;
	float ignite_len, ignite_radius;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redEp1GlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeEp1GlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowEp1GlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenEp1GlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueEp1GlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleEp1GlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.ep1blackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redEp1GlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep1SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}
		VectorSubtract(mid, cg.refdef.vieworg, dif);

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}
		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersGlowSizeEP1.value;
	coreradius = (radius * 0.4 * v2 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersCoreSizeEP1.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;
	coreradius *= 1.5;

	//Main Blade
	//--------------------
	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Trail Edge
	//--------------------
	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	//Fan Base
	//--------------------
	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Fan Top
	//--------------------
	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}
			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoEp2Saber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	float ignite_len, ignite_radius;
	refEntity_t saber;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redEp2GlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeEp2GlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowEp2GlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenEp2GlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueEp2GlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleEp2GlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.ep2blackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redEp2GlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep2SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersGlowSizeEP2.value;
	coreradius = (radius * 0.4 * v2 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersCoreSizeEP2.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;
	coreradius *= 1.5;

	//Main Blade
	//--------------------
	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Trail Edge
	//--------------------
	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	//Fan Base
	//--------------------
	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Fan Top
	//--------------------
	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoEp3Saber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	refEntity_t saber;
	float ignite_len, ignite_radius;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redEp3GlowShader;
		blade = cgs.media.ep3redSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeEp3GlowShader;
		blade = cgs.media.ep3orangeSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowEp3GlowShader;
		blade = cgs.media.ep3yellowSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenEp3GlowShader;
		blade = cgs.media.ep3greenSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueEp3GlowShader;
		blade = cgs.media.ep3blueSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleEp3GlowShader;
		blade = cgs.media.ep3purpleSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep3SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep3SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.ep3blackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redEp3GlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep3SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + crandoms() * 0.1f) * radiusmult * cg_SFXSabersGlowSizeEP3.value;
	coreradius = (radius * 0.4 * v2 + crandoms() * 0.1f) * radiusmult * cg_SFXSabersCoreSizeEP3.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;
	//coreradius *= 0.75;

	//Main Blade
	//--------------------
	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;

		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Trail Edge
	//--------------------
	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	//Fan Base
	//--------------------
	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Fan Top
	//--------------------
	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoSFXSaber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	float ignite_len, ignite_radius;
	refEntity_t saber;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.sfxblackSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.sfxSaberBladeShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + crandoms() * 0.1f) * radiusmult * cg_SFXSabersGlowSizeSFX.value;
	coreradius = (radius * 0.4 * v2 + crandoms() * 0.1f) * radiusmult * cg_SFXSabersCoreSizeSFX.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;
	coreradius *= 1.25;

	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoOTSaber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	int i;
	qhandle_t glow = 0, blade = 0, ignite = 0;
	float dist;
	int count;
	float ignite_len, ignite_radius;

	refEntity_t saber;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.otBlackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redOTGlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbOTGlowShader;
		blade = cgs.media.otSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + flrand(0.3f, 1.8f)) * radiusmult * cg_SFXSabersGlowSizeOT.value;
	coreradius = (radius * 0.4 * v2 + flrand(0.1f, 1.0f)) * radiusmult * cg_SFXSabersCoreSizeOT.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	coreradius *= 0.5;

	//Main Blade
	//--------------------
	{
		saber.radius = effectradius * angle_scale;

		dist = saber.radius / 4.0f;

		count = static_cast<int>(blade_len / dist);

		saber.renderfx = rfx;

		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = glow;
		saber.shaderRGBA[0] = 0xff * effectalpha;
		saber.shaderRGBA[1] = 0xff * effectalpha;
		saber.shaderRGBA[2] = 0xff * effectalpha;
		saber.shaderRGBA[3] = 0xff * effectalpha;

		if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
		}
		cgi_R_AddRefEntityToScene(&saber);

		for (i = 0; i < count; i++)
		{
			VectorMA(saber.origin, dist, blade_dir, saber.origin);
			effectradius = (radius * 1.6 * v1 + flrand(0.3f, 1.8f)/* * 0.1f*/) * radiusmult;
			saber.radius = effectradius * angle_scale;
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;
		saber.radius = coreradius;
		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Trail Edge
	//--------------------
	//GR - Adding this condition cuz when you look side to side quickly it looks a little funny
	if (end_len > 1)
	{
		saber.radius = effectradius * angle_scale;

		dist = saber.radius / 4.0f;

		count = static_cast<int>(trail_len / dist);

		saber.renderfx = rfx;

		VectorCopy(trail_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = glow;
		saber.shaderRGBA[0] = 0xff * effectalpha;
		saber.shaderRGBA[1] = 0xff * effectalpha;
		saber.shaderRGBA[2] = 0xff * effectalpha;
		saber.shaderRGBA[3] = 0xff * effectalpha;

		if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
		}
		cgi_R_AddRefEntityToScene(&saber);

		for (i = 0; i < count; i++)
		{
			VectorMA(saber.origin, dist, trail_dir, saber.origin);
			effectradius = (radius * 1.6 * v1 + flrand(0.3f, 1.8f)/* * 0.1f*/) * radiusmult;
			saber.radius = effectradius * angle_scale;
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;
		saber.radius = coreradius;
		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	//Fan Base
	//--------------------
	if (end_len > 1)
	{
		saber.radius = effectradius * angle_scale;

		dist = saber.radius / 4.0f;

		count = static_cast<int>(base_len / dist);

		saber.renderfx = rfx;

		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = glow;
		saber.shaderRGBA[0] = 0xff * effectalpha;
		saber.shaderRGBA[1] = 0xff * effectalpha;
		saber.shaderRGBA[2] = 0xff * effectalpha;
		saber.shaderRGBA[3] = 0xff * effectalpha;

		if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
		}
		cgi_R_AddRefEntityToScene(&saber);

		for (i = 0; i < count; i++)
		{
			VectorMA(saber.origin, dist, base_dir, saber.origin);
			effectradius = (radius * 1.6 * v1 + flrand(0.3f, 1.8f)) * radiusmult;
			saber.radius = effectradius * angle_scale;
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;
		saber.radius = coreradius;
		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Fan Top
	//--------------------
	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoRotJSaber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	refEntity_t saber;
	float ignite_len, ignite_radius;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redEp6GlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeEp6GlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowEp6GlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenEp6GlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueEp6GlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleEp6GlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.ep6BlackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redEp6GlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep6SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersGlowSizeROTJ.value;
	coreradius = (radius * 0.4 * v2 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersCoreSizeROTJ.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;
	coreradius *= 0.75;

	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoTFASaber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	refEntity_t saber;
	float ignite_len, ignite_radius;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redEp7GlowShader;
		blade = cgs.media.ep7redSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeEp7GlowShader;
		blade = cgs.media.ep7orangeSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowEp7GlowShader;
		blade = cgs.media.ep7yellowSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenEp7GlowShader;
		blade = cgs.media.ep7greenSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueEp7GlowShader;
		blade = cgs.media.ep7blueSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleEp7GlowShader;
		blade = cgs.media.ep7purpleSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep7SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep7SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.ep7blackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redEp7GlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.ep7SaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersGlowSizeTFA.value;
	coreradius = (radius * 0.4 * v2 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersCoreSizeTFA.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;

	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoCloakedSaber(vec3_t origin, vec3_t dir, float length, float length_max, float radius,
	saber_colors_t color, int rfx, qboolean do_light)
{
	vec3_t mid;
	qhandle_t blade = 0, glow = 0;
	refEntity_t saber;
	float radiusmult;
	qhandle_t ignite = 0;
	float ignite_len, ignite_radius;
	vec3_t rgb = { 1, 1, 1 };

	if (length < MIN_SABERBLADE_DRAW_LENGTH)
	{
		// if the thing is so short, just forget even adding me.
		return;
	}

	// Find the midpoint of the saber for lighting purposes
	VectorMA(origin, length * 0.5f, dir, mid);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_ORANGE:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_YELLOW:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_GREEN:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_BLUE:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_PURPLE:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_WHITE:
	case SABER_RGB:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_BLACK:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	default:
		glow = cgs.media.cloakedShader;
		blade = cgs.media.cloakedShader;
		ignite = cgs.media.cloakedShader;
		break;
	}

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		cgi_R_AddLightToScene(mid, length * 1.4f + Q_flrand(0.0f, 1.0f) * 3.0f, rgb[0], rgb[1], rgb[2]);
	}

	memset(&saber, 0, sizeof(refEntity_t));

	// Saber glow is it's own ref type because it uses a ton of sprites, otherwise it would eat up too many
	//	refEnts to do each glow blob individually
	saber.saberLength = length;

	// Jeff, I did this because I foolishly wished to have a bright halo as the saber is unleashed.
	// It's not quite what I'd hoped tho.  If you have any ideas, go for it!  --Pat
	if (length < length_max)
	{
		radiusmult = 1.0 + 2.0 / length; // Note this creates a curve, and length cannot be < 0.5.
	}
	else
	{
		radiusmult = 1.0;
	}

	float radius_range = radius * 0.075f;
	float radius_start = radius - radius_range;

	saber.radius = (radius_start + crandoms() * radius_range) * radiusmult;

	ignite_len = length_max * 0.30f;
	ignite_radius = saber.radius * saber.radius * 1.5f;
	ignite_radius -= length;
	ignite_radius *= 2.2f;

	VectorCopy(origin, saber.origin);
	VectorCopy(dir, saber.axis[0]);
	saber.reType = RT_SABER_GLOW;
	saber.customShader = glow;
	saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;
	saber.renderfx = rfx;

	if (color >= SABER_RGB)
	{
		saber.shaderRGBA[0] = color & 0xff;
		saber.shaderRGBA[1] = color >> 8 & 0xff;
		saber.shaderRGBA[2] = color >> 16 & 0xff;
	}

	cgi_R_AddRefEntityToScene(&saber);

	// Do the hot core
	VectorMA(origin, length, dir, saber.origin);
	VectorMA(origin, -1, dir, saber.oldorigin);

	saber.customShader = blade;
	saber.reType = RT_LINE;
	radius_start = radius / 3.0f;
	saber.radius = (radius_start + Q_flrand(-1.0f, 1.0f) * radius_range) * radiusmult;
	saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
	saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

	cgi_R_AddRefEntityToScene(&saber);

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (length <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(origin, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = color & 0xff;
			saber.shaderRGBA[1] = color >> 8 & 0xff;
			saber.shaderRGBA[2] = color >> 16 & 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoSaber(vec3_t origin, vec3_t dir, float length, float length_max, float radius, saber_colors_t color,
	int rfx, qboolean do_light)
{
	vec3_t mid;
	qhandle_t blade = 0, glow = 0;
	refEntity_t saber;
	float radiusmult;
	qhandle_t ignite = 0;
	float ignite_len, ignite_radius;
	vec3_t rgb = { 1, 1, 1 };

	if (length < MIN_SABERBLADE_DRAW_LENGTH)
	{
		// if the thing is so short, just forget even adding me.
		return;
	}

	// Find the midpoint of the saber for lighting purposes
	VectorMA(origin, length * 0.5f, dir, mid);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.redSaberCoreShader;
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeSaberGlowShader;
		blade = cgs.media.orangeSaberCoreShader;
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowSaberGlowShader;
		blade = cgs.media.yellowSaberCoreShader;
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenSaberGlowShader;
		blade = cgs.media.greenSaberCoreShader;
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueSaberGlowShader;
		blade = cgs.media.blueSaberCoreShader;
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleSaberGlowShader;
		blade = cgs.media.purpleSaberCoreShader;
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.rgbSaberCoreShader;
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.rgbSaberCoreShader;
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.blackSaberCoreShader;
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.rgbSaberCoreShader;
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		cgi_R_AddLightToScene(mid, length * 1.4f + Q_flrand(0.0f, 1.0f) * 3.0f, rgb[0], rgb[1], rgb[2]);
	}

	memset(&saber, 0, sizeof(refEntity_t));

	// Saber glow is it's own ref type because it uses a ton of sprites, otherwise it would eat up too many
	//	refEnts to do each glow blob individually
	saber.saberLength = length;

	// Jeff, I did this because I foolishly wished to have a bright halo as the saber is unleashed.
	// It's not quite what I'd hoped tho.  If you have any ideas, go for it!  --Pat
	if (length < length_max)
	{
		radiusmult = 1.0 + 2.0 / length; // Note this creates a curve, and length cannot be < 0.5.
	}
	else
	{
		radiusmult = 1.0;
	}

	float radius_range = radius * 0.075f;
	float radius_start = radius - radius_range;

	saber.radius = (radius_start + crandoms() * radius_range) * radiusmult;

	ignite_len = length_max * 0.30f;
	ignite_radius = saber.radius * saber.radius * 1.5f;
	ignite_radius -= length;
	ignite_radius *= 2.2f;

	VectorCopy(origin, saber.origin);
	VectorCopy(dir, saber.axis[0]);
	saber.reType = RT_SABER_GLOW;
	saber.customShader = glow;
	saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;
	saber.renderfx = rfx;

	if (color >= SABER_RGB)
	{
		saber.shaderRGBA[0] = color & 0xff;
		saber.shaderRGBA[1] = color >> 8 & 0xff;
		saber.shaderRGBA[2] = color >> 16 & 0xff;
	}

	cgi_R_AddRefEntityToScene(&saber);

	// Do the hot core
	VectorMA(origin, length, dir, saber.origin);
	VectorMA(origin, -1, dir, saber.oldorigin);

	saber.customShader = blade;
	saber.reType = RT_LINE;
	radius_start = radius / 3.0f;
	saber.radius = (radius_start + Q_flrand(-1.0f, 1.0f) * radius_range) * radiusmult;
	saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
	saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

	cgi_R_AddRefEntityToScene(&saber);

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (length <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(origin, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i];
				saber.shaderRGBA[3] = 255;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = color & 0xff;
			saber.shaderRGBA[1] = color >> 8 & 0xff;
			saber.shaderRGBA[2] = color >> 16 & 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = color & 0xff;
			saber.shaderRGBA[1] = color >> 8 & 0xff;
			saber.shaderRGBA[2] = color >> 16 & 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoUnstableSaber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	float ignite_len, ignite_radius;
	qhandle_t blade = 0, glow = 0, ignite = 0;
	refEntity_t saber;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.blackSaberCoreShader;
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_CUSTOM:
		glow = cgs.media.customSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		if (cg_SerenityJediEngineMode.integer == 2)
		{
			blade = cgs.media.rgbTFASaberCoreShader;
		}
		else
		{
			blade = cgs.media.unstableSaberCoreShader;
		}
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + randoms() * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}

		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6f * v1 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersGlowSizeUSB.value;
	coreradius = (radius * 0.4f * v2 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersCoreSizeUSB.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}

			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);
		saber.customShader = blade;

		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}

			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);
		saber.customShader = blade;

		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}

			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);
		saber.customShader = blade;

		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}

			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5f;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}

			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;

		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoSaberUnstable(vec3_t origin, vec3_t dir, float length, float length_max, float radius,
	saber_colors_t color, int rfx, qboolean do_light)
{
	vec3_t mid;
	qhandle_t blade = 0, glow = 0;
	refEntity_t saber;
	float radiusmult;
	qhandle_t ignite = 0;
	float ignite_len, ignite_radius;
	vec3_t rgb = { 1, 1, 1 };

	if (length < MIN_SABERBLADE_DRAW_LENGTH)
	{
		// if the thing is so short, just forget even adding me.
		return;
	}

	// Find the midpoint of the saber for lighting purposes
	VectorMA(origin, length * 0.5f, dir, mid);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.orangeSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.yellowSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.greenSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.blueSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.purpleSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.blackSaberCoreShader;
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.redSaberGlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.unstableSaberCoreShader;
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		cgi_R_AddLightToScene(mid, length * 1.4f + Q_flrand(0.0f, 1.0f) * 3.0f, rgb[0], rgb[1], rgb[2]);
	}

	memset(&saber, 0, sizeof(refEntity_t));

	// Saber glow is it's own ref type because it uses a ton of sprites, otherwise it would eat up too many
	//	refEnts to do each glow blob individually
	saber.saberLength = length;

	// Jeff, I did this because I foolishly wished to have a bright halo as the saber is unleashed.
	// It's not quite what I'd hoped tho.  If you have any ideas, go for it!  --Pat
	if (length < length_max)
	{
		radiusmult = 1.0 + 2.0 / length; // Note this creates a curve, and length cannot be < 0.5.
	}
	else
	{
		radiusmult = 1.0;
	}

	float radius_range = radius * 0.075f;
	float radius_start = radius - radius_range;

	saber.radius = (radius_start + crandoms() * radius_range) * radiusmult;

	ignite_len = length_max * 0.30f;
	ignite_radius = saber.radius * saber.radius * 1.5f;
	ignite_radius -= length;
	ignite_radius *= 2.2f;

	VectorCopy(origin, saber.origin);
	VectorCopy(dir, saber.axis[0]);
	saber.reType = RT_SABER_GLOW;
	saber.customShader = glow;
	saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;
	saber.renderfx = rfx;

	if (color >= SABER_RGB)
	{
		saber.shaderRGBA[0] = color & 0xff;
		saber.shaderRGBA[1] = color >> 8 & 0xff;
		saber.shaderRGBA[2] = color >> 16 & 0xff;
	}

	cgi_R_AddRefEntityToScene(&saber);

	// Do the hot core
	VectorMA(origin, length, dir, saber.origin);
	VectorMA(origin, -1, dir, saber.oldorigin);

	saber.customShader = blade;
	saber.reType = RT_LINE;
	radius_start = radius / 3.0f;
	saber.radius = (radius_start + Q_flrand(-1.0f, 1.0f) * radius_range) * radiusmult;
	saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
	saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

	cgi_R_AddRefEntityToScene(&saber);

	//Ignition Flare
	if (length <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(origin, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;

		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = color & 0xff;
			saber.shaderRGBA[1] = color >> 8 & 0xff;
			saber.shaderRGBA[2] = color >> 16 & 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

static void CG_DoRebelsSaber(centity_t* cent, vec3_t blade_muz, vec3_t blade_tip, vec3_t trail_tip, vec3_t trail_muz,
	float length_max, float radius, saber_colors_t
	color, int rfx, qboolean do_light)
{
	vec3_t dif, mid, blade_dir, end_dir, trail_dir, base_dir;
	float radiusmult, effectradius, coreradius, effectalpha, angle_scale;
	float blade_len, end_len, trail_len, base_len, dis_tip, dis_muz, dis_dif;
	float v1, v2;
	vec3_t rgb = { 1, 1, 1 };
	qhandle_t glow = 0, blade = 0, ignite = 0;
	refEntity_t saber;
	float ignite_len, ignite_radius;

	VectorSubtract(blade_tip, blade_muz, blade_dir);
	VectorSubtract(trail_tip, trail_muz, trail_dir);
	blade_len = VectorLength(blade_dir);
	trail_len = VectorLength(trail_dir);
	VectorNormalize(blade_dir);
	VectorNormalize(trail_dir);

	if (blade_len < MIN_SABERBLADE_DRAW_LENGTH)
	{
		return;
	}

	VectorSubtract(trail_tip, blade_tip, end_dir);
	VectorSubtract(trail_muz, blade_muz, base_dir);
	end_len = VectorLength(end_dir);
	base_len = VectorLength(base_dir);
	VectorNormalize(end_dir);
	VectorNormalize(base_dir);

	switch (color)
	{
	case SABER_RED:
		glow = cgs.media.RebelsredGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	case SABER_ORANGE:
		glow = cgs.media.RebelsorangeGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.orangeIgniteFlare;
		break;
	case SABER_YELLOW:
		glow = cgs.media.RebelsyellowGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.yellowIgniteFlare;
		break;
	case SABER_GREEN:
		glow = cgs.media.RebelsgreenGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.greenIgniteFlare;
		break;
	case SABER_BLUE:
		glow = cgs.media.RebelsblueGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.blueIgniteFlare;
		break;
	case SABER_PURPLE:
		glow = cgs.media.RebelspurpleGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.purpleIgniteFlare;
		break;
	case SABER_WHITE:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.whiteIgniteFlare;
		break;
	case SABER_RGB:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	case SABER_BLACK:
		glow = cgs.media.blackSaberGlowShader;
		blade = cgs.media.RebelsblackSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end_black");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail_black");
		ignite = cgs.media.blackIgniteFlare;
		break;
	case SABER_UNSTABLE_RED:
		glow = cgs.media.RebelsredGlowShader;
		blade = cgs.media.unstableRedSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.redIgniteFlare;
		break;
	default:
		glow = cgs.media.rgbSaberGlowShader;
		blade = cgs.media.RebelsSaberCoreShader;
		cgs.media.sfxSaberEndShader = cgi_R_RegisterShader("SFX_Sabers/saber_end");
		cgs.media.sfxSaberTrailShader = cgi_R_RegisterShader("SFX_Sabers/saber_trail");
		ignite = cgs.media.rgbIgniteFlare;
		break;
	}

	VectorMA(blade_muz, blade_len * 0.5f, blade_dir, mid);

	if (do_light)
	{
		CG_RGBForSaberColor(color, rgb);
		VectorScale(rgb, 0.66f, rgb);
		cgi_R_AddLightToScene(mid, blade_len * 2.0f + Q_flrand(0.0f, 1.0f) * 10.0f, rgb[0], rgb[1], rgb[2]);
	}

	//Distance Scale
	{
		float len;
		float glowscale = 0.5;
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(mid, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(mid, cg.refdef.vieworg, dif);
		}
		VectorSubtract(mid, cg.refdef.vieworg, dif);

		len = VectorLength(dif);
		if (len > 4000)
		{
			len = 4000;
		}
		else if (len < 1)
		{
			len = 1;
		}

		v1 = (len + 400) / 400;
		v2 = (len + 4000) / 4000;

		if (end_len > 1 || base_len > 1)
		{
			if (end_len > base_len)
				glowscale = (end_len + 4) * 0.1;
			else
				glowscale = (base_len + 4) * 0.1;

			if (glowscale > 1.0)
				glowscale = 1.0;
		}
		effectalpha = glowscale;
	}

	//Angle Scale
	{
		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_tip, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
		}
		dis_tip = VectorLength(dif);

		if (cg_SFXLerpOrigin.integer)
		{
			VectorSubtract(blade_muz, cent->lerpOrigin, dif);
		}
		else
		{
			VectorSubtract(blade_muz, cg.refdef.vieworg, dif);
		}

		dis_muz = VectorLength(dif);

		if (dis_tip > dis_muz)
		{
			dis_dif = dis_tip - dis_muz;
		}
		else if (dis_tip < dis_muz)
		{
			dis_dif = dis_muz - dis_tip;
		}
		else
		{
			dis_dif = 0;
		}

		angle_scale = 1.2f - dis_dif / blade_len * (dis_dif / blade_len);

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.2f)
			angle_scale = 0.2f;

		effectalpha *= angle_scale;

		angle_scale += 0.3f;

		if (angle_scale > 1.0f)
			angle_scale = 1.0f;
		if (angle_scale < 0.4f)
			angle_scale = 0.4f;
	}

	memset(&saber, 0, sizeof(refEntity_t));

	if (blade_len < length_max)
	{
		radiusmult = 0.5 + blade_len / length_max / 2;
	}
	else
	{
		radiusmult = 1.0;
	}

	effectradius = (radius * 1.6 * v1 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersGlowSizeRebels.value;
	coreradius = (radius * 0.4 * v2 + Q_flrand(-1.0f, 1.0f) * 0.1f) * radiusmult * cg_SFXSabersCoreSizeRebels.value;

	ignite_len = length_max * 0.30f;
	ignite_radius = effectradius * effectradius * 1.5f;
	ignite_radius -= blade_len;
	ignite_radius *= 2.2f;

	effectradius *= 0.75;
	coreradius *= 1.5;

	//Main Blade
	//--------------------
	{
		saber.renderfx = rfx;
		if (blade_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = blade_len - saber.radius / 2;
			VectorCopy(blade_muz, saber.origin);
			VectorCopy(blade_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, blade_len, blade_dir, saber.origin);
		VectorMA(blade_muz, -1, blade_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Trail Edge
	//--------------------
	{
		saber.renderfx = rfx;
		if (trail_len - effectradius * angle_scale / 2 > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = trail_len - saber.radius / 2;
			VectorCopy(trail_muz, saber.origin);
			VectorCopy(trail_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(trail_muz, trail_len, trail_dir, saber.origin);
		VectorMA(trail_muz, -1, trail_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	VectorMA(blade_muz, blade_len - 0.5, blade_dir, blade_tip);
	VectorMA(trail_muz, trail_len - 0.5, trail_dir, trail_tip);

	//Fan Base
	//--------------------
	if (base_len > 2)
	{
		saber.renderfx = rfx;
		if (base_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = base_len - effectradius * angle_scale;
			VectorMA(blade_muz, effectradius * angle_scale / 2, base_dir, saber.origin);
			VectorCopy(base_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_muz, base_len, base_dir, saber.origin);
		VectorMA(blade_muz, -0.1f, base_dir, saber.oldorigin);

		saber.customShader = blade;
		saber.reType = RT_LINE;

		saber.radius = coreradius;
		saber.saberLength = base_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Fan Top
	//--------------------
	if (end_len > 1)
	{
		{
			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(blade_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(blade_tip, cg.refdef.vieworg, dif);
			}
			dis_tip = VectorLength(dif);

			if (cg_SFXLerpOrigin.integer)
			{
				VectorSubtract(trail_tip, cent->lerpOrigin, dif);
			}
			else
			{
				VectorSubtract(trail_tip, cg.refdef.vieworg, dif);
			}

			dis_muz = VectorLength(dif);

			if (dis_tip > dis_muz)
			{
				dis_dif = dis_tip - dis_muz;
			}
			else if (dis_tip < dis_muz)
			{
				dis_dif = dis_muz - dis_tip;
			}
			else
			{
				dis_dif = 0;
			}

			if (dis_dif > end_len * 0.9)
			{
				effectalpha *= 0.3f;
			}
			else if (dis_dif > end_len * 0.8)
			{
				effectalpha *= 0.5;
			}
			else if (dis_dif > end_len * 0.7)
			{
				effectalpha *= 0.7f;
			}
		}

		saber.renderfx = rfx;
		if (end_len - effectradius * angle_scale > 0)
		{
			saber.radius = effectradius * angle_scale;
			saber.saberLength = end_len - effectradius * angle_scale;
			VectorMA(blade_tip, effectradius * angle_scale / 2, end_dir, saber.origin);
			VectorCopy(end_dir, saber.axis[0]);
			saber.reType = RT_SABER_GLOW;
			saber.customShader = glow;
			saber.shaderRGBA[0] = 0xff * effectalpha;
			saber.shaderRGBA[1] = 0xff * effectalpha;
			saber.shaderRGBA[2] = 0xff * effectalpha;
			saber.shaderRGBA[3] = 0xff * effectalpha;

			if (color >= SABER_RGB)
			{
				saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
				saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
				saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			}
			cgi_R_AddRefEntityToScene(&saber);
		}

		// Do the hot core
		VectorMA(blade_tip, end_len, end_dir, saber.origin);
		VectorMA(blade_tip, -0.1f, end_dir, saber.oldorigin);

		saber.customShader = cgs.media.sfxSaberEndShader;
		saber.reType = RT_LINE;

		if (end_len > 9)
		{
			angle_scale = 5;
		}
		else if (end_len < 3)
		{
			angle_scale = 1;
		}
		else
		{
			angle_scale = end_len / 5;
		}

		{
			angle_scale -= dis_dif / end_len * (dis_dif / end_len) * angle_scale;

			if (angle_scale < 0.8)
				angle_scale = 0.8f;
		}

		saber.radius = coreradius * angle_scale;
		saber.saberLength = end_len;

		saber.shaderTexCoord[0] = saber.shaderTexCoord[1] = 1.0f;
		saber.shaderRGBA[0] = saber.shaderRGBA[1] = saber.shaderRGBA[2] = saber.shaderRGBA[3] = 0xff;

		cgi_R_AddRefEntityToScene(&saber);
	}

	//Ignition Flare
	//--------------------
	//GR - Do the flares

	if (blade_len <= ignite_len)
	{
		int i;
		saber.renderfx = rfx;
		saber.radius = ignite_radius;
		VectorCopy(blade_muz, saber.origin);
		saber.reType = RT_SPRITE;
		saber.customShader = ignite;

		for (i = 0; i < 3; i++)
		{
			saber.shaderRGBA[i] = rgb[i];
			saber.shaderRGBA[3] = 255;
		}

		if (color == SABER_WHITE)
		{
			for (i = 0; i < 3; i++)
			{
				saber.shaderRGBA[i] = rgb[i] * effectalpha;
				saber.shaderRGBA[3] = 255 * effectalpha;
			}
		}
		else if (color >= SABER_RGB)
		{
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
		}
		else
		{
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
		}
		if (color == SABER_BLACK)
		{
			saber.customShader = cgs.media.blackIgniteFlare;
		}
		else
		{
			saber.customShader = ignite;
		}
		saber.radius = ignite_radius * 0.25f;
		saber.shaderRGBA[0] = 0xff;
		saber.shaderRGBA[1] = 0xff;
		saber.shaderRGBA[2] = 0xff;
		saber.shaderRGBA[3] = 0xff;
		cgi_R_AddRefEntityToScene(&saber);

		if (color == SABER_WHITE)
		{
			saber.customShader = cgs.media.whiteIgniteFlare;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = 0xff;
			saber.shaderRGBA[1] = 0xff;
			saber.shaderRGBA[2] = 0xff;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
		else if (color >= SABER_RGB)
		{
			saber.customShader = ignite;
			saber.radius = ignite_radius * 0.25f;
			saber.shaderRGBA[0] = (color & 0xff) * effectalpha;
			saber.shaderRGBA[1] = (color >> 8 & 0xff) * effectalpha;
			saber.shaderRGBA[2] = (color >> 16 & 0xff) * effectalpha;
			saber.shaderRGBA[3] = 0xff;
			cgi_R_AddRefEntityToScene(&saber);
		}
	}
}

constexpr auto MAX_MARK_FRAGMENTS = 128;
constexpr auto MAX_MARK_POINTS = 384;
extern markPoly_t* CG_AllocMark();

static void CG_CreateSaberMarks(vec3_t start, vec3_t end, vec3_t normal)
{
	//	byte			colors[4];
	int i, j;
	vec3_t axis[3]{}, original_points[4]{};
	vec3_t mark_points[MAX_MARK_POINTS]{}, projection;
	polyVert_t* v;
	markFragment_t mark_fragments[MAX_MARK_FRAGMENTS], * mf;

	if (!cg_addMarks.integer)
	{
		return;
	}

	VectorSubtract(end, start, axis[1]);
	VectorNormalizeFast(axis[1]);

	// create the texture axis
	VectorCopy(normal, axis[0]);
	CrossProduct(axis[1], axis[0], axis[2]);

	// create the full polygon that we'll project
	for (i = 0; i < 3; i++)
	{
		constexpr float radius = 0.65f;
		original_points[0][i] = start[i] - radius * axis[1][i] - radius * axis[2][i];
		original_points[1][i] = end[i] + radius * axis[1][i] - radius * axis[2][i];
		original_points[2][i] = end[i] + radius * axis[1][i] + radius * axis[2][i];
		original_points[3][i] = start[i] - radius * axis[1][i] + radius * axis[2][i];
	}

	VectorScale(normal, -1, projection);

	// get the fragments
	const int num_fragments = cgi_CM_MarkFragments(4, original_points,
		projection, MAX_MARK_POINTS, mark_points[0], MAX_MARK_FRAGMENTS,
		mark_fragments);

	for (i = 0, mf = mark_fragments; i < num_fragments; i++, mf++)
	{
		polyVert_t verts[MAX_VERTS_ON_POLY]{};
		// we have an upper limit on the complexity of polygons that we store persistently
		if (mf->numPoints > MAX_VERTS_ON_POLY)
		{
			mf->numPoints = MAX_VERTS_ON_POLY;
		}

		for (j = 0, v = verts; j < mf->numPoints; j++, v++)
		{
			vec3_t mid;
			vec3_t delta;

			// Set up our texture coords, this may need some work
			VectorCopy(mark_points[mf->firstPoint + j], v->xyz);
			VectorAdd(end, start, mid);
			VectorScale(mid, 0.5f, mid);
			VectorSubtract(v->xyz, mid, delta);

			v->st[0] = 0.5 + DotProduct(delta, axis[1]) * (0.05f + Q_flrand(0.0f, 1.0f) * 0.03f);
			v->st[1] = 0.5 + DotProduct(delta, axis[2]) * (0.15f + Q_flrand(0.0f, 1.0f) * 0.05f);
		}

		// save it persistantly, do burn first
		markPoly_t* mark = CG_AllocMark();
		mark->time = cg.time;
		mark->alphaFade = qtrue;
		mark->markShader = cgs.media.rivetMarkShader;
		mark->poly.numVerts = mf->numPoints;
		mark->color[0] = mark->color[1] = mark->color[2] = mark->color[3] = 255;
		memcpy(mark->verts, verts, mf->numPoints * sizeof verts[0]);

		// And now do a glow pass
		// by moving the start time back, we can hack it to fade out way before the burn does
		mark = CG_AllocMark();
		mark->time = cg.time - 8500;
		mark->alphaFade = qfalse;
		mark->markShader = cgs.media.bdecal_saberglow;
		mark->poly.numVerts = mf->numPoints;
		mark->color[0] = 215 + Q_flrand(0.0f, 1.0f) * 40.0f;
		mark->color[1] = 96 + Q_flrand(0.0f, 1.0f) * 32.0f;
		mark->color[2] = mark->color[3] = Q_flrand(0.0f, 1.0f) * 15.0f;
		memcpy(mark->verts, verts, mf->numPoints * sizeof verts[0]);
	}
}

extern void FX_AddPrimitive(CEffect** effect, int kill_time);
//-------------------------------------------------------
void CG_CheckSaberInWater(const centity_t* cent, const centity_t* scent, const int saber_num, const int modelIndex,
	vec3_t origin, vec3_t angles)
{
	gclient_t* client = cent->gent->client;
	if (!client)
	{
		return;
	}
	if (!scent ||
		modelIndex == -1 ||
		scent->gent->ghoul2.size() <= modelIndex ||
		scent->gent->ghoul2[modelIndex].mBltlist.size() <= 0 ||
		//using a camera puts away your saber so you have no bolts
		scent->gent->ghoul2[modelIndex].mModelindex == -1)
	{
		return;
	}
	if (cent && cent->gent && cent->gent->client
		&& cent->gent->client->ps.saber[saber_num].saberFlags & SFL_ON_IN_WATER)
	{
		//saber can stay on underwater
		return;
	}
	if (gi.totalMapContents() & (CONTENTS_WATER | CONTENTS_SLIME))
	{
		vec3_t saber_org;
		mdxaBone_t bolt_matrix;

		// figure out where the actual model muzzle is
		gi.G2API_GetBoltMatrix(scent->gent->ghoul2, modelIndex, 0, &bolt_matrix, angles, origin, cg.time,
			cgs.model_draw,
			scent->currentState.modelScale);
		// work the matrix axis stuff into the original axis and origins used.
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, saber_org);

		const int contents = gi.pointcontents(saber_org, cent->currentState.client_num);
		if (contents & (CONTENTS_WATER | CONTENTS_SLIME))
		{
			//still in water
			client->ps.saberEventFlags |= SEF_INWATER;
			return;
		}
	}
	//not in water
	client->ps.saberEventFlags &= ~SEF_INWATER;
}

static void CG_AddSaberBladeGo(centity_t* cent, centity_t* scent, const int renderfx, const int modelIndex,
	vec3_t origin, vec3_t angles, const int saber_num, const int blade_num)
{
	vec3_t org, end, axis[3] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };

	trace_t trace;

	float length;
	mdxaBone_t bolt_matrix;
	qboolean tag_hack = qfalse;

	gclient_t* client = cent->gent->client;

	if (!client)
	{
		return;
	}
	/*
	Ghoul2 Insert Start
	*/

	//	if (scent->gent->ghoul2.size())
	if (true)
	{
		if (!scent ||
			modelIndex == -1 ||
			scent->gent->ghoul2.size() <= modelIndex ||
			scent->gent->ghoul2[modelIndex].mModelindex == -1)
		{
			return;
		}
		// figure out where the actual model muzzle is

		//New way, multiple blade tags:
		const char* tag_name = va("*blade%d", blade_num + 1);
		int bolt = gi.G2API_AddBolt(&scent->gent->ghoul2[modelIndex], tag_name);

		if (bolt == -1)
		{
			tag_hack = qtrue; //use the hacked switch statement below to position and orient the blades
			//hmm, just fall back to the most basic tag (this will also make it work with pre-JKA saber models
			bolt = gi.G2API_AddBolt(&scent->gent->ghoul2[modelIndex], "*flash");
			if (bolt == -1)
			{
				//no tag_flash either?!!
				bolt = 0;
			}
		}

		//if there is an effect on this blade, play it
		if (!WP_SaberBladeUseSecondBladeStyle(&cent->gent->client->ps.saber[saber_num], blade_num)
			&& cent->gent->client->ps.saber[saber_num].bladeEffect)
		{
			CG_PlayEffectIDBolted(cent->gent->client->ps.saber[saber_num].bladeEffect, modelIndex, bolt,
				scent->currentState.client_num, scent->lerpOrigin, -1, qfalse);
		}
		else if (WP_SaberBladeUseSecondBladeStyle(&cent->gent->client->ps.saber[saber_num], blade_num)
			&& cent->gent->client->ps.saber[saber_num].bladeEffect2)
		{
			CG_PlayEffectIDBolted(cent->gent->client->ps.saber[saber_num].bladeEffect2, modelIndex, bolt,
				scent->currentState.client_num, scent->lerpOrigin, -1, qfalse);
		}
		//get the bolt_matrix
		gi.G2API_GetBoltMatrix(scent->gent->ghoul2, modelIndex, bolt, &bolt_matrix, angles, origin, cg.time,
			cgs.model_draw, scent->currentState.modelScale);

		// work the matrix axis stuff into the original axis and origins used.
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, org);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_X, axis[0]);
		//front (was NEGATIVE_Y, but the md3->glm exporter screws up this tag somethin' awful)
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, axis[1]); //right
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Z, axis[2]); //up
	}

	/*
	Ghoul2 Insert End
	*/
	if (tag_hack)
	{
		switch (cent->gent->client->ps.saber[saber_num].type)
		{
		case SABER_SINGLE:
		case SABER_SINGLE_CLASSIC:
		case SABER_DAGGER:
		case SABER_LANCE:
		case SABER_GRIE:
		case SABER_UNSTABLE:
		case SABER_THIN:
		case SABER_SFX:
		case SABER_CUSTOMSFX:
			break;
		case SABER_STAFF:
		case SABER_STAFF_UNSTABLE:
		case SABER_STAFF_THIN:
		case SABER_STAFF_SFX:
			if (blade_num == 1)
			{
				VectorScale(axis[0], -1, axis[0]);
				VectorMA(org, 16, axis[0], org);
			}
			break;
		case SABER_BROAD:
			if (blade_num == 0)
			{
				VectorMA(org, -1, axis[1], org);
			}
			else if (blade_num == 1)
			{
				VectorMA(org, 1, axis[1], org);
			}
			break;
		case SABER_PRONG:
			if (blade_num == 0)
			{
				VectorMA(org, -3, axis[1], org);
			}
			else if (blade_num == 1)
			{
				VectorMA(org, 3, axis[1], org);
			}
			break;
		case SABER_ARC:
			VectorSubtract(axis[1], axis[2], axis[1]);
			VectorNormalizeFast(axis[1]);
			switch (blade_num)
			{
			case 0:
				VectorMA(org, 8, axis[0], org);
				VectorScale(axis[0], 0.75f, axis[0]);
				VectorScale(axis[1], 0.25f, axis[1]);
				VectorAdd(axis[0], axis[1], axis[0]);
				break;
			case 1:
				VectorScale(axis[0], 0.25f, axis[0]);
				VectorScale(axis[1], 0.75f, axis[1]);
				VectorAdd(axis[0], axis[1], axis[0]);
				break;
			case 2:
				VectorMA(org, -8, axis[0], org);
				VectorScale(axis[0], -0.25f, axis[0]);
				VectorScale(axis[1], 0.75f, axis[1]);
				VectorAdd(axis[0], axis[1], axis[0]);
				break;
			case 3:
				VectorMA(org, -16, axis[0], org);
				VectorScale(axis[0], -0.75f, axis[0]);
				VectorScale(axis[1], 0.25f, axis[1]);
				VectorAdd(axis[0], axis[1], axis[0]);
				break;
			default:;
			}
			break;
		case SABER_SAI:
			if (blade_num == 1)
			{
				VectorMA(org, -3, axis[1], org);
			}
			else if (blade_num == 2)
			{
				VectorMA(org, 3, axis[1], org);
			}
			break;
		case SABER_CLAW:
			switch (blade_num)
			{
			case 0:
				VectorMA(org, 2, axis[0], org);
				VectorMA(org, 2, axis[2], org);
				break;
			case 1:
				VectorMA(org, 2, axis[0], org);
				VectorMA(org, 2, axis[2], org);
				VectorMA(org, 2, axis[1], org);
				break;
			case 2:
				VectorMA(org, 2, axis[0], org);
				VectorMA(org, 2, axis[2], org);
				VectorMA(org, -2, axis[1], org);
				break;
			default:;
			}
			break;
		case SABER_STAR:
			switch (blade_num)
			{
			case 0:
				VectorMA(org, 8, axis[0], org);
				break;
			case 1:
				VectorScale(axis[0], 0.33f, axis[0]);
				VectorScale(axis[2], 0.67f, axis[2]);
				VectorAdd(axis[0], axis[2], axis[0]);
				//VectorNormalize( axis_[0] );
				VectorMA(org, 8, axis[0], org);
				break;
			case 2:
				VectorScale(axis[0], -0.33f, axis[0]);
				VectorScale(axis[2], 0.67f, axis[2]);
				VectorAdd(axis[0], axis[2], axis[0]);
				//VectorNormalize( axis_[0] );
				VectorMA(org, 8, axis[0], org);
				break;
			case 3:
				VectorScale(axis[0], -1, axis[0]);
				VectorMA(org, 8, axis[0], org);
				break;
			case 4:
				VectorScale(axis[0], -0.33f, axis[0]);
				VectorScale(axis[2], -0.67f, axis[2]);
				VectorAdd(axis[0], axis[2], axis[0]);
				//VectorNormalize( axis_[0] );
				VectorMA(org, 8, axis[0], org);
				break;
			case 5:
				VectorScale(axis[0], 0.33f, axis[0]);
				VectorScale(axis[2], -0.67f, axis[2]);
				VectorAdd(axis[0], axis[2], axis[0]);
				//VectorNormalize( axis_[0] );
				VectorMA(org, 8, axis[0], org);
				break;
			default:;
			}
			break;
		case SABER_TRIDENT:
			switch (blade_num)
			{
			case 0:
				VectorMA(org, 24, axis[0], org);
				break;
			case 1:
				VectorMA(org, -6, axis[1], org);
				VectorMA(org, 24, axis[0], org);
				break;
			case 2:
				VectorMA(org, 6, axis[1], org);
				VectorMA(org, 24, axis[0], org);
				break;
			case 3:
				VectorMA(org, -32, axis[0], org);
				VectorScale(axis[0], -1, axis[0]);
				break;
			default:;
			}
			break;
		case SABER_SITH_SWORD:
			//no blade
			break;
		default:
			break;
		}
	}
	//====FIXMEFIXMEFIXMEFIXMEFIXME========================================================

	//store where saber is this frame
	VectorCopy(org, cent->gent->client->ps.saber[saber_num].blade[blade_num].muzzlePoint);
	VectorCopy(axis[0], cent->gent->client->ps.saber[saber_num].blade[blade_num].muzzleDir);
	if (saber_num == 0 && blade_num == 0)
	{
		VectorCopy(org, cent->gent->client->renderInfo.muzzlePoint);
		VectorCopy(axis[0], cent->gent->client->renderInfo.muzzleDir);
		cent->gent->client->renderInfo.mPCalcTime = cg.time;
	}
	//length for purposes of rendering and marks trace will be longer than blade so we don't damage past a wall's surface
	if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length < cent->gent->client->ps.saber[saber_num].blade[
		blade_num].lengthMax)
	{
		if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length < cent->gent->client->ps.saber[saber_num].
			blade
			[blade_num].lengthMax - 8)
		{
			length = cent->gent->client->ps.saber[saber_num].blade[blade_num].length + 8;
		}
		else
		{
			length = cent->gent->client->ps.saber[saber_num].blade[blade_num].lengthMax;
		}
	}
	else
	{
		length = cent->gent->client->ps.saber[saber_num].blade[blade_num].length;
	}
	VectorMA(org, length, axis[0], end);

	// Now store where the saber will be after next frame.
	//VectorCopy(org_future, cent->gent->client->renderInfo.muzzlePointNext);
	//VectorCopy(axis_future[0], cent->gent->client->renderInfo.muzzleDirNext);

	VectorAdd(end, axis[0], end);

	// If the saber is in flight we shouldn't trace from the player to the muzzle point
	if (cent->gent->client->ps.saberInFlight && saber_num == 0)
	{
		trace.fraction = 1.0f;
	}
	else
	{
		vec3_t root_origin;
		if (cent->gent->rootBone >= 0 && cent->gent->ghoul2.IsValid() && cent->gent->ghoul2[0].animModelIndexOffset)
			//If it has an animOffset it's a cinematic anim
		{
			//i might be running out of my bounding box, so get my root origin
			mdxaBone_t matrix;
			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->rootBone,
				&matrix, angles, cent->lerpOrigin,
				cg.time, cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN, root_origin);
		}
		else
		{
			VectorCopy(cent->lerpOrigin, root_origin);
		}
		gi.trace(&trace, root_origin, nullptr, nullptr,
			cent->gent->client->ps.saber[saber_num].blade[blade_num].muzzlePoint, cent->currentState.number,
			CONTENTS_SOLID, static_cast<EG2_Collision>(0), 0);
	}

	if (trace.fraction < 1.0f)
	{
		// Saber is on the other side of a wall
		cent->gent->client->ps.saber[saber_num].blade[blade_num].length = 0.1f;
		cent->gent->client->ps.saberEventFlags &= ~SEF_INWATER;
	}
	else
	{
		extern vmCvar_t cg_saberEntMarks;
		int trace_mask = MASK_SOLID;
		qboolean no_marks = qfalse;

		if (!WP_SaberBladeUseSecondBladeStyle(&cent->gent->client->ps.saber[saber_num], blade_num)
			&& cent->gent->client->ps.saber[saber_num].saberFlags2 & SFL2_NO_IDLE_EFFECT
			|| WP_SaberBladeUseSecondBladeStyle(&cent->gent->client->ps.saber[saber_num], blade_num)
			&& cent->gent->client->ps.saber[saber_num].saberFlags2 & SFL2_NO_IDLE_EFFECT2)
		{
			//do no effects when idle
			if (!cent->gent->client->ps.saberInFlight
				&& !PM_SaberInAttack(cent->gent->client->ps.saber_move)
				&& !PM_SaberInTransitionAny(cent->gent->client->ps.saber_move)
				&& !pm_saber_in_special_attack(cent->gent->client->ps.torsoAnim))
			{
				//idle, do no marks
				no_marks = qtrue;
			}
		}
		if (cg_saberEntMarks.integer)
		{
			if (cent->gent->client->ps.saberInFlight
				|| PM_SaberInAttack(cent->gent->client->ps.saber_move)
				|| pm_saber_in_special_attack(cent->gent->client->ps.torsoAnim))
			{
				trace_mask |= CONTENTS_BODY | CONTENTS_CORPSE;
			}
		}

		for (int i = 0; i < 1; i++)
			//was 2 because it would go through architecture and leave saber trails on either side of the brush - but still looks bad if we hit a corner, blade is still 8 longer than hit
		{
			if (i)
			{
				//tracing from end to base
				gi.trace(&trace, end, nullptr, nullptr, org, cent->currentState.client_num, trace_mask,
					static_cast<EG2_Collision>(0), 0);
			}
			else
			{
				//tracing from base to end
				gi.trace(&trace, org, nullptr, nullptr, end, cent->currentState.client_num,
					trace_mask | CONTENTS_WATER | CONTENTS_SLIME, static_cast<EG2_Collision>(0), 0);
			}

			if (trace.fraction < 1.0f)
			{
				if (trace.contents & CONTENTS_WATER || trace.contents & CONTENTS_SLIME)
				{
					if (!no_marks)
					{
						/*
						if ( !(cent->gent->client->ps.saberEventFlags&SEF_INWATER) )
						{
						}
						*/
						if (!Q_irand(0, 10))
						{
							//FIXME: don't do this this way.... :)
							vec3_t spot;
							VectorCopy(trace.endpos, spot);
							spot[2] += 4;
							if (Q_irand(1, client->ps.saber[saber_num].numBlades) == 1)
							{
								theFxScheduler.PlayEffect("saber/boil", spot);
								cgi_S_StartSound(spot, -1, CHAN_AUTO,
									cgi_S_RegisterSound("sound/weapons/saber/hitwater.wav"));
							}
						}
						//cent->gent->client->ps.saberEventFlags |= SEF_INWATER;
						//don't do other trace
					}
					i = 1;
				}
				else
				{
					if (!no_marks)
					{
						if (!WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && !(client->ps.
							saber[saber_num].saberFlags2 & SFL2_NO_WALL_MARKS)
							|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && !(client->ps
								.
								saber[saber_num].saberFlags2 & SFL2_NO_WALL_MARKS2))
						{
							if (!(trace.surfaceFlags & SURF_NOIMPACT) // never spark on sky
								&& (trace.entityNum == ENTITYNUM_WORLD || cg_entities[trace.entityNum].currentState.
									solid == SOLID_BMODEL)
								&& Q_irand(1, client->ps.saber[saber_num].numBlades) == 1)
							{
								//was "sparks/spark"
								theFxScheduler.PlayEffect("sparks/spark_nosnd", trace.endpos, trace.plane.normal);
							}
						}
						// All I need is a bool to mark whether I have a previous point to work with.
						//....come up with something better..
						if (client->ps.saber[saber_num].blade[blade_num].trail.haveOldPos[i])
						{
							if (trace.entityNum == ENTITYNUM_WORLD || cg_entities[trace.entityNum].currentState.eFlags
								& EF_PERMANENT || cg_entities[trace.entityNum].currentState.eType == ET_TERRAIN)
							{
								//only put marks on architecture
								if (!WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && !(
									client->ps.saber[saber_num].saberFlags2 & SFL2_NO_WALL_MARKS)
									|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && !(
										client->ps.saber[saber_num].saberFlags2 & SFL2_NO_WALL_MARKS2))
								{
									// Let's do some cool burn/glowing mark bits!!!
									CG_CreateSaberMarks(client->ps.saber[saber_num].blade[blade_num].trail.oldPos[i],
										trace.endpos, trace.plane.normal);

									if (Q_irand(1, client->ps.saber[saber_num].numBlades) == 1)
									{
										//make a sound
										if (cg.time - cent->gent->client->ps.saberHitWallSoundDebounceTime >= 100)
										{
											//ugh, need to have a real sound debouncer... or do this game-side
											cent->gent->client->ps.saberHitWallSoundDebounceTime = cg.time;

											if (PM_SaberInAttack(cent->gent->client->ps.saber_move)
												|| pm_saber_in_special_attack(cent->gent->client->ps.torsoAnim))
											{
												cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num,
													CHAN_ITEM, cgi_S_RegisterSound(
														va("sound/weapons/saber/saberstrikewall%d.mp3",
															Q_irand(1, 17))));
											}
											else
											{
												cgi_S_StartSound(cent->lerpOrigin, cent->currentState.client_num,
													CHAN_ITEM, cgi_S_RegisterSound(
														va("sound/weapons/saber/saberhitwall%d.wav",
															Q_irand(1, 3))));
											}
										}
									}
								}
							}
							else if (!i)
							{
								//can put marks on G2 clients (but only on base to tip trace)
								gentity_t* hit_ent = &g_entities[trace.entityNum];
								vec3_t uaxis, splash_back_dir;
								VectorSubtract(client->ps.saber[saber_num].blade[blade_num].trail.oldPos[i],
									trace.endpos, uaxis);
								VectorScale(axis[0], -1, splash_back_dir);

								CG_SaberDoWeaponHitMarks(client, scent != nullptr ? scent->gent : nullptr, hit_ent,
									saber_num, blade_num, trace.endpos, axis[0], uaxis, 0.25f);
							}
						}
						else
						{
							// if we impact next frame, we'll mark a slash mark
							client->ps.saber[saber_num].blade[blade_num].trail.haveOldPos[i] = qtrue;
						}
					}
				}

				// stash point so we can connect-the-dots later
				VectorCopy(trace.endpos, client->ps.saber[saber_num].blade[blade_num].trail.oldPos[i]);
				VectorCopy(trace.plane.normal, client->ps.saber[saber_num].blade[blade_num].trail.oldNormal[i]);

				if (!i && trace.contents & (CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_SHOTCLIP))
				{
					//Now that we don't let the blade go through walls, we need to shorten the blade when it hits one
					cent->gent->client->ps.saber[saber_num].blade[blade_num].length = cent->gent->client->ps.saber[
						saber_num].blade[blade_num].length * trace.fraction;
						//this will stop damage from going through walls
						if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length <= 0.1f)
						{
							//SIGH... hack so it doesn't play the saber turn-on sound that plays when you first turn the saber on (assumed when saber is active but length is zero)
							cent->gent->client->ps.saber[saber_num].blade[blade_num].length = 0.1f;
							//FIXME: may go through walls still??
						}
						//FIXME: should probably re-extend instantly, not use the "turning-on" growth rate
				}
			}
			else
			{
				cent->gent->client->ps.saberEventFlags &= ~SEF_INWATER;
				if (client->ps.saber[saber_num].blade[blade_num].trail.haveOldPos[i])
				{
					if (!no_marks)
					{
						if (!WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && !(client->ps.
							saber[saber_num].saberFlags2 & SFL2_NO_WALL_MARKS)
							|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && !(client->ps
								.
								saber[saber_num].saberFlags2 & SFL2_NO_WALL_MARKS2))
						{
							// Hmmm, no impact this frame, but we have an old point
							// Let's put the mark there, we should use an endcap mark to close the line, but we
							//	can probably just get away with a round mark
							//CG_ImpactMark( cgs.media.rivetMarkShader, client->ps.saber[saber_num].blade[blade_num].trail.oldPos[i], client->ps.saber[saber_num].blade[blade_num].trail.oldNormal[i],
							//		0.0f, 1.0f, 1.0f, 1.0f, 1.0f, qfalse, 1.1f, qfalse );
						}
					}
				}

				// we aren't impacting, so turn off our mark tracking mechanism
				client->ps.saber[saber_num].blade[blade_num].trail.haveOldPos[i] = qfalse;
			}
		}
	}

	if (!client->ps.saber[saber_num].blade[blade_num].active && client->ps.saber[saber_num].blade[blade_num].length <=
		0)
	{
		return;
	}

	qboolean no_dlight = qfalse;

	if (client->ps.saber[saber_num].numBlades >= 3
		|| !WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.saber[saber_num].
		saberFlags2 & SFL2_NO_DLIGHT
		|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.saber[saber_num].
		saberFlags2 & SFL2_NO_DLIGHT2)
	{
		no_dlight = qtrue;
	}

	if (cg_SFXSabers.integer == 0)
	{
		if (!WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.saber[saber_num].
			trailStyle < 2
			|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.saber[saber_num].
			trailStyle2 < 2)
		{
			//okay to draw the trail
			saberTrail_t* saber_trail = &client->ps.saber[saber_num].blade[blade_num].trail;

			// if we happen to be timescaled or running in a high framerate situation, we don't want to flood
			//	the system with very small trail slices...but perhaps doing it by distance would yield better results?
			if (saber_trail->lastTime > cg.time)
			{
				saber_trail->lastTime = cg.time;
			}
			if (cg.time > saber_trail->lastTime + 2 && saber_trail->inAction) // 2ms
			{
				if (saber_trail->inAction && cg.time < saber_trail->lastTime + 300)
					// if we have a stale segment, don't draw until we have a fresh one
				{
					constexpr auto SABER_TRAIL_TIME = 40.0f;
					vec3_t rgb1 = { 255, 255, 255 };

					if (cent->gent->client->ps.saber[saber_num].type != SABER_SITH_SWORD
						&& (WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) || client->ps.
							saber[
								saber_num].trailStyle != 1)
						&& (!WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) || client->ps.
							saber
							[saber_num].trailStyle2 != 1))
					{
						switch (client->ps.saber[saber_num].blade[blade_num].color)
						{
						case SABER_RED:
							VectorSet(rgb1, 255.0f, 0.0f, 0.0f);
							break;
						case SABER_ORANGE:
							VectorSet(rgb1, 255.0f, 64.0f, 0.0f);
							break;
						case SABER_YELLOW:
							VectorSet(rgb1, 255.0f, 255.0f, 0.0f);
							break;
						case SABER_GREEN:
							VectorSet(rgb1, 0.0f, 255.0f, 0.0f);
							break;
						case SABER_BLUE:
							VectorSet(rgb1, 0.0f, 64.0f, 255.0f);
							break;
						case SABER_PURPLE:
							VectorSet(rgb1, 220.0f, 0.0f, 255.0f);
							break;
						case SABER_UNSTABLE_RED:
							VectorSet(rgb1, 255.0f, 0.0f, 0.0f);
							break;
						case SABER_BLACK:
							VectorSet(rgb1, 255.0f, 255.0f, 255.0f);
							break;
						case SABER_WHITE:
							VectorSet(rgb1, 1.0f, 1.0f, 1.0f);
							break;
						default: //SABER_RGB
							VectorSet(rgb1, client->ps.saber[saber_num].blade[blade_num].color & 0xff,
								client->ps.saber[saber_num].blade[blade_num].color >> 8 & 0xff,
								client->ps.saber[saber_num].blade[blade_num].color >> 16 & 0xff);
							break;
						}
					}

					const float diff = cg.time - saber_trail->lastTime;
					// I'm not sure that clipping this is really the best idea
					if (diff <= SABER_TRAIL_TIME * 2)
					{
						// build a quad
						auto fx = new CTrail;

						float duration;

						if (cent->gent->client->ps.saber[saber_num].type == SABER_SITH_SWORD
							|| !WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.
							saber[saber_num].trailStyle == 1
							|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.
							saber[saber_num].trailStyle2 == 1
							)
						{
							fx->mShader = cgs.media.swordTrailShader;
							duration = saber_trail->duration / 2.0f; // stay around twice as long
							VectorSet(rgb1, 32.0f, 32.0f, 32.0f); // make the sith sword trail pretty faint
						}
						else if (client->ps.saber[saber_num].blade[blade_num].color == SABER_BLACK)
						{
							fx->mShader = cgs.media.blackSaberBlurShader;
							duration = saber_trail->duration / 5.0f;
						}
						else if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE
							|| cent->gent->client->ps.saber[saber_num].type == SABER_STAFF_UNSTABLE)
						{
							fx->mShader = cgs.media.unstableBlurShader;
							duration = saber_trail->duration / 5.0f;
						}
						else
						{
							fx->mShader = cgs.media.saberBlurShader;
							duration = saber_trail->duration / 5.0f;
						}

						const float old_alpha = 1.0f - diff / duration;

						// Go from new muzzle to new end...then to old end...back down to old muzzle...finally
						//	connect back to the new muzzle...this is our trail quad
						VectorCopy(org, fx->mVerts[0].origin);
						VectorMA(end, 3.0f, axis[0], fx->mVerts[1].origin);

						VectorCopy(saber_trail->tip, fx->mVerts[2].origin);
						VectorCopy(saber_trail->base, fx->mVerts[3].origin);

						// New muzzle
						VectorCopy(rgb1, fx->mVerts[0].rgb);
						fx->mVerts[0].alpha = 255.0f;

						fx->mVerts[0].ST[0] = 0.0f;
						fx->mVerts[0].ST[1] = 0.99f;
						fx->mVerts[0].destST[0] = 0.99f;
						fx->mVerts[0].destST[1] = 0.99f;

						// new tip
						VectorCopy(rgb1, fx->mVerts[1].rgb);
						fx->mVerts[1].alpha = 255.0f;

						fx->mVerts[1].ST[0] = 0.0f;
						fx->mVerts[1].ST[1] = 0.0f;
						fx->mVerts[1].destST[0] = 0.99f;
						fx->mVerts[1].destST[1] = 0.0f;

						// old tip
						VectorCopy(rgb1, fx->mVerts[2].rgb);
						fx->mVerts[2].alpha = 255.0f;

						fx->mVerts[2].ST[0] = 0.99f - old_alpha; // NOTE: this just happens to contain the value I want
						fx->mVerts[2].ST[1] = 0.0f;
						fx->mVerts[2].destST[0] = 0.99f + fx->mVerts[2].ST[0];
						fx->mVerts[2].destST[1] = 0.0f;

						// old muzzle
						VectorCopy(rgb1, fx->mVerts[3].rgb);
						fx->mVerts[3].alpha = 255.0f;

						fx->mVerts[3].ST[0] = 0.99f - old_alpha; // NOTE: this just happens to contain the value I want
						fx->mVerts[3].ST[1] = 0.99f;
						fx->mVerts[3].destST[0] = 0.99f + fx->mVerts[2].ST[0];
						fx->mVerts[3].destST[1] = 0.99f;

						//				fx->SetFlags( FX_USE_ALPHA );
						FX_AddPrimitive(reinterpret_cast<CEffect**>(&fx), duration); //SABER_TRAIL_TIME );
					}
				}

				// we must always do this, even if we aren't active..otherwise we won't know where to pick up from
				VectorCopy(org, saber_trail->base);
				VectorMA(end, 3.0f, axis[0], saber_trail->tip);
				saber_trail->lastTime = cg.time;
			}
		}
		if (cent->gent->client->ps.saber[saber_num].type == SABER_SITH_SWORD)
		{
			// don't need to do nuthin else
			return;
		}

		if (!WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.saber[saber_num].
			saberFlags2 & SFL2_NO_BLADE
			|| WP_SaberBladeUseSecondBladeStyle(&client->ps.saber[saber_num], blade_num) && client->ps.saber[saber_num].
			saberFlags2 & SFL2_NO_BLADE2)
		{
			//don't draw a blade
			if (!no_dlight)
			{
				//but still do dlight
				CG_DoSaberLight(&client->ps.saber[saber_num]);
			}
			return;
		}

		if (cent->currentState.powerups & 1 << PW_CLOAKED || cent->currentState.powerups & 1 << PW_UNCLOAKING)
		{
			CG_DoCloakedSaber(org, axis[0], length, client->ps.saber[saber_num].blade[blade_num].lengthMax,
				client->ps.saber[saber_num].blade[blade_num].radius,
				client->ps.saber[saber_num].blade[blade_num].color, renderfx,
				static_cast<qboolean>(!no_dlight));
		}
		else
		{
			if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
				saber_num].
				type == SABER_STAFF_UNSTABLE)
			{
				CG_DoSaberUnstable(org, axis[0], length, client->ps.saber[saber_num].blade[blade_num].lengthMax,
					client->ps.saber[saber_num].blade[blade_num].radius,
					client->ps.saber[saber_num].blade[blade_num].color, renderfx,
					static_cast<qboolean>(!no_dlight));
			}
			else
			{
				CG_DoSaber(org, axis[0], length, client->ps.saber[saber_num].blade[blade_num].lengthMax,
					client->ps.saber[saber_num].blade[blade_num].radius,
					client->ps.saber[saber_num].blade[blade_num].color, renderfx,
					static_cast<qboolean>(!no_dlight));
			}
		}
	}
	else
	{
		saberTrail_t* saber_trail = &client->ps.saber[saber_num].blade[blade_num].trail;
		saber_trail->duration = 0;

		if (saber_trail->lastTime > cg.time)
		{
			//after a pause, cg.time jumps ahead in time for one frame
			//and lastTime gets set to that and will freak out, so, since
			//it's never valid for saberTrail->lastTime to be > cg.time,
			//cap it to cg.time here
			saber_trail->lastTime = cg.time;
		}

		if (!saber_trail->base || !saber_trail->tip || !saber_trail->dualtip || !saber_trail->dualbase || !saber_trail->
			lastTime/* || !saberTrail->inAction*/)
		{
			VectorCopy(org, saber_trail->base);
			VectorMA(end, -1.5f, axis[0], saber_trail->tip);
			VectorCopy(saber_trail->tip, saber_trail->dualtip);
			VectorCopy(saber_trail->base, saber_trail->dualbase);
			saber_trail->lastTime = cg.time;
			saber_trail->inAction = cg.time;
			return;
		}
		if (cg.time > saber_trail->lastTime)
		{
			vec3_t dir0, dir1, dir2;

			VectorCopy(saber_trail->base, saber_trail->dualbase);
			VectorCopy(saber_trail->tip, saber_trail->dualtip);

			VectorCopy(org, saber_trail->base);
			VectorMA(end, -1.5f, axis[0], saber_trail->tip);

			VectorSubtract(saber_trail->dualtip, saber_trail->tip, dir0);
			VectorSubtract(saber_trail->dualbase, saber_trail->base, dir1);
			VectorSubtract(saber_trail->dualtip, saber_trail->dualbase, dir2);

			float dirlen0 = VectorLength(dir0);
			float dirlen1 = VectorLength(dir1);
			const float dirlen2 = VectorLength(dir2);

			if (saber_moveData[client->ps.saber_move].trailLength == 0)
			{
				dirlen0 *= 0.5f;
				dirlen1 *= 0.3f;
			}
			else
			{
				dirlen0 *= 1.0f;
				dirlen1 *= 0.5f;
			}

			float lagscale = cg.time - saber_trail->lastTime;
			lagscale = 1 - lagscale * 3 / 200;

			if (lagscale < 0.1f)
				lagscale = 0.1f;

			VectorNormalize(dir0);
			VectorNormalize(dir1);

			VectorMA(saber_trail->tip, dirlen0 * lagscale, dir0, saber_trail->dualtip);
			VectorMA(saber_trail->base, dirlen1 * lagscale, dir1, saber_trail->dualbase);
			VectorSubtract(saber_trail->dualtip, saber_trail->dualbase, dir1);
			VectorNormalize(dir1);

			VectorMA(saber_trail->dualbase, dirlen2, dir1, saber_trail->dualtip);

			saber_trail->lastTime = cg.time;
		}

		vec3_t rgb1 = { 255.0f, 255.0f, 255.0f };

		switch (client->ps.saber[saber_num].blade[blade_num].color)
		{
		case SABER_RED:
			VectorSet(rgb1, 255.0f, 0.0f, 0.0f);
			break;
		case SABER_ORANGE:
			VectorSet(rgb1, 253.0f, 125.0f, 80.0f);
			break;
		case SABER_YELLOW:
			VectorSet(rgb1, 250.0f, 250.0f, 160.0f);
			break;
		case SABER_GREEN:
			VectorSet(rgb1, 100.0f, 240.0f, 100.0f);
			break;
		case SABER_PURPLE:
			VectorSet(rgb1, 196.0f, 0.0f, 196.0f);
			break;
		case SABER_BLUE:
			VectorSet(rgb1, 0.0f, 0.0f, 255.0f);
			break;
		case SABER_UNSTABLE_RED:
			VectorSet(rgb1, 255.0f, 0.0f, 0.0f);
			break;
		case SABER_BLACK:
			VectorSet(rgb1, 255.0f, 255.0f, 255.0f);
			break;
		case SABER_CUSTOM:
			VectorSet(rgb1, 0.92f, 0.24f, 0.72f);
			break;
		default: //SABER_RGB
			VectorSet(rgb1, client->ps.saber[saber_num].blade[blade_num].color & 0xff,
				client->ps.saber[saber_num].blade[blade_num].color >> 8 & 0xff,
				client->ps.saber[saber_num].blade[blade_num].color >> 16 & 0xff);
			break;
		}

		auto fx = new CTrail;

		VectorCopy(saber_trail->base, fx->mVerts[0].origin);
		VectorCopy(saber_trail->tip, fx->mVerts[1].origin);
		VectorCopy(saber_trail->dualtip, fx->mVerts[2].origin);
		VectorCopy(saber_trail->dualbase, fx->mVerts[3].origin);

		if (cent->currentState.powerups & 1 << PW_CLOAKED || cent->currentState.powerups & 1 << PW_UNCLOAKING)
		{
			CG_DoCloakedSaber(org, axis[0], length, client->ps.saber[saber_num].blade[blade_num].lengthMax,
				client->ps.saber[saber_num].blade[blade_num].radius,
				client->ps.saber[saber_num].blade[blade_num].color, renderfx,
				static_cast<qboolean>(!no_dlight));
		}
		else
		{
			if (cg_SFXSabers.integer < 1)
			{
				if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
					saber_num].type == SABER_STAFF_UNSTABLE)
				{
					CG_DoSaberUnstable(org, axis[0], length, client->ps.saber[saber_num].blade[blade_num].lengthMax,
						client->ps.saber[saber_num].blade[blade_num].radius,
						client->ps.saber[saber_num].blade[blade_num].color, renderfx,
						static_cast<qboolean>(!no_dlight));
				}
				else
				{
					CG_DoSaber(org, axis[0], length, client->ps.saber[saber_num].blade[blade_num].lengthMax,
						client->ps.saber[saber_num].blade[blade_num].radius,
						client->ps.saber[saber_num].blade[blade_num].color, renderfx,
						static_cast<qboolean>(!no_dlight));
				}
			}
			else if (!(cent->gent->client->ps.saber[saber_num].type == SABER_SITH_SWORD || client->ps.saber[saber_num].
				saberFlags2 & SFL2_NO_BLADE))
			{
				switch (cg_SFXSabers.integer)
				{
				case 1:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoEp1Saber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 2:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoEp2Saber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 3:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoEp3Saber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 4:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoSFXSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 5:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoOTSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 6:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoRotJSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 7:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoTFASaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				case 8:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					break;
				case 9:
					if (cent->gent->client->ps.saber[saber_num].type == SABER_UNSTABLE || cent->gent->client->ps.saber[
						saber_num].type == SABER_STAFF_UNSTABLE)
					{
						CG_DoUnstableSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(no_dlight == qfalse));
					}
					else
					{
						CG_DoRebelsSaber(cent, fx->mVerts[0].origin, fx->mVerts[1].origin, fx->mVerts[2].origin,
							fx->mVerts[3].origin, client->ps.saber[saber_num].blade[blade_num].lengthMax,
							client->ps.saber[saber_num].blade[blade_num].radius,
							client->ps.saber[saber_num].blade[blade_num].color, renderfx,
							static_cast<qboolean>(client->ps.saber[saber_num].numBlades < 3 && !(client->ps
								.
								saber[saber_num].saberFlags2 &
								SFL2_NO_DLIGHT)));
					}
					break;
				default:;
				}
			}
		}

		if (cg.time > saber_trail->inAction)
		{
			saber_trail->inAction = cg.time;

			if (cent->gent->client->ps.saber[saber_num].type == SABER_SITH_SWORD || client->ps.saber[saber_num].
				trailStyle
				== 1)
			{
				fx->mShader = cgs.media.swordTrailShader;
				VectorSet(rgb1, 32.0f, 32.0f, 32.0f); // make the sith sword trail pretty faint
			}
			else
			{
				fx->mShader = cgs.media.sfxSaberTrailShader;
			}
			fx->SetFlags(FX_USE_ALPHA);

			// New muzzle
			VectorCopy(rgb1, fx->mVerts[0].rgb);
			fx->mVerts[0].alpha = 255.0f;

			fx->mVerts[0].ST[0] = 0.0f;
			fx->mVerts[0].ST[1] = 4.0f;
			fx->mVerts[0].destST[0] = 4.0f;
			fx->mVerts[0].destST[1] = 4.0f;

			// new tip
			VectorCopy(rgb1, fx->mVerts[1].rgb);
			fx->mVerts[1].alpha = 255.0f;

			fx->mVerts[1].ST[0] = 0.0f;
			fx->mVerts[1].ST[1] = 0.0f;
			fx->mVerts[1].destST[0] = 4.0f;
			fx->mVerts[1].destST[1] = 0.0f;

			// old tip
			VectorCopy(rgb1, fx->mVerts[2].rgb);
			fx->mVerts[2].alpha = 255.0f;

			fx->mVerts[2].ST[0] = 4.0f;
			fx->mVerts[2].ST[1] = 0.0f;
			fx->mVerts[2].destST[0] = 4.0f;
			fx->mVerts[2].destST[1] = 0.0f;

			// old muzzle
			VectorCopy(rgb1, fx->mVerts[3].rgb);
			fx->mVerts[3].alpha = 255.0f;

			fx->mVerts[3].ST[0] = 4.0f;
			fx->mVerts[3].ST[1] = 4.0f;
			fx->mVerts[3].destST[0] = 4.0f;
			fx->mVerts[3].destST[1] = 4.0f;

			FX_AddPrimitive(reinterpret_cast<CEffect**>(&fx), 0);
		}

		if (client->ps.saber[saber_num].saberFlags2 & SFL2_NO_BLADE)
		{
			if (!no_dlight)
			{
				CG_DoSaberLight(&client->ps.saber[saber_num]);
			}
		}
	}
}

void CG_AddSaberBlade(centity_t* cent, centity_t* scent, const int renderfx, const int modelIndex, vec3_t origin,
	vec3_t angles)
{
	//FIXME: if this is a dropped saber, it could be possible that it's the second saber?
	if (cent->gent->client)
	{
		for (int i = 0; i < cent->gent->client->ps.saber[0].numBlades; i++)
		{
			CG_AddSaberBladeGo(cent, scent, renderfx, modelIndex, origin, angles, 0, i);
		}
		if (cent->gent->client->ps.saber[0].numBlades > 2)
		{
			// add blended light
			CG_DoSaberLight(&cent->gent->client->ps.saber[0]);
		}
	}
}

//--------------- END SABER STUFF --------

/*
 ================
 GetSelfLegAnimPoint
 ================
 */
 //Get the point in the leg animation and return a percentage of the current point in the anim between 0 and the total anim length (0.0f - 1.0f)
float GetSelfLegAnimPoint()
{
	float current = 0.0f;
	int end = 0;
	int start = 0;
	if (!!gi.G2API_GetBoneAnimIndex(&
		cg_entities[cg.snap->ps.viewEntity].gent->ghoul2[cg_entities[cg.snap->ps.viewEntity]
		.gent->playerModel],
		cg_entities[cg.snap->ps.viewEntity].gent->rootBone,
		level.time,
		&current,
		&start,
		&end,
		nullptr,
		nullptr,
		nullptr))
	{
		const float percent_complete = (current - start) / (end - start);

		return percent_complete;
	}

	return 0.0f;
}

/*
 ================
 GetSelfTorsoAnimPoint

 ================
 */
 //Get the point in the torso animation and return a percentage of the current point in the anim between 0 and the total anim length (0.0f - 1.0f)
float GetSelfTorsoAnimPoint()
{
	float current = 0.0f;
	int end = 0;
	int start = 0;
	if (!!gi.G2API_GetBoneAnimIndex(&
		cg_entities[cg.snap->ps.viewEntity].gent->ghoul2[cg_entities[cg.snap->ps.viewEntity]
		.gent->playerModel],
		cg_entities[cg.snap->ps.viewEntity].gent->lowerLumbarBone,
		level.time,
		&current,
		&start,
		&end,
		nullptr,
		nullptr,
		nullptr))
	{
		const float percent_complete = (current - start) / (end - start);

		return percent_complete;
	}

	return 0.0f;
}

/*
 ===============
 SmoothTrueView

 Purpose:  Uses the currently setup model-based First Person View to calculation the final viewangles.  Features the
 following:
 1.  Simulates allowable eye movement by makes a deadzone around the inputed viewangles vs the desired
 viewangles of cg.refdef.viewangles
 2.  Prevents the sudden view flipping during moves where your camera is suppose to flip 360 on the pitch (x)
 pitch (x) axis.
 ===============
 */

static void SmoothTrueView(vec3_t eye_angles)
{
	const float leg_anim_point = GetSelfLegAnimPoint();
	const float torso_anim_point = GetSelfTorsoAnimPoint();

	qboolean eye_range = qtrue;
	qboolean use_ref_def = qfalse;
	qboolean did_special = qfalse;

	//Rolls
	if (cg_trueroll.integer)
	{
		if (cg.snap->ps.legsAnim == BOTH_WALL_RUN_LEFT
			|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_RIGHT
			|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_LEFT_STOP
			|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_RIGHT_STOP
			|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_LEFT_FLIP
			|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_RIGHT_FLIP
			|| cg.snap->ps.legsAnim == BOTH_WALL_FLIP_LEFT
			|| cg.snap->ps.legsAnim == BOTH_WALL_FLIP_RIGHT)
		{
			//Roll moves that look good with eye range
			eye_range = qtrue;
			did_special = qtrue;
		}
		else if (cg_trueroll.integer == 1)
		{
			//Use simple roll for the more complicated rolls
			if (cg.snap->ps.legsAnim == BOTH_FLIP_L
				|| cg.snap->ps.legsAnim == BOTH_ROLL_L)
			{
				//Left rolls
				VectorCopy(cg.refdefViewAngles, eye_angles);
				eye_angles[2] += AngleNormalize180(360 - 360 * leg_anim_point);
				AngleNormalize180(eye_angles[2]);
				eye_range = qfalse;
				did_special = qtrue;
			}
			else if (cg.snap->ps.legsAnim == BOTH_FLIP_R
				|| cg.snap->ps.legsAnim == BOTH_ROLL_R)
			{
				//Right rolls
				VectorCopy(cg.refdefViewAngles, eye_angles);
				eye_angles[2] += AngleNormalize180(360 * leg_anim_point);
				AngleNormalize180(eye_angles[2]);
				eye_range = qfalse;
				did_special = qtrue;
			}
		}
		else
		{
			//You're here because you're using cg_trueroll.integer == 2
			if (cg.snap->ps.legsAnim == BOTH_FLIP_L
				|| cg.snap->ps.legsAnim == BOTH_ROLL_L
				|| cg.snap->ps.legsAnim == BOTH_FLIP_R
				|| cg.snap->ps.legsAnim == BOTH_ROLL_R)
			{
				//Roll animation, lock the eyemovement
				eye_range = qfalse;
				did_special = qtrue;
			}
		}
	}
	else if (cg.snap->ps.legsAnim == BOTH_WALL_RUN_LEFT
		|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_RIGHT
		|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_LEFT_STOP
		|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_RIGHT_STOP
		|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_LEFT_FLIP
		|| cg.snap->ps.legsAnim == BOTH_WALL_RUN_RIGHT_FLIP
		|| cg.snap->ps.legsAnim == BOTH_WALL_FLIP_LEFT
		|| cg.snap->ps.legsAnim == BOTH_WALL_FLIP_RIGHT
		|| cg.snap->ps.legsAnim == BOTH_FLIP_L
		|| cg.snap->ps.legsAnim == BOTH_ROLL_L
		|| cg.snap->ps.legsAnim == BOTH_FLIP_R
		|| cg.snap->ps.legsAnim == BOTH_ROLL_R)
	{
		//you don't want rolling so use cg.refdef.viewangles as the view
		use_ref_def = qtrue;
	}

	//Flips
	if (cg_trueflip.integer)
	{
		if (cg.snap->ps.legsAnim == BOTH_WALL_FLIP_BACK1)
		{
			//Flip moves that look good with the eyemovement locked
			eye_range = qfalse;
			did_special = qtrue;
		}
		else if (cg_trueflip.integer == 1)
		{
			//Use simple flip for the more complicated flips
			if (cg.snap->ps.legsAnim == BOTH_FLIP_F
				|| cg.snap->ps.legsAnim == BOTH_FLIP_F2
				|| cg.snap->ps.legsAnim == BOTH_ROLL_F
				|| cg.snap->ps.legsAnim == BOTH_ROLL_F1
				|| cg.snap->ps.legsAnim == BOTH_ROLL_F2)
			{
				//forward flips
				VectorCopy(cg.refdefViewAngles, eye_angles);
				eye_angles[0] += AngleNormalize180(360 * leg_anim_point);
				AngleNormalize180(eye_angles[0]);
				eye_range = qfalse;
				did_special = qtrue;
			}
			else if (cg.snap->ps.legsAnim == BOTH_FLIP_B
				|| cg.snap->ps.legsAnim == BOTH_ROLL_B
				|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK1
				|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK2
				|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK3)
			{
				//back flips
				VectorCopy(cg.refdefViewAngles, eye_angles);
				eye_angles[0] += AngleNormalize180(360 - 360 * leg_anim_point);
				AngleNormalize180(eye_angles[0]);
				eye_range = qfalse;
				did_special = qtrue;
			}
		}
		else
		{
			//You're here because you're using cg_trueflip.integer = 2
			if (cg.snap->ps.legsAnim == BOTH_FLIP_F
				|| cg.snap->ps.legsAnim == BOTH_FLIP_F2
				|| cg.snap->ps.legsAnim == BOTH_ROLL_F
				|| cg.snap->ps.legsAnim == BOTH_ROLL_F1
				|| cg.snap->ps.legsAnim == BOTH_ROLL_F2
				|| cg.snap->ps.legsAnim == BOTH_FLIP_B
				|| cg.snap->ps.legsAnim == BOTH_ROLL_B
				|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK1
				|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK2
				|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK3)
			{
				//Flip animation and using cg_trueflip.integer = 2, lock the eyemovement
				eye_range = qfalse;
				did_special = qtrue;
			}
		}
	}
	else if (cg.snap->ps.legsAnim == BOTH_WALL_FLIP_BACK1
		|| cg.snap->ps.legsAnim == BOTH_FLIP_F
		|| cg.snap->ps.legsAnim == BOTH_FLIP_F2
		|| cg.snap->ps.legsAnim == BOTH_ROLL_F
		|| cg.snap->ps.legsAnim == BOTH_ROLL_F1
		|| cg.snap->ps.legsAnim == BOTH_ROLL_F2
		|| cg.snap->ps.legsAnim == BOTH_FLIP_B
		|| cg.snap->ps.legsAnim == BOTH_ROLL_B
		|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK1
		|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK2
		|| cg.snap->ps.legsAnim == BOTH_FLIP_BACK3)
	{
		//you don't want flipping so use cg.refdef.viewangles as the view
		use_ref_def = qtrue;
	}

	if (cg_truespin.integer)
	{
		if (cg_truespin.integer == 1)
		{
			//Do a simulated Spin for the more complicated spins
			if (cg.snap->ps.torsoAnim == BOTH_T1_TL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T1__L_BR
				|| cg.snap->ps.torsoAnim == BOTH_T1__L__R
				|| cg.snap->ps.torsoAnim == BOTH_T1_BL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T1_BL__R
				|| cg.snap->ps.torsoAnim == BOTH_T1_BL_TR
				|| cg.snap->ps.torsoAnim == BOTH_T2__L_BR
				|| cg.snap->ps.torsoAnim == BOTH_T2_BL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T2_BL__R
				|| cg.snap->ps.torsoAnim == BOTH_T3__L_BR
				|| cg.snap->ps.torsoAnim == BOTH_T3_BL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T3_BL__R
				|| cg.snap->ps.torsoAnim == BOTH_T4__L_BR
				|| cg.snap->ps.torsoAnim == BOTH_T4_BL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T4_BL__R
				|| cg.snap->ps.torsoAnim == BOTH_T5_TL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T5__L_BR
				|| cg.snap->ps.torsoAnim == BOTH_T5__L__R
				|| cg.snap->ps.torsoAnim == BOTH_T5_BL_BR
				|| cg.snap->ps.torsoAnim == BOTH_T5_BL__R
				|| cg.snap->ps.torsoAnim == BOTH_T5_BL_TR
				|| cg.snap->ps.torsoAnim == BOTH_ATTACK_BACK
				|| cg.snap->ps.torsoAnim == BOTH_CROUCHATTACKBACK1
				|| cg.snap->ps.torsoAnim == BOTH_BUTTERFLY_LEFT
				//This technically has 2 spins and seems to have been labeled wrong
				|| cg.snap->ps.legsAnim == BOTH_FJSS_TR_BL)
			{
				//Left Spins
				VectorCopy(cg.refdefViewAngles, eye_angles);
				eye_angles[1] += AngleNormalize180(360 * torso_anim_point);
				AngleNormalize180(eye_angles[1]);
				eye_range = qfalse;
				did_special = qtrue;
			}
			else if (cg.snap->ps.torsoAnim == BOTH_T1_BR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T1__R__L
				|| cg.snap->ps.torsoAnim == BOTH_T1__R_BL
				|| cg.snap->ps.torsoAnim == BOTH_T1_TR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T1_BR_TL
				|| cg.snap->ps.torsoAnim == BOTH_T1_BR__L
				|| cg.snap->ps.torsoAnim == BOTH_T2_BR__L
				|| cg.snap->ps.torsoAnim == BOTH_T2_BR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T2__R_BL
				|| cg.snap->ps.torsoAnim == BOTH_T3_BR__L
				|| cg.snap->ps.torsoAnim == BOTH_T3_BR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T3__R_BL
				|| cg.snap->ps.torsoAnim == BOTH_T4_BR__L
				|| cg.snap->ps.torsoAnim == BOTH_T4_BR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T4__R_BL
				|| cg.snap->ps.torsoAnim == BOTH_T5_BR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T5__R__L
				|| cg.snap->ps.torsoAnim == BOTH_T5__R_BL
				|| cg.snap->ps.torsoAnim == BOTH_T5_TR_BL
				|| cg.snap->ps.torsoAnim == BOTH_T5_BR_TL
				|| cg.snap->ps.torsoAnim == BOTH_T5_BR__L
				//This technically has 2 spins
				|| cg.snap->ps.legsAnim == BOTH_BUTTERFLY_RIGHT
				//This technically has 2 spins and seems to have been labeled wrong
				|| cg.snap->ps.legsAnim == BOTH_FJSS_TL_BR)
			{
				//Right Spins
				VectorCopy(cg.refdefViewAngles, eye_angles);
				eye_angles[1] += AngleNormalize180(360 - 360 * torso_anim_point);
				AngleNormalize180(eye_angles[1]);
				eye_range = qfalse;
				did_special = qtrue;
			}
		}
		else
		{
			//You're here because you're using cg_truespin.integer == 2
			if (PM_SpinningSaberAnim(cg.snap->ps.torsoAnim)
				&& cg.snap->ps.torsoAnim != BOTH_JUMPFLIPSLASHDOWN1
				&& cg.snap->ps.torsoAnim != BOTH_JUMPFLIPSTABDOWN)
			{
				//Flip animation and using cg_trueflip.integer = 2, lock the eyemovement
				eye_range = qfalse;
				did_special = qtrue;
			}
		}
	}
	else if (PM_SpinningSaberAnim(cg.snap->ps.torsoAnim)
		&& cg.snap->ps.torsoAnim != BOTH_JUMPFLIPSLASHDOWN1
		&& cg.snap->ps.torsoAnim != BOTH_JUMPFLIPSTABDOWN)
	{
		//you don't want spinning so use cg.refdef.viewangles as the view
		use_ref_def = qtrue;
	}
	else if (cg.snap->ps.legsAnim == BOTH_JUMPATTACK6)
	{
		use_ref_def = qtrue;
	}

	//Prevent camera flicker while landing.
	if (cg.snap->ps.legsAnim == BOTH_LAND1
		|| cg.snap->ps.legsAnim == BOTH_LAND2
		|| cg.snap->ps.legsAnim == BOTH_LANDBACK1
		|| cg.snap->ps.legsAnim == BOTH_LANDLEFT1
		|| cg.snap->ps.legsAnim == BOTH_LANDRIGHT1)
	{
		use_ref_def = qtrue;
	}

	//Prevent the camera flicker while switching to the saber.
	if (cg.snap->ps.torsoAnim == BOTH_STAND2TO1
		|| cg.snap->ps.torsoAnim == BOTH_STAND1TO2)
	{
		use_ref_def = qtrue;
	}

	//special camera view for blue backstab
	if (cg.snap->ps.torsoAnim == BOTH_A2_STABBACK1)
	{
		eye_range = qfalse;
		did_special = qtrue;
	}

	if (cg.snap->ps.torsoAnim == BOTH_JUMPFLIPSLASHDOWN1
		|| cg.snap->ps.torsoAnim == BOTH_JUMPFLIPSTABDOWN)
	{
		eye_range = qfalse;
		did_special = qtrue;
	}

	if (use_ref_def)
	{
		VectorCopy(cg.refdefViewAngles, eye_angles);
	}
	else
	{
		//Movement Roll dampener
		if (!did_special)
		{
			if (!cg_truemoveroll.integer)
			{
				eye_angles[2] = cg.refdefViewAngles[2];
			}
			else if (cg_truemoveroll.integer == 1)
			{
				//dampen the movement leaning
				eye_angles[2] *= .5;
			}
		}

		//eye movement
		if (eye_range)
		{
			//allow eye motion
			for (int i = 0; i < 2; i++)
			{
				int fov;
				if (cg_truefov.integer)
				{
					fov = cg_truefov.value;
				}
				else
				{
					fov = cg_fov.value;
				}

				float AngDiff = eye_angles[i] - cg.refdefViewAngles[i];

				AngDiff = AngleNormalize180(AngDiff);
				if (fabs(AngDiff) > fov)
				{
					if (AngDiff < 0)
					{
						eye_angles[i] += fov;
					}
					else
					{
						eye_angles[i] -= fov;
					}
				}
				else
				{
					eye_angles[i] = cg.refdefViewAngles[i];
				}
				AngleNormalize180(eye_angles[i]);
			}
		}
	}
}

/*
===============
CG_Player

  FIXME: Extend this to be a render func for all multiobject entities in the game such that:

	You can have and stack multiple animated pieces (not just legs and torso)
	You can attach "bolt-ons" that either animate or don't (weapons, heads, borg pieces)
	You can attach any object to any tag on any object (weapon on the head, etc.)

	Basically, keep a list of objects:
		Root object (in this case, the legs) with this info:
			model
			skin
			effects
			scale
			if animated, frame or anim number
			drawn at origin and angle of master entity
		Animated objects, with this info:
			model
			skin
			effects
			scale
			frame or anim number
			object it's attached to
			tag to attach it's tag_parent to
			angle offset to attach it with
		Non-animated objects, with this info:
			model
			skin
			effects
			scale
			object it's attached to
			tag to attach it's tag_parent to
			angle offset to attach it with

  ALSO:
	Move the auto angle setting back up to the game
	Implement 3-axis scaling
	Implement alpha
	Implement tint
	Implement other effects (generic call effect at org and dir (or tag), with parms?)

===============
*/
extern qboolean G_GetRootSurfNameWithVariant(gentity_t* ent, const char* root_surf_name, char* return_surf_name,
	int return_size);
extern qboolean G_RagDoll(gentity_t* ent, vec3_t forcedAngles);
int cg_saberOnSoundTime[MAX_GENTITIES] = { 0 };
extern void CG_AddRadarEnt(const centity_t* cent);
extern qboolean CG_InFighter();
extern void CG_AddBracketedEnt(const centity_t* cent);

void CG_Player(centity_t* cent)
{
	clientInfo_t* ci;
	qboolean shadow, static_scale = qfalse;
	float shadow_plane;
	const weaponData_t* w_data = nullptr;

	if (cent->currentState.eFlags & EF_NODRAW)
	{
		return;
	}

	//make sure this thing has a gent and client
	if (!cent->gent)
	{
		return;
	}

	if (!cent->gent->client)
	{
		return;
	}

	if (cent->gent->s.number == 0 && cg.weaponSelect == WP_NONE && cg.zoomMode == 1)
	{
		// HACK
		return;
	}

	calcedMp = qfalse;

	//Get the player's light level for stealth calculations
	CG_GetPlayerLightLevel(cent);

	if (in_camera && cent->currentState.client_num == 0) // If player in camera then no need for shadow
	{
		return;
	}

	if (cent->currentState.number == 0 && !cg.renderingThirdPerson && (!cg_trueguns.integer || cg.zoomMode))
	{
		calcedMp = qtrue;
	}

	ci = &cent->gent->client->clientInfo;

	if (!ci->infoValid)
	{
		return;
	}

	CG_AddRadarEnt(cent);

	if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_VEHICLE)
	{
		//add vehicles
		CG_AddRadarEnt(cent);
		if (CG_InFighter())
		{
			//this is a vehicle, bracket it
			if (cg.predictedPlayerState.m_iVehicleNum != cent->currentState.client_num)
			{
				//don't add the vehicle I'm in... :)
				CG_AddBracketedEnt(cent);
			}
		}
	}

	G_RagDoll(cent->gent, cent->lerpAngles);

	if (cent->currentState.weapon)
	{
		w_data = &weaponData[cent->currentState.weapon];
	}

	if (cent->gent->ghoul2.size()) //do we have ghoul models attached?
	{
		refEntity_t ent;
		vec3_t temp_angles;
		memset(&ent, 0, sizeof ent);

		//FIXME: if at all possible, do all our sets before our gets to do only *1* G2 skeleton transform per render frame
		CG_SetGhoul2Info(&ent, cent);

		// Weapon sounds may need to be stopped, so check now
		CG_StopWeaponSounds(cent);

		// add powerups floating behind the player
		CG_PlayerPowerups(cent);

		// add the shadow
		//FIXME: funcs that modify our origin below will cause the shadow to be in the wrong spot
		shadow = CG_PlayerShadow(cent, &shadow_plane);

		// add a water splash if partially in and out of water
		CG_PlayerSplash(cent);

		// get the player model information
		ent.renderfx = 0;
		if (!cg.renderingThirdPerson || cg.zoomMode)
		{
			//in first person or zoomed in
			if (cg.snap->ps.viewEntity <= 0 || cg.snap->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//no viewentity
				if (cent->currentState.number == cg.snap->ps.client_num)
				{
					//I am the player
					if (cg.zoomMode || !cg_trueguns.integer && cg.snap->ps.weapon != WP_SABER && cg.snap->ps.weapon !=
						WP_MELEE || cg.snap->ps.weapon == WP_SABER && cg_truesaberonly.integer)
					{
						//not using saber or fists
						ent.renderfx = RF_THIRD_PERSON; // only draw in mirrors
					}
				}
			}
			else if (cent->currentState.number == cg.snap->ps.viewEntity)
			{
				//I am the view entity
				if (cg.zoomMode || !cg_trueguns.integer && cg.snap->ps.weapon != WP_SABER && cg.snap->ps.weapon !=
					WP_MELEE || cg.snap->ps.weapon == WP_SABER && cg_truesaberonly.integer)
				{
					//not using first person saber test or, if so, not using saber
					ent.renderfx = RF_THIRD_PERSON; // only draw in mirrors
				}
			}
		}

		if (cent->gent->client->ps.powerups[PW_DISINT_2] > cg.time)
		{
			//ghost!
			ent.renderfx = RF_THIRD_PERSON; // only draw in mirrors
		}
		else if (cg_shadows.integer == 2 && ent.renderfx & RF_THIRD_PERSON)
		{
			//show stencil shadow in first person now because we can -rww
			ent.renderfx |= RF_SHADOW_ONLY;
		}

		if (cg_shadows.integer == 2 || cg_shadows.integer == 3 && shadow)
		{
			ent.renderfx |= RF_SHADOW_PLANE;
		}
		ent.shadowPlane = shadow_plane;
		ent.renderfx |= RF_LIGHTING_ORIGIN; // use the same origin for all

		if (cent->gent->NPC && cent->gent->NPC->scriptFlags & SCF_MORELIGHT)
		{
			ent.renderfx |= RF_MINLIGHT; //bigger than normal min light
		}

		if (cent->gent->client && cent->gent->flags & FL_MORELIGHPLAYER)
		{
			ent.renderfx |= RF_MINLIGHT; //bigger than normal min light
		}

		CG_RegisterWeapon(cent->currentState.weapon);

		//---------------
		Vehicle_t* p_veh;

		if (cent->currentState.eFlags & EF_LOCKED_TO_WEAPON && cent->gent && cent->gent->health > 0 && cent->gent->
			owner)
		{
			centity_t* chair = &cg_entities[cent->gent->owner->s.number];
			if (chair && chair->gent)
			{
				vec3_t temp;
				mdxaBone_t bolt_matrix;

				//NOTE: call this so it updates on the server and client
				if (chair->gent->bounceCount)
				{
					//EWeb
					// We'll set the turret angles directly
					VectorClear(temp);
					VectorClear(chair->gent->pos1);

					temp[PITCH] = cent->lerpAngles[PITCH];
					chair->gent->pos1[YAW] = AngleSubtract(cent->lerpAngles[YAW], chair->gent->s.angles[YAW]);
					//remember which dir our turret is facing for later
					cent->lerpAngles[ROLL] = 0;

					BG_G2SetBoneAngles(chair, chair->gent->lowerLumbarBone, chair->gent->pos1, BONE_ANGLES_POSTMULT,
						POSITIVE_Z, NEGATIVE_X, NEGATIVE_Y, cgs.model_draw);
					BG_G2SetBoneAngles(chair, chair->gent->upperLumbarBone, temp, BONE_ANGLES_POSTMULT, POSITIVE_Z,
						NEGATIVE_X, NEGATIVE_Y, cgs.model_draw);
				}
				else
				{
					// We'll set the turret yaw directly
					VectorClear(chair->gent->s.apos.trBase);
					VectorClear(temp);

					chair->gent->s.apos.trBase[YAW] = cent->lerpAngles[YAW];
					temp[PITCH] = -cent->lerpAngles[PITCH];
					cent->lerpAngles[ROLL] = 0;
					BG_G2SetBoneAngles(chair, chair->gent->lowerLumbarBone, temp, BONE_ANGLES_POSTMULT, POSITIVE_Y,
						POSITIVE_Z, POSITIVE_X, cgs.model_draw);
				}
				VectorCopy(temp, chair->gent->lastAngles);

				gi.G2API_StopBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->hipsBone);

				// Getting the seat bolt here
				gi.G2API_GetBoltMatrix(chair->gent->ghoul2, chair->gent->playerModel, chair->gent->headBolt,
					&bolt_matrix, chair->gent->s.apos.trBase, chair->gent->currentOrigin, cg.time,
					cgs.model_draw, chair->currentState.modelScale);

				if (chair->gent->bounceCount)
				{
					//put behind it, not in chair
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent.origin);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, chair->gent->pos3);
					chair->gent->pos3[2] = 0;
					VectorNormalizeFast(chair->gent->pos3);
					VectorMA(ent.origin, -44.0f, chair->gent->pos3, ent.origin);
					ent.origin[2] = cent->lerpOrigin[2];
					cent->lerpAngles[YAW] = vectoyaw(chair->gent->pos3);
					cent->lerpAngles[ROLL] = 0;
					CG_G2PlayerAngles(cent, ent.axis, temp_angles);
					calcedMp = qtrue;
				}
				else
				{
					//sitting in it
					// Storing ent position, bolt position, and bolt axis
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent.origin);
					VectorCopy(ent.origin, chair->gent->pos2);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Y, chair->gent->pos3);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Z, chair->gent->pos4);

					AnglesToAxis(cent->lerpAngles, ent.axis);
					VectorCopy(cent->lerpAngles, temp_angles); //tempAngles is needed a lot below
				}

				VectorCopy(ent.origin, ent.oldorigin);
				VectorCopy(ent.origin, ent.lightingOrigin);

				// FIXME:  Mike claims that hacking the eyepoint will make them shoot at me.......so,
				//	we move up from the seat bolt and store off that point.
				//			VectorMA( ent.origin, -20, chair->gent->pos3, cent->gent->client->renderInfo.eyePoint );
				//			VectorMA( cent->gent->client->renderInfo.eyePoint, 40, chair->gent->pos4, cent->gent->client->renderInfo.eyePoint );
			}
		}
		else if ((p_veh = G_IsRidingVehicle(cent->gent)) != nullptr)
		{
			//rider
			CG_G2PlayerAngles(cent, ent.axis, temp_angles);
			//Deal with facial expressions
			CG_G2PlayerHeadAnims(cent);

			centity_t* veh_ent = &cg_entities[cent->gent->owner->s.number];
			CG_CalcEntityLerpPositions(veh_ent);
			// Get the driver tag.
			mdxaBone_t bolt_matrix;
			gi.G2API_GetBoltMatrix(veh_ent->gent->ghoul2, veh_ent->gent->playerModel, veh_ent->gent->crotchBolt,
				&bolt_matrix, veh_ent->lerpAngles, veh_ent->lerpOrigin,
				cg.time ? cg.time : level.time, nullptr, veh_ent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent.origin);

			float sav_pitch = cent->lerpAngles[PITCH];
			VectorCopy(veh_ent->lerpAngles, cent->lerpAngles);
			AnglesToAxis(cent->lerpAngles, ent.axis);

			VectorCopy(ent.origin, ent.oldorigin);
			VectorCopy(ent.origin, ent.lightingOrigin);

			VectorCopy(cent->lerpAngles, temp_angles); //tempAngles is needed a lot below
			VectorCopy(ent.origin, cent->lerpOrigin);
			VectorCopy(ent.origin, cent->gent->client->ps.origin);
			//bah, keep our pitch!
			cent->lerpAngles[PITCH] = sav_pitch;
		}
		else if ((cent->gent->client->ps.eFlags & EF_HELD_BY_RANCOR || cent->gent->client->ps.eFlags &
			EF_HELD_BY_WAMPA)
			&& cent->gent && cent->gent->activator)
		{
			centity_t* monster = &cg_entities[cent->gent->activator->s.number];
			if (monster && monster->gent && monster->gent->inuse && monster->gent->health > 0)
			{
				mdxaBone_t bolt_matrix;
				// Getting the bolt here
				//in mouth
				int bolt_index = monster->gent->gutBolt;
				if (monster->gent->count == 1)
				{
					//in hand
					bolt_index = monster->gent->handRBolt;
				}
				vec3_t ranc_angles = { 0 };
				ranc_angles[YAW] = monster->lerpAngles[YAW];
				gi.G2API_GetBoltMatrix(monster->gent->ghoul2, monster->gent->playerModel, bolt_index,
					&bolt_matrix, ranc_angles, monster->lerpOrigin, cg.time,
					cgs.model_draw, monster->currentState.modelScale);
				// Storing ent position, bolt position, and bolt axis
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent.origin);
				if (cent->gent->client->ps.eFlags & EF_HELD_BY_WAMPA)
				{
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ent.axis[0]);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_X, ent.axis[1]);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Z, ent.axis[2]);
				}
				else if (monster->gent->count == 1)
				{
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ent.axis[0]);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_X, ent.axis[1]);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Z, ent.axis[2]);
				}
				else
				{
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Z, ent.axis[0]);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ent.axis[1]);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_X, ent.axis[2]);
				}
				//FIXME: this is messing up our axis and turning us inside-out
				if (cent->gent->client->isRagging)
				{
					//hack, ragdoll has you way at bottom of bounding box
					VectorMA(ent.origin, 32, ent.axis[2], ent.origin);
				}
				VectorCopy(ent.origin, ent.oldorigin);
				VectorCopy(ent.origin, ent.lightingOrigin);

				vectoangles(ent.axis[0], cent->lerpAngles);
				vec3_t temp;
				vectoangles(ent.axis[2], temp);
				cent->lerpAngles[ROLL] = -temp[PITCH];

				VectorCopy(cent->lerpAngles, temp_angles); //tempAngles is needed a lot below
				VectorCopy(ent.origin, cent->lerpOrigin);
				VectorCopy(ent.origin, cent->gent->client->ps.origin);
				//	if ( (cent->gent->client->ps.eFlags&EF_HELD_BY_WAMPA) )
				//	{
				vectoangles(ent.axis[0], cent->lerpAngles);
				VectorCopy(cent->lerpAngles, temp_angles); //tempAngles is needed a lot below
			}
			else
			{
				//wtf happened to the guy holding me?  Better get out
				cent->gent->activator = nullptr;
				cent->gent->client->ps.eFlags &= ~(EF_HELD_BY_WAMPA | EF_HELD_BY_RANCOR);
			}
		}
		else if (cent->gent->client->ps.eFlags & EF_HELD_BY_SAND_CREATURE
			&& cent->gent
			&& cent->gent->activator)
		{
			centity_t* sand_creature = &cg_entities[cent->gent->activator->s.number];
			if (sand_creature && sand_creature->gent)
			{
				mdxaBone_t bolt_matrix;
				// Getting the bolt here
				//in hand
				vec3_t sc_angles = { 0 };
				sc_angles[YAW] = sand_creature->lerpAngles[YAW];
				gi.G2API_GetBoltMatrix(sand_creature->gent->ghoul2, sand_creature->gent->playerModel,
					sand_creature->gent->gutBolt,
					&bolt_matrix, sc_angles, sand_creature->lerpOrigin, cg.time,
					cgs.model_draw, sand_creature->currentState.modelScale);
				// Storing ent position, bolt position, and bolt axis
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent.origin);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ent.axis[0]);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_X, ent.axis[1]);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Z, ent.axis[2]);
				//FIXME: this is messing up our axis and turning us inside-out
				if (cent->gent->client->isRagging)
				{
					//hack, ragdoll has you way at bottom of bounding box
					VectorMA(ent.origin, 32, ent.axis[2], ent.origin);
				}
				VectorCopy(ent.origin, ent.oldorigin);
				VectorCopy(ent.origin, ent.lightingOrigin);

				vectoangles(ent.axis[0], cent->lerpAngles);
				vec3_t temp;
				vectoangles(ent.axis[2], temp);
				cent->lerpAngles[ROLL] = -temp[PITCH];

				VectorCopy(cent->lerpAngles, temp_angles); //tempAngles is needed a lot below
				VectorCopy(ent.origin, cent->lerpOrigin);
				VectorCopy(ent.origin, cent->gent->client->ps.origin);
				cent->gent->client->ps.viewangles[YAW] = cent->lerpAngles[YAW];
			}
		}
		else
		{
			//---------------
			CG_G2PlayerAngles(cent, ent.axis, temp_angles);
			//Deal with facial expressions
			CG_G2PlayerHeadAnims(cent);

			/*
			if ( cent->gent->client->ps.eFlags & EF_FORCE_DRAINED
				&& !VectorCompare( cent->gent->client->ps.forceDrainOrg, vec3_origin ) )
			{//HACKHACKHACK!!!! being drained
				VectorCopy( cent->gent->client->ps.forceDrainOrg, ent.origin);
			}
			else
			*/
			{
				VectorCopy(cent->lerpOrigin, ent.origin);
			}

			if (ent.modelScale[2] && ent.modelScale[2] != 1.0f)
			{
				ent.origin[2] += 24 * (ent.modelScale[2] - 1);
			}
			VectorCopy(ent.origin, ent.oldorigin);
			VectorCopy(ent.origin, ent.lightingOrigin);
		}

		if (cent->gent && cent->gent->client)
		{
			cent->gent->client->ps.legsYaw = temp_angles[YAW];
		}
		ScaleModelAxis(&ent);
		/*extern vmCvar_t	cg_thirdPersonAlpha;

		if ((cent->gent->s.number == 0 || G_ControlledByPlayer(cent->gent)))
		{
			float alpha = 1.0f;
			if ((cg.overrides.active & CG_OVERRIDE_3RD_PERSON_APH))
			{
				alpha = cg.overrides.thirdPersonAlpha;
			}
			else
			{
				alpha = cg_thirdPersonAlpha.value;
			}

			if (alpha < 1.0f)
			{
				ent.renderfx |= RF_ALPHA_FADE;
				ent.shaderRGBA[3] = (unsigned char)(alpha * 255.0f);
			}
		}*/

		if (cg_debugHealthBars.integer)
		{
			if (cent->gent && cent->gent->health > 0 && cent->gent->max_health > 0)
			{
				//draw a health bar over them
				CG_AddHealthBarEnt(cent->currentState.client_num);
			}
		}
		if (cg_debugBlockBars.integer)
		{
			if (cg_SerenityJediEngineMode.integer)
			{
				if (cg_SerenityJediEngineMode.integer == 2)
				{
					if (cent->gent && cent->gent->client->ps.blockPoints > 0)
					{
						//draw a bp bar over them
						CG_AddBlockPointBarEnt(cent->currentState.client_num);
					}
				}
				else
				{
					if (cent->gent && cent->gent->client->ps.forcePower > 0)
					{
						//draw a bp bar over them
						CG_AddBlockPointBarEnt(cent->currentState.client_num);
					}
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
				//draw a bp bar over them
				CG_AddFatiguePointBarEnt(cent->currentState.client_num);
			}
			else
			{
				//Dont draw in JKA Mode
			}
		}

		CG_AddRefEntityWithPowerups(&ent, cent->currentState.powerups, cent);
		VectorCopy(temp_angles, cent->renderAngles);

		//Initialize all these to *some* valid data
		VectorCopy(ent.origin, cent->gent->client->renderInfo.headPoint);
		VectorCopy(ent.origin, cent->gent->client->renderInfo.handRPoint);
		VectorCopy(ent.origin, cent->gent->client->renderInfo.handLPoint);
		VectorCopy(ent.origin, cent->gent->client->renderInfo.footRPoint);
		VectorCopy(ent.origin, cent->gent->client->renderInfo.footLPoint);
		VectorCopy(ent.origin, cent->gent->client->renderInfo.torsoPoint);
		VectorCopy(cent->lerpAngles, cent->gent->client->renderInfo.torsoAngles);
		VectorCopy(ent.origin, cent->gent->client->renderInfo.crotchPoint);
		if (cent->currentState.number != 0
			|| cg.renderingThirdPerson
			|| cg.snap->ps.stats[STAT_HEALTH] <= 0
			|| cg_trueguns.integer
			&& !cg.zoomMode
			|| !cg.renderingThirdPerson && (cg.snap->ps.weapon == WP_SABER || cg.snap->ps.weapon == WP_MELEE))
			//First person saber
		{
			//in some third person mode or NPC
			//we don't override thes in pure 1st person because they will be set before this func
			VectorCopy(ent.origin, cent->gent->client->renderInfo.eyePoint);
			VectorCopy(cent->lerpAngles, cent->gent->client->renderInfo.eyeAngles);
			if (!cent->gent->client->ps.saberInFlight)
			{
				VectorCopy(ent.origin, cent->gent->client->renderInfo.muzzlePoint);
				VectorCopy(ent.axis[0], cent->gent->client->renderInfo.muzzleDir);
			}
		}
		//now try to get the right data

		mdxaBone_t bolt_matrix;
		vec3_t g2_angles = { 0, temp_angles[YAW], 0 };

		if (cent->gent->handRBolt != -1)
		{
			//Get handRPoint
			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->handRBolt,
				&bolt_matrix, g2_angles, ent.origin, cg.time,
				cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.handRPoint);
		}
		if (cent->gent->handLBolt != -1)
		{
			//always get handLPoint too...?
			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->handLBolt,
				&bolt_matrix, g2_angles, ent.origin, cg.time,
				cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.handLPoint);
		}
		if (cent->gent->footLBolt != -1)
		{
			//get the feet
			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->footLBolt,
				&bolt_matrix, g2_angles, ent.origin, cg.time,
				cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.footLPoint);
		}

		if (cent->gent->footRBolt != -1)
		{
			gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->footRBolt,
				&bolt_matrix, g2_angles, ent.origin, cg.time,
				cgs.model_draw, cent->currentState.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.footRPoint);
		}

		//[TrueView]
		//Restrict True View Model changes to the player and do the True View camera view work.
		if (cg.snap && cent->currentState.number == cg.snap->ps.viewEntity && cg_truebobbing.integer)
		{
			if (!cg.renderingThirdPerson &&
				(cg_trueguns.integer || cent->currentState.weapon == WP_SABER || cent->currentState.weapon == WP_MELEE) &&
				!cg.zoomMode)
			{
				//<True View varibles
				mdxaBone_t eye_matrix;
				vec3_t eye_angles{};
				vec3_t eye_axis[3]{};
				vec3_t oldeye_origin;
				qhandle_t eyes_bolt;
				qboolean bone_based = qfalse;

				//make the player's be based on the ghoul2 model

				//grab the location data for the "*head_eyes" tag surface
				eyes_bolt = gi.G2API_AddBolt(&cent->gent->ghoul2[cent->gent->playerModel], "*head_eyes");
				if (!gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, eyes_bolt, &eye_matrix,
					temp_angles, cent->lerpOrigin,
					cg.time, cgs.model_draw, cent->currentState.modelScale))
				{
					//Something prevented you from getting the "*head_eyes" information.  The model probably doesn't have a
					//*head_eyes tag surface.  Try using *head_front instead

					eyes_bolt = gi.G2API_AddBolt(&cent->gent->ghoul2[cent->gent->playerModel], "*head_front");
					if (!gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, eyes_bolt, &eye_matrix,
						temp_angles, cent->lerpOrigin,
						cg.time, cgs.model_draw, cent->currentState.modelScale))
					{
						eyes_bolt = gi.G2API_AddBolt(&cent->gent->ghoul2[cent->gent->playerModel], "reye");
						bone_based = qtrue;
						if (!gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, eyes_bolt, &eye_matrix,
							temp_angles, cent->lerpOrigin,
							cg.time, cgs.model_draw, cent->currentState.modelScale))
						{
							goto SkipTrueView;
						}
					}
				}

				//Set the original eye Origin
				VectorCopy(cg.refdef.vieworg, oldeye_origin);

				//set the player's view origin
				gi.G2API_GiveMeVectorFromMatrix(eye_matrix, ORIGIN, cg.refdef.vieworg);

				//Find the orientation of the eye tag surface
				//I based this on coordsys.h that I found at http://www.xs4all.nl/~hkuiper/cwmtx/html/coordsys_8h-source.html
				//According to the file, Harry Kuiper, Will DeVore deserve credit for making that file that I based this on.

				if (bone_based)
				{
					//the eye bone has different default axis orientation than the tag surfaces.
					eye_axis[0][0] = eye_matrix.matrix[0][1];
					eye_axis[1][0] = eye_matrix.matrix[1][1];
					eye_axis[2][0] = eye_matrix.matrix[2][1];

					eye_axis[0][1] = eye_matrix.matrix[0][0];
					eye_axis[1][1] = eye_matrix.matrix[1][0];
					eye_axis[2][1] = eye_matrix.matrix[2][0];

					eye_axis[0][2] = -eye_matrix.matrix[0][2];
					eye_axis[1][2] = -eye_matrix.matrix[1][2];
					eye_axis[2][2] = -eye_matrix.matrix[2][2];
				}
				else
				{
					eye_axis[0][0] = eye_matrix.matrix[0][0];
					eye_axis[1][0] = eye_matrix.matrix[1][0];
					eye_axis[2][0] = eye_matrix.matrix[2][0];

					eye_axis[0][1] = eye_matrix.matrix[0][1];
					eye_axis[1][1] = eye_matrix.matrix[1][1];
					eye_axis[2][1] = eye_matrix.matrix[2][1];

					eye_axis[0][2] = eye_matrix.matrix[0][2];
					eye_axis[1][2] = eye_matrix.matrix[1][2];
					eye_axis[2][2] = eye_matrix.matrix[2][2];
				}

				eye_angles[YAW] = atan2(eye_axis[1][0], eye_axis[0][0]) * 180 / M_PI;

				//I want asin but it's not setup in the libraries so I'm useing the statement asin x = (M_PI / 2) - acos x
				eye_angles[PITCH] = (M_PI / 2 - acos(-eye_axis[2][0])) * 180 / M_PI;
				eye_angles[ROLL] = atan2(eye_axis[2][1], eye_axis[2][2]) * 180 / M_PI;

				//END Find the orientation of the eye tag surface

				//Shift the camera origin by cg_trueeyeposition
				AngleVectors(eye_angles, eye_axis[0], nullptr, nullptr);
				VectorMA(cg.refdef.vieworg, cg_trueeyeposition.value, eye_axis[0], cg.refdef.vieworg);

				//Trace to see if the bolt eye origin is ok to move to.  If it's not, place it at the last safe position.
				CheckCameraLocation(oldeye_origin);

				//Singleplayer TrueView fix (ghoul2 axes calculated differently to in MP!)
				eye_angles[YAW] -= 90;

				//Do all the Eye "movement" and simplified moves here.
				SmoothTrueView(eye_angles);

				//set the player view angles
				VectorCopy(eye_angles, cg.refdefViewAngles);

				//set the player view axis
				AnglesToAxis(eye_angles, cg.refdef.viewaxis);
			}
		}

	SkipTrueView:

		//Handle saber
		if (cent->gent
			&& cent->gent->client
			&& (cent->currentState.weapon == WP_SABER || cent->gent->client->ps.saberInFlight)
			&& cent->gent->client->NPC_class != CLASS_ATST)
		{
			//FIXME: somehow saberactive is getting lost over the network
			//loop this and do for both sabers
			int num_sabers = 1;
			if (cent->gent->client->ps.dualSabers)
			{
				num_sabers = 2;
			}
			for (int saber_num = 0; saber_num < num_sabers; saber_num++)
			{
				if (cent->gent->client->ps.saberEventFlags & SEF_INWATER)
				{
					cent->gent->client->ps.saber[saber_num].Deactivate();
				}
				//loop this and do for both blades
				for (int blade_num = 0; blade_num < cent->gent->client->ps.saber[saber_num].numBlades; blade_num++)
				{
					if (!cent->gent->client->ps.saber[saber_num].blade[blade_num].active ||
						cent->gent->client->ps.saber[saber_num].blade[blade_num].length > cent->gent->client->ps.saber[
							saber_num].blade[blade_num].lengthMax) //hack around network lag for now
					{
						//saber blade is off
						if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length > 0)
						{
							if (cent->gent->client->ps.stats[STAT_HEALTH] <= 0)
							{
								//dead, didn't actively turn it off
								cent->gent->client->ps.saber[saber_num].blade[blade_num].length -= cent->gent->client->
									ps.
									saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime / 100 *
									cg_ignitionSpeed.value;
							}
							else
							{
								//actively turned it off, shrink faster
								cent->gent->client->ps.saber[saber_num].blade[blade_num].length -= cent->gent->client->
									ps.
									saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime / 100 *
									cg_ignitionSpeed.value;
							}
						}
					}
					else
					{
						//saber blade is on
						if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length < cent->gent->client->ps.
							saber
							[saber_num].blade[blade_num].lengthMax)
						{
							if (!cent->gent->client->ps.saber[saber_num].blade[blade_num].length)
							{
								qhandle_t saber_on_sound = cgs.sound_precache[g_entities[cent->currentState.client_num].
									client->ps.saber[saber_num].soundOn];
								if (!cent->gent->client->ps.weaponTime
									&& !saber_num //first saber only
									&& !blade_num) //first blade only
								{
									//make us play the turn on anim
									cent->gent->client->ps.weaponstate = WEAPON_RAISING;
									cent->gent->client->ps.weaponTime = 250;
								}
								if (cent->gent->client->ps.saberInFlight && saber_num == 0)
								{
									//play it on the saber
									if (cg_saberOnSoundTime[cent->currentState.number] < cg.time)
									{
										cgi_S_UpdateEntityPosition(cent->gent->client->ps.saberEntityNum,
											g_entities[cent->gent->client->ps.saberEntityNum].
											currentOrigin);
										cgi_S_StartSound(nullptr, cent->gent->client->ps.saberEntityNum, CHAN_AUTO,
											saber_on_sound);
										cg_saberOnSoundTime[cent->currentState.number] = cg.time;
										//so we don't play multiple on sounds at one time
									}
								}
								else
								{
									if (cg_saberOnSoundTime[cent->currentState.number] < cg.time)
									{
										cgi_S_StartSound(nullptr, cent->currentState.number, CHAN_AUTO, saber_on_sound);
										cg_saberOnSoundTime[cent->currentState.number] = cg.time;
										//so we don't play multiple on sounds at one time
									}
								}
							}
							if (cg.frametime > 0)
							{
								if (PM_SuperBreakWinAnim(cent->gent->client->ps.torsoAnim))
								{
									//just keep it full length!
									//NOTE: does this mean it will go through walls now...?
									cent->gent->client->ps.saber[saber_num].blade[blade_num].length = cent->gent->client
										->
										ps.saber[saber_num].blade[blade_num].lengthMax;
								}
								else
								{
									switch (cent->gent->client->ps.saber_anim_level)
									{
									case SS_FAST:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 5 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									case SS_STRONG:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									case SS_MEDIUM:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									case SS_DESANN:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									case SS_TAVION:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									case SS_DUAL:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									case SS_STAFF:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeedstaff.value;
										break;
									default:
										cent->gent->client->ps.saber[saber_num].blade[blade_num].length += cent->gent->
											client->ps.saber[saber_num].blade[blade_num].lengthMax / 10 * cg.frametime /
											100 * cg_ignitionSpeed.value;
										break;
									}
								}
							}
							if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length > cent->gent->client->ps
								.
								saber[saber_num].blade[blade_num].lengthMax)
							{
								cent->gent->client->ps.saber[saber_num].blade[blade_num].length = cent->gent->client->ps
									.
									saber[saber_num].blade[blade_num].lengthMax;
							}
						}
					}

					if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length > 0)
					{
						if (!cent->gent->client->ps.saberInFlight || saber_num != 0)
							//&& cent->gent->client->ps.saberActive)
						{
							//holding the saber in-hand
							//						CGhoul2Info *currentModel = &cent->gent->ghoul2[1];
							//						CGhoul2Info *nextModel = &cent->gent->ghoul2[1];
							//FIXME: need a version of this that *doesn't* need the mFileName in the ghoul2
							//FIXME: use an actual surfaceIndex?
							char hand_name[MAX_QPATH];
							if (saber_num == 0)
							{
								//this returns qfalse if it doesn't exist or isn't being rendered
								if (G_GetRootSurfNameWithVariant(cent->gent, "r_hand", hand_name, sizeof hand_name))
									//!gi.G2API_GetSurfaceRenderStatus( &cent->gent->ghoul2[cent->gent->playerModel], "r_hand" ) )//surf is still on
								{
									CG_AddSaberBladeGo(cent, cent, ent.renderfx, cent->gent->weaponModel[saber_num],
										ent.origin, temp_angles, saber_num,
										blade_num);
									//CG_AddSaberBlades( cent, ent.renderfx, ent.origin, tempAngles, saber_num );
								} //else, the limb will draw the blade itself
							}
							else if (saber_num == 1)
							{
								//this returns qfalse if it doesn't exist or isn't being rendered
								if (G_GetRootSurfNameWithVariant(cent->gent, "l_hand", hand_name, sizeof hand_name))
									//!gi.G2API_GetSurfaceRenderStatus( &cent->gent->ghoul2[cent->gent->playerModel], "l_hand" ) )//surf is still on
								{
									CG_AddSaberBladeGo(cent, cent, ent.renderfx, cent->gent->weaponModel[saber_num],
										ent.origin, temp_angles, saber_num,
										blade_num);
									//CG_AddSaberBlades( cent, ent.renderfx, ent.origin, tempAngles, saber_num );
								} //else, the limb will draw the blade itself
							}
						} //in-flight saber draws it's own blade
					}
					else
					{
						if (cent->gent->client->ps.saber[saber_num].blade[blade_num].length < 0)
						{
							cent->gent->client->ps.saber[saber_num].blade[blade_num].length = 0;
						}
						//if ( cent->gent->client->ps.saberEventFlags&SEF_INWATER )
						{
							CG_CheckSaberInWater(cent, cent, saber_num, cent->gent->weaponModel[saber_num], ent.origin,
								temp_angles);
						}
					}
					if (cent->currentState.weapon == WP_SABER
						&& (cent->gent->client->ps.saber[saber_num].blade[blade_num].length > 0 || cent->gent->client->
							ps.
							saberInFlight))
					{
						calcedMp = qtrue;
					}
				}
			}
			//add the light
			if (cent->gent->client->ps.dualSabers)
			{
				if (cent->gent->client->ps.saber[0].Length() > 0.0f
					&& !cent->gent->client->ps.saberInFlight)
				{
					if (cent->gent->client->ps.saber[0].numBlades > 2)
					{
						// add blended light
						CG_DoSaberLight(&cent->gent->client->ps.saber[0]);
					}
				}
				if (cent->gent->client->ps.saber[1].Length() > 0.0f)
				{
					if (cent->gent->client->ps.saber[1].numBlades > 2)
					{
						// add blended light
						CG_DoSaberLight(&cent->gent->client->ps.saber[1]);
					}
				}
			}
			else if (cent->gent->client->ps.saber[0].Length() > 0.0f
				&& !cent->gent->client->ps.saberInFlight)
			{
				if (cent->gent->client->ps.saber[0].numBlades > 2)
				{
					// add blended light
					CG_DoSaberLight(&cent->gent->client->ps.saber[0]);
				}
			}
		}

		if (cent->currentState.number != 0
			|| cg.renderingThirdPerson
			|| cg.snap->ps.stats[STAT_HEALTH] <= 0
			|| !cg.renderingThirdPerson && (cg.snap->ps.weapon == WP_SABER || cg.snap->ps.weapon == WP_MELEE)
			//First person saber
			|| cg_trueguns.integer && !cg.zoomMode)
		{
			vec3_t temp_axis;

			if (cent->gent->headBolt == -1)
			{
				//no headBolt
				VectorCopy(ent.origin, cent->gent->client->renderInfo.eyePoint);
				VectorCopy(temp_angles, cent->gent->client->renderInfo.eyeAngles);
				VectorCopy(ent.origin, cent->gent->client->renderInfo.headPoint);
			}
			else
			{
				//FIXME: if head is missing, we should let the dismembered head set our eyePoint...
				gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->headBolt, &bolt_matrix,
					temp_angles, ent.origin, cg.time, cgs.model_draw, cent->currentState.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.eyePoint);
				if (cent->gent->client->NPC_class == CLASS_RANCOR)
				{
					//temp hack
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_X, temp_axis);
				}
				else
				{
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, temp_axis);
				}
				vectoangles(temp_axis, cent->gent->client->renderInfo.eyeAngles);
				//estimate where the neck would be...
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Z, temp_axis); //go down to find neck
				VectorMA(cent->gent->client->renderInfo.eyePoint, 8, temp_axis,
					cent->gent->client->renderInfo.headPoint);

				// Play the breath puffs (or not).
				CG_BreathPuffs(cent, temp_angles, ent.origin);
			}
			//Get torsoPoint & torsoAngles
			if (cent->gent->chestBolt >= 0)
			{
				gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->chestBolt, &bolt_matrix,
					temp_angles, ent.origin, cg.time, cgs.model_draw, cent->currentState.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.torsoPoint);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Z, temp_axis);
				vectoangles(temp_axis, cent->gent->client->renderInfo.torsoAngles);
			}
			else
			{
				VectorCopy(ent.origin, cent->gent->client->renderInfo.torsoPoint);
				VectorClear(cent->gent->client->renderInfo.torsoAngles);
			}
			//get crotchPoint
			if (cent->gent->crotchBolt >= 0)
			{
				gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, cent->gent->crotchBolt,
					&bolt_matrix,
					temp_angles, ent.origin, cg.time, cgs.model_draw, cent->currentState.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, cent->gent->client->renderInfo.crotchPoint);
			}
			else
			{
				VectorCopy(ent.origin, cent->gent->client->renderInfo.crotchPoint);
			}
			//NOTE: these are used for any case where an NPC fires and the next shot needs to come out
			//		of a new barrel/point.  That way the muzzleflash will draw on the old barrel/point correctly
			//NOTE: I'm only doing this for the saboteur right now - AT-STs might need this... others?
			vec3_t old_mp = { 0, 0, 0 };
			vec3_t old_md = { 0, 0, 0 };

			if (!calcedMp)
			{
				if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_ATST)
				{
					//FIXME: different for the three different weapon positions
					mdxaBone_t matrix;
					int bolt;
					entityState_t* es;

					es = &cent->currentState;

					// figure out where the actual model muzzle is
					if (es->weapon == WP_ATST_MAIN)
					{
						if (!es->number)
						{
							//player, just use left one, I guess
							if (cent->gent->alt_fire)
							{
								bolt = cent->gent->handRBolt;
							}
							else
							{
								bolt = cent->gent->handLBolt;
							}
						}
						else if (cent->gent->count > 0)
						{
							cent->gent->count = 0;
							bolt = cent->gent->handLBolt;
						}
						else
						{
							cent->gent->count = 1;
							bolt = cent->gent->handRBolt;
						}
					}
					else // ATST SIDE weapons
					{
						if (cent->gent->alt_fire)
						{
							bolt = cent->gent->genericBolt2;
						}
						else
						{
							bolt = cent->gent->genericBolt1;
						}
					}

					gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, bolt, &matrix, temp_angles,
						ent.origin, cg.time, cgs.model_draw, cent->currentState.modelScale);

					// work the matrix axis stuff into the original axis and origins used.
					gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN, cent->gent->client->renderInfo.muzzlePoint);
					gi.G2API_GiveMeVectorFromMatrix(matrix, NEGATIVE_Y, cent->gent->client->renderInfo.muzzleDir);
				}
				else if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_GALAKMECH)
				{
					int bolt = -1;
					if (cent->gent->lockCount)
					{
						//using the big laser beam
						bolt = cent->gent->handLBolt;
					}
					else //repeater
					{
						if (cent->gent->alt_fire)
						{
							//fire from the lower barrel (not that anyone will ever notice this, but...)
							bolt = cent->gent->genericBolt3;
						}
						else
						{
							bolt = cent->gent->handRBolt;
						}
					}

					if (bolt == -1)
					{
						VectorCopy(ent.origin, cent->gent->client->renderInfo.muzzlePoint);
						AngleVectors(temp_angles, cent->gent->client->renderInfo.muzzleDir, nullptr, nullptr);
					}
					else
					{
						gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel, bolt, &bolt_matrix,
							temp_angles, ent.origin, cg.time, cgs.model_draw,
							cent->currentState.modelScale);

						// work the matrix axis stuff into the original axis and origins used.
						gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN,
							cent->gent->client->renderInfo.muzzlePoint);
						gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y,
							cent->gent->client->renderInfo.muzzleDir);
					}
				}
				// Set the Vehicle Muzzle Point and Direction.
				else if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_VEHICLE)
				{
					// Get the Position and Direction of the Tag and use that as our Muzzles Properties.
					mdxaBone_t matrix;
					vec3_t velocity;
					VectorCopy(cent->gent->client->ps.velocity, velocity);
					velocity[2] = 0;
					for (int i = 0; i < MAX_VEHICLE_MUZZLES; i++)
					{
						if (cent->gent->m_pVehicle->m_iMuzzleTag[i] != -1)
						{
							gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->playerModel,
								cent->gent->m_pVehicle->m_iMuzzleTag[i], &matrix,
								cent->lerpAngles, ent.origin, cg.time, cgs.model_draw,
								cent->currentState.modelScale);
							gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN,
								cent->gent->m_pVehicle->m_Muzzles[i].m_vMuzzlePos);
							gi.G2API_GiveMeVectorFromMatrix(matrix, NEGATIVE_Y,
								cent->gent->m_pVehicle->m_Muzzles[i].m_vMuzzleDir);
							VectorMA(cent->gent->m_pVehicle->m_Muzzles[i].m_vMuzzlePos, 0.075f, velocity,
								cent->gent->m_pVehicle->m_Muzzles[i].m_vMuzzlePos);
						}
						else
						{
							break;
						}
					}
				}
				else if ((cent->gent->client && cent->gent->NPC
					&& (cent->gent->s.weapon == WP_BLASTER_PISTOL)
					&& cent->gent->weaponModel[1]) || (cent->gent->client
						&& cent->gent->s.weapon == WP_DUAL_PISTOL
						&& cent->currentState.eFlags & EF2_JANGO_DUALS
						&& !G_IsRidingVehicle(cent->gent)
						&& cent->gent->weaponModel[1]) || (cent->gent->s.weapon == WP_DUAL_CLONEPISTOL
							&& cent->currentState.eFlags & EF2_DUAL_CLONE_PISTOLS
							&& !G_IsRidingVehicle(cent->gent)
							&& cent->gent->weaponModel[1]) || (cent->gent->client
								&& (cent->gent->s.weapon == WP_BLASTER_PISTOL ||
									cent->gent->s.weapon == WP_REBELBLASTER ||
									cent->gent->s.weapon == WP_REY ||
									cent->gent->s.weapon == WP_JANGO ||
									cent->gent->s.weapon == WP_CLONEPISTOL)
								&& (cent->currentState.eFlags & EF2_DUAL_PISTOLS || cent->currentState.eFlags & EF2_DUAL_CALONORD)
								&& !G_IsRidingVehicle(cent->gent)
								&& cent->gent->weaponModel[1]) || (cent->gent->client
									&& (cent->gent->s.weapon == WP_DROIDEKA || cent->gent->s.weapon == WP_DUAL_PISTOL || cent->gent->s.weapon == WP_DUAL_CLONEPISTOL)
									&& !G_IsRidingVehicle(cent->gent)
									&& cent->gent->weaponModel[1])) //one in each hand
				{
					qboolean get_both = qfalse;
					int old_one = 0;
					if (cent->muzzleFlashTime > 0 && w_data && !(cent->currentState.eFlags & EF_LOCKED_TO_WEAPON))
					{
						//we need to get both muzzles since we're toggling and we fired recently
						get_both = qtrue;
						old_one = cent->gent->count ? 0 : 1;
					}
					if (cent->gent->weaponModel[cent->gent->count] != -1
						&& cent->gent->ghoul2.size() > cent->gent->weaponModel[cent->gent->count]
						&& cent->gent->ghoul2[cent->gent->weaponModel[cent->gent->count]].mModelindex != -1)
					{
						//get whichever one we're using now
						mdxaBone_t matrix;
						// figure out where the actual model muzzle is
						gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->weaponModel[cent->gent->count], 0,
							&matrix, temp_angles, ent.origin, cg.time, cgs.model_draw,
							cent->currentState.modelScale);
						// work the matrix axis stuff into the original axis and origins used.
						gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN,
							cent->gent->client->renderInfo.muzzlePoint);
						gi.G2API_GiveMeVectorFromMatrix(matrix, NEGATIVE_Y,
							cent->gent->client->renderInfo.muzzleDir);
					}
					//get the old one too, if needbe, and store it in muzzle2
					if (get_both
						&& cent->gent->weaponModel[old_one] != -1 //have a second weapon
						&& cent->gent->ghoul2.size() > cent->gent->weaponModel[old_one]
						//have a valid ghoul model index
						&& cent->gent->ghoul2[cent->gent->weaponModel[old_one]].mModelindex != -1)
						//model exists and was loaded
					{
						//saboteur commando, toggle the muzzle point back and forth between the two pistols each time he fires
						mdxaBone_t matrix;
						// figure out where the actual model muzzle is
						gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->weaponModel[old_one], 0, &matrix,
							temp_angles, ent.origin, cg.time, cgs.model_draw,
							cent->currentState.modelScale);
						// work the matrix axis stuff into the original axis and origins used.
						gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN, old_mp);
						gi.G2API_GiveMeVectorFromMatrix(matrix, NEGATIVE_Y, old_md);
					}
				}
				else if (cent->gent->weaponModel[0] != -1 &&
					cent->gent->ghoul2.size() > cent->gent->weaponModel[0] &&
					cent->gent->ghoul2[cent->gent->weaponModel[0]].mModelindex != -1)
				{
					mdxaBone_t matrix;
					// figure out where the actual model muzzle is
					gi.G2API_GetBoltMatrix(cent->gent->ghoul2, cent->gent->weaponModel[0], 0, &matrix, temp_angles,
						ent.origin, cg.time, cgs.model_draw, cent->currentState.modelScale);
					// work the matrix axis stuff into the original axis and origins used.
					gi.G2API_GiveMeVectorFromMatrix(matrix, ORIGIN, cent->gent->client->renderInfo.muzzlePoint);
					gi.G2API_GiveMeVectorFromMatrix(matrix, NEGATIVE_Y, cent->gent->client->renderInfo.muzzleDir);
				}
				else
				{
					VectorCopy(cent->gent->client->renderInfo.eyePoint, cent->gent->client->renderInfo.muzzlePoint);
					AngleVectors(cent->gent->client->renderInfo.eyeAngles, cent->gent->client->renderInfo.muzzleDir,
						nullptr, nullptr);
				}
				cent->gent->client->renderInfo.mPCalcTime = cg.time;
			}

			// Draw Vehicle Muzzle Flashs.
			if (cent->gent && cent->gent->client && cent->gent->client->NPC_class == CLASS_VEHICLE)
			{
				for (int i = 0; i < MAX_VEHICLE_MUZZLES; i++)
				{
					if (cent->gent->m_pVehicle->m_Muzzles[i].m_bFired)
					{
						const char* effect = &weaponData[cent->gent->m_pVehicle->m_pVehicleInfo->weapMuzzle[i]].
							mMuzzleEffect[0];
						if (effect)
						{
							theFxScheduler.PlayEffect(effect, cent->gent->m_pVehicle->m_Muzzles[i].m_vMuzzlePos,
								cent->gent->m_pVehicle->m_Muzzles[i].m_vMuzzleDir);
						}
						cent->gent->m_pVehicle->m_Muzzles[i].m_bFired = false;
					}
				}
			}
			// Pick the right effect for the type of weapon we are, defaults to no effect unless explicitly specified
			else if (cent->muzzleFlashTime > 0 && w_data && !(cent->currentState.eFlags & EF_LOCKED_TO_WEAPON))
			{
				const char* effect = nullptr;

				cent->muzzleFlashTime = 0;

				// Try and get a default muzzle so we have one to fall back on
				if (w_data->mMuzzleEffect[0])
				{
					effect = &w_data->mMuzzleEffect[0];
				}

				if (cent->alt_fire)
				{
					// We're alt-firing, so see if we need to override with a custom alt-fire effect
					if (w_data->mAltMuzzleEffect[0])
					{
						effect = &w_data->mAltMuzzleEffect[0];
					}
				}

				if (effect)
				{
					if ((cent->gent->s.weapon == WP_BLASTER_PISTOL ||
						cent->gent->s.weapon == WP_REBELBLASTER ||
						cent->gent->s.weapon == WP_REY ||
						cent->gent->s.weapon == WP_JANGO ||
						cent->gent->s.weapon == WP_CLONEPISTOL ||
						cent->gent->s.weapon == WP_DUAL_CLONEPISTOL ||
						cent->gent->s.weapon == WP_DUAL_PISTOL
						&& (cent->currentState.eFlags & EF2_JANGO_DUALS || cent->currentState.eFlags & EF2_DUAL_CLONE_PISTOLS || cent->currentState.eFlags & EF2_DUAL_PISTOLS || cent->currentState.eFlags & EF2_DUAL_CALONORD))
						&& !G_IsRidingVehicle(cent->gent)) //PM_WeaponOkOnVehicle)
					{
						if (!VectorCompare(old_mp, vec3_origin) && !VectorCompare(old_md, vec3_origin))
						{
							//we have an old muzzlePoint we want to use
							theFxScheduler.PlayEffect(effect, old_mp, old_md);
						}
						else
						{
							//use the current one
							theFxScheduler.PlayEffect(effect, cent->gent->client->renderInfo.muzzlePoint,
								cent->gent->client->renderInfo.muzzleDir);
						}
					}
					else if (cent->gent->s.weapon == WP_DROIDEKA && !G_IsRidingVehicle(cent->gent)) //PM_WeaponOkOnVehicle)
					{
						if (!VectorCompare(old_mp, vec3_origin) && !VectorCompare(old_md, vec3_origin))
						{
							//we have an old muzzlePoint we want to use
							theFxScheduler.PlayEffect(effect, old_mp, old_md);
						}
						else
						{
							//use the current one
							theFxScheduler.PlayEffect(effect, cent->gent->client->renderInfo.muzzlePoint,
								cent->gent->client->renderInfo.muzzleDir);
						}
					}
					else
					{
						// We got an effect and we're firing, so let 'er rip.
						theFxScheduler.PlayEffect(effect, cent->currentState.client_num);
					}
				}
			}

			if (!in_camera && cent->muzzleOverheatTime > 0 && w_data)
			{
				const char* effect = nullptr;
				if (!cg.renderingThirdPerson && cg_trueguns.integer)
				{
					if (w_data->mTrueOverloadMuzzleEffect[0])
					{
						effect = &w_data->mTrueOverloadMuzzleEffect[0];
					}

					if (effect)
					{
						if ((cent->gent->s.weapon == WP_BLASTER_PISTOL ||
							cent->gent->s.weapon == WP_REBELBLASTER ||
							cent->gent->s.weapon == WP_REY ||
							cent->gent->s.weapon == WP_JANGO ||
							cent->gent->s.weapon == WP_CLONEPISTOL ||
							cent->gent->s.weapon == WP_DUAL_CLONEPISTOL ||
							cent->gent->s.weapon == WP_DUAL_PISTOL
							&& (cent->currentState.eFlags & EF2_JANGO_DUALS || cent->currentState.eFlags & EF2_DUAL_CLONE_PISTOLS || cent->currentState.eFlags & EF2_DUAL_PISTOLS || cent->currentState.eFlags & EF2_DUAL_CALONORD))
							&& !G_IsRidingVehicle(cent->gent)) //PM_WeaponOkOnVehicle)
						{
							if (!VectorCompare(old_mp, vec3_origin) && !VectorCompare(old_md, vec3_origin))
							{
								//we have an old muzzlePoint we want to use
								theFxScheduler.PlayEffect(effect, old_mp, old_md);
							}
							else
							{
								//use the current one
								theFxScheduler.PlayEffect(effect, cent->gent->client->renderInfo.muzzlePoint,
									cent->gent->client->renderInfo.muzzleDir);
							}
						}
						else if (cent->gent->s.weapon == WP_DROIDEKA && !G_IsRidingVehicle(cent->gent)) //PM_WeaponOkOnVehicle)
						{
							if (!VectorCompare(old_mp, vec3_origin) && !VectorCompare(old_md, vec3_origin))
							{
								//we have an old muzzlePoint we want to use
								theFxScheduler.PlayEffect(effect, old_mp, old_md);
							}
							else
							{
								//use the current one
								theFxScheduler.PlayEffect(effect, cent->gent->client->renderInfo.muzzlePoint,
									cent->gent->client->renderInfo.muzzleDir);
							}
						}
						else
						{
							// We got an effect and we're firing, so let 'er rip.
							theFxScheduler.PlayEffect(effect, cent->currentState.client_num);
						}
					}
					cent->muzzleOverheatTime = 0;
				}
				else
				{
					if (w_data->mOverloadMuzzleEffect[0])
					{
						effect = &w_data->mOverloadMuzzleEffect[0];
					}

					if (effect)
					{
						if ((cent->gent->s.weapon == WP_BLASTER_PISTOL ||
							cent->gent->s.weapon == WP_REBELBLASTER ||
							cent->gent->s.weapon == WP_REY ||
							cent->gent->s.weapon == WP_JANGO ||
							cent->gent->s.weapon == WP_CLONEPISTOL ||
							cent->gent->s.weapon == WP_DUAL_CLONEPISTOL ||
							cent->gent->s.weapon == WP_DUAL_PISTOL
							&& (cent->currentState.eFlags & EF2_JANGO_DUALS || cent->currentState.eFlags & EF2_DUAL_CLONE_PISTOLS || cent->currentState.eFlags & EF2_DUAL_PISTOLS || cent->currentState.eFlags & EF2_DUAL_CALONORD))
							&& !G_IsRidingVehicle(cent->gent)) //PM_WeaponOkOnVehicle)
						{
							if (!VectorCompare(old_mp, vec3_origin) && !VectorCompare(old_md, vec3_origin))
							{
								//we have an old muzzlePoint we want to use
								theFxScheduler.PlayEffect(effect, old_mp, old_md);
							}
							else
							{
								//use the current one
								theFxScheduler.PlayEffect(effect, cent->gent->client->renderInfo.muzzlePoint,
									cent->gent->client->renderInfo.muzzleDir);
							}
						}
						else if (cent->gent->s.weapon == WP_DROIDEKA && !G_IsRidingVehicle(cent->gent)) //PM_WeaponOkOnVehicle)
						{
							if (!VectorCompare(old_mp, vec3_origin) && !VectorCompare(old_md, vec3_origin))
							{
								//we have an old muzzlePoint we want to use
								theFxScheduler.PlayEffect(effect, old_mp, old_md);
							}
							else
							{
								//use the current one
								theFxScheduler.PlayEffect(effect, cent->gent->client->renderInfo.muzzlePoint,
									cent->gent->client->renderInfo.muzzleDir);
							}
						}
						else
						{
							// We got an effect and we're firing, so let 'er rip.
							theFxScheduler.PlayEffect(effect, cent->currentState.client_num);
						}
					}
					cent->muzzleOverheatTime = 0;
				}
			}

			if (cent->gent->client && cent->gent->forcePushTime > cg.time)
			{
				//being pushed
				if (cg_jkoeffects.integer == 0) //JKA
				{
					CG_ForcePushBodyBlur(cent, ent.origin, temp_angles);
				}
				else if (cg_jkoeffects.integer == 1) //JKO
				{
					CG_ForcePushBodyBlur(cent, ent.origin, temp_angles);
				}
				else if (cg_jkoeffects.integer == 2) // NO EFFECT
				{
					//no effect
				}
				else // BACKUP
				{
					CG_ForcePushBodyBlur(cent, ent.origin, temp_angles);
				}
			}

			if (cent->gent->client->ps.forcePowersActive & 1 << FP_DEADLYSIGHT)
			{
				//doing the gripping
				CG_ForceDeadlySightBlur(cent->gent->client->renderInfo.headPoint);
			}

			if (cent->gent->client->ps.forcePowersActive & 1 << FP_GRIP)
			{
				//doing the gripping
				CG_ForcePushBlur(cent->gent->client->renderInfo.handLPoint, qtrue);
			}

			if (cent->gent->client->ps.eFlags & EF_FORCE_GRIPPED)
			{
				//being gripped
				CG_ForcePushBlur(cent->gent->client->renderInfo.headPoint, qtrue);
			}

			if (cent->gent->client->ps.eFlags & EF_FORCE_GRASPED)
			{
				//being gripped
				CG_GraspBlur(cent->gent->client->renderInfo.crotchPoint);
			}

			if (cent->gent->client && cent->gent->client->ps.powerups[PW_SHOCKED] > cg.time)
			{
				//being electrocuted
				CG_ForceElectrocution(cent, ent.origin, temp_angles, cgs.media.boltShader);
			}

			if (cent->gent->client && cent->gent->client->ps.powerups[PW_STUNNED] > cg.time)
			{
				//being electrocuted
				CG_ForceElectrocution(cent, ent.origin, temp_angles, cgs.media.shockShader);
			}

			if (cent->gent->client->ps.forcePowersActive & 1 << FP_GRASP)
			{
				//doing the gripping
				CG_GraspBlur(cent->gent->client->renderInfo.handLPoint);
			}

			if (cent->gent->client->ps.eFlags & EF_FORCE_DRAINED
				|| cent->currentState.powerups & 1 << PW_DRAINED)
			{
				//being drained
				//do red electricity lines off them and red drain shell on them
				CG_ForceElectrocution(cent, ent.origin, temp_angles, cgs.media.drainShader, qtrue);
			}

			if (cent->gent->client->ps.forcePowersActive & 1 << FP_LIGHTNING)
			{
				//doing the electrocuting
				//FIXME: if the target is absorbing or blocking lightning w/saber, draw a beam from my hand to his (hand?chest?saber?)
				vec3_t t_ang;
				VectorCopy(cent->lerpAngles, t_ang);
				if (cent->gent->client->ps.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_2)
				{
					//arc
					vec3_t fx_axis[3];
					AnglesToAxis(t_ang, fx_axis);

					if (cg_SerenityJediEngineMode.integer == 2)
					{
						theFxScheduler.PlayEffect(cgs.effects.forceLightningWide_md,
							cent->gent->client->renderInfo.handLPoint, fx_axis);
					}
					else
					{
						theFxScheduler.PlayEffect(cgs.effects.forceLightningWide,
							cent->gent->client->renderInfo.handLPoint, fx_axis);
					}
					if (cent->gent->client->ps.torsoAnim == BOTH_FORCE_2HANDEDLIGHTNING
						|| cent->gent->client->ps.torsoAnim == BOTH_FORCE_2HANDEDLIGHTNING_START
						|| cent->gent->client->ps.torsoAnim == BOTH_FORCE_2HANDEDLIGHTNING_HOLD
						|| cent->gent->client->ps.torsoAnim == BOTH_FORCE_2HANDEDLIGHTNING_RELEASE)
					{
						//jackin' 'em up, Palpatine-style
						if (cg_SerenityJediEngineMode.integer == 2)
						{
							theFxScheduler.PlayEffect(cgs.effects.forceLightningWide_md,
								cent->gent->client->renderInfo.handRPoint, fx_axis);
						}
						else
						{
							theFxScheduler.PlayEffect(cgs.effects.forceLightningWide,
								cent->gent->client->renderInfo.handRPoint, fx_axis);
						}
					}
				}
				else
				{
					vec3_t fx_dir;
					//line
					AngleVectors(t_ang, fx_dir, nullptr, nullptr);
					if (cg_SerenityJediEngineMode.integer == 2)
					{
						theFxScheduler.PlayEffect(cgs.effects.forceLightning_md,
							cent->gent->client->renderInfo.handLPoint, fx_dir);
					}
					else
					{
						theFxScheduler.PlayEffect(cgs.effects.forceLightning, cent->gent->client->renderInfo.handLPoint,
							fx_dir);
					}
				}
			}

			if (cent->gent->client->ps.forcePowersActive & 1 << FP_LIGHTNING_STRIKE)
			{
				//doing the lightning_strike
				vec3_t t_ang;
				VectorCopy(cent->lerpAngles, t_ang);

				if (cent->gent->client->ps.forcePowerLevel[FP_LIGHTNING_STRIKE] > FORCE_LEVEL_2)
				{
					//arc
					vec3_t fx_axis[3];
					AnglesToAxis(t_ang, fx_axis);
					theFxScheduler.PlayEffect(cgs.effects.yellowstrike, cent->gent->client->renderInfo.handLPoint,
						fx_axis);
				}
				else
				{
					vec3_t fx_dir;
					//line
					AngleVectors(t_ang, fx_dir, nullptr, nullptr);
					theFxScheduler.PlayEffect(cgs.effects.yellowstrike, cent->gent->client->renderInfo.handLPoint,
						fx_dir);
				}
			}

			if (cent->gent->client->ps.eFlags & EF_POWERING_ROSH)
			{
				vec3_t t_ang, fx_dir;
				VectorCopy(cent->lerpAngles, t_ang);
				AngleVectors(t_ang, fx_dir, nullptr, nullptr);
				theFxScheduler.PlayEffect(cgs.effects.forceDrain, cent->gent->client->renderInfo.handLPoint, fx_dir);
				//theFxScheduler.RegisterEffect( "force/dr1" )
			}

			if (cent->gent->client->ps.forcePowersActive & 1 << FP_DRAIN
				&& cent->gent->client->ps.forceDrainEntityNum >= ENTITYNUM_WORLD)
			{
				//doing the draining and not on a single person
				vec3_t t_ang;
				VectorCopy(cent->lerpAngles, t_ang);
				if (cent->gent->client->ps.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_2)
				{
					//arc
					vec3_t fx_axis[3];
					AnglesToAxis(t_ang, fx_axis);
					theFxScheduler.PlayEffect(cgs.effects.forceDrainWide, cent->gent->client->renderInfo.handLPoint,
						fx_axis);
				}
				else
				{
					vec3_t fx_dir;
					//line
					AngleVectors(t_ang, fx_dir, nullptr, nullptr);
					theFxScheduler.PlayEffect(cgs.effects.forceDrain, cent->gent->client->renderInfo.handLPoint,
						fx_dir);
				}
			}
			//spotlight?
			if (cent->currentState.eFlags & EF_SPOTLIGHT)
			{
				//FIXME: player's view should glare/flare if look at this... maybe build into the effect?
				// hack for the spotlight
				vec3_t org, eye_fwd;

				AngleVectors(cent->gent->client->renderInfo.eyeAngles, eye_fwd, nullptr, nullptr);
				theFxScheduler.PlayEffect("rockettrooper/light_cone", cent->gent->client->renderInfo.eyePoint, eye_fwd);
				// stay a bit back from the server-side's trace impact point...this may not be enough?
				VectorMA(cent->gent->client->renderInfo.eyePoint, cent->gent->speed - 5, eye_fwd, org);
				float radius = cent->gent->speed;
				if (radius < 128.0f)
				{
					radius = 128.0f;
				}
				else if (radius > 1024.0f)
				{
					radius = 1024.0f;
				}
				cgi_R_AddLightToScene(org, radius, 1.0f, 1.0f, 1.0f);
			}
		}
		//"refraction" effect -rww
		if (cent->gent->client->ps.powerups[PW_FORCE_PUSH] > cg.time)
		{
			if (cg_SerenityJediEngineMode.integer == 2 && (cent->gent->client->ps.weapon == WP_NONE || cent->gent->
				client->ps.weapon == WP_MELEE)
				&& cent->gent->client->ps.groundEntityNum == ENTITYNUM_NONE && cent->gent->client->NPC_class ==
				CLASS_GALEN)
			{
				vec3_t blue;
				VectorScale(colorTable[CT_LTBLUE1], 255.0f, blue);
				CG_ForcePushRefraction(cent->gent->client->renderInfo.crotchPoint, cent, 2.0f, blue);
			}
			else
			{
				if (cg_jkoeffects.integer == 0) //JKA
				{
					CG_ForcePushRefraction(cent->gent->client->renderInfo.handLPoint, cent, 1.0f, nullptr);
				}
				else if (cg_jkoeffects.integer == 1) //JKO
				{
					CG_ForcePushBlur(cent->gent->client->renderInfo.handLPoint);

					if (cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_MELEE || cent->
						currentState.weapon == WP_SABER && !cent->gent->client->ps.SaberActive())
					{
						CG_ForcePushBlur(cent->gent->client->renderInfo.handRPoint);
					}
				}
				else if (cg_jkoeffects.integer == 2) // NO EFFECT
				{
					//no effect
				}
				else // BACKUP
				{
					CG_ForcePushRefraction(cent->gent->client->renderInfo.handLPoint, cent, 1.0f, nullptr);
				}
			}
		}
		else if (cent->gent->client->ps.powerups[PW_FORCE_REPULSE] > cg.time)
		{
			vec3_t blue;
			VectorScale(colorTable[CT_LTBLUE1], 255.0f, blue);
			cgi_R_AddLightToScene(cent->lerpOrigin, 200 + (rand() & 31), 0.2f, 0.2f, 1.0f);
			CG_ForceRepulseRefraction(cent->gent->client->renderInfo.crotchPoint, cent, 2.0f, blue);
		}
		else if (cent->gent->client->ps.powerups[PW_FORCE_PUSH_RHAND] > cg.time)
		{
			if (cg_SerenityJediEngineMode.integer == 2 && (cent->gent->client->ps.weapon == WP_NONE || cent->gent->
				client->ps.weapon == WP_MELEE)
				&& cent->gent->client->ps.groundEntityNum == ENTITYNUM_NONE && cent->gent->client->NPC_class ==
				CLASS_GALEN)
			{
				vec3_t blue;
				VectorScale(colorTable[CT_LTBLUE1], 255.0f, blue);
				CG_ForcePushRefraction(cent->gent->client->renderInfo.crotchPoint, cent, 2.0f, blue);
			}
			else
			{
				if (cg_jkoeffects.integer == 0) //JKA
				{
					CG_ForcePushRefraction(cent->gent->client->renderInfo.handLPoint, cent, 1.0f, nullptr);
				}
				else if (cg_jkoeffects.integer == 1) //JKO
				{
					CG_ForcePushBlur(cent->gent->client->renderInfo.handLPoint);

					if (cent->gent->client->ps.weapon == WP_NONE || cent->gent->client->ps.weapon == WP_MELEE || cent->
						currentState.weapon == WP_SABER && !cent->gent->client->ps.SaberActive())
					{
						CG_ForcePushBlur(cent->gent->client->renderInfo.handRPoint);
					}
				}
				else if (cg_jkoeffects.integer == 2) // NO EFFECT
				{
					//no effect
				}
				else // BACKUP
				{
					CG_ForcePushRefraction(cent->gent->client->renderInfo.handLPoint, cent, 1.0f, nullptr);
				}
			}
		}
		else
		{
			cent->gent->client->ps.forcePowersActive &= ~(1 << FP_PULL);
		}

		//bolted effects
		CG_BoltedEffects(cent);
		//As good a place as any, I suppose, to do this keyframed sound thing
		CGG2_AnimEvents(cent);
		//setup old system for gun to look at
		if (cent->gent && cent->gent->client && cent->gent->client->ps.weapon == WP_SABER)
		{
			extern qboolean PM_KickingAnim(int anim);
			if (!PM_KickingAnim(cent->gent->client->ps.torsoAnim)
				|| cent->gent->client->ps.torsoAnim == BOTH_A7_KICK_S)
			{
				//not kicking (unless it's the spinning kick)
				if (cg_timescale.value < 1.0f && cent->gent->client->ps.forcePowersActive & 1 << FP_SPEED)
				{
					int wait = floor(static_cast<float>(FRAMETIME) / 2.0f);
					//sanity check
					if (cent->gent->client->ps.saberDamageDebounceTime - cg.time > wait)
					{
						//when you unpause the game with force speed on, the time gets *really* wiggy...
						cent->gent->client->ps.saberDamageDebounceTime = cg.time + wait;
					}
					if (cent->gent->client->ps.saberDamageDebounceTime <= cg.time)
					{
						//FIXME: this causes an ASSLOAD of effects
						WP_SabersDamageTrace(cent->gent, qtrue);
						WP_SaberUpdateOldBladeData(cent->gent);
						cent->gent->client->ps.saberDamageDebounceTime = cg.time + floor(
							static_cast<float>(wait) * cg_timescale.value);
					}
				}
			}
		}
	}
	else
	{
		refEntity_t legs;
		refEntity_t torso;
		refEntity_t head;
		refEntity_t gun;
		refEntity_t flash;
		refEntity_t flashlight;
		int renderfx;

		/*
		Ghoul2 Insert End
		*/

		memset(&legs, 0, sizeof legs);
		memset(&torso, 0, sizeof torso);
		memset(&head, 0, sizeof head);
		memset(&gun, 0, sizeof gun);
		memset(&flash, 0, sizeof flash);
		memset(&flashlight, 0, sizeof flashlight);

		// Weapon sounds may need to be stopped, so check now
		CG_StopWeaponSounds(cent);

		//FIXME: pass in the axis/angles offset between the tag_torso and the tag_head?
		// get the rotation information
		CG_PlayerAngles(cent, legs.axis, torso.axis, head.axis);
		if (cent->gent && cent->gent->client)
		{
			cent->gent->client->ps.legsYaw = cent->lerpAngles[YAW];
		}

		// get the animation state (after rotation, to allow feet shuffle)
		// NB: Also plays keyframed animSounds (Bob- hope you dont mind, I was here late and at about 5:30 Am i needed to do something to keep me awake and i figured you wouldn't mind- you might want to check it, though, to make sure I wasn't smoking crack and missed something important, it is pretty late and I'm getting pretty close to being up for 24 hours here, so i wouldn't doubt if I might have messed something up, but i tested it and it looked right.... noticed in old code base i was doing it wrong too, whic            h explains why I was getting so many damn sounds all the time!  I had to lower the probabilities because it seemed like i was getting way too many sounds, and that was the problem!  Well, should be fixed now I think...)
		CG_PlayerAnimation(cent, &legs.oldframe, &legs.frame, &legs.backlerp,
			&torso.oldframe, &torso.frame, &torso.backlerp);

		cent->gent->client->renderInfo.legsFrame = cent->pe.legs.frame;
		cent->gent->client->renderInfo.torsoFrame = cent->pe.torso.frame;

		// add powerups floating behind the player
		CG_PlayerPowerups(cent);

		// add the shadow
		shadow = CG_PlayerShadow(cent, &shadow_plane);

		// add a water splash if partially in and out of water
		CG_PlayerSplash(cent);

		// if we've been hit, display proper fullscreen fx
		CG_PlayerHitFX(cent);

		// get the player model information
		renderfx = 0;
		if (!cg.renderingThirdPerson || cg.zoomMode)
		{
			if (cg.snap->ps.viewEntity <= 0 || cg.snap->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//no viewentity
				if (cent->currentState.number == cg.snap->ps.client_num)
				{
					//I am the player
					if (cg.zoomMode || !cg_trueguns.integer && cg.snap->ps.weapon != WP_SABER && cg.snap->ps.weapon !=
						WP_MELEE || cg.snap->ps.weapon == WP_SABER && cg_truesaberonly.integer)
					{
						//not using saber or fists
						renderfx = RF_THIRD_PERSON; // only draw in mirrors
					}
				}
			}
			else if (cent->currentState.number == cg.snap->ps.viewEntity)
			{
				//I am the view entity
				if (cg.zoomMode || !cg_trueguns.integer && cg.snap->ps.weapon != WP_SABER && cg.snap->ps.weapon !=
					WP_MELEE || cg.snap->ps.weapon == WP_SABER && cg_truesaberonly.integer)
				{
					//not using saber or fists
					renderfx = RF_THIRD_PERSON; // only draw in mirrors
				}
			}
		}

		if (cg_shadows.integer == 2 || cg_shadows.integer == 3 && shadow)
		{
			renderfx |= RF_SHADOW_PLANE;
		}
		renderfx |= RF_LIGHTING_ORIGIN; // use the same origin for all
		if (cent->gent->NPC && cent->gent->NPC->scriptFlags & SCF_MORELIGHT)
		{
			renderfx |= RF_MINLIGHT; //bigger than normal min light
		}

		if (cent->gent->client && cent->gent->flags & FL_MORELIGHPLAYER)
		{
			renderfx |= RF_MINLIGHT; //bigger than normal min light
		}

		if (cent->gent && cent->gent->client)
		{
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.headPoint);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.handRPoint);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.handLPoint);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.footRPoint);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.footLPoint);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.torsoPoint);
			VectorCopy(cent->lerpAngles, cent->gent->client->renderInfo.torsoAngles);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.crotchPoint);
		}
		if (cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD && cg.snap->ps.viewEntity == cent->
			currentState.client_num)
		{
			//player is in an entity camera view, ME
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.eyePoint);
			VectorCopy(cent->lerpAngles, cent->gent->client->renderInfo.eyeAngles);
			VectorCopy(cent->lerpOrigin, cent->gent->client->renderInfo.headPoint);
		}
		//
		// add the legs
		//
		legs.hModel = ci->legsModel;
		legs.customSkin = ci->legsSkin;

		VectorCopy(cent->lerpOrigin, legs.origin);

		//Scale applied to a refEnt will apply to any models attached to it...
		//This seems to copy the scale to every piece attached, kinda cool, but doesn't
		//allow the body to be scaled up without scaling a bolt on or whatnot...
		//Only apply scale if it's not 100% scale...
		if (cent->currentState.modelScale[0] != 0.0f)
		{
			VectorScale(legs.axis[0], cent->currentState.modelScale[0], legs.axis[0]);
			legs.nonNormalizedAxes = qtrue;
		}

		if (cent->currentState.modelScale[1] != 0.0f)
		{
			VectorScale(legs.axis[1], cent->currentState.modelScale[1], legs.axis[1]);
			legs.nonNormalizedAxes = qtrue;
		}

		if (cent->currentState.modelScale[2] != 0.0f)
		{
			VectorScale(legs.axis[2], cent->currentState.modelScale[2], legs.axis[2]);
			legs.nonNormalizedAxes = qtrue;
			if (!static_scale)
			{
				//FIXME:? need to know actual height of leg model bottom to origin, not hardcoded
				legs.origin[2] += 24 * (cent->currentState.modelScale[2] - 1);
			}
		}

		VectorCopy(legs.origin, legs.lightingOrigin);
		legs.shadowPlane = shadow_plane;
		legs.renderfx = renderfx;
		VectorCopy(legs.origin, legs.oldorigin); // don't positionally lerp at all

		CG_AddRefEntityWithPowerups(&legs, cent->currentState.powerups, cent);

		// if the model failed, allow the default nullmodel to be displayed
		if (!legs.hModel)
		{
			return;
		}

		//
		// add the torso
		//
		torso.hModel = ci->torsoModel;
		if (torso.hModel)
		{
			const weaponInfo_t* weapon;
			orientation_t tag_torso;

			torso.customSkin = ci->torsoSkin;

			VectorCopy(cent->lerpOrigin, torso.lightingOrigin);

			CG_PositionRotatedEntityOnTag(&torso, &legs, legs.hModel, "tag_torso", &tag_torso);
			VectorCopy(torso.origin, cent->gent->client->renderInfo.torsoPoint);
			vectoangles(tag_torso.axis[0], cent->gent->client->renderInfo.torsoAngles);

			torso.shadowPlane = shadow_plane;
			torso.renderfx = renderfx;

			CG_AddRefEntityWithPowerups(&torso, cent->currentState.powerups, cent);

			//
			// add the head
			//
			head.hModel = ci->headModel;
			if (head.hModel)
			{
				orientation_t tag_head;

				VectorCopy(cent->lerpOrigin, head.lightingOrigin);

				CG_PositionRotatedEntityOnTag(&head, &torso, torso.hModel, "tag_head", &tag_head);
				VectorCopy(head.origin, cent->gent->client->renderInfo.headPoint);
				vectoangles(tag_head.axis[0], cent->gent->client->renderInfo.headAngles);

				head.shadowPlane = shadow_plane;
				head.renderfx = renderfx;

				CG_AddRefEntityWithPowerups(&head, cent->currentState.powerups, cent);

				if (cent->gent && cent->gent->NPC && (cent->gent->NPC->confusionTime > cg.time || cent->gent->NPC->
					charmedTime > cg.time || cent->gent->NPC->controlledTime > cg.time))
				{
					// we are currently confused, so play an effect
					if (TIMER_Done(cent->gent, "confusionEffectDebounce"))
					{
						//ARGH!!!
						theFxScheduler.PlayEffect(cgs.effects.forceConfusion, head.origin);
						TIMER_Set(cent->gent, "confusionEffectDebounce", 1000);
					}
				}

				if (!calcedMp)
				{
					//First person player's eyePoint and eyeAngles should be copies from cg.refdef...
					//Calc this client's eyepoint
					VectorCopy(head.origin, cent->gent->client->renderInfo.eyePoint);
					vectoangles(head.axis[0], cent->gent->client->renderInfo.eyeAngles);
				}
			}
			else
			{
				VectorCopy(torso.origin, cent->gent->client->renderInfo.eyePoint);
				cent->gent->client->renderInfo.eyePoint[2] += cent->gent->maxs[2] - 4;
				vectoangles(torso.axis[0], cent->gent->client->renderInfo.eyeAngles);
			}

			//
			// add the gun
			//
			CG_RegisterWeapon(cent->currentState.weapon);
			weapon = &cg_weapons[cent->currentState.weapon];

			if (cg_com_kotor.integer == 1 || cent->gent->client->charKOTORWeapons == 1) //playing kotor
			{
				gun.hModel = weapon->altWeaponWorldModel;
			}
			else
			{
				gun.hModel = weapon->weaponWorldModel;
			}

			if (gun.hModel)
			{
				qboolean drawGun = qtrue;

				VectorCopy(cent->lerpOrigin, gun.lightingOrigin);

				CG_PositionEntityOnTag(&gun, &torso, torso.hModel, "tag_weapon");

				gun.shadowPlane = shadow_plane;
				gun.renderfx = renderfx;

				if (drawGun)
				{
					CG_AddRefEntityWithPowerups(
						&gun, cent->currentState.powerups & (1 << PW_CLOAKED | 1 << PW_BATTLESUIT), cent);
				}

				//
				// add the flash (even if invisible)
				//

				// impulse flash
				if (cent->muzzleFlashTime > 0 && w_data && !(cent->currentState.eFlags & EF_LOCKED_TO_WEAPON))
				{
					int effect = 0;

					cent->muzzleFlashTime = 0;

					CG_PositionEntityOnTag(&flash, &gun, gun.hModel, "tag_flash");

					// Try and get a default muzzle so we have one to fall back on
					if (w_data->mMuzzleEffectID)
					{
						effect = w_data->mMuzzleEffectID;
					}

					if (cent->currentState.eFlags & EF_ALT_FIRING)
					{
						// We're alt-firing, so see if we need to override with a custom alt-fire effect
						if (w_data->mAltMuzzleEffectID)
						{
							effect = w_data->mAltMuzzleEffectID;
						}
					}

					if ((cent->currentState.eFlags & EF_FIRING || cent->currentState.eFlags & EF_ALT_FIRING) && effect)
					{
						vec3_t up = { 0, 0, 1 }, ax[3]{};

						VectorCopy(flash.axis[0], ax[0]);

						CrossProduct(up, ax[0], ax[1]);
						CrossProduct(ax[0], ax[1], ax[2]);

						if (cent->gent && cent->gent->NPC || cg.renderingThirdPerson)
						{
							theFxScheduler.PlayEffect(effect, flash.origin, ax);
						}
						else
						{
							// We got an effect and we're firing, so let 'er rip.
							theFxScheduler.PlayEffect(effect, flash.origin, ax);
						}
					}
				}

				if (!in_camera && cent->muzzleOverheatTime > 0 && w_data)
				{
					int effect = 0;
					if (!cg.renderingThirdPerson && cg_trueguns.integer)
					{
						if (w_data->mTrueOverloadMuzzleEffectID)
						{
							effect = w_data->mTrueOverloadMuzzleEffectID;
						}

						if (effect)
						{
							vec3_t up = { 0, 0, 1 }, ax[3]{};

							VectorCopy(flash.axis[0], ax[0]);

							CrossProduct(up, ax[0], ax[1]);
							CrossProduct(ax[0], ax[1], ax[2]);

							if (cent->gent && cent->gent->NPC || cg.renderingThirdPerson)
							{
								theFxScheduler.PlayEffect(effect, flash.origin, ax);
							}
							else
							{
								// We got an effect and we're firing, so let 'er rip.
								theFxScheduler.PlayEffect(effect, flash.origin, ax);
							}
						}
						cent->muzzleOverheatTime = 0;
					}
					else
					{
						if (w_data->mOverloadMuzzleEffectID)
						{
							effect = w_data->mOverloadMuzzleEffectID;
						}

						if (effect)
						{
							vec3_t up = { 0, 0, 1 }, ax[3]{};

							VectorCopy(flash.axis[0], ax[0]);

							CrossProduct(up, ax[0], ax[1]);
							CrossProduct(ax[0], ax[1], ax[2]);

							if (cent->gent && cent->gent->NPC || cg.renderingThirdPerson)
							{
								theFxScheduler.PlayEffect(effect, flash.origin, ax);
							}
							else
							{
								// We got an effect and we're firing, so let 'er rip.
								theFxScheduler.PlayEffect(effect, flash.origin, ax);
							}
						}
						cent->muzzleOverheatTime = 0;
					}
				}

				if (!calcedMp && !(cent->currentState.eFlags & EF_LOCKED_TO_WEAPON))
				{
					int i;
					// Set the muzzle point
					orientation_t orientation;

					cgi_R_LerpTag(&orientation, weapon->weaponModel, gun.oldframe, gun.frame, 1.0f - gun.backlerp,
						"tag_flash");

					VectorCopy(gun.origin, cent->gent->client->renderInfo.muzzlePoint);
					for (i = 0; i < 3; i++)
					{
						VectorMA(cent->gent->client->renderInfo.muzzlePoint, orientation.origin[i], gun.axis[i],
							cent->gent->client->renderInfo.muzzlePoint);
					}

					cent->gent->client->renderInfo.mPCalcTime = cg.time;
				}
			}
		}
		else
		{
			VectorCopy(legs.origin, cent->gent->client->renderInfo.eyePoint);
			cent->gent->client->renderInfo.eyePoint[2] += cent->gent->maxs[2] - 4;
			vectoangles(legs.axis[0], cent->gent->client->renderInfo.eyeAngles);
		}
	}

	if (cg_DebugSaberCombat.integer)
	{
		vec3_t bmins = { -15, -15, DEFAULT_MINS_2 }, bmaxs = { 15, 15, DEFAULT_MAXS_2 };

		if (cent->currentState.solid)
		{
			int zu;
			int zd;
			int x;
			x = cent->currentState.solid & 255;
			zd = cent->currentState.solid >> 8 & 255;
			zu = (cent->currentState.solid >> 16 & 255) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;
		}

		if (!(cent->currentState.powerups & 1 << PW_CLOAKED))
		{
			vec3_t absmax;
			vec3_t absmin;
			VectorAdd(cent->lerpOrigin, bmins, absmin);
			VectorAdd(cent->lerpOrigin, bmaxs, absmax);
			CG_CubeOutline(absmin, absmax, 1, COLOR_RED);
		}
	}

	if (cent->currentState.number == 0 && (cg.renderingThirdPerson || cg_trueguns.integer && !cg.zoomMode))
	{
		playerState_t* ps = &cg.predictedPlayerState;

		if (ps->weaponstate == WEAPON_CHARGING_ALT && ps->weapon == WP_BRYAR_PISTOL
			|| ps->weaponstate == WEAPON_CHARGING_ALT && ps->weapon == WP_SBD_BLASTER
			|| ps->weaponstate == WEAPON_CHARGING_ALT && ps->weapon == WP_BLASTER_PISTOL
			|| ps->weaponstate == WEAPON_CHARGING_ALT && ps->weapon == WP_REY
			|| ps->weaponstate == WEAPON_CHARGING_ALT && ps->weapon == WP_CLONEPISTOL
			|| ps->weapon == WP_BOWCASTER && ps->weaponstate == WEAPON_CHARGING
			|| ps->weapon == WP_DEMP2 && ps->weaponstate == WEAPON_CHARGING_ALT)
		{
			int shader = 0;
			float val = 0.0f, scale = 1.0f;
			vec3_t white = { 1.0f, 1.0f, 1.0f };

			if (ps->weapon == WP_BRYAR_PISTOL
				|| ps->weapon == WP_BLASTER_PISTOL
				|| ps->weapon == WP_SBD_BLASTER)
			{
				// Hardcoded max charge time of 1 second
				val = (cg.time - ps->weaponChargeTime) * 0.001f;
				shader = cgi_R_RegisterShader("gfx/effects/bryarFrontFlash");
			}
			if (ps->weapon == WP_REY)
			{
				// Hardcoded max charge time of 1 second
				val = (cg.time - ps->weaponChargeTime) * 0.001f;
				shader = cgi_R_RegisterShader("gfx/effects/bryarFrontFlash");
			}
			if (ps->weapon == WP_CLONEPISTOL)
			{
				// Hardcoded max charge time of 1 second
				val = (cg.time - ps->weaponChargeTime) * 0.001f;
				shader = cgi_R_RegisterShader("gfx/effects/cloneFrontFlash");
			}
			else if (ps->weapon == WP_BOWCASTER)
			{
				// Hardcoded max charge time of 1 second
				val = (cg.time - ps->weaponChargeTime) * 0.001f;
				shader = cgi_R_RegisterShader("gfx/effects/greenFrontFlash");
			}
			else if (ps->weapon == WP_DEMP2)
			{
				// Hardcoded max charge time of 1 second
				val = (cg.time - ps->weaponChargeTime) * 0.001f;
				shader = cgi_R_RegisterShader("gfx/misc/lightningFlash");
				scale = 1.75f;
			}

			if (val < 0.0f)
			{
				val = 0.0f;
			}
			else if (val > 1.0f)
			{
				val = 1.0f;
				CGCam_Shake(0.1f, 100);
			}
			else
			{
				CGCam_Shake(val * val * 0.3f, 100);
			}

			val += Q_flrand(0.0f, 1.0f) * 0.5f;

			FX_AddSprite(cent->gent->client->renderInfo.muzzlePoint, nullptr, nullptr, 3.0f * val * scale, 0.7f, 0.7f,
				white, white, Q_flrand(0.0f, 1.0f) * 360, 0.0f, 1.0f, shader, FX_USE_ALPHA);
		}
	}
}

//=====================================================================

/*
===============
CG_ResetPlayerEntity

A player just came into view or teleported, so reset all animation info

FIXME: We do not need to do this, we can remember the last anim and frame they were
on and coontinue from there.
===============
*/
void CG_ResetPlayerEntity(centity_t* cent)
{
	if (cent->gent && cent->gent->ghoul2.size())
	{
		if (cent->currentState.client_num < MAX_CLIENTS)
		{
			CG_ClearLerpFrame(&cgs.clientinfo[cent->currentState.client_num], &cent->pe.legs,
				cent->currentState.legsAnim);
			CG_ClearLerpFrame(&cgs.clientinfo[cent->currentState.client_num], &cent->pe.torso,
				cent->currentState.torsoAnim);
		}
		else if (cent->gent && cent->gent->client)
		{
			CG_ClearLerpFrame(&cent->gent->client->clientInfo, &cent->pe.legs, cent->currentState.legsAnim);
			CG_ClearLerpFrame(&cent->gent->client->clientInfo, &cent->pe.torso, cent->currentState.torsoAnim);
		}
	}
	//else????

	EvaluateTrajectory(&cent->currentState.pos, cg.time, cent->lerpOrigin);
	EvaluateTrajectory(&cent->currentState.apos, cg.time, cent->lerpAngles);

	memset(&cent->pe.legs, 0, sizeof cent->pe.legs);
	cent->pe.legs.yawAngle = cent->lerpAngles[YAW];
	cent->pe.legs.yawing = qfalse;
	cent->pe.legs.pitchAngle = 0;
	cent->pe.legs.pitching = qfalse;

	memset(&cent->pe.torso, 0, sizeof cent->pe.legs);
	cent->pe.torso.yawAngle = cent->lerpAngles[YAW];
	cent->pe.torso.yawing = qfalse;
	cent->pe.torso.pitchAngle = cent->lerpAngles[PITCH];
	cent->pe.torso.pitching = qfalse;
}