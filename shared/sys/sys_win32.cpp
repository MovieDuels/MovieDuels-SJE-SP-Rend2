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

#include "sys_local.h"
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <ShlObj_core.h>
#include <qcommon\qcommon.h>
#include <qcommon\q_shared.h>
#include <string.h>
#include <qcommon\q_string.h>
#include <qcommon\q_platform.h>
#include <cstdio>
#include <qcommon\q_math.h>
#include <cstdlib>
#include <malloc.h>

constexpr auto MEM_THRESHOLD = (128 * 1024 * 1024);

// Used to determine where to store user-specific files
static char homePath[MAX_OSPATH] = { 0 };

static UINT timerResolution = 0;

/*
==============
Sys_Basename
==============
*/
const char* Sys_Basename(const char* path)
{
	static char base[MAX_OSPATH] = { 0 };

	int length = strlen(path) - 1;

	// Skip trailing slashes
	while (length > 0 && path[length] == '\\')
		length--;

	while (length > 0 && path[length - 1] != '\\')
		length--;

	Q_strncpyz(base, &path[length], sizeof(base));

	length = strlen(base) - 1;

	// Strip trailing slashes
	while (length > 0 && base[length] == '\\')
		base[length--] = '\0';

	return base;
}

/*
==============
Sys_Dirname
==============
*/
const char* Sys_Dirname(const char* path)
{
	static char dir[MAX_OSPATH] = { 0 };

	Q_strncpyz(dir, path, sizeof(dir));
	int length = strlen(dir) - 1;

	while (length > 0 && dir[length] != '\\')
		length--;

	dir[length] = '\0';

	return dir;
}

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds(const bool baseTime)
{
	static int sys_timeBase = timeGetTime();

	int sys_curtime = timeGetTime();
	if (!baseTime)
	{
		sys_curtime -= sys_timeBase;
	}

	return sys_curtime;
}

int Sys_Milliseconds2()
{
	return Sys_Milliseconds(false);
}

/*
================
Sys_RandomBytes
================
*/
bool Sys_RandomBytes(byte* string, const int len)
{
	HCRYPTPROV prov;

	if (!CryptAcquireContext(&prov, nullptr, nullptr,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		return false;
	}

	if (!CryptGenRandom(prov, len, string))
	{
		CryptReleaseContext(prov, 0);
		return false;
	}
	CryptReleaseContext(prov, 0);
	return true;
}

/*
==================
Sys_GetCurrentUser
==================
*/
char* Sys_GetCurrentUser()
{
	static char s_userName[1024];
	DWORD size = sizeof(s_userName);

	if (!GetUserName(s_userName, &size))
		strcpy(s_userName, "player");

	if (!s_userName[0])
	{
		strcpy(s_userName, "player");
	}

	return s_userName;
}

/*
* Builds the path for the user's game directory
*/
char* Sys_DefaultHomePath()
{
#if defined(_PORTABLE_VERSION)
	Com_Printf("Portable install requested, skipping homepath support\n");
	return nullptr;
#else
	if (!homePath[0])
	{
		TCHAR homeDirectory[MAX_PATH];

		if (!SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, 0, homeDirectory)))
		{
			Com_Printf("Unable to determine your home directory.\n");
			return nullptr;
		}

		Com_sprintf(homePath, sizeof(homePath), "%s%cMy Games%c", homeDirectory, PATH_SEP, PATH_SEP);

		if (com_homepath && com_homepath->string[0])
		{
			Q_strcat(homePath, sizeof(homePath), com_homepath->string);
		}
		else
		{
			Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_WIN);
		}
	}

	return homePath;
#endif
}

static const char* GetErrorString(const DWORD error)
{
	static char buf[MAX_STRING_CHARS];
	buf[0] = '\0';

	if (error)
	{
		LPVOID lpMsgBuf = nullptr;
		const DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&lpMsgBuf), 0, nullptr);
		if (bufLen)
		{
			auto lpMsgStr = static_cast<LPCSTR>(lpMsgBuf);
			Q_strncpyz(buf, lpMsgStr, Q_min((size_t)(lpMsgStr + bufLen), sizeof(buf)));
			LocalFree(lpMsgBuf);
		}
	}
	return buf;
}

void Sys_SetProcessorAffinity()
{
	DWORD_PTR processMask, processAffinityMask, systemAffinityMask;
	const HANDLE handle = GetCurrentProcess();

	if (!GetProcessAffinityMask(handle, &processAffinityMask, &systemAffinityMask))
		return;

	// Fix: sscanf expects an unsigned int*, but processMask is DWORD_PTR (unsigned __int64 on 64-bit)  //VS2026 X64 FIX
	// Use a temporary unsigned int for parsing, then assign/cast to processMask
	unsigned int tempMask = 0;
	if (sscanf(com_affinity->string, "%X", &tempMask) != 1)
		processMask = 1; // set to first core only
	else
		processMask = static_cast<DWORD_PTR>(tempMask);

	if (!processMask)
		processMask = systemAffinityMask; // use all the cores available to the system

	if (processMask == processAffinityMask)
		return; // no change

	if (!SetProcessAffinityMask(handle, processMask))
		Com_DPrintf("Setting affinity mask failed (%s)\n", GetErrorString(GetLastError()));
}

/*
==================
Sys_LowPhysicalMemory()
==================
*/

qboolean Sys_LowPhysicalMemory()
{
	static MEMORYSTATUSEX stat;
	static qboolean bAsked = qfalse;
	static cvar_t* sys_lowmem = Cvar_Get("sys_lowmem", "0", 0);

	if (!bAsked) // just in case it takes a little time for GlobalMemoryStatusEx() to gather stats on
	{		//	stuff we don't care about such as virtual mem etc.
		bAsked = qtrue;

		stat.dwLength = sizeof stat;
		GlobalMemoryStatusEx(&stat);
	}
	if (sys_lowmem->integer)
	{
		return qtrue;
	}
	return stat.ullTotalPhys <= MEM_THRESHOLD ? qtrue : qfalse;
}

/*
==============
Sys_Mkdir
==============
*/
qboolean Sys_Mkdir(const char* path)
{
	if (!CreateDirectory(path, nullptr))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			return qfalse;
	}
	return qtrue;
}

/*
==============
Sys_Cwd
==============
*/
char* Sys_Cwd()
{
	static char cwd[MAX_OSPATH];

	if (!_getcwd(cwd, sizeof(cwd) - 1))
	{
		// fallback: empty string
		cwd[0] = '\0';
	}

	cwd[MAX_OSPATH - 1] = 0;
	return cwd;
}

/* Resolves path names and determines if they are the same */
/* For use with full OS paths not quake paths */
/* Returns true if resulting paths are valid and the same, otherwise false */
bool Sys_PathCmp(const char* path1, const char* path2)
{
	char* r1 = _fullpath(nullptr, path1, MAX_OSPATH);
	char* r2 = _fullpath(nullptr, path2, MAX_OSPATH);

	if (r1 && r2 && !Q_stricmp(r1, r2))
	{
		free(r1);
		free(r2);
		return true;
	}

	free(r1);
	free(r2);
	return false;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

constexpr auto MAX_FOUND_FILES = 0x1000;

static void Sys_ListFilteredFiles(const char* basedir, char* subdirs, char* filter, char** psList, int* numfiles)
{
	char search[MAX_OSPATH];
	_finddata_t findinfo;

	if (*numfiles >= MAX_FOUND_FILES - 1)
	{
		return;
	}

	if (strlen(subdirs))
	{
		Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
	}
	else
	{
		Com_sprintf(search, sizeof(search), "%s\\*", basedir);
	}

	const intptr_t findhandle = _findfirst(search, &findinfo);
	if (findhandle == -1)
	{
		return;
	}

	do
	{
		char filename[MAX_OSPATH];
		if (findinfo.attrib & _A_SUBDIR)
		{
			if (Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, ".."))
			{
				char newsubdirs[MAX_OSPATH];
				if (strlen(subdirs))
				{
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name);
				}
				else
				{
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
				}
				Sys_ListFilteredFiles(basedir, newsubdirs, filter, psList, numfiles);
			}
		}
		if (*numfiles >= MAX_FOUND_FILES - 1)
		{
			break;
		}
		Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name);
		if (!Com_FilterPath(filter, filename, qfalse))
			continue;
		psList[*numfiles] = CopyString(filename);
		(*numfiles)++;
	} while (_findnext(findhandle, &findinfo) != -1);

	_findclose(findhandle);
}

static qboolean strgtr(const char* s0, const char* s1)
{
	int l0 = strlen(s0);
	const int l1 = strlen(s1);

	if (l1 < l0)
	{
		l0 = l1;
	}

	for (int i = 0; i < l0; i++)
	{
		if (s1[i] > s0[i])
		{
			return qtrue;
		}
		if (s1[i] < s0[i])
		{
			return qfalse;
		}
	}
	return qfalse;
}

/*
====================
Sys_ListFiles

Returns a NULL‑terminated array of filenames found in the given directory.

Supports:
- extension filtering
- substring filtering
- optional directory inclusion
- sorted output

Memory ownership:
- Caller owns the returned list (allocated with Z_Malloc)
====================
*/
char** Sys_ListFiles(const char* directory, const char* extension, char* filter, int* numfiles, const qboolean wantsubs)
{
	// -----------------------------
	// Validate input
	// -----------------------------
	if (!directory || !numfiles)
	{
		if (numfiles)
			*numfiles = 0;
		return NULL;
	}

	// -----------------------------
	// FILTERED SEARCH MODE
	// (Uses Sys_ListFilteredFiles)
	// -----------------------------
	if (filter)
	{
		int nfiles = 0;

		// Allocate temporary list on heap (not stack)
		char** list = (char**)Z_Malloc(MAX_FOUND_FILES * sizeof(char*), TAG_LISTFILES);

		Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);

		if (nfiles == 0)
		{
			Z_Free(list);
			*numfiles = 0;
			return NULL;
		}

		// Allocate final list
		char** listCopy = (char**)Z_Malloc((nfiles + 1) * sizeof(char*), TAG_LISTFILES);

		for (int i = 0; i < nfiles; i++)
			listCopy[i] = list[i];

		listCopy[nfiles] = NULL;

		Z_Free(list);

		*numfiles = nfiles;
		return listCopy;
	}

	// -----------------------------
	// EXTENSION HANDLING
	// -----------------------------
	if (!extension)
		extension = "";

	const int extLen = strlen(extension);

	// -----------------------------
	// Build search pattern
	// -----------------------------
	char search[MAX_OSPATH];
	Com_sprintf(search, sizeof(search), "%s\\*%s", directory, extension);

	// -----------------------------
	// Allocate temporary list on heap
	// -----------------------------
	char** list = (char**)Z_Malloc(MAX_FOUND_FILES * sizeof(char*), TAG_LISTFILES);
	int nfiles = 0;

	// -----------------------------
	// Begin directory scan
	// -----------------------------
	_finddata_t findinfo;
	intptr_t findhandle = _findfirst(search, &findinfo);

	if (findhandle == -1)
	{
		Z_Free(list);
		*numfiles = 0;
		return NULL;
	}

	do
	{
		const qboolean isDir = (findinfo.attrib & _A_SUBDIR) ? qtrue : qfalse;

		// Skip "." and ".."
		if (isDir &&
			(!strcmp(findinfo.name, ".") || !strcmp(findinfo.name, "..")))
		{
			continue;
		}

		// Directory filtering logic
		if (!wantsubs && isDir)
			continue;
		if (wantsubs && !isDir)
			continue;

		// Extension filtering
		if (extLen > 0)
		{
			const int nameLen = strlen(findinfo.name);

			if (nameLen < extLen ||
				Q_stricmp(findinfo.name + (nameLen - extLen), extension) != 0)
			{
				continue;
			}
		}

		// Prevent overflow
		if (nfiles >= MAX_FOUND_FILES - 1)
			break;

		list[nfiles++] = CopyString(findinfo.name);
	} while (_findnext(findhandle, &findinfo) != -1);

	_findclose(findhandle);

	if (nfiles == 0)
	{
		Z_Free(list);
		*numfiles = 0;
		return NULL;
	}

	// -----------------------------
	// Allocate final sorted list
	// -----------------------------
	char** listCopy = (char**)Z_Malloc((static_cast<unsigned long long>(nfiles) + 1) * sizeof(char*), TAG_LISTFILES);

	for (int i = 0; i < nfiles; i++)
		listCopy[i] = list[i];

	listCopy[nfiles] = NULL;

	// -----------------------------
	// Sort alphabetically (bubble sort)
	// -----------------------------
	qboolean swapped;
	do
	{
		swapped = qfalse;

		for (int i = 1; i < nfiles; i++)
		{
			if (strgtr(listCopy[i - 1], listCopy[i]))
			{
				char* temp = listCopy[i];
				listCopy[i] = listCopy[i - 1];
				listCopy[i - 1] = temp;
				swapped = qtrue;
			}
		}
	} while (swapped);

	// -----------------------------
	// Cleanup temporary list
	// -----------------------------
	Z_Free(list);

	*numfiles = nfiles;
	return listCopy;
}

void Sys_FreeFileList(char** ps_list)
{
	if (!ps_list)
	{
		return;
	}

	for (int i = 0; ps_list[i]; i++)
	{
		Z_Free(ps_list[i]);
	}

	Z_Free(ps_list);
}

/*
========================================================================

LOAD/UNLOAD DLL

========================================================================
*/
//make sure the dll can be opened by the file system, then write the
//file back out again so it can be loaded is a library. If the read
//fails then the dll is probably not in the pk3 and we are running
//a pure server -rww
UnpackDLLResult Sys_UnpackDLL(const char* name)
{
	UnpackDLLResult result = {};
	void* data;
	const long len = FS_ReadFile(name, &data);

	if (len >= 1)
	{
		if (FS_FileIsInPAK(name, nullptr) == 1)
		{
			char* tempFileName;
			if (FS_WriteToTemporaryFile(data, len, &tempFileName))
			{
				result.tempDLLPath = tempFileName;
				result.succeeded = true;
			}
		}
	}

	FS_FreeFile(data);

	return result;
}

bool Sys_DLLNeedsUnpacking()
{
#if defined(_JK2EXE)
	return false;
#else
	return Cvar_VariableIntegerValue("sv_pure") != 0;
#endif
}

/*
================
Sys_PlatformInit

Platform-specific initialization
================
*/
void Sys_PlatformInit()
{
	TIMECAPS ptc;
	if (timeGetDevCaps(&ptc, sizeof(ptc)) == MMSYSERR_NOERROR)
	{
		timerResolution = ptc.wPeriodMin;

		if (timerResolution > 1)
		{
			Com_Printf("Warning: Minimum supported timer resolution is %ums "
				"on this system, recommended resolution 1ms\n", timerResolution);
		}

		timeBeginPeriod(timerResolution);
	}
	else
		timerResolution = 0;
}

/*
================
Sys_PlatformExit

Platform-specific exit code
================
*/
void Sys_PlatformExit()
{
	if (timerResolution)
		timeEndPeriod(timerResolution);
}

void Sys_Sleep(int msec)
{
	if (msec == 0)
		return;

#ifdef DEDICATED
	if (msec < 0)
		WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), INFINITE);
	else
		WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), msec);
#else
	// Client Sys_Sleep doesn't support waiting on stdin
	if (msec < 0)
		return;

	Sleep(msec);
#endif
}