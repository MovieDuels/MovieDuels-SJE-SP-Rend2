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

#if !defined(FX_SCHEDULER_H_INC)
#include "FxScheduler.h"
#endif

#include "cg_media.h"	//for cgs.model_draw for G2

extern vmCvar_t fx_debug;
extern vmCvar_t fx_freeze;

extern void CG_ExplosionEffects(vec3_t origin, float intensity, int radius, int time);

// Stuff for the FxHelper
//------------------------------------------------------
void SFxHelper::Init()
{
	mTime = 0;
}

//------------------------------------------------------
void SFxHelper::Print(const char* msg, ...)
{
#ifndef FINAL_BUILD

	va_list		argptr;
	char		text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	gi.Printf(text);

#endif
}

//------------------------------------------------------
void SFxHelper::AdjustTime(int frame_time)
{
	if (fx_freeze.integer || frame_time <= 0)
	{
		// Allow no time progression when we are paused.
		mFrameTime = 0;
		mFloatFrameTime = 0.0f;
	}
	else
	{
		if (!cg_paused.integer)
		{
			if (frame_time > 300) // hack for returning from paused and time bursts
			{
				frame_time = 300;
			}

			mFrameTime = frame_time;
			mFloatFrameTime = mFrameTime * 0.001f;
			mTime += mFrameTime;
		}
	}
}

//------------------------------------------------------
int SFxHelper::OpenFile(const char* file, fileHandle_t* fh, int mode)
{
	return cgi_FS_FOpenFile(file, fh, FS_READ);
}

//------------------------------------------------------
int SFxHelper::ReadFile(void* data, const int len, const fileHandle_t fh)
{
	return cgi_FS_Read(data, len, fh);
}

//------------------------------------------------------
void SFxHelper::CloseFile(const fileHandle_t fh)
{
	cgi_FS_FCloseFile(fh);
}

//------------------------------------------------------
void SFxHelper::PlaySound(const vec3_t org, const int entityNum, const int entchannel, const int sfx_handle)
{
	cgi_S_StartSound(org, entityNum, entchannel, sfx_handle);
}

//------------------------------------------------------
void SFxHelper::PlayLocalSound(const int sfx_handle, const int channelNum)
{
	cgi_S_StartLocalSound(sfx_handle, channelNum);
}

//------------------------------------------------------
void SFxHelper::Trace(trace_t* tr, vec3_t start, vec3_t min, vec3_t max,
	vec3_t end, const int skipEntNum, const int flags)
{
	CG_Trace(tr, start, min, max, end, skipEntNum, flags);
}

void SFxHelper::G2Trace(trace_t* tr, vec3_t start, vec3_t min, vec3_t max,
	vec3_t end, const int skipEntNum, const int flags)
{
	//CG_Trace( tr, start, min, max, end, skipEntNum, flags, G2_COLLIDE );
	gi.trace(tr, start, nullptr, nullptr, end, skipEntNum, flags, G2_COLLIDE, 0);
}

//------------------------------------------------------
void SFxHelper::AddFxToScene(const refEntity_t* ent)
{
	cgi_R_AddRefEntityToScene(ent);
}

//------------------------------------------------------
int SFxHelper::RegisterShader(const gsl::cstring_view& shader)
{
	// TODO: it would be nice to change the ABI here to allow for passing of string views
	return cgi_R_RegisterShader(std::string(shader.begin(), shader.end()).c_str());
}

//------------------------------------------------------
int SFxHelper::RegisterSound(const gsl::cstring_view& sound)
{
	// TODO: it would be nice to change the ABI here to allow for passing of string views
	return cgi_S_RegisterSound(std::string(sound.begin(), sound.end()).c_str());
}

//------------------------------------------------------
int SFxHelper::RegisterModel(const gsl::cstring_view& model)
{
	return cgi_R_RegisterModel(std::string(model.begin(), model.end()).c_str());
}

//------------------------------------------------------
void SFxHelper::AddLightToScene(vec3_t org, const float radius, const float red, const float green, const float blue)
{
	cgi_R_AddLightToScene(org, radius, red, green, blue);
}

//------------------------------------------------------
void SFxHelper::AddPolyToScene(const int shader, const int count, const polyVert_t* verts)
{
	cgi_R_AddPolyToScene(shader, count, verts);
}

//------------------------------------------------------
void SFxHelper::CameraShake(vec3_t origin, const float intensity, const int radius, const int time)
{
	CG_ExplosionEffects(origin, intensity, radius, time);
}

//------------------------------------------------------
int SFxHelper::GetOriginAxisFromBolt(const centity_t& cent, const int modelNum, const int boltNum, vec3_t /*out*/origin,
	vec3_t /*out*/axis[3])
{
	if (cg.time - cent.snapShotTime > 200)
	{
		//you were added more than 200ms ago, so I say you are no longer valid/in our snapshot.
		return 0;
	}

	mdxaBone_t bolt_matrix;
	vec3_t G2Angles = { cent.lerpAngles[0], cent.lerpAngles[1], cent.lerpAngles[2] };
	if (cent.currentState.eType == ET_PLAYER)
	{
		//players use cent.renderAngles
		VectorCopy(cent.renderAngles, G2Angles);

		if (cent.gent //has a game entity
			&& cent.gent->s.m_iVehicleNum != 0 //in a vehicle
			&& cent.gent->m_pVehicle //have a valid vehicle pointer
			&& cent.gent->m_pVehicle->m_pVehicleInfo->type != VH_FIGHTER //it's not a fighter
			&& cent.gent->m_pVehicle->m_pVehicleInfo->type != VH_SPEEDER //not a speeder
			)
		{
			G2Angles[PITCH] = 0;
			G2Angles[ROLL] = 0;
		}
	}

	// go away and get me the bolt position for this frame please
	const int doesBoltExist = gi.G2API_GetBoltMatrix(cent.gent->ghoul2, modelNum,
		boltNum, &bolt_matrix, G2Angles,
		cent.lerpOrigin, cg.time, cgs.model_draw,
		cent.currentState.modelScale);
	// set up the axis and origin we need for the actual effect spawning
	origin[0] = bolt_matrix.matrix[0][3];
	origin[1] = bolt_matrix.matrix[1][3];
	origin[2] = bolt_matrix.matrix[2][3];

	axis[1][0] = bolt_matrix.matrix[0][0];
	axis[1][1] = bolt_matrix.matrix[1][0];
	axis[1][2] = bolt_matrix.matrix[2][0];

	axis[0][0] = bolt_matrix.matrix[0][1];
	axis[0][1] = bolt_matrix.matrix[1][1];
	axis[0][2] = bolt_matrix.matrix[2][1];

	axis[2][0] = bolt_matrix.matrix[0][2];
	axis[2][1] = bolt_matrix.matrix[1][2];
	axis[2][2] = bolt_matrix.matrix[2][2];
	return doesBoltExist;
}