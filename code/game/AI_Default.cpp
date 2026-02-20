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

#include "../cgame/cg_local.h"
#include "b_local.h"
#include "g_nav.h"
#include "Q3_Interface.h"

extern int g_crosshairEntNum;
extern void npc_check_evasion();
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern cvar_t* g_Advancedaitalk;

//
extern qboolean npc_is_dark_jedi(const gentity_t* self);
extern qboolean npc_is_light_jedi(const gentity_t* self);
extern void jedi_set_enemy_info(vec3_t enemy_dest, vec3_t enemy_dir, float* enemy_dist, vec3_t enemy_movedir,
	float* enemy_movespeed, int prediction);

void npc_check_speak(gentity_t* speaker_npc)
{
	vec3_t enemy_dir, enemy_movedir, enemy_dest;
	float enemy_dist, enemy_movespeed;

	//See where enemy will be 300 ms from now
	jedi_set_enemy_info(enemy_dest, enemy_dir, &enemy_dist, enemy_movedir, &enemy_movespeed, 300);

	if (g_Advancedaitalk->integer < 1)
	{
		return; // no
	}

	if (speaker_npc->check_speach_time > level.time)
	{
		return; // not yet...
	}

	speaker_npc->check_speach_time = level.time + 5000 + irand(0, 10000);

	if (NPC->enemy && NPC_ClearLOS(NPC->enemy)
		&& enemy_dist < 512 && enemy_dist > 64) // Have enemy...
	{
		if (npc_is_dark_jedi(speaker_npc))
		{
			// Do taunt/anger...
			const int voice_event = Q_irand(0, 8);

			switch (voice_event)
			{
			case 0:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_ANGER1, EV_ANGER3), 10000);
				break;
			case 1:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_COMBAT1, EV_COMBAT3), 10000);
				break;
			case 2:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_DEFLECT1, EV_DEFLECT3), 10000);
				break;
			case 3:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
				break;
			case 4:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_JCHASE1, EV_JCHASE3), 10000);
				break;
			case 5:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_JDETECTED1, EV_JDETECTED3), 10000);
				break;
			case 6:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_JLOST1, EV_JLOST3), 10000);
				break;
			case 7:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_TAUNT1, EV_TAUNT3), 10000);
				break;
			default:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_VICTORY1, EV_VICTORY3), 10000);
				break;
			}
		}
		else if (npc_is_light_jedi(speaker_npc))
		{
			// Do taunt...
			const int voice_event = Q_irand(0, 9);

			switch (voice_event)
			{
			case 0:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_ANGER1, EV_ANGER3), 10000);
				break;
			case 1:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_COMBAT1, EV_COMBAT3), 10000);
				break;
			case 2:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_CONFUSE1, EV_CONFUSE3), 10000);
				break;
			case 3:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_DEFLECT1, EV_DEFLECT3), 10000);
				break;
			case 4:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
				break;
			case 5:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_JCHASE1, EV_JCHASE3), 10000);
				break;
			case 6:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_JDETECTED1, EV_JDETECTED3), 10000);
				break;
			case 7:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_JLOST1, EV_JLOST3), 10000);
				break;
			case 8:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_TAUNT1, EV_TAUNT3), 10000);
				break;
			default:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_VICTORY1, EV_VICTORY3), 10000);
				break;
			}
		}
		else
		{
			// Do taunt/anger...
			const int voice_event = Q_irand(0, 9);

			switch (voice_event)
			{
			case 0:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_ANGER1, EV_ANGER3), 10000);
				break;
			case 1:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_CHASE1, EV_CHASE3), 10000);
				break;
			case 2:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_COVER1, EV_COVER5), 10000);
				break;
			case 3:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_DETECTED1, EV_DETECTED5), 10000);
				break;
			case 4:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_ESCAPING1, EV_ESCAPING3), 10000);
				break;
			case 5:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_GIVEUP1, EV_GIVEUP4), 10000);
				break;
			case 6:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_SIGHT1, EV_SIGHT3), 10000);
				break;
			case 7:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_LOOK1, EV_LOOK2), 10000);
				break;
			case 8:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_SUSPICIOUS1, EV_SUSPICIOUS5), 10000);
				break;
			default:
				G_AddVoiceEvent(speaker_npc, Q_irand(EV_TAUNT1, EV_TAUNT3), 10000);
				break;
			}
		}
	}
}

void g_do_m_block_response(const gentity_t* speaker_npc_self)
{
	const int voice_event = Q_irand(0, 5);

	if (g_Advancedaitalk->integer < 1)
	{
		return; // no
	}

	switch (voice_event)
	{
	case 0:
		G_AddVoiceEvent(speaker_npc_self, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
		break;
	case 1:
		G_AddVoiceEvent(speaker_npc_self, Q_irand(EV_JCHASE1, EV_JCHASE3), 10000);
		break;
	case 2:
		G_AddVoiceEvent(speaker_npc_self, Q_irand(EV_COMBAT1, EV_COMBAT3), 10000);
		break;
	case 3:
		G_AddVoiceEvent(speaker_npc_self, Q_irand(EV_ANGER1, EV_ANGER3), 10000);
		break;
	case 4:
		G_AddVoiceEvent(speaker_npc_self, Q_irand(EV_TAUNT1, EV_TAUNT3), 10000);
		break;
	default:
		G_AddVoiceEvent(speaker_npc_self, Q_irand(EV_PUSHED1, EV_PUSHED3), 10000);
		break;
	}
}

enum
{
	speech_chase,
	speech_confused,
	speech_cover,
	speech_detected,
	speech_giveup,
	speech_look,
	speech_lost,
	speech_outflank,
	speech_escaping,
	speech_sight,
	speech_sound,
	speech_suspicious,
	speech_yell,
	speech_greet
};

void speaker_speech(const gentity_t* self, const int speech_type, const float fail_chance)
{
	if (g_Advancedaitalk->integer < 1)
	{
		return; // no
	}

	if (Q_flrand(0.0f, 1.0f) < fail_chance)
	{
		return;
	}

	if (fail_chance >= 0)
	{
		//a negative failChance makes it always talk
		if (self->NPC->group)
		{
			if (self->NPC->group->speechDebounceTime > level.time)
			{
				return;
			}
		}
		else if (!TIMER_Done(self, "chatter"))
		{
			//personal timer
			return;
		}
	}

	TIMER_Set(self, "chatter", Q_irand(12000, 14000));

	if (self->NPC->blockedSpeechDebounceTime > level.time)
	{
		return;
	}

	switch (speech_type)
	{
	case speech_chase:
		G_AddVoiceEvent(self, Q_irand(EV_CHASE1, EV_CHASE3), 10000);
		break;
	case speech_confused:
		G_AddVoiceEvent(self, Q_irand(EV_CONFUSE1, EV_CONFUSE3), 10000);
		break;
	case speech_cover:
		G_AddVoiceEvent(self, Q_irand(EV_COVER1, EV_COVER5), 10000);
		break;
	case speech_detected:
		G_AddVoiceEvent(self, Q_irand(EV_DETECTED1, EV_DETECTED5), 10000);
		break;
	case speech_giveup:
		G_AddVoiceEvent(self, Q_irand(EV_GIVEUP1, EV_GIVEUP4), 10000);
		break;
	case speech_look:
		G_AddVoiceEvent(self, Q_irand(EV_LOOK1, EV_LOOK2), 10000);
		break;
	case speech_lost:
		G_AddVoiceEvent(self, EV_LOST1, 10000);
		break;
	case speech_outflank:
		G_AddVoiceEvent(self, Q_irand(EV_OUTFLANK1, EV_OUTFLANK2), 10000);
		break;
	case speech_escaping:
		G_AddVoiceEvent(self, Q_irand(EV_ESCAPING1, EV_ESCAPING3), 10000);
		break;
	case speech_sight:
		G_AddVoiceEvent(self, Q_irand(EV_SIGHT1, EV_SIGHT3), 10000);
		break;
	case speech_sound:
		G_AddVoiceEvent(self, Q_irand(EV_SOUND1, EV_SOUND3), 10000);
		break;
	case speech_suspicious:
		G_AddVoiceEvent(self, Q_irand(EV_SUSPICIOUS1, EV_SUSPICIOUS5), 10000);
		break;
	case speech_yell:
		G_AddVoiceEvent(self, Q_irand(EV_ANGER1, EV_ANGER3), 10000);
		break;
	case speech_greet:
		G_AddVoiceEvent(self, Q_irand(EV_PUSHED1, EV_PUSHED3), 10000);
		break;
	default:
		break;
	}

	self->NPC->blockedSpeechDebounceTime = level.time + 2000;
}

void NPC_LostEnemyDecideChase()
{
	switch (NPCInfo->behaviorState)
	{
	case BS_HUNT_AND_KILL:
		//We were chasing him and lost him, so try to find him
		if (NPC->enemy == NPCInfo->goalEntity && NPC->enemy->lastWaypoint != WAYPOINT_NONE)
		{
			//Remember his last valid Wp, then check it out
			//FIXME: Should we only do this if there's no other enemies or we've got LOCKED_ENEMY on?
			NPC_BSSearchStart(NPC->enemy->lastWaypoint, BS_SEARCH);
		}
		//If he's not our goalEntity, we're running somewhere else, so lose him
		break;
	default:
		break;
	}
	G_ClearEnemy(NPC);
}

/*
-------------------------
NPC_StandIdle
-------------------------
*/

static void NPC_StandIdle()
{
	if (!in_camera)
	{
		npc_check_evasion();
	}
}

static qboolean NPC_StandTrackAndShoot(const gentity_t* npc, const qboolean can_duck)
{
	qboolean attack_ok = qfalse;
	qboolean duck_ok = qfalse;
	qboolean faced = qfalse;

	//First see if we're hurt bad- if so, duck
	//FIXME: if even when ducked, we can shoot someone, we should.
	//Maybe is can be shot even when ducked, we should run away to the nearest cover?
	if (can_duck)
	{
		if (npc->health < 20)
		{
			//	if( NPC->svFlags&SVF_HEALING || Q_flrand(0.0f, 1.0f) )
			if (Q_flrand(0.0f, 1.0f))
			{
				duck_ok = qtrue;
			}
		}
		else if (npc->health < 40)
		{
			//			if ( NPC->svFlags&SVF_HEALING )
			//			{//Medic is on the way, get down!
			//				duck_ok = qtrue;
			//			}
		}
	}

	//NPC_CheckEnemy( qtrue, qfalse );

	if (!duck_ok)
	{
		constexpr float attack_scale = 1.0;
		//made this whole part a function call
		attack_ok = NPC_CheckCanAttack(attack_scale);
		faced = qtrue;
	}

	if (can_duck && (duck_ok || !attack_ok && client->fireDelay == 0) && ucmd.upmove != -127)
	{
		//if we didn't attack check to duck if we're not already
		if (!duck_ok)
		{
			if (npc->enemy->client)
			{
				if (npc->enemy->enemy == npc)
				{
					if (npc->enemy->client->buttons & BUTTON_ATTACK)
					{
						//FIXME: determine if enemy fire angles would hit me or get close
						if (NPC_CheckDefend(1.0)) //FIXME: Check self-preservation?  Health?
						{
							duck_ok = qtrue;
						}
					}
				}
			}
		}

		if (duck_ok)
		{
			ucmd.upmove = -127;
			NPCInfo->duckDebounceTime = level.time + 1000; //duck for a full second
		}
	}

	return faced;
}

void NPC_BSIdle()
{
	//FIXME if there is no nav data, we need to do something else
	// if we're stuck, try to move around it
	if (UpdateGoal())
	{
		NPC_MoveToGoal(qtrue);
	}

	if (ucmd.forwardmove == 0 && ucmd.rightmove == 0 && ucmd.upmove == 0)
	{
		//		NPC_StandIdle();
	}

	NPC_UpdateAngles(qtrue, qtrue);
	ucmd.buttons |= BUTTON_WALKING;
}

static void NPC_BSRun()
{
	//FIXME if there is no nav data, we need to do something else
	// if we're stuck, try to move around it
	if (UpdateGoal())
	{
		NPC_MoveToGoal(qtrue);
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

void NPC_BSStandGuard()
{
	//FIXME: Use Snapshot info
	if (NPC->enemy == nullptr)
	{
		//Possible to pick one up by being shot
		if (Q_flrand(0.0f, 1.0f) < 0.5)
		{
			if (NPC->client->enemyTeam)
			{
				gentity_t* newenemy = NPC_PickEnemy(
					NPC, NPC->client->enemyTeam,
					static_cast<qboolean>(NPC->cantHitEnemyCounter < 10),
					static_cast<qboolean>(NPC->client->enemyTeam == TEAM_PLAYER),
					qtrue);

				//only checks for vis if couldn't hit last enemy
				if (newenemy)
				{
					G_SetEnemy(NPC, newenemy);
				}
			}
		}
	}

	npc_check_speak(NPC);

	if (NPC->enemy != nullptr)
	{
		if (NPCInfo->tempBehavior == BS_STAND_GUARD)
		{
			NPCInfo->tempBehavior = BS_DEFAULT;
		}

		if (NPCInfo->behaviorState == BS_STAND_GUARD)
		{
			NPCInfo->behaviorState = BS_STAND_AND_SHOOT;
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
-------------------------
NPC_BSHuntAndKill
-------------------------
*/

void NPC_BSHuntAndKill()
{
	qboolean turned = qfalse;

	if (!in_camera)
	{
		npc_check_evasion();
	}

	NPC_CheckEnemy(static_cast<qboolean>(NPCInfo->tempBehavior != BS_HUNT_AND_KILL), qfalse);
	//don't find new enemy if this is tempbehav

	if (NPC->enemy)
	{
		const visibility_t o_e_vis = enemyVisibility = NPC_CheckVisibility(NPC->enemy, CHECK_FOV | CHECK_SHOOT);
		//CHECK_360|//CHECK_PVS|
		if (enemyVisibility > VIS_PVS)
		{
			if (!NPC_EnemyTooFar(NPC->enemy, 0, qtrue))
			{
				//Enemy is close enough to shoot - FIXME: this next func does this also, but need to know here for info on whether ot not to turn later
				NPC_CheckCanAttack(1.0);
				turned = qtrue;
			}
		}

		const int curAnim = NPC->client->ps.legsAnim;
		if (curAnim != BOTH_ATTACK1 && curAnim != BOTH_ATTACK2 && curAnim != BOTH_ATTACK3 && curAnim != BOTH_MELEE1 &&
			curAnim != BOTH_MELEE2)
		{
			vec3_t vec;
			//Don't move toward enemy if we're in a full-body attack anim
			//FIXME, use IdealDistance to determin if we need to close distance
			VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, vec);
			const float enemy_dist = VectorLength(vec);
			if (enemy_dist > 48 && (enemy_dist * 1.5 * (enemy_dist * 1.5) >= NPC_MaxDistSquaredForWeapon() ||
				o_e_vis != VIS_SHOOT ||
				//!(ucmd.buttons & BUTTON_ATTACK) ||
				enemy_dist > IdealDistance() * 3))
			{
				//We should close in?
				NPCInfo->goalEntity = NPC->enemy;

				NPC_MoveToGoal(qtrue);
			}
			else if (enemy_dist < IdealDistance())
			{
				//We should back off?
				//if(ucmd.buttons & BUTTON_ATTACK)
				{
					NPCInfo->goalEntity = NPC->enemy;
					NPCInfo->goalRadius = 12;
					NPC_MoveToGoal(qtrue);

					ucmd.forwardmove *= -1;
					ucmd.rightmove *= -1;
					VectorScale(NPC->client->ps.moveDir, -1, NPC->client->ps.moveDir);

					ucmd.buttons |= BUTTON_WALKING;
				}
			} //otherwise, stay where we are
		}
	}
	else
	{
		//ok, stand guard until we find an enemy
		if (!in_camera)
		{
			npc_check_evasion();
		}
		if (NPCInfo->tempBehavior == BS_HUNT_AND_KILL)
		{
			NPCInfo->tempBehavior = BS_DEFAULT;
		}
		else
		{
			NPCInfo->tempBehavior = BS_STAND_GUARD;
			NPC_BSStandGuard();
		}
		return;
	}

	if (!turned)
	{
		NPC_UpdateAngles(qtrue, qtrue);
	}
}

void NPC_BSStandAndShoot()
{
	if (NPC->client->playerTeam && NPC->client->enemyTeam)
	{
		//
	}

	if (!in_camera)
	{
		npc_check_evasion();
	}

	NPC_CheckEnemy(qtrue, qfalse);

	if (NPCInfo->duckDebounceTime > level.time && NPC->client->ps.weapon != WP_SABER)
	{
		ucmd.upmove = -127;
		if (NPC->enemy)
		{
			NPC_CheckCanAttack(1.0);
		}
		return;
	}

	if (NPC->enemy)
	{
		if (!NPC_StandTrackAndShoot(NPC, qtrue))
		{
			//That func didn't update our angles
			NPCInfo->desiredYaw = NPC->client->ps.viewangles[YAW];
			NPCInfo->desiredPitch = NPC->client->ps.viewangles[PITCH];
			NPC_UpdateAngles(qtrue, qtrue);
		}
	}
	else
	{
		NPCInfo->desiredYaw = NPC->client->ps.viewangles[YAW];
		NPCInfo->desiredPitch = NPC->client->ps.viewangles[PITCH];
		NPC_UpdateAngles(qtrue, qtrue);
		//		NPC_BSIdle();//only moves if we have a goal
	}
}

void NPC_BSRunAndShoot()
{
	if (!in_camera)
	{
		npc_check_evasion();
	}

	NPC_CheckEnemy(qtrue, qfalse);

	npc_check_speak(NPC);

	if (NPCInfo->duckDebounceTime > level.time)
	{
		ucmd.upmove = -127;
		if (NPC->enemy)
		{
			NPC_CheckCanAttack(1.0);
		}
		return;
	}

	if (NPC->enemy)
	{
		const int monitor = NPC->cantHitEnemyCounter;
		NPC_StandTrackAndShoot(NPC, qfalse); //(NPCInfo->hidingGoal != NULL) );

		if (!(ucmd.buttons & BUTTON_ATTACK) && ucmd.upmove >= 0 && NPC->cantHitEnemyCounter > monitor)
		{
			//not crouching and not firing
			vec3_t vec;

			VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, vec);
			vec[2] = 0;
			if (VectorLength(vec) > 128 || NPC->cantHitEnemyCounter >= 10)
			{
				//run at enemy if too far away
				//The cantHitEnemyCounter getting high has other repercussions
				//100 (10 seconds) will make you try to pick a new enemy...
				//But we're chasing, so we clamp it at 50 here
				if (NPC->cantHitEnemyCounter > 60)
				{
					NPC->cantHitEnemyCounter = 60;
				}

				if (NPC->cantHitEnemyCounter >= (NPCInfo->stats.aggression + 1) * 10)
				{
					NPC_LostEnemyDecideChase();
				}

				//chase and face
				ucmd.angles[YAW] = 0;
				ucmd.angles[PITCH] = 0;
				NPCInfo->goalEntity = NPC->enemy;
				NPCInfo->goalRadius = 12;
				NPC_MoveToGoal(qtrue);
				NPC_UpdateAngles(qtrue, qtrue);
			}
			else
			{
				//FIXME: this could happen if they're just on the other side
				//of a thin wall or something else blocking out shot.  That
				//would make us just stand there and not go around it...
				//but maybe it's okay- might look like we're waiting for
				//him to come out...?
				//Current solution: runs around if cantHitEnemyCounter gets
				//to 10 (1 second).
			}
		}
		else
		{
			//Clear the can't hit enemy counter here
			NPC->cantHitEnemyCounter = 0;
		}
	}
	else
	{
		if (NPCInfo->tempBehavior == BS_HUNT_AND_KILL)
		{
			//lost him, go back to what we were doing before
			NPCInfo->tempBehavior = BS_DEFAULT;
		}

		//		NPC_BSRun();//only moves if we have a goal
	}
}

//Simply turn until facing desired angles
static void NPC_BSFace()
{
	//FIXME: once you stop sending turning info, they reset to whatever their delta_angles was last????
	//Once this is over, it snaps back to what it was facing before- WHY???
	if (NPC_UpdateAngles(qtrue, qtrue))
	{
		Q3_TaskIDComplete(NPC, TID_BSTATE);

		NPCInfo->desiredYaw = client->ps.viewangles[YAW];
		NPCInfo->desiredPitch = client->ps.viewangles[PITCH];

		NPCInfo->aimTime = 0; //ok to turn normally now
	}
}

void NPC_BSPointShoot(const qboolean shoot)
{
	//FIXME: doesn't check for clear shot...
	vec3_t muzzle, dir, angles, org;

	if (!NPC->enemy || !NPC->enemy->inuse || NPC->enemy->NPC && NPC->enemy->health <= 0)
	{
		//FIXME: should still keep shooting for a second or two after they actually die...
		Q3_TaskIDComplete(NPC, TID_BSTATE);
		goto finished;
	}

	if (!in_camera)
	{
		npc_check_evasion();
	}

	CalcEntitySpot(NPC, SPOT_WEAPON, muzzle);
	CalcEntitySpot(NPC->enemy, SPOT_HEAD, org); //Was spot_org
	//Head is a little high, so let's aim for the chest:
	if (NPC->enemy->client)
	{
		org[2] -= 12; //NOTE: is this enough?
	}

	VectorSubtract(org, muzzle, dir);
	vectoangles(dir, angles);

	switch (NPC->client->ps.weapon)
	{
	case WP_NONE:
	case WP_MELEE:
	case WP_TUSKEN_STAFF:
	case WP_SABER:
		//don't do any pitch change if not holding a firing weapon
		break;
	default:
		NPCInfo->desiredPitch = NPCInfo->lockedDesiredPitch = AngleNormalize360(angles[PITCH]);
		break;
	}

	NPCInfo->desiredYaw = NPCInfo->lockedDesiredYaw = AngleNormalize360(angles[YAW]);

	if (NPC_UpdateAngles(qtrue, qtrue))
	{
		//FIXME: if angles clamped, this may never work!
		//NPCInfo->shotTime = NPC->attackDebounceTime = 0;

		if (shoot)
		{
			//FIXME: needs to hold this down if using a weapon that requires it, like phaser...
			ucmd.buttons |= BUTTON_ATTACK;
		}

		if (!shoot || !(NPC->svFlags & SVF_LOCKEDENEMY))
		{
			//If locked_enemy is on, dont complete until it is destroyed...
			Q3_TaskIDComplete(NPC, TID_BSTATE);
			goto finished;
		}
	}
	else if (shoot && NPC->svFlags & SVF_LOCKEDENEMY)
	{
		//shooting them till their dead, not aiming right at them yet...
		/*
		qboolean movingTarget = qfalse;

		if ( NPC->enemy->client )
		{
			if ( VectorLengthSquared( NPC->enemy->client->ps.velocity ) )
			{
				movingTarget = qtrue;
			}
		}
		else if ( VectorLengthSquared( NPC->enemy->s.pos.trDelta ) )
		{
			movingTarget = qtrue;
		}

		if (movingTarget )
		*/
		{
			const float dist = VectorLength(dir);
			float yaw_miss_allow = NPC->enemy->maxs[0];
			float pitch_miss_allow = (NPC->enemy->maxs[2] - NPC->enemy->mins[2]) / 2;

			if (yaw_miss_allow < 8.0f)
			{
				yaw_miss_allow = 8.0f;
			}

			if (pitch_miss_allow < 8.0f)
			{
				pitch_miss_allow = 8.0f;
			}

			const float yawMiss = tan(DEG2RAD(AngleDelta(NPC->client->ps.viewangles[YAW], NPCInfo->desiredYaw))) * dist;
			const float pitchMiss = tan(DEG2RAD(AngleDelta(NPC->client->ps.viewangles[PITCH], NPCInfo->desiredPitch))) *
				dist;

			if (yaw_miss_allow >= yawMiss && pitch_miss_allow > pitchMiss)
			{
				ucmd.buttons |= BUTTON_ATTACK;
			}
		}
	}

	return;

finished:
	NPCInfo->desiredYaw = client->ps.viewangles[YAW];
	NPCInfo->desiredPitch = client->ps.viewangles[PITCH];

	NPCInfo->aimTime = 0; //ok to turn normally now
}

/*
void NPC_BSMove(void)
Move in a direction, face another
*/
static void NPC_BSMove()
{
	NPC_CheckEnemy(qtrue, qfalse);
	if (NPC->enemy)
	{
		NPC_CheckCanAttack(1.0);
	}
	else
	{
		if (!in_camera)
		{
			npc_check_evasion();
		}
		NPC_UpdateAngles(qtrue, qtrue);
	}

	const gentity_t* goal = UpdateGoal();
	if (goal)
	{
		NPC_SlideMoveToGoal();
	}
}

/*
void NPC_BSShoot(void)
Move in a direction, face another
*/

static void NPC_BSShoot()
{
	if (!in_camera)
	{
		npc_check_evasion();
	}

	enemyVisibility = VIS_SHOOT;

	if (client->ps.weaponstate != WEAPON_READY && client->ps.weaponstate != WEAPON_FIRING)
	{
		client->ps.weaponstate = WEAPON_READY;
	}

	WeaponThink();
}

/*
void NPC_BSPatrol( void )

  Same as idle, but you look for enemies every "vigilance"
  using your angles, HFOV, VFOV and visrange, and listen for sounds within earshot...
*/
void NPC_BSPatrol()
{
	if (!in_camera)
	{
		npc_check_evasion();
	}

	if (level.time > NPCInfo->enemyCheckDebounceTime)
	{
		NPCInfo->enemyCheckDebounceTime = level.time + NPCInfo->stats.vigilance * 1000;
		NPC_CheckEnemy(qtrue, qfalse);
		if (NPC->enemy)
		{
			//FIXME: do anger script
			NPCInfo->behaviorState = BS_HUNT_AND_KILL;
			//NPC_AngerSound();
			return;
		}
	}

	//FIXME: Implement generic sound alerts
	/*
	alert_event_num = NPC_CheckAlertEvents( qtrue, qtrue );
	if( alert_event_num != -1 )
	{//If we heard something, see if we should check it out
		if ( NPC_CheckInvestigate( alert_event_num ) )
		{
			return;
		}
	}
	*/

	NPCInfo->investigateSoundDebounceTime = 0;
	//FIXME if there is no nav data, we need to do something else
	// if we're stuck, try to move around it
	if (UpdateGoal())
	{
		NPC_MoveToGoal(qtrue);
	}

	NPC_UpdateAngles(qtrue, qtrue);

	ucmd.buttons |= BUTTON_WALKING;
}

/*
void NPC_BSDefault(void)
	uses various scriptflags to determine how an npc should behave
*/
extern void NPC_CheckGetNewWeapon();
extern void NPC_BSST_Attack();

void NPC_BSDefault()
{
	qboolean move = qtrue;

	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}

	if (NPCInfo->scriptFlags & SCF_FORCED_MARCH)
	{
		//being forced to walk
		if (NPC->client->ps.torsoAnim != TORSO_SURRENDER_START)
		{
			NPC_SetAnim(NPC, SETANIM_TORSO, TORSO_SURRENDER_START, SETANIM_FLAG_HOLD);
		}
	}

	//look for a new enemy if don't have one and are allowed to look, validate current enemy if have one
	NPC_CheckEnemy(static_cast<qboolean>((NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES) != 0), qfalse);

	if (!NPC->enemy)
	{
		if (!(NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
		{
			const int alert_event = NPC_CheckAlertEvents(qtrue, qtrue, -1, qtrue, AEL_DISCOVERED);

			if (alert_event >= 0)
			{
				if (level.alertEvents[alert_event].level >= AEL_DISCOVERED &&
					(NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES))
				{
					if (level.alertEvents[alert_event].owner &&
						level.alertEvents[alert_event].owner != NPC &&
						level.alertEvents[alert_event].owner->client &&
						level.alertEvents[alert_event].owner->health >= 0 &&
						level.alertEvents[alert_event].owner->client->playerTeam == NPC->client->enemyTeam)
					{
						G_SetEnemy(NPC, level.alertEvents[alert_event].owner);
					}
				}
			}
		}
	}

	if (NPC->enemy && !(NPCInfo->scriptFlags & SCF_FORCED_MARCH))
	{
		NPC_CheckGetNewWeapon();

		if (NPC->client->leader &&
			NPCInfo->goalEntity == NPC->client->leader &&
			!Q3_TaskIDPending(NPC, TID_MOVE_NAV))
		{
			NPC_ClearGoal();
		}

		NPC_BSST_Attack();
		npc_check_speak(NPC);
		return;
	}

	if (UpdateGoal())
	{
		if (!NPC->enemy &&
			NPC->client->leader &&
			NPCInfo->goalEntity == NPC->client->leader &&
			!Q3_TaskIDPending(NPC, TID_MOVE_NAV))
		{
			NPC_BSFollowLeader();
		}
		else
		{
			// FACE DIRECTION LOGIC — now NULL‑safe
			if (NPCInfo->scriptFlags & SCF_FACE_MOVE_DIR ||
				(NPCInfo->goalEntity && NPCInfo->goalEntity != NPC->enemy))
			{
				NPCInfo->combatMove = qfalse;
			}
			else if (NPCInfo->goalEntity)   // NULL‑check added here
			{
				vec3_t dir, angles;

				NPCInfo->combatMove = qfalse;

				VectorSubtract(NPCInfo->goalEntity->currentOrigin, NPC->currentOrigin, dir);
				vectoangles(dir, angles);

				NPCInfo->desiredYaw = angles[YAW];

				if (NPCInfo->goalEntity == NPC->enemy)
				{
					NPCInfo->desiredPitch = angles[PITCH];
				}
			}

			// movement flags
			if (NPCInfo->scriptFlags & SCF_RUNNING)
			{
				ucmd.buttons &= ~BUTTON_WALKING;
			}
			else if (NPCInfo->scriptFlags & SCF_WALKING)
			{
				ucmd.buttons |= BUTTON_WALKING;
			}
			else if (NPCInfo->goalEntity == NPC->enemy)
			{
				ucmd.buttons &= ~BUTTON_WALKING;
			}
			else
			{
				ucmd.buttons |= BUTTON_WALKING;
			}

			if (NPCInfo->scriptFlags & SCF_FORCED_MARCH)
			{
				if (g_crosshairEntNum != NPC->s.number)
				{
					move = qfalse;
				}
			}

			if (move)
			{
				NPC_MoveToGoal(qtrue);
			}
		}
	}
	else if (!NPC->enemy && NPC->client->leader)
	{
		NPC_BSFollowLeader();
	}

	NPC_UpdateAngles(qtrue, qtrue);
}