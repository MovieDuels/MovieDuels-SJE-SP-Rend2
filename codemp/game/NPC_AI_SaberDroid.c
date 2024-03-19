//this entire file is a port of the SP version of this code.
// leave this line at the top of all AI_xxxx.cpp files for PCH reasons...
#include "b_local.h"
#include "g_nav.h"
#include "anims.h"

extern void WP_DeactivateSaber(const gentity_t* self, qboolean clear_length);
extern int PM_AnimLength(animNumber_t anim);

qboolean NPC_CheckPlayerTeamStealth(void);

static qboolean enemyLOS;
static qboolean enemyCS;
static qboolean faceEnemy;
static qboolean shoot;
static float enemyDist;

/*
-------------------------
ST_Move
-------------------------
*/

static qboolean SaberDroid_Move(void)
{
	//movement while in attack move.
	NPCS.NPCInfo->combatMove = qtrue; //always move straight toward our goal
	UpdateGoal();
	if (!NPCS.NPCInfo->goalEntity)
	{
		NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
	}
	NPCS.NPCInfo->goalRadius = 30.0f;

	const qboolean moved = NPC_MoveToGoal(qtrue);

	return moved;
}

/*
-------------------------
NPC_BSSaberDroid_Patrol
-------------------------
*/

void NPC_BSSaberDroid_Patrol(void)
{
	//FIXME: pick up on bodies of dead buddies?
	if (NPCS.NPCInfo->confusionTime < level.time)
	{
		//not confused by mindtrick
		//Look for any enemies
		if (NPCS.NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			if (NPC_CheckPlayerTeamStealth())
			{
				//found an enemy
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}

		if (!(NPCS.NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
		{
			//alert reaction behavior.
			const int alert_event = NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_SUSPICIOUS);

			if (alert_event >= 0)
			{
				if (level.alertEvents[alert_event].level >= AEL_DISCOVERED)
				{
					if (level.alertEvents[alert_event].owner &&
						level.alertEvents[alert_event].owner->client &&
						level.alertEvents[alert_event].owner->health >= 0 &&
						level.alertEvents[alert_event].owner->client->playerTeam == NPCS.NPC->client->enemyTeam)
					{
						//an enemy
						G_SetEnemy(NPCS.NPC, level.alertEvents[alert_event].owner);
						TIMER_Set(NPCS.NPC, "attackDelay", Q_irand(500, 2500));
					}
				}
				else
				{
					//FIXME: get more suspicious over time?
					VectorCopy(level.alertEvents[alert_event].position, NPCS.NPCInfo->investigateGoal);
					NPCS.NPCInfo->investigateDebounceTime = level.time + Q_irand(500, 1000);
					if (level.alertEvents[alert_event].level == AEL_SUSPICIOUS)
					{
						//suspicious looks longer
						NPCS.NPCInfo->investigateDebounceTime += Q_irand(500, 2500);
					}
				}
			}

			if (NPCS.NPCInfo->investigateDebounceTime > level.time)
			{
				//FIXME: walk over to it, maybe?  Not if not chase enemies
				//NOTE: stops walking or doing anything else below
				vec3_t dir, angles;

				VectorSubtract(NPCS.NPCInfo->investigateGoal, NPCS.NPC->client->renderInfo.eyePoint, dir);
				vectoangles(dir, angles);

				const float o_yaw = NPCS.NPCInfo->desiredYaw;
				const float o_pitch = NPCS.NPCInfo->desiredPitch;
				NPCS.NPCInfo->desiredYaw = angles[YAW];
				NPCS.NPCInfo->desiredPitch = angles[PITCH];

				NPC_UpdateAngles(qtrue, qtrue);

				NPCS.NPCInfo->desiredYaw = o_yaw;
				NPCS.NPCInfo->desiredPitch = o_pitch;
				return;
			}
		}
	}

	//If we have somewhere to go, then do that
	if (UpdateGoal())
	{
		NPCS.ucmd.buttons |= BUTTON_WALKING;
		NPC_MoveToGoal(qtrue);
	}
	else if (!NPCS.NPC->client->ps.weaponTime
		&& TIMER_Done(NPCS.NPC, "attackDelay")
		&& TIMER_Done(NPCS.NPC, "inactiveDelay"))
	{
		//we want to turn off our saber if we need to.
		if (!NPCS.NPC->client->ps.saber_holstered)
		{
			//saber is on.
			WP_DeactivateSaber(NPCS.NPC, qfalse);
			NPC_SetAnim(NPCS.NPC, SETANIM_BOTH, BOTH_TURNOFF, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
-------------------------
NPC_BSSaberDroid_Attack
-------------------------
*/

void NPC_SaberDroid_PickAttack(void)
{
	int attackAnim = Q_irand(0, 3);
	switch (attackAnim)
	{
	case 0:
	default:
		attackAnim = BOTH_A2_TR_BL;
		NPCS.NPC->client->ps.saber_move = LS_A_TR2BL;
		NPCS.NPC->client->ps.fd.saber_anim_level = SS_MEDIUM;
		break;
	case 1:
		attackAnim = BOTH_A1_BL_TR;
		NPCS.NPC->client->ps.saber_move = LS_A_BL2TR;
		NPCS.NPC->client->ps.fd.saber_anim_level = SS_FAST;
		break;
	case 2:
		attackAnim = BOTH_A1__L__R;
		NPCS.NPC->client->ps.saber_move = LS_A_L2R;
		NPCS.NPC->client->ps.fd.saber_anim_level = SS_FAST;
		break;
	case 3:
		attackAnim = BOTH_A3__L__R;
		NPCS.NPC->client->ps.saber_move = LS_A_L2R;
		NPCS.NPC->client->ps.fd.saber_anim_level = SS_STRONG;
		break;
	}
	NPCS.NPC->client->ps.saberBlocking = saber_moveData[NPCS.NPC->client->ps.saber_move].blocking;
	NPC_SetAnim(NPCS.NPC, SETANIM_BOTH, attackAnim, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
	NPCS.NPC->client->ps.torsoAnim = NPCS.NPC->client->ps.legsAnim;
	//need to do this because we have no anim split but saber code checks torsoAnim
	NPCS.NPC->client->ps.weaponTime = NPCS.NPC->client->ps.torsoTimer = NPCS.NPC->client->ps.legsTimer;
	NPCS.NPC->client->ps.weaponstate = WEAPON_FIRING;
}

void NPC_BSSaberDroid_Attack(void)
{
	//attack behavior
	//Don't do anything if we're hurt
	if (NPCS.NPC->painDebounceTime > level.time)
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}
	//If we don't have an enemy, just idle
	if (NPC_CheckEnemyExt(qfalse) == qfalse) //!NPC->enemy )//
	{
		NPCS.NPC->enemy = NULL;
		NPC_BSSaberDroid_Patrol(); //FIXME: or patrol?
		return;
	}

	if (!NPCS.NPC->enemy)
	{
		//WTF?  somehow we lost our enemy?
		NPC_BSSaberDroid_Patrol(); //FIXME: or patrol?
		return;
	}

	enemyLOS = enemyCS = qfalse;
	qboolean move = qtrue;
	faceEnemy = qfalse;
	shoot = qfalse;
	enemyDist = DistanceSquared(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin);

	//can we see our target?
	if (NPC_ClearLOS4(NPCS.NPC->enemy))
	{
		NPCS.NPCInfo->enemyLastSeenTime = level.time;
		enemyLOS = qtrue;

		if (enemyDist <= 4096 && InFOV3(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin,
			NPCS.NPC->client->ps.viewangles, 90, 45)) //within 64 & infront
		{
			VectorCopy(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPCInfo->enemyLastSeenLocation);
			enemyCS = qtrue;
		}
	}

	if (enemyLOS)
	{
		//FIXME: no need to face enemy if we're moving to some other goal and he's too far away to shoot?
		faceEnemy = qtrue;
	}

	if (!TIMER_Done(NPCS.NPC, "taunting"))
	{
		move = qfalse;
	}
	else if (enemyCS)
	{
		shoot = qtrue;
		if (enemyDist < (NPCS.NPC->r.maxs[0] + NPCS.NPC->enemy->r.maxs[0] + 32) * (NPCS.NPC->r.maxs[0] + NPCS.NPC->enemy
			->r.maxs[0] + 32))
		{
			//close enough
			move = qfalse;
		}
	} //this should make him chase enemy when out of range...?

	if (NPCS.NPC->client->ps.legsTimer
		&& NPCS.NPC->client->ps.legsAnim != BOTH_A3__L__R) //this one is a running attack
	{
		//in the middle of a held, stationary anim, can't move
		move = qfalse;
	}

	if (move)
	{
		//move toward goal
		move = SaberDroid_Move();
		if (move)
		{
			//if we had to chase him, be sure to attack as soon as possible
			TIMER_Set(NPCS.NPC, "attackDelay", NPCS.NPC->client->ps.weaponTime);
		}
	}

	if (!faceEnemy)
	{
		//we want to face in the dir we're running
		if (move)
		{
			//don't run away and shoot
			NPCS.NPCInfo->desiredYaw = NPCS.NPCInfo->lastPathAngles[YAW];
			NPCS.NPCInfo->desiredPitch = 0;
			shoot = qfalse;
		}
		NPC_UpdateAngles(qtrue, qtrue);
	}
	else // if ( faceEnemy )
	{
		//face the enemy
		NPC_FaceEnemy(qtrue);
	}

	if (NPCS.NPCInfo->scriptFlags & SCF_DONT_FIRE)
	{
		shoot = qfalse;
	}
	if (shoot)
	{
		//try to shoot if it's time
		if (TIMER_Done(NPCS.NPC, "attackDelay"))
		{
			if (!(NPCS.NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
			{
				//attack!
				NPC_SaberDroid_PickAttack();
				//set attac delay for next attack.
				if (NPCS.NPCInfo->rank > RANK_CREWMAN)
				{
					TIMER_Set(NPCS.NPC, "attackDelay", NPCS.NPC->client->ps.weaponTime + Q_irand(0, 1000));
				}
				else
				{
					TIMER_Set(NPCS.NPC, "attackDelay",
						NPCS.NPC->client->ps.weaponTime + Q_irand(0, 1000) + Q_irand(
							0, (3 - g_npcspskill.integer) * 2) * 500);
				}
			}
		}
	}
}

extern void WP_ActivateSaber(gentity_t* self);

void NPC_BSSD_Default(void)
{
	if (!NPCS.NPC->enemy)
	{
		//don't have an enemy, look for one
		NPC_BSSaberDroid_Patrol();
	}
	else //if ( NPC->enemy )
	{
		//have an enemy
		if (NPCS.NPC->client->ps.saber_holstered == 2)
		{
			//turn saber on
			WP_ActivateSaber(NPCS.NPC);
			if (NPCS.NPC->client->ps.legsAnim == BOTH_TURNOFF
				|| NPCS.NPC->client->ps.legsAnim == BOTH_STAND1)
			{
				NPC_SetAnim(NPCS.NPC, SETANIM_BOTH, BOTH_TURNON, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}

		NPC_BSSaberDroid_Attack();
		TIMER_Set(NPCS.NPC, "inactiveDelay", Q_irand(2000, 4000));
	}
	if (!NPCS.NPC->client->ps.weaponTime)
	{
		//we're not attacking.
		NPCS.NPC->client->ps.saber_move = LS_READY;
		NPCS.NPC->client->ps.saberBlocking = saber_moveData[LS_READY].blocking;
		//RAFIXME - since this is saber trail code, I think this needs to be ported to cgame.
		NPCS.NPC->client->ps.fd.saber_anim_level = SS_MEDIUM;
		NPCS.NPC->client->ps.weaponstate = WEAPON_READY;
	}
}