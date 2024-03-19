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

// sv_client.c -- server code for dealing with clients

#include "../server/exe_headers.h"

#include "server.h"

/*
==================
SV_DirectConnect

A "connect" OOB command has been received
==================
*/
void SV_DirectConnect(const netadr_t from)
{
	char userinfo[MAX_INFO_STRING];
	int i;
	client_t* cl;
	client_t temp{};

	Com_DPrintf("SVC_DirectConnect ()\n");

	Q_strncpyz(userinfo, Cmd_Argv(1), sizeof(userinfo));

	const int version = atoi(Info_ValueForKey(userinfo, "protocol"));
	if (version != PROTOCOL_VERSION)
	{
		NET_OutOfBandPrint(NS_SERVER, from, "print\nServer uses protocol version %i.\n", PROTOCOL_VERSION);
		Com_DPrintf("    rejected connect from version %i\n", version);
		return;
	}

	const int qport = atoi(Info_ValueForKey(userinfo, "qport"));

	//challenge = atoi( Info_ValueForKey( userinfo, "challenge" ) );

	// see if the challenge is valid (local clients don't need to challenge)
	if (!NET_IsLocalAddress(from))
	{
		NET_OutOfBandPrint(NS_SERVER, from, "print\nNo challenge for address.\n");
		return;
	}
	// force the "ip" info key to "localhost"
	Info_SetValueForKey(userinfo, "ip", "localhost");

	client_t* newcl = &temp;
	memset(newcl, 0, sizeof(client_t));

	// if there is already a slot for this ip, reuse it
	for (i = 0, cl = svs.clients; i < 1; i++, cl++)
	{
		if (cl->state == CS_FREE)
		{
			continue;
		}
		if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress)
			&& (cl->netchan.qport == qport
				|| from.port == cl->netchan.remoteAddress.port))
		{
			if ((sv.time - cl->lastConnectTime)
				< (sv_reconnectlimit->integer * 1000))
			{
				Com_DPrintf("%s:reconnect rejected : too soon\n", NET_AdrToString(from));
				return;
			}
			Com_Printf("%s:reconnect\n", NET_AdrToString(from));
			newcl = cl;
			goto gotnewcl;
		}
	}

	newcl = nullptr;
	for (i = 0; i < 1; i++)
	{
		cl = &svs.clients[i];
		if (cl->state == CS_FREE)
		{
			newcl = cl;
			break;
		}
	}

	if (!newcl)
	{
		NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is full.\n");
		Com_DPrintf("Rejected a connection.\n");
		return;
	}

gotnewcl:
	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	*newcl = temp;
	const int client_num = newcl - svs.clients;
	gentity_t* ent = SV_GentityNum(client_num);
	newcl->gentity = ent;

	// save the address
	Netchan_Setup(NS_SERVER, &newcl->netchan, from, qport);

	// save the userinfo
	Q_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));

	// get the game a chance to reject this connection or modify the userinfo
	char* denied = ge->ClientConnect(client_num, qtrue, e_saved_game_just_loaded); // firstTime = qtrue
	if (denied)
	{
		NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", denied);
		Com_DPrintf("Game rejected a connection: %s.\n", denied);
		return;
	}

	SV_UserinfoChanged(newcl);

	// send the connect packet to the client
	NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");

	newcl->state = CS_CONNECTED;
	newcl->lastPacketTime = sv.time;
	newcl->lastConnectTime = sv.time;

	// when we receive the first packet from the client, we will
	// notice that it is from a different serverid and that the
	// gamestate message was not just sent, forcing a retransmit
	newcl->gamestateMessageNum = -1;
}

/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing -- SV_FinalMessage() will handle that
=====================
*/
void SV_DropClient(client_t* drop, const char* reason)
{
	if (drop->state == CS_ZOMBIE)
	{
		return; // already dropped
	}
	drop->state = CS_ZOMBIE; // become free in a few seconds

	if (drop->download)
	{
		FS_FreeFile(drop->download);
		drop->download = nullptr;
	}

	// call the prog function for removing a client
	// this will remove the body, among other things
	ge->ClientDisconnect(drop - svs.clients);

	// tell everyone why they got dropped
	SV_SendServerCommand(nullptr, "print \"%s %s\n\"", drop->name, reason);

	// add the disconnect command
	SV_SendServerCommand(drop, "disconnect");
}

/*
================
SV_SendClientGameState

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each new map load.

It will be resent if the client acknowledges a later message but has
the wrong gamestate.
================
*/
void SV_SendClientGameState(client_t* client)
{
	msg_t msg;
	byte msg_buffer[MAX_MSGLEN];

	Com_DPrintf("SV_SendGameState() for %s\n", client->name);
	client->state = CS_PRIMED;

	// when we receive the first packet from the client, we will
	// notice that it is from a different serverid and that the
	// gamestate message was not just sent, forcing a retransmit
	client->gamestateMessageNum = client->netchan.outgoingSequence;

	// clear the reliable message list for this client
	client->reliableSequence = 0;
	client->reliableAcknowledge = 0;

	MSG_Init(&msg, msg_buffer, sizeof(msg_buffer));

	// send the gamestate
	MSG_WriteByte(&msg, svc_gamestate);
	MSG_WriteLong(&msg, client->reliableSequence);

	// write the configstrings
	for (int start = 0; start < MAX_CONFIGSTRINGS; start++)
	{
		if (sv.configstrings[start][0])
		{
			MSG_WriteByte(&msg, svc_configstring);
			MSG_WriteShort(&msg, start);
			MSG_WriteString(&msg, sv.configstrings[start]);
		}
	}

	MSG_WriteByte(&msg, 0);

	// check for overflow
	if (msg.overflowed)
	{
		Com_Printf("WARNING: GameState overflowed for %s\n", client->name);
	}

	// deliver this to the client
	SV_SendMessageToClient(&msg, client);
}

/*
==================
SV_ClientEnterWorld
==================
*/
void SV_ClientEnterWorld(client_t* client, const usercmd_t* cmd, const SavedGameJustLoaded_e e_saved_game_just_loaded)
{
	Com_DPrintf("SV_ClientEnterWorld() from %s\n", client->name);
	client->state = CS_ACTIVE;

	// set up the entity for the client
	const int client_num = client - svs.clients;
	gentity_t* ent = SV_GentityNum(client_num);
	ent->s.number = client_num;
	client->gentity = ent;

	// normally I check 'qbFromSavedGame' to avoid overwriting loaded client data, but this stuff I want
	//	to be reset so that client packet delta-ing bgins afresh, rather than based on your previous frame
	//	(which didn't in fact happen now if we've just loaded from a saved game...)
	//
	client->deltaMessage = -1;
	client->cmdNum = 0;

	// call the game begin function
	ge->ClientBegin(client - svs.clients, cmd, e_saved_game_just_loaded);
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/

/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately  FIXME: move to game?
=================
*/
static void SV_Disconnect_f(client_t* cl)
{
	SV_DropClient(cl, "disconnected");
}

/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string into a more C friendly form.
=================
*/
void SV_UserinfoChanged(client_t* cl)
{
	Q_strncpyz(cl->name, Info_ValueForKey(cl->userinfo, "name"), sizeof(cl->name));
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
static void SV_UpdateUserinfo_f(client_t* cl)
{
	Q_strncpyz(cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo));

	SV_UserinfoChanged(cl);
	// call prog code to allow overrides
	ge->client_userinfo_changed(cl - svs.clients);
}

using ucmd_t = struct
{
	const char* name;
	void (*func)(client_t* cl);
};

static ucmd_t ucmds[] = {
	{"userinfo", SV_UpdateUserinfo_f},
	{"disconnect", SV_Disconnect_f},

	{nullptr, nullptr}
};

/*
==================
SV_ExecuteClientCommand
==================
*/
void SV_ExecuteClientCommand(client_t* cl, const char* s)
{
	ucmd_t* u;

	Cmd_TokenizeString(s);

	// see if it is a server level command
	for (u = ucmds; u->name; u++)
	{
		if (strcmp(Cmd_Argv(0), u->name) == 0)
		{
			u->func(cl);
			break;
		}
	}

	// pass unknown strings to the game
	if (!u->name && sv.state == SS_GAME)
	{
		ge->ClientCommand(cl - svs.clients);
	}
}

constexpr auto MAX_STRINGCMDS = 8;

/*
===============
SV_ClientCommand
===============
*/
static void SV_ClientCommand(client_t* cl, msg_t* msg)
{
	const int seq = MSG_ReadLong(msg);
	const char* s = MSG_ReadString(msg);

	// see if we have already executed it
	if (cl->lastClientCommand >= seq)
	{
		return;
	}

	Com_DPrintf("clientCommand: %s : %i : %s\n", cl->name, seq, s);

	// drop the connection if we have somehow lost commands
	if (seq > cl->lastClientCommand + 1)
	{
		Com_Printf("Client %s lost %i clientCommands\n", cl->name,
			seq - cl->lastClientCommand + 1);
	}

	SV_ExecuteClientCommand(cl, s);

	cl->lastClientCommand = seq;
}

//==================================================================================

/*
==================
SV_ClientThink
==================
*/
void SV_ClientThink(client_t* cl, usercmd_t* cmd)
{
	cl->lastUsercmd = *cmd;

	if (cl->state != CS_ACTIVE)
	{
		return; // may have been kicked during the last usercmd
	}

	ge->ClientThink(cl - svs.clients, cmd);
}

/*
==================
SV_UserMove

The message usually contains all the movement commands
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_UserMove(client_t* cl, msg_t* msg)
{
	int i;
	usercmd_t nullcmd;
	usercmd_t cmds[MAX_PACKET_USERCMDS]{};

	cl->reliableAcknowledge = MSG_ReadLong(msg);
	const int serverId = MSG_ReadLong(msg);
	/*clientTime = */
	MSG_ReadLong(msg);
	cl->deltaMessage = MSG_ReadLong(msg);

	// cmdNum is the command number of the most recent included usercmd
	const int cmdNum = MSG_ReadLong(msg);
	const int cmdCount = MSG_ReadByte(msg);

	if (cmdCount < 1)
	{
		Com_Printf("cmdCount < 1\n");
		return;
	}

	if (cmdCount > MAX_PACKET_USERCMDS)
	{
		Com_Printf("cmdCount > MAX_PACKET_USERCMDS\n");
		return;
	}

	memset(&nullcmd, 0, sizeof(nullcmd));
	const usercmd_t* oldcmd = &nullcmd;
	for (i = 0; i < cmdCount; i++)
	{
		usercmd_t* cmd = &cmds[i];
		MSG_ReadDeltaUsercmd(msg, oldcmd, cmd);
		oldcmd = cmd;
	}

	// if this is a usercmd from a previous gamestate,
	// ignore it or retransmit the current gamestate
	if (serverId != sv.serverId)
	{
		// if we can tell that the client has dropped the last
		// gamestate we sent them, resend it
		if (cl->netchan.incomingAcknowledged > cl->gamestateMessageNum)
		{
			Com_DPrintf("%s : dropped gamestate, resending\n", cl->name);
			SV_SendClientGameState(cl);
		}
		return;
	}

	// if this is the first usercmd we have received
	// this gamestate, put the client into the world
	if (cl->state == CS_PRIMED)
	{
		SV_ClientEnterWorld(cl, &cmds[0], e_saved_game_just_loaded);
		if (sv_mapname->string[0] != '_')
		{
			char savename[MAX_QPATH];
			if (e_saved_game_just_loaded == eNO)
			{
				SG_WriteSavegame("auto", qtrue);
				if (Q_stricmpn(sv_mapname->string, "academy", 7) != 0)
				{
					Com_sprintf(savename, sizeof(savename), "auto_%s", sv_mapname->string);
					SG_WriteSavegame(savename, qtrue); //can't use va becuase it's nested
				}
			}
			else if (qbLoadTransition == qtrue)
			{
				Com_sprintf(savename, sizeof(savename), "hub/%s", sv_mapname->string);
				SG_WriteSavegame(savename, qfalse); //save a full one
				SG_WriteSavegame("auto", qfalse); //need a copy for auto, too
			}
		}
		e_saved_game_just_loaded = eNO;
		// the moves can be processed normaly
	}

	if (cl->state != CS_ACTIVE)
	{
		cl->deltaMessage = -1;
		return;
	}

	// if there is a time gap from the last packet to this packet,
	// fill in with the first command in the packet

	// with a packetdup of 0, firstNum == cmdNum
	const int firstNum = cmdNum - (cmdCount - 1);
	if (cl->cmdNum < firstNum - 1)
	{
		cl->droppedCommands = qtrue;
		if (sv_showloss->integer)
		{
			Com_Printf("Lost %i usercmds from %s\n", firstNum - 1 - cl->cmdNum,
				cl->name);
		}
		if (cl->cmdNum < firstNum - 6)
		{
			cl->cmdNum = firstNum - 6; // don't generate too many
		}
		while (cl->cmdNum < firstNum - 1)
		{
			cl->cmdNum++;
			SV_ClientThink(cl, &cmds[0]);
		}
	}
	// skip over any usercmd_t we have already executed
	const int start = cl->cmdNum - (firstNum - 1);
	for (i = start; i < cmdCount; i++)
	{
		SV_ClientThink(cl, &cmds[i]);
	}
	cl->cmdNum = cmdNum;
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void SV_ExecuteClientMessage(client_t* cl, msg_t* msg)
{
	while (true)
	{
		if (msg->readcount > msg->cursize)
		{
			SV_DropClient(cl, "had a badread");
			return;
		}

		const int c = MSG_ReadByte(msg);
		if (c == -1)
		{
			break;
		}

		switch (c)
		{
		default:
			SV_DropClient(cl, "had an unknown command char");
			return;

		case clc_nop:
			break;

		case clc_move:
			SV_UserMove(cl, msg);
			break;

		case clc_clientCommand:
			SV_ClientCommand(cl, msg);
			if (cl->state == CS_ZOMBIE)
			{
				return; // disconnect command
			}
			break;
		}
	}
}

void SV_FreeClient(client_t* client)
{
	if (!client) return;

	for (int i = 0; i < MAX_RELIABLE_COMMANDS; i++)
	{
		if (client->reliableCommands[i])
		{
			Z_Free(client->reliableCommands[i]);
			client->reliableCommands[i] = nullptr;
			client->reliableSequence = 0;
		}
	}
}