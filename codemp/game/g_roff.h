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

#ifndef __G_ROFF_H__
#define __G_ROFF_H__

#include "../qcommon/q_shared.h"

// ROFF Defines
//-------------------
#define ROFF_VERSIONNEW		1	// ver # for the (R)otation (O)bject (F)ile (F)ormat
#define ROFF_VERSIONNEW2	2	// ver # for the (R)otation (O)bject (F)ile (F)ormat
#define MAX_ROFFSNEW		128	// hard coded number of max roffs per level, sigh..
#define ROFF_SAMPLE_RATENEW	20	// 10hz

#define ROFF_INFO_SIZENEW	30000		//max size for ROFF file.
#define MAXNOTETRACKSNEW	8			//max number of note tracks
#define NOTETRACKSSIZENEW	100			//size of note tracks
#define MAXNUMDATANEW		500			//max number of data positions for ROFF.

// ROFF Header file definition
//-------------------------------
typedef struct roff_hdr_s
{
	char mHeader[4]; // should be "ROFF" (Rotation, Origin File Format)
	int mVersion;
	float mCount; // There isn't any reason for this to be anything other than an int, sigh...
	//
	//		Move - Rotate data follows....vec3_t delta_origin, vec3_t delta_rotation
	//
} roff_hdr_t;

// ROFF move rotate data element
//--------------------------------
typedef struct move_rotate_s
{
	vec3_t origin_delta;
	vec3_t rotate_delta;
} move_rotate_t;

typedef struct roff_hdr2_s
//-------------------------------
{
	char mHeader[4]; // should match roff_string defined above
	int mVersion; // version num, supported version defined above
	int mCount; // I think this is a float because of a limitation of the roff exporter
	int mFrameRate; // Frame rate the roff should be played at
	int mNumNotes; // number of notes (null terminated strings) after the roff data
} roff_hdr2_t;

typedef struct move_rotate2_s
//-------------------------------
{
	vec3_t origin_delta;
	vec3_t rotate_delta;
	int mStartNote, mNumNotes; // note track info
} move_rotate2_t;

// a precached ROFF list
//-------------------------
typedef struct roff_list_s
{
	int type; // roff type number, 1-old, 2-new
	char* fileName; // roff filename
	int frames; // number of roff entries
	move_rotate2_t data[MAXNOTETRACKSNEW]; // delta move and rotate vector list
	int NumData; //number of data positions we are currently using.
	int mFrameTime; // frame rate
	int mLerp; // Lerp rate (FPS)
	int mNumNoteTracks;
	char mNoteTrackIndexes[MAXNOTETRACKSNEW][NOTETRACKSSIZENEW];
} roff_list_t;

extern roff_list_t roffs[];
extern int num_roffs;

// Function prototypes
//-------------------------
int G_LoadRoffs(const char* fileName);
//void    G_Roffs(gentity_t* ent);
void G_SaveCachedRoffs();
void G_LoadCachedRoffs();

#endif
