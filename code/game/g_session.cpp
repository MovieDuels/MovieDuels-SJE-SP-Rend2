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

#include "g_local.h"
#include "objectives.h"

/*
=======================================================================

  SESSION DATA

Session data is the only data that stays persistant across level loads
and tournament restarts.
=======================================================================
*/

/*
================
G_WriteClientSessionData

Called on game shutdown
================
*/
static void G_WriteClientSessionData(const gclient_t* client)
{
	int i;

	const char* s = va("%i", client->sess.sessionTeam);
	const char* var = va("session%i", client - level.clients);
	gi.cvar_set(var, s);

	// We're saving only one objective
	const char* s2 = va("%i %i", client->sess.mission_objectives[LIGHTSIDE_OBJ].display,
		client->sess.mission_objectives[LIGHTSIDE_OBJ].status);

	var = va("sessionobj%i", client - level.clients);
	gi.cvar_set(var, s2);

	// Throw all mission stats in to a string
	s2 = va("%i %i %i %i %i %i %i %i %i %i %i %i",
		client->sess.missionStats.secretsFound,
		client->sess.missionStats.totalSecrets,
		client->sess.missionStats.shotsFired,
		client->sess.missionStats.hits,
		client->sess.missionStats.enemiesSpawned,
		client->sess.missionStats.enemiesKilled,
		client->sess.missionStats.saberThrownCnt,
		client->sess.missionStats.saberBlocksCnt,
		client->sess.missionStats.legAttacksCnt,
		client->sess.missionStats.armAttacksCnt,
		client->sess.missionStats.torsoAttacksCnt,
		client->sess.missionStats.otherAttacksCnt
	);

	var = va("missionstats%i", client - level.clients);
	gi.cvar_set(var, s2);

	s2 = "";
	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		s2 = va("%s %i", s2, client->sess.missionStats.forceUsed[i]);
	}
	var = va("sessionpowers%i", client - level.clients);
	gi.cvar_set(var, s2);

	s2 = "";
	for (i = 0; i < WP_NUM_WEAPONS; i++)
	{
		s2 = va("%s %i", s2, client->sess.missionStats.weaponUsed[i]);
	}
	var = va("sessionweapons%i", client - level.clients);
	gi.cvar_set(var, s2);
}

/*
================
G_ReadSessionData

Called on a reconnect
================
*/
void G_ReadSessionData(gclient_t* client)
{
	// Safety: client must exist
	if (client == NULL)
	{
		Com_Printf(S_COLOR_RED "G_ReadSessionData: NULL client passed in\n");
		return;
	}

	char s[MAX_STRING_CHARS] = { 0 };
	int i = 0;
	int lightside_display = 0;

	// -----------------------------
	// Load session team
	// -----------------------------
	const char* var = va("session%i", (int)(client - level.clients));
	gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

	if (sscanf(s, "%i", &i) != 1)
	{
		Com_Printf(S_COLOR_YELLOW "G_ReadSessionData: malformed team data '%s'\n", s);
		i = TEAM_FREE;
	}

	client->sess.sessionTeam = (team_t)i;

	// -----------------------------
	// Load LIGHTSIDE objective
	// -----------------------------
	var = va("sessionobj%i", (int)(client - level.clients));
	gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

	// Clear all objectives
	for (i = 0; i < MAX_OBJECTIVES; i++)
	{
		client->sess.mission_objectives[i].display = qfalse;
		client->sess.mission_objectives[i].status = OBJECTIVE_STAT_PENDING;
	}

	// Parse LIGHTSIDE objective
	if (sscanf(s, "%i %i",
		&lightside_display,
		&client->sess.mission_objectives[LIGHTSIDE_OBJ].status) != 2)
	{
		Com_Printf(S_COLOR_YELLOW "G_ReadSessionData: malformed lightside objective '%s'\n", s);
		lightside_display = 0;
		client->sess.mission_objectives[LIGHTSIDE_OBJ].status = OBJECTIVE_STAT_PENDING;
	}

	client->sess.mission_objectives[LIGHTSIDE_OBJ].display =
		(lightside_display != 0) ? qtrue : qfalse;

	// -----------------------------
	// Load mission stats
	// -----------------------------
	var = va("missionstats%i", (int)(client - level.clients));
	gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

	const int expected_stats = 12;
	int parsed_stats = sscanf(
		s, "%i %i %i %i %i %i %i %i %i %i %i %i",
		&client->sess.missionStats.secretsFound,
		&client->sess.missionStats.totalSecrets,
		&client->sess.missionStats.shotsFired,
		&client->sess.missionStats.hits,
		&client->sess.missionStats.enemiesSpawned,
		&client->sess.missionStats.enemiesKilled,
		&client->sess.missionStats.saberThrownCnt,
		&client->sess.missionStats.saberBlocksCnt,
		&client->sess.missionStats.legAttacksCnt,
		&client->sess.missionStats.armAttacksCnt,
		&client->sess.missionStats.torsoAttacksCnt,
		&client->sess.missionStats.otherAttacksCnt);

	if (parsed_stats != expected_stats)
	{
		Com_Printf(S_COLOR_YELLOW
			"G_ReadSessionData: missionstats malformed (%d/%d fields) '%s'\n",
			parsed_stats, expected_stats, s);
	}

	// -----------------------------
	// Load force power usage
	// -----------------------------
	var = va("sessionpowers%i", (int)(client - level.clients));
	gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

	i = 0;
	char* tok = strtok(s, " ");
	while (tok != NULL && i < NUM_FORCE_POWERS)
	{
		client->sess.missionStats.forceUsed[i] = atoi(tok);
		i++;
		tok = strtok(NULL, " ");
	}

	if (i != NUM_FORCE_POWERS)
	{
		Com_Printf(S_COLOR_YELLOW
			"G_ReadSessionData: sessionpowers expected %d entries, got %d\n",
			NUM_FORCE_POWERS, i);
	}

	// -----------------------------
	// Load weapon usage
	// -----------------------------
	var = va("sessionweapons%i", (int)(client - level.clients));
	gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

	i = 0;
	tok = strtok(s, " ");
	while (tok != NULL && i < WP_NUM_WEAPONS)
	{
		client->sess.missionStats.weaponUsed[i] = atoi(tok);
		i++;
		tok = strtok(NULL, " ");
	}

	if (i != WP_NUM_WEAPONS)
	{
		Com_Printf(S_COLOR_YELLOW
			"G_ReadSessionData: sessionweapons expected %d entries, got %d\n",
			WP_NUM_WEAPONS, i);
	}
}

/*
================
G_InitSessionData

Called on a first-time connect
================
*/
void G_InitSessionData(gclient_t* client, char* userinfo)
{
	clientSession_t* sess = &client->sess;

	sess->sessionTeam = TEAM_FREE;

	G_WriteClientSessionData(client);
}

/*
==================
G_InitWorldSession

==================
*/
void G_InitWorldSession()
{
}

/*
==================
G_WriteSessionData

==================
*/
void G_WriteSessionData()
{
	gi.cvar_set("session", nullptr);

	for (int i = 0; i < level.maxclients; i++)
	{
		if (level.clients[i].pers.connected == CON_CONNECTED)
		{
			G_WriteClientSessionData(&level.clients[i]);
		}
	}
}