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

using Animationstyles_t = enum Animationstyles_t
{                      // g_AnimationStyle number list
	CS_DEFAULT,        // 0
	CS_ANAKIN,         // 1
	CS_BATTLEDROID,    // 2
	CS_BENKENOBI,      // 3
	CS_CAL_KESTIS,     // 4
	CS_CLONETROOPER,   // 5
	CS_DARKFORCES2,    // 6
	CS_COUNT_DOOKU,    // 7
	CS_GALEN_MAREK,    // 8
	CS_QUI_GON_JINN,   // 9
	CS_GRIEVOUS,       // 10
	CS_JANGO,          // 11
	CS_KOTOR,          // 12
	CS_LUKE_SKYWALKER, // 13
	CS_MACE_WINDU,     // 14
	CS_MAUL,           // 15
	CS_MOVIEDUELS,     // 16
	CS_OBIWAN,         // 17
	CS_OBIWAN_EP3,     // 18
	CS_PALPATINE,      // 19
	CS_REBELS,         // 20
	CS_KYLO_REN,       // 21
	CS_REY,            // 22
	CS_VADER,          // 23
	CS_YODA,           // 24

	CS_NUM_ANIMATION_STYLES
};

typedef struct
{
	qboolean isDefault;
	qboolean isAnakin;
	qboolean isBattleDroid;
	qboolean isBenKenobi;
	qboolean isCalKestis;
	qboolean isCloneTrooper;
	qboolean isDarkForces2;
	qboolean isCountDooku;
	qboolean isGalenMarek;
	qboolean isQuiGonJinn;
	qboolean isGrievous;
	qboolean isJango;
	qboolean isKotor;
	qboolean isLukeSkywalker;
	qboolean isMaceWindu;
	qboolean isMaul;
	qboolean isMovieDuels;
	qboolean isObiWan;
	qboolean isObiWanEP3;
	qboolean isPalpatine;
	qboolean isRebels;
	qboolean isKyloRen;
	qboolean isRey;
	qboolean isVader;
	qboolean isYoda;
} animFlags_t;

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
