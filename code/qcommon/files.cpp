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

/*****************************************************************************
 * name:		files.cpp
 *
 * desc:		file code
 *
 *****************************************************************************/

#include "q_shared.h"
#include "qcommon.h"

#ifndef FINAL_BUILD
#include "../client/client.h"
#endif
#include <minizip/unzip.h>

 // for rmdir
#if defined (_MSC_VER)
#include <direct.h>
#else
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif
#include <sys\sys_public.h>
#include <qcommon\q_string.h>

/*
=============================================================================

QUAKE3 FILESYSTEM

All of Quake's data access is through a hierarchical file system, but the contents of
the file system can be transparently merged from several sources.

A "qpath" is a reference to game file data.  MAX_ZPATH is 256 characters, which must include
a terminating zero. "..", "\\", and ":" are explicitly illegal in qpaths to prevent any
references outside the quake directory system.

The "base path" is the path to the directory holding all the game directories and usually
the executable.  It defaults to ".", but can be overridden with a "+set fs_basepath c:\quake3"
command line to allow code debugging in a different directory.  Basepath cannot
be modified at all after startup.  Any files that are created (demos, screenshots,
etc) will be created relative to the base path, so base path should usually be writable.

The "home path" is the path used for all write access. On win32 systems we have "base path"
== "home path", but on *nix systems the base installation is usually readonly, and
"home path" points to ~/.q3a or similar

The user can also install custom mods and content in "home path", so it should be searched
along with "home path" and "cd path" for game content.

The "base game" is the directory under the paths where data comes from by default, and
can be either "baseq3" or "demoq3".

The "current game" may be the same as the base game, or it may be the name of another
directory under the paths that should be searched for files before looking in the base game.
This is the basis for addons.

Clients automatically set the game directory after receiving a gamestate from a server,
so only servers need to worry about +set fs_game.

No other directories outside of the base game and current game will ever be referenced by
filesystem functions.

To save disk space and speed loading, directory trees can be collapsed into zip files.
The files use a ".pk3" extension to prevent users from unzipping them accidentally, but
otherwise the are simply normal uncompressed zip files.  A game directory can have multiple
zip files of the form "pak0.pk3", "pak1.pk3", etc.  Zip files are searched in decending order
from the highest number to the lowest, and will always take precedence over the filesystem.
This allows a pk3 distributed as a patch to override all existing data.

Because we will have updated executables freely available online, there is no point to
trying to restrict demo / oem versions of the game with code changes.  Demo / oem versions
should be exactly the same executables as release versions, but with different data that
automatically restricts where game media can come from to prevent add-ons from working.

File search order: when FS_FOpenFileRead gets called it will go through the fs_searchpaths
structure and stop on the first successful hit. fs_searchpaths is built with successive
calls to FS_AddGameDirectory

Additionaly, we search in several subdirectories:
current game is the current mode
base game is a variable to allow mods based on other mods
(such as baseq3 + missionpack content combination in a mod for instance)
BASEGAME is the hardcoded base game ("baseq3")

e.g. the qpath "sound/newstuff/test.wav" would be searched for in the following places:

home path + current game's zip files
home path + current game's directory
base path + current game's zip files
base path + current game's directory
cd path + current game's zip files
cd path + current game's directory

home path + base game's zip file
home path + base game's directory
base path + base game's zip file
base path + base game's directory
cd path + base game's zip file
cd path + base game's directory

home path + BASEGAME's zip file
home path + BASEGAME's directory
base path + BASEGAME's zip file
base path + BASEGAME's directory
cd path + BASEGAME's zip file
cd path + BASEGAME's directory

server download, to be written to home path + current game's directory

The filesystem can be safely shutdown and reinitialized with different
basedir / cddir / game combinations, but all other subsystems that rely on it
(sound, video) must also be forced to restart.

Because the same files are loaded by both the clip model (CM_) and renderer (TR_)
subsystems, a simple single-file caching scheme is used.  The CM_ subsystems will
load the file with a request to cache.  Only one file will be kept cached at a time,
so any models that are going to be referenced by both subsystems should alternate
between the CM_ load function and the ref load function.

TODO: A qpath that starts with a leading slash will always refer to the base game, even if another
game is currently active.  This allows character models, skins, and sounds to be downloaded
to a common directory no matter which game is active.

How to prevent downloading zip files?
Pass pk3 file names in systeminfo, and download before FS_Restart()?

Aborting a download disconnects the client from the server.

How to mark files as downloadable?  Commercial add-ons won't be downloadable.

Non-commercial downloads will want to download the entire zip file.
the game would have to be reset to actually read the zip in

Auto-update information

Path separators

Casing

  separate server gamedir and client gamedir, so if the user starts
  a local game after having connected to a network game, it won't stick
  with the network game.

  allow menu options for game selection?

Read / write config to floppy option.

Different version coexistance?

When building a pak file, make sure a q3config.cfg isn't present in it,
or configs will never get loaded from disk!

  todo:

  downloading (outside fs?)
  game directory passing and restarting

=============================================================================

*/

/*static const uint32_t pak_checksums[] = {
	0u,
};

static const uint32_t bonuspak_checksum = 0u;*/

#define MAX_ZPATH			256
#define	MAX_SEARCH_PATHS	4096
#define MAX_FILEHASH_SIZE	1024

typedef struct fileInPack_s {
	char* name;		// name of the file
	unsigned long			pos;		// file info position in zip
	unsigned long			len;		// uncompress file size
	fileInPack_s* next;		// next file in the hash
} fileInPack_t;

typedef struct pack_s {
	char			pakPathname[MAX_OSPATH];	// c:\jediacademy\gamedata\base
	char			pakFilename[MAX_OSPATH];	// c:\jediacademy\gamedata\base\assets0.pk3
	char			pakBasename[MAX_OSPATH];	// assets0
	char			pakGamename[MAX_OSPATH];	// base
	unzFile			handle;						// handle to zip file
	int				checksum;					// regular checksum
	int				numfiles;					// number of files in pk3
	int				hashSize;					// hash table size (power of 2)
	fileInPack_t** hashTable;					// hash table
	fileInPack_t* buildBuffer;				// buffer with the filenames etc.
} pack_t;

typedef struct directory_s {
	char		path[MAX_OSPATH];		// c:\jediacademy\gamedata
	char		fullpath[MAX_OSPATH];	// c:\jediacademy\gamedata\base
	char		gamedir[MAX_OSPATH];	// base
} directory_t;

typedef struct searchpath_s {
	searchpath_s* next;

	pack_t* pack;		// only one of pack / dir will be non NULL
	directory_t* dir;
} searchpath_t;

static char		fs_gamedir[MAX_OSPATH];	// this will be a single file name with no separators
static cvar_t* fs_debug;
static cvar_t* fs_homepath;

#ifdef MACOS_X
// Also search the .app bundle for .pk3 files
static cvar_t* fs_apppath;
#endif

static cvar_t* fs_basepath;
static cvar_t* fs_basegame;
static cvar_t* fs_cdpath;
static cvar_t* fs_copyfiles;
static cvar_t* fs_gamedirvar;
static cvar_t* fs_dirbeforepak; //rww - when building search path, keep directories at top and insert pk3's under them
static searchpath_t* fs_searchpaths;
static int			fs_readCount;			// total bytes read
static int			fs_loadCount;			// total files read
static int			fs_packFiles = 0;		// total number of files in packs

typedef union qfile_gus {
	FILE* o;
	unzFile		z;
} qfile_gut;

typedef struct qfile_us {
	qfile_gut	file;
	qboolean	unique;
} qfile_ut;

typedef struct fileHandleData_s {
	qfile_ut	handleFiles;
	qboolean	handleSync;
	int			fileSize;
	int			zipFilePos;
	int			zipFileLen;
	qboolean	zipFile;
	char		name[MAX_ZPATH];
} fileHandleData_t;

static fileHandleData_t	fsh[MAX_FILE_HANDLES];

// last valid game folder used
char lastValidBase[MAX_OSPATH];
char lastValidGame[MAX_OSPATH];

/* C99 defines __func__ */
#if __STDC_VERSION__ < 199901L
#  if __GNUC__ >= 2 || _MSC_VER >= 1300
#    define __func__ __FUNCTION__
#  else
#    define __func__ "(unknown)"
#  endif
#endif

/*
==============
FS_Initialized
==============
*/

qboolean FS_Initialized() {
	return static_cast<qboolean>(fs_searchpaths != nullptr);
}

static void FS_AssertInitialised() {
	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization\n");
	}
}

/*
================
return a hash value for the filename
================
*/
static long FS_HashFileName(const char* fname, const int hashSize) {
	long hash = 0;
	int i = 0;
	while (fname[i] != '\0') {
		char letter = tolower(fname[i]);
		if (letter == '.') break;				// don't include extension
		if (letter == '\\') letter = '/';		// damn path names
		if (letter == PATH_SEP) letter = '/';		// damn path names
		hash += static_cast<long>(letter) * (i + 119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (hashSize - 1);
	return hash;
}

static fileHandle_t FS_HandleForFile() {
	int		i;

	for (i = 1; i < MAX_FILE_HANDLES; i++) {
		if (fsh[i].handleFiles.file.o == nullptr) {
			return i;
		}
	}

	Com_Printf("FS_HandleForFile: all handles taken:\n");
	for (i = 1; i < MAX_FILE_HANDLES; i++) {
		Com_Printf("%d. %s\n", i, fsh[i].name);
	}
	Com_Error(ERR_DROP, "FS_HandleForFile: none free");
}

static FILE* FS_FileForHandle(const fileHandle_t f) {
	if (f < 1 || f >= MAX_FILE_HANDLES) {
		Com_Error(ERR_DROP, "FS_FileForHandle: out of range");
	}
	if (fsh[f].zipFile == qtrue) {
		Com_Error(ERR_DROP, "FS_FileForHandle: can't get FILE on zip file");
	}
	if (!fsh[f].handleFiles.file.o) {
		Com_Error(ERR_DROP, "FS_FileForHandle: NULL");
	}

	return fsh[f].handleFiles.file.o;
}

void	FS_ForceFlush(const fileHandle_t f) {
	FILE* file = FS_FileForHandle(f);
	setvbuf(file, nullptr, _IONBF, 0);
}

/*
================
FS_fplength
================
*/

static long FS_fplength(FILE* h)
{
	const long pos = ftell(h);
	if (pos == EOF)
		return EOF;

	fseek(h, 0, SEEK_END);
	const long end = ftell(h);
	fseek(h, pos, SEEK_SET);

	return end;
}

/*
================
FS_filelength

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
int FS_filelength(const fileHandle_t f) {
	FILE* h = FS_FileForHandle(f);

	if (h == nullptr)
		return EOF;
	return FS_fplength(h);
}

/*
====================
FS_ReplaceSeparators

Fix things up differently for win/unix/mac
====================
*/
static void FS_ReplaceSeparators(char* path) {
	qboolean lastCharWasSep = qfalse;

	for (char* s = path; *s; s++) {
		if (*s == '/' || *s == '\\') {
			if (!lastCharWasSep) {
				*s = PATH_SEP;
				lastCharWasSep = qtrue;
			}
			else {
				memmove(s, s + 1, strlen(s));
			}
		}
		else {
			lastCharWasSep = qfalse;
		}
	}
}

/*
===================
FS_BuildOSPath

Qpath may have either forward or backwards slashes
===================
*/
static char* FS_BuildOSPath(const char* qpath) {
	char	temp[MAX_OSPATH];
	static char ospath[4][MAX_OSPATH];
	static int toggle;

	const int nextToggle = (toggle + 1) & 3;	// allows four returns without clash (increased from 2 during fs_copyfiles 2 enhancement)
	toggle = nextToggle;

	// Fix for filenames that are given to FS with a leading "/" (/botfiles/Foo)
	if (qpath[0] == '\\' || qpath[0] == '/')
		qpath++;

	Com_sprintf(temp, sizeof(temp), "/base/%s", qpath); // FIXME SP used fs_gamedir here as well (not sure if this func is even used)
	FS_ReplaceSeparators(temp);
	Com_sprintf(ospath[toggle], sizeof(ospath[0]), "%s%s", fs_basepath->string, temp);

	return ospath[toggle];
}

char* FS_BuildOSPath(const char* base, const char* game, const char* qpath) {
	char	temp[MAX_OSPATH];
	static char ospath[4][MAX_OSPATH];
	static int toggle;

	const int nextToggle = (toggle + 1) & 3;	// allows four returns without clash (increased from 2 during fs_copyfiles 2 enhancement)
	toggle = nextToggle;

	if (!game || !game[0]) {
		game = fs_gamedir;
	}

	Com_sprintf(temp, sizeof(temp), "/%s/%s", game, qpath);
	FS_ReplaceSeparators(temp);
	Com_sprintf(ospath[toggle], sizeof(ospath[0]), "%s%s", base, temp);

	return ospath[toggle];
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
static qboolean FS_CreatePath(char* OSPath) {
	char	path[MAX_OSPATH];

	// make absolutely sure that it can't back up the path
	// FIXME: is c: allowed???
	if (strstr(OSPath, "..") || strstr(OSPath, "::")) {
		Com_Printf("WARNING: refusing to create relative path \"%s\"\n", OSPath);
		return qtrue;
	}

	Q_strncpyz(path, OSPath, sizeof(path));
	FS_ReplaceSeparators(path);

	// Skip creation of the root directory as it will always be there
	char* ofs = strchr(path, PATH_SEP);
	if (ofs) {
		ofs++;
	}

	for (; ofs != nullptr && *ofs; ofs++) {
		if (*ofs == PATH_SEP) {
			// create the directory
			*ofs = 0;
			if (!Sys_Mkdir(path)) {
				Com_Error(ERR_FATAL, "FS_CreatePath: failed to create path \"%s\"",
					path);
			}
			*ofs = PATH_SEP;
		}
	}
	return qfalse;
}

/*
=================
FS_CheckFilenameIsMutable

ERR_FATAL if trying to maniuplate a file with the platform library, or pk3 extension
=================
*/
static void FS_CheckFilenameIsMutable(const char* filename, const char* function)
{
	// Check if the filename ends with the library, or pk3 extension
	if (COM_CompareExtension(filename, DLL_EXT)
		|| COM_CompareExtension(filename, ".pk3"))
	{
		Com_Error(ERR_FATAL, "%s: Not allowed to manipulate '%s' due "
			"to %s extension", function, filename, COM_GetExtension(filename));
	}
}

/*
=================
FS_CopyFile

Copy a fully specified file from one place to another
=================
*/
// added extra param so behind-the-scenes copying in savegames doesn't clutter up the screen -slc
void FS_CopyFile(char* fromOSPath, char* toOSPath, qboolean qbSilent = qfalse);
void FS_CopyFile(char* fromOSPath, char* toOSPath, const qboolean qbSilent) {
	FS_CheckFilenameIsMutable(fromOSPath, __func__);

	if (!qbSilent)
		Com_Printf("copy %s to %s\n", fromOSPath, toOSPath);

	FILE* f = fopen(fromOSPath, "rb");
	if (!f) {
		return;
	}
	fseek(f, 0, SEEK_END);
	const int len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len == EOF)
	{
		fclose(f);
		if (qbSilent)
			return;
		Com_Error(ERR_FATAL, "Bad file length in FS_CopyFile()");
	}

	// we are using direct malloc instead of Z_Malloc here, so it
	// probably won't work on a mac... Its only for developers anyway...
	const auto buf = static_cast<unsigned char*>(malloc(len));
	if (fread(buf, 1, len, f) != static_cast<size_t>(len))
	{
		fclose(f);
		free(buf);
		if (qbSilent)
			return;
		Com_Error(ERR_FATAL, "Short read in FS_Copyfiles()\n");
	}
	fclose(f);

	if (FS_CreatePath(toOSPath)) {
		free(buf);
		return;
	}

	f = fopen(toOSPath, "wb");
	if (!f) {
		free(buf);
		return;
	}
	if (fwrite(buf, 1, len, f) != static_cast<size_t>(len))
	{
		fclose(f);
		free(buf);
		if (qbSilent)
			return;
		Com_Error(ERR_FATAL, "Short write in FS_Copyfiles()\n");
	}
	fclose(f);
	free(buf);
}

/*
===========
FS_Remove

===========
*/
void FS_Remove(const char* osPath) {
	FS_CheckFilenameIsMutable(osPath, __func__);

	remove(osPath);
}

/*
===========
FS_HomeRemove

===========
*/
void FS_HomeRemove(const char* homePath) {
	FS_CheckFilenameIsMutable(homePath, __func__);

	remove(FS_BuildOSPath(fs_homepath->string,
		fs_gamedir, homePath));
}

// The following functions with "UserGen" in them were added for savegame handling,
//	since outside functions aren't supposed to know about full paths/dirs

// "filename" is local to the current gamedir (eg "saves/blah.sav")
//
void FS_DeleteUserGenFile(const char* filename) {
	FS_HomeRemove(filename);
}

// filenames are local (eg "saves/blah.sav")
//
// return: qtrue = OK
//
qboolean FS_MoveUserGenFile(const char* filename_src, const char* filename_dst) {
	FS_AssertInitialised();

	// don't let sound stutter
	S_ClearSoundBuffer();

	char* from_ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename_src);
	char* to_ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename_dst);

	if (fs_debug->integer) {
		Com_Printf("FS_MoveUserGenFile: %s --> %s\n", from_ospath, to_ospath);
	}

	FS_CheckFilenameIsMutable(to_ospath, __func__);

	remove(to_ospath);
	return static_cast<qboolean>(!rename(from_ospath, to_ospath));
}

/*
===========
FS_Rmdir

Removes a directory, optionally deleting all files under it
===========
*/
void FS_Rmdir(const char* osPath, const qboolean recursive) {
	FS_CheckFilenameIsMutable(osPath, __func__);

	if (recursive) {
		int numfiles;
		int i;
		char** filesToRemove = Sys_ListFiles(osPath, "", nullptr, &numfiles, qfalse);
		for (i = 0; i < numfiles; i++) {
			char fileOsPath[MAX_OSPATH];
			Com_sprintf(fileOsPath, sizeof(fileOsPath), "%s/%s", osPath, filesToRemove[i]);
			FS_Remove(fileOsPath);
		}
		FS_FreeFileList(filesToRemove);

		char** directoriesToRemove = Sys_ListFiles(osPath, "/", nullptr, &numfiles, qfalse);
		for (i = 0; i < numfiles; i++) {
			if (!Q_stricmp(directoriesToRemove[i], ".") || !Q_stricmp(directoriesToRemove[i], "..")) {
				continue;
			}
			char directoryOsPath[MAX_OSPATH];
			Com_sprintf(directoryOsPath, sizeof(directoryOsPath), "%s/%s", osPath, directoriesToRemove[i]);
			FS_Rmdir(directoryOsPath, qtrue);
		}
		FS_FreeFileList(directoriesToRemove);
	}

	rmdir(osPath);
}

/*
===========
FS_HomeRmdir

Removes a directory, optionally deleting all files under it
===========
*/
void FS_HomeRmdir(const char* homePath, const qboolean recursive) {
	FS_CheckFilenameIsMutable(homePath, __func__);

	FS_Rmdir(FS_BuildOSPath(fs_homepath->string,
		fs_gamedir, homePath), recursive);
}

/*
================
FS_FileInPathExists

Tests if path and file exists
================
*/
static qboolean FS_FileInPathExists(const char* testpath)
{
	FILE* filep = fopen(testpath, "rb");

	if (filep)
	{
		fclose(filep);
		return qtrue;
	}

	return qfalse;
}

/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_FileExists(const char* file)
{
	return FS_FileInPathExists(FS_BuildOSPath(fs_homepath->string, fs_gamedir, file));
}

/*
================
FS_SV_FileExists

Tests if the file exists
================
*/
static qboolean FS_SV_FileExists(const char* file)
{
	char* testpath = FS_BuildOSPath(fs_homepath->string, file, "");
	testpath[strlen(testpath) - 1] = '\0';

	return FS_FileInPathExists(testpath);
}

/*
===========
FS_SV_FOpenFileWrite

===========
*/
fileHandle_t FS_SV_FOpenFileWrite(const char* filename) {
	FS_AssertInitialised();

	char* ospath = FS_BuildOSPath(fs_homepath->string, filename, "");
	ospath[strlen(ospath) - 1] = '\0';

	fileHandle_t f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	if (fs_debug->integer) {
		Com_Printf("FS_SV_FOpenFileWrite: %s\n", ospath);
	}

	FS_CheckFilenameIsMutable(ospath, __func__);

	if (FS_CreatePath(ospath)) {
		return 0;
	}

	Com_DPrintf("writing to: %s\n", ospath);
	fsh[f].handleFiles.file.o = fopen(ospath, "wb");

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

/*
===========
FS_SV_FOpenFileRead
search for a file somewhere below the home path, base path or cd path
we search in that order, matching FS_SV_FOpenFileRead order
===========
*/
int FS_SV_FOpenFileRead(const char* filename, fileHandle_t* fp) {
	FS_AssertInitialised();

	fileHandle_t f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	// don't let sound stutter
	S_ClearSoundBuffer();

	// search homepath
	char* ospath = FS_BuildOSPath(fs_homepath->string, filename, "");
	// remove trailing slash
	ospath[strlen(ospath) - 1] = '\0';

	if (fs_debug->integer) {
		Com_Printf("FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath);
	}

	fsh[f].handleFiles.file.o = fopen(ospath, "rb");
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o)
	{
		// NOTE TTimo on non *nix systems, fs_homepath == fs_basepath, might want to avoid
		if (Q_stricmp(fs_homepath->string, fs_basepath->string))
		{
			// search basepath
			ospath = FS_BuildOSPath(fs_basepath->string, filename, "");
			ospath[strlen(ospath) - 1] = '\0';

			if (fs_debug->integer)
			{
				Com_Printf("FS_SV_FOpenFileRead (fs_basepath): %s\n", ospath);
			}

			fsh[f].handleFiles.file.o = fopen(ospath, "rb");
			fsh[f].handleSync = qfalse;
		}

		if (!fsh[f].handleFiles.file.o)
		{
			f = 0;
		}
	}

	if (!fsh[f].handleFiles.file.o)
	{
		// search cd path
		ospath = FS_BuildOSPath(fs_cdpath->string, filename, "");
		ospath[strlen(ospath) - 1] = '\0';

		if (fs_debug->integer)
		{
			Com_Printf("FS_SV_FOpenFileRead (fs_cdpath) : %s\n", ospath);
		}

		fsh[f].handleFiles.file.o = fopen(ospath, "rb");
		fsh[f].handleSync = qfalse;

		if (!fsh[f].handleFiles.file.o)
		{
			f = 0;
		}
	}

	*fp = f;
	if (f) {
		return FS_filelength(f);
	}
	return 0;
}

/*
===========
FS_SV_Rename

===========
*/
void FS_SV_Rename(const char* from, const char* to, const qboolean safe) {
	FS_AssertInitialised();

	// don't let sound stutter
	S_ClearSoundBuffer();

	char* from_ospath = FS_BuildOSPath(fs_homepath->string, from, "");
	char* to_ospath = FS_BuildOSPath(fs_homepath->string, to, "");
	from_ospath[strlen(from_ospath) - 1] = '\0';
	to_ospath[strlen(to_ospath) - 1] = '\0';

	if (fs_debug->integer) {
		Com_Printf("FS_SV_Rename: %s --> %s\n", from_ospath, to_ospath);
	}

	if (safe) {
		FS_CheckFilenameIsMutable(to_ospath, __func__);
	}

	if (rename(from_ospath, to_ospath)) {
		// Failed, try copying it and deleting the original
		FS_CopyFile(from_ospath, to_ospath);
		FS_Remove(from_ospath);
	}
}

/*
===========
FS_Rename

===========
*/
void FS_Rename(const char* from, const char* to) {
	FS_AssertInitialised();

	// don't let sound stutter
	S_ClearSoundBuffer();

	char* from_ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, from);
	char* to_ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, to);

	if (fs_debug->integer) {
		Com_Printf("FS_Rename: %s --> %s\n", from_ospath, to_ospath);
	}

	FS_CheckFilenameIsMutable(to_ospath, __func__);

	if (rename(from_ospath, to_ospath)) {
		// Failed, try copying it and deleting the original
		FS_CopyFile(from_ospath, to_ospath);
		FS_Remove(from_ospath);
	}
}

/*
===========
FS_FCloseFile

Close a file.

There are three cases handled:

  * normal file: closed with fclose.

  * file in pak3 archive: subfile is closed with unzCloseCurrentFile, but the
	minizip handle to the pak3 remains open.

  * file in pak3 archive, opened with "unique" flag: This file did not use
	the system minizip handle to the pak3 file, but its own dedicated one.
	The dedicated handle is closed with unzClose.

===========
*/
void FS_FCloseFile(const fileHandle_t f) {
	FS_AssertInitialised();

	if (fsh[f].zipFile == qtrue) {
		unzCloseCurrentFile(fsh[f].handleFiles.file.z);
		if (fsh[f].handleFiles.unique) {
			unzClose(fsh[f].handleFiles.file.z);
		}
		Com_Memset(&fsh[f], 0, sizeof(fsh[f]));
		return;
	}

	// we didn't find it as a pak, so close it as a unique file
	if (fsh[f].handleFiles.file.o) {
		fclose(fsh[f].handleFiles.file.o);
	}
	Com_Memset(&fsh[f], 0, sizeof(fsh[f]));
}

/*
===========
FS_FOpenFileWrite

===========
*/
fileHandle_t FS_FOpenFileWrite(const char* filename, const qboolean safe) {
	FS_AssertInitialised();

	fileHandle_t f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	char* ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);

	if (fs_debug->integer) {
		Com_Printf("FS_FOpenFileWrite: %s\n", ospath);
	}

	if (safe) {
		FS_CheckFilenameIsMutable(ospath, __func__);
	}

	if (FS_CreatePath(ospath)) {
		return 0;
	}

	// enabling the following line causes a recursive function call loop
	// when running with +set logfile 1 +set developer 1
	//Com_DPrintf( "writing to: %s\n", ospath );
	fsh[f].handleFiles.file.o = fopen(ospath, "wb");

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

/*
===========
FS_FOpenFileAppend

===========
*/
fileHandle_t FS_FOpenFileAppend(const char* filename) {
	FS_AssertInitialised();

	fileHandle_t f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	// don't let sound stutter
	S_ClearSoundBuffer();

	char* ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);

	if (fs_debug->integer) {
		Com_Printf("FS_FOpenFileAppend: %s\n", ospath);
	}

	FS_CheckFilenameIsMutable(ospath, __func__);

	if (FS_CreatePath(ospath)) {
		return 0;
	}

	fsh[f].handleFiles.file.o = fopen(ospath, "ab");
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

/*
===========
FS_FilenameCompare

Ignore case and separator char distinctions
===========
*/
qboolean FS_FilenameCompare(const char* s1, const char* s2) {
	int		c1;

	do {
		c1 = *s1++;
		int c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if (c1 == '\\' || c1 == ':') {
			c1 = '/';
		}
		if (c2 == '\\' || c2 == ':') {
			c2 = '/';
		}

		if (c1 != c2) {
			return qtrue;		// strings not equal
		}
	} while (c1);

	return qfalse;		// strings are equal
}

/*
===========
FS_IsExt

Return qtrue if ext matches file extension filename
===========
*/

static qboolean FS_IsExt(const char* filename, const char* ext, const int namelen)
{
	const int extlen = strlen(ext);

	if (extlen > namelen)
		return qfalse;

	filename += namelen - extlen;

	return static_cast<qboolean>(!Q_stricmp(filename, ext));
}

/*
===========
FS_IsDemoExt

Return qtrue if filename has a demo extension
===========
*/

#define DEMO_EXTENSION "dm_"
static qboolean FS_IsDemoExt(const char* filename, int namelen)
{
	const char* ext_test = strrchr(filename, '.');
	if (ext_test && !Q_stricmpn(ext_test + 1, DEMO_EXTENSION, ARRAY_LEN(DEMO_EXTENSION) - 1))
	{
		const int protocol = atoi(ext_test + ARRAY_LEN(DEMO_EXTENSION));

		if (protocol == PROTOCOL_VERSION)
			return qtrue;
	}

	return qfalse;
}

#ifdef _WIN32

static bool Sys_GetFileTime(const LPCSTR psFileName, FILETIME& ft)
{
	bool bSuccess = false;

	const HANDLE hFile = CreateFile(psFileName, // LPCTSTR lpFileName,          // pointer to name of the file
		GENERIC_READ, // DWORD dwDesiredAccess,       // access (read-write) mode
		FILE_SHARE_READ, // DWORD dwShareMode,           // share mode
		nullptr, // LPSECURITY_ATTRIBUTES lpSecurityAttributes,	// pointer to security attributes
		OPEN_EXISTING, // DWORD dwCreationDisposition,  // how to create
		FILE_FLAG_NO_BUFFERING, // DWORD dwFlagsAndAttributes,   // file attributes
		nullptr // HANDLE hTemplateFile          // handle to file with attributes to
	);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		if (GetFileTime(hFile,	// handle to file
			nullptr,	// LPFILETIME lpCreationTime
			nullptr,	// LPFILETIME lpLastAccessTime
			&ft		// LPFILETIME lpLastWriteTime
		)
			)
		{
			bSuccess = true;
		}

		CloseHandle(hFile);
	}

	return bSuccess;
}

static bool Sys_FileOutOfDate(const LPCSTR psFinalFileName /* dest */, const LPCSTR psDataFileName /* src */)
{
	FILETIME ftFinalFile, ftDataFile;

	if (Sys_GetFileTime(psFinalFileName, ftFinalFile) && Sys_GetFileTime(psDataFileName, ftDataFile))
	{
		// timer res only accurate to within 2 seconds on FAT, so can't do exact compare...
		//
		//LONG l = CompareFileTime( &ftFinalFile, &ftDataFile );
		if ((abs(static_cast<double>(ftFinalFile.dwLowDateTime - ftDataFile.dwLowDateTime)) <= 20000000) &&
			ftFinalFile.dwHighDateTime == ftDataFile.dwHighDateTime
			)
		{
			return false;	// file not out of date, ie use it.
		}
		return true;	// flag return code to copy over a replacement version of this file
	}

	// extra error check, report as suspicious if you find a file locally but not out on the net.,.
	//
	if (com_developer->integer)
	{
		if (!Sys_GetFileTime(psDataFileName, ftDataFile))
		{
			Com_Printf("Sys_FileOutOfDate: reading %s but it's not on the net!\n", psFinalFileName);
		}
	}

	return false;
}

#endif // _WIN32

static bool FS_FileCacheable(const char* const filename)
{
	extern	cvar_t* com_buildScript;
	if (com_buildScript && com_buildScript->integer)
	{
		return true;
	}
	return(strchr(filename, '/') != nullptr);
}

/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/
extern qboolean		com_fullyInitialized;

long FS_FOpenFileRead(const char* filename, fileHandle_t* file, const qboolean uniqueFILE) {
	long hash = 0;

	FS_AssertInitialised();

	if (file == nullptr) {
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'file' parameter passed\n");
	}

	if (!filename) {
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed\n");
	}

	// qpaths are not supposed to have a leading slash
	if (filename[0] == '/' || filename[0] == '\\') {
		filename++;
	}

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo"
	if (strstr(filename, "..") || strstr(filename, "::")) {
		*file = 0;
		return -1;
	}

	// make sure the q3key file is only readable by the quake3.exe at initialization
	// any other time the key should only be accessed in memory using the provided functions
	if (com_fullyInitialized && strstr(filename, "q3key")) {
		*file = 0;
		return -1;
	}

	const bool isUserConfig = !Q_stricmp(filename, "autoexec_sp.cfg") || !Q_stricmp(filename, Q3CONFIG_NAME);

	//
	// search through the path, one element at a time
	//

	*file = FS_HandleForFile();
	fsh[*file].handleFiles.unique = uniqueFILE;

	// this new bool is in for an optimisation, if you (eg) opened a BSP file under fs_copyfiles==2,
	//	then it triggered a copy operation to update your local HD version, then this will re-open the
	//	file handle on your local version, not the net build. This uses a bit more CPU to re-do the loop
	//	logic, but should read faster than accessing the net version a second time.
	//
	qboolean b_faster_to_re_open_using_new_local_file;

	do
	{
		b_faster_to_re_open_using_new_local_file = qfalse;

		for (const searchpath_t* search = fs_searchpaths; search; search = search->next) {
			//
			if (search->pack) {
				hash = FS_HashFileName(filename, search->pack->hashSize);
			}
			// is the element a pak file?
			if (search->pack && search->pack->hashTable[hash]) {
				// autoexec_sp.cfg and openjk_sp.cfg can only be loaded outside of pk3 files.
				if (isUserConfig) {
					continue;
				}

				// look through all the pak file elements
				pack_t* pak = search->pack;
				fileInPack_t* pakFile = pak->hashTable[hash];
				do {
					// case and separator insensitive comparisons
					if (!FS_FilenameCompare(pakFile->name, filename)) {
						// found it!

						if (uniqueFILE) {
							// open a new file on the pakfile
							fsh[*file].handleFiles.file.z = unzOpen(pak->pakFilename);
							if (fsh[*file].handleFiles.file.z == nullptr) {
								Com_Error(ERR_FATAL, "Couldn't open %s", pak->pakFilename);
							}
						}
						else {
							fsh[*file].handleFiles.file.z = pak->handle;
						}
						Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
						fsh[*file].zipFile = qtrue;

						// set the file position in the zip file (also sets the current file info)
						unzSetOffset(fsh[*file].handleFiles.file.z, pakFile->pos);

						// open the file in the zip
						unzOpenCurrentFile(fsh[*file].handleFiles.file.z);

#if 0
						zfi = (unz_s*)fsh[*file].handleFiles.file.z;
						// in case the file was new
						temp = zfi->filestream;
						// set the file position in the zip file (also sets the current file info)
						unzSetOffset(pak->handle, pakFile->pos);
						// copy the file info into the unzip structure
						Com_Memcpy(zfi, pak->handle, sizeof(unz_s));
						// we copy this back into the structure
						zfi->filestream = temp;
						// open the file in the zip
						unzOpenCurrentFile(fsh[*file].handleFiles.file.z);
#endif
						fsh[*file].zipFilePos = pakFile->pos;
						fsh[*file].zipFileLen = pakFile->len;

						if (fs_debug->integer) {
							Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n",
								filename, pak->pakFilename);
						}
						return pakFile->len;
					}
					pakFile = pakFile->next;
				} while (pakFile != nullptr);
			}
			else if (search->dir) {
				// check a file in the directory tree

				directory_t* dir = search->dir;

				char* netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
				fsh[*file].handleFiles.file.o = fopen(netpath, "rb");
				if (!fsh[*file].handleFiles.file.o) {
					continue;
				}

#ifdef _WIN32
				// if running with fs_copyfiles 2, and search path == local, then we need to fail to open
				//	if the time/date stamp != the network version (so it'll loop round again and use the network path,
				//	which comes later in the search order)
				//
				if (fs_copyfiles->integer == 2 && fs_cdpath->string[0] && !Q_stricmp(dir->path, fs_basepath->string)
					&& FS_FileCacheable(filename))
				{
					if (Sys_FileOutOfDate(netpath, FS_BuildOSPath(fs_cdpath->string, dir->gamedir, filename)))
					{
						fclose(fsh[*file].handleFiles.file.o);
						fsh[*file].handleFiles.file.o = nullptr;
						continue;	//carry on to find the cdpath version.
					}
				}
#endif
				Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
				fsh[*file].zipFile = qfalse;
				if (fs_debug->integer) {
					Com_Printf("FS_FOpenFileRead: %s (found in '%s%c%s')\n", filename,
						dir->path, PATH_SEP, dir->gamedir);
				}

#ifdef _WIN32
				// if we are getting it from the cdpath, optionally copy it
				//  to the basepath
				if (fs_copyfiles->integer && !Q_stricmp(dir->path, fs_cdpath->string)) {
					char* copypath = FS_BuildOSPath(fs_basepath->string, dir->gamedir, filename);
					switch (fs_copyfiles->integer)
					{
					default:
					case 1:
					{
						FS_CopyFile(netpath, copypath);
					}
					break;

					case 2:
					{
						if (FS_FileCacheable(filename))
						{
							// maybe change this to Com_DPrintf?   On the other hand...
							//
							Com_Printf("fs_copyfiles(2), Copying: %s to %s\n", netpath, copypath);

							FS_CreatePath(copypath);

							bool bOk = true;
							if (!CopyFile(netpath, copypath, FALSE))
							{
								const DWORD dwAttrs = GetFileAttributes(copypath);
								SetFileAttributes(copypath, dwAttrs & ~FILE_ATTRIBUTE_READONLY);
								bOk = !!CopyFile(netpath, copypath, FALSE);
							}

							if (bOk)
							{
								// clear this handle and setup for re-opening of the new local copy...
								//
								b_faster_to_re_open_using_new_local_file = qtrue;
								fclose(fsh[*file].handleFiles.file.o);
								fsh[*file].handleFiles.file.o = nullptr;
							}
						}
					}
					break;
					}
				}
#endif
				if (b_faster_to_re_open_using_new_local_file)
				{
					break;	// and re-read the local copy, not the net version
				}

				return FS_fplength(fsh[*file].handleFiles.file.o);
			}
		}
	} while (b_faster_to_re_open_using_new_local_file);

	Com_DPrintf("Can't find %s\n", filename);
	*file = 0;
	return -1;
}

/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read(void* buffer, const int len, const fileHandle_t f) {
	FS_AssertInitialised();

	if (!f) {
		return 0;
	}

	auto buf = static_cast<byte*>(buffer);
	fs_readCount += len;

	if (fsh[f].zipFile == qfalse) {
		int remaining = len;
		int tries = 0;
		while (remaining) {
			const int block = remaining;
			const int read = fread(buf, 1, block, fsh[f].handleFiles.file.o);
			if (read == 0) {
				// we might have been trying to read from a CD, which
				// sometimes returns a 0 read on windows
				if (!tries) {
					tries = 1;
				}
				else {
					return len - remaining;	//Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
				}
			}

			if (read == -1) {
				Com_Error(ERR_FATAL, "FS_Read: -1 bytes read");
			}

			remaining -= read;
			buf += read;
		}
		return len;
	}
	return unzReadCurrentFile(fsh[f].handleFiles.file.z, buffer, len);
}

/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write(const void* buffer, const int len, const fileHandle_t h) {
	FS_AssertInitialised();

	if (!h) {
		return 0;
	}

	FILE* f = FS_FileForHandle(h);
	auto buf = (byte*)buffer;

	int remaining = len;
	int tries = 0;
	while (remaining) {
		const int block = remaining;
		const int written = fwrite(buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			}
			else {
				Com_Printf("FS_Write: 0 bytes written\n");
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf("FS_Write: -1 bytes written\n");
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if (fsh[h].handleSync) {
		fflush(f);
	}
	return len;
}

#define	MAXPRINTMSG	4096
void QDECL FS_Printf(const fileHandle_t h, const char* fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	FS_Write(msg, strlen(msg), h);
}

#define PK3_SEEK_BUFFER_SIZE 65536
/*
=================
FS_Seek

=================
*/
int FS_Seek(const fileHandle_t f, const long offset, int origin) {
	int		_origin;

	FS_AssertInitialised();

	if (fsh[f].zipFile == qtrue) {
		//FIXME: this is really, really crappy
		//(but better than what was here before)
		byte	buffer[PK3_SEEK_BUFFER_SIZE];
		int		remainder;
		const int		currentPosition = FS_FTell(f);

		// change negative offsets into FS_SEEK_SET
		if (offset < 0) {
			switch (origin) {
			case FS_SEEK_END:
				remainder = fsh[f].zipFileLen + offset;
				break;

			case FS_SEEK_CUR:
				remainder = currentPosition + offset;
				break;

			case FS_SEEK_SET:
			default:
				remainder = 0;
				break;
			}

			if (remainder < 0) {
				remainder = 0;
			}

			origin = FS_SEEK_SET;
		}
		else {
			if (origin == FS_SEEK_END) {
				remainder = fsh[f].zipFileLen - currentPosition + offset;
			}
			else {
				remainder = offset;
			}
		}

		switch (origin) {
		case FS_SEEK_SET:
			if (remainder == currentPosition) {
				return offset;
			}
			unzSetOffset(fsh[f].handleFiles.file.z, fsh[f].zipFilePos);
			unzOpenCurrentFile(fsh[f].handleFiles.file.z);
			//fallthrough

		case FS_SEEK_END:
		case FS_SEEK_CUR:
			while (remainder > PK3_SEEK_BUFFER_SIZE) {
				FS_Read(buffer, PK3_SEEK_BUFFER_SIZE, f);
				remainder -= PK3_SEEK_BUFFER_SIZE;
			}
			FS_Read(buffer, remainder, f);
			return offset;

		default:
			Com_Error(ERR_FATAL, "Bad origin in FS_Seek");
		}
	}
	FILE* file = FS_FileForHandle(f);
	switch (origin) {
	case FS_SEEK_CUR:
		_origin = SEEK_CUR;
		break;
	case FS_SEEK_END:
		_origin = SEEK_END;
		break;
	case FS_SEEK_SET:
		_origin = SEEK_SET;
		break;
	default:
		_origin = SEEK_CUR;
		Com_Error(ERR_FATAL, "Bad origin in FS_Seek\n");
	}

	return fseek(file, offset, _origin);
}

/*
======================================================================================

CONVENIENCE FUNCTIONS FOR ENTIRE FILES

======================================================================================
*/

int	FS_FileIsInPAK(const char* filename) {
	long			hash = 0;

	FS_AssertInitialised();

	if (!filename) {
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed\n");
	}

	// qpaths are not supposed to have a leading slash
	if (filename[0] == '/' || filename[0] == '\\') {
		filename++;
	}

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo"
	if (strstr(filename, "..") || strstr(filename, "::")) {
		return -1;
	}

	//
	// search through the path, one element at a time
	//

	for (const searchpath_t* search = fs_searchpaths; search; search = search->next) {
		//
		if (search->pack) {
			hash = FS_HashFileName(filename, search->pack->hashSize);
		}
		// is the element a pak file?
		if (search->pack && search->pack->hashTable[hash]) {
			// look through all the pak file elements
			const pack_t* pak = search->pack;
			const fileInPack_t* pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if (!FS_FilenameCompare(pakFile->name, filename)) {
					return 1;
				}
				pakFile = pakFile->next;
			} while (pakFile != nullptr);
		}
	}
	return -1;
}

/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
long FS_ReadFile(const char* qpath, void** buffer) {
	fileHandle_t	h;

	FS_AssertInitialised();

	if (!qpath || !qpath[0]) {
		Com_Error(ERR_FATAL, "FS_ReadFile with empty name\n");
	}

	// stop sounds from repeating
	S_ClearSoundBuffer();

	// look for it in the filesystem or pack files
	const long len = FS_FOpenFileRead(qpath, &h, qfalse);
	if (h == 0) {
		if (buffer) {
			*buffer = nullptr;
		}
		return -1;
	}

	if (!buffer) {
		FS_FCloseFile(h);
		return len;
	}

	fs_loadCount++;

	const auto buf = static_cast<byte*>(Z_Malloc(len + 1, TAG_FILESYS, qfalse));
	buf[len] = '\0';	// because we're not calling Z_Malloc with optional trailing 'bZeroIt' bool
	*buffer = buf;

	Z_Label(buf, qpath);

	// PRECACE CHECKER!
#ifndef FINAL_BUILD
	if (com_sv_running && com_sv_running->integer && cls.state >= CA_ACTIVE) {	//com_cl_running
		if (strncmp(qpath, "menu/", 5)) {
			Com_DPrintf(S_COLOR_MAGENTA"FS_ReadFile: %s NOT PRECACHED!\n", qpath);
		}
	}
#endif

	FS_Read(buf, len, h);

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	FS_FCloseFile(h);
	return len;
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile(void* buffer) {
	FS_AssertInitialised();
	if (!buffer) {
		Com_Error(ERR_FATAL, "FS_FreeFile( NULL )");
	}

	Z_Free(buffer);
}

/*
============
FS_WriteFile

Filename are reletive to the quake search path
============
*/
void FS_WriteFile(const char* qpath, const void* buffer, const int size) {
	FS_AssertInitialised();

	if (!qpath || !buffer) {
		Com_Error(ERR_FATAL, "FS_WriteFile: NULL parameter");
	}

	const fileHandle_t f = FS_FOpenFileWrite(qpath);
	if (!f) {
		Com_Printf("Failed to open %s\n", qpath);
		return;
	}

	FS_Write(buffer, size, f);

	FS_FCloseFile(f);
}

/*
==========================================================================

ZIP FILE LOADING

==========================================================================
*/

/*
=================
FS_LoadZipFile

Creates a new pak_t in the search chain for the contents
of a zip file.
=================
*/
static pack_t* FS_LoadZipFile(const char* zipfile, const char* basename)
{
	unz_global_info gi;
	char			filename_inzip[MAX_ZPATH];
	unz_file_info	file_info;
	size_t			i;

	int fs_numHeaderLongs = 0;

	const unzFile uf = unzOpen(zipfile);
	int err = unzGetGlobalInfo(uf, &gi);

	if (err != UNZ_OK)
		return nullptr;

	int len = 0;
	unzGoToFirstFile(uf);
	for (i = 0; i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0);
		if (err != UNZ_OK) {
			break;
		}
		len += strlen(filename_inzip) + 1;
		unzGoToNextFile(uf);
	}

	const auto build_buffer = static_cast<fileInPack_s*>(Z_Malloc((gi.number_entry * sizeof(fileInPack_t)) + len,
		TAG_FILESYS, qtrue));
	char* namePtr = reinterpret_cast<char*>(build_buffer) + gi.number_entry * sizeof(fileInPack_t);
	const auto fs_header_longs = static_cast<int*>(Z_Malloc(gi.number_entry * sizeof(int), TAG_FILESYS, qtrue));

	// get the hash table size from the number of files in the zip
	// because lots of custom pk3 files have less than 32 or 64 files
	for (i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1) {
		if (i > gi.number_entry) {
			break;
		}
	}

	auto pack = static_cast<pack_t*>(Z_Malloc(sizeof(pack_t) + i * sizeof(fileInPack_t*), TAG_FILESYS, qtrue));
	pack->hashSize = i;
	pack->hashTable = reinterpret_cast<fileInPack_t**>(reinterpret_cast<char*>(pack) + sizeof(pack_t));
	for (int j = 0; j < pack->hashSize; j++) {
		pack->hashTable[j] = nullptr;
	}

	Q_strncpyz(pack->pakFilename, zipfile, sizeof(pack->pakFilename));
	Q_strncpyz(pack->pakBasename, basename, sizeof(pack->pakBasename));

	// strip .pk3 if needed
	if (strlen(pack->pakBasename) > 4 && !Q_stricmp(pack->pakBasename + strlen(pack->pakBasename) - 4, ".pk3")) {
		pack->pakBasename[strlen(pack->pakBasename) - 4] = 0;
	}

	pack->handle = uf;
	pack->numfiles = gi.number_entry;
	unzGoToFirstFile(uf);

	for (i = 0; i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0);
		if (err != UNZ_OK) {
			break;
		}
		if (file_info.uncompressed_size > 0) {
			fs_header_longs[fs_numHeaderLongs++] = LittleLong(file_info.crc);
		}
		Q_strlwr(filename_inzip);
		const long hash = FS_HashFileName(filename_inzip, pack->hashSize);
		build_buffer[i].name = namePtr;
		strcpy(build_buffer[i].name, filename_inzip);
		namePtr += strlen(filename_inzip) + 1;
		// store the file position in the zip
		build_buffer[i].pos = unzGetOffset(uf);
		build_buffer[i].len = file_info.uncompressed_size;
		build_buffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &build_buffer[i];
		unzGoToNextFile(uf);
	}

	pack->checksum = Com_BlockChecksum(fs_header_longs, sizeof(*fs_header_longs) * fs_numHeaderLongs);
	pack->checksum = LittleLong(pack->checksum);

	Z_Free(fs_header_longs);

	pack->buildBuffer = build_buffer;
	return pack;
}

/*
=================
FS_FreePak

Frees a pak structure and releases all associated resources
=================
*/

static void FS_FreePak(pack_t* thepak)
{
	unzClose(thepak->handle);
	Z_Free(thepak->buildBuffer);
	Z_Free(thepak);
}

/*
=================================================================================

DIRECTORY SCANNING FUNCTIONS

=================================================================================
*/

#define	MAX_FOUND_FILES	0x1000

static int FS_ReturnPath(const char* zname, char* zpath, int* depth) {
	int newdep = 0;
	zpath[0] = 0;
	int len = 0;
	int at = 0;

	while (zname[at] != 0)
	{
		if (zname[at] == '/' || zname[at] == '\\') {
			len = at;
			newdep++;
		}
		at++;
	}
	strcpy(zpath, zname);
	zpath[len] = 0;
	*depth = newdep;

	return len;
}

/*
==================
FS_AddFileToList
==================
*/
static int FS_AddFileToList(const char* name, char* list[MAX_FOUND_FILES], int nfiles) {
	if (nfiles == MAX_FOUND_FILES - 1) {
		return nfiles;
	}
	for (int i = 0; i < nfiles; i++) {
		if (!Q_stricmp(name, list[i])) {
			return nfiles;		// allready in list
		}
	}
	list[nfiles] = CopyString(name);
	nfiles++;

	return nfiles;
}

/*
===============
FS_ListFilteredFiles

Returns a uniqued list of files that match the given criteria
from all search paths
===============
*/
static char** FS_ListFilteredFiles(const char* path, const char* extension, char* filter, int* numfiles)
{
	char* list[MAX_FOUND_FILES];
	int				i;
	int pathDepth;
	char			zpath[MAX_ZPATH];
	qboolean		stripTrailingSlash = qfalse;
	char			strippedPath[MAX_ZPATH];

	FS_AssertInitialised();

	if (!path) {
		*numfiles = 0;
		return nullptr;
	}
	if (!extension) {
		extension = "";
	}
	// passing a slash as extension will find directories
	if (extension[0] == '/' && extension[1] == 0) {
		stripTrailingSlash = qtrue; //strip slash when adding to file list
	}

	int pathLength = strlen(path);
	if (path[pathLength - 1] == '\\' || path[pathLength - 1] == '/') {
		pathLength--;
	}
	const int extensionLength = strlen(extension);
	int nfiles = 0;
	FS_ReturnPath(path, zpath, &pathDepth);

	//
	// search through the path, one element at a time, adding to list
	//
	for (const searchpath_t* search = fs_searchpaths; search; search = search->next) {
		// is the element a pak file?
		if (search->pack) {
			// look through all the pak file elements
			const pack_t* pak = search->pack;
			const fileInPack_t* buildBuffer = pak->buildBuffer;
			for (i = 0; i < pak->numfiles; i++) {
				int depth;

				// check for directory match
				const char* name = buildBuffer[i].name;
				//
				if (filter) {
					// case insensitive
					if (!Com_FilterPath(filter, name, qfalse))
						continue;
					// unique the match
					nfiles = FS_AddFileToList(name, list, nfiles);
				}
				else {
					const int zpathLen = FS_ReturnPath(name, zpath, &depth);

					if ((depth - pathDepth) > 2 || pathLength > zpathLen || Q_stricmpn(name, path, pathLength)) {
						continue;
					}

					// check for extension match
					const int length = strlen(name);
					if (length < extensionLength) {
						continue;
					}

					if (Q_stricmp(name + length - extensionLength, extension)) {
						continue;
					}
					// unique the match

					int temp = pathLength;
					if (pathLength) {
						temp++;		// include the '/'
					}

					if (stripTrailingSlash) {
						Q_strncpyz(strippedPath, name, sizeof(strippedPath));
						strippedPath[length - extensionLength] = '\0';
						name = strippedPath;
					}
					nfiles = FS_AddFileToList(name + temp, list, nfiles);
				}
			}
		}
		else if (search->dir) { // scan for files in the filesystem
			int		numSysFiles;

			const char* netpath = FS_BuildOSPath(search->dir->path, search->dir->gamedir, path);
			char** sysFiles = Sys_ListFiles(netpath, extension, filter, &numSysFiles, qfalse);
			for (i = 0; i < numSysFiles; i++) {
				// unique the match
				const char* name = sysFiles[i];
				nfiles = FS_AddFileToList(name, list, nfiles);
			}
			Sys_FreeFileList(sysFiles);
		}
	}

	// return a copy of the list
	*numfiles = nfiles;

	if (!nfiles) {
		return nullptr;
	}

	char** listCopy = static_cast<char**>(Z_Malloc((nfiles + 1) * sizeof(*listCopy), TAG_FILESYS, qfalse));
	for (i = 0; i < nfiles; i++) {
		listCopy[i] = list[i];
	}
	listCopy[i] = nullptr;

	return listCopy;
}

/*
=================
FS_ListFiles
=================
*/
char** FS_ListFiles(const char* path, const char* extension, int* numfiles) {
	return FS_ListFilteredFiles(path, extension, nullptr, numfiles);
}

/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList(char** file_list) {
	FS_AssertInitialised();

	if (!file_list) {
		return;
	}

	for (int i = 0; file_list[i]; i++) {
		Z_Free(file_list[i]);
	}

	Z_Free(file_list);
}

/*
================
FS_GetFileList
================
*/
int	FS_GetFileList(const char* path, const char* extension, char* listbuf, const int bufsize) {
	int		nFiles;

	*listbuf = 0;
	nFiles = 0;
	int nTotal = 0;

	if (Q_stricmp(path, "$modlist") == 0) {
		return FS_GetModList(listbuf, bufsize);
	}

	char** pFiles = FS_ListFiles(path, extension, &nFiles);

	for (int i = 0; i < nFiles; i++) {
		const int nLen = strlen(pFiles[i]) + 1;
		if (nTotal + nLen + 1 < bufsize) {
			strcpy(listbuf, pFiles[i]);
			listbuf += nLen;
			nTotal += nLen;
		}
		else {
			nFiles = i;
			break;
		}
	}

	FS_FreeFileList(pFiles);

	return nFiles;
}

/*
=======================
Sys_ConcatenateFileLists

mkv: Naive implementation. Concatenates three lists into a
	 new list, and frees the old lists from the heap.
bk001129 - from cvs1.17 (mkv)

FIXME TTimo those two should move to common.c next to Sys_ListFiles
=======================
 */
static unsigned int Sys_CountFileList(char** fileList)
{
	int i = 0;

	if (fileList)
	{
		while (*fileList)
		{
			fileList++;
			i++;
		}
	}
	return i;
}

static char** Sys_ConcatenateFileLists(char** list0, char** list1, char** list2)
{
	int totalLength = 0;
	char** cat = nullptr, ** src;

	totalLength += Sys_CountFileList(list0);
	totalLength += Sys_CountFileList(list1);
	totalLength += Sys_CountFileList(list2);

	/* Create new list. */
	char** dst = cat = static_cast<char**>(Z_Malloc((totalLength + 1) * sizeof(char*), TAG_FILESYS, qtrue));

	/* Copy over lists. */
	if (list0) {
		for (src = list0; *src; src++, dst++)
			*dst = *src;
	}
	if (list1) {
		for (src = list1; *src; src++, dst++)
			*dst = *src;
	}
	if (list2) {
		for (src = list2; *src; src++, dst++)
			*dst = *src;
	}

	// Terminate the list
	*dst = nullptr;

	// Free our old lists.
	// NOTE: not freeing their content, it's been merged in dst and still being used
	if (list0) Z_Free(list0);
	if (list1) Z_Free(list1);
	if (list2) Z_Free(list2);

	return cat;
}
//#endif

// For base game mod listing
const char* SE_GetString(const char* psPackageAndStringReference);

/*
================
FS_GetModList

Returns a list of mod directory names
A mod directory is a peer to base with a pk3 in it
The directories are searched in base path, cd path and home path
================
*/
int	FS_GetModList(char* listbuf, const int bufsize) {
	int nTotal, nPaks, nPotential;
	char descPath[MAX_OSPATH]{};
	fileHandle_t descHandle;

	int dummy;
	qboolean bDrop = qfalse;

	*listbuf = 0;
	int nMods = nPotential = nTotal = 0;

	char** pFiles0 = Sys_ListFiles(fs_homepath->string, nullptr, nullptr, &dummy, qtrue);
	char** pFiles1 = Sys_ListFiles(fs_basepath->string, nullptr, nullptr, &dummy, qtrue);
	char** pFiles2 = Sys_ListFiles(fs_cdpath->string, nullptr, nullptr, &dummy, qtrue);
	// we searched for mods in the three paths
	// it is likely that we have duplicate names now, which we will cleanup below
	char** pFiles = Sys_ConcatenateFileLists(pFiles0, pFiles1, pFiles2);
	nPotential = Sys_CountFileList(pFiles);

	for (int i = 0; i < nPotential; i++) {
		const char* name = pFiles[i];
		// NOTE: cleaner would involve more changes
		// ignore duplicate mod directories
		if (i != 0) {
			bDrop = qfalse;
			for (int j = 0; j < i; j++)
			{
				if (Q_stricmp(pFiles[j], name) == 0) {
					// this one can be dropped
					bDrop = qtrue;
					break;
				}
			}
		}
		if (bDrop) {
			continue;
		}
		// we drop "." and ".."
		if (Q_stricmpn(name, ".", 1)) {
			// now we need to find some .pk3 files to validate the mod
			// NOTE TTimo: (actually I'm not sure why .. what if it's a mod under developement with no .pk3?)
			// we didn't keep the information when we merged the directory names, as to what OS Path it was found under
			//   so it could be in base path, cd path or home path
			//   we will try each three of them here (yes, it's a bit messy)
			const char* path = FS_BuildOSPath(fs_basepath->string, name, "");
			nPaks = 0;
			char** pPaks = Sys_ListFiles(path, ".pk3", nullptr, &nPaks, qfalse);
			Sys_FreeFileList(pPaks); // we only use Sys_ListFiles to check wether .pk3 files are present

			/* Try on cd path */
			if (nPaks <= 0) {
				path = FS_BuildOSPath(fs_cdpath->string, name, "");
				nPaks = 0;
				pPaks = Sys_ListFiles(path, ".pk3", nullptr, &nPaks, qfalse);
				Sys_FreeFileList(pPaks);
			}

			/* try on home path */
			if (nPaks <= 0)
			{
				path = FS_BuildOSPath(fs_homepath->string, name, "");
				nPaks = 0;
				pPaks = Sys_ListFiles(path, ".pk3", nullptr, &nPaks, qfalse);
				Sys_FreeFileList(pPaks);
			}

			if (nPaks > 0) {
				const bool isBase = !Q_stricmp(name, BASEGAME);
				const int nLen = isBase ? 1 : strlen(name) + 1;
				// nLen is the length of the mod path
				// we need to see if there is a description available
				descPath[0] = '\0';
				strcpy(descPath, name);
				strcat(descPath, "/description.txt");
				int nDescLen = FS_SV_FOpenFileRead(descPath, &descHandle);
				if (nDescLen > 0 && descHandle) {
					FILE* file = FS_FileForHandle(descHandle);
					Com_Memset(descPath, 0, sizeof(descPath));
					nDescLen = fread(descPath, 1, 48, file);
					if (nDescLen >= 0) {
						descPath[nDescLen] = '\0';
					}
					FS_FCloseFile(descHandle);
				}
				else if (isBase) {
					strcpy(descPath, SE_GetString("MENUS_JEDI_ACADEMY"));
				}
				else {
					strcpy(descPath, name);
				}
				nDescLen = strlen(descPath) + 1;

				if (nTotal + nLen + 1 + nDescLen + 1 < bufsize) {
					if (isBase)
						strcpy(listbuf, "");
					else
						strcpy(listbuf, name);
					listbuf += nLen;
					strcpy(listbuf, descPath);
					listbuf += nDescLen;
					nTotal += nLen + nDescLen;
					nMods++;
				}
				else {
					break;
				}
			}
		}
	}
	Sys_FreeFileList(pFiles);

	return nMods;
}

//============================================================================

/*
================
FS_Dir_f
================
*/
static void FS_Dir_f() {
	char* path;
	char* extension;
	int		ndirs;

	if (Cmd_Argc() < 2 || Cmd_Argc() > 3) {
		Com_Printf("usage: dir <directory> [extension]\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		path = Cmd_Argv(1);
		extension = "";
	}
	else {
		path = Cmd_Argv(1);
		extension = Cmd_Argv(2);
	}

	Com_Printf("Directory of %s %s\n", path, extension);
	Com_Printf("---------------\n");

	char** dirnames = FS_ListFiles(path, extension, &ndirs);

	for (int i = 0; i < ndirs; i++) {
		Com_Printf("%s\n", dirnames[i]);
	}
	FS_FreeFileList(dirnames);
}

/*
===========
FS_ConvertPath
===========
*/
static void FS_ConvertPath(char* s) {
	while (*s) {
		if (*s == '\\' || *s == ':') {
			*s = '/';
		}
		s++;
	}
}

/*
===========
FS_PathCmp

Ignore case and separator char distinctions
===========
*/
static int FS_PathCmp(const char* s1, const char* s2) {
	int		c1;

	do {
		c1 = *s1++;
		int c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if (c1 == '\\' || c1 == ':') {
			c1 = '/';
		}
		if (c2 == '\\' || c2 == ':') {
			c2 = '/';
		}

		if (c1 < c2) {
			return -1;		// strings not equal
		}
		if (c1 > c2) {
			return 1;
		}
	} while (c1);

	return 0;		// strings are equal
}

/*
================
FS_SortFileList
================
*/
static void FS_SortFileList(char** filelist, const int numfiles) {
	int j;

	char** sortedlist = static_cast<char**>(Z_Malloc((numfiles + 1) * sizeof(*sortedlist), TAG_FILESYS, qtrue));
	sortedlist[0] = nullptr;
	int numsortedfiles = 0;
	for (int i = 0; i < numfiles; i++) {
		for (j = 0; j < numsortedfiles; j++) {
			if (FS_PathCmp(filelist[i], sortedlist[j]) < 0) {
				break;
			}
		}
		for (int k = numsortedfiles; k > j; k--) {
			sortedlist[k] = sortedlist[k - 1];
		}
		sortedlist[j] = filelist[i];
		numsortedfiles++;
	}
	Com_Memcpy(filelist, sortedlist, numfiles * sizeof(*filelist));
	Z_Free(sortedlist);
}

/*
================
FS_NewDir_f
================
*/
static void FS_NewDir_f() {
	int		ndirs;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: fdir <filter>\n");
		Com_Printf("example: fdir *t1*.bsp\n");
		return;
	}

	char* filter = Cmd_Argv(1);

	Com_Printf("---------------\n");

	char** dirnames = FS_ListFilteredFiles("", "", filter, &ndirs);

	FS_SortFileList(dirnames, ndirs);

	for (int i = 0; i < ndirs; i++) {
		FS_ConvertPath(dirnames[i]);
		Com_Printf("%s\n", dirnames[i]);
	}
	Com_Printf("%d files listed\n", ndirs);
	FS_FreeFileList(dirnames);
}

/*
============
FS_Path_f

============
*/
static void FS_Path_f() {
	Com_Printf("Current search path:\n");
	for (const searchpath_t* s = fs_searchpaths; s; s = s->next) {
		if (s->pack) {
			Com_Printf("%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles);
		}
		else {
			Com_Printf("%s%c%s\n", s->dir->path, PATH_SEP, s->dir->gamedir);
		}
	}

	Com_Printf("\n");
	for (int i = 1; i < MAX_FILE_HANDLES; i++) {
		if (fsh[i].handleFiles.file.o) {
			Com_Printf("handle %i: %s\n", i, fsh[i].name);
		}
	}
}

/*
============
FS_TouchFile_f

The only purpose of this function is to allow game script files to copy
arbitrary files furing an "fs_copyfiles 1" run.
============
*/
static void FS_TouchFile_f() {
	fileHandle_t	f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: touchFile <file>\n");
		return;
	}

	FS_FOpenFileRead(Cmd_Argv(1), &f, qfalse);
	if (f) {
		FS_FCloseFile(f);
	}
}

/*
============
FS_Which_f
============
*/
static void FS_Which_f() {
	char* filename = Cmd_Argv(1);

	if (!filename[0]) {
		Com_Printf("Usage: which <file>\n");
		return;
	}

	// qpaths are not supposed to have a leading slash
	if (filename[0] == '/' || filename[0] == '\\') {
		filename++;
	}

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo"
	if (strstr(filename, "..") || strstr(filename, "::")) {
		return;
	}

	// just wants to see if file is there
	for (const searchpath_t* search = fs_searchpaths; search; search = search->next) {
		if (search->pack) {
			const long hash = FS_HashFileName(filename, search->pack->hashSize);

			// is the element a pak file?
			if (search->pack->hashTable[hash]) {
				// look through all the pak file elements
				pack_t* pak = search->pack;
				const fileInPack_t* pakFile = pak->hashTable[hash];

				do {
					// case and separator insensitive comparisons
					if (!FS_FilenameCompare(pakFile->name, filename)) {
						// found it!
						Com_Printf("File \"%s\" found in \"%s\"\n", filename, pak->pakFilename);
						return;
					}

					pakFile = pakFile->next;
				} while (pakFile != nullptr);
			}
		}
		else if (search->dir) {
			directory_t* dir = search->dir;

			const char* netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
			FILE* filep = fopen(netpath, "rb");

			if (filep) {
				fclose(filep);

				char buf[MAX_OSPATH];
				Com_sprintf(buf, sizeof(buf), "%s%c%s", dir->path, PATH_SEP, dir->gamedir);
				FS_ReplaceSeparators(buf);
				Com_Printf("File \"%s\" found at \"%s\"\n", filename, buf);
				return;
			}
		}
	}

	Com_Printf("File not found: \"%s\"\n", filename);
}

//===========================================================================

static int QDECL paksort(const void* a, const void* b) {
	const char* aa = *(char**)a;
	const char* bb = *(char**)b;

	return FS_PathCmp(aa, bb);
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads the zip headers
================
*/
#define	MAX_PAKFILES	1024
static void FS_AddGameDirectory(const char* path, const char* dir) {
	int				i;
	pack_t* pak;
	char			curpath[MAX_OSPATH + 1];
	int				numfiles;
	char* sorted[MAX_PAKFILES]{};

	// this fixes the case where fs_basepath is the same as fs_cdpath
	// which happens on full installs
	for (const searchpath_t* sp = fs_searchpaths; sp; sp = sp->next) {
		// TODO Sys_PathCmp SDL-Port will contain this for SP as well
		// Should be Sys_PathCmp(sp->dir->path, path)
		if (sp->dir && !Q_stricmp(sp->dir->path, path) && !Q_stricmp(sp->dir->gamedir, dir)) {
			return;			// we've already got this one
		}
	}

	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	// find all pak files in this directory
	Q_strncpyz(curpath, FS_BuildOSPath(path, dir, ""), sizeof(curpath));
	curpath[strlen(curpath) - 1] = '\0';	// strip the trailing slash

	//
	// add the directory to the search path
	//
	searchpath_t* search = static_cast<searchpath_s*>(Z_Malloc(sizeof(searchpath_t), TAG_FILESYS, qtrue));
	search->dir = static_cast<directory_t*>(Z_Malloc(sizeof(*search->dir), TAG_FILESYS, qtrue));

	Q_strncpyz(search->dir->path, path, sizeof(search->dir->path));
	Q_strncpyz(search->dir->fullpath, curpath, sizeof(search->dir->fullpath));
	Q_strncpyz(search->dir->gamedir, dir, sizeof(search->dir->gamedir));
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	searchpath_t* thedir = search;

	char** pakfiles = Sys_ListFiles(curpath, ".pk3", nullptr, &numfiles, qfalse);

	// sort them so that later alphabetic matches override
	// earlier ones.  This makes pak1.pk3 override pak0.pk3
	if (numfiles > MAX_PAKFILES) {
		numfiles = MAX_PAKFILES;
	}
	for (i = 0; i < numfiles; i++) {
		sorted[i] = pakfiles[i];
	}

	qsort(sorted, numfiles, sizeof(char*), paksort);

	for (i = 0; i < numfiles; i++) {
		const char* pakfile = FS_BuildOSPath(path, dir, sorted[i]);
		if ((pak = FS_LoadZipFile(pakfile, sorted[i])) == nullptr)
			continue;
		Q_strncpyz(pak->pakPathname, curpath, sizeof(pak->pakPathname));
		// store the game name for downloading
		Q_strncpyz(pak->pakGamename, dir, sizeof(pak->pakGamename));

		fs_packFiles += pak->numfiles;

		search = static_cast<searchpath_s*>(Z_Malloc(sizeof(searchpath_t), TAG_FILESYS, qtrue));
		search->pack = pak;

		if (fs_dirbeforepak && fs_dirbeforepak->integer && thedir)
		{
			searchpath_t* oldnext = thedir->next;
			thedir->next = search;

			while (oldnext)
			{
				search->next = oldnext;
				search = search->next;
				oldnext = oldnext->next;
			}
		}
		else
		{
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
	}

	// done
	Sys_FreeFileList(pakfiles);
}

/*
================
FS_CheckDirTraversal

Check whether the string contains stuff like "../" to prevent directory traversal bugs
and return qtrue if it does.
================
*/

qboolean FS_CheckDirTraversal(const char* checkdir)
{
	if (strstr(checkdir, "../") || strstr(checkdir, "..\\"))
		return qtrue;

	return qfalse;
}

/*
================
FS_Shutdown

Frees all resources and closes all files
================
*/
void FS_Shutdown() {
	searchpath_t* next = nullptr;

	for (int i = 0; i < MAX_FILE_HANDLES; i++) {
		if (fsh[i].fileSize) {
			FS_FCloseFile(i);
		}
	}

	// free everything
	for (searchpath_t* p = fs_searchpaths; p; p = next) {
		next = p->next;

		if (p->pack) {
			FS_FreePak(p->pack);
		}
		if (p->dir) {
			Z_Free(p->dir);
		}
		Z_Free(p);
	}

	// any FS_ calls will now be an error until reinitialized
	fs_searchpaths = nullptr;

	Cmd_RemoveCommand("path");
	Cmd_RemoveCommand("dir");
	Cmd_RemoveCommand("fdir");
	Cmd_RemoveCommand("touchFile");
	Cmd_RemoveCommand("which");
}

/*
================
FS_Startup
================
*/
static void FS_Startup(const char* gameName)
{
	Com_Printf("----- FS_Startup -----\n");

	fs_packFiles = 0;

	fs_debug = Cvar_Get("fs_debug", "0", 0);
	fs_copyfiles = Cvar_Get("fs_copyfiles", "0", CVAR_INIT);
	fs_cdpath = Cvar_Get("fs_cdpath", "", CVAR_INIT | CVAR_PROTECTED);
	fs_basepath = Cvar_Get("fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT | CVAR_PROTECTED);
	fs_basegame = Cvar_Get("fs_basegame", "", CVAR_INIT);
	const char* homePath = Sys_DefaultHomePath();
	if (!homePath || !homePath[0]) {
		homePath = fs_basepath->string;
	}
	fs_homepath = Cvar_Get("fs_homepath", homePath, CVAR_INIT | CVAR_PROTECTED);

	fs_gamedirvar = Cvar_Get("fs_game", "MD", CVAR_INIT | CVAR_SYSTEMINFO);

	fs_dirbeforepak = Cvar_Get("fs_dirbeforepak", "0", CVAR_INIT | CVAR_PROTECTED);

	Cvar_Get("com_outcast", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	Cvar_Get("com_kotor", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	Cvar_Get("com_rend2", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	g_trueguns = Cvar_Get("cg_trueguns", "0", 0);

	Cvar_Get("r_weather", "0", CVAR_ARCHIVE);

	// add search path elements in reverse priority order
	if (fs_cdpath->string[0]) {
		FS_AddGameDirectory(fs_cdpath->string, gameName);
	}
	if (fs_basepath->string[0]) {
		FS_AddGameDirectory(fs_basepath->string, gameName);
	}

#ifdef MACOS_X
	fs_apppath = Cvar_Get("fs_apppath", Sys_DefaultAppPath(), CVAR_INIT | CVAR_PROTECTED);
	// Make MacOSX also include the base path included with the .app bundle
	if (fs_apppath->string[0]) {
		FS_AddGameDirectory(fs_apppath->string, gameName);
	}
#endif

	// fs_homepath is somewhat particular to *nix systems, only add if relevant
	// NOTE: same filtering below for mods and basegame
	// TODO Sys_PathCmp see previous comment for why
	// !Sys_PathCmp(fs_homepath->string, fs_basepath->string)
	if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string)) {
		FS_CreatePath(fs_homepath->string);
		FS_AddGameDirectory(fs_homepath->string, gameName);
	}

	// check for additional base game so mods can be based upon other mods
	if (fs_basegame->string[0] && Q_stricmp(fs_basegame->string, gameName)) {
		if (fs_cdpath->string[0]) {
			FS_AddGameDirectory(fs_cdpath->string, fs_basegame->string);
		}
		if (fs_basepath->string[0]) {
			FS_AddGameDirectory(fs_basepath->string, fs_basegame->string);
		}
		if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string)) {
			FS_AddGameDirectory(fs_homepath->string, fs_basegame->string);
		}
	}

	// check for additional game folder for mods
	if (fs_gamedirvar->string[0] && Q_stricmp(fs_gamedirvar->string, gameName)) {
		if (fs_cdpath->string[0]) {
			FS_AddGameDirectory(fs_cdpath->string, fs_gamedirvar->string);
		}
		if (fs_basepath->string[0]) {
			FS_AddGameDirectory(fs_basepath->string, fs_gamedirvar->string);
		}
		if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string)) {
			FS_AddGameDirectory(fs_homepath->string, fs_gamedirvar->string);
		}
	}

	// add our commands
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("dir", FS_Dir_f);
	Cmd_AddCommand("fdir", FS_NewDir_f);
	Cmd_AddCommand("touchFile", FS_TouchFile_f);
	Cmd_AddCommand("which", FS_Which_f);

	// print the current search paths
	FS_Path_f();

	fs_gamedirvar->modified = qfalse; // We just loaded, it's not modified

	Com_Printf("----------------------\n");
	Com_Printf("%d files in pk3 files\n", fs_packFiles);
}

/*
================
FS_InitFilesystem

Called only at inital startup, not when the filesystem
is resetting due to a game change
================
*/
void FS_InitFilesystem() {
	// allow command line parms to override our defaults
	// we have to specially handle this, because normal command
	// line variable sets don't happen until after the filesystem
	// has already been initialized
	Com_StartupVariable("fs_cdpath");
	Com_StartupVariable("fs_basepath");
	Com_StartupVariable("fs_homepath");
	Com_StartupVariable("fs_game");
	Com_StartupVariable("fs_copyfiles");
	Com_StartupVariable("fs_dirbeforepak");
#ifdef MACOS_X
	Com_StartupVariable("fs_apppath");
#endif

	const char* gamedir = Cvar_VariableString("fs_game");
	bool requestbase = false;
	if (!FS_FilenameCompare(gamedir, BASEGAME))
		requestbase = true;

	if (requestbase)
		Cvar_Set2("fs_game", "", qtrue);

	// try to start up normally
	FS_Startup(BASEGAME);

	// if we can't find MD-SP-default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
	if (FS_ReadFile("MD-SP-default.cfg", nullptr) <= 0) {
		Com_Error(ERR_FATAL, "Couldn't load MD-SP-default.cfg");
		// bk001208 - SafeMode see below, FIXME?
	}

	Q_strncpyz(lastValidBase, fs_basepath->string, sizeof(lastValidBase));
	Q_strncpyz(lastValidGame, fs_gamedirvar->string, sizeof(lastValidGame));

	// bk001208 - SafeMode see below, FIXME?
}

/*
================
FS_Restart
================
*/
void FS_Restart() {
	// free anything we currently have loaded
	FS_Shutdown();

	// try to start up normally
	FS_Startup(BASEGAME);

	// if we can't find MD-SP-default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
	if (FS_ReadFile("MD-SP-default.cfg", nullptr) <= 0) {
		// this might happen when connecting to a pure server not using BASEGAME/pak0.pk3
		// (for instance a TA demo server)
		if (lastValidBase[0]) {
			Cvar_Set("fs_basepath", lastValidBase);
			Cvar_Set("fs_game", lastValidGame);
			lastValidBase[0] = '\0';
			lastValidGame[0] = '\0';
			FS_Restart();
			Com_Error(ERR_DROP, "Invalid game folder");
		}
		Com_Error(ERR_FATAL, "Couldn't load MD-SP-default.cfg");
	}

	if (Q_stricmp(fs_gamedirvar->string, lastValidGame)) {
		// skip the jaconfig.cfg if "safe" is on the command line
		if (!Com_SafeMode()) {
			Cbuf_AddText("exec " Q3CONFIG_NAME "\n");
		}
	}

	Q_strncpyz(lastValidBase, fs_basepath->string, sizeof(lastValidBase));
	Q_strncpyz(lastValidGame, fs_gamedirvar->string, sizeof(lastValidGame));
}

/*
=================
FS_ConditionalRestart

Restart if necessary
=================
*/
qboolean FS_ConditionalRestart() {
	if (fs_gamedirvar->modified) {
		FS_Restart();
		return qtrue;
	}
	return qfalse;
}

/*
========================================================================================

Handle based file calls for virtual machines

========================================================================================
*/

int		FS_FOpenFileByMode(const char* qpath, fileHandle_t* f, const fsMode_t mode) {
	int		r;

	qboolean sync = qfalse;

	switch (mode) {
	case FS_READ:
		r = FS_FOpenFileRead(qpath, f, qtrue);
		break;
	case FS_WRITE:
		*f = FS_FOpenFileWrite(qpath);
		r = 0;
		if (*f == 0) {
			r = -1;
		}
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
		*f = FS_FOpenFileAppend(qpath);
		r = 0;
		if (*f == 0) {
			r = -1;
		}
		break;
	default:
		Com_Error(ERR_FATAL, "FSH_FOpenFile: bad mode");
	}

	if (!f) {
		return r;
	}

	if (*f) {
		fsh[*f].fileSize = r;
	}
	fsh[*f].handleSync = sync;

	return r;
}

int		FS_FTell(const fileHandle_t f) {
	int pos;
	if (fsh[f].zipFile == qtrue) {
		pos = unztell(fsh[f].handleFiles.file.z);
	}
	else {
		pos = ftell(fsh[f].handleFiles.file.o);
	}
	return pos;
}

void	FS_Flush(const fileHandle_t f) {
	fflush(fsh[f].handleFiles.file.o);
}

void FS_FilenameCompletion(const char* dir, const char* ext, const qboolean stripExt, const callbackFunc_t callback, qboolean allowNonPureFilesOnDisk) {
	int nfiles;

	char** filenames = FS_ListFilteredFiles(dir, ext, nullptr, &nfiles);

	FS_SortFileList(filenames, nfiles);

	// pass all the files to callback (FindMatches)
	for (int i = 0; i < nfiles; i++) {
		char filename[MAX_STRING_CHARS];
		FS_ConvertPath(filenames[i]);
		Q_strncpyz(filename, filenames[i], MAX_STRING_CHARS);

		if (stripExt)
			COM_StripExtension(filename, filename, sizeof(filename));

		callback(filename);
	}
	FS_FreeFileList(filenames);
}

const char* FS_GetCurrentGameDir(const bool emptybase)
{
	if (fs_gamedirvar->string[0])
		return fs_gamedirvar->string;

	return emptybase ? "" : BASEGAME;
}

qboolean FS_WriteToTemporaryFile(const void* data, size_t dataLength, char** tempFilePath)
{
	// SP doesn't need to do this.
	return qfalse;
}