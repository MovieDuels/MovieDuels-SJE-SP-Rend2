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
#include "g_navigator.h"
#include "g_functions.h"
#include "g_public.h"
#include "g_shared.h"
#include "ghoul2_shared.h"
#include <qcommon\q_math.h>
#include "bg_public.h"
#include <qcommon\q_platform.h>
#include "ai.h"
#include "bstate.h"
#include "g_local.h"
#include "b_public.h"
#include "weapons.h"
#include <qcommon\q_shared.h>

extern void CG_DrawAlert(vec3_t origin, float rating);
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern void NPC_TempLookTarget(const gentity_t* self, int lookEntNum, int minLookTime, int maxLookTime);
extern qboolean G_ExpandPointToBBox(vec3_t point, const vec3_t mins, const vec3_t maxs, int ignore, int clipmask);
extern void NPC_AimAdjust(int change);
extern qboolean FlyingCreature(const gentity_t* ent);

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
extern void npc_check_speak(gentity_t* speaker_npc);

qboolean NPC_CheckPlayerTeamStealth();

static qboolean enemy_los;
static qboolean enemy_cs;
static qboolean face_enemy;
static qboolean do_move;
static qboolean shoot;
static float enemyDist;

//Local state enums
enum
{
	LSTATE_NONE = 0,
	LSTATE_UNDERFIRE,
	LSTATE_INVESTIGATE,
};

static void Grenadier_ClearTimers(const gentity_t* ent)
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
}

static void NPC_Grenadier_PlayConfusionSound(gentity_t* self)
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

	//self->NPC->behaviorState = BS_PATROL;
	G_ClearEnemy(self); //FIXME: or just self->enemy = NULL;?

	self->NPC->investigateCount = 0;
}

/*
-------------------------
NPC_ST_Pain
-------------------------
*/

static void NPC_Grenadier_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* other, vec3_t point, const int damage,
	const int mod)
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

static void Grenadier_HoldPosition()
{
	NPC_FreeCombatPoint(NPCInfo->combatPoint, qtrue);
	NPCInfo->goalEntity = nullptr;
}

/*
-------------------------
ST_Move
-------------------------
*/

static qboolean Grenadier_Move()
{
	NPCInfo->combatMove = qtrue; //always doMove straight toward our goal

	const qboolean moved = NPC_MoveToGoal(qtrue);
	navInfo_t info;

	//Get the doMove info
	NAV_GetLastMove(info);

	//FIXME: if we bump into another one of our guys and can't get around him, just stop!
	//If we hit our target, then stop and fire!
	if (info.flags & NIF_COLLISION)
	{
		if (info.blocker == NPC->enemy)
		{
			Grenadier_HoldPosition();
		}
	}

	//If our doMove failed, then reset
	if (moved == qfalse)
	{
		//couldn't get to enemy
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES && NPC->client->ps.weapon == WP_THERMAL && NPCInfo->goalEntity &&
			NPCInfo->goalEntity == NPC->enemy)
		{
			//we were running after enemy
			//Try to find a combat point that can hit the enemy
			int cp_flags = CP_CLEAR | CP_HAS_ROUTE;
			if (NPCInfo->scriptFlags & SCF_USE_CP_NEAREST)
			{
				cp_flags &= ~(CP_FLANK | CP_APPROACH_ENEMY | CP_CLOSEST);
				cp_flags |= CP_NEAREST;
			}
			int cp = NPC_FindCombatPoint(NPC->currentOrigin, NPC->currentOrigin, NPC->currentOrigin, cp_flags, 32);
			if (cp == -1 && !(NPCInfo->scriptFlags & SCF_USE_CP_NEAREST))
			{
				//okay, try one by the enemy
				cp = NPC_FindCombatPoint(NPC->currentOrigin, NPC->currentOrigin, NPC->enemy->currentOrigin,
					CP_CLEAR | CP_HAS_ROUTE | CP_HORZ_DIST_COLL, 32);
			}
			//NOTE: there may be a perfectly valid one, just not one within CP_COLLECT_RADIUS of either me or him...
			if (cp != -1)
			{
				//found a combat point that has a clear shot to enemy
				NPC_SetCombatPoint(cp);
				NPC_SetMoveGoal(NPC, level.combatPoints[cp].origin, 8, qtrue, cp);
				return moved;
			}
		}
		//just hang here
		Grenadier_HoldPosition();
	}

	return moved;
}

/*
-------------------------
NPC_BSGrenadier_Patrol
-------------------------
*/

static void NPC_BSGrenadier_Patrol()
{
	//FIXME: pick up on bodies of dead buddies?
	if (NPCInfo->confusionTime < level.time)
	{
		//Look for any enemies
		if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			if (NPC_CheckPlayerTeamStealth())
			{
				//NPCInfo->behaviorState = BS_HUNT_AND_KILL;//should be automatic now
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

/*
-------------------------
NPC_BSGrenadier_Idle
-------------------------
*/
/*
void NPC_BSGrenadier_Idle( void )
{
	//FIXME: check for other alert events?

	//Is there danger nearby?
	if ( NPC_CheckForDanger( NPC_CheckAlertEvents( qtrue, qtrue, -1, qfalse, AEL_DANGER ) ) )
	{
		NPC_UpdateAngles( qtrue, qtrue );
		return;
	}

	TIMER_Set( NPC, "roamTime", 2000 + Q_irand( 1000, 2000 ) );

	NPC_UpdateAngles( qtrue, qtrue );
}
*/
/*
-------------------------
ST_CheckMoveState
-------------------------
*/

static void Grenadier_CheckMoveState()
{
	//See if we're a scout
	if (!(NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)) //behaviorState == BS_STAND_AND_SHOOT )
	{
		if (NPCInfo->goalEntity == NPC->enemy)
		{
			do_move = qfalse;
			return;
		}
	}
	//See if we're running away
	else if (NPCInfo->squadState == SQUAD_RETREAT)
	{
		if (TIMER_Done(NPC, "flee"))
		{
			NPCInfo->squadState = SQUAD_IDLE;
		}
		else
		{
			face_enemy = qfalse;
		}
	}
	/*
	else if ( NPCInfo->squadState == SQUAD_IDLE )
	{
		if ( !NPCInfo->goalEntity )
		{
			doMove = qfalse;
			return;
		}
		//Should keep moving toward player when we're out of range... right?
	}
	*/

	//See if we're moving towards a goal, not the enemy
	if (NPCInfo->goalEntity != NPC->enemy && NPCInfo->goalEntity != nullptr)
	{
		//Did we make it?
		if (STEER::Reached(NPC, NPCInfo->goalEntity, 16, !!FlyingCreature(NPC)) ||
			NPCInfo->squadState == SQUAD_SCOUT && enemy_los && enemyDist <= 10000)
		{
			//int	newSquadState = SQUAD_STAND_AND_SHOOT;
			//we got where we wanted to go, set timers based on why we were running
			switch (NPCInfo->squadState)
			{
			case SQUAD_RETREAT: //was running away
				TIMER_Set(NPC, "duck", (NPC->max_health - NPC->health) * 100);
				TIMER_Set(NPC, "hideTime", Q_irand(3000, 7000));
				//newSquadState = SQUAD_COVER;
				break;
			case SQUAD_TRANSITION: //was heading for a combat point
				TIMER_Set(NPC, "hideTime", Q_irand(2000, 4000));
				break;
			case SQUAD_SCOUT: //was running after player
				break;
			default:
				break;
			}
			NPC_ReachedGoal();
			//don't attack right away
			TIMER_Set(NPC, "attackDelay", Q_irand(250, 500)); //FIXME: Slant for difficulty levels
			//don't do something else just yet
			TIMER_Set(NPC, "roamTime", Q_irand(1000, 4000));
			//stop fleeing
			if (NPCInfo->squadState == SQUAD_RETREAT)
			{
				TIMER_Set(NPC, "flee", -level.time);
				NPCInfo->squadState = SQUAD_IDLE;
			}
			return;
		}

		//keep going, hold of roamTimer until we get there
		TIMER_Set(NPC, "roamTime", Q_irand(4000, 8000));
	}

	if (!NPCInfo->goalEntity)
	{
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			NPCInfo->goalEntity = NPC->enemy;
			NPCInfo->goalRadius = NPC->maxs[0] * 1.5f;
		}
	}
}

/*
-------------------------
ST_CheckFireState
-------------------------
*/

static void Grenadier_CheckFireState()
{
	if (enemy_cs)
	{
		//if have a clear shot, always try
		return;
	}

	if (NPCInfo->squadState == SQUAD_RETREAT || NPCInfo->squadState == SQUAD_TRANSITION || NPCInfo->squadState ==
		SQUAD_SCOUT)
	{
		//runners never try to fire at the last pos
		return;
	}

	if (!VectorCompare(NPC->client->ps.velocity, vec3_origin))
	{
		//if moving at all, don't do this
	}
}

static qboolean Grenadier_EvaluateShot(const int hit)
{
	if (!NPC || !NPC->enemy)
	{
		return qfalse;
	}

	// Validate hit index (SP uses MAX_GENTITIES, not level.num_entities)
	if (hit >= 0 && hit < MAX_GENTITIES)
	{
		// Can hit enemy directly?
		if (hit == NPC->enemy->s.number)
		{
			return qtrue;
		}

		// Can hit glass?
		if (g_entities[hit].svFlags & SVF_GLASS_BRUSH)
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
-------------------------
NPC_BSGrenadier_Attack
-------------------------
*/

static void NPC_BSGrenadier_Attack()
{
	//Don't do anything if we're hurt
	if (NPC->painDebounceTime > level.time)
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	//NPC_CheckEnemy( qtrue, qfalse );
	//If we don't have an enemy, just idle
	if (NPC_CheckEnemyExt() == qfalse) //!NPC->enemy )//
	{
		NPC_BSGrenadier_Patrol(); //FIXME: or patrol?
		return;
	}

	if (TIMER_Done(NPC, "flee") && NPC_CheckForDanger(NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER)))
	{
		//going to run
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	if (!NPC->enemy)
	{
		//WTF?  somehow we lost our enemy?
		NPC_BSGrenadier_Patrol(); //FIXME: or patrol?
		return;
	}

	enemy_los = enemy_cs = qfalse;
	do_move = qtrue;
	face_enemy = qfalse;
	shoot = qfalse;
	enemyDist = DistanceSquared(NPC->enemy->currentOrigin, NPC->currentOrigin);

	//See if we should switch to melee attack
	if (enemyDist < 16384 && (!NPC->enemy->client || NPC->enemy->client->ps.weapon != WP_SABER || !NPC->enemy->client->
		ps.SaberActive())) //128
	{
		//enemy is close and not using saber
		if (NPC->client->ps.weapon == WP_THERMAL)
		{
			//grenadier
			trace_t trace;
			gi.trace(&trace, NPC->currentOrigin, NPC->enemy->mins, NPC->enemy->maxs, NPC->enemy->currentOrigin,
				NPC->s.number, NPC->enemy->clipmask, static_cast<EG2_Collision>(0), 0);
			if (!trace.allsolid && !trace.startsolid && (trace.fraction == 1.0 || trace.entityNum == NPC->enemy->s.
				number))
			{
				//I can get right to him
				//reset fire-timing variables
				NPC_ChangeWeapon(WP_MELEE);
				if (!(NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)) //NPCInfo->behaviorState == BS_STAND_AND_SHOOT )
				{
					//FIXME: should we be overriding scriptFlags?
					NPCInfo->scriptFlags |= SCF_CHASE_ENEMIES; //NPCInfo->behaviorState = BS_HUNT_AND_KILL;
				}
			}
		}
	}
	else if (enemyDist > 65536 || NPC->enemy->client && NPC->enemy->client->ps.weapon == WP_SABER && NPC->enemy->client
		->ps.SaberActive()) //256
	{
		//enemy is far or using saber
		if (NPC->client->ps.weapon == WP_MELEE && NPC->client->ps.weapons[WP_THERMAL])
		{
			//fisticuffs, make switch to thermal if have it
			//reset fire-timing variables
			NPC_ChangeWeapon(WP_THERMAL);
		}
	}

	//can we see our target?
	if (NPC_ClearLOS(NPC->enemy))
	{
		NPCInfo->enemyLastSeenTime = level.time;
		enemy_los = qtrue;

		if (NPC->client->ps.weapon == WP_MELEE)
		{
			if (enemyDist <= 4096 && InFOV(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles,
				90, 45)) //within 64 & infront
			{
				VectorCopy(NPC->enemy->currentOrigin, NPCInfo->enemyLastSeenLocation);
				enemy_cs = qtrue;
			}
		}
		else if (InFOV(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles, 45, 90))
		{
			//in front of me
			//can we shoot our target?
			//FIXME: how accurate/necessary is this check?
			const int hit = NPC_ShotEntity(NPC->enemy);
			const gentity_t* hit_ent = &g_entities[hit];
			if (hit == NPC->enemy->s.number
				|| hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPC->client->enemyTeam)
			{
				VectorCopy(NPC->enemy->currentOrigin, NPCInfo->enemyLastSeenLocation);
				const float enemy_horz_dist = DistanceHorizontalSquared(NPC->enemy->currentOrigin, NPC->currentOrigin);
				if (enemy_horz_dist < 1048576)
				{
					//within 1024
					enemy_cs = qtrue;
					NPC_AimAdjust(2); //adjust aim better longer we have clear shot at enemy
				}
				else
				{
					NPC_AimAdjust(2); //adjust aim better longer we can see enemy
				}
			}
		}
	}
	else
	{
		NPC_AimAdjust(-1); //adjust aim worse longer we cannot see enemy
	}
	/*
	else if ( gi.inPVS( NPC->enemy->currentOrigin, NPC->currentOrigin ) )
	{
		NPCInfo->enemyLastSeenTime = level.time;
		faceEnemy = qtrue;
	}
	*/

	if (enemy_los)
	{
		//FIXME: no need to face enemy if we're moving to some other goal and he's too far away to shoot?
		face_enemy = qtrue;
	}

	if (enemy_cs)
	{
		shoot = qtrue;
		if (NPC->client->ps.weapon == WP_THERMAL)
		{
			//don't chase and throw
			do_move = qfalse;
		}
		else if (NPC->client->ps.weapon == WP_MELEE && enemyDist < (NPC->maxs[0] + NPC->enemy->maxs[0] + 16) * (NPC->
			maxs[0] + NPC->enemy->maxs[0] + 16))
		{
			//close enough
			do_move = qfalse;
		}
	} //this should make him chase enemy when out of range...?

	//Check for movement to take care of
	Grenadier_CheckMoveState();

	//See if we should override shooting decision with any special considerations
	Grenadier_CheckFireState();

	if (do_move)
	{
		//doMove toward goal
		if (NPCInfo->goalEntity) //&& ( NPCInfo->goalEntity != NPC->enemy || enemyDist > 10000 ) )//100 squared
		{
			do_move = Grenadier_Move();
		}
		else
		{
			do_move = qfalse;
		}
	}

	if (!do_move)
	{
		if (!TIMER_Done(NPC, "duck"))
		{
			ucmd.upmove = -127;
		}
		//FIXME: what about leaning?
	}
	else
	{
		//stop ducking!
		TIMER_Set(NPC, "duck", -1);
	}

	if (!face_enemy)
	{
		//we want to face in the dir we're running
		if (do_move)
		{
			//don't run away and shoot
			NPCInfo->desiredYaw = NPCInfo->lastPathAngles[YAW];
			NPCInfo->desiredPitch = 0;
			shoot = qfalse;
		}
		NPC_UpdateAngles(qtrue, qtrue);
	}
	else // if ( faceEnemy )
	{
		//face the enemy
		NPC_FaceEnemy();
	}

	if (NPCInfo->scriptFlags & SCF_DONT_FIRE)
	{
		shoot = qfalse;
	}

	//FIXME: don't shoot right away!
	if (shoot)
	{
		//try to shoot if it's time
		if (TIMER_Done(NPC, "attackDelay"))
		{
			if (!(NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
			{
				WeaponThink();
				TIMER_Set(NPC, "attackDelay", NPCInfo->shotTime - level.time);
			}
		}
	}
}

void NPC_BSGrenadier_Default()
{
	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}

	if (!NPC->enemy)
	{
		//don't have an enemy, look for one
		NPC_BSGrenadier_Patrol();
	}
	else //if ( NPC->enemy )
	{
		//have an enemy
		NPC_BSGrenadier_Attack();

		npc_check_speak(NPC);
	}
}