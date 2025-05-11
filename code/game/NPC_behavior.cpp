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

//NPC_behavior.cpp
/*
FIXME - MCG:
These all need to make use of the snapshots.  Write something that can look for only specific
things in a snapshot or just go through the snapshot every frame and save the info in case
we need it...
*/

#include "../qcommon/q_shared.h"
#include "../cgame/cg_local.h"
#include "g_navigator.h"
#include "Q3_Interface.h"
#include "b_local.h"
#include "g_functions.h"
#include "g_nav.h"

extern cvar_t* g_AIsurrender;
extern qboolean showBBoxes;
static vec3_t NPCDEBUG_BLUE = { 0.0, 0.0, 1.0 };
extern void CG_Cube(vec3_t mins, vec3_t maxs, vec3_t color, float alpha);
extern void NPC_CheckGetNewWeapon();
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern void NPC_AimAdjust(int change);
extern qboolean g_standard_humanoid(gentity_t* self);
/*
 void NPC_BSAdvanceFight ()

Advance towards your captureGoal and shoot anyone you can along the way.
*/
void NPC_BSAdvanceFight()
{
	//Make sure we're still headed where we want to capture
	if (NPCInfo->captureGoal)
	{
		NPC_SetMoveGoal(NPC, NPCInfo->captureGoal->currentOrigin, 16, qtrue);

		NPCInfo->goalTime = level.time + 100000;
	}

	NPC_CheckEnemy(qtrue, qfalse);

	//FIXME: Need melee code
	if (NPC->enemy)
	{
		//See if we can shoot him
		vec3_t delta;
		vec3_t angle_to_enemy;
		vec3_t muzzle, enemy_org;
		qboolean attack_ok = qfalse;
		qboolean dead_on = qfalse;
		float attack_scale = 1.0;

		//Yaw to enemy
		VectorMA(NPC->enemy->absmin, 0.5, NPC->enemy->maxs, enemy_org);
		CalcEntitySpot(NPC, SPOT_WEAPON, muzzle);

		VectorSubtract(enemy_org, muzzle, delta);
		vectoangles(delta, angle_to_enemy);
		const float distance_to_enemy = VectorNormalize(delta);

		if (!NPC_EnemyTooFar(NPC->enemy, distance_to_enemy * distance_to_enemy, qtrue))
		{
			attack_ok = qtrue;
		}

		if (attack_ok)
		{
			NPC_UpdateShootAngles(angle_to_enemy, qfalse, qtrue);

			NPCInfo->enemyLastVisibility = enemyVisibility;
			enemyVisibility = NPC_CheckVisibility(NPC->enemy, CHECK_FOV); //CHECK_360|//CHECK_PVS|

			if (enemyVisibility == VIS_FOV)
			{
				vec3_t enemy_head;
				vec3_t hitspot;
				//He's in our FOV
				attack_ok = qtrue;
				CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemy_head);

				if (attack_ok)
				{
					trace_t tr;
					//are we gonna hit him if we shoot at his center?
					gi.trace(&tr, muzzle, nullptr, nullptr, enemy_org, NPC->s.number, MASK_SHOT,
						static_cast<EG2_Collision>(0), 0);
					const gentity_t* traceEnt = &g_entities[tr.entityNum];
					if (traceEnt != NPC->enemy &&
						(!traceEnt || !traceEnt->client || !NPC->client->enemyTeam || NPC->client->enemyTeam !=
							traceEnt
							->client->playerTeam))
					{
						//no, so shoot for the head
						attack_scale *= 0.75;
						gi.trace(&tr, muzzle, nullptr, nullptr, enemy_head, NPC->s.number, MASK_SHOT,
							static_cast<EG2_Collision>(0), 0);
						traceEnt = &g_entities[tr.entityNum];
					}

					VectorCopy(tr.endpos, hitspot);

					if (traceEnt == NPC->enemy || traceEnt->client && NPC->client->enemyTeam && NPC->client->enemyTeam
						== traceEnt->client->playerTeam)
					{
						dead_on = qtrue;
					}
					else
					{
						attack_scale *= 0.5;
						if (NPC->client->playerTeam)
						{
							if (traceEnt && traceEnt->client && traceEnt->client->playerTeam)
							{
								if (NPC->client->playerTeam == traceEnt->client->playerTeam)
								{
									//Don't shoot our own team
									attack_ok = qfalse;
								}
							}
						}
					}
				}

				if (attack_ok)
				{
					//ok, now adjust pitch aim
					VectorSubtract(hitspot, muzzle, delta);
					vectoangles(delta, angle_to_enemy);
					NPC->NPC->desiredPitch = angle_to_enemy[PITCH];
					NPC_UpdateShootAngles(angle_to_enemy, qtrue, qfalse);

					if (!dead_on)
					{
						constexpr float max_aim_off = 64;
						vec3_t diff;
						vec3_t forward;
						//We're not going to hit him directly, try a suppressing fire
						//see if where we're going to shoot is too far from his origin
						AngleVectors(NPCInfo->shootAngles, forward, nullptr, nullptr);
						VectorMA(muzzle, distance_to_enemy, forward, hitspot);
						VectorSubtract(hitspot, enemy_org, diff);
						float aim_off = VectorLength(diff);
						if (aim_off > Q_flrand(0.0f, 1.0f) * max_aim_off) //FIXME: use aim value to allow poor aim?
						{
							attack_scale *= 0.75;
							//see if where we're going to shoot is too far from his head
							VectorSubtract(hitspot, enemy_head, diff);
							aim_off = VectorLength(diff);
							if (aim_off > Q_flrand(0.0f, 1.0f) * max_aim_off)
							{
								attack_ok = qfalse;
							}
						}
						attack_scale *= (max_aim_off - aim_off + 1) / max_aim_off;
					}
				}
			}
		}

		if (attack_ok)
		{
			if (NPC_CheckAttack(attack_scale))
			{
				//check aggression to decide if we should shoot
				enemyVisibility = VIS_SHOOT;
				WeaponThink();
			}
		}
	}
	else
	{
		NPC_UpdateShootAngles(NPC->client->ps.viewangles, qtrue, qtrue);
	}

	if (!ucmd.forwardmove && !ucmd.rightmove)
	{
		//We reached our captureGoal
		if (NPC->m_iIcarusID != IIcarusInterface::ICARUS_INVALID /*NPC->taskManager*/)
		{
			Q3_TaskIDComplete(NPC, TID_BSTATE);
		}
	}
}

void Disappear(gentity_t* self)
{
	//	ClientDisconnect(self);
	self->s.eFlags |= EF_NODRAW;
	self->e_ThinkFunc = thinkF_NULL;
	self->nextthink = -1;
}

void MakeOwnerInvis(gentity_t* self);

static void BeamOut(gentity_t* self)
{
	//fixme: doesn't actually go away!
	self->nextthink = level.time + 1500;
	self->e_ThinkFunc = thinkF_Disappear;
	self->client->playerTeam = TEAM_FREE;
	self->svFlags |= SVF_BEAMING;
}

void NPC_BSCinematic()
{
	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}
	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON_NO_ANIM)
	{
		if (TIMER_Done(NPC, "NoAnimFireDelay"))
		{
			TIMER_Set(NPC, "NoAnimFireDelay", NPC_AttackDebounceForWeapon());
			FireWeapon(NPC, static_cast<qboolean>((NPCInfo->scriptFlags & SCF_ALT_FIRE) != 0));
		}
	}

	if (UpdateGoal())
	{
		//have a goalEntity
		//move toward goal, should also face that goal
		NPC_MoveToGoal(qtrue);
	}

	if (NPCInfo->watchTarget)
	{
		//have an entity which we want to keep facing
		//NOTE: this will override any angles set by NPC_MoveToGoal
		vec3_t eyes, view_spot, viewvec, viewangles;

		CalcEntitySpot(NPC, SPOT_HEAD_LEAN, eyes);
		CalcEntitySpot(NPCInfo->watchTarget, SPOT_HEAD_LEAN, view_spot);

		VectorSubtract(view_spot, eyes, viewvec);

		vectoangles(viewvec, viewangles);

		NPCInfo->lockedDesiredYaw = NPCInfo->desiredYaw = viewangles[YAW];
		NPCInfo->lockedDesiredPitch = NPCInfo->desiredPitch = viewangles[PITCH];
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

void NPC_BSWait()
{
	NPC_UpdateAngles(qtrue, qtrue);
}

void NPC_BSInvestigate()
{
	/*
		//FIXME: maybe allow this to be set as a tempBState in a script?  Just specify the
		//investigateGoal, investigateDebounceTime and investigateCount? (Needs a macro)
		vec3_t		invDir, invAngles, spot;
		gentity_t	*saveGoal;
		//BS_INVESTIGATE would turn toward goal, maybe take a couple steps towards it,
		//look for enemies, then turn away after your investigate counter was down-
		//investigate counter goes up every time you set it...

		if(level.time > NPCInfo->enemyCheckDebounceTime)
		{
			NPCInfo->enemyCheckDebounceTime = level.time + (NPCInfo->stats.vigilance * 1000);
			NPC_CheckEnemy(qtrue, qfalse);
			if(NPC->enemy)
			{//FIXME: do anger script
				NPCInfo->goalEntity = NPC->enemy;
				NPCInfo->behaviorState = BS_RUN_AND_SHOOT;
				NPCInfo->tempBehavior = BS_DEFAULT;
				NPC_AngerSound();
				return;
			}
		}

		NPC_SetAnim( NPC, SETANIM_TORSO, TORSO_WEAPONREADY3, SETANIM_FLAG_NORMAL );

		if(NPCInfo->stats.vigilance <= 1.0 && NPCInfo->eventOwner)
		{
			VectorCopy(NPCInfo->eventOwner->currentOrigin, NPCInfo->investigateGoal);
		}

		saveGoal = NPCInfo->goalEntity;
		if(	level.time > NPCInfo->walkDebounceTime )
		{
			vec3_t	vec;

			VectorSubtract(NPCInfo->investigateGoal, NPC->currentOrigin, vec);
			vec[2] = 0;
			if(VectorLength(vec) > 64)
			{
				if(Q_irand(0, 100) < NPCInfo->investigateCount)
				{//take a full step
					//NPCInfo->walkDebounceTime = level.time + 1400;
					//actually finds length of my BOTH_WALK anim
					NPCInfo->walkDebounceTime = PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_WALK1 );
				}
			}
		}

		if(	level.time < NPCInfo->walkDebounceTime )
		{//walk toward investigateGoal
			/ *
			NPCInfo->goalEntity = NPCInfo->tempGoal;
			VectorCopy(NPCInfo->investigateGoal, NPCInfo->tempGoal->currentOrigin);
			*/

			/*		NPC_SetMoveGoal( NPC, NPCInfo->investigateGoal, 16, qtrue );

					NPC_MoveToGoal( qtrue );

					//FIXME: walk2?
					NPC_SetAnim(NPC,SETANIM_LEGS,BOTH_WALK1,SETANIM_FLAG_NORMAL);

					ucmd.buttons |= BUTTON_WALKING;
				}
				else
				{
					NPC_SetAnim(NPC,SETANIM_LEGS,BOTH_STAND1,SETANIM_FLAG_NORMAL);

					if(NPCInfo->hlookCount > 30)
					{
						if(Q_irand(0, 10) > 7)
						{
							NPCInfo->hlookCount = 0;
						}
					}
					else if(NPCInfo->hlookCount < -30)
					{
						if(Q_irand(0, 10) > 7)
						{
							NPCInfo->hlookCount = 0;
						}
					}
					else if(NPCInfo->hlookCount == 0)
					{
						NPCInfo->hlookCount = Q_irand(-1, 1);
					}
					else if(Q_irand(0, 10) > 7)
					{
						if(NPCInfo->hlookCount > 0)
						{
							NPCInfo->hlookCount++;
						}
						else//lookCount < 0
						{
							NPCInfo->hlookCount--;
						}
					}

					if(NPCInfo->vlookCount >= 15)
					{
						if(Q_irand(0, 10) > 7)
						{
							NPCInfo->vlookCount = 0;
						}
					}
					else if(NPCInfo->vlookCount <= -15)
					{
						if(Q_irand(0, 10) > 7)
						{
							NPCInfo->vlookCount = 0;
						}
					}
					else if(NPCInfo->vlookCount == 0)
					{
						NPCInfo->vlookCount = Q_irand(-1, 1);
					}
					else if(Q_irand(0, 10) > 8)
					{
						if(NPCInfo->vlookCount > 0)
						{
							NPCInfo->vlookCount++;
						}
						else//lookCount < 0
						{
							NPCInfo->vlookCount--;
						}
					}

					//turn toward investigateGoal
					CalcEntitySpot( NPC, SPOT_HEAD, spot );
					VectorSubtract(NPCInfo->investigateGoal, spot, invDir);
					VectorNormalize(invDir);
					vectoangles(invDir, invAngles);
					NPCInfo->desiredYaw = AngleNormalize360(invAngles[YAW] + NPCInfo->hlookCount);
					NPCInfo->desiredPitch = AngleNormalize360(invAngles[PITCH] + NPCInfo->hlookCount);
				}

				NPC_UpdateAngles(qtrue, qtrue);

				NPCInfo->goalEntity = saveGoal;

				if(level.time > NPCInfo->investigateDebounceTime)
				{
					NPCInfo->tempBehavior = BS_DEFAULT;
				}

				NPC_CheckSoundEvents();
				*/
}

qboolean NPC_CheckInvestigate(const int alert_event_num)
{
	gentity_t* owner = level.alertEvents[alert_event_num].owner;
	const int inv_add = level.alertEvents[alert_event_num].level;
	vec3_t sound_pos;
	const float sound_rad = level.alertEvents[alert_event_num].radius;
	const float earshot = NPCInfo->stats.earshot;

	VectorCopy(level.alertEvents[alert_event_num].position, sound_pos);

	//NOTE: Trying to preserve previous investigation behavior
	if (!owner)
	{
		return qfalse;
	}

	if (owner->s.eType != ET_PLAYER && owner == NPCInfo->goalEntity)
	{
		return qfalse;
	}

	if (owner->s.eFlags & EF_NODRAW)
	{
		return qfalse;
	}

	if (owner->flags & FL_NOTARGET)
	{
		return qfalse;
	}

	if (sound_rad < earshot)
	{
		return qfalse;
	}

	if (!gi.inPVS(sound_pos, NPC->currentOrigin))
	{
		//can hear through doors?
		return qfalse;
	}

	if (owner->client && owner->client->playerTeam && NPC->client->playerTeam && owner->client->playerTeam != NPC->
		client->playerTeam)
	{
		if (static_cast<float>(NPCInfo->investigateCount) >= NPCInfo->stats.vigilance * 200 && owner)
		{
			//If investigateCount == 10, just take it as enemy and go
			if (NPC_ValidEnemy(owner))
			{
				//FIXME: run angerscript
				G_SetEnemy(NPC, owner);
				NPCInfo->goalEntity = NPC->enemy;
				NPCInfo->goalRadius = 12;
				NPCInfo->behaviorState = BS_HUNT_AND_KILL;
				return qtrue;
			}
		}
		else
		{
			NPCInfo->investigateCount += inv_add;
		}
		//run awakescript
		G_ActivateBehavior(NPC, BSET_AWAKE);

		//NPCInfo->hlookCount = NPCInfo->vlookCount = 0;
		NPCInfo->eventOwner = owner;
		VectorCopy(sound_pos, NPCInfo->investigateGoal);
		if (NPCInfo->investigateCount > 20)
		{
			NPCInfo->investigateDebounceTime = level.time + 10000;
		}
		else
		{
			NPCInfo->investigateDebounceTime = level.time + NPCInfo->investigateCount * 500;
		}
		NPCInfo->tempBehavior = BS_INVESTIGATE;
		return qtrue;
	}

	return qfalse;
}

/*
void NPC_BSSleep()
*/
void NPC_BSSleep()
{
	const int alert_event = NPC_CheckAlertEvents(qtrue, qfalse);

	//There is an event to look at
	if (alert_event >= 0)
	{
		G_ActivateBehavior(NPC, BSET_AWAKE);
	}
}

extern qboolean npc_move_dir_clear(int forwardmove, int rightmove, qboolean reset);

static bool NPC_BSFollowLeader_UpdateLeader()
{
	if (NPC->client->leader //have a leader
		&& NPC->client->leader->s.number < MAX_CLIENTS //player
		&& NPC->client->leader->client //player is a client
		&& !NPC->client->leader->client->pers.enterTime) //player has not finished spawning in yet
	{
		//don't do anything just yet, but don't clear the leader either
		return false;
	}

	if (NPC->client->leader && NPC->client->leader->health <= 0)
	{
		NPC->client->leader = nullptr;
	}

	if (!NPC->client->leader)
	{
		//ok, stand guard until we find an enemy
		if (NPCInfo->tempBehavior == BS_HUNT_AND_KILL)
		{
			NPCInfo->tempBehavior = BS_DEFAULT;
		}
		else
		{
			NPCInfo->tempBehavior = BS_STAND_GUARD;
			NPC_BSStandGuard();
		}
		if (NPCInfo->behaviorState == BS_FOLLOW_LEADER)
		{
			NPCInfo->behaviorState = BS_DEFAULT;
		}
		if (NPCInfo->defaultBehavior == BS_FOLLOW_LEADER)
		{
			NPCInfo->defaultBehavior = BS_DEFAULT;
		}
		return false;
	}
	return true;
}

static void NPC_BSFollowLeader_UpdateEnemy()
{
	if (!NPC->enemy)
	{
		//no enemy, find one
		NPC_CheckEnemy(static_cast<qboolean>(NPCInfo->confusionTime < level.time), qfalse);
		//don't find new enemy if this is tempbehav
		if (NPC->enemy)
		{
			//just found one
			NPCInfo->enemyCheckDebounceTime = level.time + Q_irand(3000, 10000);
		}
		else
		{
			if (!(NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
			{
				const int event_id = NPC_CheckAlertEvents(qtrue, qtrue);
				if (event_id > -1 && level.alertEvents[event_id].level >= AEL_SUSPICIOUS && NPCInfo->scriptFlags &
					SCF_LOOK_FOR_ENEMIES)
				{
					//NPCInfo->lastAlertID = level.alertEvents[eventID].ID;
					if (!level.alertEvents[event_id].owner ||
						!level.alertEvents[event_id].owner->client ||
						level.alertEvents[event_id].owner->health <= 0 ||
						level.alertEvents[event_id].owner->client->playerTeam != NPC->client->enemyTeam)
					{
						//not an enemy
					}
					else
					{
						//FIXME: what if can't actually see enemy, don't know where he is... should we make them just become very alert and start looking for him?  Or just let combat AI handle this... (act as if you lost him)
						G_SetEnemy(NPC, level.alertEvents[event_id].owner);
						NPCInfo->enemyCheckDebounceTime = level.time + Q_irand(3000, 10000);
						NPCInfo->enemyLastSeenTime = level.time;
						TIMER_Set(NPC, "attackDelay", Q_irand(500, 1000));
					}
				}
			}
		}
		if (!NPC->enemy)
		{
			if (NPC->client->leader
				&& NPC->client->leader->enemy
				&& NPC->client->leader->enemy != NPC
				&& (NPC->client->leader->enemy->client && NPC->client->leader->enemy->client->playerTeam == NPC->client
					->enemyTeam
					|| NPC->client->leader->enemy->svFlags & SVF_NONNPC_ENEMY && NPC->client->leader->enemy->
					noDamageTeam == NPC->client->enemyTeam)
				&& NPC->client->leader->enemy->health > 0)
			{
				G_SetEnemy(NPC, NPC->client->leader->enemy);
				NPCInfo->enemyCheckDebounceTime = level.time + Q_irand(3000, 10000);
				NPCInfo->enemyLastSeenTime = level.time;
			}
		}
	}
	else
	{
		if (NPC->enemy->health <= 0 || NPC->enemy->flags & FL_NOTARGET)
		{
			G_ClearEnemy(NPC);
			if (NPCInfo->enemyCheckDebounceTime > level.time + 1000)
			{
				NPCInfo->enemyCheckDebounceTime = level.time + Q_irand(1000, 2000);
			}
		}
		else if (NPC->client->ps.weapon && NPCInfo->enemyCheckDebounceTime < level.time)
		{
			NPC_CheckEnemy(
				static_cast<qboolean>(NPCInfo->confusionTime < level.time || NPCInfo->tempBehavior != BS_FOLLOW_LEADER),
				qfalse); //don't find new enemy if this is tempbehav
		}
	}
}

static bool NPC_BSFollowLeader_AttackEnemy()
{
	if (NPC->client->ps.weapon == WP_SABER) //|| NPCInfo->confusionTime>level.time )
	{
		//lightsaber user or charmed enemy
		if (NPCInfo->tempBehavior != BS_FOLLOW_LEADER)
		{
			//not already in a temp b_state
			//go after the guy
			NPCInfo->tempBehavior = BS_HUNT_AND_KILL;
			NPC_UpdateAngles(qtrue, qtrue);
			return true;
		}
	}

	enemyVisibility = NPC_CheckVisibility(NPC->enemy, CHECK_FOV | CHECK_SHOOT);

	if (enemyVisibility > VIS_PVS)
	{
		//face
		vec3_t enemy_org, muzzle, delta, angle_to_enemy;

		CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemy_org);
		NPC_AimWiggle(enemy_org);

		CalcEntitySpot(NPC, SPOT_WEAPON, muzzle);

		VectorSubtract(enemy_org, muzzle, delta);
		vectoangles(delta, angle_to_enemy);
		VectorNormalize(delta);

		NPCInfo->desiredYaw = angle_to_enemy[YAW];
		NPCInfo->desiredPitch = angle_to_enemy[PITCH];
		NPC_UpdateFiringAngles(qtrue, qtrue);

		if (enemyVisibility >= VIS_SHOOT)
		{
			//shoot
			NPC_AimAdjust(2);
			if (NPC_GetHFOVPercentage(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles,
				NPCInfo->stats.hfov) > 0.6f
				&& NPC_GetHFOVPercentage(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles,
					NPCInfo->stats.vfov) > 0.5f)
			{
				//actually withing our front cone
				WeaponThink();
			}
		}
		else
		{
			NPC_AimAdjust(1);
		}

		//NPC_CheckCanAttack(1.0, qfalse);
	}
	else
	{
		NPC_AimAdjust(-1);
	}
	return false;
}

static bool NPC_BSFollowLeader_CanAttack()
{
	return NPC->enemy
		&& NPC->client->ps.weapon
		&& !(NPCInfo->aiFlags & NPCAI_HEAL_ROSH) //Kothos twins never go after their enemy
		;
}

static bool NPC_BSFollowLeader_InFullBodyAttack()
{
	return NPC->client->ps.legsAnim == BOTH_ATTACK1 ||
		NPC->client->ps.legsAnim == BOTH_ATTACK2 ||
		NPC->client->ps.legsAnim == BOTH_ATTACK3 ||
		NPC->client->ps.legsAnim == BOTH_MELEE1 ||
		NPC->client->ps.legsAnim == BOTH_MELEE2;
}

static void NPC_BSFollowLeader_LookAtLeader()
{
	vec3_t head, leader_head, delta, angle_to_leader;

	CalcEntitySpot(NPC->client->leader, SPOT_HEAD, leader_head);
	CalcEntitySpot(NPC, SPOT_HEAD, head);
	VectorSubtract(leader_head, head, delta);
	vectoangles(delta, angle_to_leader);
	VectorNormalize(delta);
	NPC->NPC->desiredYaw = angle_to_leader[YAW];
	NPC->NPC->desiredPitch = angle_to_leader[PITCH];

	NPC_UpdateAngles(qtrue, qtrue);
}

void NPC_BSFollowLeader()
{
	// If In A Jump, Return
	//----------------------
	if (NPC_Jumping())
	{
		return;
	}

	// If There Is No Leader, Return
	//-------------------------------
	if (!NPC_BSFollowLeader_UpdateLeader())
	{
		return;
	}

	// Don't Do Anything Else If In A Full Body Attack
	//-------------------------------------------------
	if (NPC_BSFollowLeader_InFullBodyAttack())
	{
		return;
	}

	// Update The Enemy
	//------------------
	NPC_BSFollowLeader_UpdateEnemy();

	// Do Any Attacking
	//------------------
	if (NPC_BSFollowLeader_CanAttack())
	{
		if (NPC_BSFollowLeader_AttackEnemy())
		{
			return;
		}
	}
	else
	{
		NPC_BSFollowLeader_LookAtLeader();
	}

	const float follow_dist = NPCInfo->followDist ? NPCInfo->followDist : 110.0f;

	STEER::Activate(NPC);
	{
		if (NPC->client->leader->client && NPC->client->leader->client->ps.groundEntityNum != ENTITYNUM_NONE)
		{
			// If Too Close, Back Away Some
			//------------------------------
			if (STEER::Reached(NPC, NPC->client->leader, 65.0f))
			{
				STEER::Evade(NPC, NPC->client->leader);
			}
			else
			{
				// Attempt To Steer Directly To Our Goal
				//---------------------------------------
				bool move_success = STEER::GoTo(NPC, NPC->client->leader, follow_dist);

				// Perhaps Not Close Enough?  Try To Use The Navigation Grid
				//-----------------------------------------------------------
				if (!move_success)
				{
					move_success = NAV::GoTo(NPC, NPC->client->leader);
					if (!move_success)
					{
						STEER::Stop(NPC);
					}
				}
			}
		}
		else
		{
			STEER::Stop(NPC);
		}
	}
	STEER::DeActivate(NPC, &ucmd);
}

constexpr auto APEX_HEIGHT = 200.0f;
#define	PARA_WIDTH		(sqrt(APEX_HEIGHT)+sqrt(APEX_HEIGHT))
constexpr auto JUMP_SPEED = 200.0f;

void NPC_BSJump()
{
	vec3_t dir, p1, p2, apex;
	float time, height, forward, z, xy, dist, apexHeight;

	if (!NPCInfo->goalEntity)
	{
		//Should have task completed the navgoal
		return;
	}

	if (NPCInfo->jumpState != JS_JUMPING && NPCInfo->jumpState != JS_LANDING)
	{
		vec3_t angles;
		//Face navgoal
		VectorSubtract(NPCInfo->goalEntity->currentOrigin, NPC->currentOrigin, dir);
		vectoangles(dir, angles);
		NPCInfo->desiredPitch = NPCInfo->lockedDesiredPitch = AngleNormalize360(angles[PITCH]);
		NPCInfo->desiredYaw = NPCInfo->lockedDesiredYaw = AngleNormalize360(angles[YAW]);
	}

	NPC_UpdateAngles(qtrue, qtrue);
	const float yaw_error = AngleDelta(NPC->client->ps.viewangles[YAW], NPCInfo->desiredYaw);
	//We don't really care about pitch here

	switch (NPCInfo->jumpState)
	{
	case JS_FACING:
		if (yaw_error < MIN_ANGLE_ERROR)
		{
			//Facing it, Start crouching
			NPC_SetAnim(NPC, SETANIM_LEGS, BOTH_CROUCH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			NPCInfo->jumpState = JS_CROUCHING;
		}
		break;
	case JS_CROUCHING:
		if (NPC->client->ps.legsAnimTimer > 0)
		{
			//Still playing crouching anim
			return;
		}

		//Create a parabola

		if (NPC->currentOrigin[2] > NPCInfo->goalEntity->currentOrigin[2])
		{
			VectorCopy(NPC->currentOrigin, p1);
			VectorCopy(NPCInfo->goalEntity->currentOrigin, p2);
		}
		else if (NPC->currentOrigin[2] < NPCInfo->goalEntity->currentOrigin[2])
		{
			VectorCopy(NPCInfo->goalEntity->currentOrigin, p1);
			VectorCopy(NPC->currentOrigin, p2);
		}
		else
		{
			VectorCopy(NPC->currentOrigin, p1);
			VectorCopy(NPCInfo->goalEntity->currentOrigin, p2);
		}

		//z = xy*xy
		VectorSubtract(p2, p1, dir);
		dir[2] = 0;

		//Get xy and z diffs
		xy = VectorNormalize(dir);
		z = p1[2] - p2[2];

		apexHeight = APEX_HEIGHT / 2;

		//FIXME: length of xy will change curve of parabola, need to account for this
		//somewhere... PARA_WIDTH

		z = sqrt(apexHeight + z) - sqrt(apexHeight);

		assert(z >= 0);

		//		gi.Printf("apex is %4.2f percent from p1: ", (xy-z)*0.5/xy*100.0f);

		// Don't need to set apex xy if NPC is jumping directly up.
		if (xy > 0.0f)
		{
			xy -= z;
			xy *= 0.5;

			assert(xy > 0);
		}

		VectorMA(p1, xy, dir, apex);
		apex[2] += apexHeight;

		VectorCopy(apex, NPC->pos1);

		//Now we have the apex, aim for it
		height = apex[2] - NPC->currentOrigin[2];
		time = sqrt(height / (.5 * NPC->client->ps.gravity));
		if (!time)
		{
			//			gi.Printf("ERROR no time in jump\n");
			return;
		}

		// set s.origin2 to the push velocity
		VectorSubtract(apex, NPC->currentOrigin, NPC->client->ps.velocity);
		NPC->client->ps.velocity[2] = 0;
		dist = VectorNormalize(NPC->client->ps.velocity);

		forward = dist / time;
		VectorScale(NPC->client->ps.velocity, forward, NPC->client->ps.velocity);

		NPC->client->ps.velocity[2] = time * NPC->client->ps.gravity;

		//		gi.Printf( "%s jumping %s, gravity at %4.0f percent\n", NPC->targetname, vtos(NPC->client->ps.velocity), NPC->client->ps.gravity/8.0f );

		NPCInfo->jumpState = JS_JUMPING;
		//FIXME: jumpsound?
		break;
	case JS_JUMPING:

		if (showBBoxes)
		{
			VectorAdd(NPC->mins, NPC->pos1, p1);
			VectorAdd(NPC->maxs, NPC->pos1, p2);
			CG_Cube(p1, p2, NPCDEBUG_BLUE, 0.5);
		}

		if (NPC->s.groundEntityNum != ENTITYNUM_NONE)
		{
			//Landed, start landing anim
			//FIXME: if the
			VectorClear(NPC->client->ps.velocity);
			NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_LAND1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			NPCInfo->jumpState = JS_LANDING;
			//FIXME: landsound?
		}
		else if (NPC->client->ps.legsAnimTimer > 0)
		{
			//Still playing jumping anim
		}
		else
		{
			//still in air, but done with jump anim, play inair anim
			NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_INAIR1, SETANIM_FLAG_OVERRIDE);
		}
		break;
	case JS_LANDING:
	{
		if (NPC->client->ps.legsAnimTimer > 0)
		{
			//Still playing landing anim
			return;
		}
		NPCInfo->jumpState = JS_WAITING;

		NPCInfo->goalEntity = UpdateGoal();
		// If he made it to his goal or his task is no longer pending.
		if (!NPCInfo->goalEntity || !Q3_TaskIDPending(NPC, TID_MOVE_NAV))
		{
			NPC_ClearGoal();
			NPCInfo->goalTime = level.time;
			NPCInfo->aiFlags &= ~NPCAI_MOVING;
			ucmd.forwardmove = 0;
			NPC->flags &= ~FL_NO_KNOCKBACK;
			//Return that the goal was reached
			Q3_TaskIDComplete(NPC, TID_MOVE_NAV);
		}
	}
	break;
	case JS_WAITING:
	default:
		NPCInfo->jumpState = JS_FACING;
		break;
	}
}

void NPC_BSRemove()
{
	NPC_UpdateAngles(qtrue, qtrue);
	if (!gi.inPVS(NPC->currentOrigin, g_entities[0].currentOrigin)) //FIXME: use cg.vieworg?
	{
		G_UseTargets2(NPC, NPC, NPC->target3);
		NPC->s.eFlags |= EF_NODRAW;
		NPC->svFlags &= ~SVF_NPC;
		NPC->s.eType = ET_INVISIBLE;
		NPC->contents = 0;
		NPC->health = 0;
		NPC->targetname = nullptr;

		//Disappear in half a second
		NPC->e_ThinkFunc = thinkF_G_FreeEntity;
		NPC->nextthink = level.time + FRAMETIME;
	} //FIXME: else allow for out of FOV???
}

void NPC_BSSearch()
{
	NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER, qfalse);
	//FIXME: do something with these alerts...?
	//FIXME: do the Stormtrooper alert reaction?  (investigation)
	if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES
		&& NPC->client->enemyTeam != TEAM_NEUTRAL)
	{
		//look for enemies
		NPC_CheckEnemy(qtrue, qfalse);
		if (NPC->enemy)
		{
			//found one
			if (NPCInfo->tempBehavior == BS_SEARCH)
			{
				//if tempbehavior, set tempbehavior to default
				NPCInfo->tempBehavior = BS_DEFAULT;
			}
			else
			{
				//if b_state, change to run and shoot
				NPCInfo->behaviorState = BS_HUNT_AND_KILL;
				NPC_BSRunAndShoot();
			}
			return;
		}
	}

	//FIXME: what if our goalEntity is not NULL and NOT our tempGoal - they must
	//want us to do something else?  If tempBehavior, just default, else set
	//to run and shoot...?

	//FIXME: Reimplement

	if (!NPCInfo->investigateDebounceTime)
	{
		//On our way to a tempGoal
		float min_goal_reached_dist_squared = 32 * 32;
		vec3_t vec;

		//Keep moving toward our tempGoal
		NPCInfo->goalEntity = NPCInfo->tempGoal;

		VectorSubtract(NPCInfo->tempGoal->currentOrigin, NPC->currentOrigin, vec);
		if (vec[2] < 24)
		{
			vec[2] = 0;
		}

		if (NPCInfo->tempGoal->waypoint != WAYPOINT_NONE)
		{
			min_goal_reached_dist_squared = 32 * 32; //12*12;
		}

		if (VectorLengthSquared(vec) < min_goal_reached_dist_squared)
		{
			//Close enough, just got there
			NPC->waypoint = NAV::GetNearestNode(NPC);

			if (NPCInfo->homeWp == WAYPOINT_NONE || NPC->waypoint == WAYPOINT_NONE)
			{
				//Heading for or at an invalid waypoint, get out of this b_state
				if (NPCInfo->tempBehavior == BS_SEARCH)
				{
					//if tempbehavior, set tempbehavior to default
					NPCInfo->tempBehavior = BS_DEFAULT;
				}
				else
				{
					//if b_state, change to stand guard
					NPCInfo->behaviorState = BS_STAND_GUARD;
					NPC_BSRunAndShoot();
				}
				return;
			}

			if (NPC->waypoint == NPCInfo->homeWp)
			{
				//Just Reached our homeWp, if this is the first time, run your lostenemyscript
				if (NPCInfo->aiFlags & NPCAI_ENROUTE_TO_HOMEWP)
				{
					NPCInfo->aiFlags &= ~NPCAI_ENROUTE_TO_HOMEWP;
					G_ActivateBehavior(NPC, BSET_LOSTENEMY);
				}
			}

			//gi.Printf("Got there.\n");
			//gi.Printf("Looking...");
			if (!Q_irand(0, 1))
			{
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_GUARD_LOOKAROUND1, SETANIM_FLAG_NORMAL);
			}
			else
			{
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_GUARD_IDLE1, SETANIM_FLAG_NORMAL);
			}
			NPCInfo->investigateDebounceTime = level.time + Q_irand(3000, 10000);
		}
		else
		{
			NPC_MoveToGoal(qtrue);
		}
	}
	else
	{
		//We're there
		if (NPCInfo->investigateDebounceTime > level.time)
		{
			//Still waiting around for a bit
			//Turn angles every now and then to look around
			if (NPCInfo->tempGoal->waypoint != WAYPOINT_NONE)
			{
				if (!Q_irand(0, 30))
				{
					// NAV_TODO: What if there are no neighbors?
					vec3_t branch_pos, look_dir;

					NAV::GetNodePosition(NAV::ChooseRandomNeighbor(NPCInfo->tempGoal->waypoint), branch_pos);

					VectorSubtract(branch_pos, NPCInfo->tempGoal->currentOrigin, look_dir);
					NPCInfo->desiredYaw = AngleNormalize360(vectoyaw(look_dir) + Q_flrand(-45, 45));
				}
			}
			//gi.Printf(".");
		}
		else
		{
			//Just finished waiting
			NPC->waypoint = NAV::GetNearestNode(NPC);

			if (NPC->waypoint == NPCInfo->homeWp)
			{
				// NAV_TODO: What if there are no neighbors?

				const int next_wp = NAV::ChooseRandomNeighbor(NPCInfo->tempGoal->waypoint);
				NAV::GetNodePosition(next_wp, NPCInfo->tempGoal->currentOrigin);
				NPCInfo->tempGoal->waypoint = next_wp;
			}
			else
			{
				//At a branch, so return home
				NAV::GetNodePosition(NPCInfo->homeWp, NPCInfo->tempGoal->currentOrigin);
				NPCInfo->tempGoal->waypoint = NPCInfo->homeWp;
			}

			NPCInfo->investigateDebounceTime = 0;
			//Start moving toward our tempGoal
			NPCInfo->goalEntity = NPCInfo->tempGoal;
			NPC_MoveToGoal(qtrue);
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
-------------------------
NPC_BSSearchStart
-------------------------
*/

void NPC_BSSearchStart(const int home_wp, const bState_t b_state)
{
	//FIXME: Reimplement
	NPCInfo->homeWp = home_wp;
	NPCInfo->tempBehavior = b_state;
	NPCInfo->aiFlags |= NPCAI_ENROUTE_TO_HOMEWP;
	NPCInfo->investigateDebounceTime = 0;
	NAV::GetNodePosition(home_wp, NPCInfo->tempGoal->currentOrigin);
	NPCInfo->tempGoal->waypoint = home_wp;
	//gi.Printf("\nHeading for wp %d...\n", NPCInfo->homeWp);
}

/*
-------------------------
NPC_BSNoClip

  Use in extreme circumstances only
-------------------------
*/

void NPC_BSNoClip()
{
	if (UpdateGoal())
	{
		vec3_t dir, forward, right, angles;
		constexpr vec3_t up = { 0, 0, 1 };

		VectorSubtract(NPCInfo->goalEntity->currentOrigin, NPC->currentOrigin, dir);

		vectoangles(dir, angles);
		NPCInfo->desiredYaw = angles[YAW];

		AngleVectors(NPC->currentAngles, forward, right, nullptr);

		VectorNormalize(dir);

		const float f_dot = DotProduct(forward, dir) * 127;
		const float r_dot = DotProduct(right, dir) * 127;
		const float u_dot = DotProduct(up, dir) * 127;

		ucmd.forwardmove = floor(f_dot);
		ucmd.rightmove = floor(r_dot);
		ucmd.upmove = floor(u_dot);
	}
	else
	{
		//Cut velocity?
		VectorClear(NPC->client->ps.velocity);
	}

	NPC_UpdateAngles(qtrue, qtrue);
}

void NPC_BSWander()
{
	//FIXME: don't actually go all the way to the next waypoint, just move in fits and jerks...?
	NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER, qfalse);
	//FIXME: do something with these alerts...?
	//FIXME: do the Stormtrooper alert reaction?  (investigation)
	if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES && NPC->client->enemyTeam != TEAM_NEUTRAL)
	{
		//look for enemies
		NPC_CheckEnemy(qtrue, qfalse);
		if (NPC->enemy)
		{
			//found one
			if (NPCInfo->tempBehavior == BS_WANDER)
			{
				//if tempbehavior, set tempbehavior to default
				NPCInfo->tempBehavior = BS_DEFAULT;
			}
			else
			{
				//if b_state, change to run and shoot
				NPCInfo->behaviorState = BS_HUNT_AND_KILL;
				NPC_BSRunAndShoot();
			}
			return;
		}
	}

	STEER::Activate(NPC);

	// Are We Doing A Path?
	//----------------------
	bool has_path = NAV::HasPath(NPC);
	if (has_path)
	{
		has_path = NAV::UpdatePath(NPC);
		if (has_path)
		{
			STEER::Path(NPC); // Follow The Path
			STEER::AvoidCollisions(NPC);

			if (NPCInfo->aiFlags & NPCAI_BLOCKED && level.time - NPCInfo->blockedDebounceTime > 1000)
			{
				has_path = false; // find a new one
			}
		}
	}

	if (!has_path)
	{
		// If Debounce Time Has Expired, Choose A New Sub State
		//------------------------------------------------------
		if (NPCInfo->investigateDebounceTime < level.time ||
			NPCInfo->aiFlags & NPCAI_BLOCKED && level.time - NPCInfo->blockedDebounceTime > 1000)
		{
			// Clear Out Flags From The Previous Substate
			//--------------------------------------------
			NPCInfo->aiFlags &= ~NPCAI_OFF_PATH;
			NPCInfo->aiFlags &= ~NPCAI_WALKING;

			// Pick Another Spot
			//-------------------
			const int nextsubstate = Q_irand(0, 10);

			const bool random_path_node = nextsubstate < 9; //(NEXTSUBSTATE<4);

			// Random Path Node
			//------------------
			if (random_path_node)
			{
				// Sometimes, Walk
				//-----------------
				if (Q_irand(0, 1) == 0)
				{
					NPCInfo->aiFlags |= NPCAI_WALKING;
				}

				NPCInfo->investigateDebounceTime = level.time + Q_irand(3000, 10000);
				NAV::FindPath(NPC, NAV::ChooseRandomNeighbor(NAV::GetNearestNode(NPC)));
			}

			// Pathless Wandering
			//--------------------
			else
			{
				NPCInfo->investigateDebounceTime = level.time + Q_irand(2000, 10000);
				NPC_SetAnim(NPC, SETANIM_BOTH,
					Q_irand(0, 1) == 0 ? BOTH_GUARD_LOOKAROUND1 : BOTH_GUARD_IDLE1,
					SETANIM_FLAG_NORMAL);
			}
		}

		// Ok, So We Don't Have A Path, And Debounce Time Is Still Active, So We Are Either Wandering Or Looking Around
		//--------------------------------------------------------------------------------------------------------------
		else
		{
			if (NPCInfo->aiFlags & NPCAI_OFF_PATH)
			{
				STEER::Wander(NPC);
				STEER::AvoidCollisions(NPC);
			}
			else
			{
				STEER::Stop(NPC);
			}
		}
	}
	STEER::DeActivate(NPC, &ucmd);

	NPC_UpdateAngles(qtrue, qtrue);
}

/*
void NPC_BSFaceLeader (void)
{
	vec3_t	head, leaderHead, delta, angleToLeader;

	if ( !NPC->client->leader )
	{//uh.... okay.
		return;
	}

	CalcEntitySpot( NPC->client->leader, SPOT_HEAD, leaderHead );
	CalcEntitySpot( NPC, SPOT_HEAD, head );
	VectorSubtract( leaderHead, head, delta );
	vectoangles( delta, angleToLeader );
	VectorNormalize( delta );
	NPC->NPC->desiredYaw = angleToLeader[YAW];
	NPC->NPC->desiredPitch = angleToLeader[PITCH];

	NPC_UpdateAngles(qtrue, qtrue);
}
*/
/*
-------------------------
NPC_BSFlee
-------------------------
*/
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern void WP_DropWeapon(gentity_t* dropper, vec3_t velocity);
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern int g_crosshairEntNum;

static qboolean NPC_CanSurrender()
{
	if (NPC->client)
	{
		switch (NPC->client->NPC_class)
		{
		case CLASS_ATST:
		case CLASS_CLAW:
		case CLASS_DESANN:
		case CLASS_VADER:
		case CLASS_SITHLORD:
		case CLASS_GROGU:
		case CLASS_FISH:
		case CLASS_FLIER2:
		case CLASS_GALAK:
		case CLASS_GLIDER:
		case CLASS_GONK: // droid
		case CLASS_HOWLER:
		case CLASS_RANCOR:
		case CLASS_SAND_CREATURE:
		case CLASS_WAMPA:
		case CLASS_INTERROGATOR: // droid
		case CLASS_JAN:
		case CLASS_JEDI:
		case CLASS_AHSOKA:
		case CLASS_KYLE:
		case CLASS_LANDO:
		case CLASS_LIZARD:
		case CLASS_LUKE:
		case CLASS_MARK1: // droid
		case CLASS_MARK2: // droid
		case CLASS_GALAKMECH: // droid
		case CLASS_MINEMONSTER:
		case CLASS_MONMOTHA:
		case CLASS_MORGANKATARN:
		case CLASS_MOUSE: // droid
		case CLASS_MURJJ:
		case CLASS_PROBE: // droid
		case CLASS_PROTOCOL: // droid
		case CLASS_R2D2: // droid
		case CLASS_R5D2: // droid
		case CLASS_REBORN:
		case CLASS_REELO:
		case CLASS_REMOTE:
		case CLASS_SEEKER: // droid
		case CLASS_SENTRY:
		case CLASS_SHADOWTROOPER:
		case CLASS_SWAMP:
		case CLASS_TAVION:
		case CLASS_ALORA:
		case CLASS_TUSKEN:
		case CLASS_BOBAFETT:
		case CLASS_MANDALORIAN:
		case CLASS_JANGO:
		case CLASS_JANGODUAL:
		case CLASS_ROCKETTROOPER:
		case CLASS_SABER_DROID:
		case CLASS_ASSASSIN_DROID:
		case CLASS_DROIDEKA:
		case CLASS_HAZARD_TROOPER:
		case CLASS_PLAYER:
		case CLASS_PROJECTION:
		case CLASS_VEHICLE:
		case CLASS_GALEN:
		case CLASS_GUARD:
		case CLASS_YODA:
		case CLASS_BOC:
		case CLASS_JEDIMASTER:
		case CLASS_CALONORD:
			return qfalse;
		default:
			break;
		}
		if (!g_standard_humanoid(NPC))
		{
			return qfalse;
		}
		if (NPC->client->ps.weapon == WP_SABER)
		{
			return qfalse;
		}
	}
	if (NPCInfo)
	{
		if (NPCInfo->aiFlags & NPCAI_BOSS_SERENITYJEDIENGINE)
		{
			return qfalse;
		}
		if (NPCInfo->aiFlags & NPCAI_BOSS_CHARACTER)
		{
			return qfalse;
		}
		if (NPCInfo->aiFlags & NPCAI_SUBBOSS_CHARACTER)
		{
			return qfalse;
		}
		if (NPCInfo->aiFlags & NPCAI_ROSH)
		{
			return qfalse;
		}
		if (NPCInfo->aiFlags & NPCAI_HEAL_ROSH)
		{
			return qfalse;
		}
	}
	return qtrue;
}

static void NPC_Surrender()
{
	//FIXME: say "don't shoot!" if we weren't already surrendering
	if (NPC->client->ps.weaponTime || PM_InKnockDown(&NPC->client->ps))
	{
		return;
	}
	if (!NPC_CanSurrender())
	{
		return;
	}
	if (NPC->s.weapon != WP_NONE &&
		NPC->s.weapon != WP_MELEE &&
		NPC->s.weapon != WP_SABER)
	{
		WP_DropWeapon(NPC, nullptr);
	}
	if (NPCInfo->surrenderTime < level.time - 5000)
	{
		//haven't surrendered for at least 6 seconds, tell them what you're doing
		//FIXME: need real dialogue EV_SURRENDER
		NPCInfo->blockedSpeechDebounceTime = 0; //make sure we say this
		G_AddVoiceEvent(NPC, Q_irand(EV_PUSHED1, EV_PUSHED3), 3000);
	}

	// Already Surrendering?  If So, Just Update Animations
	//------------------------------------------------------
	if (NPCInfo->surrenderTime > level.time)
	{
		if (NPC->client->ps.torsoAnim == BOTH_COWER1_START && NPC->client->ps.torsoAnimTimer <= 100)
		{
			NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_COWER1, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
			NPCInfo->surrenderTime = level.time + NPC->client->ps.torsoAnimTimer;
		}
		if (NPC->client->ps.torsoAnim == BOTH_COWER1 && NPC->client->ps.torsoAnimTimer <= 100)
		{
			NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_COWER1_STOP, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
			NPCInfo->surrenderTime = level.time + NPC->client->ps.torsoAnimTimer;
		}
	}

	// New To The Surrender, So Start The Animation
	//----------------------------------------------
	else
	{
		if (NPC->client->NPC_class == CLASS_JAWA && NPC->client->ps.weapon == WP_NONE)
		{
			//an unarmed Jawa is very scared
			NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_COWER1, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
			//FIXME: stop doing this if decide to take off and run
		}
		else
		{
			// A Big Monster?  OR: Being Tracked By A Homing Rocket?  So Do The Cower Sequence
			//------------------------------------------
			if (NPC->enemy && NPC->enemy->client && NPC->enemy->client->NPC_class == CLASS_RANCOR || !TIMER_Done(
				NPC, "rocketChasing"))
			{
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_COWER1_START, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
			}

			// Otherwise, Use The Old Surrender "Arms In Air" Animation
			//----------------------------------------------------------
			else
			{
				NPC_SetAnim(NPC, SETANIM_TORSO, TORSO_SURRENDER_START, SETANIM_FLAG_HOLD | SETANIM_FLAG_OVERRIDE);
				NPC->client->ps.torsoAnimTimer = Q_irand(3000, 8000); // Pretend the anim lasts longer
			}
		}
		NPCInfo->surrenderTime = level.time + NPC->client->ps.torsoAnimTimer + 1000;
	}
}

qboolean NPC_CheckSurrender()
{
	if (!g_AIsurrender->integer
		&& NPC->client->NPC_class != CLASS_UGNAUGHT
		&& NPC->client->NPC_class != CLASS_JAWA)
	{
		//not enabled
		return qfalse;
	}
	if (!Q3_TaskIDPending(NPC, TID_MOVE_NAV) //not scripted to go somewhere
		&& NPC->client->ps.groundEntityNum != ENTITYNUM_NONE //not in the air
		&& !NPC->client->ps.weaponTime && !PM_InKnockDown(&NPC->client->ps) //not firing and not on the ground
		&& NPC->enemy && NPC->enemy->client && NPC->enemy->enemy == NPC && NPC->enemy->s.weapon != WP_NONE && (NPC->
			enemy->s.weapon != WP_MELEE || (NPC->enemy->client->NPC_class == CLASS_RANCOR || NPC->enemy->client->
				NPC_class == CLASS_WAMPA)) //enemy is using a weapon or is a Rancor or Wampa
		&& NPC->enemy->health > 20 && NPC->enemy->painDebounceTime < level.time - 3000 && NPC->enemy->client->ps.
		forcePowerDebounce[FP_SABER_DEFENSE] < level.time - 1000)
	{
		//don't surrender if scripted to run somewhere or if we're in the air or if we're busy or if we don't have an enemy or if the enemy is not mad at me or is hurt or not a threat or busy being attacked
		//FIXME: even if not in a group, don't surrender if there are other enemies in the PVS and within a certain range?
		if (NPC->s.weapon != WP_ROCKET_LAUNCHER
			&& NPC->s.weapon != WP_CONCUSSION
			&& NPC->s.weapon != WP_REPEATER
			&& NPC->s.weapon != WP_FLECHETTE
			&& NPC->s.weapon != WP_SABER)
		{
			//jedi and heavy weapons guys never surrender
			//FIXME: rework all this logic into some orderly fashion!!!
			if (NPC->s.weapon != WP_NONE)
			{
				//they have a weapon so they'd have to drop it to surrender
				//don't give up unless low on health
				if (NPC->health > 25 || NPC->health >= NPC->max_health)
				{
					return qfalse;
				}
				if (g_crosshairEntNum == NPC->s.number && NPC->painDebounceTime > level.time)
				{
					//if he just shot me, always give up
					//fall through
				}
				else
				{
					//don't give up unless facing enemy and he's very close
					if (!InFOV(player, NPC, 60, 30))
					{
						//I'm not looking at them
						return qfalse;
					}
					if (DistanceSquared(NPC->currentOrigin, player->currentOrigin) < 65536/*256*256*/)
					{
						//they're not close
						return qfalse;
					}
					if (!gi.inPVS(NPC->currentOrigin, player->currentOrigin))
					{
						//they're not in the same room
						return qfalse;
					}
				}
			}
			if (!NPCInfo->group || NPCInfo->group && NPCInfo->group->numGroup <= 1)
			{
				//I'm alone but I was in a group//FIXME: surrender anyway if just melee or no weap?
				if (NPC->s.weapon == WP_NONE
					//NPC has a weapon
					|| NPC->enemy == player
					|| NPC->enemy->s.weapon == WP_SABER && NPC->enemy->client && NPC->enemy->client->ps.
					SaberActive()
					|| NPC->enemy->NPC && NPC->enemy->NPC->group && NPC->enemy->NPC->group->numGroup > 2)
				{
					//surrender only if have no weapon or fighting a player or jedi or if we are outnumbered at least 3 to 1
					if (NPC->enemy == player)
					{
						//player is the guy I'm running from
						if (g_crosshairEntNum == NPC->s.number)
						{
							//give up if player is aiming at me
							NPC_Surrender();
							NPC_UpdateAngles(qtrue, qtrue);
							return qtrue;
						}
						if (player->s.weapon == WP_SABER)
						{
							//player is using saber
							if (InFOV(NPC, player, 60, 30))
							{
								//they're looking at me
								if (DistanceSquared(NPC->currentOrigin, player->currentOrigin) < 16384/*128*128*/)
								{
									//they're close
									if (gi.inPVS(NPC->currentOrigin, player->currentOrigin))
									{
										//they're in the same room
										NPC_Surrender();
										NPC_UpdateAngles(qtrue, qtrue);
										return qtrue;
									}
								}
							}
						}
					}
					else if (NPC->enemy)
					{
						//???
						//should NPC's surrender to others?
						if (InFOV(NPC, NPC->enemy, 30, 30))
						{
							//they're looking at me
							float max_dist = 64 + NPC->maxs[0] * 1.5 + NPC->enemy->maxs[0] * 1.5;
							max_dist *= max_dist;
							if (DistanceSquared(NPC->currentOrigin, NPC->enemy->currentOrigin) < max_dist)
							{
								//they're close
								if (gi.inPVS(NPC->currentOrigin, NPC->enemy->currentOrigin))
								{
									//they're in the same room
									//FIXME: should player-team NPCs not fire on surrendered NPCs?
									NPC_Surrender();
									NPC_UpdateAngles(qtrue, qtrue);
									return qtrue;
								}
							}
						}
					}
				}
			}
		}
	}
	return qfalse;
}

static void NPC_JawaFleeSound()
{
	if (NPC
		&& NPC->client
		&& NPC->client->NPC_class == CLASS_JAWA
		&& !Q_irand(0, 3)
		&& NPCInfo->blockedSpeechDebounceTime < level.time
		&& !Q3_TaskIDPending(NPC, TID_CHAN_VOICE))
	{
		//ooteenee!!!!
		//Com_Printf( "ooteenee!!!!\n" );
		G_SoundOnEnt(NPC, CHAN_VOICE, "sound/chars/jawa/misc/ooh-tee-nee.wav");
		NPCInfo->blockedSpeechDebounceTime = level.time + 2000;
	}
}

extern gentity_t* NPC_SearchForWeapons();
extern qboolean G_CanPickUpWeapons(const gentity_t* other);

qboolean NPC_BSFlee()
{
	float enemy_too_close_dist = 50.0f;
	bool reached_escape_point = false;
	const bool in_surrender = level.time < NPCInfo->surrenderTime;

	// Check For Enemies And Alert Events
	//------------------------------------
	NPC_CheckEnemy(qtrue, qfalse);
	NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER, qfalse);
	if (NPC->enemy && G_ClearLOS(NPC, NPC->enemy))
	{
		NPCInfo->enemyLastSeenTime = level.time;
	}
	const bool enemy_recently_seen = NPC->enemy && level.time - NPCInfo->enemyLastSeenTime < 3000;
	if (enemy_recently_seen)
	{
		if (NPC->enemy->client && NPC->enemy->client->NPC_class == CLASS_RANCOR)
		{
			enemy_too_close_dist = 400.0f;
		}
		enemy_too_close_dist += NPC->maxs[0] + NPC->enemy->maxs[0];
	}

	// Look For Weapons To Pick Up
	//-----------------------------
	if (enemy_recently_seen && // Is There An Enemy Near?
		NPC->client->NPC_class != CLASS_PRISONER && // Prisoners can't pickup weapons
		NPCInfo->rank > RANK_CIVILIAN && // Neither can civilians
		TIMER_Done(NPC, "panic") && // Panic causes him to run for a bit, don't pickup weapons
		TIMER_Done(NPC, "CheckForWeaponToPickup") &&
		G_CanPickUpWeapons(NPC) //Allowed To Pick Up Dropped Weapons
		)
	{
		gentity_t* found_weap = NPC_SearchForWeapons();

		// Ok, There Is A Weapon!  Try Going To It!
		//------------------------------------------
		if (found_weap && NAV::SafePathExists(NPC->currentOrigin, found_weap->currentOrigin,
			NPC->enemy->currentOrigin, 150.0f))
		{
			NAV::ClearPath(NPC); // Remove Any Old Path

			NPCInfo->goalEntity = found_weap; // Change Our Target Goal
			NPCInfo->goalRadius = 30.0f; // 30 good enough?

			TIMER_Set(NPC, "CheckForWeaponToPickup", Q_irand(1000, 5000) * 10);
		}

		// Look Again Soon
		//-----------------
		else
		{
			TIMER_Set(NPC, "CheckForWeaponToPickup", Q_irand(1000, 5000));
		}
	}

	// If Attempting To Get To An Entity That Is Gone, Clear The Pointer
	//-------------------------------------------------------------------
	if (NPCInfo->goalEntity
		&& !Q3_TaskIDPending(NPC, TID_MOVE_NAV)
		&& NPC->enemy
		&& Distance(NPCInfo->goalEntity->currentOrigin, NPC->enemy->currentOrigin) < enemy_too_close_dist)
	{
		//our goal is too close to our enemy, dump it...
		NPCInfo->goalEntity = nullptr;
	}
	if (NPCInfo->goalEntity && !NPCInfo->goalEntity->inuse)
	{
		NPCInfo->goalEntity = nullptr;
	}
	const bool has_escape_point = NPCInfo->goalEntity && NPCInfo->goalRadius != 0.0f;

	STEER::Activate(NPC);
	{
		// Have We Reached The Escape Point?
		//-----------------------------------
		if (has_escape_point && STEER::Reached(NPC, NPCInfo->goalEntity, NPCInfo->goalRadius, false))
		{
			if (Q3_TaskIDPending(NPC, TID_MOVE_NAV))
			{
				Q3_TaskIDComplete(NPC, TID_MOVE_NAV);
			}
			reached_escape_point = true;
		}

		// If Super Close To The Enemy, Run In The Other Direction
		//---------------------------------------------------------
		if (enemy_recently_seen &&
			Distance(NPC->enemy->currentOrigin, NPC->currentOrigin) < enemy_too_close_dist)
		{
			STEER::Evade(NPC, NPC->enemy);
			STEER::AvoidCollisions(NPC);
		}

		// If Already At The Escape Point, Or Surrendering, Don't Move
		//-------------------------------------------------------------
		else if (reached_escape_point || in_surrender)
		{
			STEER::Stop(NPC);
		}
		else
		{
			bool move_success = false;
			// Try To Get To The Escape Point
			//--------------------------------
			if (has_escape_point)
			{
				move_success = STEER::GoTo(NPC, NPCInfo->goalEntity, true);
				if (!move_success)
				{
					move_success = NAV::GoTo(NPC, NPCInfo->goalEntity, 0.3f);
				}
			}

			// Cant Get To The Escape Point, So If There Is An Enemy
			//-------------------------------------------------------
			if (!move_success && enemy_recently_seen)
			{
				// Try To Get To The Farthest Combat Point From Him
				//--------------------------------------------------
				const NAV::TNodeHandle nbr = NAV::ChooseFarthestNeighbor(NPC, NPC->enemy->currentOrigin, 0.25f);
				if (nbr > 0)
				{
					move_success = STEER::GoTo(NPC, NAV::GetNodePosition(nbr), true);
					if (!move_success)
					{
						move_success = NAV::GoTo(NPC, nbr, 0.3f);
					}
				}
			}

			// If We Still Can't (Or Don't Need To) Move, Just Stop
			//------------------------------------------------------
			if (!move_success)
			{
				STEER::Stop(NPC);
			}
		}
	}
	STEER::DeActivate(NPC, &ucmd);

	// Is There An Enemy Around?
	//---------------------------
	if (enemy_recently_seen)
	{
		// Time To Surrender?
		//--------------------
		if (TIMER_Done(NPC, "panic"))
		{
			//done panicking, time to realize we're dogmeat, if we haven't been able to flee for a few seconds
			if (level.time - NPC->lastMoveTime > 3000
				&& level.time - NPCInfo->surrenderTime > 3000) //and haven't just finished surrendering
			{
				NPC_FaceEnemy();
				NPC_Surrender();
			}
		}

		// Time To Choose A New Escape Point?
		//------------------------------------
		if ((!has_escape_point || reached_escape_point) && TIMER_Done(NPC, "FindNewEscapePointDebounce"))
		{
			TIMER_Set(NPC, "FindNewEscapePointDebounce", 2500);

			const int escape_point = NPC_FindCombatPoint(
				NPC->currentOrigin,
				NPC->enemy->currentOrigin,
				NPC->currentOrigin,
				CP_COVER | CP_AVOID_ENEMY | CP_HAS_ROUTE,
				128);
			if (escape_point != -1)
			{
				NPC_JawaFleeSound();
				NPC_SetCombatPoint(escape_point);
				NPC_SetMoveGoal(NPC, level.combatPoints[escape_point].origin, 8, qtrue, escape_point);
			}
		}
	}

	// If Only Temporarly In Flee, Think About Perhaps Returning To Combat
	//---------------------------------------------------------------------
	if (NPCInfo->tempBehavior == BS_FLEE &&
		TIMER_Done(NPC, "flee") &&
		NPC->s.weapon != WP_NONE &&
		NPC->s.weapon != WP_MELEE
		)
	{
		NPCInfo->tempBehavior = BS_DEFAULT;
	}

	// Always Update Angles
	//----------------------
	NPC_UpdateAngles(qtrue, qtrue);
	if (reached_escape_point)
	{
		return qtrue;
	}
	return qfalse;
}

void NPC_StartFlee(gentity_t* enemy, vec3_t danger_point, const int danger_level, const int flee_time_min,
	const int flee_time_max)
{
	if (Q3_TaskIDPending(NPC, TID_MOVE_NAV))
	{
		//running somewhere that a script requires us to go, don't interrupt that!
		return;
	}

	if (NPCInfo->scriptFlags & SCF_DONT_FLEE) // no flee for you
	{
		return;
	}

	//if have a fleescript, run that instead
	if (G_ActivateBehavior(NPC, BSET_FLEE))
	{
		return;
	}

	if (enemy)
	{
		NPC_JawaFleeSound();
		G_SetEnemy(NPC, enemy);
	}

	//FIXME: if don't have a weapon, find nearest one we have a route to and run for it?
	int cp = -1;
	if (danger_level > AEL_DANGER || NPC->s.weapon == WP_NONE || (!NPCInfo->group || NPCInfo->group->numGroup <= 1)
		&& NPC->health <= 10)
	{
		//IF either great danger OR I have no weapon OR I'm alone and low on health, THEN try to find a combat point out of PVS
		cp = NPC_FindCombatPoint(NPC->currentOrigin, danger_point, NPC->currentOrigin,
			CP_COVER | CP_AVOID | CP_HAS_ROUTE | CP_NO_PVS, 128);
	}
	//FIXME: still happens too often...
	if (cp == -1)
	{
		//okay give up on the no PVS thing
		cp = NPC_FindCombatPoint(NPC->currentOrigin, danger_point, NPC->currentOrigin,
			CP_COVER | CP_AVOID | CP_HAS_ROUTE, 128);
		if (cp == -1)
		{
			//okay give up on the avoid
			cp = NPC_FindCombatPoint(NPC->currentOrigin, danger_point, NPC->currentOrigin, CP_COVER | CP_HAS_ROUTE,
				128);
			if (cp == -1)
			{
				//okay give up on the cover
				cp = NPC_FindCombatPoint(NPC->currentOrigin, danger_point, NPC->currentOrigin, CP_HAS_ROUTE, 128);
			}
		}
	}

	//see if we got a valid one
	if (cp != -1)
	{
		//found a combat point
		NPC_SetCombatPoint(cp);
		NPC_SetMoveGoal(NPC, level.combatPoints[cp].origin, 8, qtrue, cp);
		NPCInfo->behaviorState = BS_HUNT_AND_KILL;
		NPCInfo->tempBehavior = BS_DEFAULT;
	}
	else
	{
		//need to just run like hell!
		if (NPC->s.weapon != WP_NONE)
		{
			return; //let's just not flee?
		}
		//FIXME: other evasion AI?  Duck?  Strafe?  Dodge?
		NPCInfo->tempBehavior = BS_FLEE;
		//Run straight away from here... FIXME: really want to find farthest waypoint/navgoal from this pos... maybe based on alert event radius?
		NPC_SetMoveGoal(NPC, danger_point, 0, qtrue);
		//store the danger point
		VectorCopy(danger_point, NPCInfo->investigateGoal); //FIXME: make a new field for this?
	}

	if (danger_level > AEL_DANGER //geat danger always makes people turn and run
		|| NPC->s.weapon == WP_NONE //melee/unarmed guys turn and run, others keep facing you and shooting
		|| NPC->s.weapon == WP_MELEE
		|| NPC->s.weapon == WP_TUSKEN_STAFF)
	{
		NPCInfo->tempBehavior = BS_FLEE; //we don't want to do this forever!
	}

	//FIXME: localize this Timer?
	TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
	//FIXME: is this always applicable?
	NPCInfo->squadState = SQUAD_RETREAT;
	TIMER_Set(NPC, "flee", Q_irand(flee_time_min, flee_time_max));
	TIMER_Set(NPC, "panic", Q_irand(1000, 4000)); //how long to wait before trying to nav to a dropped weapon
	TIMER_Set(NPC, "duck", 0);
}

void G_StartFlee(gentity_t* self, gentity_t* enemy, vec3_t danger_point, const int danger_level,
	const int flee_time_min,
	const int flee_time_max)
{
	if (!self->NPC)
	{
		//player
		return;
	}
	SaveNPCGlobals();
	SetNPCGlobals(self);

	NPC_StartFlee(enemy, danger_point, danger_level, flee_time_min, flee_time_max);

	RestoreNPCGlobals();
}

void NPC_BSEmplaced()
{
	//Don't do anything if we're hurt
	if (NPC->painDebounceTime > level.time)
	{
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}

	//If we don't have an enemy, just idle
	if (NPC_CheckEnemyExt() == qfalse)
	{
		if (!Q_irand(0, 30))
		{
			NPCInfo->desiredYaw = NPC->s.angles[1] + Q_irand(-90, 90);
		}
		if (!Q_irand(0, 30))
		{
			NPCInfo->desiredPitch = Q_irand(-20, 20);
		}
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	qboolean enemy_los = qfalse;
	qboolean enemy_cs = qfalse;
	qboolean face_enemy = qfalse;
	qboolean shoot = qfalse;

	if (NPC_ClearLOS(NPC->enemy))
	{
		vec3_t impact_pos;
		enemy_los = qtrue;

		const int hit = NPC_ShotEntity(NPC->enemy, impact_pos);
		const gentity_t* hit_ent = &g_entities[hit];

		if (hit == NPC->enemy->s.number || hit_ent && hit_ent->takedamage)
		{
			//can hit enemy or will hit glass or other minor breakable (or in emplaced gun), so shoot anyway
			enemy_cs = qtrue;
			NPC_AimAdjust(2); //adjust aim better longer we have clear shot at enemy
			VectorCopy(NPC->enemy->currentOrigin, NPCInfo->enemyLastSeenLocation);
		}
	}

	if (enemy_los)
	{
		//FIXME: no need to face enemy if we're moving to some other goal and he's too far away to shoot?
		face_enemy = qtrue;
	}
	if (enemy_cs)
	{
		shoot = qtrue;
	}

	if (face_enemy)
	{
		//face the enemy
		NPC_FaceEnemy(qtrue);
	}
	else
	{
		//we want to face in the dir we're running
		NPC_UpdateAngles(qtrue, qtrue);
	}

	if (NPCInfo->scriptFlags & SCF_DONT_FIRE)
	{
		shoot = qfalse;
	}

	if (NPC->enemy && NPC->enemy->enemy)
	{
		if (NPC->enemy->s.weapon == WP_SABER && NPC->enemy->enemy->s.weapon == WP_SABER)
		{
			//don't shoot at an enemy jedi who is fighting another jedi, for fear of injuring one or causing rogue blaster deflections (a la Obi Wan/Vader duel at end of ANH)
			shoot = qfalse;
		}
	}
	if (shoot)
	{
		//try to shoot if it's time
		if (!(NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
		{
			WeaponThink();
		}
	}
}