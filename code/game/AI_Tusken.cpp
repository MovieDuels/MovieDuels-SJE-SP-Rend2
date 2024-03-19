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
#include "anims.h"
#include "g_navigator.h"
#include "../cgame/cg_local.h"
#include "g_functions.h"

extern void CG_DrawAlert(vec3_t origin, float rating);
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern void NPC_TempLookTarget(const gentity_t* self, int lookEntNum, int minLookTime, int maxLookTime);
extern qboolean G_ExpandPointToBBox(vec3_t point, const vec3_t mins, const vec3_t maxs, int ignore, int clipmask);
extern void NPC_AimAdjust(int change);
extern qboolean FlyingCreature(const gentity_t* ent);
extern int PM_AnimLength(int index, animNumber_t anim);

constexpr auto MAX_VIEW_DIST = 1024;
constexpr auto MAX_VIEW_SPEED = 250;
constexpr auto MAX_LIGHT_INTENSITY = 255;
constexpr auto MIN_LIGHT_THRESHOLD = 0.1;

constexpr auto DISTANCE_SCALE = 0.25f;
constexpr auto DISTANCE_THRESHOLD = 0.075f;
constexpr auto SPEED_SCALE = 0.25f;
constexpr auto FOV_SCALE = 0.5f;
constexpr auto LIGHT_SCALE = 0.25f;

constexpr auto REALIZE_THRESHOLD = 0.6f;
#define CAUTIOUS_THRESHOLD	( REALIZE_THRESHOLD * 0.75 )

qboolean NPC_CheckPlayerTeamStealth();

static float enemyDist;

//Local state enums
enum
{
	LSTATE_NONE = 0,
	LSTATE_UNDERFIRE,
	LSTATE_INVESTIGATE,
};

/*
-------------------------
NPC_Tusken_Precache
-------------------------
*/
void NPC_Tusken_Precache()
{
	for (int i = 1; i < 5; i++)
	{
		G_SoundIndex(va("sound/weapons/tusken_staff/stickhit%d.wav", i));
	}
}

static void Tusken_ClearTimers(const gentity_t* ent)
{
	TIMER_Set(ent, "chatter", 0);
	TIMER_Set(ent, "duck", 0);
	TIMER_Set(ent, "stand", 0);
	TIMER_Set(ent, "shuffleTime", 0);
	TIMER_Set(ent, "sleepTime", 0);
	TIMER_Set(ent, "enemyLastVisible", 0);
	TIMER_Set(ent, "roamTime", 0);
	TIMER_Set(ent, "hideTime", 0);
	TIMER_Set(ent, "attackDelay", 0); //FIXME: Slant for difficulty levels
	TIMER_Set(ent, "stick", 0);
	TIMER_Set(ent, "scoutTime", 0);
	TIMER_Set(ent, "flee", 0);
	TIMER_Set(ent, "taunting", 0);
}

static void NPC_Tusken_PlayConfusionSound(gentity_t* self)
{
	//FIXME: make this a custom sound in sound set
	if (self->health > 0)
	{
		G_AddVoiceEvent(self, Q_irand(EV_CONFUSE1, EV_CONFUSE3), 2000);
	}
	//reset him to be totally unaware again
	TIMER_Set(self, "enemyLastVisible", 0);
	TIMER_Set(self, "flee", 0);
	self->NPC->squadState = SQUAD_IDLE;
	self->NPC->tempBehavior = BS_DEFAULT;
	G_ClearEnemy(self); //FIXME: or just self->enemy = NULL;?

	self->NPC->investigateCount = 0;
}

/*
-------------------------
NPC_ST_Pain
-------------------------
*/

static void NPC_Tusken_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* other, vec3_t point, const int damage, const int mod)
{
	self->NPC->localState = LSTATE_UNDERFIRE;

	TIMER_Set(self, "duck", -1);
	TIMER_Set(self, "stand", 2000);

	NPC_Pain(self, inflictor, other, point, damage, mod);

	if (!damage && self->health > 0)
	{
		//FIXME: better way to know I was pushed
		G_AddVoiceEvent(self, Q_irand(EV_PUSHED1, EV_PUSHED3), 2000);
	}
}

/*
-------------------------
ST_HoldPosition
-------------------------
*/

static void Tusken_HoldPosition()
{
	NPC_FreeCombatPoint(NPCInfo->combatPoint, qtrue);
	NPCInfo->goalEntity = nullptr;
}

/*
-------------------------
ST_Move
-------------------------
*/

static qboolean Tusken_Move()
{
	NPCInfo->combatMove = qtrue; //always move straight toward our goal

	const qboolean moved = NPC_MoveToGoal(qtrue);

	//If our move failed, then reset
	if (moved == qfalse)
	{
		//couldn't get to enemy
		//just hang here
		Tusken_HoldPosition();
	}

	return moved;
}

/*
-------------------------
NPC_BSTusken_Patrol
-------------------------
*/

static void NPC_BSTusken_Patrol()
{
	//FIXME: pick up on bodies of dead buddies?
	if (NPCInfo->confusionTime < level.time)
	{
		//Look for any enemies
		if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			if (NPC_CheckPlayerTeamStealth())
			{
				//NPC_AngerSound();
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}

		if (!(NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
		{
			//Is there danger nearby
			const int alert_event = NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_SUSPICIOUS);
			if (NPC_CheckForDanger(alert_event))
			{
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
			//check for other alert events
			//There is an event to look at
			if (alert_event >= 0) //&& level.alertEvents[alert_event].ID != NPCInfo->lastAlertID )
			{
				//NPCInfo->lastAlertID = level.alertEvents[alert_event].ID;
				if (level.alertEvents[alert_event].level == AEL_DISCOVERED)
				{
					if (level.alertEvents[alert_event].owner &&
						level.alertEvents[alert_event].owner->client &&
						level.alertEvents[alert_event].owner->health >= 0 &&
						level.alertEvents[alert_event].owner->client->playerTeam == NPC->client->enemyTeam)
					{
						//an enemy
						G_SetEnemy(NPC, level.alertEvents[alert_event].owner);
						//NPCInfo->enemyLastSeenTime = level.time;
						TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
					}
				}
				else
				{
					//FIXME: get more suspicious over time?
					//Save the position for movement (if necessary)
					VectorCopy(level.alertEvents[alert_event].position, NPCInfo->investigateGoal);
					NPCInfo->investigateDebounceTime = level.time + Q_irand(500, 1000);
					if (level.alertEvents[alert_event].level == AEL_SUSPICIOUS)
					{
						//suspicious looks longer
						NPCInfo->investigateDebounceTime += Q_irand(500, 2500);
					}
				}
			}

			if (NPCInfo->investigateDebounceTime > level.time)
			{
				//FIXME: walk over to it, maybe?  Not if not chase enemies
				//NOTE: stops walking or doing anything else below
				vec3_t dir, angles;

				VectorSubtract(NPCInfo->investigateGoal, NPC->client->renderInfo.eyePoint, dir);
				vectoangles(dir, angles);

				const float o_yaw = NPCInfo->desiredYaw;
				const float o_pitch = NPCInfo->desiredPitch;
				NPCInfo->desiredYaw = angles[YAW];
				NPCInfo->desiredPitch = angles[PITCH];

				NPC_UpdateAngles(qtrue, qtrue);

				NPCInfo->desiredYaw = o_yaw;
				NPCInfo->desiredPitch = o_pitch;
				return;
			}
		}
	}

	//If we have somewhere to go, then do that
	if (UpdateGoal())
	{
		ucmd.buttons |= BUTTON_WALKING;
		NPC_MoveToGoal(qtrue);
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

void NPC_Tusken_Taunt()
{
	NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_TUSKENTAUNT1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	TIMER_Set(NPC, "taunting", NPC->client->ps.torsoAnimTimer);
	TIMER_Set(NPC, "duck", -1);
}

/*
-------------------------
NPC_BSTusken_Attack
-------------------------
*/

static void NPC_BSTusken_Attack()
{
	// IN PAIN
	//---------
	if (NPC->painDebounceTime > level.time)
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	// IN FLEE
	//---------
	if (TIMER_Done(NPC, "flee") && NPC_CheckForDanger(NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER)))
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	// UPDATE OUR ENEMY
	//------------------
	if (NPC_CheckEnemyExt() == qfalse || !NPC->enemy)
	{
		NPC_BSTusken_Patrol();
		return;
	}
	enemyDist = Distance(NPC->enemy->currentOrigin, NPC->currentOrigin);

	// Is The Current Enemy A Jawa?
	//------------------------------
	if (NPC->enemy->client && NPC->enemy->client->NPC_class == CLASS_JAWA)
	{
		// Make Sure His Enemy Is Me
		//---------------------------
		if (NPC->enemy->enemy != NPC)
		{
			G_SetEnemy(NPC->enemy, NPC);
		}

		// Should We Forget About Our Current Enemy And Go After The Player?
		//-------------------------------------------------------------------
		if (player && // If There Is A Player Pointer
			player != NPC->enemy && // The Player Is Not Currently My Enemy
			Distance(player->currentOrigin, NPC->currentOrigin) < 130.0f && // The Player Is Close Enough
			NAV::InSameRegion(NPC, player) // And In The Same Region
			)
		{
			G_SetEnemy(NPC, player);
		}
	}

	// Update Our Last Seen Time
	//---------------------------
	if (NPC_ClearLOS(NPC->enemy))
	{
		NPCInfo->enemyLastSeenTime = level.time;
	}

	// Check To See If We Are In Attack Range
	//----------------------------------------
	const float bounds_min = NPC->maxs[0] + NPC->enemy->maxs[0];
	const float lunge_range = bounds_min + 65.0f;
	const float strike_range = bounds_min + 40.0f;
	const bool melee_range = enemyDist < lunge_range;
	const bool melee_weapon = NPC->client->ps.weapon != WP_TUSKEN_RIFLE;
	const bool can_see_enemy = level.time - NPCInfo->enemyLastSeenTime < 3000;

	// Check To Start Taunting
	//-------------------------
	if (can_see_enemy && !melee_range && TIMER_Done(NPC, "tuskenTauntCheck"))
	{
		TIMER_Set(NPC, "tuskenTauntCheck", Q_irand(2000, 6000));
		if (!Q_irand(0, 3))
		{
			NPC_Tusken_Taunt();
		}
	}

	if (TIMER_Done(NPC, "taunting"))
	{
		// Should I Attack?
		//------------------
		if (melee_range || !melee_weapon && can_see_enemy)
		{
			if (!(NPCInfo->scriptFlags & SCF_FIRE_WEAPON) && // If This Flag Is On, It Calls Attack From Elsewhere
				!(NPCInfo->scriptFlags & SCF_DONT_FIRE) && // If This Flag Is On, Don't Fire At All
				TIMER_Done(NPC, "attackDelay")
				)
			{
				ucmd.buttons &= ~BUTTON_ALT_ATTACK;

				// If Not In Strike Range, Do Lunge, Or If We Don't Have The Staff, Just Shoot Normally
				//--------------------------------------------------------------------------------------
				if (enemyDist > strike_range)
				{
					ucmd.buttons |= BUTTON_ALT_ATTACK;
				}

				WeaponThink();
				TIMER_Set(NPC, "attackDelay", NPCInfo->shotTime - level.time);
			}

			if (!TIMER_Done(NPC, "duck"))
			{
				ucmd.upmove = -127;
			}
		}

		// Or Should I Move?
		//-------------------
		else if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			NPCInfo->goalEntity = NPC->enemy;
			NPCInfo->goalRadius = lunge_range;
			Tusken_Move();
		}
	}

	// UPDATE ANGLES
	//---------------
	if (can_see_enemy)
	{
		NPC_FaceEnemy(qtrue);
	}
	NPC_UpdateAngles(qtrue, qtrue);
}

extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength,
	qboolean break_saber_lock);

static void Tusken_StaffTrace()
{
	if (!NPC->ghoul2.size()
		|| NPC->weaponModel[0] <= 0)
	{
		return;
	}

	const int bolt_index = gi.G2API_AddBolt(&NPC->ghoul2[NPC->weaponModel[0]], "*weapon");
	if (bolt_index != -1)
	{
		const int curTime = cg.time ? cg.time : level.time;
		qboolean hit = qfalse;
		int last_hit = ENTITYNUM_NONE;
		for (int time = curTime - 25; time <= curTime + 25 && !hit; time += 25)
		{
			mdxaBone_t bolt_matrix;
			vec3_t tip, dir, base;
			const vec3_t angles = { 0, NPC->currentAngles[YAW], 0 };
			constexpr vec3_t mins = { -2, -2, -2 }, maxs = { 2, 2, 2 };
			trace_t trace;

			gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->weaponModel[0],
				bolt_index,
				&bolt_matrix, angles, NPC->currentOrigin, time,
				nullptr, NPC->s.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, base);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, dir);
			VectorMA(base, -20, dir, base);
			VectorMA(base, 78, dir, tip);
#ifndef FINAL_BUILD
			if (d_saberCombat->integer > 1)
			{
				G_DebugLine(base, tip, 1000, 0x000000ff, qtrue);
			}
#endif
			gi.trace(&trace, base, mins, maxs, tip, NPC->s.number, MASK_SHOT, G2_RETURNONHIT, 10);
			if (trace.fraction < 1.0f && trace.entityNum != last_hit)
			{
				//hit something
				gentity_t* traceEnt = &g_entities[trace.entityNum];
				if (traceEnt->takedamage
					&& (!traceEnt->client || traceEnt == NPC->enemy || traceEnt->client->NPC_class != NPC->client->
						NPC_class))
				{
					//smack
					const int dmg = Q_irand(5, 10) * (g_spskill->integer + 1);

					//FIXME: debounce?
					G_Sound(traceEnt, G_SoundIndex(va("sound/weapons/tusken_staff/stickhit%d.wav", Q_irand(1, 4))));
					G_Damage(traceEnt, NPC, NPC, vec3_origin, trace.endpos, dmg, DAMAGE_NO_KNOCKBACK, MOD_MELEE);
					if (traceEnt->health > 0
						&& (traceEnt->client && traceEnt->client->NPC_class == CLASS_JAWA && !Q_irand(0, 1)
							|| dmg > 19)) //FIXME: base on skill!
					{
						//do pain on enemy
						G_Knockdown(traceEnt, NPC, dir, 300, qtrue);
					}
					last_hit = trace.entityNum;
					hit = qtrue;
				}
			}
		}
	}
}

void Tusken_StaffTracenew(gentity_t* self)
{
	if (!self->ghoul2.size()
		|| self->weaponModel[0] <= 0)
	{
		return;
	}

	const int bolt_index = gi.G2API_AddBolt(&self->ghoul2[self->weaponModel[0]], "*weapon");
	if (bolt_index != -1)
	{
		const int curTime = cg.time ? cg.time : level.time;
		qboolean hit = qfalse;
		int last_hit = ENTITYNUM_NONE;
		for (int time = curTime - 25; time <= curTime + 25 && !hit; time += 25)
		{
			mdxaBone_t bolt_matrix;
			vec3_t tip, dir, base;
			const vec3_t angles = { 0, self->currentAngles[YAW], 0 };
			constexpr vec3_t mins = { -2, -2, -2 }, maxs = { 2, 2, 2 };
			trace_t trace;

			gi.G2API_GetBoltMatrix(self->ghoul2, self->weaponModel[0],
				bolt_index,
				&bolt_matrix, angles, self->currentOrigin, time,
				nullptr, self->s.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, base);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, dir);
			VectorMA(base, -20, dir, base);
			VectorMA(base, 78, dir, tip);
#ifndef FINAL_BUILD
			if (d_saberCombat->integer > 1)
			{
				G_DebugLine(base, tip, 1000, 0x000000ff, qtrue);
			}
#endif
			gi.trace(&trace, base, mins, maxs, tip, self->s.number, MASK_SHOT, G2_RETURNONHIT, 10);
			if (trace.fraction < 1.0f && trace.entityNum != last_hit)
			{
				//hit something
				gentity_t* traceEnt = &g_entities[trace.entityNum];
				if (traceEnt->takedamage
					&& (!traceEnt->client || traceEnt == self->enemy || traceEnt->client->NPC_class != self->client->
						NPC_class))
				{
					//smack
					const int dmg = Q_irand(5, 10) * (g_spskill->integer + 1);

					//FIXME: debounce?
					G_Sound(traceEnt, G_SoundIndex(va("sound/weapons/tusken_staff/stickhit%d.wav", Q_irand(1, 4))));
					G_Damage(traceEnt, self, self, vec3_origin, trace.endpos, dmg, DAMAGE_NO_KNOCKBACK, MOD_MELEE);
					if (traceEnt->health > 0
						&& (traceEnt->client && traceEnt->client->NPC_class == CLASS_JAWA && !Q_irand(0, 1)
							|| dmg > 19)) //FIXME: base on skill!
					{
						//do pain on enemy
						G_Knockdown(traceEnt, self, dir, 300, qtrue);
					}
					last_hit = trace.entityNum;
					hit = qtrue;
				}
			}
		}
	}
}

qboolean G_TuskenAttackAnimDamage(gentity_t* self)
{
	if (self->client->ps.torsoAnim == BOTH_TUSKENATTACK1 ||
		self->client->ps.torsoAnim == BOTH_TUSKENATTACK2 ||
		self->client->ps.torsoAnim == BOTH_TUSKENATTACK3 ||
		self->client->ps.torsoAnim == BOTH_TUSKENLUNGE1)
	{
		float current = 0.0f;
		int end = 0;
		int start = 0;
		if (!!gi.G2API_GetBoneAnimIndex(&
			self->ghoul2[self->playerModel],
			self->lowerLumbarBone,
			level.time,
			&current,
			&start,
			&end,
			nullptr,
			nullptr,
			nullptr))
		{
			const float percent_complete = (current - start) / (end - start);
			//gi.Printf("%f\n", percentComplete);
			switch (self->client->ps.torsoAnim)
			{
			case BOTH_TUSKENATTACK1: return static_cast<qboolean>(percent_complete > 0.3 && percent_complete < 0.7);
			case BOTH_TUSKENATTACK2: return static_cast<qboolean>(percent_complete > 0.3 && percent_complete < 0.7);
			case BOTH_TUSKENATTACK3: return static_cast<qboolean>(percent_complete > 0.1 && percent_complete < 0.5);
			case BOTH_TUSKENLUNGE1: return static_cast<qboolean>(percent_complete > 0.3 && percent_complete < 0.5);
			default:;
			}
		}
	}
	return qfalse;
}

void NPC_BSTusken_Default()
{
	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}

	if (G_TuskenAttackAnimDamage(NPC))
	{
		//Tusken_StaffTrace();
		Tusken_StaffTracenew(NPC);
	}

	if (!NPC->enemy)
	{
		//don't have an enemy, look for one
		NPC_BSTusken_Patrol();
	}
	else //if ( NPC->enemy )
	{
		//have an enemy
		NPC_BSTusken_Attack();

		npc_check_speak(NPC);
	}
}