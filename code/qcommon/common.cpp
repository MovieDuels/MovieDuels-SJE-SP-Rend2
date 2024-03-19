/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// common.c -- misc functions used in client and server

#include "q_shared.h"
#include "qcommon.h"
#include "sstring.h"	// to get Gil's string class, because MS's doesn't compile properly in here
#include "stringed_ingame.h"
#include "stv_version.h"
#include "../shared/sys/sys_local.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Because renderer.
#include "../rd-common/tr_public.h"
extern refexport_t re;

static fileHandle_t logfile;
static fileHandle_t speedslog;
static fileHandle_t camerafile;
fileHandle_t com_journalFile;
fileHandle_t com_journalDataFile; // config files are written here

cvar_t* com_speeds;
cvar_t* com_developer;
cvar_t* com_timescale;
cvar_t* com_fixedtime;
cvar_t* com_sv_running;
cvar_t* com_cl_running;
cvar_t* com_logfile; // 1 = buffer log, 2 = flush after each print
cvar_t* com_showtrace;
cvar_t* com_version;
cvar_t* com_buildScript; // for automated data building scripts
cvar_t* com_bootlogo;
cvar_t* cl_paused;
cvar_t* sv_paused;
cvar_t* com_skippingcin;
cvar_t* com_speedslog; // 1 = buffer log, 2 = flush after each print
cvar_t* com_homepath;
cvar_t* com_outcast;
cvar_t* g_update6firststartup;
cvar_t* g_totgfirststartup;
cvar_t* g_Weather;
cvar_t* com_kotor;
cvar_t* g_trueguns;
cvar_t* g_spskill;
cvar_t* debugNPCFreeze;
cvar_t* com_rend2;

#ifndef _WIN32
cvar_t* com_ansiColor = nullptr;
#endif
cvar_t* com_busyWait;

#ifdef G2_PERFORMANCE_ANALYSIS
cvar_t* com_G2Report;
#endif

cvar_t* com_affinity;

// com_speeds times
int time_game;
int time_frontend; // renderer frontend time
int time_backend; // renderer backend time

int timeInTrace;
int timeInPVSCheck;
int numTraces;

int com_frameTime;
int com_frameNumber = 0;

qboolean com_errorEntered = qfalse;
qboolean com_fullyInitialized = qfalse;

char com_errorMessage[MAXPRINTMSG] = { 0 };

void Com_WriteConfig_f();
//JLF

//============================================================================

static char* rd_buffer;
static int rd_buffersize;
static void (*rd_flush)(char* buffer);

void Com_BeginRedirect(char* buffer, const int buffersize, void (*flush)(char*))
{
	if (!buffer || !buffersize || !flush)
		return;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect()
{
	if (rd_flush)
	{
		rd_flush(rd_buffer);
	}

	rd_buffer = nullptr;
	rd_buffersize = 0;
	rd_flush = nullptr;
}
#if !defined(FINAL_BUILD) && defined(_WIN32)
#define OUTPUT_TO_BUILD_WINDOW
#endif

/*
=============
Com_Printf

Both client and server can use this, and it will output
to the apropriate place.

A raw string should NEVER be passed as fmt, because of "%f" type crashers.
=============
*/
void QDECL Com_Printf(const char* fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (rd_buffer)
	{
		if ((strlen(msg) + strlen(rd_buffer)) > static_cast<unsigned>(rd_buffersize - 1))
		{
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, strlen(rd_buffer), msg);
		return;
	}

	CL_ConsolePrint(msg);

	// echo to dedicated console and early console
	Sys_Print(msg);

#ifdef OUTPUT_TO_BUILD_WINDOW
	OutputDebugString(msg);
#endif

	// logfile
	if (com_logfile && com_logfile->integer)
	{
		if (!logfile && FS_Initialized())
		{
			logfile = FS_FOpenFileWrite("qconsole.log");
			if (com_logfile->integer > 1)
			{
				// force it to not buffer so we get valid
				// data even if we are crashing
				FS_ForceFlush(logfile);
			}
		}
		if (logfile)
		{
			FS_Write(msg, strlen(msg), logfile);
		}
	}
}

/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void QDECL Com_DPrintf(const char* fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!com_developer || !com_developer->integer)
	{
		return; // don't confuse non-developers with techie stuff...
	}

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("%s", msg);
}

void Com_WriteCam(const char* text)
{
	static char mapname[MAX_QPATH];
	// camerafile
	if (!camerafile)
	{
		extern cvar_t* sv_mapname;

		//NOTE: always saves in working dir if using one...
		Com_sprintf(mapname, MAX_QPATH, "maps/%s_cam.map", sv_mapname->string);
		camerafile = FS_FOpenFileWrite(mapname);
	}

	if (camerafile)
	{
		FS_Printf(camerafile, "%s", text);
	}

	Com_Printf("%s\n", mapname);
}

void Com_FlushCamFile()
{
	if (!camerafile)
	{
		// nothing to flush, right?
		Com_Printf("No cam file available\n");
		return;
	}
	FS_ForceFlush(camerafile);
	FS_FCloseFile(camerafile);
	camerafile = 0;

	static char flushedMapname[MAX_QPATH];
	extern cvar_t* sv_mapname;
	Com_sprintf(flushedMapname, MAX_QPATH, "maps/%s_cam.map", sv_mapname->string);
	Com_Printf("flushed all cams to %s\n", flushedMapname);
}

/*
=============
Com_Error

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void SG_Shutdown();
#ifdef JK2_MODE
extern void SCR_UnprecacheScreenshot();
#endif
void NORETURN QDECL Com_Error(int level, const char* fmt, ...)
{
	va_list argptr;
	static int lastErrorTime;
	static int errorCount;

	if (com_errorEntered)
	{
		Sys_Error("recursive error after: %s", com_errorMessage);
	}
	com_errorEntered = qtrue;

	// when we are running automated scripts, make sure we
	// know if anything failed
	if (com_buildScript && com_buildScript->integer)
	{
		level = ERR_FATAL;
	}

	// if we are getting a solid stream of ERR_DROP, do an ERR_FATAL
	const int current_time = Sys_Milliseconds();
	if (current_time - lastErrorTime < 100)
	{
		if (++errorCount > 3)
		{
			level = ERR_FATAL;
		}
	}
	else
	{
		errorCount = 0;
	}
	lastErrorTime = current_time;

#ifdef JK2_MODE
	SCR_UnprecacheScreenshot();
#endif

	va_start(argptr, fmt);
	Q_vsnprintf(com_errorMessage, sizeof(com_errorMessage), fmt, argptr);
	va_end(argptr);

	if (level != ERR_DISCONNECT)
	{
		Cvar_Get("com_errorMessage", "", CVAR_ROM);
		//give com_errorMessage a default so it won't come back to life after a resetDefaults
		Cvar_Set("com_errorMessage", com_errorMessage);
	}

	SG_Shutdown(); // close any file pointers
	if (level == ERR_DISCONNECT || level == ERR_DROP)
	{
		throw level;
	}
	SV_Shutdown(va("Server fatal crashed: %s\n", com_errorMessage));
	CL_Shutdown();

	Com_Shutdown();

	Sys_Error("%s", com_errorMessage);
}

/*
=============
Com_Quit_f

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void NORETURN Com_Quit_f()
{
	// don't try to shutdown if we are in a recursive error
	if (!com_errorEntered)
	{
		SV_Shutdown("Server quit\n");
		CL_Shutdown();
		Com_Shutdown();
		FS_Shutdown();
	}
	Sys_Quit();
}

/*
============================================================================

COMMAND LINE FUNCTIONS

+ characters seperate the commandLine string into multiple console
command lines.

All of these are valid:

quake3 +set test blah +map test
quake3 set test blah+map test
quake3 set test blah + map test

============================================================================
*/

#define	MAX_CONSOLE_LINES	32
int com_numConsoleLines;
char* com_consoleLines[MAX_CONSOLE_LINES];

/*
==================
Com_ParseCommandLine

Break it up into multiple console lines
==================
*/
static void Com_ParseCommandLine(char* commandLine)
{
	int inq = 0;
	com_consoleLines[0] = commandLine;
	com_numConsoleLines = 1;

	while (*commandLine)
	{
		if (*commandLine == '"')
		{
			inq = !inq;
		}
		// look for a + separating character
		// if commandLine came from a file, we might have real line seperators
		if ((*commandLine == '+' && !inq) || *commandLine == '\n' || *commandLine == '\r')
		{
			if (com_numConsoleLines == MAX_CONSOLE_LINES)
			{
				return;
			}
			com_consoleLines[com_numConsoleLines] = commandLine + 1;
			com_numConsoleLines++;
			*commandLine = 0;
		}
		commandLine++;
	}
}

/*
===================
Com_SafeMode

Check for "safe" on the command line, which will
skip loading of MovieDuels-SP.cfg
===================
*/
qboolean Com_SafeMode()
{
	for (int i = 0; i < com_numConsoleLines; i++)
	{
		Cmd_TokenizeString(com_consoleLines[i]);
		if (!Q_stricmp(Cmd_Argv(0), "safe")
			|| !Q_stricmp(Cmd_Argv(0), "cvar_restart"))
		{
			com_consoleLines[i][0] = 0;
			return qtrue;
		}
	}
	return qfalse;
}

/*
===============
Com_StartupVariable

Searches for command line parameters that are set commands.
If match is not NULL, only that cvar will be looked for.
That is necessary because cddir and basedir need to be set
before the filesystem is started, but all other sets should
be after execing the config and default.
===============
*/
void Com_StartupVariable(const char* match)
{
	for (int i = 0; i < com_numConsoleLines; i++)
	{
		Cmd_TokenizeString(com_consoleLines[i]);
		if (strcmp(Cmd_Argv(0), "set") != 0)
		{
			continue;
		}

		const char* s = Cmd_Argv(1);

		if (!match || strcmp(s, match) == 0)
		{
			if (static_cast<unsigned>(Cvar_Flags(s)) == CVAR_NONEXISTENT)
				Cvar_Get(s, Cmd_ArgsFrom(2), CVAR_USER_CREATED);
			else
				Cvar_Set2(s, Cmd_ArgsFrom(2), qfalse);
		}
	}
}

/*
=================
Com_AddStartupCommands

Adds command line parameters as script statements
Commands are seperated by + signs

Returns qtrue if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
static qboolean Com_AddStartupCommands()
{
	qboolean added = qfalse;
	// quote every token, so args with semicolons can work
	for (int i = 0; i < com_numConsoleLines; i++)
	{
		if (!com_consoleLines[i] || !com_consoleLines[i][0])
		{
			continue;
		}

		// set commands won't override menu startup
		if (Q_stricmpn(com_consoleLines[i], "set", 3))
		{
			added = qtrue;
		}
		Cbuf_AddText(com_consoleLines[i]);
		Cbuf_AddText("\n");
	}

	return added;
}

//============================================================================

void Info_Print(const char* s)
{
	char key[512]{};

	if (*s == '\\')
		s++;
	while (*s)
	{
		char value[512]{};
		char* o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		const int l = o - key;
		if (l < 20)
		{
			memset(o, ' ', 20 - l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf("%s", key);

		if (!*s)
		{
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf("%s\n", value);
	}
}

/*
============
Com_StringContains
============
*/
static const char* Com_StringContains(const char* str1, const char* str2, const int casesensitive)
{
	int j;

	const int len = strlen(str1) - strlen(str2);
	for (int i = 0; i <= len; i++, str1++)
	{
		for (j = 0; str2[j]; j++)
		{
			if (casesensitive)
			{
				if (str1[j] != str2[j])
				{
					break;
				}
			}
			else
			{
				if (toupper(str1[j]) != toupper(str2[j]))
				{
					break;
				}
			}
		}
		if (!str2[j])
		{
			return str1;
		}
	}
	return nullptr;
}

/*
============
Com_Filter
============
*/
int Com_Filter(const char* filter, const char* name, const int casesensitive)
{
	char buf[MAX_TOKEN_CHARS]{};
	int i;

	while (*filter)
	{
		if (*filter == '*')
		{
			filter++;
			for (i = 0; *filter; i++)
			{
				if (*filter == '*' || *filter == '?') break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (strlen(buf))
			{
				const char* ptr = Com_StringContains(name, buf, casesensitive);
				if (!ptr) return qfalse;
				name = ptr + strlen(buf);
			}
		}
		else if (*filter == '?')
		{
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter + 1) == '[')
		{
			filter++;
		}
		else if (*filter == '[')
		{
			filter++;
			int found = qfalse;
			while (*filter && !found)
			{
				if (*filter == ']' && *(filter + 1) != ']') break;
				if (*(filter + 1) == '-' && *(filter + 2) && (*(filter + 2) != ']' || *(filter + 3) == ']'))
				{
					if (casesensitive)
					{
						if (*name >= *filter && *name <= *(filter + 2)) found = qtrue;
					}
					else
					{
						if (toupper(*name) >= toupper(*filter) &&
							toupper(*name) <= toupper(*(filter + 2)))
							found = qtrue;
					}
					filter += 3;
				}
				else
				{
					if (casesensitive)
					{
						if (*filter == *name) found = qtrue;
					}
					else
					{
						if (toupper(*filter) == toupper(*name)) found = qtrue;
					}
					filter++;
				}
			}
			if (!found) return qfalse;
			while (*filter)
			{
				if (*filter == ']' && *(filter + 1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else
		{
			if (casesensitive)
			{
				if (*filter != *name) return qfalse;
			}
			else
			{
				if (toupper(*filter) != toupper(*name)) return qfalse;
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}

/*
============
Com_FilterPath
============
*/
int Com_FilterPath(const char* filter, const char* name, const int casesensitive)
{
	int i;
	char new_filter[MAX_QPATH]{};
	char new_name[MAX_QPATH]{};

	for (i = 0; i < MAX_QPATH - 1 && filter[i]; i++)
	{
		if (filter[i] == '\\' || filter[i] == ':')
		{
			new_filter[i] = '/';
		}
		else
		{
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for (i = 0; i < MAX_QPATH - 1 && name[i]; i++)
	{
		if (name[i] == '\\' || name[i] == ':')
		{
			new_name[i] = '/';
		}
		else
		{
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter(new_filter, new_name, casesensitive);
}

/*
=================
Com_InitHunkMemory
=================
*/
static void Com_InitHunkMemory()
{
	Hunk_Clear();

	//	Cmd_AddCommand( "meminfo", Z_Details_f );
}

// I'm leaving this in just in case we ever need to remember where's a good place to hook something like this in.
//
void Com_ShutdownHunkMemory()
{
}

/*
===================
Hunk_SetMark

The server calls this after the level and game VM have been loaded
===================
*/
void Hunk_SetMark()
{
}

/*
=================
Hunk_ClearToMark

The client calls this before starting a vid_restart or snd_restart
=================
*/
void Hunk_ClearToMark()
{
	Z_TagFree(TAG_HUNKALLOC);
	Z_TagFree(TAG_HUNKMISCMODELS);
}

/*
=================
Hunk_Clear

The server calls this before shutting down or loading a new map
=================
*/
void Hunk_Clear()
{
	Z_TagFree(TAG_HUNKALLOC);
	Z_TagFree(TAG_HUNKMISCMODELS);

	extern void CIN_CloseAllVideos();
	CIN_CloseAllVideos();

	if (re.R_ClearStuffToStopGhoul2CrashingThings)
		re.R_ClearStuffToStopGhoul2CrashingThings();
}

/*
===================================================================

EVENTS AND JOURNALING

In addition to these events, .cfg files are also copied to the
journaled file
===================================================================
*/

#define	MAX_PUSHED_EVENTS	64
int com_pushedEventsHead, com_pushedEventsTail;
sysEvent_t com_pushedEvents[MAX_PUSHED_EVENTS];

/*
=================
Com_GetRealEvent
=================
*/
static sysEvent_t Com_GetRealEvent()
{
	// get an event from the system
	const sysEvent_t ev = Sys_GetEvent();

	return ev;
}

/*
=================
Com_PushEvent
=================
*/
static void Com_PushEvent(const sysEvent_t* event)
{
	static int printedWarning;

	sysEvent_t* ev = &com_pushedEvents[com_pushedEventsHead & (MAX_PUSHED_EVENTS - 1)];

	if (com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS)
	{
		// don't print the warning constantly, or it can give time for more...
		if (!printedWarning)
		{
			printedWarning = qtrue;
			Com_Printf("WARNING: Com_PushEvent overflow\n");
		}

		if (ev->evPtr)
		{
			Z_Free(ev->evPtr);
		}
		com_pushedEventsTail++;
	}
	else
	{
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

/*
=================
Com_GetEvent
=================
*/
static sysEvent_t Com_GetEvent()
{
	if (com_pushedEventsHead > com_pushedEventsTail)
	{
		com_pushedEventsTail++;
		return com_pushedEvents[(com_pushedEventsTail - 1) & (MAX_PUSHED_EVENTS - 1)];
	}
	return Com_GetRealEvent();
}

/*
=================
Com_RunAndTimeServerPacket
=================
*/
void Com_RunAndTimeServerPacket(const netadr_t* evFrom, msg_t* buf)
{
	int t1 = 0;

	if (com_speeds->integer)
	{
		t1 = Sys_Milliseconds();
	}

	SV_PacketEvent(*evFrom, buf);

	if (com_speeds->integer)
	{
		const int t2 = Sys_Milliseconds();
		const int msec = t2 - t1;
		if (com_speeds->integer == 3)
		{
			Com_Printf("SV_PacketEvent time: %i\n", msec);
		}
	}
}

/*
=================
Com_EventLoop

Returns last event time
=================
*/
int Com_EventLoop()
{
	netadr_t evFrom;
	byte bufData[MAX_MSGLEN];
	msg_t buf;

	MSG_Init(&buf, bufData, sizeof(bufData));

	while (true)
	{
		sysEvent_t ev = Com_GetEvent();

		// if no more events are available
		if (ev.evType == SE_NONE)
		{
			// manually send packet events for the loopback channel
			while (NET_GetLoopPacket(NS_CLIENT, &evFrom, &buf))
			{
				CL_PacketEvent(evFrom, &buf);
			}

			while (NET_GetLoopPacket(NS_SERVER, &evFrom, &buf))
			{
				// if the server just shut down, flush the events
				if (com_sv_running->integer)
				{
					Com_RunAndTimeServerPacket(&evFrom, &buf);
				}
			}

			return ev.evTime;
		}

		switch (ev.evType)
		{
		default:
			Com_Error(ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType);
		case SE_NONE:
			break;
		case SE_KEY:
			CL_KeyEvent(ev.evValue, static_cast<qboolean>(ev.evValue2), ev.evTime);
			break;
		case SE_CHAR:
			CL_CharEvent(ev.evValue);
			break;
		case SE_MOUSE:
			CL_MouseEvent(ev.evValue, ev.evValue2, ev.evTime);
			break;
		case SE_JOYSTICK_AXIS:
			CL_JoystickEvent(ev.evValue, ev.evValue2, ev.evTime);
			break;
		case SE_CONSOLE:
			Cbuf_AddText(static_cast<char*>(ev.evPtr));
			Cbuf_AddText("\n");
			break;
		}

		// free any block data
		if (ev.evPtr)
		{
			Z_Free(ev.evPtr);
		}
	}
}

/*
================
Com_Milliseconds

Can be used for profiling, but will be journaled accurately
================
*/
int Com_Milliseconds()
{
	sysEvent_t ev;

	// get events and push them until we get a null event with the current time
	do
	{
		ev = Com_GetRealEvent();
		if (ev.evType != SE_NONE)
		{
			Com_PushEvent(&ev);
		}
	} while (ev.evType != SE_NONE);

	return ev.evTime;
}

//============================================================================

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
static void NORETURN Com_Error_f()
{
	if (Cmd_Argc() > 1)
	{
		Com_Error(ERR_DROP, "Testing drop error");
	}
	Com_Error(ERR_FATAL, "Testing fatal error");
}

/*
=============
Com_Freeze_f

Just freeze in place for a given number of seconds to test
error recovery
=============
*/
static void Com_Freeze_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("freeze <seconds>\n");
		return;
	}
	const float s = atof(Cmd_Argv(1));

	const int start = Com_Milliseconds();

	while (true)
	{
		const int now = Com_Milliseconds();
		if ((now - start) * 0.001 > s)
		{
			break;
		}
	}
}

/*
=================
Com_Crash_f

A way to force a bus error for development reasons
=================
*/
static void NORETURN Com_Crash_f()
{
	*static_cast<volatile int*>(nullptr) = 0x12345678;
	/* that should crash already, but to reassure the compiler: */
	abort();
}

/*
==================
Com_ExecuteCfg
==================
*/

static void Com_ExecuteCfg()
{
	Cbuf_ExecuteText(EXEC_NOW, "exec MD-SP-default.cfg\n");
	Cbuf_Execute(); // Always execute after exec to prevent text buffer overflowing

	if (!Com_SafeMode())
	{
		// skip the q3config.cfg and autoexec_sp.cfg if "safe" is on the command line
		Cbuf_ExecuteText(EXEC_NOW, "exec " Q3CONFIG_NAME "\n");
		Cbuf_Execute();
		Cbuf_ExecuteText(EXEC_NOW, "exec autoexec_sp.cfg\n");
		Cbuf_Execute();

		if (g_update6firststartup->integer > 0) //playing academy
		{
			//
		}

		if (g_totgfirststartup->integer > 0) //playing academy
		{
			//
		}
	}
}

/*
=================
Com_ErrorString
Error string for the given error code (from Com_Error).
=================
*/
static const char* Com_ErrorString(const int code)
{
	switch (code)
	{
	case ERR_DISCONNECT:
		return "DISCONNECTED";

	case ERR_DROP:
		return "DROPPED";

	default:
		return "UNKNOWN";
	}
}

/*
=================
Com_CatchError
Handles freeing up of resources when Com_Error is called.
=================
*/
void SG_WipeSavegame(const char* name); // pretty sucky, but that's how SoF did it...<g>
static void Com_CatchError(const int code)
{
	if (code == ERR_DISCONNECT)
	{
		SV_Shutdown("Server disconnected");
		CL_Disconnect();
		CL_FlushMemory();
		com_errorEntered = qfalse;
	}
	else if (code == ERR_DROP)
	{
		// If loading/saving caused the crash/error - delete the temp file
		SG_WipeSavegame("current"); // delete file

		Com_Printf("********************\n"
			"ERROR: %s\n"
			"********************\n", com_errorMessage);
		SV_Shutdown(va("Server crashed: %s\n", com_errorMessage));
		CL_Disconnect();
		CL_FlushMemory();
		com_errorEntered = qfalse;
	}
}

/*
=================
Com_Init
=================
*/
void Com_Init(char* commandLine)
{
	Com_Printf("%s %s %s\n", Q3_VERSION, PLATFORM_STRING, SOURCE_DATE);

	try
	{
		char* s;
		Com_InitZoneMemory();
		Cvar_Init();

		// prepare enough of the subsystems to handle
		// cvar and command buffer management
		Com_ParseCommandLine(commandLine);

		//Swap_Init ();
		Cbuf_Init();

		Com_InitZoneMemoryVars();
		Cmd_Init();

		// override anything from the config files with command line args
		Com_StartupVariable(nullptr);

		// done early so bind command exists
		CL_InitKeyCommands();

		com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT);

		FS_InitFilesystem(); //uses z_malloc
		//re.R_InitWorldEffects();   // this doesn't do much but I want to be sure certain variables are intialized.

		Com_ExecuteCfg();

		// override anything from the config files with command line args
		Com_StartupVariable(nullptr);

		// allocate the stack based hunk allocator
		Com_InitHunkMemory();

		// if any archived cvars are modified after this, we will trigger a writing
		// of the config file
		cvar_modifiedFlags &= ~CVAR_ARCHIVE;

		//
		// init commands and vars
		//
		Cmd_AddCommand("quit", Com_Quit_f);
		Cmd_AddCommand("writeconfig", Com_WriteConfig_f);

		com_developer = Cvar_Get("developer", "0", CVAR_TEMP);
		com_outcast = Cvar_Get("com_outcast", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
		com_kotor = Cvar_Get("com_kotor", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
		g_spskill = Cvar_Get("g_spskill", "2", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);
		debugNPCFreeze = Cvar_Get("d_npcfreeze", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

		g_trueguns = Cvar_Get("cg_trueguns", "0", 0);

		g_Weather = Cvar_Get("r_weather", "0", CVAR_ARCHIVE);

		g_update6firststartup = Cvar_Get("g_update6firststartup", "1", 0);

		g_totgfirststartup = Cvar_Get("g_totgfirststartup", "1", 0);

		com_logfile = Cvar_Get("logfile", "0", CVAR_TEMP);
		com_speedslog = Cvar_Get("speedslog", "0", CVAR_TEMP);

		com_timescale = Cvar_Get("timescale", "1", CVAR_CHEAT);
		com_fixedtime = Cvar_Get("fixedtime", "0", CVAR_CHEAT);
		com_showtrace = Cvar_Get("com_showtrace", "0", CVAR_CHEAT);
		com_speeds = Cvar_Get("com_speeds", "0", 0);

		com_rend2 = Cvar_Get("com_rend2", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

#ifdef G2_PERFORMANCE_ANALYSIS
		com_G2Report = Cvar_Get("com_G2Report", "0", 0);
#endif

		cl_paused = Cvar_Get("cl_paused", "0", CVAR_ROM);
		sv_paused = Cvar_Get("sv_paused", "0", CVAR_ROM);
		com_sv_running = Cvar_Get("sv_running", "0", CVAR_ROM);
		com_cl_running = Cvar_Get("cl_running", "0", CVAR_ROM);
		com_skippingcin = Cvar_Get("skippingCinematic", "0", CVAR_ROM);
		com_buildScript = Cvar_Get("com_buildScript", "0", 0);

		com_affinity = Cvar_Get("com_affinity", "0", CVAR_ARCHIVE_ND);
		com_busyWait = Cvar_Get("com_busyWait", "0", CVAR_ARCHIVE_ND);

		com_bootlogo = Cvar_Get("com_bootlogo", "1", CVAR_ARCHIVE_ND);

		if (com_developer && com_developer->integer)
		{
			Cmd_AddCommand("error", Com_Error_f);
			Cmd_AddCommand("crash", Com_Crash_f);
			Cmd_AddCommand("freeze", Com_Freeze_f);
		}

		s = va("%s %s %s", Q3_VERSION, PLATFORM_STRING, SOURCE_DATE);
		com_version = Cvar_Get("version", s, CVAR_ROM | CVAR_SERVERINFO);

#ifdef JK2_MODE
		JK2SP_Init();
		Com_Printf("Running Jedi Outcast Mode\n");
#else
		SE_Init(); // Initialize StringEd
		if (com_outcast->integer == 1) //playing outcast
		{
			Com_Printf("Running MovieDuels JKO Mode\n");
		}
		else
		{
			Com_Printf("Running MovieDuels JKA Mode\n");
		}

		Cvar_Set("com_kotor", "0");
		Cvar_Set("cg_trueguns", "0");
		Cvar_Set("d_npcfreeze", "0");
		Cvar_Set("g_ffamode", "0"); // MESSAGE TO MIKE//make sure its off when the mod starts

#endif

		Sys_Init(); // this also detects CPU type, so I can now do this CPU check below...

		Sys_SetProcessorAffinity();

		Netchan_Init(Com_Milliseconds() & 0xffff); // pick a port value that should be nice and random
		//	VM_Init();
		SV_Init();

		CL_Init();

		// set com_frameTime so that if a map is started on the
		// command line it will still be able to count on com_frameTime
		// being random enough for a serverid
		com_frameTime = Com_Milliseconds();

		// add + commands from command line
		if (!Com_AddStartupCommands())
		{
			// if the user didn't give any commands, run default action
			if (com_bootlogo->integer)
			{
				Cbuf_AddText("cinematic openinglogos\n");
			}
		}
		com_fullyInitialized = qtrue;
		Com_Printf("--- Common Initialization Complete ---\n");
	}
	catch (int code)
	{
		Com_CatchError(code);
		Sys_Error("Error during initialization %s", Com_ErrorString(code));
	}
}

//==================================================================

static void Com_WriteConfigToFile(const char* filename)
{
	const fileHandle_t f = FS_FOpenFileWrite(filename);
	if (!f)
	{
		Com_Printf("Couldn't write %s.\n", filename);
		return;
	}

	FS_Printf(f, "// Genuine MovieDuels SerenityJediEngine(Solaris Edition), do not modify\n");
	Key_WriteBindings(f);
	Cvar_WriteVariables(f);
	FS_FCloseFile(f);
}

/*
===============
Com_WriteConfiguration

Writes key bindings and archived cvars to config file if modified
===============
*/
static void Com_WriteConfiguration()
{
	// if we are quiting without fully initializing, make sure
	// we don't write out anything
	if (!com_fullyInitialized)
	{
		return;
	}

	if (!(cvar_modifiedFlags & CVAR_ARCHIVE))
	{
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile(Q3CONFIG_NAME);
}

/*
===============
Com_WriteConfig_f

Write the config file to a specific name
===============
*/
void Com_WriteConfig_f()
{
	char filename[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: writeconfig <filename>\n");
		return;
	}

	Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
	COM_DefaultExtension(filename, sizeof(filename), ".cfg");

	if (!COM_CompareExtension(filename, ".cfg"))
	{
		Com_Printf("Com_WriteConfig_f: Only the \".cfg\" extension is supported by this command!\n");
		return;
	}

	if (!FS_FilenameCompare(filename, "MD-SP-default.cfg") || !FS_FilenameCompare(filename, "MD-MP-default.cfg"))
	{
		Com_Printf(S_COLOR_YELLOW "Com_WriteConfig_f: The filename \"%s\" is reserved! Please choose another name.\n",
			filename);
		return;
	}

	Com_Printf("Writing %s.\n", filename);
	Com_WriteConfigToFile(filename);
}

/*
================
Com_ModifyMsec
================
*/

static int Com_ModifyMsec(int msec, float& fraction)
{
	int clampTime;

	fraction = 0.0f;

	//
	// modify time for debugging values
	//
	if (com_fixedtime->integer)
	{
		msec = com_fixedtime->integer;
	}
	else if (com_timescale->value)
	{
		fraction = static_cast<float>(msec);
		fraction *= com_timescale->value;
		msec = static_cast<int>(floor(fraction));
		fraction -= static_cast<float>(msec);
	}

	// don't let it scale below 1 msec
	if (msec < 1)
	{
		msec = 1;
		fraction = 0.0f;
	}

	if (com_skippingcin->integer)
	{
		// we're skipping ahead so let it go a bit faster
		clampTime = 500;
	}
	else
	{
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if (msec > clampTime)
	{
		msec = clampTime;
		fraction = 0.0f;
	}

	return msec;
}

/*
=================
Com_TimeVal
=================
*/

static int Com_TimeVal(const int minMsec)
{
	int timeVal = Sys_Milliseconds() - com_frameTime;

	if (timeVal >= minMsec)
		timeVal = 0;
	else
		timeVal = minMsec - timeVal;

	return timeVal;
}

/*
=================
Com_Frame
=================
*/
static vec3_t corg;
static vec3_t cangles;
static bool bComma;

void Com_SetOrgAngles(vec3_t org, vec3_t angles)
{
	VectorCopy(org, corg);
	VectorCopy(angles, cangles);
}

#ifdef G2_PERFORMANCE_ANALYSIS
void G2Time_ResetTimers(void);
void G2Time_ReportTimers(void);
#endif

void Com_Frame()
{
	try
	{
		int time_before_first_events = 0, time_before_server = 0, time_before_events = 0, time_before_client = 0, time_after = 0;
		int min_msec;
		static int last_time = 0, bias = 0;

		// write config file if anything changed
		Com_WriteConfiguration();

		//
		// main event loop
		//
		if (com_speeds->integer)
		{
			time_before_first_events = Sys_Milliseconds();
		}

		// Figure out how much time we have
		if (com_minimized->integer && com_maxfpsMinimized->integer > 0)
			min_msec = 1000 / com_maxfpsMinimized->integer;
		else if (com_unfocused->integer && com_maxfpsUnfocused->integer > 0)
			min_msec = 1000 / com_maxfpsUnfocused->integer;
		else if (com_maxfps->integer > 0)
			min_msec = 1000 / com_maxfps->integer;
		else
			min_msec = 1;

		int time_val = com_frameTime - last_time;
		bias += time_val - min_msec;

		if (bias > min_msec)
			bias = min_msec;

		// Adjust minMsec if previous frame took too long to render so
		// that framerate is stable at the requested value.
		min_msec -= bias;

		time_val = Com_TimeVal(min_msec);
		do
		{
			// Busy sleep the last millisecond for better timeout precision
			if (com_busyWait->integer || time_val < 1)
				Sys_Sleep(0);
			else
				Sys_Sleep(time_val - 1);
		} while ((time_val = Com_TimeVal(min_msec)) != 0);
		IN_Frame();

		last_time = com_frameTime;
		com_frameTime = Com_EventLoop();

		int msec = com_frameTime - last_time;

		Cbuf_Execute();

		// mess with msec if needed
		float fraction_msec = 0.0f;
		msec = Com_ModifyMsec(msec, fraction_msec);

		//
		// server side
		//
		if (com_speeds->integer)
		{
			time_before_server = Sys_Milliseconds();
		}

		SV_Frame(msec, fraction_msec);

		//
		// client system
		//

		//	if ( !com_dedicated->integer )
		{
			//
			// run event loop a second time to get server to client packets
			// without a frame of latency
			//
			if (com_speeds->integer)
			{
				time_before_events = Sys_Milliseconds();
			}
			Com_EventLoop();
			Cbuf_Execute();

			//
			// client side
			//
			if (com_speeds->integer)
			{
				time_before_client = Sys_Milliseconds();
			}

			CL_Frame(msec, fraction_msec);

			if (com_speeds->integer)
			{
				time_after = Sys_Milliseconds();
			}
		}

		//
		// report timing information
		//
		if (com_speeds->integer)
		{
			const int all = time_after - time_before_server;
			int sv = time_before_events - time_before_server;
			const int ev = time_before_server - time_before_first_events + time_before_client - time_before_events;
			int cl = time_after - time_before_client;
			sv -= time_game;
			cl -= time_frontend + time_backend;

			Com_Printf("fr:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i tr:%3i pvs:%3i rf:%3i bk:%3i\n",
				com_frameNumber, all, sv, ev, cl, time_game, timeInTrace, timeInPVSCheck, time_frontend,
				time_backend);

			// speedslog
			if (com_speedslog && com_speedslog->integer)
			{
				if (!speedslog)
				{
					speedslog = FS_FOpenFileWrite("speeds.log");
					FS_Write("data={\n", strlen("data={\n"), speedslog);
					bComma = false;
					if (com_speedslog->integer > 1)
					{
						// force it to not buffer so we get valid
						// data even if we are crashing
						FS_ForceFlush(logfile);
					}
				}
				if (speedslog)
				{
					char msg[MAXPRINTMSG];

					if (bComma)
					{
						FS_Write(",\n", strlen(",\n"), speedslog);
						bComma = false;
					}
					FS_Write("{", strlen("{"), speedslog);
					Com_sprintf(msg, sizeof(msg),
						"%8.4f,%8.4f,%8.4f,%8.4f,%8.4f,%8.4f,", corg[0], corg[1], corg[2], cangles[0],
						cangles[1], cangles[2]);
					FS_Write(msg, strlen(msg), speedslog);
					Com_sprintf(msg, sizeof(msg),
						"%i,%3i,%3i,%3i,%3i,%3i,%3i,%3i,%3i,%3i}",
						com_frameNumber, all, sv, ev, cl, time_game, timeInTrace, timeInPVSCheck, time_frontend,
						time_backend);
					FS_Write(msg, strlen(msg), speedslog);
					bComma = true;
				}
			}

			timeInTrace = timeInPVSCheck = 0;
		}

		//
		// trace optimization tracking
		//
		if (com_showtrace->integer)
		{
			extern int c_traces, c_brush_traces, c_patch_traces;
			extern int c_pointcontents;

			/*
			Com_Printf( "%4i non-sv_traces, %4i sv_traces, %4i ms, ave %4.2f ms\n", c_traces - numTraces, numTraces, timeInTrace, (float)timeInTrace/(float)numTraces );
			timeInTrace = numTraces = 0;
			c_traces = 0;
			*/

			Com_Printf("%4i traces  (%ib %ip) %4i points\n", c_traces,
				c_brush_traces, c_patch_traces, c_pointcontents);
			c_traces = 0;
			c_brush_traces = 0;
			c_patch_traces = 0;
			c_pointcontents = 0;
		}

		if (com_affinity->modified)
		{
			com_affinity->modified = qfalse;
			Sys_SetProcessorAffinity();
		}

		com_frameNumber++;
	}
	catch (int code)
	{
		Com_CatchError(code);
		Com_Printf("%s\n", Com_ErrorString(code));
	}

#ifdef G2_PERFORMANCE_ANALYSIS
	if (com_G2Report && com_G2Report->integer)
	{
		re.G2Time_ReportTimers();
	}

	re.G2Time_ResetTimers();
#endif
}

/*
=================
Com_Shutdown
=================
*/
void Com_Shutdown()
{
	CM_ClearMap();

	if (logfile)
	{
		FS_FCloseFile(logfile);
		logfile = 0;
	}

	if (speedslog)
	{
		FS_Write("\n};", strlen("\n};"), speedslog);
		FS_FCloseFile(speedslog);
		speedslog = 0;
	}

	if (camerafile)
	{
		FS_FCloseFile(camerafile);
		camerafile = 0;
	}

	if (com_journalFile)
	{
		FS_FCloseFile(com_journalFile);
		com_journalFile = 0;
	}

#ifdef JK2_MODE
	JK2SP_Shutdown();
#else
	SE_ShutDown(); //close the string packages
#endif

	extern void Netchan_Shutdown();
	Netchan_Shutdown();
}

/*
==================
Field_Clear
==================
*/
void Field_Clear(field_t* edit)
{
	memset(edit->buffer, 0, MAX_EDIT_LINE);
	edit->cursor = 0;
	edit->scroll = 0;
}

/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/

static const char* completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int matchCount;
// field we are working on, passed to Field_AutoComplete(&g_consoleCommand for instance)
static field_t* completionField;

/*
===============
FindMatches

===============
*/
static void FindMatches(const char* s)
{
	int i;

	if (Q_stricmpn(s, completionString, strlen(completionString)))
	{
		return;
	}
	matchCount++;
	if (matchCount == 1)
	{
		Q_strncpyz(shortestMatch, s, sizeof(shortestMatch));
		return;
	}

	// cut shortestMatch to the amount common with s
	for (i = 0; s[i]; i++)
	{
		if (tolower(shortestMatch[i]) != tolower(s[i]))
		{
			shortestMatch[i] = 0;
			break;
		}
	}
	if (!s[i])
	{
		shortestMatch[i] = 0;
	}
}

/*
===============
PrintMatches

===============
*/
static void PrintMatches(const char* s)
{
	if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch)))
	{
		Com_Printf(S_COLOR_GREY "Cmd  " S_COLOR_WHITE "%s\n", s);
	}
}

/*
===============
PrintArgMatches

===============
*/
#if 0
// This is here for if ever commands with other argument completion
static void PrintArgMatches(const char* s) {
	if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch))) {
		Com_Printf(S_COLOR_WHITE "  %s\n", s);
	}
}
#endif

/*
===============
PrintKeyMatches

===============
*/
static void PrintKeyMatches(const char* s)
{
	if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch)))
	{
		Com_Printf(S_COLOR_GREY "Key  " S_COLOR_WHITE "%s\n", s);
	}
}

/*
===============
PrintFileMatches

===============
*/
static void PrintFileMatches(const char* s)
{
	if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch)))
	{
		Com_Printf(S_COLOR_GREY "File " S_COLOR_WHITE "%s\n", s);
	}
}

/*
===============
PrintCvarMatches

===============
*/
static void PrintCvarMatches(const char* s)
{
	char value[TRUNCATE_LENGTH] = { 0 };

	if (!Q_stricmpn(s, shortestMatch, static_cast<int>(strlen(shortestMatch))))
	{
		Com_TruncateLongString(value, Cvar_VariableString(s));
		Com_Printf(
			S_COLOR_GREY "Cvar " S_COLOR_WHITE "%s = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\""
			S_COLOR_WHITE "\n", s, value);
	}
}

/*
===============
Field_FindFirstSeparator
===============
*/
static char* Field_FindFirstSeparator(char* s)
{
	for (size_t i = 0; i < strlen(s); i++)
	{
		if (s[i] == ';')
			return &s[i];
	}

	return nullptr;
}

/*
===============
Field_Complete
===============
*/
static qboolean Field_Complete()
{
	if (matchCount == 0)
		return qtrue;

	const int completionOffset = strlen(completionField->buffer) - strlen(completionString);

	Q_strncpyz(&completionField->buffer[completionOffset], shortestMatch,
		sizeof(completionField->buffer) - completionOffset);

	completionField->cursor = strlen(completionField->buffer);

	if (matchCount == 1)
	{
		Q_strcat(completionField->buffer, sizeof(completionField->buffer), " ");
		completionField->cursor++;
		return qtrue;
	}

	Com_Printf("%c%s\n", CONSOLE_PROMPT_CHAR, completionField->buffer);

	return qfalse;
}

/*
===============
Field_CompleteKeyname
===============
*/
void Field_CompleteKeyname()
{
	matchCount = 0;
	shortestMatch[0] = 0;

	Key_KeynameCompletion(FindMatches);

	if (!Field_Complete())
		Key_KeynameCompletion(PrintKeyMatches);
}

/*
===============
Field_CompleteFilename
===============
*/
void Field_CompleteFilename(const char* dir, const char* ext, const qboolean stripExt, const qboolean allowNonPureFilesOnDisk)
{
	matchCount = 0;
	shortestMatch[0] = 0;

	FS_FilenameCompletion(dir, ext, stripExt, FindMatches, allowNonPureFilesOnDisk);

	if (!Field_Complete())
		FS_FilenameCompletion(dir, ext, stripExt, PrintFileMatches, allowNonPureFilesOnDisk);
}

/*
===============
Field_CompleteCommand
===============
*/
void Field_CompleteCommand(char* cmd, const qboolean doCommands, const qboolean doCvars)
{
	// Skip leading whitespace and quotes
	cmd = Com_SkipCharset(cmd, " \"");

	Cmd_TokenizeStringIgnoreQuotes(cmd);
	int completionArgument = Cmd_Argc();

	// If there is trailing whitespace on the cmd
	if (*(cmd + strlen(cmd) - 1) == ' ')
	{
		completionString = "";
		completionArgument++;
	}
	else
		completionString = Cmd_Argv(completionArgument - 1);

	if (completionArgument > 1)
	{
		const char* baseCmd = Cmd_Argv(0);
		char* p;

		if (baseCmd[0] == '\\' || baseCmd[0] == '/')
			baseCmd++;

		if ((p = Field_FindFirstSeparator(cmd)))
			Field_CompleteCommand(p + 1, qtrue, qtrue); // Compound command
		else
			Cmd_CompleteArgument(baseCmd, cmd, completionArgument);
	}
	else
	{
		if (completionString[0] == '\\' || completionString[0] == '/')
			completionString++;

		matchCount = 0;
		shortestMatch[0] = 0;

		if (strlen(completionString) == 0)
			return;

		if (doCommands)
			Cmd_CommandCompletion(FindMatches);

		if (doCvars)
			Cvar_CommandCompletion(FindMatches);

		if (!Field_Complete())
		{
			// run through again, printing matches
			if (doCommands)
				Cmd_CommandCompletion(PrintMatches);

			if (doCvars)
				Cvar_CommandCompletion(PrintCvarMatches);
		}
	}
}

/*
===============
Field_AutoComplete

Perform Tab expansion
===============
*/
void Field_AutoComplete(field_t* field)
{
	if (!field || !field->buffer[0])
		return;

	completionField = field;

	Field_CompleteCommand(completionField->buffer, qtrue, qtrue);
}

/*
===============
Converts a UTF-8 character to UTF-32.
===============
*/
uint32_t ConvertUTF8ToUTF32(char* utf8CurrentChar, char** utf8NextChar)
{
	uint32_t utf32 = 0;
	char* c = utf8CurrentChar;

	if ((*c & 0x80) == 0)
		utf32 = *c++;
	else if ((*c & 0xE0) == 0xC0) // 110x xxxx
	{
		utf32 |= (*c++ & 0x1F) << 6;
		utf32 |= (*c++ & 0x3F);
	}
	else if ((*c & 0xF0) == 0xE0) // 1110 xxxx
	{
		utf32 |= (*c++ & 0x0F) << 12;
		utf32 |= (*c++ & 0x3F) << 6;
		utf32 |= (*c++ & 0x3F);
	}
	else if ((*c & 0xF8) == 0xF0) // 1111 0xxx
	{
		utf32 |= (*c++ & 0x07) << 18;
		utf32 |= (*c++ & 0x3F) << 12;
		utf32 |= (*c++ & 0x3F) << 6;
		utf32 |= (*c++ & 0x3F);
	}
	else
	{
		Com_DPrintf("Unrecognised UTF-8 lead byte: 0x%x\n", static_cast<unsigned>(*c));
		c++;
	}

	*utf8NextChar = c;

	return utf32;
}