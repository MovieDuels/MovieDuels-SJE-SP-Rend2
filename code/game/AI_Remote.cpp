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
#include "../cgame/cg_local.h"
#include "g_functions.h"

gentity_t* create_missile(vec3_t org, vec3_t dir, float vel, int life, gentity_t* owner, qboolean alt_fire = qfalse);
void Remote_Strafe();

constexpr auto VELOCITY_DECAY = 0.85f;

//Local state enums
enum
{
	LSTATE_NONE = 0,
};

void Remote_Idle();

void NPC_Remote_Precache()
{
	G_SoundIndex("sound/chars/remote/misc/fire.wav");
	G_SoundIndex("sound/chars/remote/misc/hiss.wav");
	G_EffectIndex("env/small_explode");
}

/*
-------------------------
NPC_Remote_Pain
-------------------------
*/
void NPC_Remote_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* other, const vec3_t point, const int damage,
	const int mod,
	int hit_loc)
{
	SaveNPCGlobals();
	SetNPCGlobals(self);
	Remote_Strafe();
	RestoreNPCGlobals();

	NPC_Pain(self, inflictor, other, point, damage, mod);
}

/*
-------------------------
Remote_MaintainHeight
-------------------------
*/
static void Remote_MaintainHeight()
{
	float dif;

	// Update our angles regardless
	NPC_UpdateAngles(qtrue, qtrue);

	if (NPC->client->ps.velocity[2])
	{
		NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

		if (fabs(NPC->client->ps.velocity[2]) < 2)
		{
			NPC->client->ps.velocity[2] = 0;
		}
	}
	// If we have an enemy, we should try to hover at or a little below enemy eye level
	if (NPC->enemy)
	{
		if (TIMER_Done(NPC, "heightChange"))
		{
			TIMER_Set(NPC, "heightChange", Q_irand(1000, 3000));

			// Find the height difference
			dif = NPC->enemy->currentOrigin[2] + Q_irand(0, NPC->enemy->maxs[2] + 8) - NPC->currentOrigin[2];

			// cap to prevent dramatic height shifts
			if (fabs(dif) > 2)
			{
				if (fabs(dif) > 24)
				{
					dif = dif < 0 ? -24 : 24;
				}
				dif *= 10;
				NPC->client->ps.velocity[2] = (NPC->client->ps.velocity[2] + dif) / 2;
				NPC->fx_time = level.time;
				G_Sound(NPC, G_SoundIndex("sound/chars/remote/misc/hiss.wav"));
			}
		}
	}
	else
	{
		const gentity_t* goal;

		if (NPCInfo->goalEntity) // Is there a goal?
		{
			goal = NPCInfo->goalEntity;
		}
		else
		{
			goal = NPCInfo->lastGoalEntity;
		}
		if (goal)
		{
			dif = goal->currentOrigin[2] - NPC->currentOrigin[2];

			if (fabs(dif) > 24)
			{
				dif = dif < 0 ? -24 : 24;
				NPC->client->ps.velocity[2] = (NPC->client->ps.velocity[2] + dif) / 2;
			}
		}
	}

	// Apply friction
	if (NPC->client->ps.velocity[0])
	{
		NPC->client->ps.velocity[0] *= VELOCITY_DECAY;

		if (fabs(NPC->client->ps.velocity[0]) < 1)
		{
			NPC->client->ps.velocity[0] = 0;
		}
	}

	if (NPC->client->ps.velocity[1])
	{
		NPC->client->ps.velocity[1] *= VELOCITY_DECAY;

		if (fabs(NPC->client->ps.velocity[1]) < 1)
		{
			NPC->client->ps.velocity[1] = 0;
		}
	}
}

constexpr auto REMOTE_STRAFE_VEL = 256;
constexpr auto REMOTE_STRAFE_DIS = 200;
constexpr auto REMOTE_UPWARD_PUSH = 32;

/*
-------------------------
Remote_Strafe
-------------------------
*/
void Remote_Strafe()
{
	vec3_t end, right;
	trace_t tr;

	AngleVectors(NPC->client->renderInfo.eyeAngles, nullptr, right, nullptr);

	// Pick a random strafe direction, then check to see if doing a strafe would be
	//	reasonable valid
	const int dir = rand() & 1 ? -1 : 1;
	VectorMA(NPC->currentOrigin, REMOTE_STRAFE_DIS * dir, right, end);

	gi.trace(&tr, NPC->currentOrigin, nullptr, nullptr, end, NPC->s.number, MASK_SOLID, static_cast<EG2_Collision>(0),
		0);

	// Close enough
	if (tr.fraction > 0.9f)
	{
		VectorMA(NPC->client->ps.velocity, REMOTE_STRAFE_VEL * dir, right, NPC->client->ps.velocity);

		G_Sound(NPC, G_SoundIndex("sound/chars/remote/misc/hiss.wav"));

		// Add a slight upward push
		NPC->client->ps.velocity[2] += REMOTE_UPWARD_PUSH;

		// Set the strafe start time so we can do a controlled roll
		NPC->fx_time = level.time;
		NPCInfo->standTime = level.time + 3000 + Q_flrand(0.0f, 1.0f) * 500;
	}
}

constexpr auto REMOTE_FORWARD_BASE_SPEED = 10;
constexpr auto REMOTE_FORWARD_MULTIPLIER = 5;

/*
-------------------------
Remote_Hunt
-------------------------
*/
static void Remote_Hunt(const qboolean visible, const qboolean advance, const qboolean retreat)
{
	vec3_t forward;

	//If we're not supposed to stand still, pursue the player
	if (NPCInfo->standTime < level.time)
	{
		// Only strafe when we can see the player
		if (visible)
		{
			Remote_Strafe();
			return;
		}
	}

	//If we don't want to advance, stop here
	if (advance == qfalse && visible == qtrue)
		return;

	//Only try and navigate if the player is visible
	if (visible == qfalse)
	{
		// Move towards our goal
		NPCInfo->goalEntity = NPC->enemy;
		NPCInfo->goalRadius = 12;

		NPC_MoveToGoal(qtrue);
		return;
	}
	VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, forward);
	/*distance = */
	VectorNormalize(forward);

	float speed = REMOTE_FORWARD_BASE_SPEED + REMOTE_FORWARD_MULTIPLIER * g_spskill->integer;
	if (retreat == qtrue)
	{
		speed *= -1;
	}
	VectorMA(NPC->client->ps.velocity, speed, forward, NPC->client->ps.velocity);
}

/*
-------------------------
Remote_Fire
-------------------------
*/
static void Remote_Fire()
{
	vec3_t delta1, enemy_org1, muzzle1;
	vec3_t angle_to_enemy1;
	static vec3_t forward, vright, up;

	CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemy_org1);
	VectorCopy(NPC->currentOrigin, muzzle1);

	VectorSubtract(enemy_org1, muzzle1, delta1);

	vectoangles(delta1, angle_to_enemy1);
	AngleVectors(angle_to_enemy1, forward, vright, up);

	gentity_t* missile = create_missile(NPC->currentOrigin, forward, 1000, 10000, NPC);

	G_PlayEffect("bryar/muzzle_flash", NPC->currentOrigin, forward);

	missile->classname = "briar";
	missile->s.weapon = WP_BRYAR_PISTOL;

	missile->damage = 10;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_ENERGY;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
}

/*
-------------------------
Remote_Ranged
-------------------------
*/
static void Remote_Ranged(const qboolean visible, const qboolean advance, const qboolean retreat)
{
	if (TIMER_Done(NPC, "attackDelay")) // Attack?
	{
		TIMER_Set(NPC, "attackDelay", Q_irand(500, 3000));
		Remote_Fire();
	}

	if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
	{
		Remote_Hunt(visible, advance, retreat);
	}
}

constexpr auto MIN_MELEE_RANGE = 320;
#define	MIN_MELEE_RANGE_SQR	( MIN_MELEE_RANGE * MIN_MELEE_RANGE )

constexpr auto MIN_DISTANCE = 80;
#define MIN_DISTANCE_SQR	( MIN_DISTANCE * MIN_DISTANCE )

/*
-------------------------
Remote_Attack
-------------------------
*/
static void Remote_Attack()
{
	if (TIMER_Done(NPC, "spin"))
	{
		TIMER_Set(NPC, "spin", Q_irand(250, 1500));
		NPCInfo->desiredYaw += Q_irand(-200, 200);
	}
	// Always keep a good height off the ground
	Remote_MaintainHeight();

	// If we don't have an enemy, just idle
	if (NPC_CheckEnemyExt() == qfalse)
	{
		Remote_Idle();
		return;
	}

	// Rate our distance to the target, and our visibility
	const float distance = static_cast<int>(DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin));
	//	distance_e	distRate	= ( distance > MIN_MELEE_RANGE_SQR ) ? DIST_LONG : DIST_MELEE;
	const qboolean visible = NPC_ClearLOS(NPC->enemy);
	const float idealDist = MIN_DISTANCE_SQR + MIN_DISTANCE_SQR * Q_flrand(0, 1);
	const auto advance = static_cast<qboolean>(distance > idealDist * 1.25);
	const auto retreat = static_cast<qboolean>(distance < idealDist * 0.75);

	// If we cannot see our target, move to see it
	if (visible == qfalse)
	{
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			Remote_Hunt(visible, advance, retreat);
			return;
		}
	}

	Remote_Ranged(visible, advance, retreat);
}

/*
-------------------------
Remote_Idle
-------------------------
*/
void Remote_Idle()
{
	Remote_MaintainHeight();

	NPC_BSIdle();
}

/*
-------------------------
Remote_Patrol
-------------------------
*/
static void Remote_Patrol()
{
	Remote_MaintainHeight();

	//If we have somewhere to go, then do that
	if (!NPC->enemy)
	{
		if (UpdateGoal())
		{
			//start loop sound once we move
			ucmd.buttons |= BUTTON_WALKING;
			NPC_MoveToGoal(qtrue);
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
-------------------------
NPC_BSRemote_Default
-------------------------
*/
void NPC_BSRemote_Default()
{
	if (NPC->enemy)
	{
		Remote_Attack();
	}
	else if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
	{
		Remote_Patrol();
	}
	else
	{
		Remote_Idle();
	}
}