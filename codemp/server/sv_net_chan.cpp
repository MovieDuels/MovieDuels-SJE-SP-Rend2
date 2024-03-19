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

#include "server.h"

// TTimo: unused, commenting out to make gcc happy
#if 1
/*
==============
SV_Netchan_Encode

	// first four bytes of the data are always:
	long reliableAcknowledge;

==============
*/
static void SV_Netchan_Encode(client_t* client, msg_t* msg) {
	if (msg->cursize < SV_ENCODE_START) {
		return;
	}

	const int srdc = msg->readcount;
	const int sbit = msg->bit;
	const qboolean soob = msg->oob;

	msg->bit = 0;
	msg->readcount = 0;
	msg->oob = qfalse;

	/*reliableAcknowledge =*/ MSG_ReadLong(msg);

	msg->oob = soob;
	msg->bit = sbit;
	msg->readcount = srdc;

	const byte* string = (byte*)client->lastClientCommandString;
	long index = 0;
	// xor the client challenge with the netchan sequence number
	byte key = client->challenge ^ client->netchan.outgoingSequence;
	for (long i = SV_ENCODE_START; i < msg->cursize; i++) {
		// modify the key with the last received and with this message acknowledged client command
		if (!string[index])
			index = 0;
		if (/*string[index] > 127 ||*/	// eurofix: remove this so we can chat in european languages...	-ste
			string[index] == '%')
		{
			key ^= '.' << (i & 1);
		}
		else {
			key ^= string[index] << (i & 1);
		}
		index++;
		// encode the data with this key
		*(msg->data + i) = *(msg->data + i) ^ key;
	}
}

/*
==============
SV_Netchan_Decode

	// first 12 bytes of the data are always:
	long serverId;
	long messageAcknowledge;
	long reliableAcknowledge;

==============
*/
static void SV_Netchan_Decode(client_t* client, msg_t* msg) {
	const int srdc = msg->readcount;
	const int sbit = msg->bit;
	const qboolean soob = msg->oob;

	msg->oob = qfalse;

	const int serverId = MSG_ReadLong(msg);
	const int messageAcknowledge = MSG_ReadLong(msg);
	const int reliableAcknowledge = MSG_ReadLong(msg);

	msg->oob = soob;
	msg->bit = sbit;
	msg->readcount = srdc;

	const byte* string = (byte*)client->reliableCommands[reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1)];
	int index = 0;
	//
	byte key = client->challenge ^ serverId ^ messageAcknowledge;
	for (int i = msg->readcount + SV_DECODE_START; i < msg->cursize; i++) {
		// modify the key with the last sent and acknowledged server command
		if (!string[index])
			index = 0;
		if (/*string[index] > 127 || */	// eurofix: remove this so we can chat in european languages...	-ste
			string[index] == '%')
		{
			key ^= '.' << (i & 1);
		}
		else {
			key ^= string[index] << (i & 1);
		}
		index++;
		// decode the data with this key
		*(msg->data + i) = *(msg->data + i) ^ key;
	}
}
#endif

/*
=================
SV_Netchan_TransmitNextFragment
=================
*/
void SV_Netchan_TransmitNextFragment(netchan_t* chan) {
	Netchan_TransmitNextFragment(chan);
}

/*
===============
SV_Netchan_Transmit
================
*/

void SV_Netchan_Transmit(client_t* client, msg_t* msg) {	//int length, const byte *data ) {
	//	int i;
	MSG_WriteByte(msg, svc_EOF);
	//	for(i=SV_ENCODE_START;i<msg->cursize;i++) {
	//		chksum[i-SV_ENCODE_START] = msg->data[i];
	//	}
	//	Huff_Compress( msg, SV_ENCODE_START );
	SV_Netchan_Encode(client, msg);
	Netchan_Transmit(&client->netchan, msg->cursize, msg->data);
}

/*
=================
Netchan_SV_Process
=================
*/
qboolean SV_Netchan_Process(client_t* client, msg_t* msg) {
	//	int i;
	const int ret = Netchan_Process(&client->netchan, msg);
	if (!ret)
		return qfalse;
	SV_Netchan_Decode(client, msg);
	//	Huff_Decompress( msg, SV_DECODE_START );
	//	for(i=SV_DECODE_START+msg->readcount;i<msg->cursize;i++) {
	//		if (msg->data[i] != chksum[i-(SV_DECODE_START+msg->readcount)]) {
	//			Com_Error(ERR_DROP,"bad\n");
	//		}
	//	}
	return qtrue;
}