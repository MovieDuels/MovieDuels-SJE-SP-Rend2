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

// cl_parse.c  -- parse a message received from the server

#include "../server/exe_headers.h"

#include "client.h"
#include "client_ui.h"

const char* svc_strings[256] = {
	"svc_bad",

	"svc_nop",
	"svc_gamestate",
	"svc_configstring",
	"svc_baseline",
	"svc_serverCommand",
	"svc_download",
	"svc_snapshot"
};

void SHOWNET(const msg_t* msg, const char* s)
{
	if (cl_shownet->integer >= 2)
	{
		Com_Printf("%3i:%s\n", msg->readcount - 1, s);
	}
}

/*
=========================================================================

MESSAGE PARSING

=========================================================================
*/

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity(msg_t* msg, clSnapshot_t* frame)
{
	// save the parsed entity state into the big circular buffer so
	// it can be used as the source for a later delta
	entityState_t* state = &cl.parseEntities[cl.parseEntitiesNum & (MAX_PARSE_ENTITIES - 1)];

	MSG_ReadEntity(msg, state);

	if (state->number == (MAX_GENTITIES - 1))
	{
		return; // entity was delta removed
	}
	cl.parseEntitiesNum++;
	frame->numEntities++;
}

/*
==================
CL_ParsePacketEntities

==================
*/
void CL_ParsePacketEntities(msg_t* msg, const clSnapshot_t* oldframe, clSnapshot_t* newframe)
{
	int oldnum;

	newframe->parseEntitiesNum = cl.parseEntitiesNum;
	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	int oldindex = 0;
	const entityState_t* oldstate;
	if (!oldframe)
	{
		oldnum = 99999;
	}
	else
	{
		if (oldindex >= oldframe->numEntities)
		{
			oldnum = 99999;
		}
		else
		{
			oldstate = &cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}

	while (true)
	{
		// read the entity index number
		const int newnum = MSG_ReadBits(msg, GENTITYNUM_BITS);

		if (newnum == (MAX_GENTITIES - 1))
		{
			break;
		}

		if (msg->readcount > msg->cursize)
		{
			Com_Error(ERR_DROP, "CL_ParsePacketEntities: end of message");
		}

		while (oldnum < newnum)
		{
			// one or more entities from the old packet are unchanged
			if (cl_shownet->integer == 3)
			{
				Com_Printf("%3i:  unchanged: %i\n", msg->readcount, oldnum);
			}
			CL_DeltaEntity(msg, newframe);

			oldindex++;

			if (oldindex >= oldframe->numEntities)
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
		}
		if (oldnum == newnum)
		{
			// delta from previous state
			if (cl_shownet->integer == 3)
			{
				Com_Printf("%3i:  delta: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity(msg, newframe);

			oldindex++;

			if (oldindex >= oldframe->numEntities)
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum > newnum)
		{
			// delta from baseline
			if (cl_shownet->integer == 3)
			{
				Com_Printf("%3i:  baseline: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity(msg, newframe);
		}
	}

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{
		// one or more entities from the old packet are unchanged
		if (cl_shownet->integer == 3)
		{
			Com_Printf("%3i:  unchanged: %i\n", msg->readcount, oldnum);
		}
		CL_DeltaEntity(msg, newframe);

		oldindex++;

		if (oldindex >= oldframe->numEntities)
		{
			oldnum = 99999;
		}
		else
		{
			oldstate = &cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}
}

/*
================
CL_ParseSnapshot

If the snapshot is parsed properly, it will be copied to
cl.frame and saved in cl.frames[].  If the snapshot is invalid
for any reason, no changes to the state will be made at all.
================
*/
void CL_ParseSnapshot(msg_t* msg)
{
	clSnapshot_t* old;
	clSnapshot_t new_snap;

	// get the reliable sequence acknowledge number
	clc.reliableAcknowledge = MSG_ReadLong(msg);

	// read in the new snapshot to a temporary buffer
	// we will only copy to cl.frame if it is valid
	memset(&new_snap, 0, sizeof new_snap);

	new_snap.serverCommandNum = clc.serverCommandSequence;

	new_snap.serverTime = MSG_ReadLong(msg);

	// if we were just unpaused, we can only *now* really let the
	// change come into effect or the client hangs.
	cl_paused->modified = qfalse;

	new_snap.messageNum = MSG_ReadLong(msg);
	const int delta_num = MSG_ReadByte(msg);
	if (!delta_num)
	{
		new_snap.deltaNum = -1;
	}
	else
	{
		new_snap.deltaNum = new_snap.messageNum - delta_num;
	}
	new_snap.cmdNum = MSG_ReadLong(msg);
	new_snap.snapFlags = MSG_ReadByte(msg);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message
	if (new_snap.deltaNum <= 0)
	{
		new_snap.valid = qtrue; // uncompressed frame
		old = nullptr;
	}
	else
	{
		old = &cl.frames[new_snap.deltaNum & PACKET_MASK];
		if (!old->valid)
		{
			// should never happen
			Com_Printf("Delta from invalid frame (not supposed to happen!).\n");
		}
		else if (old->messageNum != new_snap.deltaNum)
		{
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_Printf("Delta frame too old.\n");
		}
		else if (cl.parseEntitiesNum - old->parseEntitiesNum > MAX_PARSE_ENTITIES)
		{
			Com_Printf("Delta parseEntitiesNum too old.\n");
		}
		else
		{
			new_snap.valid = qtrue; // valid delta parse
		}
	}

	// read areamask
	const int len = MSG_ReadByte(msg);
	MSG_ReadData(msg, &new_snap.areamask, len);

	// read playerinfo
	SHOWNET(msg, "playerstate");
	if (old)
	{
		MSG_ReadDeltaPlayerstate(msg, &old->ps, &new_snap.ps);
	}
	else
	{
		MSG_ReadDeltaPlayerstate(msg, nullptr, &new_snap.ps);
	}

	// read packet entities
	SHOWNET(msg, "packet entities");
	CL_ParsePacketEntities(msg, old, &new_snap);

	// if not valid, dump the entire thing now that it has
	// been properly read
	if (!new_snap.valid)
	{
		return;
	}

	// clear the valid flags of any snapshots between the last
	// received and this one
	int oldMessageNum = cl.frame.messageNum + 1;

	if (cl.frame.messageNum - oldMessageNum >= PACKET_BACKUP)
	{
		oldMessageNum = cl.frame.messageNum - (PACKET_BACKUP - 1);
	}
	for (; oldMessageNum < new_snap.messageNum; oldMessageNum++)
	{
		cl.frames[oldMessageNum & PACKET_MASK].valid = qfalse;
	}

	// copy to the current good spot
	cl.frame = new_snap;

	// calculate ping time
	for (int i = 0; i < PACKET_BACKUP; i++)
	{
		const int packetNum = (clc.netchan.outgoingSequence - 1 - i) & PACKET_MASK;
		if (cl.frame.cmdNum == cl.packetCmdNumber[packetNum])
		{
			cl.frame.ping = cls.realtime - cl.packetTime[packetNum];
			break;
		}
	}
	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.messageNum & PACKET_MASK] = cl.frame;

	if (cl_shownet->integer == 3)
	{
		Com_Printf("   frame:%i  delta:%i\n", cl.frame.messageNum,
			cl.frame.deltaNum);
	}

	// actions for valid frames
	cl.newSnapshots = qtrue;
}

//=====================================================================

int cl_connectedToCheatServer;

/*
==================
CL_SystemInfoChanged

The systeminfo configstring has been changed, so parse
new information out of it.  This will happen at every
gamestate, and possibly during gameplay.
==================
*/
void CL_SystemInfoChanged()
{
	const char* systemInfo = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SYSTEMINFO];
	cl.serverId = atoi(Info_ValueForKey(systemInfo, "sv_serverid"));

	const char* s = Info_ValueForKey(systemInfo, "helpUsObi");
	cl_connectedToCheatServer = atoi(s);
	if (!cl_connectedToCheatServer)
	{
		Cvar_SetCheatState();
	}

	// scan through all the variables in the systeminfo and locally set cvars to match
	s = systemInfo;
	while (s)
	{
		char value[MAX_INFO_VALUE];
		char key[MAX_INFO_KEY];
		Info_NextPair(&s, key, value);
		if (!key[0])
		{
			break;
		}

		Cvar_Set(key, value);
	}
	//if ( Cvar_VariableIntegerValue("ui_iscensored") == 1 )
	//{
	//	Cvar_Set( "g_dismemberment", "0");
	//}
}

void UI_UpdateConnectionString(const char* string);

/*
==================
CL_ParseGamestate
==================
*/
void CL_ParseGamestate(msg_t* msg)
{
	Con_Close();

	UI_UpdateConnectionString("");

	// wipe local client state
	CL_ClearState();

	// a gamestate always marks a server command sequence
	clc.serverCommandSequence = MSG_ReadLong(msg);

	// parse all the configstrings and baselines
	cl.gameState.dataCount = 1; // leave a 0 at the beginning for uninitialized configstrings
	while (true)
	{
		const int cmd = MSG_ReadByte(msg);

		if (cmd <= 0)
		{
			break;
		}

		if (cmd == svc_configstring)
		{
			const int i = MSG_ReadShort(msg);
			if (i < 0 || i >= MAX_CONFIGSTRINGS)
			{
				Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
			}
			char* s = MSG_ReadString(msg);
			const int len = strlen(s);

			if (len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS)
			{
				Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded");
			}

			// append it to the gameState string buffer
			cl.gameState.stringOffsets[i] = cl.gameState.dataCount;
			memcpy(cl.gameState.stringData + cl.gameState.dataCount, s, len + 1);
			cl.gameState.dataCount += len + 1;
			if (cl_shownet->integer == 3)
			{
				Com_Printf("%3i:  CS# %d %s (%d)\n", msg->readcount, i, s, len);
			}
		}
		else if (cmd == svc_baseline)
		{
			assert(0);
		}
		else
		{
			Com_Error(ERR_DROP, "CL_ParseGamestate: bad command byte");
		}
	}

	// parse serverId and other cvars
	CL_SystemInfoChanged();

	// reinitialize the filesystem if the game directory has changed
#if 0
	if (fs_game->modified) {
	}
#endif

	// let the client game init and load data
	cls.state = CA_LOADING;

	CL_StartHunkUsers();

	// make sure the game starts
	Cvar_Set("cl_paused", "0");
}

//=====================================================================

void CL_FreeServerCommands()
{
	for (int i = 0; i < MAX_RELIABLE_COMMANDS; i++)
	{
		if (clc.serverCommands[i])
		{
			Z_Free(clc.serverCommands[i]);
			clc.serverCommands[i] = nullptr;
		}
	}
}

/*
=====================
CL_ParseCommandString

Command strings are just saved off until cgame asks for them
when it transitions a snapshot
=====================
*/
void CL_ParseCommandString(msg_t* msg)
{
	const int seq = MSG_ReadLong(msg);
	const char* s = MSG_ReadString(msg);

	// see if we have already executed stored it off
	if (clc.serverCommandSequence >= seq)
	{
		return;
	}
	clc.serverCommandSequence = seq;

	const int index = seq & (MAX_RELIABLE_COMMANDS - 1);
	if (clc.serverCommands[index])
	{
		Z_Free(clc.serverCommands[index]);
	}
	clc.serverCommands[index] = CopyString(s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage(msg_t* msg)
{
	if (cl_shownet->integer == 1)
	{
		Com_Printf("%i ", msg->cursize);
	}
	else if (cl_shownet->integer >= 2)
	{
		Com_Printf("------------------\n");
	}

	//
	// parse the message
	//
	while (true)
	{
		if (msg->readcount > msg->cursize)
		{
			Com_Error(ERR_DROP, "CL_ParseServerMessage: read past end of server message");
		}

		const int cmd = MSG_ReadByte(msg);

		if (cmd == -1)
		{
			SHOWNET(msg, "END OF MESSAGE");
			break;
		}

		if (cl_shownet->integer >= 2)
		{
			if (!svc_strings[cmd])
			{
				Com_Printf("%3i:BAD CMD %i\n", msg->readcount - 1, cmd);
			}
			else
			{
				SHOWNET(msg, svc_strings[cmd]);
			}
		}

		// other commands
		switch (cmd)
		{
		default:
			Com_Error(ERR_DROP, "CL_ParseServerMessage: Illegible server message\n");
		case svc_nop:
			break;
		case svc_serverCommand:
			CL_ParseCommandString(msg);
			break;
		case svc_gamestate:
			CL_ParseGamestate(msg);
			break;
		case svc_snapshot:
			CL_ParseSnapshot(msg);
			break;
		}
	}
}