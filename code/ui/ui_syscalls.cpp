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

#include "../server/exe_headers.h"

#include "ui_local.h"

float trap_Cvar_VariableValue(const char* var_name)
{
	return Cvar_VariableValue(var_name);
}

void trap_R_ClearScene()
{
	ui.R_ClearScene();
}

void trap_R_AddRefEntityToScene(const refEntity_t* re)
{
	ui.R_AddRefEntityToScene(re);
}

void trap_R_RenderScene(const refdef_t* fd)
{
	//	syscall( UI_R_RENDERSCENE, fd );
	ui.R_RenderScene(fd);
}

void trap_R_SetColor(const float* rgba)
{
	//	syscall( UI_R_SETCOLOR, rgba );
	//	re.SetColor( rgba );
	ui.R_SetColor(rgba);
}

void trap_R_DrawStretchPic(const float x, const float y, const float w, const float h, const float s1, const float t1, const float s2, const float t2,
	const qhandle_t hShader)
{
	//	syscall( UI_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader );
	//	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader  );

	ui.R_DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

void trap_R_ModelBounds(const clipHandle_t model, vec3_t mins, vec3_t maxs)
{
	//	syscall( UI_R_MODELBOUNDS, model, mins, maxs );
	ui.R_ModelBounds(model, mins, maxs);
}

void trap_S_StartLocalSound(const sfxHandle_t sfx, const int channelNum)
{
	//	syscall( UI_S_STARTLOCALSOUND, sfx, channelNum );
	S_StartLocalSound(sfx, channelNum);
}

void trap_S_StopSounds()
{
	S_StopSounds();
}

sfxHandle_t trap_S_RegisterSound(const char* sample, qboolean compressed)
{
	return S_RegisterSound(sample);
}

void trap_Key_SetBinding(const int keynum, const char* binding)
{
	Key_SetBinding(keynum, binding);
}

qboolean trap_Key_GetOverstrikeMode()
{
	return Key_GetOverstrikeMode();
}

void trap_Key_SetOverstrikeMode(const qboolean state)
{
	Key_SetOverstrikeMode(state);
}

void trap_Key_ClearStates()
{
	Key_ClearStates();
}

int Key_GetCatcher();

int trap_Key_GetCatcher()
{
	return Key_GetCatcher();
}

void Key_SetCatcher(int catcher);

void trap_Key_SetCatcher(const int catcher)
{
	Key_SetCatcher(catcher);
}

/*
void trap_GetClipboardData( char *buf, int bufsize ) {
	syscall( UI_GETCLIPBOARDDATA, buf, bufsize );
}

void trap_GetClientState( uiClientState_t *state ) {
	syscall( UI_GETCLIENTSTATE, state );
}
*/

void CL_GetGlconfig(glconfig_t* glconfig);

void trap_GetGlconfig(glconfig_t* glconfig)
{
	//	syscall( UI_GETGLCONFIG, glconfig );
	CL_GetGlconfig(glconfig);
}

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse (do not alter gamestate)
int trap_CIN_PlayCinematic(const char* arg0, const int xpos, const int ypos, const int width, const int height, const int bits,
	const char* psAudioFile)
{
	//  return syscall(UI_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits, psAudioFile);
	return CIN_PlayCinematic(arg0, xpos, ypos, width, height, bits, psAudioFile);
}

// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
int trap_CIN_StopCinematic(const int handle)
{
	//  return syscall(UI_CIN_STOPCINEMATIC, handle);
	return CIN_StopCinematic(handle);
}