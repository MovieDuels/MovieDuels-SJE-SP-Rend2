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
extern gitem_t* FindItemForAmmo(ammo_t ammo);

//Local state enums
enum
{
	LSTATE_NONE = 0,
	LSTATE_BACKINGUP,
	LSTATE_SPINNING,
	LSTATE_PAIN,
	LSTATE_DROP
};

void ImperialProbe_Idle();

void NPC_Probe_Precache()
{
	for (int i = 1; i < 4; i++)
	{
		G_SoundIndex(va("sound/chars/probe/misc/probetalk%d", i));
	}
	G_SoundIndex("sound/chars/probe/misc/probedroidloop");
	G_SoundIndex("sound/chars/probe/misc/anger1");
	G_SoundIndex("sound/chars/probe/misc/fire");

	G_EffectIndex("chunks/probehead");
	G_EffectIndex("env/med_explode2");
	G_EffectIndex("explosions/probeexplosion1");
	G_EffectIndex("bryar/muzzle_flash");

	RegisterItem(FindItemForAmmo(AMMO_BLASTER));
	RegisterItem(FindItemForWeapon(WP_BRYAR_PISTOL));
}

/*
-------------------------
Hunter_MaintainHeight
-------------------------
*/

constexpr auto VELOCITY_DECAY = 0.85f;

static void ImperialProbe_MaintainHeight()
{
	float dif;
	//	vec3_t	endPos;
	//	trace_t	trace;

	// Update our angles regardless
	NPC_UpdateAngles(qtrue, qtrue);

	// If we have an enemy, we should try to hover at about enemy eye level
	if (NPC->enemy)
	{
		// Find the height difference
		dif = NPC->enemy->currentOrigin[2] - NPC->currentOrigin[2];

		// cap to prevent dramatic height shifts
		if (fabs(dif) > 8)
		{
			if (fabs(dif) > 16)
			{
				dif = dif < 0 ? -16 : 16;
			}

			NPC->client->ps.velocity[2] = (NPC->client->ps.velocity[2] + dif) / 2;
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
				ucmd.upmove = ucmd.upmove < 0 ? -4 : 4;
			}
			else
			{
				if (NPC->client->ps.velocity[2])
				{
					NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

					if (fabs(NPC->client->ps.velocity[2]) < 2)
					{
						NPC->client->ps.velocity[2] = 0;
					}
				}
			}
		}
		// Apply friction
		else if (NPC->client->ps.velocity[2])
		{
			NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

			if (fabs(NPC->client->ps.velocity[2]) < 1)
			{
				NPC->client->ps.velocity[2] = 0;
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

/*
-------------------------
ImperialProbe_Strafe
-------------------------
*/

constexpr auto HUNTER_STRAFE_VEL = 256;
constexpr auto HUNTER_STRAFE_DIS = 200;
constexpr auto HUNTER_UPWARD_PUSH = 32;

static void ImperialProbe_Strafe()
{
	vec3_t end, right;
	trace_t tr;

	AngleVectors(NPC->client->renderInfo.eyeAngles, nullptr, right, nullptr);

	// Pick a random strafe direction, then check to see if doing a strafe would be
	//	reasonable valid
	const int dir = rand() & 1 ? -1 : 1;
	VectorMA(NPC->currentOrigin, HUNTER_STRAFE_DIS * dir, right, end);

	gi.trace(&tr, NPC->currentOrigin, nullptr, nullptr, end, NPC->s.number, MASK_SOLID, static_cast<EG2_Collision>(0),
		0);

	// Close enough
	if (tr.fraction > 0.9f)
	{
		VectorMA(NPC->client->ps.velocity, HUNTER_STRAFE_VEL * dir, right, NPC->client->ps.velocity);

		// Add a slight upward push
		NPC->client->ps.velocity[2] += HUNTER_UPWARD_PUSH;

		// Set the strafe start time so we can do a controlled roll
		NPC->fx_time = level.time;
		NPCInfo->standTime = level.time + 3000 + Q_flrand(0.0f, 1.0f) * 500;
	}
}

/*
-------------------------
ImperialProbe_Hunt
-------------------------`
*/

constexpr auto HUNTER_FORWARD_BASE_SPEED = 10;
constexpr auto HUNTER_FORWARD_MULTIPLIER = 5;

static void ImperialProbe_Hunt(const qboolean visible, const qboolean advance)
{
	vec3_t forward;

	NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_RUN1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

	//If we're not supposed to stand still, pursue the player
	if (NPCInfo->standTime < level.time)
	{
		// Only strafe when we can see the player
		if (visible)
		{
			ImperialProbe_Strafe();
			return;
		}
	}

	//If we don't want to advance, stop here
	if (advance == qfalse)
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

	const float speed = HUNTER_FORWARD_BASE_SPEED + HUNTER_FORWARD_MULTIPLIER * g_spskill->integer;
	VectorMA(NPC->client->ps.velocity, speed, forward, NPC->client->ps.velocity);
}

/*
-------------------------
ImperialProbe_FireBlaster
-------------------------
*/
static void ImperialProbe_FireBlaster()
{
	vec3_t muzzle1;
	static vec3_t forward, vright, up;
	mdxaBone_t bolt_matrix;

	//FIXME: use {0, NPC->client->ps.legsYaw, 0}
	gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel,
		NPC->genericBolt1,
		&bolt_matrix, NPC->currentAngles, NPC->currentOrigin, cg.time ? cg.time : level.time,
		nullptr, NPC->s.modelScale);

	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, muzzle1);

	G_PlayEffect("bryar/muzzle_flash", muzzle1);

	G_Sound(NPC, G_SoundIndex("sound/chars/probe/misc/fire"));

	if (NPC->health)
	{
		vec3_t angleToEnemy1;
		vec3_t delta1;
		vec3_t enemy_org1;
		CalcEntitySpot(NPC->enemy, SPOT_CHEST, enemy_org1);
		enemy_org1[0] += Q_irand(0, 10);
		enemy_org1[1] += Q_irand(0, 10);
		VectorSubtract(enemy_org1, muzzle1, delta1);
		vectoangles(delta1, angleToEnemy1);
		AngleVectors(angleToEnemy1, forward, vright, up);
	}
	else
	{
		AngleVectors(NPC->currentAngles, forward, vright, up);
	}

	gentity_t* missile = create_missile(muzzle1, forward, 1600, 10000, NPC);

	missile->classname = "bryar_proj";
	missile->s.weapon = WP_BRYAR_PISTOL;

	if (g_spskill->integer <= 1)
	{
		missile->damage = 5;
	}
	else
	{
		missile->damage = 10;
	}

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_ENERGY;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
}

/*
-------------------------
ImperialProbe_Ranged
-------------------------
*/
static void ImperialProbe_Ranged(const qboolean visible, const qboolean advance)
{
	if (TIMER_Done(NPC, "attackDelay")) // Attack?
	{
		int delay_max;
		int delay_min;
		if (g_spskill->integer == 0)
		{
			delay_min = 500;
			delay_max = 3000;
		}
		else if (g_spskill->integer > 1)
		{
			delay_min = 500;
			delay_max = 2000;
		}
		else
		{
			delay_min = 300;
			delay_max = 1500;
		}

		TIMER_Set(NPC, "attackDelay", Q_irand(delay_min, delay_max));
		ImperialProbe_FireBlaster();
		//		ucmd.buttons |= BUTTON_ATTACK;
	}

	if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
	{
		ImperialProbe_Hunt(visible, advance);
	}
}

/*
-------------------------
ImperialProbe_AttackDecision
-------------------------
*/

constexpr auto MIN_MELEE_RANGE = 320;
#define	MIN_MELEE_RANGE_SQR	( MIN_MELEE_RANGE * MIN_MELEE_RANGE )

constexpr auto MIN_DISTANCE = 128;
#define MIN_DISTANCE_SQR	( MIN_DISTANCE * MIN_DISTANCE )

static void ImperialProbe_AttackDecision()
{
	// Always keep a good height off the ground
	ImperialProbe_MaintainHeight();

	//randomly talk
	if (TIMER_Done(NPC, "patrolNoise"))
	{
		if (TIMER_Done(NPC, "angerNoise"))
		{
			G_SoundOnEnt(NPC, CHAN_AUTO, va("sound/chars/probe/misc/probetalk%d", Q_irand(1, 3)));

			TIMER_Set(NPC, "patrolNoise", Q_irand(4000, 10000));
		}
	}

	// If we don't have an enemy, just idle
	if (NPC_CheckEnemyExt() == qfalse)
	{
		ImperialProbe_Idle();
		return;
	}

	NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_RUN1, SETANIM_FLAG_NORMAL);

	// Rate our distance to the target, and our visibilty
	const float distance = static_cast<int>(DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin));
	//	distance_e	distRate	= ( distance > MIN_MELEE_RANGE_SQR ) ? DIST_LONG : DIST_MELEE;
	const qboolean visible = NPC_ClearLOS(NPC->enemy);
	const auto advance = static_cast<qboolean>(distance > MIN_DISTANCE_SQR);

	// If we cannot see our target, move to see it
	if (visible == qfalse)
	{
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			ImperialProbe_Hunt(visible, advance);
			return;
		}
	}

	// Sometimes I have problems with facing the enemy I'm attacking, so force the issue so I don't look dumb
	NPC_FaceEnemy(qtrue);

	// Decide what type of attack to do
	ImperialProbe_Ranged(visible, advance);
}

/*
-------------------------
NPC_BSDroid_Pain
-------------------------
*/
void NPC_Probe_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, const vec3_t point, const int damage,
	const int mod,
	int hit_loc)
{
	VectorCopy(self->NPC->lastPathAngles, self->s.angles);

	if (self->health < 30 || mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) // demp2 always messes them up real good
	{
		vec3_t end_pos;
		trace_t trace;

		VectorSet(end_pos, self->currentOrigin[0], self->currentOrigin[1], self->currentOrigin[2] - 128);
		gi.trace(&trace, self->currentOrigin, nullptr, nullptr, end_pos, self->s.number, MASK_SOLID,
			static_cast<EG2_Collision>(0), 0);

		if (trace.fraction == 1.0f || mod == MOD_DEMP2) // demp2 always does this
		{
			if (self->client->clientInfo.headModel != 0)
			{
				vec3_t origin;

				VectorCopy(self->currentOrigin, origin);
				origin[2] += 50;
				G_PlayEffect("chunks/probehead", origin);
				G_PlayEffect("env/med_explode2", origin);
				self->client->clientInfo.headModel = 0;
				self->client->moveType = MT_RUNJUMP;
				self->client->ps.gravity = g_gravity->value * .1;
			}

			if ((mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) && attacker)
			{
				vec3_t dir;

				NPC_SetAnim(self, SETANIM_BOTH, BOTH_PAIN1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

				VectorSubtract(self->currentOrigin, attacker->currentOrigin, dir);
				VectorNormalize(dir);

				VectorMA(self->client->ps.velocity, 550, dir, self->client->ps.velocity);
				self->client->ps.velocity[2] -= 127;
			}

			self->s.powerups |= 1 << PW_SHOCKED;
			self->client->ps.powerups[PW_SHOCKED] = level.time + 3000;

			self->NPC->localState = LSTATE_DROP;
		}
	}
	else
	{
		const float pain_chance = NPC_GetPainChance(self, damage);

		if (Q_flrand(0.0f, 1.0f) < pain_chance) // Spin around in pain?
		{
			NPC_SetAnim(self, SETANIM_BOTH, BOTH_PAIN1, SETANIM_FLAG_OVERRIDE);
		}
	}

	NPC_Pain(self, inflictor, attacker, point, damage, mod);
}

/*
-------------------------
ImperialProbe_Idle
-------------------------
*/

void ImperialProbe_Idle()
{
	ImperialProbe_MaintainHeight();

	NPC_BSIdle();
}

/*
-------------------------
NPC_BSImperialProbe_Patrol
-------------------------
*/
static void ImperialProbe_Patrol()
{
	ImperialProbe_MaintainHeight();

	if (NPC_CheckPlayerTeamStealth())
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	//If we have somewhere to go, then do that
	if (!NPC->enemy)
	{
		NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_RUN1, SETANIM_FLAG_NORMAL);

		if (UpdateGoal())
		{
			//start loop sound once we move
			NPC->s.loopSound = G_SoundIndex("sound/chars/probe/misc/probedroidloop");
			ucmd.buttons |= BUTTON_WALKING;
			NPC_MoveToGoal(qtrue);
		}
		//randomly talk
		if (TIMER_Done(NPC, "patrolNoise"))
		{
			G_SoundOnEnt(NPC, CHAN_AUTO, va("sound/chars/probe/misc/probetalk%d", Q_irand(1, 3)));

			TIMER_Set(NPC, "patrolNoise", Q_irand(2000, 4000));
		}
	}
	else // He's got an enemy. Make him angry.
	{
		G_SoundOnEnt(NPC, CHAN_AUTO, "sound/chars/probe/misc/anger1");
		TIMER_Set(NPC, "angerNoise", Q_irand(2000, 4000));
		//NPCInfo->behaviorState = BS_HUNT_AND_KILL;
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
-------------------------
ImperialProbe_Wait
-------------------------
*/
static void ImperialProbe_Wait()
{
	if (NPCInfo->localState == LSTATE_DROP)
	{
		vec3_t end_pos;
		trace_t trace;

		NPCInfo->desiredYaw = AngleNormalize360(NPCInfo->desiredYaw + 25);

		VectorSet(end_pos, NPC->currentOrigin[0], NPC->currentOrigin[1], NPC->currentOrigin[2] - 32);
		gi.trace(&trace, NPC->currentOrigin, nullptr, nullptr, end_pos, NPC->s.number, MASK_SOLID,
			static_cast<EG2_Collision>(0), 0);

		if (trace.fraction != 1.0f)
		{
			G_Damage(NPC, NPC->enemy, NPC->enemy, nullptr, nullptr, 2000, 0, MOD_UNKNOWN);
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
-------------------------
NPC_BSImperialProbe_Default
-------------------------
*/
void NPC_BSImperialProbe_Default()
{
	if (NPC->enemy)
	{
		NPCInfo->goalEntity = NPC->enemy;
		ImperialProbe_AttackDecision();
	}
	else if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
	{
		ImperialProbe_Patrol();
	}
	else if (NPCInfo->localState == LSTATE_DROP)
	{
		ImperialProbe_Wait();
	}
	else
	{
		ImperialProbe_Idle();
	}
}