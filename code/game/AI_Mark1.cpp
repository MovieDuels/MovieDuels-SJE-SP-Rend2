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

constexpr auto MIN_MELEE_RANGE = 320;
#define	MIN_MELEE_RANGE_SQR			( MIN_MELEE_RANGE * MIN_MELEE_RANGE )

constexpr auto MIN_DISTANCE = 128;
#define MIN_DISTANCE_SQR			( MIN_DISTANCE * MIN_DISTANCE )

constexpr auto TURN_OFF = 0x00000100;

constexpr auto LEFT_ARM_HEALTH = 40;
constexpr auto RIGHT_ARM_HEALTH = 40;
constexpr auto AMMO_POD_HEALTH = 40;

//Local state enums
enum
{
	LSTATE_NONE = 0,
	LSTATE_ASLEEP,
	LSTATE_WAKEUP,
	LSTATE_FIRED0,
	LSTATE_FIRED1,
	LSTATE_FIRED2,
	LSTATE_FIRED3,
	LSTATE_FIRED4,
};

qboolean NPC_CheckPlayerTeamStealth();
gentity_t* create_missile(vec3_t org, vec3_t dir, float vel, int life, gentity_t* owner, qboolean alt_fire = qfalse);
void Mark1_BlasterAttack(qboolean advance);
void DeathFX(const gentity_t* ent);
extern gitem_t* FindItemForAmmo(ammo_t ammo);

/*
-------------------------
NPC_Mark1_Precache
-------------------------
*/
void NPC_Mark1_Precache()
{
	G_SoundIndex("sound/chars/mark1/misc/mark1_wakeup");
	G_SoundIndex("sound/chars/mark1/misc/shutdown");
	G_SoundIndex("sound/chars/mark1/misc/walk");
	G_SoundIndex("sound/chars/mark1/misc/run");
	G_SoundIndex("sound/chars/mark1/misc/death1");
	G_SoundIndex("sound/chars/mark1/misc/death2");
	G_SoundIndex("sound/chars/mark1/misc/anger");
	G_SoundIndex("sound/chars/mark1/misc/mark1_fire");
	G_SoundIndex("sound/chars/mark1/misc/mark1_pain");
	G_SoundIndex("sound/chars/mark1/misc/mark1_explo");

	//	G_EffectIndex( "small_chunks");
	G_EffectIndex("env/med_explode2");
	G_EffectIndex("explosions/probeexplosion1");
	G_EffectIndex("blaster/smoke_bolton");
	G_EffectIndex("bryar/muzzle_flash");
	G_EffectIndex("explosions/droidexplosion1");

	RegisterItem(FindItemForAmmo(AMMO_METAL_BOLTS));
	RegisterItem(FindItemForAmmo(AMMO_BLASTER));
	RegisterItem(FindItemForWeapon(WP_BOWCASTER));
	RegisterItem(FindItemForWeapon(WP_BRYAR_PISTOL));
}

/*
-------------------------
NPC_Mark1_Part_Explode
-------------------------
*/
static void NPC_Mark1_Part_Explode(gentity_t* self, const int bolt)
{
	if (bolt >= 0)
	{
		mdxaBone_t bolt_matrix;
		vec3_t org, dir;

		gi.G2API_GetBoltMatrix(self->ghoul2, self->playerModel,
			bolt,
			&bolt_matrix, self->currentAngles, self->currentOrigin, cg.time ? cg.time : level.time,
			nullptr, self->s.modelScale);

		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, org);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, dir);

		G_PlayEffect("env/med_explode2", org, dir);
		G_PlayEffect(G_EffectIndex("blaster/smoke_bolton"), self->playerModel, bolt, self->s.number, org);
	}
}

/*
-------------------------
Mark1_Idle
-------------------------
*/
static void Mark1_Idle()
{
	NPC_BSIdle();

	NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_SLEEP1, SETANIM_FLAG_NORMAL);
}

/*
-------------------------
Mark1Dead_FireRocket
- Shoot the left weapon, the multi-blaster
-------------------------
*/
static void Mark1Dead_FireRocket()
{
	mdxaBone_t bolt_matrix;
	vec3_t muzzle1, muzzle_dir;

	constexpr int damage = 50;

	gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel,
		NPC->genericBolt5,
		&bolt_matrix, NPC->currentAngles, NPC->currentOrigin, cg.time ? cg.time : level.time,
		nullptr, NPC->s.modelScale);

	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, muzzle1);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, muzzle_dir);

	G_PlayEffect("bryar/muzzle_flash", muzzle1, muzzle_dir);

	G_Sound(NPC, G_SoundIndex("sound/chars/mark1/misc/mark1_fire"));

	gentity_t* missile = create_missile(muzzle1, muzzle_dir, BOWCASTER_VELOCITY, 10000, NPC);

	missile->classname = "bowcaster_proj";
	missile->s.weapon = WP_BOWCASTER;

	VectorSet(missile->maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
	VectorScale(missile->maxs, -1, missile->mins);

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_ENERGY;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = BOWCASTER_SPLASH_DAMAGE;
	missile->splashRadius = BOWCASTER_SPLASH_RADIUS;

	// we don't want it to bounce
	missile->bounceCount = 0;
}

/*
-------------------------
Mark1Dead_FireBlaster
- Shoot the left weapon, the multi-blaster
-------------------------
*/
static void Mark1Dead_FireBlaster()
{
	vec3_t muzzle1, muzzle_dir;
	mdxaBone_t bolt_matrix;

	const int bolt = NPC->genericBolt1;

	gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel,
		bolt,
		&bolt_matrix, NPC->currentAngles, NPC->currentOrigin, cg.time ? cg.time : level.time,
		nullptr, NPC->s.modelScale);

	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, muzzle1);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, muzzle_dir);

	G_PlayEffect("bryar/muzzle_flash", muzzle1, muzzle_dir);

	gentity_t* missile = create_missile(muzzle1, muzzle_dir, 1600, 10000, NPC);

	G_Sound(NPC, G_SoundIndex("sound/chars/mark1/misc/mark1_fire"));

	missile->classname = "bryar_proj";
	missile->s.weapon = WP_BRYAR_PISTOL;

	missile->damage = 1;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_ENERGY;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
}

/*
-------------------------
Mark1_die
-------------------------
*/
void Mark1_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod, int d_flags, int hit_loc)
{
	G_Sound(self, G_SoundIndex(va("sound/chars/mark1/misc/death%d.wav", Q_irand(1, 2))));

	// Choose a death anim
	if (Q_irand(1, 10) > 5)
	{
		NPC_SetAnim(self, SETANIM_BOTH, BOTH_DEATH2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}
	else
	{
		NPC_SetAnim(self, SETANIM_BOTH, BOTH_DEATH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}
}

/*
-------------------------
Mark1_dying
-------------------------
*/
void Mark1_dying(gentity_t* self)
{
	if (self->client->ps.torsoAnimTimer > 0)
	{
		if (TIMER_Done(self, "dyingExplosion"))
		{
			int new_bolt;
			int num = Q_irand(1, 3);

			// Find place to generate explosion
			if (num == 1)
			{
				num = Q_irand(8, 10);
				new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], va("*flash%d", num));
				NPC_Mark1_Part_Explode(self, new_bolt);
			}
			else
			{
				num = Q_irand(1, 6);
				new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], va("*torso_tube%d", num));
				NPC_Mark1_Part_Explode(self, new_bolt);
				gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], va("torso_tube%d", num), TURN_OFF);
			}

			TIMER_Set(self, "dyingExplosion", Q_irand(300, 1000));
		}

		// See which weapons are there
		// Randomly fire blaster
		if (!gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel], "l_arm"))
			// Is the blaster still on the model?
		{
			if (Q_irand(1, 5) == 1)
			{
				SaveNPCGlobals();
				SetNPCGlobals(self);
				Mark1Dead_FireBlaster();
				RestoreNPCGlobals();
			}
		}

		// Randomly fire rocket
		if (!gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel], "r_arm"))
			// Is the rocket still on the model?
		{
			if (Q_irand(1, 10) == 1)
			{
				SaveNPCGlobals();
				SetNPCGlobals(self);
				Mark1Dead_FireRocket();
				RestoreNPCGlobals();
			}
		}
	}
}

/*
-------------------------
NPC_Mark1_Pain
- look at what was hit and see if it should be removed from the model.
-------------------------
*/
void NPC_Mark1_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, const vec3_t point, const int damage,
	const int mod,
	const int hit_loc)
{
	int new_bolt;

	NPC_Pain(self, inflictor, attacker, point, damage, mod);

	G_Sound(self, G_SoundIndex("sound/chars/mark1/misc/mark1_pain"));

	// Hit in the CHEST???
	if (hit_loc == HL_CHEST)
	{
		const int chance = Q_irand(1, 4);

		if (chance == 1 && damage > 5)
		{
			NPC_SetAnim(self, SETANIM_BOTH, BOTH_PAIN1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}
	// Hit in the left arm?
	else if (hit_loc == HL_ARM_LT && self->locationDamage[HL_ARM_LT] > LEFT_ARM_HEALTH)
	{
		if (self->locationDamage[hit_loc] >= LEFT_ARM_HEALTH) // Blow it up?
		{
			new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], "*flash3");
			if (new_bolt != -1)
			{
				NPC_Mark1_Part_Explode(self, new_bolt);
			}

			gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], "l_arm", TURN_OFF);
		}
	}
	// Hit in the right arm?
	else if (hit_loc == HL_ARM_RT && self->locationDamage[HL_ARM_RT] > RIGHT_ARM_HEALTH) // Blow it up?
	{
		if (self->locationDamage[hit_loc] >= RIGHT_ARM_HEALTH)
		{
			new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], "*flash4");
			if (new_bolt != -1)
			{
				//				G_PlayEffect( "small_chunks", self->playerModel, self->genericBolt2, self->s.number);
				NPC_Mark1_Part_Explode(self, new_bolt);
			}

			gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], "r_arm", TURN_OFF);
		}
	}
	// Check ammo pods
	else
	{
		for (int i = 0; i < 6; i++)
		{
			if (hit_loc == HL_GENERIC1 + i && self->locationDamage[HL_GENERIC1 + i] > AMMO_POD_HEALTH) // Blow it up?
			{
				if (self->locationDamage[hit_loc] >= AMMO_POD_HEALTH)
				{
					new_bolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], va("*torso_tube%d", i + 1));
					if (new_bolt != -1)
					{
						NPC_Mark1_Part_Explode(self, new_bolt);
					}
					gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], va("torso_tube%d", i + 1), TURN_OFF);
					NPC_SetAnim(self, SETANIM_BOTH, BOTH_PAIN1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					break;
				}
			}
		}
	}

	// Are both guns shot off?
	if (gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel], "l_arm") &&
		gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel], "r_arm"))
	{
		G_Damage(self, nullptr, nullptr, nullptr, nullptr, self->health, 0, MOD_UNKNOWN);
	}
}

/*
-------------------------
Mark1_Hunt
- look for enemy.
-------------------------`
*/
static void Mark1_Hunt()
{
	if (NPCInfo->goalEntity == nullptr)
	{
		NPCInfo->goalEntity = NPC->enemy;
	}

	NPC_FaceEnemy(qtrue);

	NPCInfo->combatMove = qtrue;
	NPC_MoveToGoal(qtrue);
}

/*
-------------------------
Mark1_FireBlaster
- Shoot the left weapon, the multi-blaster
-------------------------
*/
static void Mark1_FireBlaster()
{
	vec3_t muzzle1;
	static vec3_t forward, vright, up;
	mdxaBone_t bolt_matrix;
	int bolt;

	// Which muzzle to fire from?
	if (NPCInfo->localState <= LSTATE_FIRED0 || NPCInfo->localState == LSTATE_FIRED4)
	{
		NPCInfo->localState = LSTATE_FIRED1;
		bolt = NPC->genericBolt1;
	}
	else if (NPCInfo->localState == LSTATE_FIRED1)
	{
		NPCInfo->localState = LSTATE_FIRED2;
		bolt = NPC->genericBolt2;
	}
	else if (NPCInfo->localState == LSTATE_FIRED2)
	{
		NPCInfo->localState = LSTATE_FIRED3;
		bolt = NPC->genericBolt3;
	}
	else
	{
		NPCInfo->localState = LSTATE_FIRED4;
		bolt = NPC->genericBolt4;
	}

	gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel,
		bolt,
		&bolt_matrix, NPC->currentAngles, NPC->currentOrigin, cg.time ? cg.time : level.time,
		nullptr, NPC->s.modelScale);

	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, muzzle1);

	if (NPC->health)
	{
		vec3_t angle_to_enemy1;
		vec3_t delta1;
		vec3_t enemy_org1;
		CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemy_org1);
		VectorSubtract(enemy_org1, muzzle1, delta1);
		vectoangles(delta1, angle_to_enemy1);
		AngleVectors(angle_to_enemy1, forward, vright, up);
	}
	else
	{
		AngleVectors(NPC->currentAngles, forward, vright, up);
	}

	G_PlayEffect("bryar/muzzle_flash", muzzle1, forward);

	G_Sound(NPC, G_SoundIndex("sound/chars/mark1/misc/mark1_fire"));

	gentity_t* missile = create_missile(muzzle1, forward, 1600, 10000, NPC);

	missile->classname = "bryar_proj";
	missile->s.weapon = WP_BRYAR_PISTOL;

	missile->damage = 1;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_ENERGY;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
}

/*
-------------------------
Mark1_BlasterAttack
-------------------------
*/
void Mark1_BlasterAttack(const qboolean advance)
{
	if (TIMER_Done(NPC, "attackDelay")) // Attack?
	{
		int chance = Q_irand(1, 5);

		NPCInfo->burstCount++;

		if (NPCInfo->burstCount < 3) // Too few shots this burst?
		{
			chance = 2; // Force it to keep firing.
		}
		else if (NPCInfo->burstCount > 12) // Too many shots fired this burst?
		{
			NPCInfo->burstCount = 0;
			chance = 1; // Force it to stop firing.
		}

		// Stop firing.
		if (chance == 1)
		{
			NPCInfo->burstCount = 0;
			TIMER_Set(NPC, "attackDelay", Q_irand(1000, 3000));
			NPC->client->ps.torsoAnimTimer = 0; // Just in case the firing anim is running.
		}
		else
		{
			if (TIMER_Done(NPC, "attackDelay2")) // Can't be shooting every frame.
			{
				TIMER_Set(NPC, "attackDelay2", Q_irand(50, 50));
				Mark1_FireBlaster();
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_ATTACK1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
	}
	else if (advance)
	{
		if (NPC->client->ps.torsoAnim == BOTH_ATTACK1)
		{
			NPC->client->ps.torsoAnimTimer = 0; // Just in case the firing anim is running.
		}
		Mark1_Hunt();
	}
	else // Make sure he's not firing.
	{
		if (NPC->client->ps.torsoAnim == BOTH_ATTACK1)
		{
			NPC->client->ps.torsoAnimTimer = 0; // Just in case the firing anim is running.
		}
	}
}

/*
-------------------------
Mark1_FireRocket
-------------------------
*/
static void Mark1_FireRocket()
{
	mdxaBone_t bolt_matrix;
	vec3_t muzzle1, enemy_org1, delta1, angle_to_enemy1;
	static vec3_t forward, vright, up;

	constexpr int damage = 50;

	gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel,
		NPC->genericBolt5,
		&bolt_matrix, NPC->currentAngles, NPC->currentOrigin, cg.time ? cg.time : level.time,
		nullptr, NPC->s.modelScale);

	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, muzzle1);

	CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemy_org1);
	VectorSubtract(enemy_org1, muzzle1, delta1);
	vectoangles(delta1, angle_to_enemy1);
	AngleVectors(angle_to_enemy1, forward, vright, up);

	G_Sound(NPC, G_SoundIndex("sound/chars/mark1/misc/mark1_fire"));

	gentity_t* missile = create_missile(muzzle1, forward, BOWCASTER_VELOCITY, 10000, NPC);

	missile->classname = "bowcaster_proj";
	missile->s.weapon = WP_BOWCASTER;

	VectorSet(missile->maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
	VectorScale(missile->maxs, -1, missile->mins);

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_ENERGY;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = BOWCASTER_SPLASH_DAMAGE;
	missile->splashRadius = BOWCASTER_SPLASH_RADIUS;

	// we don't want it to bounce
	missile->bounceCount = 0;
}

/*
-------------------------
Mark1_RocketAttack
-------------------------
*/
static void Mark1_RocketAttack(const qboolean advance)
{
	if (TIMER_Done(NPC, "attackDelay")) // Attack?
	{
		TIMER_Set(NPC, "attackDelay", Q_irand(1000, 3000));
		NPC_SetAnim(NPC, SETANIM_TORSO, BOTH_ATTACK2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		Mark1_FireRocket();
	}
	else if (advance)
	{
		Mark1_Hunt();
	}
}

/*
-------------------------
Mark1_AttackDecision
-------------------------
*/
static void Mark1_AttackDecision()
{
	//randomly talk
	if (TIMER_Done(NPC, "patrolNoise"))
	{
		if (TIMER_Done(NPC, "angerNoise"))
		{
			//			G_Sound( NPC, G_SoundIndex(va("sound/chars/mark1/misc/talk%d.wav",	Q_irand(1, 4))));
			TIMER_Set(NPC, "patrolNoise", Q_irand(4000, 10000));
		}
	}

	// Enemy is dead or he has no enemy.
	if (NPC->enemy->health < 1 || NPC_CheckEnemyExt() == qfalse)
	{
		NPC->enemy = nullptr;
		return;
	}

	// Rate our distance to the target and visibility
	const float distance = static_cast<int>(DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin));
	distance_e dist_rate = distance > MIN_MELEE_RANGE_SQR ? DIST_LONG : DIST_MELEE;
	const qboolean visible = NPC_ClearLOS(NPC->enemy);
	const auto advance = static_cast<qboolean>(distance > MIN_DISTANCE_SQR);

	// If we cannot see our target, move to see it
	if (!visible || !NPC_FaceEnemy(qtrue))
	{
		Mark1_Hunt();
		return;
	}

	// See if the side weapons are there
	const int blaster_test = gi.G2API_GetSurfaceRenderStatus(&NPC->ghoul2[NPC->playerModel], "l_arm");
	const int rocket_test = gi.G2API_GetSurfaceRenderStatus(&NPC->ghoul2[NPC->playerModel], "r_arm");

	// It has both side weapons
	if (!blaster_test && !rocket_test)
	{
	}
	else if (blaster_test)
	{
		dist_rate = DIST_LONG;
	}
	else if (rocket_test)
	{
		dist_rate = DIST_MELEE;
	}
	else // It should never get here, but just in case
	{
		NPC->health = 0;
		NPC->client->ps.stats[STAT_HEALTH] = 0;
		GEntity_DieFunc(NPC, NPC, NPC, 100, MOD_UNKNOWN);
	}

	// We can see enemy so shoot him if timers let you.
	NPC_FaceEnemy(qtrue);

	if (dist_rate == DIST_MELEE)
	{
		Mark1_BlasterAttack(advance);
	}
	else if (dist_rate == DIST_LONG)
	{
		Mark1_RocketAttack(advance);
	}
}

/*
-------------------------
Mark1_Patrol
-------------------------
*/
static void Mark1_Patrol()
{
	if (NPC_CheckPlayerTeamStealth())
	{
		G_Sound(NPC, G_SoundIndex("sound/chars/mark1/misc/mark1_wakeup"));
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
NPC_BSMark1_Default
-------------------------
*/
void NPC_BSMark1_Default()
{
	if (NPC->enemy)
	{
		NPCInfo->goalEntity = NPC->enemy;
		Mark1_AttackDecision();
	}
	else if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
	{
		Mark1_Patrol();
	}
	else
	{
		Mark1_Idle();
	}
}