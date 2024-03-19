//ported from SP
#include "b_local.h"

#define	ASSASSIN_SHIELD_SIZE	75
#define TURN_ON					0x00000000
#define TURN_OFF				0x00000100

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
qboolean BubbleShield_IsOn(const gentity_t* self)
{
	return self->flags & FL_SHIELDED;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void BubbleShield_TurnOn(gentity_t* self)
{
	if (!BubbleShield_IsOn(self))
	{
		self->flags |= FL_SHIELDED;
		NPCS.NPC->client->ps.powerups[PW_GALAK_SHIELD] = Q3_INFINITE;
		NPC_SetSurfaceOnOff(self, "force_shield", TURN_ON);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void BubbleShield_TurnOff(gentity_t* self)
{
	if (BubbleShield_IsOn(self))
	{
		self->flags &= ~FL_SHIELDED;
		NPCS.NPC->client->ps.powerups[PW_GALAK_SHIELD] = 0;
		NPC_SetSurfaceOnOff(self, "force_shield", TURN_OFF);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Push A Particular Ent
////////////////////////////////////////////////////////////////////////////////////////
void BubbleShield_PushEnt(gentity_t* pushed, vec3_t smack_dir)
{
	G_Damage(pushed, NPCS.NPC, NPCS.NPC, smack_dir, NPCS.NPC->r.currentOrigin,
		(g_npcspskill.integer + 1) * Q_irand(5, 10), DAMAGE_NO_KNOCKBACK, MOD_ELECTROCUTE);
	g_throw(pushed, smack_dir, 10);

	// Make Em Electric
	//------------------
	if (pushed->client)
	{
		pushed->client->ps.electrifyTime = level.time + 1000;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Go Through All The Ents Within The Radius Of The Shield And Push Them
////////////////////////////////////////////////////////////////////////////////////////
void BubbleShield_PushRadiusEnts()
{
	int i;
	int entity_list[MAX_GENTITIES];
	const float radius = ASSASSIN_SHIELD_SIZE;
	vec3_t mins, maxs;

	for (i = 0; i < 3; i++)
	{
		mins[i] = NPCS.NPC->r.currentOrigin[i] - radius;
		maxs[i] = NPCS.NPC->r.currentOrigin[i] + radius;
	}

	const int num_ents = trap->EntitiesInBox(mins, maxs, entity_list, 128);
	for (i = 0; i < num_ents; i++)
	{
		vec3_t smack_dir;
		gentity_t* radius_ent = &g_entities[entity_list[i]];
		// Only Clients
		//--------------
		if (!radius_ent || !radius_ent->client)
		{
			continue;
		}

		// Don't Push Away Other Assassin Droids
		//---------------------------------------
		if (radius_ent->client->NPC_class == NPCS.NPC->client->NPC_class)
		{
			continue;
		}

		// Should Have Already Pushed The Enemy If He Touched Us
		//-------------------------------------------------------
		if (NPCS.NPC->enemy && NPCS.NPCInfo->touchedByPlayer == NPCS.NPC->enemy && radius_ent == NPCS.NPC->enemy)
		{
			continue;
		}

		// Do The Vector Distance Test
		//-----------------------------
		VectorSubtract(radius_ent->r.currentOrigin, NPCS.NPC->r.currentOrigin, smack_dir);
		const float smackDist = VectorNormalize(smack_dir);
		if (smackDist < radius)
		{
			BubbleShield_PushEnt(radius_ent, smack_dir);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void bubble_shield_update(void)
{
	// Shields Go When You Die
	//-------------------------
	if (NPCS.NPC->health <= 0)
	{
		BubbleShield_TurnOff(NPCS.NPC);
		return;
	}

	// Recharge Shields
	//------------------
	NPCS.NPC->client->ps.stats[STAT_ARMOR] += 1;
	if (NPCS.NPC->client->ps.stats[STAT_ARMOR] > 250)
	{
		NPCS.NPC->client->ps.stats[STAT_ARMOR] = 250;
	}

	// If We Have Enough Armor And Are Not Shooting Right Now, Kick The Shield On
	//----------------------------------------------------------------------------
	if (NPCS.NPC->client->ps.stats[STAT_ARMOR] > 100 && TIMER_Done(NPCS.NPC, "ShieldsDown"))
	{
		// Check On Timers To Raise And Lower Shields
		//--------------------------------------------
		if (level.time - NPCS.NPCInfo->enemyLastSeenTime < 1000 && TIMER_Done(NPCS.NPC, "ShieldsUp"))
		{
			TIMER_Set(NPCS.NPC, "ShieldsDown", 2000); // Drop Shields
			TIMER_Set(NPCS.NPC, "ShieldsUp", Q_irand(4000, 5000)); // Then Bring Them Back Up For At Least 3 sec
		}

		BubbleShield_TurnOn(NPCS.NPC);
		if (BubbleShield_IsOn(NPCS.NPC))
		{
			// Update Our Shader Value
			//-------------------------
			NPCS.NPC->s.customRGBA[0] =
				NPCS.NPC->s.customRGBA[1] =
				NPCS.NPC->s.customRGBA[2] =
				NPCS.NPC->s.customRGBA[3] = NPCS.NPC->client->ps.stats[STAT_ARMOR] - 100;

			// If Touched By An Enemy, ALWAYS Shove Them
			//-------------------------------------------
			if (NPCS.NPC->enemy && NPCS.NPCInfo->touchedByPlayer == NPCS.NPC->enemy)
			{
				vec3_t dir;
				VectorSubtract(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin, dir);
				VectorNormalize(dir);
				BubbleShield_PushEnt(NPCS.NPC->enemy, dir);
			}

			// Push Anybody Else Near
			//------------------------
			BubbleShield_PushRadiusEnts();
		}
	}
	else
	{
		BubbleShield_TurnOff(NPCS.NPC);
	}
}