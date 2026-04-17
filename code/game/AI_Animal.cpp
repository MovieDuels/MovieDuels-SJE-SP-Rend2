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

#if !defined(RAVL_VEC_INC)
#include "../Ravl/CVec.h"
#endif
#include <Ravl/CVec.h>
#include "../Ratl/vector_vs.h"
#include "bg_public.h"
#include <qcommon/q_platform.h>
#include <qcommon/q_math.h>
#include "g_local.h"
#include <cmath>
#include "b_public.h"

constexpr auto MAX_PACKS = 10;

constexpr auto LEAVE_PACK_DISTANCE = 1000;
constexpr auto JOIN_PACK_DISTANCE = 800;
constexpr auto WANDER_RANGE = 1000;
constexpr auto FRIGHTEN_DISTANCE = 300;

extern qboolean G_PlayerSpawned();

ratl::vector_vs<gentity_t*, MAX_PACKS> mPacks;

////////////////////////////////////////////////////////////////////////////////////////
// Update The Packs, Delete Dead Leaders, Join / Split Packs, Find MY Leader
////////////////////////////////////////////////////////////////////////////////////////
static gentity_t* NPC_AnimalUpdateLeader()
{
	// NPC must exist and must have a client
	if (!NPC || !NPC->client)
	{
		return NULL;
	}

	gentity_t* closest_leader = NULL;
	float closest_dist = 0.0f;
	int my_leader_num = -1;

	for (int i = 0; i < mPacks.size(); )
	{
		gentity_t* ent = mPacks[i];

		// Remove dead or invalid leaders
		if (!ent || ent->health <= 0)
		{
			if (NPC->client->leader == ent)
			{
				NPC->client->leader = NULL;
			}

			mPacks.erase_swap(i);
			continue; // do NOT increment i after erase_swap
		}

		// Skip self
		if (ent == NPC)
		{
			my_leader_num = i;
			i++;
			continue;
		}

		// Compute distance
		float dist = Distance(ent->currentOrigin, NPC->currentOrigin);

		if (!closest_leader || dist < closest_dist)
		{
			closest_leader = ent;
			closest_dist = dist;
		}

		i++;
	}

	// Join a nearby leader
	if (closest_leader && closest_dist < JOIN_PACK_DISTANCE)
	{
		// If I was a leader, remove myself from the pack list
		if (NPC->client->leader == NPC && my_leader_num >= 0 && my_leader_num < mPacks.size())
		{
			mPacks.erase_swap(my_leader_num);
		}

		NPC->client->leader = closest_leader;
	}

	// If I have a leader, validate it
	if (NPC->client->leader)
	{
		gentity_t* L = NPC->client->leader;

		// Leader dead or removed?
		if (!L->inuse || L->health <= 0)
		{
			NPC->client->leader = NULL;
		}
		// Leader has a leader? Follow the chain
		else if (L->client && L->client->leader != L)
		{
			NPC->client->leader = L->client->leader;
		}
		// Leader too far away?
		else if (Distance(L->currentOrigin, NPC->currentOrigin) > LEAVE_PACK_DISTANCE)
		{
			NPC->client->leader = NULL;
		}
	}
	else
	{
		// No leader → become one
		if (!mPacks.full())
		{
			NPC->client->leader = NPC;
			mPacks.push_back(NPC);
		}
	}

	return NPC->client->leader;
}

/*
-------------------------
NPC_BSAnimal_Default
-------------------------
*/
void NPC_BSAnimal_Default()
{
	if (!NPC || !NPC->client)
	{
		return;
	}

	// Update Some Positions
	//-----------------------
	const CVec3 current_location(NPC->currentOrigin);

	// Update The Leader
	//-------------------
	gentity_t* leader = NPC_AnimalUpdateLeader();

	// Select Closest Threat Location
	//--------------------------------
	CVec3 threat_location{};
	const qboolean player_spawned = G_PlayerSpawned();
	if (player_spawned)
	{
		//player is actually in the level now
		threat_location = player->currentOrigin;
	}
	const int alert_event = NPC_CheckAlertEvents(qtrue, qtrue, -1, qfalse, AEL_MINOR, qfalse);
	if (alert_event >= 0)
	{
		const alertEvent_t* event = &level.alertEvents[alert_event];
		if (event->owner != NPC && Distance(event->position, current_location.v) < event->radius)
		{
			threat_location = event->position;
		}
	}

	//	float	DistToThreat	= CurrentLocation.Dist(ThreatLocation);
	//	float	DistFromHome	= CurrentLocation.Dist(mHome);

	const bool evade_threat = level.time < NPCInfo->investigateSoundDebounceTime;
	const bool charmed_docile = level.time < NPCInfo->confusionTime;
	const bool charmed_approach = level.time < NPCInfo->charmedTime;

	// If Not Already Evading, Test To See If We Should "Know" About The Threat
	//--------------------------------------------------------------------------
	/*	if (false && !EvadeThreat && PlayerSpawned && (DistToThreat<FRIGHTEN_DISTANCE))
		{
			CVec3	LookAim(NPC->currentAngles);
			LookAim.AngToVec();
			CVec3	MyPos(CurrentLocation);
			MyPos -= ThreatLocation;
			MyPos.SafeNorm();

			float	DirectionSimilarity = MyPos.Dot(LookAim);

			if (fabsf(DirectionSimilarity)<0.8f)
			{
				EvadeThreat = true;
				NPCInfo->investigateSoundDebounceTime = level.time + Q_irand(0, 1000);
				VectorCopy(ThreatLocation.v, NPCInfo->investigateGoal);
			}
		}*/

	STEER::Activate(NPC);
	{
		// Charmed Approach - Walk TOWARD The Threat Location
		//----------------------------------------------------
		if (charmed_approach)
		{
			NAV::GoTo(NPC, NPCInfo->investigateGoal);
		}

		// Charmed Docile - Stay Put
		//---------------------------
		else if (charmed_docile)
		{
			NAV::ClearPath(NPC);
			STEER::Stop(NPC);
		}

		// Run Away From This Threat
		//---------------------------
		else if (evade_threat)
		{
			NAV::ClearPath(NPC);
			STEER::Flee(NPC, NPCInfo->investigateGoal);
		}

		// Normal Behavior
		//-----------------
		else
		{
			// Follow Our Pack Leader!
			//-------------------------
			if (leader && leader != NPC)
			{
				constexpr float follow_dist = 100.0f;
				const float cur_dist = Distance(NPC->currentOrigin, leader->followPos);

				// Update The Leader's Follow Position
				//-------------------------------------
				STEER::FollowLeader(NPC, leader, follow_dist);

				const bool in_seek_range = cur_dist < follow_dist * 10.0f;
				const bool on_nbr_points =
					NAV::OnNeighboringPoints(NAV::GetNearestNode(NPC), leader->followPosWaypoint);
				const bool leader_stop = level.time - leader->lastMoveTime > 500;

				// If Close Enough, Dump Any Existing Path
				//-----------------------------------------
				if (in_seek_range || on_nbr_points)
				{
					NAV::ClearPath(NPC);

					// If The Leader Isn't Moving, Stop
					//----------------------------------
					if (leader_stop)
					{
						STEER::Stop(NPC);
					}

					// Otherwise, Try To Get To The Follow Position
					//----------------------------------------------
					else
					{
						STEER::Seek(NPC, leader->followPos, fabsf(follow_dist) / 2.0f/*slowing distance*/,
							1.0f/*wight*/,
							leader->resultspeed);
					}
				}

				// Otherwise, Get A Path To The Follow Position
				//----------------------------------------------
				else
				{
					NAV::GoTo(NPC, leader->followPosWaypoint);
				}
				STEER::Separation(NPC, 4.0f);
				STEER::AvoidCollisions(NPC, leader);
			}

			// Leader AI - Basically Wander
			//------------------------------
			else
			{
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
					}
				}

				if (!has_path)
				{
					// If Debounce Time Has Expired, Choose A New Sub State
					//------------------------------------------------------
					if (NPCInfo->investigateDebounceTime < level.time)
					{
						// Clear Out Flags From The Previous Substate
						//--------------------------------------------
						NPCInfo->aiFlags &= ~NPCAI_OFF_PATH;
						NPCInfo->aiFlags &= ~NPCAI_WALKING;

						// Pick Another Spot
						//-------------------
						const int NEXTSUBSTATE = Q_irand(0, 10);

						const bool random_path_node = NEXTSUBSTATE < 8; //(NEXTSUBSTATE<9);
						const bool pathless_wander = NEXTSUBSTATE < 9; //false;

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
							//, mHome.v, WANDER_RANGE));
						}

						// Pathless Wandering
						//--------------------
						else if (pathless_wander)
						{
							// Sometimes, Walk
							//-----------------
							if (Q_irand(0, 1) == 0)
							{
								NPCInfo->aiFlags |= NPCAI_WALKING;
							}

							NPCInfo->investigateDebounceTime = level.time + Q_irand(3000, 10000);
							NPCInfo->aiFlags |= NPCAI_OFF_PATH;
						}

						// Just Stand Here
						//-----------------
						else
						{
							NPCInfo->investigateDebounceTime = level.time + Q_irand(2000, 6000);
							//NPC_SetAnim(NPC, SETANIM_BOTH, ((Q_irand(0, 1)==0)?(BOTH_GUARD_LOOKAROUND1):(BOTH_GUARD_IDLE1)), SETANIM_FLAG_NORMAL);
						}
					}

					// Ok, So We Don't Have A Path, And Debounce Time Is Still Active, So We Are Either Wandering Or Looking Around
					//--------------------------------------------------------------------------------------------------------------
					else
					{
						//	if (DistFromHome>(WANDER_RANGE))
						//	{
						//		STEER::Seek(NPC, mHome);
						//	}
						//	else
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
				}
			}
		}
	}
	STEER::DeActivate(NPC, &ucmd);

	NPC_UpdateAngles(qtrue, qtrue);
}