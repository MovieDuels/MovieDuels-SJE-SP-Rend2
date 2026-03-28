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

//Client camera controls for cinematics

#include "cg_headers.h"

#include "cg_media.h"

#include "../game/g_roff.h"

bool in_camera = false;
camera_t client_camera = {};
extern qboolean player_locked;

extern gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match);
extern void G_UseTargets(gentity_t* ent, gentity_t* activator);
void CGCam_FollowDisable();
void CGCam_TrackDisable();
void CGCam_Distance(float distance, float init_lerp);
void CGCam_DistanceDisable();
extern qboolean CG_CalcFOVFromX(float fov_x);
extern void WP_SaberCatch(gentity_t* self, gentity_t* saber, qboolean switch_to_saber);
extern vmCvar_t cg_drawwidescreenmode;
extern void WP_ForcePowerStop(gentity_t* self, forcePowers_t force_power);

/*
-------------------------
CGCam_Init
-------------------------
*/

void CGCam_Init()
{
	extern qboolean qbVidRestartOccured;
	if (!qbVidRestartOccured)
	{
		memset(&client_camera, 0, sizeof(camera_t));
	}
}

/*
-------------------------
CGCam_Enable
-------------------------
*/
extern void CG_CalcVrect();
extern vmCvar_t cg_SerenityJediEngineMode;

void CGCam_Enable()
{
	client_camera.bar_alpha = 0.0f;
	client_camera.bar_time = cg.time;

	client_camera.bar_alpha_source = 0.0f;
	client_camera.bar_alpha_dest = 1.0f;

	client_camera.bar_height_source = 0.0f;
	client_camera.bar_height_dest = static_cast<float>(480) / 10;
	client_camera.bar_height = 0.0f;

	client_camera.info_state |= CAMERA_BAR_FADING;

	client_camera.FOV = CAMERA_DEFAULT_FOV;
	client_camera.FOV2 = CAMERA_DEFAULT_FOV;

	in_camera = true;

	client_camera.next_roff_time = 0;

	if (g_entities[0].inuse && g_entities[0].client)
	{
		//Player zero not allowed to do anything
		VectorClear(g_entities[0].client->ps.velocity);
		g_entities[0].contents = 0;

		if (cg.zoomMode)
		{
			// need to shut off some form of zooming
			cg.zoomMode = 0;
		}

		if (g_entities[0].client->ps.saberInFlight && g_entities[0].client->ps.saber[0].Active())
		{
			//saber is out
			gentity_t* saberent = &g_entities[g_entities[0].client->ps.saberEntityNum];
			if (saberent)
			{
				WP_SaberCatch(&g_entities[0], saberent, qfalse);
			}
		}
		if (g_entities[0].client->ps.saberFatigueChainCount >= MISHAPLEVEL_MIN)
		{
			g_entities[0].client->ps.saberFatigueChainCount = MISHAPLEVEL_MIN;
		}

		if (cg_SerenityJediEngineMode.integer == 2 && g_entities[0].client->ps.blockPoints <= BLOCK_POINTS_MAX)
		{
			g_entities[0].client->ps.blockPoints = BLOCK_POINTS_MAX;
		}

		if (cg_SerenityJediEngineMode.integer == 1 && g_entities[0].client->ps.forcePower <= BLOCK_POINTS_MAX)
		{
			g_entities[0].client->ps.forcePower = BLOCK_POINTS_MAX;
		}

		for (int i = 0; i < NUM_FORCE_POWERS; i++)
		{
			//deactivate any active force powers
			g_entities[0].client->ps.forcePowerDuration[i] = 0;
			if (g_entities[0].client->ps.forcePowerDuration[i] || g_entities[0].client->ps.forcePowersActive & 1 <<
				i)
			{
				WP_ForcePowerStop(&g_entities[0], static_cast<forcePowers_t>(i));
			}
		}
	}
}

/*
-------------------------
CGCam_Disable
-------------------------
*/

void CGCam_Disable()
{
	in_camera = false;

	client_camera.bar_alpha = 1.0f;
	client_camera.bar_time = cg.time;

	client_camera.bar_alpha_source = 1.0f;
	client_camera.bar_alpha_dest = 0.0f;

	client_camera.bar_height_source = static_cast<float>(480) / 10;
	client_camera.bar_height_dest = 0.0f;

	client_camera.info_state |= CAMERA_BAR_FADING;

	if (g_entities[0].client)
	{
		g_entities[0].contents = CONTENTS_BODY; //MASK_PLAYERSOLID;
	}

	gi.SendServerCommand(0, "cts");

	//if ( cg_skippingcin.integer )
	{
		//We're skipping the cinematic and it's over now
		gi.cvar_set("timescale", "1");
		gi.cvar_set("skippingCinematic", "0");
	}

	//we just came out of camera, so update cg.refdef.vieworg out of the camera's origin so the snapshot will know our new ori
	VectorCopy(g_entities[0].currentOrigin, cg.refdef.vieworg);
	VectorCopy(g_entities[0].client->ps.viewangles, cg.refdefViewAngles);
}

/*
-------------------------
CGCam_SetPosition
-------------------------
*/

void CGCam_SetPosition(vec3_t org)
{
	VectorCopy(org, client_camera.origin);
	VectorCopy(client_camera.origin, cg.refdef.vieworg);
}

/*
-------------------------
CGCam_Move
-------------------------
*/

void CGCam_Move(vec3_t dest, const float duration)
{
	if (client_camera.info_state & CAMERA_ROFFING)
	{
		client_camera.info_state &= ~CAMERA_ROFFING;
	}

	CGCam_TrackDisable();
	CGCam_DistanceDisable();

	if (!duration)
	{
		client_camera.info_state &= ~CAMERA_MOVING;
		CGCam_SetPosition(dest);
		return;
	}

	client_camera.info_state |= CAMERA_MOVING;

	VectorCopy(dest, client_camera.origin2);

	client_camera.move_duration = duration;
	client_camera.move_time = cg.time;
}

/*
-------------------------
CGCam_SetAngles
-------------------------
*/

void CGCam_SetAngles(vec3_t ang)
{
	VectorCopy(ang, client_camera.angles);
	VectorCopy(client_camera.angles, cg.refdefViewAngles);
}

/*
-------------------------
CGCam_Pan
-------------------------
*/

void CGCam_Pan(vec3_t dest, vec3_t pan_direction, const float duration)
{
	float delta2;

	CGCam_FollowDisable();
	CGCam_DistanceDisable();

	if (!duration)
	{
		CGCam_SetAngles(dest);
		client_camera.info_state &= ~CAMERA_PANNING;
		return;
	}

	//FIXME: make the dest an absolute value, and pass in a
	//panDirection as well.  If a panDirection's axis value is
	//zero, find the shortest difference for that axis.
	//Store the delta in client_camera.angles2.
	for (int i = 0; i < 3; i++)
	{
		dest[i] = AngleNormalize360(dest[i]);
		const float delta1 = dest[i] - AngleNormalize360(client_camera.angles[i]);
		if (delta1 < 0)
		{
			delta2 = delta1 + 360;
		}
		else
		{
			delta2 = delta1 - 360;
		}
		if (!pan_direction[i])
		{
			//Didn't specify a direction, pick shortest
			if (Q_fabs(delta1) < Q_fabs(delta2))
			{
				client_camera.angles2[i] = delta1;
			}
			else
			{
				client_camera.angles2[i] = delta2;
			}
		}
		else if (pan_direction[i] < 0)
		{
			if (delta1 < 0)
			{
				client_camera.angles2[i] = delta1;
			}
			else if (delta1 > 0)
			{
				client_camera.angles2[i] = delta2;
			}
			else
			{
				//exact
				client_camera.angles2[i] = 0;
			}
		}
		else if (pan_direction[i] > 0)
		{
			if (delta1 > 0)
			{
				client_camera.angles2[i] = delta1;
			}
			else if (delta1 < 0)
			{
				client_camera.angles2[i] = delta2;
			}
			else
			{
				//exact
				client_camera.angles2[i] = 0;
			}
		}
	}
	//VectorCopy( dest, client_camera.angles2 );

	client_camera.info_state |= CAMERA_PANNING;

	client_camera.pan_duration = duration;
	client_camera.pan_time = cg.time;
}

/*
-------------------------
CGCam_SetRoll
-------------------------
*/

void CGCam_SetRoll(const float roll)
{
	client_camera.angles[2] = roll;
}

/*
-------------------------
CGCam_Roll
-------------------------
*/

void CGCam_Roll(const float dest, const float duration)
{
	if (!duration)
	{
		CGCam_SetRoll(dest);
		return;
	}

	//FIXME/NOTE: this will override current panning!!!
	client_camera.info_state |= CAMERA_PANNING;

	VectorCopy(client_camera.angles, client_camera.angles2);
	client_camera.angles2[2] = AngleDelta(dest, client_camera.angles[2]);

	client_camera.pan_duration = duration;
	client_camera.pan_time = cg.time;
}

/*
-------------------------
CGCam_SetFOV
-------------------------
*/

void CGCam_SetFOV(const float FOV)
{
	client_camera.FOV = FOV;
}

/*
-------------------------
CGCam_Zoom
-------------------------
*/

void CGCam_Zoom(const float FOV, const float duration)
{
	if (!duration)
	{
		CGCam_SetFOV(FOV);
		return;
	}
	client_camera.info_state |= CAMERA_ZOOMING;

	client_camera.FOV_time = cg.time;
	client_camera.FOV2 = FOV;

	client_camera.FOV_duration = duration;
}

void CGCam_Zoom2(const float FOV, const float FOV2, const float duration)
{
	if (!duration)
	{
		CGCam_SetFOV(FOV2);
		return;
	}
	client_camera.info_state |= CAMERA_ZOOMING;

	client_camera.FOV_time = cg.time;
	client_camera.FOV = FOV;
	client_camera.FOV2 = FOV2;

	client_camera.FOV_duration = duration;
}

void CGCam_ZoomAccel(const float initial_fov, const float fov_velocity, const float fov_accel, const float duration)
{
	if (!duration)
	{
		return;
	}
	client_camera.info_state |= CAMERA_ACCEL;

	client_camera.FOV_time = cg.time;
	client_camera.FOV2 = initial_fov;
	client_camera.FOV_vel = fov_velocity;
	client_camera.FOV_acc = fov_accel;

	client_camera.FOV_duration = duration;
}

/*
-------------------------
CGCam_Fade
-------------------------
*/

void CGCam_SetFade(vec4_t dest)
{
	//Instant completion
	client_camera.info_state &= ~CAMERA_FADING;
	client_camera.fade_duration = 0;
	VectorCopy4(dest, client_camera.fade_source);
	VectorCopy4(dest, client_camera.fade_color);
}

/*
-------------------------
CGCam_Fade
-------------------------
*/

void CGCam_Fade(vec4_t source, vec4_t dest, const float duration)
{
	if (!duration)
	{
		CGCam_SetFade(dest);
		return;
	}

	VectorCopy4(source, client_camera.fade_source);
	VectorCopy4(dest, client_camera.fade_dest);

	client_camera.fade_duration = duration;
	client_camera.fade_time = cg.time;

	client_camera.info_state |= CAMERA_FADING;
}

void CGCam_FollowDisable()
{
	client_camera.info_state &= ~CAMERA_FOLLOWING;
	client_camera.cameraGroup[0] = 0;
	client_camera.cameraGroupZOfs = 0;
	client_camera.cameraGroupTag[0] = 0;
}

void CGCam_TrackDisable()
{
	client_camera.info_state &= ~CAMERA_TRACKING;
	client_camera.trackEntNum = ENTITYNUM_WORLD;
}

void CGCam_DistanceDisable()
{
	client_camera.distance = 0;
}

/*
-------------------------
CGCam_Follow
-------------------------
*/

void CGCam_Follow(const char* camera_group, const float speed, const float init_lerp)
{
	//Clear any previous
	CGCam_FollowDisable();

	if (!camera_group || !camera_group[0])
	{
		return;
	}

	if (Q_stricmp("none", camera_group) == 0)
	{
		//Turn off all aiming
		return;
	}

	if (Q_stricmp("NULL", camera_group) == 0)
	{
		//Turn off all aiming
		return;
	}

	//NOTE: if this interrupts a pan before it's done, need to copy the cg.refdef.viewAngles to the camera.angles!
	client_camera.info_state |= CAMERA_FOLLOWING;
	client_camera.info_state &= ~CAMERA_PANNING;

	//NULL terminate last char in case they type a name too long
	Q_strncpyz(client_camera.cameraGroup, camera_group, sizeof client_camera.cameraGroup);

	if (speed)
	{
		client_camera.followSpeed = speed;
	}
	else
	{
		client_camera.followSpeed = 100.0f;
	}

	if (init_lerp)
	{
		client_camera.followInitLerp = qtrue;
	}
	else
	{
		client_camera.followInitLerp = qfalse;
	}
}

/*
-------------------------
CGCam_Track
-------------------------
*/
void CGCam_Track(const char* track_name, const float speed, const float init_lerp)
{
	CGCam_TrackDisable();

	if (Q_stricmp("none", track_name) == 0)
	{
		//turn off tracking
		return;
	}

	//NOTE: if this interrupts a move before it's done, need to copy the cg.refdef.vieworg to the camera.origin!
	//This will find a path_corner now, not a misc_camera_track
	const gentity_t* trackEnt = G_Find(nullptr, FOFS(targetname), track_name);

	if (!trackEnt)
	{
		gi.Printf(S_COLOR_RED"ERROR: %s camera track target not found\n", track_name);
		return;
	}

	client_camera.info_state |= CAMERA_TRACKING;
	client_camera.info_state &= ~CAMERA_MOVING;

	client_camera.trackEntNum = trackEnt->s.number;
	client_camera.initSpeed = speed / 10.0f;
	client_camera.speed = speed;
	client_camera.nextTrackEntUpdateTime = cg.time;

	if (init_lerp)
	{
		client_camera.trackInitLerp = qtrue;
	}
	else
	{
		client_camera.trackInitLerp = qfalse;
	}
	/*
	if ( client_camera.info_state & CAMERA_FOLLOWING )
	{//Used to snap angles?  Do what...?
	}
	*/

	//Set a moveDir
	VectorSubtract(trackEnt->currentOrigin, client_camera.origin, client_camera.moveDir);

	if (!client_camera.trackInitLerp)
	{
		//want to snap to first position
		//Snap to trackEnt's origin
		VectorCopy(trackEnt->currentOrigin, client_camera.origin);

		//Set new moveDir if trackEnt has a next path_corner
		//Possible that track has no next point, in which case we won't be moving anyway
		if (trackEnt->target && trackEnt->target[0])
		{
			const gentity_t* newTrackEnt = G_Find(nullptr, FOFS(targetname), trackEnt->target);
			if (newTrackEnt)
			{
				VectorSubtract(newTrackEnt->currentOrigin, client_camera.origin, client_camera.moveDir);
			}
		}
	}

	VectorNormalize(client_camera.moveDir);
}

/*
-------------------------
CGCam_Distance
-------------------------
*/

void CGCam_Distance(const float distance, const float init_lerp)
{
	client_camera.distance = distance;

	if (init_lerp)
	{
		client_camera.distanceInitLerp = qtrue;
	}
	else
	{
		client_camera.distanceInitLerp = qfalse;
	}
}

//========================================================================================

void CGCam_FollowUpdate()
{
	vec3_t center, dir, camera_angles, vec; //No more than 16 subjects in a cameraGroup
	int i;

	if (client_camera.cameraGroup[0])
	{
		int num_subjects = 0;
		gentity_t* from = nullptr;
		vec3_t focus[MAX_CAMERA_GROUP_SUBJECTS]{};
		//Stay centered in my cameraGroup, if I have one
		while (nullptr != (from = G_Find(from, FOFS(cameraGroup), client_camera.cameraGroup)))
		{
			if (num_subjects >= MAX_CAMERA_GROUP_SUBJECTS)
			{
				gi.Printf(S_COLOR_RED"ERROR: Too many subjects in shot composition %s", client_camera.cameraGroup);
				break;
			}

			const centity_t* from_cent = &cg_entities[from->s.number];
			if (!from_cent)
			{
				continue;
			}

			qboolean focused = qfalse;
			if (from->client && client_camera.cameraGroupTag[0] && from_cent->gent->ghoul2.size())
			{
				const int new_bolt = gi.G2API_AddBolt(&from_cent->gent->ghoul2[from->playerModel],
					client_camera.cameraGroupTag);
				if (new_bolt != -1)
				{
					mdxaBone_t bolt_matrix;
					const vec3_t from_angles = { 0, from->client->ps.legsYaw, 0 };

					gi.G2API_GetBoltMatrix(from_cent->gent->ghoul2, from->playerModel, new_bolt, &bolt_matrix, from_angles,
						from_cent->lerpOrigin, cg.time, cgs.model_draw,
						from_cent->currentState.modelScale);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, focus[num_subjects]);

					focused = qtrue;
				}
			}
			if (!focused)
			{
				if (from->s.pos.trType != TR_STATIONARY)
					//				if ( from->s.pos.trType == TR_INTERPOLATE )
				{
					//use interpolated origin?
					if (!VectorCompare(vec3_origin, from_cent->lerpOrigin))
					{
						//hunh?  Somehow we've never seen this gentity on the client, so there is no lerpOrigin, so cheat over to the game and use the currentOrigin
						VectorCopy(from->currentOrigin, focus[num_subjects]);
					}
					else
					{
						VectorCopy(from_cent->lerpOrigin, focus[num_subjects]);
					}
				}
				else
				{
					VectorCopy(from->currentOrigin, focus[num_subjects]);
				}
				//FIXME: make a list here of their s.numbers instead so we can do other stuff with the list below
				if (from->client)
				{
					//Track to their eyes - FIXME: maybe go off a tag?
					//FIXME:
					//Based on FOV and distance to subject from camera, pick the point that
					//keeps eyes 3/4 up from bottom of screen... what about bars?
					focus[num_subjects][2] += from->client->ps.viewheight;
				}
			}
			if (client_camera.cameraGroupZOfs)
			{
				focus[num_subjects][2] += client_camera.cameraGroupZOfs;
			}
			num_subjects++;
		}

		if (!num_subjects) // Bad cameragroup
		{
#ifndef FINAL_BUILD
			gi.Printf(S_COLOR_RED"ERROR: Camera Focus unable to locate cameragroup: %s\n", client_camera.cameraGroup);
#endif
			return;
		}

		//Now average all points
		VectorCopy(focus[0], center);
		for (i = 1; i < num_subjects; i++)
		{
			VectorAdd(focus[i], center, center);
		}
		VectorScale(center, 1.0f / static_cast<float>(num_subjects), center);
	}
	else
	{
		return;
	}

	//Need to set a speed to keep a distance from
	//the subject- fixme: only do this if have a distance
	//set
	VectorSubtract(client_camera.subjectPos, center, vec);
	client_camera.subjectSpeed = VectorLengthSquared(vec) * 100.0f / cg.frametime;

	/*
	if ( !cg_skippingcin.integer )
	{
		Com_Printf( S_COLOR_RED"org: %s\n", vtos(center) );
	}
	*/
	VectorCopy(center, client_camera.subjectPos);

	VectorSubtract(center, cg.refdef.vieworg, dir);
	//can't use client_camera.origin because it's not updated until the end of the move.

	//Get desired angle
	vectoangles(dir, camera_angles);

	if (client_camera.followInitLerp)
	{
		//Lerping
		const float frac = cg.frametime / 100.0f * client_camera.followSpeed / 100.f;
		for (i = 0; i < 3; i++)
		{
			camera_angles[i] = AngleNormalize180(camera_angles[i]);
			camera_angles[i] = AngleNormalize180(
				client_camera.angles[i] + frac * AngleNormalize180(camera_angles[i] - client_camera.angles[i]));
			camera_angles[i] = AngleNormalize180(camera_angles[i]);
		}
#if 0
		Com_Printf("%s\n", vtos(cameraAngles));
#endif
	}
	else
	{
		//Snapping, should do this first time if follow_lerp_to_start_duration is zero
		//will lerp from this point on
		client_camera.followInitLerp = qtrue;
		for (i = 0; i < 3; i++)
		{
			//normalize so that when we start lerping, it doesn't freak out
			camera_angles[i] = AngleNormalize180(camera_angles[i]);
		}
		//So tracker doesn't move right away thinking the first angle change
		//is the subject moving... FIXME: shouldn't set this until lerp done OR snapped?
		client_camera.subjectSpeed = 0;
	}
	VectorCopy(camera_angles, client_camera.angles);
}

void CGCam_TrackEntUpdate()
{
	//FIXME: only do every 100 ms
	gentity_t* track_ent = nullptr;
	const gentity_t* new_track_ent = nullptr;
	qboolean reached = qfalse;

	if (client_camera.trackEntNum >= 0 && client_camera.trackEntNum < ENTITYNUM_WORLD)
	{
		vec3_t vec;
		//We're already heading to a path_corner
		track_ent = &g_entities[client_camera.trackEntNum];
		VectorSubtract(track_ent->currentOrigin, client_camera.origin, vec);
		const float dist = VectorLengthSquared(vec);
		if (dist < 256) //16 squared
		{
			//FIXME: who should be doing the using here?
			G_UseTargets(track_ent, track_ent);
			reached = qtrue;
		}
	}

	if (track_ent && reached)
	{
		if (track_ent->target && track_ent->target[0])
		{
			//Find our next path_corner
			new_track_ent = G_Find(nullptr, FOFS(targetname), track_ent->target);
			if (new_track_ent)
			{
				if (new_track_ent->radius < 0)
				{
					//Don't bother trying to maintain a radius
					client_camera.distance = 0;
					client_camera.speed = client_camera.initSpeed;
				}
				else if (new_track_ent->radius > 0)
				{
					client_camera.distance = new_track_ent->radius;
				}

				if (new_track_ent->speed < 0)
				{
					//go back to our default speed
					client_camera.speed = client_camera.initSpeed;
				}
				else if (new_track_ent->speed > 0)
				{
					client_camera.speed = new_track_ent->speed / 10.0f;
				}
			}
		}
		else
		{
			//stop thinking if this is the last one
			CGCam_TrackDisable();
		}
	}

	if (new_track_ent)
	{
		//Update will lerp this
		client_camera.info_state |= CAMERA_TRACKING;
		client_camera.trackEntNum = new_track_ent->s.number;
		VectorCopy(new_track_ent->currentOrigin, client_camera.trackToOrg);
	}

	client_camera.nextTrackEntUpdateTime = cg.time + 100;
}

void CGCam_TrackUpdate()
{
	vec3_t goal_vec, cur_vec, track_pos;

	if (client_camera.nextTrackEntUpdateTime <= cg.time)
	{
		CGCam_TrackEntUpdate();
	}

	VectorSubtract(client_camera.trackToOrg, client_camera.origin, goal_vec);

	if (client_camera.distance && client_camera.info_state & CAMERA_FOLLOWING)
	{
		vec3_t vec;

		if (!client_camera.distanceInitLerp)
		{
			VectorSubtract(client_camera.origin, client_camera.subjectPos, vec);
			VectorNormalize(vec);
			//FIXME: use client_camera.moveDir here?
			VectorMA(client_camera.subjectPos, client_camera.distance, vec, client_camera.origin);
			//Snap to first time only
			client_camera.distanceInitLerp = qtrue;
			return;
		}
		if (client_camera.subjectSpeed > 0.05f)
		{
			float adjust = 0.0f;
			//Don't start moving until subject moves
			VectorSubtract(client_camera.subjectPos, client_camera.origin, vec);
			const float dist = VectorNormalize(vec);
			const float dot = DotProduct(goal_vec, vec);

			if (dist > client_camera.distance)
			{
				//too far away
				if (dot > 0)
				{
					//Camera is moving toward the subject
					adjust = dist - client_camera.distance; //Speed up
				}
				else if (dot < 0)
				{
					//Camera is moving away from the subject
					adjust = (dist - client_camera.distance) * -1.0f; //Slow down
				}
			}
			else if (dist < client_camera.distance)
			{
				//too close
				if (dot > 0)
				{
					//Camera is moving toward the subject
					adjust = (client_camera.distance - dist) * -1.0f; //Slow down
				}
				else if (dot < 0)
				{
					//Camera is moving away from the subject
					adjust = client_camera.distance - dist; //Speed up
				}
			}

			//Speed of the focus + our error
			//desiredSpeed = aimCent->gent->speed + (adjust * cg.frametime/100.0f);//cg.frameInterpolation);
			const float desired_speed = adjust; // * cg.frametime/100.0f);//cg.frameInterpolation);

			//self->moveInfo.speed = desiredSpeed;

			//Don't change speeds faster than 10 every 10th of a second
			const float max_allowed_accel = MAX_ACCEL_PER_FRAME * (cg.frametime / 100.0f);

			if (!client_camera.subjectSpeed)
			{
				//full stop
				client_camera.speed = desired_speed;
			}
			else if (client_camera.speed - desired_speed > max_allowed_accel)
			{
				//new speed much slower, slow down at max accel
				client_camera.speed -= max_allowed_accel;
			}
			else if (desired_speed - client_camera.speed > max_allowed_accel)
			{
				//new speed much faster, speed up at max accel
				client_camera.speed += max_allowed_accel;
			}
			else
			{
				//remember this speed
				client_camera.speed = desired_speed;
			}

			//Com_Printf("Speed: %4.2f (%4.2f)\n", self->moveInfo.speed, aimCent->gent->speed);
		}
	}
	else
	{
		//slowDown = qtrue;
	}

	//FIXME: this probably isn't right, round it out more
	VectorScale(goal_vec, cg.frametime / 100.0f, goal_vec);
	VectorScale(client_camera.moveDir, (100.0f - cg.frametime) / 100.0f, cur_vec);
	VectorAdd(goal_vec, cur_vec, client_camera.moveDir);
	VectorNormalize(client_camera.moveDir);
	/*if(slowDown)
	{
		VectorMA( client_camera.origin, client_camera.speed * goalDist/100.0f * cg.frametime/100.0f, client_camera.moveDir, trackPos );
	}
	else*/
	{
		VectorMA(client_camera.origin, client_camera.speed * cg.frametime / 100.0f, client_camera.moveDir, track_pos);
	}

	//FIXME: Implement
	//Need to find point on camera's path that is closest to the desired distance from subject
	//OR: Need to intelligently pick this desired distance based on framing...
	VectorCopy(track_pos, client_camera.origin);
}

//=========================================================================================

/*
-------------------------
CGCam_UpdateBarFade
-------------------------
*/

void CGCam_UpdateBarFade()
{
	if (client_camera.bar_time + BAR_DURATION < cg.time)
	{
		client_camera.bar_alpha = client_camera.bar_alpha_dest;
		client_camera.info_state &= ~CAMERA_BAR_FADING;
		client_camera.bar_height = client_camera.bar_height_dest;
	}
	else
	{
		client_camera.bar_alpha = client_camera.bar_alpha_source + (client_camera.bar_alpha_dest - client_camera.
			bar_alpha_source) / BAR_DURATION * (cg.time - client_camera.bar_time);
		client_camera.bar_height = client_camera.bar_height_source + (client_camera.bar_height_dest - client_camera.
			bar_height_source) / BAR_DURATION * (cg.time - client_camera.bar_time);
	}
}

/*
-------------------------
CGCam_UpdateFade
-------------------------
*/

void CGCam_UpdateFade()
{
	if (client_camera.info_state & CAMERA_FADING)
	{
		if (client_camera.fade_time + client_camera.fade_duration < cg.time)
		{
			VectorCopy4(client_camera.fade_dest, client_camera.fade_color);
			client_camera.info_state &= ~CAMERA_FADING;
		}
		else
		{
			for (int i = 0; i < 4; i++)
			{
				client_camera.fade_color[i] = client_camera.fade_source[i] + (client_camera.fade_dest[i] -
					client_camera.fade_source[i]) / client_camera.fade_duration * (cg.time - client_camera.fade_time);
			}
		}
	}
}

/*
-------------------------
CGCam_Update
-------------------------
*/
static void CGCam_Roff();

void CGCam_Update()
{
	int i;
	qboolean check_follow = qfalse;
	qboolean check_track = qfalse;

	// Apply new roff data to the camera as needed
	if (client_camera.info_state & CAMERA_ROFFING)
	{
		CGCam_Roff();
	}

	//Check for a zoom
	if (client_camera.info_state & CAMERA_ACCEL)
	{
		// x = x0 + vt + 0.5*a*t*t
		float actual_fov_x = client_camera.FOV;
		const float t = (cg.time - client_camera.FOV_time) * 0.001; // mult by 0.001 cuz otherwise t is too darned big
		const float fov_duration = client_camera.FOV_duration;

#ifndef FINAL_BUILD
		if (cg_roffval4.integer)
		{
			fov_duration = cg_roffval4.integer;
		}
#endif
		if (client_camera.FOV_time + fov_duration < cg.time)
		{
			client_camera.info_state &= ~CAMERA_ACCEL;
		}
		else
		{
			constexpr float sanity_max = 180;
			constexpr float sanity_min = 1;
			const float initial_pos_val = client_camera.FOV2;
			const float vel_val = client_camera.FOV_vel;
			const float acc_val = client_camera.FOV_acc;

#ifndef FINAL_BUILD
			if (cg_roffdebug.integer)
			{
				if (fabs(cg_roffval1.value) > 0.001f)
				{
					initial_pos_val = cg_roffval1.value;
				}
				if (fabs(cg_roffval2.value) > 0.001f)
				{
					vel_val = cg_roffval2.value;
				}
				if (fabs(cg_roffval3.value) > 0.001f)
				{
					acc_val = cg_roffval3.value;
				}
			}
#endif
			const float initial_pos = initial_pos_val;
			const float vel = vel_val * t;
			const float acc = 0.5 * acc_val * t * t;

			actual_fov_x = initial_pos + vel + acc;
			if (cg_roffdebug.integer)
			{
				Com_Printf("%d: fovaccel from %2.1f using vel = %2.4f, acc = %2.4f (current fov calc = %5.6f)\n",
					cg.time, initial_pos_val, vel_val, acc_val, actual_fov_x);
			}

			if (actual_fov_x < sanity_min)
			{
				actual_fov_x = sanity_min;
			}
			else if (actual_fov_x > sanity_max)
			{
				actual_fov_x = sanity_max;
			}
			client_camera.FOV = actual_fov_x;
		}
		CG_CalcFOVFromX(actual_fov_x);
	}
	else if (client_camera.info_state & CAMERA_ZOOMING)
	{
		float actual_fov_x;

		if (client_camera.FOV_time + client_camera.FOV_duration < cg.time)
		{
			actual_fov_x = client_camera.FOV = client_camera.FOV2;
			client_camera.info_state &= ~CAMERA_ZOOMING;
		}
		else
		{
			actual_fov_x = client_camera.FOV + (client_camera.FOV2 - client_camera.FOV) / client_camera.FOV_duration
				* (cg.time - client_camera.FOV_time);
		}
		CG_CalcFOVFromX(actual_fov_x);
	}
	else
	{
		CG_CalcFOVFromX(client_camera.FOV);
	}

	//Check for roffing angles
	if (client_camera.info_state & CAMERA_ROFFING && !(client_camera.info_state & CAMERA_FOLLOWING))
	{
		if (client_camera.info_state & CAMERA_CUT)
		{
			// we're doing a cut, so just go to the new angles. none of this hifalutin lerping business.
			for (i = 0; i < 3; i++)
			{
				cg.refdefViewAngles[i] = AngleNormalize360(client_camera.angles[i] + client_camera.angles2[i]);
			}
		}
		else
		{
			for (i = 0; i < 3; i++)
			{
				cg.refdefViewAngles[i] = client_camera.angles[i] + client_camera.angles2[i] / client_camera.
					pan_duration * (cg.time - client_camera.pan_time);
			}
		}
	}
	else if (client_camera.info_state & CAMERA_PANNING)
	{
		if (client_camera.info_state & CAMERA_CUT)
		{
			// we're doing a cut, so just go to the new angles. none of this hifalutin lerping business.
			for (i = 0; i < 3; i++)
			{
				cg.refdefViewAngles[i] = AngleNormalize360(client_camera.angles[i] + client_camera.angles2[i]);
			}
		}
		else
		{
			//Note: does not actually change the camera's angles until the pan time is done!
			if (client_camera.pan_time + client_camera.pan_duration < cg.time)
			{
				//finished panning
				for (i = 0; i < 3; i++)
				{
					client_camera.angles[i] = AngleNormalize360(client_camera.angles[i] + client_camera.angles2[i]);
				}

				client_camera.info_state &= ~CAMERA_PANNING;
				VectorCopy(client_camera.angles, cg.refdefViewAngles);
			}
			else
			{
				//still panning
				for (i = 0; i < 3; i++)
				{
					//NOTE: does not store the resultant angle in client_camera.angles until pan is done
					cg.refdefViewAngles[i] = client_camera.angles[i] + client_camera.angles2[i] / client_camera.
						pan_duration * (cg.time - client_camera.pan_time);
				}
			}
		}
	}
	else
	{
		check_follow = qtrue;
	}

	//Check for movement
	if (client_camera.info_state & CAMERA_MOVING)
	{
		//NOTE: does not actually move the camera until the movement time is done!
		if (client_camera.move_time + client_camera.move_duration < cg.time)
		{
			VectorCopy(client_camera.origin2, client_camera.origin);
			client_camera.info_state &= ~CAMERA_MOVING;
			VectorCopy(client_camera.origin, cg.refdef.vieworg);
		}
		else
		{
			if (client_camera.info_state & CAMERA_CUT)
			{
				// we're doing a cut, so just go to the new origin. none of this fancypants lerping stuff.
				for (i = 0; i < 3; i++)
				{
					cg.refdef.vieworg[i] = client_camera.origin2[i];
				}
			}
			else
			{
				for (i = 0; i < 3; i++)
				{
					cg.refdef.vieworg[i] = client_camera.origin[i] + (client_camera.origin2[i] - client_camera.origin[
						i]) / client_camera.move_duration * (cg.time - client_camera.move_time);
				}
			}
		}
	}
	else
	{
		check_track = qtrue;
	}

	if (check_follow)
	{
		if (client_camera.info_state & CAMERA_FOLLOWING)
		{
			//This needs to be done after camera movement
			CGCam_FollowUpdate();
		}
		VectorCopy(client_camera.angles, cg.refdefViewAngles);
	}

	if (check_track)
	{
		if (client_camera.info_state & CAMERA_TRACKING)
		{
			//This has to run AFTER Follow if the camera is following a cameraGroup
			CGCam_TrackUpdate();
		}

		VectorCopy(client_camera.origin, cg.refdef.vieworg);
	}

	//Bar fading
	if (client_camera.info_state & CAMERA_BAR_FADING)
	{
		CGCam_UpdateBarFade();
	}

	//Normal fading - separate call because can finish after camera is disabled
	CGCam_UpdateFade();

	//Update shaking if there's any
	CGCam_UpdateShake(cg.refdef.vieworg, cg.refdefViewAngles);
	AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);
}

/*
-------------------------
CGCam_DrawWideScreen
-------------------------
*/

void CGCam_DrawWideScreen()
{
	//Only draw if visible
	if (client_camera.bar_alpha)
	{
		CGCam_UpdateBarFade();

		if (cg_drawwidescreenmode.integer)
		{
			//
		}
		else
		{
			vec4_t modulate{};
			modulate[0] = modulate[1] = modulate[2] = 0.0f;
			modulate[3] = client_camera.bar_alpha;

			CG_FillRect(cg.refdef.x, cg.refdef.y, 640, client_camera.bar_height, modulate);
			CG_FillRect(cg.refdef.x, cg.refdef.y + 480 - client_camera.bar_height, 640, client_camera.bar_height,
				modulate);
		}
	}

	//NOTENOTE: Camera always draws the fades unless the alpha is 0
	if (client_camera.fade_color[3] == 0.0f)
		return;

	CG_FillRect(cg.refdef.x, cg.refdef.y, 640, 480, client_camera.fade_color);
}

/*
-------------------------
CGCam_RenderScene
-------------------------
*/
void CGCam_RenderScene()
{
	CGCam_Update();
	CG_CalcVrect();
}

/*
-------------------------
CGCam_Shake
-------------------------
*/

void CGCam_Shake(float intensity, const int duration)
{
	if (intensity > MAX_SHAKE_INTENSITY)
		intensity = MAX_SHAKE_INTENSITY;

	client_camera.shake_intensity = intensity;
	client_camera.shake_duration = duration;
	client_camera.shake_start = cg.time;
}

void CGCam_BlockShakeSP(float intensity, const int duration)
{
	if (intensity > MAX_BLOCKSHAKE_INTENSITY)
	{
		intensity = MAX_BLOCKSHAKE_INTENSITY;
	}

	client_camera.shake_intensity = intensity;
	client_camera.shake_duration = duration;
	client_camera.shake_start = cg.time;
}

/*
-------------------------
CGCam_UpdateShake

This doesn't actually affect the camera's info, but passed information instead
-------------------------
*/

void CGCam_UpdateShake(vec3_t origin, vec3_t angles)
{
	vec3_t move_dir{};

	if (client_camera.shake_duration <= 0)
		return;

	if (cg.time > client_camera.shake_start + client_camera.shake_duration)
	{
		client_camera.shake_intensity = 0;
		client_camera.shake_duration = 0;
		client_camera.shake_start = 0;
		return;
	}

	//intensity_scale now also takes into account FOV with 90.0 as normal
	const float intensity_scale = 1.0f - static_cast<float>(cg.time - client_camera.shake_start) / static_cast<float>(
		client_camera.shake_duration)
		* ((client_camera.FOV + client_camera.FOV2) / 2.0f / 90.0f);

	const float intensity = client_camera.shake_intensity * intensity_scale;

	for (float& i : move_dir)
	{
		i = Q_flrand(-1.0f, 1.0f) * intensity;
	}

	//FIXME: Lerp

	//Move the camera
	VectorAdd(origin, move_dir, origin);

	for (int i = 0; i < 2; i++) // Don't do ROLL
		move_dir[i] = Q_flrand(-1.0f, 1.0f) * intensity;

	//FIXME: Lerp

	//Move the angles
	VectorAdd(angles, move_dir, angles);
}

void CGCam_Smooth(const float intensity, const int duration)
{
	client_camera.smooth_active = false; // means smooth_origin and angles are valid
	if (intensity > 1.0f || intensity == 0.0f || duration < 1)
	{
		client_camera.info_state &= ~CAMERA_SMOOTHING;
		return;
	}
	client_camera.info_state |= CAMERA_SMOOTHING;
	client_camera.smooth_intensity = intensity;
	client_camera.smooth_duration = duration;
	client_camera.smooth_start = cg.time;
}

void CGCam_UpdateSmooth(vec3_t origin)
{
	if (!(client_camera.info_state & CAMERA_SMOOTHING) || cg.time > client_camera.smooth_start + client_camera.
		smooth_duration)
	{
		client_camera.info_state &= ~CAMERA_SMOOTHING;
		return;
	}
	if (!client_camera.smooth_active)
	{
		client_camera.smooth_active = true;
		VectorCopy(origin, client_camera.smooth_origin);
		return;
	}
	float factor = client_camera.smooth_intensity;
	if (client_camera.smooth_duration > 200 && cg.time > client_camera.smooth_start + client_camera.smooth_duration -
		100)
	{
		factor += (1.0f - client_camera.smooth_intensity) *
			(100.0f - (client_camera.smooth_start + client_camera.smooth_duration - cg.time)) / 100.0f;
	}
	for (int i = 0; i < 3; i++)
	{
		client_camera.smooth_origin[i] *= 1.0f - factor;
		client_camera.smooth_origin[i] += factor * origin[i];
		origin[i] = client_camera.smooth_origin[i];
	}
}

void CGCam_NotetrackProcessFov(const char* addl_arg)
{
	int a = 0;

	if (!addl_arg || !addl_arg[0])
	{
		Com_Printf("camera roff 'fov' notetrack missing fov argument\n", addl_arg);
		return;
	}
	if (isdigit(addl_arg[a]))
	{
		char t[64];
		// "fov <new fov>"
		int d = 0;
		constexpr int tsize = 64;

		memset(t, 0, tsize * sizeof(char));
		while (addl_arg[a] && d < tsize)
		{
			t[d++] = addl_arg[a++];
		}
		// now the contents of t represent our desired fov
		const float new_fov = atof(t);
#ifndef FINAL_BUILD
		if (cg_roffdebug.integer)
		{
			if (fabs(cg_roffval1.value) > 0.001f)
			{
				new_fov = cg_roffval1.value;
			}
		}
#endif
		if (cg_roffdebug.integer)
		{
			Com_Printf("notetrack: 'fov %2.2f' on frame %d\n", new_fov, client_camera.roff_frame);
		}
		CGCam_Zoom(new_fov, 0);
	}
}

void CGCam_NotetrackProcessFovZoom(const char* addl_arg)
{
	int a = 0;
	float begin_fov;

	if (!addl_arg || !addl_arg[0])
	{
		Com_Printf("camera roff 'fovzoom' notetrack missing arguments\n", addl_arg);
		return;
	}
	//
	// "fovzoom <begin fov> <end fov> <time>"
	//
	char t[64];
	int d = 0;
	constexpr int tsize = 64;

	memset(t, 0, tsize * sizeof(char));
	while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
	{
		t[d++] = addl_arg[a++];
	}
	if (!isdigit(t[0]))
	{
		// assume a non-number here means we should start from our current fov
		begin_fov = client_camera.FOV;
	}
	else
	{
		// now the contents of t represent our beginning fov
		begin_fov = atof(t);
	}

	// eat leading whitespace
	while (addl_arg[a] && addl_arg[a] == ' ')
	{
		a++;
	}
	if (addl_arg[a])
	{
		float fov_time;
		d = 0;
		memset(t, 0, tsize * sizeof(char));
		while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
		{
			t[d++] = addl_arg[a++];
		}
		// now the contents of t represent our end fov
		const float end_fov = atof(t);

		// eat leading whitespace
		while (addl_arg[a] && addl_arg[a] == ' ')
		{
			a++;
		}
		if (addl_arg[a])
		{
			d = 0;
			memset(t, 0, tsize * sizeof(char));
			while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
			{
				t[d++] = addl_arg[a++];
			}
			// now the contents of t represent our time
			fov_time = atof(t);
		}
		else
		{
			Com_Printf("camera roff 'fovzoom' notetrack missing 'time' argument\n", addl_arg);
			return;
		}
#ifndef FINAL_BUILD
		if (cg_roffdebug.integer)
		{
			if (fabs(cg_roffval1.value) > 0.001f)
			{
				begin_fov = cg_roffval1.value;
			}
			if (fabs(cg_roffval2.value) > 0.001f)
			{
				end_fov = cg_roffval2.value;
			}
			if (fabs(cg_roffval3.value) > 0.001f)
			{
				fov_time = cg_roffval3.value;
			}
		}
#endif
		if (cg_roffdebug.integer)
		{
			Com_Printf("notetrack: 'fovzoom %2.2f %2.2f %5.1f' on frame %d\n", begin_fov, end_fov, fov_time,
				client_camera.roff_frame);
		}
		CGCam_Zoom2(begin_fov, end_fov, fov_time);
	}
	else
	{
		Com_Printf("camera roff 'fovzoom' notetrack missing 'end fov' argument\n", addl_arg);
	}
}

void CGCam_NotetrackProcessFovAccel(const char* addl_arg)
{
	int a = 0;
	float begin_fov;

	if (!addl_arg || !addl_arg[0])
	{
		Com_Printf("camera roff 'fovaccel' notetrack missing arguments\n", addl_arg);
		return;
	}
	//
	// "fovaccel <begin fov> <fov delta> <fov delta2> <time>"
	//
	// where 'begin fov' is initial position, 'fov delta' is velocity, and 'fov delta2' is acceleration.
	char t[64];
	int d = 0;
	constexpr int tsize = 64;

	memset(t, 0, tsize * sizeof(char));
	while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
	{
		t[d++] = addl_arg[a++];
	}
	if (!isdigit(t[0]))
	{
		// assume a non-number here means we should start from our current fov
		begin_fov = client_camera.FOV;
	}
	else
	{
		// now the contents of t represent our beginning fov
		begin_fov = atof(t);
	}

	// eat leading whitespace
	while (addl_arg[a] && addl_arg[a] == ' ')
	{
		a++;
	}
	if (addl_arg[a])
	{
		d = 0;
		memset(t, 0, tsize * sizeof(char));
		while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
		{
			t[d++] = addl_arg[a++];
		}
		// now the contents of t represent our delta
		const float fov_delta = atof(t);

		// eat leading whitespace
		while (addl_arg[a] && addl_arg[a] == ' ')
		{
			a++;
		}
		if (addl_arg[a])
		{
			float fov_time;
			d = 0;
			memset(t, 0, tsize * sizeof(char));
			while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
			{
				t[d++] = addl_arg[a++];
			}
			// now the contents of t represent our fovDelta2
			const float fovDelta2 = atof(t);

			// eat leading whitespace
			while (addl_arg[a] && addl_arg[a] == ' ')
			{
				a++;
			}
			if (addl_arg[a])
			{
				d = 0;
				memset(t, 0, tsize * sizeof(char));
				while (addl_arg[a] && !isspace(addl_arg[a]) && d < tsize)
				{
					t[d++] = addl_arg[a++];
				}
				// now the contents of t represent our time
				fov_time = atof(t);
			}
			else
			{
				Com_Printf("camera roff 'fovaccel' notetrack missing 'time' argument\n", addl_arg);
				return;
			}
			if (cg_roffdebug.integer)
			{
				Com_Printf("notetrack: 'fovaccel %2.2f %3.5f %3.5f %d' on frame %d\n", begin_fov, fov_delta, fovDelta2,
					fov_time, client_camera.roff_frame);
			}
			CGCam_ZoomAccel(begin_fov, fov_delta, fovDelta2, fov_time);
		}
		else
		{
			Com_Printf("camera roff 'fovaccel' notetrack missing 'delta2' argument\n", addl_arg);
		}
	}
	else
	{
		Com_Printf("camera roff 'fovaccel' notetrack missing 'delta' argument\n", addl_arg);
	}
}

// 3/18/03 kef -- blatantly thieved from G_RoffNotetrackCallback
static void CG_RoffNotetrackCallback(const char* notetrack)
{
	int i = 0;
	char type[256]{};
	char addl_arg[512]{};
	int addl_args = 0;

	if (!notetrack)
	{
		return;
	}

	//notetrack = "fov 65";

	while (notetrack[i] && notetrack[i] != ' ')
	{
		type[i] = notetrack[i];
		i++;
	}

	type[i] = '\0';

	//if (notetrack[i] != ' ')
	//{ //didn't pass in a valid notetrack type, or forgot the argument for it
	//	return;
	//}

	/*	i++;

		while (notetrack[i] && notetrack[i] != ' ')
		{
			if (notetrack[i] != '\n' && notetrack[i] != '\r')
			{ //don't read line ends for an argument
				argument[r] = notetrack[i];
				r++;
			}
			i++;
		}
		argument[r] = '\0';
		if (!r)
		{
			return;
		}
	*/

	if (notetrack[i] == ' ')
	{
		//additional arguments...
		addl_args = 1;

		i++;
		int r = 0;
		while (notetrack[i])
		{
			addl_arg[r] = notetrack[i];
			r++;
			i++;
		}
		addl_arg[r] = '\0';
	}

	if (strcmp(type, "cut") == 0)
	{
		client_camera.info_state |= CAMERA_CUT;
		if (cg_roffdebug.integer)
		{
			Com_Printf("notetrack: 'cut' on frame %d\n", client_camera.roff_frame);
		}

		// this is just a really hacky way of getting a cut and a fov command on the same frame
		if (addl_args)
		{
			CG_RoffNotetrackCallback(addl_arg);
		}
	}
	else if (strcmp(type, "fov") == 0)
	{
		if (addl_args)
		{
			CGCam_NotetrackProcessFov(addl_arg);
			return;
		}
		Com_Printf("camera roff 'fov' notetrack missing fov argument\n", addl_arg);
	}
	else if (strcmp(type, "fovzoom") == 0)
	{
		if (addl_args)
		{
			CGCam_NotetrackProcessFovZoom(addl_arg);
			return;
		}
		Com_Printf("camera roff 'fovzoom' notetrack missing 'begin fov' argument\n", addl_arg);
	}
	else if (strcmp(type, "fovaccel") == 0)
	{
		if (addl_args)
		{
			CGCam_NotetrackProcessFovAccel(addl_arg);
			return;
		}
		Com_Printf("camera roff 'fovaccel' notetrack missing 'begin fov' argument\n", addl_arg);
	}
}

/*
-------------------------
CGCam_StartRoff

Sets up the camera to use
a rof file
-------------------------
*/

void CGCam_StartRoff(const char* roff)
{
	CGCam_FollowDisable();
	CGCam_TrackDisable();

	// Set up the roff state info..we'll hijack the moving and panning code until told otherwise
	//	...CAMERA_FOLLOWING would be a case that could override this..
	client_camera.info_state |= CAMERA_MOVING;
	client_camera.info_state |= CAMERA_PANNING;

	if (!G_LoadRoff(roff))
	{
		// The load failed so don't turn on the roff playback...
		Com_Printf(S_COLOR_RED"ROFF camera playback failed\n");
		return;
	}

	client_camera.info_state |= CAMERA_ROFFING;

	Q_strncpyz(client_camera.sRoff, roff, sizeof client_camera.sRoff);
	client_camera.roff_frame = 0;
	client_camera.next_roff_time = cg.time; // I can work right away
}

/*
-------------------------
CGCam_StopRoff

Stops camera rof
-------------------------
*/

static void CGCam_StopRoff()
{
	// Clear the roff flag
	client_camera.info_state &= ~CAMERA_ROFFING;
	client_camera.info_state &= ~CAMERA_MOVING;
}

/*
------------------------------------------------------
CGCam_Roff

Applies the sampled roff data to the camera and does
the lerping itself...this is done because the current
camera interpolation doesn't seem to work all that
great when you are adjusting the camera org and angles
so often...or maybe I'm just on crack.
------------------------------------------------------
*/

static void CGCam_Roff()
{
	while (client_camera.next_roff_time <= cg.time)
	{
		// Make sure that the roff is cached
		const int roff_id = G_LoadRoff(client_camera.sRoff);

		if (!roff_id)
		{
			return;
		}

		// The ID is one higher than the array index
		const roff_list_t* roff = &roffs[roff_id - 1];
		vec3_t org, ang;

		if (roff->type == 2)
		{
			const move_rotate2_t* data = &static_cast<move_rotate2_t*>(roff->data)[client_camera.roff_frame];
			VectorCopy(data->origin_delta, org);
			VectorCopy(data->rotate_delta, ang);

			// since we just hit a new frame, clear our CUT flag
			client_camera.info_state &= ~CAMERA_CUT;

			if (data->mStartNote != -1 || data->mNumNotes)
			{
				CG_RoffNotetrackCallback(roffs[roff_id - 1].mNoteTrackIndexes[data->mStartNote]);
			}
		}
		else
		{
			const move_rotate_t* data = &static_cast<move_rotate_t*>(roff->data)[client_camera.roff_frame];
			VectorCopy(data->origin_delta, org);
			VectorCopy(data->rotate_delta, ang);
		}

		// Yeah, um, I guess this just has to be negated?
		//ang[PITCH]	=- ang[PITCH];
		ang[ROLL] = -ang[ROLL];
		// might need to to yaw as well.  need a test...

		if (cg_developer.integer)
		{
			Com_Printf(S_COLOR_GREEN"CamROFF: frame: %d o:<%.2f %.2f %.2f> a:<%.2f %.2f %.2f>\n",
				client_camera.roff_frame,
				org[0], org[1], org[2],
				ang[0], ang[1], ang[2]);
		}

		if (client_camera.roff_frame)
		{
			// Don't mess with angles if we are following
			if (!(client_camera.info_state & CAMERA_FOLLOWING))
			{
				VectorAdd(client_camera.angles, client_camera.angles2, client_camera.angles);
			}

			VectorCopy(client_camera.origin2, client_camera.origin);
		}

		// Don't mess with angles if we are following
		if (!(client_camera.info_state & CAMERA_FOLLOWING))
		{
			VectorCopy(ang, client_camera.angles2);
			client_camera.pan_time = cg.time;
			client_camera.pan_duration = roff->mFrameTime;
		}

		VectorAdd(client_camera.origin, org, client_camera.origin2);

		client_camera.move_time = cg.time;
		client_camera.move_duration = roff->mFrameTime;

		if (++client_camera.roff_frame >= roff->frames)
		{
			CGCam_StopRoff();
			return;
		}

		// Check back in frameTime to get the next roff entry
		client_camera.next_roff_time += roff->mFrameTime;
	}
}

void CMD_CGCam_Disable()
{
	vec4_t fade = { 0, 0, 0, 0 };

	CGCam_Disable();
	CGCam_SetFade(fade);
	player_locked = qfalse;
}