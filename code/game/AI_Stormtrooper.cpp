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
#include "g_nav.h"
#include "anims.h"
#include "g_navigator.h"
#include "../cgame/cg_local.h"
#include "g_functions.h"
#include <cgame\cg_camera.h>

extern void CG_DrawAlert(vec3_t origin, float rating);
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern void AI_GroupUpdateSquadstates(AIGroupInfo_t* group, const gentity_t* member, int new_squad_state);
extern qboolean AI_GroupContainsEntNum(const AIGroupInfo_t* group, int entNum);
extern void AI_GroupUpdateEnemyLastSeen(AIGroupInfo_t* group, vec3_t spot);
extern void AI_GroupUpdateClearShotTime(AIGroupInfo_t* group);
extern void NPC_TempLookTarget(const gentity_t* self, int lookEntNum, int minLookTime, int maxLookTime);
extern qboolean G_ExpandPointToBBox(vec3_t point, const vec3_t mins, const vec3_t maxs, int ignore, int clipmask);
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern void NPC_CheckGetNewWeapon();
extern qboolean Q3_TaskIDPending(const gentity_t* ent, taskID_t taskType);
extern int GetTime(int lastTime);
extern void NPC_AimAdjust(int change);
extern qboolean FlyingCreature(const gentity_t* ent);
extern void npc_evasion_saber();
extern qboolean RT_Flying(const gentity_t* self);
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern qboolean InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern qboolean PM_CrouchAnim(int anim);
extern void npc_check_evasion();
extern cvar_t* g_SerenityJediEngineMode;
extern cvar_t* g_allowgunnerbash;
extern qboolean char_can_gun_bash(const gentity_t* self);
extern qboolean WP_AbsorbKick(gentity_t* hit_ent, const gentity_t* pusher, const vec3_t push_dir);
extern void speaker_speech(const gentity_t* self, int speech_type, float fail_chance);

extern cvar_t* d_asynchronousGroupAI;
extern void npc_check_speak(gentity_t* speaker_npc);

constexpr auto MAX_VIEW_DIST = 1024;
constexpr auto MAX_VIEW_SPEED = 250;
constexpr auto MAX_LIGHT_INTENSITY = 255;
constexpr auto MIN_LIGHT_THRESHOLD = 0.1;
constexpr auto ST_MIN_LIGHT_THRESHOLD = 30;
constexpr auto ST_MAX_LIGHT_THRESHOLD = 180;
constexpr auto DISTANCE_THRESHOLD = 0.075f;
constexpr auto MIN_TURN_AROUND_DIST_SQ = 10000;
//(100 squared) don't stop running backwards if your goal is less than 100 away;
constexpr auto SABER_AVOID_DIST = 128.0f; //256.0f;
#define SABER_AVOID_DIST_SQ (SABER_AVOID_DIST*SABER_AVOID_DIST)

constexpr auto DISTANCE_SCALE = 0.35f; //These first three get your base detection rating, ideally add up to 1;
constexpr auto FOV_SCALE = 0.40f; //;
constexpr auto LIGHT_SCALE = 0.25f; //;

constexpr auto SPEED_SCALE = 0.25f; //These next two are bonuses;
constexpr auto TURNING_SCALE = 0.25f; //;

constexpr auto REALIZE_THRESHOLD = 0.6f;
#define CAUTIOUS_THRESHOLD	( REALIZE_THRESHOLD * 0.75 )

qboolean NPC_CheckPlayerTeamStealth();

static qboolean enemy_los;
static qboolean enemy_cs;
static qboolean enemyInFOV;
static qboolean hitAlly;
static qboolean face_enemy;
static qboolean do_move;
static qboolean shoot;
static float enemyDist;
static vec3_t impactPos;

int groupSpeechDebounceTime[TEAM_NUM_TEAMS]; //used to stop several group AI from speaking all at once

void NPC_Saboteur_Precache()
{
	G_SoundIndex("sound/chars/shadowtrooper/cloak.wav");
	G_SoundIndex("sound/chars/shadowtrooper/decloak.wav");
}

void Saboteur_Decloak(gentity_t* self, const int uncloak_time)
{
	if (self && self->client)
	{
		if (self->client->ps.powerups[PW_CLOAKED] && TIMER_Done(self, "decloakwait"))
		{
			//Uncloak
			self->client->ps.powerups[PW_CLOAKED] = 0;
			self->client->ps.powerups[PW_UNCLOAKING] = level.time + 2000;
			G_SoundOnEnt(self, CHAN_ITEM, "sound/chars/shadowtrooper/decloak.wav");
			TIMER_Set(self, "nocloak", uncloak_time);
			NPC_SetAnim(self, SETANIM_TORSO, BOTH_FORCE_DRAIN_RELEASE, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}
}

void Saboteur_Cloak(gentity_t* self)
{
	if (self && self->client && self->NPC)
	{
		if (in_camera) // Cinematic
		{
			Saboteur_Decloak(self);
		}
		if (TIMER_Done(self, "nocloak"))
		{
			//not sitting around waiting to cloak again
			if (!(self->NPC->aiFlags & NPCAI_SHIELDS))
			{
				//not allowed to cloak, actually
				Saboteur_Decloak(self);
			}
			else if (!self->client->ps.powerups[PW_CLOAKED])
			{
				//cloak
				self->client->ps.powerups[PW_CLOAKED] = Q3_INFINITE;
				self->client->ps.powerups[PW_UNCLOAKING] = level.time + 2000;
				G_SoundOnEnt(self, CHAN_ITEM, "sound/chars/shadowtrooper/cloak.wav");
				NPC_SetAnim(self, SETANIM_TORSO, BOTH_FORCE_PROTECT_FAST, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
	}
}

//Local state enums
enum
{
	LSTATE_NONE = 0,
	LSTATE_UNDERFIRE,
	LSTATE_INVESTIGATE,
};

void ST_AggressionAdjust(const gentity_t* self, const int change)
{
	int upper_threshold, lower_threshold;

	self->NPC->stats.aggression += change;

	//FIXME: base this on initial NPC stats
	if (self->client->playerTeam == TEAM_PLAYER)
	{
		//good guys are less aggressive
		upper_threshold = 7;
		lower_threshold = 2;
	}
	else
	{
		//bad guys are more aggressive
		upper_threshold = 10;
		lower_threshold = 3;
	}

	if (self->NPC->stats.aggression > upper_threshold)
	{
		self->NPC->stats.aggression = upper_threshold;
	}
	else if (self->NPC->stats.aggression < lower_threshold)
	{
		self->NPC->stats.aggression = lower_threshold;
	}
}

void ST_ClearTimers(const gentity_t* ent)
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
	TIMER_Set(ent, "interrogating", 0);
	TIMER_Set(ent, "verifyCP", 0);
	TIMER_Set(ent, "strafeRight", 0);
	TIMER_Set(ent, "strafeLeft", 0);
}

enum
{
	SPEECH_CHASE,
	SPEECH_CONFUSED,
	SPEECH_COVER,
	SPEECH_DETECTED,
	SPEECH_GIVEUP,
	SPEECH_LOOK,
	SPEECH_LOST,
	SPEECH_OUTFLANK,
	SPEECH_ESCAPING,
	SPEECH_SIGHT,
	SPEECH_SOUND,
	SPEECH_SUSPICIOUS,
	SPEECH_YELL,
	SPEECH_PUSHED
};

qboolean NPC_IsGunner(const gentity_t* self)
{
	switch (self->client->NPC_class)
	{
	case CLASS_BESPIN_COP:
	case CLASS_COMMANDO:
	case CLASS_GALAK:
	case CLASS_GRAN:
	case CLASS_IMPERIAL:
	case CLASS_IMPWORKER:
	case CLASS_JAN:
	case CLASS_LANDO:
	case CLASS_GALAKMECH:
	case CLASS_MONMOTHA:
	case CLASS_PRISONER:
	case CLASS_REBEL:
	case CLASS_REELO:
	case CLASS_RODIAN:
	case CLASS_STORMTROOPER:
	case CLASS_SWAMPTROOPER:
	case CLASS_SABOTEUR:
	case CLASS_TRANDOSHAN:
	case CLASS_UGNAUGHT:
	case CLASS_JAWA:
	case CLASS_WEEQUAY:
	case CLASS_BOBAFETT:
	case CLASS_MANDALORIAN:
	case CLASS_JANGO:
	case CLASS_JANGODUAL:
	case CLASS_SBD:
	case CLASS_BATTLEDROID:
	case CLASS_STORMCOMMANDO:
	case CLASS_CLONETROOPER:
	case CLASS_TUSKEN:
	case CLASS_WOOKIE:
	case CLASS_CALONORD:
		return qtrue;
	default:
		break;
	}

	return qfalse;
}

static void ST_Speech(const gentity_t* self, const int speech_type, const float fail_chance)
{
	if (Q_flrand(0.0f, 1.0f) < fail_chance)
	{
		return;
	}

	if (fail_chance >= 0)
	{
		//a negative failChance makes it always talk
		if (self->NPC->group)
		{
			//group AI speech debounce timer
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
		else if (groupSpeechDebounceTime[self->client->playerTeam] > level.time)
		{
			//for those not in group AI
			//FIXME: let certain speech types interrupt others?  Let closer NPCs interrupt farther away ones?
			return;
		}
	}

	if (self->NPC->group)
	{
		//So they don't all speak at once...
		//FIXME: if they're not yet mad, they have no group, so distracting a group of them makes them all speak!
		self->NPC->group->speechDebounceTime = level.time + Q_irand(2000, 4000);
	}
	else
	{
		TIMER_Set(self, "chatter", Q_irand(12000, 14000));
	}
	groupSpeechDebounceTime[self->client->playerTeam] = level.time + Q_irand(2000, 4000);

	if (self->NPC->blockedSpeechDebounceTime > level.time)
	{
		return;
	}

	switch (speech_type)
	{
	case SPEECH_CHASE:
		G_AddVoiceEvent(self, Q_irand(EV_CHASE1, EV_CHASE3), 10000);
		break;
	case SPEECH_CONFUSED:
		G_AddVoiceEvent(self, Q_irand(EV_CONFUSE1, EV_CONFUSE3), 10000);
		break;
	case SPEECH_COVER:
		G_AddVoiceEvent(self, Q_irand(EV_COVER1, EV_COVER5), 10000);
		break;
	case SPEECH_DETECTED:
		G_AddVoiceEvent(self, Q_irand(EV_DETECTED1, EV_DETECTED5), 10000);
		break;
	case SPEECH_GIVEUP:
		G_AddVoiceEvent(self, Q_irand(EV_GIVEUP1, EV_GIVEUP4), 10000);
		break;
	case SPEECH_LOOK:
		G_AddVoiceEvent(self, Q_irand(EV_LOOK1, EV_LOOK2), 10000);
		break;
	case SPEECH_LOST:
		G_AddVoiceEvent(self, EV_LOST1, 10000);
		break;
	case SPEECH_OUTFLANK:
		G_AddVoiceEvent(self, Q_irand(EV_OUTFLANK1, EV_OUTFLANK2), 10000);
		break;
	case SPEECH_ESCAPING:
		G_AddVoiceEvent(self, Q_irand(EV_ESCAPING1, EV_ESCAPING3), 10000);
		break;
	case SPEECH_SIGHT:
		G_AddVoiceEvent(self, Q_irand(EV_SIGHT1, EV_SIGHT3), 10000);
		break;
	case SPEECH_SOUND:
		G_AddVoiceEvent(self, Q_irand(EV_SOUND1, EV_SOUND3), 10000);
		break;
	case SPEECH_SUSPICIOUS:
		G_AddVoiceEvent(self, Q_irand(EV_SUSPICIOUS1, EV_SUSPICIOUS5), 10000);
		break;
	case SPEECH_YELL:
		G_AddVoiceEvent(self, Q_irand(EV_ANGER1, EV_ANGER3), 10000);
		break;
	case SPEECH_PUSHED:
		G_AddVoiceEvent(self, Q_irand(EV_PUSHED1, EV_PUSHED3), 10000);
		break;
	default:
		break;
	}

	self->NPC->blockedSpeechDebounceTime = level.time + 2000;
}

void ST_MarkToCover(const gentity_t* self)
{
	if (!self || !self->NPC)
	{
		return;
	}
	self->NPC->localState = LSTATE_UNDERFIRE;
	TIMER_Set(self, "attackDelay", Q_irand(500, 2500));
	ST_AggressionAdjust(self, -3);
	if (self->NPC->group && self->NPC->group->numGroup > 1)
	{
		ST_Speech(self, SPEECH_COVER, 0); //FIXME: flee sound?
	}
}

void ST_StartFlee(gentity_t* self, gentity_t* enemy, vec3_t danger_point, const int danger_level, const int min_time,
	const int max_time)
{
	if (!self || !self->NPC)
	{
		return;
	}
	G_StartFlee(self, enemy, danger_point, danger_level, min_time, max_time);
	if (self->NPC->group && self->NPC->group->numGroup > 1)
	{
		ST_Speech(self, SPEECH_COVER, 0); //FIXME: flee sound?
	}
}

/*
-------------------------
NPC_ST_Pain
-------------------------
*/

void NPC_ST_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, const vec3_t point, const int damage,
	const int mod,
	const int hit_loc)
{
	self->NPC->localState = LSTATE_UNDERFIRE;

	TIMER_Set(self, "duck", -1);
	TIMER_Set(self, "hideTime", -1);
	TIMER_Set(self, "stand", 2000);

	NPC_Pain(self, inflictor, attacker, point, damage, mod, hit_loc);

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

static void ST_HoldPosition()
{
	if (NPCInfo->squadState == SQUAD_RETREAT)
	{
		TIMER_Set(NPC, "flee", -level.time);
	}
	TIMER_Set(NPC, "verifyCP", Q_irand(1000, 3000)); //don't look for another one for a few seconds
	NPC_FreeCombatPoint(NPCInfo->combatPoint, qtrue);
	//NPCInfo->combatPoint = -1;//???
	if (!Q3_TaskIDPending(NPC, TID_MOVE_NAV))
	{
		//don't have a script waiting for me to get to my point, okay to stop trying and stand
		AI_GroupUpdateSquadstates(NPCInfo->group, NPC, SQUAD_STAND_AND_SHOOT);
		NPCInfo->goalEntity = nullptr;
	}

	speaker_speech(NPC, SPEECH_COVER, 0);
}

static void NPC_ST_SayMovementSpeech()
{
	if (!NPCInfo->movementSpeech)
	{
		return;
	}
	if (NPCInfo->group &&
		NPCInfo->group->commander &&
		NPCInfo->group->commander->client &&
		NPCInfo->group->commander->client->NPC_class == CLASS_IMPERIAL &&
		!Q_irand(0, 3))
	{
		//imperial (commander) gives the order
		ST_Speech(NPCInfo->group->commander, NPCInfo->movementSpeech, NPCInfo->movementSpeechChance);
	}
	else
	{
		//really don't want to say this unless we can actually get there...
		ST_Speech(NPC, NPCInfo->movementSpeech, NPCInfo->movementSpeechChance);
	}

	NPCInfo->movementSpeech = 0;
	NPCInfo->movementSpeechChance = 0.0f;
}

static void NPC_ST_StoreMovementSpeech(const int speech, const float chance)
{
	NPCInfo->movementSpeech = speech;
	NPCInfo->movementSpeechChance = chance;
}

/*
-------------------------
ST_Move
-------------------------
*/
void ST_TransferMoveGoal(const gentity_t* self, const gentity_t* other);

static qboolean ST_Move()
{
	NPCInfo->combatMove = qtrue; //always move straight toward our goal

	const qboolean moved = NPC_MoveToGoal(qtrue);
	navInfo_t info;

	//Get the move info
	NAV_GetLastMove(info);

	//FIXME: if we bump into another one of our guys and can't get around him, just stop!
	//If we hit our target, then stop and fire!
	if (info.flags & NIF_COLLISION)
	{
		if (info.blocker == NPC->enemy)
		{
			ST_HoldPosition();
		}
	}

	//If our move failed, then reset
	if (moved == qfalse)
	{
		//FIXME: if we're going to a combat point, need to pick a different one
		if (!Q3_TaskIDPending(NPC, TID_MOVE_NAV))
		{
			//can't transfer movegoal or stop when a script we're running is waiting to complete
			if (info.blocker && info.blocker->NPC && NPCInfo->group != nullptr && info.blocker->NPC->group == NPCInfo->
				group) //(NPCInfo->aiFlags&NPCAI_BLOCKED) && NPCInfo->group != NULL )
			{
				//dammit, something is in our way
				//see if it's one of ours
				for (int j = 0; j < NPCInfo->group->numGroup; j++)
				{
					if (NPCInfo->group->member[j].number == NPCInfo->blockingEntNum)
					{
						//we're being blocked by one of our own, pass our goal onto them and I'll stand still
						ST_TransferMoveGoal(NPC, &g_entities[NPCInfo->group->member[j].number]);
						break;
					}
				}
			}

			ST_HoldPosition();
		}
	}
	else
	{
		//First time you successfully move, say what it is you're doing
		NPC_ST_SayMovementSpeech();
	}

	return moved;
}

/*
-------------------------
NPC_ST_SleepShuffle
-------------------------
*/

static void NPC_ST_SleepShuffle()
{
	//Play an awake script if we have one
	if (G_ActivateBehavior(NPC, BSET_AWAKE))
	{
		return;
	}

	//Automate some movement and noise
	if (TIMER_Done(NPC, "shuffleTime"))
	{
		TIMER_Set(NPC, "shuffleTime", 4000);
		TIMER_Set(NPC, "sleepTime", 2000);
		return;
	}

	//They made another noise while we were stirring, see if we can see them
	if (TIMER_Done(NPC, "sleepTime"))
	{
		NPC_CheckPlayerTeamStealth();
		TIMER_Set(NPC, "sleepTime", 2000);
	}
}

/*
-------------------------
NPC_ST_Sleep
-------------------------
*/

void NPC_BSST_Sleep()
{
	const int alert_event = NPC_CheckAlertEvents(qfalse, qtrue); //only check sounds since we're alseep!

	//There is an event we heard
	if (alert_event >= 0)
	{
		//See if it was enough to wake us up
		if (level.alertEvents[alert_event].level == AEL_DISCOVERED && NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			if (g_entities[0].health > 0)
			{
				G_SetEnemy(NPC, &g_entities[0]);
				return;
			}
		}

		//Otherwise just stir a bit
		NPC_ST_SleepShuffle();
	}
}

/*
-------------------------
NPC_CheckEnemyStealth
-------------------------
*/

static qboolean NPC_CheckEnemyStealth(gentity_t* target)
{
	float min_dist = 40; //any closer than 40 and we definitely notice

	//In case we aquired one some other way
	if (NPC->enemy != nullptr)
		return qtrue;

	//Ignore notarget
	if (target->flags & FL_NOTARGET)
		return qfalse;

	if (target->health <= 0)
	{
		return qfalse;
	}

	if (target->client->ps.weapon == WP_SABER && target->client->ps.SaberActive() && !target->client->ps.saberInFlight)
	{
		//if target has saber in hand and activated, we wake up even sooner even if not facing him
		min_dist = 100;
	}

	float target_dist = DistanceSquared(target->currentOrigin, NPC->currentOrigin);
	//If the target is this close, then wake up regardless
	if (!(target->client->ps.pm_flags & PMF_DUCKED) //not ducking
		&& NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES //looking for enemies
		&& target_dist < min_dist * min_dist) //closer than minDist
	{
		G_SetEnemy(NPC, target);
		NPCInfo->enemyLastSeenTime = level.time;
		TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
		return qtrue;
	}

	float max_view_dist;

	//	if ( NPCInfo->stats.visrange > maxViewDist )
	{
		//FIXME: should we always just set maxViewDist to this?
		max_view_dist = NPCInfo->stats.visrange;
	}

	if (target_dist > max_view_dist * max_view_dist)
	{
		//out of possible visRange
		return qfalse;
	}

	//Check FOV first
	if (InFOV(target, NPC, NPCInfo->stats.hfov, NPCInfo->stats.vfov) == qfalse)
		return qfalse;

	const qboolean clear_los = target->client->ps.leanofs
		? NPC_ClearLOS(target->client->renderInfo.eyePoint)
		: NPC_ClearLOS(target);

	//Now check for clear line of vision
	if (clear_los)
	{
		if (target->client->NPC_class == CLASS_ATST)
		{
			//can't miss 'em!
			G_SetEnemy(NPC, target);
			TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
			return qtrue;
		}
		vec3_t targ_org = {
			target->currentOrigin[0], target->currentOrigin[1], target->currentOrigin[2] + target->maxs[2] - 4
		};
		float h_angle_perc = NPC_GetHFOVPercentage(targ_org, NPC->client->renderInfo.eyePoint,
			NPC->client->renderInfo.eyeAngles, NPCInfo->stats.hfov);
		float vAngle_perc = NPC_GetVFOVPercentage(targ_org, NPC->client->renderInfo.eyePoint,
			NPC->client->renderInfo.eyeAngles, NPCInfo->stats.vfov);

		//Scale them vertically some, and horizontally pretty harshly
		vAngle_perc *= vAngle_perc; //( vAngle_perc * vAngle_perc );
		h_angle_perc *= h_angle_perc * h_angle_perc;

		//Cap our vertical vision severely
		//if ( vAngle_perc <= 0.3f ) // was 0.5f
		//	return qfalse;

		//Assess the player's current status
		target_dist = Distance(target->currentOrigin, NPC->currentOrigin);

		const float target_speed = VectorLength(target->client->ps.velocity);
		const int target_crouching = target->client->usercmd.upmove < 0;
		const float dist_rating = target_dist / max_view_dist;
		float speed_rating = target_speed / MAX_VIEW_SPEED;
		const float turning_rating = AngleDelta(target->client->ps.viewangles[PITCH], target->lastAngles[PITCH]) /
			180.0f + AngleDelta(target->client->ps.viewangles[YAW], target->lastAngles[YAW]) / 180.0f;
		const float light_level = target->lightLevel / MAX_LIGHT_INTENSITY;
		const float fov_perc = 1.0f - (h_angle_perc + vAngle_perc) * 0.5f; //FIXME: Dunno about the average...
		float vis_rating = 0.0f;

		//Too dark
		if (light_level < MIN_LIGHT_THRESHOLD)
			return qfalse;

		//Too close?
		if (dist_rating < DISTANCE_THRESHOLD)
		{
			G_SetEnemy(NPC, target);
			TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
			return qtrue;
		}

		//Out of range
		if (dist_rating > 1.0f)
			return qfalse;

		//Cap our speed checks
		if (speed_rating > 1.0f)
			speed_rating = 1.0f;

		//Calculate the distance, fov and light influences
		//...Visibilty linearly wanes over distance
		const float dist_influence = DISTANCE_SCALE * (1.0f - dist_rating);
		//...As the percentage out of the FOV increases, straight perception suffers on an exponential scale
		const float fov_influence = FOV_SCALE * (1.0f - fov_perc);
		//...Lack of light hides, abundance of light exposes
		const float light_influence = (light_level - 0.5f) * LIGHT_SCALE;

		//Calculate our base rating
		float target_rating = dist_influence + fov_influence + light_influence;

		//Now award any final bonuses to this number
		const int contents = gi.pointcontents(targ_org, target->s.number);
		if (contents & CONTENTS_WATER)
		{
			const int myContents = gi.pointcontents(NPC->client->renderInfo.eyePoint, NPC->s.number);
			if (!(myContents & CONTENTS_WATER))
			{
				//I'm not in water
				if (NPC->client->NPC_class == CLASS_SWAMPTROOPER)
				{
					//these guys can see in in/through water pretty well
					vis_rating = 0.10f; //10% bonus
				}
				else
				{
					vis_rating = 0.35f; //35% bonus
				}
			}
			else
			{
				//else, if we're both in water
				if (NPC->client->NPC_class == CLASS_SWAMPTROOPER)
				{
					//I can see him just fine
				}
				else
				{
					vis_rating = 0.15f; //15% bonus
				}
			}
		}
		else
		{
			//not in water
			if (contents & CONTENTS_FOG)
			{
				vis_rating = 0.15f; //15% bonus
			}
		}

		target_rating *= 1.0f - vis_rating;

		//...Motion draws the eye quickly
		target_rating += speed_rating * SPEED_SCALE;
		target_rating += turning_rating * TURNING_SCALE;
		//FIXME: check to see if they're animating, too?  But can we do something as simple as frame != oldframe?

		//...Smaller targets are harder to indentify
		if (target_crouching)
		{
			target_rating *= 0.9f; //10% bonus
		}

		//If he's violated the threshold, then realize him
		//float difficulty_scale = 1.0f + (2.0f-g_spskill->value);//if playing on easy, 20% harder to be seen...?
		float realize, cautious;
		if (NPC->client->NPC_class == CLASS_SWAMPTROOPER)
		{
			//swamptroopers can see much better
			realize = static_cast<float>(CAUTIOUS_THRESHOLD
				/**difficulty_scale*/)/**difficulty_scale*/;
			cautious = static_cast<float>(CAUTIOUS_THRESHOLD) * 0.75f/**difficulty_scale*/;
		}
		else
		{
			realize = static_cast<float>(REALIZE_THRESHOLD/**difficulty_scale*/)/**difficulty_scale*/;
			cautious = static_cast<float>(CAUTIOUS_THRESHOLD) * 0.75f/**difficulty_scale*/;
		}

		if (target_rating > realize && NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			G_SetEnemy(NPC, target);
			NPCInfo->enemyLastSeenTime = level.time;
			TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
			return qtrue;
		}

		//If he's above the caution threshold, then realize him in a few seconds unless he moves to cover
		if (target_rating > cautious && !(NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
		{
			//FIXME: ambushing guys should never talk
			if (TIMER_Done(NPC, "enemyLastVisible"))
			{
				//If we haven't already, start the counter
				const int look_time = Q_irand(4500, 8500);
				//NPCInfo->timeEnemyLastVisible = level.time + 2000;
				TIMER_Set(NPC, "enemyLastVisible", look_time);
				ST_Speech(NPC, SPEECH_SIGHT, 0);
				NPC_TempLookTarget(NPC, target->s.number, look_time, look_time);
				//FIXME: set desired yaw and pitch towards this guy?
			}
			else if (TIMER_Get(NPC, "enemyLastVisible") <= level.time + 500 && NPCInfo->scriptFlags &
				SCF_LOOK_FOR_ENEMIES) //FIXME: Is this reliable?
			{
				if (NPCInfo->rank < RANK_LT && !Q_irand(0, 2))
				{
					const int interrogateTime = Q_irand(2000, 4000);
					ST_Speech(NPC, SPEECH_SUSPICIOUS, 0);
					TIMER_Set(NPC, "interrogating", interrogateTime);
					G_SetEnemy(NPC, target);
					NPCInfo->enemyLastSeenTime = level.time;
					TIMER_Set(NPC, "attackDelay", interrogateTime);
					TIMER_Set(NPC, "stand", interrogateTime);
				}
				else
				{
					G_SetEnemy(NPC, target);
					NPCInfo->enemyLastSeenTime = level.time;
					//FIXME: ambush guys (like those popping out of water) shouldn't delay...
					TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
					TIMER_Set(NPC, "stand", Q_irand(500, 2500));
				}
				return qtrue;
			}

			return qfalse;
		}
	}

	return qfalse;
}

qboolean NPC_CheckPlayerTeamStealth()
{
	for (int i = 0; i < ENTITYNUM_WORLD; i++)
	{
		if (!PInUse(i))
			continue;
		gentity_t* enemy = &g_entities[i];
		if (enemy
			&& enemy->client
			&& NPC_ValidEnemy(enemy))
		{
			if (NPC_CheckEnemyStealth(enemy)) //Change this pointer to assess other entities
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

static qboolean NPC_CheckEnemiesInSpotlight()
{
	gentity_t* entity_list[MAX_GENTITIES];
	gentity_t* suspect = nullptr;
	int i;
	vec3_t mins{}, maxs{};

	for (i = 0; i < 3; i++)
	{
		mins[i] = NPC->client->renderInfo.eyePoint[i] - NPC->speed;
		maxs[i] = NPC->client->renderInfo.eyePoint[i] + NPC->speed;
	}

	const int num_listed_entities = gi.EntitiesInBox(mins, maxs, entity_list, MAX_GENTITIES);

	for (i = 0; i < num_listed_entities; i++)
	{
		if (!PInUse(i))
			continue;

		gentity_t* enemy = entity_list[i];

		if (enemy && enemy->client && NPC_ValidEnemy(enemy) && enemy->client->playerTeam == NPC->client->enemyTeam)
		{
			//valid ent & client, valid enemy, on the target team
			//check to see if they're in my FOV
			if (InFOV(enemy->currentOrigin, NPC->client->renderInfo.eyePoint, NPC->client->renderInfo.eyeAngles,
				NPCInfo->stats.hfov, NPCInfo->stats.vfov))
			{
				//in my cone
				//check to see that they're close enough
				if (DistanceSquared(NPC->client->renderInfo.eyePoint, enemy->currentOrigin) - 256
					/*fudge factor: 16 squared*/ <= NPC->speed * NPC->speed)
				{
					//within range
					//check to see if we have a clear trace to them
					if (G_ClearLOS(NPC, enemy))
					{
						//clear LOS
						//make sure their light level is at least my beam's brightness
						//FIXME: HOW?
						//enemy->lightLevel / MAX_LIGHT_INTENSITY

						//good enough, take him!
						//FIXME: pick closest one?
						//FIXME: have the graduated noticing like other NPCs? (based on distance, FOV dot, etc...)
						G_SetEnemy(NPC, enemy);
						TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
						return qtrue;
					}
				}
			}
			if (InFOV(enemy->currentOrigin, NPC->client->renderInfo.eyePoint, NPC->client->renderInfo.eyeAngles, 90,
				NPCInfo->stats.vfov * 3))
			{
				//one to look at if we don't get an enemy
				if (G_ClearLOS(NPC, enemy))
				{
					//clear LOS
					if (suspect == nullptr || DistanceSquared(NPC->client->renderInfo.eyePoint, enemy->currentOrigin) <
						DistanceSquared(NPC->client->renderInfo.eyePoint, suspect->currentOrigin))
					{
						//remember him
						suspect = enemy;
					}
				}
			}
		}
	}
	if (suspect && Q_flrand(0, NPCInfo->stats.visrange * NPCInfo->stats.visrange) > DistanceSquared(
		NPC->client->renderInfo.eyePoint, suspect->currentOrigin))
	{
		//hey!  who's that?
		if (TIMER_Done(NPC, "enemyLastVisible"))
		{
			//If we haven't already, start the counter
			const int look_time = Q_irand(4500, 8500);
			//NPCInfo->timeEnemyLastVisible = level.time + 2000;
			TIMER_Set(NPC, "enemyLastVisible", look_time);
			ST_Speech(NPC, SPEECH_SIGHT, 0);
			//set desired yaw and pitch towards this guy?
			//FIXME: this is permanent, they will never look away... *sigh*
			NPC_FacePosition(suspect->currentOrigin, qtrue);
			//FIXME: they still need some sort of eye/head tag/bone that can turn?
			//NPC_TempLookTarget( NPC, suspect->s.number, lookTime, lookTime );
		}
		else if (TIMER_Get(NPC, "enemyLastVisible") <= level.time + 500
			&& NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES) //FIXME: Is this reliable?
		{
			if (!Q_irand(0, 2))
			{
				const int interrogate_time = Q_irand(2000, 4000);
				ST_Speech(NPC, SPEECH_SUSPICIOUS, 0);
				TIMER_Set(NPC, "interrogating", interrogate_time);
				//G_SetEnemy( NPC, target );
				//NPCInfo->enemyLastSeenTime = level.time;
				//TIMER_Set( NPC, "attackDelay", interrogateTime );
				//TIMER_Set( NPC, "stand", interrogateTime );
				//set desired yaw and pitch towards this guy?
				//FIXME: this is permanent, they will never look away... *sigh*
				NPC_FacePosition(suspect->currentOrigin, qtrue);
				//FIXME: they still need some sort of eye/head tag/bone that can turn?
				//NPC_TempLookTarget( NPC, suspect->s.number, interrogateTime, interrogateTime );
			}
		}
	}
	return qfalse;
}

/*
-------------------------
NPC_ST_InvestigateEvent
-------------------------
*/

constexpr auto MAX_CHECK_THRESHOLD = 1;

static qboolean NPC_ST_InvestigateEvent(const int event_id, const bool extra_suspicious)
{
	//If they've given themselves away, just take them as an enemy
	if (NPCInfo->confusionTime < level.time)
	{
		if (level.alertEvents[event_id].level == AEL_DISCOVERED && NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			//NPCInfo->lastAlertID = level.alertEvents[eventID].ID;
			if (!level.alertEvents[event_id].owner ||
				!level.alertEvents[event_id].owner->client ||
				level.alertEvents[event_id].owner->health <= 0 ||
				level.alertEvents[event_id].owner->client->playerTeam != NPC->client->enemyTeam)
			{
				//not an enemy
				return qfalse;
			}
			//FIXME: what if can't actually see enemy, don't know where he is... should we make them just become very alert and start looking for him?  Or just let combat AI handle this... (act as if you lost him)
			//ST_Speech( NPC, SPEECH_CHARGE, 0 );
			G_SetEnemy(NPC, level.alertEvents[event_id].owner);
			NPCInfo->enemyLastSeenTime = level.time;
			TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
			if (level.alertEvents[event_id].type == AET_SOUND)
			{
				//heard him, didn't see him, stick for a bit
				TIMER_Set(NPC, "roamTime", Q_irand(500, 2500));
			}
			return qtrue;
		}
	}

	//don't look at the same alert twice
	/*
	if ( level.alertEvents[eventID].ID == NPCInfo->lastAlertID )
	{
		return qfalse;
	}
	NPCInfo->lastAlertID = level.alertEvents[eventID].ID;
	*/

	//Must be ready to take another sound event
	/*
	if ( NPCInfo->investigateSoundDebounceTime > level.time )
	{
		return qfalse;
	}
	*/

	if (level.alertEvents[event_id].type == AET_SIGHT)
	{
		//sight alert, check the light level
		if (level.alertEvents[event_id].light < Q_irand(ST_MIN_LIGHT_THRESHOLD, ST_MAX_LIGHT_THRESHOLD))
		{
			//below my threshhold of potentially seeing
			return qfalse;
		}
	}

	//Save the position for movement (if necessary)
	VectorCopy(level.alertEvents[event_id].position, NPCInfo->investigateGoal);

	//First awareness of it
	NPCInfo->investigateCount += extra_suspicious ? 2 : 1;

	//Clamp the value
	if (NPCInfo->investigateCount > 4)
		NPCInfo->investigateCount = 4;

	//See if we should walk over and investigate
	if (level.alertEvents[event_id].level > AEL_MINOR && NPCInfo->investigateCount > 1 && NPCInfo->scriptFlags &
		SCF_CHASE_ENEMIES)
	{
		//make it so they can walk right to this point and look at it rather than having to use combatPoints
		if (G_ExpandPointToBBox(NPCInfo->investigateGoal, NPC->mins, NPC->maxs, NPC->s.number,
			NPC->clipmask & ~CONTENTS_BODY | CONTENTS_BOTCLIP))
		{
			//we were able to doMove the investigateGoal to a point in which our bbox would fit
			//drop the goal to the ground so we can get at it
			vec3_t end;
			trace_t trace;
			VectorCopy(NPCInfo->investigateGoal, end);
			end[2] -= 512; //FIXME: not always right?  What if it's even higher, somehow?
			gi.trace(&trace, NPCInfo->investigateGoal, NPC->mins, NPC->maxs, end, ENTITYNUM_NONE,
				NPC->clipmask & ~CONTENTS_BODY | CONTENTS_BOTCLIP, static_cast<EG2_Collision>(0), 0);
			if (trace.fraction >= 1.0f)
			{
				//too high to even bother
				//FIXME: look at them???
			}
			else
			{
				VectorCopy(trace.endpos, NPCInfo->investigateGoal);
				NPC_SetMoveGoal(NPC, NPCInfo->investigateGoal, 16, qtrue);
				NPCInfo->localState = LSTATE_INVESTIGATE;
			}
		}
		else
		{
			const int id = NPC_FindCombatPoint(NPCInfo->investigateGoal, NPCInfo->investigateGoal,
				NPCInfo->investigateGoal, CP_INVESTIGATE | CP_HAS_ROUTE, 0);

			if (id != -1)
			{
				NPC_SetMoveGoal(NPC, level.combatPoints[id].origin, 16, qtrue, id);
				NPCInfo->localState = LSTATE_INVESTIGATE;
			}
		}
		//Say something
		//FIXME: only if have others in group... these should be responses?
		if (NPCInfo->investigateDebounceTime + NPCInfo->pauseTime > level.time)
		{
			//was already investigating
			if (NPCInfo->group &&
				NPCInfo->group->commander &&
				NPCInfo->group->commander->client &&
				NPCInfo->group->commander->client->NPC_class == CLASS_IMPERIAL &&
				!Q_irand(0, 3))
			{
				ST_Speech(NPCInfo->group->commander, SPEECH_LOOK, 0); //FIXME: "I'll go check it out" type sounds
			}
			else
			{
				ST_Speech(NPC, SPEECH_LOOK, 0); //FIXME: "I'll go check it out" type sounds
			}
		}
		else
		{
			if (level.alertEvents[event_id].type == AET_SIGHT)
			{
				ST_Speech(NPC, SPEECH_SIGHT, 0);
			}
			else if (level.alertEvents[event_id].type == AET_SOUND)
			{
				ST_Speech(NPC, SPEECH_SOUND, 0);
			}
		}
		//Setup the debounce info
		NPCInfo->investigateDebounceTime = NPCInfo->investigateCount * 5000;
		NPCInfo->investigateSoundDebounceTime = level.time + 2000;
		NPCInfo->pauseTime = level.time;
	}
	else
	{
		//just look?
		//Say something
		if (level.alertEvents[event_id].type == AET_SIGHT)
		{
			ST_Speech(NPC, SPEECH_SIGHT, 0);
		}
		else if (level.alertEvents[event_id].type == AET_SOUND)
		{
			ST_Speech(NPC, SPEECH_SOUND, 0);
		}
		//Setup the debounce info
		NPCInfo->investigateDebounceTime = NPCInfo->investigateCount * 1000;
		NPCInfo->investigateSoundDebounceTime = level.time + 1000;
		NPCInfo->pauseTime = level.time;
		VectorCopy(level.alertEvents[event_id].position, NPCInfo->investigateGoal);
		if (NPC->client->NPC_class == CLASS_ROCKETTROOPER
			&& !RT_Flying(NPC))
		{
			//if ( !Q_irand( 0, 2 ) )
			{
				//look around
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_GUARD_LOOKAROUND1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
	}

	if (level.alertEvents[event_id].level >= AEL_DANGER)
	{
		NPCInfo->investigateDebounceTime = Q_irand(500, 2500);
	}

	//Start investigating
	NPCInfo->tempBehavior = BS_INVESTIGATE;
	return qtrue;
}

/*
-------------------------
ST_OffsetLook
-------------------------
*/

static void ST_OffsetLook(const float offset, vec3_t out)
{
	vec3_t angles, forward, temp;

	GetAnglesForDirection(NPC->currentOrigin, NPCInfo->investigateGoal, angles);
	angles[YAW] += offset;
	AngleVectors(angles, forward, nullptr, nullptr);
	VectorMA(NPC->currentOrigin, 64, forward, out);

	CalcEntitySpot(NPC, SPOT_HEAD, temp);
	out[2] = temp[2];
}

/*
-------------------------
ST_LookAround
-------------------------
*/

static void ST_LookAround()
{
	vec3_t look_pos;
	const float perc = static_cast<float>(level.time - NPCInfo->pauseTime) / static_cast<float>(NPCInfo->
		investigateDebounceTime);

	//Keep looking at the spot
	if (perc < 0.25)
	{
		VectorCopy(NPCInfo->investigateGoal, look_pos);
	}
	else if (perc < 0.5f) //Look up but straight ahead
	{
		ST_OffsetLook(0.0f, look_pos);
	}
	else if (perc < 0.75f) //Look right
	{
		ST_OffsetLook(45.0f, look_pos);
	}
	else //Look left
	{
		ST_OffsetLook(-45.0f, look_pos);
	}

	NPC_FacePosition(look_pos);
}

/*
-------------------------
NPC_BSST_Investigate
-------------------------
*/

void NPC_BSST_Investigate()
{
	//get group- mainly for group speech debouncing, but may use for group scouting/investigating AI, too
	AI_GetGroup(NPC);

	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}

	if (NPCInfo->confusionTime < level.time)
	{
		if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			//Look for an enemy
			if (NPC_CheckPlayerTeamStealth())
			{
				//NPCInfo->behaviorState	= BS_HUNT_AND_KILL;//should be auto now
				ST_Speech(NPC, SPEECH_DETECTED, 0);
				NPCInfo->tempBehavior = BS_DEFAULT;
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}
	}

	if (!(NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
	{
		const int alert_event = NPC_CheckAlertEvents(qtrue, qtrue, NPCInfo->lastAlertID);

		//There is an event to look at
		if (alert_event >= 0)
		{
			if (NPCInfo->confusionTime < level.time)
			{
				if (NPC_CheckForDanger(alert_event))
				{
					//running like hell
					ST_Speech(NPC, SPEECH_COVER, 0); //FIXME: flee sound?
					return;
				}
			}

			//if ( level.alertEvents[alert_event].ID != NPCInfo->lastAlertID )
			{
				NPC_ST_InvestigateEvent(alert_event, qtrue);
			}
		}
	}

	//If we're done looking, then just return to what we were doing
	if (NPCInfo->investigateDebounceTime + NPCInfo->pauseTime < level.time)
	{
		NPCInfo->tempBehavior = BS_DEFAULT;
		NPCInfo->goalEntity = UpdateGoal();

		NPC_UpdateAngles(qtrue, qtrue);
		//Say something
		ST_Speech(NPC, SPEECH_GIVEUP, 0);
		return;
	}

	//FIXME: else, look for new alerts

	//See if we're searching for the noise's origin
	if (NPCInfo->localState == LSTATE_INVESTIGATE && NPCInfo->goalEntity != nullptr)
	{
		//See if we're there
		if (!STEER::Reached(NPC, NPCInfo->goalEntity, 32, FlyingCreature(NPC) != qfalse))
		{
			ucmd.buttons |= BUTTON_WALKING;

			//Try and doMove there
			if (NPC_MoveToGoal(qtrue))
			{
				//Bump our times
				NPCInfo->investigateDebounceTime = NPCInfo->investigateCount * 5000;
				NPCInfo->pauseTime = level.time;

				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}

		//Otherwise we're done or have given up
		//Say something
		//ST_Speech( NPC, SPEECH_LOOK, 0.33f );
		NPCInfo->localState = LSTATE_NONE;
	}

	//Look around
	ST_LookAround();
}

/*
-------------------------
NPC_BSST_Patrol
-------------------------
*/

void NPC_BSST_Patrol()
{
	//FIXME: pick up on bodies of dead buddies?
	//Not a scriptflag, but...
	if (NPC->client->NPC_class == CLASS_ROCKETTROOPER && NPC->client->ps.eFlags & EF_SPOTLIGHT)
	{
		//using spotlight search mode
		vec3_t eye_fwd, end;
		constexpr vec3_t maxs = { 2, 2, 2 };
		constexpr vec3_t mins = { -2, -2, -2 };
		trace_t trace;
		AngleVectors(NPC->client->renderInfo.eyeAngles, eye_fwd, nullptr, nullptr);
		VectorMA(NPC->client->renderInfo.eyePoint, NPCInfo->stats.visrange, eye_fwd, end);
		//get server-side trace impact point
		gi.trace(&trace, NPC->client->renderInfo.eyePoint, mins, maxs, end, NPC->s.number,
			MASK_OPAQUE | CONTENTS_BODY | CONTENTS_CORPSE, static_cast<EG2_Collision>(0), 0);
		NPC->speed = trace.fraction * NPCInfo->stats.visrange;
		if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
		{
			//FIXME: do a FOV cone check, then a trace
			if (trace.entityNum < ENTITYNUM_WORLD)
			{
				//hit something
				//try cheap check first
				gentity_t* enemy = &g_entities[trace.entityNum];
				if (enemy && enemy->client && NPC_ValidEnemy(enemy) && enemy->client->playerTeam == NPC->client->
					enemyTeam)
				{
					G_SetEnemy(NPC, enemy);
					TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
					//NPCInfo->behaviorState = BS_HUNT_AND_KILL;//should be auto now
					//NPC_AngerSound();
					NPC_UpdateAngles(qtrue, qtrue);
					return;
				}
			}
			//FIXME: maybe do a quick check of ents within the spotlight's radius?
			//hmmm, look around
			if (NPC_CheckEnemiesInSpotlight())
			{
				//NPCInfo->behaviorState = BS_HUNT_AND_KILL;//should be auto now
				//NPC_AngerSound();
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}
	}
	else
	{
		//get group- mainly for group speech debouncing, but may use for group scouting/investigating AI, too
		AI_GetGroup(NPC);

		if (NPCInfo->confusionTime < level.time)
		{
			//Look for any enemies
			if (NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES)
			{
				if (NPC_CheckPlayerTeamStealth())
				{
					//NPCInfo->behaviorState = BS_HUNT_AND_KILL;//should be auto now
					//NPC_AngerSound();
					NPC_UpdateAngles(qtrue, qtrue);
					return;
				}
			}
		}
	}

	if (!(NPCInfo->scriptFlags & SCF_IGNORE_ALERTS))
	{
		const int alert_event = NPC_CheckAlertEvents(qtrue, qtrue);

		//There is an event to look at
		if (alert_event >= 0)
		{
			if (NPC_CheckForDanger(alert_event))
			{
				//going to run?
				ST_Speech(NPC, SPEECH_COVER, 0);
				return;
			}
			if (NPC->client->NPC_class == CLASS_BOBAFETT ||
				NPC->client->NPC_class == CLASS_MANDALORIAN ||
				NPC->client->NPC_class == CLASS_JANGO ||
				NPC->client->NPC_class == CLASS_JANGODUAL)
			{
				if (!level.alertEvents[alert_event].owner ||
					!level.alertEvents[alert_event].owner->client ||
					level.alertEvents[alert_event].owner->health <= 0 ||
					level.alertEvents[alert_event].owner->client->playerTeam != NPC->client->enemyTeam)
				{
					//not an enemy
					return;
				}
				G_SetEnemy(NPC, level.alertEvents[alert_event].owner);
				NPCInfo->enemyLastSeenTime = level.time;
				TIMER_Set(NPC, "attackDelay", Q_irand(500, 2500));
				return;
			}
			if (NPC_ST_InvestigateEvent(alert_event, qfalse))
			{
				//actually going to investigate it
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}
	}

	//If we have somewhere to go, then do that
	if (UpdateGoal())
	{
		ucmd.buttons |= BUTTON_WALKING;
		//ST_Move( NPCInfo->goalEntity );
		NPC_MoveToGoal(qtrue);
	}
	else // if ( !(NPCInfo->scriptFlags&SCF_IGNORE_ALERTS) )
	{
		if (NPC->client->NPC_class != CLASS_IMPERIAL && NPC->client->NPC_class != CLASS_IMPWORKER)
		{
			//imperials do not look around
			if (TIMER_Done(NPC, "enemyLastVisible"))
			{
				//nothing suspicious, look around
				if (!Q_irand(0, 30))
				{
					NPCInfo->desiredYaw = NPC->s.angles[1] + Q_irand(-90, 90);
				}
				if (!Q_irand(0, 30))
				{
					NPCInfo->desiredPitch = Q_irand(-20, 20);
				}
			}
		}
	}

	NPC_UpdateAngles(qtrue, qtrue);
	//TEMP hack for Imperial stand anim
	if (NPC->client->NPC_class == CLASS_IMPERIAL
		|| NPC->client->NPC_class == CLASS_IMPWORKER)
	{
		//hack
		if (NPC->client->ps.weapon != WP_CONCUSSION)
		{
			//not Rax
			if (ucmd.forwardmove || ucmd.rightmove || ucmd.upmove)
			{
				//moving
				if (!NPC->client->ps.torsoAnimTimer || NPC->client->ps.torsoAnim == BOTH_STAND4)
				{
					if (ucmd.buttons & BUTTON_WALKING && !(NPCInfo->scriptFlags & SCF_RUNNING))
					{
						//not running, only set upper anim
						//  No longer overrides scripted anims
						NPC_SetAnim(NPC, SETANIM_TORSO, BOTH_STAND4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						NPC->client->ps.torsoAnimTimer = 200;
					}
				}
			}
			else
			{
				//standing still, set both torso and legs anim
				//  No longer overrides scripted anims
				if ((!NPC->client->ps.torsoAnimTimer || NPC->client->ps.torsoAnim == BOTH_STAND4) &&
					(!NPC->client->ps.legsAnimTimer || NPC->client->ps.legsAnim == BOTH_STAND4))
				{
					NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_STAND4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					NPC->client->ps.torsoAnimTimer = NPC->client->ps.legsAnimTimer = 200;
				}
			}
			//FIXME: this is a disgusting hack that is supposed to make the Imperials start with their weapon holstered- need a better way
			if (NPC->client->ps.weapon != WP_NONE)
			{
				ChangeWeapon(NPC, WP_NONE);
				NPC->client->ps.weapon = WP_NONE;
				NPC->client->ps.weaponstate = WEAPON_READY;
				G_RemoveWeaponModels(NPC);
			}
		}
	}
}

/*
-------------------------
NPC_BSST_Idle
-------------------------
*/
/*
void NPC_BSST_Idle( void )
{
	int alert_event = NPC_CheckAlertEvents( qtrue, qtrue );

	//There is an event to look at
	if ( alert_event >= 0 )
	{
		NPC_ST_InvestigateEvent( alert_event, qfalse );
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

static void ST_CheckMoveState()
{
	if (Q3_TaskIDPending(NPC, TID_MOVE_NAV))
	{
		//moving toward a goal that a script is waiting on, so don't stop for anything!
		do_move = qtrue;
	}
	else if (NPC->client->NPC_class == CLASS_ROCKETTROOPER
		&& NPC->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		//no squad stuff
		return;
	}
	//	else if ( NPC->NPC->scriptFlags&SCF_NO_GROUPS )
	{
		do_move = qtrue;
	}
	//See if we're a scout

	//See if we're moving towards a goal, not the enemy
	if (NPCInfo->goalEntity != NPC->enemy && NPCInfo->goalEntity != nullptr)
	{
		//Did we make it?
		if (STEER::Reached(NPC, NPCInfo->goalEntity, 16, !!FlyingCreature(NPC)) ||
			enemy_los && NPCInfo->aiFlags & NPCAI_STOP_AT_LOS && !Q3_TaskIDPending(NPC, TID_MOVE_NAV)
			)
		{
			//either hit our navgoal or our navgoal was not a crucial (scripted) one (maybe a combat point) and we're scouting and found our enemy
			int new_squad_state = SQUAD_STAND_AND_SHOOT;
			//we got where we wanted to go, set timers based on why we were running
			switch (NPCInfo->squadState)
			{
			case SQUAD_RETREAT: //was running away
				//done fleeing, obviously
				TIMER_Set(NPC, "duck", (NPC->max_health - NPC->health) * 100);
				TIMER_Set(NPC, "hideTime", Q_irand(3000, 7000));
				TIMER_Set(NPC, "flee", -level.time);
				new_squad_state = SQUAD_COVER;
				break;
			case SQUAD_TRANSITION: //was heading for a combat point
				TIMER_Set(NPC, "hideTime", Q_irand(2000, 4000));
				break;
			case SQUAD_SCOUT: //was running after player
				break;
			default:
				break;
			}
			AI_GroupUpdateSquadstates(NPCInfo->group, NPC, new_squad_state);
			NPC_ReachedGoal();
			//don't attack right away
			TIMER_Set(NPC, "attackDelay", Q_irand(250, 500)); //FIXME: Slant for difficulty levels
			//don't do something else just yet

			// THIS IS THE ONE TRUE PLACE WHERE ROAM TIME IS SET
			TIMER_Set(NPC, "roamTime", Q_irand(8000, 15000)); //Q_irand( 1000, 4000 ) );
			if (Q_irand(0, 3) == 0)
			{
				TIMER_Set(NPC, "duck", Q_irand(5000, 10000)); // just reached our goal, chance of ducking now
			}
			return;
		}

		//keep going, hold of roamTimer until we get there
		TIMER_Set(NPC, "roamTime", Q_irand(8000, 9000));
	}
}

static void ST_ResolveBlockedShot(const int hit)
{
	int stuck_time;
	//figure out how long we intend to stand here, max
	if (TIMER_Get(NPC, "roamTime") > TIMER_Get(NPC, "stick"))
	{
		stuck_time = TIMER_Get(NPC, "roamTime") - level.time;
	}
	else
	{
		stuck_time = TIMER_Get(NPC, "stick") - level.time;
	}

	if (TIMER_Done(NPC, "duck"))
	{
		//we're not ducking
		if (AI_GroupContainsEntNum(NPCInfo->group, hit))
		{
			const gentity_t* member = &g_entities[hit];
			if (TIMER_Done(member, "duck"))
			{
				//they aren't ducking
				if (TIMER_Done(member, "stand"))
				{
					//they're not being forced to stand
					//tell them to duck at least as long as I'm not moving
					TIMER_Set(member, "duck", stuck_time); // tell my friend to duck so I can shoot over his head
					return;
				}
			}
		}
	}
	else
	{
		//maybe we should stand
		if (TIMER_Done(NPC, "stand"))
		{
			//stand for as long as we'll be here
			TIMER_Set(NPC, "stand", stuck_time);
			return;
		}
	}
	//Hmm, can't resolve this by telling them to duck or telling me to stand
	//We need to doMove!
	TIMER_Set(NPC, "roamTime", -1);
	TIMER_Set(NPC, "stick", -1);
	TIMER_Set(NPC, "duck", -1);
	TIMER_Set(NPC, "attakDelay", Q_irand(1000, 3000));
}

/*
-------------------------
ST_CheckFireState
-------------------------
*/

static void ST_CheckFireState()
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
		return;
	}

	//See if we should continue to fire on their last position
	//!TIMER_Done( NPC, "stick" ) ||
	if (!hitAlly //we're not going to hit an ally
		&& enemyInFOV //enemy is in our FOV //FIXME: or we don't have a clear LOS?
		&& NPCInfo->enemyLastSeenTime > 0 //we've seen the enemy
		&& NPCInfo->group //have a group
		&& (NPCInfo->group->numState[SQUAD_RETREAT] > 0 || NPCInfo->group->numState[SQUAD_TRANSITION] > 0 || NPCInfo->
			group->numState[SQUAD_SCOUT] > 0)) //laying down covering fire
	{
		if (level.time - NPCInfo->enemyLastSeenTime < 10000 && //we have seem the enemy in the last 10 seconds
			(!NPCInfo->group || level.time - NPCInfo->group->lastSeenEnemyTime < 10000))
			//we are not in a group or the group has seen the enemy in the last 10 seconds
		{
			if (!Q_irand(0, 10))
			{
				//Fire on the last known position
				vec3_t muzzle;
				qboolean too_close = qfalse;
				qboolean too_far = qfalse;

				CalcEntitySpot(NPC, SPOT_HEAD, muzzle);
				if (VectorCompare(impactPos, vec3_origin))
				{
					//never checked ShotEntity this frame, so must do a trace...
					trace_t tr;
					//vec3_t	mins = {-2,-2,-2}, maxs = {2,2,2};
					vec3_t forward, end;
					AngleVectors(NPC->client->ps.viewangles, forward, nullptr, nullptr);
					VectorMA(muzzle, 8192, forward, end);
					gi.trace(&tr, muzzle, vec3_origin, vec3_origin, end, NPC->s.number, MASK_SHOT,
						static_cast<EG2_Collision>(0), 0);
					VectorCopy(tr.endpos, impactPos);
				}

				//see if impact would be too close to me
				float dist_threshold = 16384/*128*128*/; //default
				switch (NPC->s.weapon)
				{
				case WP_ROCKET_LAUNCHER:
				case WP_FLECHETTE:
				case WP_THERMAL:
				case WP_TRIP_MINE:
				case WP_DET_PACK:
					dist_threshold = 65536/*256*256*/;
					break;
				case WP_REPEATER:
					if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
					{
						dist_threshold = 65536/*256*256*/;
					}
					break;
				case WP_CONCUSSION:
					if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
					{
						dist_threshold = 65536/*256*256*/;
					}
					break;
				default:
					break;
				}

				float dist = DistanceSquared(impactPos, muzzle);

				if (dist < dist_threshold)
				{
					//impact would be too close to me
					too_close = qtrue;
				}
				else if (level.time - NPCInfo->enemyLastSeenTime > 5000 ||
					NPCInfo->group && level.time - NPCInfo->group->lastSeenEnemyTime > 5000)
				{
					//we've haven't seen them in the last 5 seconds
					//see if it's too far from where he is
					dist_threshold = 65536/*256*256*/; //default
					switch (NPC->s.weapon)
					{
					case WP_ROCKET_LAUNCHER:
					case WP_FLECHETTE:
					case WP_THERMAL:
					case WP_TRIP_MINE:
					case WP_DET_PACK:
						dist_threshold = 262144/*512*512*/;
						break;
					case WP_REPEATER:
						if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
						{
							dist_threshold = 262144/*512*512*/;
						}
						break;
					case WP_CONCUSSION:
						if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
						{
							dist_threshold = 262144/*512*512*/;
						}
						break;
					default:
						break;
					}
					dist = DistanceSquared(impactPos, NPCInfo->enemyLastSeenLocation);
					if (dist > dist_threshold)
					{
						//impact would be too far from enemy
						too_far = qtrue;
					}
				}

				if (!too_close && !too_far)
				{
					vec3_t angles;
					vec3_t dir;
					//okay too shoot at last pos
					VectorSubtract(NPCInfo->enemyLastSeenLocation, muzzle, dir);
					VectorNormalize(dir);
					vectoangles(dir, angles);

					NPCInfo->desiredYaw = angles[YAW];
					NPCInfo->desiredPitch = angles[PITCH];

					shoot = qtrue;
					face_enemy = qfalse;
				}
			}
		}
	}
}

static void ST_TrackEnemy(const gentity_t* self, vec3_t enemy_pos)
{
	//clear timers
	TIMER_Set(self, "attackDelay", Q_irand(1000, 2000));
	//TIMER_Set( self, "duck", -1 );
	TIMER_Set(self, "stick", Q_irand(500, 1500));
	TIMER_Set(self, "stand", -1);
	TIMER_Set(self, "scoutTime", TIMER_Get(self, "stick") - level.time + Q_irand(5000, 10000));
	//leave my combat point
	NPC_FreeCombatPoint(self->NPC->combatPoint);
	//go after his last seen pos
	NPC_SetMoveGoal(self, enemy_pos, 100.0f, qfalse);
	if (Q_irand(0, 3) == 0)
	{
		NPCInfo->aiFlags |= NPCAI_STOP_AT_LOS;
	}
}

static int ST_ApproachEnemy(const gentity_t* self)
{
	TIMER_Set(self, "attackDelay", Q_irand(250, 500));
	//TIMER_Set( self, "duck", -1 );
	TIMER_Set(self, "stick", Q_irand(1000, 2000));
	TIMER_Set(self, "stand", -1);
	TIMER_Set(self, "scoutTime", TIMER_Get(self, "stick") - level.time + Q_irand(5000, 10000));
	//leave my combat point
	NPC_FreeCombatPoint(self->NPC->combatPoint);
	//return the relevant combat point flags
	return CP_CLEAR | CP_CLOSEST;
}

static void ST_HuntEnemy(const gentity_t* self)
{
	//TIMER_Set( NPC, "attackDelay", Q_irand( 250, 500 ) );//Disabled this for now, guys who couldn't hunt would never attack
	//TIMER_Set( NPC, "duck", -1 );
	TIMER_Set(NPC, "stick", Q_irand(250, 1000));
	TIMER_Set(NPC, "stand", -1);
	TIMER_Set(NPC, "scoutTime", TIMER_Get(NPC, "stick") - level.time + Q_irand(5000, 10000));
	//leave my combat point
	NPC_FreeCombatPoint(NPCInfo->combatPoint);
	//go directly after the enemy
	if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
	{
		self->NPC->goalEntity = NPC->enemy;
	}
}

static void ST_TransferTimers(const gentity_t* self, const gentity_t* other)
{
	TIMER_Set(other, "attackDelay", TIMER_Get(self, "attackDelay") - level.time);
	TIMER_Set(other, "duck", TIMER_Get(self, "duck") - level.time);
	TIMER_Set(other, "stick", TIMER_Get(self, "stick") - level.time);
	TIMER_Set(other, "scoutTime", TIMER_Get(self, "scoutTime") - level.time);
	TIMER_Set(other, "roamTime", TIMER_Get(self, "roamTime") - level.time);
	TIMER_Set(other, "stand", TIMER_Get(self, "stand") - level.time);
	TIMER_Set(self, "attackDelay", -1);
	TIMER_Set(self, "duck", -1);
	TIMER_Set(self, "stick", -1);
	TIMER_Set(self, "scoutTime", -1);
	TIMER_Set(self, "roamTime", -1);
	TIMER_Set(self, "stand", -1);
}

void ST_TransferMoveGoal(const gentity_t* self, const gentity_t* other)
{
	if (Q3_TaskIDPending(self, TID_MOVE_NAV))
	{
		//can't transfer movegoal when a script we're running is waiting to complete
		return;
	}
	if (self->NPC->combatPoint != -1)
	{
		//I've got a combatPoint I'm going to, give it to him
		self->NPC->lastFailedCombatPoint = other->NPC->combatPoint = self->NPC->combatPoint;
		self->NPC->combatPoint = -1;
	}
	else
	{
		//I must be going for a goal, give that to him instead
		if (self->NPC->goalEntity == self->NPC->tempGoal)
		{
			NPC_SetMoveGoal(other, self->NPC->tempGoal->currentOrigin, self->NPC->goalRadius,
				static_cast<qboolean>((self->NPC->tempGoal->svFlags & SVF_NAVGOAL) != 0));
		}
		else
		{
			other->NPC->goalEntity = self->NPC->goalEntity;
		}
	}
	//give him my squadstate
	AI_GroupUpdateSquadstates(self->NPC->group, other, NPCInfo->squadState);

	//give him my timers and clear mine
	ST_TransferTimers(self, other);

	//now make me stand around for a second or two at least
	AI_GroupUpdateSquadstates(self->NPC->group, self, SQUAD_STAND_AND_SHOOT);
	TIMER_Set(self, "stand", Q_irand(1000, 3000));
}

static int ST_GetCPFlags()
{
	int cpFlags = 0;
	if (NPC && NPCInfo->group)
	{
		if (NPC == NPCInfo->group->commander && NPC->client->NPC_class == CLASS_IMPERIAL)
		{
			//imperials hang back and give orders
			if (NPCInfo->group->numGroup > 1 && Q_irand(-3, NPCInfo->group->numGroup) > 1)
			{
				//FIXME: make sure he;s giving orders with these lines
				if (Q_irand(0, 1))
				{
					ST_Speech(NPC, SPEECH_CHASE, 0.5);
				}
				else
				{
					ST_Speech(NPC, SPEECH_YELL, 0.5);
				}
			}
			cpFlags = CP_CLEAR | CP_COVER | CP_AVOID | CP_SAFE | CP_RETREAT;
		}
		else if (NPCInfo->group->morale < 0)
		{
			cpFlags = CP_COVER | CP_AVOID | CP_SAFE | CP_RETREAT;
			if (NPC->client->NPC_class == CLASS_SABOTEUR && !Q_irand(0, 3))
			{
				Saboteur_Cloak(NPC);
			}
		}
		else if (NPCInfo->group->morale < NPCInfo->group->numGroup)
		{
			//morale is low for our size
			const int morale_drop = NPCInfo->group->numGroup - NPCInfo->group->morale;
			if (morale_drop < -6)
			{
				//flee (no clear shot needed)
				cpFlags = CP_FLEE | CP_RETREAT | CP_COVER | CP_AVOID | CP_SAFE;
			}
			else if (morale_drop < -3)
			{
				//retreat (no clear shot needed)
				cpFlags = CP_RETREAT | CP_COVER | CP_AVOID | CP_SAFE;
			}
			else if (morale_drop < 0)
			{
				//cover (no clear shot needed)
				cpFlags = (CP_COVER | CP_AVOID | CP_SAFE);
			}
		}
		else
		{
			const int morale_boost = NPCInfo->group->morale - NPCInfo->group->numGroup;
			if (morale_boost > 20)
			{
				//charge to any one and outflank (no cover needed)
				cpFlags = CP_CLEAR | CP_FLANK | CP_APPROACH_ENEMY;
				//Saboteur_Decloak( NPC );
			}
			else if (morale_boost > 15)
			{
				//charge to closest one (no cover needed)
				cpFlags = CP_CLEAR | CP_CLOSEST | CP_APPROACH_ENEMY;
				if (NPC->client->NPC_class == CLASS_SABOTEUR && !Q_irand(0, 3))
				{
					Saboteur_Decloak(NPC);
				}
			}
			else if (morale_boost > 10)
			{
				//charge closer (no cover needed)
				cpFlags = CP_CLEAR | CP_APPROACH_ENEMY;
				if (NPC->client->NPC_class == CLASS_SABOTEUR && !Q_irand(0, 6))
				{
					Saboteur_Decloak(NPC);
				}
			}
		}
	}
	if (!cpFlags)
	{
		//at some medium level of morale
		switch (Q_irand(0, 3))
		{
		case 0: //just take the nearest one
			cpFlags = CP_CLEAR | CP_COVER | CP_NEAREST;
			break;
		case 1: //take one closer to the enemy
			cpFlags = CP_CLEAR | CP_COVER | CP_APPROACH_ENEMY;
			break;
		case 2: //take the one closest to the enemy
			cpFlags = CP_CLEAR | CP_COVER | CP_CLOSEST | CP_APPROACH_ENEMY;
			break;
		case 3: //take the one on the other side of the enemy
			cpFlags = CP_CLEAR | CP_COVER | CP_FLANK | CP_APPROACH_ENEMY;
			break;
		default:;
		}
	}
	if (NPC && NPCInfo->scriptFlags & SCF_USE_CP_NEAREST)
	{
		cpFlags &= ~(CP_FLANK | CP_APPROACH_ENEMY | CP_CLOSEST);
		cpFlags |= CP_NEAREST;
	}
	return cpFlags;
}

/*
-------------------------
ST_Commander

  Make decisions about who should go where, etc.

FIXME: leader (group-decision-making) AI?
FIXME: need alternate routes!
FIXME: more group voice interaction
FIXME: work in pairs?

-------------------------
*/
static void ST_Commander()
{
	int i, j;
	int cp, cp_flags;
	AIGroupInfo_t* group = NPCInfo->group;
	gentity_t* member;
	qboolean runner = qfalse;
	qboolean enemy_lost = qfalse;
	qboolean enemy_protected = qfalse;
	float avoid_dist;
	int squad_state;

	group->processed = qtrue;

	if (group->enemy == nullptr || group->enemy->client == nullptr)
	{
		//hmm, no enemy...?!
		return;
	}

	//FIXME: have this group commander check the enemy group (if any) and see if they have
	//		superior numbers.  If they do, fall back rather than advance.  If you have
	//		superior numbers, advance on them.
	//FIXME: find the group commander and have him occasionally give orders when there is speech
	//FIXME: start fleeing when only a couple of you vs. a lightsaber, possibly give up if the only one left

	SaveNPCGlobals();

	if (group->lastSeenEnemyTime < level.time - 180000)
	{
		//dissolve the group
		ST_Speech(NPC, SPEECH_LOST, 0.0f);
		group->enemy->waypoint = NAV::GetNearestNode(group->enemy);
		for (i = 0; i < group->numGroup; i++)
		{
			member = &g_entities[group->member[i].number];
			SetNPCGlobals(member);
			if (Q3_TaskIDPending(NPC, TID_MOVE_NAV))
			{
				//running somewhere that a script requires us to go, don't break from that
				continue;
			}
			if (!(NPCInfo->scriptFlags & SCF_CHASE_ENEMIES))
			{
				//not allowed to doMove on my own
				continue;
			}
			//Lost enemy for three minutes?  go into search mode?
			G_ClearEnemy(NPC);
			NPC->waypoint = NAV::GetNearestNode(group->enemy);
			if (NPC->waypoint == WAYPOINT_NONE)
			{
				NPCInfo->behaviorState = BS_DEFAULT; //BS_PATROL;
			}
			else if (group->enemy->waypoint == WAYPOINT_NONE || NAV::EstimateCostToGoal(
				NPC->waypoint, group->enemy->waypoint) >= Q3_INFINITE)
			{
				NPC_BSSearchStart(NPC->waypoint, BS_SEARCH);
			}
			else
			{
				NPC_BSSearchStart(group->enemy->waypoint, BS_SEARCH);
			}
		}
		group->enemy = nullptr;
		RestoreNPCGlobals();
		return;
	}

	if (group->numState[SQUAD_SCOUT] > 0 ||
		group->numState[SQUAD_TRANSITION] > 0 ||
		group->numState[SQUAD_RETREAT] > 0)
	{
		//someone is running
		runner = qtrue;
	}

	if (group->lastSeenEnemyTime > level.time - 32000 && group->lastSeenEnemyTime < level.time - 30000)
	{
		//no-one has seen the enemy for 30 seconds// and no-one is running after him
		if (group->commander && !Q_irand(0, 1))
		{
			ST_Speech(group->commander, SPEECH_ESCAPING, 0.0f);
		}
		else
		{
			ST_Speech(NPC, SPEECH_ESCAPING, 0.0f);
		}
		//don't say this again
		NPCInfo->blockedSpeechDebounceTime = level.time + 3000;
	}

	if (group->lastSeenEnemyTime < level.time - 7000)
	{
		//no-one has seen the enemy for at least 10 seconds!  Should send a scout
		enemy_lost = qtrue;
	}

	if (group->lastClearShotTime < level.time - 5000)
	{
		//no-one has had a clear shot for 5 seconds!
		enemy_protected = qtrue;
	}

	//Go through the list:

	//Everyone should try to get to a combat point if possible
	int cur_member_num, last_member_num;
	if (d_asynchronousGroupAI->integer)
	{
		//do one member a turn
		group->activeMemberNum++;
		if (group->activeMemberNum >= group->numGroup)
		{
			group->activeMemberNum = 0;
		}
		cur_member_num = group->activeMemberNum;
		last_member_num = cur_member_num + 1;
	}
	else
	{
		cur_member_num = 0;
		last_member_num = group->numGroup;
	}
	for (i = cur_member_num; i < last_member_num; i++)
	{
		//reset combat point flags
		cp = -1;
		cp_flags = 0;
		squad_state = SQUAD_IDLE;
		avoid_dist = 0;
		avoid_dist = 0;

		//get the next guy
		member = &g_entities[group->member[i].number];
		if (!member->enemy)
		{
			//don't include guys that aren't angry
			continue;
		}
		SetNPCGlobals(member);

		if (!TIMER_Done(NPC, "flee"))
		{
			//running away
			continue;
		}

		if (Q3_TaskIDPending(NPC, TID_MOVE_NAV))
		{
			//running somewhere that a script requires us to go
			continue;
		}

		if (NPC->s.weapon == WP_NONE
			&& NPCInfo->goalEntity
			&& NPCInfo->goalEntity == NPCInfo->tempGoal
			&& NPCInfo->goalEntity->s.eType == ET_ITEM)
		{
			//running to pick up a gun, don't do other logic
			continue;
		}

		//see if this member should start running (only if have no officer... FIXME: should always run from AEL_DANGER_GREAT?)
		if (!group->commander || group->commander->NPC->rank < RANK_ENSIGN)
		{
			if (NPC_CheckForDanger(NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER)))
			{
				//going to run
				ST_Speech(NPC, SPEECH_COVER, 0);
				continue;
			}
		}

		if (!(NPCInfo->scriptFlags & SCF_CHASE_ENEMIES))
		{
			//not allowed to do combat-movement
			continue;
		}

		if (NPCInfo->squadState != SQUAD_RETREAT)
		{
			//not already retreating
			if (NPC->client->ps.weapon == WP_NONE)
			{
				//weaponless, should be hiding
				if (NPCInfo->goalEntity == nullptr || NPCInfo->goalEntity->enemy == nullptr || NPCInfo->goalEntity->
					enemy->s.eType != ET_ITEM)
				{
					//not running after a pickup
					if (TIMER_Done(NPC, "hideTime") || DistanceSquared(group->enemy->currentOrigin, NPC->currentOrigin)
						< 65536 && NPC_ClearLOS(NPC->enemy))
					{
						//done hiding or enemy near and can see us
						//er, start another flee I guess?
						NPC_StartFlee(NPC->enemy, NPC->enemy->currentOrigin, AEL_DANGER_GREAT, 5000, 10000);
					} //else, just hang here
				}
				continue;
			}
			if (TIMER_Done(NPC, "roamTime") && TIMER_Done(NPC, "hideTime") && NPC->health > 10 && !gi.inPVS(
				group->enemy->currentOrigin, NPC->currentOrigin))
			{
				//cant even see enemy
				//better go after him
				cp_flags |= CP_CLEAR | CP_COVER;
			}
			else if (NPCInfo->localState == LSTATE_UNDERFIRE)
			{
				//we've been shot
				switch (group->enemy->client->ps.weapon)
				{
				case WP_SABER:
					if (DistanceSquared(group->enemy->currentOrigin, NPC->currentOrigin) < 65536) //256 squared
					{
						cp_flags |= CP_AVOID_ENEMY | CP_COVER | CP_AVOID | CP_RETREAT;
						if (!group->commander || group->commander->NPC->rank < RANK_ENSIGN)
						{
							squad_state = SQUAD_RETREAT;
						}
						avoid_dist = 256;
					}
					break;
				default:
				case WP_BLASTER:
					cp_flags |= CP_COVER;
					break;
				}
				if (NPC->health <= 10)
				{
					if (!group->commander || group->commander->NPC && group->commander->NPC->rank < RANK_ENSIGN)
					{
						cp_flags |= CP_FLEE | CP_AVOID | CP_RETREAT;
						squad_state = SQUAD_RETREAT;
					}
				}
			}
			else
			{
				//not hit, see if there are other reasons we should run
				if (gi.inPVS(NPC->currentOrigin, group->enemy->currentOrigin))
				{
					//in the same room as enemy
					if (NPC->client->ps.weapon == WP_ROCKET_LAUNCHER &&
						DistanceSquared(group->enemy->currentOrigin, NPC->currentOrigin) < MIN_ROCKET_DIST_SQUARED &&
						NPCInfo->squadState != SQUAD_TRANSITION)
					{
						//too close for me to fire my weapon and I'm not already on the move
						cp_flags |= CP_AVOID_ENEMY | CP_CLEAR | CP_AVOID;
						avoid_dist = 256;
					}
					else
					{
						switch (group->enemy->client->ps.weapon)
						{
						case WP_SABER:
							if (group->enemy->client->ps.SaberLength() > 0)
							{
								if (DistanceSquared(group->enemy->currentOrigin, NPC->currentOrigin) < 65536)
								{
									if (TIMER_Done(NPC, "hideTime"))
									{
										if (NPCInfo->squadState != SQUAD_TRANSITION)
										{
											//not already moving: FIXME: we need to see if where we're going is good now?
											cp_flags |= CP_AVOID_ENEMY | CP_CLEAR | CP_AVOID;
											avoid_dist = 256;
										}
									}
								}
							}
						default:
							break;
						}
					}
				}
			}
		}

		if (!cp_flags)
		{
			//okay, we have no new enemy-driven reason to run... let's use tactics now
			if (runner && NPCInfo->combatPoint != -1)
			{
				//someone is running and we have a combat point already
				if (NPCInfo->squadState != SQUAD_SCOUT &&
					NPCInfo->squadState != SQUAD_TRANSITION &&
					NPCInfo->squadState != SQUAD_RETREAT)
				{
					//it's not us
					if (TIMER_Done(NPC, "verifyCP") && DistanceSquared(NPC->currentOrigin,
						level.combatPoints[NPCInfo->combatPoint].origin)
								> 64 * 64)
					{
						//1 - 3 seconds have passed since you chose a CP, see if you're there since, for some reason, you've stopped running...
						//uh, WTF, we're not on our combat point?
						//er, try again, I guess?
						cp = NPCInfo->combatPoint;
						cp_flags |= ST_GetCPFlags();
					}
					else
					{
						//cover them
						//stop ducking
						TIMER_Set(NPC, "duck", -1);
						//start shooting
						TIMER_Set(NPC, "attackDelay", -1);
						//AI should take care of the rest - fire at enemy
					}
				}
				else
				{
					//we're running
					//see if we're blocked
					if (NPCInfo->aiFlags & NPCAI_BLOCKED)
					{
						//dammit, something is in our way
						//see if it's one of ours
						for (j = 0; j < group->numGroup; j++)
						{
							if (group->member[j].number == NPCInfo->blockingEntNum)
							{
								//we're being blocked by one of our own, pass our goal onto them and I'll stand still
								ST_TransferMoveGoal(NPC, &g_entities[group->member[j].number]);
								break;
							}
						}
					}
					//we don't need to do anything else
					continue;
				}
			}
			else
			{
				//okay no-one is running, use some tactics
				if (NPCInfo->combatPoint != -1)
				{
					//we have a combat point we're supposed to be running to
					if (NPCInfo->squadState != SQUAD_SCOUT &&
						NPCInfo->squadState != SQUAD_TRANSITION &&
						NPCInfo->squadState != SQUAD_RETREAT)
					{
						//but we're not running
						if (TIMER_Done(NPC, "verifyCP"))
						{
							//1 - 3 seconds have passed since you chose a CP, see if you're there since, for some reason, you've stopped running...
							if (DistanceSquared(NPC->currentOrigin,
								level.combatPoints[NPCInfo->combatPoint].origin) > 64 * 64)
							{
								//uh, WTF, we're not on our combat point?
								//er, try again, I guess?
								cp = NPCInfo->combatPoint;
								cp_flags |= ST_GetCPFlags();
							}
						}
					}
				}
				if (enemy_lost)
				{
					//if no-one has seen the enemy for a while, send a scout
					//ask where he went
					if (group->numState[SQUAD_SCOUT] <= 0)
					{
						//scouting = qtrue;
						NPC_ST_StoreMovementSpeech(SPEECH_CHASE, 0.0f);
					}
					//Since no-one else has done this, I should be the closest one, so go after him...
					ST_TrackEnemy(NPC, group->enemyLastSeenPos);
					//set me into scout mode
					AI_GroupUpdateSquadstates(group, NPC, SQUAD_SCOUT);
					//we're not using a cp, so we need to set runner to true right here
					runner = qtrue;
				}
				else if (enemy_protected)
				{
					//if no-one has a clear shot at the enemy, someone should go after him
					//FIXME: if I'm in an area where no safe combat points have a clear shot at me, they don't come after me... they should anyway, though after some extra hesitation.
					//ALSO: seem to give up when behind an area portal?
					//since no-one else here has done this, I should be the closest one
					if (TIMER_Done(NPC, "roamTime") && !Q_irand(0, group->numGroup))
					{
						//only do this if we're ready to move again and we feel like it
						cp_flags |= ST_ApproachEnemy(NPC);
						//set me into scout mode
						AI_GroupUpdateSquadstates(group, NPC, SQUAD_SCOUT);
					}
				}
				else
				{
					//group can see and has been shooting at the enemy
					//see if we should do something fancy?

					{
						//we're ready to move
						if (NPCInfo->combatPoint == -1)
						{
							//we're not on a combat point
							if (true) //!Q_irand( 0, 2 ) )
							{
								//we should go for a combat point
								cp_flags |= ST_GetCPFlags();
							}
						}
						else if (TIMER_Done(NPC, "roamTime"))
						{
							//we are already on a combat point
							if (i == 0)
							{
								//we're the closest
								if (group->morale - group->numGroup > 0 && !Q_irand(0, 4))
								{
									//try to outflank him
									cp_flags |= CP_CLEAR | CP_COVER | CP_FLANK | CP_APPROACH_ENEMY;
								}
								else if (group->morale - group->numGroup < 0)
								{
									//better move!
									cp_flags |= ST_GetCPFlags();
								}
								else
								{
									//If we're point, then get down
									TIMER_Set(NPC, "roamTime", Q_irand(2000, 5000));
									TIMER_Set(NPC, "stick", Q_irand(2000, 5000));
									//FIXME: what if we can't shoot from a ducked pos?
									TIMER_Set(NPC, "duck", Q_irand(3000, 4000));
									AI_GroupUpdateSquadstates(group, NPC, SQUAD_POINT);
								}
							}
							else if (i == group->numGroup - 1)
							{
								//farthest from the enemy
								if (group->morale - group->numGroup < 0)
								{
									//low morale, just hang here
									TIMER_Set(NPC, "roamTime", Q_irand(2000, 5000));
									TIMER_Set(NPC, "stick", Q_irand(2000, 5000));
								}
								else if (group->morale - group->numGroup > 0)
								{
									//try to move in on the enemy
									cp_flags |= ST_ApproachEnemy(NPC);
									//set me into scout mode
									AI_GroupUpdateSquadstates(group, NPC, SQUAD_SCOUT);
								}
								else
								{
									//use normal decision making process
									cp_flags |= ST_GetCPFlags();
								}
							}
							else
							{
								//someone in-between
								if (group->morale - group->numGroup < 0 || !Q_irand(0, 4))
								{
									//do something
									cp_flags |= ST_GetCPFlags();
								}
								else
								{
									TIMER_Set(NPC, "stick", Q_irand(2000, 4000));
									TIMER_Set(NPC, "roamTime", Q_irand(2000, 4000));
								}
							}
						}
					}
					if (!cp_flags)
					{
						//still not moving
						//see if we should do other fun stuff
						if (NPC->attackDebounceTime < level.time - 2000)
						{
							//we, personally, haven't shot for 2 seconds
							//maybe yell at the enemy?
							ST_Speech(NPC, SPEECH_LOOK, 0.9f);
						}
						//toy with ducking
						if (TIMER_Done(NPC, "duck"))
						{
							//not ducking
							if (TIMER_Done(NPC, "stand"))
							{
								//don't have to keep standing
								if (NPCInfo->combatPoint == -1 || level.combatPoints[NPCInfo->combatPoint].flags &
									CPF_DUCK)
								{
									//okay to duck here
									if (!Q_irand(0, 3))
									{
										TIMER_Set(NPC, "duck", Q_irand(1000, 3000));
									}
								}
							}
						}
					}
				}
			}
		}

		if (enemy_lost && NAV::InSameRegion(NPC, NPC->enemy->currentOrigin))
		{
			ST_TrackEnemy(NPC, NPC->enemy->currentOrigin);
			continue;
		}

		if (!NPC->enemy)
		{
			continue;
		}

		// Check To See We Have A Clear Shot To The Enemy Every Couple Seconds
		//---------------------------------------------------------------------
		if (TIMER_Done(NPC, "checkGrenadeTooCloseDebouncer"))
		{
			TIMER_Set(NPC, "checkGrenadeTooCloseDebouncer", Q_irand(300, 600));

			vec3_t mins{};
			vec3_t maxs{};
			bool fled = false;
			gentity_t* ent;

			gentity_t* entity_list[MAX_GENTITIES];

			for (int i1 = 0; i1 < 3; i1++)
			{
				mins[i1] = NPC->currentOrigin[i1] - 200;
				maxs[i1] = NPC->currentOrigin[i1] + 200;
			}

			int num_listed_entities = gi.EntitiesInBox(mins, maxs, entity_list, MAX_GENTITIES);

			for (int e = 0; e < num_listed_entities; e++)
			{
				ent = entity_list[e];

				if (ent == NPC)
					continue;
				if (ent->owner == NPC)
					continue;
				if (!ent->inuse)
					continue;
				if (ent->s.eType == ET_MISSILE)
				{
					if (ent->s.weapon == WP_THERMAL)
					{
						//a thermal
						if (ent->has_bounced && (!ent->owner || !OnSameTeam(ent->owner, NPC)))
						{
							//bounced and an enemy thermal
							ST_Speech(NPC, SPEECH_COVER, 0); //FIXME: flee sound?
							NPC_StartFlee(NPC->enemy, ent->currentOrigin, AEL_DANGER_GREAT, 1000, 2000);
							fled = true;
							TIMER_Set(NPC, "checkGrenadeTooCloseDebouncer", Q_irand(2000, 4000));
							break;
						}
					}
				}
			}
			if (fled)
			{
				continue;
			}
		}

		// Check To See We Have A Clear Shot To The Enemy Every Couple Seconds
		//---------------------------------------------------------------------
		if (TIMER_Done(NPC, "checkEnemyVisDebouncer"))
		{
			TIMER_Set(NPC, "checkEnemyVisDebouncer", Q_irand(3000, 7000));
			if (!NPC_ClearLOS(NPC->enemy))
			{
				cp_flags |= CP_CLEAR | CP_COVER; // NOPE, Can't See The Enemy, So Find A New Combat Point
			}
		}

		// Check To See If The Enemy Is Too Close For Comfort
		//----------------------------------------------------
		if (NPC->client->NPC_class != CLASS_ASSASSIN_DROID && NPC->client->NPC_class != CLASS_DROIDEKA)
		{
			if (TIMER_Done(NPC, "checkEnemyTooCloseDebouncer"))
			{
				TIMER_Set(NPC, "checkEnemyTooCloseDebouncer", Q_irand(1000, 6000));

				float dist_threshold = 16384/*128*128*/; //default
				switch (NPC->s.weapon)
				{
				case WP_ROCKET_LAUNCHER:
				case WP_FLECHETTE:
				case WP_THERMAL:
				case WP_TRIP_MINE:
				case WP_DET_PACK:
					dist_threshold = 65536/*256*256*/;
					break;
				case WP_REPEATER:
					if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
					{
						dist_threshold = 65536/*256*256*/;
					}
					break;
				case WP_CONCUSSION:
					if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
					{
						dist_threshold = 65536/*256*256*/;
					}
					break;
				default:
					break;
				}

				if (DistanceSquared(group->enemy->currentOrigin, NPC->currentOrigin) < dist_threshold)
				{
					cp_flags |= CP_CLEAR | CP_COVER;
				}
			}
		}

		//clear the local state
		NPCInfo->localState = LSTATE_NONE;

		cp_flags &= ~CP_NEAREST;
		//Assign combat points
		if (cp_flags)
		{
			//we want to run to a combat point
			//always avoid enemy when picking combat points, and we always want to be able to get there

			if (group->enemy->client->ps.weapon == WP_SABER && group->enemy->client->ps.SaberLength() > 0)
			{
				//we obviously want to avoid the enemy if he has a saber
				cp_flags |= CP_AVOID_ENEMY;
				avoid_dist = 256;
			}
			else
			{
				cp_flags |= CP_AVOID_ENEMY | CP_HAS_ROUTE | CP_TRYFAR;
				avoid_dist = 200;
			}

			//now get a combat point
			if (cp == -1)
			{
				//may have had sone set above
				cp = NPC_FindCombatPointRetry(NPC->currentOrigin, NPC->currentOrigin, NPC->currentOrigin, &cp_flags,
					avoid_dist, NPCInfo->lastFailedCombatPoint);
			}
			while (cp == -1 && cp_flags != CP_ANY)
			{
				//start "OR"ing out certain flags to see if we can find *any* point
				if (cp_flags & CP_INVESTIGATE)
				{
					//don't need to investigate
					cp_flags &= ~CP_INVESTIGATE;
				}
				else if (cp_flags & CP_SQUAD)
				{
					//don't need to stick to squads
					cp_flags &= ~CP_SQUAD;
				}
				else if (cp_flags & CP_DUCK)
				{
					//don't need to duck
					cp_flags &= ~CP_DUCK;
				}
				else if (cp_flags & CP_NEAREST)
				{
					//don't need closest one to me
					cp_flags &= ~CP_NEAREST;
				}
				else if (cp_flags & CP_FLANK)
				{
					//don't need to flank enemy
					cp_flags &= ~CP_FLANK;
				}
				else if (cp_flags & CP_SAFE)
				{
					//don't need one that hasn't been shot at recently
					cp_flags &= ~CP_SAFE;
				}
				else if (cp_flags & CP_CLOSEST)
				{
					//don't need to get closest to enemy
					cp_flags &= ~CP_CLOSEST;
					//but let's try to approach at least
					cp_flags |= CP_APPROACH_ENEMY;
				}
				else if (cp_flags & CP_APPROACH_ENEMY)
				{
					//don't need to approach enemy
					cp_flags &= ~CP_APPROACH_ENEMY;
				}
				else if (cp_flags & CP_COVER)
				{
					//don't need cover
					cp_flags &= ~CP_COVER;
					//but let's pick one that makes us duck
					cp_flags |= CP_DUCK;
				}
				else if (cp_flags & CP_CLEAR)
				{
					//don't need a clear shot to enemy
					cp_flags &= ~CP_CLEAR;
				}
				else if (cp_flags & CP_AVOID_ENEMY)
				{
					//don't need to avoid enemy
					cp_flags &= ~CP_AVOID_ENEMY;
				}
				else if (cp_flags & CP_RETREAT)
				{
					//don't need to retreat
					cp_flags &= ~CP_RETREAT;
				}
				else if (cp_flags & CP_FLEE)
				{
					//don't need to flee
					cp_flags &= ~CP_FLEE;
					//but at least avoid enemy and pick one that gives cover
					cp_flags |= CP_COVER | CP_AVOID_ENEMY;
				}
				else if (cp_flags & CP_AVOID)
				{
					//okay, even pick one right by me
					cp_flags &= ~CP_AVOID;
				}
				else
				{
					cp_flags = CP_ANY;
				}
				//now try again
				cp = NPC_FindCombatPoint(NPC->currentOrigin, NPC->currentOrigin, group->enemy->currentOrigin,
					cp_flags | CP_HAS_ROUTE, avoid_dist);
			}

			//see if we got a valid one
			if (cp != -1)
			{
				//found a combat point
				//let others know that someone is now running
				runner = qtrue;
				//let others know that someone is now running
				//don't change course again until we get to where we're going
				TIMER_Set(NPC, "roamTime", Q3_INFINITE);
				TIMER_Set(NPC, "verifyCP", Q_irand(1000, 3000)); //don't make sure you're in your CP for 1 - 3 seconds

				NPC_SetCombatPoint(cp);
				NPC_SetMoveGoal(NPC, level.combatPoints[cp].origin, 8, qtrue, cp);

				if (squad_state != SQUAD_IDLE)
				{
					AI_GroupUpdateSquadstates(group, NPC, squad_state);
				}
				else if (cp_flags & CP_FLEE)
				{
					//outright running for your life
					AI_GroupUpdateSquadstates(group, NPC, SQUAD_RETREAT);
				}
				else
				{
					//any other kind of transition between combat points
					AI_GroupUpdateSquadstates(group, NPC, SQUAD_TRANSITION);
				}
				// If Successfully
				if (cp_flags & CP_COVER && cp_flags & CP_CLEAR)
				{
					if (group->numGroup > 1)
					{
						NPC_ST_StoreMovementSpeech(SPEECH_OUTFLANK, -1);
					}
				} //okay, try a doMove right now to see if we can even get there
				else if (cp_flags & CP_FLANK)
				{
					if (group->numGroup > 1)
					{
						NPC_ST_StoreMovementSpeech(SPEECH_OUTFLANK, -1);
					}
				}
				else if (cp_flags & CP_COVER && !(cp_flags & CP_CLEAR))
				{
					//going into hiding
					NPC_ST_StoreMovementSpeech(SPEECH_COVER, -1);
				}
				else if (Q_irand(0, 3) == 0)
				{
					NPCInfo->aiFlags |= NPCAI_STOP_AT_LOS;
				}
				else
				{
					if (group->numGroup > 1)
					{
						float dot = 1.0f;
						if (!Q_irand(0, 3))
						{
							//25% of the time, see if we're flanking the enemy
							vec3_t e_dir2_me, e_dir2_cp;

							VectorSubtract(NPC->currentOrigin, group->enemy->currentOrigin, e_dir2_me);
							VectorNormalize(e_dir2_me);

							VectorSubtract(level.combatPoints[NPCInfo->combatPoint].origin, group->enemy->currentOrigin,
								e_dir2_cp);
							VectorNormalize(e_dir2_cp);

							dot = DotProduct(e_dir2_me, e_dir2_cp);
						}

						if (dot < 0.4)
						{
							//flanking!
							NPC_ST_StoreMovementSpeech(SPEECH_OUTFLANK, -1);
						}
						else if (!Q_irand(0, 10))
						{
							//regular movement
							NPC_ST_StoreMovementSpeech(SPEECH_YELL, 0.2f); //was SPEECH_COVER
						}
					}
					else if (cp_flags & CP_CLOSEST || cp_flags & CP_APPROACH_ENEMY)
					{
						if (group->numGroup > 1)
						{
							NPC_ST_StoreMovementSpeech(SPEECH_CHASE, 0.4f);
						}
					}
					else if (!Q_irand(0, 20))
					{
						//hell, we're loading the sounds, use them every now and then!
						if (Q_irand(0, 1))
						{
							NPC_ST_StoreMovementSpeech(SPEECH_OUTFLANK, -1);
						}
						else
						{
							NPC_ST_StoreMovementSpeech(SPEECH_ESCAPING, -1);
						}
					}
				}
			}
			else if (NPCInfo->squadState == SQUAD_SCOUT)
			{
				//we couldn't find a combatPoint by the player, so just go after him directly
				ST_HuntEnemy(NPC);
				//set me into scout mode
				AI_GroupUpdateSquadstates(group, NPC, SQUAD_SCOUT);
				//AI should take care of rest
			}
		}
	}

	RestoreNPCGlobals();
}

extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength, const qboolean breakSaberLock);

static void Noghri_StickTrace()
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
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Y, dir);
			VectorMA(base, 48, dir, tip);
#ifndef FINAL_BUILD
			if (d_saberCombat->integer > 1)
			{
				G_DebugLine(base, tip, FRAMETIME, 0x000000ff, qtrue);
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
					const int dmg = Q_irand(12, 20); //FIXME: base on skill!
					//FIXME: debounce?
					G_Sound(traceEnt, G_SoundIndex(va("sound/weapons/tusken_staff/stickhit%d.wav", Q_irand(1, 4))));
					G_Damage(traceEnt, NPC, NPC, vec3_origin, trace.endpos, dmg, DAMAGE_NO_KNOCKBACK, MOD_MELEE);
					if (traceEnt->health > 0 && dmg > 17)
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

void Noghri_StickTracennew(gentity_t* self)
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
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, POSITIVE_Y, dir);
			VectorMA(base, 48, dir, tip);
#ifndef FINAL_BUILD
			if (d_saberCombat->integer > 1)
			{
				G_DebugLine(base, tip, FRAMETIME, 0x000000ff, qtrue);
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
					const int dmg = Q_irand(12, 20); //FIXME: base on skill!
					//FIXME: debounce?
					G_Sound(traceEnt, G_SoundIndex(va("sound/weapons/tusken_staff/stickhit%d.wav", Q_irand(1, 4))));
					G_Damage(traceEnt, self, self, vec3_origin, trace.endpos, dmg, DAMAGE_NO_KNOCKBACK, MOD_MELEE);
					if (traceEnt->health > 0 && dmg > 17)
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

/*
-------------------------
NPC_BSST_Attack
-------------------------
*/
constexpr auto MELEE_DIST_SQUARED = 6400;
extern qboolean PM_InOnGroundAnims(int anim);
extern float NPC_EnemyRangeFromBolt(int bolt_index);

static qboolean Melee_CanDoGrab()
{
	if (NPC->client->NPC_class == CLASS_STORMTROOPER || NPC->client->NPC_class == CLASS_CLONETROOPER)
	{
		if (NPC->enemy && NPC->enemy->client)
		{
			//have a valid enemy
			if (TIMER_Done(NPC, "grabEnemyDebounce"))
			{
				//okay to grab again
				if (NPC->client->ps.groundEntityNum != ENTITYNUM_NONE
					&& NPC->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{
					//me and enemy are on ground
					if (!PM_InOnGroundAnims(NPC->enemy->client->ps.legsAnim))
					{
						if ((NPC->client->ps.weaponTime <= 200 || NPC->client->ps.torsoAnim == BOTH_KYLE_GRAB)
							&& !NPC->client->ps.saberInFlight)
						{
							if (fabs(NPC->enemy->currentOrigin[2] - NPC->currentOrigin[2]) <= 8.0f)
							{
								//close to same level of ground
								if (DistanceSquared(NPC->enemy->currentOrigin, NPC->currentOrigin) <= 10000.0f)
								{
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

static void Melee_GrabEnemy()
{
	TIMER_Set(NPC, "grabEnemyDebounce", NPC->client->ps.torsoAnimTimer + Q_irand(4000, 20000));
}

void NPC_BSST_Attack()
{
	//Don't do anything if we're hurt
	if (NPC->painDebounceTime > level.time)
	{
		NPC_UpdateAngles(qtrue, qtrue);

		if (NPC->client->ps.torsoAnim == BOTH_KYLE_GRAB)
		{
			//see if we grabbed enemy
			if (NPC->client->ps.torsoAnimTimer <= 200)
			{
				if (Melee_CanDoGrab()
					&& NPC_EnemyRangeFromBolt(0) <= 88.0f)
				{
					//grab him!
					Melee_GrabEnemy();
					return;
				}
				NPC_SetAnim(NPC, SETANIM_BOTH, BOTH_KYLE_MISS, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				NPC->client->ps.weaponTime = NPC->client->ps.torsoAnimTimer;
				return;
			}
		}
		else
		{
			ST_Speech(NPC, SPEECH_COVER, 0);

			npc_check_evasion();
		}
		return;
	}
	if (!in_camera && (TIMER_Done(NPC, "flee") && NPC_CheckForDanger(
		NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER))))
	{
		ST_Speech(NPC, SPEECH_COVER, 0);

		npc_check_evasion();
	}

	//If we don't have an enemy, just idle
	if (NPC_CheckEnemyExt() == qfalse)
	{
		if (NPC->client->playerTeam == TEAM_PLAYER)
		{
			NPC_BSPatrol();
		}
		else
		{
			NPC_BSST_Patrol(); //FIXME: or patrol?
		}
		return;
	}

	// now if this timer is done and the enemy is no longer in our line of sight, try to get a new enemy later
	if (TIMER_Done(NPC, "sje_check_enemy"))
	{
		TIMER_Set(NPC, "sje_check_enemy", Q_irand(5000, 10000));

		if (NPC->enemy && !NPC_ClearLOS(NPC->enemy) && NPC->health > 0)
		{
			// if enemy cant be seen, try getting one later
			if (NPC->client)
			{
				NPC->enemy = nullptr;
				if (NPC->client->playerTeam == TEAM_PLAYER)
				{
					NPC_BSPatrol();
				}
				else
				{
					NPC_BSST_Patrol();
				}
				return;
			}
			if (NPC->client)
			{
				// guardians have a different way to find enemies. He tries to find the quest player and his allies

				for (int sje_it = 0; sje_it < level.maxclients; sje_it++)
				{
					gentity_t* allied_player = &g_entities[sje_it];

					if (allied_player && allied_player->client &&
						NPC_ClearLOS(allied_player))
					{
						// the quest player or one of his allies. If one of them is in line of sight, choose him as enemy
						NPC->enemy = allied_player;
					}
				}
			}
		}
	}

	//Get our group info
	if (TIMER_Done(NPC, "interrogating"))
	{
		AI_GetGroup(NPC);
	}
	else
	{
		ST_Speech(NPC, SPEECH_YELL, 0);
	}

	if (NPCInfo->group)
	{
		//I belong to a squad of guys - we should *always* have a group
		if (!NPCInfo->group->processed)
		{
			//I'm the first ent in my group, I'll make the command decisions
#if	AI_TIMERS
			int	startTime = GetTime(0);
#endif//	AI_TIMERS
			ST_Commander();
#if	AI_TIMERS
			int commTime = GetTime(startTime);
			if (commTime > 20)
			{
				gi.Printf(S_COLOR_RED"ERROR: Commander time: %d\n", commTime);
			}
			else if (commTime > 10)
			{
				gi.Printf(S_COLOR_YELLOW"WARNING: Commander time: %d\n", commTime);
			}
			else if (commTime > 2)
			{
				gi.Printf(S_COLOR_GREEN"Commander time: %d\n", commTime);
			}
#endif//	AI_TIMERS
		}
	}
	else if (TIMER_Done(NPC, "flee") && NPC_CheckForDanger(NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_DANGER)))
	{
		//not already fleeing, and going to run
		ST_Speech(NPC, SPEECH_COVER, 0);
		NPC_UpdateAngles(qtrue, qtrue);
		return;
	}

	if (!NPC->enemy)
	{
		//WTF?  somehow we lost our enemy?
		NPC_BSST_Patrol(); //FIXME: or patrol?
		return;
	}

	if (NPCInfo->goalEntity && NPCInfo->goalEntity != NPC->enemy)
	{
		NPCInfo->goalEntity = UpdateGoal();
	}

	enemy_los = enemy_cs = enemyInFOV = qfalse;
	do_move = qtrue;
	face_enemy = qfalse;
	shoot = qfalse;
	hitAlly = qfalse;
	VectorClear(impactPos);
	enemyDist = DistanceSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);

	vec3_t enemy_dir, shoot_dir;
	VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, enemy_dir);
	VectorNormalize(enemy_dir);
	AngleVectors(NPC->client->ps.viewangles, shoot_dir, nullptr, nullptr);
	const float dot = DotProduct(enemy_dir, shoot_dir);
	if (dot > 0.5f || enemyDist * (1.0f - dot) < 10000)
	{
		//enemy is in front of me or they're very close and not behind me
		enemyInFOV = qtrue;
	}

	if (enemyDist < MIN_ROCKET_DIST_SQUARED) //128
	{
		//enemy within 128
		if ((NPC->client->ps.weapon == WP_FLECHETTE || NPC->client->ps.weapon == WP_REPEATER) &&
			NPCInfo->scriptFlags & SCF_ALT_FIRE)
		{
			//shooting an explosive, but enemy too close, switch to primary fire
			NPCInfo->scriptFlags &= ~SCF_ALT_FIRE;
			//FIXME: we can never go back to alt-fire this way since, after this, we don't know if we were initially supposed to use alt-fire or not...
		}
	}
	else if (enemyDist > 65536) //256 squared
	{
		if (NPC->client->ps.weapon == WP_DISRUPTOR)
		{
			//sniping...
			if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
			{
				//use primary fire
				NPCInfo->scriptFlags |= SCF_ALT_FIRE;
				//reset fire-timing variables
				NPC_ChangeWeapon(NPC->client->ps.weapon);
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}
	}

	//can we see our target?
	if (NPC_ClearLOS(NPC->enemy))
	{
		AI_GroupUpdateEnemyLastSeen(NPCInfo->group, NPC->enemy->currentOrigin);
		NPCInfo->enemyLastSeenTime = level.time;
		enemy_los = qtrue;

		if (NPC->client->ps.weapon == WP_NONE)
		{
			enemy_cs = qfalse; //not true, but should stop us from firing
			NPC_AimAdjust(-1); //adjust aim worse longer we have no weapon
		}
		else
		{
			//can we shoot our target?
			if (enemyDist < MIN_ROCKET_DIST_SQUARED &&
				level.time - NPC->lastMoveTime < 5000 &&
				(
					NPC->client->ps.weapon == WP_ROCKET_LAUNCHER
					|| NPC->client->ps.weapon == WP_CONCUSSION && !(NPCInfo->scriptFlags & SCF_ALT_FIRE)
					|| NPC->client->ps.weapon == WP_FLECHETTE && NPCInfo->scriptFlags & SCF_ALT_FIRE))
			{
				enemy_cs = qfalse; //not true, but should stop us from firing
				hitAlly = qtrue; //us!
				//FIXME: if too close, run away!
			}
			else if (enemyInFOV)
			{
				//if enemy is FOV, go ahead and check for shooting
				const int hit = NPC_ShotEntity(NPC->enemy, impactPos);
				const gentity_t* hit_ent = &g_entities[hit];

				if (hit == NPC->enemy->s.number
					|| hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPC->client->enemyTeam
					|| hit_ent && hit_ent->takedamage && (hit_ent->svFlags & SVF_GLASS_BRUSH || hit_ent->health < 40 ||
						NPC->s.weapon == WP_EMPLACED_GUN))
				{
					//can hit enemy or enemy ally or will hit glass or other minor breakable (or in emplaced gun), so shoot anyway
					AI_GroupUpdateClearShotTime(NPCInfo->group);
					enemy_cs = qtrue;
					NPC_AimAdjust(2); //adjust aim better longer we have clear shot at enemy
					VectorCopy(NPC->enemy->currentOrigin, NPCInfo->enemyLastSeenLocation);
				}
				else
				{
					//Hmm, have to get around this bastard
					NPC_AimAdjust(2); //adjust aim better longer we can see enemy
					ST_ResolveBlockedShot(hit);
					if (hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPC->client->playerTeam)
					{
						//would hit an ally, don't fire!!!
						hitAlly = qtrue;
					}
					else
					{
						//Check and see where our shot *would* hit... if it's not close to the enemy (within 256?), then don't fire
					}
				}
			}
			else
			{
				enemy_cs = qfalse; //not true, but should stop us from firing
			}
		}
	}
	else if (gi.inPVS(NPC->enemy->currentOrigin, NPC->currentOrigin))
	{
		NPCInfo->enemyLastSeenTime = level.time;
		face_enemy = qtrue;
		NPC_AimAdjust(-1); //adjust aim worse longer we cannot see enemy
	}

	if (NPC->client->ps.weapon == WP_NONE)
	{
		face_enemy = qfalse;
		shoot = qfalse;
	}
	else
	{
		if (enemy_los)
		{
			//FIXME: no need to face enemy if we're moving to some other goal and he's too far away to shoot?
			face_enemy = qtrue;
		}
		if (enemy_cs)
		{
			shoot = qtrue;
		}
	}

	//Check for movement to take care of
	ST_CheckMoveState();

	//See if we should override shooting decision with any special considerations
	ST_CheckFireState();

	if (face_enemy)
	{
		//face the enemy
		NPC_FaceEnemy(qtrue);
	}

	if (!(NPCInfo->scriptFlags & SCF_CHASE_ENEMIES))
	{
		//not supposed to chase my enemies
		if (NPCInfo->goalEntity == NPC->enemy)
		{
			//goal is my entity, so don't doMove
			do_move = qfalse;
		}
	}
	else if (NPC->NPC->scriptFlags & SCF_NO_GROUPS)
	{
		//	NPCInfo->goalEntity = UpdateGoal();

		NPCInfo->goalEntity = enemy_los ? nullptr : NPC->enemy;
	}

	if (NPC->client->fireDelay && NPC->s.weapon == WP_ROCKET_LAUNCHER)
	{
		do_move = qfalse;
	}

	if (!ucmd.rightmove)
	{
		//only if not already strafing for some strange reason...?
		//NOTE: these are never set here, but can be set in AI_Jedi.cpp for those NPCs who are sort of Stormtrooper/Jedi hybrids
		//NOTE: this stomps navigation movement entirely!
		//FIXME: if enemy behind me and turning to face enemy, don't strafe in that direction, too
		if (!TIMER_Done(NPC, "strafeLeft"))
		{
			ucmd.rightmove = -127;
			//re-check the duck as we might want to be rolling
			VectorClear(NPC->client->ps.moveDir);
			do_move = qfalse;
		}
		else if (!TIMER_Done(NPC, "strafeRight"))
		{
			//go ahead and strafe left
			ucmd.rightmove = 127;
			VectorClear(NPC->client->ps.moveDir);
			do_move = qfalse;
		}
	}

	if (NPC->client->ps.legsAnim == BOTH_GUARD_LOOKAROUND1)
	{
		//don't doMove when doing silly look around thing
		do_move = qfalse;
	}
	if (do_move)
	{
		//doMove toward goal
		if (NPCInfo->goalEntity) //&& ( NPCInfo->goalEntity != NPC->enemy || enemyDist > 10000 ) )//100 squared
		{
			do_move = ST_Move();
			if ((NPC->client->NPC_class != CLASS_ROCKETTROOPER || NPC->s.weapon != WP_ROCKET_LAUNCHER || enemyDist <
				MIN_ROCKET_DIST_SQUARED)
				//rockettroopers who use rocket launchers turn around and run if you get too close (closer than 128)
				&& ucmd.forwardmove <= -32)
			{
				//moving backwards at least 45 degrees
				if (NPCInfo->goalEntity
					&& DistanceSquared(NPCInfo->goalEntity->currentOrigin, NPC->currentOrigin) >
					MIN_TURN_AROUND_DIST_SQ)
				{
					//don't stop running backwards if your goal is less than 100 away
					if (TIMER_Done(NPC, "runBackwardsDebounce"))
					{
						//not already waiting for next run backwards
						if (!TIMER_Exists(NPC, "runningBackwards"))
						{
							//start running backwards
							TIMER_Set(NPC, "runningBackwards", Q_irand(500, 1000)); //Q_irand( 2000, 3500 ) );
						}
						else if (TIMER_Done2(NPC, "runningBackwards", qtrue))
						{
							//done running backwards
							TIMER_Set(NPC, "runBackwardsDebounce", Q_irand(3000, 5000));
						}
					}
				}
			}
			else
			{
				//not running backwards
				//TIMER_Remove( NPC, "runningBackwards" );
			}
		}
		else
		{
			do_move = qfalse;
		}
	}

	if (!do_move)
	{
		if (NPC->client->NPC_class != CLASS_ASSASSIN_DROID && NPC->client->NPC_class != CLASS_DROIDEKA)
		{
			if (!TIMER_Done(NPC, "duck"))
			{
				ucmd.upmove = -127;
			}
		}
		//FIXME: what about leaning?
	}
	else
	{
		//stop ducking!
		TIMER_Set(NPC, "duck", -1);
	}

	//////////////////////////////////////////////////////////////////////// slap start

	//SLAP
	if (!Q_irand(0, 3) && g_SerenityJediEngineMode->integer > 0 && g_allowgunnerbash->integer > 0 &&
		char_can_gun_bash(NPC) && NPC->client->NPC_class != CLASS_SBD && !PM_InKnockDown(&NPC->client->ps))
	{
		if (NPC->client->ps.torsoAnim == BOTH_TUSKENATTACK2 || NPC->client->ps.torsoAnim == BOTH_A7_HILT)
		{
			shoot = qfalse;
			if (TIMER_Done(NPC, "smackTime") && !NPCInfo->blockedDebounceTime)
			{
				//time to smack
				//recheck enemyDist and InFront
				if (enemyDist < MELEE_DIST_SQUARED
					&& !NPC->client->ps.weaponTime //not firing
					&& !PM_InKnockDown(&NPC->client->ps) //not knocked down
					&& InFront(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles, 0.3f))
				{
					vec3_t smack_dir;
					VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, smack_dir);
					smack_dir[2] += 30;
					VectorNormalize(smack_dir);
					//hurt them
					G_Sound(NPC->enemy, G_SoundIndex(va("sound/weapons/melee/punch%d", Q_irand(1, 4))));
					G_Damage(NPC->enemy, NPC, NPC, smack_dir, NPC->currentOrigin,
						(g_spskill->integer + 1) * Q_irand(2, 5), DAMAGE_NO_KNOCKBACK, MOD_MELEE);
					WP_AbsorbKick(NPC->enemy, NPC, smack_dir);
					//done with the damage
					NPCInfo->blockedDebounceTime = 1;
				}
			}
		}
		else
		{
			if (enemyDist < MELEE_DIST_SQUARED
				&& !NPC->client->ps.weaponTime //not firing
				&& !PM_InKnockDown(&NPC->client->ps) //not knocked down
				&& InFront(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles, 0.3f))
				//within 80 and in front
			{
				//enemy within 80, if very close, use melee attack to slap away
				if (TIMER_Done(NPC, "slapattackDelay"))
				{
					//animate me
					int swing_anim;
					if (NPC->health > BLOCKPOINTS_HALF)
					{
						swing_anim = BOTH_TUSKENATTACK2;
					}
					else
					{
						swing_anim = BOTH_A7_HILT;
					}
					G_AddVoiceEvent(NPC, Q_irand(EV_OUTFLANK1, EV_OUTFLANK2), 2000);
					NPC_SetAnim(NPC, SETANIM_BOTH, swing_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					if (NPC->health > BLOCKPOINTS_HALF)
					{
						TIMER_Set(NPC, "slapattackDelay", NPC->client->ps.torsoAnimTimer + Q_irand(12000, 18000));
					}
					else
					{
						TIMER_Set(NPC, "slapattackDelay", NPC->client->ps.torsoAnimTimer + Q_irand(6000, 12000));
					}
					//delay the hurt until the proper point in the anim
					TIMER_Set(NPC, "smackTime", 300);
					NPCInfo->blockedDebounceTime = 0;
				}
			}
		}
	}

	///////////////////////////////////////////SBD ///////////////////////////////////////////////

	if (g_SerenityJediEngineMode->integer > 0 && g_allowgunnerbash->integer > 0 &&
		NPC->client->NPC_class == CLASS_SBD && !PM_InKnockDown(&NPC->client->ps))
	{
		if (NPC->client->ps.torsoAnim == BOTH_SLAP_R || NPC->client->ps.torsoAnim == BOTH_SLAP_L)
		{
			shoot = qfalse;
			if (TIMER_Done(NPC, "smackTime") && !NPCInfo->blockedDebounceTime)
			{
				//time to smack
				//recheck enemyDist and InFront
				if (enemyDist < MELEE_DIST_SQUARED
					&& !NPC->client->ps.weaponTime //not firing
					&& !PM_InKnockDown(&NPC->client->ps) //not knocked down
					&& InFront(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles, 0.3f))
				{
					vec3_t smack_dir;
					VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, smack_dir);
					smack_dir[2] += 30;
					VectorNormalize(smack_dir);
					//hurt them
					G_Sound(NPC->enemy, G_SoundIndex(va("sound/weapons/melee/punch%d", Q_irand(1, 4))));
					G_Damage(NPC->enemy, NPC, NPC, smack_dir, NPC->currentOrigin,
						(g_spskill->integer + 1) * Q_irand(2, 5), DAMAGE_NO_KNOCKBACK, MOD_MELEE);

					WP_AbsorbKick(NPC->enemy, NPC, smack_dir);
					//done with the damage
					NPCInfo->blockedDebounceTime = 1;
				}
			}
		}
		else
		{
			if (enemyDist < MELEE_DIST_SQUARED
				&& !NPC->client->ps.weaponTime //not firing
				&& !PM_InKnockDown(&NPC->client->ps) //not knocked down
				&& InFront(NPC->enemy->currentOrigin, NPC->currentOrigin, NPC->client->ps.viewangles, 0.3f))
				//within 80 and in front
			{
				//enemy within 80, if very close, use melee attack to slap away
				if (TIMER_Done(NPC, "slapattackDelay"))
				{
					//animate me
					int swing_anim;
					if (NPC->health > BLOCKPOINTS_THIRTY)
					{
						swing_anim = BOTH_SLAP_R;
					}
					else
					{
						swing_anim = BOTH_SLAP_L;
					}
					G_AddVoiceEvent(NPC, Q_irand(EV_OUTFLANK1, EV_OUTFLANK2), 2000);
					NPC_SetAnim(NPC, SETANIM_BOTH, swing_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					if (NPC->health > BLOCKPOINTS_THIRTY)
					{
						TIMER_Set(NPC, "slapattackDelay", NPC->client->ps.torsoAnimTimer + Q_irand(12000, 18000));
					}
					else
					{
						TIMER_Set(NPC, "slapattackDelay", NPC->client->ps.torsoAnimTimer + Q_irand(6000, 12000));
					}
					//delay the hurt until the proper point in the anim
					TIMER_Set(NPC, "smackTime", 300);
					NPCInfo->blockedDebounceTime = 0;
				}
			}
		}
	}
	//slap end

	if (NPC->client->NPC_class == CLASS_REBORN //cultist using a gun
		&& NPCInfo->rank >= RANK_LT_COMM //commando or better
		&& NPC->enemy->s.weapon == WP_SABER) //fighting a saber-user
	{
		//see if we need to avoid their saber
		npc_evasion_saber();
	}

	if ( //!TIMER_Done( NPC, "flee" ) ||
		do_move && !TIMER_Done(NPC, "runBackwardsDebounce"))
	{
		//running away
		face_enemy = qfalse;
	}

	if (!face_enemy)
	{
		//we want to face in the dir we're running
		if (!do_move)
		{
			//if we haven't moved, we should look in the direction we last looked?
			VectorCopy(NPC->client->ps.viewangles, NPCInfo->lastPathAngles);
		}
		NPCInfo->desiredYaw = NPCInfo->lastPathAngles[YAW];
		NPCInfo->desiredPitch = 0;
		NPC_UpdateAngles(qtrue, qtrue);
		if (do_move)
		{
			//don't run away and shoot
			shoot = qfalse;
		}
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
	//FIXME: don't shoot right away!
	if (NPC->client->fireDelay)
	{
		if (NPC->client->NPC_class == CLASS_SABOTEUR)
		{
			Saboteur_Decloak(NPC);
		}
		if (NPC->s.weapon == WP_ROCKET_LAUNCHER
			|| NPC->s.weapon == WP_CONCUSSION && !(NPCInfo->scriptFlags & SCF_ALT_FIRE))
		{
			if (!enemy_los || !enemy_cs)
			{
				//cancel it
				NPC->client->fireDelay = 0;
			}
			else
			{
				//delay our next attempt
				TIMER_Set(NPC, "attackDelay", Q_irand(3000, 5000));
			}
		}
	}
	else if (shoot)
	{
		//try to shoot if it's time
		if (NPC->client->NPC_class == CLASS_SABOTEUR)
		{
			Saboteur_Decloak(NPC);
		}
		if (TIMER_Done(NPC, "attackDelay"))
		{
			if (!(NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
			{
				WeaponThink();
			}
			//NASTY
			if (NPC->s.weapon == WP_ROCKET_LAUNCHER)
			{
				if (ucmd.buttons & BUTTON_ATTACK
					&& !do_move
					&& g_spskill->integer > 1
					&& !Q_irand(0, 3))
				{
					//every now and then, shoot a homing rocket
					ucmd.buttons &= ~BUTTON_ATTACK;
					ucmd.buttons |= BUTTON_ALT_ATTACK;
					NPC->client->fireDelay = Q_irand(1000, 2500);
				}
			}
			else if (NPC->s.weapon == WP_NOGHRI_STICK
				&& enemyDist < 48 * 48) //?
			{
				ucmd.buttons &= ~BUTTON_ATTACK;
				ucmd.buttons |= BUTTON_ALT_ATTACK;
				NPC->client->fireDelay = Q_irand(1500, 2000);
			}
		}
	}
	else
	{
		if (NPC->attackDebounceTime < level.time)
		{
			if (NPC->client->NPC_class == CLASS_SABOTEUR)
			{
				Saboteur_Cloak(NPC);
			}
		}
	}
}

extern qboolean G_TuskenAttackAnimDamage(gentity_t* self);

void NPC_BSST_Default()
{
	if (NPCInfo->scriptFlags & SCF_FIRE_WEAPON)
	{
		WeaponThink();
	}

	if (NPC->s.weapon == WP_NOGHRI_STICK)
	{
		if (G_TuskenAttackAnimDamage(NPC))
		{
			//Noghri_StickTrace();
			Noghri_StickTracennew(NPC);
		}
	}

	if (!NPC->enemy)
	{
		//don't have an enemy, look for one
		NPC_BSST_Patrol();
	}
	else //if ( NPC->enemy )
	{
		//have an enemy
		if (NPC->enemy->client //enemy is a client
			&& (NPC->enemy->client->NPC_class == CLASS_UGNAUGHT || NPC->enemy->client->NPC_class == CLASS_JAWA)
			//enemy is a lowly jawa or ugnaught
			&& NPC->enemy->enemy != NPC //enemy's enemy is not me
			&& (!NPC->enemy->enemy || !NPC->enemy->enemy->client || NPC->enemy->enemy->client->NPC_class !=
				CLASS_RANCOR && NPC->enemy->enemy->client->NPC_class != CLASS_WAMPA))
			//enemy's enemy is not a client or is not a wampa or rancor (which is scarier than me)
		{
			//they should be scared of ME and no-one else
			G_SetEnemy(NPC->enemy, NPC);
		}
		NPC_CheckGetNewWeapon();
		NPC_BSST_Attack();

		npc_check_speak(NPC);
	}
}