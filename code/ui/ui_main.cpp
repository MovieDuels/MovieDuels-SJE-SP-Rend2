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

/*
=======================================================================

USER INTERFACE MAIN

=======================================================================
*/

#include <algorithm>
#include <vector>

#include "../server/exe_headers.h"

#include "ui_local.h"

#include "menudef.h"

#include "ui_shared.h"

#include "../game/bg_public.h"
#include "../game/anims.h"
extern stringID_table_t anim_table[MAX_ANIMATIONS + 1];

#include "../qcommon/stringed_ingame.h"
#include "../qcommon/stv_version.h"
#include "../qcommon/q_shared.h"

#ifdef NEW_FEEDER
static int uiModelIndex = 0;
static int uiVariantIndex = 0;

extern qhandle_t mdBorder;
extern qhandle_t mdBorderSel;
extern qhandle_t mdBackground;

typedef enum {
	ERA_OLD_REPUBLIC,
	ERA_SITH_EMPIRE,
	ERA_REPUBLIC,
	ERA_SEPARATIST,
	ERA_REBELS,
	ERA_EMPIRE,
	ERA_RESISTANCE,
	ERA_FIRST_ORDER,
	TOTAL_ERAS,
} eraMD_t;
static int uiEra = 0;

#ifdef NEW_FEEDER_V1
stringID_table_t era_table[TOTAL_ERAS] = {
	ENUM2STRING(ERA_OLD_REPUBLIC),
	ENUM2STRING(ERA_SITH_EMPIRE),
	ENUM2STRING(ERA_REPUBLIC),
	ENUM2STRING(ERA_SEPARATIST),
	ENUM2STRING(ERA_REBELS),
	ENUM2STRING(ERA_EMPIRE),
	ENUM2STRING(ERA_RESISTANCE),
	ENUM2STRING(ERA_FIRST_ORDER),
};
#endif

static constexpr char* eraNames[TOTAL_ERAS]{
	"Old Republic",
	"Sith Empire",
	"Galactic Republic",
	"Separatist Alliance",
	"Rebel Alliance",
	"Galactic Empire",
	"Resistance",
	"First Order",
	//"TOTAL_ERA",
};

typedef struct {
	const char* name;
	const char* title;
	const char* icon;
	const char* npc;

	const char* model;
	const char* skin;

	const char* saber1;
	const char* saber2;

	const char* color1;
	const char* color2;


	int				style;
	int				styleBitFlag;

	eraMD_t			era;

	int 		    selectAnimation;
	int				plyAnimation; // Disabled as menu closes on selection
	int 		    npcAnimation; // Disabled as menu closes on selection 

	const char* plySelectSound;
	const char* npcSelectSound;

	float		fov;
	const char* desc;

	bool		isSubDiv; // hasSubDiv
} charMD_t;

static constexpr charMD_t charMD[] = {
	//name								title									icon			npc								model					skin							saber1						saber2					color1			color2		style	sbFlag	era				
	
	// Old Republic
	{ "Atris",							"[Jedi Master]",						"",				"md_atris",						"atris",				"model_default",				"revan",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/atris/misc/anger2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ATRIS",						qfalse },
	{ "Atton Rand",						"[KOTOR]",								"",				"md_atton",						"atton",				"model_default",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/atton/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ATTON_RAND",					qfalse },
	{ "Bao Dur",						"[KOTOR]",								"",				"md_baodur",					"bao_dur",				"model_default",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bao/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_BAO_DUR",					qfalse },
	{ "Bastila Shan",					"[Jedi Battle Robes]",					"",				"md_bastila",					"bastila",				"model_default",				"bastila_staff",			"",						"yellow",		"",			7,		128,	ERA_OLD_REPUBLIC,	BOTH_SABERSTAFF_STANCE,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bastila/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BASTILA_SHAN",				qfalse },
	{ "Bastila Shan",					"[Jedi Battle Robes - Hooded]",			"",				"md_bastila_robed",				"bastila",				"model_robe",					"bastila_staff",			"",						"yellow",		"",			7,		128,	ERA_OLD_REPUBLIC,	BOTH_SABERSTAFF_STANCE,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bastila/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BASTILA_SHAN",				 qtrue },
	{ "Bastila Shan",					"[Jedi Battle Robes - Single Saber]",	"",				"md_bastila_single",			"bastila",				"model_default",				"bastila_single",			"",						"yellow",		"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bastila/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BASTILA_SHAN",				 qtrue },
	{ "Bastila Shan",					"[Dark Jedi]",							"",				"md_bastila_dark",				"jedi_kotor_fem",		"model_bastila",				"bastila_staff",			"",						"red",			"",			7,		128,	ERA_OLD_REPUBLIC,	BOTH_SABERSTAFF_STANCE,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bastila/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BASTILA_SHAN",				 qtrue },
	{ "Canderous Ordo",					"[Mandalorian]",						"",				"md_canderous",					"canderous",			"model_default",				"menu_wp_repeater_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/canderous/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CANDEROUS_ORDO",				qfalse },
	{ "Canderous Ordo",					"[Mandalorian Armor]",					"",				"md_canderous_armor",			"canderous_mando",		"model_default",				"menu_wp_repeater_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/canderous/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CANDEROUS_ORDO",				 qtrue },
	{ "Canderous Ordo",					"[Mandalorian Armor - No Helmet]",		"",				"md_canderous_armor_nh",		"canderous_mando",		"model_nohelmet",				"menu_wp_repeater_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/canderous/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CANDEROUS_ORDO",				 qtrue },
	{ "Canderous Ordo",					"[Mandalorian Armor - Beard]",			"",				"md_canderous_armor_nh2",		"canderous_mando",		"model_nohelmet2",				"menu_wp_repeater_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/canderous/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CANDEROUS_ORDO",				 qtrue },
	{ "Carth Onasi",					"[KOTOR]",								"",				"md_carth",						"carth",				"model_default",				"menu_wp_clonepistol_kotor", "menu_wp_clonepistol_kotor", "",		"",			0,		0,		ERA_OLD_REPUBLIC,	BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel1/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_CARTH_ONASI",				qfalse },
	{ "Dorak",							"[Jedi]",								"",				"jedi_dorak",					"jedi_kotor",			"model_dorak",					"single_2",					"",						"purple",		"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dorak/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DORAK",						qfalse },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor",					"jedi_kotor",			"model_default_new",			"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/exile/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					qfalse },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor2",					"jedi_kotor",			"model_deadeye",				"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jediconsular/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor3",					"jedi_kotor",			"model_gerevick",				"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedimaster/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor4",					"jedi_kotor",			"model_marl",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedimaster/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor5",					"jedi_kotor",			"model_jedi1",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jediguardian/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor6",					"jedi_kotor",			"model_jedi2",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_tor/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor7",					"jedi_kotor",			"model_jedi3",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedisentinel/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[KOTOR]",								"",				"jedi_kotor8",					"jedi_kotor",			"model_jedi4",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jediguardian/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Knight",					"[Twilek]",								"",				"jedi_kotor9",					"jedi_kotor",			"model_twilek",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/twilek/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_KOTOR",					 qtrue },
	{ "Jedi Warrior",					"[Jedi]",								"",				"md_jedi_warrior",				"sith_eradicator",		"model_jedi",					"aayla",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jtguard/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_WARRIOR",				qfalse },
	{ "Jolee Bindo",					"[Jedi]",								"",				"md_jolee",						"jolee_bindo",			"model_default",				"jolee",					"",						"green",		"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jolee/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JOLEE_BINDO",				qfalse },
	{ "Jolee Bindo",					"[Jedi]",								"",				"jedi_jolee",					"jedi_kotor",			"model_jolee",					"jolee",					"",						"green",		"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jolee/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JOLEE_BINDO",				 qtrue },
	{ "Juhani",							"[Jedi]",								"",				"md_juhani",					"juhani",				"model_default",				"juhani",					"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/juhani/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JUHANI",						qfalse },
	{ "Juhani",							"[Jedi - Alternative]",					"",				"md_juhani_alt",				"juhani",				"model_alt",					"juhani",					"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/juhani/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JUHANI",						 qtrue },
	{ "Juhani",							"[Jedi - Robed]",						"",				"md_juhani_jedir",				"jedi_kotor_fem",		"model_default",				"juhani",					"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/juhani/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JUHANI",						 qtrue },
	{ "Kao Cen Darach",					"[Jedi Master]",						"",				"md_kao",						"jedi_tor",				"model_default",				"jolee",					"",						"green",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_tor/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KAO_CEN_DARACH",				qfalse },
	{ "Kreia",							"[Jedi]",								"",				"md_kreia",						"kreia",				"model_default",				"revan",					"",						"green",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kreia/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KREIA",						qfalse },
	{ "Meetra Surik",					"[Jedi]",								"",				"md_meetra",					"meetra",				"model_default",				"jolee",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MEETRA_SURIK",				qfalse },
	{ "Meetra Surik",					"[Jedi - Robed]",						"",				"md_meetra_robed",				"meetra",				"model_robed",					"jolee",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MEETRA_SURIK",				 qtrue },
	{ "Meetra Surik",					"[Jedi - Hooded]",						"",				"md_meetra_hooded",				"meetra",				"model_hooded",					"jolee",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MEETRA_SURIK",				 qtrue },
	{ "Republic Officer",				"[KOTOR]",								"",				"md_officer_or",				"OR_officer",			"model_default",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel1/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_OFFICER",			qfalse },
	{ "Republic Officer",				"[KOTOR]",								"",				"md_officer2_or",				"OR_officer",			"model_default2",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel1/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_OFFICER",			 qtrue },
	{ "Republic Soldier",				"[KOTOR]",								"",				"md_trooper_or",				"OR_trooper",			"model_default",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel1/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER",			qfalse },
	{ "Republic Trooper",				"[Soldier - Rifle]",					"",				"md_reptrooper",				"republictrooper",		"model_default",				"menu_wp_rebelrifle_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/republictrooper/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER_KOTOR",		qfalse },
	{ "Republic Trooper",				"[Soldier - Sniper]",					"",				"md_reptrooper_disruptor",		"republictrooper",		"model_default",				"menu_wp_disruptor_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/republictrooper/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER_KOTOR",		 qtrue },
	{ "Republic Trooper",				"[Belsavis]",							"",				"md_reptrooper_belsavis",		"republictrooper",		"model_belsavis",				"menu_wp_rebelrifle_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/republictrooper/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER_KOTOR",		 qtrue },
	{ "Republic Trooper",				"[Havoc]",								"",				"md_reptrooper_havoc",			"republictrooper",		"model_havoc",					"menu_wp_rebelrifle_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/republictrooper/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER_KOTOR",		 qtrue },
	{ "Republic Trooper",				"[Havoc - Minigun]",					"",				"md_reptrooper_havoc_minigun",	"republictrooper",		"model_havoc",					"menu_wp_z6_rotary_cannon",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/republictrooper/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER_KOTOR",		 qtrue },
	{ "Republic Trooper",				"[MA-35]",								"",				"md_reptrooper_ma35",			"republictrooper",		"model_ma35",					"menu_wp_rebelrifle_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/republictrooper/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_TROOPER_KOTOR",		 qtrue },
	{ "Revan",							"[Star Forge - Masked]",				"",				"md_revan_jedi",				"revan_jedi",			"model_default",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				qfalse },
	{ "Revan",							"[Star Forge - Unmasked]",				"",				"md_revan_jedi_nomask",			"revan_jedi",			"model_nomask",					"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Star Forge]",							"",				"md_revan_jedi_nomask_nh",		"revan_jedi",			"model_nomask_nohood",			"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Mandalorian Wars - Masked]",			"",				"md_revan_mw",					"revan_mw",				"model_default",				"revan_jedi",				"",						"purple",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Mandalorian Wars - Unmasked]",		"",				"md_revan_mw_nomask",			"revan_mw",				"model_nomask",					"revan_jedi",				"",						"purple",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Mandalorian Wars]",					"",				"md_revan_mw_nomask_nh",		"revan_mw",				"model_nomask_nohood",			"revan_jedi",				"",						"purple",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Cathar - Masked]",					"",				"md_revan_pk",					"revan_prekotor",		"model_default",				"revan_jedi",				"",						"purple",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Cathar - Unmasked]",					"",				"md_revan_pk_nomask",			"revan_prekotor",		"model_nomask",					"revan_jedi",				"",						"purple",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Cathar]",								"",				"md_revan_pk_nomask_nh",		"revan_prekotor",		"model_nomask_nohood",			"revan_jedi",				"",						"purple",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Republic Trooper]",					"",				"md_revan_trooper",				"OR_trooper",			"model_revan",					"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Republic Trooper - Damaged]",			"",				"md_revan_trooper_dmg",			"OR_trooper",			"model_revanDM",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Republic Trooper - Helmet]",			"",				"md_revan_trooper_helmet",		"OR_trooper",			"model_revan_helmet",			"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Jedi]",								"",				"md_revan_jedir",				"jedi_robe_revan",		"model_default",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Jedi]",								"",				"md_revan_jedir2",				"jedi_robe_revan",		"model_default2",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Jedi]",								"",				"md_revan_jedir3",				"jedi_robe_revan",		"model_default3",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Jedi]",								"",				"md_revan_jedir4",				"jedi_robe_revan",		"model_default4",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Jedi]",								"",				"md_revan_jedir5",				"jedi_robe_revan",		"model_default5",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan",							"[Student]",							"",				"md_revan_student",				"jedi_robe_revan",		"model_student",				"revan_jedi",				"",						"blue",			"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Trask Ulgo",						"[KOTOR]",								"",				"md_trask",						"OR_trooper",			"model_trask",					"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_OLD_REPUBLIC,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel1/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TRASK_ULGO",					qfalse },
	{ "Vandar Tokare",					"[Jedi]",								"",				"md_vandar",					"vandar",				"model_default",				"",							"",						"",				"",			5,		32,		ERA_OLD_REPUBLIC,	BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/vandar/misc/gloat2.mp3",				"",		0.7f,	"@MD_CHAR_DESC_VANDAR_TOKARE",				qfalse },
	{ "Ven Zallow",						"[Jedi]",								"",				"md_ven",						"jedi_tor",				"model_ven",					"jolee",					"",						"green",		"",			1,		30,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_tor/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_VEN_ZALLOW",					qfalse },
	{ "Vrook Lamar",					"[Jedi]",								"",				"jedi_vrook",					"jedi_kotor",			"model_vrook",					"single_2",					"",						"green",		"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/vrook/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_VROOK_LAMAR",				qfalse },
	{ "Zhar Lestin",					"[Jedi]",								"",				"jedi_zhar",					"jedi_kotor",			"model_zhar",					"single_2",					"",						"blue",			"",			1,		14,		ERA_OLD_REPUBLIC,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/zhar/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_ZHAR_LESTIN",				qfalse },


	// Sith Empire
	{ "Arcann",							"[KOTOR]",								"",				"md_arcann",					"thexan",				"model_arcann",					"galenmarek_cjr",			"",						"yellow",		"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/arcann/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ARCANN",						qfalse },
	{ "Arcann",							"[KOTOR - Masked]",						"",				"md_arcann_mask",				"arcann",				"model_default",				"galenmarek_cjr",			"",						"yellow",		"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/arcann/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ARCANN",						 qtrue },
	{ "Calo Nord",						"[KOTOR]",								"",				"md_calonord",					"calonord",				"model_default",				"menu_wp_rebelblaster_kotor", "",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/calonord/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_CALO_NORD",					qfalse },
	{ "Dark Jedi",						"[KOTOR]",								"",				"md_darkjedi",					"darkjedi",				"model_default",				"shadowtrooper",			"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darkjedi/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARK_JEDI",					qfalse },
	{ "Dark Jedi",						"[KOTOR]",								"",				"md_darkjedi2",					"darkjedi",				"model_default2",				"shadowtrooper",			"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darkjedi/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARK_JEDI",					 qtrue },
	{ "Dark Jedi",						"[KOTOR]",								"",				"md_darkjedi3",					"darkjedi",				"model_default3",				"shadowtrooper",			"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darkjedi/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARK_JEDI",					 qtrue },
	{ "Darth Bandon",					"[KOTOR]",								"",				"md_bandon",					"sith_warrior",			"model_bandon",					"phobos",					"",						"red",			"",			7,		128,	ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bandon/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANDON",				qfalse },
	{ "Darth Bane",						"[KOTOR]",								"",				"md_bane",						"darthbane",			"model_default",				"bane",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bane/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANE",					qfalse },
	{ "Darth Bane",						"[KOTOR - Alternative]",				"",				"md_bane_alt",					"darthbane",			"model_alt",					"bane",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bane/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANE",					 qtrue },
	{ "Darth Bane",						"[KOTOR - Armor]",						"",				"md_bane_armor",				"darthbane",			"model_armor",					"bane",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bane/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANE",					 qtrue },
	{ "Darth Bane",						"[KOTOR - Alternative Armor]",			"",				"md_bane_armor_alt",			"darthbane",			"model_armor_alt",				"bane",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bane/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANE",					 qtrue },
	{ "Darth Bane",						"[KOTOR - Shirtless]",					"",				"md_bane_shirtless",			"darthbane",			"model_shirtless",				"bane",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bane/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANE",					 qtrue },
	{ "Darth Bane",						"[TCW - Armor]",						"",				"md_bane_armortcw",				"darthbane2",			"model_default",				"bane",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bane/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_BANE",					 qtrue },
	{ "Darth Malgus",					"[Sith]",								"",				"md_malgus",					"darthmalgus",			"model_default",				"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				qfalse },
	{ "Darth Malgus",					"[Sith - Hooded]",						"",				"md_malgus_hooded",				"darthmalgus",			"model_hood",					"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Sith - No Cape]",						"",				"md_malgus_nocape",				"darthmalgus",			"model_nocape",					"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Sith Lord]",							"",				"md_malgus_lord",				"darthmalgus",			"model_lord",					"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Sith Lord - Damaged]",				"",				"md_malgus_lorddm",				"darthmalgus",			"model_lordDM",					"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Sith Lord - Young]",					"",				"md_malgus_young",				"darthmalgus",			"model_nomask",					"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Sith Lord - Young Hooded]",			"",				"md_malgus_youngh",				"darthmalgus",			"model_nomask_hood",			"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Onslaught]",							"",				"md_malgus_onslaught",			"darthmalgus_onslaught", "model_default",				"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Onslaught - No Mask]",				"",				"md_malgus_onslaught_nomask",	"darthmalgus_onslaught", "model_nomask",				"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malgus",					"[Onslaught - No Cape]",				"",				"md_malgus_onslaught_nocape",	"darthmalgus_onslaught", "model_nocape",				"malgus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthmalgus/misc/victory3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALGUS",				 qtrue },
	{ "Darth Malak",					"[Sith Lord]",							"",				"md_malak",						"malak",				"model_default",				"malak",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/malak/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALAK",				qfalse },
	{ "Darth Malak",					"[Sith Lord - Damaged]",				"",				"md_malak_mdmg",				"malak",				"model_mouth_damaged",			"malak",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/malak/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MALAK",				 qtrue },
	{ "Darth Nihilus",					"[Sith]",								"",				"md_nihilus",					"nihilus",				"model_default",				"nihilus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/nihilus/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_NIHILUS",				qfalse },
	{ "Darth Nihilus",					"[Sith - Alternative]",					"",				"md_nihilus_alt",				"nihilus",				"model_new",					"nihilus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/nihilus/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_NIHILUS",				 qtrue },
	{ "Darth Nihilus",					"[Sith - Alternative]",					"",				"md_nihilus_alt2",				"nihilus",				"model_new2",					"nihilus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/nihilus/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_NIHILUS",				 qtrue },
	{ "Darth Revan",					"[Hooded - Masked]",					"",				"md_revan",						"revan",				"model_default",				"revan",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				qfalse },
	{ "Darth Revan",					"[Hooded - Unmasked]",					"",				"md_revan_nomask",				"revan",				"model_nomask",					"revan",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Darth Revan",					"[Unmasked]",							"",				"md_revan_nomask_nh",			"revan",				"model_nomask_nohood",			"revan",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan Reborn",					"[Hooded - Masked]",					"",				"md_revanr",					"revan_reborn",			"model_default",				"revan",					"revan2",				"red",			"purple",	6,		64,		ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_DUAL,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan Reborn",					"[Hooded - Unmasked]",					"",				"md_revanr_nomask",				"revan_reborn",			"model_nomask",					"revan",					"revan2",				"red",			"purple",	6,		64,		ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_DUAL,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Revan Reborn",					"[Unmasked]",							"",				"md_revanr_nomask_nh",			"revan_reborn",			"model_nomask_nohood",			"revan",					"revan2",				"red",			"purple",	6,		64,		ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_DUAL,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/revan/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_REVAN",				 qtrue },
	{ "Darth Sion",						"[KOTOR]",								"",				"md_sion",						"sion",					"model_default",				"sion",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sion/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_SION",					qfalse },
	{ "Darth Sion",						"[KOTOR - Alternative]",				"",				"md_sion_alt",					"sion",					"model_k2",						"sion",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sion/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_SION",					 qtrue },
	{ "Darth Sion",						"[TFU]",								"",				"md_sion_tfu",					"sion",					"model_tfu",					"sion",						"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sion/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_DARTH_SION",					 qtrue },
	{ "Darth Traya",					"[Sith]",								"",				"md_traya",						"traya",				"model_default",				"revan",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/traya/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_TRAYA",				qfalse },
	{ "Darth Vindican",					"[KOTOR]",								"",				"md_vindican",					"vindican",				"model_default",				"phobos",					"",						"red",			"",			7,		128,	ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_DARTH_VINDICAN",				qfalse },
	{ "Darth Vindican",					"[KOTOR - Hooded]",						"",				"md_vindican_hooded",			"vindican",				"model_hood",					"phobos",					"",						"red",			"",			7,		128,	ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_DARTH_VINDICAN",				 qtrue },
	{ "Davik Kang",						"[KOTOR]",								"",				"md_davik",						"davik",				"model_default",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/calonord/misc/anger1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DAVIK_KANG",					qfalse },
	{ "Davik Kang",						"[KOTOR - Alternative]",				"",				"md_davik_alt",					"davik",				"model_armorold",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/calonord/misc/anger1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DAVIK_KANG",					 qtrue },
	{ "Emperor Valkorion",				"[KOTOR]",								"",				"md_valkorion",					"valkorion",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/valkorion/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_EMPEROR_VALKORION",			qfalse },
	{ "HK 47",							"[KOTOR]",								"",				"md_hk47",						"hk47",					"model_default",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hk47/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_HK47",						qfalse },
	{ "Imperial Trooper",				"[Soldier - Rifle]",					"",				"md_sithtrooper_tor",			"sithtrooper_tor",		"model_default",				"menu_wp_rebelrifle_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper_tor/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_IMPERIAL_TROOPER_KOTOR",		qfalse },
	{ "Imperial Trooper",				"[Soldier - Sniper]",					"",				"md_sithtrooper_tor_disruptor",	"sithtrooper_tor",		"model_default",				"menu_wp_disruptor_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper_tor/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_IMPERIAL_TROOPER_KOTOR",		 qtrue },
	{ "Jorak Uln",						"[KOTOR]",								"",				"md_jorak",						"sith_officer",			"model_jorak",					"dual_5",					"",						"red",			"",			7,		128,	ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jorak/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JORAK_ULN",					qfalse },
	{ "Lord Scourge",					"[KOTOR]",								"",				"md_scourge",					"scourge",				"model_default",				"nihilus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_LORD_SCOURGE",				qfalse },
	{ "Mandalore The Ultimate",			"[KOTOR]",								"",				"md_mandalore_ultimate",		"mandalore_ultimate",	"model_default",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_MANDALORE_ULTIMATE",			qfalse },
	{ "Neo-Crusader",					"[Blue]",								"",				"md_neocrusader",				"neocrusader",			"model_default",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_NEO_CRUSADER",				qfalse },
	{ "Neo-Crusader",					"[Gold]",								"",				"md_neocrusader_gold",			"neocrusader",			"model_gold",					"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_NEO_CRUSADER",				 qtrue },
	{ "Neo-Crusader",					"[Red]",								"",				"md_neocrusader_red",			"neocrusader",			"model_red",					"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_NEO_CRUSADER",				 qtrue },
	{ "Saul Karath",					"[KOTOR]",								"",				"md_saul",						"sith_officer",			"model_saul",					"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saul/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SAUL_KARATH",				qfalse },
	{ "Sith Eradicator",				"[KOTOR]",								"",				"md_sith_eradicator",			"sith_eradicator",		"model_default",				"nihilus",					"",						"red",			"",			1,		30,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sith_eradicator/misc/taunt4.mp3",		"",		1.0f,	"@MD_CHAR_DESC_SITH_ERADICATOR",			qfalse },
	{ "Sith Officer",					"[KOTOR]",								"",				"md_sith_officer",				"sith_officer",			"model_default",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saul/misc/victory3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SITH_OFFICER",				qfalse },
	{ "Sith Officer",					"[KOTOR]",								"",				"md_sith_officer2",				"sith_officer",			"model_default2",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saul/misc/victory3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SITH_OFFICER",				 qtrue },
	{ "Sith Officer",					"[KOTOR]",								"",				"md_sith_officer3",				"sith_officer",			"model_officer2",				"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saul/misc/victory3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SITH_OFFICER",				 qtrue },
	{ "Sith Officer",					"[KOTOR]",								"",				"md_sith_officer_afro",			"sith_officer",			"model_officer_afro",			"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saul/misc/victory3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SITH_OFFICER",				 qtrue },
	{ "Sith Officer",					"[KOTOR]",								"",				"md_sith_officer_asian",		"sith_officer",			"model_officer_asian",			"menu_wp_blaster_pistol_kotor",	"",					"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saul/misc/victory3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SITH_OFFICER",				 qtrue },
	{ "Sith Trooper",					"[White]",								"",				"md_sithtrooper_kotor",			"sithtrooper_kotor",	"model_default",				"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_TROOPER_KOTOR",			qfalse },
	{ "Sith Trooper",					"[Gold]",								"",				"md_sithtrooper_kotor_gold",	"sithtrooper_kotor",	"model_gold",					"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_TROOPER_KOTOR",			 qtrue },
	{ "Sith Trooper",					"[Red]",								"",				"md_sithtrooper_kotor_red",		"sithtrooper_kotor",	"model_red",					"menu_wp_blaster_kotor",	"",						"",				"",			0,		0,		ERA_SITH_EMPIRE,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithtrooper/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_TROOPER_KOTOR",			 qtrue },
	{ "Sith Warrior",					"[KOTOR]",								"",				"sith_kotor",					"jedi_kotor",			"model_baturem",				"single_2",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sith_acolyte/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_KOTOR",					qfalse },
	{ "Sith Warrior",					"[KOTOR]",								"",				"sith_kotor2",					"jedi_kotor",			"model_sith1",					"single_2",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sith_acolyte/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_KOTOR",					 qtrue },
	{ "Sith Warrior",					"[KOTOR]",								"",				"sith_kotor3",					"jedi_kotor",			"model_sith2",					"single_2",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sith_acolyte/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_KOTOR",					 qtrue },
	{ "Sith Warrior",					"[KOTOR]",								"",				"sith_kotor4",					"jedi_kotor",			"model_sith3",					"single_2",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sith_acolyte/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SITH_KOTOR",					 qtrue },
	{ "Thexan",							"[KOTOR]",								"",				"md_thexan",					"thexan",				"model_default",				"galenmarek_cjr",			"",						"yellow",		"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/thexan/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_THEXAN",						qfalse },
	{ "Uthar Wynn",						"[KOTOR]",								"",				"md_uthar",						"sith_officer",			"model_uthar",					"dual_4",					"",						"red",			"",			7,		128,	ERA_SITH_EMPIRE,	BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/uthar/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_UTHAR_WYNN",					qfalse },
	{ "Visas Marr",						"[KOTOR]",								"",				"md_visas",						"visas",				"model_default",				"dagan",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/visas/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_VISAS_MARR",					qfalse },
	{ "Visas Marr",						"[KOTOR - Alternative]",				"",				"md_visas_dark",				"visas",				"model_dark",					"dagan",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/visas/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_VISAS_MARR",					 qtrue },
	{ "Yuthura Ban",					"[KOTOR]",								"",				"md_yuthura",					"yuthura",				"model_default",				"single_1",					"",						"red",			"",			1,		14,		ERA_SITH_EMPIRE,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yuthura/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_YUTHURA_BAN",				qfalse },


	// Galactic Republic
	{ "Aayla Secura",					"[Episode I-III]",						"",				"md_aayla",						"aayla",				"model_default",				"aayla",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/aayla/misc/gloat3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AAYLA_SECURA",				qfalse },
	{ "Adi Gallia",						"[Episode I-III]",						"",				"md_adi_gallia",				"adi_gallia",			"model_robed",					"adi_gallia",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/adi_gallia/misc/gloat2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ADI_GALLIA",					qfalse },
	{ "Adi Gallia",						"[General]",							"",				"md_adi_tcw",					"adi_gallia",			"model_cw",						"adi_gallia",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/adi_gallia/misc/gloat2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ADI_GALLIA",					 qtrue },
	{ "Agen Kolar",						"[Episode I-III]",						"",				"md_agen",						"agen_kolar",			"model_default",				"shaak",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/eeth_koth/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AGEN_KOLAR",					qfalse },
	{ "Agen Kolar",						"[Robed]",								"",				"md_agen_robed",				"agen_kolar",			"model_robed",					"shaak",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/eeth_koth/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AGEN_KOLAR",					 qtrue },
	{ "Ahsoka Tano",					"[TCW - Padawan]",						"",				"md_ahsoka",					"ahsoka",				"model_default",				"ahsoka",					"ahsoka_short",			"green",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				qfalse },
	{ "Ahsoka Tano",					"[TCW - Padawan]",						"",				"md_ahsoka_padawan",			"ahsoka",				"model_padawan",				"ahsoka",					"ahsoka_short",			"green",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Padawan]",						"",				"md_ahsoka_tcw",				"ahsoka",				"model_tcw",					"ahsoka",					"ahsoka_short",			"green",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Eva Suit]",						"",				"md_ahsoka_eva",				"ahsoka",				"model_eva",					"ahsoka",					"ahsoka_short",			"green",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TOTJ]",								"",				"md_ahsoka_totj",				"ahsoka",				"model_totj",					"ahsoka",					"ahsoka_short",			"green",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Mortis]",						"",				"md_ahsoka_mortis",				"ahsoka",				"model_mortis",					"ahsoka",					"ahsoka_short",			"green",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Season 7]",						"",				"md_ahsoka_s7",					"ahsoka_s7",			"model_default",				"ahsoka_alt",				"ahsoka_short_alt",		"blue",			"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Season 7 Robed]",				"",				"md_ahsoka_s7_r",				"ahsoka_s7",			"model_robe",					"ahsoka_alt",				"ahsoka_short_alt",		"blue",			"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Season 7 Live Action]",			"",				"md_ahsoka_s7_y",				"ahsoka_s7",			"model_young",					"ahsoka_alt",				"ahsoka_short_alt",		"blue",			"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[TCW - Season 7 Live Action Robed]",	"",				"md_ahsoka_s7_y_r",				"ahsoka_s7",			"model_young_robe",				"ahsoka_alt",				"ahsoka_short_alt",		"blue",			"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[Rebels]",								"",				"md_ahsoka_rebels",				"ahsoka_rebels",		"model_default",				"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_rebels/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[Rebels - Fulcrum Poncho]",			"",				"md_ahsoka_rebels_poncho",		"ahsoka_rebels",		"model_poncho",					"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_rebels/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[The Mandolorian]",					"",				"md_ahsoka_tm",					"ahsoka_tm",			"model_default",				"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_tm/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[The Mandolorian - Hooded]",			"",				"md_ahsoka_h_tm",				"ahsoka_tm",			"model_hood",					"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_tm/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[The Mandolorian - Poncho]",			"",				"md_ahsoka_p_tm",				"ahsoka_tm",			"model_poncho",					"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_tm/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[Ahsoka]",								"",				"md_ahsoka_ahs",				"ahsoka_tm",			"model_white",					"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_tm/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[Ahsoka - Hooded]",					"",				"md_ahsoka_h_ahs",				"ahsoka_tm",			"model_white_hood",				"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_tm/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Ahsoka Tano",					"[Ahsoka - Poncho]",					"",				"md_ahsoka_p_ahs",				"ahsoka_tm",			"model_white_poncho",			"ahsoka_reb1",				"ahsoka_reb2",			"white",		"white",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ahsoka_tm/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_AHSOKA_TANO",				 qtrue },
	{ "Anakin Skywalker",				"[Episode III]",						"",				"md_ani_ep3",					"anakin",				"model_default",				"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep3/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			qfalse },
	{ "Anakin Skywalker",				"[Episode III - Robed]",				"",				"md_ani_ep3_robed",				"anakin",				"model_robed",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep3/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode III - Hooded]",				"",				"md_ani_ep3_hooded",			"anakin",				"model_hood",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep3/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode III - Pyjama]",				"",				"md_ani_ep3_pj",				"anakin",				"model_shirtless_robe",			"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep3/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode III - Shirtless]",			"",				"md_ani_ep3_pj2",				"anakin",				"model_shirtless",				"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep3/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode II]",							"",				"md_ani_ep2",					"anakin_ep2",			"model_default",				"anakin_ep2",				"",						"blue",			"",			2,		12,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode II - Robed]",					"",				"md_ani_ep2_robed",				"anakin_ep2",			"model_robed",					"anakin_ep2",				"",						"blue",			"",			2,		12,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode II - Hooded]",				"",				"md_ani_ep2_hooded",			"anakin_ep2",			"model_hooded",					"anakin_ep2",				"",						"blue",			"",			2,		12,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[Episode II - Dual Lightsabers]",		"",				"md_ani_ep2_dual",				"anakin_ep2",			"model_default",				"obiwan_ep2b",				"anakin_ep2b",			"blue",			"green",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[TCW]",								"",				"md_ani_tcw",					"anakin",				"model_tcw",					"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[TCW - Cape]",							"",				"md_ani_tcw_caped",				"anakin",				"model_tcw_caped",				"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[TCW - Season 3]",						"",				"md_ani_tcw_s3",				"anakin",				"model_tcw_s3",					"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[TCW Animated]",						"",				"md_ani_tcwa",					"anakin",				"model_tcwa",					"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[TCW Animated - Robed]",				"",				"md_ani_tcwa_robed",			"anakin",				"model_tcwa_robed",				"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[TCW Animated - Hooded]",				"",				"md_ani_tcwa_hooded",			"anakin",				"model_tcwa_hooded",			"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[CW 2003]",							"",				"md_ani_cw",					"anakin",				"model_cw",						"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_cw/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[CW 2003 - Cape]",						"",				"md_ani_cw_caped",				"anakin",				"model_cw_caped",				"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_cw/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[CW 2003 - Jedi Trials]",				"",				"md_ani_jtrial",				"anakin",				"model_jtrial",					"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_cw/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[CW 2003 - Nelvaan Trials]",			"",				"md_ani_cw_nelvaan",			"anakin",				"model_swolo",					"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_cw/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[CW 2003 - Yavin]",					"",				"md_ani_cw_yavin",				"anakin",				"model_ep2_cw",					"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_cw/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Anakin Skywalker",				"[CW 2003 - Yavin Battleworn]",			"",				"md_ani_cw_yavin_bw",			"anakin",				"model_ep2_cw_bw",				"anakin_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_cw/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ANAKIN_SKYWALKER",			 qtrue },
	{ "Bail Organa",					"[Episode III]",						"",				"md_bail_organa_tfu",			"bailorgana",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/Bail_organa/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BAIL_ORGANA",				qfalse },
	{ "Barris Offee",					"[TCW]",								"",				"md_barriss",					"barriss_offee",		"model_default",				"barriss",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/barriss/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BARRISS_OFFEE",				qfalse },
	{ "Bo-Katan Kryze",					"[Helmet]",								"",				"bokatan",						"bokatan",				"model_default",				"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_BO_KATAN_KRYZE",				qfalse },
	{ "Bo-Katan Kryze",					"[No Helmet]",							"",				"bokatan_nohelm",				"bokatan",				"model_nohelm",					"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_BO_KATAN_KRYZE",				 qtrue },
	{ "Bolla Ropal",					"[TCW]",								"",				"md_bolla_ropal",				"bolla_ropal",			"model_default",				"aayla",					"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_BOLLA_ROPAL",				qfalse },
	{ "Bultar Swan",					"[Episode II-III]",						"",				"md_bultar",					"bultar",				"model_default",				"shaak",					"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BULTAR_SWAN",				qfalse },
	{ "Bultar Swan",					"[Episode II-III - Robed]",				"",				"md_bultar_robed",				"bultar",				"model_robed",					"shaak",					"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BULTAR_SWAN",				 qtrue },
	{ "Cin Drallig",					"[TCW - Episode III]",					"",				"md_drallig",					"cin_drallig",			"model_default",				"drallig",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cin_drallig/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CIN_DRALLIG",				qfalse },
	{ "Cin Drallig",					"[TCW]",								"",				"md_drallig_tcw",				"cin_drallig",			"model_cw",						"drallig",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cin_drallig/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CIN_DRALLIG",				 qtrue },
	{ "Cin Drallig",					"[Legends]",							"",				"md_drallig_old",				"cin_drallig",			"model_old",					"drallig",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cin_drallig/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CIN_DRALLIG",				 qtrue },
	{ "Captain Rex",					"[TCW]",								"",				"md_clo_rex",					"clonetrooper_p2",		"model_501_rex",				"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CAPTAIN_REX",				qfalse },
	{ "Commander Cody",					"[TCW]",								"",				"md_clo_cody",					"clonetrooper_p2",		"model_212_cody",				"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_COMMANDER_CODY",				 qtrue },
	{ "Commander Fox",					"[TCW]",								"",				"md_clo_fox",					"clonetrooper_p2",		"model_fox",					"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_COMMANDER_FOX",				 qtrue },
	{ "Commander Neyo",					"[TCW]",								"",				"md_clo_neyo",					"clonetrooper_p2",		"model_91_neyo",				"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_COMMANDER_NEYO",				 qtrue },
	{ "Commander Vaughn",				"[TCW]",								"",				"md_clo_vaughn",				"clonetrooper_p2",		"model_332_vaughn",				"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_COMMANDER_VAUGHN",			 qtrue },
	{ "Fives",							"[TCW]",								"",				"md_clo_fives",					"clonetrooper_p2",		"model_fives",					"menu_wp_clonepistol",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_FIVES",						 qtrue },
	{ "Clone Commando",					"[Clone Wars Armor]",					"",				"md_clo_rc",					"clonerc2",				"model_default",				"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clone_rc/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_COMMANDO",			qfalse },
	{ "Clone Commando",					"[Night Ops Armor]",					"",				"md_clo_rc2",					"clonerc2",				"model_commando",				"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clone_rc/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REPUBLIC_COMMANDO",			 qtrue },
	{ "Boss",							"[Commando]",							"",				"md_clo_boss",					"clonerc2",				"model_boss",					"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/deltas/misc/anger1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BOSS",						 qtrue },
	{ "Fixer",							"[Commando]",							"",				"md_clo_fixer",					"clonerc2",				"model_fixer",					"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_FIXER",						 qtrue },
	{ "Gregor",							"[Commando]",							"",				"md_clo_gregor",				"clonerc2",				"model_gregor",					"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_GREGOR",						 qtrue },
	{ "Scorch",							"[Commando]",							"",				"md_clo_scorch",				"clonerc2",				"model_scorch",					"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/scorch/misc/anger2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SCORCH",						 qtrue },
	{ "Sev",							"[Commando]",							"",				"md_clo_sev",					"clonerc2",				"model_sev",					"menu_wp_clonecommando",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sev/misc/taunt5.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SEV",						 qtrue },
	{ "Clone Trooper",					"[Phase 1]",							"",				"md_clo_ep2",					"clonetrooper_p1",		"model_default",				"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p1/misc/victory1.mp3",	"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				qfalse },
	{ "Clone Trooper",					"[212th Battalion]",					"",				"md_clo_212th",					"clonetrooper_p2",		"model_212",					"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Paratrooper",				"[212th Battalion]",					"",				"md_clo_212_airborne",			"clonetrooper_p2",		"model_212_airborne",			"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Trooper",					"[13th Battalion]",						"",				"md_clo_13th",					"clonetrooper_p2",		"model_13",						"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Sharpshooter",				"[501st Legion]",						"",				"md_clo4_rt",					"clonetrooper_p2",		"model_airborne_rgb",			"menu_wp_disruptor",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Trooper",					"[501st Legion]",						"",				"md_clo1_jt",					"clonetrooper_p2",		"model_501",					"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Commander",				"[501st Legion]",						"",				"md_clo2_jt",					"clonetrooper_p2",		"model_deviss_rgb",				"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Shock Trooper",			"[Shock Trooper]",						"",				"md_clo_red_player",			"clonetrooper_p2",		"model_shock",					"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Commander Deviss",				"[TCW]",								"",				"md_clo_red2_player",			"clonetrooper_p2",		"model_327_deviss",				"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Heavy Trooper",			"[Shock Trooper]",						"",				"md_clo6_rt_player",			"clonetrooper_p2",		"model_327_deviss",				"menu_wp_rocket_launcher",	"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Trooper",					"[332nd Company]",						"",				"md_clo_332",					"clonetrooper_p2",		"model_332",					"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Trooper",					"[327th Star Corps]",					"",				"md_clo_327",					"clonetrooper_p2",		"model_327",					"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Clone Shadow Trooper",			"[Shadow Trooper]",						"",				"md_clo_shadow",				"clonetrooper_p2",		"model_shadow",					"menu_wp_clonecarbine",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Galactic Marine",				"[21st Nova Corps]",					"",				"md_clonemarine",				"clonemarine",			"model_default",				"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/clonetrooper_p2/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_CLONE_TROOPER",				 qtrue },
	{ "Coleman Trebor",					"[Episode I-II]",						"",				"md_coleman",					"coleman",				"model_default",				"coleman",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/coleman/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COLEMAN_TREBOR",				qfalse },
	{ "Depa Billaba",					"[TCW - Robed]",						"",				"md_depa_tcw_robed",			"depabillaba_tcw",		"model_robed",					"depa",						"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/depabillaba/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEPA_BILLABA",				qfalse },
	{ "Depa Billaba",					"[TCW - Hooded]",						"",				"md_depa_tcw_hooded",			"depabillaba_tcw",		"model_hooded",					"depa",						"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/depabillaba/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEPA_BILLABA",				 qtrue },
	{ "Depa Billaba",					"[Episode I-III]",						"",				"md_depa",						"depabillaba",			"model_default",				"depa",						"",						"green",		"",			1,		14, 	ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/depabillaba/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEPA_BILLABA",				 qtrue },
	{ "Eekar Oki",						"[TCW]",								"",				"md_eekar",						"eekar",				"model_default",				"quigon",					"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_EEKAR_OKI",					qfalse },
	{ "Eeth Koth",						"[General]",							"",				"md_eeth_koth",					"eeth_koth",			"model_cw",						"fisto",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/eeth_koth/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_EETH_KOTH",					qfalse },
	{ "Even Piell",						"[Episode I-III]",						"",				"md_even_piell",				"even_piell",			"model_default",				"even_piell",				"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/even_piell/misc/victory2.mp3",			"",		0.7f,	"@MD_CHAR_DESC_EVEN_PIELL",					qfalse },	
	{ "Falon Grey",						"[Battlefront]",						"",				"md_falon_grey",				"falon_grey",			"model_default",				"galenmarek_alt",			"",						"blue",			"",			1,		14, 	ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi2/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_FALON_GREY",					qfalse },
	{ "Fi-Ek Sirch",					"[Episode II]",							"",				"md_sirch",						"sirch",				"model_robed",					"luminara",					"",						"blue",			"",			2,		12,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_FI_EK_SIRCH",				qfalse },
	{ "Foul Moudama",					"[TCW]",								"",				"md_foul_moudama",				"foul_moudama",			"model_default",				"coleman",					"",						"blue",			"",			4,		24,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/talz/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_FOUL_MOUDAMA",				qfalse },
	{ "Gungan Warrior",					"[Episode I]",							"",				"md_gungan_warrior",			"gungan",				"model_default",				"gungan_shield",			"gammobaxe",			"",				"",			6,		64,		ERA_REPUBLIC,		BOTH_STAND_BLOCKING_ON_DUAL,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/gungan/misc/pain75.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GUNGAN_WARRIOR",				qfalse },
	{ "Halsey",							"[TCW]",								"",				"md_halsey",					"halsey",				"model_cw",						"shaak",					"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_HALSEY",						qfalse },
	{ "Ima-Gun Di",						"[TCW]",								"",				"md_ima_gundi",					"ima_gundi",			"model_default",				"saesee",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_IMA_GUN_DI",					qfalse },
	{ "J'oopi She",						"[TCW]",								"",				"md_joopi_robed",				"joopi_she",			"model_robed",					"coleman",					"",						"blue",			"",			2,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_J_OOPI_SHE",					qfalse },
	{ "Jar Jar Binks",					"[Episode I]",							"",				"md_jarjar",					"jarjar",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jarjar/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JAR_JAR_BINKS",				qfalse },
	{ "Jedi Master",					"[Jedi]",								"",				"md_jed1",						"jedi_spanki",			"|head_b4|torso_a4|lower_f1",	"quinlan",					"",						"green",		"",			2,		4,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_MASTER",				qfalse },
	{ "Jedi Master",					"[Jedi]",								"",				"md_jed10",						"jedi_spanki",			"|head_b2|torso_a3|lower_a1",	"shaak",					"",						"blue",			"",			3,		8,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_MASTER",				 qtrue },
	{ "Jedi Veteran",					"[Jedi - Dual Lightsabers]",			"",				"md_jed11",						"jedi_spanki",			"|head_d4|torso_b2|lower_b1",	"quinlan",					"shaak",				"blue",			"green",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_VETERAN",				 qtrue },
	{ "Jedi Veteran",					"[Jedi - Dual Lightsabers]",			"",				"md_jed13",						"jedi_spanki",			"|head_f1|torso_b4|lower_c1",	"shaak",					"quigon",				"green",		"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_VETERAN",				 qtrue },
	{ "Jedi Knight",					"[Jedi]",								"",				"md_jed4",						"jedi_spanki",			"|head_f1|torso_f2|lower_g1",	"shaak",					"",						"green",		"",			3,		8,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Knight",					"[Jedi - Dual Lightsabers]",			"",				"md_jed6",						"jedi_female1",			"|head_c2|torso_d2|lower_f1",	"shaak",					"shaak",				"blue",			"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Knight",					"[Jedi]",								"",				"md_jed8",						"zabrak_rots",			"model_default",				"quigon",					"",						"blue",			"",			2,		12,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Knight",					"[Jedi]",								"",				"md_jed9",						"jedi_spanki",			"|head_d1|torso_g1|lower_d1",	"quinlan",					"",						"blue",			"",			3,		8,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Knight",					"[Jedi]",								"",				"md_jed15",						"jedi_tf",				"|head_b4|torso_b1|lower_a1",	"saesee",					"",						"blue",			"",			1,		2,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Knight",					"[Jedi]",								"",				"md_jed16",						"jedi_zf",				"|head_c1|torso_b1|lower_a1",	"shaak",					"",						"blue",			"",			3,		8,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Padawan",					"[Jedi]",								"",				"md_jed2",						"jedi_spanki",			"|head_e5|torso_c1|lower_d1",	"shaak",					"",						"blue",			"",			2,		4,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_PADAWAN",				 qtrue },
	{ "Jedi Padawan",					"[Jedi]",								"",				"md_jed3",						"jedi_spanki",			"|head_d5|torso_f1|lower_g1",	"shaak",					"",						"green",		"",			2,		4,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_PADAWAN",				 qtrue },
	{ "Jedi Padawan",					"[Jedi]",								"",				"md_jed5",						"jedi_female1",			"|head_d3|torso_f1|lower_e1",	"shaak",					"",						"green",		"",			2,		4,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/female_jedi1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_PADAWAN",				 qtrue },
	{ "Jedi Brute",						"[Jedi]",								"",				"md_jbrute",					"jedibrute",			"model_default",				"dual_4",					"",						"green",		"",			7,		128,	ERA_REPUBLIC,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_brute/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JEDI_BRUTE",					 qtrue },
	{ "Ti'Ori",							"[Jedi]",								"",				"theory",						"Theory",				"model_default",				"quigon",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/theory/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Temple Guard",				"[TCW]",								"",				"md_templeguard",				"jtguard",				"model_default",				"temple_guard",				"",						"yellow",		"",			7,		128,	ERA_REPUBLIC,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jtguard/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_TEMPLE_GUARD",			qfalse },
	{ "Jedi Temple Guard",				"[Rebels]",								"",				"md_templeguard_rebels",		"GI_rebels",			"model_jtguard",				"temple_guard_reb",			"",						"yellow",		"",			7,		128,	ERA_REPUBLIC,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jtguard/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_TEMPLE_GUARD",			 qtrue },
	{ "Jocasta Nu",						"[Episode I-III]",						"",				"md_jocasta",					"jocasta",				"model_default",				"jocasta_nu",				"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jocasta/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JOCASTA_NU",					qfalse },
	{ "Kelleran Beq",					"[Jedi]",								"",				"md_kelleran",					"kelleran_beq",			"model_robed",					"adi_gallia",				"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_KELLERAN_BEQ",				qfalse },
	{ "Kelleran Beq",					"[Dual Lightsabers]",					"",				"md_kelleran_dual",				"kelleran_beq",			"model_robed",					"adi_gallia",				"coleman",				"green",		"blue",		6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_KELLERAN_BEQ",			 	 qtrue },
	{ "Ki-Adi Mundi",					"[Episode I-III]",						"",				"md_mundi",						"ki_adi_mundi",			"model_default",				"mundi",					"",						"blue",			"",			1,		14, 	ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ki_adi_mundi/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KI_ADI_MUNDI",				qfalse },
	{ "Kit Fisto",						"[Episode I-III]",						"",				"md_fisto",						"fisto",				"model_default",				"fisto",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/fisto/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KIT_FISTO",					qfalse },
	{ "Kit Fisto",						"[Episode I-III - Robed]",				"",				"md_fisto_robed",				"fisto",				"model_robed",					"fisto",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/fisto/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KIT_FISTO",					 qtrue },
	{ "Kit Fisto",						"[General]",							"",				"md_fisto_tcw",					"fisto",				"model_cw",						"fisto",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/fisto/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KIT_FISTO",					 qtrue },
	{ "Kit Fisto",						"[Shirtless]",							"",				"md_fisto_tcw_noshirt",			"kitfisto_cw",			"model_default",				"fisto",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/fisto/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KIT_FISTO",					 qtrue },
	{ "Knox",							"[TCW]",								"",				"md_knox",						"knox",					"model_default",				"rig",						"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_KNOX",						qfalse },
	{ "Koffi Arana",					"[Jedi]",								"",				"md_koffi_robed",				"koffi_arana",			"model_robed",					"aayla",					"",						"blue",			"",			3,		12,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_KOFFI_ARANA",				qfalse },
	{ "Luminara Unduli",				"[Episode I-III]",						"",				"md_luminara",					"luminara",				"model_default",				"luminara",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luminara/misc/gloat3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUMINARA_UNDULI",			qfalse },
	{ "Ma'kis'shaalas",					"[TCW]",								"",				"md_jed_nikto",					"jedi_nikto",			"model_default",				"luminara",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedibrute/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MA_KIS_SHAALAS",				qfalse },
	{ "Ma'kis'shaalas",					"[Robed]",								"",				"md_jed_nikto_robed",			"jedi_nikto",			"model_robed",					"luminara",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedibrute/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MA_KIS_SHAALAS",				 qtrue },
	{ "Mace Windu",						"[Episode II-III]",						"",				"md_windu",						"macewindu",			"model_default",				"windu",					"",						"purple",		"",			3,		28,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_windu/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					qfalse },
	{ "Mace Windu",						"[Episode II-III - Robed]",				"",				"md_windu_robed",				"macewindu",			"model_robed",					"windu",					"",						"purple",		"",			3,		28,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_windu/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					 qtrue },
	{ "Mace Windu",						"[Episode II-III - Hooded]",			"",				"md_windu_hooded",				"macewindu",			"model_hooded",					"windu_ep1",				"",						"purple",		"",			3,		28,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_windu/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					 qtrue },
	{ "Mace Windu",						"[Force Ghost]",						"",				"md_windu_ghost",				"macewindu",			"model_ghost",					"windu",					"",						"purple",		"",			3,		28,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_windu/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					 qtrue },
	{ "Mace Windu",						"[TCW - General]",						"",				"md_windu_tcw",					"macewindu",			"model_cw",						"windu",					"",						"purple",		"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_tcw/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					 qtrue },
	{ "Mace Windu",						"[CW - General]",						"",				"md_windu_cw",					"macewindu_cw",			"model_default",				"windu",					"",						"purple",		"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_tcw/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					 qtrue },
	{ "Mace Windu",						"[Tales of the Jedi]",					"",				"md_windu_totj",				"macewindu",			"model_totj",					"windu",					"",						"purple",		"",			1,		30,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mace_tcw/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_MACE_WINDU",					 qtrue },
	{ "Mandalorian",					"[Death Watch]",						"",				"md_mando_dw",					"deathwatch",			"model_default",				"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mando_dw/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEATH_WATCH",				qfalse },	
	{ "Micah Giiett",					"[TCW]",								"",				"md_micah_robed",				"micah_giiett",			"model_robed",					"coleman",					"coleman",				"yellow",		"yellow",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_MICAH_GIIETT",				qfalse },
	{ "Nahdar Vebb",					"[TCW]",								"",				"md_nahdar",					"nahdar",				"model_default",				"quigon",					"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_NAHDAR_VEBB",				qfalse },
	{ "Nahdar Vebb",					"[Robed]",								"",				"md_nahdar_robed",				"nahdar",				"model_robed",					"quigon",					"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_NAHDAR_VEBB",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode III]",						"",				"md_obi_ep3",					"obiwan_ep3",			"model_default",				"obiwan_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				qfalse },
	{ "Obi-Wan Kenobi",					"[Episode III - Robed]",				"",				"md_obi_ep3_robed",				"obiwan_ep3",			"model_robed",					"obiwan_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode III - Hooded]",				"",				"md_obi_ep3_hooded",			"obiwan_ep3",			"model_hood",					"obiwan_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode III - Mustafar Worn]",		"",				"md_obi_ep3_mus",				"obiwan_ep3",			"model_bw",						"obiwan_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode II]",							"",				"md_obi_ep2",					"obiwan_ep2",			"model_default",				"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode II - Robed]",					"",				"md_obi_ep2_robed",				"obiwan_ep2",			"model_robed",					"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode II - Hooded]",				"",				"md_obi_ep2_hooded",			"obiwan_ep2",			"model_hooded",					"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep2/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode I]",							"",				"md_obi_ep1",					"obiwan_ep1",			"model_default",				"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode I - Robed]",					"",				"md_obi_ep1_robed",				"obiwan_ep1",			"model_robed",					"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Episode I - Hooded]",					"",				"md_obi_ep1_hooded",			"obiwan_ep1",			"model_hooded",					"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep1/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Jabiim]",								"",				"md_obi_jabiim",				"obiwan_jabiim",		"model_default",				"obiwan_owk",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Jabiim - Robed]",						"",				"md_obi_jabiim_robed",			"obiwan_jabiim",		"model_robed",					"obiwan_owk",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Pre-ANH]",							"",				"md_obi_preanh",				"obiwan_jabiim",		"model_defaultb",				"obiwan_owk",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Pre-ANH - Desert]",					"",				"md_obi_preanh_desert",			"obiwan_jabiim",		"model_robedc",					"obiwan_owk",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[Exiled]",								"",				"md_obi_exile",					"obiwan_ep3",			"model_exile",					"obiwan_owk",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_ep3/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[TCW - General]",						"",				"md_obi_tcw",					"obiwan_tcw",			"model_default",				"obiwan_ep3",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_tcw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[CW - General]",						"",				"md_obi_cw",					"obiwan_cw",			"model_default",				"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_cw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Obi-Wan Kenobi",					"[CW - General]",						"",				"md_obi_cw_helmet",				"obiwan_cw",			"model_helmet",					"obiwan_ep1",				"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/obiwan_cw/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_OBI_WAN_KENOBI",				 qtrue },
	{ "Oppo Rancisis",					"[Episode I-III]",						"",				"md_oppo_rancisis",				"oppo_rancisis",		"model_default",				"oppo",						"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_OPPO_RANCISIS",				qfalse },
	{ "Pablo Jill",						"[Episode I-III]",						"",				"md_ongree",					"ongree",				"model_robed",					"coleman",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ongree/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_PABLO_JILL",					qfalse },
	{ "Padme Amidala",					"[Queen]",								"",				"queen_amidala",				"queen_amidala",		"model_default",				"menu_wp_jango",			"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/Queenamidala/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_PADME_AMIDALA",				qfalse },
	{ "Padme Amidala",					"[Geonosis]",							"",				"md_pad_ga",					"padme_ep2",			"model_default_bw",				"menu_wp_battledroid",		"",						"",				"",			0,		0,		ERA_REPUBLIC,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/padme/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_PADME_AMIDALA",				 qtrue },
	{ "Padme Amidala",					"[Pregnant]",							"",				"md_padme_mus",					"Padme_Mus",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/padme/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_PADME_AMIDALA",				 qtrue },
	{ "Plo Koon",						"[Episode I-III]",						"",				"md_plo_koon",					"plo_koon",				"model_default",				"plo_koon",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/plo_koon/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_PLO_KOON",					qfalse },
	{ "Plo Koon",						"[Jedi Power Battles]",					"",				"md_plo_jpb",					"plo_koon",				"model_jpb",					"plo_koon",					"",						"yellow",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/plo_koon/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_PLO_KOON",					 qtrue },
	{ "Plo Koon",						"[TCW - General]",						"",				"md_plo_tcw",					"plo_tcw",				"model_default",				"plo_koon",					"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/plo_koon/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_PLO_KOON",					 qtrue },
	{ "Pong Krell",						"[TCW]",								"",				"md_pong_krell",				"pong_krell",			"model_default",				"pong_krell",				"pong_krell",			"blue",			"green",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/pong_krell/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_PONG_KRELL",					qfalse },
	{ "Que-Mars Redath-Gom",			"[TCW]",								"",				"md_redath_robed",				"redathgom",			"model_robed",					"saesee",					"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sora_bulq/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_QUE_MARS_REDATH_GOM",		qfalse },
	{ "Qui-Gon Jinn",					"[Episode I]",							"",				"md_quigon",					"quigon",				"model_default",				"quigon",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/quigon/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_QUI_GON_JINN",				qfalse },
	{ "Qui-Gon Jinn",					"[Episode I - Robed]",					"",				"md_quigon_robed",				"quigon",				"model_robed",					"quigon",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/quigon/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_QUI_GON_JINN",				 qtrue },
	{ "Qui-Gon Jinn",					"[Episode I - Poncho]",					"",				"md_quigon_poncho",				"quigon",				"model_poncho",					"quigon",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/quigon/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_QUI_GON_JINN",				 qtrue },
	{ "Qui-Gon Jinn",					"[Force Ghost]",						"",				"md_quigon_ghost",				"quigon",				"model_ghost",					"quigon",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/quigon/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_QUI_GON_JINN",				 qtrue },
	{ "Quinlan Vos",					"[TCW]",								"",				"md_quinlan",					"quinlan_vos",			"model_default",				"quinlan",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/quinlan/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_QUINLAN_VOS",				qfalse },
	{ "Satine Kryze",					"[Mandalore]",							"",				"md_duchess_satine",			"satine",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_SATINE_KRYZE",				qfalse },
	{ "Saesee Tiin",					"[Episode I-III]",						"",				"md_saesee",					"saesee_tiin",			"model_default",				"saesee",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saesee/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SAESEE_TIIN",				qfalse },
	{ "Saesee Tiin",					"[Episode I-III - Robed]",				"",				"md_saesee_robed",				"saesee_tiin",			"model_robed",					"saesee",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/saesee/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SAESEE_TIIN",				 qtrue },
	{ "Sarissa Jeng",					"[TCW]",								"",				"md_sarissa_jeng",				"sarissa_jeng",			"model_default",				"luminara",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_SARISSA_JENG",				qfalse },
	{ "Shaak Ti",						"[Episode I-III]",						"",				"md_shaak_ti",					"shaak_ti",				"model_default",				"shaak",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/shaak_ti/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SHAAK_TI",					qfalse },
	{ "Shaak Ti",						"[Exiled - Felucia]",					"",				"md_shaakti_tfu",				"shaakti_TFU",			"model_default",				"shaak",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/shaak_ti/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SHAAK_TI",					 qtrue },
	{ "Shakkra Kien",					"[TCW]",								"",				"md_guardboss_jt",				"jtguard_boss",			"model_default",				"temple_guard",				"",						"orange",		"",			7,		128,	ERA_REPUBLIC,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jtguard/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SHAKKRA_KIEN",				qfalse },
	{ "Sora Bulq",						"[TCW]",								"",				"md_sora",						"sora_bulq",			"model_default",				"saesee",					"",						"blue",			"",			1,		10,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sora_bulq/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SORA_BULQ",					qfalse },
	{ "Serra Keto",						"[Jedi]",								"",				"md_serra",						"serraketo",			"model_default",				"quigon",					"quigon",				"green",		"green",	6,		64,		ERA_REPUBLIC,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/serraketo/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_SERRA_KETO",					qfalse },
	{ "Stass Allie",					"[Episode I-III]",						"",				"md_stass_allie",				"stass_allie",			"model_default",				"coleman",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_STASS_ALLIE",				qfalse },
	{ "Stass Allie",					"[Robed]",								"",				"md_stass_allie_robed",			"stass_allie",			"model_robed",					"coleman",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_STASS_ALLIE",				 qtrue },
	{ "Tarados Gon",					"[TCW]",								"",				"md_tarados",					"tarados_gon",			"model_default",				"luminara",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedibrute/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_TARADOS_GON",				qfalse },
	{ "Tera Sinube",					"[TCW]",								"",				"md_tera_sinube",				"tera_sinube",			"model_default",				"sinube",					"",						"white",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_TERA_SINUBE",				qfalse },
	{ "Thongla Jur",					"[TCW]",								"",				"md_thongla_jur",				"thongla_jur",			"model_default",				"windu",					"",						"green",		"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		0.9f,	"@MD_CHAR_DESC_THONGLA_JUR",				qfalse },
	{ "Tiplar",							"[TCW]",								"",				"md_tiplar",					"tiplee",				"model_tiplar",					"shaak",					"",						"green",		"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tiplee/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TIPLAR",						qfalse },
	{ "Tiplee",							"[TCW]",								"",				"md_tiplee",					"tiplee",				"model_default",				"shaak",					"",						"blue",			"",			1,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tiplee/misc/gloat2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TIPLEE",						qfalse },
	{ "Tsui Choi",						"[TCW]",								"",				"md_tsuichoi",					"tsuichoi",				"model_default",				"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		0.8f,	"@MD_CHAR_DESC_TSUI_CHOI",					qfalse },
	{ "Yaddle",							"[Episode I]",							"",				"md_yaddle",					"yaddle",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/depabillaba/misc/taunt1.mp3",			"",		0.7f,	"@MD_CHAR_DESC_YADDLE",						qfalse },
	{ "Yarael Poof",					"[Episode I-III]",						"",				"md_yarael",					"yarael",				"model_default",				"yarael",					"",						"blue",			"",			1,		14,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yarael/misc/taunt4.mp3",				"",		0.9f,	"@MD_CHAR_DESC_YARAEL_POOF",				qfalse },
	{ "Yoda",							"[Episode III]",						"",				"md_yoda",						"yoda",					"model_default",				"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yoda/misc/taunt3.mp3",					"",		0.7f,	"@MD_CHAR_DESC_YODA",						qfalse },
	{ "Yoda",							"[Episode I-II]",						"",				"md_yoda_ep2",					"yoda",					"model_ep2",					"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yoda/misc/taunt1.mp3",					"",		0.7f,	"@MD_CHAR_DESC_YODA",						 qtrue },
	{ "Yoda",							"[High Republic]",						"",				"md_yoda_hr",					"yoda",					"model_hr",						"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yoda/misc/taunt1.mp3",					"",		0.7f,	"@MD_CHAR_DESC_YODA",						 qtrue },
	{ "Yoda",							"[Clone Wars]",							"",				"md_yoda_cw",					"yoda",					"model_cw",						"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yoda/misc/taunt1.mp3",					"",		0.7f,	"@MD_CHAR_DESC_YODA",						 qtrue },
	{ "Yoda",							"[Episode V]",							"",				"md_yoda_ot",					"yoda",					"model_OT",						"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yoda/misc/taunt1.mp3",					"",		0.7f,	"@MD_CHAR_DESC_YODA",						 qtrue },
	{ "Yoda",							"[Force Ghost]",						"",				"md_yoda_ghost",				"yoda",					"model_ghost",					"",							"",						"",				"",			5,		32,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yoda/misc/taunt1.mp3",					"",		0.7f,	"@MD_CHAR_DESC_YODA",						 qtrue },
	{ "Youngling",						"[Jedi Student]",						"",				"md_youngling",					"youngling",			"model_default",				"",							"",						"",				"",			1,		14,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/youngling/misc/taunt4.mp3",			"",		0.7f,	"@MD_CHAR_DESC_YOUNGLING",					qfalse },
	{ "Youngling",						"[Jedi Student]",						"",				"md_you5",						"youngfem",				"model_default",				"",							"",						"",				"",			1,		14,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/youngling/misc/taunt4.mp3",			"",		0.7f,	"@MD_CHAR_DESC_YOUNGLING",					 qtrue },
	{ "Youngling",						"[Jedi Student]",						"",				"md_you4",						"youngshak",			"model_default",				"",							"",						"",				"",			1,		14,		ERA_REPUBLIC,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/youngling/misc/taunt4.mp3",			"",		0.7f,	"@MD_CHAR_DESC_YOUNGLING",					 qtrue },
	{ "Zett Jukassa",					"[Episode III]",						"",				"md_zett_jukassa",				"zett_jukassa",			"model_default",				"coleman",					"",						"blue",			"",			2,		6,		ERA_REPUBLIC,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_ZETT_JUKASSA",				qfalse },
	

	// Separatist Alliance																										
	{ "Asajj Ventress",					"[TCW]",								"",				"md_ventress",					"asajj",				"model_default",				"ventress_tcw",				"ventress_tcw",			"red",			"red",		6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/assajj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ASAJJ_VENTRESS",				qfalse },
	{ "Asajj Ventress",					"[Staff]",								"",				"md_ven_dual",					"asajj",				"model_default",				"ventress_tcw_dual",		"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/assajj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ASAJJ_VENTRESS",				 qtrue },
	{ "Asajj Ventress",					"[Nightsister]",						"",				"md_ven_ns",					"asajj",				"model_ns",						"ventress_tcw",				"ventress_tcw",			"red",			"red",		6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/assajj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ASAJJ_VENTRESS",				 qtrue },
	{ "Asajj Ventress",					"[Bounty Hunter]",						"",				"md_ven_bh",					"asajj_bh",				"model_default",				"ventress_tcw",				"ventress_tcw",			"red",			"red",		6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/assajj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ASAJJ_VENTRESS",				 qtrue },
	{ "Asajj Ventress",					"[Disguised]",							"",				"md_ven_dg",					"asajj_bh",				"model_disguise",				"ventress_tcw",				"ventress_tcw",			"red",			"red",		6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/assajj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ASAJJ_VENTRESS",				 qtrue },
	{ "B1 Battle Droid",				"[Infantry]",							"",				"battledroid",					"battledroid",			"model_default",				"menu_wp_battledroid",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/battledroid/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_B1_BATTLE_DROID",			qfalse },
	{ "B1 Battle Droid",				"[Security]",							"",				"battledroid_security",			"battledroid",			"model_security",				"menu_wp_battledroid",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/battledroid/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_B1_BATTLE_DROID",			 qtrue },
	{ "B1 Battle Droid",				"[Pilot]",								"",				"battledroid_pilot",			"battledroid",			"model_pilot",					"menu_wp_battledroid",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/battledroid/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_B1_BATTLE_DROID",			 qtrue },
	{ "B1 Battle Droid",				"[Commander]",							"",				"battledroid_comm",				"battledroid",			"model_comm",					"menu_wp_battledroid",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/battledroid/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_B1_BATTLE_DROID",			 qtrue },
	{ "B1 Battle Droid",				"[Sniper]",								"",				"battledroid_sniper",			"battledroid",			"model_sniper",					"menu_wp_disruptor",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/battledroid/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_B1_BATTLE_DROID",			 qtrue },
	{ "B1 Battle Droid",				"[Geonosis]",							"",				"battledroid_geonosis",			"battledroid",			"model_geonosis",				"menu_wp_battledroid",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/battledroid/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_B1_BATTLE_DROID",			 qtrue },
	{ "B2 Super Battle Droid",			"[Battle Droid]",						"",				"md_sbd_am",					"SBD",					"model_default",				"",							"",						"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sbd/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_B2_SUPER_BATTLE_DROID",		qfalse },
	{ "Cad Bane",						"[Bounty Hunter]",						"",				"md_cadbane",					"cadbane",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cadbane/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_CAD_BANE",					qfalse },
	{ "Clone Assassin",					"[Jedi Temple]",						"",				"md_clone_assassin",			"clonetrooper_p2",		"model_assassin",				"arc_shiv",					"arc_shiv_dual",		"",				"",			6,		64,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ultraclones/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CLONE_ASSASSIN",				qfalse },
	{ "Count Dooku",					"[Episode II-III]",						"",				"md_dooku",						"dooku",				"model_default",				"dooku",					"",						"red",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				qfalse },
	{ "Count Dooku",					"[TCW]",								"",				"md_dooku_tcw_unrobed",			"dooku_tcw",			"model_unrobed",				"dooku",					"",						"red",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				 qtrue },
	{ "Count Dooku",					"[TCW - Robed]",						"",				"md_dooku_tcw",					"dooku_tcw",			"model_default",				"dooku",					"",						"red",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				 qtrue },
	{ "Count Dooku",					"[TCW - Dark Ritual]",					"",				"md_dooku_dr",					"dooku_dr",				"model_default",				"dooku",					"",						"red",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				 qtrue },
	{ "Count Dooku",					"[TCW - Pyjama]",						"",				"md_dooku_pj",					"dooku_pyjama",			"model_default",				"dooku",					"",						"red",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				 qtrue },
	{ "Count Dooku",					"[TCW - Pyjama Shoeless]",				"",				"md_dooku_pj_sl",				"dooku_pyjama",			"model_shoeless",				"dooku",					"",						"red",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				 qtrue },
	{ "Count Dooku",					"[Tales of the Jedi]",					"",				"md_dooku_totj",				"dooku_totj",			"model_default",				"dooku_jedi",				"",						"blue",			"",			4,		48,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dooku/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_COUNT_DOOKU",				 qtrue },
	{ "Darth Maul",						"[Episode I]",							"",				"md_maul",						"darthmaul",			"model_default",				"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_bf2/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					qfalse },
	{ "Darth Maul",						"[Robed]",								"",				"md_maul_robed",				"darthmaul",			"model_robed",					"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_bf2/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Hooded]",								"",				"md_maul_hooded",				"darthmaul",			"model_hood",					"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_bf2/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Warrior]",							"",				"md_maul_wots",					"darthmaul",			"model_noshirt_gloves",			"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_bf2/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[TCW]",								"",				"md_maul_tcw",					"maul_tcw",				"model_default",				"single_maul",				"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_tcw/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[TCW - Reborn]",						"",				"md_maul_cyber_tcw",			"maul_cyber_tcw",		"model_default",				"single_maul",				"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_tcw/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[TCW - Darksaber]",					"",				"md_maul_tcw_dual",				"maul_tcw",				"model_default",				"single_maul",				"darksaber_tcw",		"red",			"black",	6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_tcw/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[TCW - Staff]",						"",				"md_maul_tcw_staff",			"maul_tcw",				"model_default",				"maul_tcw",					"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_tcw/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Rebels]",								"",				"md_maul_rebels",				"maul_rebels",			"model_default",				"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Hooded]",								"",				"md_maul_rebels3",				"maul_rebels",			"model_shirtless_hooded",		"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Cowel]",								"",				"md_maul_rebels4",				"maul_rebels",			"model_shirtless_cowelbase",	"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Coweless]",							"",				"md_maul_rebels2",				"maul_rebels",			"model_shirtless",				"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Tatooine]",							"",				"md_maul_rebels5",				"maul_rebels",			"model_desert",					"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Twin Suns Duel]",						"",				"md_maul_rebels6",				"maul_rebels",			"model_twinsuns",				"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },	
	{ "Darth Maul",						"[Crimson Dawn]",						"",				"md_maul_solo",					"maul_tcw",				"model_solo",					"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Crimson Dawn - Hooded]",				"",				"md_maul_solo_hood",			"maul_tcw",				"model_solo_hood",				"maul_rebels",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maul_rebels/misc/gloat3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Old Wounds]",							"",				"md_mau_luke",					"cyber_maul",			"model_default",				"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cyber_maul/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Robed - Old Wounds]",					"",				"md_mau_luke_robed",			"cyber_maul",			"model_robed",					"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cyber_maul/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Maul",						"[Hooded - Old Wounds]",				"",				"md_mau_luke_hooded",			"cyber_maul",			"model_hood",					"dual_maul",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_CIN_8,							BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cyber_maul/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_MAUL",					 qtrue },
	{ "Darth Plagueis",					"[Sith]",								"",				"md_plagueis",					"darthplagueis",		"model_default",				"yarael",					"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthplagueis/misc/victory1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_PLAGUESIS",			qfalse },
	{ "Darth Sidious",					"[Emperor]",							"",				"md_sidious",					"palpatine",			"model_senate",					"sidious",					"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_sith/misc/taunt4.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_SIDIOUS",				qfalse },
	{ "Darth Sidious",					"[Episode III]",						"",				"md_sidious_ep3_red",			"palpatine",			"model_sith_hood",				"sidious",					"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_sith/misc/gloat3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_SIDIOUS",				 qtrue },
	{ "Darth Sidious",					"[Episode III - Robed]",				"",				"md_sidious_ep3_robed",			"palpatine",			"model_sith_hood2",				"sidious2_m",				"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_sith/misc/taunt4.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_SIDIOUS",				 qtrue },
	{ "Darth Sidious",					"[TCW]",								"",				"md_sidious_tcw",				"palpatine",			"model_robed_tcw",				"sidious2",					"sidious",				"red",			"red",		6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_sith/misc/gloat3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_SIDIOUS",				 qtrue },
	{ "Darth Sidious",					"[Episode I-II]",						"",				"md_sidious_ep2",				"palpatine",			"model_robed",					"sidious",					"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_sith/misc/gloat3.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_SIDIOUS",				 qtrue },
	{ "Sheev Palpatine",				"[Episode III]",						"",				"md_palpatine",					"palpatine",			"model_default",				"sidious",					"",						"red",			"",			1,		14,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_SIDIOUS",				 qtrue },
	{ "Droideka",						"[Battle Droid]",						"",				"md_dro_am",					"droideka",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		0.5f,	"@MD_CHAR_DESC_DROIDEKA",					qfalse },
	{ "Durge",							"[TCW]",								"",				"md_durge",						"durge",				"model_default",				"menu_wp_rocket_launcher",	"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/durge/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DURGE",						qfalse },
	{ "Durge",							"[Jetpack]",							"",				"md_durge_jetpack",				"durge",				"model_jetpack",				"menu_wp_rocket_launcher",	"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/durge/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DURGE",						 qtrue },
	{ "Embo",							"[Bounty Hunter]",						"",				"md_embo",						"embo",					"model_default",				"menu_wp_bowcaster",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/embo/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_EMBO",						qfalse },
	{ "General Grievous",				"[Episode III]",						"",				"md_grievous",					"grievous",				"model_default",				"single_1",					"single_1",				"blue",			"green",	6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/grievous_bf2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GENERAL_GRIEVOUS",			qfalse },
	{ "General Grievous",				"[Robed]",								"",				"md_grievous_robed",			"grievous",				"model_cape",					"single_1",					"single_1",				"blue",			"green",	6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/grievous_bf2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GENERAL_GRIEVOUS",			 qtrue },
	{ "General Grievous",				"[Four Arms]",							"",				"md_grievous4",					"grievous4",			"model_default",				"greeveeb4",				"greeveeg4",			"blue",			"green",	6,		64,		ERA_SEPARATIST,		BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/grievous_bf2/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GENERAL_GRIEVOUS",			 qtrue },
	{ "Hondo Ohnaka",					"[TCW]",								"",				"md_hondo",						"hondo",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hondo/misc/taunt2.mp3"		,			"",		1.0f,	"@MD_CHAR_DESC_HONDO_OHNAKA",				qfalse },
	{ "Jango Fett",						"[Episode II]",							"",				"md_jango",						"jangofett",			"model_default",				"menu_wp_jango",			"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jango_bf2/misc/anger2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JANGO_FETT",					qfalse },
	{ "Jango Fett",						"[Geonosis]",							"",				"md_jango_geo",					"jangofett",			"model_jetpack2",				"menu_wp_jango",			"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jango_bf2/misc/anger2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JANGO_FETT",					 qtrue },
	{ "Jango Fett",						"[Dual Pistols]",						"",				"md_jango_dual",				"jangofett",			"model_default",				"menu_wp_jango",			"menu_wp_jango",		"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jango_bf2/misc/anger2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JANGO_FETT",					 qtrue },
	{ "Lord Vader",						"[Episode III]",						"",				"md_ani_sith",					"anakin",				"model_sith",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_SEPARATIST,		BOTH_SABERTAVION_STANCE_JKA,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_sith/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LORD_VADER",					qfalse },
	{ "Lord Vader",						"[Episode III - Robed]",				"",				"md_ani_sith_robed",			"anakin",				"model_srobed",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_SEPARATIST,		BOTH_SABERTAVION_STANCE_JKA,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_sith/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LORD_VADER",					 qtrue },
	{ "Lord Vader",						"[Episode III - Hooded]",				"",				"md_ani_sith_hooded",			"anakin",				"model_shood",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_SEPARATIST,		BOTH_SABERTAVION_STANCE_JKA,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_sith/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LORD_VADER",					 qtrue },
	{ "Lord Vader",						"[Episode III - Mustafar]",				"",				"md_ani_bw",					"anakin",				"model_smus",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_SEPARATIST,		BOTH_SABERTAVION_STANCE_JKA,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_sith/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LORD_VADER",					 qtrue },
	{ "Anakin Skywalker",				"[TCW - Mortis]",						"",				"md_ani_tcw_mortis",			"anakin",				"model_mortis",					"anakin_ep3",				"",						"blue",			"",			1,		30,		ERA_SEPARATIST,		BOTH_SABERTAVION_STANCE_JKA,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/anakin_sith/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LORD_VADER",					 qtrue },
	{ "Magnaguard",						"[Grievous's Bodyguard]",				"",				"md_magnaguard",				"magnaguard",			"model_utapau",					"electrostaff",				"",						"",				"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/magnaguard/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MAGNAGUARD",					qfalse },
	{ "Magnaguard",						"[Dooku's Bodyguard]",					"",				"md_magnaguard_tcw",			"magnaguard",			"model_tcw",					"magnaguard_staff2",		"",						"",				"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/magnaguard/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_MAGNAGUARD",					 qtrue },
	{ "Mandalorian",					"[Death Watch]",						"",				"md_mando_maul",				"deathwatch",			"model_red",					"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mando_maul/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEATH_WATCH_MAUL",			qfalse },
	{ "Mandalorian",					"[Death Watch Warrior]",				"",				"md_mando_maul2",				"deathwatch",			"model_redhand",				"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mando_maul/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEATH_WATCH_MAUL",			 qtrue },
	{ "Mandalorian",					"[Death Watch Super Commando]",			"",				"md_mando_maul3",				"deathwatch",			"model_mauldalorian",			"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mando_maul/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEATH_WATCH_MAUL",			 qtrue },
	{ "Mother Talzin",					"[TCW]",								"",				"md_mother_talzin",				"MotherTalzin",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_MOTHER_TALZIN",				qfalse },
	{ "Neimoidian Guard",				"[Episode III]",						"",				"md_gua_am",					"NeimoidianSecurity",	"model_default",				"electrostaff",				"",						"",				"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/neimoidian_guard/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_NEIMOIDIAN_GUARD",			qfalse },
	{ "Neimoidian Brute",				"[Episode III]",						"",				"md_gua2_am",					"neimoidian_guard",		"model_default",				"gammobaxe",				"",						"",				"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/neimoidian_guard/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_NEIMOIDIAN_BRUTE",			 qtrue },
	{ "Nute Gunray",					"[Episode I-III]",						"",				"md_gunray",					"gunray_ep3",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/gunray/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_NUTE_GUNRAY",				qfalse },
	{ "Pre Vizsla",						"[TCW - Season 2]",						"",				"md_mando_vizsla",				"deathwatch",			"model_vizsla",					"darksaber_tcw",			"",						"black",		"",			2,		4,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRE_VIZSLA",					qfalse },
	{ "Pre Vizsla",						"[TCW - Season 4]",						"",				"md_mando_vizsla2",				"deathwatch",			"model_vizsla2",				"darksaber_tcw",			"",						"black",		"",			2,		4,		ERA_SEPARATIST,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRE_VIZSLA",					 qtrue },
	{ "Savage Opress",					"[TCW]",								"",				"md_savage",					"savage_opress",		"model_default",				"dual_opress",				"",						"red",			"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/savage_opress/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_SAVAGE_OPRESS",				qfalse },
	{ "Shu Mai",						"[Episode II-III]",						"",				"md_shu_mai",					"shu_mai",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_SHU_MAI",					qfalse },
	{ "Tusken Raider",					"[Episode II]",							"",				"md_tus1_tc",					"tusken_ep1n2",			"model_default",				"menu_wp_tusken_staff",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tusken/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TUSKAN_RAIDER",				qfalse },
	{ "Tusken Sniper",					"[Episode II]",							"",				"md_tus2_tc",					"tusken_ep1n2",			"model_default",				"menu_wp_tusken_rifle",		"",						"",				"",			0,		0,		ERA_SEPARATIST,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tusken/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TUSKAN_RAIDER",				 qtrue },
	{ "Tusken Chieftain",				"[Episode II]",							"",				"md_tus5_tc",					"tusken_quarak",		"model_ep2",					"tusken_chieftain_gaffi",	"",						"",				"",			7,		128,	ERA_SEPARATIST,		BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tusken/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TUSKAN_RAIDER",				 qtrue },
	{ "Wat Tambor",						"[Episode II-III]",						"",				"md_wat_tambor",				"wat_tambor",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_SEPARATIST,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/wat_tambor/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_WAT_TAMBOR",					qfalse },


#if 1
	// Rebel Alliance
	{ "A'Sharad Hett",					"[Legends]",							"",				"md_asharad",					"asharad_hett",			"model_default",				"asharad",					"asharad2",				"green",		"green",	6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/asharad_hett/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_A_SHARAD_HETT",				qfalse },
	{ "A'Sharad Hett",					"[Tusken Mask]",						"",				"md_asharad_tus",				"asharad_hett",			"model_tusken",					"asharad",					"asharad2",				"green",		"green",	6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/asharad_hett/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_A_SHARAD_HETT",				 qtrue },
	{ "Admiral Ackbar",					"[Episode VI]",							"",				"md_ackbar",					"ackbar",				"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ackbar/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ADMIRAL_ACKBAR",				qfalse },
	{ "Baze Malbus",					"[Rogue One]",							"",				"md_baze_malbus",				"Baze",					"model_default",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/baze/misc/victory2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BAZE_MALBUS",				qfalse },
	{ "Ben Kenobi",						"[Robed]",								"",				"md_ben_robed",					"obiwan_ot",			"model_default_robed",			"obiwan_ep4",				"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/benkenobi/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BEN_KENOBI",					qfalse },
	{ "Ben Kenobi",						"[Hooded]",								"",				"md_ben_hooded",				"obiwan_ot",			"model_default_hooded",			"obiwan_ep4",				"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/benkenobi/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BEN_KENOBI",					 qtrue },
	{ "Ben Kenobi",						"[Force Ghost]",						"",				"md_ben_ghost",					"obiwan_ot",			"model_ghost",					"obiwan_ep4",				"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/benkenobi/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BEN_KENOBI",					 qtrue },
	{ "Bespin Security",				"[Episode V]",							"",				"bespincop",					"bespin_cop",			"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bespincop1/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BESPIN_SECURITY",			qfalse },
	{ "Bodhi Rook",						"[Rogue One]",							"",				"md_bodhi",						"bodhi",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/bodhi/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BODHI_ROOK",					qfalse },
	{ "C-3PO",							"[Droid]",								"",				"md_c3po_char",					"protocol",				"model_ep3",					"",							"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/protocol/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_C3PO",						qfalse },
	{ "Cade Skywalker",					"[Legends]",							"",				"md_cade",						"cade",					"model_default",				"anakin_ep3",				"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cade/misc/taunt6.mp3",					"",		1.0f,	"@MD_CHAR_DESC_CADE_SKYWALKER",				qfalse },
	{ "Cal Kestis",						"[Jedi Fallen Order]",					"",				"cal_kestis",					"cal_kestis",			"model_default",				"cal",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					qfalse },
	{ "Cal Kestis",						"[Poncho]",								"",				"cal_kestis_cape",				"cal_kestis",			"model_cape",					"cal",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					 qtrue },
	{ "Cal Kestis",						"[Staff]",								"",				"cal_kestis_staff",				"cal_kestis",			"model_default2",				"cal_staff",				"",						"blue",			"",			7,		128,	ERA_REBELS,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					 qtrue },
	{ "Cal Kestis",						"[Padawan]",							"",				"cal_kestis_jedi",				"cal_kestis_jedi",		"model_default",				"cal",						"",						"blue",			"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					 qtrue },
	{ "Inquisitor Kestis",				"[Inquisitor]",							"",				"cal_inquisitor",				"cal_inquisitor",		"model_default",				"cal_staff",				"",						"red",			"",			7,		128,	ERA_REBELS,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					 qtrue },
	{ "Cal Kestis",						"[Jedi Survivor]",						"",				"cal_survivor",					"cal_survivor",			"model_default",				"cal_js",					"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					 qtrue },
	{ "Cal Kestis",						"[Jedi Survivor - Staff]",				"",				"cal_survivor_staff",			"cal_survivor",			"model_default",				"cal_staff_js",				"",						"blue",			"",			7,		128,	ERA_REBELS,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cal_kestis/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAL_KESTIS",					 qtrue },
	{ "Caleb Dume",						"[Rebels]",								"",				"md_caleb",						"caleb_dume",			"model_default",				"kanan_tbb",				"",						"blue",			"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/caleb_dume/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CALEB_DUME",					qfalse },
	{ "Caleb Dume",						"[Robed]",								"",				"md_caleb_robed",				"caleb_dume",			"model_robed",					"kanan_tbb",				"",						"blue",			"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/caleb_dume/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CALEB_DUME",					 qtrue },
	{ "Caleb Dume",						"[Hooded]",								"",				"md_caleb_hooded",				"caleb_dume",			"model_hooded",					"kanan_tbb",				"",						"blue",			"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/caleb_dume/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CALEB_DUME",					 qtrue },
	{ "Captain Rex",					"[Old]",								"",				"md_rex_old",					"rex_old",				"model_default",				"menu_wp_clonepistol",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rex_rebels/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAPTAIN_REX_OT",				qfalse },
	{ "Captain Rex",					"[Endor - Old]",						"",				"md_rex_endor",					"rex_endor",			"model_default",				"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rex_rebels/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CAPTAIN_REX_OT",				 qtrue },
	{ "Cassian Andor",					"[Rogue One]",							"",				"md_cassian",					"cassian",				"model_default",				"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cassian/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CASSIAN_ANDOR",				qfalse },
	{ "Cassian Andor",					"[Parka Jacket]",						"",				"md_cassian_parka",				"cassian",				"model_parka",					"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cassian/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CASSIAN_ANDOR",				 qtrue },
	{ "Cassian Andor",					"[Protective Vest]",					"",				"md_cassian_vest",				"cassian",				"model_vest",					"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cassian/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CASSIAN_ANDOR",				 qtrue },
	{ "Cassian Andor",					"[Scarif]",								"",				"md_cassian_scarif",			"cassian",				"model_scarif",					"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cassian/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CASSIAN_ANDOR",				 qtrue },
	{ "Chewbacca",						"[Episode IV]",							"",				"md_chewie",					"chewbacca",			"model_ot",						"menu_wp_bowcaster",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/chewbacca/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CHEWBACCA",					qfalse },
	{ "Chewbacca",						"[Bespin]",								"",				"md_chewie_bespin",				"chewbacca",			"model_bespin",					"menu_wp_bowcaster",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/chewbacca/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CHEWBACCA",					 qtrue },
	{ "Chirrut Imwe",					"[Rogue One]",							"",				"md_chirrut",					"Chirrut",				"model_default",				"menu_wp_bowcaster",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/chirrut/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_CHIRRUT_IMWE",				qfalse },
	{ "Corran Horn",					"[Legends]",							"",				"md_corran",					"corran",				"model_default",				"galenmarek_alt",			"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CORRAN_HORN",				qfalse },
	{ "Corran Horn",					"[Robed]",								"",				"md_corran_robed",				"corran",				"model_robe",					"galenmarek_alt",			"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CORRAN_HORN",				 qtrue },
	{ "Corran Horn",					"[Hooded]",								"",				"md_corran_hooded",				"corran",				"model_hood",					"galenmarek_alt",			"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi_spanki/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_CORRAN_HORN",				 qtrue },
	{ "Ezra Bridger",					"[Seasons 1-2]",						"",				"md_ezra_s1",					"ezra",					"model_default",				"ezra",						"",						"blue",			"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ezra/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_EZRA_BRIDGER",				qfalse },
	{ "Ezra Bridger",					"[Seasons 3-4]",						"",				"md_ezra",						"ezrabridger",			"model_default",				"ezra2",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/ezra/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_EZRA_BRIDGER",				 qtrue },
	{ "Galen Marek",					"[Jedi Adventure Robes]",				"",				"md_galen",						"stk_adventure_robes",	"model_default",				"starkiller_m",				"",						"blue",			"",			7,		128,	ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				qfalse },
	{ "Galen Marek",					"[Temple Exploration Gear]",			"",				"md_galen_jt",					"stk_temple_eg",		"model_default",				"starkiller_m",				"",						"blue",			"",			7,		128,	ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Corellian Flight Suit]",				"",				"md_galen_cor",					"stk_corellian_fs",		"model_default",				"starkiller_m",				"",						"blue",			"",			7,		128,	ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Ceremonial Jedi Robes]",				"",				"md_galencjr",					"stk_ceremonial_robes",	"model_default",				"galenmarek_cjr_m",			"",						"green",		"",			7,		128,	ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Tie Flightsuit]",						"",				"md_galen_tie",					"stk_tiepilot",			"model_default",				"galenmarek",				"galenmarek",			"red",			"red",		6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Arena Combat Gear]",					"",				"md_galen_arena",				"stk_arena",			"model_default",				"galenmarek",				"galenmarek",			"blue",			"blue",		6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Hero's Armor]",						"",				"md_galen_hero",				"stk_rebel_general",	"model_default",				"galenmarek",				"galenmarek",			"blue",			"blue",		6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Test Subject Garments]",				"",				"md_galen_kamino",				"stk_kamino",			"model_default",				"galenmarek",				"galenmarek",			"red",			"red",		6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "Galen Marek",					"[Evil Clone Experiment]",				"",				"md_galen_clone",				"stk_kamino",			"model_clone",					"galenmarek",				"galenmarek",			"red",			"red",		6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GALEN_MAREK",				 qtrue },
	{ "GNK Droid",						"[Droid]",								"",				"gonk",							"gonk",					"model_default",				"",							"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/gonk/misc/gonktalk1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GNK_DROID",					qfalse },
	{ "Han Solo",						"[A New Hope]",							"",				"md_han_anh",					"captain_solo",			"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/han_anh/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_HAN_SOLO",					qfalse },
	{ "Han Solo",						"[Empire Strikes Back]",				"",				"md_han_esb",					"captain_solo",			"model_esb",					"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/han_esb/misc/taunt5.mp3",				"",		1.0f,	"@MD_CHAR_DESC_HAN_SOLO",					 qtrue },
	{ "Han Solo",						"[Return of the Jedi]",					"",				"md_han_rotj",					"captain_solo",			"model_rotj",					"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/han_esb/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_HAN_SOLO",					 qtrue },
	{ "Han Solo",						"[Young]",								"",				"md_han_young",					"han_young",			"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/han_anh/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_HAN_SOLO",					 qtrue },
	{ "Hera Syndulla",					"[Rebels]",								"",				"md_hera",						"hera",					"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hera/misc/victory2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_HERA_SYNDULLA",				qfalse },
	{ "Jan Ors",						"[Jedi Outcast]",						"",				"jan",							"jan",					"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jan/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_JAN_ORS",					qfalse },
	{ "Jan Ors",						"[Dark Forces 2]",						"",				"md_jan_df2",					"jan",					"model_df2",					"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jan/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_JAN_ORS",					 qtrue },
	{ "Jaro Tapal",						"[Jedi Fallen Order]",					"",				"jaro_tapal",					"jaro_tapal",			"model_default",				"cal_staff",				"",						"blue",			"",			7,		128,	ERA_REBELS,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jarotapal/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JARO_TAPAL",					qfalse },
	{ "Jedi Knight",					"[Jedi]",								"",				"jedi",							"jedi",					"model_default",				"single_2",					"",						"yellow",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi1/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				qfalse },
	{ "Jedi Knight",					"[Jedi]",								"",				"jedi2",						"jedi",					"model_j2",						"single_7",					"",						"orange",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi2/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_KNIGHT",				 qtrue },
	{ "Jedi Master",					"[Jedi]",								"",				"jedimaster",					"jedi",					"model_master",					"dual_3",					"",						"green",		"",			7,		128,	ERA_REBELS,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi2/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_MASTER",				 qtrue },
	{ "Jedi Trainer",					"[Jedi]",								"",				"jeditrainer",					"jeditrainer",			"model_default",				"jedi",						"jedi",					"purple",		"purple",	6,		64,		ERA_REBELS,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jedi2/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEDI_TRAINER",				 qtrue },
	{ "Jod Na Nawood",					"[Skeleton Crew]",						"",				"md_jodnanawood",				"jodnanawood",			"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jodnanawood/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_JO_NA_NAWOOD",				qfalse },
	{ "Jyn Erso",						"[Rogue One]",							"",				"md_jyn_erso",					"jynerso",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jynerso/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JYN_ERSO",					qfalse },
	{ "K-2SO",							"[Rogue One]",							"",				"md_k2so",						"k2so",					"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/k2so/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_K2SO",						qfalse },
	{ "Kanan Jarrus",					"[Rebels]",								"",				"md_kanan",						"kanan",				"model_default",				"kanan",					"",						"blue",			"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kanan/misc/victory2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KANAN_JARRUS",				qfalse },
	{ "Kanan Jarrus",					"[Blind]",								"",				"md_kanan_blind",				"kanan",				"model_blind",					"kanan",					"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kanan/misc/gloat2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KANAN_JARRUS",				 qtrue },
	{ "Kento Marek",					"[TFU]",								"",				"md_kento_marek",				"kentomarek",			"model_default",				"drallig",					"",						"blue",			"",			1,		38,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kentomarek/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KENTO_MAREK",				qfalse },
	{ "Kento Marek",					"[Alternate Design]",					"",				"md_kento_marek2",				"kentomarek",			"model_wii",					"drallig",					"",						"blue",			"",			1,		38,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kentomarek/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KENTO_MAREK",				 qtrue },
	{ "Kota's Militia",					"[Saboteur]",							"",				"militiasaboteur",				"militiasaboteur",		"model_default",				"menu_wp_rebelblaster",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/militiasaboteur/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KOTAS_MILITIA",				qfalse },
	{ "Kota's Militia",					"[Trooper]",							"",				"militiatrooper",				"militiatrooper",		"model_default",				"militia_baton",			"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/militiasaboteur/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KOTAS_MILITIA",				 qtrue },
	{ "Kyle Katarn",					"[Jedi Outcast / Academy]",				"",				"md_kyle",						"kyle",					"model_default",				"Kyle",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_jo/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				qfalse },
	{ "Kyle Katarn",					"[Dark]",								"",				"md_kyle_alt",					"kyle_alternate",		"model_default",				"Kyle",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Mercenary]",							"",				"md_kyle_df1",					"kyleDF1",				"model_default",				"stunbaton",				"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_df2/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Dark Forces 2 - Coat]",				"",				"md_kyle_df2_coat",				"kyleDF2",				"model_coat",					"rahn",						"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_df2/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Dark Forces 2]",						"",				"md_kyle_df2",					"kyleDF2",				"model_default",				"rahn",						"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_df2/misc/victory2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Mysteries of the Sith]",				"",				"md_kyle_mots",					"kyle_mots",			"model_default",				"yun",						"",						"orange",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_mots/misc/anger3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Stealth Jedi Gear]",					"",				"md_kyle_sj",					"kyle_SJ",				"model_default",				"Kyle",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_jo/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Jedi Master]",						"",				"md_kyle_jm",					"kyle_jedimaster",		"model_default",				"Kyle",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_master/misc/anger1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Emperor Katarn",					"[Sith]",								"",				"md_kyle_emp",					"kyle_jedimaster",		"model_default_emperor",		"rahn",						"",						"red",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_emperor/misc/anger2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[DF2 In-Game]",						"",				"md_kyle_df2ig",				"kyle_lowRemake",		"model_default",				"yun",						"",						"yellow",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_df2/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Imperial Officer]",					"",				"md_kyle_officer",				"Kyle_officer",			"model_default",				"stunbaton",				"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_df2/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[Old]",								"",				"md_kyle_old",					"kyle_old",				"model_default",				"Kyle",						"",						"blue",			"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_master/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Kyle Katarn",					"[1997 Low-Poly]",						"",				"md_kyle_df2lowpoly",			"kyle_df2low",			"model_default",				"yun",						"",						"yellow",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kyle_df2/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLE_KATARN",				 qtrue },
	{ "Lando Calrissian",				"[Episode V]",							"",				"md_lando",						"landoT",				"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/lando/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LANDO_CALRISSIAN",			qfalse },
	{ "Lando Calrissian",				"[Bespin Administrator]",				"",				"md_lando_bespin",				"landoT",				"model_bespin",					"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/lando/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LANDO_CALRISSIAN",			 qtrue },
	{ "Lando Calrissian",				"[Alliance General]",					"",				"md_lando_endor",				"landoT",				"model_endor",					"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/lando/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LANDO_CALRISSIAN",			 qtrue },
	{ "Lando Calrissian",				"[Jabba's Palace Disguise]",			"",				"md_lando_jabba",				"landoskiff",			"model_default",				"tusken_chieftain_gaffi",	"",						"",				"",			2,		4,		ERA_REBELS,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/lando/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LANDO_CALRISSIAN",			 qtrue },
	{ "Leia Organa",					"[Princess]",							"",				"md_leia_anh",					"leia_anh",				"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/leiaorgana/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LEIA_ORGANA",				qfalse },
	{ "Leia Organa",					"[Hoth]",								"",				"md_leia_esb_hoth",				"leia_esb",				"model_hoth",					"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/leiaorgana/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LEIA_ORGANA",				 qtrue },
	{ "Leia Organa",					"[Bespin]",								"",				"md_leia_esb",					"leia_esb",				"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/leiaorgana/misc/victory2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LEIA_ORGANA",				 qtrue },
	{ "Leia Organa",					"[Jabba's Slave]",						"",				"md_leia_slave",				"leia_slave",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/leiaorgana/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LEIA_ORGANA",				 qtrue },
	{ "Leia Organa",					"[Endor]",								"",				"md_leia_endor",				"leia_endor",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/leiaorgana/misc/victory2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LEIA_ORGANA",				 qtrue },
	{ "Leia Organa",					"[Endor - Poncho]",						"",				"md_leia_endor_poncho",			"leia_endor",			"model_poncho",					"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/leiaorgana/misc/victory2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LEIA_ORGANA",				 qtrue },
	{ "Luke Skywalker",					"[Jedi Knight]",						"",				"md_luke_rotj",					"luke_rotj",			"model_default",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				qfalse },
	{ "Luke Skywalker",					"[Jedi Knight - Exposed Flap]",			"",				"md_luke_fd",					"luke_rotj",			"model_default_fd",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Endor Camouflage]",					"",				"md_luke_endor",				"luke_rotj",			"model_endor",					"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Endor Camouflage - Helmetless]",		"",				"md_luke_endor2",				"luke_rotj",			"model_endor_nohelmet",			"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi - Tunic]",						"",				"md_luke_tunic",				"luke_rotj",			"model_tunic",					"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi - Robed]",						"",				"md_luke_robed",				"luke_rotj",			"model_tunic_robe",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi - Hooded]",						"",				"md_luke_hooded",				"luke_rotj",			"model_tunic_hood",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Hoth]",								"",				"md_luke_hoth",					"luke_hoth",			"model_default",				"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Pilot]",								"",				"md_luke_pilot",				"luke_pilot",			"model_default",				"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Pilot - Helmet]",						"",				"md_luke_pilothlm",				"luke_pilot",			"model_helmet",					"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Pilot - Sith]",						"",				"md_luke_sith",					"luke_pilot",			"model_sith",					"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_fallen/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Dagobah]",							"",				"md_luke_dago",					"luke_dago",			"model_default",				"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Dagobah - Yoda In Backpack]",			"",				"md_luke_dago_backpack",		"luke_dago",			"model_backpack",				"luke_ep5",					"",						"blue",			"",			2,		6, 		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Bespin]",								"",				"md_luke_esb",					"luke_esb",				"model_default",				"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Bespin - Dirty]",						"",				"md_luke_esb_dirty",			"luke_esb",				"model_dirty",					"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Bespin - Battleworn]",				"",				"md_luke_esb_bw",				"luke_esb",				"model_bw",						"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Bespin - Black]",						"",				"md_luke_esb_ds",				"luke_esb",				"model_ds",						"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Yavin Ceremony]",						"",				"md_luke_yavin",				"luke_yavin",			"model_default",				"luke_ep4",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_esb/misc/gloat1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Farm Boy]",							"",				"md_luke_anh",					"luke_anh",				"model_default",				"luke_ep4",					"",						"blue",			"",			2,		4,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_anh/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi Master]",						"",				"md_luke_tbobf",				"luke_trainer",			"model_nobag",					"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_anh/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi Master - Grogu In Backpack]",	"",				"md_luke_tbobf_bag",			"luke_trainer",			"model_default",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_anh/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi Master - Robed]",				"",				"md_luke_robed_tm",				"luke_rotj",			"model_tm_robe",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi Master - Hooded]",				"",				"md_luke_hooded_tm",			"luke_rotj",			"model_tm_hood",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Battlefront 2]",						"",				"md_luke_bf2",					"luke_rotj",			"model_bf2",					"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[JKA]",								"",				"md_luke_jka",					"luke_rotj",			"model_jka",					"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Jedi Master - TROKR]",				"",				"md_luke_jedi_master",			"luke_rotj",			"model_master",					"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Pre-ROTJ - Temple]",					"",				"md_luke_temple",				"luke_temple",			"model_default",				"temple_guard2_single",		"",						"yellow",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Grand Master]",						"",				"luke_gm",						"luke_gm",				"model_default",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Shadows of the Empire]",				"",				"md_luke_sote",					"luke_sote",			"model_default",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luke Skywalker",					"[Dark Empire]",						"",				"md_luke_de",					"luke_de",				"model_default",				"luke_ep6",					"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luuke Skywalker",				"[Evil Clone]",							"",				"md_luuke",						"luuke",				"model_default",				"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luuke Skywalker",				"[Evil Clone - Hooded]",				"",				"md_luuke2",					"luuke",				"model_hood",					"luke_ep5",					"",						"blue",			"",			2,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_rotj/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER",				 qtrue },
	{ "Luthen Rael",					"[Andor]",								"",				"md_luthen",					"luthen",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luthen/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUTHEN_RAEL",				qfalse },
	{ "Mara Jade",						"[Legends]",							"",				"md_mara",						"mara_jumpsuit",		"model_nocape",					"mara",						"",						"purple",		"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mara/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_MARA_JADE",					qfalse },
	{ "Mara Jade",						"[Mysteries of the Sith]",				"",				"md_mara_mots",					"mara",					"model_default",				"mara_mots",				"",						"purple",		"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mara/misc/anger1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_MARA_JADE",					 qtrue },
	{ "Mon Mothma",						"[Episode IV]",							"",				"md_mon_mothma",				"monmothma",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mothma/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_MON_MOTHMA",					qfalse },
	{ "Mon Mothma",						"[Young]",								"",				"md_mon_mothma_young",			"mothma_young",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/mothma/misc/victory1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_MON_MOTHMA",					 qtrue },
	{ "Nien Nunb",						"[Episode VI]",							"",				"md_nien_nunb",					"niennunb",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/niennunb/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_NIEN_NUNB",					qfalse },
	{ "Qu Rahn",						"[Dark Forces II]",						"",				"md_rahn",						"Qu_Rahn",				"model_default",				"rahn",						"",						"green",		"",			1,		14,		ERA_REBELS,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_QU_RAHN",					qfalse },
	{ "R2-D2",							"[Droid]",								"",				"md_r2d2_char",					"r2d2",					"model_default",				"",							"",						"",				"",			0,		0,		ERA_REBELS,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/r2d2/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_R2D2",						qfalse },
	{ "Rahm Kota",						"[TFU]",								"",				"md_kota",						"kota",					"model_default",				"kota",						"",						"green",		"",			2,		36,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kota/misc/taunt2.wav",					"",		1.0f,	"@MD_CHAR_DESC_RAHM_KOTA",					qfalse },
	{ "Rahm Kota",						"[Drunken]",							"",				"md_kota_drunk",				"kota_drunk",			"model_default",				"kota",						"",						"green",		"",			2,		36,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kota_drunk/misc/victory1.wav",			"",		1.0f,	"@MD_CHAR_DESC_RAHM_KOTA",					 qtrue },
	{ "Rahm Kota",						"[Alliance General]",					"",				"md_kota_blind",				"kota",					"model_blind",					"kota",						"",						"green",		"",			2,		36,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kota/misc/taunt2.wav",					"",		1.0f,	"@MD_CHAR_DESC_RAHM_KOTA",					 qtrue },
	{ "Rebel Alliance Trooper",			"[Episode IV]",							"",				"rebel2",						"rebel",				"model_default",				"menu_wp_rebelblaster",		"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel1/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REBEL_ALLIANCE_TROOPER",		qfalse },
	{ "Rebel Alliance Pilot",			"[Episode IV]",							"",				"md_rebel_pilot",				"rebel_pilot_tfu",		"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/militiatrooper/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_REBEL_ALLIANCE_PILOT",		 qtrue },
	{ "Rosh Penin",						"[Jedi Academy]",						"",				"rosh_penin",					"rosh_penin_new",		"model_default",				"training",					"",						"yellow",		"",			3,		8,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rosh/misc/victory2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ROSH_PENIN",					qfalse },
	{ "Sabine Wren",					"[Rebels]",								"",				"md_sabine",					"sabine",				"model_default",				"darksaber_reb",			"",						"black",		"",			1,		6,		ERA_REBELS,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sabine/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SABINE_WREN",				qfalse },
	{ "Sabine Wren",					"[Rebels - Helmet]",					"",				"md_sabine_helmet",				"sabine",				"model_helmet",					"menu_wp_clonepistol",		"menu_wp_clonepistol",	"",				"",			0,		0,		ERA_REBELS,			BOTH_WEAPONREST2P,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sabine/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SABINE_WREN",				 qtrue },
	{ "Tobias Beckett",					"[Andor]",								"",				"md_beckett",					"beckett",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_REBELS,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/beckett/misc/anger1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TOBIAS_BECKETT",				qfalse },


	// Galactic Empire
	{ "Alora",							"[Jedi Academy]",						"",				"alora_dual",					"alora",				"model_default",				"single_4",					"single_4",				"red",			"red",		6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/alora/misc/anger1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_ALORA",						qfalse },
	{ "Baylan Skoll",					"[Ahsoka]",								"",				"md_baylan",					"baylan",				"model_default",				"baylan",					"",						"red",			"",			2,		12,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/baylan/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BAYLAN_SKOLL",				qfalse },
	{ "Baylan Skoll",					"[Robed]",								"",				"md_baylan_robed",				"baylan",				"model_cape",					"baylan",					"",						"red",			"",			2,		12,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/baylan/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BAYLAN_SKOLL",				 qtrue },
	{ "Baylan Skoll",					"[Hooded]",								"",				"md_baylan_hooded",				"baylan",				"model_hood",					"baylan",					"",						"red",			"",			2,		12,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/baylan/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BAYLAN_SKOLL",				 qtrue },
	{ "Boba Fett",						"[The Empire Strikes Back]",			"",				"boba_fett_esb",				"bobafett",				"model_ESB",					"menu_wp_boba",				"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boba_fett/misc/anger1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BOBA_FETT",					qfalse },
	{ "Boba Fett",						"[Return of the Jedi]",					"",				"boba_fett_rotj",				"bobafett",				"model_default",				"menu_wp_boba",				"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boba_bf2/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_BOBA_FETT",					 qtrue },
	{ "Boc",							"[Dark Forces II]",						"",				"md_boc",						"boc",					"model_default",				"boc",						"boc",					"purple",		"purple",	6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boc/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_BOC",						qfalse },
	{ "Bossk",							"[Episode V]",							"",				"md_bossk",						"bossk",				"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/trandoshan/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BOSSK",						qfalse },
	{ "Dagan Gera",						"[Jedi Fallen Order]",					"",				"md_dagan",						"dagan_gera",			"model_default",				"dagan_staff",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dagan/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DAGAN_GERA",					qfalse },
	{ "Dagan Gera",						"[No Arm]",								"",				"md_dagan_noarm",				"dagan_gera",			"model_noarm",					"dagan_staff",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dagan/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DAGAN_GERA",					 qtrue },
	{ "Dagan Gera",						"[Shirtless]",							"",				"md_dagan_shirtless",			"dagan_gera",			"model_shirtless",				"dagan",					"",						"red",			"",			1,		14,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dagan/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DAGAN_GERA",					 qtrue },
	{ "Dagan Gera",						"[Jedi]",								"",				"md_dagan_jedi",				"dagan_gera",			"model_jedi",					"dagan",					"",						"yellow",		"",			1,		14,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/dagan/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DAGAN_GERA",					 qtrue },
	{ "Death Trooper",					"[Rogue One]",							"",				"md_deathtrooper",				"deathtrooper",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/deathtrooper/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEATH_TROOPER",				qfalse },
	{ "Death Trooper",					"[Commander]",							"",				"md_deathtrooper_commander",	"deathtrooper",			"model_commander",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/deathtrooper/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DEATH_TROOPER",				 qtrue },
	{ "Dark Trooper",					"[The Mandalorian]",					"",				"md_darktrooper",				"darktrooper_tv",		"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/3rd_dt/misc/taunt1.mp3",				"",		0.7f,	"@MD_CHAR_DESC_DARK_TROOPER",				qfalse },
	{ "Darth Desolous",					"[Legends]",							"",				"darthdesolous",				"darthdesolous",		"model_default",				"desolous",					"desolous_shield",		"red",			"",			6,		64,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthdesolous/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_DESOLOUS",				qfalse },
	{ "Darth Krayt",					"[Emperor]",							"",				"md_krayt",						"darthkrayt",			"model_default",				"asharad",					"asharad2",				"red",			"red",		6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthkrayt/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_KRAYT",				qfalse },
	{ "Darth Krayt",					"[Reborn]",								"",				"md_krayt_reborn",				"darthkrayt_r",			"model_default",				"asharad",					"asharad2",				"red",			"green",	6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthkrayt/misc/taunt5.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_KRAYT",				 qtrue },
	{ "Darth Phobos",					"[TFU]",								"",				"darthphobos",					"darthphobos",			"model_default",				"phobos",					"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthphobos/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_PHOBOS",				qfalse },
	{ "Darth Talon",					"[Legends]",							"",				"md_talon",						"darthtalon",			"model_default",				"talon",					"",						"red",			"",			1,		38,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthtalon/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_TALON",				qfalse },
	{ "Darth Vader",					"[Episode III]",						"",				"md_vader_ep3",					"darthvader",			"model_ep3",					"vader_ro_m",				"",						"red",			"",			1,		30,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthvader/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_VADER",				qfalse },
	{ "Darth Vader",					"[Episode IV]",							"",				"md_vader_anh",					"darthvader",			"model_anh",					"vader_ep4_m",				"",						"red",			"",			1,		30,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthvader/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_VADER",				 qtrue },
	{ "Darth Vader",					"[Episode V-VI]",						"",				"md_vader",						"darthvader",			"model_default",				"vader_m",					"",						"red",			"",			1,		30,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthvader/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_VADER",				 qtrue },
	{ "Darth Vader",					"[Kenobi Damaged]",						"",				"md_vader_tv",					"darthvader",			"model_tv",						"vader_ro_m",				"",						"red",			"",			1,		30,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthvader_tv/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_DARTH_VADER",				 qtrue },
	{ "Darth Vader",					"[Battle Damaged]",						"",				"md_vader_bw",					"darthvader",			"model_battle",					"vader_ro_m",				"",						"red",			"",			1,		30,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/darthvader/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DARTH_VADER",				 qtrue },
	{ "Desann",							"[Jedi Outcast]",						"",				"md_desann",					"desann",				"model_default",				"desann",					"",						"red",			"",			4,		16, 	ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/desann/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DESANN",						qfalse },
	{ "Lord Desann",					"[Jedi Outcast]",						"",				"md_desann_robed",				"desann",				"model_robed",					"desann",					"",						"red",			"",			4,		16,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/desann/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DESANN",						 qtrue },
	{ "Director Orson Krennic",			"[Rogue One]",							"",				"md_krennic",					"krennic",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/krennic/misc/victory1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_DIRECTOR_ORSON_KRENNIC",		qfalse },
	{ "Emperor Palpatine",				"[Episode VI]",							"",				"md_emperor",					"palpatine_fa",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_emperor/misc/victory2.mp3",	"",		1.0f,	"@MD_CHAR_DESC_EMPEROR_PALPATINE",			qfalse },
	{ "Emperor Palpatine",				"[Lightsaber]",							"",				"md_emperor2",					"palpatine_fa",			"model_default",				"sidious_m",				"",						"red",			"",			1,		14,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_emperor2/misc/taunt3.mp3",	"",		1.0f,	"@MD_CHAR_DESC_EMPEROR_PALPATINE",			 qtrue },
	{ "Galak Fyyar",					"[Jedi Outcast]",						"",				"galak",						"imperial",				"model_galak",					"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/galak/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GALAK_FYYAR",				qfalse },
	{ "Gamorrean Gaurd",				"[Episode VI]",							"",				"gamorrean",					"gamorrean",			"model_default",				"gammobaxe",				"",						"",				"",			2,		12,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/gamorrean/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_GAMORREAN_GUARD",			qfalse },
	{ "Gorc",							"[Dark Forces II]",						"",				"md_gorc",						"Gorc",					"model_default",				"Gorc",						"",						"orange",		"",			3,		8,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/gorc/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_GORC",						qfalse },
	{ "Grand Admiral Thrawn",			"[Rebels]",								"",				"md_thrawn",					"thrawn",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/thrawn/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GRAND_ADMIRAL_THRAWN",		qfalse },
	{ "Grand Moff Tarkin",				"[Episode IV]",							"",				"md_tarkin",					"tarkin",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tarkin/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GRAND_MOFF_TARKIN",			qfalse },
	{ "Greedo",							"[Episode IV]",							"",				"md_greedo",					"greedo",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/greedo/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GREEDO",						qfalse },
	{ "Hazardtrooper",					"[Jedi Academy]",						"",				"hazardtrooperconcussion",		"hazardtrooper",		"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hazardtrooper1/misc/anger3.mp3",		"",		0.5f,	"@MD_CHAR_DESC_HAZARDTROOPER",				qfalse },
	{ "Imperial Royal Guard",			"[Episode VI]",							"",				"md_royalguard",				"royal",				"model_default",				"forcepike",				"",						"",				"",			3,		8,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/royalguard/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_IMPERIAL_ROYAL_GUARD",		qfalse },
	{ "Imperial Shadow Guard",			"[Legends]",							"",				"md_shadowguard",				"royal",				"model_default_b",				"pikesaber",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/royalguard/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_IMPERIAL_SHADOW_GUARD",		 qtrue },
	{ "Royal Combat Guard",				"[Legends]",							"",				"md_royalcombat",				"royalcombatguard",		"model_default",				"pikesaber",				"",						"red",			"",			3,		8,		ERA_EMPIRE,			BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/royalguard/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_ROYAL_COMBAT_GUARD",			 qtrue },
	{ "Imperial Worker",				"[Episode IV]",							"",				"impworker",					"imperial_worker",		"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/worker1/misc/taunt.mp3",				"",		1.0f,	"@MD_CHAR_DESC_IMPERIAL_WORKER",			qfalse },
	{ "Jerec",							"[Dark Forces II]",						"",				"md_jerec",						"jerec",				"model_robed",					"jerec",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jerec/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEREC",						qfalse },
	{ "Jerec",							"[No Cape]",							"",				"md_jerec_valley",				"jerec",				"model_default",				"jerec",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jerec/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEREC",						 qtrue },
	{ "Jerec",							"[DF2 In-Game]",						"",				"md_jerec_classic",				"jerec",				"model_classic",				"jerec",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jerec/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEREC",						 qtrue },
	{ "Jerec",							"[1997 Low Poly]",						"",				"md_jerec_lowpoly",				"jerec_lowpoly",		"model_default",				"jerec",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/jerec/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_JEREC",						 qtrue },
	{ "Maw",							"[Dark Forces II]",						"",				"md_maw_legs",					"maw_intro",			"model_default",				"Maw",						"",						"red",			"",			4,		16,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maw/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_MAW",						qfalse },
	{ "Maw",							"[Dismembered]",						"",				"md_maw",						"Maw",					"model_default",				"Maw",						"",						"red",			"",			4,		16,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/maw/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_MAW",						 qtrue },
	{ "Moff Gideon",					"[The Mandalorian]",					"",				"md_gideon",					"gideon",				"model_default",				"darksaber_tm",				"",						"black",		"",			2,		4,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/finn/gideon/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_MOFF_GIDEON",				qfalse },
	{ "Pic",							"[Dark Forces II]",						"",				"md_pic",						"Pic",					"model_default",				"",							"",						"",				"",			1,		2,		ERA_EMPIRE,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/pic/misc/taunt1.mp3",					"",		0.6f,	"@MD_CHAR_DESC_PIC",						qfalse },
	{ "Reborn",							"[Single Lightsaber]",					"",				"reborn",						"reborn",				"model_default",				"reborn",					"",						"red",			"",			1,		2,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1JO/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_REBORN",						qfalse },
	{ "Reborn",							"[Force User]",							"",				"rebornforceuser",				"reborn",				"model_forceuser",				"reborn",					"",						"red",			"",			2,		4,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1JO/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn",							"[Acrobat]",							"",				"rebornacrobat",				"reborn",				"model_acrobat",				"reborn",					"",						"red",			"",			2,		4,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1JO/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn",							"[Fencer]",								"",				"rebornfencer",					"reborn",				"model_fencer",					"reborn",					"",						"red",			"",			1,		2,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1JO/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn",							"[Boss]",								"",				"rebornboss",					"reborn",				"model_boss",					"shadowtrooper",			"",						"red",			"",			1,		14,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1JO/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn",							"[Dual Lightsabers]",					"",				"reborn_dual",					"reborn_new_f",			"model_red",					"reborn",					"reborn",				"red",			"red",		6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/alora/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn",							"[Lightsaber Staff]",					"",				"reborn_staff",					"reborn_new",			"model_red",					"dual_1",					"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn",							"[Master]",								"",				"rebornmaster",					"reborn_twin",			"model_boss",					"rebornmaster",				"",						"red",			"",			1,		14,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/reborn1/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_REBORN",						 qtrue },
	{ "Reborn Shadowtrooper",			"[Shadowtrooper]",						"",				"shadowtrooper",				"shadowtrooper",		"model_default",				"shadowtrooper",			"",						"red",			"",			1,		14,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/shadowtrooper/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_REBORN",						qfalse },
	{ "Purge Trooper",					"[Jedi Fallen Order]",					"",				"purge_trooper",				"purgetrooper",			"model_staff",					"purge_staff",				"",						"",				"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/purgetrooper/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_PURGE_TROOPER",				qfalse },
	{ "Purge Trooper",					"[Commander]",							"",				"purge_trooper_comm",			"purgetrooper",			"model_default",				"menu_wp_clonerifle",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/purgetrooper/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_PURGE_TROOPER",				 qtrue },
	{ "Purge Trooper",					"[Hammer]",								"",				"purge_trooper_hammer",			"purgetrooper",			"model_hammer",					"purge_hammer",				"",						"",				"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/purgetrooper/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_PURGE_TROOPER",				 qtrue },
	{ "Purge Trooper",					"[Dagger]",								"",				"purge_trooper_dual",			"purgetrooper",			"model_baton",					"purge_dagger",				"purge_dagger",			"",				"",			6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/purgetrooper/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_PURGE_TROOPER",				 qtrue },
	{ "Rax Joris",						"[Jedi Academy]",						"",				"rax",							"rax_joris",			"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rax/misc/taunt.mp3",					"",		1.0f,	"@MD_CHAR_DESC_RAX_JORIS",					qfalse },
	{ "Rockettrooper",					"[Jedi Academy]",						"",				"rockettrooper2officer",		"rockettrooper",		"model_default",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rockettrooper/misc/anger3.mp3",		"",		0.5f,	"@MD_CHAR_DESC_ROCKETTROOPER",				qfalse },
	{ "Saber Training Droid",			"[TFU]",								"",				"sabertraining_droid",			"sabertraining_droid",	"model_default",				"training",					"",						"blue",			"",			2,		4,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_SABER_TRAINING_DROID",		qfalse },
	{ "Force Training Droid",			"[TFU]",								"",				"combattraining_droid",			"sabertraining_droid",	"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_FORCE_TRAINING_DROID",		 qtrue },
	{ "Saboteur",						"[Jedi Academy]",						"",				"saboteur",						"saboteur",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sab1/misc/taunt.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SABOTEUR",					qfalse },
	{ "Sariss",							"[Dark Forces II]",						"",				"md_sariss",					"sariss",				"model_default",				"sariss",					"",						"blue",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sariss/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SARISS",						qfalse },
	{ "Sariss",							"[Cloaked]",							"",				"md_sariss_cape",				"sariss",				"model_cape",					"sariss",					"",						"blue",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sariss/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SARISS",						 qtrue },
	{ "Shin Hati",						"[Ahsoka]",								"",				"md_shin",						"shin",					"model_default",				"shin",						"",						"red",			"",			1,		6,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/shin/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SHIN_HATI",					qfalse },
	{ "Shin Hati",						"[Robed]",								"",				"md_shin_robed",				"shin",					"model_cloak",					"shin",						"",						"red",			"",			1,		6,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/shin/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SHIN_HATI",					 qtrue },
	{ "Shin Hati",						"[Hooded]",								"",				"md_shin_hooded",				"shin",					"model_hood",					"shin",						"",						"red",			"",			1,		6,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/shin/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SHIN_HATI",					 qtrue },
	{ "Starkiller",						"[TFU]",								"",				"md_starkiller",				"stk_training_gear",	"model_default",				"starkiller_m",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/starkiller/misc/taunt.wav",			"",		1.0f,	"@MD_CHAR_DESC_STARKILLER",					qfalse },
	{ "Starkiller",						"[Sith Stalker Armor]",					"",				"md_sithstalker",				"sithstalker",			"model_default",				"sith_stalker",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/sithstalker/misc/taunt.wav",			"",		1.0f,	"@MD_CHAR_DESC_STARKILLER",					 qtrue },
	{ "Lord Starkiller",				"[TFU]",								"",				"md_stk_lord",					"lord_stk",				"model_default",				"sith_stalker",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/lord_stk/misc/taunt.wav",				"",		1.0f,	"@MD_CHAR_DESC_STARKILLER",					 qtrue },
	{ "Lord Starkiller",				"[Tatooine]",							"",				"md_stk_tat",					"lord_stk_tat",			"model_default",				"sith_stalker",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/lord_stk/misc/taunt.wav",				"",		1.0f,	"@MD_CHAR_DESC_STARKILLER",					 qtrue },
	{ "Stormtrooper",					"[Episode IV-VI]",						"",				"stormtrooper",					"stormtrooper",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				qfalse },
	{ "Stormtrooper",					"[Officer]",							"",				"stofficer",					"stormtrooper",			"model_officer",				"menu_wp_flechette",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Scout Trooper",					"[Episode VI]",							"",				"scouttrooper",					"scouttrooper",			"model_default",				"menu_wp_disruptor",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Scout Trooper Commander",		"[Episode VI]",							"",				"scouttrooper_officer",			"scouttrooper",			"model_captain",				"militia_baton",			"",						"",				"",			0,		0,		ERA_EMPIRE,			BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Shock Trooper",					"[Coruscant Guard]",					"",				"stshock",						"stshock",				"model_default",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Shock Trooper Commander",		"[Coruscant Guard]",					"",				"stshock2",						"stshock",				"model_commander",				"menu_wp_flechette",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Stormtrooper",					"[Vader's Fist]",						"",				"501st_st",						"501st_stormie",		"model_stcommando",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Stormtrooper",					"[Officer - Vader's Fist]",				"",				"501st_stofficer",				"501st_stormie",		"model_officer",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Shoretrooper",					"[Rogue One]",							"",				"md_shoretrooper",				"shoretrooper",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Shoretrooper Elite",				"[Rogue One]",							"",				"md_shoretrooper_elite",		"shoretrooper",			"model_elite",					"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Tank Trooper",					"[Rogue One]",							"",				"md_tanktrooper",				"shoretrooper",			"model_tank",					"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Shadow Trooper",					"[Jedi Academy]",						"",				"shadowst",						"shadow_stormtrooper",	"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Imperial Evo Trooper",			"[Jedi Academy]",						"",				"evotrooper",					"evotrooper",			"model_default",				"menu_wp_flechette",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Shadow Evo Trooper",				"[Jedi Academy]",						"",				"evoshadow",					"evotrooper",			"model_shadow",					"menu_wp_concussion",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Swamptrooper",					"[Jedi Academy]",						"",				"swamptrooper",					"swamptrooper",			"model_default",				"menu_wp_flechette",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER",				 qtrue },
	{ "Taron Malicos",					"[Jedi Fallen Order]",					"",				"md_malicos",					"taron_malicos",		"model_default",				"malicos",					"malicos",				"red",			"red",		6,		64,		ERA_EMPIRE,			BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/malicos/misc/anger3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TARON_MALICOS",				qfalse },
	{ "Tavion Axmis",					"[Jedi Academy]",						"",				"tavion",						"tavion",				"model_default",				"tavion",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tavion/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TAVION_AXMIS",				qfalse },
	{ "Tavion Axmis",					"[Cult of Ragnos]",						"",				"tavion_new",					"tavion_new",			"model_default",				"tavion",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tavion/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TAVION_AXMIS",				 qtrue },
	{ "Tavion Axmis",					"[Ragnos' Scepter]",					"",				"tavion_scepter",				"tavion_new",			"model_default",				"tavion",					"",						"red",			"",			6,		64,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tavion/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TAVION_AXMIS",				 qtrue },
	{ "Tavion Axmis",					"[Alternate]",							"",				"tavion2",						"reborn_concept",		"model_tavion",					"tavion",					"",						"red",			"",			5,		32,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/tavion/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_TAVION_AXMIS",				 qtrue },
	{ "The Grand Inquisitor",			"[Rebels]",								"",				"md_inquisitor_rebels",			"GI_rebels",			"model_default",				"inquisitor",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/inquisitor/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_THE_GRAND_INQUISITOR",		qfalse },
	{ "The Grand Inquisitor",			"[Temple Guard]",						"",				"md_templeguard_inquisitor",	"GI_rebels",			"model_jtguard_gi",				"temple_guard_reb",			"",						"yellow",		"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/inquisitor/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_THE_GRAND_INQUISITOR",		 qtrue },
	{ "The Eighth Brother",				"[Rebels]",								"",				"md_8thbrother",				"8thbrother",			"model_default",				"inquisitor4",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/5th_brother/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_THE_EIGHTH_BROTHER",			qfalse },
	{ "The Fifth Brother",				"[Rebels]",								"",				"md_5thbrother",				"5thbrother",			"model_default",				"inquisitor2",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/5th_brother/misc/taunt3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_THE_FIFTH_BROTHER",			qfalse },
	{ "The Ninth Sister",				"[Rebels]",								"",				"md_9thsister",					"9thsister",			"model_default",				"inquisitor5",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_THE_NINTH_SISTER",			qfalse },
	{ "The Second Sister",				"[Jedi Fallen Order]",					"",				"md_2ndsister",					"secondsister",			"model_default",				"saber_trilla",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/secondsister/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_THE_SECOND_SISTER",			qfalse },
	{ "The Seventh Sister",				"[Rebels]",								"",				"md_7thsister",					"7thsister",			"model_default",				"inquisitor3",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/7th_sister/misc/taunt4.mp3",			"",		1.0f,	"@MD_CHAR_DESC_THE_SEVENTH_SISTER",			qfalse },
	{ "The Third Sister",				"[Reva Sevander]",						"",				"md_reva",						"reva",					"model_default",				"saber_trilla",				"",						"red",			"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_THE_THIRD_SISTER",			qfalse },
	{ "Vizam",							"[Episode VI]",							"",				"vizam_tgpoc",					"vizam",				"model_default",				"tusken_chieftain_gaffi",	"",						"",				"",			7,		128,	ERA_EMPIRE,			BOTH_STAND_BLOCKING_ON,				BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/weequay/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_VIZAM",						qfalse },
	{ "Weequay",						"[Gunner]",								"",				"weequay",						"weequay",				"model_sp",						"menu_wp_bowcaster",		"",						"",				"",			0,		0,		ERA_EMPIRE,			TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/weequay/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_WEEQUAY",					qfalse },
	{ "Yun",							"[Dark Forces II]",						"",				"md_yun",						"yun",					"model_default",				"Yun",						"",						"yellow",		"",			2,		4,		ERA_EMPIRE,			BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/yun/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_YUN",						qfalse },
	//{ "Imperial Jumptrooper",	?
	//{ "ATST" ?
	#endif

	// Resistance
	{ "Black Krrsantan",				"[The Mandalorian]",					"",				"md_krrsantan",					"krrsantan",			"model_default",				"menu_wp_bowcaster",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/chewbacca/misc/ffturn.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BLACK_KRRSTANTAN",			qfalse },
	{ "Boba Fett",						"[Worn Armor]",							"",				"boba_fett_mand1",				"bobafett",				"model_mand1",					"menu_wp_boba",				"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boba_fett/misc/anger1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BOBA_FETT",					qfalse },
	{ "Boba Fett",						"[Worn Armor - No Helmet]",				"",				"boba_fett_nohelmet",			"bobafett",				"model_nohelm",					"menu_wp_boba",				"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boba_fett/misc/anger1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BOBA_FETT",					 qtrue },
	{ "Boba Fett",						"[Re-armored]",							"",				"boba_fett_mand2",				"bobafett",				"model_mand2",					"menu_wp_boba",				"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boba_fett/misc/anger1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BOBA_FETT",					 qtrue },
	{ "Boba Fett",						"[Re-armored - No Helmet]",				"",				"boba_fett_nohelmet2",			"bobafett",				"model_nohelm2",				"menu_wp_boba",				"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/boba_fett/misc/anger1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_BOBA_FETT",					 qtrue },
	{ "Cara Dune",						"[The Mandalorian]",					"",				"md_cara_dune",					"caradune",				"model_default",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cara/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_CARA_DUNE",					qfalse },
	{ "Din Djarin",						"[The Mandalorian]",					"",				"md_dindjarin",					"dindjarin",			"model_default",				"darksaber_tm",				"",						"black",		"",			2,		4,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/themando/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DIN_DJARIN",					qfalse },
	{ "Din Djarin",						"[Jetpack]",							"",				"md_dindjarin_s3",				"dindjarin",			"model_jetpack",				"darksaber_tm",				"",						"black",		"",			2,		4,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/themando/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_DIN_DJARIN",					 qtrue },
	{ "Fennec Shand",					"[Helmet]",								"",				"md_fennec_helmet",				"fennec",				"model_helmet",					"menu_wp_disruptor",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_FENNEC_SHAND",				qfalse },
	{ "Fennec Shand",					"[No Helmet]",							"",				"md_fennec",					"fennec",				"model_default",				"menu_wp_disruptor",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_FENNEC_SHAND",				 qtrue },
	{ "Finn",							"[TFA]",								"",				"md_finn",						"finn",					"model_default",				"rey_ep7",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/finn/misc/taunt5.mp3",					"",		1.0f,	"@MD_CHAR_DESC_FINN",						qfalse },
	{ "Greef Karga",					"[The Mandalorian]",					"",				"md_greef",						"greef",				"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/greef/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_GREEF_KARGA",				qfalse },
	{ "Grogu",							"[The Mandalorian]",					"",				"md_grogu",						"grogu",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_RESISTANCE,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		0.5f,	"@MD_CHAR_DESC_GROGU",						qfalse },
	{ "Kuiil",							"[The Mandalorian]",					"",				"md_kuiil",						"kuiil",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_RESISTANCE,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/cara/misc/taunt2.mp3",					"",		0.7f,	"@MD_CHAR_DESC_KUIIL",						qfalse },
	{ "Luke Skywalker",					"[Ach-to - Robed]",						"",				"md_luke_tfa",					"luke_tfa",				"model_cloak_glove",			"luke",						"",						"green",		"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_tlj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER_SEQUEL",		qfalse },
	{ "Luke Skywalker",					"[Ach-to - Hooded]",					"",				"md_luke_tfa_hooded",			"luke_tfa",				"model_hood_glove",				"luke",						"",						"green",		"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_tlj/misc/taunt2.mp3",				"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER_SEQUEL",		 qtrue },
	{ "Luke Skywalker",					"[Crait Illusion]",						"",				"md_luke_crait",				"luke_crait",			"model_default",				"rey_ep7",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/luke_tlj/misc/victory3.mp3",			"",		1.0f,	"@MD_CHAR_DESC_LUKE_SKYWALKER_SEQUEL",		 qtrue },
	{ "New Republic Soldier",			"[The Mandalorian]",					"",				"nrep_sold",					"nrep_sold",			"model_default",				"menu_wp_rebelblaster",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rebel/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_NEW_REPUBLIC_SOLDIER",		qfalse },
	{ "New Republic Security Droid",	"[The Mandalorian]",					"",				"md_new_rep_sec_droid",			"NRSD",					"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_NEW_REPUBLIC_SECURITY_DROID", qfalse },
	{ "Paz Vizsla",						"[The Mandalorian]",					"",				"pazvizsla",					"pazvizsla",			"model_default",				"menu_wp_repeater_kotor",	"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PAZ_VIZSLA",					qfalse },
	{ "Poe Dameron",					"[TFA]",								"",				"md_poe_resistance",			"poe",					"model_resistance",				"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/poe/misc/gloat2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_POE_DAMERON",				qfalse },
	{ "Poe Dameron",					"[Officer]",							"",				"md_poe_officer",				"poe",					"model_officer",				"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/poe/misc/gloat2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_POE_DAMERON",				 qtrue },
	{ "Poe Dameron",					"[Pilot]",								"",				"md_poe",						"poe",					"model_default",				"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/poe/misc/gloat2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_POE_DAMERON",				 qtrue },
	{ "Poe Dameron",					"[Pilot - Helmet]",						"",				"md_poe_helmet",				"poe",					"model_helmet",					"menu_wp_blaster_pistol",	"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST2,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/poe/misc/gloat2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_POE_DAMERON",				 qtrue },
	{ "Resistance Trooper",				"[TFA]",								"",				"md_resistance2",				"resistance",			"model_soldier",				"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/resistance/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_RESISTANCE_TROOPER",			qfalse },
	{ "Resistance Trooper",				"[TFA]",								"",				"md_resistance",				"resistance",			"model_default",				"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/resistance/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_RESISTANCE_TROOPER",			 qtrue },
	{ "Resistance Trooper",				"[TFA]",								"",				"md_resistance_elite",			"resistance",			"model_elite",					"menu_wp_rebelrifle",		"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/resistance/misc/taunt2.mp3",			"",		1.0f,	"@MD_CHAR_DESC_RESISTANCE_TROOPER",			 qtrue },
	{ "Rey",							"[Jakku]",								"",				"md_rey",						"rey",					"model_default",				"rey_ep7",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rey/misc/taunt2.mp3",					"",		1.0f,	"@MD_CHAR_DESC_REY",						qfalse },
	{ "Rey",							"[Resistance Scout]",					"",				"md_rey_res",					"rey",					"model_resistance",				"rey_ep7",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rey/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_REY",						 qtrue },
	{ "Rey",							"[Grey Jedi Wrap]",						"",				"md_rey_jedi",					"rey",					"model_jedi",					"rey_ep7",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rey/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_REY",						 qtrue },
	{ "Rey",							"[White Jedi Wrap]",					"",				"md_rey_ros",					"rey_skywalker",		"model_default",				"rey_ep9",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rey/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_REY",						 qtrue },
	{ "Rey",							"[White Jedi Wrap - Hooded]",			"",				"md_rey_ros_hooded",			"rey_skywalker",		"model_hood",					"rey_ep9",					"",						"blue",			"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rey/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_REY",						 qtrue },
	{ "Rey Skywalker",					"[ROS]",								"",				"md_rey_skywalker",				"rey_skywalker",		"model_default",				"rey2_ep9",					"",						"yellow",		"",			1,		14,		ERA_RESISTANCE,		BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rey/misc/taunt4.mp3",					"",		1.0f,	"@MD_CHAR_DESC_REY",						 qtrue },
	{ "Rose Tico",						"[TLJ]",								"",				"md_rose_tico",					"rose_tico",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_RESISTANCE,		TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/rose/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_ROSE_TICO",					qfalse },
	{ "The Armorer",					"[The Mandalorian]",					"",				"armorer",						"mando_arm",			"model_default",				"mando_arm",				"mando_arm2",			"",				"",			0,		0,		ERA_RESISTANCE,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_THE_ARMORER",				qfalse },
	{ "The Armorer",					"[Jetpack]",							"",				"armorer_jet",					"mando_arm",			"model_jetpack",				"mando_arm",				"mando_arm2",			"",				"",			0,		0,		ERA_RESISTANCE,		BOTH_MD_CIN_1,						BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_THE_ARMORER",				 qtrue },


	// First Order
	{ "Armitage Hux",					"[TFA]",								"",				"md_hux",						"hux",					"model_default",				"",							"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hux/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_ARMITAGE_HUX",				qfalse },
	{ "Armitage Hux",					"[Coat]",								"",				"md_hux_coat",					"hux",					"model_coat",					"",							"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hux/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_ARMITAGE_HUX",				 qtrue },
	{ "Armitage Hux",					"[Coat & Hat]",							"",				"md_hux_coat_hat",				"hux",					"model_coat_hat",				"",							"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	BOTH_STAND4,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/hux/misc/taunt1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_ARMITAGE_HUX",				 qtrue },
	{ "Captain Phasma",					"[TFA]",								"",				"md_phasma",					"phasma",				"model_default",				"menu_wp_firstorder",		"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/phasma/misc/taunt1.mp3",				"",		1.0f,	"@MD_CHAR_DESC_CAPTAIN_PHASMA",				qfalse },
	{ "Emperor Palpatine",				"[Clone - Reborn]",						"",				"md_emperor_ros",				"palpatine_ros",		"model_default",				"",							"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_ros/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_EMPEROR_PALPATINE_SEQUEL",	qfalse },
	{ "Emperor Palpatine",				"[Clone - Blind]",						"",				"md_emperor_ros_blind",			"palpatine_ros",		"model_blind",					"",							"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/palpatine_ros/misc/taunt1.mp3",		"",		1.0f,	"@MD_CHAR_DESC_EMPEROR_PALPATINE_SEQUEL",	 qtrue },
	{ "Kylo Ren",						"[TFA - No Helmet]",					"",				"md_kylo_tfa",					"kylo_ren",				"model_nomask",					"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					qfalse },
	{ "Kylo Ren",						"[TFA - Helmet]",						"",				"md_kylo_tfa_helmet",			"kylo_ren",				"model_nohood",					"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TFA - Hooded]",						"",				"md_kylo_tfa_hooded",			"kylo_ren",				"model_default",				"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TLJ - No Helmet]",					"",				"md_kylo_tlj",					"kylo_ren",				"model_tlj_nomaskb",			"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TLJ - Helmet]",						"",				"md_kylo_tlj_helmet",			"kylo_ren",				"model_tlj",					"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TLJ - Shirtless]",					"",				"md_kylo_shirtless",			"ben_swolo",			"model_default",				"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TLJ - Robed]",						"",				"md_kylo_tlj_cape",				"kylo_ren",				"model_tlj_nomaska",			"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TROS - Robed]",						"",				"md_kylo_ros_cape",				"kylo_ren",				"model_ros_nomask",				"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TROS - Helmet]",						"",				"md_kylo_ros_helmet",			"kylo_ren",				"model_ros",					"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren/misc/taunt3.mp3",				"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TROS - Hooded]",						"",				"md_kylo_ros_hooded",			"kylo_ren",				"model_ros_hood",				"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[TROS - Redeemed]",					"",				"md_ben_solo",					"ben_solo",				"model_default",				"rey_ep9",					"",						"blue",			"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/kylo_ren_nomask/misc/taunt2.mp3",		"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Kylo Ren",						"[Matt the Radar Technician]",			"",				"md_kylo_matt",					"matradartech",			"model_default",				"kylo_ren",					"",						"unstable_red",	"",			1,		46,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/matradartech/misc/taunt1.mp3",			"",		1.0f,	"@MD_CHAR_DESC_KYLO_REN",					 qtrue },
	{ "Praetorian Guard",				"[Lance]",								"",				"md_pguard",					"praetorian_guard",		"model_default",				"vibro-voluge",				"",						"",				"",			2,		4,		ERA_FIRST_ORDER,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRAETORIAN_GUARD",			qfalse },
	{ "Praetorian Guard",				"[Axe]",								"",				"md_pguard2",					"praetorian_guard",		"model_thirdguard",				"electro-bisento",			"",						"",				"",			3,		8,		ERA_FIRST_ORDER,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRAETORIAN_GUARD",			 qtrue },
	{ "Praetorian Guard",				"[Sword]",								"",				"md_pguard3",					"praetorian_guard",		"model_thirdguard",				"electro-chain_whip",		"",						"",				"",			1,		2,		ERA_FIRST_ORDER,	BOTH_SABERFAST_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRAETORIAN_GUARD",			 qtrue },
	{ "Praetorian Guard",				"[Vibroaxe]",							"",				"md_pguard4",					"praetorian_guard",		"model_secondguard",			"vibro-arbir_blades",		"vibro-arbir_blades",	"",				"",			6,		64,		ERA_FIRST_ORDER,	BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRAETORIAN_GUARD",			 qtrue },
	{ "Praetorian Guard",				"[Vibrostaff]",							"",				"md_pguard5",					"praetorian_guard",		"model_secondguard",			"vibro-arbir_blades_staff",	"",						"",				"",			7,		128,	ERA_FIRST_ORDER,	BOTH_STAND_BLOCKING_ON_STAFF,		BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_PRAETORIAN_GUARD",			 qtrue },
	{ "Ren",							"[Knights of Ren]",						"",				"md_ren",						"ren",					"model_default",				"ren",						"",						"red",			"",			3,		12,		ERA_FIRST_ORDER,	BOTH_SABERSLOW_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"",													"",		1.0f,	"@MD_CHAR_DESC_REN",						qfalse },
	{ "Stormtrooper",					"[First Order]",						"",				"stormie_tfa",					"stormie_tfa",			"model_default",				"menu_wp_firstorder",		"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER_FIRST_ORDER",	qfalse },
	{ "Stormtrooper",					"[Officer - First Order]",				"",				"stormie_tfa_officer",			"stormie_tfa",			"model_pauldron",				"menu_wp_firstorder",		"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_STORMTROOPER_FIRST_ORDER",	 qtrue },
	{ "Riot Stormtrooper",				"[First Order]",						"",				"stormie_tfa_riot",				"stormie_tfa",			"model_default",				"riot_baton",				"riot_shield",			"",				"",			6,		64,		ERA_FIRST_ORDER,	BOTH_SABERDUAL_STANCE_JKA,			BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger1.mp3",					"",		1.0f,	"@MD_CHAR_DESC_RIOT_STORMTROOPER_FIRST_ORDER", qtrue },
	{ "Sith Trooper",					"[Final Order]",						"",				"md_sithtrooper",				"sithtrooper",			"model_default",				"menu_wp_blaster",			"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SITH_TROOPER",				 qtrue },
	{ "Sith Trooper",					"[Commander - Final Order]",			"",				"md_sithtrooper_commander",		"sithtrooper",			"model_officer",				"menu_wp_repeater",			"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	TORSO_WEAPONREST3,					BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/st1/misc/anger3.mp3",					"",		1.0f,	"@MD_CHAR_DESC_SITH_TROOPER",				 qtrue },
	{ "Supreme Leader Snoke",			"[TLJ]",								"",				"md_snoke",						"snoke",				"model_default",				"",							"",						"",				"",			0,		0,		ERA_FIRST_ORDER,	BOTH_STAND1,						BOTH_PAIN1,			BOTH_PAIN2,			"sound/chars/snoke/misc/taunt4.mp3",				"",		1.0f,	"@MD_CHAR_DESC_SUPREME_LEADER_SNOKE",		qfalse },

	{ "",								"",										"",				"",								"",						"",								"",							"",						"",				"",			0,		0,		TOTAL_ERAS,			0,						0,					0,						"",			"",		1.0f,	"",											qfalse },
};
constexpr int NO_OF_MD_MODELS = sizeof(charMD) / sizeof(charMD[0]);

static qhandle_t mdHeadIcons[NO_OF_MD_MODELS];

static int eraIndex[TOTAL_ERAS + 1];
static void MD_BG_GenerateEraIndexes(void) {
	int i = 0;
	int factionMD = 0;
	eraIndex[factionMD] = 0;

	for (i = 0; i < NO_OF_MD_MODELS; i++)
	{ // Search only in our index location, we have been given a class
		if (charMD[i].era != factionMD) {
			factionMD++;
			eraIndex[factionMD] = i;
		}
	}
	//	eraIndex[factionMD + 1] = i; // Set up out of bounds case.
}

static const char* MD_UI_SelectedTeamHead(int index, int* actual) {
	int i, c = 0;
	for (i = eraIndex[uiEra]; i < eraIndex[uiEra + 1]; i++) {
		if (i >= eraIndex[uiEra] && i < eraIndex[uiEra + 1]) {
			if (c == index) {
				*actual = i;
				return charMD[i].model;
			}
			else {
				c++;
			}
		}
	}
	return "";
}

static const char* MD_UI_SelectedTeamHead_SubDivs(int index, int* actual) {
	int i, c = 0;

	for (i = eraIndex[uiEra]; i < eraIndex[uiEra + 1]; i++) {
		if (i >= eraIndex[uiEra] && i < eraIndex[uiEra + 1]) {

			if (!charMD[i].isSubDiv) { // hasSubDiv
				int k;
				for (k = 1; k < NO_OF_MD_MODELS; k++) {
					if (charMD[i + k].isSubDiv)
						c--;
					else
						k = NO_OF_MD_MODELS;
				}
			}

			if (c == index) {
				while (charMD[i].isSubDiv)
					i--;

				*actual = i;
				return charMD[i].model;
			}
			else {
				c++;
			}

		}
	}
	return "";
}
#endif

static int duelMapIndex = 1;

typedef struct {
	const char* duelMapName;
	const char* duelMapDesc;
	const char* duelMapBSP;
	const char* duelMapImage;
} duelMaps_t;

static constexpr duelMaps_t duelMaps[] = {
	// duelMapName							// duelMapDesc							// duelMapBSP					// duelMapImage
	// Episode 1
	{ "@MD_MENU_MAIN_DM_EPISODE_1",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_N_TFF",				"@MD_MENU_MAIN_DM_N_TFF_DESC",			"duel_tradefed",				"levelshots/bg_tradefed"			},
	{ "@MD_MENU_MAIN_DM_N_COT",				"@MD_MENU_MAIN_DM_N_COT_DESC",			"duel_theed",					"levelshots/bg_theed"				},
	{ "@MD_MENU_MAIN_DM_N_OG",				"@MD_MENU_MAIN_DM_N_OG_DESC",			"duel_otoh_gunga",				"levelshots/bg_otoh_gunga"			},
	{ "@MD_MENU_MAIN_DM_T_MES",				"@MD_MENU_MAIN_DM_T_MES_DESC",			"duel_mos_espa",				"levelshots/bg_mos_espa"			},
	{ "@MD_MENU_MAIN_DM_N_TGC",				"@MD_MENU_MAIN_DM_N_TGC_DESC",			"duel_dotf",					"levelshots/bg_dotf"				},
	{ "@MD_MENU_MAIN_DM_N_QGF",				"@MD_MENU_MAIN_DM_N_QGF_DESC",			"duel_quigonfuneral",			"levelshots/bg_quigonfuneral"		},

	// Episode 2
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_2",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_K_CF",				"@MD_MENU_MAIN_DM_K_CF_DESC",			"duel_kamino",					"levelshots/bg_kamino"				},
	{ "@MD_MENU_MAIN_DM_K_LP",				"@MD_MENU_MAIN_DM_K_LP_DESC",			"duel_kamino_lp",				"levelshots/bg_kamino_lp"			},
	{ "@MD_MENU_MAIN_DM_T_TC",				"@MD_MENU_MAIN_DM_T_TC_DESC",			"duel_tusken",					"levelshots/bg_tusken"				},
	{ "@MD_MENU_MAIN_DM_G_PA",				"@MD_MENU_MAIN_DM_G_PA_DESC",			"duel_geonosis_arena",			"levelshots/bg_geonosis_arena"		},
	{ "@MD_MENU_MAIN_DM_G_B",				"@MD_MENU_MAIN_DM_G_B_DESC",			"duel_geonosis_battle",			"levelshots/bg_geonosis_battle"		},
	{ "@MD_MENU_MAIN_DM_G_DSH",				"@MD_MENU_MAIN_DM_G_DSH_DESC",			"duel_geonosis_hangar",			"levelshots/bg_geonosis_hangar"		},

	// Episode 3
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_3",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_C_TIH",				"@MD_MENU_MAIN_DM_C_TIH_DESC",			"duel_invisible_hand",			"levelshots/bg_invisible_hand"		},
	{ "@MD_MENU_MAIN_DM_U_PC",				"@MD_MENU_MAIN_DM_U_PC_DESC",			"duel_utapau",					"levelshots/bg_utapau"				},
	{ "@MD_MENU_MAIN_DM_U_SLP",				"@MD_MENU_MAIN_DM_U_SLP_DESC",			"duel_utapau_lp",				"levelshots/bg_utapau_lp"			},
	{ "@MD_MENU_MAIN_DM_M_B",				"@MD_MENU_MAIN_DM_M_B_DESC",			"duel_mygeeto",					"levelshots/bg_mygeeto"				},
	{ "@MD_MENU_MAIN_DM_C_C",				"@MD_MENU_MAIN_DM_C_C_DESC",			"duel_coruscantcity",			"levelshots/bg_coruscantcity"		},
	{ "@MD_MENU_MAIN_DM_C_PA",				"@MD_MENU_MAIN_DM_C_PA_DESC",			"duel_padme_home",				"levelshots/bg_padme_home"			},
	{ "@MD_MENU_MAIN_DM_C_PO",				"@MD_MENU_MAIN_DM_C_PO_DESC",			"duel_office",					"levelshots/bg_office"				},
	{ "@MD_MENU_MAIN_DM_C_JTO",				"@MD_MENU_MAIN_DM_C_JTO_DESC",			"duel_jt_outside",				"levelshots/bg_jt_outside"			},
	{ "@MD_MENU_MAIN_DM_C_JT",				"@MD_MENU_MAIN_DM_C_JT_DESC",			"duel_jeditemple",				"levelshots/bg_jeditemple"			},
	{ "@MD_MENU_MAIN_DM_JT_TR",				"@MD_MENU_MAIN_DM_JT_TR_DESC",			"duel_jt_training",				"levelshots/bg_jt_training"			},
	{ "@MD_MENU_MAIN_DM_C_SC",				"@MD_MENU_MAIN_DM_C_SC_DESC",			"duel_senate",					"levelshots/bg_senate"				},
	{ "@MD_MENU_MAIN_DM_M_MF",				"@MD_MENU_MAIN_DM_M_MF_DESC",			"duel_mustafar",				"levelshots/bg_mustafar"			},
	{ "@MD_MENU_MAIN_DM_M_MFD",				"@MD_MENU_MAIN_DM_M_MFD_DESC",			"duel_mus_both",				"levelshots/bg_mus_both"			},

	// Clone Wars and Rebels
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_CW_REBELS",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_M_SPD",				"@MD_MENU_MAIN_DM_M_SPD_DESC",			"duel_mandalore",				"levelshots/bg_mandalore"			},
	{ "@MD_MENU_MAIN_DM_M_SPN",				"@MD_MENU_MAIN_DM_M_SPN_DESC",			"duel_mandalore_night",			"levelshots/bg_mandalore_night"		},
	{ "@MD_MENU_MAIN_DM_JT_D",				"@MD_MENU_MAIN_DM_JT_D_DESC",			"duel_jedi_dojo",				"levelshots/bg_jedi_dojo"			},
	{ "@MD_MENU_MAIN_DM_S_WBW",				"@MD_MENU_MAIN_DM_S_WBW_DESC",			"duel_wbw",						"levelshots/bg_wbw"					},

	// The Force Unleashed
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_THE_FORCE_UNLEASHED", "",									"",								""									},
	{ "@MD_MENU_MAIN_DM_RS_TR",				"@MD_MENU_MAIN_DM_RS_TR_DESC",			"duel_rogue_tr",				"levelshots/bg_rogue_tr"			},
	{ "@MD_MENU_MAIN_DM_JT_LP",				"@MD_MENU_MAIN_DM_JT_LP_DESC",			"duel_coruscantlp",				"levelshots/bg_coruscantlp"			},
	{ "@MD_MENU_MAIN_DM_JT_LA",				"@MD_MENU_MAIN_DM_JT_LA_DESC",			"duel_jt_tfu",					"levelshots/bg_jt_library"			},
	{ "@MD_MENU_MAIN_DM_DS_TFU",			"@MD_MENU_MAIN_DM_DS_TFU_DESC",			"duel_deathstar_tfu",			"levelshots/bg_deathstar_tfu"		},
	{ "@MD_MENU_MAIN_DM_SD_TFH",			"@MD_MENU_MAIN_DM_SD_TFH_DESC",			"duel_tfu_tie_hangar",			"levelshots/bg_tfu_tie_hangar"		},
	{ "@MD_MENU_MAIN_DM_K_TC",				"@MD_MENU_MAIN_DM_K_TC_DESC",			"duel_tfu2_kamino",				"levelshots/bg_tfu2_kamino"			},

	// Rogue One
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_ROGUE_ONE",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_J_HC",				"@MD_MENU_MAIN_DM_J_HC_DESC",			"duel_jedhacity",				"levelshots/bg_jedhacity"			},
	{ "@MD_MENU_MAIN_DM_M_VC",				"@MD_MENU_MAIN_DM_M_VC_DESC",			"duel_vad_castle",				"levelshots/bg_vad_castle"			},
	{ "@MD_MENU_MAIN_DM_S_ISC",				"@MD_MENU_MAIN_DM_S_ISC_DESC",			"duel_scarif",					"levelshots/bg_scarif"				},
	{ "@MD_MENU_MAIN_DM_SC_PH",				"@MD_MENU_MAIN_DM_SC_PH_DESC",			"duel_profundity",				"levelshots/bg_profundity"			},

	// Episode 4
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_4",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_T_TS",				"@MD_MENU_MAIN_DM_T_TS_DESC",			"duel_twinsuns",				"levelshots/bg_twinsuns"			},
	{ "@MD_MENU_MAIN_DM_T_LHD",				"@MD_MENU_MAIN_DM_T_LHD_DESC",			"duel_lars_homestead_day",		"levelshots/bg_lars_homestead_day"	},
	{ "@MD_MENU_MAIN_DM_T_LHS",				"@MD_MENU_MAIN_DM_T_LHS_DESC",			"duel_lars_homestead_sunset",	"levelshots/bg_lars_homestead_sunset" },
	{ "@MD_MENU_MAIN_DM_T_LHN",				"@MD_MENU_MAIN_DM_T_LHN_DESC",			"duel_lars_homestead_night",	"levelshots/bg_lars_homestead_night" },
	{ "@MD_MENU_MAIN_DM_T_LHO",				"@MD_MENU_MAIN_DM_T_LHO_DESC",			"duel_luke",					"levelshots/bg_luke"				},
	{ "@MD_MENU_MAIN_DM_T_O",				"@MD_MENU_MAIN_DM_T_O_DESC",			"duel_tatooine_outpost",		"levelshots/bg_tatooine_outpost"	},
	{ "@MD_MENU_MAIN_DM_T_RBR",				"@MD_MENU_MAIN_DM_T_RBR_DESC",			"duel_tantiveiv",				"levelshots/bg_tantiveiv"			},
	{ "@MD_MENU_MAIN_DM_TE_MC",				"@MD_MENU_MAIN_DM_TE_MC_DESC",			"duel_executor",				"levelshots/bg_executor"			},
	{ "@MD_MENU_MAIN_DM_DS_SS",				"@MD_MENU_MAIN_DM_DS_SS_DESC",			"duel_deathstar",				"levelshots/bg_deathstar"			},
	{ "@MD_MENU_MAIN_DM_Y_TGT",				"@MD_MENU_MAIN_DM_Y_TGT_DESC",			"duel_yavin",					"levelshots/bg_yavin"				},
	{ "@MD_MENU_MAIN_DM_Y_CH",				"@MD_MENU_MAIN_DM_Y_CH_DESC",			"duel_yavin_jk2",				"levelshots/bg_yavin_jk2"			},

	// Episode 5
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_5",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_H_EB",				"@MD_MENU_MAIN_DM_H_EB_DESC",			"duel_hoth",					"levelshots/bg_hoth"				},
	{ "@MD_MENU_MAIN_DM_B_CC",				"@MD_MENU_MAIN_DM_B_CC_DESC",			"duel_cloudcity",				"levelshots/bg_cloudcity"			},
	{ "@MD_MENU_MAIN_DM_B_CFC",				"@MD_MENU_MAIN_DM_B_CFC_DESC",			"duel_carbon",					"levelshots/bg_carbon"				},

	// Episode 6
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_6",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_T_JPD",				"@MD_MENU_MAIN_DM_T_JPD_DESC",			"duel_jabba",					"levelshots/bg_jabba"				},
	{ "@MD_MENU_MAIN_DM_T_JPN",				"@MD_MENU_MAIN_DM_T_JPN_DESC",			"duel_jabba_night",				"levelshots/bg_jabba_night"			},
	{ "@MD_MENU_MAIN_DM_T_GPOC",			"@MD_MENU_MAIN_DM_T_GPOC_DESC",			"duel_sarlacc",					"levelshots/bg_sarlacc"				},
	{ "@MD_MENU_MAIN_DM_DS_ETR",			"@MD_MENU_MAIN_DM_DS_ETR_DESC",			"duel_emperor",					"levelshots/bg_emperor"				},

	// Episode 7
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_7",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_J_T",				"@MD_MENU_MAIN_DM_J_T_DESC",			"duel_jakku",					"levelshots/bg_jakku"				},
	{ "@MD_MENU_MAIN_DM_E_BF",				"@MD_MENU_MAIN_DM_E_BF_DESC",			"duel_eravana",					"levelshots/bg_eravana"				},
	{ "@MD_MENU_MAIN_DM_SB_C",				"@MD_MENU_MAIN_DM_SB_C_DESC",			"duel_star_base",				"levelshots/bg_star_base"			},
	{ "@MD_MENU_MAIN_DM_SB_F",				"@MD_MENU_MAIN_DM_SB_F_DESC",			"duel_star_basef",				"levelshots/bg_star_basef"			},

	// Episode 8
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_8",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_TS_STR",			"@MD_MENU_MAIN_DM_TS_STR_DESC",			"duel_supremacy",				"levelshots/bg_supremacy"			},
	{ "@MD_MENU_MAIN_DM_C_RO",				"@MD_MENU_MAIN_DM_C_RO_DESC",			"duel_crait",					"levelshots/bg_crait"				},

	// Episode 9
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EPISODE_9",			"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_E_SC",				"@MD_MENU_MAIN_DM_E_SC_DESC",			"duel_exegol",					"levelshots/bg_exegol"				},

	// KOTOR
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_KOTOR",				"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_DA_EN",				"@MD_MENU_MAIN_DM_DA_EN_DESC",			"duel_dantooine_enclave",		"levelshots/bg_dantooine_enclave"	},
	{ "@MD_MENU_MAIN_DM_K_SA",				"@MD_MENU_MAIN_DM_K_SA_DESC",			"duel_korriban_sith_academy",	"levelshots/bg_korriban_sith_academy" },
	{ "@MD_MENU_MAIN_DM_L_B",				"@MD_MENU_MAIN_DM_L_B_DESC",			"duel_leviathan_bridge",		"levelshots/bg_leviathan_bridge"	},
	{ "@MD_MENU_MAIN_DM_M_TA",				"@MD_MENU_MAIN_DM_M_TA_DESC",			"duel_malachor_trayus_academy",	"levelshots/bg_malachor_trayus_academy" },
	{ "@MD_MENU_MAIN_DM_M_TC",				"@MD_MENU_MAIN_DM_M_TC_DESC",			"duel_malachor_trayus_core",	"levelshots/bg_malachor_trayus_core" },
	{ "@MD_MENU_MAIN_DM_I_OP",				"@MD_MENU_MAIN_DM_I_OP_DESC",			"duel_onderon_palace",			"levelshots/bg_onderon_palace"		},
	{ "@MD_MENU_MAIN_DM_P_OD",				"@MD_MENU_MAIN_DM_P_OD_DESC",			"duel_peragus_observation_deck","levelshots/bg_peragus_observation_deck" },
	{ "@MD_MENU_MAIN_DM_R_OD",				"@MD_MENU_MAIN_DM_R_OD_DESC",			"duel_ravager",					"levelshots/bg_ravager"				},
	{ "@MD_MENU_MAIN_DM_SF_OD",				"@MD_MENU_MAIN_DM_SF_OD_DESC",			"duel_starforge",				"levelshots/bg_starforge"			},
	{ "@MD_MENU_MAIN_DM_T_H",				"@MD_MENU_MAIN_DM_T_H_DESC",			"duel_taris_horizons",			"levelshots/bg_taris_horizons"		},
	{ "@MD_MENU_MAIN_DM_T_CS",				"@MD_MENU_MAIN_DM_T_CS_DESC",			"duel_telos_citadel_station",	"levelshots/bg_telos_citadel_station" },
	{ "@MD_MENU_MAIN_DM_T_PA",				"@MD_MENU_MAIN_DM_T_PA_DESC",			"duel_telos_polar_academy",		"levelshots/bg_telos_polar_academy" },

	// Extra
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_EXTRA",				"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_C_CA",				"@MD_MENU_MAIN_DM_C_CA_DESC",			"duel_calodan",					"levelshots/bg_calodan"				},
	{ "@MD_MENU_MAIN_DM_K_F",				"@MD_MENU_MAIN_DM_K_F_DESC",			"duel_khofar",					"levelshots/bg_khofar"				},
	{ "@MD_MENU_MAIN_DM_YV_DA",				"@MD_MENU_MAIN_DM_YV_DA_DESC",			"duel_vong",					"levelshots/bg_vong"				},

	// Dark Force 2
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_DARK_FORCES_2",		"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_DF2_SC",			"@MD_MENU_MAIN_DM_DF2_SC_DESC",			"duel_sithcatacombs",			"levelshots/bg_sithcatacombs"		},
	{ "@MD_MENU_MAIN_DM_DF2_TV",			"@MD_MENU_MAIN_DM_DF2_TV_DESC",			"duel_vengeance",				"levelshots/bg_vengeance"			},
	{ "@MD_MENU_MAIN_DM_DF2_DP",			"@MD_MENU_MAIN_DM_DF2_DP_DESC",			"duel_dark_palace",				"levelshots/bg_dark_palace"			},
	{ "@MD_MENU_MAIN_DM_DF2_SS",			"@MD_MENU_MAIN_DM_DF2_SS_DESC",			"duel_sulon_star",				"levelshots/bg_sulon_star"			},
	{ "@MD_MENU_MAIN_DM_DF2_JB",			"@MD_MENU_MAIN_DM_DF2_JB_DESC",			"duel_jedi_battleground",		"levelshots/bg_jedi_battleground"	},
	{ "@MD_MENU_MAIN_DM_DF2_R",				"@MD_MENU_MAIN_DM_DF2_R_DESC",			"duel_tower",					"levelshots/bg_tower"				},
	{ "@MD_MENU_MAIN_DM_R_VOTJ",			"@MD_MENU_MAIN_DM_R_VOTJ_DESC",			"duel_votj",					"levelshots/bg_votj"				},

	// Jedi Outcast
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_JEDI_OUTCAST",		"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_JKO_D_BA",			"@MD_MENU_MAIN_DM_JKO_D_BA_DESC",		"duel_bay",						"levelshots/bg_duel_bay"			},
	{ "@MD_MENU_MAIN_DM_JKO_D_BE",			"@MD_MENU_MAIN_DM_JKO_D_BE_DESC",		"duel_bespin",					"levelshots/bg_duel_bespin"			},
	{ "@MD_MENU_MAIN_DM_JKO_D_HA",			"@MD_MENU_MAIN_DM_JKO_D_HA_DESC",		"duel_hangar",					"levelshots/bg_duel_hangar"			},
	{ "@MD_MENU_MAIN_DM_JKO_D_PI",			"@MD_MENU_MAIN_DM_JKO_D_PI_DESC",		"duel_pit",						"levelshots/bg_duel_pit"			},
	{ "@MD_MENU_MAIN_DM_JKO_F_BE",			"@MD_MENU_MAIN_DM_JKO_F_BE_DESC",		"ffa_bespin",					"levelshots/bg_ffa_bespin"			},
	{ "@MD_MENU_MAIN_DM_JKO_F_DE",			"@MD_MENU_MAIN_DM_JKO_F_DE_DESC",		"ffa_deathstar",				"levelshots/bg_ffa_deathstar"		},
	{ "@MD_MENU_MAIN_DM_JKO_F_IM",			"@MD_MENU_MAIN_DM_JKO_F_IM_DESC",		"ffa_imperial",					"levelshots/bg_ffa_imperial"		},
	{ "@MD_MENU_MAIN_DM_JKO_F_NH",			"@MD_MENU_MAIN_DM_JKO_F_NH_DESC",		"ffa_ns_hideout",				"levelshots/bg_ffa_ns_hideout"		},
	{ "@MD_MENU_MAIN_DM_JKO_F_NS",			"@MD_MENU_MAIN_DM_JKO_F_NS_DESC",		"ffa_ns_streets",				"levelshots/bg_ffa_ns_streets"		},
	{ "@MD_MENU_MAIN_DM_JKO_F_RA",			"@MD_MENU_MAIN_DM_JKO_F_RA_DESC",		"ffa_raven",					"levelshots/bg_ffa_raven"			},
	{ "@MD_MENU_MAIN_DM_JKO_F_YA",			"@MD_MENU_MAIN_DM_JKO_F_YA_DESC",		"ffa_yavin",					"levelshots/bg_ffa_yavin"			},
	{ "@MD_MENU_MAIN_DM_JKO_C_IM",			"@MD_MENU_MAIN_DM_JKO_C_IM_DESC",		"ctf_imperial",					"levelshots/bg_ctf_imperial"		},
	{ "@MD_MENU_MAIN_DM_JKO_C_YA",			"@MD_MENU_MAIN_DM_JKO_C_YA_DESC",		"ctf_yavin",					"levelshots/bg_ctf_yavin"			},

	// Jedi Academy
	{ "",									"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_JEDI_ACADEMY",		"",										"",								""									},
	{ "@MD_MENU_MAIN_DM_K_TOR",				"@MD_MENU_MAIN_DM_K_TOR_DESC",			"duel_kor",						"levelshots/bg_kor"					},
	{ "@MD_MENU_MAIN_DM_D1_BC",				"@MD_MENU_MAIN_DM_D1_BC_DESC",			"mp/duel1",						"levelshots/mp/duel1"				},
	{ "@MD_MENU_MAIN_DM_D3_IS",				"@MD_MENU_MAIN_DM_D3_IS_DESC",			"mp/duel3",						"levelshots/mp/duel3"				},
	{ "@MD_MENU_MAIN_DM_D4_ICR",			"@MD_MENU_MAIN_DM_D4_ICR_DESC",			"mp/duel4",						"levelshots/mp/duel4"				},
	{ "@MD_MENU_MAIN_DM_D5_TL",				"@MD_MENU_MAIN_DM_D5_TL_DESC",			"mp/duel5",						"levelshots/mp/duel5"				},
	{ "@MD_MENU_MAIN_DM_D6_YTG",			"@MD_MENU_MAIN_DM_D6_YTG_DESC",			"mp/duel6",						"levelshots/mp/duel6"				},
	{ "@MD_MENU_MAIN_DM_D7_RP",				"@MD_MENU_MAIN_DM_D7_RP_DESC",			"mp/duel7",						"levelshots/mp/duel7"				},
	{ "@MD_MENU_MAIN_DM_D8_AC",				"@MD_MENU_MAIN_DM_D8_AC_DESC",			"mp/duel8",						"levelshots/mp/duel8"				},
	{ "@MD_MENU_MAIN_DM_D10_VFP",			"@MD_MENU_MAIN_DM_D10_VFP_DESC",		"mp/duel10",					"levelshots/mp/duel10"				},
	{ "@MD_MENU_MAIN_DM_CTF1_IDZ",			"@MD_MENU_MAIN_DM_CTF1_IDZ_DESC",		"mp/ctf1",						"levelshots/mp/ctf1"				},
	{ "@MD_MENU_MAIN_DM_CF2_HW",			"@MD_MENU_MAIN_DM_CF2_HW_DESC",			"mp/ctf2",						"levelshots/mp/ctf2"				},
	{ "@MD_MENU_MAIN_DM_CF3_YH",			"@MD_MENU_MAIN_DM_CF3_YH_DESC",			"mp/ctf3",						"levelshots/mp/ctf3"				},
	{ "@MD_MENU_MAIN_DM_CF4_CS",			"@MD_MENU_MAIN_DM_CF4_CS_DESC",			"mp/ctf4",						"levelshots/mp/ctf4"				},
	{ "@MD_MENU_MAIN_DM_CF5_F",				"@MD_MENU_MAIN_DM_CF5_F_DESC",			"mp/ctf5",						"levelshots/mp/ctf5"				},
	{ "@MD_MENU_MAIN_DM_FFA1_VS",			"@MD_MENU_MAIN_DM_FFA1_VS_DESC",		"mp/ffa1",						"levelshots/mp/ffa1"				},
	{ "@MD_MENU_MAIN_DM_FFA2_K",			"@MD_MENU_MAIN_DM_FFA2_K_DESC",			"mp/ffa2",						"levelshots/mp/ffa2"				},
	{ "@MD_MENU_MAIN_DM_FFA3_T",			"@MD_MENU_MAIN_DM_FFA3_T_DESC",			"mp/ffa3",						"levelshots/mp/ffa3"				},
	{ "@MD_MENU_MAIN_DM_FFA4_RS",			"@MD_MENU_MAIN_DM_FFA4_RS_DESC",		"mp/ffa4",						"levelshots/mp/ffa4"				},
	{ "@MD_MENU_MAIN_DM_FFA5_T",			"@MD_MENU_MAIN_DM_FFA5_T_DESC",			"mp/ffa5",						"levelshots/mp/ffa5"				},
	{ "@MD_MENU_MAIN_DM_SIEGE_DR",			"@MD_MENU_MAIN_DM_SIEGE_DR_DESC",		"mp/siege_desert",				"levelshots/mp/siege_desert"		},
	{ "@MD_MENU_MAIN_DM_SIEGE_K",			"@MD_MENU_MAIN_DM_SIEGE_K_DESC",		"mp/siege_korriban",			"levelshots/mp/siege_korriban"		},

	{ "",									"",										"",								""									},
};
constexpr int NO_OF_DUEL_MAPS = sizeof(duelMaps) / sizeof(duelMaps[0]);

static int saberHiltIndex = 1;

typedef struct {
	const char* singleSaberHiltName;
	const char* singleSaberHilt;
} singleSaberHilts_t;

static constexpr singleSaberHilts_t singleSaberHilts[] = {
	// singleSaberHiltName					// singleSaberHilt
	{ "@MENUS_SINGLE_HILT1",				"single_1"				},
	{ "@MENUS_SINGLE_HILT2",				"single_2"				},
	{ "@MENUS_SINGLE_HILT3",				"single_3"				},
	{ "@MENUS_SINGLE_HILT4",				"single_4"				},
	{ "@MENUS_SINGLE_HILT5",				"single_5"				},
	{ "@MENUS_SINGLE_HILT6",				"single_6"				},
	{ "@MENUS_SINGLE_HILT7",				"single_7"				},
	{ "@MENUS_SINGLE_HILT8",				"single_8"				},
	{ "@MENUS_SINGLE_HILT9",				"single_9"				},
	{ "@MD_MENU_SABER_AAYLA",				"aayla"					},
	{ "@MD_MENU_SABER_AHSOKA",				"ahsoka"				},
	{ "@MD_MENU_SABER_AHSOKA_SHORT",		"ahsoka_short"			},
	{ "@MD_MENU_SABER_AHSOKA_REBELS",		"ahsoka_reb1"			},
	{ "@MD_MENU_SABER_AHSOKA_REBELS_SHORT", "ahsoka_reb2"			},
	{ "@MD_MENU_SABER_ANAKIN_EP2",			"anakin_ep2"			},
	{ "@MD_MENU_SABER_ANAKIN_EP3",			"anakin_ep3"			},
	{ "@MD_MENU_SABER_VENTRESS_CW",			"ventress_cw"			},
	{ "@MD_MENU_SABER_VENTRESS_TCW",		"ventress_tcw"			},
	{ "@MD_MENU_SABER_VENTRESS_TCW",		"ventress2_tcw"			},
	{ "@MD_MENU_SABER_ASHARAD",				"asharad"				},
	{ "@MD_MENU_SABER_ASHARAD_ALT",			"asharad2"				},
	{ "@MD_MENU_SABER_BARRISS",				"barriss"				},
	{ "@MD_MENU_SABER_BASTILA",				"bastila_single"		},
	{ "@MD_MENU_SABER_BOC",					"boc"					},
	{ "@MD_MENU_SABER_CAL_KESTIS",			"cal"					},
	{ "@MD_MENU_SABER_CAL_KESTIS",			"cal_js"				},
	{ "@MD_MENU_SABER_CAL_KESTIS",			"cal_cg_js"				},
	{ "@MD_MENU_SABER_COLEMAN",				"coleman"				},
	{ "@MD_MENU_SABER_COUNT_DOOKU",			"dooku2"				},
	{ "@MD_MENU_SABER_COUNT_DOOKU_TFU",		"dooku_tfu"				},
	{ "@MD_MENU_SABER_DAGAN",				"dagan"					},
	{ "@MD_MENU_SABER_DARKSABER_TCW",		"darksaber_tcw"			},
	{ "@MD_MENU_SABER_DARKSABER_REBELS",	"darksaber_reb"			},
	{ "@MD_MENU_SABER_DARKSABER_TM",		"darksaber_tm"			},
	{ "@MD_MENU_SABER_D_DESOLOUS",			"desolous"				},
	{ "@MD_MENU_SABER_D_MAUL",				"single_maul"			},
	{ "@MD_MENU_SABER_BANE",				"bane"					},
	{ "@MD_MENU_SABER_MALGUS",				"malgus"				},
	{ "@MD_MENU_SABER_MALAK",				"malak"					},
	{ "@MD_MENU_SABER_NIHILUS",				"nihilus"				},
	{ "@MD_MENU_SABER_D_SIDIOUS",			"sidious"				},
	{ "@MD_MENU_SABER_D_SIDIOUS_ALT",		"sidious2"				},
	{ "@MD_MENU_SABER_D_VADER_EP3",			"vader_ro"				},
	{ "@MD_MENU_SABER_D_VADER_EP4",			"vader_ep4"				},
	{ "@MD_MENU_SABER_D_VADER_EP5",			"vader"					},
	{ "@MD_MENU_SABER_D_VADER_EP6",			"vader_ep6"				},
	{ "@MD_MENU_SABER_DEPA",				"depa"					},
	{ "@MD_MENU_SABER_EVEN",				"even_piell"			},
	{ "@MD_MENU_SABER_EZRA",				"ezra"					},
	{ "@MD_MENU_SABER_EZRA_S3",				"ezra2"					},
	{ "@MD_MENU_SABER_GALEN_MAREK",			"galenmarek"			},
	{ "@MD_MENU_SABER_GALEN_MAREK_ALT",		"galenmarek_alt"		},
	{ "@MD_MENU_SABER_GALEN_MAREK_CJR",		"galenmarek_cjr"		},
	{ "@MD_MENU_SABER_GORC",				"gorc"					},
	{ "@MD_MENU_SABER_GRANDINQUISITOR",		"inquisitor_single"		},
	{ "@MD_MENU_SABER_GRANDINQUISITOR_OWK", "inquisitor_owk_single" },
	{ "@MD_MENU_SABER_FIFTHBROTHER",		"inquisitor2_single"	},
	{ "@MD_MENU_SABER_SEVENTHSISTER",		"inquisitor3_single"	},
	{ "@MD_MENU_SABER_JEREC",				"jerec"					},
	{ "@MD_MENU_SABER_JOLEE",				"jolee"					},
	{ "@MD_MENU_SABER_KANAN",				"kanan"					},
	{ "@MD_MENU_SABER_KYLE",				"kyle"					},
	{ "@MD_MENU_SABER_KI_ADI",				"mundi"					},
	{ "@MD_MENU_SABER_KYLO",				"kylo_ren"				},
	{ "@MD_MENU_SABER_LEIA_EP9",			"leia_ep9"				},
	{ "@MD_MENU_SABER_LUKE_EP4",			"luke_ep4"				},
	{ "@MD_MENU_SABER_LUKE_EP5",			"luke_ep5"				},
	{ "@MD_MENU_SABER_LUKE_EP6",			"luke_ep6"				},
	{ "@MD_MENU_SABER_LUMINARA",			"luminara"				},
	{ "@MD_MENU_SABER_MACE_EP1",			"windu_ep1"				},
	{ "@MD_MENU_SABER_MACE",				"windu"					},
	{ "@MD_MENU_SABER_MARA",				"mara"					},
	{ "@MD_MENU_SABER_MARA_MOTS",			"mara_mots"				},
	{ "@MD_MENU_SABER_MAW",					"maw"					},
	{ "@MD_MENU_SABER_OBI_EP1",				"obiwan_ep1"			},
	{ "@MD_MENU_SABER_OBI_EP3",				"obiwan_ep3"			},
	{ "@MD_MENU_SABER_OBI_OWK",				"obiwan_owk"			},
	{ "@MD_MENU_SABER_OBI_EP4",				"obiwan_ep4"			},
	{ "@MD_MENU_SABER_OPPO",				"oppo"					},
	{ "@MD_MENU_SABER_PIC",					"pic"					},
	{ "@MD_MENU_SABER_PLO",					"plo_koon"				},
	{ "@MD_MENU_SABER_QUI",					"quigon"				},
	{ "@MD_MENU_SABER_QUINLAN",				"quinlan"				},
	{ "@MD_MENU_SABER_RAHN",				"rahn"					},
	{ "@MD_MENU_SABER_KOTA",				"kota"					},
	{ "@MD_MENU_SABER_REY_EP7",				"rey_ep7"				},
	{ "@MD_MENU_SABER_REY_EP8",				"rey_ep8"				},
	{ "@MD_MENU_SABER_REY_EP9",				"rey_ep9"				},
	{ "@MD_MENU_SABER_REY2_EP9",			"rey2_ep9"				},
	{ "@MD_MENU_SABER_REY_DARKFOLD_EP9",	"rey_darkfold_ep9"		},
	{ "@MD_MENU_SABER_REVAN",				"revan"					},
	{ "@MD_MENU_SABER_RIG_NEMA",			"rig"					},
	{ "@MD_MENU_SABER_SAESSEE",				"saesee"				},
	{ "@MD_MENU_SABER_SARISS",				"sariss"				},
	{ "@MD_MENU_SABER_SHAAK",				"shaak"					},
	{ "@MD_MENU_SABER_STALKER",				"sith_stalker"			},
	{ "@MD_MENU_SABER_STALKER2",			"sith_stalker2"			},
	{ "@MD_MENU_SABER_STARKILLER",			"starkiller"			},
	{ "@MD_MENU_SABER_TARON",				"malicos"				},
	{ "@MD_MENU_SABER_TAVION",				"tavion"				},
	{ "@MD_MENU_SABER_YARAEL",				"yarael"				},
	{ "@MD_MENU_SABER_YODA",				"yoda"					},
	{ "@MD_MENU_SABER_YUN",					"yun"					},
	
	{ "",									""						},
};
constexpr int NO_OF_SINGLE_SABER_HILTS = sizeof(singleSaberHilts) / sizeof(singleSaberHilts[0]);

typedef struct {
	const char* staffSaberHiltName;
	const char* staffSaberHilt;
} staffSaberHilts_t;

static constexpr staffSaberHilts_t staffSaberHilts[] = {
	// staffSaberHiltName					// staffSaberHilt
	{ "@MENUS_STAFF_HILT1",					"dual_1"				},
	{ "@MENUS_STAFF_HILT2",					"dual_2"				},
	{ "@MENUS_STAFF_HILT3",					"dual_3"				},
	{ "@MENUS_STAFF_HILT4",					"dual_4"				},
	{ "@MENUS_STAFF_HILT5",					"dual_5"				},
	{ "@MD_MENU_SABER_VENTRESS_CW",			"ventress_cw_dual"		},
	{ "@MD_MENU_SABER_VENTRESS_TCW",		"ventress_tcw_dual"		},
	{ "@MD_MENU_SABER_CAL_KESTIS",			"cal_staff"				},
	{ "@MD_MENU_SABER_CAL_KESTIS",			"cal_staff_js"			},
	{ "@MD_MENU_SABER_DAGAN_STAFF",			"dagan_staff"			},
	{ "@MD_MENU_SABER_D_MAUL_STAFF",		"dual_maul"				},
	{ "@MD_MENU_SABER_D_MAUL_TCW",			"maul_tcw"				},
	{ "@MD_MENU_SABER_D_MAUL_REBELS",		"maul_rebels"			},
	{ "@MD_MENU_SABER_D_PHOBOS",			"phobos"				},
	{ "@MD_MENU_SABER_GRANDINQUISITOR",		"inquisitor"			},
	{ "@MD_MENU_SABER_GRANDINQUISITOR_OWK", "inquisitor_owk"		},
	{ "@MD_MENU_SABER_FIFTHBROTHER",		"inquisitor2"			},
	{ "@MD_MENU_SABER_SEVENTHSISTER",		"inquisitor3"			},
	{ "@MD_MENU_SABER_EIGHTHBROTHER",		"inquisitor4"			},
	{ "@MD_MENU_SABER_REY_DARKSTAFF_EP9",	"rey_darkstaff_ep9"		},
	{ "@MD_MENU_SABER_SAVAGE",				"dual_opress"			},
	{ "@MD_MENU_SABER_JT_GUARD",			"temple_guard"			},
	{ "@MD_MENU_SABER_BASTILA",				"bastila_staff"			},

	{ "",									""						},
};
constexpr int NO_OF_STAFF_SABER_HILTS = sizeof(staffSaberHilts) / sizeof(staffSaberHilts[0]);

extern qboolean ItemParse_model_g2anim_go(itemDef_t* item, const char* animName);
extern qboolean ItemParse_asset_model_go(itemDef_t* item, const char* name);
extern qboolean ItemParse_model_g2skin_go(itemDef_t* item, const char* skinName);
extern qboolean UI_SaberModelForSaber(const char* saber_name, char* saber_model);
extern qboolean UI_SaberSkinForSaber(const char* saber_name, char* saber_skin);
extern void UI_SaberAttachToChar(itemDef_t* item);
extern void FS_Restart();

extern qboolean PC_Script_Parse(const char** out);

constexpr auto LISTBUFSIZE = 10240;

static struct
{
	char listBuf[LISTBUFSIZE]; //	The list of file names read in

	// For scrolling through file names
	int currentLine; //	Index to currentSaveFileComments[] currently highlighted
	int saveFileCnt; //	Number of save files read in

	int awaitingSave; //	Flag to see if user wants to overwrite a game.

	char* savegameMap;
	int savegameFromFlag;
} s_savegame;

constexpr auto MAX_SAVELOADFILES = 100;
constexpr auto MAX_SAVELOADNAME = 32;

#ifdef JK2_MODE
byte screenShotBuf[SG_SCR_WIDTH * SG_SCR_HEIGHT * 4];
#endif

using savedata_t = struct
{
	char* currentSaveFileName; // file name of savegame
	char currentSaveFileComments[iSG_COMMENT_SIZE]; // file comment
	char currentSaveFileDateTimeString[iSG_COMMENT_SIZE]; // file time and date
	time_t currentSaveFileDateTime;
	char currentSaveFileMap[MAX_TOKEN_CHARS]; // map save game is from
};

static savedata_t s_savedata[MAX_SAVELOADFILES];
void UI_SetActiveMenu(const char* menuname, const char* menuID);
void ReadSaveDirectory();
void Item_RunScript(itemDef_t* item, const char* s);
qboolean Item_SetFocus(itemDef_t* item, float x, float y);

qboolean Asset_Parse(char** buffer);
menuDef_t* Menus_FindByName(const char* p);
void Menus_HideItems(const char* menuName);
int Text_Height(const char* text, float scale, int iFontIndex);
int Text_Width(const char* text, float scale, int iFontIndex);
void _UI_DrawTopBottom(float x, float y, float w, float h, float size);
void _UI_DrawSides(float x, float y, float w, float h, float size);
void UI_CheckVid1Data(const char* menuTo, const char* warningMenuName);
void UI_GetVideoSetup();
void UI_UpdateVideoSetup();
static void UI_UpdateCharacterCvars();
static void UI_GetCharacterCvars();
static void UI_UpdateSaberCvars();
static void UI_GetSaberCvars();
static void UI_ResetSaberCvars();
static void UI_InitAllocForcePowers(const char* forceName);
static void UI_AffectForcePowerLevel(const char* forceName);
static void UI_ShowForceLevelDesc(const char* forceName);
static void UI_ResetForceLevels();
static void UI_ClearWeapons();
static void UI_clearsabers();
static void UI_GiveAmmo(int ammoIndex, int ammoAmount, const char* soundfile);
static void UI_GiveWeapon(int weaponIndex);
static void UI_EquipWeapon(int weaponIndex);
static void UI_LoadMissionSelectMenu(const char* cvarName);
static void UI_AddWeaponSelection(int weaponIndex, int ammoIndex, int ammoAmount, const char* iconItemName,
	const char* litIconItemName, const char* hexBackground, const char* soundfile);
static void UI_AddPistolSelection(int weaponIndex, int ammoIndex, int ammoAmount, const char* iconItemName,
	const char* litIconItemName, const char* hexBackground, const char* soundfile);
static void UI_AddThrowWeaponSelection(int weaponIndex, int ammoIndex, int ammoAmount, const char* iconItemName,
	const char* litIconItemName, const char* hexBackground, const char* soundfile);
static void UI_RemoveWeaponSelection(int weapon_selection_index);
static void UI_Removepistolselection();
static void UI_RemoveThrowWeaponSelection();
static void UI_HighLightWeaponSelection(int selectionslot);
static void UI_Highlightpistolselection();
static void UI_NormalWeaponSelection(int selectionslot);
static void UI_Normalpistolselection();
static void UI_NormalThrowSelection();
static void UI_HighLightThrowSelection();
static void UI_ClearInventory();
static void UI_GiveInventory(int itemIndex, int amount);
static void UI_ForcePowerWeaponsButton(qboolean activeFlag);
static void UI_ViewWeaponWheel();
static void UI_ViewForceWheel();
static void UI_UpdateCharacterSkin();
#ifdef NEW_FEEDER_V7
static void UI_UpdateCharacter(qboolean changedModel, qboolean newLoad);
#else
static void UI_UpdateCharacter(qboolean changedModel);
#endif
static void UI_UpdateSaberType();
static void UI_UpdateSaberHilt(qboolean secondSaber);
//static void		UI_UpdateSaberColor( qboolean secondSaber );
static void UI_InitWeaponSelect();
static void UI_WeaponHelpActive();

#ifndef JK2_MODE
static void UI_UpdateFightingStyle();
static void UI_UpdateFightingStyleChoices();
static void UI_CalcForceStatus();
#endif // !JK2_MODE

static void UI_DecrementForcePowerLevel();
static void UI_DecrementCurrentForcePower();
static void UI_ShutdownForceHelp();
static void UI_ForceHelpActive();

#ifndef JK2_MODE
static void UI_DemoSetForceLevels();
#endif // !JK2_MODE

static void UI_RecordForceLevels();
static void UI_RecordWeapons();
static void UI_ResetCharacterListBoxes();

void UI_LoadMenus(const char* menuFile, qboolean reset);
static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw,
	int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader,
	int textStyle, int iFontIndex);
static qboolean UI_OwnerDrawVisible(int flags);
int UI_OwnerDrawWidth(int ownerDraw, float scale);
static void UI_Update(const char* name);
void UI_UpdateCvars();
void UI_ResetDefaults();
void UI_AdjustSaveGameListBox(int currentLine);

void Menus_CloseByName(const char* p);

// Movedata Sounds
enum
{
	MDS_NONE = 0,
	MDS_FORCE_JUMP,
	MDS_ROLL,
	MDS_SABER,
	MDS_MOVE_SOUNDS_MAX
};

enum
{
	MD_ACROBATICS = 0,
	MD_SINGLE_FAST,
	MD_SINGLE_MEDIUM,
	MD_SINGLE_STRONG,
	MD_DUAL_SABERS,
	MD_SABER_STAFF,
	MD_MOVE_TITLE_MAX
};

// Some hard coded badness
// At some point maybe this should be externalized to a .dat file
const char* datapadMoveTitleData[MD_MOVE_TITLE_MAX] =
{
	"@MENUS_ACROBATICS",
	"@MENUS_SINGLE_FAST",
	"@MENUS_SINGLE_MEDIUM",
	"@MENUS_SINGLE_STRONG",
	"@MENUS_DUAL_SABERS",
	"@MENUS_SABER_STAFF",
};

const char* datapadMoveTitleBaseAnims[MD_MOVE_TITLE_MAX] =
{
	"BOTH_RUN1",
	"BOTH_SABERFAST_STANCE",
	"BOTH_STAND2",
	"BOTH_SABERSLOW_STANCE",
	"BOTH_SABERDUAL_STANCE",
	"BOTH_SABERSTAFF_STANCE",
};

constexpr auto MAX_MOVES = 16;

using datpadmovedata_t = struct
{
	const char* title;
	const char* desc;
	const char* anim;
	short sound;
};

static datpadmovedata_t datapadMoveData[MD_MOVE_TITLE_MAX][MAX_MOVES] =
{
	{
		// Acrobatics
		{"@MENUS_FORCE_JUMP1", "@MENUS_FORCE_JUMP1_DESC", "BOTH_FORCEJUMP1", MDS_FORCE_JUMP},
		{"@MENUS_FORCE_FLIP", "@MENUS_FORCE_FLIP_DESC", "BOTH_FLIP_F", MDS_FORCE_JUMP},
		{"@MENUS_ROLL", "@MENUS_ROLL_DESC", "BOTH_ROLL_F", MDS_ROLL},
		{"@MENUS_BACKFLIP_OFF_WALL", "@MENUS_BACKFLIP_OFF_WALL_DESC", "BOTH_WALL_FLIP_BACK1", MDS_FORCE_JUMP},
		{"@MENUS_SIDEFLIP_OFF_WALL", "@MENUS_SIDEFLIP_OFF_WALL_DESC", "BOTH_WALL_FLIP_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_WALL_RUN", "@MENUS_WALL_RUN_DESC", "BOTH_WALL_RUN_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_LONG_JUMP", "@MENUS_LONG_JUMP_DESC", "BOTH_FORCELONGLEAP_START", MDS_FORCE_JUMP},
		{"@MENUS_WALL_GRAB_JUMP", "@MENUS_WALL_GRAB_JUMP_DESC", "BOTH_FORCEWALLREBOUND_FORWARD", MDS_FORCE_JUMP},
		{
			"@MENUS_RUN_UP_WALL_BACKFLIP", "@MENUS_RUN_UP_WALL_BACKFLIP_DESC", "BOTH_FORCEWALLRUNFLIP_START",
			MDS_FORCE_JUMP
		},
		{"@MENUS_JUMPUP_FROM_KNOCKDOWN", "@MENUS_JUMPUP_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN3", MDS_NONE},
		{"@MENUS_JUMPKICK_FROM_KNOCKDOWN", "@MENUS_JUMPKICK_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN2", MDS_NONE},
		{"@MENUS_ROLL_FROM_KNOCKDOWN", "@MENUS_ROLL_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN1", MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Single Saber, Fast Style
		{"@MENUS_STAB_BACK", "@MENUS_STAB_BACK_DESC", "BOTH_A2_STABBACK1", MDS_SABER},
		{"@MENUS_LUNGE_ATTACK", "@MENUS_LUNGE_ATTACK_DESC", "BOTH_LUNGE2_B__T_", MDS_SABER},
		{"@MENUS_FORCE_PULL_IMPALE", "@MENUS_FORCE_PULL_IMPALE_DESC", "BOTH_PULL_IMPALE_STAB", MDS_SABER},
		{"@MENUS_FAST_ATTACK_KATA", "@MENUS_FAST_ATTACK_KATA_DESC", "BOTH_A1_SPECIAL", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", MDS_FORCE_JUMP},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Single Saber, Medium Style
		{"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", MDS_SABER},
		{"@MENUS_FLIP_ATTACK", "@MENUS_FLIP_ATTACK_DESC", "BOTH_JUMPFLIPSLASHDOWN1", MDS_FORCE_JUMP},
		{"@MENUS_FORCE_PULL_SLASH", "@MENUS_FORCE_PULL_SLASH_DESC", "BOTH_PULL_IMPALE_SWING", MDS_SABER},
		{"@MENUS_MEDIUM_ATTACK_KATA", "@MENUS_MEDIUM_ATTACK_KATA_DESC", "BOTH_A2_SPECIAL", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", MDS_FORCE_JUMP},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Single Saber, Strong Style
		{"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", MDS_SABER},
		{"@MENUS_JUMP_ATTACK", "@MENUS_JUMP_ATTACK_DESC", "BOTH_FORCELEAP2_T__B_", MDS_FORCE_JUMP},
		{"@MENUS_FORCE_PULL_SLASH", "@MENUS_FORCE_PULL_SLASH_DESC", "BOTH_PULL_IMPALE_SWING", MDS_SABER},
		{"@MENUS_STRONG_ATTACK_KATA", "@MENUS_STRONG_ATTACK_KATA_DESC", "BOTH_A3_SPECIAL", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", MDS_FORCE_JUMP},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		//Dual Sabers
		{"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", MDS_SABER},
		{"@MENUS_FLIP_FORWARD_ATTACK", "@MENUS_FLIP_FORWARD_ATTACK_DESC", "BOTH_JUMPATTACK6", MDS_FORCE_JUMP},
		{"@MENUS_DUAL_SABERS_TWIRL", "@MENUS_DUAL_SABERS_TWIRL_DESC", "BOTH_SPINATTACK6", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_DUAL", MDS_FORCE_JUMP},
		{"@MENUS_DUAL_SABER_BARRIER", "@MENUS_DUAL_SABER_BARRIER_DESC", "BOTH_A6_SABERPROTECT", MDS_SABER},
		{"@MENUS_DUAL_STAB_FRONT_BACK", "@MENUS_DUAL_STAB_FRONT_BACK_DESC", "BOTH_A6_FB", MDS_SABER},
		{"@MENUS_DUAL_STAB_LEFT_RIGHT", "@MENUS_DUAL_STAB_LEFT_RIGHT_DESC", "BOTH_A6_LR", MDS_SABER},
		{"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", MDS_FORCE_JUMP},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	},

	{
		// Saber Staff
		{"@MENUS_STAB_BACK", "@MENUS_STAB_BACK_DESC", "BOTH_A2_STABBACK1", MDS_SABER},
		{"@MENUS_BACK_FLIP_ATTACK", "@MENUS_BACK_FLIP_ATTACK_DESC", "BOTH_JUMPATTACK7", MDS_FORCE_JUMP},
		{"@MENUS_SABER_STAFF_TWIRL", "@MENUS_SABER_STAFF_TWIRL_DESC", "BOTH_SPINATTACK7", MDS_SABER},
		{"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_STAFF", MDS_FORCE_JUMP},
		{"@MENUS_SPINNING_KATA", "@MENUS_SPINNING_KATA_DESC", "BOTH_A7_SOULCAL", MDS_SABER},
		{"@MENUS_KICK1", "@MENUS_KICK1_DESC", "BOTH_A7_KICK_F", MDS_FORCE_JUMP},
		{"@MENUS_JUMP_KICK", "@MENUS_JUMP_KICK_DESC", "BOTH_A7_KICK_F_AIR", MDS_FORCE_JUMP},
		{"@MENUS_SPLIT_KICK", "@MENUS_SPLIT_KICK_DESC", "BOTH_A7_KICK_RL", MDS_FORCE_JUMP},
		{"@MENUS_SPIN_KICK", "@MENUS_SPIN_KICK_DESC", "BOTH_A7_KICK_S", MDS_FORCE_JUMP},
		{"@MENUS_FLIP_KICK", "@MENUS_FLIP_KICK_DESC", "BOTH_A7_KICK_BF", MDS_FORCE_JUMP},
		{"@MENUS_BUTTERFLY_ATTACK", "@MENUS_BUTTERFLY_ATTACK_DESC", "BOTH_BUTTERFLY_FR1", MDS_SABER},
		{"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", MDS_SABER},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
		{nullptr, nullptr, nullptr, MDS_NONE},
	}
};

static int gamecodetoui[] = { 4, 2, 3, 0, 5, 1, 6 };

uiInfo_t uiInfo;

static void UI_RegisterCvars();
void UI_Load();

static int UI_GetScreenshotFormatForString(const char* str)
{
	if (!Q_stricmp(str, "jpg") || !Q_stricmp(str, "jpeg"))
		return SSF_JPEG;
	if (!Q_stricmp(str, "tga"))
		return SSF_TGA;
	if (!Q_stricmp(str, "png"))
		return SSF_PNG;
	return -1;
}

static const char* UI_GetScreenshotFormatString(const int format)
{
	switch (format)
	{
	default:
	case SSF_JPEG:
		return "jpg";
	case SSF_TGA:
		return "tga";
	case SSF_PNG:
		return "png";
	}
}

using cvarTable_t = struct cvarTable_s
{
	vmCvar_t* vmCvar;
	const char* cvarName;
	const char* defaultString;
	void (*update)();
	uint32_t cvarFlags;
};

vmCvar_t ui_menuFiles;
vmCvar_t ui_hudFiles;

vmCvar_t ui_char_anim;
vmCvar_t ui_char_model;
#ifdef NEW_FEEDER
vmCvar_t ui_char_skin;
vmCvar_t ui_model;
#endif
#ifdef NEW_FEEDER_V6
vmCvar_t ui_char_team_select;
vmCvar_t ui_char_health_select;
vmCvar_t ui_char_mute_voice_line;
#endif
vmCvar_t ui_char_skin_head;
vmCvar_t ui_char_skin_torso;
vmCvar_t ui_char_skin_legs;
vmCvar_t ui_saber_type;
vmCvar_t ui_saber;
vmCvar_t ui_saber2;
vmCvar_t ui_saber_color;
vmCvar_t ui_saber2_color;
#ifdef NEW_FEEDER_V9
vmCvar_t ui_saber_type_md;
vmCvar_t ui_saber_md;
vmCvar_t ui_saber2_md;
vmCvar_t ui_saber_color_md;
vmCvar_t ui_saber2_color_md;
#endif
vmCvar_t ui_char_color_red;
vmCvar_t ui_char_color_green;
vmCvar_t ui_char_color_blue;
vmCvar_t ui_PrecacheModels;
vmCvar_t ui_screenshotType;

vmCvar_t ui_rgb_saber_red;
vmCvar_t ui_rgb_saber_green;
vmCvar_t ui_rgb_saber_blue;

vmCvar_t ui_rgb_saber2_red;
vmCvar_t ui_rgb_saber2_green;
vmCvar_t ui_rgb_saber2_blue;

vmCvar_t ui_SFXSabers;
vmCvar_t ui_SFXSabersGlowSize;
vmCvar_t ui_SFXSabersCoreSize;
vmCvar_t ui_SFXLerpOrigin;
//
vmCvar_t ui_SFXSabersGlowSizeEP1;
vmCvar_t ui_SFXSabersCoreSizeEP1;
vmCvar_t ui_SFXSabersGlowSizeEP2;
vmCvar_t ui_SFXSabersCoreSizeEP2;
vmCvar_t ui_SFXSabersGlowSizeEP3;
vmCvar_t ui_SFXSabersCoreSizeEP3;
vmCvar_t ui_SFXSabersGlowSizeSFX;
vmCvar_t ui_SFXSabersCoreSizeSFX;
vmCvar_t ui_SFXSabersGlowSizeOT;
vmCvar_t ui_SFXSabersCoreSizeOT;
vmCvar_t ui_SFXSabersGlowSizeROTJ;
vmCvar_t ui_SFXSabersCoreSizeROTJ;
vmCvar_t ui_SFXSabersGlowSizeTFA;
vmCvar_t ui_SFXSabersCoreSizeTFA;
vmCvar_t ui_SFXSabersGlowSizeUSB;
vmCvar_t ui_SFXSabersCoreSizeUSB;
//
vmCvar_t ui_SFXSabersGlowSizeRebels;
vmCvar_t ui_SFXSabersCoreSizeRebels;

vmCvar_t ui_SerenityJediEngineMode;

vmCvar_t ui_char_model_angle;

vmCvar_t ui_com_outcast;
vmCvar_t ui_update7firststartup;
vmCvar_t ui_totgfirststartup;
vmCvar_t ui_cursor;

vmCvar_t ui_npc_saber;
vmCvar_t ui_npc_sabertwo;

vmCvar_t ui_npc_sabercolor;
vmCvar_t ui_npc_sabertwocolor;

cvar_t* g_NPCsabercolor;
cvar_t* g_NPCsabertwo;
cvar_t* g_NPCsabertwocolor;
vmCvar_t ui_com_kotor;
vmCvar_t ui_com_rend2;

static void UI_UpdateScreenshot()
{
	qboolean changed = qfalse;
	// check some things
	if (ui_screenshotType.string[0] && isalpha(ui_screenshotType.string[0]))
	{
		const int ssf = UI_GetScreenshotFormatForString(ui_screenshotType.string);
		if (ssf == -1)
		{
			ui.Printf("UI Screenshot Format Type '%s' unrecognised, defaulting to JPEG\n", ui_screenshotType.string);
			uiInfo.uiDC.screenshotFormat = SSF_JPEG;
			changed = qtrue;
		}
		else
			uiInfo.uiDC.screenshotFormat = ssf;
	}
	else if (ui_screenshotType.integer < SSF_JPEG || ui_screenshotType.integer > SSF_PNG)
	{
		ui.Printf("ui_screenshotType %i is out of range, defaulting to 0 (JPEG)\n", ui_screenshotType.integer);
		uiInfo.uiDC.screenshotFormat = SSF_JPEG;
		changed = qtrue;
	}
	else
	{
		uiInfo.uiDC.screenshotFormat = atoi(ui_screenshotType.string);
		changed = qtrue;
	}

	if (changed)
	{
		Cvar_Set("ui_screenshotType", UI_GetScreenshotFormatString(uiInfo.uiDC.screenshotFormat));
		Cvar_Update(&ui_screenshotType);
	}
}

vmCvar_t r_ratiofix;

static void UI_Set2DRatio()
{
	if (r_ratiofix.integer)
	{
		uiInfo.uiDC.widthRatioCoef = static_cast<float>(SCREEN_WIDTH * uiInfo.uiDC.glconfig.vidHeight) / static_cast<
			float>(SCREEN_HEIGHT * uiInfo.uiDC.glconfig.vidWidth);
	}
	else
	{
		uiInfo.uiDC.widthRatioCoef = 1.0f;
	}
}

static cvarTable_t cvarTable[] =
{
	{&ui_menuFiles, "ui_menuFiles", "ui/menus.txt", nullptr, CVAR_ARCHIVE},
#ifdef JK2_MODE
	{ &ui_hudFiles,				"cg_hudFiles",			"ui/jk2hud.txt", nullptr, CVAR_ARCHIVE},
#else
	{&ui_hudFiles, "cg_hudFiles", "ui/jahud.txt", nullptr, CVAR_ARCHIVE},
#endif

	{&ui_char_anim, "ui_char_anim", "BOTH_MENUIDLE1", nullptr, 0},

	{&ui_char_model, "ui_char_model", "", nullptr, 0}, //these are filled in by the "g_*" versions on load
#ifdef NEW_FEEDER
	{&ui_char_skin, "ui_char_skin", "", nullptr, 0},
	{&ui_model, "ui_model", "", nullptr, 0},
#endif
#ifdef NEW_FEEDER_V6
	{&ui_char_team_select, "ui_char_team_select", "", nullptr, 0},
	{&ui_char_health_select, "ui_char_health_select", "0", nullptr, 0},
	{&ui_char_mute_voice_line, "ui_char_mute_voice_line", "0", nullptr, CVAR_ARCHIVE},
#endif
	{&ui_char_skin_head, "ui_char_skin_head", "", nullptr, 0},
	//the "g_*" versions are initialized in UI_Init, ui_atoms.cpp
	{&ui_char_skin_torso, "ui_char_skin_torso", "", nullptr, 0},
	{&ui_char_skin_legs, "ui_char_skin_legs", "", nullptr, 0},

	{&ui_saber_type, "ui_saber_type", "", nullptr, 0},
	{&ui_saber, "ui_saber", "", nullptr, 0},
	{&ui_saber2, "ui_saber2", "", nullptr, 0},
	{&ui_saber_color, "ui_saber_color", "", nullptr, 0},
	{&ui_saber2_color, "ui_saber2_color", "", nullptr, 0},

#ifdef NEW_FEEDER_V9
	{&ui_saber_type_md, "ui_saber_type_md", "single", nullptr, 0},
	{&ui_saber_md, "ui_saber_md", "single_1", nullptr, 0},
	{&ui_saber2_md, "ui_saber2_md", "", nullptr, 0},
	{&ui_saber_color_md, "ui_saber_color_md", "yellow", nullptr, 0},
	{&ui_saber2_color_md, "ui_saber2_color_md", "yellow", nullptr, 0},
#endif

	{&ui_char_color_red, "ui_char_color_red", "", nullptr, 0},
	{&ui_char_color_green, "ui_char_color_green", "", nullptr, 0},
	{&ui_char_color_blue, "ui_char_color_blue", "", nullptr, 0},

	{&ui_PrecacheModels, "ui_PrecacheModels", "1", nullptr, CVAR_ARCHIVE},

	{&ui_screenshotType, "ui_screenshotType", "jpg", UI_UpdateScreenshot, CVAR_ARCHIVE},

	{&ui_rgb_saber_red, "ui_rgb_saber_red", "", nullptr, 0},
	{&ui_rgb_saber_blue, "ui_rgb_saber_blue", "", nullptr, 0},
	{&ui_rgb_saber_green, "ui_rgb_saber_green", "", nullptr, 0},
	{&ui_rgb_saber2_red, "ui_rgb_saber2_red", "", nullptr, 0},
	{&ui_rgb_saber2_blue, "ui_rgb_saber2_blue", "", nullptr, 0},
	{&ui_rgb_saber2_green, "ui_rgb_saber2_green", "", nullptr, 0},

	{&ui_SFXSabers, "cg_SFXSabers", "4", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSize, "cg_SFXSabersGlowSize", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSize, "cg_SFXSabersCoreSize", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXLerpOrigin, "cg_SFXLerpOrigin", "0", nullptr, CVAR_ARCHIVE},
	//
	{&ui_SFXSabersGlowSizeEP1, "cg_SFXSabersGlowSizeEP1", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeEP1, "cg_SFXSabersCoreSizeEP1", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeEP2, "cg_SFXSabersGlowSizeEP2", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeEP2, "cg_SFXSabersCoreSizeEP2", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeEP3, "cg_SFXSabersGlowSizeEP3", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeEP3, "cg_SFXSabersCoreSizeEP3", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeSFX, "cg_SFXSabersGlowSizeSFX", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeSFX, "cg_SFXSabersCoreSizeSFX", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeOT, "cg_SFXSabersGlowSizeOT", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeOT, "cg_SFXSabersCoreSizeOT", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeROTJ, "cg_SFXSabersGlowSizeROTJ", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeROTJ, "cg_SFXSabersCoreSizeROTJ", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeTFA, "cg_SFXSabersGlowSizeTFA", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeTFA, "cg_SFXSabersCoreSizeTFA", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersGlowSizeUSB, "cg_SFXSabersGlowSizeUSB", "1.0", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeUSB, "cg_SFXSabersCoreSizeUSB", "1.0", nullptr, CVAR_ARCHIVE},
	//
	{&ui_SFXSabersGlowSizeRebels, "cg_SFXSabersGlowSizeRebels", "0.6", nullptr, CVAR_ARCHIVE},
	{&ui_SFXSabersCoreSizeRebels, "cg_SFXSabersCoreSizeRebels", "0.6", nullptr, CVAR_ARCHIVE},

	{&ui_SerenityJediEngineMode, "g_SerenityJediEngineMode", "1", nullptr, CVAR_ARCHIVE},

	{&ui_char_model_angle, "ui_char_model_angle", "175", nullptr, 0},

	{&ui_com_outcast, "com_outcast", "0", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART},

	{&ui_update7firststartup, "g_update7firststartup", "1", nullptr, 0},

	{&ui_totgfirststartup, "g_totgfirststartup", "1", nullptr, 0},

	{&ui_cursor, "g_cursor", "10", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART}, //11 or 12 with base cursor

	{&r_ratiofix, "r_ratiofix", "0", UI_Set2DRatio, CVAR_ARCHIVE},

	{&ui_npc_saber, "ui_npc_saber", "", nullptr},
	{&ui_npc_sabertwo, "ui_npc_sabertwo", "", nullptr},

	{&ui_npc_sabercolor, "ui_npc_sabercolor", "", nullptr},
	{&ui_npc_sabertwocolor, "ui_npc_sabertwocolor", "", nullptr},

	{&ui_com_kotor, "com_kotor", "0", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART},

	{&ui_com_rend2, "com_rend2", "0", nullptr, CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART},
};

constexpr auto FP_UPDATED_NONE = -1;
constexpr auto NOWEAPON = -1;

static constexpr size_t cvarTableSize = std::size(cvarTable);

extern void SE_CheckForLanguageUpdates();

void Text_Paint(float x, float y, float scale, vec4_t color, const char* text, int iMaxPixelWidth, int style,
	int iFontIndex);
int Key_GetCatcher();

constexpr auto UI_FPS_FRAMES = 4;

void _UI_Refresh(const int realtime)
{
	static int index;
	static int previousTimes[UI_FPS_FRAMES];

	if (!(Key_GetCatcher() & KEYCATCH_UI))
	{
		return;
	}
	SE_CheckForLanguageUpdates();

	if (Menus_AnyFullScreenVisible())
	{
		//if not in full screen, don't mess with ghoul2
		//rww - ghoul2 needs to know what time it is even if the client/server are not running
		//FIXME: this screws up the game when you go back to the game...
		re.G2API_SetTime(realtime, 0);
		re.G2API_SetTime(realtime, 1);
	}

	uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime = realtime;

	previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
	index++;
	if (index > UI_FPS_FRAMES)
	{
		// average multiple frames together to smooth changes out a bit
		int total = 0;
		for (const int previousTime : previousTimes)
		{
			total += previousTime;
		}
		if (!total)
		{
			total = 1;
		}
		uiInfo.uiDC.FPS = static_cast<float>(1000 * UI_FPS_FRAMES) / total;
	}

	UI_UpdateCvars();

	if (Menu_Count() > 0)
	{
		// paint all the menus
		Menu_PaintAll();
	}

	// draw cursor
	UI_SetColor(nullptr);

	if (Menu_Count() > 0)
	{
		if (uiInfo.uiDC.cursorShow == qtrue)
		{
			if (ui_cursor.integer == 0)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor);
			}
			else if (ui_cursor.integer == 1)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_anakin);
			}
			else if (ui_cursor.integer == 2)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_jk);
			}
			else if (ui_cursor.integer == 3)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_katarn);
			}
			else if (ui_cursor.integer == 4)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_kylo);
			}
			else if (ui_cursor.integer == 5)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_luke);
			}
			else if (ui_cursor.integer == 6)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_obiwan);
			}
			else if (ui_cursor.integer == 7)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_oldrepublic);
			}
			else if (ui_cursor.integer == 8)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_quigon);
			}
			else if (ui_cursor.integer == 9)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_rey);
			}
			else if (ui_cursor.integer == 10)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_vader);
			}
			else if (ui_cursor.integer == 11)
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_windu);
			}
			else
			{
				UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor_kylo);
			}
		}
	}
}

#define MODSBUFSIZE (MAX_MODS * MAX_QPATH)

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods()
{
	char dirlist[MODSBUFSIZE];

	uiInfo.modCount = 0;

	const int numdirs = FS_GetFileList("$modlist", "", dirlist, sizeof dirlist);
	char* dirptr = dirlist;
	for (int i = 0; i < numdirs; i++)
	{
		const int dirlen = strlen(dirptr) + 1;
		const char* descptr = dirptr + dirlen;
		uiInfo.modList[uiInfo.modCount].mod_name = String_Alloc(dirptr);
		uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
		dirptr += dirlen + strlen(descptr) + 1;
		uiInfo.modCount++;
		if (uiInfo.modCount >= MAX_MODS)
		{
			break;
		}
	}
}

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .qvm file
================
*/
extern "C" Q_EXPORT intptr_t QDECL vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5,
	int arg6, int arg7, int arg8, int arg9, int arg10, int arg11)
{
	return 0;
}

/*
================
Text_PaintChar
================
*/
/*
static void Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader)
{
	float w, h;

	w = width * scale;
	h = height * scale;
	ui.R_DrawStretchPic((int)x, (int)y, w, h, s, t, s2, t2, hShader );	//make the coords (int) or else the chars bleed
}
*/

/*
================
Text_Paint
================
*/
// iMaxPixelWidth is 0 here for no limit (but gets converted to -1), else max printable pixel width relative to start pos
//
void Text_Paint(const float x, const float y, const float scale, vec4_t color, const char* text, const int iMaxPixelWidth, const int style,
	int iFontIndex)
{
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	// kludge.. convert JK2 menu styles to SOF2 printstring ctrl codes...
	//
	int iStyleOR = 0;
	switch (style)
	{
		//	case  ITEM_TEXTSTYLE_NORMAL:			iStyleOR = 0;break;					// JK2 normal text
		//	case  ITEM_TEXTSTYLE_BLINK:				iStyleOR = STYLE_BLINK;break;		// JK2 fast blinking
	case ITEM_TEXTSTYLE_PULSE: iStyleOR = STYLE_BLINK;
		break; // JK2 slow pulsing
	case ITEM_TEXTSTYLE_SHADOWED: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	case ITEM_TEXTSTYLE_OUTLINED: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	case ITEM_TEXTSTYLE_OUTLINESHADOWED: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	case ITEM_TEXTSTYLE_SHADOWEDMORE: iStyleOR = STYLE_DROPSHADOW;
		break; // JK2 drop shadow ( need a color for this )
	default:;
	}

	ui.R_Font_DrawString(x, // int ox
		y, // int oy
		text, // const char *text
		color, // paletteRGBA_c c
		iStyleOR | iFontIndex, // const int iFontHandle
		!iMaxPixelWidth ? -1 : iMaxPixelWidth, // iMaxPixelWidth (-1 = none)
		scale // const float scale = 1.0f
	);
}

/*
================
Text_PaintWithCursor
================
*/
// iMaxPixelWidth is 0 here for no-limit
static void Text_PaintWithCursor(const float x, const float y, const float scale, vec4_t color, const char* text, const int cursorPos, const char cursor,
	const int iMaxPixelWidth, const int style, const int iFontIndex)
{
	Text_Paint(x, y, scale, color, text, iMaxPixelWidth, style, iFontIndex);

	// now print the cursor as well...
	//
	char sTemp[1024];
	int iCopyCount = iMaxPixelWidth > 0 ? Q_min((int)strlen(text), iMaxPixelWidth) : static_cast<int>(strlen(text));
	iCopyCount = Q_min(iCopyCount, cursorPos);
	iCopyCount = Q_min(iCopyCount, (int)sizeof(sTemp));

	// copy text into temp buffer for pixel measure...
	//
	strncpy(sTemp, text, iCopyCount);
	sTemp[iCopyCount] = '\0';

	const int iNextXpos = ui.R_Font_StrLenPixels(sTemp, iFontIndex, scale);

	Text_Paint(x + iNextXpos, y, scale, color, va("%c", cursor), iMaxPixelWidth, style | ITEM_TEXTSTYLE_BLINK,
		iFontIndex);
}

static const char* UI_FeederItemText(const float feederID, const int index, const int column, qhandle_t* handle)
{
	*handle = -1;

	if (feederID == FEEDER_SAVEGAMES)
	{
		if (column == 0)
		{
			return s_savedata[index].currentSaveFileComments;
		}
		return s_savedata[index].currentSaveFileDateTimeString;
	}
	if (feederID == FEEDER_MOVES)
	{
		return datapadMoveData[uiInfo.movesTitleIndex][index].title;
	}
	if (feederID == FEEDER_MOVES_TITLES)
	{
		return datapadMoveTitleData[index];
	}
	if (feederID == FEEDER_PLAYER_SPECIES)
	{
		if (index >= 0 && index < uiInfo.playerSpeciesCount)
		{
			return uiInfo.playerSpecies[index].Name;
		}
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
#ifdef JK2_MODE
		// FIXME
		return nullptr;
#else
		return SE_GetLanguageName(index);
#endif
	}
	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].
				name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name;
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].
				name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name;
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name;
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			*handle = ui.R_RegisterShaderNoMip(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader;
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		if (index >= 0 && index < uiInfo.modCount)
		{
			if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr)
			{
				return uiInfo.modList[index].modDescr;
			}
			return uiInfo.modList[index].mod_name;
		}
	}
#ifdef NEW_FEEDER
	else if (feederID == FEEDER_MD_FACTION)
	{ 
		if (index >= 0 && index < TOTAL_ERAS) 
		{ 
			return eraNames[index]; 
		} 
	} 
#endif
	else if (feederID == FEEDER_DUEL_MAPS)
	{
		if (index >= 0 && index < NO_OF_DUEL_MAPS)
		{
			return duelMaps[index].duelMapName;
		}
	}
	else if (feederID == FEEDER_SINGLE_SABER_HILTS)
	{
		if (index >= 0 && index < NO_OF_SINGLE_SABER_HILTS)
		{
			return singleSaberHilts[index].singleSaberHiltName;
		}
	}
	else if (feederID == FEEDER_STAFF_SABER_HILTS)
	{
		if (index >= 0 && index < NO_OF_STAFF_SABER_HILTS)
		{
			return staffSaberHilts[index].staffSaberHiltName;
		}
	}

	return "";
}

#ifdef NEW_FEEDER
static qhandle_t UI_FeederItemImage(const float feederID, int index)
#else
static qhandle_t UI_FeederItemImage(const float feederID, const int index)
#endif
{
	if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name));
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name));
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg",
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
				uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name));
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			return ui.R_RegisterShaderNoMip(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
		}
	}
	/*	else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS)
		{
			int actual;
			UI_SelectedMap(index, &actual);
			index = actual;
			if (index >= 0 && index < uiInfo.mapCount)
			{
				if (uiInfo.mapList[index].levelShot == -1)
				{
					uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
				}
				return uiInfo.mapList[index].levelShot;
			}
		}
	*/
#ifdef NEW_FEEDER
	else if ((feederID == FEEDER_MD_MODELS) || (feederID == FEEDER_MD_VARIANTS))
	{
		int actual = 0;
		int start = eraIndex[uiEra];
		int end = eraIndex[uiEra + 1];

		if (feederID == FEEDER_MD_MODELS)
			MD_UI_SelectedTeamHead_SubDivs(index, &actual);
		else
			MD_UI_SelectedTeamHead(index, &actual);
		index = actual;

		if (feederID == FEEDER_MD_VARIANTS)
		{
			int i = 1;
			for (i = 1; i < NO_OF_MD_MODELS; i++) {
				if (!charMD[uiModelIndex + i].isSubDiv)
					break;
			}

			start = uiModelIndex;
			end = uiModelIndex + i;

			index += uiModelIndex - eraIndex[uiEra];
		}

		if (index >= start && index < end)
		{
			if (!mdHeadIcons[index])
				mdHeadIcons[index] = ui.R_RegisterShaderNoMip(va("gfx/menus/charactermenuicons/%s", ((!strcmp(charMD[index].icon,"")) ? charMD[index].npc : charMD[index].icon)));//(va("models/players/%s/%s", charMD[index].model, charMD[index].icon));

			return mdHeadIcons[index];
		}
	}
#endif
	return 0;
}

/*
=================
CreateNextSaveName
=================
*/
static int CreateNextSaveName(char* fileName)
{
	// Loop through all the save games and look for the first open name
	for (int i = 0; i < MAX_SAVELOADFILES; i++)
	{
#ifdef JK2_MODE
		Com_sprintf(fileName, MAX_SAVELOADNAME, "jkii%02d", i);
#else
		Com_sprintf(fileName, MAX_SAVELOADNAME, "jedi_%02d", i);
#endif

		if (!ui.SG_GetSaveGameComment(fileName, nullptr, nullptr))
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
===============
UI_DeferMenuScript

Return true if the menu script should be deferred for later
===============
*/
static qboolean UI_DeferMenuScript(const char** args)
{
	const char* name;

	// Whats the reason for being deferred?
	if (!String_Parse(args, &name))
	{
		return qfalse;
	}

	// Handle the custom cases
	if (!Q_stricmp(name, "VideoSetup"))
	{
		const char* warningMenuName;

		// No warning menu specified
		if (!String_Parse(args, &warningMenuName))
		{
			return qfalse;
		}

		// Defer if the video options were modified
		const qboolean deferred = Cvar_VariableIntegerValue("ui_r_modified") ? qtrue : qfalse;

		if (deferred)
		{
			// Open the warning menu
			Menus_OpenByName(warningMenuName);
		}

		return deferred;
	}

	return qfalse;
}

#ifdef NEW_FEEDER_V5
static void UI_ClearForce()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		memset(pState->forcePowerLevel, 0, sizeof(pState->forcePowerLevel));
		pState->forcePowersKnown = 0;
		pState->saberStylesKnown = 0;
	}
}
#endif
#ifdef NEW_FEEDER_V6
static bool firstTimeLoad = false;
#endif

/*
===============
UI_RunMenuScript
===============
*/
static qboolean UI_RunMenuScript(const char** args)
{
	const char* name, * name2, * mapName, * menuName, * warningMenuName;

	if (String_Parse(args, &name))
	{

#ifdef NEW_FEEDER_V1
#ifdef NEW_FEEDER_V6
		if (Q_stricmp(name, "md_char_init") == 0)
		{
			if (!firstTimeLoad) {
#ifdef NEW_FEEDER_V7
#else
				firstTimeLoad = true;
#endif
				goto runEra;
			}
			return qtrue;
		}
#endif
		for (int i = 0; i < TOTAL_ERAS; i++) {
			if (Q_stricmp(name, era_table[i].name) == 0) {
				uiEra = i;
#ifdef NEW_FEEDER_V6
runEra:
#endif
#ifdef NEW_FEEDER_V7
				int select = eraIndex[uiEra];
				int positionM = 0;
				int positionV = 0;

				if (!firstTimeLoad) {
					int actual = 0;
					for (int j = eraIndex[uiEra]; j < uiModelIndex; j++) {
						if (charMD[j].isSubDiv) // hasSubDiv
							actual++;
					}
					positionM = uiModelIndex - eraIndex[uiEra] - actual;
					positionV = uiVariantIndex - uiModelIndex;
					select = uiVariantIndex;
				}
				else
				{
					uiModelIndex = eraIndex[uiEra];
					uiVariantIndex = eraIndex[uiEra];
				}

				if (Q_strncmp(charMD[select].skin, "model_", 6) == 0)
					Cvar_Set("ui_model", va("%s/%s", charMD[select].model, &charMD[select].skin[6]));
				else
					Cvar_Set("ui_model", va("%s/%s", charMD[select].model, charMD[select].skin));

				Cvar_Set("ui_char_model", charMD[select].model);
				Cvar_Set("ui_char_skin", charMD[select].skin);

				if (!firstTimeLoad)
					UI_UpdateCharacter(qtrue, qtrue);

				firstTimeLoad = true;
#else
				uiModelIndex = eraIndex[uiEra];
				uiVariantIndex = eraIndex[uiEra];

				if (Q_strncmp(charMD[eraIndex[uiEra]].skin, "model_", 6) == 0)
					Cvar_Set("ui_model", va("%s/%s", charMD[eraIndex[uiEra]].model, &charMD[eraIndex[uiEra]].skin[6])); //standard model // strip model_
				else
					Cvar_Set("ui_model", va("%s/%s", charMD[eraIndex[uiEra]].model, charMD[eraIndex[uiEra]].skin));

				Cvar_Set("ui_char_model", charMD[eraIndex[uiEra]].model);//UI_Cvar_VariableString("ui_model"));
				Cvar_Set("ui_char_skin", charMD[eraIndex[uiEra]].skin);//
#endif
				menuDef_t* menu = Menus_FindByName("ingamecharacter");
				if (menu) {
#ifdef NEW_FEEDER_V3
					itemDef_t* item = Menu_FindItemByName(menu, "modellist");
					listBoxDef_t* list = static_cast<listBoxDef_t*>(item->typeData);
#ifdef NEW_FEEDER_V7
					if (list) {
						list->cursorPos = positionM;
					}
					item->cursorPos = positionM;
#else
					if (list) {
						list->cursorPos = 0;
					}
					item->cursorPos = 0;
#endif
#endif

					itemDef_t* itemFeeder = Menu_FindItemByName(menu, "variantlist");
					listBoxDef_t* listPtr = static_cast<listBoxDef_t*>(itemFeeder->typeData);
#ifdef NEW_FEEDER_V7
					if (listPtr) {
						listPtr->cursorPos = positionV;
					}
					itemFeeder->cursorPos = positionV;
#else
					if (listPtr) {
						listPtr->cursorPos = 0;
					}
					itemFeeder->cursorPos = 0;
#endif

#ifdef NEW_FEEDER_V2
					itemDef_t* itemDesc = Menu_FindItemByName(menu, "char_desc");
					if (itemDesc) {
						itemDesc->text = (char *)charMD[uiVariantIndex].desc;
					}
#endif
				}
				return qtrue;
			}
		}
#endif

		if (Q_stricmp(name, "resetdefaults") == 0)
		{
			UI_ResetDefaults();
		}
		else if (Q_stricmp(name, "saveControls") == 0)
		{
			Controls_SetConfig();
		}
		else if (Q_stricmp(name, "loadControls") == 0)
		{
			Controls_GetConfig();
		}
		else if (Q_stricmp(name, "clearError") == 0)
		{
			Cvar_Set("com_errorMessage", "");
		}
		else if (Q_stricmp(name, "ReadSaveDirectory") == 0)
		{
			s_savegame.saveFileCnt = -1; //force a refresh at drawtime
			//			ReadSaveDirectory();
		}
		else if (Q_stricmp(name, "loadAuto") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "load auto\n"); //load game menu
		}
		else if (Q_stricmp(name, "loadgame") == 0)
		{
			if (s_savedata[s_savegame.currentLine].currentSaveFileName) // && (*s_file_desc_field.field.buffer))
			{
				Menus_CloseAll();
				ui.Cmd_ExecuteText(EXEC_APPEND,
					va("load %s\n", s_savedata[s_savegame.currentLine].currentSaveFileName));
			}
			// after loading a game, the list box (and it's highlight) get's reset back to 0, but currentLine sticks around, so set it to 0 here
			s_savegame.currentLine = 0;
		}
		else if (Q_stricmp(name, "deletegame") == 0)
		{
			if (s_savedata[s_savegame.currentLine].currentSaveFileName) // A line was chosen
			{
#ifndef FINAL_BUILD
				ui.Printf(va("%s\n", "Attempting to delete game"));
#endif

				ui.Cmd_ExecuteText(EXEC_NOW, va("wipe %s\n", s_savedata[s_savegame.currentLine].currentSaveFileName));

				if (s_savegame.currentLine > 0 && s_savegame.currentLine + 1 == s_savegame.saveFileCnt)
				{
					s_savegame.currentLine--;
					// yeah this is a pretty bad hack
					// adjust cursor position of listbox so correct item is highlighted
					UI_AdjustSaveGameListBox(s_savegame.currentLine);
				}

				//				ReadSaveDirectory();	//refresh
				s_savegame.saveFileCnt = -1; //force a refresh at drawtime
			}
		}
		else if (Q_stricmp(name, "savegame") == 0)
		{
			char fileName[MAX_SAVELOADNAME];
			char description[64];
			// Create a new save game
			//			if ( !s_savedata[s_savegame.currentLine].currentSaveFileName)	// No line was chosen
			{
				CreateNextSaveName(fileName); // Get a name to save to
			}
			//			else	// Overwrite a current save game? Ask first.
			{
				//				s_savegame.yes.generic.flags	= QMF_HIGHLIGHT_IF_FOCUS;
				//				s_savegame.no.generic.flags		= QMF_HIGHLIGHT_IF_FOCUS;

				//				strcpy(fileName,s_savedata[s_savegame.currentLine].currentSaveFileName);
				//				s_savegame.awaitingSave = qtrue;
				//				s_savegame.deletegame.generic.flags	= QMF_GRAYED;	// Turn off delete button
				//				break;
			}

			// Save description line
			ui.Cvar_VariableStringBuffer("ui_gameDesc", description, sizeof description);
			ui.SG_StoreSaveGameComment(description);

			ui.Cmd_ExecuteText(EXEC_APPEND, va("save %s\n", fileName));
			s_savegame.saveFileCnt = -1; //force a refresh the next time around
		}
		else if (Q_stricmp(name, "LoadMods") == 0)
		{
			UI_LoadMods();
		}
		else if (Q_stricmp(name, "RunMod") == 0)
		{
			if (uiInfo.modList[uiInfo.modIndex].mod_name)
			{
				Cvar_Set("fs_game", uiInfo.modList[uiInfo.modIndex].mod_name);
				FS_Restart();
				Cbuf_ExecuteText(EXEC_APPEND, "vid_restart;");
			}
		}
		else if (Q_stricmp(name, "Quit") == 0)
		{
			Cbuf_ExecuteText(EXEC_NOW, "quit");
		}
		else if (Q_stricmp(name, "Controls") == 0)
		{
			Cvar_Set("cl_paused", "1");
			trap_Key_SetCatcher(KEYCATCH_UI);
			Menus_CloseAll();
			Menus_ActivateByName("setup_menu2");
		}
		else if (Q_stricmp(name, "Leave") == 0)
		{
			Cbuf_ExecuteText(EXEC_APPEND, "disconnect\n");
			trap_Key_SetCatcher(KEYCATCH_UI);
			Menus_CloseAll();
		}
		else if (Q_stricmp(name, "getvideosetup") == 0)
		{
			UI_GetVideoSetup();
		}
		else if (Q_stricmp(name, "updatevideosetup") == 0)
		{
			UI_UpdateVideoSetup();
		}
		else if (Q_stricmp(name, "nextDataPadForcePower") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpforcenext\n");
		}
		else if (Q_stricmp(name, "prevDataPadForcePower") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpforceprev\n");
		}
		else if (Q_stricmp(name, "nextDataPadWeapon") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpweapnext\n");
		}
		else if (Q_stricmp(name, "prevDataPadWeapon") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpweapprev\n");
		}
		else if (Q_stricmp(name, "nextDataPadInventory") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpinvnext\n");
		}
		else if (Q_stricmp(name, "prevDataPadInventory") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "dpinvprev\n");
		}
		else if (Q_stricmp(name, "checkvid1data") == 0) // Warn user data has changed before leaving screen?
		{
			String_Parse(args, &menuName);

			String_Parse(args, &warningMenuName);

			UI_CheckVid1Data(menuName, warningMenuName);
		}
#ifdef NEW_FEEDER
		else if (Q_stricmp(name, "md_char") == 0) 
		{
			//Cvar_Set("ui_md_name", charMD[uiModelIndex].name);
			//Cvar_Set("ui_md_title", charMD[uiModelIndex].title);
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && !strcmp(menu->window.name, "ingamecharacter"))
			{
				{
					const auto item = Menu_FindItemByName(menu, "char_name");
					if (item) {
						item->text = (char*)charMD[uiVariantIndex].name;
					}
				}
				{
					const auto item = Menu_FindItemByName(menu, "char_title");
					if (item) {
						item->text = (char*)charMD[uiVariantIndex].title;
					}
				}
			}
		}
		else if (Q_stricmp(name, "md_npc") == 0) 
		{
			const char* targetname = va("%s%d", charMD[uiVariantIndex].npc, Q_irand(0, 9999));
			const char* team;
			const int npcHealth = ui_char_health_select.integer;

			switch (ui_char_team_select.integer)
			{
				case 1: team = "TEAM_PLAYER";		break;
				case 2: team = "TEAM_ENEMY";		break;
				case 3: team = "TEAM_SOLO";			break;
				case 4: team = "TEAM_NEUTRAL";		break;
				default: team = "TEAM_DEFAULT";		break;
			}
			
			if (strcmp(team, "TEAM_DEFAULT") == 0 && npcHealth == 0) {
				// Default npc spawn
				ui.Cmd_ExecuteText(EXEC_APPEND, va("npc spawn %s %s\n", charMD[uiVariantIndex].npc, targetname));
				ui.Cmd_ExecuteText(EXEC_APPEND, va("set npc_spawn_recent npc spawn %s %s\n", charMD[uiVariantIndex].npc, targetname));
			}
			else {
				// Spawn NPC with set team and health
				ui.Cmd_ExecuteText(EXEC_APPEND, va("npc spawn %s %s %s %d\n", charMD[uiVariantIndex].npc, targetname, team, npcHealth));
				ui.Cmd_ExecuteText(EXEC_APPEND, va("set npc_spawn_recent npc spawn %s %s %s %d\n", charMD[uiVariantIndex].npc, targetname, team, npcHealth));
			}
			
			/*
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && !strcmp(menu->window.name, "ingamecharacter"))
			{
				const auto item = Menu_FindItemByName(menu, "character");
				if (item) {
					ItemParse_model_g2anim_go(item, anim_table[charMD[uiVariantIndex].npcAnimation].name);
					uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				}
			}
			*/

			if (strcmp(charMD[uiVariantIndex].npcSelectSound, "") && ui_char_mute_voice_line.integer == 0)
				DC->startLocalSound(DC->registerSound(charMD[uiVariantIndex].npcSelectSound, qfalse), CHAN_VOICE);
		}
		else if (Q_stricmp(name, "md_player") == 0)
		{
#ifdef NEW_FEEDER_V5
			UI_ClearInventory();
			UI_ClearForce();
#endif
			UI_ClearWeapons();
			ui.Cmd_ExecuteText(EXEC_APPEND, va("playermodel %s\n", charMD[uiVariantIndex].npc));
#ifdef NEW_FEEDER_V8
			//Cvar_Set("g_saber", Cvar_VariableString("ui_saber"));
			//Cvar_Set("g_saber2", Cvar_VariableString("ui_saber2"));

			//Cvar_Set("g_saber_color", Cvar_VariableString("ui_saber_color"));
			//Cvar_Set("g_saber2_color", Cvar_VariableString("ui_saber2_color"));
#endif
			switch (ui_char_team_select.integer)
			{
				case 1:	ui.Cmd_ExecuteText(EXEC_APPEND, "playerteam player\n");			break;
				case 2:	ui.Cmd_ExecuteText(EXEC_APPEND, "playerteam enemy\n");			break;
				case 3:	ui.Cmd_ExecuteText(EXEC_APPEND, "playerteam solo\n");			break;
				case 4:	ui.Cmd_ExecuteText(EXEC_APPEND, "playerteam neutral\n");		break;

				default: break;
			}
			
			const int playerHealth = ui_char_health_select.integer;
			if (playerHealth != 0)
			{
				// Set player health
				ui.Cmd_ExecuteText(EXEC_APPEND, va("give maxhealth %d\n", playerHealth));
			}
			else
			{
				// Make sure player starts with full health
				ui.Cmd_ExecuteText(EXEC_APPEND, "give health\n");
				ui.Cmd_ExecuteText(EXEC_APPEND, "give armor\n");
			}

			/*
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && !strcmp(menu->window.name, "ingamecharacter"))
			{
				const auto item = Menu_FindItemByName(menu, "character");
				if (item) {
					ItemParse_model_g2anim_go(item, anim_table[charMD[uiVariantIndex].plyAnimation].name);
					uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
				}
			}
			*/

			if (strcmp(charMD[uiVariantIndex].plySelectSound, "") && ui_char_mute_voice_line.integer == 0)
				DC->startLocalSound(DC->registerSound(charMD[uiVariantIndex].plySelectSound, qfalse), CHAN_VOICE);

			ui.Cmd_ExecuteText(EXEC_APPEND, va("setsaberstances %i %i\n", charMD[uiVariantIndex].style, charMD[uiVariantIndex].styleBitFlag));
		}
#endif
		else if (Q_stricmp(name, "md_updateduelmap") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && (!strcmp(menu->window.name, "duelmapsMenu") || !strcmp(menu->window.name, "ingameduelmapsMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "duelmapdesc");
				if (item && duelMaps[duelMapIndex].duelMapBSP != "") {
					item->text = (char*)duelMaps[duelMapIndex].duelMapDesc;
				}
			}
		}
		else if (Q_stricmp(name, "md_playduelmap") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && (!strcmp(menu->window.name, "duelmapsMenu") || !strcmp(menu->window.name, "ingameduelmapsMenu")))
			{
				if (duelMaps[duelMapIndex].duelMapBSP != "")
				{
					Menus_CloseAll();
					ui.Cmd_ExecuteText(EXEC_APPEND, va("devmap %s\n", duelMaps[duelMapIndex].duelMapBSP));
					duelMapIndex = 1; // Reset index back to 1
				}
			}
		}
		else if (Q_stricmp(name, "md_assignsaber1") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			// These menus affect JKA character saber cvars
			if (menu && (!strcmp(menu->window.name, "saberMenu") || !strcmp(menu->window.name, "ingamespsaberMenu") || !strcmp(menu->window.name, "challengesaberMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "singlesaberhiltslist");
				if (item) {
					Cvar_Set("ui_saber", singleSaberHilts[saberHiltIndex].singleSaberHilt);
				}
			}

			// Player saber cheat menu
			if (menu && (!strcmp(menu->window.name, "ingamesabercustomizationMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "singlesaberhiltslist");
				if (item) {
					ui.Cmd_ExecuteText(EXEC_APPEND, va("saber %s\n", singleSaberHilts[saberHiltIndex].singleSaberHilt));
				}
			}

			// NPC saber cheat menu
			if (menu && (!strcmp(menu->window.name, "ingamespawncommandsMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "singlesaberhiltslist");
				if (item) {
					ui.Cmd_ExecuteText(EXEC_APPEND, va("workshop_set_sabersingle %s\n", singleSaberHilts[saberHiltIndex].singleSaberHilt));
					Cvar_Set("npc_recent_command", va("aiworkshop; cl_noprint 1; workshop_select; workshop_set_sabersingle %s; aiworkshop; wait 10; cl_noprint 0", singleSaberHilts[saberHiltIndex].singleSaberHilt));
				}
			}
		}
		else if (Q_stricmp(name, "md_assignsaber2") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			// These menus affect JKA character saber cvars
			if (menu && (!strcmp(menu->window.name, "saberMenu") || !strcmp(menu->window.name, "ingamespsaberMenu") || !strcmp(menu->window.name, "challengesaberMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "singlesaberhiltslist2");
				if (item) {
					Cvar_Set("ui_saber2", singleSaberHilts[saberHiltIndex].singleSaberHilt);
				}
			}

			// Play saber cheat menu
			if (menu && (!strcmp(menu->window.name, "ingamesabercustomizationMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "singlesaberhiltslist2");
				if (item) {
					ui.Cmd_ExecuteText(EXEC_APPEND, va("saber %s %s\n", singleSaberHilts[saberHiltIndex].singleSaberHilt, singleSaberHilts[saberHiltIndex].singleSaberHilt));
				}
			}

			// NPC saber cheat menu
			if (menu && (!strcmp(menu->window.name, "ingamespawncommandsMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "singlesaberhiltslist2");
				if (item) {
					ui.Cmd_ExecuteText(EXEC_APPEND, va("workshop_set_sabersecond none %s\n", singleSaberHilts[saberHiltIndex].singleSaberHilt));
					Cvar_Set("npc_recent_command", va("aiworkshop; cl_noprint 1; workshop_select; workshop_set_sabersecond none %s; aiworkshop; wait 10; cl_noprint 0", singleSaberHilts[saberHiltIndex].singleSaberHilt));
				}
			}
		}
		else if (Q_stricmp(name, "md_assignsaberstaff") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			// These menus affect JKA character saber cvars
			if (menu && (!strcmp(menu->window.name, "saberMenu") || !strcmp(menu->window.name, "ingamespsaberMenu") || !strcmp(menu->window.name, "challengesaberMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "staffsaberhiltslist");
				if (item && saberHiltIndex < NO_OF_STAFF_SABER_HILTS) {
					Cvar_Set("ui_saber", staffSaberHilts[saberHiltIndex].staffSaberHilt);
				}
			}

			// Player saber cheat menu
			if (menu && (!strcmp(menu->window.name, "ingamesabercustomizationMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "staffsaberhiltslist");
				if (item && saberHiltIndex < NO_OF_STAFF_SABER_HILTS) {
					ui.Cmd_ExecuteText(EXEC_APPEND, va("saber %s\n", staffSaberHilts[saberHiltIndex].staffSaberHilt));
				}
			}

			// NPC saber cheat menu
			if (menu && (!strcmp(menu->window.name, "ingamespawncommandsMenu")))
			{
				const auto item = Menu_FindItemByName(menu, "staffsaberhiltslist");
				if (item && saberHiltIndex < NO_OF_STAFF_SABER_HILTS) {
					ui.Cmd_ExecuteText(EXEC_APPEND, va("workshop_set_sabersingle %s\n", staffSaberHilts[saberHiltIndex].staffSaberHilt));
					Cvar_Set("npc_recent_command", va("aiworkshop; cl_noprint 1; workshop_select; workshop_set_sabersingle %s; aiworkshop; wait 10; cl_noprint 0", staffSaberHilts[saberHiltIndex].staffSaberHilt));
				}
			}
		}
		else if (Q_stricmp(name, "md_viewweaponwheel") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && !strcmp(menu->window.name, "ingameWeaponWheelMenu"))
			{
				UI_ViewWeaponWheel();
			}
		}
		else if (Q_stricmp(name, "md_viewforcewheel") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();
			if (menu && !strcmp(menu->window.name, "ingameForceWheelMenu"))
			{
				UI_ViewForceWheel();
			}
		}
		else if (Q_stricmp(name, "startgame") == 0)
		{
			Menus_CloseAll();
#ifdef JK2_MODE
			ui.Cmd_ExecuteText(EXEC_APPEND, "map kejim_post\n");
#else
			if (Cvar_VariableIntegerValue("com_demo"))
			{
				ui.Cmd_ExecuteText(EXEC_APPEND, "map demo\n");
			}
			else
			{
				ui.Cmd_ExecuteText(EXEC_APPEND, "map yavin1\n");
			}
#endif
		}
		else if (Q_stricmp(name, "startoutcast") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map kejim_post\n");
		}
		else if (Q_stricmp(name, "startdemo") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map jodemo\n");
		}
		else if (Q_stricmp(name, "startdarkforces") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map secbase\n");
		}
		else if (Q_stricmp(name, "startdarkforces2") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map 01nar\n");
		}
		else if (Q_stricmp(name, "startdeception") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map yavindec\n");
		}
		else if (Q_stricmp(name, "starthoth") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map imphoth_a\n");
		}
		else if (Q_stricmp(name, "startkessel") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map kessel\n");
		}
		else if (Q_stricmp(name, "starttournament") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map tournament_ruins\n");
		}
		else if (Q_stricmp(name, "startprivateer") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map intro\n");
		}
		else if (Q_stricmp(name, "startredemption") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map redemption1\n");
		}
		else if (Q_stricmp(name, "startrendar") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map drlevel1\n");
		}
		else if (Q_stricmp(name, "startvalley") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map sith_valley\n");
		}
		else if (Q_stricmp(name, "starttatooine") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map cot\n");
		}
		else if (Q_stricmp(name, "startvengance") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map part_1\n");
		}
		else if (Q_stricmp(name, "startpuzzle") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map puzzler\n");
		}
		else if (Q_stricmp(name, "startnina") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map nina01\n");
		}
		else if (Q_stricmp(name, "startyav") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText(EXEC_APPEND, "map level0\n");
		}
		else if (Q_stricmp(name, "startmap") == 0)
		{
			Menus_CloseAll();

			String_Parse(args, &mapName);

			ui.Cmd_ExecuteText(EXEC_APPEND, va("maptransition %s\n", mapName));
		}
		else if (Q_stricmp(name, "closeingame") == 0)
		{
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Cvar_Set("cl_paused", "0");
			Menus_CloseAll();

			if (1 == Cvar_VariableIntegerValue("ui_missionfailed"))
			{
				Menus_ActivateByName("missionfailed_menu");
				ui.Key_SetCatcher(KEYCATCH_UI);
			}
			else
			{
				Menus_ActivateByName("mainhud");
			}
		}
		else if (Q_stricmp(name, "closedatapad") == 0)
		{
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Cvar_Set("cl_paused", "0");
			Menus_CloseAll();
			Menus_ActivateByName("mainhud");

			Cvar_Set("cg_updatedDataPadForcePower1", "0");
			Cvar_Set("cg_updatedDataPadForcePower2", "0");
			Cvar_Set("cg_updatedDataPadForcePower3", "0");
			Cvar_Set("cg_updatedDataPadObjective", "0");
		}
		else if (Q_stricmp(name, "closesabermenu") == 0)
		{
			// if we're in the saber menu when creating a character, close this down
			if (!Cvar_VariableIntegerValue("saber_menu"))
			{
				Menus_CloseByName("saberMenu");
				Menus_OpenByName("characterMenu");
			}
		}
		else if (Q_stricmp(name, "clearmouseover") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();

			if (menu)
			{
				const char* itemName;
				String_Parse(args, &itemName);
				const auto item = Menu_FindItemByName(menu, itemName);
				if (item)
				{
					item->window.flags &= ~WINDOW_MOUSEOVER;
				}
			}
		}
		else if (Q_stricmp(name, "setMovesListDefault") == 0)
		{
			uiInfo.movesTitleIndex = 2;
		}
		else if (Q_stricmp(name, "resetMovesDesc") == 0)
		{
			const menuDef_t* menu = Menu_GetFocused();

			if (menu)
			{
				const auto item = Menu_FindItemByName(menu, "item_desc");
				if (item)
				{
					const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
					if (listPtr)
					{
						listPtr->cursorPos = 0;
						listPtr->startPos = 0;
					}
					item->cursorPos = 0;
				}
			}
		}
		else if (Q_stricmp(name, "resetMovesList") == 0)
		{
			const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");
			//update saber models
			if (menu)
			{
				const auto item = Menu_FindItemByName(menu, "character");
				if (item)
				{
					UI_SaberAttachToChar(item);
				}
			}

			Cvar_Set("ui_move_desc", " ");
		}
		else if (Q_stricmp(name, "setMoveCharacter") == 0)
		{
			UI_GetCharacterCvars();
			UI_GetSaberCvars();

			uiInfo.movesTitleIndex = 0;

			const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");

			if (menu)
			{
				const auto item = Menu_FindItemByName(menu, "character");
				if (item)
				{
					const modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);
					if (modelPtr)
					{
						char skin[MAX_QPATH];
						uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
						ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);

						uiInfo.moveAnimTime = 0;
						DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue);
						Com_sprintf(skin, sizeof skin, "models/players/%s/|%s|%s|%s",
							Cvar_VariableString("g_char_model"),
							Cvar_VariableString("g_char_skin_head"),
							Cvar_VariableString("g_char_skin_torso"),
							Cvar_VariableString("g_char_skin_legs")
						);

						ItemParse_model_g2skin_go(item, skin);
						UI_SaberAttachToChar(item);
					}
				}
			}
		}
		else if (Q_stricmp(name, "glCustom") == 0)
		{
			Cvar_Set("ui_r_glCustom", "4");
		}
		else if (Q_stricmp(name, "character") == 0)
		{
#ifdef NEW_FEEDER_V7
			UI_UpdateCharacter(qfalse, qfalse);
#else
			UI_UpdateCharacter(qfalse);
#endif
		}
		else if (Q_stricmp(name, "characterchanged") == 0)
		{
#ifdef NEW_FEEDER_V7
			UI_UpdateCharacter(qtrue, qfalse);
#else
			UI_UpdateCharacter(qtrue);
#endif
		}
		else if (Q_stricmp(name, "char_skin") == 0)
		{
			UI_UpdateCharacterSkin();
		}
		else if (Q_stricmp(name, "saber_type") == 0)
		{
			UI_UpdateSaberType();
		}
		else if (Q_stricmp(name, "saber_hilt") == 0)
		{
			UI_UpdateSaberHilt(qfalse);
		}
		else if (Q_stricmp(name, "saber_color") == 0)
		{
			//			UI_UpdateSaberColor( qfalse );
		}
		else if (Q_stricmp(name, "saber2_hilt") == 0)
		{
			UI_UpdateSaberHilt(qtrue);
		}
		else if (Q_stricmp(name, "saber2_color") == 0)
		{
			//			UI_UpdateSaberColor( qtrue );
		}
		else if (Q_stricmp(name, "updatecharcvars") == 0)
		{
			UI_UpdateCharacterCvars();
		}
		else if (Q_stricmp(name, "getcharcvars") == 0)
		{
			UI_GetCharacterCvars();
		}
		else if (Q_stricmp(name, "updatesabercvars") == 0)
		{
			UI_UpdateSaberCvars();
		}
		else if (Q_stricmp(name, "getsabercvars") == 0)
		{
			UI_GetSaberCvars();
		}
		else if (Q_stricmp(name, "resetsabercvardefaults") == 0)
		{
			// NOTE : ONLY do this if saber menu is set properly (ie. first time we enter this menu)
			if (!Cvar_VariableIntegerValue("saber_menu"))
			{
				UI_ResetSaberCvars();
			}
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "updatefightingstylechoices") == 0)
		{
			UI_UpdateFightingStyleChoices();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "initallocforcepower") == 0)
		{
			const char* forceName;
			String_Parse(args, &forceName);

			UI_InitAllocForcePowers(forceName);
		}
		else if (Q_stricmp(name, "affectforcepowerlevel") == 0)
		{
			const char* forceName;
			String_Parse(args, &forceName);

			UI_AffectForcePowerLevel(forceName);
		}
		else if (Q_stricmp(name, "decrementcurrentforcepower") == 0)
		{
			UI_DecrementCurrentForcePower();
		}
		else if (Q_stricmp(name, "shutdownforcehelp") == 0)
		{
			UI_ShutdownForceHelp();
		}
		else if (Q_stricmp(name, "forcehelpactive") == 0)
		{
			UI_ForceHelpActive();
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "demosetforcelevels") == 0)
		{
			UI_DemoSetForceLevels();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "recordforcelevels") == 0)
		{
			UI_RecordForceLevels();
		}
		else if (Q_stricmp(name, "recordweapons") == 0)
		{
			UI_RecordWeapons();
		}
		else if (Q_stricmp(name, "showforceleveldesc") == 0)
		{
			const char* forceName;
			String_Parse(args, &forceName);

			UI_ShowForceLevelDesc(forceName);
		}
		else if (Q_stricmp(name, "resetforcelevels") == 0)
		{
			UI_ResetForceLevels();
		}
		else if (Q_stricmp(name, "weaponhelpactive") == 0)
		{
			UI_WeaponHelpActive();
		}
		// initialize weapon selection screen
		else if (Q_stricmp(name, "initweaponselect") == 0)
		{
			UI_InitWeaponSelect();
		}
		else if (Q_stricmp(name, "clearweapons") == 0)
		{
			UI_ClearWeapons();
		}
		else if (Q_stricmp(name, "clearsabers") == 0)
		{
			UI_clearsabers();
		}
		else if (Q_stricmp(name, "stopgamesounds") == 0)
		{
			trap_S_StopSounds();
		}
		else if (Q_stricmp(name, "loadmissionselectmenu") == 0)
		{
			const char* cvarName;
			String_Parse(args, &cvarName);

			if (cvarName)
			{
				UI_LoadMissionSelectMenu(cvarName);
			}
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "calcforcestatus") == 0)
		{
			UI_CalcForceStatus();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "giveweapon") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			UI_GiveWeapon(atoi(weaponIndex));
		}
		else if (Q_stricmp(name, "giveammo") == 0)
		{
			const char* ammoIndex;
			String_Parse(args, &ammoIndex);

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);

			const char* soundfile = nullptr;
			String_Parse(args, &soundfile);

			UI_GiveAmmo(atoi(ammoIndex), atoi(ammoAmount), soundfile);
		}
		else if (Q_stricmp(name, "equipweapon") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			UI_EquipWeapon(atoi(weaponIndex));
		}
		else if (Q_stricmp(name, "addweaponselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char* ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char* itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char* litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char* backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char* soundfile = nullptr;
			String_Parse(args, &soundfile);

			UI_AddWeaponSelection(atoi(weaponIndex), atoi(ammoIndex), atoi(ammoAmount), itemName, litItemName,
				backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "addpistolselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char* ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char* itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char* litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char* backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char* soundfile = nullptr;
			String_Parse(args, &soundfile);

			UI_AddPistolSelection(atoi(weaponIndex), atoi(ammoIndex), atoi(ammoAmount), itemName, litItemName,
				backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "addthrowweaponselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char* ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char* ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char* itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char* litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char* backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char* soundfile;
			String_Parse(args, &soundfile);

			UI_AddThrowWeaponSelection(atoi(weaponIndex), atoi(ammoIndex), atoi(ammoAmount), itemName, litItemName,
				backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "removeweaponselection") == 0)
		{
			const char* weaponIndex;
			String_Parse(args, &weaponIndex);
			if (weaponIndex)
			{
				UI_RemoveWeaponSelection(atoi(weaponIndex));
			}
		}
		else if (Q_stricmp(name, "removepistolselection") == 0)
		{
			UI_Removepistolselection();
		}
		else if (Q_stricmp(name, "removethrowweaponselection") == 0)
		{
			UI_RemoveThrowWeaponSelection();
		}
		else if (Q_stricmp(name, "normalthrowselection") == 0)
		{
			UI_NormalThrowSelection();
		}
		else if (Q_stricmp(name, "highlightthrowselection") == 0)
		{
			UI_HighLightThrowSelection();
		}
		else if (Q_stricmp(name, "normalweaponselection") == 0)
		{
			const char* slotIndex;
			String_Parse(args, &slotIndex);
			if (!slotIndex)
			{
				return qfalse;
			}

			UI_NormalWeaponSelection(atoi(slotIndex));
		}
		else if (Q_stricmp(name, "normalpistolselection") == 0)
		{
			UI_Normalpistolselection();
		}
		else if (Q_stricmp(name, "highlightweaponselection") == 0)
		{
			const char* slotIndex;
			String_Parse(args, &slotIndex);
			if (!slotIndex)
			{
				return qfalse;
			}

			UI_HighLightWeaponSelection(atoi(slotIndex));
		}
		else if (Q_stricmp(name, "highlightpistolselection") == 0)
		{
			UI_Highlightpistolselection();
		}
		else if (Q_stricmp(name, "clearinventory") == 0)
		{
			UI_ClearInventory();
		}
		else if (Q_stricmp(name, "giveinventory") == 0)
		{
			const char* inventoryIndex, * amount;
			String_Parse(args, &inventoryIndex);
			String_Parse(args, &amount);
			UI_GiveInventory(atoi(inventoryIndex), atoi(amount));
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "updatefightingstyle") == 0)
		{
			UI_UpdateFightingStyle();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "update") == 0)
		{
			if (String_Parse(args, &name2))
			{
				UI_Update(name2);
			}
			else
			{
				Com_Printf("update missing cmd\n");
			}
		}
		else if (Q_stricmp(name, "load_quick") == 0)
		{
#ifdef JK2_MODE
			ui.Cmd_ExecuteText(EXEC_APPEND, "load quik\n");
#else
			ui.Cmd_ExecuteText(EXEC_APPEND, "load quick\n");
#endif
		}
		else if (Q_stricmp(name, "load_auto") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND, "load *respawn\n");
			//death menu, might load a saved game instead if they just loaded on this map
		}
		else if (Q_stricmp(name, "decrementforcepowerlevel") == 0)
		{
			UI_DecrementForcePowerLevel();
		}
		else if (Q_stricmp(name, "getmousepitch") == 0)
		{
			Cvar_Set("ui_mousePitch", trap_Cvar_VariableValue("m_pitch") >= 0 ? "0" : "1");
		}
		else if (Q_stricmp(name, "resetcharacterlistboxes") == 0)
		{
			UI_ResetCharacterListBoxes();
		}
		else if (Q_stricmp(name, "LaunchMP") == 0)
		{
			// TODO for MAC_PORT, will only be valid for non-JK2 mode
		}
		else
		{
			Com_Printf("unknown UI script %s\n", name);
		}
	}

	return qtrue;
}

/*
=================
UI_GetValue
=================
*/
static float UI_GetValue(int ownerDraw)
{
	return 0;
}

//Force Warnings
enum
{
	FW_VERY_LIGHT = 0,
	FW_SEMI_LIGHT,
	FW_NEUTRAL,
	FW_SEMI_DARK,
	FW_VERY_DARK
};

const char* lukeForceStatusSounds[] =
{
	"sound/chars/luke/misc/MLUK_03.mp3", // Very Light
	"sound/chars/luke/misc/MLUK_04.mp3", // Semi Light
	"sound/chars/luke/misc/MLUK_05.mp3", // Neutral
	"sound/chars/luke/misc/MLUK_01.mp3", // Semi dark
	"sound/chars/luke/misc/MLUK_02.mp3", // Very dark
};

const char* kyleForceStatusSounds[] =
{
	"sound/chars/kyle/misc/MKYK_05.mp3", // Very Light
	"sound/chars/kyle/misc/MKYK_04.mp3", // Semi Light
	"sound/chars/kyle/misc/MKYK_03.mp3", // Neutral
	"sound/chars/kyle/misc/MKYK_01.mp3", // Semi dark
	"sound/chars/kyle/misc/MKYK_02.mp3", // Very dark
};

#ifndef JK2_MODE
static void UI_CalcForceStatus()
{
	short index;
	qboolean lukeFlag = qtrue;
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	char value[256];

	if (!cl)
	{
		return;
	}
	const playerState_t* pState = cl->gentity->client;

	if (!cl->gentity || !cl->gentity->client)
	{
		return;
	}

	memset(value, 0, sizeof value);

	const float lightSide = pState->forcePowerLevel[FP_HEAL] +
		pState->forcePowerLevel[FP_TELEPATHY] +
		pState->forcePowerLevel[FP_PROTECT] +
		pState->forcePowerLevel[FP_ABSORB] +
		pState->forcePowerLevel[FP_STASIS] +
		pState->forcePowerLevel[FP_GRASP] +
		pState->forcePowerLevel[FP_REPULSE] +
		pState->forcePowerLevel[FP_PROJECTION] +
		pState->forcePowerLevel[FP_BLAST];

	const float darkSide = pState->forcePowerLevel[FP_GRIP] +
		pState->forcePowerLevel[FP_LIGHTNING] +
		pState->forcePowerLevel[FP_RAGE] +
		pState->forcePowerLevel[FP_DRAIN] +
		pState->forcePowerLevel[FP_DESTRUCTION] +
		pState->forcePowerLevel[FP_LIGHTNING_STRIKE] +
		pState->forcePowerLevel[FP_FEAR] +
		pState->forcePowerLevel[FP_DEADLYSIGHT];

	const float total = lightSide + darkSide;

	const float percent = lightSide / total;

	const short who = Q_irand(0, 100);
	if (percent >= 0.90f) //  90 - 100%
	{
		index = FW_VERY_LIGHT;
		if (who < 50)
		{
			strcpy(value, "vlk"); // Very light Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "vll"); // Very light Luke
		}
	}
	else if (percent > 0.60f)
	{
		index = FW_SEMI_LIGHT;
		if (who < 50)
		{
			strcpy(value, "slk"); // Semi-light Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "sll"); // Semi-light light Luke
		}
	}
	else if (percent > 0.40f)
	{
		index = FW_NEUTRAL;
		if (who < 50)
		{
			strcpy(value, "ntk"); // Neutral Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "ntl"); // Netural Luke
		}
	}
	else if (percent > 0.10f)
	{
		index = FW_SEMI_DARK;
		if (who < 50)
		{
			strcpy(value, "sdk"); // Semi-dark Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "sdl"); // Semi-Dark Luke
		}
	}
	else
	{
		index = FW_VERY_DARK;
		if (who < 50)
		{
			strcpy(value, "vdk"); // Very dark Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value, "vdl"); // Very Dark Luke
		}
	}

	Cvar_Set("ui_forcestatus", value);

	if (lukeFlag)
	{
		DC->startLocalSound(DC->registerSound(lukeForceStatusSounds[index], qfalse), CHAN_VOICE);
	}
	else
	{
		DC->startLocalSound(DC->registerSound(kyleForceStatusSounds[index], qfalse), CHAN_VOICE);
	}
}
#endif // !JK2_MODE

/*
=================
UI_StopCinematic
=================
*/
static void UI_StopCinematic(int handle)
{
	if (handle >= 0)
	{
		trap_CIN_StopCinematic(handle);
	}
	else
	{
		handle = abs(handle);
		if (handle == UI_MAPCINEMATIC)
		{
			// FIXME - BOB do we need this?
			//			if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0)
			//			{
			//				trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
			//				uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
			//			}
		}
		else if (handle == UI_NETMAPCINEMATIC)
		{
			// FIXME - BOB do we need this?
			//			if (uiInfo.serverStatus.currentServerCinematic >= 0)
			//			{
			//				trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
			//				uiInfo.serverStatus.currentServerCinematic = -1;
			//			}
		}
		else if (handle == UI_CLANCINEMATIC)
		{
			// FIXME - BOB do we need this?
			//			int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
			//			if (i >= 0 && i < uiInfo.teamCount)
			//			{
			//				if (uiInfo.teamList[i].cinematic >= 0)
			//				{
			//					trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
			//					uiInfo.teamList[i].cinematic = -1;
			//				}
			//			}
		}
	}
}

static void UI_HandleLoadSelection()
{
	Cvar_Set("ui_SelectionOK", va("%d", s_savegame.currentLine < s_savegame.saveFileCnt));
	if (s_savegame.currentLine >= s_savegame.saveFileCnt)
		return;
#ifdef JK2_MODE
	Cvar_Set("ui_gameDesc", s_savedata[s_savegame.currentLine].currentSaveFileComments);	// set comment

	if (!ui.SG_GetSaveImage(s_savedata[s_savegame.currentLine].currentSaveFileName, &screenShotBuf))
	{
		memset(screenShotBuf, 0, (SG_SCR_WIDTH * SG_SCR_HEIGHT * 4));
	}
#endif
}

/*
=================
UI_FeederCount
=================
*/
static int UI_FeederCount(const float feederID)
{
	if (feederID == FEEDER_SAVEGAMES)
	{
		if (s_savegame.saveFileCnt == -1)
		{
			ReadSaveDirectory(); //refresh
			UI_HandleLoadSelection();
#ifndef JK2_MODE
			UI_AdjustSaveGameListBox(s_savegame.currentLine);
#endif
		}
		return s_savegame.saveFileCnt;
	}
	// count number of moves for the current title
	if (feederID == FEEDER_MOVES)
	{
		int count = 0;

		for (int i = 0; i < MAX_MOVES; i++)
		{
			if (datapadMoveData[uiInfo.movesTitleIndex][i].title)
			{
				count++;
			}
		}

		return count;
	}
	if (feederID == FEEDER_MOVES_TITLES)
	{
		return MD_MOVE_TITLE_MAX;
	}
	if (feederID == FEEDER_MODS)
	{
		return uiInfo.modCount;
	}
	if (feederID == FEEDER_LANGUAGES)
	{
		return uiInfo.languageCount;
	}
	if (feederID == FEEDER_PLAYER_SPECIES)
	{
		return uiInfo.playerSpeciesCount;
	}
	if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount;
	}
	if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount;
	}
	if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount;
	}
	if (feederID == FEEDER_COLORCHOICES)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount;
	}
#ifdef NEW_FEEDER
	if (feederID == FEEDER_MD_FACTION) 
	{
		return TOTAL_ERAS;
	}
	if (feederID == FEEDER_MD_MODELS)
	{
		int finalCount = eraIndex[uiEra + 1] - eraIndex[uiEra];
		int count = eraIndex[uiEra + 1] - eraIndex[uiEra];

		for (int i = 0; i < count; i++) {
			if (charMD[eraIndex[uiEra] + i].isSubDiv)
				finalCount--;
		}
		return finalCount;
	}
	if (feederID == FEEDER_MD_VARIANTS)
	{
		for (int i = 1; i < NO_OF_MD_MODELS; i++) {
			if (!charMD[uiModelIndex + i].isSubDiv)
				return i;
		}
		return 1; // Should not happen...
	}
#endif
	if (feederID == FEEDER_DUEL_MAPS)
	{
		return NO_OF_DUEL_MAPS;
	}
	if (feederID == FEEDER_SINGLE_SABER_HILTS)
	{
		return NO_OF_SINGLE_SABER_HILTS;
	}
	if (feederID == FEEDER_STAFF_SABER_HILTS)
	{
		return NO_OF_STAFF_SABER_HILTS;
	}

	return 0;
}

/*
=================
UI_FeederSelection
=================
*/
#ifdef NEW_FEEDER
static void UI_FeederSelection(const float feederID, int index, itemDef_t* item)
#else
static void UI_FeederSelection(const float feederID, const int index, itemDef_t* item)
#endif
{
	if (feederID == FEEDER_SAVEGAMES)
	{
		s_savegame.currentLine = index;
		UI_HandleLoadSelection();
	}
	else if (feederID == FEEDER_MOVES)
	{
		const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");

		if (menu)
		{
			const auto item_def_s = Menu_FindItemByName(menu, "character");
			if (item_def_s)
			{
				const modelDef_t* modelPtr = static_cast<modelDef_t*>(item_def_s->typeData);
				if (modelPtr)
				{
					if (datapadMoveData[uiInfo.movesTitleIndex][index].anim)
					{
						char skin[MAX_QPATH];
						ItemParse_model_g2anim_go(item_def_s, datapadMoveData[uiInfo.movesTitleIndex][index].anim);
						uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item_def_s->ghoul2[0], "model_root",
							modelPtr->g2anim, qtrue);

						uiInfo.moveAnimTime += uiInfo.uiDC.realTime;

						// Play sound for anim
						if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_FORCE_JUMP)
						{
							DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveJumpSound, CHAN_LOCAL);
						}
						else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_ROLL)
						{
							DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveRollSound, CHAN_LOCAL);
						}
						else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_SABER)
						{
							// Randomly choose one sound
							const int soundI = Q_irand(1, 6);
							const sfxHandle_t* soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound1;
							if (soundI == 2)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound2;
							}
							else if (soundI == 3)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound3;
							}
							else if (soundI == 4)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound4;
							}
							else if (soundI == 5)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound5;
							}
							else if (soundI == 6)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound6;
							}

							DC->startLocalSound(*soundPtr, CHAN_LOCAL);
						}

						if (datapadMoveData[uiInfo.movesTitleIndex][index].desc)
						{
							Cvar_Set("ui_move_desc", datapadMoveData[uiInfo.movesTitleIndex][index].desc);
						}

						Com_sprintf(skin, sizeof skin, "models/players/%s/|%s|%s|%s",
							Cvar_VariableString("g_char_model"),
							Cvar_VariableString("g_char_skin_head"),
							Cvar_VariableString("g_char_skin_torso"),
							Cvar_VariableString("g_char_skin_legs")
						);

						ItemParse_model_g2skin_go(item_def_s, skin);
					}
				}
			}
		}
	}
	else if (feederID == FEEDER_MOVES_TITLES)
	{
		uiInfo.movesTitleIndex = index;
		uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
		const menuDef_t* menu = Menus_FindByName("datapadMovesMenu");

		if (menu)
		{
			const auto item_def_s = Menu_FindItemByName(menu, "character");
			if (item_def_s)
			{
				const modelDef_t* modelPtr = static_cast<modelDef_t*>(item_def_s->typeData);
				if (modelPtr)
				{
					ItemParse_model_g2anim_go(item_def_s, uiInfo.movesBaseAnim);
					uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item_def_s->ghoul2[0], "model_root", modelPtr->g2anim,
						qtrue);
				}
			}
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		uiInfo.modIndex = index;
	}
	else if (feederID == FEEDER_PLAYER_SPECIES)
	{
		if (index >= 0 && index < uiInfo.playerSpeciesCount)
		{
			uiInfo.playerSpeciesIndex = index;
		}
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
		uiInfo.languageCountIndex = index;
	}
	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name);
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			Cvar_Set("ui_char_skin_torso", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name);
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			Cvar_Set("ui_char_skin_legs", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name);
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		extern void Item_RunScript(itemDef_t * item_def_item, const char* s); //from ui_shared;
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			Item_RunScript(item, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].actionText);
		}
	}
	/*	else if (feederID == FEEDER_CINEMATICS)
		{
			uiInfo.movieIndex = index;
			if (uiInfo.previewMovie >= 0)
			{
				trap_CIN_StopCinematic(uiInfo.previewMovie);
			}
			uiInfo.previewMovie = -1;
		}
		else if (feederID == FEEDER_DEMOS)
		{
			uiInfo.demoIndex = index;
		}
	*/
#ifdef NEW_FEEDER
	else if (feederID == FEEDER_MD_FACTION) 
	{ 
		uiEra = index; 
		return;
	} 
	else if ((feederID == FEEDER_MD_MODELS) || (feederID == FEEDER_MD_VARIANTS))
	{
		int actual = 0;
#ifdef NEW_FEEDER_V2
		menuDef_t* menu = Menus_FindByName("ingamecharacter");
#endif
		if (feederID == FEEDER_MD_MODELS)
			MD_UI_SelectedTeamHead_SubDivs(index, &actual);
		else
			MD_UI_SelectedTeamHead(index, &actual);

		if (feederID == FEEDER_MD_VARIANTS) {
			index = uiModelIndex + index;
			uiVariantIndex = index;
		}
		if (feederID != FEEDER_MD_VARIANTS) {
			uiModelIndex = actual;
			uiVariantIndex = actual;
			index = actual;
#ifdef NEW_FEEDER_V1
			if (menu) {
				itemDef_t* itemFeeder = Menu_FindItemByName(menu, "variantlist");
				listBoxDef_t* listPtr = static_cast<listBoxDef_t*>(itemFeeder->typeData);
				if (listPtr) {
					listPtr->cursorPos = 0;
				}
				itemFeeder->cursorPos = 0;
			}
#endif
		}

#ifdef NEW_FEEDER_V2
		if (menu) {
			itemDef_t* itemDesc = Menu_FindItemByName(menu, "char_desc");
			if (itemDesc) {
				itemDesc->text = (char*)charMD[uiVariantIndex].desc;
			}
		}
#endif

		if (index >= eraIndex[uiEra] && index < eraIndex[uiEra + 1]) {
			if (Q_strncmp(charMD[index].skin, "model_", 6) == 0)
				Cvar_Set("ui_model", va("%s/%s", charMD[index].model, &charMD[index].skin[6])); //standard model // strip model_
			else
				Cvar_Set("ui_model", va("%s/%s", charMD[index].model, charMD[index].skin));

			Cvar_Set("ui_char_model", charMD[index].model);//UI_Cvar_VariableString("ui_model"));
			Cvar_Set("ui_char_skin", charMD[index].skin);//
		}
	} 
#endif

	else if (feederID == FEEDER_DUEL_MAPS)
	{
		// Don't change the index if heading/empty space is selected
		if (duelMaps[index].duelMapBSP != "") {
			duelMapIndex = index;
		}
	}
	else if (feederID == FEEDER_SINGLE_SABER_HILTS)
	{
		if (singleSaberHilts[index].singleSaberHilt != "") {
			saberHiltIndex = index;
			DC->startLocalSound(DC->registerSound("sound/interface/choose_hilt.wav", qfalse), CHAN_LOCAL);
		}
	}
	else if (feederID == FEEDER_STAFF_SABER_HILTS)
	{
		if (staffSaberHilts[index].staffSaberHilt != "") {
			saberHiltIndex = index;
			DC->startLocalSound(DC->registerSound("sound/interface/choose_hilt.wav", qfalse), CHAN_LOCAL);
		}
	}
}

void Key_KeynumToStringBuf(int keynum, char* buf, int buflen);
void Key_GetBindingBuf(int keynum, char* buf, int buflen);

static qboolean UI_Crosshair_HandleKey(int flags, float* special, const int key)
{
	if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER)
	{
		if (key == A_MOUSE2)
		{
			uiInfo.currentCrosshair--;
		}
		else
		{
			uiInfo.currentCrosshair++;
		}

		if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS)
		{
			uiInfo.currentCrosshair = 0;
		}
		else if (uiInfo.currentCrosshair < 0)
		{
			uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
		}
		Cvar_Set("cg_drawCrosshair", va("%d", uiInfo.currentCrosshair));
		return qtrue;
	}
	return qfalse;
}

static qboolean UI_OwnerDrawHandleKey(const int ownerDraw, const int flags, float* special, const int key)
{
	switch (ownerDraw)
	{
	case UI_CROSSHAIR:
		UI_Crosshair_HandleKey(flags, special, key);
		break;
	default:
		break;
	}

	return qfalse;
}

//unfortunately we cannot rely on any game/cgame module code to do our animation stuff,
//because the ui can be loaded while the game/cgame are not loaded. So we're going to recreate what we need here.
#undef MAX_ANIM_FILES
#define MAX_ANIM_FILES 32

class ui_animFileSet_t
{
public:
	char filename[MAX_QPATH];
	animation_t animations[MAX_ANIMATIONS];
}; // ui_animFileSet_t
static ui_animFileSet_t ui_knownAnimFileSets[MAX_ANIM_FILES];

int ui_numKnownAnimFileSets;

static qboolean UI_ParseAnimationFile(const char* af_filename)
{
	const char* text_p;
	char text[80000];
	animation_t* animations = ui_knownAnimFileSets[ui_numKnownAnimFileSets].animations;

	const int len = re.GetAnimationCFG(af_filename, text, sizeof text);
	if (len <= 0)
	{
		return qfalse;
	}
	if (len >= static_cast<int>(sizeof text - 1))
	{
		Com_Error(ERR_FATAL, "UI_ParseAnimationFile: File %s too long\n (%d > %d)", af_filename, len, sizeof text - 1);
	}

	// parse the text
	text_p = text;

	//FIXME: have some way of playing anims backwards... negative numFrames?

	//initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
	for (int i = 0; i < MAX_ANIMATIONS; i++)
	{
		animations[i].firstFrame = 0;
		animations[i].numFrames = 0;
		animations[i].loopFrames = -1;
		animations[i].frameLerp = 100;
		//		animations[i].initialLerp = 100;
	}

	// read information for each frame
	COM_BeginParseSession();
	while (true)
	{
		const char* token = COM_Parse(&text_p);

		if (!token || !token[0])
		{
			break;
		}

		const int animNum = GetIDForString(anim_table, token);
		if (animNum == -1)
		{
			//#ifndef FINAL_BUILD
#ifdef _DEBUG
			if (strcmp(token, "ROOT"))
			{
				Com_Printf(S_COLOR_RED"WARNING: Unknown token %s in %s\n", token, af_filename);
			}
#endif
			while (token[0])
			{
				token = COM_ParseExt(&text_p, qfalse); //returns empty string when next token is EOL
			}
			continue;
		}

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		animations[animNum].firstFrame = atoi(token);

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		animations[animNum].numFrames = atoi(token);

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		animations[animNum].loopFrames = atoi(token);

		token = COM_Parse(&text_p);
		if (!token)
		{
			break;
		}
		float fps = atof(token);
		if (fps == 0)
		{
			fps = 1; //Don't allow divide by zero error
		}
		if (fps < 0)
		{
			//backwards
			animations[animNum].frameLerp = floor(1000.0f / fps);
		}
		else
		{
			animations[animNum].frameLerp = ceil(1000.0f / fps);
		}

		//		animations[animNum].initialLerp = ceil(1000.0f / fabs(fps));
	}
	COM_EndParseSession();

	return qtrue;
}

static qboolean UI_ParseAnimFileSet(const char* animCFG, int* animFileIndex)
{
	//Not going to bother parsing the sound config here.
	char afilename[MAX_QPATH];
	char strippedName[MAX_QPATH];
	int i;

	Q_strncpyz(strippedName, animCFG, sizeof strippedName);
	char* slash = strrchr(strippedName, '/');
	if (slash)
	{
		// truncate modelName to find just the dir not the extension
		*slash = 0;
	}

	//if this anims file was loaded before, don't parse it again, just point to the correct table of info
	for (i = 0; i < ui_numKnownAnimFileSets; i++)
	{
		if (Q_stricmp(ui_knownAnimFileSets[i].filename, strippedName) == 0)
		{
			*animFileIndex = i;
			return qtrue;
		}
	}

	if (ui_numKnownAnimFileSets == MAX_ANIM_FILES)
	{
		//TOO MANY!
		for (i = 0; i < MAX_ANIM_FILES; i++)
		{
			Com_Printf("animfile[%d]: %s\n", i, ui_knownAnimFileSets[i].filename);
		}
		Com_Error(ERR_FATAL, "UI_ParseAnimFileSet: %d == MAX_ANIM_FILES == %d", ui_numKnownAnimFileSets,
			MAX_ANIM_FILES);
	}

	//Okay, time to parse in a new one
	Q_strncpyz(ui_knownAnimFileSets[ui_numKnownAnimFileSets].filename, strippedName,
		sizeof ui_knownAnimFileSets[ui_numKnownAnimFileSets].filename);

	// Load and parse animations.cfg file
	Com_sprintf(afilename, sizeof afilename, "%s/animation.cfg", strippedName);
	if (!UI_ParseAnimationFile(afilename))
	{
		*animFileIndex = -1;
		return qfalse;
	}

	//set index and increment
	*animFileIndex = ui_numKnownAnimFileSets++;

	return qtrue;
}

static int UI_G2SetAnim(CGhoul2Info* ghlInfo, const char* boneName, const int animNum, const qboolean freeze)
{
	int animIndex;

	const char* GLAName = re.G2API_GetGLAName(ghlInfo);

	if (!GLAName || !GLAName[0])
	{
		return 0;
	}

	UI_ParseAnimFileSet(GLAName, &animIndex);

	if (animIndex != -1)
	{
		const animation_t* anim = &ui_knownAnimFileSets[animIndex].animations[animNum];
		if (anim->numFrames <= 0)
		{
			return 0;
		}
		const int sFrame = anim->firstFrame;
		const int eFrame = anim->firstFrame + anim->numFrames;
		int flags = BONE_ANIM_OVERRIDE;
		const int time = uiInfo.uiDC.realTime;
		const float animSpeed = 50.0f / anim->frameLerp;

		// Freeze anim if it's not looping, special hack for datapad moves menu
		if (freeze)
		{
			if (anim->loopFrames == -1)
			{
				flags = BONE_ANIM_OVERRIDE_FREEZE;
			}
			else
			{
				flags = BONE_ANIM_OVERRIDE_LOOP;
			}
		}
		else if (anim->loopFrames != -1)
		{
			flags = BONE_ANIM_OVERRIDE_LOOP;
		}
		flags |= BONE_ANIM_BLEND;
		constexpr int blend_time = 150;

		re.G2API_SetBoneAnim(ghlInfo, boneName, sFrame, eFrame, flags, animSpeed, time, -1, blend_time);

		return anim->frameLerp * (anim->numFrames - 2);
	}

	return 0;
}

static qboolean UI_ParseColorData(const char* buf, playerSpeciesInfo_t& species)
{
	const char* p;

	p = buf;
	COM_BeginParseSession();
	species.ColorCount = 0;
	species.ColorMax = 16;
	species.Color = static_cast<playerColor_t*>(malloc(species.ColorMax * sizeof(playerColor_t)));

	while (p)
	{
		const char* token = COM_ParseExt(&p, qtrue); //looking for the shader
		if (token[0] == 0)
		{
			COM_EndParseSession();
			return static_cast<qboolean>(species.ColorCount != 0);
		}

		if (species.ColorCount >= species.ColorMax)
		{
			species.ColorMax *= 2;
			species.Color = static_cast<playerColor_t*>(
				realloc(species.Color, species.ColorMax * sizeof(playerColor_t)));
		}

		memset(&species.Color[species.ColorCount], 0, sizeof(playerColor_t));

		Q_strncpyz(species.Color[species.ColorCount].shader, token, MAX_QPATH);

		token = COM_ParseExt(&p, qtrue); //looking for action block {
		if (token[0] != '{')
		{
			COM_EndParseSession();
			return qfalse;
		}

		token = COM_ParseExt(&p, qtrue); //looking for action commands
		while (token[0] != '}')
		{
			if (token[0] == 0)
			{
				//EOF
				COM_EndParseSession();
				return qfalse;
			}
			Q_strcat(species.Color[species.ColorCount].actionText, ACTION_BUFFER_SIZE, token);
			Q_strcat(species.Color[species.ColorCount].actionText, ACTION_BUFFER_SIZE, " ");
			token = COM_ParseExt(&p, qtrue); //looking for action commands or final }
		}
		species.ColorCount++; //next color please
	}
	COM_EndParseSession();
	return qtrue; //never get here
}

/*
=================
bIsImageFile
builds path and scans for valid image extentions
=================
*/
static qboolean IsImageFile(const char* dirptr, const char* skinname, const qboolean building)
{
	char fpath[MAX_QPATH];
	int f;

	Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.jpg", dirptr, skinname);
	ui.FS_FOpenFile(fpath, &f, FS_READ);
	if (!f)
	{
		//not there, try png
		Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.png", dirptr, skinname);
		ui.FS_FOpenFile(fpath, &f, FS_READ);
	}
	if (!f)
	{
		//not there, try tga
		Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.tga", dirptr, skinname);
		ui.FS_FOpenFile(fpath, &f, FS_READ);
	}
	if (f)
	{
		ui.FS_FCloseFile(f);
		if (building) ui.R_RegisterShaderNoMip(fpath);
		return qtrue;
	}

	return qfalse;
}

static void UI_FreeSpecies(playerSpeciesInfo_t* species)
{
	free(species->SkinHead);
	free(species->SkinTorso);
	free(species->SkinLeg);
	free(species->Color);
	memset(species, 0, sizeof(playerSpeciesInfo_t));
}

static void UI_FreeAllSpecies()
{
	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		UI_FreeSpecies(&uiInfo.playerSpecies[i]);
	}
	free(uiInfo.playerSpecies);

	uiInfo.playerSpeciesCount = 0;
	uiInfo.playerSpecies = nullptr;
}

/*
=================
PlayerModel_BuildList
=================
*/
static void UI_BuildPlayerModel_List(const qboolean inGameLoad)
{
	static constexpr size_t DIR_LIST_SIZE = 16384;

	size_t dirListSize = DIR_LIST_SIZE;
	char stackDirList[8192]{};
	int dirlen = 0;
	const int building = Cvar_VariableIntegerValue("com_buildscript");

	auto dirlist = static_cast<char*>(malloc(DIR_LIST_SIZE));
	if (!dirlist)
	{
		Com_Printf(S_COLOR_YELLOW "WARNING: Failed to allocate %u bytes of memory for player model "
			"directory list. Using stack allocated buffer of %u bytes instead.",
			DIR_LIST_SIZE, sizeof stackDirList);

		dirlist = stackDirList;
		dirListSize = sizeof stackDirList;
	}

	uiInfo.playerSpeciesCount = 0;
	uiInfo.playerSpeciesIndex = 0;
	uiInfo.playerSpeciesMax = 8;
	uiInfo.playerSpecies = static_cast<playerSpeciesInfo_t*>(malloc(
		uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t)));

	// iterate directory of all player models
	const int numdirs = ui.FS_GetFileList("models/players", "/", dirlist, dirListSize);
	char* dirptr = dirlist;
	for (int i = 0; i < numdirs; i++, dirptr += dirlen + 1)
	{
		int f = 0;
		char fpath[MAX_QPATH];

		dirlen = strlen(dirptr);

		if (dirlen)
		{
			if (dirptr[dirlen - 1] == '/')
				dirptr[dirlen - 1] = '\0';
		}
		else
		{
			continue;
		}

		if (strcmp(dirptr, ".") == 0 || strcmp(dirptr, "..") == 0)
			continue;

		Com_sprintf(fpath, sizeof fpath, "models/players/%s/PlayerChoice.txt", dirptr);
		int filelen = ui.FS_FOpenFile(fpath, &f, FS_READ);

		if (f)
		{
			char filelist[2048];

			std::vector<char> buffer(filelen + 1);
			ui.FS_Read(&buffer[0], filelen, f);
			ui.FS_FCloseFile(f);

			buffer[filelen] = 0;

			//record this species
			if (uiInfo.playerSpeciesCount >= uiInfo.playerSpeciesMax)
			{
				uiInfo.playerSpeciesMax *= 2;
				uiInfo.playerSpecies = static_cast<playerSpeciesInfo_t*>(realloc(
					uiInfo.playerSpecies, uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t)));
			}
			playerSpeciesInfo_t* species = &uiInfo.playerSpecies[uiInfo.playerSpeciesCount];
			memset(species, 0, sizeof(playerSpeciesInfo_t));
			Q_strncpyz(species->Name, dirptr, MAX_QPATH);

			if (!UI_ParseColorData(buffer.data(), *species))
			{
				ui.Printf("UI_BuildPlayerModel_List: Errors parsing '%s'\n", fpath);
			}

			species->SkinHeadMax = 8;
			species->SkinTorsoMax = 8;
			species->SkinLegMax = 8;

			species->SkinHead = static_cast<skinName_t*>(malloc(species->SkinHeadMax * sizeof(skinName_t)));
			species->SkinTorso = static_cast<skinName_t*>(malloc(species->SkinTorsoMax * sizeof(skinName_t)));
			species->SkinLeg = static_cast<skinName_t*>(malloc(species->SkinLegMax * sizeof(skinName_t)));

			int iSkinParts = 0;

			const int numfiles = ui.
				FS_GetFileList(va("models/players/%s", dirptr), ".skin", filelist, sizeof filelist);
			char* fileptr = filelist;
			for (int j = 0; j < numfiles; j++, fileptr += filelen + 1)
			{
				char skinname[64];
				if (building)
				{
					ui.FS_FOpenFile(va("models/players/%s/%s", dirptr, fileptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/animevents.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
				}

				filelen = strlen(fileptr);
				COM_StripExtension(fileptr, skinname, sizeof skinname);

				if (IsImageFile(dirptr, skinname, static_cast<qboolean>(building != 0)))
				{
					//if it exists
					if (Q_stricmpn(skinname, "head_", 5) == 0)
					{
						if (species->SkinHeadCount >= species->SkinHeadMax)
						{
							species->SkinHeadMax *= 2;
							species->SkinHead = static_cast<skinName_t*>(realloc(
								species->SkinHead, species->SkinHeadMax * sizeof(skinName_t)));
						}
						Q_strncpyz(species->SkinHead[species->SkinHeadCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 0;
					}
					else if (Q_stricmpn(skinname, "torso_", 6) == 0)
					{
						if (species->SkinTorsoCount >= species->SkinTorsoMax)
						{
							species->SkinTorsoMax *= 2;
							species->SkinTorso = static_cast<skinName_t*>(realloc(
								species->SkinTorso, species->SkinTorsoMax * sizeof(skinName_t)));
						}
						Q_strncpyz(species->SkinTorso[species->SkinTorsoCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 1;
					}
					else if (Q_stricmpn(skinname, "lower_", 6) == 0)
					{
						if (species->SkinLegCount >= species->SkinLegMax)
						{
							species->SkinLegMax *= 2;
							species->SkinLeg = static_cast<skinName_t*>(realloc(
								species->SkinLeg, species->SkinLegMax * sizeof(skinName_t)));
						}
						Q_strncpyz(species->SkinLeg[species->SkinLegCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 2;
					}
				}
			}
			if (iSkinParts < 7)
			{
				//didn't get a skin for each, then skip this model.
				UI_FreeSpecies(species);
				continue;
			}
			uiInfo.playerSpeciesCount++;

			if (ui_com_rend2.integer == 0) //rend2 is off
			{
				if (!inGameLoad && ui_PrecacheModels.integer)
				{
					CGhoul2Info_v ghoul2;
					Com_sprintf(fpath, sizeof fpath, "models/players/%s/model.glm", dirptr);
					const int g2Model = DC->g2_InitGhoul2Model(ghoul2, fpath, 0, 0, 0, 0, 0);
					if (g2Model >= 0)
					{
						DC->g2_RemoveGhoul2Model(ghoul2, 0);
					}
				}
			}
		}
	}

	if (dirlist != stackDirList)
	{
		free(dirlist);
	}
}

/*
================
UI_Shutdown
=================
*/
void UI_Shutdown()
{
	UI_FreeAllSpecies();
}

/*
=================
UI_Init
=================
*/
void _UI_Init(const qboolean inGameLoad)
{
	// Get the list of possible languages
#ifndef JK2_MODE
	uiInfo.languageCount = SE_GetNumLanguages(); // this does a dir scan, so use carefully
#else
	// sod it, parse every menu strip file until we find a gap in the sequence...
	//
	for (int i = 0; i < 10; i++)
	{
		if (!ui.SP_Register(va("menus%d", i), /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			break;
	}
#endif

	uiInfo.inGameLoad = inGameLoad;

	UI_RegisterCvars();

	UI_InitMemory();

	// cache redundant calulations
	trap_GetGlconfig(&uiInfo.uiDC.glconfig);

	// for 640x480 virtualized screen
	uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0 / 480.0);
	uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0 / 640.0);
	if (uiInfo.uiDC.glconfig.vidWidth * 480 > uiInfo.uiDC.glconfig.vidHeight * 640)
	{
		// wide screen
		uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - uiInfo.uiDC.glconfig.vidHeight * (640.0 / 480.0));
	}
	else
	{
		// no wide screen
		uiInfo.uiDC.bias = 0;
	}

	Init_Display(&uiInfo.uiDC);

	uiInfo.uiDC.drawText = &Text_Paint;
	uiInfo.uiDC.drawHandlePic = &UI_DrawHandlePic;
	uiInfo.uiDC.drawRect = &_UI_DrawRect;
	uiInfo.uiDC.drawSides = &_UI_DrawSides;
	uiInfo.uiDC.drawTextWithCursor = &Text_PaintWithCursor;
	uiInfo.uiDC.executeText = &Cbuf_ExecuteText;
	uiInfo.uiDC.drawTopBottom = &_UI_DrawTopBottom;
	uiInfo.uiDC.feederCount = &UI_FeederCount;
	uiInfo.uiDC.feederSelection = &UI_FeederSelection;
	uiInfo.uiDC.fillRect = &UI_FillRect;
	uiInfo.uiDC.getBindingBuf = &Key_GetBindingBuf;
	uiInfo.uiDC.getCVarString = Cvar_VariableStringBuffer;
	uiInfo.uiDC.getCVarValue = trap_Cvar_VariableValue;
	uiInfo.uiDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
	uiInfo.uiDC.getValue = &UI_GetValue;
	uiInfo.uiDC.keynumToStringBuf = &Key_KeynumToStringBuf;
	uiInfo.uiDC.modelBounds = &trap_R_ModelBounds;
	uiInfo.uiDC.ownerDrawVisible = &UI_OwnerDrawVisible;
	uiInfo.uiDC.ownerDrawWidth = &UI_OwnerDrawWidth;
	uiInfo.uiDC.ownerDrawItem = &UI_OwnerDraw;
	uiInfo.uiDC.Print = &Com_Printf;
	uiInfo.uiDC.registerSound = &trap_S_RegisterSound;
	uiInfo.uiDC.registerModel = ui.R_RegisterModel;
	uiInfo.uiDC.clearScene = &trap_R_ClearScene;
	uiInfo.uiDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
	uiInfo.uiDC.renderScene = &trap_R_RenderScene;
	uiInfo.uiDC.runScript = &UI_RunMenuScript;
	uiInfo.uiDC.deferScript = &UI_DeferMenuScript;
	uiInfo.uiDC.setBinding = &trap_Key_SetBinding;
	uiInfo.uiDC.setColor = &UI_SetColor;
	uiInfo.uiDC.setCVar = Cvar_Set;
	uiInfo.uiDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
	uiInfo.uiDC.startLocalSound = &trap_S_StartLocalSound;
	uiInfo.uiDC.stopCinematic = &UI_StopCinematic;
	uiInfo.uiDC.textHeight = &Text_Height;
	uiInfo.uiDC.textWidth = &Text_Width;
	uiInfo.uiDC.feederItemImage = &UI_FeederItemImage;
	uiInfo.uiDC.feederItemText = &UI_FeederItemText;
	uiInfo.uiDC.ownerDrawHandleKey = &UI_OwnerDrawHandleKey;

	uiInfo.uiDC.registerSkin = re.RegisterSkin;

	uiInfo.uiDC.g2_SetSkin = re.G2API_SetSkin;
	uiInfo.uiDC.g2_SetBoneAnim = re.G2API_SetBoneAnim;
	uiInfo.uiDC.g2_RemoveGhoul2Model = re.G2API_RemoveGhoul2Model;
	uiInfo.uiDC.g2_InitGhoul2Model = re.G2API_InitGhoul2Model;
	uiInfo.uiDC.g2_CleanGhoul2Models = re.G2API_CleanGhoul2Models;
	uiInfo.uiDC.g2_AddBolt = re.G2API_AddBolt;
	uiInfo.uiDC.g2_GetBoltMatrix = re.G2API_GetBoltMatrix;
	uiInfo.uiDC.g2_GiveMeVectorFromMatrix = re.G2API_GiveMeVectorFromMatrix;

	uiInfo.uiDC.g2hilev_SetAnim = UI_G2SetAnim;

	UI_BuildPlayerModel_List(inGameLoad);

	String_Init();

	const char* menuSet = UI_Cvar_VariableString("ui_menuFiles");

	if (menuSet == nullptr || menuSet[0] == '\0')
	{
		menuSet = "ui/menus.txt";
	}

#ifndef JK2_MODE
	if (inGameLoad)
	{
		UI_LoadMenus("ui/ingame.txt", qtrue);
	}
	else
#endif
	{
		UI_LoadMenus(menuSet, qtrue);
	}

	Menus_CloseAll();

	uiInfo.uiDC.whiteShader = ui.R_RegisterShaderNoMip("white");

	AssetCache();

	uis.debugMode = qfalse;

	// sets defaults for ui temp cvars
	uiInfo.effectsColor = static_cast<int>(trap_Cvar_VariableValue("color")) - 1;
	if (uiInfo.effectsColor < 0)
	{
		uiInfo.effectsColor = 0;
	}
	uiInfo.effectsColor = gamecodetoui[uiInfo.effectsColor];
	uiInfo.currentCrosshair = static_cast<int>(trap_Cvar_VariableValue("cg_drawCrosshair"));
	Cvar_Set("ui_mousePitch", trap_Cvar_VariableValue("m_pitch") >= 0 ? "0" : "1");

	Cvar_Set("cg_endcredits", "0"); // Reset value
	Cvar_Set("ui_missionfailed", "0"); // reset

	uiInfo.forcePowerUpdated = FP_UPDATED_NONE;
	uiInfo.selectedWeapon1 = NOWEAPON;
	uiInfo.selectedWeapon2 = NOWEAPON;
	uiInfo.selectedThrowWeapon = NOWEAPON;
	uiInfo.selectedPistolWeapon = NOWEAPON;

	uiInfo.uiDC.Assets.nullSound = trap_S_RegisterSound("sound/null", qfalse);

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	trap_S_RegisterSound("sound/interface/weapon_deselect", qfalse);
#endif

#ifdef NEW_FEEDER
	memset(mdHeadIcons, 0, sizeof(mdHeadIcons));
	MD_BG_GenerateEraIndexes();
	mdBorder = ui.R_RegisterShaderNoMip("gfx/menus/charactermenuicons/icon_border.tga");
	mdBorderSel = ui.R_RegisterShaderNoMip("gfx/menus/charactermenuicons/icon_border_gold.tga");
	mdBackground = ui.R_RegisterShaderNoMip("gfx/menus/charactermenuicons/icon_background.tga");
#endif
#ifdef NEW_FEEDER_V6
	firstTimeLoad = false;
#endif
#ifdef NEW_FEEDER_V9
	uiEra = 0;
	uiModelIndex = 0;
	uiVariantIndex = 0;

	//var_Set("g_saber_type", Cvar_VariableString("ui_saber_type_md"));
	//Cvar_Set("g_saber", Cvar_VariableString("ui_saber_md"));
	//Cvar_Set("g_saber2", Cvar_VariableString("ui_saber2_md"));
	//Cvar_Set("g_saber_color", Cvar_VariableString("ui_saber_color_md"));
	//Cvar_Set("g_saber2_color", Cvar_VariableString("ui_saber2_color_md"));
#endif
}

/*
=================
UI_RegisterCvars
=================
*/
static void UI_RegisterCvars()
{
	size_t i;
	const cvarTable_t* cv;

	for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++)
	{
		Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
		if (cv->update)
			cv->update();
	}
}

/*
=================
UI_ParseMenu
=================
*/
static void UI_ParseMenu(const char* menuFile)
{
	char* buffer, * holdBuffer;
	//	pc_token_t token;

	//Com_DPrintf("Parsing menu file: %s\n", menuFile);
	const int len = PC_StartParseSession(menuFile, &buffer);

	holdBuffer = buffer;

	if (len <= 0)
	{
		Com_Printf("UI_ParseMenu: Unable to load menu %s\n", menuFile);
		return;
	}

	while (true)
	{
		char* token2 = PC_ParseExt();

		if (!*token2)
		{
			break;
		}
		/*
				if ( menuCount == MAX_MENUS )
				{
					PC_ParseWarning("Too many menus!");
					break;
				}
		*/
		if (*token2 == '{')
		{
			continue;
		}
		if (*token2 == '}')
		{
			break;
		}
		if (Q_stricmp(token2, "assetGlobalDef") == 0)
		{
			if (Asset_Parse(&holdBuffer))
			{
				continue;
			}
			break;
		}
		if (Q_stricmp(token2, "menudef") == 0)
		{
			// start a new menu
			Menu_New(holdBuffer);
			continue;
		}

		PC_ParseWarning(va("Invalid keyword '%s'", token2));
	}

	PC_EndParseSession(buffer);
}

/*
=================
Load_Menu
	Load current menu file
=================
*/
static qboolean Load_Menu(const char** holdBuffer)
{
	const char* token2 = COM_ParseExt(holdBuffer, qtrue);

	if (!token2[0])
	{
		return qfalse;
	}

	if (*token2 != '{')
	{
		return qfalse;
	}

	while (true)
	{
		token2 = COM_ParseExt(holdBuffer, qtrue);

		if (!token2 || token2 == nullptr)
		{
			return qfalse;
		}

		if (*token2 == '}')
		{
			return qtrue;
		}

		//#ifdef _DEBUG
		//		extern void UI_Debug_AddMenuFilePath(const char *);
		//		UI_Debug_AddMenuFilePath(token2);
		//#endif
		UI_ParseMenu(token2);
	}
}

/*
=================
UI_LoadMenus
	Load all menus based on the files listed in the data file in menuFile (default "ui/menus.txt")
=================
*/
void UI_LoadMenus(const char* menuFile, const qboolean reset)
{
	char* buffer = nullptr;
	const char* holdBuffer;

	const int start = Sys_Milliseconds();

	int len = ui.FS_ReadFile(menuFile, reinterpret_cast<void**>(&buffer));

	if (len < 1)
	{
		Com_Printf(va(S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile));
		len = ui.FS_ReadFile("ui/menus.txt", reinterpret_cast<void**>(&buffer));

		if (len < 1)
		{
			Com_Error(ERR_FATAL, "%s",
				va("default menu file not found: ui/menus.txt, unable to continue!\n", menuFile));
		}
	}

	if (reset)
	{
		Menu_Reset();
	}

	holdBuffer = buffer;
	COM_BeginParseSession();
	while (true)
	{
		const char* token2 = COM_ParseExt(&holdBuffer, qtrue);
		if (!*token2)
		{
			break;
		}

		if (*token2 == 0 || *token2 == '}') // End of the menus file
		{
			break;
		}

		if (*token2 == '{')
		{
			continue;
		}
		if (Q_stricmp(token2, "loadmenu") == 0)
		{
			if (Load_Menu(&holdBuffer))
			{
				continue;
			}
			break;
		}
		Com_Printf("Unknown keyword '%s' in menus file %s\n", token2, menuFile);
	}
	COM_EndParseSession();

	Com_Printf("UI menu load time = %d milli seconds\n", Sys_Milliseconds() - start);

	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("--------------------- Client Initialization ---------------------\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("---- Genuine MovieDuels SerenityJediEngine (Solaris Edition) ----\n");
	Com_Printf("----------------------- MovieDuels-SJE-SP -----------------------\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("-------------------------- Update 7.0 ---------------------------\n");
	Com_Printf("--------------------- Build Date 26/01/2026 ---------------------\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("-------------------------- Lightsaber ---------------------------\n");
	Com_Printf("---------- An elegant weapon for a more civilized age -----------\n");
	Com_Printf("-----------------------------------------------------------------\n");

	//Com_Printf("------Type (seta cl_noprint 0) to see text------\n");
	//Com_Printf("------Type (Debuginfo) to open debug command list------\n");

	ui.FS_FreeFile(buffer); //let go of the buffer
}

/*
=================
UI_Load
=================
*/
void UI_Load()
{
	const char* menuSet;
	char lastName[1024];
	const menuDef_t* menu = Menu_GetFocused();

	if (menu && menu->window.name)
	{
		strcpy(lastName, menu->window.name);
	}
	else
	{
		lastName[0] = 0;
	}

#ifndef JK2_MODE
	if (uiInfo.inGameLoad)
	{
		menuSet = "ui/ingame.txt";
	}
	else
#endif
	{
		menuSet = UI_Cvar_VariableString("ui_menuFiles");
	}
	if (menuSet == nullptr || menuSet[0] == '\0')
	{
		menuSet = "ui/menus.txt";
	}

	String_Init();

#ifdef NEW_FEEDER_V6
//	firstTimeLoad = false;
#endif

	UI_LoadMenus(menuSet, qtrue);
	Menus_CloseAll();
	Menus_ActivateByName(lastName);
}

/*
=================
Asset_Parse
=================
*/
qboolean Asset_Parse(char** buffer)
{
	const char* tempStr;
	int pointSize;

	char* token = PC_ParseExt();

	if (!token)
	{
		return qfalse;
	}

	if (*token != '{')
	{
		return qfalse;
	}

	while (true)
	{
		token = PC_ParseExt();

		if (!token)
		{
			return qfalse;
		}

		if (*token == '}')
		{
			return qtrue;
		}

		// fonts
		if (Q_stricmp(token, "smallFont") == 0) //legacy, really it only matters which order they are registered
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'smallFont'");
				return qfalse;
			}

			UI_RegisterFont(tempStr);

			//not used anymore
			if (PC_ParseInt(&pointSize))
			{
				//				PC_ParseWarning("Bad 2nd parameter for keyword 'smallFont'");
			}

			continue;
		}

		if (Q_stricmp(token, "mediumFont") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'font'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.qhMediumFont = UI_RegisterFont(tempStr);
			uiInfo.uiDC.Assets.fontRegistered = qtrue;

			//not used
			if (PC_ParseInt(&pointSize))
			{
				//				PC_ParseWarning("Bad 2nd parameter for keyword 'font'");
			}
			continue;
		}

		if (Q_stricmp(token, "bigFont") == 0) //legacy
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'bigFont'");
				return qfalse;
			}

			UI_RegisterFont(tempStr);

			if (PC_ParseInt(&pointSize))
			{
				//				PC_ParseWarning("Bad 2nd parameter for keyword 'bigFont'");
			}

			continue;
		}

#ifdef JK2_MODE
		if (Q_stricmp(token, "stripedFile") == 0)
		{
			if (!PC_ParseStringMem((const char**)&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'stripedFile'");
				return qfalse;
			}

			char sTemp[1024];
			Q_strncpyz(sTemp, tempStr, sizeof(sTemp));
			if (!ui.SP_Register(sTemp, /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			{
				PC_ParseWarning(va("(.SP file \"%s\" not found)", sTemp));
				//return qfalse;	// hmmm... dunno about this, don't want to break scripts for just missing subtitles
			}
			else
			{
				//				extern void AddMenuPackageRetryKey(const char *);
				//				AddMenuPackageRetryKey(sTemp);
			}

			continue;
		}
#endif

		// gradientbar
		if (Q_stricmp(token, "gradientbar") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'gradientbar'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.gradientBar = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		// enterMenuSound
		if (Q_stricmp(token, "menuEnterSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuEnterSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// exitMenuSound
		if (Q_stricmp(token, "menuExitSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuExitSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// itemFocusSound
		if (Q_stricmp(token, "itemFocusSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'itemFocusSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// menuBuzzSound
		if (Q_stricmp(token, "menuBuzzSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuBuzzSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// Chose a force power from the ingame force allocation screen (the one where you get to allocate a force power point)
		if (Q_stricmp(token, "forceChosenSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'forceChosenSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.forceChosenSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		// Unchose a force power from the ingame force allocation screen (the one where you get to allocate a force power point)
		if (Q_stricmp(token, "forceUnchosenSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'forceUnchosenSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.forceUnchosenSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveRollSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveRollSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveRollSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveJumpSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveRoll'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveJumpSound = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound1") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound1'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound1 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound2") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound2'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound2 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound3") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound3'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound3 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound4") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound4'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound4 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound5") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound5'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound5 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound6") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound6'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound6 = trap_S_RegisterSound(tempStr, qfalse);
			continue;
		}

		////////////////////////////////////////////////////////////////////////////////////

		if (Q_stricmp(token, "cursor") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_anakin") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_anakin'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_anakin = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_jk") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_jk3'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_jk = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_katarn") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_katarn'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_katarn = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_kylo") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_kylo'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_kylo = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_luke") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_luke'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_luke = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_obiwan") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_obiwan'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_obiwan = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_oldrepublic") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_oldrepublic'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_oldrepublic = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_quigon") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_quigon'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_quigon = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_rey") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_rey'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_rey = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_vader") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_vader'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_vader = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		if (Q_stricmp(token, "cursor_windu") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor_windu'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor_windu = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////////

		if (Q_stricmp(token, "fadeClamp") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.fadeClamp))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeClamp'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "fadeCycle") == 0)
		{
			if (PC_ParseInt(&uiInfo.uiDC.Assets.fadeCycle))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeCycle'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "fadeAmount") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.fadeAmount))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeAmount'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowX") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.shadowX))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowX'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowY") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.shadowY))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowY'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowColor") == 0)
		{
			if (PC_ParseColor(&uiInfo.uiDC.Assets.shadowColor))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowColor'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
			continue;
		}

		// precaching various sound files used in the menus
		if (Q_stricmp(token, "precacheSound") == 0)
		{
			if (PC_Script_Parse(&tempStr))
			{
				char* soundFile;
				do
				{
					soundFile = COM_ParseExt(&tempStr, qfalse);
					if (soundFile[0] != 0 && soundFile[0] != ';')
					{
						if (!trap_S_RegisterSound(soundFile, qfalse))
						{
							PC_ParseWarning("Can't locate precache sound");
						}
					}
				} while (soundFile[0]);
			}
		}
	}
}

/*
=================
UI_Update
=================
*/
static void UI_Update(const char* name)
{
	const int val = trap_Cvar_VariableValue(name);

	if (Q_stricmp(name, "s_khz") == 0)
	{
		ui.Cmd_ExecuteText(EXEC_APPEND, "snd_restart\n");
		return;
	}

	if (Q_stricmp(name, "ui_SetName") == 0)
	{
		Cvar_Set("name", UI_Cvar_VariableString("ui_Name"));
	}
	else if (Q_stricmp(name, "ui_GetName") == 0)
	{
		Cvar_Set("ui_Name", UI_Cvar_VariableString("name"));
	}
	else if (Q_stricmp(name, "ui_r_colorbits") == 0)
	{
		switch (val)
		{
		case 0:
			Cvar_SetValue("ui_r_depthbits", 0);
			break;

		case 16:
			Cvar_SetValue("ui_r_depthbits", 16);
			break;

		case 32:
			Cvar_SetValue("ui_r_depthbits", 24);
			break;
		default:;
		}
	}
	else if (Q_stricmp(name, "ui_r_lodbias") == 0)
	{
		switch (val)
		{
		case 0:
			Cvar_SetValue("ui_r_subdivisions", 4);
			break;
		case 1:
			Cvar_SetValue("ui_r_subdivisions", 12);
			break;

		case 2:
			Cvar_SetValue("ui_r_subdivisions", 20);
			break;
		default:;
		}
	}
	else if (Q_stricmp(name, "ui_r_glCustom") == 0)
	{
		switch (val)
		{
		case 0: // high quality

			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 4);
			Cvar_SetValue("ui_r_lodbias", 0);
			Cvar_SetValue("ui_r_colorbits", 32);
			Cvar_SetValue("ui_r_depthbits", 24);
			Cvar_SetValue("ui_r_picmip", 0);
			Cvar_SetValue("ui_r_mode", 4);
			Cvar_SetValue("ui_r_texturebits", 32);
			Cvar_SetValue("ui_r_fastSky", 0);
			Cvar_SetValue("ui_r_inGameVideo", 1);
			Cvar_SetValue("ui_cg_shadows", 1);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
			break;

		case 1: // normal
			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 4);
			Cvar_SetValue("ui_r_lodbias", 0);
			Cvar_SetValue("ui_r_colorbits", 0);
			Cvar_SetValue("ui_r_depthbits", 24);
			Cvar_SetValue("ui_r_picmip", 1);
			Cvar_SetValue("ui_r_mode", 3);
			Cvar_SetValue("ui_r_texturebits", 0);
			Cvar_SetValue("ui_r_fastSky", 0);
			Cvar_SetValue("ui_r_inGameVideo", 1);
			Cvar_SetValue("ui_cg_shadows", 1);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
			break;

		case 2: // fast

			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 12);
			Cvar_SetValue("ui_r_lodbias", 1);
			Cvar_SetValue("ui_r_colorbits", 0);
			Cvar_SetValue("ui_r_depthbits", 0);
			Cvar_SetValue("ui_r_picmip", 2);
			Cvar_SetValue("ui_r_mode", 3);
			Cvar_SetValue("ui_r_texturebits", 0);
			Cvar_SetValue("ui_r_fastSky", 1);
			Cvar_SetValue("ui_r_inGameVideo", 0);
			Cvar_SetValue("ui_cg_shadows", 1);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
			break;

		case 3: // fastest

			Cvar_SetValue("ui_r_fullScreen", 1);
			Cvar_SetValue("ui_r_subdivisions", 20);
			Cvar_SetValue("ui_r_lodbias", 2);
			Cvar_SetValue("ui_r_colorbits", 16);
			Cvar_SetValue("ui_r_depthbits", 16);
			Cvar_SetValue("ui_r_mode", 3);
			Cvar_SetValue("ui_r_picmip", 3);
			Cvar_SetValue("ui_r_texturebits", 16);
			Cvar_SetValue("ui_r_fastSky", 1);
			Cvar_SetValue("ui_r_inGameVideo", 0);
			Cvar_SetValue("ui_cg_shadows", 0);
			Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
			break;
		default:;
		}
	}
	else if (Q_stricmp(name, "ui_mousePitch") == 0)
	{
		if (val == 0)
		{
			Cvar_SetValue("m_pitch", 0.022f);
		}
		else
		{
			Cvar_SetValue("m_pitch", -0.022f);
		}
	}
	else
	{
		//failure!!
		Com_Printf("unknown UI script UPDATE %s\n", name);
	}
}

constexpr auto ASSET_SCROLLBAR = "gfx/menus/scrollbar.tga";
constexpr auto ASSET_SCROLLBAR_ARROWDOWN = "gfx/menus/scrollbar_arrow_dwn_a.tga";
constexpr auto ASSET_SCROLLBAR_ARROWUP = "gfx/menus/scrollbar_arrow_up_a.tga";
constexpr auto ASSET_SCROLLBAR_ARROWLEFT = "gfx/menus/scrollbar_arrow_left.tga";
constexpr auto ASSET_SCROLLBAR_ARROWRIGHT = "gfx/menus/scrollbar_arrow_right.tga";
constexpr auto ASSET_SCROLL_THUMB = "gfx/menus/scrollbar_thumb.tga";
//
constexpr auto ASSET_ANAKIN = "gfx/menus/cursor_anakin.tga";
constexpr auto ASSET_JK = "gfx/menus/cursor_jk.tga";
constexpr auto ASSET_KATARN = "gfx/menus/cursor_katarn.tga";
constexpr auto ASSET_KYLO = "gfx/menus/cursor_kylo.tga";
constexpr auto ASSET_LUKE = "gfx/menus/cursor_luke.tga";
constexpr auto ASSET_OBIWAN = "gfx/menus/cursor_obiwan.tga";
constexpr auto ASSET_OLDREPUBLIC = "gfx/menus/cursor_oldrepublic.tga";
constexpr auto ASSET_QUIGON = "gfx/menus/cursor_quigon.tga";
constexpr auto ASSET_RAY = "gfx/menus/cursor_rey.tga";
constexpr auto ASSET_VADER = "gfx/menus/cursor_vader.tga";
constexpr auto ASSET_WINDU = "gfx/menus/cursor_windu.tga";

/*
=================
AssetCache
=================
*/
void AssetCache()
{
	//	int n;
	uiInfo.uiDC.Assets.scrollBar = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR);
	uiInfo.uiDC.Assets.scrollBarArrowDown = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
	uiInfo.uiDC.Assets.scrollBarArrowUp = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
	uiInfo.uiDC.Assets.scrollBarArrowLeft = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
	uiInfo.uiDC.Assets.scrollBarArrowRight = ui.R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
	uiInfo.uiDC.Assets.scrollBarThumb = ui.R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);

	uiInfo.uiDC.Assets.sliderBar = ui.R_RegisterShaderNoMip("menu/new/slider");
	uiInfo.uiDC.Assets.sliderThumb = ui.R_RegisterShaderNoMip("menu/new/sliderthumb");

	uiInfo.uiDC.Assets.cursor_anakin = ui.R_RegisterShaderNoMip(ASSET_ANAKIN);
	uiInfo.uiDC.Assets.cursor_jk = ui.R_RegisterShaderNoMip(ASSET_JK);
	uiInfo.uiDC.Assets.cursor_katarn = ui.R_RegisterShaderNoMip(ASSET_KATARN);
	uiInfo.uiDC.Assets.cursor_kylo = ui.R_RegisterShaderNoMip(ASSET_KYLO);
	uiInfo.uiDC.Assets.cursor_luke = ui.R_RegisterShaderNoMip(ASSET_LUKE);
	uiInfo.uiDC.Assets.cursor_obiwan = ui.R_RegisterShaderNoMip(ASSET_OBIWAN);
	uiInfo.uiDC.Assets.cursor_oldrepublic = ui.R_RegisterShaderNoMip(ASSET_OLDREPUBLIC);
	uiInfo.uiDC.Assets.cursor_quigon = ui.R_RegisterShaderNoMip(ASSET_QUIGON);
	uiInfo.uiDC.Assets.cursor_rey = ui.R_RegisterShaderNoMip(ASSET_RAY);
	uiInfo.uiDC.Assets.cursor_vader = ui.R_RegisterShaderNoMip(ASSET_VADER);
	uiInfo.uiDC.Assets.cursor_windu = ui.R_RegisterShaderNoMip(ASSET_WINDU);
}

/*
================
_UI_DrawSides
=================
*/
void _UI_DrawSides(const float x, const float y, const float w, const float h, float size)
{
	size *= uiInfo.uiDC.xscale;
	trap_R_DrawStretchPic(x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
	trap_R_DrawStretchPic(x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

/*
================
_UI_DrawTopBottom
=================
*/
void _UI_DrawTopBottom(const float x, const float y, const float w, const float h, float size)
{
	size *= uiInfo.uiDC.yscale;
	trap_R_DrawStretchPic(x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
	trap_R_DrawStretchPic(x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void _UI_DrawRect(const float x, const float y, const float width, const float height, const float size, const float* color)
{
	trap_R_SetColor(color);

	_UI_DrawTopBottom(x, y, width, height, size);
	_UI_DrawSides(x, y, width, height, size);

	trap_R_SetColor(nullptr);
}

/*
=================
UI_UpdateCvars
=================
*/
void UI_UpdateCvars()
{
	size_t i;
	const cvarTable_t* cv;

	for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++)
	{
		if (cv->vmCvar)
		{
			const int modCount = cv->vmCvar->modificationCount;
			Cvar_Update(cv->vmCvar);
			if (cv->vmCvar->modificationCount != modCount)
			{
				if (cv->update)
					cv->update();
			}
		}
	}
}

/*
=================
UI_DrawEffects
=================
*/
static void UI_DrawEffects(const rectDef_t* rect, float scale, vec4_t color)
{
	UI_DrawHandlePic(rect->x, rect->y - 14, 128, 8, 0/*uiInfo.uiDC.Assets.fxBasePic*/);
	UI_DrawHandlePic(rect->x + uiInfo.effectsColor * 16 + 8, rect->y - 16, 16, 12,
		0/*uiInfo.uiDC.Assets.fxPic[uiInfo.effectsColor]*/);
}

/*
=================
UI_Version
=================
*/
static void UI_Version(const rectDef_t* rect, const float scale, vec4_t color, const int iFontIndex)
{
	const int width = DC->textWidth(Q3_VERSION, scale, 0);

	DC->drawText(rect->x - width, rect->y, scale, color, Q3_VERSION, 0, ITEM_TEXTSTYLE_SHADOWED, iFontIndex);
}

/*
=================
UI_DrawKeyBindStatus
=================
*/
static void UI_DrawKeyBindStatus(rectDef_t* rect, float scale, vec4_t color, int textStyle, int iFontIndex)
{
	if (Display_KeyBindPending())
	{
#ifdef JK2_MODE
		Text_Paint(rect->x, rect->y, scale, color, ui.SP_GetStringTextString("MENUS_WAITINGFORKEY"), 0, textStyle, iFontIndex);
#else
		Text_Paint(rect->x, rect->y, scale, color, SE_GetString("MENUS_WAITINGFORKEY"), 0, textStyle, iFontIndex);
#endif
	}
	else
	{
		//		Text_Paint(rect->x, rect->y, scale, color, ui.SP_GetStringTextString("MENUS_ENTERTOCHANGE"), 0, textStyle, iFontIndex);
	}
}

/*
=================
UI_DrawKeyBindStatus
=================
*/
static void UI_DrawGLInfo(const rectDef_t* rect, const float scale, vec4_t color, const int textStyle, const int iFontIndex)
{
	constexpr auto MAX_LINES = 64;
	char buff[4096]{};
	char* eptr = buff;
	const char* lines[MAX_LINES]{};
	int numLines = 0, i = 0;

	int y = rect->y;
	Text_Paint(rect->x, y, scale, color, va("GL_VENDOR: %s", uiInfo.uiDC.glconfig.vendor_string), rect->w, textStyle,
		iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color,
		va("GL_VERSION: %s: %s", uiInfo.uiDC.glconfig.version_string, uiInfo.uiDC.glconfig.renderer_string),
		rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color, "GL_PIXELFORMAT:", rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color,
		va("Color(%d-bits) Z(%d-bits) stencil(%d-bits)", uiInfo.uiDC.glconfig.colorBits,
			uiInfo.uiDC.glconfig.depthBits, uiInfo.uiDC.glconfig.stencilBits), rect->w, textStyle, iFontIndex);
	y += 15;
	// build null terminated extension strings
	Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, sizeof buff);
	int testy = y - 16;
	while (testy <= rect->y + rect->h && *eptr && numLines < MAX_LINES)
	{
		while (*eptr && *eptr == ' ')
			*eptr++ = '\0';

		// track start of valid string
		if (*eptr && *eptr != ' ')
		{
			lines[numLines++] = eptr;
			testy += 16;
		}

		while (*eptr && *eptr != ' ')
			eptr++;
	}

	numLines--;
	while (i < numLines)
	{
		Text_Paint(rect->x, y, scale, color, lines[i++], rect->w, textStyle, iFontIndex);
		y += 16;
	}
}

/*
=================
UI_DataPad_Inventory
=================
*/
/*
static void UI_DataPad_Inventory(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	Text_Paint(rect->x, rect->y, scale, color, "INVENTORY", 0, 1, iFontIndex);
}
*/
/*
=================
UI_DataPad_ForcePowers
=================
*/
/*
static void UI_DataPad_ForcePowers(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	Text_Paint(rect->x, rect->y, scale, color, "FORCE POWERS", 0, 1, iFontIndex);
}
*/

static void UI_DrawCrosshair(const rectDef_t* rect, float scale, vec4_t color)
{
	trap_R_SetColor(color);
	if (uiInfo.currentCrosshair < 0 || uiInfo.currentCrosshair >= NUM_CROSSHAIRS)
	{
		uiInfo.currentCrosshair = 0;
	}
	UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
	trap_R_SetColor(nullptr);
}

/*
=================
UI_OwnerDraw
=================
*/
static void UI_OwnerDraw(float x, float y, float w, float h, const float text_x, const float text_y, const int ownerDraw,
	int ownerDrawFlags, int align, float special, const float scale, vec4_t color, qhandle_t shader,
	const int textStyle, const int iFontIndex)
{
	rectDef_t rect{};

	rect.x = x + text_x;
	rect.y = y + text_y;
	rect.w = w;
	rect.h = h;

	switch (ownerDraw)
	{
	case UI_EFFECTS:
		UI_DrawEffects(&rect, scale, color);
		break;
	case UI_VERSION:
		UI_Version(&rect, scale, color, iFontIndex);
		break;

	case UI_DATAPAD_MISSION:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_OBJECTIVES);
		break;

	case UI_DATAPAD_WEAPONS:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_WEAPONS);
		break;

	case UI_DATAPAD_INVENTORY:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_INVENTORY);
		break;

	case UI_DATAPAD_FORCEPOWERS:
		ui.Draw_DataPad(DP_HUD);
		ui.Draw_DataPad(DP_FORCEPOWERS);
		break;

	case UI_ALLMAPS_SELECTION: //saved game thumbnail

		int levelshot;
		levelshot = ui.
			R_RegisterShaderNoMip(va("levelshots/%s", s_savedata[s_savegame.currentLine].currentSaveFileMap));
#ifdef JK2_MODE
		if (screenShotBuf[0])
		{
			ui.DrawStretchRaw(x, y, w, h, SG_SCR_WIDTH, SG_SCR_HEIGHT, screenShotBuf, 0, qtrue);
		}
		else
#endif
			if (levelshot)
			{
				ui.R_DrawStretchPic(x, y, w, h, 0, 0, 1, 1, levelshot);
			}
			else
			{
				UI_DrawHandlePic(x, y, w, h, uis.menuBackShader);
			}

		ui.R_Font_DrawString(x, // int ox
			y + h, // int oy
			s_savedata[s_savegame.currentLine].currentSaveFileMap, // const char *text
			color, // paletteRGBA_c c
			iFontIndex, // const int iFontHandle
			w, //-1,		// iMaxPixelWidth (-1 = none)
			scale // const float scale = 1.0f
		);
		break;
	case UI_PREVIEWCINEMATIC:
		// FIXME BOB - make this work?
		//			UI_DrawPreviewCinematic(&rect, scale, color);
		break;
	case UI_CROSSHAIR:
		UI_DrawCrosshair(&rect, scale, color);
		break;
	case UI_GLINFO:
		UI_DrawGLInfo(&rect, scale, color, textStyle, iFontIndex);
		break;
	case UI_KEYBINDSTATUS:
		UI_DrawKeyBindStatus(&rect, scale, color, textStyle, iFontIndex);
		break;
	case UI_DUELMAPS_SELECTION: // Duel maps

		int duelMapImage;
		duelMapImage = ui.
			R_RegisterShaderNoMip(duelMaps[duelMapIndex].duelMapImage);
			if (duelMapImage)
			{
				ui.R_DrawStretchPic(x, y, w, h, 0, 0, 1, 1, duelMapImage);
			}
			else
			{
				UI_DrawHandlePic(x, y, w, h, uis.menuBackShader);
			}
		break;
	default:
		break;
	}
}

/*
=================
UI_OwnerDrawVisible
=================
*/
static qboolean UI_OwnerDrawVisible(int flags)
{
	constexpr qboolean vis = qtrue;

	while (flags)
	{
		/*		if (flags & UI_SHOW_DEMOAVAILABLE)
				{
					if (!uiInfo.demoAvailable)
					{
						vis = qfalse;
					}
					flags &= ~UI_SHOW_DEMOAVAILABLE;
				}
				else
		*/
		{
			flags = 0;
		}
	}
	return vis;
}

/*
=================
Text_Width
=================
*/
int Text_Width(const char* text, const float scale, int iFontIndex)
{
	// temp code until Bob retro-fits all menus to have font specifiers...
	//
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	return ui.R_Font_StrLenPixels(text, iFontIndex, scale);
}

/*
=================
UI_OwnerDrawWidth
=================
*/
int UI_OwnerDrawWidth(const int ownerDraw, const float scale)
{
	//	int i, h, value;
	//	const char *text;
	const char* s = nullptr;

	switch (ownerDraw)
	{
	case UI_KEYBINDSTATUS:
		if (Display_KeyBindPending())
		{
#ifdef JK2_MODE
			s = ui.SP_GetStringTextString("MENUS_WAITINGFORKEY");
#else
			s = SE_GetString("MENUS_WAITINGFORKEY");
#endif
		}
		else
		{
			//			s = ui.SP_GetStringTextString("MENUS_ENTERTOCHANGE");
		}
		break;

		// FIXME BOB
		//	case UI_SERVERREFRESHDATE:
		//		s = UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer));
		//		break;
	default:
		break;
	}

	if (s)
	{
		return Text_Width(s, scale, 0);
	}
	return 0;
}

/*
=================
Text_Height
=================
*/
int Text_Height(const char* text, const float scale, int iFontIndex)
{
	// temp until Bob retro-fits all menu files with font specifiers...
	//
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	return ui.R_Font_HeightPixels(iFontIndex, scale);
}

/*
=================
UI_MouseEvent
=================
*/
//JLFMOUSE  CALLED EACH FRAME IN UI
void _UI_MouseEvent(const int dx, const int dy)
{
	// update mouse screen position
	uiInfo.uiDC.cursorx += dx;
	if (uiInfo.uiDC.cursorx < 0)
	{
		uiInfo.uiDC.cursorx = 0;
	}
	else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH)
	{
		uiInfo.uiDC.cursorx = SCREEN_WIDTH;
	}

	uiInfo.uiDC.cursory += dy;
	if (uiInfo.uiDC.cursory < 0)
	{
		uiInfo.uiDC.cursory = 0;
	}
	else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
	{
		uiInfo.uiDC.cursory = SCREEN_HEIGHT;
	}

	if (Menu_Count() > 0)
	{
		Display_MouseMove(nullptr, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
	}
}

/*
=================
UI_KeyEvent
=================
*/
void _UI_KeyEvent(const int key, const qboolean down)
{
	/*	extern qboolean SwallowBadNumLockedKPKey( int iKey );
		if (SwallowBadNumLockedKPKey(key)){
			return;
		}
	*/

	if (Menu_Count() > 0)
	{
		menuDef_t* menu = Menu_GetFocused();
		if (menu)
		{
			//DemoEnd();
			if (key == A_ESCAPE && down && !Menus_AnyFullScreenVisible() && !(menu->window.flags & WINDOW_IGNORE_ESCAPE))
			{
				Menus_CloseAll();
			}
			else
			{
				Menu_HandleKey(menu, key, down);
			}
		}
		else
		{
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Cvar_Set("cl_paused", "0");
		}
	}
}

/*
=================
UI_Report
=================
*/
void UI_Report()
{
	String_Report();
}

/*
=================
UI_DataPadMenu
=================
*/
void UI_DataPadMenu()
{
	Menus_CloseByName("mainhud");

	const int newForcePower = static_cast<int>(trap_Cvar_VariableValue("cg_updatedDataPadForcePower1"));
	const int newObjective = static_cast<int>(trap_Cvar_VariableValue("cg_updatedDataPadObjective"));

	if (newForcePower)
	{
		Menus_ActivateByName("datapadForcePowersMenu");
	}
	else if (newObjective)
	{
		Menus_ActivateByName("datapadMissionMenu");
	}
	else
	{
		Menus_ActivateByName("datapadMissionMenu");
	}
	ui.Key_SetCatcher(KEYCATCH_UI);
}

/*
=================
UI_InGameMenu
=================
*/
void UI_InGameMenu(const char* menuID)
{
#ifdef JK2_MODE
	ui.PrecacheScreenshot();
#endif
	Menus_CloseByName("mainhud");

	if (menuID)
	{
		Menus_ActivateByName(menuID);
	}
	else
	{
		Menus_ActivateByName("ingameMainMenu");
	}
	ui.Key_SetCatcher(KEYCATCH_UI);
}

qboolean _UI_IsFullscreen()
{
	return Menus_AnyFullScreenVisible();
}

/*
=======================================================================

MAIN MENU

=======================================================================
*/

/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu()
{
	char buf[256];
	ui.Cvar_Set("sv_killserver", "1"); // let the demo server know it should shut down

	ui.Key_SetCatcher(KEYCATCH_UI);

	const menuDef_t* m = Menus_ActivateByName("mainMenu");
	if (!m)
	{
		//wha? try again
		UI_LoadMenus("ui/menus.txt", qfalse);
	}
	ui.Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof buf);
	if (strlen(buf))
	{
		Menus_ActivateByName("error_popmenu");
	}
}

int SCREENSHOT_TOTAL = 8;
int SCREENSHOT_CHOICE = 0;
int SCREENSHOT_NEXT_UPDATE_TIME = 0;

static char* UI_GetCurrentLevelshot()
{
	const int time = Sys_Milliseconds();

	if (SCREENSHOT_NEXT_UPDATE_TIME < time)
	{
		SCREENSHOT_NEXT_UPDATE_TIME = time + 5000;
		SCREENSHOT_CHOICE = Q_irand(0, SCREENSHOT_TOTAL);
	}

	return va("menu/art/unknownmap%i", SCREENSHOT_CHOICE);
}

/*
=================
Menu_Cache
=================
*/
void Menu_Cache()
{
	uis.cursor = ui.R_RegisterShaderNoMip("menu/new/crosshairb");
	// Common menu graphics
	uis.whiteShader = ui.R_RegisterShader("white");
	//uis.menuBackShader = ui.R_RegisterShaderNoMip( "menu/art/unknownmap" );
	//uis.menuBackShader = ui.R_RegisterShaderNoMip(UI_GetCurrentLevelshot());
	uis.menuBackShader = ui.R_RegisterShaderNoMip(va("menu/art/unknownmap%d", Q_irand(0, SCREENSHOT_TOTAL)));
}

/*
=================
UI_UpdateVideoSetup

Copies the temporary user interface version of the video cvars into
their real counterparts.  This is to create a interface which allows
you to discard your changes if you did something you didnt want
=================
*/
void UI_UpdateVideoSetup()
{
	Cvar_Set("r_mode", Cvar_VariableString("ui_r_mode"));
	Cvar_Set("r_fullscreen", Cvar_VariableString("ui_r_fullscreen"));
	Cvar_Set("r_colorbits", Cvar_VariableString("ui_r_colorbits"));
	Cvar_Set("r_lodbias", Cvar_VariableString("ui_r_lodbias"));
	Cvar_Set("r_picmip", Cvar_VariableString("ui_r_picmip"));
	Cvar_Set("r_texturebits", Cvar_VariableString("ui_r_texturebits"));
	Cvar_Set("r_texturemode", Cvar_VariableString("ui_r_texturemode"));
	Cvar_Set("r_detailtextures", Cvar_VariableString("ui_r_detailtextures"));
	Cvar_Set("r_ext_compress_textures", Cvar_VariableString("ui_r_ext_compress_textures"));
	Cvar_Set("r_depthbits", Cvar_VariableString("ui_r_depthbits"));
	Cvar_Set("r_subdivisions", Cvar_VariableString("ui_r_subdivisions"));
	Cvar_Set("r_fastSky", Cvar_VariableString("ui_r_fastSky"));
	Cvar_Set("r_inGameVideo", Cvar_VariableString("ui_r_inGameVideo"));
	Cvar_Set("r_allowExtensions", Cvar_VariableString("ui_r_allowExtensions"));
	Cvar_Set("cg_shadows", Cvar_VariableString("ui_cg_shadows"));
	Cvar_Set("ui_r_modified", "0");

	Cbuf_ExecuteText(EXEC_APPEND, "vid_restart;");
}

/*
=================
UI_GetVideoSetup

Retrieves the current actual video settings into the temporary user
interface versions of the cvars.
=================
*/
void UI_GetVideoSetup()
{
	Cvar_Register(nullptr, "ui_r_glCustom", "4", CVAR_ARCHIVE);

	// Make sure the cvars are registered as read only.
	Cvar_Register(nullptr, "ui_r_mode", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_fullscreen", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_colorbits", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_lodbias", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_picmip", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_texturebits", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_texturemode", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_detailtextures", "1", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_ext_compress_textures", "1", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_depthbits", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_subdivisions", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_fastSky", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_inGameVideo", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_allowExtensions", "0", CVAR_ROM);
	Cvar_Register(nullptr, "ui_cg_shadows", "1", CVAR_ROM);
	Cvar_Register(nullptr, "ui_r_modified", "0", CVAR_ROM);

	// Copy over the real video cvars into their temporary counterparts
	Cvar_Set("ui_r_mode", Cvar_VariableString("r_mode"));
	Cvar_Set("ui_r_colorbits", Cvar_VariableString("r_colorbits"));
	Cvar_Set("ui_r_fullscreen", Cvar_VariableString("r_fullscreen"));
	Cvar_Set("ui_r_lodbias", Cvar_VariableString("r_lodbias"));
	Cvar_Set("ui_r_picmip", Cvar_VariableString("r_picmip"));
	Cvar_Set("ui_r_texturebits", Cvar_VariableString("r_texturebits"));
	Cvar_Set("ui_r_texturemode", Cvar_VariableString("r_texturemode"));
	Cvar_Set("ui_r_detailtextures", Cvar_VariableString("r_detailtextures"));
	Cvar_Set("ui_r_ext_compress_textures", Cvar_VariableString("r_ext_compress_textures"));
	Cvar_Set("ui_r_depthbits", Cvar_VariableString("r_depthbits"));
	Cvar_Set("ui_r_subdivisions", Cvar_VariableString("r_subdivisions"));
	Cvar_Set("ui_r_fastSky", Cvar_VariableString("r_fastSky"));
	Cvar_Set("ui_r_inGameVideo", Cvar_VariableString("r_inGameVideo"));
	Cvar_Set("ui_r_allowExtensions", Cvar_VariableString("r_allowExtensions"));
	Cvar_Set("ui_cg_shadows", Cvar_VariableString("cg_shadows"));
	Cvar_Set("ui_r_modified", "0");
}

static void UI_SetSexandSoundForModel(const char* char_model)
{
	int f;
	char soundpath[MAX_QPATH]{};
	qboolean isFemale = qfalse;

	int i = ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", char_model), &f, FS_READ);
	if (!f)
	{
		//no?  oh bother.
		Cvar_Reset("snd");
		Cvar_Reset("sex");
		return;
	}

	soundpath[0] = 0;

	ui.FS_Read(&soundpath, i, f);

	while (i >= 0 && soundpath[i] != '\n')
	{
		if (soundpath[i] == 'f')
		{
			isFemale = qtrue;
			soundpath[i] = 0;
		}

		i--;
	}

	i = 0;

	while (soundpath[i] && soundpath[i] != '\r' && soundpath[i] != '\n')
	{
		i++;
	}
	soundpath[i] = 0;

	ui.FS_FCloseFile(f);

	Cvar_Set("snd", soundpath);
	if (isFemale)
	{
		Cvar_Set("sex", "f");
	}
	else
	{
		Cvar_Set("sex", "m");
	}
}

static void UI_UpdateCharacterCvars()
{
	const char* char_model = Cvar_VariableString("ui_char_model");
	UI_SetSexandSoundForModel(char_model);
	Cvar_Set("g_char_model", char_model);
	Cvar_Set("g_char_skin_head", Cvar_VariableString("ui_char_skin_head"));
	Cvar_Set("g_char_skin_torso", Cvar_VariableString("ui_char_skin_torso"));
	Cvar_Set("g_char_skin_legs", Cvar_VariableString("ui_char_skin_legs"));
	Cvar_Set("g_char_color_red", Cvar_VariableString("ui_char_color_red"));
	Cvar_Set("g_char_color_green", Cvar_VariableString("ui_char_color_green"));
	Cvar_Set("g_char_color_blue", Cvar_VariableString("ui_char_color_blue"));
}

static void UI_GetCharacterCvars()
{
	Cvar_Set("ui_char_skin_head", Cvar_VariableString("g_char_skin_head"));
	Cvar_Set("ui_char_skin_torso", Cvar_VariableString("g_char_skin_torso"));
	Cvar_Set("ui_char_skin_legs", Cvar_VariableString("g_char_skin_legs"));
	Cvar_Set("ui_char_color_red", Cvar_VariableString("g_char_color_red"));
	Cvar_Set("ui_char_color_green", Cvar_VariableString("g_char_color_green"));
	Cvar_Set("ui_char_color_blue", Cvar_VariableString("g_char_color_blue"));

	const char* model = Cvar_VariableString("g_char_model");
	Cvar_Set("ui_char_model", model);

	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		if (!Q_stricmp(model, uiInfo.playerSpecies[i].Name))
		{
			uiInfo.playerSpeciesIndex = i;
		}
	}
}

extern saber_colors_t TranslateSaberColor(const char* name);

static void UI_UpdateSaberCvars()
{
	if (!Cvar_VariableIntegerValue("g_NPCsaber") && !Cvar_VariableIntegerValue("g_NPCsabertwo"))
	{
		Cvar_Set("g_saber_type", Cvar_VariableString("ui_saber_type"));
		Cvar_Set("g_saber", Cvar_VariableString("ui_saber"));
		Cvar_Set("g_saber2", Cvar_VariableString("ui_saber2"));
		Cvar_Set("g_saber_color", Cvar_VariableString("ui_saber_color"));
		Cvar_Set("g_saber2_color", Cvar_VariableString("ui_saber2_color"));
#ifdef NEW_FEEDER_V9
		Cvar_Set("ui_saber_type_md", Cvar_VariableString("ui_saber_type"));
		Cvar_Set("ui_saber_md", Cvar_VariableString("ui_saber"));
		Cvar_Set("ui_saber2_md", Cvar_VariableString("ui_saber2"));
		Cvar_Set("ui_saber_color_md", Cvar_VariableString("ui_saber_color"));
		Cvar_Set("ui_saber2_color_md", Cvar_VariableString("ui_saber2_color"));
#endif
	}
	else
	{
		Cvar_Set("g_NPCsaber", Cvar_VariableString("ui_npc_saber"));
		Cvar_Set("g_NPCsabertwo", Cvar_VariableString("ui_npc_sabertwo"));

		Cvar_Set("g_NPCsabercolor", Cvar_VariableString("ui_npc_sabercolor"));
		Cvar_Set("g_NPCsabertwocolor", Cvar_VariableString("ui_npc_sabertwocolor"));
	}

	if (TranslateSaberColor(Cvar_VariableString("ui_saber_color")) >= SABER_RGB)
	{
		char rgbColor[8];
		Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber_red"),
			Cvar_VariableIntegerValue("ui_rgb_saber_green"),
			Cvar_VariableIntegerValue("ui_rgb_saber_blue"));
		if (!Cvar_VariableIntegerValue("g_NPCsaber") && !Cvar_VariableIntegerValue("g_NPCsabertwo"))
		{
			Cvar_Set("g_saber_color", rgbColor);
		}
		else
		{
			Cvar_Set("g_NPCsabercolor", rgbColor);
		}
	}

	if (TranslateSaberColor(Cvar_VariableString("ui_saber2_color")) >= SABER_RGB)
	{
		char rgbColor[8];
		Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber2_red"),
			Cvar_VariableIntegerValue("ui_rgb_saber2_green"),
			Cvar_VariableIntegerValue("ui_rgb_saber2_blue"));
		if (!Cvar_VariableIntegerValue("g_NPCsaber") && !Cvar_VariableIntegerValue("g_NPCsabertwo"))
		{
			Cvar_Set("g_saber2_color", rgbColor);
		}
		else
		{
			Cvar_Set("g_NPCsabertwocolor", rgbColor);
		}
	}
}

#ifndef JK2_MODE
static void UI_UpdateFightingStyleChoices()
{
	//
	if (!Q_stricmp("staff", Cvar_VariableString("ui_saber_type")))
	{
		Cvar_Set("ui_fightingstylesallowed", "0");
		Cvar_Set("ui_newfightingstyle", "4"); // SS_STAFF
	}
	else if (!Q_stricmp("dual", Cvar_VariableString("ui_saber_type")))
	{
		Cvar_Set("ui_fightingstylesallowed", "0");
		Cvar_Set("ui_newfightingstyle", "3"); // SS_DUAL
	}
	else
	{
		// Get player state
		const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

		if (cl && cl->gentity && cl->gentity->client)
		{
			const playerState_t* pState = cl->gentity->client;

			// Knows Fast style?
			if (pState->saberStylesKnown & 1 << SS_FAST)
			{
				// And Medium?
				if (pState->saberStylesKnown & 1 << SS_MEDIUM)
				{
					Cvar_Set("ui_fightingstylesallowed", "6"); // Has FAST and MEDIUM, so can only choose STRONG
					Cvar_Set("ui_newfightingstyle", "2"); // STRONG
				}
				else
				{
					Cvar_Set("ui_fightingstylesallowed", "1"); // Has FAST, so can choose from MEDIUM and STRONG
					Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
				}
			}
			// Knows Medium style?
			else if (pState->saberStylesKnown & 1 << SS_MEDIUM)
			{
				// And Strong?
				if (pState->saberStylesKnown & 1 << SS_STRONG)
				{
					Cvar_Set("ui_fightingstylesallowed", "4"); // Has MEDIUM and STRONG, so can only choose FAST
					Cvar_Set("ui_newfightingstyle", "0"); // FAST
				}
				else
				{
					Cvar_Set("ui_fightingstylesallowed", "2"); // Has MEDIUM, so can choose from FAST and STRONG
					Cvar_Set("ui_newfightingstyle", "0"); // FAST
				}
			}
			// Knows Strong style?
			else if (pState->saberStylesKnown & 1 << SS_STRONG)
			{
				// And Fast
				if (pState->saberStylesKnown & 1 << SS_FAST)
				{
					Cvar_Set("ui_fightingstylesallowed", "5"); // Has STRONG and FAST, so can only take MEDIUM
					Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
				}
				else
				{
					Cvar_Set("ui_fightingstylesallowed", "3"); // Has STRONG, so can choose from FAST and MEDIUM
					Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
				}
			}
			else // They have nothing, which should not happen
			{
				Cvar_Set("ui_currentfightingstyle", "1"); // Default MEDIUM
				Cvar_Set("ui_newfightingstyle", "0"); // FAST??
				Cvar_Set("ui_fightingstylesallowed", "0"); // Default to no new styles allowed
			}

			// Determine current style
			if (pState->saber_anim_level == SS_FAST)
			{
				Cvar_Set("ui_currentfightingstyle", "0"); // FAST
			}
			else if (pState->saber_anim_level == SS_STRONG)
			{
				Cvar_Set("ui_currentfightingstyle", "2"); // STRONG
			}
			else
			{
				Cvar_Set("ui_currentfightingstyle", "1"); // default MEDIUM
			}
		}
		else // No client so this must be first time
		{
			Cvar_Set("ui_currentfightingstyle", "1"); // Default to MEDIUM
			Cvar_Set("ui_fightingstylesallowed", "0"); // Default to no new styles allowed
			Cvar_Set("ui_newfightingstyle", "1"); // MEDIUM
		}
	}
}
#endif // !JK2_MODE

constexpr auto MAX_POWER_ENUMS = 25;

using powerEnum_t = struct
{
	const char* title;
	short powerEnum;
};

static powerEnum_t powerEnums[MAX_POWER_ENUMS] =
{
#ifndef JK2_MODE
	{"absorb", FP_ABSORB},
#endif // !JK2_MODE

	{"heal", FP_HEAL},
	{"mindtrick", FP_TELEPATHY},

#ifndef JK2_MODE
	{"protect", FP_PROTECT},
	{"stasis", FP_STASIS},
#endif // !JK2_MODE

	// Core powers
	{"jump", FP_LEVITATION},
	{"pull", FP_PULL},
	{"push", FP_PUSH},

#ifndef JK2_MODE
	{"sense", FP_SEE},
#endif // !JK2_MODE

	{"speed", FP_SPEED},
	{"sabdef", FP_SABER_DEFENSE},
	{"saboff", FP_SABER_OFFENSE},
	{"sabthrow", FP_SABERTHROW},

	// Dark powers
#ifndef JK2_MODE
	{"drain", FP_DRAIN},
#endif // !JK2_MODE

	{"grip", FP_GRIP},
	{"lightning", FP_LIGHTNING},

#ifndef JK2_MODE
	{"rage", FP_RAGE},
	{"destruction", FP_DESTRUCTION},

	{"grasp", FP_GRASP},
	{"repulse", FP_REPULSE},
	{"strike", FP_LIGHTNING_STRIKE},
	{"fear", FP_FEAR},
	{"deadlysight", FP_DEADLYSIGHT},
	{"projection", FP_PROJECTION},
	{"blast", FP_BLAST},
#endif // !JK2_MODE
};

// Find the index to the Force Power in powerEnum array
static qboolean UI_GetForcePowerIndex(const char* forceName, short* forcePowerI)
{
	// Find a match for the forceName passed in
	for (int i = 0; i < MAX_POWER_ENUMS; i++)
	{
		if (!Q_stricmp(forceName, powerEnums[i].title))
		{
			*forcePowerI = i;
			return qtrue;
		}
	}

	*forcePowerI = FP_UPDATED_NONE; // Didn't find it

	return qfalse;
}

// Set the fields for the allocation of force powers (Used by Force Power Allocation screen)
static void UI_InitAllocForcePowers(const char* forceName)
{
	short forcePowerI = 0;
	int forcelevel;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex(forceName, &forcePowerI))
	{
		return;
	}

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE: this UIScript can be called outside the running game now, so handle that case
	// by getting info frim UIInfo instead of PlayerState
	if (cl)
	{
		const playerState_t* pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	char itemName[128];
	Com_sprintf(itemName, sizeof itemName, "%s_hexpic", powerEnums[forcePowerI].title);
	auto item = Menu_FindItemByName(menu, itemName);

	if (item)
	{
		char itemGraphic[128];
		Com_sprintf(itemGraphic, sizeof itemGraphic, "gfx/menus/hex_pattern_%d", forcelevel >= 4 ? 3 : forcelevel);
		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);

		// If maxed out on power - don't allow update
		if (forcelevel >= 3)
		{
			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[forcePowerI].title);
			item = Menu_FindItemByName(menu, itemName);
			if (item)
			{
				item->action = nullptr; //you are bad, no action for you!
				item->descText = nullptr; //no desc either!
			}
		}
	}

	// Set weapons button to inactive
	UI_ForcePowerWeaponsButton(qfalse);
}

// Flip flop between being able to see the text showing the Force Point has or hasn't been allocated (Used by Force Power Allocation screen)
static void UI_SetPowerTitleText(const qboolean showAllocated)
{
	itemDef_t* item;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (showAllocated)
	{
		// Show the text saying the force point has been allocated
		item = Menu_FindItemByName(menu, "allocated_text");
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}

		// Hide text saying the force point needs to be allocated
		item = Menu_FindItemByName(menu, "allocate_text");
		if (item)
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}
	}
	else
	{
		// Hide the text saying the force point has been allocated
		item = Menu_FindItemByName(menu, "allocated_text");
		if (item)
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}

		// Show text saying the force point needs to be allocated
		item = Menu_FindItemByName(menu, "allocate_text");
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}
}

static int UI_CountForcePowers(void)
{
	const client_t* cl = &svs.clients[0];

	if (cl && cl->gentity)
	{
		const playerState_t* ps = cl->gentity->client;

		return ps->forcePowerLevel[FP_HEAL] +
			ps->forcePowerLevel[FP_TELEPATHY] +
			ps->forcePowerLevel[FP_PROTECT] +
			ps->forcePowerLevel[FP_ABSORB] +
			ps->forcePowerLevel[FP_GRIP] +
			ps->forcePowerLevel[FP_LIGHTNING] +
			ps->forcePowerLevel[FP_RAGE] +
			ps->forcePowerLevel[FP_DRAIN];
	}
	else
	{
		return uiInfo.forcePowerLevel[FP_HEAL] +
			uiInfo.forcePowerLevel[FP_TELEPATHY] +
			uiInfo.forcePowerLevel[FP_PROTECT] +
			uiInfo.forcePowerLevel[FP_ABSORB] +
			uiInfo.forcePowerLevel[FP_GRIP] +
			uiInfo.forcePowerLevel[FP_LIGHTNING] +
			uiInfo.forcePowerLevel[FP_RAGE] +
			uiInfo.forcePowerLevel[FP_DRAIN];
	}
}

//. Find weapons button and make active/inactive  (Used by Force Power Allocation screen)
static void UI_ForcePowerWeaponsButton(qboolean activeFlag)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// Cheats are on so lets always let us pass
	if ((trap_Cvar_VariableValue("helpUsObi") != 0) || (UI_CountForcePowers() >= 24))
	{
		activeFlag = qtrue;
	}

	const auto item = Menu_FindItemByName(menu, "weaponbutton");
	if (item)
	{
		// Make it active
		if (activeFlag)
		{
			item->window.flags &= ~WINDOW_INACTIVE;
		}
		else
		{
			item->window.flags |= WINDOW_INACTIVE;
		}
	}
}

static void UI_ViewWeaponWheel() {
	struct WeaponIcon
	{
		const char* iconName;
		int weaponIndex;
	};
	const WeaponIcon weaponIcons[] = {
		{"melee_icon", 14},
		{"saber_icon", 1},
		{"stun_baton_icon", 17},
		{"noghristick_icon", 33},
		{"blaster_pistol_icon", 2},
		{"clone_pistol_icon", 45},
		{"lpann14_icon", 42},
		{"westar_icon", 43},
		{"briar_icon", 18},
		{"dh17_icon", 37},
		{"blaster_icon", 3},
		{"a280_icon", 41},
		{"clone_rifle_icon", 38},
		{"dc15s_icon", 36},
		{"dc17m_icon", 39},
		{"e5_icon", 34},
		{"ee3_icon", 44},
		{"f11d_icon", 35},
		{"repeater_icon", 6},
		{"rotary_cannon_icon", 40},
		{"bowcaster_icon", 5},
		{"rocket_launcher_icon", 9},
		{"demp2_icon", 7},
		{"concussion_icon", 13},
		{"flechette_icon", 8},
		{"disruptor_icon", 4},
		{"thermal_icon", 10},
		{"trip_mine_icon", 11},
		{"det_pack_icon", 12},
	};

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl)
	{
		return; // No client, get out
	}

	const playerState_t* pState = cl->gentity->client;
	const menuDef_t* menu = Menu_GetFocused();

	// Enable/disable weapon icons if in player inventory
	for (const auto& weaponIcon : weaponIcons)
	{
		const auto item = Menu_FindItemByName(menu, weaponIcon.iconName);
		if (item)
		{
			if (pState->weapons[weaponIcon.weaponIndex]) {
				item->disabled = qfalse;
				item->window.foreColor[3] = 1.0f;
			}
			else {
				item->window.foreColor[3] = 0.4f;
				item->disabled = qtrue;
			}
		}
	}
}

static void UI_ViewForceWheel() {
	struct ForceIcon
	{
		const char* iconName;
		int forceIndex;
	};
	const ForceIcon forceIcons[] = {
		{"absorb_icon", 0},
		{"heal_icon", 1},
		{"telepathy_icon", 2},
		{"protect_icon", 3},
		{"stasis_icon", 4},
		{"pull_icon", 6},
		{"push_icon", 7},
		{"sight_icon", 8},
		{"speed_icon", 9},
		{"drain_icon", 13},
		{"grip_icon", 14},
		{"lightning_icon", 15},
		{"rage_icon", 16},
		{"destruction_icon", 17},
		{"grasp_icon", 18},
		{"repulse_icon", 19},
		{"strike_icon", 20},
		{"fear_icon", 21},
		{"deadly_sight_icon", 22},
		{"projection_icon", 23},
		{"blast_icon", 24},
	};

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl)
	{
		return; // No client, get out
	}

	const playerState_t* pState = cl->gentity->client;
	const menuDef_t* menu = Menu_GetFocused();

	// Enable/disable force icons if player has ability
	for (const auto& forceIcon : forceIcons)
	{
		const auto item = Menu_FindItemByName(menu, forceIcon.iconName);
		if (item)
		{
			if (pState->forcePowerLevel[powerEnums[forceIcon.forceIndex].powerEnum] > 0)
			{
				item->disabled = qfalse;
				item->window.foreColor[3] = 1.0f;
			}
			else {
				item->window.foreColor[3] = 0.4f;
				item->disabled = qtrue;
			}
		}
	}
}

void UI_SetItemColor(const itemDef_t* item, const char* itemname, const char* name, vec4_t color);

static void UI_SetHexPicLevel(const menuDef_t* menu, const int forcePowerI, const int powerLevel,
	const qboolean goldFlag)
{
	char itemName[128];

	// Find proper hex picture on menu
	Com_sprintf(itemName, sizeof itemName, "%s_hexpic", powerEnums[forcePowerI].title);
	auto item = Menu_FindItemByName(menu, itemName);

	// Now give it the proper hex graphic
	if (item)
	{
		char itemGraphic[128];
		if (goldFlag)
		{
			Com_sprintf(itemGraphic, sizeof itemGraphic, "gfx/menus/hex_pattern_%d_gold",
				powerLevel >= 4 ? 3 : powerLevel);
		}
		else
		{
			Com_sprintf(itemGraphic, sizeof itemGraphic, "gfx/menus/hex_pattern_%d", powerLevel >= 4 ? 3 : powerLevel);
		}

		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);

		Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[forcePowerI].title);
		item = Menu_FindItemByName(menu, itemName);
		if (item)
		{
			if (goldFlag)
			{
				// Change description text to tell player they can decrement the force point
				item->descText = "@MENUS_REMOVEFP";
			}
			else
			{
				// Change description text to tell player they can increment the force point
				item->descText = "@MENUS_ADDFP";
			}
		}
	}
}

void UI_SetItemVisible(const menuDef_t* menu, const char* itemname, qboolean visible);

// if this is the first time into the force power allocation screen, show the INSTRUCTION screen
static void UI_ForceHelpActive()
{
	const int tier_storyinfo = Cvar_VariableIntegerValue("tier_storyinfo");

	// First time, show instructions
	if (tier_storyinfo == 1)
	{
		//		Menus_OpenByName("ingameForceHelp");
		Menus_ActivateByName("ingameForceHelp");
	}
}

#ifndef JK2_MODE
// Set the force levels depending on the level chosen
static void UI_DemoSetForceLevels()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	char buffer[MAX_STRING_CHARS];

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	const playerState_t* pState = nullptr;
	if (cl)
	{
		pState = cl->gentity->client;
	}

	ui.Cvar_VariableStringBuffer("ui_demo_level", buffer, sizeof buffer);
	if (Q_stricmp(buffer, "t1_sour") == 0)
	{
		// NOTE : always set the uiInfo powers
		// level 1 in all core powers
		uiInfo.forcePowerLevel[FP_LEVITATION] = 1;
		uiInfo.forcePowerLevel[FP_SPEED] = 1;
		uiInfo.forcePowerLevel[FP_PUSH] = 1;
		uiInfo.forcePowerLevel[FP_PULL] = 1;
		uiInfo.forcePowerLevel[FP_SEE] = 1;
		uiInfo.forcePowerLevel[FP_SABER_OFFENSE] = 1;
		uiInfo.forcePowerLevel[FP_SABER_DEFENSE] = 1;
		uiInfo.forcePowerLevel[FP_SABERTHROW] = 1;
		// plus these extras
		uiInfo.forcePowerLevel[FP_HEAL] = 1;
		uiInfo.forcePowerLevel[FP_TELEPATHY] = 1;
		uiInfo.forcePowerLevel[FP_GRIP] = 1;

		// and set the rest to zero
		uiInfo.forcePowerLevel[FP_ABSORB] = 0;
		uiInfo.forcePowerLevel[FP_PROTECT] = 0;
		uiInfo.forcePowerLevel[FP_DRAIN] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING] = 0;
		uiInfo.forcePowerLevel[FP_RAGE] = 0;

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = 0;
		uiInfo.forcePowerLevel[FP_STASIS] = 0;

		uiInfo.forcePowerLevel[FP_GRASP] = 0;
		uiInfo.forcePowerLevel[FP_REPULSE] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = 0;
		uiInfo.forcePowerLevel[FP_FEAR] = 0;
		uiInfo.forcePowerLevel[FP_DEADLYSIGHT] = 0;
		uiInfo.forcePowerLevel[FP_BLAST] = 0;
		uiInfo.forcePowerLevel[FP_PROJECTION] = 0;
	}
	else
	{
		// level 3 in all core powers
		uiInfo.forcePowerLevel[FP_LEVITATION] = 3;
		uiInfo.forcePowerLevel[FP_SPEED] = 3;
		uiInfo.forcePowerLevel[FP_PUSH] = 3;
		uiInfo.forcePowerLevel[FP_PULL] = 3;
		uiInfo.forcePowerLevel[FP_SEE] = 3;
		uiInfo.forcePowerLevel[FP_SABER_OFFENSE] = 3;
		uiInfo.forcePowerLevel[FP_SABER_DEFENSE] = 3;
		uiInfo.forcePowerLevel[FP_SABERTHROW] = 3;

		// plus these extras
		uiInfo.forcePowerLevel[FP_HEAL] = 1;
		uiInfo.forcePowerLevel[FP_TELEPATHY] = 1;
		uiInfo.forcePowerLevel[FP_GRIP] = 2;
		uiInfo.forcePowerLevel[FP_LIGHTNING] = 1;
		uiInfo.forcePowerLevel[FP_PROTECT] = 1;

		// and set the rest to zero

		uiInfo.forcePowerLevel[FP_ABSORB] = 0;
		uiInfo.forcePowerLevel[FP_DRAIN] = 0;
		uiInfo.forcePowerLevel[FP_RAGE] = 0;

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = 0;
		uiInfo.forcePowerLevel[FP_STASIS] = 0;

		uiInfo.forcePowerLevel[FP_GRASP] = 0;
		uiInfo.forcePowerLevel[FP_REPULSE] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = 0;
		uiInfo.forcePowerLevel[FP_FEAR] = 0;
		uiInfo.forcePowerLevel[FP_DEADLYSIGHT] = 0;
		uiInfo.forcePowerLevel[FP_BLAST] = 0;
		uiInfo.forcePowerLevel[FP_PROJECTION] = 0;
	}

	if (pState)
	{
		//i am carrying over from a previous level, so get the increased power! (non-core only)
		uiInfo.forcePowerLevel[FP_HEAL] = Q_max(pState->forcePowerLevel[FP_HEAL], uiInfo.forcePowerLevel[FP_HEAL]);
		uiInfo.forcePowerLevel[FP_TELEPATHY] = Q_max(pState->forcePowerLevel[FP_TELEPATHY],
			uiInfo.forcePowerLevel[FP_TELEPATHY]);
		uiInfo.forcePowerLevel[FP_GRIP] = Q_max(pState->forcePowerLevel[FP_GRIP], uiInfo.forcePowerLevel[FP_GRIP]);
		uiInfo.forcePowerLevel[FP_LIGHTNING] = Q_max(pState->forcePowerLevel[FP_LIGHTNING],
			uiInfo.forcePowerLevel[FP_LIGHTNING]);
		uiInfo.forcePowerLevel[FP_PROTECT] = Q_max(pState->forcePowerLevel[FP_PROTECT],
			uiInfo.forcePowerLevel[FP_PROTECT]);

		uiInfo.forcePowerLevel[FP_ABSORB] =
			Q_max(pState->forcePowerLevel[FP_ABSORB], uiInfo.forcePowerLevel[FP_ABSORB]);
		uiInfo.forcePowerLevel[FP_DRAIN] = Q_max(pState->forcePowerLevel[FP_DRAIN], uiInfo.forcePowerLevel[FP_DRAIN]);
		uiInfo.forcePowerLevel[FP_RAGE] = Q_max(pState->forcePowerLevel[FP_RAGE], uiInfo.forcePowerLevel[FP_RAGE]);

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = Q_max(pState->forcePowerLevel[FP_DESTRUCTION],
			uiInfo.forcePowerLevel[FP_DESTRUCTION]);
		uiInfo.forcePowerLevel[FP_STASIS] =
			Q_max(pState->forcePowerLevel[FP_STASIS], uiInfo.forcePowerLevel[FP_STASIS]);

		uiInfo.forcePowerLevel[FP_GRASP] = Q_max(pState->forcePowerLevel[FP_GRASP], uiInfo.forcePowerLevel[FP_GRASP]);
		uiInfo.forcePowerLevel[FP_REPULSE] = Q_max(pState->forcePowerLevel[FP_REPULSE],
			uiInfo.forcePowerLevel[FP_REPULSE]);
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = Q_max(pState->forcePowerLevel[FP_LIGHTNING_STRIKE],
			uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE]);
		uiInfo.forcePowerLevel[FP_FEAR] = Q_max(pState->forcePowerLevel[FP_FEAR], uiInfo.forcePowerLevel[FP_FEAR]);
		uiInfo.forcePowerLevel[FP_DEADLYSIGHT] = Q_max(pState->forcePowerLevel[FP_DEADLYSIGHT],
			uiInfo.forcePowerLevel[FP_DEADLYSIGHT]);
		uiInfo.forcePowerLevel[FP_PROJECTION] = Q_max(pState->forcePowerLevel[FP_PROJECTION],
			uiInfo.forcePowerLevel[FP_PROJECTION]);
		uiInfo.forcePowerLevel[FP_BLAST] = Q_max(pState->forcePowerLevel[FP_BLAST], uiInfo.forcePowerLevel[FP_BLAST]);
	}
}
#endif // !JK2_MODE

// record the force levels into a cvar so when restoring player from map transition
// the force levels are set up correctly
static void UI_RecordForceLevels()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	auto s2 = "";
	for (const int i : uiInfo.forcePowerLevel)
	{
		s2 = va("%s %i", s2, i);
	}
	Cvar_Set("demo_playerfplvl", s2);
}

// record the weapons into a cvar so when restoring player from map transition
// the force levels are set up correctly
static void UI_RecordWeapons()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	int wpns = 0;
	// always add blaster and saber
	wpns |= 1 << WP_SABER;
	wpns |= 1 << WP_BLASTER_PISTOL;
	wpns |= 1 << uiInfo.selectedWeapon1;
	wpns |= 1 << uiInfo.selectedWeapon2;
	wpns |= 1 << uiInfo.selectedThrowWeapon;
	wpns |= 1 << uiInfo.selectedPistolWeapon;
	const char* s2 = va("%i", wpns);

	Cvar_Set("demo_playerwpns", s2);
}

// Shut down the help screen in the force power allocation screen
static void UI_ShutdownForceHelp()
{
	char itemName[128];
	itemDef_t* item;
	vec4_t color = { 0.65f, 0.65f, 0.65f, 1.0f };

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// Not in upgrade mode so turn on all the force buttons, the big forceicon and description text
	if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
	{
		// We just decremented a field so turn all buttons back on
		// Make it so all  buttons can be clicked
		for (const auto& powerEnum : powerEnums)
		{
			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnum.title);

			UI_SetItemVisible(menu, itemName, qtrue);
		}

		UI_SetItemVisible(menu, "force_icon", qtrue);
		UI_SetItemVisible(menu, "force_desc", qtrue);
		UI_SetItemVisible(menu, "leveltext", qtrue);

		// Set focus on the top left button
		item = Menu_FindItemByName(menu, "absorb_fbutton");
		item->window.flags |= WINDOW_HASFOCUS;

		if (item->onFocus)
		{
			Item_RunScript(item, item->onFocus);
		}
	}
	// In upgrade mode so just turn the deallocate button on
	else
	{
		UI_SetItemVisible(menu, "force_icon", qtrue);
		UI_SetItemVisible(menu, "force_desc", qtrue);
		UI_SetItemVisible(menu, "deallocate_fbutton", qtrue);

		item = Menu_FindItemByName(menu, va("%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title));
		if (item)
		{
			item->window.flags |= WINDOW_HASFOCUS;
		}

		// Get player state
		const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

		if (!cl) // No client, get out
		{
			return;
		}

		const playerState_t* pState = cl->gentity->client;

		if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
		{
			return;
		}

		// Update level description
		Com_sprintf(
			itemName,
			sizeof itemName,
			"%s_level%ddesc",
			powerEnums[uiInfo.forcePowerUpdated].title,
			pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]
		);

		item = Menu_FindItemByName(menu, itemName);
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	// If one was a chosen force power, high light it again.
	if (uiInfo.forcePowerUpdated > FP_UPDATED_NONE)
	{
		char itemhexName[128];
		char itemiconName[128];
		vec4_t color2 = { 1.0f, 1.0f, 1.0f, 1.0f };

		Com_sprintf(itemhexName, sizeof itemhexName, "%s_hexpic", powerEnums[uiInfo.forcePowerUpdated].title);
		Com_sprintf(itemiconName, sizeof itemiconName, "%s_iconpic", powerEnums[uiInfo.forcePowerUpdated].title);

		UI_SetItemColor(item, itemhexName, "forecolor", color2);
		UI_SetItemColor(item, itemiconName, "forecolor", color2);
	}
	else
	{
		// Un-grey-out all icons
		UI_SetItemColor(item, "hexpic", "forecolor", color);
		UI_SetItemColor(item, "iconpic", "forecolor", color);
	}
}

// Decrement force power level (Used by Force Power Allocation screen)
static void UI_DecrementCurrentForcePower()
{
	itemDef_t* item;
	vec4_t color = { 0.65f, 0.65f, 0.65f, 1.0f };

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	playerState_t* pState = nullptr;
	int forcelevel;

	if (cl)
	{
		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
	}

	if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
	{
		return;
	}

	DC->startLocalSound(uiInfo.uiDC.Assets.forceUnchosenSound, CHAN_AUTO);

	if (forcelevel > 0)
	{
		if (pState)
		{
			pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--; // Decrement it
			forcelevel = pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
			// Turn off power if level is 0
			if (pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum] < 1)
			{
				pState->forcePowersKnown &= ~(1 << powerEnums[uiInfo.forcePowerUpdated].powerEnum);
			}
		}
		else
		{
			uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--; // Decrement it
			forcelevel = uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
		}
	}

	UI_SetHexPicLevel(menu, uiInfo.forcePowerUpdated, forcelevel, qfalse);

	UI_ShowForceLevelDesc(powerEnums[uiInfo.forcePowerUpdated].title);

	// We just decremented a field so turn all buttons back on
	// Make it so all  buttons can be clicked
	for (const auto& powerEnum : powerEnums)
	{
		char itemName[128];
		Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnum.title);
		item = Menu_FindItemByName(menu, itemName);
		if (item) // This is okay, because core powers don't have a hex button
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	// Show point has not been allocated
	UI_SetPowerTitleText(qfalse);

	// Make weapons button inactive
	UI_ForcePowerWeaponsButton(qfalse);

	// Hide the deallocate button
	item = Menu_FindItemByName(menu, "deallocate_fbutton");
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE; //

		// Un-grey-out all icons
		UI_SetItemColor(item, "hexpic", "forecolor", color);
		UI_SetItemColor(item, "iconpic", "forecolor", color);
	}

	item = Menu_FindItemByName(menu, va("%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title));
	if (item)
	{
		item->window.flags |= WINDOW_HASFOCUS;
	}

	uiInfo.forcePowerUpdated = FP_UPDATED_NONE; // It's as if nothing happened.
}

void Item_MouseEnter(itemDef_t* item, float x, float y);

// Try to increment force power level (Used by Force Power Allocation screen)
static void UI_AffectForcePowerLevel(const char* forceName)
{
	short forcePowerI = 0;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex(forceName, &forcePowerI))
	{
		return;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player
	playerState_t* pState = nullptr;
	int forcelevel;
	if (cl)
	{
		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	if (forcelevel > 2)
	{
		// Too big, can't be incremented
		return;
	}

	// Increment power level.
	DC->startLocalSound(uiInfo.uiDC.Assets.forceChosenSound, CHAN_AUTO);

	uiInfo.forcePowerUpdated = forcePowerI; // Remember which power was updated

	if (pState)
	{
		pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]++; // Increment it
		pState->forcePowersKnown |= 1 << powerEnums[forcePowerI].powerEnum;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum]++; // Increment it
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	UI_SetHexPicLevel(menu, uiInfo.forcePowerUpdated, forcelevel, qtrue);

	UI_ShowForceLevelDesc(forceName);

	// A field was updated, so make it so others can't be
	if (uiInfo.forcePowerUpdated > FP_UPDATED_NONE)
	{
		itemDef_t* item;
		vec4_t color = { 0.25f, 0.25f, 0.25f, 1.0f };

		// Make it so none of the other buttons can be clicked
		for (short i = 0; i < MAX_POWER_ENUMS; i++)
		{
			char itemName[128];
			if (i == uiInfo.forcePowerUpdated)
			{
				continue;
			}

			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[i].title);
			item = Menu_FindItemByName(menu, itemName);
			if (item) // This is okay, because core powers don't have a hex button
			{
				item->window.flags &= ~WINDOW_VISIBLE;
			}
		}

		// Show point has been allocated
		UI_SetPowerTitleText(qtrue);

		// Make weapons button active
		UI_ForcePowerWeaponsButton(qtrue);

		// Make user_info
		Cvar_Set("ui_forcepower_inc", va("%d", uiInfo.forcePowerUpdated));

		// Just grab an item to hand it to the function.
		item = Menu_FindItemByName(menu, "deallocate_fbutton");
		if (item)
		{
			// Show all icons as greyed-out
			UI_SetItemColor(item, "hexpic", "forecolor", color);
			UI_SetItemColor(item, "iconpic", "forecolor", color);

			item->window.flags |= WINDOW_HASFOCUS;
		}
	}
}

static void UI_DecrementForcePowerLevel()
{
	const int forcePowerI = Cvar_VariableIntegerValue("ui_forcepower_inc");
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	playerState_t* pState = cl->gentity->client;

	pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]--; // Decrement it
}

// Show force level description that matches current player level (Used by Force Power Allocation screen)
static void UI_ShowForceLevelDesc(const char* forceName)
{
	short forcePowerI = 0;
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex(forceName, &forcePowerI))
	{
		return;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}
	const playerState_t* pState = cl->gentity->client;

	char itemName[128];

	// Update level description
	Com_sprintf(
		itemName,
		sizeof itemName,
		"%s_level%ddesc",
		powerEnums[forcePowerI].title,
		pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]
	);

	const auto item = Menu_FindItemByName(menu, itemName);
	if (item)
	{
		item->window.flags |= WINDOW_VISIBLE;
	}
}

// Reset force level powers screen to what it was before player upgraded them (Used by Force Power Allocation screen)
static void UI_ResetForceLevels()
{
	// What force ppower had the point added to it?
	if (uiInfo.forcePowerUpdated != FP_UPDATED_NONE)
	{
		// Get player state
		const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

		if (!cl) // No client, get out
		{
			return;
		}
		playerState_t* pState = cl->gentity->client;

		// Decrement that power
		pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--;

		itemDef_t* item;

		const menuDef_t* menu = Menu_GetFocused(); // Get current menu

		if (!menu)
		{
			return;
		}

		char itemName[128];

		// Make it so all  buttons can be clicked
		for (const auto& powerEnum : powerEnums)
		{
			Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnum.title);
			item = Menu_FindItemByName(menu, itemName);
			if (item) // This is okay, because core powers don't have a hex button
			{
				item->window.flags |= WINDOW_VISIBLE;
			}
		}

		UI_SetPowerTitleText(qfalse);

		Com_sprintf(itemName, sizeof itemName, "%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title);
		item = Menu_FindItemByName(menu, itemName);
		if (item)
		{
			// Change description text to tell player they can increment the force point
			item->descText = "@MENUS_ADDFP";
		}

		uiInfo.forcePowerUpdated = FP_UPDATED_NONE;
	}

	UI_ForcePowerWeaponsButton(qfalse);
}

#ifndef JK2_MODE
// Set the Players known saber style
static void UI_UpdateFightingStyle()
{
	int saberStyle;

	const int fightingStyle = Cvar_VariableIntegerValue("ui_newfightingstyle");

	if (fightingStyle == 1)
	{
		saberStyle = SS_MEDIUM;
	}
	else if (fightingStyle == 2)
	{
		saberStyle = SS_STRONG;
	}
	else if (fightingStyle == 3)
	{
		saberStyle = SS_DUAL;
	}
	else if (fightingStyle == 4)
	{
		saberStyle = SS_STAFF;
	}
	else // 0 is Fast
	{
		saberStyle = SS_FAST;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// No client, get out
	if (cl && cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;
		pState->saberStylesKnown |= 1 << saberStyle;
	}
	else // Must be at the beginning of the game so the client hasn't been created, shove data in a cvar
	{
		Cvar_Set("g_fighting_style", va("%d", saberStyle));
	}
}
#endif // !JK2_MODE

static void UI_ResetCharacterListBoxes()
{
	const menuDef_t* menu = Menus_FindByName("characterMenu");

	if (menu)
	{
		listBoxDef_t* listPtr;
		auto item = Menu_FindItemByName(menu, "headlistbox");
		if (item)
		{
			const auto list_box_def_s = static_cast<listBoxDef_t*>(item->typeData);
			if (list_box_def_s)
			{
				list_box_def_s->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = Menu_FindItemByName(menu, "torsolistbox");
		if (item)
		{
			listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = Menu_FindItemByName(menu, "lowerlistbox");
		if (item)
		{
			listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = Menu_FindItemByName(menu, "colorbox");
		if (item)
		{
			listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}
	}
}

static void UI_ClearInventory()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		for (int& i : pState->inventory)
		{
			i = 0;
		}
	}
}

static void UI_GiveInventory(const int itemIndex, const int amount)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (itemIndex < MAX_INVENTORY)
		{
			pState->inventory[itemIndex] = amount;
		}
	}
}

//. Find weapons allocation screen BEGIN button and make active/inactive
static void UI_WeaponAllocBeginButton(const qboolean activeFlag)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const int weap = Cvar_VariableIntegerValue("weapon_menu");

	itemDef_t* item = Menu_GetMatchingItemByNumber(menu, weap, "beginmission");

	if (item)
	{
		// Make it active
		if (activeFlag)
		{
			item->window.flags &= ~WINDOW_INACTIVE;
		}
		else
		{
			item->window.flags |= WINDOW_INACTIVE;
		}
	}
}

// If we have both weapons and the throwable weapon, turn on the begin mission button,
// otherwise, turn it off
static void UI_WeaponsSelectionsComplete()
{
	// We need two weapons and one throwable
	if (uiInfo.selectedWeapon1 != NOWEAPON &&
		uiInfo.selectedWeapon2 != NOWEAPON &&
		uiInfo.selectedThrowWeapon != NOWEAPON &&
		uiInfo.selectedPistolWeapon != NOWEAPON)
	{
		UI_WeaponAllocBeginButton(qtrue); // Turn it on
	}
	else
	{
		UI_WeaponAllocBeginButton(qfalse); // Turn it off
	}
}

// if this is the first time into the weapon allocation screen, show the INSTRUCTION screen
static void UI_WeaponHelpActive()
{
	const int tier_storyinfo = Cvar_VariableIntegerValue("tier_storyinfo");

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	// First time, show instructions
	if (tier_storyinfo == 1)
	{
		UI_SetItemVisible(menu, "weapon_button", qfalse);

		UI_SetItemVisible(menu, "inst_stuff", qtrue);
	}
	// just act like normal
	else
	{
		UI_SetItemVisible(menu, "weapon_button", qtrue);

		UI_SetItemVisible(menu, "inst_stuff", qfalse);
	}
}

static void UI_InitWeaponSelect()
{
	UI_WeaponAllocBeginButton(qfalse);
	uiInfo.selectedWeapon1 = NOWEAPON;
	uiInfo.selectedWeapon2 = NOWEAPON;
	uiInfo.selectedThrowWeapon = NOWEAPON;
	uiInfo.selectedPistolWeapon = NOWEAPON;
}

static void UI_clearsabers()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		// Clear out any weapons for the player
		if (pState->stats[STAT_WEAPONS] |= 1 << WP_SABER)
		{
			pState->stats[STAT_WEAPONS] |= 1 << WP_MELEE;

			pState->weapon = WP_MELEE;
		}
	}
}

static void UI_ClearWeapons()
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		// Clear out any weapons for the player
		for (char& weapon : pState->weapons)
		{
			weapon = 0;
		}

		pState->weapon = WP_NONE;
	}
}

static void UI_GiveAmmo(const int ammoIndex, const int ammoAmount, const char* soundfile)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (ammoIndex < AMMO_MAX)
		{
			pState->ammo[ammoIndex] = ammoAmount;
		}

		if (soundfile)
		{
			DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
		}
	}
}

static void UI_GiveWeapon(const int weaponIndex)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (weaponIndex < WP_NUM_WEAPONS)
		{
			pState->weapons[weaponIndex] = 1;
		}
	}
}

static void UI_EquipWeapon(const int weaponIndex)
{
	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	if (!cl) // No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		if (weaponIndex < WP_NUM_WEAPONS)
		{
			pState->weapon = weaponIndex;
			//force it to change
			//CG_ChangeWeapon( wp );
		}
	}
}

static void UI_LoadMissionSelectMenu(const char* cvarName)
{
	const int holdLevel = static_cast<int>(trap_Cvar_VariableValue(cvarName));

	// Figure out which tier menu to load
	if (holdLevel > 0 && holdLevel < 5)
	{
		UI_LoadMenus("ui/tier1.txt", qfalse);

		Menus_CloseByName("ingameMissionSelect1");
	}
	else if (holdLevel > 6 && holdLevel < 10)
	{
		UI_LoadMenus("ui/tier2.txt", qfalse);

		Menus_CloseByName("ingameMissionSelect2");
	}
	else if (holdLevel > 11 && holdLevel < 15)
	{
		UI_LoadMenus("ui/tier3.txt", qfalse);

		Menus_CloseByName("ingameMissionSelect3");
	}
}

// Update the player weapons with the chosen weapon
static void UI_AddWeaponSelection(const int weaponIndex, const int ammoIndex, const int ammoAmount,
	const char* iconItemName, const char* litIconItemName, const char* hexBackground,
	const char* soundfile)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const itemDef_s* iconItem = Menu_FindItemByName(menu, iconItemName);
	const itemDef_s* litIconItem = Menu_FindItemByName(menu, litIconItemName);

	const char* chosenItemName, * chosenButtonName;

	// has this weapon already been chosen?
	if (weaponIndex == uiInfo.selectedWeapon1)
	{
		UI_RemoveWeaponSelection(1);
		return;
	}
	if (weaponIndex == uiInfo.selectedWeapon2)
	{
		UI_RemoveWeaponSelection(2);
		return;
	}

	// See if either slot is empty
	if (uiInfo.selectedWeapon1 == NOWEAPON)
	{
		chosenItemName = "chosenweapon1_icon";
		chosenButtonName = "chosenweapon1_button";
		uiInfo.selectedWeapon1 = weaponIndex;
		uiInfo.selectedWeapon1AmmoIndex = ammoIndex;

		memcpy(uiInfo.selectedWeapon1ItemName, hexBackground, sizeof uiInfo.selectedWeapon1ItemName);

		//Save the lit and unlit icons for the selected weapon slot
		uiInfo.litWeapon1Icon = litIconItem->window.background;
		uiInfo.unlitWeapon1Icon = iconItem->window.background;

		uiInfo.weapon1ItemButton = uiInfo.runScriptItem;
		uiInfo.weapon1ItemButton->descText = "@MENUS_CLICKREMOVE";
	}
	else if (uiInfo.selectedWeapon2 == NOWEAPON)
	{
		chosenItemName = "chosenweapon2_icon";
		chosenButtonName = "chosenweapon2_button";
		uiInfo.selectedWeapon2 = weaponIndex;
		uiInfo.selectedWeapon2AmmoIndex = ammoIndex;

		memcpy(uiInfo.selectedWeapon2ItemName, hexBackground, sizeof uiInfo.selectedWeapon2ItemName);

		//Save the lit and unlit icons for the selected weapon slot
		uiInfo.litWeapon2Icon = litIconItem->window.background;
		uiInfo.unlitWeapon2Icon = iconItem->window.background;

		uiInfo.weapon2ItemButton = uiInfo.runScriptItem;
		uiInfo.weapon2ItemButton->descText = "@MENUS_CLICKREMOVE";
	}
	else // Both slots are used, can't add it.
	{
		return;
	}

	auto item = Menu_FindItemByName(menu, chosenItemName);
	if (item && iconItem)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on chosenweapon button so player can unchoose the weapon
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = Menu_FindItemByName(menu, hexBackground);
	if (item)
	{
		item->window.foreColor[0] = 0;
		item->window.foreColor[1] = 1;
		item->window.foreColor[2] = 0;
		item->window.foreColor[3] = 1;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl)
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weaponIndex > 0 && weaponIndex < WP_NUM_WEAPONS)
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if (ammoIndex > 0 && ammoIndex < AMMO_MAX)
			{
				pState->ammo[ammoIndex] = ammoAmount;
			}
		}
	}

	if (soundfile)
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
	}

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_AddPistolSelection(const int weaponIndex, const int ammoIndex, const int ammoAmount,
	const char* iconItemName, const char* litIconItemName, const char* hexBackground,
	const char* soundfile)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const itemDef_s* iconItem = Menu_FindItemByName(menu, iconItemName);
	const itemDef_s* litIconItem = Menu_FindItemByName(menu, litIconItemName);

	// Has a throw weapon already been chosen?
	if (uiInfo.selectedPistolWeapon != NOWEAPON)
	{
		// Clicked on the selected throwable weapon
		if (uiInfo.selectedPistolWeapon == weaponIndex)
		{
			// Deselect it
			UI_Removepistolselection();
		}
		return;
	}

	const auto chosenItemName = "chosenpistol1_icon";
	const auto chosenButtonName = "chosenpistol1_button";
	uiInfo.selectedPistolWeapon = weaponIndex;
	uiInfo.selectedPistolWeaponAmmoIndex = ammoIndex;
	uiInfo.PistolArmButton = uiInfo.runScriptItem;

	if (uiInfo.PistolArmButton)
	{
		uiInfo.PistolArmButton->descText = "@MENUS_CLICKREMOVE";
	}

	memcpy(uiInfo.selectedPistolWeaponItemName, hexBackground, sizeof uiInfo.selectedWeapon1ItemName);

	//Save the lit and unlit icons for the selected weapon slot
	uiInfo.litPistolIcon = litIconItem->window.background;
	uiInfo.unlitPistolIcon = iconItem->window.background;

	auto item = Menu_FindItemByName(menu, chosenItemName);
	if (item && iconItem)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on throwchosenweapon button so player can unchoose the weapon
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = Menu_FindItemByName(menu, hexBackground);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 1.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weaponIndex > 0 && weaponIndex < WP_NUM_WEAPONS)
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if (ammoIndex > 0 && ammoIndex < AMMO_MAX)
			{
				pState->ammo[ammoIndex] = ammoAmount;
			}
		}
	}

	if (soundfile)
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
	}

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

// Update the player weapons with the chosen weapon
static void UI_RemoveWeaponSelection(const int weapon_selection_index)
{
	const char* chosen_item_name, * chosen_button_name, * background;
	int ammo_index, weapon_index;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	// Which item has it?
	if (weapon_selection_index == 1)
	{
		chosen_item_name = "chosenweapon1_icon";
		chosen_button_name = "chosenweapon1_button";
		background = uiInfo.selectedWeapon1ItemName;
		ammo_index = uiInfo.selectedWeapon1AmmoIndex;
		weapon_index = uiInfo.selectedWeapon1;

		if (uiInfo.weapon1ItemButton)
		{
			uiInfo.weapon1ItemButton->descText = "@MENUS_CLICKSELECT";
			uiInfo.weapon1ItemButton = nullptr;
		}
	}
	else if (weapon_selection_index == 2)
	{
		chosen_item_name = "chosenweapon2_icon";
		chosen_button_name = "chosenweapon2_button";
		background = uiInfo.selectedWeapon2ItemName;
		ammo_index = uiInfo.selectedWeapon2AmmoIndex;
		weapon_index = uiInfo.selectedWeapon2;

		if (uiInfo.weapon2ItemButton)
		{
			uiInfo.weapon2ItemButton->descText = "@MENUS_CLICKSELECT";
			uiInfo.weapon2ItemButton = nullptr;
		}
	}
	else
	{
		return;
	}

	// Reset background of upper icon
	auto item = Menu_FindItemByName(menu, background);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.5f;
		item->window.foreColor[2] = 0.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = Menu_FindItemByName(menu, chosen_item_name);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = Menu_FindItemByName(menu, chosen_button_name);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state
	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weapon_index > 0 && weapon_index < WP_NUM_WEAPONS)
			{
				pState->weapons[weapon_index] = 0;
			}

			// Remove ammo too
			if (ammo_index > 0 && ammo_index < AMMO_MAX)
			{
				// But don't take it away if the other weapon is using that ammo
				if (uiInfo.selectedWeapon1AmmoIndex != uiInfo.selectedWeapon2AmmoIndex)
				{
					pState->ammo[ammo_index] = 0;
				}
			}
		}
	}

	// Now do a little clean up
	if (weapon_selection_index == 1)
	{
		uiInfo.selectedWeapon1 = NOWEAPON;
		memset(uiInfo.selectedWeapon1ItemName, 0, sizeof uiInfo.selectedWeapon1ItemName);
		uiInfo.selectedWeapon1AmmoIndex = 0;
	}
	else if (weapon_selection_index == 2)
	{
		uiInfo.selectedWeapon2 = NOWEAPON;
		memset(uiInfo.selectedWeapon2ItemName, 0, sizeof uiInfo.selectedWeapon2ItemName);
		uiInfo.selectedWeapon2AmmoIndex = 0;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL);
#endif

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_Removepistolselection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	// Weapon not chosen
	if (uiInfo.selectedPistolWeapon == NOWEAPON)
	{
		return;
	}

	const auto chosenItemName = "chosenpistol1_icon";
	const auto chosenButtonName = "chosenpistol1_button";
	const char* background = uiInfo.selectedPistolWeaponItemName;

	// Reset background of upper icon
	auto item = Menu_FindItemByName(menu, background);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 0.5f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = Menu_FindItemByName(menu, chosenItemName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (uiInfo.selectedPistolWeapon > 0 && uiInfo.selectedPistolWeapon < WP_NUM_WEAPONS)
			{
				pState->weapons[uiInfo.selectedPistolWeapon] = 0;
			}

			// Remove ammo too
			if (uiInfo.selectedPistolWeaponAmmoIndex > 0 && uiInfo.selectedPistolWeaponAmmoIndex < AMMO_MAX)
			{
				pState->ammo[uiInfo.selectedPistolWeaponAmmoIndex] = 0;
			}
		}
	}

	// Now do a little clean up
	uiInfo.selectedPistolWeapon = NOWEAPON;
	memset(uiInfo.selectedPistolWeaponItemName, 0, sizeof uiInfo.selectedPistolWeaponItemName);
	uiInfo.selectedPistolWeaponAmmoIndex = 0;

	if (uiInfo.PistolArmButton)
	{
		uiInfo.PistolArmButton->descText = "@MENUS_CLICKSELECT";
		uiInfo.PistolArmButton = nullptr;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL);
#endif

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_NormalWeaponSelection(const int selectionslot)
{
	itemDef_s* item;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	if (selectionslot == 1)
	{
		item = Menu_FindItemByName(menu, "chosenweapon1_icon");
		if (item)
		{
			item->window.background = uiInfo.unlitWeapon1Icon;
		}
	}

	if (selectionslot == 2)
	{
		item = Menu_FindItemByName(menu, "chosenweapon2_icon");
		if (item)
		{
			item->window.background = uiInfo.unlitWeapon2Icon;
		}
	}
}

static void UI_Normalpistolselection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenpistol1_icon");
	item->window.background = uiInfo.unlitPistolIcon;
}

static void UI_HighLightWeaponSelection(const int selectionslot)
{
	itemDef_s* item;

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	if (selectionslot == 1)
	{
		item = Menu_FindItemByName(menu, "chosenweapon1_icon");
		if (item)
		{
			item->window.background = uiInfo.litWeapon1Icon;
		}
	}

	if (selectionslot == 2)
	{
		item = Menu_FindItemByName(menu, "chosenweapon2_icon");
		if (item)
		{
			item->window.background = uiInfo.litWeapon2Icon;
		}
	}
}

static void UI_Highlightpistolselection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenpistol1_icon");
	item->window.background = uiInfo.litPistolIcon;
}

// Update the player throwable weapons (okay it's a bad description) with the chosen weapon
static void UI_AddThrowWeaponSelection(const int weaponIndex, const int ammoIndex, const int ammoAmount,
	const char* iconItemName, const char* litIconItemName, const char* hexBackground,
	const char* soundfile)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const itemDef_s* iconItem = Menu_FindItemByName(menu, iconItemName);
	const itemDef_s* litIconItem = Menu_FindItemByName(menu, litIconItemName);

	// Has a throw weapon already been chosen?
	if (uiInfo.selectedThrowWeapon != NOWEAPON)
	{
		// Clicked on the selected throwable weapon
		if (uiInfo.selectedThrowWeapon == weaponIndex)
		{
			// Deselect it
			UI_RemoveThrowWeaponSelection();
		}
		return;
	}

	const auto chosenItemName = "chosenthrowweapon_icon";
	const auto chosenButtonName = "chosenthrowweapon_button";
	uiInfo.selectedThrowWeapon = weaponIndex;
	uiInfo.selectedThrowWeaponAmmoIndex = ammoIndex;
	uiInfo.weaponThrowButton = uiInfo.runScriptItem;

	if (uiInfo.weaponThrowButton)
	{
		uiInfo.weaponThrowButton->descText = "@MENUS_CLICKREMOVE";
	}

	memcpy(uiInfo.selectedThrowWeaponItemName, hexBackground, sizeof uiInfo.selectedWeapon1ItemName);

	//Save the lit and unlit icons for the selected weapon slot
	uiInfo.litThrowableIcon = litIconItem->window.background;
	uiInfo.unlitThrowableIcon = iconItem->window.background;

	auto item = Menu_FindItemByName(menu, chosenItemName);
	if (item && iconItem)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on throwchosenweapon button so player can unchoose the weapon
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = Menu_FindItemByName(menu, hexBackground);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 1.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (weaponIndex > 0 && weaponIndex < WP_NUM_WEAPONS)
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if (ammoIndex > 0 && ammoIndex < AMMO_MAX)
			{
				pState->ammo[ammoIndex] = ammoAmount;
			}
		}
	}

	if (soundfile)
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL);
	}

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

// Update the player weapons with the chosen throw weapon
static void UI_RemoveThrowWeaponSelection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	// Weapon not chosen
	if (uiInfo.selectedThrowWeapon == NOWEAPON)
	{
		return;
	}

	const auto chosenItemName = "chosenthrowweapon_icon";
	const auto chosenButtonName = "chosenthrowweapon_button";
	const char* background = uiInfo.selectedThrowWeaponItemName;

	// Reset background of upper icon
	auto item = Menu_FindItemByName(menu, background);
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 0.5f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = Menu_FindItemByName(menu, chosenItemName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = Menu_FindItemByName(menu, chosenButtonName);
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state

	const client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl) // No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t* pState = cl->gentity->client;

			if (uiInfo.selectedThrowWeapon > 0 && uiInfo.selectedThrowWeapon < WP_NUM_WEAPONS)
			{
				pState->weapons[uiInfo.selectedThrowWeapon] = 0;
			}

			// Remove ammo too
			if (uiInfo.selectedThrowWeaponAmmoIndex > 0 && uiInfo.selectedThrowWeaponAmmoIndex < AMMO_MAX)
			{
				pState->ammo[uiInfo.selectedThrowWeaponAmmoIndex] = 0;
			}
		}
	}

	// Now do a little clean up
	uiInfo.selectedThrowWeapon = NOWEAPON;
	memset(uiInfo.selectedThrowWeaponItemName, 0, sizeof uiInfo.selectedThrowWeaponItemName);
	uiInfo.selectedThrowWeaponAmmoIndex = 0;

	if (uiInfo.weaponThrowButton)
	{
		uiInfo.weaponThrowButton->descText = "@MENUS_CLICKSELECT";
		uiInfo.weaponThrowButton = nullptr;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL);
#endif

	UI_WeaponsSelectionsComplete(); // Test to see if the mission begin button should turn on or off
}

static void UI_NormalThrowSelection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenthrowweapon_icon");
	item->window.background = uiInfo.unlitThrowableIcon;
}

static void UI_HighLightThrowSelection()
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu
	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "chosenthrowweapon_icon");
	item->window.background = uiInfo.litThrowableIcon;
}

static void UI_GetSaberCvars()
{
	Cvar_Set("ui_saber_type", Cvar_VariableString("g_saber_type"));
	if (!Cvar_VariableIntegerValue("ui_npc_saber") && !Cvar_VariableIntegerValue("ui_npc_sabertwo"))
	{
		Cvar_Set("ui_saber", Cvar_VariableString("g_saber"));
		Cvar_Set("ui_saber2", Cvar_VariableString("g_saber2"));
		Cvar_Set("ui_saber_color", Cvar_VariableString("g_saber_color"));
		Cvar_Set("ui_saber2_color", Cvar_VariableString("g_saber2_color"));
#ifdef NEW_FEEDER_V9
		//Cvar_Set("ui_saber_type", Cvar_VariableString("ui_saber_type_md"));
		//Cvar_Set("ui_saber", Cvar_VariableString("ui_saber_md"));
		//Cvar_Set("ui_saber2", Cvar_VariableString("ui_saber2_md"));
		//Cvar_Set("ui_saber_color", Cvar_VariableString("ui_saber_color_md"));
		//Cvar_Set("ui_saber2_color", Cvar_VariableString("ui_saber2_color_md"));
#endif
	}
	else
	{
		Cvar_Set("ui_npc_saber", Cvar_VariableString("g_NPCsaber"));
		Cvar_Set("ui_npc_sabertwo", Cvar_VariableString("g_NPCsabertwo"));
		Cvar_Set("ui_npc_sabercolor", Cvar_VariableString("g_NPCsabercolor"));
		Cvar_Set("ui_npc_sabertwocolor", Cvar_VariableString("g_NPCsabertwocolor"));
	}

	const saber_colors_t saberColour = TranslateSaberColor(Cvar_VariableString("ui_saber_color"));

	if (saberColour >= SABER_RGB)
	{
		Cvar_SetValue("ui_rgb_saber_red", saberColour & 0xff);
		Cvar_SetValue("ui_rgb_saber_green", saberColour >> 8 & 0xff);
		Cvar_SetValue("ui_rgb_saber_blue", saberColour >> 16 & 0xff);
	}

	const saber_colors_t saber2Colour = TranslateSaberColor(Cvar_VariableString("ui_saber2_color"));

	if (saber2Colour >= SABER_RGB)
	{
		Cvar_SetValue("ui_rgb_saber2_red", saber2Colour & 0xff);
		Cvar_SetValue("ui_rgb_saber2_green", saber2Colour >> 8 & 0xff);
		Cvar_SetValue("ui_rgb_saber2_blue", saber2Colour >> 16 & 0xff);
	}

	Cvar_Set("ui_newfightingstyle", "0");
}

static void UI_ResetSaberCvars()
{
	Cvar_Set("g_saber_type", "single");
	Cvar_Set("g_saber", "single_1");
	Cvar_Set("g_saber2", "");

	Cvar_Set("ui_saber_type", "single");
	Cvar_Set("ui_saber", "single_1");
	Cvar_Set("ui_saber2", "");
}

extern qboolean ItemParse_asset_model_go(itemDef_t* item, const char* name);
extern qboolean ItemParse_model_g2skin_go(itemDef_t* item, const char* skinName);

static void UI_UpdateCharacterSkin()
{
	char skin[MAX_QPATH];

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "character");

	if (!item)
	{
		Com_Error(ERR_FATAL, "UI_UpdateCharacterSkin: Could not find item (character) in menu (%s)", menu->window.name);
	}

#ifdef NEW_FEEDER
#ifdef NEW_FEEDER_V2
	if (!strcmp(menu->window.name, "ingamecharacter"))
	{
		if (!strchr(UI_Cvar_VariableString("ui_model"), '|'))
		{
			Com_sprintf(skin, sizeof skin, "models/players/%s/%s.skin",
				Cvar_VariableString("ui_char_model"),
				Cvar_VariableString("ui_char_skin")
			);
		}
		else
		{
			Com_sprintf(skin, sizeof skin, "models/players/%s",
				Cvar_VariableString("ui_model")
			);
		}
	}
	else
#else
	if (!strcmp(menu->window.name,"ingamecharacter") && !strchr(UI_Cvar_VariableString("ui_model"), '|')) // Not a multipart custom character
	{
		Com_sprintf(skin, sizeof skin, "models/players/%s/%s.skin",
			Cvar_VariableString("ui_char_model"),
			Cvar_VariableString("ui_char_skin")
		);
	}
	else
#endif
#endif
	Com_sprintf(skin, sizeof skin, "models/players/%s/|%s|%s|%s",
		Cvar_VariableString("ui_char_model"),
		Cvar_VariableString("ui_char_skin_head"),
		Cvar_VariableString("ui_char_skin_torso"),
		Cvar_VariableString("ui_char_skin_legs")
	);

	ItemParse_model_g2skin_go(item, skin);

#ifdef NEW_FEEDER // Saber
	if (!strcmp(menu->window.name, "ingamecharacter"))
	{
#ifdef NEW_FEEDER_V5 // Saber
		modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);
		if (modelPtr) { // Using divide instead so its consistant
			modelPtr->fov_x = (32.0f / charMD[uiVariantIndex].fov);
			modelPtr->fov_y = (32.0f / charMD[uiVariantIndex].fov);
		}
#endif

		//UI_SaberAttachToChar(item);

		int num_sabers = 1;

		if (item->ghoul2.size() > 2 && item->ghoul2[2].mModelindex >= 0)
			DC->g2_RemoveGhoul2Model(item->ghoul2, 2); //remove any extra models
		if (item->ghoul2.size() > 1 && item->ghoul2[1].mModelindex >= 0)
			DC->g2_RemoveGhoul2Model(item->ghoul2, 1); //remove any extra models

		if (!strcmp(charMD[uiVariantIndex].saber1, "")) {
			item->flags &= ~ITF_ISSABER;
			item->flags &= ~ITF_ISSABER2;
			return;
		}

		item->flags |= ITF_ISSABER;
		if (strcmp(charMD[uiVariantIndex].saber2, ""))//(uiInfo.movesTitleIndex == 4 /*MD_DUAL_SABERS*/)
		{
			num_sabers = 2;
			item->flags |= ITF_ISSABER2;
		}

		Cvar_Set("ui_saber", charMD[uiVariantIndex].saber1);
		Cvar_Set("ui_saber2", charMD[uiVariantIndex].saber2);
#ifdef NEW_FEEDER_V8
#else
		Cvar_Set("g_saber", charMD[uiVariantIndex].saber1);
		Cvar_Set("g_saber2", charMD[uiVariantIndex].saber2);
#endif
		Cvar_Set("ui_saber_color", charMD[uiVariantIndex].color1);
		Cvar_Set("ui_saber2_color", charMD[uiVariantIndex].color2);
#ifdef NEW_FEEDER_V8
#else
		Cvar_Set("g_saber_color", charMD[uiVariantIndex].color1);
		Cvar_Set("g_saber2_color", charMD[uiVariantIndex].color2);
#endif

		for (int saber_num = 0; saber_num < num_sabers; saber_num++)
		{
			//bolt sabers
			char model_path[MAX_QPATH];

			if (UI_SaberModelForSaber(saber_num ? charMD[uiVariantIndex].saber2 : charMD[uiVariantIndex].saber1, model_path))
			{ //successfully found a model
				const int g2_saber = DC->g2_InitGhoul2Model(item->ghoul2, model_path, 0, 0, 0, 0, 0); //add the model
				if (g2_saber) {
					DC->g2_SetSkin(&item->ghoul2[g2_saber], -1, 0); //turn off custom skin, only use default for sabers and guns
					re.G2API_AttachG2Model(&item->ghoul2[g2_saber], &item->ghoul2[0], DC->g2_AddBolt(&item->ghoul2[0], saber_num ? "*l_hand" : "*r_hand"), 0);
				}
			}
		}
#ifdef NEW_FEEDER_V5
#else
#ifdef NEW_FEEDER_V2
		modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);
		if (modelPtr) {
#ifdef NEW_FEEDER_V5_C // Using divide instead so its consistant
			modelPtr->fov_x = (32.0f / charMD[uiVariantIndex].fov);
			modelPtr->fov_y = (32.0f / charMD[uiVariantIndex].fov);
#else
			modelPtr->fov_x = (32.0f * charMD[uiVariantIndex].fov);
			modelPtr->fov_y = (32.0f * charMD[uiVariantIndex].fov);
#endif
		}
#endif
#endif
		 // Play the Select Animation
		//uiInfo.movesBaseAnim = anim_table[charMD[uiVariantIndex].selectAnimation].name;
		//ItemParse_model_g2anim_go(item, anim_table[charMD[uiVariantIndex].selectAnimation].name);
		//const modelDef_t* modelPtr = static_cast<modelDef_t*>(item->typeData);
		//if (modelPtr->g2anim)
		//	uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue);

	}
#endif
}

#ifdef NEW_FEEDER_V7
static void UI_UpdateCharacter(qboolean changedModel, qboolean newLoad)
#else
static void UI_UpdateCharacter(const qboolean changedModel)
#endif
{
	char modelPath[MAX_QPATH];

	const menuDef_t* menu = Menu_GetFocused(); // Get current menu

	if (!menu)
	{
		return;
	}

	const auto item = Menu_FindItemByName(menu, "character");

	if (!item)
	{
		Com_Error(ERR_FATAL, "UI_UpdateCharacter: Could not find item (character) in menu (%s)", menu->window.name);
	}

#ifdef NEW_FEEDER // Animation
#ifdef NEW_FEEDER_V4
	static int mdSelected = -1;
	if (!strcmp(menu->window.name, "ingamecharacter"))
	{
#ifdef NEW_FEEDER_V7
		if ((uiVariantIndex != mdSelected) || newLoad)
#else
		if (uiVariantIndex != mdSelected) 
#endif
		{
			mdSelected = uiVariantIndex;
#ifdef NEW_FEEDER_V5
			uiInfo.movesBaseAnim = anim_table[charMD[uiVariantIndex].selectAnimation].name;
#endif
			ItemParse_model_g2anim_go(item, anim_table[charMD[uiVariantIndex].selectAnimation].name);
			uiInfo.moveAnimTime += uiInfo.uiDC.realTime; 
		}
		else
			return;
	}
	else
#else
	if (!strcmp(menu->window.name, "ingamecharacter")) 
	{
		ItemParse_model_g2anim_go(item, anim_table[charMD[uiVariantIndex].selectAnimation].name);
		uiInfo.moveAnimTime += uiInfo.uiDC.realTime;
	}
	else
#endif
#endif
	ItemParse_model_g2anim_go(item, ui_char_anim.string);

	Com_sprintf(modelPath, sizeof modelPath, "models/players/%s/model.glm", Cvar_VariableString("ui_char_model"));
	ItemParse_asset_model_go(item, modelPath);

	if (changedModel)
	{
		//set all skins to first skin since we don't know you always have all skins
		//FIXME: could try to keep the same spot in each list as you swtich models
		UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, 0, item); //fixme, this is not really the right item!!
		UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, 0, item);
		UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, 0, item);
		UI_FeederSelection(FEEDER_COLORCHOICES, 0, item);
	}
	UI_UpdateCharacterSkin();
}

void UI_UpdateSaberType()
{
	char sType[MAX_QPATH];
	DC->getCVarString("ui_saber_type", sType, sizeof sType);
	if (Q_stricmp("single", sType) == 0 ||
		Q_stricmp("staff", sType) == 0)
	{
		DC->setCVar("ui_saber2", "");
	}
}

static void UI_UpdateSaberHilt(const qboolean secondSaber)
{
	char model[MAX_QPATH];
	char modelPath[MAX_QPATH];
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

	if (!menu)
	{
		return;
	}

	const char* itemName;
	const char* saberCvarName;
	if (secondSaber)
	{
		itemName = "saber2";
		saberCvarName = "ui_saber2";
	}
	else
	{
		itemName = "saber";
		saberCvarName = "ui_saber";
	}

	const auto item = Menu_FindItemByName(menu, itemName);

	if (!item)
	{
		Com_Error(ERR_FATAL, "UI_UpdateSaberHilt: Could not find item (%s) in menu (%s)", itemName, menu->window.name);
	}
	DC->getCVarString(saberCvarName, model, sizeof model);
	//read this from the sabers.cfg
	if (UI_SaberModelForSaber(model, modelPath))
	{
		char skinPath[MAX_QPATH];
		//successfully found a model
		ItemParse_asset_model_go(item, modelPath); //set the model
		//get the customSkin, if any
		//COM_StripExtension( modelPath, skinPath, sizeof(skinPath) );
		//COM_DefaultExtension( skinPath, sizeof( skinPath ), ".skin" );
		if (UI_SaberSkinForSaber(model, skinPath))
		{
			ItemParse_model_g2skin_go(item, skinPath); //apply the skin
		}
		else
		{
			ItemParse_model_g2skin_go(item, nullptr); //apply the skin
		}
	}
}

char GoToMenu[1024];

/*
=================
Menus_SaveGoToMenu
=================
*/
static void Menus_SaveGoToMenu(const char* menuTo)
{
	memcpy(GoToMenu, menuTo, sizeof GoToMenu);
}

/*
=================
UI_CheckVid1Data
=================
*/
void UI_CheckVid1Data(const char* menuTo, const char* warningMenuName)
{
	const menuDef_t* menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

	if (!menu)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: No videoMenu was found. Video data could not be checked\n");
		return;
	}

	const itemDef_t* applyChanges = Menu_FindItemByName(menu, "applyChanges");

	if (!applyChanges)
	{
		//		Menus_CloseAll();
		Menus_OpenByName(menuTo);
		return;
	}

	if (applyChanges->window.flags & WINDOW_VISIBLE) // Is the APPLY CHANGES button active?
	{
		Menus_OpenByName(warningMenuName); // Give warning
	}
	else
	{
		//		Menus_CloseAll();
		//		Menus_OpenByName(menuTo);
	}
}

/*
=================
UI_ResetDefaults
=================
*/
void UI_ResetDefaults()
{
	ui.Cmd_ExecuteText(EXEC_APPEND, "cvar_restart\n");
	Controls_SetDefaults();
	ui.Cmd_ExecuteText(EXEC_APPEND, "exec MD-SP-default.cfg\n");
	ui.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
}

/*
=======================
UI_SortSaveGames
=======================
*/
static int UI_SortSaveGames(const void* A, const void* B)
{
	const int& a = ((savedata_t*)A)->currentSaveFileDateTime;
	const int& b = ((savedata_t*)B)->currentSaveFileDateTime;

	if (a > b)
	{
		return -1;
	}
	return a < b;
}

/*
=======================
UI_AdjustSaveGameListBox
=======================
*/
// Yeah I could get fired for this... in a world of good and bad, this is bad
// I wish we passed in the menu item to RunScript(), oh well...
void UI_AdjustSaveGameListBox(const int currentLine)
{
	// could be in either the ingame or shell load menu (I know, I know it's bad)
	const menuDef_t* menu = Menus_FindByName("loadgameMenu");
	if (!menu)
	{
		menu = Menus_FindByName("ingameloadMenu");
	}

	if (menu)
	{
		const auto item = Menu_FindItemByName(menu, "loadgamelist");
		if (item)
		{
			const auto listPtr = static_cast<listBoxDef_t*>(item->typeData);
			if (listPtr)
			{
				listPtr->cursorPos = currentLine;
			}

			item->cursorPos = currentLine;
		}
	}
}

/*
=================
ReadSaveDirectory
=================
*/
//JLFSAVEGAME MPNOTUSED
void ReadSaveDirectory()
{
	char* holdChar;
	int len;
	int fileCnt;
	// Clear out save data
	memset(s_savedata, 0, sizeof s_savedata);
	s_savegame.saveFileCnt = 0;
	Cvar_Set("ui_gameDesc", ""); // Blank out comment
	Cvar_Set("ui_SelectionOK", "0");
#ifdef JK2_MODE
	memset(screenShotBuf, 0, (SG_SCR_WIDTH * SG_SCR_HEIGHT * 4)); //blank out sshot
#endif

	// Get everything in saves directory
	fileCnt = ui.FS_GetFileList("Account/Saved-Missions-MovieDuels/", ".sav", s_savegame.listBuf, LISTBUFSIZE);

	Cvar_Set("ui_ResumeOK", "0");
	holdChar = s_savegame.listBuf;
	for (int i = 0; i < fileCnt; i++)
	{
		// strip extension
		len = strlen(holdChar);
		holdChar[len - 4] = '\0';

		if (Q_stricmp("current", holdChar) != 0)
		{
			time_t result;
			if (Q_stricmp("auto", holdChar) == 0)
			{
				Cvar_Set("ui_ResumeOK", "1");
			}
			else
			{
				// Is this a valid file??? & Get comment of file
				result = ui.SG_GetSaveGameComment(holdChar, s_savedata[s_savegame.saveFileCnt].currentSaveFileComments,
					s_savedata[s_savegame.saveFileCnt].currentSaveFileMap);
				if (result != 0) // ignore Bad save game
				{
					s_savedata[s_savegame.saveFileCnt].currentSaveFileName = holdChar;
					s_savedata[s_savegame.saveFileCnt].currentSaveFileDateTime = result;

					const tm* localTime = localtime(&result);
					strcpy(s_savedata[s_savegame.saveFileCnt].currentSaveFileDateTimeString, asctime(localTime));
					s_savegame.saveFileCnt++;
					if (s_savegame.saveFileCnt == MAX_SAVELOADFILES)
					{
						break;
					}
				}
			}
		}

		holdChar += len + 1; //move to next item
	}

	qsort(s_savedata, s_savegame.saveFileCnt, sizeof(savedata_t), UI_SortSaveGames);
}