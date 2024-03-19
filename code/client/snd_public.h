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

#ifndef _SND_PUBLIC_H
#define _SND_PUBLIC_H

void S_Init();
void S_Shutdown();

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_AddAmbientLoopingSound(const vec3_t origin, unsigned char volume, sfxHandle_t sfxHandle);
void S_StartAmbientSound(const vec3_t origin, int entityNum, unsigned char volume, sfxHandle_t sfxHandle);
void S_StartSound(const vec3_t origin, int entityNum, soundChannel_t entchannel, sfxHandle_t sfxHandle);
void S_StartLocalSound(sfxHandle_t sfxHandle, int channelNum);
void S_StartLocalLoopingSound(sfxHandle_t sfxHandle);

void S_UnCacheDynamicMusic();
void S_RestartMusic();
void S_StartBackgroundTrack(const char* intro, const char* loop, qboolean bCalledByCGameStart);
void S_StopBackgroundTrack();
float S_GetSampleLengthInMilliSeconds(sfxHandle_t sfxHandle);

// cinematics and voice-over-network will send raw samples
// 1.0 volume will be direct output of source samples
void S_RawSamples(int samples, int rate, int width, int channels, const byte* data, float volume,
	qboolean b_first_or_only_update_this_frame);
// stop all sounds
void S_StopSounds(); // from snd_dma.cpp
// stop all sounds and the background track
void S_StopAllSounds();

// scan all MP3s in the sound dir and add maxvol info if necessary.
void S_MP3_CalcVols_f();

// all continuous looping sounds must be added before calling S_Update
void S_ClearLoopingSounds();
void S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle,
	soundChannel_t chan = CHAN_AUTO);

// recompute the reletive volumes for all running sounds
// relative to the given entityNum / orientation
void S_Respatialize(int entityNum, const vec3_t head, vec3_t axis[3], qboolean inwater);

// let the sound system know where an entity currently is
void S_UpdateEntityPosition(int entityNum, const vec3_t origin);

void S_Update();

void S_DisableSounds();

void S_BeginRegistration();

// RegisterSound will allways return a valid sample, even if it
// has to create a placeholder.  This prevents continuous filesystem
// checks for missing files
sfxHandle_t S_RegisterSound(const char* name);

void S_FreeAllSFXMem();

#endif
