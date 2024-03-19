/*
===========================================================================
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

#pragma once

#include "qcommon/q_shared.h"

constexpr auto MAXPRINTMSG = 4096;

using netadrtype_t = enum netadrtype_s
{
	NA_BAD = 0,
	// an address lookup failed
	NA_BOT,
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP
};

using netadr_t = struct netadr_s
{
	netadrtype_t type;

	byte ip[4];
	uint16_t port;
};

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

#define MAX_JOYSTICK_AXIS 16

//using joystickAxis_t = enum
//{
//	AXIS_SIDE,
//	AXIS_FORWARD,
//	AXIS_UP,
//	AXIS_ROLL,
//	AXIS_YAW,
//	AXIS_PITCH,
//	MAX_JOYSTICK_AXIS
//};

using sysEventType_t = enum
{
	// bk001129 - make sure SE_NONE is zero
	SE_NONE = 0,
	// evTime is still valid
	SE_KEY,
	// evValue is a key code, evValue2 is the down flag
	SE_CHAR,
	// evValue is an ascii char
	SE_MOUSE,
	// evValue and evValue2 are reletive signed x / y moves
	SE_JOYSTICK_AXIS,
	// evValue is an axis number and evValue2 is the current state (-127 to 127)
	SE_CONSOLE,
	// evPtr is a char*
	SE_MAX
};

using sysEvent_t = struct sysEvent_s
{
	int evTime;
	sysEventType_t evType;
	int evValue, evValue2;
	int evPtrLength; // bytes of data pointed to by evPtr, for journaling
	void* evPtr; // this must be manually freed if not NULL
};

extern cvar_t* com_minimized;
extern cvar_t* com_unfocused;
extern cvar_t* com_maxfps;
extern cvar_t* com_maxfpsMinimized;
extern cvar_t* com_maxfpsUnfocused;

sysEvent_t Sys_GetEvent();

void Sys_Init();

// general development dll loading for virtual machine testing
using GetGameAPIProc = void* (void*);
using VMMainProc = intptr_t QDECL(int, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t,
	intptr_t, intptr_t, intptr_t, intptr_t);
using SystemCallProc = intptr_t QDECL(intptr_t, ...);
using GetModuleAPIProc = void* QDECL(int, ...);

void* Sys_LoadSPGameDll(const char* name, GetGameAPIProc** GetGameAPI);
void* QDECL Sys_LoadDll(const char* name, qboolean useSystemLib);
void* QDECL Sys_LoadLegacyGameDll(const char* name, VMMainProc** vmMain, SystemCallProc* systemcalls);
void* QDECL Sys_LoadGameDll(const char* name, GetModuleAPIProc** moduleAPI);
void Sys_UnloadDll(void* dllHandle);

char* Sys_GetCurrentUser();

void NORETURN QDECL Sys_Error(const char* error, ...);
void NORETURN Sys_Quit();
char* Sys_GetClipboardData(); // note that this isn't journaled...

void Sys_Print(const char* msg);

// Sys_Milliseconds should only be used for profiling purposes,
// any game related timing information should come from event timestamps
int Sys_Milliseconds(bool baseTime = false);
int Sys_Milliseconds2();
void Sys_Sleep(int msec);

extern "C" void Sys_SnapVector(float* v);

bool Sys_RandomBytes(byte* string, int len);

void Sys_SendPacket(int length, const void* data, netadr_t to);

qboolean Sys_StringToAdr(const char* s, netadr_t* a);
//Does NOT parse port numbers, only base addresses.

qboolean Sys_IsLANAddress(netadr_t adr);
void Sys_ShowIP();

qboolean Sys_Mkdir(const char* path);
char* Sys_Cwd();
void Sys_SetDefaultInstallPath(const char* path);
char* Sys_DefaultInstallPath();

#ifdef MACOS_X
char* Sys_DefaultAppPath(void);
#endif

char* Sys_DefaultHomePath();
const char* Sys_Dirname(const char* path);
const char* Sys_Basename(const char* path);

bool Sys_PathCmp(const char* path1, const char* path2);

char** Sys_ListFiles(const char* directory, const char* extension, char* filter, int* numfiles, qboolean wantsubs);
void Sys_FreeFileList(char** ps_list);
//rwwRMG - changed to fileList to not conflict with list type

time_t Sys_FileTime(const char* path);

qboolean Sys_LowPhysicalMemory();

void Sys_SetProcessorAffinity();

using graphicsApi_t = enum graphicsApi_e
{
	GRAPHICS_API_GENERIC,

	// Only OpenGL needs special treatment..
	GRAPHICS_API_OPENGL,
};

// Graphics API
using window_t = struct window_s
{
	void* handle; // OS-dependent window handle
	graphicsApi_t api;
};

using glProfile_t = enum glProfile_e
{
	GLPROFILE_COMPATIBILITY,
	GLPROFILE_CORE,
	GLPROFILE_ES,
};

using glContextFlag_t = enum glContextFlag_e
{
	GLCONTEXT_DEBUG = (1 << 1),
};

using windowDesc_t = struct windowDesc_s
{
	graphicsApi_t api;

	// Only used if api == GRAPHICS_API_OPENGL
	struct gl_
	{
		int majorVersion;
		int minorVersion;
		glProfile_t profile;
		uint32_t contextFlags;
	} gl;
};

using glconfig_t = struct glconfig_s;
window_t WIN_Init(const windowDesc_t* window_desc, glconfig_t* glConfig);
void WIN_Present(window_t* window);
void WIN_SetGamma(glconfig_t* glConfig, byte red[256], byte green[256], byte blue[256]);
void WIN_Shutdown();
void* WIN_GL_GetProcAddress(const char* proc);
qboolean WIN_GL_ExtensionSupported(const char* extension);

uint8_t ConvertUTF32ToExpectedCharset(uint32_t utf32);
