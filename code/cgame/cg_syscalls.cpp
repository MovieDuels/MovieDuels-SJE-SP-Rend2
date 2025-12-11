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

#include "cg_headers.h"

// this file is only included when building a dll

//prototypes
extern void CG_PreInit();

static intptr_t(QDECL* Q_syscall)(intptr_t arg, ...) = reinterpret_cast<intptr_t(__cdecl*)(intptr_t, ...)>(-1);

extern "C" Q_EXPORT void QDECL dllEntry(intptr_t(QDECL * syscallptr)(intptr_t arg, ...))
{
	Q_syscall = syscallptr;
	CG_PreInit();
}

inline int PASSFLOAT(const float x)
{
	byteAlias_t fi{};
	fi.f = x;
	return fi.i;
}

void cgi_Printf(const char* fmt)
{
	Q_syscall(CG_PRINT, fmt);
}

NORETURN void cgi_Error(const char* fmt)
{
	Q_syscall(CG_ERROR, fmt);
	// shut up GCC warning about returning functions, because we know better
	exit(1);
}

int cgi_Milliseconds()
{
	return Q_syscall(CG_MILLISECONDS);
}

void cgi_Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, const int flags)
{
	Q_syscall(CG_CVAR_REGISTER, vmCvar, varName, defaultValue, flags);
}

void cgi_Cvar_Update(vmCvar_t* vmCvar)
{
	Q_syscall(CG_CVAR_UPDATE, vmCvar);
}

void cgi_Cvar_Set(const char* var_name, const char* value)
{
	Q_syscall(CG_CVAR_SET, var_name, value);
}

int cgi_Argc()
{
	return Q_syscall(CG_ARGC);
}

void cgi_Argv(const int n, char* buffer, const int bufferLength)
{
	Q_syscall(CG_ARGV, n, buffer, bufferLength);
}

void cgi_Args(char* buffer, const int bufferLength)
{
	Q_syscall(CG_ARGS, buffer, bufferLength);
}

int cgi_FS_FOpenFile(const char* qpath, fileHandle_t* f, const fsMode_t mode)
{
	return Q_syscall(CG_FS_FOPENFILE, qpath, f, mode);
}

int cgi_FS_Read(void* buffer, const int len, const fileHandle_t f)
{
	return Q_syscall(CG_FS_READ, buffer, len, f);
}

int cgi_FS_Write(const void* buffer, const int len, const fileHandle_t f)
{
	return Q_syscall(CG_FS_WRITE, buffer, len, f);
}

void cgi_FS_FCloseFile(const fileHandle_t f)
{
	Q_syscall(CG_FS_FCLOSEFILE, f);
}

void cgi_SendConsoleCommand(const char* text)
{
	Q_syscall(CG_SENDCONSOLECOMMAND, text);
}

void cgi_AddCommand(const char* cmdName)
{
	Q_syscall(CG_ADDCOMMAND, cmdName);
}

void cgi_SendClientCommand(const char* s)
{
	Q_syscall(CG_SENDCLIENTCOMMAND, s);
}

void cgi_UpdateScreen()
{
	Q_syscall(CG_UPDATESCREEN);
}

//RMG BEGIN
void cgi_RMG_Init(const int terrainID, const char* terrainInfo)
{
	Q_syscall(CG_RMG_INIT, terrainID, terrainInfo);
}

int cgi_CM_RegisterTerrain(const char* terrainInfo)
{
	return Q_syscall(CG_CM_REGISTER_TERRAIN, terrainInfo);
}

void cgi_RE_InitRendererTerrain(const char* terrainInfo)
{
	Q_syscall(CG_RE_INIT_RENDERER_TERRAIN, terrainInfo);
}

//RMG END

void cgi_CM_LoadMap(const char* mapname, const qboolean subBSP)
{
	Q_syscall(CG_CM_LOADMAP, mapname, subBSP);
}

int cgi_CM_NumInlineModels()
{
	return Q_syscall(CG_CM_NUMINLINEMODELS);
}

clipHandle_t cgi_CM_InlineModel(const int index)
{
	return Q_syscall(CG_CM_INLINEMODEL, index);
}

clipHandle_t cgi_CM_TempBoxModel(const vec3_t mins, const vec3_t maxs)
{
	//, const int contents ) {
	return Q_syscall(CG_CM_TEMPBOXMODEL, mins, maxs); //, contents );
}

int cgi_CM_PointContents(const vec3_t p, const clipHandle_t model)
{
	return Q_syscall(CG_CM_POINTCONTENTS, p, model);
}

int cgi_CM_TransformedPointContents(const vec3_t p, const clipHandle_t model, const vec3_t origin, const vec3_t angles)
{
	return Q_syscall(CG_CM_TRANSFORMEDPOINTCONTENTS, p, model, origin, angles);
}

void cgi_CM_BoxTrace(trace_t* results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs,
	const clipHandle_t model, const int brushmask)
{
	Q_syscall(CG_CM_BOXTRACE, results, start, end, mins, maxs, model, brushmask);
}

void cgi_CM_TransformedBoxTrace(trace_t* results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs,
	const clipHandle_t model, const int brushmask,
	const vec3_t origin, const vec3_t angles)
{
	Q_syscall(CG_CM_TRANSFORMEDBOXTRACE, results, start, end, mins, maxs, model, brushmask, origin, angles);
}

int cgi_CM_MarkFragments(const int numPoints, const vec3_t* points,
	const vec3_t projection,
	const int max_points, vec3_t point_buffer,
	const int max_fragments, markFragment_t* fragment_buffer)
{
	return Q_syscall(CG_CM_MARKFRAGMENTS, numPoints, points, projection, max_points, point_buffer, max_fragments,
		fragment_buffer);
}

void cgi_CM_SnapPVS(vec3_t origin, byte* buffer)
{
	Q_syscall(CG_CM_SNAPPVS, origin, buffer);
}

void cgi_S_StopSounds()
{
	Q_syscall(CG_S_STOPSOUNDS);
}

void cgi_S_StartSound(const vec3_t origin, const int entityNum, const int entchannel, const sfxHandle_t sfx)
{
	Q_syscall(CG_S_STARTSOUND, origin, entityNum, entchannel, sfx);
}

void cgi_AS_ParseSets()
{
	Q_syscall(CG_AS_PARSESETS);
}

void cgi_AS_AddPrecacheEntry(const char* name)
{
	Q_syscall(CG_AS_ADDENTRY, name);
}

void cgi_S_UpdateAmbientSet(const char* name, vec3_t origin)
{
	Q_syscall(CG_S_UPDATEAMBIENTSET, name, origin);
}

int cgi_S_AddLocalSet(const char* name, vec3_t listener_origin, vec3_t origin, const int entID, const int time)
{
	return Q_syscall(CG_S_ADDLOCALSET, name, listener_origin, origin, entID, time);
}

sfxHandle_t cgi_AS_GetBModelSound(const char* name, const int stage)
{
	return Q_syscall(CG_AS_GETBMODELSOUND, name, stage);
}

void cgi_S_StartLocalSound(const sfxHandle_t sfx, const int channelNum)
{
	Q_syscall(CG_S_STARTLOCALSOUND, sfx, channelNum);
}

void cgi_S_ClearLoopingSounds()
{
	Q_syscall(CG_S_CLEARLOOPINGSOUNDS);
}

void cgi_S_AddLoopingSound(const int entityNum, const vec3_t origin, const vec3_t velocity, const sfxHandle_t sfx,
	const soundChannel_t chan)
{
	Q_syscall(CG_S_ADDLOOPINGSOUND, entityNum, origin, velocity, sfx, chan);
}

void cgi_S_UpdateEntityPosition(const int entityNum, const vec3_t origin)
{
	Q_syscall(CG_S_UPDATEENTITYPOSITION, entityNum, origin);
}

void cgi_S_Respatialize(const int entityNum, const vec3_t origin, vec3_t axis[3], const qboolean inwater)
{
	Q_syscall(CG_S_RESPATIALIZE, entityNum, origin, axis, inwater);
}

sfxHandle_t cgi_S_RegisterSound(const char* sample)
{
	return Q_syscall(CG_S_REGISTERSOUND, sample);
}

void cgi_S_StartBackgroundTrack(const char* intro, const char* loop, const qboolean bForceStart)
{
	Q_syscall(CG_S_STARTBACKGROUNDTRACK, intro, loop, bForceStart);
}

float cgi_S_GetSampleLength(const sfxHandle_t sfx)
{
	return Q_syscall(CG_S_GETSAMPLELENGTH, sfx);
}

void cgi_R_LoadWorldMap(const char* mapname)
{
	Q_syscall(CG_R_LOADWORLDMAP, mapname);
}

qhandle_t cgi_R_RegisterModel(const char* name)
{
	return Q_syscall(CG_R_REGISTERMODEL, name);
}

qhandle_t cgi_R_RegisterSkin(const char* name)
{
	return Q_syscall(CG_R_REGISTERSKIN, name);
}

qhandle_t cgi_R_RegisterShader(const char* name)
{
	return Q_syscall(CG_R_REGISTERSHADER, name);
}

qhandle_t cgi_R_RegisterShaderNoMip(const char* name)
{
	return Q_syscall(CG_R_REGISTERSHADERNOMIP, name);
}

qhandle_t cgi_R_RegisterFont(const char* name)
{
	return Q_syscall(CG_R_REGISTERFONT, name);
}

int cgi_R_Font_StrLenPixels(const char* text, const int iFontIndex, const float scale /*= 1.0f*/)
{
	return Q_syscall(CG_R_FONTSTRLENPIXELS, text, iFontIndex, PASSFLOAT(scale));
}

int cgi_R_Font_StrLenChars(const char* text)
{
	return Q_syscall(CG_R_FONTSTRLENCHARS, text);
}

int cgi_R_Font_HeightPixels(const int iFontIndex, const float scale /*= 1.0f*/)
{
	return Q_syscall(CG_R_FONTHEIGHTPIXELS, iFontIndex, PASSFLOAT(scale));
}

qboolean cgi_Language_IsAsian()
{
	return static_cast<qboolean>(Q_syscall(CG_LANGUAGE_ISASIAN) != 0);
}

qboolean cgi_Language_UsesSpaces()
{
	return static_cast<qboolean>(Q_syscall(CG_LANGUAGE_USESSPACES) != 0);
}

unsigned int cgi_AnyLanguage_ReadCharFromString(const char* psText, int* pi_advance_count,
	qboolean* pbIsTrailingPunctuation /* = NULL */)
{
	return Q_syscall(CG_ANYLANGUAGE_READFROMSTRING, psText, pi_advance_count, pbIsTrailingPunctuation);
}

void cgi_R_Font_DrawString(const int ox, const int oy, const char* text, const float* rgba, const int setIndex,
	const int iMaxPixelWidth,
	const float scale /*= 1.0f*/)
{
	Q_syscall(CG_R_FONTDRAWSTRING, ox, oy, text, rgba, setIndex, iMaxPixelWidth, PASSFLOAT(scale));
}

//set some properties for the draw layer for my refractive effect (here primarily for mod authors) -rww
void cgi_R_SetRefractProp(const float alpha, const float stretch, const qboolean prepost, const qboolean negate)
{
	Q_syscall(CG_R_SETREFRACTIONPROP, PASSFLOAT(alpha), PASSFLOAT(stretch), prepost, negate);
}

void cgi_R_ClearScene()
{
	Q_syscall(CG_R_CLEARSCENE);
}

void cgi_R_AddRefEntityToScene(const refEntity_t* re)
{
	Q_syscall(CG_R_ADDREFENTITYTOSCENE, re);
}

qboolean cgi_R_inPVS(vec3_t p1, vec3_t p2)
{
	byte* mask;
	return (qboolean)(Q_syscall(CG_R_INPVS, p1, p2, mask) != 0);
}

void cgi_R_GetLighting(const vec3_t origin, vec3_t ambientLight, vec3_t directedLight, vec3_t ligthDir)
{
	Q_syscall(CG_R_GETLIGHTING, origin, ambientLight, directedLight, ligthDir);
}

void cgi_R_AddPolyToScene(const qhandle_t hShader, const int numVerts, const polyVert_t* verts)
{
	Q_syscall(CG_R_ADDPOLYTOSCENE, hShader, numVerts, verts);
}

void cgi_R_AddLightToScene(const vec3_t org, const float intensity, const float r, const float g, const float b)
{
	Q_syscall(CG_R_ADDLIGHTTOSCENE, org, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b));
}

void cgi_R_RenderScene(const refdef_t* fd)
{
	Q_syscall(CG_R_RENDERSCENE, fd);
}

void cgi_R_SetColor(const float* rgba)
{
	Q_syscall(CG_R_SETCOLOR, rgba);
}

void cgi_R_DrawStretchPic(const float x, const float y, const float w, const float h,
	const float s1, const float t1, const float s2, const float t2, const qhandle_t hShader)
{
	Q_syscall(CG_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1),
		PASSFLOAT(s2), PASSFLOAT(t2), hShader);
}

void cgi_R_ModelBounds(const qhandle_t model, vec3_t mins, vec3_t maxs)
{
	Q_syscall(CG_R_MODELBOUNDS, model, mins, maxs);
}

void cgi_R_LerpTag(orientation_t* tag, const qhandle_t mod, const int startFrame, const int endFrame,
	const float frac, const char* tagName)
{
	Q_syscall(CG_R_LERPTAG, tag, mod, startFrame, endFrame, PASSFLOAT(frac), tagName);
}

void cgi_R_DrawRotatePic(const float x, const float y, const float w, const float h,
	const float s1, const float t1, const float s2, const float t2, const float a,
	const qhandle_t hShader, const float aspectCorrection)
{
	Q_syscall(CG_R_DRAWROTATEPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1),
		PASSFLOAT(s2), PASSFLOAT(t2), PASSFLOAT(a), hShader, PASSFLOAT(aspectCorrection));
}

void cgi_R_DrawRotatePic2(const float x, const float y, const float w, const float h,
	const float s1, const float t1, const float s2, const float t2, const float a,
	const qhandle_t hShader, const float aspectCorrection)
{
	Q_syscall(CG_R_DRAWROTATEPIC2, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1),
		PASSFLOAT(s2), PASSFLOAT(t2), PASSFLOAT(a), hShader, PASSFLOAT(aspectCorrection));
}

//linear fogging, with settable range -rww
void cgi_R_SetRangeFog(const float range)
{
	Q_syscall(CG_R_SETRANGEFOG, PASSFLOAT(range));
}

void cgi_R_LAGoggles()
{
	Q_syscall(CG_R_LA_GOGGLES);
}

void cgi_R_Scissor(const float x, const float y, const float w, const float h)
{
	Q_syscall(CG_R_SCISSOR, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h));
}

void cgi_GetGlconfig(glconfig_t* glconfig)
{
	Q_syscall(CG_GETGLCONFIG, glconfig);
}

void cgi_GetGameState(gameState_t* gamestate)
{
	Q_syscall(CG_GETGAMESTATE, gamestate);
}

void cgi_GetCurrentSnapshotNumber(int* snapshotNumber, int* serverTime)
{
	Q_syscall(CG_GETCURRENTSNAPSHOTNUMBER, snapshotNumber, serverTime);
}

qboolean cgi_GetSnapshot(const int snapshotNumber, snapshot_t* snapshot)
{
	return static_cast<qboolean>(Q_syscall(CG_GETSNAPSHOT, snapshotNumber, snapshot) != 0);
}

qboolean cgi_GetDefaultState(const int entityIndex, entityState_t* state)
{
	return static_cast<qboolean>(Q_syscall(CG_GETDEFAULTSTATE, entityIndex, state) != 0);
}

qboolean cgi_GetServerCommand(const int serverCommandNumber)
{
	return static_cast<qboolean>(Q_syscall(CG_GETSERVERCOMMAND, serverCommandNumber) != 0);
}

int cgi_GetCurrentCmdNumber()
{
	return Q_syscall(CG_GETCURRENTCMDNUMBER);
}

qboolean cgi_GetUserCmd(const int cmdNumber, usercmd_t* ucmd)
{
	return static_cast<qboolean>(Q_syscall(CG_GETUSERCMD, cmdNumber, ucmd) != 0);
}

void cgi_SetUserCmdValue(const int stateValue, const float sensitivityScale, const float mPitchOverride,
	const float mYawOverride)
{
	Q_syscall(CG_SETUSERCMDVALUE, stateValue, PASSFLOAT(sensitivityScale), PASSFLOAT(mPitchOverride),
		PASSFLOAT(mYawOverride));
}

void cgi_SetUserCmdAngles(const float pitchOverride, const float yawOverride, const float rollOverride)
{
	Q_syscall(CG_SETUSERCMDANGLES, PASSFLOAT(pitchOverride), PASSFLOAT(yawOverride), PASSFLOAT(rollOverride));
}

/*
Ghoul2 Insert Start
*/
// CG Specific API calls
void trap_G2_SetGhoul2ModelIndexes(CGhoul2Info_v& ghoul2, qhandle_t* model_list, qhandle_t* skin_list)
{
	Q_syscall(CG_G2_SETMODELS, &ghoul2, model_list, skin_list);
}

/*
Ghoul2 Insert End
*/

void trap_Com_SetOrgAngles(vec3_t org, vec3_t angles)
{
	Q_syscall(COM_SETORGANGLES, org, angles);
}

void trap_R_GetLightStyle(const int style, color4ub_t color)
{
	Q_syscall(CG_R_GET_LIGHT_STYLE, style, color);
}

void trap_R_SetLightStyle(const int style, const int color)
{
	Q_syscall(CG_R_SET_LIGHT_STYLE, style, color);
}

void cgi_R_GetBModelVerts(const int bmodelIndex, vec3_t* verts, vec3_t normal)
{
	Q_syscall(CG_R_GET_BMODEL_VERTS, bmodelIndex, verts, normal);
}

void cgi_R_WorldEffectCommand(const char* command)
{
	Q_syscall(CG_R_WORLD_EFFECT_COMMAND, command);
}

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse (do not alter gamestate)
int trap_CIN_PlayCinematic(const char* arg0, const int xpos, const int ypos, const int width, const int height,
	const int bits,
	const char* psAudioFile /* = NULL */)
{
	return Q_syscall(CG_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits, psAudioFile);
}

// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
e_status trap_CIN_StopCinematic(const int handle)
{
	return static_cast<e_status>(Q_syscall(CG_CIN_STOPCINEMATIC, handle));
}

// will run a frame of the cinematic but will not draw it.  Will return FMV_EOF if the end of the cinematic has been reached.
e_status trap_CIN_RunCinematic(const int handle)
{
	return static_cast<e_status>(Q_syscall(CG_CIN_RUNCINEMATIC, handle));
}

// draws the current frame
void trap_CIN_DrawCinematic(const int handle)
{
	Q_syscall(CG_CIN_DRAWCINEMATIC, handle);
}

// allows you to resize the animation dynamically
void trap_CIN_SetExtents(const int handle, const int x, const int y, const int w, const int h)
{
	Q_syscall(CG_CIN_SETEXTENTS, handle, x, y, w, h);
}

void* cgi_Z_Malloc(const int size, const int tag)
{
	return reinterpret_cast<void*>(Q_syscall(CG_Z_MALLOC, size, tag));
}

void cgi_Z_Free(void* ptr)
{
	Q_syscall(CG_Z_FREE, ptr);
}

void cgi_UI_SetActive_Menu(char* name)
{
	Q_syscall(CG_UI_SETACTIVE_MENU, name);
}

void cgi_UI_Menu_OpenByName(char* buf)
{
	Q_syscall(CG_UI_MENU_OPENBYNAME, buf);
}

void cgi_UI_Menu_Reset()
{
	Q_syscall(CG_UI_MENU_RESET);
}

void cgi_UI_Menu_New(char* buf)
{
	Q_syscall(CG_UI_MENU_NEW, buf);
}

void cgi_UI_Parse_Int(int* value)
{
	Q_syscall(CG_UI_PARSE_INT, value);
}

void cgi_UI_Parse_String(char* buf)
{
	Q_syscall(CG_UI_PARSE_STRING, buf);
}

void cgi_UI_Parse_Float(float* value)
{
	Q_syscall(CG_UI_PARSE_FLOAT, value);
}

int cgi_UI_StartParseSession(char* menuFile, char** buf)
{
	return Q_syscall(CG_UI_STARTPARSESESSION, menuFile, buf);
}

void cgi_UI_EndParseSession(char* buf)
{
	Q_syscall(CG_UI_ENDPARSESESSION, buf);
}

void cgi_UI_ParseExt(char** token)
{
	Q_syscall(CG_UI_PARSEEXT, token);
}

void cgi_UI_MenuCloseAll()
{
	Q_syscall(CG_UI_MENUCLOSE_ALL);
}

void cgi_UI_MenuPaintAll()
{
	Q_syscall(CG_UI_MENUPAINT_ALL);
}

void cgi_UI_String_Init()
{
	Q_syscall(CG_UI_STRING_INIT);
}

int cgi_UI_GetMenuInfo(char* menuFile, int* x, int* y, int* w, int* h)
{
	return Q_syscall(CG_UI_GETMENUINFO, menuFile, x, y, w, h);
}

int cgi_UI_GetMenuItemInfo(const char* menuFile, const char* itemName, int* x, int* y, int* w, int* h, vec4_t color,
	qhandle_t* background)
{
	return Q_syscall(CG_UI_GETITEMINFO, menuFile, itemName, x, y, w, h, color, background);
}

int cgi_UI_GetItemText(char* menuFile, char* itemName, char* text)
{
	return Q_syscall(CG_UI_GETITEMTEXT, menuFile, itemName, text);
}

int cgi_SP_GetStringTextString(const char* text, char* buffer, const int buffer_length)
{
	return Q_syscall(CG_SP_GETSTRINGTEXTSTRING, text, buffer, buffer_length);
}

/*
OpenJK Add
Since the modules are incompatible, might as well break base compat even further amirite?
*/

void* cgi_UI_GetMenuByName(const char* menu)
{
	return reinterpret_cast<void*>(Q_syscall(CG_OPENJK_GETMENU_BYNAME, menu));
}

void cgi_UI_Menu_Paint(void* menu, const qboolean force)
{
	Q_syscall(CG_OPENJK_MENU_PAINT, menu, force);
}