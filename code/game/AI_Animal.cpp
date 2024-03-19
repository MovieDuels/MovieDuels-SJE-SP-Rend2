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
#include "../Ratl/vector_vs.h"

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
	// Find The Closest Pack Leader, Not Counting Myself
	//---------------------------------------------------
	gentity_t* closest_leader = nullptr;
	float closest_dist = 0;
	int my_leader_num = 0;

	for (int i = 0; i < mPacks.size(); i++)
	{
		// Dump Dead Leaders
		//-------------------
		if (mPacks[i] == nullptr || mPacks[i]->health <= 0)
		{
			if (mPacks[i] == NPC->client->leader)
			{
				NPC->client->leader = nullptr;
			}

			mPacks.erase_swap(i);

			if (i >= mPacks.size())
			{
				closest_leader = nullptr;
				break;
			}
		}

		// Don't Count Self
		//------------------
		if (mPacks[i] == NPC)
		{
			my_leader_num = i;
			continue;
		}

		const float dist = Distance(mPacks[i]->currentOrigin, NPC->currentOrigin);
		if (!closest_leader || dist < closest_dist)
		{
			closest_dist = dist;
			closest_leader = mPacks[i];
		}
	}

	// In Joining Distance?
	//----------------------
	if (closest_leader && closest_dist < JOIN_PACK_DISTANCE)
	{
		// Am I Already A Leader?
		//------------------------
		if (NPC->client->leader == NPC)
		{
			mPacks.erase_swap(my_leader_num); // Erase Myself From The Leader List
		}

		// Join The Pack!
		//----------------
		NPC->client->leader = closest_leader;
	}

	// Do I Have A Leader?
	//---------------------
	if (NPC->client->leader)
	{
		// AM I A Leader?
		//----------------
		if (NPC->client->leader != NPC)
		{
			// If Our Leader Is Dead, Clear Him Out

			if (NPC->client->leader->health <= 0 || NPC->client->leader->inuse == 0)
			{
				NPC->client->leader = nullptr;
			}

			// If My Leader Isn't His Own Leader, Then, Use His Leader
			//---------------------------------------------------------
			else if (NPC->client->leader->client->leader != NPC->client->leader)
			{
				// Eh.  Can this get more confusing?
				NPC->client->leader = NPC->client->leader->client->leader;
			}

			// If Our Leader Is Too Far Away, Clear Him Out
			//------------------------------------------------------
			else if (Distance(NPC->client->leader->currentOrigin, NPC->currentOrigin) > LEAVE_PACK_DISTANCE)
			{
				NPC->client->leader = nullptr;
			}
		}
	}

	// If We Couldn't Find A Leader, Then Become One
	//-----------------------------------------------
	else if (!mPacks.full())
	{
		NPC->client->leader = NPC;
		mPacks.push_back(NPC);
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