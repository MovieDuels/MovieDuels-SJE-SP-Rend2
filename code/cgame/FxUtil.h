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

#pragma once
#if !defined(FX_PRIMITIVES_H_INC)
#include "FxPrimitives.h"
#endif

#ifndef FX_UTIL_H_INC
#define FX_UTIL_H_INC

bool FX_Free(); // ditches all active effects;
int FX_Init(); // called in CG_Init to purge the fx system.
void FX_Add(bool portal); // called every cgame frame to add all fx into the scene.
void FX_Stop(); // ditches all active effects without touching the templates.

bool FX_ActiveFx(); // returns whether there are any active or scheduled effects

CParticle* FX_AddParticle(int client_id, const vec3_t org, const vec3_t vel, const vec3_t accel, float gravity,
	float size1, float size2, float size_parm,
	float alpha1, float alpha2, float alpha_parm,
	const vec3_t s_rgb, const vec3_t e_rgb, float rgb_parm,
	float rotation, float rotation_delta,
	const vec3_t min, const vec3_t max, float elasticity,
	int death_id, int impact_id,
	int kill_time, qhandle_t shader, int flags, int model_num = -1, int bolt_num = -1);

CLine* FX_AddLine(int client_id, vec3_t start, vec3_t end,
	float size1, float size2, float size_parm,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t s_rgb, vec3_t e_rgb, float rgb_parm,
	int kill_time, qhandle_t shader, int impact_fx_id, int flags, int model_num = -1, int bolt_num = -1);

CElectricity* FX_AddElectricity(int client_id, vec3_t start, vec3_t end, float size1, float size2, float size_parm,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t s_rgb, vec3_t e_rgb, float rgb_parm,
	float chaos, int kill_time, qhandle_t shader, int flags, int model_num = -1,
	int bolt_num = -1);

CTail* FX_AddTail(int client_id, vec3_t org, vec3_t vel, vec3_t accel,
	float size1, float size2, float size_parm,
	float length1, float length2, float length_parm,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t s_rgb, vec3_t e_rgb, float rgb_parm,
	vec3_t min, vec3_t max, float elasticity,
	int death_id, int impact_id,
	int kill_time, qhandle_t shader, int flags, int model_num = -1, int bolt_num = -1);

CCylinder* FX_AddCylinder(int client_id, vec3_t start, vec3_t normal,
	float size1_s, float size1_e, float size_parm,
	float size2_s, float size2_e, float size2_parm,
	float length1, float length2, float length_parm,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t rgb1, vec3_t rgb2, float rgb_parm,
	int kill_time, qhandle_t shader, int flags, int model_num = -1, int bolt_num = -1);

CEmitter* FX_AddEmitter(vec3_t org, vec3_t vel, vec3_t accel,
	float size1, float size2, float size_parm,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t rgb1, vec3_t rgb2, float rgb_parm,
	vec3_t angs, vec3_t delta_angs,
	vec3_t min, vec3_t max, float elasticity,
	int death_id, int impact_id, int emitter_id,
	float density, float variance,
	int kill_time, qhandle_t model, int flags);

CLight* FX_AddLight(vec3_t org, float size1, float size2, float size_parm,
	vec3_t rgb1, vec3_t rgb2, float rgb_parm,
	int kill_time, int flags);

COrientedParticle* FX_AddOrientedParticle(int client_id, vec3_t org, vec3_t norm, vec3_t vel, vec3_t accel,
	float size1, float size2, float size_parm,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t rgb1, vec3_t rgb2, float rgb_parm,
	float rotation, float rotation_delta,
	vec3_t min, vec3_t max, float bounce,
	int death_id, int impact_id,
	int kill_time, qhandle_t shader, int flags, int model_num = -1,
	int bolt_num = -1);

CPoly* FX_AddPoly(const vec3_t* verts, const vec2_t* st, int numVerts,
	vec3_t vel, vec3_t accel,
	float alpha1, float alpha2, float alpha_parm,
	vec3_t rgb1, vec3_t rgb2, float rgbParm,
	vec3_t rotation_delta, float bounce, int motion_delay,
	int kill_time, qhandle_t shader, int flags);

CFlash* FX_AddFlash(vec3_t origin, vec3_t s_rgb, vec3_t e_rgb, float rgb_parm,
	int kill_time, qhandle_t shader, int flags);

// Included for backwards compatibility with CHC and for doing quick programmatic effects.
void FX_AddSprite(vec3_t origin, vec3_t vel, vec3_t accel,
	float scale,
	float s_alpha, float e_alpha,
	float rotation, float bounce,
	int life, qhandle_t shader, int flags = 0);

void FX_AddSprite(vec3_t origin, vec3_t vel, vec3_t accel,
	float scale,
	float s_alpha, float e_alpha,
	vec3_t sRGB, vec3_t e_rgb,
	float rotation, float bounce,
	int life, qhandle_t shader, int flags = 0);

void FX_AddLine(vec3_t start, vec3_t end,
	float width,
	float s_alpha, float e_alpha,
	int life, qhandle_t shader);

void FX_AddLine(vec3_t start, vec3_t end,
	float width,
	float s_alpha, float e_alpha,
	vec3_t s_rgb, vec3_t e_rgb,
	int life, qhandle_t shader, int flags = 0);

void FX_AddQuad(vec3_t origin, vec3_t normal,
	vec3_t vel, vec3_t accel,
	float sradius, float eradius,
	float salpha, float ealpha,
	vec3_t s_rgb, vec3_t e_rgb,
	float rotation, int life, qhandle_t shader);

CBezier* FX_AddBezier(const vec3_t start, const vec3_t end,
	const vec3_t control1, const vec3_t control1_vel,
	const vec3_t control2, const vec3_t control2_vel,
	float size1, float size2, float size_parm,
	float alpha1, float alpha2, float alpha_parm,
	const vec3_t s_rgb, const vec3_t e_rgb, float rgb_parm,
	int kill_time, qhandle_t shader, int flags = 0);

#endif //FX_UTIL_H_INC
