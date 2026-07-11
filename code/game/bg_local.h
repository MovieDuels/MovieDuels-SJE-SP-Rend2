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

// bg_local.h -- local definitions for the bg (both games) files

#ifndef _BG_LOCAL_H
#define _BG_LOCAL_H

constexpr auto TIMER_LAND = 130;
constexpr auto TIMER_GESTURE = 34 * 66 + 50;

constexpr auto OVERCLIP = 1.001f;

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server
using pml_t = struct
{
    // movement orientation
    vec3_t forward = { 0.0f, 0.0f, 0.0f };
    vec3_t right = { 0.0f, 0.0f, 0.0f };
    vec3_t up = { 0.0f, 0.0f, 0.0f };

    // frame timing
    float frametime = 0.0f;
    int   msec = 0;

    // movement state
    qboolean walking = qfalse;
    qboolean groundPlane = qfalse;
    trace_t  groundTrace = {};

    float impactSpeed = 0.0f;

    // previous frame state (explicitly initialized to avoid C26495)
    vec3_t previous_origin = { 0.0f, 0.0f, 0.0f };
    vec3_t previous_velocity = { 0.0f, 0.0f, 0.0f };
    int    previous_waterlevel = 0;
};

extern pmove_t* pm;
extern pml_t pml;

extern int c_pmove;

void PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce);
void PM_AddTouchEnt(int entityNum);
void PM_AddEvent(int new_event);

qboolean PM_SlideMove(float grav_mod);
void PM_StepSlideMove(float grav_mod);

extern qboolean BG_AllowThirdPersonSpecialMove(const playerState_t* ps);

#endif
