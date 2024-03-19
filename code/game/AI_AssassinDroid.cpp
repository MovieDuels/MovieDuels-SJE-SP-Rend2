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

#include "bg_public.h"
#include "b_local.h"

constexpr auto ASSASSIN_SHIELD_SIZE = 75;
constexpr auto TURN_ON = 0x00000000;
constexpr auto TURN_OFF = 0x00000100;

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static bool BubbleShield_IsOn()
{
	return NPC->flags & FL_SHIELDED;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void BubbleShield_TurnOn()
{
	if (!BubbleShield_IsOn() && !NPC->client->ps.powerups[PW_STUNNED])
	{
		NPC->flags |= FL_SHIELDED;
		NPC->client->ps.powerups[PW_GALAK_SHIELD] = Q3_INFINITE;
		gi.G2API_SetSurfaceOnOff(&NPC->ghoul2[NPC->playerModel], "force_shield", TURN_ON);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static void BubbleShield_TurnOff()
{
	if (BubbleShield_IsOn())
	{
		NPC->flags &= ~FL_SHIELDED;
		NPC->client->ps.powerups[PW_GALAK_SHIELD] = 0;
		gi.G2API_SetSurfaceOnOff(&NPC->ghoul2[NPC->playerModel], "force_shield", TURN_OFF);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Push A Particular Ent
////////////////////////////////////////////////////////////////////////////////////////
static void BubbleShield_PushEnt(gentity_t* pushed, vec3_t smack_dir)
{
	G_Damage(pushed, NPC, NPC, smack_dir, NPC->currentOrigin, (g_spskill->integer + 1) * Q_irand(5, 10), DAMAGE_NO_KNOCKBACK, MOD_ELECTROCUTE);
	G_Throw(pushed, smack_dir, 10);

	// Make Em Electric
	//------------------
	pushed->s.powerups |= 1 << PW_SHOCKED;
	if (pushed->client)
	{
		pushed->client->ps.powerups[PW_SHOCKED] = level.time + 1000;
	}
}

extern void G_BlastDown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength);
static void deka_bubble_shield_push_ent(gentity_t* pushed, vec3_t smack_dir)
{
	G_Damage(pushed, NPC, NPC, smack_dir, NPC->currentOrigin, Q_irand(10, 20), DAMAGE_EXTRA_KNOCKBACK, MOD_ELECTROCUTE);
	G_BlastDown(pushed, NPC, smack_dir, 50);

	// Make Em Electric
	//------------------
	pushed->s.powerups |= 1 << PW_SHOCKED;
	if (pushed->client)
	{
		pushed->client->ps.powerups[PW_SHOCKED] = level.time + 3000;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Go Through All The Ents Within The Radius Of The Shield And Push Them
////////////////////////////////////////////////////////////////////////////////////////
static void BubbleShield_PushRadiusEnts()
{
	gentity_t* radius_ents[128];
	constexpr float radius = ASSASSIN_SHIELD_SIZE;
	vec3_t mins{}, maxs{};

	for (int i = 0; i < 3; i++)
	{
		mins[i] = NPC->currentOrigin[i] - radius;
		maxs[i] = NPC->currentOrigin[i] + radius;
	}

	const int num_ents = gi.EntitiesInBox(mins, maxs, radius_ents, 128);
	for (int ent_index = 0; ent_index < num_ents; ent_index++)
	{
		vec3_t smack_dir;
		// Only Clients
		//--------------
		if (!radius_ents[ent_index] || !radius_ents[ent_index]->client)
		{
			continue;
		}

		// Don't Push Away Other Assassin Droids
		//---------------------------------------
		if (radius_ents[ent_index]->client->NPC_class == NPC->client->NPC_class)
		{
			continue;
		}

		// Should Have Already Pushed The Enemy If He Touched Us
		//-------------------------------------------------------
		if (NPC->enemy && NPCInfo->touchedByPlayer == NPC->enemy && radius_ents[ent_index] == NPC->enemy)
		{
			continue;
		}

		// Do The Vector Distance Test
		//-----------------------------
		VectorSubtract(radius_ents[ent_index]->currentOrigin, NPC->currentOrigin, smack_dir);
		const float smack_dist = VectorNormalize(smack_dir);
		if (smack_dist < radius)
		{
			BubbleShield_PushEnt(radius_ents[ent_index], smack_dir);
		}
	}
}

static void deka_bubble_shield_push_radius_ents()
{
	gentity_t* radius_ents[128];
	constexpr float radius = ASSASSIN_SHIELD_SIZE;
	vec3_t mins{}, maxs{};

	for (int i = 0; i < 3; i++)
	{
		mins[i] = NPC->currentOrigin[i] - radius;
		maxs[i] = NPC->currentOrigin[i] + radius;
	}

	const int num_ents = gi.EntitiesInBox(mins, maxs, radius_ents, 128);
	for (int ent_index = 0; ent_index < num_ents; ent_index++)
	{
		vec3_t smack_dir;
		// Only Clients
		//--------------
		if (!radius_ents[ent_index] || !radius_ents[ent_index]->client)
		{
			continue;
		}

		// Don't Push Away Other Assassin Droids
		//---------------------------------------
		if (radius_ents[ent_index]->client->NPC_class == NPC->client->NPC_class)
		{
			continue;
		}

		// Should Have Already Pushed The Enemy If He Touched Us
		//-------------------------------------------------------
		if (NPC->enemy && NPCInfo->touchedByPlayer == NPC->enemy && radius_ents[ent_index] == NPC->enemy)
		{
			continue;
		}

		// Do The Vector Distance Test
		//-----------------------------
		VectorSubtract(radius_ents[ent_index]->currentOrigin, NPC->currentOrigin, smack_dir);
		const float smack_dist = VectorNormalize(smack_dir);
		if (smack_dist < radius)
		{
			deka_bubble_shield_push_ent(radius_ents[ent_index], smack_dir);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void bubble_shield_update()
{
	// Shields Go When You Die
	//-------------------------
	if (NPC->health <= 0)
	{
		if (BubbleShield_IsOn())
		{
			BubbleShield_TurnOff();
		}
		return;
	}

	// Recharge Shields
	//------------------
	NPC->client->ps.stats[STAT_ARMOR] += 1;
	if (NPC->client->ps.stats[STAT_ARMOR] > 250)
	{
		NPC->client->ps.stats[STAT_ARMOR] = 250;
	}

	// If We Have Enough Armor And Are Not Shooting Right Now, Kick The Shield On
	//----------------------------------------------------------------------------
	if (NPC->client->ps.stats[STAT_ARMOR] > 100 && TIMER_Done(NPC, "ShieldsDown"))
	{
		// Check On Timers To Raise And Lower Shields
		//--------------------------------------------
		if (level.time - NPCInfo->enemyLastSeenTime < 1000 && TIMER_Done(NPC, "ShieldsUp"))
		{
			TIMER_Set(NPC, "ShieldsDown", 2000); // Drop Shields
			TIMER_Set(NPC, "ShieldsUp", Q_irand(4000, 5000)); // Then Bring Them Back Up For At Least 3 sec
		}

		BubbleShield_TurnOn();

		if (BubbleShield_IsOn())
		{
			// Update Our Shader Value
			//-------------------------
			NPC->client->renderInfo.customRGBA[0] =
				NPC->client->renderInfo.customRGBA[1] =
				NPC->client->renderInfo.customRGBA[2] =
				NPC->client->renderInfo.customRGBA[3] = NPC->client->ps.stats[STAT_ARMOR] - 100;

			// If Touched By An Enemy, ALWAYS Shove Them
			//-------------------------------------------
			if (NPC->enemy && NPCInfo->touchedByPlayer == NPC->enemy)
			{
				vec3_t dir;
				VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, dir);
				VectorNormalize(dir);
				BubbleShield_PushEnt(NPC->enemy, dir);
			}

			// Push Anybody Else Near
			//------------------------
			BubbleShield_PushRadiusEnts();
		}
	}

	// Shields Gone
	//--------------
	else
	{
		BubbleShield_TurnOff();
	}
}

//extern cvar_t* g_allowgunnerbash;
//extern cvar_t* g_SerenityJediEngineMode;
void deka_bubble_shield_update()
{
	// Shields Go When You Die
	//-------------------------
	if (NPC->health <= 0)
	{
		if (BubbleShield_IsOn())
		{
			BubbleShield_TurnOff();
		}
		return;
	}

	if (NPC->client->ps.powerups[PW_STUNNED])
	{
		if (BubbleShield_IsOn())
		{
			BubbleShield_TurnOff();
		}
		return;
	}

	// Recharge Shields
	//------------------
	NPC->client->ps.stats[STAT_ARMOR] += 1;

	if (NPC->client->ps.stats[STAT_ARMOR] > 250)
	{
		NPC->client->ps.stats[STAT_ARMOR] = 250;
	}

	// If We Have Enough Armor And Are Not Shooting Right Now, Kick The Shield On
	//----------------------------------------------------------------------------
	if (NPC->client->ps.stats[STAT_ARMOR] > 100 && TIMER_Done(NPC, "ShieldsDown"))
	{
		// Check On Timers To Raise And Lower Shields
		//--------------------------------------------
		if (level.time - NPCInfo->enemyLastSeenTime < 1000 && TIMER_Done(NPC, "ShieldsUp"))
		{
			TIMER_Set(NPC, "ShieldsDown", 2000); // Drop Shields
			TIMER_Set(NPC, "ShieldsUp", Q_irand(8000, 16000)); // Then Bring Them Back Up For At Least 3 sec
		}

		BubbleShield_TurnOn();

		if (BubbleShield_IsOn() && NPC->client->ps.stats[STAT_ARMOR] > 50)
		{
			// Update Our Shader Value
			//-------------------------
			NPC->client->renderInfo.customRGBA[0] =
				NPC->client->renderInfo.customRGBA[1] =
				NPC->client->renderInfo.customRGBA[2] =
				NPC->client->renderInfo.customRGBA[3] = NPC->client->ps.stats[STAT_ARMOR] - 100;

			//if (g_SerenityJediEngineMode->integer == 2 && g_allowgunnerbash->integer > 0)
			//{
			//	// If Touched By An Enemy, ALWAYS Shove Them
			//	//-------------------------------------------
			//	if (NPC->enemy && NPCInfo->touchedByPlayer == NPC->enemy)
			//	{
			//		vec3_t dir;
			//		VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, dir);
			//		VectorNormalize(dir);
			//		deka_bubble_shield_push_ent(NPC->enemy, dir);
			//	}

			//	// Push Anybody Else Near
			//	//------------------------
			//	deka_bubble_shield_push_radius_ents();
			//}
		}
	}

	// Shields Gone
	//--------------
	else
	{
		BubbleShield_TurnOff();
	}
}