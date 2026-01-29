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
#include "wp_saber.h"

extern void WP_DeactivateSaber(const gentity_t* self, qboolean clear_length = qfalse);
extern int PM_AnimLength(const int index, const animNumber_t anim);

qboolean NPC_CheckPlayerTeamStealth();

static qboolean enemy_los;
static qboolean enemy_cs;
static qboolean face_enemy;
static qboolean do_move;
static qboolean shoot;
static float enemyDist;

static void SaberDroid_HoldPosition()
{
	NPC_FreeCombatPoint(NPCInfo->combatPoint, qtrue);
	NPCInfo->goalEntity = nullptr;
}

/*
-------------------------
ST_Move
-------------------------
*/

static qboolean SaberDroid_Move()
{
	NPCInfo->combatMove = qtrue; //always doMove straight toward our goal
	UpdateGoal();
	if (!NPCInfo->goalEntity)
	{
		NPCInfo->goalEntity = NPC->enemy;
	}
	NPCInfo->goalRadius = 30.0f;

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
			SaberDroid_HoldPosition();
		}
	}

	//If our doMove failed, then reset
	if (moved == qfalse)
	{
		//couldn't get to enemy
		//just hang here
		SaberDroid_HoldPosition();
	}

	return moved;
}

/*
-------------------------
NPC_BSSaberDroid_Patrol
-------------------------
*/

static void NPC_BSSaberDroid_Patrol()
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
			//There is an event to look at
			if (alert_event >= 0) //&& level.alertEvents[alert_event].ID != NPCInfo->lastAlertID )
			{
				//NPCInfo->lastAlertID = level.alertEvents[alert_event].ID;
				if (level.alertEvents[alert_event].level >= AEL_DISCOVERED)
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
	else if (!NPC->client->ps.weaponTime
		&& TIMER_Done(NPC, "attackDelay")
		&& TIMER_Done(NPC, "inactiveDelay"))
	{
		if (NPC->client->ps.SaberActive())
		{
			WP_DeactivateSaber(NPC, qfalse);
			NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_TURNOFF, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

int SaberDroid_PowerLevelForSaberAnim(const gentity_t* self)
{
	switch (self->client->ps.legsAnim)
	{
	case BOTH_A2_TR_BL:
	{
		if (self->client->ps.torsoAnimTimer <= 200)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (PM_AnimLength(self->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(self->client->ps.legsAnim)) - self->client->ps.torsoAnimTimer <
			200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_2;
	case BOTH_A1_BL_TR:
	{
		if (self->client->ps.torsoAnimTimer <= 300)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (PM_AnimLength(self->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(self->client->ps.legsAnim)) - self->client->ps.torsoAnimTimer <
			200)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_1;
	case BOTH_A1__L__R:
	{
		if (self->client->ps.torsoAnimTimer <= 250)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (PM_AnimLength(self->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(self->client->ps.legsAnim)) - self->client->ps.torsoAnimTimer <
			150)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_1;
	case BOTH_A3__L__R:
	{
		if (self->client->ps.torsoAnimTimer <= 200)
		{
			//end of anim
			return FORCE_LEVEL_0;
		}
		if (PM_AnimLength(self->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(self->client->ps.legsAnim)) - self->client->ps.torsoAnimTimer <
			300)
		{
			//beginning of anim
			return FORCE_LEVEL_0;
		}
	}
	return FORCE_LEVEL_3;
	default:;
	}
	return FORCE_LEVEL_0;
}

/*
-------------------------
NPC_BSSaberDroid_Attack
-------------------------
*/

static void NPC_SaberDroid_PickAttack()
{
	int attack_anim = Q_irand(0, 3);
	switch (attack_anim)
	{
	case 0:
	default:
		attack_anim = BOTH_A2_TR_BL;
		NPC->client->ps.saber_move = LS_A_TR2BL;
		NPC->client->ps.saber_anim_level = SS_MEDIUM;
		break;
	case 1:
		attack_anim = BOTH_A1_BL_TR;
		NPC->client->ps.saber_move = LS_A_BL2TR;
		NPC->client->ps.saber_anim_level = SS_FAST;
		break;
	case 2:
		attack_anim = BOTH_A1__L__R;
		NPC->client->ps.saber_move = LS_A_L2R;
		NPC->client->ps.saber_anim_level = SS_FAST;
		break;
	case 3:
		attack_anim = BOTH_A3__L__R;
		NPC->client->ps.saber_move = LS_A_L2R;
		NPC->client->ps.saber_anim_level = SS_STRONG;
		break;
	}
	NPC->client->ps.saberBlocking = saber_moveData[NPC->client->ps.saber_move].blocking;
	if (saber_moveData[NPC->client->ps.saber_move].trailLength > 0)
	{
		NPC->client->ps.SaberActivateTrail(saber_moveData[NPC->client->ps.saber_move].trailLength);
		// saber trail lasts for 75ms...feel free to change this if you want it longer or shorter
	}
	else
	{
		NPC->client->ps.SaberDeactivateTrail(0);
	}

	NPC_SetAnim(NPC, SETANIM_BOTH, attack_anim, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
	NPC->client->ps.torsoAnim = NPC->client->ps.legsAnim;
	//need to do this because we have no anim split but saber code checks torsoAnim
	NPC->client->ps.weaponTime = NPC->client->ps.torsoAnimTimer = NPC->client->ps.legsAnimTimer;
	NPC->client->ps.weaponstate = WEAPON_FIRING;
}

static void NPC_BSSaberDroid_Attack()
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
		NPC->enemy = nullptr;
		NPC_BSSaberDroid_Patrol(); //FIXME: or patrol?
		return;
	}

	if (!NPC->enemy)
	{
		//WTF?  somehow we lost our enemy?
		NPC_BSSaberDroid_Patrol(); //FIXME: or patrol?
		return;
	}

	enemy_los = enemy_cs = qfalse;
	do_move = qtrue;
	face_enemy = qfalse;
	shoot = qfalse;
	enemyDist = DistanceSquared(NPC->enemy->currentOrigin, NPC->currentOrigin);

	//can we see our target?
	if (NPC_ClearLOS(NPC->enemy))
	{
		NPCInfo->enemyLastSeenTime = level.time;
		enemy_los = qtrue;

		if (enemyDist <= 4096 && InFOV(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles, 90,
			45)) //within 64 & infront
		{
			VectorCopy(NPC->enemy->currentOrigin, NPCInfo->enemyLastSeenLocation);
			enemy_cs = qtrue;
		}
	}

	if (enemy_los)
	{
		//FIXME: no need to face enemy if we're moving to some other goal and he's too far away to shoot?
		face_enemy = qtrue;
	}

	if (!TIMER_Done(NPC, "taunting"))
	{
		do_move = qfalse;
	}
	else if (enemy_cs)
	{
		shoot = qtrue;
		if (enemyDist < (NPC->maxs[0] + NPC->enemy->maxs[0] + 32) * (NPC->maxs[0] + NPC->enemy->maxs[0] + 32))
		{
			//close enough
			do_move = qfalse;
		}
	} //this should make him chase enemy when out of range...?

	if (NPC->client->ps.legsAnimTimer
		&& NPC->client->ps.legsAnim != BOTH_A3__L__R) //this one is a running attack
	{
		//in the middle of a held, stationary anim, can't doMove
		do_move = qfalse;
	}

	if (do_move)
	{
		//doMove toward goal
		do_move = SaberDroid_Move();
		if (do_move)
		{
			//if we had to chase him, be sure to attack as soon as possible
			TIMER_Set(NPC, "attackDelay", NPC->client->ps.weaponTime);
		}
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

	//FIXME: need predicted blocking?
	//FIXME: don't shoot right away!
	if (shoot)
	{
		//try to shoot if it's time
		if (TIMER_Done(NPC, "attackDelay"))
		{
			if (!(NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
			{
				NPC_SaberDroid_PickAttack();
				if (NPCInfo->rank > RANK_CREWMAN)
				{
					TIMER_Set(NPC, "attackDelay", NPC->client->ps.weaponTime + Q_irand(0, 1000));
				}
				else
				{
					TIMER_Set(NPC, "attackDelay",
						NPC->client->ps.weaponTime + Q_irand(0, 1000) + Q_irand(0, (3 - g_spskill->integer) * 2)
						* 500);
				}
			}
		}
	}
}

void NPC_BSSD_Default()
{
	if (!NPC->enemy)
	{
		//don't have an enemy, look for one
		NPC_BSSaberDroid_Patrol();
	}
	else //if ( NPC->enemy )
	{
		//have an enemy
		if (!NPC->client->ps.SaberActive())
		{
			NPC->client->ps.SaberActivate();
			if (NPC->client->ps.legsAnim == BOTH_TURNOFF
				|| NPC->client->ps.legsAnim == BOTH_STAND1)
			{
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_TURNON, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}

		NPC_BSSaberDroid_Attack();
		TIMER_Set(NPC, "inactiveDelay", Q_irand(2000, 4000));
	}
	if (!NPC->client->ps.weaponTime)
	{
		NPC->client->ps.saber_move = LS_READY;
		NPC->client->ps.saberBlocking = saber_moveData[LS_READY].blocking;
		NPC->client->ps.SaberDeactivateTrail(0);
		NPC->client->ps.saber_anim_level = SS_MEDIUM;
		NPC->client->ps.weaponstate = WEAPON_READY;
	}
}