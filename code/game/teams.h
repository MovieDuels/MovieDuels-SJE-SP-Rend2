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

#ifndef TEAMS_H
#define TEAMS_H

using team_t = enum team_t //# team_e
{
	TEAM_FREE,
	// caution, some code checks avia "if (!team_t_varname)" so I guess this should stay as entry 0, great or what? -slc
	TEAM_PLAYER,
	TEAM_ENEMY,
	TEAM_NEUTRAL,
	// most droids are team_neutral, there are some exceptions like Probe,Seeker,Interrogator
	TEAM_SOLO,
	// Kill everyone
	TEAM_PROJECTION,
	// Kill everyone

	//# #eol
	TEAM_NUM_TEAMS
};

using faction_t = enum faction_t //# team_e
{
	FACTION_SOLO,
	FACTION_LIGHT,
	FACTION_DARK,
	FACTION_NEUTRAL,
	FACTION_KOTOR,

	//# #eol
	TEAM_NUM_FACTIONS
};

using Animationstyles_t = enum Animationstyles_t //# animationstyles_e
{
	CS_DEFAULT,        // "g_AnimationStyle 0"  //Uses the default _humanoid
	CS_ANAKIN,         // "g_AnimationStyle 1"  //Uses the Anakin style
	CS_BATTLEDROID,    // "g_AnimationStyle 2"  //Uses the Battle Droid style
	CS_BENKENOBI,      // "g_AnimationStyle 3"  //Uses the Ben Kenobi style
	CS_CAL_KESTIS,     // "g_AnimationStyle 4"  //Uses the Cal Kestis style
	CS_CLONETROOPER,   // "g_AnimationStyle 5"  //Uses the Clone Trooper style
	CS_DROIDEKA,       // "g_AnimationStyle 6"  //Uses the Droideka style
	CS_DARKFORCES2,    // "g_AnimationStyle 7"  //Uses the Dark Forces 2 style
	CS_COUNT_DOOKU,    // "g_AnimationStyle 8"  //Uses the Count Dooku style
	CS_GALEN_MAREK,    // "g_AnimationStyle 9"  //Uses the Galen Marek style
	CS_QUI_GON_JINN,   // "g_AnimationStyle 10"  //Uses the Qui-Gon Jinn style
	CS_GRIEVOUS,       // "g_AnimationStyle 11"  //Uses the General Grievous style
	CS_JABBA,          // "g_AnimationStyle 12"  //Uses the Jabba style
	CS_JANGO,          // "g_AnimationStyle 13"  //Uses the Jango style
	CS_KOTOR,          // "g_AnimationStyle 14"  //Uses the KOTOR style
	CS_LUKE_SKYWALKER, // "g_AnimationStyle 15"  //Uses the Luke Skywalker style
	CS_MACE_WINDU,     // "g_AnimationStyle 16"  //Uses the Mace Windu style
	CS_MAUL,           // "g_AnimationStyle 17"  //Uses the Darth Maul style
	CS_MOVIEDUELS,     // "g_AnimationStyle 18"  //Uses the Movie Duels style
	CS_MELEE,          // "g_AnimationStyle 19"  //Uses the Melee style
	CS_OBIWAN,         // "g_AnimationStyle 20"  //Uses the Obi-Wan style
	CS_OBIWAN_EP3,     // "g_AnimationStyle 21"  //Uses the Obi-Wan style from Episode 3
	CS_PALPATINE,      // "g_AnimationStyle 22"  //Uses the Palpatine style
	CS_REBELS,         // "g_AnimationStyle 23"  //Uses the Rebels style
	CS_KYLO_REN,       // "g_AnimationStyle 24"  //Uses the Kylo Ren style
	CS_REY,            // "g_AnimationStyle 25"  //Uses the Rey style
	CS_SBD,            // "g_AnimationStyle 26"  //Uses the SBD style
	CS_VADER,          // "g_AnimationStyle 27"  //Uses the Darth Vader style
	CS_YODA,           // "g_AnimationStyle 28"  //Uses the Yoda style
	//# #eol
	CS_NUM_ANIMATION_STYLES
};

extern stringID_table_t TeamTable[];

extern stringID_table_t FactionTable[];

extern stringID_table_t AnimationstylesTable[];

// This list is made up from the model directories, this MUST be in the same order as the ClassNames array in NPC_stats.cpp
using class_t = enum class_t
{
	CLASS_NONE,
	// hopefully this will never be used by an npc, just covering all bases
	CLASS_ATST,
	// technically droid...
	CLASS_BARTENDER,
	CLASS_BESPIN_COP,
	CLASS_CLAW,
	CLASS_COMMANDO,
	CLASS_DESANN,
	CLASS_FISH,
	CLASS_FLIER2,
	CLASS_GALAK,
	CLASS_GLIDER,
	CLASS_GONK,
	// droid
	CLASS_GRAN,
	CLASS_HOWLER,
	CLASS_RANCOR,
	CLASS_SAND_CREATURE,
	CLASS_WAMPA,
	CLASS_IMPERIAL,
	CLASS_IMPWORKER,
	CLASS_INTERROGATOR,
	// droid
	CLASS_JAN,
	CLASS_JEDI,
	CLASS_KYLE,
	CLASS_LANDO,
	CLASS_LIZARD,
	CLASS_LUKE,
	CLASS_MARK1,
	// droid
	CLASS_MARK2,
	// droid
	CLASS_GALAKMECH,
	// droid
	CLASS_MINEMONSTER,
	CLASS_MONMOTHA,
	CLASS_MORGANKATARN,
	CLASS_MOUSE,
	// droid
	CLASS_MURJJ,
	CLASS_PRISONER,
	CLASS_PROBE,
	// droid
	CLASS_PROTOCOL,
	// droid
	CLASS_R2D2,
	// droid
	CLASS_R5D2,
	// droid
	CLASS_REBEL,
	CLASS_REBORN,
	CLASS_REELO,
	CLASS_REMOTE,
	CLASS_RODIAN,
	CLASS_SEEKER,
	// droid
	CLASS_SENTRY,
	CLASS_SHADOWTROOPER,
	CLASS_SABOTEUR,
	CLASS_STORMTROOPER,
	CLASS_SWAMP,
	CLASS_SWAMPTROOPER,
	CLASS_NOGHRI,
	CLASS_TAVION,
	CLASS_ALORA,
	CLASS_TRANDOSHAN,
	CLASS_UGNAUGHT,
	CLASS_JAWA,
	CLASS_WEEQUAY,
	CLASS_TUSKEN,
	CLASS_BOBAFETT,
	CLASS_ROCKETTROOPER,
	CLASS_SABER_DROID,
	CLASS_ASSASSIN_DROID,
	CLASS_HAZARD_TROOPER,
	CLASS_PLAYER,
	CLASS_VEHICLE,
	CLASS_MANDALORIAN,
	CLASS_JANGO,
	CLASS_SBD,
	CLASS_BATTLEDROID,
	CLASS_DROIDEKA,
	CLASS_WOOKIE,
	CLASS_CLONETROOPER,
	CLASS_STORMCOMMANDO,
	CLASS_VADER,
	CLASS_SITHLORD,
	CLASS_GALEN,
	CLASS_GUARD,
	CLASS_YODA,
	CLASS_OBJECT,
	CLASS_AHSOKA,
	CLASS_JANGODUAL,
	CLASS_BOC,
	CLASS_PROJECTION,
	CLASS_JEDIMASTER,
	CLASS_GROGU,
	CLASS_CALONORD,

	CLASS_NUM_CLASSES
};

extern stringID_table_t ClassTable[];

#endif	// #ifndef TEAMS_H
