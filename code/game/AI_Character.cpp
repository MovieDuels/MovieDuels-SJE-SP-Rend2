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

#include "b_local.h"
extern cvar_t* g_SerenityJediEngineMode;
extern qboolean G_ControlledByPlayer(const gentity_t* self);

qboolean HeIsJedi(const gentity_t* ent)
{
	switch (ent->client->NPC_class)
	{
	case CLASS_SITHLORD:
	case CLASS_DESANN:
	case CLASS_VADER:
	case CLASS_JEDI:
	case CLASS_KYLE:
	case CLASS_LUKE:
	case CLASS_MORGANKATARN:
	case CLASS_REBORN:
	case CLASS_BOC:
	case CLASS_SHADOWTROOPER:
	case CLASS_TAVION:
	case CLASS_ALORA:
	case CLASS_GALEN:
	case CLASS_YODA:
	case CLASS_AHSOKA:
	case CLASS_PROJECTION:
	case CLASS_JEDIMASTER:
	case CLASS_GROGU:
		// Is Jedi...
		return qtrue;
	default:
		// NOT Jedi...
		break;
	}

	return qfalse;
}

qboolean droideka_npc(const gentity_t* ent)
{
	if (ent->NPC
		&& ent->client->NPC_class == CLASS_DROIDEKA
		&& ent->s.weapon == WP_DROIDEKA
		&& ent->s.client_num >= MAX_CLIENTS
		&& !G_ControlledByPlayer(ent))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Mandalorian_Character(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_BOBAFETT
		|| self->client->NPC_class == CLASS_MANDALORIAN
		|| self->client->NPC_class == CLASS_JANGO
		|| self->client->NPC_class == CLASS_JANGODUAL)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Mandalorian_Repeater(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_MANDALORIAN
		&& (Q_stricmp("pazvizsla", self->NPC_type) == 0 ||
			Q_stricmp("pazvizsla_nohelm", self->NPC_type) == 0))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Armorer_clone_pistol(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_MANDALORIAN
		&& (Q_stricmp("armorer_jet", self->NPC_type) == 0))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Mandalorian_boba_pistol(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_MANDALORIAN
		&& (Q_stricmp("boba_fett_esb", self->NPC_type) == 0 ||
			Q_stricmp("boba_fett_rotj", self->NPC_type) == 0 ||
			Q_stricmp("boba_fett_mand1", self->NPC_type) == 0 ||
			Q_stricmp("boba_fett_mand2", self->NPC_type) == 0 ||
			Q_stricmp("boba_fett_nohelmet", self->NPC_type) == 0 ||
			Q_stricmp("boba_fett_nohelmet2", self->NPC_type) == 0))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Lando_Class_boba_pistol(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_LANDO
		&& (Q_stricmp("boba_fett_tgpoc", self->NPC_type) == 0))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Bokatan_Dual_Clone_Pistol(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_MANDALORIAN
		&& (Q_stricmp("bokatan", self->NPC_type) == 0 ||
			Q_stricmp("bokatan_jp2", self->NPC_type) == 0 ||
			Q_stricmp("bokatan_nohelm", self->NPC_type) == 0 ||
			Q_stricmp("bokatan_nohelm_jp2", self->NPC_type) == 0))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Calo_Nord(const gentity_t* self)
{
	if (self->client->NPC_class == CLASS_CALONORD
		&& (Q_stricmp("calonord", self->NPC_type) == 0 ||
			Q_stricmp("md_calo", self->NPC_type) == 0 ||
			Q_stricmp("md_calonord", self->NPC_type) == 0))
	{
		return qtrue;
	}
	return qfalse;
}

qboolean jedi_is_kick_resistant(gentity_t* self)
{
	if (Q_stricmp("md_gorc", self->NPC_type) == 0
		|| Q_stricmp("md_vader_ep3", self->NPC_type) == 0
		|| Q_stricmp("md_vader_anh", self->NPC_type) == 0
		|| Q_stricmp("md_vad_vr", self->NPC_type) == 0
		|| Q_stricmp("md_vader_tv", self->NPC_type) == 0
		|| Q_stricmp("md_vader_ds", self->NPC_type) == 0
		|| Q_stricmp("md_vader", self->NPC_type) == 0
		|| Q_stricmp("md_vad_tfu", self->NPC_type) == 0
		|| Q_stricmp("md_vader_bw", self->NPC_type) == 0
		|| Q_stricmp("md_vad2_tfu", self->NPC_type) == 0
		|| Q_stricmp("md_maw", self->NPC_type) == 0
		|| Q_stricmp("md_grie_egg", self->NPC_type) == 0
		|| Q_stricmp("md_grie2_egg", self->NPC_type) == 0
		|| Q_stricmp("md_grie3_egg", self->NPC_type) == 0
		|| Q_stricmp("md_grie4_egg", self->NPC_type) == 0
		|| Q_stricmp("md_grievous", self->NPC_type) == 0
		|| Q_stricmp("md_grievous4", self->NPC_type) == 0
		|| Q_stricmp("md_grievous_robed", self->NPC_type) == 0
		|| Q_stricmp("md_dro_am", self->NPC_type) == 0
		|| Q_stricmp("md_dro_sn", self->NPC_type) == 0)
	{
		self->flags |= FL_NO_KNOCKBACK;
		return qtrue;
	}

	return qfalse;
}

qboolean jedi_win_po(const gentity_t* self)
{
	if (Q_stricmp("md_win_po", self->NPC_type) == 0)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean Grievious_Classes(const gentity_t* self)
{
	if (Q_stricmp("Arena_Grievous", self->NPC_type) == 0
		|| Q_stricmp("Arena_Grievous2", self->NPC_type) == 0
		|| Q_stricmp("md_grievous", self->NPC_type) == 0
		|| Q_stricmp("md_grievous4", self->NPC_type) == 0
		|| Q_stricmp("md_grievous_robed", self->NPC_type) == 0)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean jedi_jedi_master_low(const gentity_t* self)
{
	if (g_SerenityJediEngineMode->integer == 2)
	{
		if (self->client->NPC_class == CLASS_JEDIMASTER
			&& self->client->ps.blockPoints < BLOCKPOINTS_THIRTY)
		{
			return qtrue;
		}
	}
	else
	{
		if (self->client->NPC_class == CLASS_JEDIMASTER
			&& self->client->ps.forcePower < BLOCKPOINTS_THIRTY)
		{
			return qtrue;
		}
	}
	return qfalse;
}

qboolean jedi_jedi_master_high(const gentity_t* self)
{
	if (g_SerenityJediEngineMode->integer == 2)
	{
		if (self->client->NPC_class == CLASS_JEDIMASTER
			&& self->client->ps.blockPoints > BLOCKPOINTS_THIRTY)
		{
			return qtrue;
		}
	}
	else
	{
		if (self->client->NPC_class == CLASS_JEDIMASTER
			&& self->client->ps.forcePower > BLOCKPOINTS_THIRTY)
		{
			return qtrue;
		}
	}

	return qfalse;
}