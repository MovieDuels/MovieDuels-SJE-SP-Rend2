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
#include "g_functions.h"
#include "../cgame/cg_local.h"

constexpr auto MIN_MELEE_RANGE = 640;
#define	MIN_MELEE_RANGE_SQR	( MIN_MELEE_RANGE * MIN_MELEE_RANGE )

constexpr auto MIN_DISTANCE = 128;
#define MIN_DISTANCE_SQR	( MIN_DISTANCE * MIN_DISTANCE )

#define TURN_OFF			0x00000100//G2SURFACEFLAG_NODESCENDANTS

constexpr auto LEFT_ARM_HEALTH = 40;
constexpr auto RIGHT_ARM_HEALTH = 40;

/*
-------------------------
NPC_ATST_Precache
-------------------------
*/
void NPC_ATST_Precache()
{
	G_SoundIndex("sound/chars/atst/atst_damaged1");
	G_SoundIndex("sound/chars/atst/atst_damaged2");

	RegisterItem(FindItemForWeapon(WP_ATST_MAIN)); //precache the weapon
	RegisterItem(FindItemForWeapon(WP_BOWCASTER)); //precache the weapon
	RegisterItem(FindItemForWeapon(WP_ROCKET_LAUNCHER)); //precache the weapon

	G_EffectIndex("env/med_explode2");
	//	G_EffectIndex( "smaller_chunks" );
	G_EffectIndex("blaster/smoke_bolton");
	G_EffectIndex("explosions/droidexplosion1");
}

//-----------------------------------------------------------------
static void ATST_PlayEffect(gentity_t* self, const int bolt_id, const char* fx)
{
	if (bolt_id >= 0 && fx && fx[0])
	{
		mdxaBone_t bolt_matrix;
		vec3_t org, dir;

		gi.G2API_GetBoltMatrix(self->ghoul2, self->playerModel,
			bolt_id,
			&bolt_matrix, self->currentAngles, self->currentOrigin, cg.time ? cg.time : level.time,
			nullptr, self->s.modelScale);

		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, org);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, dir);

		G_PlayEffect(fx, org, dir);
	}
}

/*
-------------------------
G_ATSTCheckPain

Called by NPC's and player in an ATST
-------------------------
*/

void G_ATSTCheckPain(gentity_t* self, gentity_t* other, const vec3_t point, int damage, int mod, const int hit_loc)
{
	int new_bolt;

	if (rand() & 1)
	{
		G_SoundOnEnt(self, CHAN_LESS_ATTEN, "sound/chars/atst/atst_damaged1");
	}
	else
	{
		G_SoundOnEnt(self, CHAN_LESS_ATTEN, "sound/chars/atst/atst_damaged2");
	}

	if (hit_loc == HL_ARM_LT && self->locationDamage[HL_ARM_LT] > LEFT_ARM_HEALTH)
	{
		if (self->locationDamage[hit_loc] >= LEFT_ARM_HEALTH) // Blow it up?
		{
			new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], "*flash3");
			if (new_bolt != -1)
			{
				ATST_PlayEffect(self, self->genericBolt1, "env/med_explode2");
				G_PlayEffect(G_EffectIndex("blaster/smoke_bolton"), self->playerModel, new_bolt, self->s.number, point);
			}

			gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], "head_light_blaster_cann", TURN_OFF);
		}
	}
	else if (hit_loc == HL_ARM_RT && self->locationDamage[HL_ARM_RT] > RIGHT_ARM_HEALTH) // Blow it up?
	{
		if (self->locationDamage[hit_loc] >= RIGHT_ARM_HEALTH)
		{
			new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], "*flash4");
			if (new_bolt != -1)
			{
				ATST_PlayEffect(self, self->genericBolt2, "env/med_explode2");
				G_PlayEffect(G_EffectIndex("blaster/smoke_bolton"), self->playerModel, new_bolt, self->s.number, point);
			}

			gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], "head_concussion_charger", TURN_OFF);
		}
	}
}

/*
-------------------------
NPC_ATST_Pain
-------------------------
*/
void NPC_ATST_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, const vec3_t point, const int damage,
	const int mod,
	const int hit_loc)
{
	G_ATSTCheckPain(self, attacker, point, damage, mod, hit_loc);
	NPC_Pain(self, inflictor, attacker, point, damage, mod);
}

/*
-------------------------
ATST_Hunt
-------------------------`
*/
static void ATST_Hunt()
{
	if (NPCInfo->goalEntity == nullptr)
	{
		//hunt
		NPCInfo->goalEntity = NPC->enemy;
	}

	NPCInfo->combatMove = qtrue;

	NPC_MoveToGoal(qtrue);
}

/*
-------------------------
ATST_Ranged
-------------------------
*/
static void ATST_Ranged(const qboolean visible, const qboolean alt_attack)
{
	if (TIMER_Done(NPC, "atkDelay") && visible) // Attack?
	{
		TIMER_Set(NPC, "atkDelay", Q_irand(500, 3000));

		if (alt_attack)
		{
			ucmd.buttons |= BUTTON_ATTACK | BUTTON_ALT_ATTACK;
		}
		else
		{
			ucmd.buttons |= BUTTON_ATTACK;
		}
	}

	if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
	{
		ATST_Hunt();
	}
}

/*
-------------------------
ATST_Attack
-------------------------
*/
static void ATST_Attack()
{
	qboolean alt_attack = qfalse;
	int blaster_test, charger_test;

	if (NPC_CheckEnemyExt() == qfalse) //!NPC->enemy )//
	{
		NPC->enemy = nullptr;
		return;
	}

	NPC_FaceEnemy(qtrue);

	// Rate our distance to the target, and our visibilty
	const float distance = static_cast<int>(DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin));
	const distance_e dist_rate = distance > MIN_MELEE_RANGE_SQR ? DIST_LONG : DIST_MELEE;
	const qboolean visible = NPC_ClearLOS(NPC->enemy);

	// If we cannot see our target, move to see it
	if (visible == qfalse)
	{
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			ATST_Hunt();
			return;
		}
	}

	// Decide what type of attack to do
	switch (dist_rate)
	{
	case DIST_MELEE:
		NPC_ChangeWeapon(WP_ATST_MAIN);
		break;

	case DIST_LONG:

		NPC_ChangeWeapon(WP_ATST_SIDE);

		// See if the side weapons are there
		blaster_test = gi.G2API_GetSurfaceRenderStatus(&NPC->ghoul2[NPC->playerModel], "head_light_blaster_cann");
		charger_test = gi.G2API_GetSurfaceRenderStatus(&NPC->ghoul2[NPC->playerModel], "head_concussion_charger");

		// It has both side weapons
		if (!(blaster_test & TURN_OFF) && !(charger_test & TURN_OFF))
		{
			const int weapon = Q_irand(0, 1); // 0 is blaster, 1 is charger (ALT SIDE)

			if (weapon) // Fire charger
			{
				alt_attack = qtrue;
			}
			else
			{
				alt_attack = qfalse;
			}
		}
		else if (!(blaster_test & TURN_OFF)) // Blaster is on
		{
			alt_attack = qfalse;
		}
		else if (!(charger_test & TURN_OFF)) // Blaster is on
		{
			alt_attack = qtrue;
		}
		else
		{
			NPC_ChangeWeapon(WP_NONE);
		}
		break;
	}

	NPC_FaceEnemy(qtrue);

	ATST_Ranged(visible, alt_attack);
}

/*
-------------------------
ATST_Patrol
-------------------------
*/
static void ATST_Patrol()
{
	if (NPC_CheckPlayerTeamStealth())
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	//If we have somewhere to go, then do that
	if (!NPC->enemy)
	{
		if (UpdateGoal())
		{
			ucmd.buttons |= BUTTON_WALKING;
			NPC_MoveToGoal(qtrue);
			NPC_UpdateAngles(qtrue, qtrue);
		}
	}
}

/*
-------------------------
ATST_Idle
-------------------------
*/
static void ATST_Idle()
{
	NPC_BSIdle();

	NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_STAND1, SETANIM_FLAG_NORMAL);
}

/*
-------------------------
NPC_BSDroid_Default
-------------------------
*/
void NPC_BSATST_Default()
{
	if (NPC->enemy)
	{
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			NPCInfo->goalEntity = NPC->enemy;
		}
		ATST_Attack();
	}
	else if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
	{
		ATST_Patrol();
	}
	else
	{
		ATST_Idle();
	}
}