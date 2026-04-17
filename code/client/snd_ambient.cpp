/*
===========================================================================
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

// Ambient Sound System (ASS!)
#include "../server/exe_headers.h"

#include "client.h"
#include "snd_ambient.h"
#include "snd_local.h"

static constexpr int MAX_SET_VOLUME = 255;

static void AS_GetGeneralSet(ambientSet_t&);
static void AS_GetLocalSet(ambientSet_t&);
static void AS_GetBModelSet(ambientSet_t&);

//Current set and old set for crossfading
static int currentSet = -1;
static int oldSet = -1;
static int crossDelay = 1000; //1 second

static int currentSetTime = 0;
static int oldSetTime = 0;

// Globals for debug purposes
static int numSets = 0;

// Main ambient sound group
static CSetGroup* aSets = nullptr;

// Globals for speed, blech
static char* parseBuffer = nullptr;
static int parseSize = 0;
static int parsePos = 0;
static char tempBuffer[1024];

//NOTENOTE: Be sure to change the mirrored code in g_spawn.cpp, and cg_main.cpp
using namePrecache_m = std::map<sstring_t, unsigned char>;
static namePrecache_m* pMap;

// Used for enum / string matching
static const char* setNames[NUM_AS_SETS] =
{
	"generalSet",
	"localSet",
	"bmodelSet",
};

// Used for enum / function matching
static constexpr parseFunc_t parseFuncs[NUM_AS_SETS] =
{
	AS_GetGeneralSet,
	AS_GetLocalSet,
	AS_GetBModelSet,
};

// Used for keyword / enum matching
static const char* keywordNames[NUM_AS_KEYWORDS] =
{
	"timeBetweenWaves",
	"subWaves",
	"loopedWave",
	"volRange",
	"radius",
	"type",
	"amsdir",
	"outdir",
	"basedir",
};

CSetGroup::CSetGroup()
{
	m_ambientSets = new std::vector<ambientSet_t*>;
	m_setMap = new std::map<sstring_t, ambientSet_t*>;
	m_numSets = 0;
}

CSetGroup::~CSetGroup()
{
	delete m_ambientSets;
	delete m_setMap;
}

/*
-------------------------
Free
-------------------------
*/

void CSetGroup::Free()
{
	for (const auto& ambientSet : *m_ambientSets)
	{
		Z_Free(ambientSet);
	}

	//Do this in place of clear() so it *really* frees the memory.
	delete m_ambientSets;
	delete m_setMap;
	m_ambientSets = new std::vector<ambientSet_t*>;
	m_setMap = new std::map<sstring_t, ambientSet_t*>;

	m_numSets = 0;
}

/*
-------------------------
AddSet
-------------------------
*/

ambientSet_t* CSetGroup::AddSet(const char* name)
{
	//Allocate the memory
	const auto set = static_cast<ambientSet_t*>(Z_Malloc(sizeof(ambientSet_t), TAG_AMBIENTSET, qtrue));

	//Set up some defaults
	Q_strncpyz(set->name, name, sizeof(set->name));
	set->loopedVolume = MAX_SET_VOLUME;
	set->masterVolume = MAX_SET_VOLUME;
	set->radius = 250;
	set->time_start = 10;
	set->time_end = 25;

	set->volRange_start = MAX_SET_VOLUME;
	set->volRange_end = MAX_SET_VOLUME;

	m_ambientSets->insert(m_ambientSets->end(), set);

	set->id = m_numSets++;

	//Map the name to the pointer for reference later
	(*m_setMap)[name] = set;

	return set;
}

/*
-------------------------
GetSet
-------------------------
*/

ambientSet_t* CSetGroup::GetSet(const char* name) const
{
	if (name == nullptr)
		return nullptr;

	const auto mi = m_setMap->find(name);

	if (mi == m_setMap->end())
		return nullptr;

	return (*mi).second;
}

ambientSet_t* CSetGroup::GetSet(const int ID) const
{
	if (m_ambientSets->empty())
		return nullptr;

	if (ID < 0)
		return nullptr;

	if (ID >= m_numSets)
		return nullptr;

	return (*m_ambientSets)[ID];
}

/*
===============================================

File Parsing

===============================================
*/

/*
-------------------------
AS_GetSetNameIDForString
-------------------------
*/

static int AS_GetSetNameIDForString(const char* name)
{
	//Make sure it's valid
	if (name == nullptr || name[0] == '\0')
		return -1;

	for (int i = 0; i < NUM_AS_SETS; i++)
	{
		if (Q_stricmp(name, setNames[i]) == 0)
			return i;
	}

	return -1;
}

/*
-------------------------
AS_GetKeywordIDForString
-------------------------
*/

static int AS_GetKeywordIDForString(const char* name)
{
	//Make sure it's valid
	if (name == nullptr || name[0] == '\0')
		return -1;

	for (int i = 0; i < NUM_AS_KEYWORDS; i++)
	{
		if (Q_stricmp(name, keywordNames[i]) == 0)
			return i;
	}

	return -1;
}

/*
-------------------------
AS_SkipLine

Skips a line in the character buffer
-------------------------
*/

static void AS_SkipLine()
{
	if (parsePos > parseSize) // needed to avoid a crash because of some OOR access that shouldn't be done
		return;

	while ((parseBuffer[parsePos] != '\n') && (parseBuffer[parsePos] != '\r'))
	{
		parsePos++;

		if (parsePos > parseSize)
			return;
	}

	parsePos++;
}

/*
-------------------------
AS_GetTimeBetweenWaves

Parses:
	timeBetweenWaves <start> <end>

Stores the values into the ambientSet_t.
-------------------------
*/
static void AS_GetTimeBetweenWaves(ambientSet_t& set)
{
	int startTime = 0;
	int endTime = 0;

	// Attempt to parse two integers from the current line.
	// sscanf return value must be checked to avoid C6031 warning.
	const int parsed = sscanf(parseBuffer + parsePos, "%s %d %d", tempBuffer, &startTime, &endTime);

	if (parsed != 3)
	{
		// Debug print instead of assert (Rule 9)
		Com_Printf(S_COLOR_RED "AS_GetTimeBetweenWaves: WARNING - malformed timeBetweenWaves line. "
			"Expected 2 integers, got %d\n", parsed);

		// Skip line and leave set unchanged (Rule 4: preserve behaviour)
		AS_SkipLine();
		return;
	}

	// If start > end, swap them (original behaviour preserved)
	if (startTime > endTime)
	{
#ifndef FINAL_BUILD
		Com_Printf(S_COLOR_YELLOW
			"AS_GetTimeBetweenWaves: Corrected swapped start/end times in a \"timeBetweenWaves\" keyword\n");
#endif

		const int swap = startTime;
		startTime = endTime;
		endTime = swap;
	}

	// Store parsed values
	set.time_start = startTime;
	set.time_end = endTime;

	// Move parser to next line
	AS_SkipLine();
}

/*
-------------------------
AS_GetSubWaves

subWaves <directory> <wave1> <wave2> ...
Parses a directory name followed by one or more wave names.
Each wave is registered as "sound/<directory>/<wave>.wav".
-------------------------
*/
static void AS_GetSubWaves(ambientSet_t& set)
{
	char dirBuffer[512];

	// ------------------------------------------------------------
	// Parse directory name and first token after the keyword.
	// Fix C6031: check sscanf return value.
	// ------------------------------------------------------------
	const int parsed = sscanf(parseBuffer + parsePos, "%s %s", tempBuffer, dirBuffer);

	// Force null-termination to satisfy MSVC analyzer (C6054)
	dirBuffer[sizeof(dirBuffer) - 1] = '\0';

	if (parsed != 2)
	{
		Com_Printf(S_COLOR_RED
			"AS_GetSubWaves: WARNING - malformed subWaves line. Expected directory + first wave.\n");

		AS_SkipLine();
		return;
	}

	// ------------------------------------------------------------
	// Move parsePos past: "<keyword> <directory>"
	// ------------------------------------------------------------
	parsePos += (strlen(keywordNames[SET_KEYWORD_SUBWAVES]) + 1);
	parsePos += (strlen(dirBuffer) + 1);

	// ------------------------------------------------------------
	// Parse all subwave names until newline or buffer end
	// ------------------------------------------------------------
	while (parsePos <= parseSize)
	{
		char waveBuffer[256];

		// Fix C6031: check sscanf return value
		const int waveParsed = sscanf(parseBuffer + parsePos, "%s", waveBuffer);

		// Force null-termination to satisfy MSVC analyzer (C6054)
		waveBuffer[sizeof(waveBuffer) - 1] = '\0';

		if (waveParsed != 1)
		{
			Com_Printf(S_COLOR_RED
				"AS_GetSubWaves: WARNING - malformed wave entry in set \"%s\"\n", set.name);
			break;
		}

		// --------------------------------------------------------
		// Enforce maximum subwave count
		// --------------------------------------------------------
		if (set.numSubWaves >= MAX_WAVES_PER_GROUP)
		{
#ifndef FINAL_BUILD
			Com_Printf(S_COLOR_YELLOW
				"AS_GetSubWaves: WARNING - Too many subwaves on set \"%s\"\n", set.name);
#endif
		}
		else
		{
			char waveName[1024];

			// Build full path: sound/<directory>/<wave>.wav
			Com_sprintf(waveName, sizeof(waveName),
				"sound/%s/%s.wav", dirBuffer, waveBuffer);

			// Register sound and store ID
			const sfxHandle_t id = S_RegisterSound(waveName);

			if (id <= 0)
			{
#ifndef FINAL_BUILD
				Com_Printf(S_COLOR_RED
					"AS_GetSubWaves: ERROR - Unable to load ambient sound \"%s\"\n", waveName);
#endif
			}
			else
			{
				set.subWaves[set.numSubWaves++] = id;
			}
		}

		// --------------------------------------------------------
		// Advance parsePos past this wave token
		// --------------------------------------------------------
		parsePos += strlen(waveBuffer) + 1;

		// Stop if newline reached
		const char nextChar = (parseBuffer + parsePos)[0];
		if (nextChar == '\n' || nextChar == '\r')
		{
			break;
		}
	}

	AS_SkipLine();
}

/*
-------------------------
AS_GetLoopedWave

loopedWave <name>

Parses a single looped ambient sound and registers it.
-------------------------
*/
static void AS_GetLoopedWave(ambientSet_t& set)
{
	char waveBuffer[256];
	char waveName[1024];

	// ------------------------------------------------------------
	// Parse: "<keyword> <waveName>"
	// Fix C6031: check sscanf return value.
	// ------------------------------------------------------------
	const int parsed = sscanf(parseBuffer + parsePos, "%s %s", tempBuffer, waveBuffer);

	if (parsed != 2)
	{
		Com_Printf(S_COLOR_RED
			"AS_GetLoopedWave: WARNING - malformed loopedWave line. Expected a single wave name.\n");

		AS_SkipLine();
		return;
	}

	// ------------------------------------------------------------
	// Build full path: sound/<wave>.wav
	// ------------------------------------------------------------
	Com_sprintf(waveName, sizeof(waveName), "sound/%s.wav", waveBuffer);

	// ------------------------------------------------------------
	// Register the sound and store the ID
	// ------------------------------------------------------------
	const sfxHandle_t id = S_RegisterSound(waveName);

	if (id <= 0)
	{
#ifndef FINAL_BUILD
		Com_Printf(S_COLOR_RED
			"AS_GetLoopedWave: ERROR - Unable to load ambient sound \"%s\"\n", waveName);
#endif
	}
	else
	{
		set.loopedWave = id;
	}

	// ------------------------------------------------------------
	// Advance to next line
	// ------------------------------------------------------------
	AS_SkipLine();
}
/*
-------------------------
AS_GetVolumeRange

volRange <min> <max>

Parses a minimum and maximum volume range for an ambient set.
-------------------------
*/
static void AS_GetVolumeRange(ambientSet_t& set)
{
	int min = 0;
	int max = 0;

	// ------------------------------------------------------------
	// Parse: "<keyword> <min> <max>"
	// Fix C6031: check sscanf return value.
	// ------------------------------------------------------------
	const int parsed = sscanf(parseBuffer + parsePos, "%s %d %d", tempBuffer, &min, &max);

	if (parsed != 3)
	{
		Com_Printf(S_COLOR_RED
			"AS_GetVolumeRange: WARNING - malformed volRange line. Expected 2 integers.\n");

		AS_SkipLine();
		return;
	}

	// ------------------------------------------------------------
	// Swap if reversed (original behaviour preserved)
	// ------------------------------------------------------------
	if (min > max)
	{
#ifndef FINAL_BUILD
		Com_Printf(S_COLOR_YELLOW
			"AS_GetVolumeRange: Corrected swapped min/max range in a \"volRange\" keyword\n");
#endif

		const int swap = min;
		min = max;
		max = swap;
	}

	// ------------------------------------------------------------
	// Store parsed values
	// ------------------------------------------------------------
	set.volRange_start = min;
	set.volRange_end = max;

	// ------------------------------------------------------------
	// Advance to next line
	// ------------------------------------------------------------
	AS_SkipLine();
}

/*
-------------------------
AS_GetRadius

radius <value>

Parses a single integer radius value for an ambient set.
-------------------------
*/
static void AS_GetRadius(ambientSet_t& set)
{
	int radius = 0;

	// ------------------------------------------------------------
	// Parse: "<keyword> <radius>"
	// Fix C6031: check sscanf return value.
	// ------------------------------------------------------------
	const int parsed = sscanf(parseBuffer + parsePos, "%s %d", tempBuffer, &radius);

	if (parsed != 2)
	{
		Com_Printf(S_COLOR_RED
			"AS_GetRadius: WARNING - malformed radius line. Expected a single integer.\n");

		AS_SkipLine();
		return;
	}

	// ------------------------------------------------------------
	// Store parsed radius
	// ------------------------------------------------------------
	set.radius = radius;

	// ------------------------------------------------------------
	// Advance to next line
	// ------------------------------------------------------------
	AS_SkipLine();
}

/*
-------------------------
AS_GetGeneralSet
-------------------------
*/

static void AS_GetGeneralSet(ambientSet_t& set)
{
	//The other parameters of the set come in a specific order
	while (parsePos <= parseSize)
	{
		const int iFieldsScanned = sscanf(parseBuffer + parsePos, "%s", tempBuffer);
		if (iFieldsScanned <= 0)
			return;

		const int keywordID = AS_GetKeywordIDForString(reinterpret_cast<const char*>(&tempBuffer));

		//Find and parse the keyword info
		switch (keywordID)
		{
		case SET_KEYWORD_TIMEBETWEENWAVES:
			AS_GetTimeBetweenWaves(set);
			break;

		case SET_KEYWORD_SUBWAVES:
			AS_GetSubWaves(set);
			break;

		case SET_KEYWORD_LOOPEDWAVE:
			AS_GetLoopedWave(set);
			break;

		case SET_KEYWORD_VOLRANGE:
			AS_GetVolumeRange(set);
			break;

		default:

			//Check to see if we've finished this group
			if (AS_GetSetNameIDForString(reinterpret_cast<const char*>(&tempBuffer)) == -1)
			{
				//Ignore comments
				if (tempBuffer[0] == ';')
					return;

				//This wasn't a set name, so it's an error
#ifndef FINAL_BUILD
				Com_Printf(S_COLOR_YELLOW"WARNING: Unknown ambient set keyword \"%s\"\n", tempBuffer);
#endif
			}

			return;
		}
	}
}

/*
-------------------------
AS_GetLocalSet
-------------------------
*/

static void AS_GetLocalSet(ambientSet_t& set)
{
	//The other parameters of the set come in a specific order
	while (parsePos <= parseSize)
	{
		const int iFieldsScanned = sscanf(parseBuffer + parsePos, "%s", tempBuffer);
		if (iFieldsScanned <= 0)
			return;

		const int keywordID = AS_GetKeywordIDForString(reinterpret_cast<const char*>(&tempBuffer));

		//Find and parse the keyword info
		switch (keywordID)
		{
		case SET_KEYWORD_TIMEBETWEENWAVES:
			AS_GetTimeBetweenWaves(set);
			break;

		case SET_KEYWORD_SUBWAVES:
			AS_GetSubWaves(set);
			break;

		case SET_KEYWORD_LOOPEDWAVE:
			AS_GetLoopedWave(set);
			break;

		case SET_KEYWORD_VOLRANGE:
			AS_GetVolumeRange(set);
			break;

		case SET_KEYWORD_RADIUS:
			AS_GetRadius(set);
			break;

		default:

			//Check to see if we've finished this group
			if (AS_GetSetNameIDForString(reinterpret_cast<const char*>(&tempBuffer)) == -1)
			{
				//Ignore comments
				if (tempBuffer[0] == ';')
					return;

				//This wasn't a set name, so it's an error
#ifndef FINAL_BUILD
				Com_Printf(S_COLOR_YELLOW"WARNING: Unknown ambient set keyword \"%s\"\n", tempBuffer);
#endif
			}

			return;
		}
	}
}

/*
-------------------------
AS_GetBModelSet
-------------------------
*/

static void AS_GetBModelSet(ambientSet_t& set)
{
	//The other parameters of the set come in a specific order
	while (parsePos <= parseSize)
	{
		const int iFieldsScanned = sscanf(parseBuffer + parsePos, "%s", tempBuffer);
		if (iFieldsScanned <= 0)
			return;

		const int keywordID = AS_GetKeywordIDForString(reinterpret_cast<const char*>(&tempBuffer));

		//Find and parse the keyword info
		switch (keywordID)
		{
		case SET_KEYWORD_SUBWAVES:
			AS_GetSubWaves(set);
			break;

		default:

			//Check to see if we've finished this group
			if (AS_GetSetNameIDForString(reinterpret_cast<const char*>(&tempBuffer)) == -1)
			{
				//Ignore comments
				if (tempBuffer[0] == ';')
					return;

				//This wasn't a set name, so it's an error
#ifndef FINAL_BUILD
				Com_Printf(S_COLOR_YELLOW"WARNING: Unknown ambient set keyword \"%s\"\n", tempBuffer);
#endif
			}

			return;
		}
	}
}/*
-------------------------
AS_ParseSet

Parses an individual ambient set group out of a set file buffer.
-------------------------
*/
static qboolean AS_ParseSet(const int setID, CSetGroup* sg)
{
	// ------------------------------------------------------------
	// Validate set index
	// ------------------------------------------------------------
	if (setID >= NUM_AS_SETS)
	{
		return qfalse;
	}

	// Reset parser position for this run
	parsePos = 0;

	const char* name = setNames[setID];

	// ------------------------------------------------------------
	// Iterate through the entire buffer looking for occurrences
	// of the set keyword (e.g., "set1", "set2", etc.)
	// ------------------------------------------------------------
	while (parsePos <= parseSize)
	{
		// Check if this line begins with the set keyword
		if (Q_strncmp(parseBuffer + parsePos, name, strlen(name)) == 0)
		{
			numSets++;

			// Move past "<keyword> "
			parsePos += strlen(name) + 1;

			// --------------------------------------------------------
			// Parse the set name (must be first token)
			// Fix C6031: check sscanf return value
			// --------------------------------------------------------
			const int parsed = sscanf(parseBuffer + parsePos, "%s", tempBuffer);

			if (parsed != 1)
			{
				Com_Printf(S_COLOR_RED
					"AS_ParseSet: WARNING - malformed set name for setID %d\n", setID);

				AS_SkipLine();
				continue;
			}

			AS_SkipLine();

			// --------------------------------------------------------
			// Validate against precache map
			// If not precached, skip this set entirely
			// --------------------------------------------------------
			if (tempBuffer[0] != '\0')
			{
				if (pMap->find(reinterpret_cast<const char*>(&tempBuffer)) == pMap->end())
				{
					continue;
				}
			}

			// --------------------------------------------------------
			// Create a new ambient set entry
			// --------------------------------------------------------
			ambientSet_t* set = sg->AddSet(reinterpret_cast<const char*>(&tempBuffer));

			// --------------------------------------------------------
			// Parse the set's internal data using the appropriate function
			// --------------------------------------------------------
			parseFuncs[setID](*set);
			continue;
		}

		// Not found on this line — skip to next
		AS_SkipLine();
	}

	return qtrue;
}/*
-------------------------
AS_ParseHeader

Parses the directory/type information at the beginning of the file.
Stops when it finds:
	type ambientSet
-------------------------
*/
static void AS_ParseHeader()
{
	while (parsePos <= parseSize)
	{
		char typeBuffer[128];

		// ------------------------------------------------------------
		// Parse first token on the line
		// Fix C6031: check sscanf return value
		// ------------------------------------------------------------
		const int parsed = sscanf(parseBuffer + parsePos, "%s", tempBuffer);

		if (parsed != 1)
		{
			Com_Printf(S_COLOR_RED
				"AS_ParseHeader: WARNING - malformed header line (missing keyword)\n");
			AS_SkipLine();
			continue;
		}

		// Identify keyword
		const int keywordID = AS_GetKeywordIDForString(reinterpret_cast<const char*>(&tempBuffer));

		switch (keywordID)
		{
			// --------------------------------------------------------
			// TYPE keyword: must be "ambientSet"
			// --------------------------------------------------------
		case SET_KEYWORD_TYPE:
		{
			const int parsedType = sscanf(parseBuffer + parsePos, "%s %s", tempBuffer, typeBuffer);

			if (parsedType != 2)
			{
				Com_Printf(S_COLOR_RED
					"AS_ParseHeader: WARNING - malformed type line\n");
				AS_SkipLine();
				continue;
			}

			// Valid type?
			if (Q_stricmp(typeBuffer, "ambientSet") == 0)
			{
				return; // Header complete
			}

			// Invalid type → drop with error (original behaviour)
			Com_Error(ERR_DROP,
				"AS_ParseHeader: Set type \"%s\" is not a valid set type!\n",
				typeBuffer);
			break;
		}

		// --------------------------------------------------------
		// AMSDIR / OUTDIR / BASEDIR
		// Not implemented in original code — preserve behaviour
		// --------------------------------------------------------
		case SET_KEYWORD_AMSDIR:
			// TODO: Implement
			break;

		case SET_KEYWORD_OUTDIR:
			// TODO: Implement
			break;

		case SET_KEYWORD_BASEDIR:
			// TODO: Implement
			break;

		default:
			// Unknown keyword — skip line
			break;
		}

		// Move to next line
		AS_SkipLine();
	}
}

/*
-------------------------
AS_ParseFile

Opens and parses a sound set file
-------------------------
*/

static qboolean AS_ParseFile(const char* filename, CSetGroup* sg)
{
	//Open the file and read the information from it
	parseSize = FS_ReadFile(filename, reinterpret_cast<void**>(&parseBuffer));

	if (parseSize <= 0)
		return qfalse;

	//Parse the directory information out of the file
	AS_ParseHeader();

	//Parse all the relevent sets out of it
	for (int i = 0; i < NUM_AS_SETS; i++)
		AS_ParseSet(i, sg);

	//Free the memory and close the file
	FS_FreeFile(parseBuffer);

	return qtrue;
}

/*
===============================================

Main code

===============================================
*/

/*
-------------------------
AS_Init

Loads the ambient sound sets and prepares to play them when needed
-------------------------
*/

static namePrecache_m* TheNamePrecache()
{
	// we use these singletons so we can find memory leaks
	// if you let things like this leak, you never can tell
	// what is really leaking and what is merely not ever freed
	static namePrecache_m singleton;
	return &singleton;
}

void AS_Init()
{
	if (!aSets)
	{
		numSets = 0;

		pMap = TheNamePrecache();

		//Setup the structure
		aSets = new CSetGroup();
		aSets->Init();
	}
}

/*
-------------------------
AS_AddPrecacheEntry
-------------------------
*/

void AS_AddPrecacheEntry(const char* name)
{
	if (!pMap) //s_initsound 0 probably
	{
		return;
	}
	if (!Q_stricmp(name, "#clear"))
	{
		pMap->clear();
		currentSet = -1;
		oldSet = -1;
	}
	else
	{
		(*pMap)[name] = 1;
	}
}

/*
-------------------------
AS_ParseSets

Called on the client side to load and precache all the ambient sound sets
-------------------------
*/

void AS_ParseSets()
{
	const cvar_t* cv = Cvar_Get("s_initsound", "1", 0);
	if (!cv->integer)
	{
		return;
	}
	AS_Init();

	//Parse all the sets
	if (AS_ParseFile(AMBIENT_SET_FILENAME, aSets) == qfalse)
	{
		Com_Error(ERR_FATAL, S_COLOR_RED"ERROR: Couldn't load ambient sound sets from %s", AMBIENT_SET_FILENAME);
	}

	//Com_Printf( "AS_ParseFile: Loaded %d of %d ambient set(s)\n", pMap.size(), numSets );

	int iErrorsOccured = 0;
	for (auto& it : *pMap)
	{
		const char* str = it.first.c_str();
		const ambientSet_t* aSet = aSets->GetSet(str);
		if (!aSet)
		{
			// I print these red instead of yellow because they're going to cause an ERR_DROP if they occur
			Com_Printf(S_COLOR_RED"ERROR: AS_ParseSets: Unable to find ambient soundset \"%s\"!\n", str);
			iErrorsOccured++;
		}
	}

	if (iErrorsOccured)
	{
		Com_Error(ERR_DROP, "....%d missing sound sets! (see above)\n", iErrorsOccured);
	}

	//	//Done with the precache info, it will be rebuilt on a restart
	//	pMap->clear();	// do NOT do this here now
}

/*
-------------------------
AS_Free

Frees up the ambient sound system
-------------------------
*/

void AS_Free()
{
	if (aSets)
	{
		aSets->Free();
		delete aSets;
		aSets = nullptr;

		currentSet = -1;
		oldSet = -1;

		currentSetTime = 0;
		oldSetTime = 0;

		numSets = 0;
	}
}

/*
===============================================

Sound code

===============================================
*/

/*
-------------------------
AS_UpdateSetVolumes

Fades volumes up or down depending on the action being taken on them.
-------------------------
*/

static void AS_UpdateSetVolumes()
{
	float scale;
	int deltaTime;

	//Get the sets and validate them
	ambientSet_t* current = aSets->GetSet(currentSet);

	if (current == nullptr)
		return;

	if (current->masterVolume < MAX_SET_VOLUME)
	{
		deltaTime = cls.realtime - current->fadeTime;
		scale = (static_cast<float>(deltaTime) / static_cast<float>(crossDelay));
		current->masterVolume = static_cast<int>((scale) * static_cast<float>(MAX_SET_VOLUME));
	}

	if (current->masterVolume > MAX_SET_VOLUME)
		current->masterVolume = MAX_SET_VOLUME;

	//Only update the old set if it's still valid
	if (oldSet == -1)
		return;

	ambientSet_t* old = aSets->GetSet(oldSet);

	if (old == nullptr)
		return;

	//Update the volumes
	if (old->masterVolume > 0)
	{
		deltaTime = cls.realtime - old->fadeTime;
		scale = (static_cast<float>(deltaTime) / static_cast<float>(crossDelay));
		old->masterVolume = MAX_SET_VOLUME - static_cast<int>((scale) * static_cast<float>(MAX_SET_VOLUME));
	}

	if (old->masterVolume <= 0)
	{
		old->masterVolume = 0;
		oldSet = -1;
	}
}

/*
-------------------------
S_UpdateCurrentSet

Does internal maintenance to keep track of changing sets.
-------------------------
*/

static void AS_UpdateCurrentSet(const int id)
{
	//Check for a change
	if (id != currentSet)
	{
		//This is new, so start the fading
		oldSet = currentSet;
		currentSet = id;

		ambientSet_t* old = aSets->GetSet(oldSet);
		ambientSet_t* current = aSets->GetSet(currentSet);
		// Ste, I just put this null check in for now, not sure if there's a more graceful way to exit this function - dmv
		if (!current)
		{
			return;
		}
		if (old)
		{
			old->masterVolume = MAX_SET_VOLUME;
			old->fadeTime = cls.realtime;
		}

		current->masterVolume = 0;

		//Set the fading starts
		current->fadeTime = cls.realtime;
	}

	//Update their volumes if fading
	AS_UpdateSetVolumes();
}

/*
-------------------------
AS_PlayLocalSet

Plays a local set taking volume and subwave playing into account.
Alters lastTime to reflect the time updates.
-------------------------
*/

static void AS_PlayLocalSet(vec3_t listener_origin, vec3_t origin, const ambientSet_t* set, const int entID, int* lastTime)
{
	vec3_t dir;
	const int time = cl.serverTime;

	//Make sure it's valid
	if (set == nullptr)
		return;

	VectorSubtract(origin, listener_origin, dir);
	const float dist = VectorLength(dir);

	//Determine the volume based on distance (NOTE: This sits on top of what SpatializeOrigin does)
	const float distScale = (dist < (set->radius * 0.5f)) ? 1 : (set->radius - dist) / (set->radius * 0.5f);
	unsigned char volume = (distScale > 1.0f || distScale < 0.0f)
		? 0
		: static_cast<unsigned char>(set->masterVolume * distScale);

	//Add the looping sound
	if (set->loopedWave)
		S_AddAmbientLoopingSound(origin, volume, set->loopedWave);

	//Check the time to start another one-shot subwave
	if ((time - *lastTime) < ((Q_irand(set->time_start, set->time_end)) * 1000))
		return;

	//Update the time
	*lastTime = time;

	//Scale the volume ranges for the subwaves based on the overall master volume
	const float volScale = static_cast<float>(volume) / static_cast<float>(MAX_SET_VOLUME);
	volume = static_cast<unsigned char>(Q_irand(static_cast<int>(volScale * set->volRange_start),
		static_cast<int>(volScale * set->volRange_end)));

	//Add the random subwave
	if (set->numSubWaves)
		S_StartAmbientSound(origin, entID, volume, set->subWaves[Q_irand(0, set->numSubWaves - 1)]);
}

/*
-------------------------
AS_PlayAmbientSet

Plays an ambient set taking volume and subwave playing into account.
Alters lastTime to reflect the time updates.
-------------------------
*/

static void AS_PlayAmbientSet(vec3_t origin, const ambientSet_t* set, int* lastTime)
{
	const int time = cls.realtime;

	//Make sure it's valid
	if (set == nullptr)
		return;

	//Add the looping sound
	if (set->loopedWave)
		S_AddAmbientLoopingSound(origin, static_cast<unsigned char>(set->masterVolume), set->loopedWave);

	//Check the time to start another one-shot subwave
	if ((time - *lastTime) < ((Q_irand(set->time_start, set->time_end)) * 1000))
		return;

	//Update the time
	*lastTime = time;

	//Scale the volume ranges for the subwaves based on the overall master volume
	const float volScale = static_cast<float>(set->masterVolume) / static_cast<float>(MAX_SET_VOLUME);
	unsigned char volume = Q_irand(static_cast<int>(volScale * set->volRange_start),
		static_cast<int>(volScale * set->volRange_end));

	//Allow for softer noises than the masterVolume, but not louder
	if (volume > set->masterVolume)
		volume = set->masterVolume;

	//Add the random subwave
	if (set->numSubWaves)
		S_StartAmbientSound(origin, 0, volume, set->subWaves[Q_irand(0, set->numSubWaves - 1)]);
}

/*
-------------------------
S_UpdateAmbientSet

Does maintenance and plays the ambient sets (two if crossfading)
-------------------------
*/

void S_UpdateAmbientSet(const char* name, vec3_t origin)
{
	if (aSets == nullptr)
	{
		return;
	}
	const ambientSet_t* set = aSets->GetSet(name);

	if (set == nullptr)
		return;

	//Update the current and old set for crossfading
	AS_UpdateCurrentSet(set->id);

	const ambientSet_t* current = aSets->GetSet(currentSet);
	const ambientSet_t* old = aSets->GetSet(oldSet);

	if (current)
		AS_PlayAmbientSet(origin, set, &currentSetTime);

	if (old)
		AS_PlayAmbientSet(origin, old, &oldSetTime);
}

/*
-------------------------
S_AddLocalSet
-------------------------
*/

int S_AddLocalSet(const char* name, vec3_t listener_origin, vec3_t origin, const int entID, const int time)
{
	int current_time;

	const ambientSet_t* set = aSets->GetSet(name);

	if (set == nullptr)
		return cl.serverTime;

	current_time = time;

	AS_PlayLocalSet(listener_origin, origin, set, entID, &current_time);

	return current_time;
}

/*
-------------------------
AS_GetBModelSound
-------------------------
*/

sfxHandle_t AS_GetBModelSound(const char* name, const int stage)
{
	const ambientSet_t* set = aSets->GetSet(name);

	if (set == nullptr)
		return -1;

	//Stage must be within a valid range
	if ((stage > (set->numSubWaves - 1)) || (stage < 0))
		return -1;

	return set->subWaves[stage];
}