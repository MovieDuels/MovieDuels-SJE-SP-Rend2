/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

#include "common_headers.h"

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include "../qcommon/q_shared.h"
#include "g_shared.h"
#include "bg_local.h"
#include "anims.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "../ghoul2/ghoul2_gore.h"

extern void CG_SetClientViewAngles(vec3_t angles, qboolean override_view_ent);
extern qboolean PM_InAnimForSaberMove(int anim, int saber_move);
extern qboolean PM_InForceGetUp(const playerState_t* ps);
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern qboolean PM_InReboundJump(int anim);
extern qboolean PM_StabDownAnim(int anim);
extern qboolean PM_DodgeAnim(int anim);
extern qboolean PM_DodgeHoldAnim(int anim);
extern qboolean PM_InReboundHold(int anim);
extern qboolean PM_InKnockDownNoGetup(const playerState_t* ps);
extern qboolean PM_InGetUpNoRoll(const playerState_t* ps);
extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern qboolean PlayerAffectedByStasis();
extern qboolean PM_WalkingOrRunningAnim(int anim);
extern qboolean PM_BlockAnim(int anim);
extern qboolean PM_BlockHoldAnim(int anim);
extern qboolean PM_BlockDualAnim(int anim);
extern qboolean PM_BlockHoldDualAnim(int anim);
extern qboolean PM_BlockStaffAnim(int anim);
extern qboolean PM_BlockHoldStaffAnim(int anim);
extern qboolean PM_KickMove(int move);
extern cvar_t* g_SerenityJediEngineMode;
extern void PM_AddBlockFatigue(playerState_t* ps, int fatigue);
extern void PM_AddFatigue(playerState_t* ps, int fatigue);
extern qboolean PM_InLedgeMove(int anim);
extern qboolean PM_SaberInMassiveBounce(int move);
extern qboolean PM_SaberInBashedAnim(int anim);
extern qboolean PM_CrouchAnim(int anim);

extern qboolean cg_usingInFrontOf;
extern qboolean player_locked;
extern pmove_t* pm;
extern pml_t pml;

void BG_IK_MoveLimb(CGhoul2Info_v& ghoul2, const int bolt_index, const char* anim_bone, const char* first_bone,
	const char* second_bone,
	const int time, const entityState_t* ent, const int anim_file_index, const int base_pose,
	vec3_t desired_pos, qboolean* ik_in_progress, vec3_t origin,
	vec3_t angles, vec3_t scale, const int blend_time, const qboolean force_halt)
{
	mdxaBone_t hold_point_matrix;
	const animation_t* anim = &level.knownAnimFileSets[anim_file_index].animations[base_pose];

	assert(ghoul2.size());

	assert(anim->firstFrame > 0); //FIXME: umm...?

	if (!*ik_in_progress && !force_halt)
	{
		sharedSetBoneIKStateParams_t ik_p{};

		//restrict the shoulder joint
		//VectorSet(ikP.pcjMins,-50.0f,-80.0f,-15.0f);
		//VectorSet(ikP.pcjMaxs,15.0f,40.0f,15.0f);

		//for now, leaving it unrestricted, but restricting elbow joint.
		//This lets us break the arm however we want in order to fling people
		//in throws, and doesn't look bad.
		VectorSet(ik_p.pcjMins, 0, 0, 0);
		VectorSet(ik_p.pcjMaxs, 0, 0, 0);

		//give the info on our entity.
		ik_p.blend_time = blend_time;
		VectorCopy(origin, ik_p.origin);
		VectorCopy(angles, ik_p.angles);
		ik_p.angles[PITCH] = 0;
		ik_p.pcjOverrides = 0;
		ik_p.radius = 10.0f;
		VectorCopy(scale, ik_p.scale);

		//base pose frames for the limb
		ik_p.startFrame = anim->firstFrame + anim->numFrames;
		ik_p.endFrame = anim->firstFrame + anim->numFrames;

		//ikP.forceAnimOnBone = qfalse; //let it use existing anim if it's the same as this one.

		//we want to call with a null bone name first. This will init all of the
		//ik system stuff on the g2 instance, because we need ragdoll effectors
		//in order for our pcj's to know how to angle properly.
		if (!gi.G2API_SetBoneIKState(ghoul2, time, nullptr, IKS_DYNAMIC, &ik_p))
		{
			assert(!"Failed to init IK system for g2 instance!");
		}

		//Now, create our IK bone state.
		if (gi.G2API_SetBoneIKState(ghoul2, time, "lower_lumbar", IKS_DYNAMIC, &ik_p))
		{
			//restrict the elbow joint
			VectorSet(ik_p.pcjMins, -90.0f, -20.0f, -20.0f);
			VectorSet(ik_p.pcjMaxs, 30.0f, 20.0f, -20.0f);
			if (gi.G2API_SetBoneIKState(ghoul2, time, "upper_lumbar", IKS_DYNAMIC, &ik_p))
			{
				//restrict the elbow joint
				VectorSet(ik_p.pcjMins, -90.0f, -20.0f, -20.0f);
				VectorSet(ik_p.pcjMaxs, 30.0f, 20.0f, -20.0f);
				if (gi.G2API_SetBoneIKState(ghoul2, time, "thoracic", IKS_DYNAMIC, &ik_p))
				{
					//restrict the elbow joint
					VectorSet(ik_p.pcjMins, -90.0f, -20.0f, -20.0f);
					VectorSet(ik_p.pcjMaxs, 30.0f, 20.0f, -20.0f);
					if (gi.G2API_SetBoneIKState(ghoul2, time, second_bone, IKS_DYNAMIC, &ik_p))
					{
						//restrict the elbow joint
						VectorSet(ik_p.pcjMins, -90.0f, -20.0f, -20.0f);
						VectorSet(ik_p.pcjMaxs, 30.0f, 20.0f, -20.0f);

						if (gi.G2API_SetBoneIKState(ghoul2, time, first_bone, IKS_DYNAMIC, &ik_p))
						{
							//everything went alright.
							*ik_in_progress = qtrue;
						}
					}
				}
			}
		}
	}

	if (*ik_in_progress && !force_halt)
	{
		vec3_t torg;
		vec3_t hold_point{};
		//actively update our ik state.
		sharedIKMoveParams_t ik_m{};
		CRagDollUpdateParams tu_parms;
		vec3_t t_angles;

		//set the argument struct up
		VectorCopy(desired_pos, ik_m.desiredOrigin); //we want the bone to move here.. if possible

		VectorCopy(angles, t_angles);
		t_angles[PITCH] = t_angles[ROLL] = 0;

		gi.G2API_GetBoltMatrix(ghoul2, 0, bolt_index, &hold_point_matrix, t_angles, origin, time, nullptr, scale);
		//Get the point position from the matrix.
		hold_point[0] = hold_point_matrix.matrix[0][3];
		hold_point[1] = hold_point_matrix.matrix[1][3];
		hold_point[2] = hold_point_matrix.matrix[2][3];

		VectorSubtract(hold_point, desired_pos, torg);
		const float dist_to_dest = VectorLength(torg);

		//closer we are, more we want to keep updated.
		//if we're far away we don't want to be too fast or we'll start twitching all over.
		if (dist_to_dest < 2)
		{
			//however if we're this close we want very precise movement
			ik_m.movementSpeed = 0.4f;
		}
		else if (dist_to_dest < 16)
		{
			ik_m.movementSpeed = 0.9f; //8.0f;
		}
		else if (dist_to_dest < 32)
		{
			ik_m.movementSpeed = 0.8f; //4.0f;
		}
		else if (dist_to_dest < 64)
		{
			ik_m.movementSpeed = 0.7f; //2.0f;
		}
		else
		{
			ik_m.movementSpeed = 0.6f;
		}
		VectorCopy(origin, ik_m.origin); //our position in the world.

		ik_m.boneName[0] = 0;
		if (gi.G2API_IKMove(ghoul2, time, &ik_m))
		{
			//now do the standard model animate stuff with ragdoll update params.
			VectorCopy(angles, tu_parms.angles);
			tu_parms.angles[PITCH] = 0;

			VectorCopy(origin, tu_parms.position);
			VectorCopy(scale, tu_parms.scale);

			tu_parms.me = ent->number;
			VectorClear(tu_parms.velocity);

			gi.G2API_AnimateG2Models(ghoul2, time, &tu_parms);
		}
		else
		{
			*ik_in_progress = qfalse;
		}
	}
	else if (*ik_in_progress)
	{
		//kill it
		float c_frame, animSpeed;
		int s_frame, e_frame, flags;

		gi.G2API_SetBoneIKState(ghoul2, time, "lower_lumbar", IKS_NONE, nullptr);
		gi.G2API_SetBoneIKState(ghoul2, time, "upper_lumbar", IKS_NONE, nullptr);
		gi.G2API_SetBoneIKState(ghoul2, time, "thoracic", IKS_NONE, nullptr);
		gi.G2API_SetBoneIKState(ghoul2, time, second_bone, IKS_NONE, nullptr);
		gi.G2API_SetBoneIKState(ghoul2, time, first_bone, IKS_NONE, nullptr);

		//then reset the angles/anims on these PCJs
		gi.G2API_SetBoneAngles(&ghoul2[0], "lower_lumbar", vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, nullptr, 0, time);
		gi.G2API_SetBoneAngles(&ghoul2[0], "upper_lumbar", vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, nullptr, 0, time);
		gi.G2API_SetBoneAngles(&ghoul2[0], "thoracic", vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, nullptr, 0, time);
		gi.G2API_SetBoneAngles(&ghoul2[0], second_bone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, nullptr, 0, time);
		gi.G2API_SetBoneAngles(&ghoul2[0], first_bone, vec3_origin, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y,
			NEGATIVE_Z, nullptr, 0, time);

		//Get the anim/frames that the pelvis is on exactly, and match the left arm back up with them again.
		gi.G2API_GetBoneAnim(&ghoul2[0], anim_bone, time, &c_frame, &s_frame, &e_frame, &flags,
			&animSpeed, nullptr);
		gi.G2API_SetBoneAnim(&ghoul2[0], "lower_lumbar", s_frame, e_frame, flags, animSpeed, time, s_frame, 300);
		gi.G2API_SetBoneAnim(&ghoul2[0], "upper_lumbar", s_frame, e_frame, flags, animSpeed, time, s_frame, 300);
		gi.G2API_SetBoneAnim(&ghoul2[0], "thoracic", s_frame, e_frame, flags, animSpeed, time, s_frame, 300);
		gi.G2API_SetBoneAnim(&ghoul2[0], second_bone, s_frame, e_frame, flags, animSpeed, time, s_frame, 300);
		gi.G2API_SetBoneAnim(&ghoul2[0], first_bone, s_frame, e_frame, flags, animSpeed, time, s_frame, 300);

		//And finally, get rid of all the ik state effector data by calling with null bone name (similar to how we init it).
		gi.G2API_SetBoneIKState(ghoul2, time, nullptr, IKS_NONE, nullptr);

		*ik_in_progress = qfalse;
	}
}

void PM_IKUpdate(gentity_t* ent)
{
	//The bone we're holding them by and the next bone after that
	const auto anim_bone = "lower_lumbar";
	const auto first_bone = "lradius";
	const auto second_bone = "lhumerus";
	const auto default_bolt_name = "*r_hand";

	if (!ent->client)
	{
		return;
	}
	if (ent->client->ps.heldByClient <= ENTITYNUM_WORLD)
	{
		//then put our arm in this client's hand
		gentity_t* holder = &g_entities[ent->client->ps.heldByClient];

		if (holder && holder->inuse && holder->client && holder->ghoul2.size())
		{
			if (!ent->client->ps.heldByBolt)
			{
				//bolt wasn't set
				ent->client->ps.heldByBolt = gi.G2API_AddBolt(&holder->ghoul2[0], default_bolt_name);
			}
		}
		else
		{
			//they're gone, stop holding me
			ent->client->ps.heldByClient = 0;
			return;
		}

		if (ent->client->ps.heldByBolt)
		{
			mdxaBone_t bolt_matrix;
			vec3_t bolt_org;
			vec3_t t_angles;

			VectorCopy(holder->client->ps.viewangles, t_angles);
			t_angles[PITCH] = t_angles[ROLL] = 0;

			gi.G2API_GetBoltMatrix(holder->ghoul2, 0, ent->client->ps.heldByBolt, &bolt_matrix, t_angles,
				holder->client->ps.origin, level.time, nullptr, holder->s.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, bolt_org);

			const int grabbed_by_bolt = gi.G2API_AddBolt(&ent->ghoul2[0], first_bone);
			if (grabbed_by_bolt)
			{
				//point the limb
				BG_IK_MoveLimb(ent->ghoul2, grabbed_by_bolt, anim_bone, first_bone, second_bone,
					level.time, &ent->s, ent->client->clientInfo.animFileIndex,
					ent->client->ps.torsoAnim/*BOTH_DEAD1*/, bolt_org, &ent->client->ps.ikStatus,
					ent->client->ps.origin, ent->client->ps.viewangles, ent->s.modelScale,
					500, qfalse);

				//now see if we need to be turned and/or pulled
				vec3_t grab_diff, grabbed_by_org;

				VectorCopy(ent->client->ps.viewangles, t_angles);
				t_angles[PITCH] = t_angles[ROLL] = 0;

				gi.G2API_GetBoltMatrix(ent->ghoul2, 0, grabbed_by_bolt, &bolt_matrix, t_angles, ent->client->ps.origin,
					level.time, nullptr, ent->s.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, grabbed_by_org);

				//check for turn
				vec3_t org2_targ, org2_bolt;
				VectorSubtract(bolt_org, ent->currentOrigin, org2_targ);
				const float org2_targ_yaw = vectoyaw(org2_targ);
				VectorSubtract(grabbed_by_org, ent->currentOrigin, org2_bolt);
				const float org2_bolt_yaw = vectoyaw(org2_bolt);
				if (org2_targ_yaw - 1.0f > org2_bolt_yaw)
				{
					ent->currentAngles[YAW]++;
					G_SetAngles(ent, ent->currentAngles);
				}
				else if (org2_targ_yaw + 1.0f < org2_bolt_yaw)
				{
					ent->currentAngles[YAW]--;
					G_SetAngles(ent, ent->currentAngles);
				}

				//check for pull
				VectorSubtract(bolt_org, grabbed_by_org, grab_diff);
				if (VectorLength(grab_diff) > 128.0f)
				{
					//too far, release me
					ent->client->ps.heldByClient = holder->client->ps.heldClient = ENTITYNUM_NONE;
				}
				else if (true)
				{
					//pull me along
					trace_t trace;
					vec3_t dest_org;
					VectorAdd(ent->currentOrigin, grab_diff, dest_org);
					gi.trace(&trace, ent->currentOrigin, ent->mins, ent->maxs, dest_org, ent->s.number,
						ent->clipmask & ~holder->contents, static_cast<EG2_Collision>(0), 0);
					G_SetOrigin(ent, trace.endpos);
				}
				//FIXME: if I need to turn to keep my bone facing him, do so...
			}
			//don't let us fall?
			VectorClear(ent->client->ps.velocity);
			//FIXME: also make the holder point his holding limb at you?
		}
	}
	else if (ent->client->ps.ikStatus)
	{
		//make sure we aren't IKing if we don't have anyone to hold onto us.
		if (ent && ent->inuse && ent->client && ent->ghoul2.size())
		{
			if (!ent->client->ps.heldByBolt)
			{
				ent->client->ps.heldByBolt = gi.G2API_AddBolt(&ent->ghoul2[0], default_bolt_name);
			}
		}
		else
		{
			//This shouldn't happen, but just in case it does, we'll have a fail safe.
			ent->client->ps.heldByBolt = 0;
			ent->client->ps.ikStatus = qfalse;
		}

		if (ent->client->ps.heldByBolt)
		{
			BG_IK_MoveLimb(ent->ghoul2, ent->client->ps.heldByBolt, anim_bone, first_bone, second_bone,
				level.time, &ent->s, ent->client->clientInfo.animFileIndex,
				ent->client->ps.torsoAnim/*BOTH_DEAD1*/, vec3_origin,
				&ent->client->ps.ikStatus, ent->client->ps.origin,
				ent->client->ps.viewangles, ent->s.modelScale, 500, qtrue);
		}
	}
}

void BG_G2SetBoneAngles(const centity_t* cent, const int bone_index, const vec3_t angles, const int flags,
	const Eorientations up, const Eorientations right, const Eorientations forward,
	qhandle_t* model_list)
{
	if (bone_index != -1)
	{
		gi.G2API_SetBoneAnglesIndex(&cent->gent->ghoul2[0], bone_index, angles, flags, up, right, forward, model_list,
			0,
			0);
	}
}

constexpr auto MAX_YAWSPEED_X_WING = 1;
constexpr auto MAX_PITCHSPEED_X_WING = 1;

void PM_ScaleUcmd(const playerState_t* ps, usercmd_t* cmd, const gentity_t* gent)
{
	if (G_IsRidingVehicle(gent))
	{
		//driving a vehicle
		//clamp the turn rate
		constexpr int max_pitch_speed = MAX_PITCHSPEED_X_WING; //switch, eventually?  Or read from file?
		int diff = AngleNormalize180(SHORT2ANGLE((cmd->angles[PITCH] + ps->delta_angles[PITCH]))) - floor(
			ps->viewangles[PITCH]);

		if (diff > max_pitch_speed)
		{
			cmd->angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH] + max_pitch_speed) - ps->delta_angles[PITCH];
		}
		else if (diff < -max_pitch_speed)
		{
			cmd->angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH] - max_pitch_speed) - ps->delta_angles[PITCH];
		}

		//Um, WTF?  When I turn in a certain direction, I start going backwards?  Or strafing?
		constexpr int max_yaw_speed = MAX_YAWSPEED_X_WING; //switch, eventually?  Or read from file?
		diff = AngleNormalize180(SHORT2ANGLE(cmd->angles[YAW] + ps->delta_angles[YAW]) - floor(ps->viewangles[YAW]));

		//clamp the turn rate
		if (diff > max_yaw_speed)
		{
			cmd->angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW] + max_yaw_speed) - ps->delta_angles[YAW];
		}
		else if (diff < -max_yaw_speed)
		{
			cmd->angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW] - max_yaw_speed) - ps->delta_angles[YAW];
		}
	}
}

extern void SetClientViewAngle(gentity_t* ent, vec3_t angle);

qboolean PM_LockAngles(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, ent->client->ps.viewangles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAnglesToGripper(gentity_t* ent, usercmd_t* ucmd)
{
	if ((ent->client->ps.eFlags & EF_FORCE_GRIPPED
		|| ent->client->ps.eFlags & EF_FORCE_DRAINED
		|| ent->client->ps.eFlags & EF_FORCE_GRASPED)
		&& ent->enemy)
	{
		vec3_t dir, angles;

		VectorSubtract(ent->enemy->currentOrigin, ent->currentOrigin, dir);
		vectoangles(dir, angles);
		angles[PITCH] = AngleNormalize180(angles[PITCH]);
		angles[YAW] = AngleNormalize180(angles[YAW]);
		if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
		{
			//don't clamp angles when looking through a viewEntity
			SetClientViewAngle(ent, angles);
		}
		ucmd->angles[PITCH] = ANGLE2SHORT(angles[PITCH]) - ent->client->ps.delta_angles[PITCH];
		ucmd->angles[YAW] = ANGLE2SHORT(angles[YAW]) - ent->client->ps.delta_angles[YAW];
		return qtrue;
	}
	return qfalse;
}

qboolean PM_AdjustAnglesToPuller(gentity_t* ent, const gentity_t* puller, usercmd_t* ucmd, const qboolean face_away)
{
	//FIXME: make this more generic and have it actually *tell* the client what cmd angles it should be locked at?
	vec3_t dir, angles;

	VectorSubtract(puller->currentOrigin, ent->currentOrigin, dir);
	vectoangles(dir, angles);
	angles[PITCH] = AngleNormalize180(angles[PITCH]);
	if (face_away)
	{
		angles[YAW] += 180;
	}
	angles[YAW] = AngleNormalize180(angles[YAW]);
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, angles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(angles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(angles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAngleForWallRun(gentity_t* ent, usercmd_t* ucmd, const qboolean do_move)
{
	if ((ent->client->ps.legsAnim == BOTH_WALL_RUN_RIGHT || ent->client->ps.legsAnim == BOTH_WALL_RUN_LEFT) && ent->
		client->ps.legsAnimTimer > 500)
	{
		//wall-running and not at end of anim
		//stick to wall, if there is one
		vec3_t fwd, rt, trace_to;
		const vec3_t fwd_angles = { 0, ent->client->ps.viewangles[YAW], 0 };
		const vec3_t maxs = { ent->maxs[0], ent->maxs[1], 24 };
		const vec3_t mins = { ent->mins[0], ent->mins[1], 0 };
		trace_t trace;
		float dist, yaw_adjust;

		AngleVectors(fwd_angles, fwd, rt, nullptr);

		if (ent->client->ps.legsAnim == BOTH_WALL_RUN_RIGHT)
		{
			dist = 128;
			yaw_adjust = -90;
		}
		else
		{
			dist = -128;
			yaw_adjust = 90;
		}
		VectorMA(ent->currentOrigin, dist, rt, trace_to);
		gi.trace(&trace, ent->currentOrigin, mins, maxs, trace_to, ent->s.number, ent->clipmask,
			static_cast<EG2_Collision>(0), 0);
		if (trace.fraction < 1.0f
			&& (trace.plane.normal[2] >= 0.0f && trace.plane.normal[2] <= 0.4f))
		{
			trace_t trace2;
			vec3_t trace_to2;
			vec3_t wall_run_fwd, wall_run_angles = { 0 };

			wall_run_angles[YAW] = vectoyaw(trace.plane.normal) + yaw_adjust;
			AngleVectors(wall_run_angles, wall_run_fwd, nullptr, nullptr);

			VectorMA(ent->currentOrigin, 32, wall_run_fwd, trace_to2);
			gi.trace(&trace2, ent->currentOrigin, mins, maxs, trace_to2, ent->s.number, ent->clipmask,
				static_cast<EG2_Collision>(0), 0);
			if (trace2.fraction < 1.0f && DotProduct(trace2.plane.normal, wall_run_fwd) <= -0.999f)
			{
				//wall we can't run on in front of us
				trace.fraction = 1.0f; //just a way to get it to kick us off the wall below
			}
		}
		if (trace.fraction < 1.0f
			&& (trace.plane.normal[2] >= 0.0f && trace.plane.normal[2] <= 0.4f))
		{
			//still a vertical wall there
			//FIXME: don't pull around 90 turns
			//FIXME: simulate stepping up steps here, somehow?
			if (ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) || !player_locked && !
				PlayerAffectedByStasis())
			{
				if (ent->client->ps.legsAnim == BOTH_WALL_RUN_RIGHT)
				{
					ucmd->rightmove = 127;
				}
				else
				{
					ucmd->rightmove = -127;
				}
			}
			if (ucmd->upmove < 0)
			{
				ucmd->upmove = 0;
			}
			if (ent->NPC)
			{
				//invalid now
				VectorClear(ent->client->ps.moveDir);
			}
			//make me face perpendicular to the wall
			ent->client->ps.viewangles[YAW] = vectoyaw(trace.plane.normal) + yaw_adjust;
			if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//don't clamp angles when looking through a viewEntity
				SetClientViewAngle(ent, ent->client->ps.viewangles);
			}
			ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
			if (ent->s.number && !G_ControlledByPlayer(ent) || !player_locked && !PlayerAffectedByStasis())
			{
				if (do_move)
				{
					//push me forward
					float z_vel = ent->client->ps.velocity[2];
					if (z_vel > forceJumpStrength[FORCE_LEVEL_2] / 2.0f)
					{
						z_vel = forceJumpStrength[FORCE_LEVEL_2] / 2.0f;
					}
					//pull me toward the wall
					VectorScale(trace.plane.normal, -128, ent->client->ps.velocity);
					if (ent->client->ps.legsAnimTimer > 500)
					{
						//not at end of anim yet, pushing forward
						//FIXME: or MA?
						float speed = 175;
						if (ucmd->forwardmove < 0)
						{
							//slower
							speed = 100;
						}
						else if (ucmd->forwardmove > 0)
						{
							speed = 250; //running speed
						}
						VectorMA(ent->client->ps.velocity, speed, fwd, ent->client->ps.velocity);
					}
					ent->client->ps.velocity[2] = z_vel; //preserve z velocity
				}
			}
			ucmd->forwardmove = 0;
			return qtrue;
		}
		if (do_move)
		{
			//stop it
			if (ent->client->ps.legsAnim == BOTH_WALL_RUN_RIGHT)
			{
				NPC_SetAnim(ent, SETANIM_BOTH, BOTH_WALL_RUN_RIGHT_STOP, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			else if (ent->client->ps.legsAnim == BOTH_WALL_RUN_LEFT)
			{
				NPC_SetAnim(ent, SETANIM_BOTH, BOTH_WALL_RUN_LEFT_STOP, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
	}
	return qfalse;
}

extern int PM_AnimLength(const int index, const animNumber_t anim);

qboolean PM_AdjustAnglesForSpinningFlip(gentity_t* ent, usercmd_t* ucmd, const qboolean angles_only)
{
	float spin_start, spin_end, spin_amt;
	animNumber_t spin_anim;

	if (ent->client->ps.legsAnim == BOTH_JUMPFLIPSTABDOWN)
	{
		spin_anim = BOTH_JUMPFLIPSTABDOWN;
		spin_start = 300.0f; //700.0f;
		spin_end = 1400.0f;
		spin_amt = 180.0f;
	}
	else if (ent->client->ps.legsAnim == BOTH_JUMPFLIPSLASHDOWN1)
	{
		spin_anim = BOTH_JUMPFLIPSLASHDOWN1;
		spin_start = 300.0f; //700.0f;//1500.0f;
		spin_end = 1400.0f; //2300.0f;
		spin_amt = 180.0f;
	}
	else
	{
		if (!angles_only)
		{
			if (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
			{
				cg.overrides.active &= ~CG_OVERRIDE_3RD_PERSON_VOF;
				cg.overrides.thirdPersonVertOffset = 0;
			}
		}
		return qfalse;
	}
	const float anim_length = PM_AnimLength(ent->client->clientInfo.animFileIndex, spin_anim);
	const float elapsed_time = anim_length - ent->client->ps.legsAnimTimer;
	//face me
	if (elapsed_time >= spin_start && elapsed_time <= spin_end)
	{
		vec3_t new_angles;
		const float spin_length = spin_end - spin_start;
		VectorCopy(ent->client->ps.viewangles, new_angles);
		new_angles[YAW] = ent->angle + spin_amt * (elapsed_time - spin_start) / spin_length;
		if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
		{
			//don't clamp angles when looking through a viewEntity
			SetClientViewAngle(ent, new_angles);
		}
		ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
		ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
		if (angles_only)
		{
			return qtrue;
		}
	}
	else if (angles_only)
	{
		return qfalse;
	}
	//push me
	if (ent->client->ps.legsAnimTimer > 300)
	{
		//haven't landed or reached end of anim yet
		if (ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) || !player_locked && !
			PlayerAffectedByStasis())
		{
			vec3_t push_dir;
			const vec3_t push_angles = { 0, ent->angle, 0 };
			AngleVectors(push_angles, push_dir, nullptr, nullptr);
			if (DotProduct(ent->client->ps.velocity, push_dir) < 100)
			{
				VectorMA(ent->client->ps.velocity, 10, push_dir, ent->client->ps.velocity);
			}
		}
	}
	//do a dip in the view
	if (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent))
	{
		float view_dip;
		if (elapsed_time < anim_length / 2.0f)
		{
			//starting anim
			view_dip = elapsed_time / anim_length * -120.0f;
		}
		else
		{
			//ending anim
			view_dip = (anim_length - elapsed_time) / anim_length * -120.0f;
		}
		cg.overrides.active |= CG_OVERRIDE_3RD_PERSON_VOF;
		cg.overrides.thirdPersonVertOffset = cg_thirdPersonVertOffset.value + view_dip;
		//pm->ps->viewheight = standheight + viewDip;
	}
	return qtrue;
}

qboolean PM_AdjustAnglesForBackAttack(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent))
	{
		return qfalse;
	}
	if ((ent->client->ps.saber_move == LS_A_BACK || ent->client->ps.saber_move == LS_A_BACK_CR || ent->client->ps.
		saber_move == LS_A_BACKSTAB)
		&& PM_InAnimForSaberMove(ent->client->ps.torsoAnim, ent->client->ps.saber_move))
	{
		if (ent->client->ps.saber_move != LS_A_BACKSTAB || !ent->enemy || ent->s.number >= MAX_CLIENTS && !
			G_ControlledByPlayer(ent))
		{
			if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//don't clamp angles when looking through a viewEntity
				SetClientViewAngle(ent, ent->client->ps.viewangles);
			}
			ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
			ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
		}
		else
		{
			//keep player facing away from their enemy
			vec3_t enemy_behind_dir;
			VectorSubtract(ent->currentOrigin, ent->enemy->currentOrigin, enemy_behind_dir);
			const float enemy_behind_yaw = AngleNormalize180(vectoyaw(enemy_behind_dir));
			float yaw_error = AngleNormalize180(enemy_behind_yaw - AngleNormalize180(ent->client->ps.viewangles[YAW]));
			if (yaw_error > 1)
			{
				yaw_error = 1;
			}
			else if (yaw_error < -1)
			{
				yaw_error = -1;
			}
			ucmd->angles[YAW] = ANGLE2SHORT(AngleNormalize180(ent->client->ps.viewangles[YAW] + yaw_error)) - ent->
				client
				->ps.delta_angles[YAW];
			ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
		}
		return qtrue;
	}
	return qfalse;
}

qboolean PM_AdjustAnglesForSaberLock(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.saberLockTime > level.time)
	{
		if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
		{
			//don't clamp angles when looking through a viewEntity
			SetClientViewAngle(ent, ent->client->ps.viewangles);
		}

		ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
		ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
		return qtrue;
	}
	return qfalse;
}

int G_MinGetUpTime(const gentity_t* ent)
{
	if (ent
		&& ent->client
		&& (ent->client->ps.legsAnim == BOTH_PLAYER_PA_3_FLY
			|| ent->client->ps.legsAnim == BOTH_LK_DL_ST_T_SB_1_L
			|| ent->client->ps.legsAnim == BOTH_RELEASED))
	{
		//special cases
		return 200;
	}
	if (ent && ent->client && ent->client->NPC_class == CLASS_ALORA)
	{
		//alora springs up very quickly from knockdowns!
		return 1000;
	}
	if (ent && ent->client && ent->client->NPC_class == CLASS_STORMTROOPER)
	{
		//stormtroopers are slow to get up
		return 100;
	}
	if (ent && ent->client && ent->client->NPC_class == CLASS_GUARD)
	{
		//stormtroopers are slow to get up
		return 100;
	}
	if (ent && ent->client && ent->client->NPC_class == CLASS_STORMCOMMANDO)
	{
		//stormtroopers are slow to get up
		return 100;
	}
	if (ent && ent->s.clientNum < MAX_CLIENTS || G_ControlledByPlayer(ent))
	{
		//player can get up faster based on his/her force jump skill
		constexpr int get_up_time = PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME;
		if (ent && ent->client->ps.forcePowerLevel[FP_LEVITATION] >= FORCE_LEVEL_3)
		{
			return get_up_time + 400; //750
		}
		if (ent->client->ps.forcePowerLevel[FP_LEVITATION] == FORCE_LEVEL_2)
		{
			return get_up_time + 200; //500
		}
		if (ent->client->ps.forcePowerLevel[FP_LEVITATION] == FORCE_LEVEL_1)
		{
			return get_up_time + 100; //250
		}
		return get_up_time;
	}
	return 200;
}

qboolean PM_AdjustAnglesForKnockdown(gentity_t* ent, usercmd_t* ucmd, const qboolean angle_clamp_only)
{
	if (PM_InKnockDown(&ent->client->ps))
	{
		//being knocked down or getting up, can't do anything!
		if (!angle_clamp_only)
		{
			if (!PM_InForceGetUp(&ent->client->ps) && (ent->client->ps.legsAnimTimer > G_MinGetUpTime(ent)
				|| ent->s.number >= MAX_CLIENTS
				|| ent->client->ps.legsAnim == BOTH_GETUP1
				|| ent->client->ps.legsAnim == BOTH_GETUP2
				|| ent->client->ps.legsAnim == BOTH_GETUP3
				|| ent->client->ps.legsAnim == BOTH_GETUP4
				|| ent->client->ps.legsAnim == BOTH_GETUP5))
			{
				//can't get up yet
				ucmd->forwardmove = 0;
				ucmd->rightmove = 0;
			}
			if (ent->NPC)
			{
				VectorClear(ent->client->ps.moveDir);
			}
			//you can jump up out of a knockdown and you get get up into a crouch from a knockdown
			//if (!PM_InForceGetUp(&ent->client->ps) || ent->client->ps.legsAnimTimer > 800 || ent->s.weapon != WP_SABER)
			//{//can only attack if you've started a force-getup and are using the saber
			//	ucmd->buttons = 0;
			//}
			if (ent->health > 0)
			{
				//can only attack if you've started a force-getup and are using the saber
				ucmd->buttons = 0;
			}
		}
		if (!PM_InForceGetUp(&ent->client->ps))
		{
			//can't turn unless in a force getup
			if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//don't clamp angles when looking through a viewEntity
				SetClientViewAngle(ent, ent->client->ps.viewangles);
			}
			ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
			ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
			return qtrue;
		}
	}
	return qfalse;
}

qboolean PM_AdjustAnglesForDualJumpAttack(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, ent->client->ps.viewangles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAnglesForLongJump(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, ent->client->ps.viewangles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAnglesForGrapple(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, ent->client->ps.viewangles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAngleForWallRunUp(gentity_t* ent, usercmd_t* ucmd, const qboolean do_move)
{
	if (ent->client->ps.legsAnim == BOTH_FORCEWALLRUNFLIP_START)
	{
		//wall-running up
		//stick to wall, if there is one
		vec3_t fwd, trace_to;
		const vec3_t fwd_angles = { 0, ent->client->ps.viewangles[YAW], 0 };
		const vec3_t maxs = { ent->maxs[0], ent->maxs[1], 24 };
		const vec3_t mins = { ent->mins[0], ent->mins[1], 0 };
		trace_t trace;
		constexpr float dist = 128;

		AngleVectors(fwd_angles, fwd, nullptr, nullptr);
		VectorMA(ent->currentOrigin, dist, fwd, trace_to);
		gi.trace(&trace, ent->currentOrigin, mins, maxs, trace_to, ent->s.number, ent->clipmask,
			static_cast<EG2_Collision>(0), 0);
		if (trace.fraction > 0.5f)
		{
			//hmm, some room, see if there's a floor right here
			trace_t trace2;
			vec3_t top, bottom;
			VectorCopy(trace.endpos, top);
			top[2] += ent->mins[2] * -1 + 4.0f;
			VectorCopy(top, bottom);
			bottom[2] -= 64.0f; //was 32.0f
			gi.trace(&trace2, top, ent->mins, ent->maxs, bottom, ent->s.number, ent->clipmask,
				static_cast<EG2_Collision>(0), 0);
			if (!trace2.allsolid
				&& !trace2.startsolid
				&& trace2.fraction < 1.0f
				&& trace2.plane.normal[2] > 0.7f) //slope we can stand on
			{
				//cool, do the alt-flip and land on whet ever it is we just scaled up
				VectorScale(fwd, 100, ent->client->ps.velocity);
				ent->client->ps.velocity[2] += 200;
				NPC_SetAnim(ent, SETANIM_BOTH, BOTH_FORCEWALLRUNFLIP_ALT, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				ent->client->ps.pm_flags |= PMF_JUMP_HELD;
				ent->client->ps.pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
				ent->client->ps.forcePowersActive |= 1 << FP_LEVITATION;
				G_AddEvent(ent, EV_JUMP, 0);
				ucmd->upmove = 0;
				return qfalse;
			}
		}
		if (ent->client->ps.legsAnimTimer > 0
			&& ucmd->forwardmove > 0
			&& trace.fraction < 1.0f
			&& (trace.plane.normal[2] >= 0.0f && trace.plane.normal[2] <= MAX_WALL_RUN_Z_NORMAL))
		{
			//still a vertical wall there
			//make sure there's not a ceiling above us!
			trace_t trace2;
			VectorCopy(ent->currentOrigin, trace_to);
			trace_to[2] += 64;
			gi.trace(&trace2, ent->currentOrigin, mins, maxs, trace_to, ent->s.number, ent->clipmask,
				static_cast<EG2_Collision>(0), 0);
			if (trace2.fraction < 1.0f)
			{
				//will hit a ceiling, so force jump-off right now
				//NOTE: hits any entity or clip brush in the way, too, not just architecture!
			}
			else
			{
				//all clear, keep going
				//FIXME: don't pull around 90 turns
				//FIXME: simulate stepping up steps here, somehow?
				if (ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) || !player_locked && !
					PlayerAffectedByStasis())
				{
					ucmd->forwardmove = 127;
				}
				if (ucmd->upmove < 0)
				{
					ucmd->upmove = 0;
				}
				if (ent->NPC)
				{
					//invalid now
					VectorClear(ent->client->ps.moveDir);
				}
				//make me face the wall
				ent->client->ps.viewangles[YAW] = vectoyaw(trace.plane.normal) + 180;
				if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
				{
					//don't clamp angles when looking through a viewEntity
					SetClientViewAngle(ent, ent->client->ps.viewangles);
				}
				ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
				if (ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) || !player_locked && !
					PlayerAffectedByStasis())
				{
					if (do_move)
					{
						//pull me toward the wall
						VectorScale(trace.plane.normal, -128, ent->client->ps.velocity);
						//push me up
						if (ent->client->ps.legsAnimTimer > 200)
						{
							//not at end of anim yet
							constexpr float speed = 300;
							ent->client->ps.velocity[2] = speed; //preserve z velocity
						}
						//pull me toward the wall
						//VectorMA( ent->client->ps.velocity, -128, trace.plane.normal, ent->client->ps.velocity );
					}
				}
				ucmd->forwardmove = 0;
				return qtrue;
			}
		}
		//failed!
		if (do_move)
		{
			//stop it
			VectorScale(fwd, WALL_RUN_UP_BACKFLIP_SPEED, ent->client->ps.velocity);
			ent->client->ps.velocity[2] += 200;
			NPC_SetAnim(ent, SETANIM_BOTH, BOTH_FORCEWALLRUNFLIP_END, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			ent->client->ps.pm_flags |= PMF_JUMP_HELD;
			ent->client->ps.pm_flags |= PMF_JUMPING | PMF_SLOW_MO_FALL;
			ent->client->ps.forcePowersActive |= 1 << FP_LEVITATION;
			G_AddEvent(ent, EV_JUMP, 0);
			ucmd->upmove = 0;
			//return qtrue;
		}
	}
	return qfalse;
}

float G_ForceWallJumpStrength()
{
	return forceJumpStrength[FORCE_LEVEL_3] / 2.5f;
}

qboolean PM_AdjustAngleForWallJump(gentity_t* ent, usercmd_t* ucmd, const qboolean do_move)
{
	if (PM_InLedgeMove(ent->client->ps.legsAnim))
	{
		//Ledge movin'  Let the ledge move function handle it.
		return qfalse;
	}
	if (PM_InReboundJump(ent->client->ps.legsAnim)
		|| PM_InReboundHold(ent->client->ps.legsAnim))
	{
		//hugging wall, getting ready to jump off
		//stick to wall, if there is one
		vec3_t check_dir, trace_to;
		const vec3_t fwd_angles = { 0, ent->client->ps.viewangles[YAW], 0 };
		const vec3_t maxs = { ent->maxs[0], ent->maxs[1], 24 };
		const vec3_t mins = { ent->mins[0], ent->mins[1], 0 };
		trace_t trace;
		constexpr float dist = 128;
		float yaw_adjust;
		switch (ent->client->ps.legsAnim)
		{
		case BOTH_FORCEWALLREBOUND_RIGHT:
		case BOTH_FORCEWALLHOLD_RIGHT:
			AngleVectors(fwd_angles, nullptr, check_dir, nullptr);
			yaw_adjust = -90;
			break;
		case BOTH_FORCEWALLREBOUND_LEFT:
		case BOTH_FORCEWALLHOLD_LEFT:
			AngleVectors(fwd_angles, nullptr, check_dir, nullptr);
			VectorScale(check_dir, -1, check_dir);
			yaw_adjust = 90;
			break;
		case BOTH_FORCEWALLREBOUND_FORWARD:
		case BOTH_FORCEWALLHOLD_FORWARD:
			AngleVectors(fwd_angles, check_dir, nullptr, nullptr);
			yaw_adjust = 180;
			break;
		case BOTH_FORCEWALLREBOUND_BACK:
		case BOTH_FORCEWALLHOLD_BACK:
			AngleVectors(fwd_angles, check_dir, nullptr, nullptr);
			VectorScale(check_dir, -1, check_dir);
			yaw_adjust = 0;
			break;
		default:
			//WTF???
			return qfalse;
		}
		if (ent->client->ps.forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
		{
			//uber-skillz
			if (ucmd->upmove > 0)
			{
				//hold on until you let go manually
				if (PM_InReboundHold(ent->client->ps.legsAnim))
				{
					//keep holding
					if (ent->client->ps.legsAnimTimer < 150)
					{
						ent->client->ps.legsAnimTimer = 150;
					}
				}
				else
				{
					//if got to hold part of anim, play hold anim
					if (ent->client->ps.legsAnimTimer <= 300)
					{
						ent->client->ps.SaberDeactivate();
						NPC_SetAnim(ent, SETANIM_BOTH,
							BOTH_FORCEWALLRELEASE_FORWARD + (ent->client->ps.legsAnim -
								BOTH_FORCEWALLHOLD_FORWARD), SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						ent->client->ps.legsAnimTimer = ent->client->ps.torsoAnimTimer = 150;
					}
				}
			}
		}
		VectorMA(ent->currentOrigin, dist, check_dir, trace_to);
		gi.trace(&trace, ent->currentOrigin, mins, maxs, trace_to, ent->s.number, ent->clipmask,
			static_cast<EG2_Collision>(0), 0);
		if ( //ucmd->upmove <= 0 &&
			ent->client->ps.legsAnimTimer > 100 &&
			trace.fraction < 1.0f && fabs(trace.plane.normal[2]) <= MAX_WALL_GRAB_SLOPE)
		{
			if (ucmd->upmove < 0)
			{
				ucmd->upmove = 0;
			}
			if (ent->NPC)
			{
				//invalid now
				VectorClear(ent->client->ps.moveDir);
			}
			//align me to the wall
			ent->client->ps.viewangles[YAW] = vectoyaw(trace.plane.normal) + yaw_adjust;
			if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//don't clamp angles when looking through a viewEntity
				SetClientViewAngle(ent, ent->client->ps.viewangles);
			}
			ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
			if (ent->s.number >= MAX_CLIENTS && !G_ControlledByPlayer(ent) || !player_locked && !
				PlayerAffectedByStasis())
			{
				if (do_move)
				{
					//pull me toward the wall
					VectorScale(trace.plane.normal, -128, ent->client->ps.velocity);
				}
			}
			ucmd->upmove = 0;
			ent->client->ps.pm_flags |= PMF_STUCK_TO_WALL;
			return qtrue;
		}
		if (do_move
			&& ent->client->ps.pm_flags & PMF_STUCK_TO_WALL)
		{
			//jump off
			//push off of it!
			ent->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
			ent->client->ps.velocity[0] = ent->client->ps.velocity[1] = 0;
			VectorScale(check_dir, -JUMP_OFF_WALL_SPEED, ent->client->ps.velocity);
			ent->client->ps.velocity[2] = G_ForceWallJumpStrength();
			ent->client->ps.pm_flags |= PMF_JUMPING | PMF_JUMP_HELD;
			if (g_SerenityJediEngineMode->integer == 2 && ent->client->ps.forcePowerLevel[FP_LEVITATION] <
				FORCE_LEVEL_3)
			{
				//short burst
				G_SoundOnEnt(ent, CHAN_BODY, "sound/weapons/force/jumpsmall.mp3");
			}
			else
			{
				//holding it
				G_SoundOnEnt(ent, CHAN_BODY, "sound/weapons/force/jump.mp3");
			}
			ent->client->ps.forcePowersActive |= 1 << FP_LEVITATION;
			WP_ForcePowerDrain(ent, FP_LEVITATION, 10);
			if (PM_InReboundHold(ent->client->ps.legsAnim))
			{
				//if was in hold pose, release now
				NPC_SetAnim(ent, SETANIM_BOTH,
					BOTH_FORCEWALLRELEASE_FORWARD + (ent->client->ps.legsAnim - BOTH_FORCEWALLHOLD_FORWARD),
					SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
			//no control for half a second
			ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
			ent->client->ps.pm_time = 500;
			ucmd->forwardmove = 0;
			ucmd->rightmove = 0;
			ucmd->upmove = 0;
			//return qtrue;
		}
	}
	ent->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
	return qfalse;
}

qboolean PM_AdjustAnglesForBFKick(gentity_t* self, usercmd_t* ucmd, vec3_t fwd_angs, const qboolean aim_front)
{
	gentity_t* entity_list[MAX_GENTITIES];
	vec3_t mins{}, maxs{};
	const int radius = self->maxs[0] * 1.5f + self->maxs[0] * 1.5f + STAFF_KICK_RANGE + 24.0f;
	//a little wide on purpose
	vec3_t center, v_fwd;
	float best_dist = Q3_INFINITE;
	float best_dot = -1.1f;
	float best_yaw = Q3_INFINITE;

	AngleVectors(fwd_angs, v_fwd, nullptr, nullptr);

	VectorCopy(self->currentOrigin, center);

	for (int i = 0; i < 3; i++)
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	const int num_listed_entities = gi.EntitiesInBox(mins, maxs, entity_list, MAX_GENTITIES);

	for (int e = 0; e < num_listed_entities; e++)
	{
		vec3_t vec2_ent;
		const gentity_t* ent = entity_list[e];

		if (ent == self)
			continue;
		if (ent->owner == self)
			continue;
		if (!ent->inuse)
			continue;
		//not a client?
		if (!ent->client)
			continue;
		//ally?
		if (ent->client->playerTeam == self->client->playerTeam)
			continue;
		//on the ground
		if (PM_InKnockDown(&ent->client->ps))
			continue;
		//dead?
		if (ent->health <= 0)
		{
			if (level.time - ent->s.time > 2000)
			{
				//died more than 2 seconds ago, forget him
				continue;
			}
		}
		//too far?
		VectorSubtract(ent->currentOrigin, center, vec2_ent);
		const float dist_to_ent = VectorNormalize(vec2_ent);
		if (dist_to_ent > radius)
			continue;

		if (!aim_front)
		{
			//aim away from them
			VectorScale(vec2_ent, -1, vec2_ent);
		}
		const float dot = DotProduct(vec2_ent, v_fwd);
		if (dot < 0.0f)
		{
			//never turn all the way around
			continue;
		}
		if (dot > best_dot || best_dot - dot < 0.25f && dist_to_ent - best_dist > 8.0f)
		{
			//more in front... OR: still relatively close to in front and significantly closer
			best_dot = dot;
			best_dist = dist_to_ent;
			best_yaw = vectoyaw(vec2_ent);
		}
	}
	if (best_yaw != Q3_INFINITE && best_yaw != fwd_angs[YAW])
	{
		//aim us at them
		AngleNormalize180(best_yaw);
		AngleNormalize180(fwd_angs[YAW]);
		const float ang_diff = AngleSubtract(best_yaw, fwd_angs[YAW]);
		AngleNormalize180(ang_diff);
		if (fabs(ang_diff) <= 3.0f)
		{
			self->client->ps.viewangles[YAW] = best_yaw;
		}
		else if (ang_diff > 0.0f)
		{
			//more than 3 degrees higher
			self->client->ps.viewangles[YAW] += 3.0f;
		}
		else
		{
			//must be more than 3 less than
			self->client->ps.viewangles[YAW] -= 3.0f;
		}
		if (self->client->ps.viewEntity <= 0 || self->client->ps.viewEntity >= ENTITYNUM_WORLD)
		{
			//don't clamp angles when looking through a viewEntity
			SetClientViewAngle(self, self->client->ps.viewangles);
		}
		ucmd->angles[YAW] = ANGLE2SHORT(self->client->ps.viewangles[YAW]) - self->client->ps.delta_angles[YAW];
		return qtrue;
	}
	//lock these angles
	if (self->client->ps.viewEntity <= 0 || self->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(self, self->client->ps.viewangles);
	}
	ucmd->angles[YAW] = ANGLE2SHORT(self->client->ps.viewangles[YAW]) - self->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAnglesForStabDown(gentity_t* ent, usercmd_t* ucmd)
{
	if (PM_StabDownAnim(ent->client->ps.torsoAnim)
		&& ent->client->ps.torsoAnimTimer)
	{
		//lock our angles
		ucmd->forwardmove = ucmd->rightmove = ucmd->upmove = 0;
		const float elapsed_time = PM_AnimLength(ent->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(ent->client->ps.torsoAnim)) - ent->client->ps
			.
			torsoAnimTimer;
		//FIXME: scale forwardmove by dist from enemy - none if right next to him (so we don't slide off!)
		if (ent->enemy)
		{
			const float dist2_enemy = DistanceHorizontal(ent->enemy->currentOrigin, ent->currentOrigin);
			if (dist2_enemy > ent->enemy->maxs[0] * 1.5f + ent->maxs[0] * 1.5f)
			{
				ent->client->ps.speed = dist2_enemy * 2.0f;
			}
			else
			{
				ent->client->ps.speed = 0;
			}
		}
		else
		{
			ent->client->ps.speed = 150;
		}
		switch (ent->client->ps.legsAnim)
		{
		case BOTH_STABDOWN:
			if (elapsed_time >= 300 && elapsed_time < 900)
			{
				//push forward?
				//FIXME: speed!
				ucmd->forwardmove = 127;
			}
			break;
		case BOTH_STABDOWN_STAFF:
			if (elapsed_time > 400 && elapsed_time < 950)
			{
				//push forward?
				//FIXME: speed!
				ucmd->forwardmove = 127;
			}
			break;
		case BOTH_STABDOWN_DUAL:
			if (elapsed_time >= 300 && elapsed_time < 900)
			{
				//push forward?
				//FIXME: speed!
				ucmd->forwardmove = 127;
			}
			break;
		default:;
		}
		VectorClear(ent->client->ps.moveDir);

		if (ent->enemy //still have a valid enemy
			&& ent->enemy->client //who is a client
			&& (PM_InKnockDownNoGetup(&ent->enemy->client->ps) //enemy still on ground
				|| PM_InGetUpNoRoll(&ent->enemy->client->ps))) //or getting straight up
		{
			//aim at the enemy
			vec3_t enemy_dir;
			VectorSubtract(ent->enemy->currentOrigin, ent->currentOrigin, enemy_dir);
			const float enemy_yaw = AngleNormalize180(vectoyaw(enemy_dir));
			float yaw_error = AngleNormalize180(enemy_yaw - AngleNormalize180(ent->client->ps.viewangles[YAW]));
			if (yaw_error > 1)
			{
				yaw_error = 1;
			}
			else if (yaw_error < -1)
			{
				yaw_error = -1;
			}
			ucmd->angles[YAW] = ANGLE2SHORT(AngleNormalize180(ent->client->ps.viewangles[YAW] + yaw_error)) - ent->
				client
				->ps.delta_angles[YAW];
			ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
		}
		else
		{
			//can't turn
			if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//don't clamp angles when looking through a viewEntity
				SetClientViewAngle(ent, ent->client->ps.viewangles);
			}
			ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
			ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
		}
		return qtrue;
	}
	return qfalse;
}

qboolean PM_AdjustAnglesForSpinProtect(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.torsoAnim == BOTH_A6_SABERPROTECT)
	{
		//in the dual spin thing
		if (ent->client->ps.torsoAnimTimer)
		{
			//flatten and lock our angles, pull back camera
			//FIXME: lerp this
			ent->client->ps.viewangles[PITCH] = 0;
			if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
			{
				//don't clamp angles when looking through a viewEntity
				SetClientViewAngle(ent, ent->client->ps.viewangles);
			}
			ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
			ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
			return qtrue;
		}
	}
	return qfalse;
}

qboolean PM_AdjustAnglesForWallRunUpFlipAlt(gentity_t* ent, usercmd_t* ucmd)
{
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, ent->client->ps.viewangles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(ent->client->ps.viewangles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(ent->client->ps.viewangles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean PM_AdjustAnglesForHeldByMonster(gentity_t* ent, const gentity_t* monster, usercmd_t* ucmd)
{
	vec3_t new_view_angles;
	if (!monster || !monster->client)
	{
		return qfalse;
	}
	VectorScale(monster->client->ps.viewangles, -1, new_view_angles);
	if (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)
	{
		//don't clamp angles when looking through a viewEntity
		SetClientViewAngle(ent, new_view_angles);
	}
	ucmd->angles[PITCH] = ANGLE2SHORT(new_view_angles[PITCH]) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT(new_view_angles[YAW]) - ent->client->ps.delta_angles[YAW];
	return qtrue;
}

qboolean G_OkayToLean(const playerState_t* ps, const usercmd_t* cmd, const qboolean interrupt_okay)
{
	if ((ps->clientNum < MAX_CLIENTS || G_ControlledByPlayer(&g_entities[ps->clientNum])) //player
		&& ps->groundEntityNum != ENTITYNUM_NONE //on ground
		&& (interrupt_okay //okay to interrupt a lean
			&& !PM_CrouchAnim(ps->legsAnim)
			&& PM_DodgeAnim(ps->torsoAnim)
			|| PM_BlockAnim(ps->torsoAnim) || PM_BlockDualAnim(ps->torsoAnim) || PM_BlockStaffAnim(ps->torsoAnim)
			|| !ps->weaponTime //not attacking or being prevented from attacking
			&& !ps->legsAnimTimer //not in any held legs anim
			&& !ps->torsoAnimTimer) //not in any held torso anim
		&& !(cmd->buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_FORCE_LIGHTNING | BUTTON_USE_FORCE | BUTTON_DASH
			| BUTTON_FORCE_DRAIN | BUTTON_FORCEGRIP | BUTTON_REPULSE | BUTTON_FORCEGRASP)) //not trying to attack
		&& !(ps->ManualBlockingFlags & 1 << HOLDINGBLOCK)
		&& VectorCompare(ps->velocity, vec3_origin) //not moving
		&& !cg_usingInFrontOf) //use button wouldn't be used for anything else
	{
		return qtrue;
	}
	return qfalse;
}

qboolean G_OkayToDoStandingBlock(const playerState_t* ps, const usercmd_t* cmd, const qboolean interrupt_okay)
{
	if ((ps->clientNum < MAX_CLIENTS || G_ControlledByPlayer(&g_entities[ps->clientNum])) //player
		&& ps->groundEntityNum != ENTITYNUM_NONE //on ground
		&& (interrupt_okay
			&& !in_camera
			&& !PM_SaberInMassiveBounce(ps->torsoAnim)
			&& !PM_SaberInBashedAnim(ps->torsoAnim)
			&& (PM_BlockAnim(ps->torsoAnim) || PM_BlockDualAnim(ps->torsoAnim) || PM_BlockStaffAnim(ps->torsoAnim))
			//already leaning
			|| !ps->weaponTime //not attacking or being prevented from attacking
			&& !ps->legsAnimTimer //not in any held legs anim
			&& !ps->torsoAnimTimer) //not in any held torso anim
		&& !(cmd->buttons & (BUTTON_ALT_ATTACK | BUTTON_FORCE_LIGHTNING | BUTTON_USE_FORCE | BUTTON_FORCE_DRAIN |
			BUTTON_DASH | BUTTON_FORCEGRIP | BUTTON_REPULSE | BUTTON_FORCEGRASP | BUTTON_LIGHTNING_STRIKE |
			BUTTON_PROJECTION)) //not trying to attack
		&& VectorCompare(ps->velocity, vec3_origin) //not moving
		&& !cg_usingInFrontOf) //use button wouldn't be used for anything else
	{
		return qtrue;
	}
	return qfalse;
}

/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move

//FIXME: Now that they pmove twice per think, they snap-look really fast
================
*/
void PM_UpdateViewAngles(int saberAnimLevel, playerState_t* ps, usercmd_t* cmd, gentity_t* gent)
{
	short temp;
	float root_pitch = 0, pitch_min = -75, pitch_max = 75, yaw_min = 0, yaw_max = 0, locked_yaw_value = 0;
	//just to shut up warnings
	int i;
	vec3_t start, end, tmins, tmaxs, right;
	trace_t trace;
	qboolean locked_yaw = qfalse/*, clamped = qfalse*/;

	if (ps->pm_type == PM_INTERMISSION)
	{
		return; // no view changes at all
	}

	//TEMP
#if 0 //rww 12/23/02 - I'm disabling this for now, I'm going to try to make it work with my new rag stuff
	if (gent != NULL)
	{
		PM_IKUpdate(gent);
	}
#endif

	if (ps->pm_type != PM_SPECTATOR && ps->stats[STAT_HEALTH] <= 0)
	{
		return; // no view changes at all
	}

	if (ps->stasisTime > level.time)
	{
		return;
	}

	if (ps->stasisJediTime > level.time)
	{
		return;
	}

	if (ps->clientNum != 0 && gent != nullptr && gent->client != nullptr)
	{
		if (gent->client->renderInfo.renderFlags & RF_LOCKEDANGLE)
		{
			pitch_min = 0 - gent->client->renderInfo.headPitchRangeUp - gent->client->renderInfo.torsoPitchRangeUp;
			pitch_max = gent->client->renderInfo.headPitchRangeDown + gent->client->renderInfo.torsoPitchRangeDown;

			yaw_min = 0 - gent->client->renderInfo.headYawRangeLeft - gent->client->renderInfo.torsoYawRangeLeft;
			yaw_max = gent->client->renderInfo.headYawRangeRight + gent->client->renderInfo.torsoYawRangeRight;

			locked_yaw = qtrue;
			locked_yaw_value = gent->client->renderInfo.lockYaw;
		}
		else
		{
			pitch_min = -gent->client->renderInfo.headPitchRangeUp - gent->client->renderInfo.torsoPitchRangeUp;
			pitch_max = gent->client->renderInfo.headPitchRangeDown + gent->client->renderInfo.torsoPitchRangeDown;
		}
	}

	if (ps->eFlags & EF_LOCKED_TO_WEAPON)
	{
		// Emplaced guns have different pitch capabilities
		if (gent && gent->owner && gent->owner->e_UseFunc == useF_eweb_use)
		{
			pitch_min = -15;
			pitch_max = 10;
		}
		else
		{
			pitch_min = -35;
			pitch_max = 30;
		}
	}

	// If we're a vehicle, or we're riding a vehicle...?
	if (gent && gent->client && gent->client->NPC_class == CLASS_VEHICLE && gent->m_pVehicle)
	{
		Vehicle_t* p_veh = nullptr;
		p_veh = gent->m_pVehicle;
		// If we're a vehicle...
		if (p_veh->m_pVehicleInfo->Inhabited(p_veh) || p_veh->m_iBoarding != 0 || p_veh->m_pVehicleInfo->type !=
			VH_ANIMAL)
		{
			locked_yaw_value = p_veh->m_vOrientation[YAW];
			yaw_max = yaw_min = 0;
			root_pitch = p_veh->m_vOrientation[PITCH]; //???  what if goes over 90 when add the min/max?
			pitch_max = 0.0f; //p_veh->m_pVehicleInfo->pitchLimit;
			pitch_min = 0.0f; //-pitchMax;
			locked_yaw = qtrue;
		}
		// If we're riding a vehicle...
		else if ((p_veh = G_IsRidingVehicle(gent)) != nullptr)
		{
			if (p_veh->m_pVehicleInfo->type != VH_ANIMAL)
			{
				//animals just turn normally, no clamping
				locked_yaw_value = 0; //gent->owner->client->ps.vehicleAngles[YAW];
				locked_yaw = qtrue;
				yaw_max = p_veh->m_pVehicleInfo->lookYaw;
				yaw_min = -yaw_max;
				if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
				{
					root_pitch = p_veh->m_vOrientation[PITCH];
					//gent->owner->client->ps.vehicleAngles[PITCH];//???  what if goes over 90 when add the min/max?
					pitch_max = p_veh->m_pVehicleInfo->pitchLimit;
					pitch_min = -pitch_max;
				}
				else
				{
					root_pitch = 0;
					//gent->owner->client->ps.vehicleAngles[PITCH];//???  what if goes over 90 when add the min/max?
					pitch_max = p_veh->m_pVehicleInfo->lookPitch;
					pitch_min = -pitch_max;
				}
			}
		}
	}

	const short pitch_clamp_min = ANGLE2SHORT(root_pitch + pitch_min);
	const short pitch_clamp_max = ANGLE2SHORT(root_pitch + pitch_max);
	const short yaw_clamp_min = ANGLE2SHORT(locked_yaw_value + yaw_min);
	const short yaw_clamp_max = ANGLE2SHORT(locked_yaw_value + yaw_max);

	// circularly clamp the angles with deltas
	for (i = 0; i < 3; i++)
	{
		temp = cmd->angles[i] + ps->delta_angles[i];
		if (i == PITCH)
		{
			//FIXME get this limit from the NPCs stats?
			// don't let the player look up or down more than 90 degrees
			if (temp > pitch_clamp_max)
			{
				ps->delta_angles[i] = pitch_clamp_max - cmd->angles[i] & 0xffff; //& clamp to short
				temp = pitch_clamp_max;
				//clamped = qtrue;
			}
			else if (temp < pitch_clamp_min)
			{
				ps->delta_angles[i] = pitch_clamp_min - cmd->angles[i] & 0xffff; //& clamp to short
				temp = pitch_clamp_min;
				//clamped = qtrue;
			}
		}

		if (i == YAW && locked_yaw)
		{
			//FIXME get this limit from the NPCs stats?
			// don't let the player look up or down more than 90 degrees
			if (temp > yaw_clamp_max)
			{
				ps->delta_angles[i] = yaw_clamp_max - cmd->angles[i] & 0xffff; //& clamp to short
				temp = yaw_clamp_max;
				//clamped = qtrue;
			}
			else if (temp < yaw_clamp_min)
			{
				ps->delta_angles[i] = yaw_clamp_min - cmd->angles[i] & 0xffff; //& clamp to short
				temp = yaw_clamp_min;
				//clamped = qtrue;
			}
			ps->viewangles[i] = SHORT2ANGLE(temp);
		}
		else
		{
			ps->viewangles[i] = SHORT2ANGLE(temp);
		}
	}

	if (gent
		&& gent->client && gent->client->NPC_class != CLASS_DROIDEKA
		&& gent->client->ps.ManualBlockingFlags & 1 << MBF_MELEEDODGE)
	{
		//check leaning
		if (G_OkayToLean(ps, cmd, qtrue) && (cmd->rightmove || cmd->forwardmove)) //pushing a direction
		{
			int anim = -1;
			if (cmd->rightmove > 0)
			{
				//lean right
				if (cmd->forwardmove > 0)
				{
					//lean forward right
					if (ps->torsoAnim == BOTH_DODGE_HOLD_FR)
					{
						anim = ps->torsoAnim;
					}
					else
					{
						anim = BOTH_DODGE_FR;
					}
				}
				else if (cmd->forwardmove < 0)
				{
					//lean backward right
					if (ps->torsoAnim == BOTH_DODGE_HOLD_BR)
					{
						anim = ps->torsoAnim;
					}
					else
					{
						anim = BOTH_DODGE_BR;
					}
				}
				else
				{
					//lean right
					if (ps->torsoAnim == BOTH_DODGE_HOLD_R)
					{
						anim = ps->torsoAnim;
					}
					else
					{
						anim = BOTH_DODGE_R;
					}
				}
			}
			else if (cmd->rightmove < 0)
			{
				//lean left
				if (cmd->forwardmove > 0)
				{
					//lean forward left
					if (ps->torsoAnim == BOTH_DODGE_HOLD_FL)
					{
						anim = ps->torsoAnim;
					}
					else
					{
						anim = BOTH_DODGE_FL;
					}
				}
				else if (cmd->forwardmove < 0)
				{
					//lean backward left
					if (ps->torsoAnim == BOTH_DODGE_HOLD_BL)
					{
						anim = ps->torsoAnim;
					}
					else
					{
						anim = BOTH_DODGE_BL;
					}
				}
				else
				{
					//lean left
					if (ps->torsoAnim == BOTH_DODGE_HOLD_L)
					{
						anim = ps->torsoAnim;
					}
					else
					{
						anim = BOTH_DODGE_L;
					}
				}
			}
			else
			{
				//not pressing either side
				if (cmd->forwardmove > 0)
				{
					//lean forward
					if (PM_DodgeAnim(ps->torsoAnim))
					{
						anim = ps->torsoAnim;
					}
					else if (Q_irand(0, 1))
					{
						anim = BOTH_DODGE_FL;
					}
					else
					{
						anim = BOTH_DODGE_FR;
					}
				}
				else if (cmd->forwardmove < 0)
				{
					//lean backward
					if (PM_DodgeAnim(ps->torsoAnim))
					{
						anim = ps->torsoAnim;
					}
					else if (Q_irand(0, 1))
					{
						anim = BOTH_DODGE_B;
					}
					else
					{
						anim = BOTH_DODGE_B;
					}
				}
			}
			if (anim != -1)
			{
				int extra_hold_time = 0;
				if (PM_DodgeAnim(ps->torsoAnim) && !PM_DodgeHoldAnim(ps->torsoAnim))
				{
					//already in a dodge
					//use the hold pose, don't start it all over again
					anim = BOTH_DODGE_HOLD_FL + (anim - BOTH_DODGE_FL);
					extra_hold_time = 300;
				}
				if (anim == pm->ps->torsoAnim)
				{
					if (pm->ps->torsoAnimTimer < 200)
					{
						pm->ps->torsoAnimTimer = 200;
					}
				}
				else
				{
					NPC_SetAnim(gent, SETANIM_TORSO, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				}
				if (extra_hold_time && ps->torsoAnimTimer < extra_hold_time)
				{
					ps->torsoAnimTimer += extra_hold_time;
				}
				if (ps->groundEntityNum != ENTITYNUM_NONE && !cmd->upmove)
				{
					NPC_SetAnim(gent, SETANIM_LEGS, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					ps->legsAnimTimer = ps->torsoAnimTimer;
				}
				else
				{
					NPC_SetAnim(gent, SETANIM_LEGS, anim, SETANIM_FLAG_NORMAL);
				}
				ps->weaponTime = ps->torsoAnimTimer;
				ps->leanStopDebounceTime = ceil(static_cast<float>(ps->torsoAnimTimer) / 50.0f); //20;
			}
		}
		else if (!cg.zoomMode && cmd->rightmove != 0 && !cmd->forwardmove && cmd->upmove <= 0)
		{
			//Only lean if holding use button, strafing and not moving forward or back and not jumping
			int leanofs = 0;
			vec3_t viewangles;

			if (cmd->rightmove > 0)
			{
				if (ps->leanofs <= 28)
				{
					leanofs = ps->leanofs + 4;
				}
				else
				{
					leanofs = 32;
				}
			}
			else
			{
				if (ps->leanofs >= -28)
				{
					leanofs = ps->leanofs - 4;
				}
				else
				{
					leanofs = -32;
				}
			}

			VectorCopy(ps->origin, start);
			start[2] += ps->viewheight;
			VectorCopy(ps->viewangles, viewangles);
			viewangles[ROLL] = 0;
			AngleVectors(ps->viewangles, nullptr, right, nullptr);
			VectorNormalize(right);
			right[2] = leanofs < 0 ? 0.25 : -0.25;
			VectorMA(start, leanofs, right, end);
			VectorSet(tmins, -8, -8, -4);
			VectorSet(tmaxs, 8, 8, 4);
			gi.trace(&trace, start, tmins, tmaxs, end, gent->s.number, MASK_PLAYERSOLID, static_cast<EG2_Collision>(0),
				0);

			ps->leanofs = floor(static_cast<float>(leanofs) * trace.fraction);

			ps->leanStopDebounceTime = 20;
		}
		else
		{
			if (cmd->forwardmove || cmd->upmove > 0)
			{
				if (pm->ps->legsAnim == LEGS_LEAN_RIGHT1 ||
					pm->ps->legsAnim == LEGS_LEAN_LEFT1)
				{
					pm->ps->legsAnimTimer = 0; //Force it to stop the anim
				}

				if (ps->leanofs > 0)
				{
					ps->leanofs -= 4;
					if (ps->leanofs < 0)
					{
						ps->leanofs = 0;
					}
				}
				else if (ps->leanofs < 0)
				{
					ps->leanofs += 4;
					if (ps->leanofs > 0)
					{
						ps->leanofs = 0;
					}
				}
			}
		}
	}
	else //BUTTON_USE
	{
		if (ps->leanofs > 0)
		{
			ps->leanofs -= 4;
			if (ps->leanofs < 0)
			{
				ps->leanofs = 0;
			}
		}
		else if (ps->leanofs < 0)
		{
			ps->leanofs += 4;
			if (ps->leanofs > 0)
			{
				ps->leanofs = 0;
			}
		}
	}

	//standing block
	if (g_SerenityJediEngineMode->integer
		&& gent
		&& gent->client
		&& ~gent->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCKANDATTACK
		&& ~gent->client->ps.ManualBlockingFlags & 1 << MBF_BLOCKWALKING
		&& gent->s.weapon == WP_SABER
		&& pm->ps->SaberActive()
		&& !gent->client->ps.saberInFlight
		&& !PM_KickMove(pm->ps->saber_move)
		&& cmd->forwardmove >= 0
		&& !PM_WalkingOrRunningAnim(pm->ps->legsAnim)
		&& !PM_WalkingOrRunningAnim(pm->ps->torsoAnim)
		&& !(pm->ps->pm_flags & PMF_DUCKED)
		&& pm->ps->blockPoints > BLOCKPOINTS_FAIL
		&& pm->ps->forcePower > BLOCKPOINTS_FAIL
		&& pm->ps->forcePowersKnown & 1 << FP_SABER_DEFENSE)
	{
		if (cmd->buttons & BUTTON_BLOCK && !(cmd->buttons & BUTTON_USE) && !(cmd->buttons & BUTTON_WALKING))
		{
			//check leaning
			int anim = -1;

			if (G_OkayToDoStandingBlock(ps, cmd, qtrue)) //pushing a direction
			{
				//third person lean
				if (cmd->upmove <= 0)
				{
					if (cmd->rightmove > 0)
					{
						//lean right
						if (saberAnimLevel == SS_DUAL)
						{
							if (ps->torsoAnim == BOTH_BLOCK_HOLD_R_DUAL)
							{
								anim = ps->torsoAnim;
							}
							else
							{
								anim = BOTH_BLOCK_R_DUAL;
							}
						}
						else if (saberAnimLevel == SS_STAFF)
						{
							if (ps->torsoAnim == BOTH_BLOCK_HOLD_R_STAFF)
							{
								anim = ps->torsoAnim;
							}
							else
							{
								anim = BOTH_BLOCK_R_STAFF;
							}
						}
						else
						{
							if (ps->torsoAnim == BOTH_BLOCK_HOLD_R)
							{
								anim = ps->torsoAnim;
							}
							else
							{
								anim = BOTH_BLOCK_R;
							}
						}
					}
					else if (cmd->rightmove < 0)
					{
						//lean left
						if (saberAnimLevel == SS_DUAL)
						{
							if (ps->torsoAnim == BOTH_BLOCK_HOLD_L_DUAL)
							{
								anim = ps->torsoAnim;
							}
							else
							{
								anim = BOTH_BLOCK_L_DUAL;
							}
						}
						else if (saberAnimLevel == SS_STAFF)
						{
							if (ps->torsoAnim == BOTH_BLOCK_HOLD_L_STAFF)
							{
								anim = ps->torsoAnim;
							}
							else
							{
								anim = BOTH_BLOCK_L_STAFF;
							}
						}
						else
						{
							if (ps->torsoAnim == BOTH_BLOCK_HOLD_L)
							{
								anim = ps->torsoAnim;
							}
							else
							{
								anim = BOTH_BLOCK_L;
							}
						}
					}
				}
				if (anim != -1)
				{
					int extra_hold_time = 0;

					if (saberAnimLevel == SS_DUAL)
					{
						if (PM_BlockDualAnim(ps->torsoAnim) && !PM_BlockHoldDualAnim(ps->torsoAnim))
						{
							//already in a dodge
							//use the hold pose, don't start it all over again
							anim = BOTH_BLOCK_HOLD_L_DUAL + (anim - BOTH_BLOCK_L_DUAL);
							extra_hold_time = 100;
						}
					}
					else if (saberAnimLevel == SS_STAFF)
					{
						if (PM_BlockStaffAnim(ps->torsoAnim) && !PM_BlockHoldStaffAnim(ps->torsoAnim))
						{
							//already in a dodge
							//use the hold pose, don't start it all over again
							anim = BOTH_BLOCK_HOLD_L_STAFF + (anim - BOTH_BLOCK_L_STAFF);
							extra_hold_time = 100;
						}
					}
					else
					{
						if (PM_BlockAnim(ps->torsoAnim) && !PM_BlockHoldAnim(ps->torsoAnim))
						{
							//already in a dodge
							//use the hold pose, don't start it all over again
							anim = BOTH_BLOCK_HOLD_L + (anim - BOTH_BLOCK_L);
							extra_hold_time = 100;
						}
					}
					if (anim == pm->ps->torsoAnim)
					{
						if (pm->ps->torsoAnimTimer < 100)
						{
							pm->ps->torsoAnimTimer = 100;
						}
					}
					else
					{
						NPC_SetAnim(gent, SETANIM_TORSO, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					}
					if (extra_hold_time && ps->torsoAnimTimer < extra_hold_time)
					{
						ps->torsoAnimTimer += extra_hold_time;
					}
					if (ps->groundEntityNum != ENTITYNUM_NONE && !cmd->upmove)
					{
						NPC_SetAnim(gent, SETANIM_TORSO, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						ps->legsAnimTimer = ps->torsoAnimTimer;
					}
					else
					{
						NPC_SetAnim(gent, SETANIM_TORSO, anim, SETANIM_FLAG_NORMAL);
					}
					ps->weaponTime = ps->torsoAnimTimer;
					ps->leanStopDebounceTime = ceil(static_cast<float>(ps->torsoAnimTimer) / 50.0f); //20;
				}
			}
			else if (!cg.zoomMode && cmd->rightmove != 0 && !cmd->forwardmove && cmd->upmove <= 0)
			{
				//Only lean if holding use button, strafing and not moving forward or back and not jumping
				int leanofs = 0;
				vec3_t viewangles;

				if (cmd->rightmove > 0)
				{
					if (ps->leanofs <= 28)
					{
						leanofs = ps->leanofs + 4;
					}
					else
					{
						leanofs = 32;
					}
				}
				else
				{
					if (ps->leanofs >= -28)
					{
						leanofs = ps->leanofs - 4;
					}
					else
					{
						leanofs = -32;
					}
				}

				VectorCopy(ps->origin, start);
				start[2] += ps->viewheight;
				VectorCopy(ps->viewangles, viewangles);
				viewangles[ROLL] = 0;
				AngleVectors(ps->viewangles, nullptr, right, nullptr);
				VectorNormalize(right);
				right[2] = leanofs < 0 ? 0.25 : -0.25;
				VectorMA(start, leanofs, right, end);
				VectorSet(tmins, -8, -8, -4);
				VectorSet(tmaxs, 8, 8, 4);
				gi.trace(&trace, start, tmins, tmaxs, end, gent->s.number, MASK_PLAYERSOLID,
					static_cast<EG2_Collision>(0), 0);

				ps->leanofs = floor(static_cast<float>(leanofs) * trace.fraction);

				ps->leanStopDebounceTime = 20;
			}
			else
			{
				if (cmd->forwardmove || cmd->upmove > 0)
				{
					if (pm->ps->legsAnim == LEGS_LEAN_RIGHT1 || pm->ps->legsAnim == LEGS_LEAN_LEFT1)
					{
						pm->ps->legsAnimTimer = 0; //Force it to stop the anim
					}

					if (ps->leanofs > 0)
					{
						ps->leanofs -= 4;
						if (ps->leanofs < 0)
						{
							ps->leanofs = 0;
						}
					}
					else if (ps->leanofs < 0)
					{
						ps->leanofs += 4;
						if (ps->leanofs > 0)
						{
							ps->leanofs = 0;
						}
					}
				}
			}
		}
		else //BUTTON_USE
		{
			if (ps->leanofs > 0)
			{
				ps->leanofs -= 4;
				if (ps->leanofs < 0)
				{
					ps->leanofs = 0;
				}
			}
			else if (ps->leanofs < 0)
			{
				ps->leanofs += 4;
				if (ps->leanofs > 0)
				{
					ps->leanofs = 0;
				}
			}
		}
	}

	if (ps->leanStopDebounceTime)
	{
		ps->leanStopDebounceTime -= 1;
		cmd->rightmove = 0;
		cmd->buttons &= ~BUTTON_USE;
	}
}