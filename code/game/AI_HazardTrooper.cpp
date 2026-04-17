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

////////////////////////////////////////////////////////////////////////////////////////
// RAVEN SOFTWARE - STAR WARS: JK II
//  (c) 2002 Activision
//
// Troopers
//
// TODO
// ----
//
//
//
//

////////////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////////////
#include "b_local.h"
#include "g_navigator.h"
#if !defined(RAVL_VEC_INC)
#include "../Ravl/CVec.h"
#endif
#if !defined(RATL_ARRAY_VS_INC)
#include "../Ratl/array_vs.h"
#endif
#if !defined(RATL_VECTOR_VS_INC)
#include "../Ratl/vector_vs.h"
#endif
#if !defined(RATL_HANDLE_POOL_VS_INC)
#include "../Ratl/handle_pool_vs.h"
#endif
#if !defined(RUFL_HSTRING_INC)
#include "../Rufl/hstring.h"
#endif
#include <Ratl/pool_vs.h>
#include "bstate.h"
#include "ai.h"
#include "surfaceflags.h"
#include "ghoul2_shared.h"
#include "anims.h"
#include <qcommon/q_platform.h>
#include <qcommon/q_shared.h>
#include "weapons.h"
#include "b_public.h"
#include "g_shared.h"
#include <cassert>
#include "teams.h"
#include <qcommon/q_math.h>
#include "bg_public.h"

////////////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////////////
constexpr auto MAX_TROOPS = 100;
constexpr auto MAX_ENTS_PER_TROOP = 7;
constexpr auto MAX_TROOP_JOIN_DIST2 = 1000000; //1000 units;
constexpr auto MAX_TROOP_MERGE_DIST2 = 250000; //500 units;
constexpr auto TARGET_POS_VISITED = 10000; //100 units;

bool NPC_IsTrooper(const gentity_t* actor);

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

extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern void CG_DrawEdge(vec3_t start, vec3_t end, int type);

static void HT_Speech(const gentity_t* self, const int speech_type, const float fail_chance)
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
	}

	TIMER_Set(self, "chatter", Q_irand(2000, 4000));

	if (self->NPC->blockedSpeechDebounceTime > level.time)
	{
		return;
	}

	switch (speech_type)
	{
	case SPEECH_CHASE:
		G_AddVoiceEvent(self, Q_irand(EV_CHASE1, EV_CHASE3), 2000);
		break;
	case SPEECH_CONFUSED:
		G_AddVoiceEvent(self, Q_irand(EV_CONFUSE1, EV_CONFUSE3), 2000);
		break;
	case SPEECH_COVER:
		G_AddVoiceEvent(self, Q_irand(EV_COVER1, EV_COVER5), 2000);
		break;
	case SPEECH_DETECTED:
		G_AddVoiceEvent(self, Q_irand(EV_DETECTED1, EV_DETECTED5), 2000);
		break;
	case SPEECH_GIVEUP:
		G_AddVoiceEvent(self, Q_irand(EV_GIVEUP1, EV_GIVEUP4), 2000);
		break;
	case SPEECH_LOOK:
		G_AddVoiceEvent(self, Q_irand(EV_LOOK1, EV_LOOK2), 2000);
		break;
	case SPEECH_LOST:
		G_AddVoiceEvent(self, EV_LOST1, 2000);
		break;
	case SPEECH_OUTFLANK:
		G_AddVoiceEvent(self, Q_irand(EV_OUTFLANK1, EV_OUTFLANK2), 2000);
		break;
	case SPEECH_ESCAPING:
		G_AddVoiceEvent(self, Q_irand(EV_ESCAPING1, EV_ESCAPING3), 2000);
		break;
	case SPEECH_SIGHT:
		G_AddVoiceEvent(self, Q_irand(EV_SIGHT1, EV_SIGHT3), 2000);
		break;
	case SPEECH_SOUND:
		G_AddVoiceEvent(self, Q_irand(EV_SOUND1, EV_SOUND3), 2000);
		break;
	case SPEECH_SUSPICIOUS:
		G_AddVoiceEvent(self, Q_irand(EV_SUSPICIOUS1, EV_SUSPICIOUS5), 2000);
		break;
	case SPEECH_YELL:
		G_AddVoiceEvent(self, Q_irand(EV_ANGER1, EV_ANGER3), 2000);
		break;
	case SPEECH_PUSHED:
		G_AddVoiceEvent(self, Q_irand(EV_PUSHED1, EV_PUSHED3), 2000);
		break;
	default:
		break;
	}

	self->NPC->blockedSpeechDebounceTime = level.time + 2000;
}

////////////////////////////////////////////////////////////////////////////////////////
// The Troop
////////////////////////////////////////////////////////////////////////////////////////
class c_troop
{
	////////////////////////////////////////////////////////////////////////////////////
	// Various Troop Wide Data
	////////////////////////////////////////////////////////////////////////////////////
	int  mTroopHandle = 0;
	int  mTroopTeam = 0;
	bool mTroopReform = false;

	float mFormSpacingFwd = 0.0f;
	float mFormSpacingRight = 0.0f;

public:
	bool Empty() const { return mActors.empty(); }
	int  Team()  const { return mTroopTeam; }
	int  Handle() const { return mTroopHandle; }

	////////////////////////////////////////////////////////////////////////////////////
	// Initialize - Clear out all data, all actors, reset all variables
	////////////////////////////////////////////////////////////////////////////////////
	void Initialize(const int TroopHandle = 0)
	{
		mActors.clear();
		mTarget = nullptr;
		mState = TS_NONE;
		mTroopHandle = TroopHandle;
		mTroopTeam = 0;
		mTroopReform = false;
		mTargetVisable = false;
		mTargetVisableStartTime = 0;
		mTargetVisableStopTime = 0;
		mTargetIndex = 0;
		mTargetLastKnownTime = 0;
		mTargetLastKnownPositionVisited = false;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// DistanceSq - Quick Operation to see how far an ent is from the rest of the troop
	////////////////////////////////////////////////////////////////////////////////////
	float DistanceSq(const gentity_t* ent) const
	{
		if (!ent || mActors.empty() || !mActors[0])
		{
			return 0.0f;
		}
		return DistanceSquared(ent->currentOrigin, mActors[0]->currentOrigin);
	}

private:
	////////////////////////////////////////////////////////////////////////////////////
	// The Actors
	////////////////////////////////////////////////////////////////////////////////////
	ratl::vector_vs<gentity_t*, MAX_ENTS_PER_TROOP> mActors;

	////////////////////////////////////////////////////////////////////////////////////
	// MakeActorLeader - Move A Given Index To A Leader Position
	////////////////////////////////////////////////////////////////////////////////////
	void MakeActorLeader(const int index)
	{
		if (mActors.empty())
		{
			return;
		}

		if (index < 0 || index >= mActors.size())
		{
			return;
		}

		gentity_t* oldLeader = mActors[0];

		if (oldLeader && oldLeader->client)
		{
			oldLeader->client->leader = nullptr;
		}

		if (index != 0)
		{
			mActors.swap(index, 0);
		}

		gentity_t* newLeader = mActors[0];

		if (newLeader && newLeader->client)
		{
			newLeader->client->leader = newLeader;

			if (newLeader->client->NPC_class == CLASS_HAZARD_TROOPER)
			{
				mFormSpacingFwd = 75.0f;
				mFormSpacingRight = 50.0f;
			}
			else
			{
				mFormSpacingFwd = 75.0f;
				mFormSpacingRight = 20.0f;
			}
		}
	}

public:
	////////////////////////////////////////////////////////////////////////////////////
	// AddActor - Adds a new actor to the troop & automatically promote to leader
	////////////////////////////////////////////////////////////////////////////////////
	void AddActor(gentity_t* actor)
	{
		if (!actor || !actor->NPC || !actor->client || mActors.full())
		{
			return;
		}

		assert(actor->NPC->troop == 0 && !mActors.full());

		actor->NPC->troop = mTroopHandle;
		mActors.push_back(actor);
		mTroopReform = true;

		if (mActors.size() == 1 || actor->NPC->rank > mActors[0]->NPC->rank)
		{
			MakeActorLeader(mActors.size() - 1);
		}

		if (!mTroopTeam)
		{
			mTroopTeam = actor->client->playerTeam;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// RemoveActor - Removes an actor from the troop & automatically promote leader
	////////////////////////////////////////////////////////////////////////////////////
	void RemoveActor(const gentity_t* actor)
	{
		if (!actor || !actor->NPC)
		{
			return;
		}

		assert(actor->NPC->troop == mTroopHandle);

		int bestNewLeader = -1;
		int num_ents = mActors.size();
		mTroopReform = true;

		for (int i = 0; i < num_ents; )
		{
			if (mActors[i] == actor)
			{
				mActors.erase_swap(i);
				num_ents--;

				if (i == 0 && !mActors.empty())
				{
					bestNewLeader = 0;
				}
				continue;
			}

			if (bestNewLeader >= 0 &&
				mActors[i] && mActors[bestNewLeader] &&
				mActors[i]->NPC && mActors[bestNewLeader]->NPC &&
				mActors[i]->NPC->rank > mActors[bestNewLeader]->NPC->rank)
			{
				bestNewLeader = i;
			}

			i++;
		}

		if (!mActors.empty() && bestNewLeader >= 0)
		{
			MakeActorLeader(bestNewLeader);
		}

		const_cast<gentity_t*>(actor)->NPC->troop = 0;
	}

private:
	////////////////////////////////////////////////////////////////////////////////////
	// Enemy
	////////////////////////////////////////////////////////////////////////////////////
	gentity_t* mTarget = nullptr;
	bool       mTargetVisable = false;
	int        mTargetVisableStartTime = 0;
	int        mTargetVisableStopTime = 0;
	CVec3      mTargetVisablePosition;
	int        mTargetIndex = 0;
	int        mTargetLastKnownTime = 0;
	CVec3      mTargetLastKnownPosition;
	bool       mTargetLastKnownPositionVisited = false;

	////////////////////////////////////////////////////////////////////////////////////
	// RegisterTarget - Records That the target is seen, when and where
	////////////////////////////////////////////////////////////////////////////////////
	void RegisterTarget(gentity_t* target, const int index, const bool visable)
	{
		if (!target || mActors.empty() || !mActors[0])
		{
			return;
		}

		if (!mTarget)
		{
			HT_Speech(mActors[0], SPEECH_DETECTED, 0);
		}
		else if (level.time - mTargetLastKnownTime > 8000)
		{
			HT_Speech(mActors[0], SPEECH_SIGHT, 0);
		}

		if (visable)
		{
			mTargetVisableStopTime = level.time;
			if (!mTargetVisable)
			{
				mTargetVisableStartTime = level.time;
			}

			CalcEntitySpot(target, SPOT_HEAD, mTargetVisablePosition.v);
			mTargetVisablePosition[2] -= 10.0f;
		}

		mTarget = target;
		mTargetVisable = visable;
		mTargetIndex = index;
		mTargetLastKnownTime = level.time;
		mTargetLastKnownPosition = target->currentOrigin;
		mTargetLastKnownPositionVisited = false;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// TargetLastKnownPositionVisited
	////////////////////////////////////////////////////////////////////////////////////
	bool TargetLastKnownPositionVisited()
	{
		if (!mTargetLastKnownPositionVisited && !mActors.empty() && mActors[0])
		{
			const float dist = DistanceSquared(mTargetLastKnownPosition.v, mActors[0]->currentOrigin);
			mTargetLastKnownPositionVisited = dist < TARGET_POS_VISITED;
		}
		return mTargetLastKnownPositionVisited;
	}

	static float ClampScale(float val)
	{
		if (val > 1.0f)
		{
			val = 1.0f;
		}
		if (val < 0.0f)
		{
			val = 0.0f;
		}
		return val;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Target Visibility
	////////////////////////////////////////////////////////////////////////////////////
	static float TargetVisibility(const gentity_t* target)
	{
		if (!target)
		{
			return 0.0f;
		}

		float Scale = 0.8f;
		if (target->client &&
			target->client->ps.weapon == WP_SABER &&
			target->client->ps.SaberActive())
		{
			Scale += 0.1f;
		}
		return ClampScale(Scale);
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Target Noise Level
	////////////////////////////////////////////////////////////////////////////////////
	static float TargetNoiseLevel(const gentity_t* target)
	{
		if (!target)
		{
			return 0.0f;
		}

		float Scale = 0.1f;
		if (g_speed && g_speed->integer > 0)
		{
			Scale += target->resultspeed / static_cast<float>(g_speed->integer);
		}
		if (target->client &&
			target->client->ps.weapon == WP_SABER &&
			target->client->ps.SaberActive())
		{
			Scale += 0.2f;
		}
		return ClampScale(Scale);
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Scan For Enemies
	////////////////////////////////////////////////////////////////////////////////////
	void ScanForTarget(const int scannerIndex)
	{
		if (mActors.empty() || scannerIndex < 0 || scannerIndex >= mActors.size())
		{
			return;
		}

		gentity_t* scanner = mActors[scannerIndex];
		if (!scanner || !scanner->NPC)
		{
			return;
		}

		int   targetIndex = 0;
		int   targetStop = ENTITYNUM_WORLD;

		const gNPCstats_t* scannerStats = &scanner->NPC->stats;
		const float        scannerMaxViewDist = scannerStats->visrange;
		const float        scannerMaxHearDist = scannerStats->earshot;
		const CVec3        scannerPos(scanner->currentOrigin);
		CVec3              scannerFwd(scanner->currentAngles);
		scannerFwd.AngToVec();

		if (mTarget)
		{
			targetIndex = mTargetIndex;
			targetStop = mTargetIndex + 1;
		}

		SaveNPCGlobals();
		SetNPCGlobals(scanner);

		for (; targetIndex < targetStop; targetIndex++)
		{
			gentity_t* target = &g_entities[targetIndex];
			if (!NPC_ValidEnemy(target))
			{
				continue;
			}

			CVec3 targetPos = target->currentOrigin;
			if (target->client && target->client->ps.leanofs)
			{
				targetPos = target->client->renderInfo.eyePoint;
			}

			CVec3 targetDirection = targetPos - scannerPos;
			const float targetDistance = targetDirection.SafeNorm();

			// SEE
			if (targetDistance < scannerMaxViewDist)
			{
				constexpr float scanner_min_visability = 0.1f;
				float target_visibility = TargetVisibility(target);
				target_visibility *= targetDirection.Dot(scannerFwd);
				if (target_visibility > scanner_min_visability)
				{
					if (NPC_ClearLOS(targetPos.v))
					{
						RegisterTarget(target, targetIndex, true);
						RestoreNPCGlobals();
						return;
					}
				}
			}

			// HEAR
			if (targetDistance < scannerMaxHearDist)
			{
				constexpr float scanner_min_noise_level = 0.3f;
				float target_noise_level = TargetNoiseLevel(target);
				target_noise_level *= 1.0f - targetDistance / scannerMaxHearDist;
				if (target_noise_level > scanner_min_noise_level)
				{
					RegisterTarget(target, targetIndex, false);
					RestoreNPCGlobals();
					return;
				}
			}
		}

		RestoreNPCGlobals();
	}

private:
	////////////////////////////////////////////////////////////////////////////////////
	// Troop State
	////////////////////////////////////////////////////////////////////////////////////
	enum ETroopState
	{
		TS_NONE = 0,

		TS_ADVANCE,
		TS_ADVANCE_REGROUP,
		TS_ADVANCE_SEARCH,
		TS_ADVANCE_COVER,
		TS_ADVANCE_FORMATION,

		TS_ATTACK,
		TS_ATTACK_LINE,
		TS_ATTACK_FLANK,
		TS_ATTACK_SURROUND,
		TS_ATTACK_COVER,

		TS_MAX
	};

	ETroopState mState = TS_NONE;

	CVec3 mFormHead;
	CVec3 mFormFwd;
	CVec3 mFormRight;

	////////////////////////////////////////////////////////////////////////////////////
	// TroopInFormation
	////////////////////////////////////////////////////////////////////////////////////
	bool TroopInFormation()
	{
		if (mActors.size() <= 1)
		{
			return true;
		}

		float maxActorRange = (mActors.size() / 2.0f + 2.0f) * mFormSpacingFwd;
		float maxActorRangeSq = maxActorRange * maxActorRange;

		for (int actorIndex = 1; actorIndex < mActors.size(); actorIndex++)
		{
			if (!mActors[actorIndex])
			{
				continue;
			}

			if (DistanceSq(mActors[actorIndex]) > maxActorRangeSq)
			{
				return false;
			}
		}
		return true;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// SActorOrder
	////////////////////////////////////////////////////////////////////////////////////
	struct SActorOrder
	{
		CVec3 mPosition{};
		int   mCombatPoint = -1;
		bool  mKneelAndShoot = false;
	};

	ratl::array_vs<SActorOrder, MAX_ENTS_PER_TROOP> mOrders;

	////////////////////////////////////////////////////////////////////////////////////
	// LeaderIssueAndUpdateOrders
	////////////////////////////////////////////////////////////////////////////////////
	void LeaderIssueAndUpdateOrders(const ETroopState NextState)
	{
		const int actor_count = mActors.size();
		if (actor_count == 0 || !mActors[0])
		{
			return;
		}

		// Assign closest actors to order positions
		for (int orderIndex = 1; orderIndex < actor_count; orderIndex++)
		{
			if (mOrders[orderIndex].mCombatPoint != -1)
			{
				continue;
			}

			int   closest_actor_index = orderIndex;
			float closest_actor_distance = DistanceSquared(mOrders[orderIndex].mPosition.v,
				mActors[orderIndex]->currentOrigin);

			for (int actor_index = orderIndex + 1; actor_index < actor_count; actor_index++)
			{
				if (!mActors[actor_index])
				{
					continue;
				}

				const float currentDistance = DistanceSquared(mOrders[orderIndex].mPosition.v,
					mActors[actor_index]->currentOrigin);
				if (currentDistance < closest_actor_distance)
				{
					closest_actor_distance = currentDistance;
					closest_actor_index = actor_index;
				}
			}

			if (orderIndex != closest_actor_index)
			{
				mActors.swap(orderIndex, closest_actor_index);
			}
		}

		// Copy orders to actors
		for (int actor_index = 1; actor_index < actor_count; actor_index++)
		{
			if (!mActors[actor_index])
			{
				continue;
			}
			VectorCopy(mOrders[actor_index].mPosition.v, mActors[actor_index]->pos1);
		}

		gentity_t* leader = mActors[0];

		// Phase I: voice/anim
		if (NextState != mState)
		{
			switch (NextState)
			{
			case TS_ADVANCE_SEARCH:
				HT_Speech(leader, SPEECH_LOOK, 0);
				break;
			case TS_ADVANCE_COVER:
				HT_Speech(leader, SPEECH_COVER, 0);
				NPC_SetAnim(leader, SETANIM_TORSO, TORSO_HANDSIGNAL4,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS);
				break;
			case TS_ADVANCE_FORMATION:
				HT_Speech(leader, SPEECH_ESCAPING, 0);
				break;
			case TS_ATTACK_LINE:
				HT_Speech(leader, SPEECH_CHASE, 0);
				NPC_SetAnim(leader, SETANIM_TORSO, TORSO_HANDSIGNAL1,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS);
				break;
			case TS_ATTACK_FLANK:
				HT_Speech(leader, SPEECH_OUTFLANK, 0);
				NPC_SetAnim(leader, SETANIM_TORSO, TORSO_HANDSIGNAL3,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS);
				break;
			case TS_ATTACK_SURROUND:
				HT_Speech(leader, SPEECH_GIVEUP, 0);
				NPC_SetAnim(leader, SETANIM_TORSO, TORSO_HANDSIGNAL2,
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLDLESS);
				break;
			case TS_ATTACK_COVER:
				HT_Speech(leader, SPEECH_COVER, 0);
				break;
			default:
				break;
			}
		}
		else if (NextState > TS_ATTACK && !mTroopReform)
		{
			return;
		}

		// Phase II: formation vectors
		mFormHead = leader->currentOrigin;
		mFormFwd = NAV::HasPath(leader) ? NAV::NextPosition(leader) : mTargetLastKnownPosition;
		mFormFwd -= mFormHead;
		mFormFwd[2] = 0.0f;
		mFormFwd *= -1.0f;
		mFormFwd.Norm();

		mFormRight = mFormFwd;
		mFormRight.Cross(CVec3::mZ);

		mFormFwd *= mFormSpacingFwd;
		mFormRight *= mFormSpacingRight;

		if (NextState > TS_ATTACK)
		{
			if (!mTroopReform)
			{
				const int fwd_num = actor_count / 2 + 1;
				for (int i = 0; i < fwd_num; i++)
				{
					mFormHead -= mFormFwd;
				}
			}

			trace_t trace;
			mOrders[0].mPosition = mFormHead;

			gi.trace(&trace,
				mActors[0]->currentOrigin,
				mActors[0]->mins,
				mActors[0]->maxs,
				mOrders[0].mPosition.v,
				mActors[0]->s.number,
				mActors[0]->clipmask,
				static_cast<EG2_Collision>(0),
				0);

			if (trace.fraction < 1.0f)
			{
				mOrders[0].mPosition = trace.endpos;
			}
		}
		else
		{
			mOrders[0].mPosition = mTargetLastKnownPosition;
		}

		VectorCopy(mOrders[0].mPosition.v, mActors[0]->pos1);

		CVec3 FormTgtToHead(mFormHead);
		FormTgtToHead -= mTargetLastKnownPosition;
		FormTgtToHead.SafeNorm();

		CVec3 BaseAngleToHead(FormTgtToHead);
		BaseAngleToHead.VecToAng();

		// Phase III: per‑actor orders
		for (int actor_index = 1; actor_index < actor_count; actor_index++)
		{
			gentity_t* actor = mActors[actor_index];
			if (!actor || !actor->NPC)
			{
				continue;
			}

			SaveNPCGlobals();
			SetNPCGlobals(actor);

			SActorOrder& Order = mOrders[actor_index];
			const float  fwd_scale = static_cast<float>((actor_index + 1) / 2);
			const float  side_scale = (actor_index % 2 == 0) ? -1.0f : 1.0f;

			if (actor->NPC->combatPoint != -1)
			{
				NPC_FreeCombatPoint(actor->NPC->combatPoint, qfalse);
				actor->NPC->combatPoint = -1;
			}

			Order.mPosition = mFormHead;
			Order.mCombatPoint = -1;
			Order.mKneelAndShoot = false;

			// Advance
			if (NextState < TS_ATTACK)
			{
				if (NextState == TS_ADVANCE_REGROUP ||
					NextState == TS_ADVANCE_SEARCH ||
					NextState == TS_ADVANCE_FORMATION ||
					NextState == TS_ADVANCE_COVER)
				{
					Order.mPosition.ScaleAdd(mFormFwd, fwd_scale);
					Order.mPosition.ScaleAdd(mFormRight, side_scale);
				}
			}
			// Attack
			else
			{
				if (NextState == TS_ATTACK_LINE ||
					(NextState == TS_ATTACK_FLANK && actor_index < 4))
				{
					Order.mPosition.ScaleAdd(mFormFwd, fwd_scale);
					Order.mPosition.ScaleAdd(mFormRight, side_scale);
				}
				else if (NextState == TS_ATTACK_FLANK && actor_index >= 4)
				{
					int cpFlags = CP_HAS_ROUTE | CP_AVOID_ENEMY | CP_CLEAR | CP_COVER | CP_FLANK | CP_APPROACH_ENEMY;
					constexpr float avoid_dist = 128.0f;

					Order.mCombatPoint = NPC_FindCombatPointRetry(
						actor->currentOrigin,
						actor->currentOrigin,
						actor->currentOrigin,
						&cpFlags,
						avoid_dist,
						0);

					if (Order.mCombatPoint != -1 && (cpFlags & CP_CLEAR))
					{
						Order.mPosition = level.combatPoints[Order.mCombatPoint].origin;
						NPC_SetCombatPoint(Order.mCombatPoint);
					}
					else
					{
						Order.mPosition.ScaleAdd(mFormFwd, fwd_scale);
						Order.mPosition.ScaleAdd(mFormRight, side_scale);
					}
				}
				else if (NextState == TS_ATTACK_SURROUND)
				{
					Order.mPosition.ScaleAdd(mFormFwd, fwd_scale);
					Order.mPosition.ScaleAdd(mFormRight, side_scale);
				}
				else if (NextState == TS_ATTACK_COVER)
				{
					Order.mPosition.ScaleAdd(mFormFwd, fwd_scale);
					Order.mPosition.ScaleAdd(mFormRight, side_scale);
				}
			}

			if (NextState >= TS_ATTACK)
			{
				trace_t trace;
				CVec3   OrderUp(Order.mPosition);
				OrderUp[2] += 10.0f;

				gi.trace(&trace,
					Order.mPosition.v,
					actor->mins,
					actor->maxs,
					OrderUp.v,
					actor->s.number,
					CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP,
					static_cast<EG2_Collision>(0),
					0);

				if (trace.startsolid || trace.allsolid)
				{
					int cpFlags = CP_HAS_ROUTE | CP_AVOID_ENEMY | CP_CLEAR | CP_COVER | CP_FLANK | CP_APPROACH_ENEMY;
					constexpr float avoid_dist = 128.0f;

					Order.mCombatPoint = NPC_FindCombatPointRetry(
						actor->currentOrigin,
						actor->currentOrigin,
						actor->currentOrigin,
						&cpFlags,
						avoid_dist,
						0);

					if (Order.mCombatPoint != -1)
					{
						Order.mPosition = level.combatPoints[Order.mCombatPoint].origin;
						NPC_SetCombatPoint(Order.mCombatPoint);
					}
					else
					{
						Order.mPosition = mOrders[0].mPosition;
					}
				}
			}

			RestoreNPCGlobals();
		}

		mTroopReform = false;
		mState = NextState;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// SufficientCoverNearby
	////////////////////////////////////////////////////////////////////////////////////
	static bool SufficientCoverNearby()
	{
		// TODO: Evaluate Available Combat Points
		return false;
	}

public:
	////////////////////////////////////////////////////////////////////////////////////
	// Update - primary "think"
	////////////////////////////////////////////////////////////////////////////////////
	void Update()
	{
		if (mActors.empty())
		{
			return;
		}

		ScanForTarget(0);

		if (!mTarget)
		{
			return;
		}

		ETroopState NextState = mState;
		const int   TimeSinceLastSeen = level.time - mTargetVisableStopTime;
		const bool  Attack = TimeSinceLastSeen < 2000;

		if (Attack)
		{
			if (mState < TS_ATTACK)
			{
				if (TroopInFormation())
				{
					NextState = (mActors.size() > 4) ? TS_ATTACK_FLANK : TS_ATTACK_LINE;
				}
				else
				{
					NextState = SufficientCoverNearby() ? TS_ATTACK_COVER : TS_ATTACK_SURROUND;
				}
			}
		}
		else
		{
			if (!TroopInFormation())
			{
				NextState = TS_ADVANCE_REGROUP;
			}
			else
			{
				if (TargetLastKnownPositionVisited())
				{
					NextState = TS_ADVANCE_SEARCH;
				}
				else
				{
					NextState = (TimeSinceLastSeen < 10000) ? TS_ADVANCE_COVER : TS_ADVANCE_FORMATION;
				}
			}
		}

		LeaderIssueAndUpdateOrders(NextState);
	}

	////////////////////////////////////////////////////////////////////////////////////
	// MergeInto - Merges all actors into another troop
	////////////////////////////////////////////////////////////////////////////////////
	void MergeInto(c_troop& Other)
	{
		const int num_ents = mActors.size();
		for (int i = 0; i < num_ents; i++)
		{
			if (!mActors[i] || !mActors[i]->NPC || !mActors[i]->client)
			{
				continue;
			}

			mActors[i]->client->leader = nullptr;
			mActors[i]->NPC->troop = 0;
			Other.AddActor(mActors[i]);
		}
		mActors.clear();

		if (!Other.mTarget && mTarget)
		{
			Other.mTarget = mTarget;
			Other.mTargetIndex = mTargetIndex;
			Other.mTargetLastKnownPosition = mTargetLastKnownPosition;
			Other.mTargetLastKnownPositionVisited = mTargetLastKnownPositionVisited;
			Other.mTargetLastKnownTime = mTargetLastKnownTime;
			Other.mTargetVisableStartTime = mTargetVisableStartTime;
			Other.mTargetVisableStopTime = mTargetVisableStopTime;
			Other.mTargetVisable = mTargetVisable;
			Other.mTargetVisablePosition = mTargetVisablePosition;
			Other.LeaderIssueAndUpdateOrders(mState);
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	gentity_t* TrackingTarget() const
	{
		return mTarget;
	}

	////////////////////////////////////////////////////////////////////////////////////
	gentity_t* TroopLeader()
	{
		if (mActors.empty())
		{
			return nullptr;
		}
		return mActors[0];
	}

	////////////////////////////////////////////////////////////////////////////////////
	int TimeSinceSeenTarget() const
	{
		return level.time - mTargetVisableStopTime;
	}

	////////////////////////////////////////////////////////////////////////////////////
	CVec3& TargetVisablePosition()
	{
		return mTargetVisablePosition;
	}

	////////////////////////////////////////////////////////////////////////////////////
	float FormSpacingFwd() const
	{
		return mFormSpacingFwd;
	}

	////////////////////////////////////////////////////////////////////////////////////
	gentity_t* TooCloseToTroopMember(const gentity_t* actor)
	{
		if (!actor)
		{
			return nullptr;
		}

		for (int i = 0; i < mActors.size(); i++)
		{
			if (!mActors[i])
			{
				continue;
			}

			if (actor == mActors[i])
			{
				return nullptr;
			}

			if (Distance(actor->currentOrigin, mActors[i]->currentOrigin) < mFormSpacingFwd * 0.5f)
			{
				return mActors[i];
			}
		}

		assert("Somehow this actor is not actually in the troop..." == nullptr);
		return nullptr;
	}
};

using TTroopPool = ratl::handle_pool_vs<c_troop, MAX_TROOPS>;
TTroopPool mTroops;

////////////////////////////////////////////////////////////////////////////////////////
// Erase All Data, Set To Default Vals Before Entities Spawn
////////////////////////////////////////////////////////////////////////////////////////
void Troop_Reset()
{
	mTroops.clear();
}

////////////////////////////////////////////////////////////////////////////////////////
// Entities Have Just Spawned, Initialize
////////////////////////////////////////////////////////////////////////////////////////
void Troop_Initialize()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// Global Update Of All Troops
////////////////////////////////////////////////////////////////////////////////////////
void Troop_Update()
{
	for (auto& mTroop : mTroops)
	{
		mTroop.Update();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Erase All Data, Set To Default Vals Before Entities Spawn
////////////////////////////////////////////////////////////////////////////////////////
static void Trooper_UpdateTroop(gentity_t* actor)
{
	// Try To Join A Troop
	//---------------------
	if (!actor->NPC->troop)
	{
		float closestDist = 0;
		TTroopPool::iterator closestTroop = mTroops.end();
		trace_t trace;

		for (TTroopPool::iterator iTroop = mTroops.begin(); iTroop != mTroops.end(); ++iTroop)
		{
			if (iTroop->Team() == actor->client->playerTeam)
			{
				const float curDist = iTroop->DistanceSq(actor);
				if (curDist < MAX_TROOP_JOIN_DIST2 && (!closestDist || curDist < closestDist))
				{
					// Only Join A Troop If You Can See The Leader
					//---------------------------------------------
					gi.trace(&trace,
						actor->currentOrigin,
						actor->mins,
						actor->maxs,
						iTroop->TroopLeader()->currentOrigin,
						actor->s.number,
						CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP,
						static_cast<EG2_Collision>(0),
						0);

					if (!trace.allsolid &&
						!trace.startsolid &&
						(trace.fraction >= 1.0f || trace.entityNum == iTroop->TroopLeader()->s.number))
					{
						closestDist = curDist;
						closestTroop = iTroop;
					}
				}
			}
		}

		// If Found, Add The Actor To It
		//--------------------------------
		if (closestTroop != mTroops.end())
		{
			closestTroop->AddActor(actor);
		}

		// If We Couldn't Find One, Create A New Troop
		//---------------------------------------------
		else if (!mTroops.full())
		{
			const int nTroopHandle = mTroops.alloc();
			mTroops[nTroopHandle].Initialize(nTroopHandle);
			mTroops[nTroopHandle].AddActor(actor);
		}
	}

	// If This Is A Leader, Then He Is Responsible For Merging Troops
	//----------------------------------------------------------------
	else if (actor->client->leader == actor)
	{
		float closestDist = 0;
		TTroopPool::iterator closestTroop = mTroops.end();

		for (TTroopPool::iterator iTroop = mTroops.begin(); iTroop != mTroops.end(); ++iTroop)
		{
			const float curDist = iTroop->DistanceSq(actor);
			if (curDist < MAX_TROOP_MERGE_DIST2 &&
				(!closestDist || curDist < closestDist) &&
				mTroops.index_to_handle(iTroop.index()) != actor->NPC->troop)
			{
				closestDist = curDist;
				closestTroop = iTroop;
			}
		}

		if (closestTroop != mTroops.end())
		{
			const int oldTroopNum = actor->NPC->troop;
			mTroops[oldTroopNum].MergeInto(*closestTroop);
			mTroops.free(oldTroopNum);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static bool Trooper_UpdateSmackAway(gentity_t* actor, gentity_t* target)
{
	if (actor->client->ps.legsAnim == BOTH_MELEE1)
	{
		if (TIMER_Done(actor, "Trooper_SmackAway"))
		{
			const CVec3 ActorPos(actor->currentOrigin);
			CVec3 ActorToTgt(target->currentOrigin);
			ActorToTgt -= ActorPos;
			const float ActorToTgtDist = ActorToTgt.SafeNorm();

			if (ActorToTgtDist < 100.0f)
			{
				G_Throw(target, ActorToTgt.v, 200.0f);
			}
		}
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static void Trooper_SmackAway(gentity_t* actor, gentity_t* target)
{
	assert(actor && actor->NPC);
	if (actor->client->ps.legsAnim != BOTH_MELEE1)
	{
		NPC_SetAnim(actor, SETANIM_BOTH, BOTH_MELEE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		TIMER_Set(actor, "Trooper_SmackAway", actor->client->ps.torsoAnimTimer / 4.0f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static bool Trooper_Kneeling(const gentity_t* actor)
{
	return actor->NPC->aiFlags & NPCAI_KNEEL || actor->client->ps.legsAnim == BOTH_STAND_TO_KNEEL;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static void Trooper_KneelDown(gentity_t* actor)
{
	assert(actor && actor->NPC);
	if (!Trooper_Kneeling(actor) && level.time > actor->NPC->kneelTime)
	{
		NPC_SetAnim(actor, SETANIM_BOTH, BOTH_STAND_TO_KNEEL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		actor->NPC->aiFlags |= NPCAI_KNEEL;
		actor->NPC->kneelTime = level.time + Q_irand(3000, 6000);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static void Trooper_StandUp(gentity_t* actor, const bool always = false)
{
	assert(actor && actor->NPC);
	if (Trooper_Kneeling(actor) && (always || level.time > actor->NPC->kneelTime))
	{
		actor->NPC->aiFlags &= ~NPCAI_KNEEL;
		NPC_SetAnim(actor, SETANIM_BOTH, BOTH_KNEEL_TO_STAND, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		actor->NPC->kneelTime = level.time + Q_irand(3000, 6000);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
static int Trooper_CanHitTarget(gentity_t* actor, const gentity_t* target, c_troop& troop, float& MuzzleToTargetDistance,
	CVec3& MuzzleToTarget)
{
	trace_t tr;
	CVec3 MuzzlePoint(actor->currentOrigin);
	CalcEntitySpot(actor, SPOT_WEAPON, MuzzlePoint.v);

	MuzzleToTarget = troop.TargetVisablePosition();
	MuzzleToTarget -= MuzzlePoint;
	MuzzleToTargetDistance = MuzzleToTarget.SafeNorm();

	CVec3 MuzzleDirection(actor->currentAngles);
	MuzzleDirection.AngToVec();

	// Aiming In The Right Direction?
	//--------------------------------
	if (MuzzleDirection.Dot(MuzzleToTarget) > 0.95)
	{
		// Clear Line Of Sight To Target?
		//--------------------------------
		gi.trace(&tr, MuzzlePoint.v, nullptr, nullptr, troop.TargetVisablePosition().v, actor->s.number, MASK_SHOT,
			static_cast<EG2_Collision>(0), 0);
		if (tr.startsolid || tr.allsolid)
		{
			return ENTITYNUM_NONE;
		}
		if (tr.entityNum == target->s.number || tr.fraction > 0.9f)
		{
			return target->s.number;
		}
		return tr.entityNum;
	}
	return ENTITYNUM_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////
// Run The Per Trooper Update
////////////////////////////////////////////////////////////////////////////////////////
static void Trooper_Think(gentity_t* actor)
{
	gentity_t* target = actor->NPC->troop ? mTroops[actor->NPC->troop].TrackingTarget() : nullptr;
	if (target)
	{
		G_SetEnemy(actor, target);

		c_troop& troop = mTroops[actor->NPC->troop];
		bool AtPos = STEER::Reached(actor, actor->pos1, 10.0f);
		int traceTgt = ENTITYNUM_NONE;
		bool traced = false;
		bool inSmackAway = false;

		float MuzzleToTargetDistance = 0.0f;
		CVec3 MuzzleToTarget;

		if (actor->NPC->combatPoint != -1)
		{
			traceTgt = Trooper_CanHitTarget(actor, target, troop, MuzzleToTargetDistance, MuzzleToTarget);
			traced = true;
			if (traceTgt == target->s.number)
			{
				AtPos = true;
			}
		}

		// Smack!
		//-------
		if (Trooper_UpdateSmackAway(actor, target))
		{
			traced = true;
			AtPos = true;
			inSmackAway = true;
		}

		// If There, Stop Moving
		//-----------------------
		STEER::Activate(actor);
		{
			const gentity_t* fleeFrom = troop.TooCloseToTroopMember(actor);

			// If Too Close To The Leader, Get Out Of His Way
			//------------------------------------------------
			if (fleeFrom)
			{
				STEER::Flee(actor, fleeFrom->currentOrigin, 1.0f);
				AtPos = false;
			}

			// If In Position, Stop Moving
			//-----------------------------
			if (AtPos)
			{
				NAV::ClearPath(actor);
				STEER::Stop(actor);
			}

			// Otherwise, Try To Get To Position
			//-----------------------------------
			else
			{
				Trooper_StandUp(actor, true);

				// If Close Enough, Persue Our Target Directly
				//---------------------------------------------
				bool moveSuccess = STEER::GoTo(NPC, actor->pos1, 10.0f, false);

				// Otherwise
				//-----------
				if (!moveSuccess)
				{
					moveSuccess = NAV::GoTo(NPC, actor->pos1);
				}

				// If No Way To Get To Position, Stay Here
				//-----------------------------------------
				if (!moveSuccess || level.time - actor->lastMoveTime > 4000)
				{
					AtPos = true;
				}
			}
		}
		STEER::DeActivate(actor, &ucmd);

		// If There And Target Was Recently Visable
		//------------------------------------------
		if (AtPos && troop.TimeSinceSeenTarget() < 1500)
		{
			if (!traced)
			{
				traceTgt = Trooper_CanHitTarget(actor, target, troop, MuzzleToTargetDistance, MuzzleToTarget);
			}

			// Shoot!
			//--------
			if (traceTgt == target->s.number)
			{
				WeaponThink();
			}
			else if (!inSmackAway)
			{
				// Otherwise, If Kneeling, Get Up!
				//---------------------------------
				if (Trooper_Kneeling(actor))
				{
					Trooper_StandUp(actor);
				}

				// If The Enemy Is Close Enough, Smack Him Away
				//----------------------------------------------
				else if (MuzzleToTargetDistance < 40.0f)
				{
					Trooper_SmackAway(actor, target);
				}

				// If We Would Have It A Friend, Ask Him To Kneel
				//------------------------------------------------
				else if (traceTgt != ENTITYNUM_NONE &&
					traceTgt != ENTITYNUM_WORLD &&
					g_entities[traceTgt].client &&
					g_entities[traceTgt].NPC &&
					g_entities[traceTgt].client->playerTeam == actor->client->playerTeam &&
					NPC_IsTrooper(&g_entities[traceTgt]) &&
					g_entities[traceTgt].resultspeed < 1.0f &&
					!(g_entities[traceTgt].NPC->aiFlags & NPCAI_KNEEL))
				{
					Trooper_KneelDown(&g_entities[traceTgt]);
				}
			}

			// Convert To Angles And Set That As Our Desired Look Direction
			//--------------------------------------------------------------
			if (MuzzleToTargetDistance > 100)
			{
				MuzzleToTarget.VecToAng();

				NPCInfo->desiredYaw = MuzzleToTarget[YAW];
				NPCInfo->desiredPitch = MuzzleToTarget[PITCH];
			}
			else
			{
				MuzzleToTarget = troop.TargetVisablePosition();
				MuzzleToTarget.v[2] -= 20.0f; // Aim Lower
				MuzzleToTarget -= actor->currentOrigin;
				MuzzleToTarget.SafeNorm();
				MuzzleToTarget.VecToAng();

				NPCInfo->desiredYaw = MuzzleToTarget[YAW];
				NPCInfo->desiredPitch = MuzzleToTarget[PITCH];
			}
		}

		NPC_UpdateFiringAngles(qtrue, qtrue);
		NPC_UpdateAngles(qtrue, qtrue);

		if (Trooper_Kneeling(actor))
		{
			ucmd.upmove = -127; // Set Crouch Height
		}
	}
	else
	{
		NPC_BSST_Default();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
/*
-------------------------
NPC_BehaviorSet_Trooper
-------------------------
*/
////////////////////////////////////////////////////////////////////////////////////////
void NPC_BehaviorSet_Trooper(const int b_state)
{
	Trooper_UpdateTroop(NPC);
	switch (b_state)
	{
	case BS_STAND_GUARD:
	case BS_PATROL:
	case BS_STAND_AND_SHOOT:
	case BS_HUNT_AND_KILL:
	case BS_DEFAULT:
		Trooper_Think(NPC);
		break;

	case BS_INVESTIGATE:
		NPC_BSST_Investigate();
		break;

	case BS_SLEEP:
		NPC_BSST_Sleep();
		break;

	default:
		Trooper_Think(NPC);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// IsTrooper - return true if you want a given actor to use trooper AI
////////////////////////////////////////////////////////////////////////////////////////
bool NPC_IsTrooper(const gentity_t* actor)
{
	return actor &&
		actor->NPC &&
		actor->s.weapon &&
		!!(actor->NPC->scriptFlags & SCF_NO_GROUPS);
}

void NPC_LeaveTroop(const gentity_t* actor)
{
	assert(actor->NPC->troop);
	const int wasInTroop = actor->NPC->troop;
	mTroops[actor->NPC->troop].RemoveActor(actor);
	if (mTroops[wasInTroop].Empty())
	{
		mTroops.free(wasInTroop);
	}
}