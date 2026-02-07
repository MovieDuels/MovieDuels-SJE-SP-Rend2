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

#include "../icarus/IcarusInterface.h"
#include "../cgame/cg_local.h"
#include "Q3_Interface.h"
#include "g_local.h"
#include "g_functions.h"
#include "anims.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "objectives.h"
#include "b_local.h"
#include <cstdio>

extern int WP_SaberInitBladeData(gentity_t* ent);
extern void G_CreateG2AttachedWeaponModel(gentity_t* ent, const char* ps_weapon_model, int bolt_num, int weapon_num);
extern qboolean CheatsOk(const gentity_t* ent);
extern void Boba_Precache();
extern qboolean HeIsJedi(const gentity_t* ent);
extern qboolean Char_Dual_Pistols(const gentity_t* self);
extern qboolean char_has_beskar_armor(const gentity_t* self);
extern qboolean Grievious_Classes(const gentity_t* self);
extern qboolean Calo_Nord(const gentity_t* self);

extern qboolean JET_Flying(const gentity_t* self);
extern cvar_t* com_kotor;
extern cvar_t* g_allowdualpistols;
extern cvar_t* g_char_model;
extern cvar_t* g_char_skin_head;
extern cvar_t* g_char_skin_torso;
extern cvar_t* g_char_skin_legs;
extern cvar_t* g_char_color_red;
extern cvar_t* g_char_color_green;
extern cvar_t* g_char_color_blue;
extern cvar_t* g_saber;
extern cvar_t* g_saber2;
extern cvar_t* g_saber_color;
extern cvar_t* g_saber2_color;
extern cvar_t* g_saberDarkSideSaberColor;
extern cvar_t* g_SerenityJediEngineMode;
void G_ChangePlayerModel(gentity_t* ent, const char* new_model);
extern cvar_t* com_outcast;
extern cvar_t* com_freeMelee;

// g_client.c -- client functions that don't happen every frame

float DEFAULT_MINS_0 = -16;
float DEFAULT_MINS_1 = -16;
float DEFAULT_MAXS_0 = 16;
float DEFAULT_MAXS_1 = 16;
float DEFAULT_PLAYER_RADIUS = sqrt(DEFAULT_MAXS_0 * DEFAULT_MAXS_0 + DEFAULT_MAXS_1 * DEFAULT_MAXS_1);
vec3_t playerMins = { DEFAULT_MINS_0, DEFAULT_MINS_1, DEFAULT_MINS_2 };
vec3_t playerMinsStep = { DEFAULT_MINS_0, DEFAULT_MINS_1, DEFAULT_MINS_2 + STEPSIZE };
vec3_t playerMaxs = { DEFAULT_MAXS_0, DEFAULT_MAXS_1, DEFAULT_MAXS_2 };
extern void Player_CheckBurn(const gentity_t* self);
extern void Player_CheckFreeze(gentity_t* self);
extern void TurnBarrierOff(gentity_t* ent);
extern void TurnBarrierON(gentity_t* ent);

void SP_misc_teleporter_dest(gentity_t* ent);

/*QUAK-ED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) KEEP_PREV DROPTOFLOOR x x x STUN_BATON NOWEAPON x
potential spawning position for deathmatch games.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_intermission(gentity_t* ent)
{
}

/*QUAKED info_player_intermission_red (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.

In a Siege game, the intermission will happen here if the Red (attacking) team wins
target - ent to look at
target2 - ents to use when this intermission point is chosen
*/
void SP_info_player_intermission_red(gentity_t* ent)
{
}

/*QUAKED info_player_intermission_blue (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.

In a Siege game, the intermission will happen here if the Blue (defending) team wins
target - ent to look at
target2 - ents to use when this intermission point is chosen
*/
void SP_info_player_intermission_blue(gentity_t* ent)
{
}

/*QUAKED info_player_duel (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for duelists in duel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel(gentity_t* ent)
{
	//
}

/*QUAKED info_player_duel1 (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for lone duelists in powerduel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel1(gentity_t* ent)
{
	//
}

/*QUAKED info_player_duel2 (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for paired duelists in powerduel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel2(gentity_t* ent)
{
	//
}

/*QUAKED info_player_siegeteam1 (1 0 0) (-16 -16 -24) (16 16 32)
siege start point - team1
name and behavior of team1 depends on what is defined in the
.siege file for this level

startoff - if non-0 spawn point will be disabled until used
idealclass - if specified, this spawn point will be considered
"ideal" for players of this class name. Corresponds to the name
entry in the .scl (siege class) file.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_siegeteam1(gentity_t* ent)
{
	//
}

/*QUAKED info_player_siegeteam2 (0 0 1) (-16 -16 -24) (16 16 32)
siege start point - team2
name and behavior of team2 depends on what is defined in the
.siege file for this level

startoff - if non-0 spawn point will be disabled until used
idealclass - if specified, this spawn point will be considered
"ideal" for players of this class name. Corresponds to the name
entry in the .scl (siege class) file.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_siegeteam2(gentity_t* ent)
{
	//
}

void SP_info_player_deathmatch(gentity_t* ent)
{
	SP_misc_teleporter_dest(ent);

	if (ent->spawnflags & 32) // STUN_BATON
	{
		RegisterItem(FindItemForWeapon(WP_STUN_BATON));
	}
	else
	{
		RegisterItem(FindItemForWeapon(WP_SABER));
		//these are given in ClientSpawn(), but we register them now before cgame starts
		saberInfo_t saber;
		WP_SaberParseParms(g_saber->string, &saber); //get saber sounds and models cached before client begins
		if (saber.model) G_ModelIndex(saber.model);
		if (saber.brokenSaber1) G_ModelIndex(saber.brokenSaber1);
		if (saber.brokenSaber2) G_ModelIndex(saber.brokenSaber2);
		if (saber.skin) G_SkinIndex(saber.skin);
		WP_SaberFreeStrings(saber);
	}
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32) KEEP_PREV DROPTOFLOOR x x x STUN_BATON NOWEAPON x
KEEP_PREV - keep previous health + armor
DROPTOFLOOR - Player will start on the first solid structure under it
STUN_BATON - Gives player the stun baton and bryar pistol, but not the saber, plus any weapons they may have carried over from previous levels.

Targets will be fired when someone spawns in on them.
equivalant to info_player_deathmatch
*/
void SP_info_player_start(gentity_t* ent)
{
	ent->classname = "info_player_deathmatch";

	SP_info_player_deathmatch(ent);
}

/*QUAKED info_player_start_blue (1 0 0) (-16 -16 -24) (16 16 32) INITIAL
For Blue Team DM starts

Targets will be fired when someone spawns in on them.
equivalent to info_player_deathmatch

INITIAL - The first time a player enters the game, they will be at an 'initial' spot.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_start_blue(gentity_t* ent)
{
	SP_info_player_deathmatch(ent);
}

/*QUAKED info_player_start_red (1 0 0) (-16 -16 -24) (16 16 32) INITIAL
For Red Team DM starts

Targets will be fired when someone spawns in on them.
equivalent to info_player_deathmatch

INITIAL - The first time a player enters the game, they will be at an 'initial' spot.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_start_red(gentity_t* ent)
{
	SP_info_player_deathmatch(ent);
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag(const gentity_t* spot, const team_t checkteam)
{
	gentity_t* touch[MAX_GENTITIES];
	vec3_t mins, maxs;

	// If we have a mins, use that instead of the hardcoded bounding box
	if (!VectorCompare(spot->mins, vec3_origin) && VectorLength(spot->mins))
		VectorAdd(spot->s.origin, spot->mins, mins);
	else
		VectorAdd(spot->s.origin, playerMins, mins);

	// If we have a maxs, use that instead of the hardcoded bounding box
	if (!VectorCompare(spot->maxs, vec3_origin) && VectorLength(spot->maxs))
		VectorAdd(spot->s.origin, spot->maxs, maxs);
	else
		VectorAdd(spot->s.origin, playerMaxs, maxs);

	const int num = gi.EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

	for (int i = 0; i < num; i++)
	{
		const gentity_t* hit = touch[i];
		if (hit != spot && hit->client && hit->client->ps.stats[STAT_HEALTH] > 0)
		{
			if (hit->contents & CONTENTS_BODY)
			{
				if (checkteam == TEAM_FREE || hit->client->playerTeam == checkteam)
				{
					//checking against teammates only...?
					return qtrue;
				}
			}
		}
	}

	return qfalse;
}

qboolean SpotWouldTelefrag2(const gentity_t* mover, vec3_t dest)
{
	static gentity_t* touch[MAX_GENTITIES]; // moved off stack
	vec3_t mins, maxs;

	VectorAdd(dest, mover->mins, mins);
	VectorAdd(dest, mover->maxs, maxs);

	const int num = gi.EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

	for (int i = 0; i < num; i++)
	{
		const gentity_t* hit = touch[i];
		if (hit == mover)
			continue;

		if (hit->contents & mover->contents)
			return qtrue;
	}

	return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
constexpr auto MAX_SPAWN_POINTS = 128;

static gentity_t* SelectNearestDeathmatchSpawnPoint(vec3_t from)
{
	float nearest_dist = static_cast<float>(WORLD_SIZE) * static_cast<float>(WORLD_SIZE);
	gentity_t* nearest_spot = nullptr;
	gentity_t* spot = nullptr;

	while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != nullptr)
	{
		if (spot->targetname != nullptr)
		{
			//this search routine should never find a spot that is targetted
			continue;
		}
		const float dist = DistanceSquared(spot->s.origin, from);
		if (dist < nearest_dist)
		{
			nearest_dist = dist;
			nearest_spot = spot;
		}
	}

	return nearest_spot;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
static gentity_t* SelectRandomDeathmatchSpawnPoint()
{
	gentity_t* spots[MAX_SPAWN_POINTS]{};

	int count = 0;
	gentity_t* spot = nullptr;

	while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != nullptr && count < MAX_SPAWN_POINTS)
	{
		if (spot->targetname != nullptr)
		{
			//this search routine should never find a spot that is targetted
			continue;
		}
		if (SpotWouldTelefrag(spot, TEAM_FREE))
		{
			continue;
		}
		spots[count] = spot;
		count++;
	}

	if (!count)
	{
		// no spots that won't telefrag
		spot = G_Find(nullptr, FOFS(classname), "info_player_deathmatch");
		if (!spot)
		{
			return nullptr;
		}
		if (spot->targetname != nullptr)
		{
			//this search routine should never find a spot that is targetted
			return nullptr;
		}
		return spot;
	}

	const int selection = rand() % count;
	return spots[selection];
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t* SelectSpawnPoint(vec3_t avoid_point, vec3_t origin, vec3_t angles)
{
	gentity_t* spot;
	gentity_t* nearest_spot;

	if (level.spawntarget[0])
	{
		//we have a spawnpoint specified, try to find it
		if ((nearest_spot = spot = G_Find(nullptr, FOFS(targetname), level.spawntarget)) == nullptr)
		{
			//you HAVE to be able to find the desired spot
			G_Error("Couldn't find spawntarget %s\n", level.spawntarget);
		}
	}
	else
	{
		//not looking for a special startspot
		nearest_spot = SelectNearestDeathmatchSpawnPoint(avoid_point);

		spot = SelectRandomDeathmatchSpawnPoint();
		if (spot == nearest_spot)
		{
			// roll again if it would be real close to point of death
			spot = SelectRandomDeathmatchSpawnPoint();
		}
	}

	// find a single player start spot
	if (!spot)
	{
		G_Error("Couldn't find a spawn point\n");
	}

	VectorCopy(spot->s.origin, origin);
	if (spot->spawnflags & 2)
	{
		trace_t tr;

		origin[2] = MIN_WORLD_COORD;
		gi.trace(&tr, spot->s.origin, playerMins, playerMaxs, origin, ENTITYNUM_NONE, MASK_PLAYERSOLID,
			static_cast<EG2_Collision>(0), 0);
		if (tr.fraction < 1.0 && !tr.allsolid && !tr.startsolid)
		{
			//found a floor
			VectorCopy(tr.endpos, origin);
		}
		else
		{
			//In solid or too far
			VectorCopy(spot->s.origin, origin);
		}
	}

	origin[2] += 9;
	VectorCopy(spot->s.angles, angles);

	return spot;
}

//======================================================================

/*
==================
SetClientViewAngle

==================
*/
void SetClientViewAngle(gentity_t* ent, vec3_t angle)
{
	// set the delta angle
	for (int i = 0; i < 3; i++)
	{
		ent->client->ps.delta_angles[i] = ANGLE2SHORT(angle[i]) - ent->client->pers.cmd_angles[i] & 0xffff;
	}
	VectorCopy(angle, ent->s.angles);
	VectorCopy(ent->s.angles, ent->client->ps.viewangles);
}

/*
================
respawn
================
*/
void respawn(gentity_t* ent)
{
	gi.SendConsoleCommand("load *respawn\n"); // special case
}

/*
===========
ClientCheckName
============
*/
static void ClientCleanName(const char* in, char* out, const int out_size)
{
	int outpos = 0, colorless_len = 0, spaces = 0, ats = 0;

	// discard leading spaces
	// ReSharper disable once CppPossiblyErroneousEmptyStatements
	for (; *in == ' '; in++);

	// discard leading asterisk's (fail raven for using * as a skipnotify)
	// apparently .* causes the issue too so... derp
	//for(; *in == '*'; in++);

	for (; *in && outpos < out_size - 1; in++)
	{
		out[outpos] = *in;

		if (*in == ' ')
		{
			// don't allow too many consecutive spaces
			if (spaces > 2)
				continue;

			spaces++;
		}
		else if (*in == '@')
		{
			// don't allow too many consecutive at signs
			if (ats > 2)
				continue;

			ats++;
		}
		else if (outpos > 0 && out[outpos - 1] == Q_COLOR_ESCAPE)
		{
			if (Q_IsColorStringExt(&out[outpos - 1]))
			{
				colorless_len--;

#if 0
				if (ColorIndex(*in) == 0)
				{// Disallow color black in names to prevent players from getting advantage playing in front of black backgrounds
					outpos--;
					continue;
				}
#endif
			}
			else
			{
				spaces = ats = 0;
				colorless_len++;
			}
		}
		else
		{
			spaces = ats = 0;
			colorless_len++;
		}

		outpos++;
	}

	out[outpos] = '\0';

	// don't allow empty names
	if (*out == '\0' || colorless_len == 0)
		Q_strncpyz(out, "Padawan", out_size);
}

/*
===========
client_userinfo_changed

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call gi.SetUserinfo
if desired.
============
*/
void client_userinfo_changed(const int client_num)
{
	const gentity_t* ent = g_entities + client_num;
	gclient_t* client = ent->client;
	char userinfo[MAX_INFO_STRING] = { 0 }, buf[MAX_INFO_STRING] = { 0 },
		sound[MAX_STRING_CHARS] = { 0 }, oldname[34] = { 0 };

	gi.GetUserinfo(client_num, userinfo, sizeof userinfo);

	// set name
	Q_strncpyz(oldname, client->pers.netname, sizeof oldname);
	const char* s = Info_ValueForKey(userinfo, "name");
	ClientCleanName(s, client->pers.netname, sizeof client->pers.netname);

	// set max health
	constexpr int max_health = 100;

	client->pers.maxHealth = player->client->ps.stats[STAT_MAX_HEALTH];
	if (client->pers.maxHealth < 1 || client->pers.maxHealth > max_health)
		client->pers.maxHealth = 100;
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

	// sounds
	Q_strncpyz(sound, Info_ValueForKey(userinfo, "snd"), sizeof sound);

	// send over a subset of the userinfo keys so other clients can
	// print scoreboards, display models, and play custom sounds
	buf[0] = '\0';
	Q_strcat(buf, sizeof buf, va("n\\%s\\", client->pers.netname));
	Q_strcat(buf, sizeof buf, va("t\\%i\\", client->sess.sessionTeam));
	Q_strcat(buf, sizeof buf, "headModel\\\\");
	Q_strcat(buf, sizeof buf, "torsoModel\\\\");
	Q_strcat(buf, sizeof buf, "legsModel\\\\");
	Q_strcat(buf, sizeof buf, va("hc\\%i\\", client->pers.maxHealth));
	Q_strcat(buf, sizeof buf, va("snd\\%s\\", sound));

	gi.SetConfigstring(CS_PLAYERS + client_num, buf);
}

/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
char* ClientConnect(const int client_num, const qboolean first_time,
	const SavedGameJustLoaded_e e_saved_game_just_loaded)
{
	gentity_t* ent = &g_entities[client_num];
	char userinfo[MAX_INFO_STRING] = { 0 };

	gi.GetUserinfo(client_num, userinfo, sizeof userinfo);

	// they can connect
	ent->client = level.clients + client_num;
	gclient_t* client = ent->client;

	if (e_saved_game_just_loaded != eFULL)
	{
		const clientSession_t saved_sess = client->sess;
		memset(client, 0, sizeof * client);
		client->sess = saved_sess;

		if (first_time)
		{
			//not loading full, and directconnect
			client->playerTeam = TEAM_PLAYER;
			//set these now because after an auto_load kyle can see your team for a bit before you really join.
			client->enemyTeam = TEAM_ENEMY;
			client->friendlyfaction = FACTION_LIGHT;
			client->enemyfaction = FACTION_DARK;
		}
	}

	client->pers.connected = CON_CONNECTING;

	if (e_saved_game_just_loaded == eFULL)
	{
		// get and distribute relevent paramters
		client_userinfo_changed(client_num);
	}
	else
	{
		// read or initialize the session data
		if (first_time)
		{
			G_InitSessionData(client, userinfo);
		}
		G_ReadSessionData(client);

		// get and distribute relevent paramters
		client_userinfo_changed(client_num);

		// don't do the "xxx connected" messages if they were caried over from previous level
		if (first_time)
		{
			gi.SendServerCommand(-1, "print \"%s connected\n\"", client->pers.netname);
		}
	}

	return nullptr;
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
void ClientBegin(const int client_num, const usercmd_t* cmd, const SavedGameJustLoaded_e e_saved_game_just_loaded)
{
	gentity_t* ent = g_entities + client_num;
	gclient_t* client = level.clients + client_num;

	if (e_saved_game_just_loaded == eFULL)
	{
		client->pers.connected = CON_CONNECTED;
		ent->client = client;
		ClientSpawn(ent, e_saved_game_just_loaded);
	}
	else
	{
		if (ent->linked)
		{
			gi.unlinkentity(ent);
		}
		G_InitGentity(ent, qfalse);
		ent->e_TouchFunc = touchF_NULL;
		ent->e_PainFunc = painF_PlayerPain; //painF_NULL;
		ent->client = client;

		client->pers.connected = CON_CONNECTED;
		client->pers.teamState.state = TEAM_BEGIN;
		VectorCopyM(cmd->angles, client->pers.cmd_angles);

		memset(&client->ps, 0, sizeof client->ps);
		if (gi.Cvar_VariableIntegerValue("g_clearstats"))
		{
			memset(&client->sess.missionStats, 0, sizeof client->sess.missionStats);
			client->sess.missionStats.totalSecrets = gi.Cvar_VariableIntegerValue("newTotalSecrets");
		}

		// locate ent at a spawn point
		if (ClientSpawn(ent, e_saved_game_just_loaded))
		{
			// send teleport event
		}
		client->ps.inventory[INV_GOODIE_KEY] = 0;
		client->ps.inventory[INV_SECURITY_KEY] = 0;
	}
}

/*
============
Player_CacheFromPrevLevel
  Description	: just need to grab the weapon items we're going to have when we spawn so they'll be cached
  Return type	: void
  Argument		: void
============
*/
extern gitem_t* FindItemForInventory(int inv);

void Player_CacheFromPrevLevel()
{
	char s[MAX_STRING_CHARS];
	int i;

	gi.Cvar_VariableStringBuffer(sCVARNAME_PLAYERSAVE, s, sizeof s);

	if (s[0]) // actually this would be safe anyway because of the way sscanf() works, but this is clearer
	{
		int iDummy, ibits;

		sscanf(s, "%i %i %i",
			&iDummy, //client->ps.stats[STAT_HEALTH],
			&iDummy, //client->ps.stats[STAT_ARMOR],
			&ibits //client->ps.stats[STAT_ITEMS]
		);

		for (i = 0; i < 16; i++)
		{
			if (ibits & 1 << i)
			{
				RegisterItem(FindItemForInventory(i));
			}
		}
	}

	gi.Cvar_VariableStringBuffer("playerweaps", s, sizeof s);

	i = 0;

	if (s[0])
	{
		const char* var = strtok(s, " ");
		while (var != nullptr)
		{
			if (atoi(var) > 0)
			{
				if (i == WP_NONE) //don't register!
				{
					i++;
				}
				else
				{
					RegisterItem(FindItemForWeapon(static_cast<weapon_t>(i++)));
				}
			}
			else
			{
				i++;
			}
			var = strtok(nullptr, " ");
		}
		assert(i == WP_NUM_WEAPONS);
	}
}

/*
============
Player_RestoreFromPrevLevel
  Description	: retrieve maptransition data recorded by server when exiting previous level (to carry over weapons/ammo/health/etc)
  Return type	: void
  Argument		: gentity_t *ent
============
*/
static void Player_RestoreFromPrevLevel(gentity_t* ent)
{
	gclient_t* client = ent->client;

	assert(client);
	if (client) // though I can't see it not being true...
	{
		char s[MAX_STRING_CHARS];

		gi.Cvar_VariableStringBuffer(sCVARNAME_PLAYERSAVE, s, sizeof s);

		if (strlen(s)) // actually this would be safe anyway because of the way sscanf() works, but this is clearer
		{
			char saber1_name[MAX_QPATH];
			char saber0_name[MAX_QPATH];
			//				|general info				  |-force powers |-saber 1										   |-saber 2										  |-general saber
			int saber1_blade_active[8]{};
			int saber2_blade_active[8]{};
			unsigned int saber1_blade_color[8]{};
			unsigned int saber2_blade_color[8]{};

			sscanf(
				s,
				"%i %i %i %i %i %i %f %f %f %i %i %i %i %i %s %i %i %i %i %i %i %i %i %u %u %u %u %u %u %u %u %s %i %i %i %i %i %i %i %i %u %u %u %u %u %u %u %u %i %i %i %i",
				&client->ps.stats[STAT_HEALTH],
				&client->ps.stats[STAT_ARMOR],
				&client->ps.stats[STAT_ITEMS],
				&client->ps.weapon,
				&client->ps.weaponstate,
				&client->ps.batteryCharge,
				&client->ps.viewangles[0],
				&client->ps.viewangles[1],
				&client->ps.viewangles[2],
				//force power data
				&client->ps.forcePowersKnown,
				&client->ps.forcePower,
				&client->ps.forcePowerMax,
				&client->ps.forcePowerRegenRate,
				&client->ps.forcePowerRegenAmount,
				//saber 1 data
				saber0_name,
				&saber1_blade_active[0],
				&saber1_blade_active[1],
				&saber1_blade_active[2],
				&saber1_blade_active[3],
				&saber1_blade_active[4],
				&saber1_blade_active[5],
				&saber1_blade_active[6],
				&saber1_blade_active[7],
				&saber1_blade_color[0],
				&saber1_blade_color[1],
				&saber1_blade_color[2],
				&saber1_blade_color[3],
				&saber1_blade_color[4],
				&saber1_blade_color[5],
				&saber1_blade_color[6],
				&saber1_blade_color[7],
				//saber 2 data
				saber1_name,
				&saber2_blade_active[0],
				&saber2_blade_active[1],
				&saber2_blade_active[2],
				&saber2_blade_active[3],
				&saber2_blade_active[4],
				&saber2_blade_active[5],
				&saber2_blade_active[6],
				&saber2_blade_active[7],
				&saber2_blade_color[0],
				&saber2_blade_color[1],
				&saber2_blade_color[2],
				&saber2_blade_color[3],
				&saber2_blade_color[4],
				&saber2_blade_color[5],
				&saber2_blade_color[6],
				&saber2_blade_color[7],
				//general saber data
				&client->ps.saberStylesKnown,
				&client->ps.saber_anim_level,
				&client->ps.saberLockEnemy,
				&client->ps.saberLockTime
			);
			for (int j = 0; j < 8; j++)
			{
				client->ps.saber[0].blade[j].active = saber1_blade_active[j] ? qtrue : qfalse;
				client->ps.saber[0].blade[j].color = static_cast<saber_colors_t>(saber1_blade_color[j]);
				client->ps.saber[1].blade[j].active = saber2_blade_active[j] ? qtrue : qfalse;
				client->ps.saber[1].blade[j].color = static_cast<saber_colors_t>(saber2_blade_color[j]);
			}

			ent->health = client->ps.stats[STAT_HEALTH];

			if (ent->client->ps.saber[0].name && gi.bIsFromZone(ent->client->ps.saber[0].name, TAG_G_ALLOC))
			{
				gi.Free(ent->client->ps.saber[0].name);
			}
			ent->client->ps.saber[0].name = nullptr;

			if (ent->client->ps.saber[1].name && gi.bIsFromZone(ent->client->ps.saber[1].name, TAG_G_ALLOC))
			{
				gi.Free(ent->client->ps.saber[1].name);
			}
			ent->client->ps.saber[1].name = nullptr;
			//NOTE: if sscanf can get a "(null)" out of strings that had NULL string pointers plugged into the original string
			if (saber0_name[0] && Q_stricmp("(null)", saber0_name) != 0)
			{
				ent->client->ps.saber[0].name = G_NewString(saber0_name);
			}
			if (saber1_name[0] && Q_stricmp("(null)", saber1_name) != 0)
			{
				//have a second saber
				ent->client->ps.saber[1].name = G_NewString(saber1_name);
				ent->client->ps.dualSabers = qtrue;
			}
			else
			{
				//have only 1 saber
				ent->client->ps.dualSabers = qfalse;
			}

			//weapons
			gi.Cvar_VariableStringBuffer("playerweaps", s, sizeof s);
			int i = 0;
			const char* var = strtok(s, " ");
			while (var != nullptr)
			{
				/* While there are tokens in "s" */
				client->ps.weapons[i++] = atoi(var);
				/* Get next token: */
				var = strtok(nullptr, " ");
			}
			assert(i == WP_NUM_WEAPONS);

			//ammo
			gi.Cvar_VariableStringBuffer("playerammo", s, sizeof s);
			i = 0;
			var = strtok(s, " ");
			while (var != nullptr)
			{
				/* While there are tokens in "s" */
				client->ps.ammo[i++] = atoi(var);
				/* Get next token: */
				var = strtok(nullptr, " ");
			}
			assert(i == AMMO_MAX);

			//inventory
			gi.Cvar_VariableStringBuffer("playerinv", s, sizeof s);
			i = 0;
			var = strtok(s, " ");
			while (var != nullptr)
			{
				/* While there are tokens in "s" */
				client->ps.inventory[i++] = atoi(var);
				/* Get next token: */
				var = strtok(nullptr, " ");
			}
			assert(i == INV_MAX);

			// the new JK2 stuff - force powers, etc...
			//
			gi.Cvar_VariableStringBuffer("playerfplvl", s, sizeof s);
			i = 0;
			var = strtok(s, " ");
			while (var != nullptr)
			{
				/* While there are tokens in "s" */
				client->ps.forcePowerLevel[i++] = atoi(var);
				/* Get next token: */
				var = strtok(nullptr, " ");
			}
			assert(i == NUM_FORCE_POWERS);

			client->ps.forceGripEntityNum = client->ps.forceDrainEntityNum = ENTITYNUM_NONE;
		}
	}
}

static void G_SetSkin(gentity_t* ent)
{
	char skin_name[MAX_QPATH];
	//ok, lets register the skin name, and then pass that name to the config strings so the client can get it too.
	if (Q_stricmp("hoth2", level.mapname) == 0 //hack, is this the only map?
		||
		Q_stricmp("hoth3", level.mapname) == 0 // no! ;-)
		)
	{
		Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/|%s|%s|%s", g_char_model->string,
			g_char_skin_head->string, "torso_g1", "lower_e1");
	}
	else if (Q_stricmp(g_char_skin_head->string, "model_default") == 0 &&
		Q_stricmp(g_char_skin_torso->string, "model_default") == 0 && Q_stricmp(
			g_char_skin_legs->string, "model_default") == 0)
	{
		Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/model_default.skin", g_char_model->string);
	}
	else
	{
		Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/|%s|%s|%s", g_char_model->string,
			g_char_skin_head->string, g_char_skin_torso->string, g_char_skin_legs->string);
	}

	// lets see if it's out there
	const int skin = gi.RE_RegisterSkin(skin_name);
	if (skin)
	{
		//what if this returns 0 because *one* part of a multi-skin didn't load?
		// put it in the config strings
		// and set the ghoul2 model to use it
		gi.G2API_SetSkin(&ent->ghoul2[ent->playerModel], G_SkinIndex(skin_name), skin);
	}

	//color tinting
	if (g_char_color_red->integer
		|| g_char_color_green->integer
		|| g_char_color_blue->integer)
	{
		ent->client->renderInfo.customRGBA[0] = g_char_color_red->integer;
		ent->client->renderInfo.customRGBA[1] = g_char_color_green->integer;
		ent->client->renderInfo.customRGBA[2] = g_char_color_blue->integer;
		ent->client->renderInfo.customRGBA[3] = 255;
	}
}

qboolean g_standard_humanoid(gentity_t* self)
{
	if (!self || !self->ghoul2.size())
	{
		return qfalse;
	}
	if (self->playerModel < 0 || self->playerModel >= self->ghoul2.size())
	{
		return qfalse;
	}
	const char* gla_name = gi.G2API_GetGLAName(&self->ghoul2[self->playerModel]);
	assert(gla_name);
	if (gla_name)
	{
		if (!Q_stricmpn("models/players/_humanoid", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/JK2anims/", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_ani", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_bdroid", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_ben", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_df2", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_cal", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_clo", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_dooku", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_galen", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_gon", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_grievous", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_jango", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_kotor", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_luke", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_mace", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_maul", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_md", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_obi", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_obi3", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_pal", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_ren", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_rey", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_sbd", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_deka", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_vader", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmpn("models/players/_humanoid_yoda", gla_name, 24))
		{
			//only _humanoid skeleton is expected to have these
			return qtrue;
		}
		//
		if (!Q_stricmp("models/players/protocol/protocol", gla_name))
		{
			//protocol droid duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/assassin_droid/model", gla_name))
		{
			//assassin_droid duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/saber_droid/model", gla_name))
		{
			//saber_droid duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/hazardtrooper/hazardtrooper", gla_name))
		{
			//hazardtrooper duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/rockettrooper/rockettrooper", gla_name))
		{
			//rockettrooper duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/wampa/wampa", gla_name))
		{
			//rockettrooper duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/galak_mech/galak_mech", gla_name))
		{
			//galak duplicates many of these
			return qtrue;
		}
		if (!Q_stricmp("models/players/droideka/droideka", gla_name))
		{
			//assassin_droid duplicates many of these
			return qtrue;
		}
	}
	return qfalse;
}

qboolean G_StandardHumanoid(const char* gla_name)
{
	if (gla_name)
	{
		if (!Q_stricmp("_humanoid_mp", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("JK2anims", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_ani", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_bdroid", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_ben", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_df2", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_cal", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_clo", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_dooku", gla_name))
		{
			// only _humanoid skeleton is expected eto have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_galen", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_gon", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_grievous", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_jango", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_kotor", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_luke", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_mace", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_maul", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_md", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_obi", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_obi3", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_pal", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_ren", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_rey", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_sbd", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_deka", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_vader", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
		if (!Q_stricmp("_humanoid_yoda", gla_name))
		{
			// only _humanoid skeleton is expected to have these
			return qtrue;
		}
	}
	return qfalse;
}

qboolean G_ClassHasBadBones(const int npc_class)
{
	switch (npc_class)
	{
	case CLASS_WAMPA:
	case CLASS_ROCKETTROOPER:
	case CLASS_SABER_DROID:
	case CLASS_HAZARD_TROOPER:
	case CLASS_ASSASSIN_DROID:
	case CLASS_DROIDEKA:
	case CLASS_RANCOR:
		return qtrue;
	default:;
	}
	return qfalse;
}

const char* AxesNames[] =
{
	"ORIGIN", //ORIGIN,
	"POSITIVE_X", //POSITIVE_X,
	"POSITIVE_Z", //POSITIVE_Z,
	"POSITIVE_Y", //POSITIVE_Y,
	"NEGATIVE_X", //NEGATIVE_X,
	"NEGATIVE_Z", //NEGATIVE_Z,
	"NEGATIVE_Y" //NEGATIVE_Y
};

Eorientations testAxes[3] = { POSITIVE_X, POSITIVE_Z, POSITIVE_Y };
int axes_0 = POSITIVE_X;
int axes_1 = POSITIVE_Z;
int axes_2 = POSITIVE_Y;

void G_NextTestAxes()
{
	static int which_axes = 0;
	do
	{
		which_axes++;
		if (which_axes > 216)
		{
			which_axes = 0;
			Com_Printf(S_COLOR_RED"WRAPPED\n");
			break;
		}
		int axes_count = 0;
		axes_0 = 0;
		axes_1 = 0;
		axes_2 = 0;
		for (axes_0 = 0; axes_0 < 6 && axes_count < which_axes; axes_0++)
		{
			axes_count++;
			for (axes_1 = 0; axes_1 < 6 && axes_count < which_axes; axes_1++)
			{
				axes_count++;
				for (axes_2 = 0; axes_2 < 6 && axes_count < which_axes; axes_2++)
				{
					axes_count++;
				}
			}
		}
		testAxes[0] = static_cast<Eorientations>(axes_0 % 6 + 1);
		testAxes[1] = static_cast<Eorientations>(axes_1 % 6 + 1);
		testAxes[2] = static_cast<Eorientations>(axes_2 % 6 + 1);
	} while (testAxes[1] == testAxes[0] || testAxes[1] - testAxes[0] == 3 || testAxes[0] - testAxes[1] == 3
		|| testAxes[2] == testAxes[0] || testAxes[2] - testAxes[0] == 3 || testAxes[0] - testAxes[2] == 3
		|| testAxes[2] == testAxes[1] || testAxes[2] - testAxes[1] == 3 || testAxes[1] - testAxes[2] == 3);

	Com_Printf("Up: %s\nRight: %s\nForward: %s\n", AxesNames[testAxes[0]], AxesNames[testAxes[1]],
		AxesNames[testAxes[2]]);
	if (testAxes[0] == POSITIVE_X
		&& testAxes[1] == POSITIVE_Z
		&& testAxes[2] == POSITIVE_Y)
	{
		Com_Printf(S_COLOR_RED"WRAPPED\n");
	}
}

void G_BoneOrientationsForClass(const int npc_class, const char* boneName, Eorientations* o_up, Eorientations* o_rt,
	Eorientations* o_fwd)
{
	//defaults
	*o_up = POSITIVE_X;
	*o_rt = NEGATIVE_Y;
	*o_fwd = NEGATIVE_Z;
	//switch off class
	switch (npc_class)
	{
	case CLASS_RANCOR:
		*o_up = NEGATIVE_X;
		*o_rt = POSITIVE_Y;
		*o_fwd = POSITIVE_Z;
		//*oUp = testAxes[0];
		//*oRt = testAxes[1];
		//*oFwd = testAxes[2];
		break;
	case CLASS_ROCKETTROOPER:
	case CLASS_HAZARD_TROOPER:
		//Root is:
		//*oUp = POSITIVE_Z;
		//*oRt = NEGATIVE_X;
		//*oFwd = NEGATIVE_Y;
		if (Q_stricmp("pelvis", boneName) == 0)
		{
			//child of root
			//in ModView:
			//*oUp = NEGATIVE_X;
			//*oRt = NEGATIVE_Z;
			//*oFwd = NEGATIVE_Y;
			//actual, when differences with root are accounted for:
			*o_up = POSITIVE_Z;
			*o_rt = NEGATIVE_X;
			*o_fwd = NEGATIVE_Y;
		}
		else
		{
			//all the rest are the same, children of root (not pelvis)
			//in ModView:
			//*oUp = POSITIVE_X;
			//*oRt = POSITIVE_Y;
			//*oFwd = POSITIVE_Z;
			//actual, when differences with root are accounted for:
			//*oUp = POSITIVE_Z;
			//*oRt = NEGATIVE_Y;
			//*oFwd = NEGATIVE_X;
			*o_up = NEGATIVE_X;
			*o_rt = POSITIVE_Y;
			*o_fwd = POSITIVE_Z;
		}
		break;
	case CLASS_SABER_DROID:
		if (Q_stricmp("pelvis", boneName) == 0
			|| Q_stricmp("thoracic", boneName) == 0)
		{
			*o_up = NEGATIVE_X;
			*o_rt = NEGATIVE_Z;
			*o_fwd = NEGATIVE_Y;
		}
		else
		{
			*o_up = NEGATIVE_X; //POSITIVE_X;
			*o_rt = POSITIVE_Y;
			*o_fwd = POSITIVE_Z;
		}
		break;
	case CLASS_WAMPA:
		if (Q_stricmp("pelvis", boneName) == 0)
		{
			*o_up = NEGATIVE_X;
			*o_rt = POSITIVE_Y;
			*o_fwd = NEGATIVE_Z;
		}
		else
		{
			//*oUp = POSITIVE_X;
			//*oRt = POSITIVE_Y;
			//*oFwd = POSITIVE_Z;
			//kinda worked
			*o_up = NEGATIVE_X;
			*o_rt = POSITIVE_Y;
			*o_fwd = POSITIVE_Z;
		}
		break;
	case CLASS_ASSASSIN_DROID:
		if (Q_stricmp("pelvis", boneName) == 0
			|| Q_stricmp("lower_lumbar", boneName) == 0
			|| Q_stricmp("upper_lumbar", boneName) == 0)
		{
			//only these 3 bones on them are wrong
			//*oUp = POSITIVE_X;
			//*oRt = POSITIVE_Y;
			//*oFwd = POSITIVE_Z;
			*o_up = NEGATIVE_X;
			*o_rt = POSITIVE_Y;
			*o_fwd = POSITIVE_Z;
		}
		break;
	case CLASS_DROIDEKA:
		if (Q_stricmp("pelvis", boneName) == 0
			|| Q_stricmp("lower_lumbar", boneName) == 0
			|| Q_stricmp("upper_lumbar", boneName) == 0)
		{
			//only these 3 bones on them are wrong
			//*oUp = POSITIVE_X;
			//*oRt = POSITIVE_Y;
			//*oFwd = POSITIVE_Z;
			*o_up = NEGATIVE_X;
			*o_rt = POSITIVE_Y;
			*o_fwd = POSITIVE_Z;
		}
		break;
	default:;
	}
}

extern void G_LoadAnimFileSet(gentity_t* ent, const char* p_model_name);

qboolean g_set_g2_player_model_info(gentity_t* ent, const char* model_name, const char* surf_off, const char* surf_on)
{
	if (ent->playerModel != -1)
	{
		// we found the model ok
		constexpr vec3_t angles = { 0, 0, 0 };
		const char* token;
		const char* p;

		//Now turn on/off any surfaces
		if (surf_off && surf_off[0])
		{
			p = surf_off;
			COM_BeginParseSession();
			while (true)
			{
				token = COM_ParseExt(&p, qtrue);
				if (!token[0])
				{
					//reached end of list
					break;
				}
				//turn off this surf
				gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], token, 0x00000002);
			}
			COM_EndParseSession();
		}
		if (surf_on && surf_on[0])
		{
			p = surf_on;
			COM_BeginParseSession();
			while (true)
			{
				token = COM_ParseExt(&p, qtrue);
				if (!token[0])
				{
					//reached end of list
					break;
				}
				//turn on this surf
				gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], token, 0);
			}
			COM_EndParseSession();
		}
		if (ent->client && ent->client->NPC_class == CLASS_IMPERIAL && ent->message)
		{
			//carrying a key, turn on the key sleeve surface (assuming we have one)
			gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "l_arm_key", 0);
		}

		G_LoadAnimFileSet(ent, model_name);

		ent->headBolt = ent->cervicalBolt = ent->torsoBolt = ent->gutBolt = ent->chestBolt =
			ent->crotchBolt = ent->elbowLBolt = ent->elbowRBolt = ent->handLBolt =
			ent->handRBolt = ent->kneeLBolt = ent->kneeRBolt = ent->footLBolt =
			ent->footRBolt = -1;
		// now turn on the bolt in the hand - this one would be best always turned on.
		if (g_standard_humanoid(ent))
		{
			//only _humanoid skeleton is expected to have these
			ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head_eyes");
			ent->cervicalBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "cervical");
			if (!Q_stricmp("protocol", model_name))
			{
				//*sigh*, no thoracic bone
				ent->gutBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "upper_lumbar");
				ent->chestBolt = ent->gutBolt;
			}
			else
			{
				ent->chestBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "thoracic");
				ent->gutBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "upper_lumbar");
			}
			ent->torsoBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "lower_lumbar");
			ent->crotchBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "pelvis");
			ent->elbowLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_arm_elbow");
			ent->elbowRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_arm_elbow");
			ent->handLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_hand");
			ent->handRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_hand");
			ent->kneeLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*hips_l_knee");
			ent->kneeRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*hips_r_knee");
			ent->footLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_leg_foot");
			ent->footRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_leg_foot");

			if (ent->client->NPC_class == CLASS_BOBAFETT || ent->client->NPC_class == CLASS_MANDALORIAN
				|| ent->client->NPC_class == CLASS_JANGO || ent->client->NPC_class == CLASS_JANGODUAL || ent->client->
				NPC_class == CLASS_ROCKETTROOPER)
			{
				//get jet bolts
				ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*jet1");
				ent->genericBolt2 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*jet2");
			}
			if (ent->client->NPC_class == CLASS_BOBAFETT || ent->client->NPC_class == CLASS_MANDALORIAN || ent->client->
				NPC_class == CLASS_JANGO || ent->client->NPC_class == CLASS_JANGODUAL)
			{
				//get the flamethrower bolt
				ent->genericBolt3 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flamethrower");
				if (ent->genericBolt3 == -1)
					ent->genericBolt3 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_hand");
			}
		}
		else
		{
			if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
			{
				//do vehicles tags
				// Setup the driver tag (where the driver is mounted to).
				ent->crotchBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*driver");

				// Setup the droid unit (or other misc tag we're using this for).
				ent->m_pVehicle->m_iDroidUnitTag = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*droidunit");

				char str_temp[128];

				// Setup the Exhausts.
				for (int i = 0; i < MAX_VEHICLE_EXHAUSTS; i++)
				{
					Com_sprintf(str_temp, 128, "*exhaust%d", i + 1);
					ent->m_pVehicle->m_iExhaustTag[i] = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], str_temp);
				}

				// Setup the Muzzles.
				for (int i = 0; i < MAX_VEHICLE_MUZZLES; i++)
				{
					Com_sprintf(str_temp, 128, "*muzzle%d", i + 1);
					ent->m_pVehicle->m_iMuzzleTag[i] = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], str_temp);
					if (ent->m_pVehicle->m_iMuzzleTag[i] == -1)
					{
						//ergh, try *flash?
						Com_sprintf(str_temp, 128, "*flash%d", i + 1);
						ent->m_pVehicle->m_iMuzzleTag[i] = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], str_temp);
					}
				}
			}
			else if (ent->client && ent->client->NPC_class == CLASS_HOWLER)
			{
				ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "Tongue01"); // tongue base
				ent->genericBolt2 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "Tongue08"); // tongue tip
			}
			else if (!Q_stricmp("gonk", model_name) || !Q_stricmp("seeker", model_name) || !Q_stricmp(
				"remote", model_name)
				|| !Q_stricmpn("r2d2", model_name, 4) || !Q_stricmpn("r5d2", model_name, 4))
			{
				//TEMP HACK: not a non-humanoid droid
				ent->headBolt = -1;
			}
			else if (!Q_stricmp("interrogator", model_name))
			{
				ent->headBolt = -1;
			}
			else if (!Q_stricmpn("probe", model_name, 5))
			{
				ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "cranium"); // head pivot point
				ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash"); // Gun 1
			}
			else if (!Q_stricmp("sentry", model_name))
			{
				ent->headBolt = -1;
				ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash1"); // Gun 1
				ent->genericBolt2 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash2"); // Gun 2
				ent->genericBolt3 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash03"); // Gun 3
			}
			else if (!Q_stricmp("mark1", model_name))
			{
				ent->headBolt = -1;
				ent->handRBolt = ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash1");
				// Blaster Gun 1
				ent->genericBolt2 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash2"); // Blaster Gun 2
				ent->genericBolt3 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash3"); // Blaster Gun 3
				ent->genericBolt4 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash4"); // Blaster Gun 4
				ent->genericBolt5 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash5"); // Missile Gun 1
			}
			else if (!Q_stricmp("mark2", model_name))
			{
				ent->headBolt = -1;
				ent->handRBolt = ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash");
				// Blaster Gun 1
			}
			else if (!Q_stricmp("atst", model_name)) //&& (ent->client->playerTeam != TEAM_PLAYER))
			{
				ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head");

				ent->handLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash1"); // Front guns
				ent->handRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash2");

				ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash3"); // Left side gun
				ent->genericBolt2 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flash4");
				// Right side missle launcher

				ent->footLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_foot");
				ent->footRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_foot");
			}
			else if (!Q_stricmp("minemonster", model_name))
			{
				ent->handRBolt = ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head_f1");
			}
			else if (!Q_stricmp("galak_mech", model_name))
			{
				ent->genericBolt1 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*antenna_effect");
				ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head_eyes");
				ent->torsoBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "torso");
				ent->crotchBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "hips");
				ent->handRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flasha");
				ent->genericBolt3 = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flashb");
				ent->handLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*flashc");
			}
			else if (!Q_stricmp("rancor", model_name)
				|| !Q_stricmp("mutant_rancor", model_name))
			{
				ent->handLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_hand");
				ent->handRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_hand");
				ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head_eyes");
				ent->gutBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head_mouth");
			}
			else if (!Q_stricmp("sand_creature", model_name))
			{
				ent->gutBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*mouth");
				ent->crotchBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*ground");
			}
			else if (!Q_stricmp("wampa", model_name))
			{
				ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*head_eyes");
				ent->cervicalBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "neck_bone");
				ent->chestBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "upper_spine");
				ent->gutBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "mid_spine");
				ent->torsoBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "lower_spine");
				ent->crotchBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "rear_bone");
				ent->elbowLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_arm_elbow");
				ent->elbowRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_arm_elbow");
				ent->handLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_hand");
				ent->handRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_hand");
				ent->kneeLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*hips_l_knee");
				ent->kneeRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*hips_r_knee");
				ent->footLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*l_leg_foot");
				ent->footRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*r_leg_foot");
			}
			else
			{
				//TEMP HACK: not a non-humanoid droid
				ent->handRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*weapon"); //should be r_hand
				if (Q_stricmp("atst", model_name))
				{
					//not an ATST
					ent->headBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*headg");
					ent->cervicalBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "cervical");
					ent->torsoBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "lower_lumbar");
					ent->gutBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "upper_lumbar");
					ent->chestBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "thoracic");
					ent->crotchBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "pelvis");
					ent->elbowLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*bicep_lg");
					ent->elbowRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*bicep_rg");
					ent->handLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*hand_l");
					ent->kneeLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*thigh_lg");
					ent->kneeRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*thigh_rg");
					ent->footLBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*foot_lg");
					ent->footRBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "*foot_rg");
				}
			}
		}

		ent->faceBone = BONE_INDEX_INVALID;
		ent->craniumBone = BONE_INDEX_INVALID;
		ent->cervicalBone = BONE_INDEX_INVALID;
		ent->thoracicBone = BONE_INDEX_INVALID;
		ent->upperLumbarBone = BONE_INDEX_INVALID;
		ent->lowerLumbarBone = BONE_INDEX_INVALID;
		ent->motionBone = BONE_INDEX_INVALID;
		ent->hipsBone = BONE_INDEX_INVALID;
		ent->rootBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "model_root", qtrue);
#ifndef FINAL_BUILD
		if (g_developer->integer && ent->rootBone == -1)
		{
			Com_Error(ERR_DROP, "ERROR: model %s has no model_root bone (and hence cannot animate)!!!\n", modelName);
		}
#endif
		ent->footLBone = BONE_INDEX_INVALID;
		ent->footRBone = BONE_INDEX_INVALID;
		ent->humerusRBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "rhumerus", qtrue);

		// now add overrides on specific joints so the client can set angle overrides on the legs, torso and head
		if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
		{
			//do vehicles tags
			//vehicleInfo_t *vehicle = ent->m_pVehicle->m_pVehicleInfo;
		}
		else if (ent->client->NPC_class == CLASS_HOWLER)
		{
		}
		else if (!Q_stricmp("gonk", model_name) || !Q_stricmp("seeker", model_name) || !Q_stricmp("remote", model_name))
		{
			//
		}
		else if (!Q_stricmp("sentry", model_name))
		{
		}
		else if (!Q_stricmpn("probe", model_name, 5))
		{
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "pelvis", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmp("interrogator", model_name))
		{
			ent->genericBone1 = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "left_arm", qtrue);
			if (ent->genericBone1 >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->genericBone1, angles,
					BONE_ANGLES_POSTMULT, NEGATIVE_Y, NEGATIVE_X, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->genericBone2 = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "right_arm", qtrue);
			if (ent->genericBone2 >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->genericBone2, angles,
					BONE_ANGLES_POSTMULT, NEGATIVE_Y, NEGATIVE_X, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->genericBone3 = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "claw", qtrue);
			if (ent->genericBone3 >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->genericBone3, angles,
					BONE_ANGLES_POSTMULT, NEGATIVE_Y, NEGATIVE_X, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmpn("r2d2", model_name, 4))
		{
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "body", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->genericBone1 = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "f_eye", qtrue);
			if (ent->genericBone1 >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->genericBone1, angles,
					BONE_ANGLES_POSTMULT, NEGATIVE_Y, NEGATIVE_X, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmpn("r5d2", model_name, 4))
		{
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "body", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmp("atst", model_name))
		{
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "thoracic", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->footLBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "l_tarsal", qtrue);
			if (ent->footLBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->footLBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, nullptr, 0, 0);
			}
			ent->footRBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "r_tarsal", qtrue);
			if (ent->footRBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->footRBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmp("mark1", model_name))
		{
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->upperLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->upperLumbarBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->upperLumbarBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmp("mark2", model_name))
		{
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "thoracic", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmp("minemonster", model_name))
		{
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "thoracic1", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		else if (ent->client->NPC_class == CLASS_RANCOR)
			/*!Q_stricmp( "rancor", modelName )	|| !Q_stricmp( "mutant_rancor", modelName ) )*/
		{
			Eorientations o_up, o_rt, o_fwd;
			//regular bones we need
			ent->lowerLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "lower_spine", qtrue);
			if (ent->lowerLumbarBone >= 0)
			{
				G_BoneOrientationsForClass(ent->client->NPC_class, "lower_lumbar", &o_up, &o_rt, &o_fwd);
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->lowerLumbarBone, angles,
					BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
			}
			ent->upperLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "mid_spine", qtrue);
			if (ent->upperLumbarBone >= 0)
			{
				G_BoneOrientationsForClass(ent->client->NPC_class, "upper_lumbar", &o_up, &o_rt, &o_fwd);
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->upperLumbarBone, angles,
					BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "upper_spine", qtrue);
			if (ent->thoracicBone >= 0)
			{
				G_BoneOrientationsForClass(ent->client->NPC_class, "thoracic", &o_up, &o_rt, &o_fwd);
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
			}
		}
		else if (!Q_stricmp("sand_creature", model_name))
		{
		}
		else if (!Q_stricmp("wampa", model_name))
		{
		}
		else if (!Q_stricmp("rockettrooper", model_name)
			|| !Q_stricmp("hazardtrooper", model_name)
			|| !Q_stricmp("saber_droid", model_name)
			|| !Q_stricmp("assassin_droid", model_name)
			|| !Q_stricmp("md_dro_am", model_name)
			|| !Q_stricmp("md_dro_sn", model_name))
		{
			Eorientations o_up, o_rt, o_fwd;

			if (Q_stricmp("saber_droid", model_name))
			{
				//saber droid doesn't use these lower bones
				//regular bones we need
				ent->upperLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "upper_lumbar", qtrue);
				if (ent->upperLumbarBone >= 0)
				{
					G_BoneOrientationsForClass(ent->client->NPC_class, "upper_lumbar", &o_up, &o_rt, &o_fwd);
					gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->upperLumbarBone, angles,
						BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
				}
				ent->lowerLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "lower_lumbar", qtrue);
				if (ent->lowerLumbarBone >= 0)
				{
					G_BoneOrientationsForClass(ent->client->NPC_class, "lower_lumbar", &o_up, &o_rt, &o_fwd);
					gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->lowerLumbarBone, angles,
						BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
				}
			}
			if (Q_stricmp("hazardtrooper", model_name))
			{
				//hazard trooper doesn't have these upper bones
				if (Q_stricmp("saber_droid", model_name))
				{
					//saber droid doesn't use thoracic bone
					ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "thoracic", qtrue);
					if (ent->thoracicBone >= 0)
					{
						G_BoneOrientationsForClass(ent->client->NPC_class, "thoracic", &o_up, &o_rt, &o_fwd);
						gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
							BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
					}
				}
				ent->cervicalBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cervical", qtrue);
				if (ent->cervicalBone >= 0)
				{
					G_BoneOrientationsForClass(ent->client->NPC_class, "cervical", &o_up, &o_rt, &o_fwd);
					gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->cervicalBone, angles,
						BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
				}
				ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
				if (ent->craniumBone >= 0)
				{
					G_BoneOrientationsForClass(ent->client->NPC_class, "cranium", &o_up, &o_rt, &o_fwd);
					gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
						BONE_ANGLES_POSTMULT, o_up, o_rt, o_fwd, nullptr, 0, 0);
				}
			}
		}
		else
		{
			//special case motion bone - to match up split anims
			ent->motionBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "Motion", qtrue);
			if (ent->motionBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->motionBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_X, NEGATIVE_Y, nullptr, 0, 0);
			}
			ent->motionBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], "Motion");
			//bone needed for turning anims
			ent->hipsBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "pelvis", qtrue);
			if (ent->hipsBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->hipsBone, angles, BONE_ANGLES_POSTMULT,
					POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			//regular bones we need
			ent->upperLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "upper_lumbar", qtrue);
			if (ent->upperLumbarBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->upperLumbarBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->lowerLumbarBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "lower_lumbar", qtrue);
			if (ent->lowerLumbarBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->lowerLumbarBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}

			ent->faceBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "face", qtrue);
			if (ent->faceBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->faceBone, angles, BONE_ANGLES_POSTMULT,
					POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->craniumBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cranium", qtrue);
			if (ent->craniumBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->craniumBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->cervicalBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "cervical", qtrue);
			if (ent->cervicalBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->cervicalBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
			ent->thoracicBone = gi.G2API_GetBoneIndex(&ent->ghoul2[ent->playerModel], "thoracic", qtrue);
			if (ent->thoracicBone >= 0)
			{
				gi.G2API_SetBoneAnglesIndex(&ent->ghoul2[ent->playerModel], ent->thoracicBone, angles,
					BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, nullptr, 0, 0);
			}
		}
		ent->client->clientInfo.infoValid = qtrue;
	}

	if (ent->client->NPC_class == CLASS_SAND_CREATURE)
	{
		ent->s.radius = 256;
	}
	else if (ent->client->NPC_class == CLASS_RANCOR)
	{
		if (ent->spawnflags & 1)
		{
			//mutant
			ent->s.radius = 300;
		}
		else
		{
			ent->s.radius = 150;
		}
	}
	else if (ent->s.radius <= 0) //radius cannot be negative or zero
	{
		//set the radius to be the largest axial distance on the entity
		float max = ent->mins[0]; //NOTE: mins is always negative
		if (max > ent->mins[1])
		{
			max = ent->mins[1];
		}

		if (max > ent->mins[2])
		{
			max = ent->mins[2];
		}

		max = fabs(max); //convert to positive to compare with maxs
		if (max < ent->maxs[0])
		{
			max = ent->maxs[0];
		}

		if (max < ent->maxs[1])
		{
			max = ent->maxs[1];
		}

		if (max < ent->maxs[2])
		{
			max = ent->maxs[2];
		}

		ent->s.radius = static_cast<int>(max);

		if (!ent->s.radius) // Still no radius?
		{
			ent->s.radius = 60;
		}
	}

	// set the weaponmodel to -1 so we don't try to remove it in Pmove before we have it built
	ent->weaponModel[0] = -1;

	if (ent->playerModel == -1)
	{
		return qfalse;
	}
	return qtrue;
}

constexpr auto TURN_OFF = 0x00000100;
void g_set_g2_player_model(gentity_t* ent, const char* model_name, const char* customSkin, const char* surf_off, const char* surf_on)
{
	char skin_name[MAX_QPATH];

	//ok, lets register the skin name, and then pass that name to the config strings so the client can get it too.
	if (!customSkin)
	{
		//use the default
		Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/model_default.skin", model_name);
	}
	else
	{
		if (strchr(customSkin, '|'))
		{
			//three part skin
			Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/|%s", model_name, customSkin);

			//if (ent == player)
			//{
			//	char name[MAX_QPATH];
			//	strcpy(name, customSkin);
			//	char* p = strchr(name, '|');
			//	*p = 0;
			//	p++;

			//	gi.cvar_set("g_char_skin_head", name);

			//	//advance to second
			//	char* p2 = strchr(p, '|');
			//	if (!p2)
			//	{
			//		return;
			//	}
			//	*p2 = 0;
			//	p2++;
			//	gi.cvar_set("g_char_skin_torso", p);
			//	gi.cvar_set("g_char_skin_legs", p2);
			//}
		}
		else
		{
			Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/model_%s.skin", model_name, customSkin);

			/*if (ent == player)
			{
				gi.cvar_set("g_char_skin_head", va("model_%s", customSkin));
				gi.cvar_set("g_char_skin_torso", va("model_%s", customSkin));
				gi.cvar_set("g_char_skin_legs", va("model_%s", customSkin));
			}*/
		}
	}
	int skin = gi.RE_RegisterSkin(skin_name);
	//now generate the ghoul2 model this client should be.
	if (ent->client->NPC_class == CLASS_VEHICLE)
	{
		//vehicles actually grab their model from the appropriate vehicle data entry
		// This will register the model and other assets.
		Vehicle_t* p_veh = ent->m_pVehicle;
		p_veh->m_pVehicleInfo->RegisterAssets(p_veh);
		ent->playerModel = gi.G2API_InitGhoul2Model(ent->ghoul2, va("models/players/%s/model.glm", model_name),
			p_veh->m_pVehicleInfo->modelIndex, G_SkinIndex(skin_name),
			NULL_HANDLE, 0, 0);
	}
	else
	{
		//NOTE: it still loads the default skin's tga's because they're referenced in the .glm.
		ent->playerModel = gi.G2API_InitGhoul2Model(ent->ghoul2, va("models/players/%s/model.glm", model_name),
			G_ModelIndex(va("models/players/%s/model.glm", model_name)),
			G_SkinIndex(skin_name), NULL_HANDLE, 0, 0);
	}
	if (ent->playerModel == -1)
	{
		//try the stormtrooper as a default
		gi.Printf(S_COLOR_RED"G_SetG2PlayerModel: cannot load model %s\n", model_name);
		model_name = "stormtrooper";
		Com_sprintf(skin_name, sizeof skin_name, "models/players/%s/model_default.skin", model_name);
		skin = gi.RE_RegisterSkin(skin_name);
		ent->playerModel = gi.G2API_InitGhoul2Model(ent->ghoul2, va("models/players/%s/model.glm", model_name),
			G_ModelIndex(va("models/players/%s/model.glm", model_name)),
			NULL_HANDLE, NULL_HANDLE, 0, 0);
	}
	if (ent->playerModel == -1)
	{
		//very bad thing here!
		Com_Error(ERR_DROP, "Cannot fall back to default model %s!", model_name);
	}

	gi.G2API_SetSkin(&ent->ghoul2[ent->playerModel], G_SkinIndex(skin_name), skin);
	//this is going to set the surfs on/off matching the skin file

	// did we find a ghoul2 model? if so, load the animation.cfg file
	if (!g_set_g2_player_model_info(ent, model_name, surf_off, surf_on))
	{
		//couldn't set g2 info, fall back to a mouse md3
		NPC_ParseParms("mouse", ent);
		Com_Printf(S_COLOR_RED"couldn't load playerModel %s!\n", va("models/players/%s/model.glm", model_name));
	}

	gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "force_shield", TURN_OFF);
}

/*
Ghoul2 Insert End
*/

void G_RemovePlayerModel(gentity_t* ent)
{
	if (ent->playerModel >= 0 && ent->ghoul2.size())
	{
		gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->playerModel);
		ent->playerModel = -1;
	}
}

void G_RemoveWeaponModels(gentity_t* ent)
{
	if (ent->ghoul2.size())
	{
		if (ent->weaponModel[0] > 0)
		{
			gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[0]);
			ent->weaponModel[0] = -1;
		}
		if (ent->weaponModel[1] > 0)
		{
			gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[1]);
			ent->weaponModel[1] = -1;
		}
	}
}

void G_RemoveHolsterModels(gentity_t* ent)
{
	if (ent->ghoul2.size())
	{
		if (ent->holsterModel[0] > 0)
		{
			gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->holsterModel[0]);
			ent->holsterModel[0] = -1;
		}
		if (ent->holsterModel[1] > 0)
		{
			gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->holsterModel[1]);
			ent->holsterModel[1] = -1;
		}
	}
}

static void G_AddWeaponModels(gentity_t* ent)
{
	if (!ent || !ent->client)
	{
		return;
	}
	if (ent->weaponModel[0] == -1)
	{
		if (ent->client->ps.weapon == WP_SABER)
		{
			WP_SaberAddG2SaberModels(ent);
			G_RemoveHolsterModels(ent);
		}
		else if (ent->client->ps.weapon != WP_NONE)
		{
			if (ent->client->ps.weapon == WP_EMPLACED_GUN && !(ent->client->ps.eFlags & EF_LOCKED_TO_WEAPON))
			{
				G_CreateG2AttachedWeaponModel(ent, "models/map_objects/hoth/eweb_model.glm", ent->handRBolt, 0);
			}
			else if (ent->client->ps.weapon == WP_DROIDEKA)
			{
				if (com_kotor->integer == 1) //playing kotor
				{
					G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handLBolt, 1);
				}
				else
				{
					if (ent->client->charKOTORWeapons == 1)
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handLBolt, 1);
					}
					else
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handRBolt, 0);
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handLBolt, 1);
					}
				}
			}
			else
			{
				if (com_kotor->integer == 1) //playing kotor
				{
					G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
				}
				else
				{
					if (ent->client->charKOTORWeapons == 1)
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
					}
					else
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handRBolt, 0);
					}
				}
			}
			WP_SaberAddHolsteredG2SaberModels(ent);
		}
		else
		{
			WP_SaberAddHolsteredG2SaberModels(ent);
		}
	}
}

extern saber_colors_t TranslateSaberColor(const char* name);
extern void WP_RemoveSaber(gentity_t* ent, int saber_num);
extern void WP_RemoveSecondSaber(gentity_t* ent, int saber_num);

void G_SetSabersFromCVars(gentity_t* ent)
{
	if (g_saber->string
		&& g_saber->string[0]
		&& Q_stricmp("none", g_saber->string)
		&& Q_stricmp("NULL", g_saber->string))
	{
		WP_SaberParseParms(g_saber->string, &ent->client->ps.saber[0]);
		if (ent->client->ps.saber[0].stylesLearned)
		{
			ent->client->ps.saberStylesKnown |= ent->client->ps.saber[0].stylesLearned;
		}
		if (ent->client->ps.saber[0].singleBladeStyle)
		{
			ent->client->ps.saberStylesKnown |= ent->client->ps.saber[0].singleBladeStyle;
		}
	}

	if (player
		&& player->client
		&& player->client->sess.mission_objectives[LIGHTSIDE_OBJ].status == 2
		&& g_saberDarkSideSaberColor->integer)
	{
		//dark side!
		//always use red
		for (auto& n : ent->client->ps.saber[0].blade)
		{
			n.color = SABER_RED;
		}
	}
	else if (g_saber_color->string)
	{
		//FIXME: how to specify color for each blade and/or color for second saber?
		const saber_colors_t color = TranslateSaberColor(g_saber_color->string);
		for (auto& n : ent->client->ps.saber[0].blade)
		{
			n.color = color;
		}
	}
	if (g_saber2->string
		&& g_saber2->string[0]
		&& Q_stricmp("none", g_saber2->string)
		&& Q_stricmp("NULL", g_saber2->string))
	{
		if (!(ent->client->ps.saber[0].saberFlags & SFL_TWO_HANDED))
		{
			//can't use a second saber if first one is a two-handed saber...?
			WP_SaberParseParms(g_saber2->string, &ent->client->ps.saber[1]);
			if (ent->client->ps.saber[1].stylesLearned)
			{
				ent->client->ps.saberStylesKnown |= ent->client->ps.saber[1].stylesLearned;
			}
			if (ent->client->ps.saber[1].singleBladeStyle)
			{
				ent->client->ps.saberStylesKnown |= ent->client->ps.saber[1].singleBladeStyle;
			}
			if (ent->client->ps.saber[1].saberFlags & SFL_TWO_HANDED)
			{
				//tsk tsk, can't use a twoHanded saber as second saber
				WP_RemoveSaber(ent, 1);
			}
			else
			{
				ent->client->ps.dualSabers = qtrue;
				if (player
					&& player->client
					&& player->client->sess.mission_objectives[LIGHTSIDE_OBJ].status == 2
					&& g_saberDarkSideSaberColor->integer)
				{
					//dark side!
					//always use red
					for (auto& n : ent->client->ps.saber[1].blade)
					{
						n.color = SABER_RED;
					}
				}
				else if (g_saber2_color->string)
				{
					const saber_colors_t color = TranslateSaberColor(g_saber2_color->string);
					for (auto& n : ent->client->ps.saber[1].blade)
					{
						n.color = color;
					}
				}
			}
		}
	}
}

void G_InitPlayerFromCvars(gentity_t* ent)
{
	//set model based on cvars
	if (Q_stricmp(g_char_skin_head->string, "model_default") == 0 &&
		Q_stricmp(g_char_skin_torso->string, "model_default") == 0 && Q_stricmp(
			g_char_skin_legs->string, "model_default") == 0)
		G_ChangePlayerModel(ent, va("%s|model_default", g_char_model->string));
	else
		G_ChangePlayerModel(ent, va("%s|%s|%s|%s", g_char_model->string, g_char_skin_head->string,
			g_char_skin_torso->string, g_char_skin_legs->string));

	//FIXME: parse these 2 from some cvar or require playermodel to be in a *.npc?
	if (ent->NPC_type && gi.bIsFromZone(ent->NPC_type, TAG_G_ALLOC))
	{
		gi.Free(ent->NPC_type);
	}

	// Bad casting I know, but NPC_type can also come the memory manager,
	// and you can't free a const-pointer later on. This seemed like the
	// better options.
	ent->NPC_type = static_cast<char*>("player"); //default for now
	if (ent->client->clientInfo.customBasicSoundDir && gi.bIsFromZone(ent->client->clientInfo.customBasicSoundDir,
		TAG_G_ALLOC))
	{
		gi.Free(ent->client->clientInfo.customBasicSoundDir);
	}

	char snd[512];
	gi.Cvar_VariableStringBuffer("snd", snd, sizeof snd);

	ent->client->clientInfo.customBasicSoundDir = G_NewString(snd); //copy current cvar

	//set the lightsaber
	G_RemoveWeaponModels(ent);
	G_RemoveHolsterModels(ent);

	G_SetSabersFromCVars(ent);
	//set up weapon models, etc.
	G_AddWeaponModels(ent);
	NPC_SetAnim(ent, SETANIM_LEGS, ent->client->ps.legsAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);
	NPC_SetAnim(ent, SETANIM_TORSO, ent->client->ps.torsoAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);
	if (!ent->s.number)
	{
		//the actual player, not an NPC pretending to be a player
		client_userinfo_changed(ent->s.number);
	}
	//color tinting
	//FIXME: the customRGBA shouldn't be set if the shader this guys .skin is using doesn't have the tinting on it
	if (g_char_color_red->integer
		|| g_char_color_green->integer
		|| g_char_color_blue->integer)
	{
		ent->client->renderInfo.customRGBA[0] = g_char_color_red->integer;
		ent->client->renderInfo.customRGBA[1] = g_char_color_green->integer;
		ent->client->renderInfo.customRGBA[2] = g_char_color_blue->integer;
		ent->client->renderInfo.customRGBA[3] = 255;
	}
}

static qboolean AllowedDualPistol(const gentity_t* ent)
{
	switch (ent->s.weapon)
	{
	case WP_BLASTER_PISTOL:
	case WP_REY:
	case WP_JANGO:
	case WP_CLONEPISTOL:
	case WP_REBELBLASTER:
		break;
	default:;
	}
	return qfalse;
}

void G_ChangeModel(const char* new_model)
{
	gi.cvar_set("g_char_model", new_model);
	gi.cvar_set("g_char_skin_head", "model_default");
	gi.cvar_set("g_char_skin_torso", "model_default");
	gi.cvar_set("g_char_skin_legs", "model_default");
	G_InitPlayerFromCvars(&g_entities[0]);
}

void G_ChangeScale(const char* data)
{
	const int value = std::stoi(data);
	player->s.modelScale[0] = player->s.modelScale[1] = player->s.modelScale[2] = value / 100.0f;
}

extern const char* GetSaberColor(int color);

void G_ChangePlayerModel(gentity_t* ent, const char* new_model)
{
	if (!ent || !ent->client || !new_model)
	{
		return;
	}

	if (cg_trueguns.integer > 0)
	{
		gi.cvar_set("cg_trueguns", "0");
	}

	if (ent->client && (ent->client->moveType == MT_FLYSWIM || ent->s.groundEntityNum == ENTITYNUM_NONE ||
		JET_Flying(ent) || ent->client->flamethrowerOn)
		&& (ent->client->NPC_class == CLASS_BOBAFETT
			|| ent->client->NPC_class == CLASS_MANDALORIAN
			|| ent->client->NPC_class == CLASS_JANGO
			|| ent->client->NPC_class == CLASS_JANGODUAL
			|| ent->client->NPC_class == CLASS_ROCKETTROOPER))
	{
		gi.Printf(S_COLOR_RED "ERROR: You can not change characters when jetpacking or using the flamethrower. You must now re-select a character.\n");
		return;
	}

	if (!in_camera
		&& (ent->client->usercmd.buttons & BUTTON_ATTACK
			|| ent->client->usercmd.buttons & BUTTON_ALT_ATTACK
			|| ent->client->usercmd.buttons & BUTTON_USE_FORCE
			|| ent->client->usercmd.buttons & BUTTON_FORCEGRIP
			|| ent->client->usercmd.buttons & BUTTON_FORCE_LIGHTNING
			|| ent->client->usercmd.buttons & BUTTON_LIGHTNING_STRIKE
			|| ent->client->usercmd.buttons & BUTTON_PROJECTION
			|| ent->client->usercmd.buttons & BUTTON_FORCE_DRAIN
			|| ent->client->usercmd.buttons & BUTTON_BLOCK
			|| ent->client->usercmd.buttons & BUTTON_KICK
			//|| (ent->client->usercmd.buttons & BUTTON_USE)
			|| ent->client->usercmd.buttons & BUTTON_DASH))
	{
		gi.Printf(S_COLOR_RED "ERROR: You can not change characters when holding function buttons. You must now re-select a character.\n");
		return;
	}

	if (ent->client->NPC_class != CLASS_JANGODUAL && ent->client->ps.eFlags & EF2_JANGO_DUALS)
	{
		ent->client->ps.eFlags &= ~EF2_JANGO_DUALS;
	}

	if (ent->client->ps.weapon != WP_DUAL_PISTOL && ent->client->ps.eFlags & EF2_JANGO_DUALS)
	{
		ent->client->ps.eFlags &= ~EF2_JANGO_DUALS;
	}

	if (ent->client->ps.weapon != WP_DUAL_CLONEPISTOL)
	{
		ent->client->ps.eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
	}

	if (!Char_Dual_Pistols(ent))
	{
		ent->client->ps.eFlags &= ~EF2_DUAL_CLONE_PISTOLS;
	}

	if (!Calo_Nord(ent))
	{
		ent->client->ps.eFlags &= ~EF2_DUAL_CALONORD;
	}

	if ((!AllowedDualPistol(ent) || g_allowdualpistols->integer < 1) && ent->client->ps.eFlags & EF2_DUAL_PISTOLS)
	{
		//Changing
		ent->client->ps.eFlags &= ~EF2_DUAL_PISTOLS;
	}

	if (ent->client->ps.weapon == WP_DROIDEKA && ent->client->NPC_class == CLASS_DROIDEKA)
	{
		if (com_kotor->integer == 1) //playing kotor
		{
			G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handLBolt, 1);
		}
		else
		{
			if (ent->client->charKOTORWeapons == 1)
			{
				G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handLBolt, 1);
			}
			else
			{
				G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handLBolt, 1);
			}
		}
	}

	if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE && PM_InLedgeMove(ent->client->ps.legsAnim))
	{
		ent->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
		ent->client->ps.legsAnimTimer = 0;
		ent->client->ps.torsoAnimTimer = 0;

		G_RemovePlayerModel(ent);

		if (Q_stricmp("player", new_model) == 0)
		{
			G_InitPlayerFromCvars(ent);
			return;
		}

		if (ent->NPC_type && gi.bIsFromZone(ent->NPC_type, TAG_G_ALLOC))
		{
			gi.Free(ent->NPC_type);
		}
		ent->NPC_type = G_NewString(new_model);

		G_RemoveWeaponModels(ent);
		G_RemoveHolsterModels(ent);

		if (strchr(new_model, '|'))
		{
			char name[MAX_QPATH];
			strcpy(name, new_model);
			char* p = strchr(name, '|');
			*p = 0;
			p++;

			if (strstr(p, "model_default"))
			{
				g_set_g2_player_model(ent, name, nullptr, nullptr, nullptr);
			}
			else
			{
				g_set_g2_player_model(ent, name, p, nullptr, nullptr);
			}
		}
		else
		{
			if (NPC_ParseParms(ent->NPC_type, ent))
			{
				G_AddWeaponModels(ent);

				NPC_SetAnim(ent, SETANIM_LEGS, ent->client->ps.legsAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);
				NPC_SetAnim(ent, SETANIM_TORSO, ent->client->ps.torsoAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);

				client_userinfo_changed(ent->s.number);

				if (ent->client->NPC_class == CLASS_BOBAFETT
					|| ent->client->NPC_class == CLASS_MANDALORIAN
					|| ent->client->NPC_class == CLASS_JANGO
					|| ent->client->NPC_class == CLASS_JANGODUAL
					|| ent->client->NPC_class == CLASS_ROCKETTROOPER)
				{
					Boba_Precache(); // player as boba?
					G_RemoveHolsterModels(ent);
				}

				if (g_SerenityJediEngineMode->integer)
				{
					if (com_outcast->integer == 1) //playing outcast
					{
						ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] = 1;
					}
					else
					{
						ent->client->ps.inventory[INV_ELECTROBINOCULARS] = 1;
					}

					ent->client->ps.inventory[INV_BACTA_CANISTER] = 2;

					if (HeIsJedi(ent))
					{
						ent->client->ps.inventory[INV_CLOAK] = 1;
						ent->client->ps.inventory[INV_SEEKER] = 2;

						ent->client->ps.inventory[INV_GRAPPLEHOOK] = 0;
						ent->client->ps.inventory[INV_BARRIER] = 0;
						ent->client->ps.inventory[INV_SENTRY] = 0;
					}

					if (Grievious_Classes(ent))
					{
						ent->client->ps.inventory[INV_GRAPPLEHOOK] = 1;
					}

					if (Calo_Nord(ent))
					{
						ent->client->ps.inventory[INV_BARRIER] = 1;
						ent->client->ps.inventory[INV_SENTRY] = 1;
					}

					if (!HeIsJedi(ent))
					{
						if (ent->client->NPC_class == CLASS_DROIDEKA)
						{
							ent->client->ps.inventory[INV_BARRIER] = 1;

							ent->client->ps.inventory[INV_CLOAK] = 0;
							ent->client->ps.inventory[INV_SEEKER] = 0;
							ent->client->ps.inventory[INV_ELECTROBINOCULARS] = 0;
							ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] = 0;
							ent->client->ps.inventory[INV_CLOAK] = 0;
							ent->client->ps.inventory[INV_SEEKER] = 0;
							ent->client->ps.inventory[INV_BACTA_CANISTER] = 0;
							ent->client->ps.inventory[INV_SENTRY] = 0;

							if (ent->client->ps.powerups[PW_GALAK_SHIELD] || ent->flags & FL_SHIELDED)
							{
								TurnBarrierOff(ent);
							}
						}
						else
						{
							if (ent->client->NPC_class == CLASS_BOBAFETT
								|| ent->client->NPC_class == CLASS_JANGO
								|| ent->client->NPC_class == CLASS_JANGODUAL
								|| ent->client->NPC_class == CLASS_MANDALORIAN
								|| char_has_beskar_armor(ent))
							{
								ent->client->ps.inventory[INV_GRAPPLEHOOK] = 1;

								if (char_has_beskar_armor(ent))
								{
									ent->flags |= FL_DINDJARIN; //low-level shots bounce off, no knockback
								}

								if (!Q_stricmp("boba_fett", ent->NPC_type))
								{
									ent->flags |= FL_BOBAFETT; //low-level shots bounce off, no knockback
								}
								ent->flags |= FL_SABERDAMAGE_RESIST; //Partially resistant to sabers
							}
							else
							{
								ent->client->ps.inventory[INV_GRAPPLEHOOK] = 0;
							}
							ent->client->ps.inventory[INV_BARRIER] = 1;
							ent->client->ps.inventory[INV_SENTRY] = 2;

							ent->client->ps.inventory[INV_CLOAK] = 0;
							ent->client->ps.inventory[INV_SEEKER] = 0;
						}
					}
				}
				else
				{
					if (com_outcast->integer == 1) //playing outcast
					{
						ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] = 1;
					}
					else
					{
						ent->client->ps.inventory[INV_ELECTROBINOCULARS] = 1;
					}
					ent->client->ps.inventory[INV_BACTA_CANISTER] = 1;

					ent->client->ps.inventory[INV_GRAPPLEHOOK] = 0;
					ent->client->ps.inventory[INV_BARRIER] = 0;
					ent->client->ps.inventory[INV_SEEKER] = 0;
					ent->client->ps.inventory[INV_SENTRY] = 0;
					ent->client->ps.inventory[INV_CLOAK] = 0;
				}
			}
			else
			{
				gi.Printf(S_COLOR_RED"G_ChangePlayerModel: cannot find NPC %s\n", new_model);
				G_ChangePlayerModel(ent, "stormtrooper"); //need a better fallback?
			}
		}
	}
	else
	{
		G_RemovePlayerModel(ent);

		if (Q_stricmp("player", new_model) == 0)
		{
			G_InitPlayerFromCvars(ent);
			return;
		}

		if (ent->NPC_type && gi.bIsFromZone(ent->NPC_type, TAG_G_ALLOC))
		{
			gi.Free(ent->NPC_type);
		}
		ent->NPC_type = G_NewString(new_model);

		G_RemoveWeaponModels(ent);
		G_RemoveHolsterModels(ent);

		if (strchr(new_model, '|'))
		{
			char name[MAX_QPATH];
			strcpy(name, new_model);
			char* p = strchr(name, '|');
			*p = 0;
			p++;

			if (strstr(p, "model_default"))
			{
				g_set_g2_player_model(ent, name, nullptr, nullptr, nullptr);
			}
			else
			{
				g_set_g2_player_model(ent, name, p, nullptr, nullptr);
			}
		}
		else
		{
			if (NPC_ParseParms(ent->NPC_type, ent))
			{
				G_AddWeaponModels(ent);

				NPC_SetAnim(ent, SETANIM_LEGS, ent->client->ps.legsAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);
				NPC_SetAnim(ent, SETANIM_TORSO, ent->client->ps.torsoAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);

				client_userinfo_changed(ent->s.number);

				if (ent->client->NPC_class == CLASS_BOBAFETT
					|| ent->client->NPC_class == CLASS_MANDALORIAN
					|| ent->client->NPC_class == CLASS_JANGO
					|| ent->client->NPC_class == CLASS_JANGODUAL
					|| ent->client->NPC_class == CLASS_ROCKETTROOPER)
				{
					Boba_Precache(); // player as boba?
					G_RemoveHolsterModels(ent);
				}

				if (g_SerenityJediEngineMode->integer)
				{
					if (com_outcast->integer == 1) //playing outcast
					{
						ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] = 1;
					}
					else
					{
						ent->client->ps.inventory[INV_ELECTROBINOCULARS] = 1;
					}

					ent->client->ps.inventory[INV_BACTA_CANISTER] = 2;

					if (HeIsJedi(ent))
					{
						ent->client->ps.inventory[INV_CLOAK] = 1;
						ent->client->ps.inventory[INV_SEEKER] = 2;

						ent->client->ps.inventory[INV_GRAPPLEHOOK] = 0;
						ent->client->ps.inventory[INV_BARRIER] = 0;
						ent->client->ps.inventory[INV_SENTRY] = 0;
					}

					if (Grievious_Classes(ent))
					{
						ent->client->ps.inventory[INV_GRAPPLEHOOK] = 1;
					}

					if (Calo_Nord(ent))
					{
						ent->client->ps.inventory[INV_BARRIER] = 1;
						ent->client->ps.inventory[INV_SENTRY] = 1;
					}

					if (!HeIsJedi(ent))
					{
						if (ent->client->NPC_class == CLASS_DROIDEKA)
						{
							ent->client->ps.inventory[INV_BARRIER] = 1;

							ent->client->ps.inventory[INV_CLOAK] = 0;
							ent->client->ps.inventory[INV_SEEKER] = 0;
							ent->client->ps.inventory[INV_ELECTROBINOCULARS] = 0;
							ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] = 0;
							ent->client->ps.inventory[INV_CLOAK] = 0;
							ent->client->ps.inventory[INV_SEEKER] = 0;
							ent->client->ps.inventory[INV_BACTA_CANISTER] = 0;
							ent->client->ps.inventory[INV_SENTRY] = 0;

							if (ent->client->ps.powerups[PW_GALAK_SHIELD] || ent->flags & FL_SHIELDED)
							{
								TurnBarrierOff(ent);
							}
						}
						else
						{
							if (ent->client->NPC_class == CLASS_BOBAFETT
								|| ent->client->NPC_class == CLASS_JANGO
								|| ent->client->NPC_class == CLASS_JANGODUAL
								|| ent->client->NPC_class == CLASS_MANDALORIAN
								|| char_has_beskar_armor(ent))
							{
								ent->client->ps.inventory[INV_GRAPPLEHOOK] = 1;

								if (char_has_beskar_armor(ent))
								{
									ent->flags |= FL_DINDJARIN; //low-level shots bounce off, no knockback
								}

								if (!Q_stricmp("boba_fett", ent->NPC_type))
								{
									ent->flags |= FL_BOBAFETT; //low-level shots bounce off, no knockback
								}
								ent->flags |= FL_SABERDAMAGE_RESIST; //Partially resistant to sabers
							}
							else
							{
								ent->client->ps.inventory[INV_GRAPPLEHOOK] = 0;
							}
							ent->client->ps.inventory[INV_BARRIER] = 1;
							ent->client->ps.inventory[INV_SENTRY] = 2;

							ent->client->ps.inventory[INV_CLOAK] = 0;
							ent->client->ps.inventory[INV_SEEKER] = 0;
						}
					}
				}
				else
				{
					if (com_outcast->integer == 1) //playing outcast
					{
						ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] = 1;
					}
					else
					{
						ent->client->ps.inventory[INV_ELECTROBINOCULARS] = 1;
					}
					ent->client->ps.inventory[INV_BACTA_CANISTER] = 1;

					ent->client->ps.inventory[INV_GRAPPLEHOOK] = 0;
					ent->client->ps.inventory[INV_BARRIER] = 0;
					ent->client->ps.inventory[INV_SEEKER] = 0;
					ent->client->ps.inventory[INV_SENTRY] = 0;
					ent->client->ps.inventory[INV_CLOAK] = 0;

					TurnBarrierOff(ent);
				}
			}
			else
			{
				gi.Printf(S_COLOR_RED"G_ChangePlayerModel: cannot find NPC %s\n", new_model);
				G_ChangePlayerModel(ent, "stormtrooper"); //need a better fallback?

				TurnBarrierOff(ent);
			}
		}
	}
}

void G_ReloadSaberData(const gentity_t* ent)
{
	//dualSabers should already be set
	if (ent->client->ps.saber[0].name != nullptr)
	{
		WP_SaberParseParms(ent->client->ps.saber[0].name, &ent->client->ps.saber[0], qfalse);
		if (ent->client->ps.saber[0].stylesLearned)
		{
			ent->client->ps.saberStylesKnown |= ent->client->ps.saber[0].stylesLearned;
		}
		if (ent->client->ps.saber[0].singleBladeStyle)
		{
			ent->client->ps.saberStylesKnown |= ent->client->ps.saber[0].singleBladeStyle;
		}
	}
	if (ent->client->ps.saber[1].name != nullptr)
	{
		WP_SaberParseParms(ent->client->ps.saber[1].name, &ent->client->ps.saber[1], qfalse);
		if (ent->client->ps.saber[1].stylesLearned)
		{
			ent->client->ps.saberStylesKnown |= ent->client->ps.saber[1].stylesLearned;
		}
		if (ent->client->ps.saber[1].singleBladeStyle)
		{
			ent->client->ps.saberStylesKnown |= ent->client->ps.saber[1].singleBladeStyle;
		}
	}
}

qboolean G_PlayerSpawned()
{
	if (!player
		|| !player->client
		|| player->client->pers.teamState.state != TEAM_ACTIVE
		|| level.time - player->client->pers.enterTime < 100)
	{
		//player hasn't spawned yet
		return qfalse;
	}
	return qtrue;
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/

qboolean G_CheckPlayerDarkSide()
{
	if (player && player->client && player->client->sess.mission_objectives[LIGHTSIDE_OBJ].status == 2)
	{
		//dark side player!
		player->client->playerTeam = TEAM_FREE;
		player->client->enemyTeam = TEAM_FREE;
		if (g_saberDarkSideSaberColor->integer)
		{
			//dark side!
			//always use red
			for (int n = 0; n < MAX_BLADES; n++)
			{
				player->client->ps.saber[0].blade[n].color = player->client->ps.saber[1].blade[n].color = SABER_RED;
			}
		}
		G_SoundIndex("sound/chars/jedi2/28je2008.wav");
		G_SoundIndex("sound/chars/jedi2/28je2009.wav");
		G_SoundIndex("sound/chars/jedi2/28je2012.wav");
		return qtrue;
	}
	return qfalse;
}

qboolean ClientSpawn(gentity_t* ent, SavedGameJustLoaded_e e_saved_game_just_loaded)
{
	int index;
	gclient_t* client;
	int i;
	clientInfo_t saved_ci;
	usercmd_t ucmd;
	qboolean beam_in_effect = qfalse;
	extern qboolean g_qbLoadTransition;

	index = ent - g_entities;
	client = ent->client;

	client->lastSaberTarget = nullptr;
	client->lastSaberTargetTime = 0;

	if (e_saved_game_just_loaded == eFULL && g_qbLoadTransition == qfalse) //qbFromSavedGame)
	{
		//loading up a full save game
		ent->client->pers.teamState.state = TEAM_ACTIVE;

		// increment the spawncount so the client will detect the respawn
		client->ps.persistant[PERS_SPAWN_COUNT]++;
		client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

		client->airOutTime = level.time + 12000;

		for (i = 0; i < 3; i++)
		{
			ent->client->pers.cmd_angles[i] = 0.0f;
		}

		SetClientViewAngle(ent, ent->client->ps.viewangles); //spawn_angles );

		gi.linkentity(ent);

		// run the presend to set anything else
		ClientEndFrame(ent);

		// clear entity state values
		PlayerStateToEntityState(&client->ps, &ent->s);

		// ALL OF MY RAGE... they decided it would be a great idea to treat NPC_type like a player model here,
		// which is all kinds of unbelievable. I will be having a stern talk with James later. --eez
		if (ent->NPC_type && Q_stricmp(ent->NPC_type, "player"))
		{
			// FIXME: game doesn't like it when you pass ent->NPC_type into this func. Insert all kinds of noises here --eez
			char bleh[MAX_SPAWN_VARS_CHARS];
			Q_strncpyz(bleh, ent->NPC_type, sizeof bleh);

			G_ChangePlayerModel(ent, bleh);
		}
		else
		{
			G_LoadAnimFileSet(ent, ent->NPC_type);
			G_SetSkin(ent);
		}

		//setup sabers
		G_ReloadSaberData(ent);
		//force power levels should already be set
	}
	else
	{
		gentity_t* spawn_point;
		int persistant[MAX_PERSISTANT]{};
		clientSession_t savedSess;
		clientPersistant_t saved;
		vec3_t spawn_angles;
		vec3_t spawn_origin;
		// find a spawn point
		// do it before setting health back up, so farthest
		// ranging doesn't count this client
		// don't spawn near existing origin if possible
		spawn_point = SelectSpawnPoint(ent->client->ps.origin,
			spawn_origin, spawn_angles);

		ent->client->pers.teamState.state = TEAM_ACTIVE;

		// clear everything but the persistant data
		saved = client->pers;
		savedSess = client->sess;
		for (i = 0; i < MAX_PERSISTANT; i++)
		{
			persistant[i] = client->ps.persistant[i];
		}
		//Preserve clientInfo
		memcpy(&saved_ci, &client->clientInfo, sizeof(clientInfo_t));

		memset(client, 0, sizeof * client);

		memcpy(&client->clientInfo, &saved_ci, sizeof(clientInfo_t));

		client->pers = saved;
		client->sess = savedSess;
		for (i = 0; i < MAX_PERSISTANT; i++)
		{
			client->ps.persistant[i] = persistant[i];
		}

		// increment the spawncount so the client will detect the respawn
		client->ps.persistant[PERS_SPAWN_COUNT]++;
		client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

		client->airOutTime = level.time + 12000;

		// clear entity values
		client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
		ent->s.groundEntityNum = ENTITYNUM_NONE;
		ent->client = &level.clients[index];
		ent->mass = 10;
		ent->takedamage = qtrue;
		ent->inuse = qtrue;
		SetInUse(ent);
		ent->m_iIcarusID = IIcarusInterface::ICARUS_INVALID;
		if (!ent->NPC_type)
		{
			ent->NPC_type = static_cast<char*>("player");
		}
		ent->classname = "player";
		ent->targetname = ent->script_targetname = "player";
		if (ent->client->NPC_class == CLASS_NONE)
		{
			ent->client->NPC_class = CLASS_PLAYER;
		}
		client->playerTeam = TEAM_PLAYER;
		client->enemyTeam = TEAM_ENEMY;
		client->friendlyfaction = FACTION_LIGHT;
		client->enemyfaction = FACTION_DARK;
		ent->contents = CONTENTS_BODY;
		ent->clipmask = MASK_PLAYERSOLID;
		ent->e_DieFunc = dieF_player_die;
		ent->waterlevel = 0;
		ent->watertype = 0;
		client->ps.friction = 6;
		client->ps.gravity = g_gravity->value;
		ent->flags &= ~FL_NO_KNOCKBACK;
		ent->flags &= ~FL_NO_SABER_RETURN;
		client->renderInfo.lookTarget = ENTITYNUM_NONE;
		client->renderInfo.lookTargetClearTime = 0;
		client->renderInfo.lookMode = LM_ENT;

		VectorCopy(playerMins, ent->mins);
		VectorCopy(playerMaxs, ent->maxs);
		client->crouchheight = CROUCH_MAXS_2;
		client->standheight = DEFAULT_MAXS_2;

		client->ps.client_num = index;

		// give default weapons
		//these are precached in g_items, ClearRegisteredItems()
		if (com_outcast->integer == 0) //playing academy
		{
			for (char& weapon : client->ps.weapons)
			{
				weapon = 0;
			}
			client->ps.weapons[WP_NONE] = 1;

			if (g_SerenityJediEngineMode->integer)
			{
				client->ps.inventory[INV_ELECTROBINOCULARS] = 1;
				client->ps.inventory[INV_BACTA_CANISTER] = 1;
				client->ps.inventory[INV_CLOAK] = 1;
				client->ps.inventory[INV_SEEKER] = 1;
			}
			else
			{
				client->ps.inventory[INV_BACTA_CANISTER] = 1;
			}
		}
		else if (com_outcast->integer == 1) //playing outcast
		{
			client->ps.weapons[WP_MELEE] = 1;
			client->ps.weapon = WP_BRYAR_PISTOL;

			if (g_SerenityJediEngineMode->integer)
			{
				client->ps.inventory[INV_BACTA_CANISTER] = 1;
				client->ps.inventory[INV_BARRIER] = 1;
				client->ps.inventory[INV_SENTRY] = 1;
			}
			else
			{
				client->ps.inventory[INV_BACTA_CANISTER] = 1;
			}
		}
		else
		{
			for (char& weapon : client->ps.weapons)
			{
				weapon = 0;
			}
			client->ps.weapons[WP_NONE] = 1;

			if (g_SerenityJediEngineMode->integer)
			{
				client->ps.inventory[INV_ELECTROBINOCULARS] = 1;
				client->ps.inventory[INV_BACTA_CANISTER] = 1;
				client->ps.inventory[INV_CLOAK] = 1;
				client->ps.inventory[INV_SEEKER] = 1;
			}
			else
			{
				client->ps.inventory[INV_BACTA_CANISTER] = 1;
				client->ps.inventory[INV_GRAPPLEHOOK] = 0;
				client->ps.inventory[INV_BARRIER] = 0;
				client->ps.inventory[INV_SEEKER] = 0;
				client->ps.inventory[INV_SENTRY] = 0;
				client->ps.inventory[INV_CLOAK] = 0;
			}
		}

		if (com_freeMelee->integer == 1)
		{
			client->ps.weapons[WP_MELEE] = 1;
		}

		// give EITHER the saber or the stun baton..never both
		if (spawn_point->spawnflags & 32) // STUN_BATON
		{
			client->ps.weapons[WP_STUN_BATON] = 1;
			client->ps.weapon = WP_STUN_BATON;
		}
		else
		{
			// give the saber because most test maps will not have the STUN BATON flag set
			client->ps.weapons[WP_SABER] = 1; //this is precached in SP_info_player_deathmatch
			client->ps.weapon = WP_SABER;
		}
		// force the base weapon up
		client->ps.weaponstate = WEAPON_READY;

		for (i = FIRST_WEAPON; i < WP_NUM_WEAPONS; i++) // don't give ammo for explosives
		{
			if (playerUsableWeapons[i] && client->ps.weapons[i])
			{
				//if starting with this weapon, gimme max ammo for it
				client->ps.ammo[weaponData[i].ammoIndex] = ammoData[weaponData[i].ammoIndex].max;
			}
		}

		if (e_saved_game_just_loaded == eNO)
		{
			//FIXME: get player's info from NPCs.cfg
			client->ps.dualSabers = qfalse;
			WP_SaberParseParms(g_saber->string, &client->ps.saber[0]); //get saber info

			client->ps.saberStylesKnown |= 1 << gi.Cvar_VariableIntegerValue("g_fighting_style");
			WP_InitForcePowers(ent); //Initialize force powers
		}
		else
		{
			//autoload, will be taken care of below
		}

		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];
		ent->client->dismemberProbHead = 90;
		ent->client->dismemberProbArms = 95;
		ent->client->dismemberProbHands = 20;
		ent->client->dismemberProbWaist = 90;
		ent->client->dismemberProbLegs = 90;

		ent->client->ps.batteryCharge = 2500;
		ent->client->ps.cloakFuel = 100;
		ent->client->ps.blockPoints = 100;
		ent->client->ps.jetpackFuel = 100;
		ent->client->ps.sprintFuel = 100;
		ent->client->ps.BarrierFuel = 100;

		ent->reloadTime = 0;

		VectorCopy(spawn_origin, client->ps.origin);
		VectorCopy(spawn_origin, ent->currentOrigin);

		// the respawned flag will be cleared after the attack and jump keys come up
		client->ps.pm_flags |= PMF_RESPAWNED;

		SetClientViewAngle(ent, spawn_angles);

		G_KillBox(ent);
		gi.linkentity(ent);

		// don't allow full run speed for a bit
		client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
		client->ps.pm_time = 100;

		client->respawnTime = level.time;
		client->latched_buttons = 0;

		// set default animations
		client->ps.torsoAnim = BOTH_STAND2;
		client->ps.legsAnim = BOTH_STAND2;

		//clear IK grabbing stuff
		client->ps.heldClient = client->ps.heldByClient = ENTITYNUM_NONE;
		client->ps.saberLockEnemy = ENTITYNUM_NONE; //duh, don't think i'm locking with myself

		// restore some player data
		//
		Player_RestoreFromPrevLevel(ent);

		//FIXME: put this BEFORE the Player_RestoreFromPrevLevel check above?
		if (e_saved_game_just_loaded == eNO)
		{
			//fresh start
			if (!(spawn_point->spawnflags & 1)) // not KEEP_PREV
			{
				//then restore health and armor
				ent->health = client->ps.stats[STAT_ARMOR] = client->ps.stats[STAT_HEALTH] = client->ps.stats[
					STAT_MAX_HEALTH];
				ent->client->ps.forcePower = ent->client->ps.forcePowerMax;
				ent->client->ps.cloakFuel = 100;
				ent->client->ps.blockPoints = 100;
				ent->client->ps.jetpackFuel = 100;
				ent->client->ps.sprintFuel = 100;
				ent->client->ps.BarrierFuel = 100;

				ent->reloadTime = 0;
				ent->client->ps.muzzleOverheatTime = 0;
			}
			G_InitPlayerFromCvars(ent);
		}
		else
		{
			//autoload
			if (ent->NPC_type && Q_stricmp(ent->NPC_type, "player"))
			{
				// FIXME: game doesn't like it when you pass ent->NPC_type into this func. Insert all kinds of noises here --eez
				char bleh[MAX_SPAWN_VARS_CHARS];
				Q_strncpyz(bleh, ent->NPC_type, sizeof bleh);

				G_ChangePlayerModel(ent, bleh);
			}
			else
			{
				G_LoadAnimFileSet(ent, ent->NPC_type);
				G_SetSkin(ent);
			}
			G_ReloadSaberData(ent);
			//force power levels should already be set
		}

		//NEVER start a map with either of your sabers or blades on...
		ent->client->ps.SaberDeactivate();
		// run a client frame to drop exactly to the floor,
		// initialize animations and other things
		client->ps.commandTime = level.time - 100;
		ucmd = client->pers.lastCommand;
		ucmd.serverTime = level.time;
		VectorCopyM(client->pers.cmd_angles, ucmd.angles);
		ucmd.weapon = client->ps.weapon;
		// client think calls Pmove which sets the client->ps.weapon to ucmd.weapon, so ...
		ent->client->ps.groundEntityNum = ENTITYNUM_NONE;
		ClientThink(ent - g_entities, &ucmd);

		// run the presend to set anything else
		ClientEndFrame(ent);

		// clear entity state values
		PlayerStateToEntityState(&client->ps, &ent->s);

		//ICARUS include
		Quake3Game()->FreeEntity(ent);
		Quake3Game()->InitEntity(ent);

		// Make sure no Sequencer exists then Get a new one.
		IIcarusInterface::GetIcarus()->DeleteIcarusID(ent->m_iIcarusID);
		ent->m_iIcarusID = IIcarusInterface::GetIcarus()->GetIcarusID(ent->s.number);

		if (spawn_point->spawnflags & 64) //NOWEAPON
		{
			//player starts with absolutely no weapons
			for (char& weapon : ent->client->ps.weapons)
			{
				weapon = 0;
			}
			ent->client->ps.weapons[WP_NONE] = 1;
			ent->client->ps.ammo[weaponData[WP_NONE].ammoIndex] = 32000;
			ent->client->ps.weapon = WP_NONE;
			ent->client->ps.weaponstate = WEAPON_READY;
			ent->client->ps.dualSabers = qfalse;
		}

		if (ent->client->ps.weapons[WP_SABER])
		{
			//set up so has lightsaber
			WP_SaberInitBladeData(ent);
			if ((ent->weaponModel[0] <= 0 || (ent->weaponModel[1] <= 0 && ent->client->ps.dualSabers))
				//one or both of the saber models is not initialized
				&& ent->client->ps.weapon == WP_SABER) //current weapon is saber
			{
				//add the proper models
				WP_SaberAddG2SaberModels(ent);
				G_RemoveHolsterModels(ent);
			}
		}
		if (ent->weaponModel[0] == -1 && ent->client->ps.weapon != WP_NONE)
		{
			if (ent->client->ps.weapon == WP_EMPLACED_GUN && !(ent->client->ps.eFlags & EF_LOCKED_TO_WEAPON))
			{
				G_CreateG2AttachedWeaponModel(ent, "models/map_objects/hoth/eweb_model.glm", ent->handRBolt, 0);
			}
			else if (ent->client->ps.weapon == WP_DROIDEKA)
			{
				if (com_kotor->integer == 1) //playing kotor
				{
					G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handLBolt, 1);
				}
				else
				{
					if (ent->client->charKOTORWeapons == 1)
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handLBolt, 1);
					}
					else
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handRBolt, 0);
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handLBolt, 1);
					}
				}
			}
			else
			{
				if (com_kotor->integer == 1) //playing kotor
				{
					G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
				}
				else
				{
					if (ent->client->charKOTORWeapons == 1)
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].altweaponMdl, ent->handRBolt, 0);
					}
					else
					{
						G_CreateG2AttachedWeaponModel(ent, weaponData[ent->client->ps.weapon].weaponMdl, ent->handRBolt, 0);
					}
				}
			}
			//holster sabers
			WP_SaberAddHolsteredG2SaberModels(ent);
		}

		{
			// fire the targets of the spawn point
			G_UseTargets(spawn_point, ent);
			//Designers needed them to fire off target2's as well... this is kind of messy
			G_UseTargets2(spawn_point, ent, spawn_point->target2);
		}
	}

	client->pers.enterTime = level.time; //needed mainly to stop the weapon switch to WP_NONE that happens on loads
	ent->max_health = client->ps.stats[STAT_MAX_HEALTH];

	if (e_saved_game_just_loaded == eNO)
	{
		//on map transitions, Ghoul2 frame gets reset to zero, restart our anim
		NPC_SetAnim(ent, SETANIM_LEGS, ent->client->ps.legsAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);
		NPC_SetAnim(ent, SETANIM_TORSO, ent->client->ps.torsoAnim, SETANIM_FLAG_NORMAL | SETANIM_FLAG_RESTART);
	}

	if (ent->s.number == 0)
	{
		//player
		G_CheckPlayerDarkSide();
	}

	if (ent->client->ps.weapons[WP_SABER]
		&& !ent->client->ps.saberStylesKnown)
	{
		//um, if you have a saber, you need at least 1 style to use it with...
		ent->client->ps.saberStylesKnown |= 1 << SS_MEDIUM;
	}

	Player_CheckBurn(ent);
	Player_CheckFreeze(ent);

	return beam_in_effect;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect(const int client_num)
{
	gentity_t* ent = g_entities + client_num;

	if (!ent->client)
	{
		return;
	}

	gi.unlinkentity(ent);
	ent->s.modelindex = 0;
	ent->inuse = qfalse;
	ClearInUse(ent);
	ent->classname = "disconnected";
	ent->client->pers.connected = CON_DISCONNECTED;
	ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
	if (ent->client->hook)
	{
		// free it!
		Weapon_HookFree(ent->client->hook);
	}
	if (ent->client->stun)
	{
		// free it!
		Weapon_StunFree(ent->client->stun);
	}

	gi.SetConfigstring(CS_PLAYERS + client_num, "");

	IIcarusInterface::GetIcarus()->DeleteIcarusID(ent->m_iIcarusID);
}