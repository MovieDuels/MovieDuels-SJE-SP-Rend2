/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "../server/exe_headers.h"

#include "client.h"
#include "client_ui.h"

extern console_t con;

qboolean scr_initialized; // ready to draw

cvar_t* cl_timegraph;
cvar_t* cl_debuggraph;
cvar_t* cl_graphheight;
cvar_t* cl_graphscale;
cvar_t* cl_graphshift;
extern cvar_t* cl_com_outcast;

extern cvar_t* cl_com_rend2;

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic(const float x, const float y, const float width, const float height, const char* picname)
{
	assert(width != 0);

	const qhandle_t hShader = re.RegisterShader(picname);
	re.DrawStretchPic(x, y, width, height, 0, 0, 1, 1, hShader);
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect(const float x, const float y, const float width, const float height, const float* color)
{
	re.SetColor(color);

	re.DrawStretchPic(x, y, width, height, 0, 0, 0, 0, cls.whiteShader);

	re.SetColor(nullptr);
}

/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
=================
*/
void SCR_DrawPic(const float x, const float y, const float width, const float height, const qhandle_t hShader)
{
	re.DrawStretchPic(x, y, width, height, 0, 0, 1, 1, hShader);
}

/*
** SCR_DrawBigChar
** big chars are drawn at 640*480 virtual screen size
*/
void SCR_DrawBigChar(const int x, const int y, int ch)
{
	ch &= 255;

	if (ch == ' ')
	{
		return;
	}

	if (y < -BIGCHAR_HEIGHT)
	{
		return;
	}

	const float ax = x;
	const float ay = y;
	constexpr float aw = BIGCHAR_WIDTH;
	constexpr float ah = BIGCHAR_HEIGHT;

	const int row = ch >> 4;
	const int col = ch & 15;

	const float frow = row * 0.0625;
	const float fcol = col * 0.0625;
	constexpr float size = 0.03125;
	constexpr float size2 = 0.0625;

	re.DrawStretchPic(ax, ay, aw, ah,
		fcol, frow,
		fcol + size, frow + size2,
		cls.charSetShader);
}

/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar(const int x, const int y, int ch)
{
	ch &= 255;

	if (ch == ' ')
	{
		return;
	}

	if (y < -SMALLCHAR_HEIGHT)
	{
		return;
	}

	const int row = ch >> 4;
	const int col = ch & 15;
	/*
		frow = row*0.0625;
		fcol = col*0.0625;
		size = 0.0625;

		re.DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT,
						   fcol, frow,
						   fcol + size, frow + size,
						   cls.charSetShader );
	*/

	const float frow = row * 0.0625;
	const float fcol = col * 0.0625;
	constexpr float size = 0.03125;
	constexpr float size2 = 0.0625;

	re.DrawStretchPic(x * con.xadjust, y * con.yadjust,
		SMALLCHAR_WIDTH * con.xadjust, SMALLCHAR_HEIGHT * con.yadjust,
		fcol, frow,
		fcol + size, frow + size2,
		cls.charSetShader);
}

/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawBigStringExt(const int x, const int y, const char* string, const float* setColor, const qboolean forceColor,
	const qboolean noColorEscape)
{
	vec4_t color{};

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0;
	color[3] = setColor[3];
	re.SetColor(color);
	const char* s = string;
	int xx = x;
	while (*s)
	{
		if (!noColorEscape && Q_IsColorString(s))
		{
			s += 2;
			continue;
		}
		SCR_DrawBigChar(xx + 2, y + 2, *s);
		xx += BIGCHAR_WIDTH;
		s++;
	}

	// draw the colored text
	s = string;
	xx = x;
	re.SetColor(setColor);
	while (*s)
	{
		if (Q_IsColorString(s))
		{
			if (!forceColor)
			{
				memcpy(color, g_color_table[ColorIndex(*(s + 1))], sizeof(color));
				color[3] = setColor[3];
				re.SetColor(color);
			}
			if (!noColorEscape)
			{
				s += 2;
				continue;
			}
		}
		SCR_DrawBigChar(xx, y, *s);
		xx += BIGCHAR_WIDTH;
		s++;
	}
	re.SetColor(nullptr);
}

void SCR_DrawBigString(const int x, const int y, const char* s, const float alpha, const qboolean noColorEscape)
{
	float color[4]{};

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawBigStringExt(x, y, s, color, qfalse, noColorEscape);
}

void SCR_DrawBigStringColor(const int x, const int y, const char* s, vec4_t color, const qboolean noColorEscape)
{
	SCR_DrawBigStringExt(x, y, s, color, qtrue, noColorEscape);
}

/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.
==================
*/
void SCR_DrawSmallStringExt(const int x, const int y, const char* string, const float* setColor, const qboolean forceColor,
	const qboolean noColorEscape)
{
	vec4_t color;

	// draw the colored text
	const char* s = string;
	int xx = x;
	re.SetColor(setColor);
	while (*s)
	{
		if (Q_IsColorString(s))
		{
			if (!forceColor)
			{
				memcpy(color, g_color_table[ColorIndex(*(s + 1))], sizeof(color));
				color[3] = setColor[3];
				re.SetColor(color);
			}
			if (!noColorEscape)
			{
				s += 2;
				continue;
			}
		}
		SCR_DrawSmallChar(xx, y, *s);
		xx += SMALLCHAR_WIDTH;
		s++;
	}
	re.SetColor(nullptr);
}

/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen(const char* str)
{
	const char* s = str;
	int count = 0;

	while (*s)
	{
		if (Q_IsColorString(s))
		{
			s += 2;
		}
		else
		{
			count++;
			s++;
		}
	}

	return count;
}

/*
** SCR_GetBigStringWidth
*/
int SCR_GetBigStringWidth(const char* str)
{
	return SCR_Strlen(str) * BIGCHAR_WIDTH;
}

//===============================================================================

/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/
using graphsamp_t = struct
{
	float value;
	int color;
};

static int current;
static graphsamp_t values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph(const float value, const int color)
{
	values[current & 1023].value = value;
	values[current & 1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph()
{
	//
	// draw the graph
	//
	const int w = cls.glconfig.vidWidth;
	constexpr int x = 0;
	const int y = cls.glconfig.vidHeight;
	re.SetColor(g_color_table[0]);
	re.DrawStretchPic(x, y - cl_graphheight->integer,
		w, cl_graphheight->integer, 0, 0, 0, 0, 0);
	re.SetColor(nullptr);

	for (int a = 0; a < w; a++)
	{
		const int i = (current - 1 - a + 1024) & 1023;
		float v = values[i].value;
		v = v * cl_graphscale->integer + cl_graphshift->integer;

		if (v < 0)
			v += cl_graphheight->integer * (1 + static_cast<int>(-v / cl_graphheight->integer));
		const int h = static_cast<int>(v) % cl_graphheight->integer;
		re.DrawStretchPic(x + w - 1 - a, y - h, 1, h, 0, 0, 0, 0, 0);
	}
}

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init()
{
	cl_timegraph = Cvar_Get("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get("graphshift", "0", CVAR_CHEAT);

	scr_initialized = qtrue;
}

//=======================================================

void UI_SetActiveMenu(const char* menuname, const char* menuID);
void _UI_Refresh(int realtime);
void UI_DrawConnect(const char* servername, const char* updateInfoString);
extern cvar_t* cl_com_outcast;

extern cvar_t* cl_com_rend2;

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField(const stereoFrame_t stereoFrame)
{
	re.BeginFrame(stereoFrame);

	const qboolean uiFullscreen = _UI_IsFullscreen();

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	if (!uiFullscreen)
	{
		switch (cls.state)
		{
		default:
			Com_Error(ERR_FATAL, "SCR_DrawScreenField: bad cls.state");
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			UI_SetActiveMenu("mainMenu", nullptr);
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			UI_DrawConnect(clc.servername, cls.updateInfoString);
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			CL_CGameRendering(stereoFrame);
			break;
		case CA_ACTIVE:
			if (CL_IsRunningInGameCinematic() || CL_InGameCinematicOnStandBy())
			{
				SCR_DrawCinematic();
			}
			else
			{
				CL_CGameRendering(stereoFrame);
			}
			break;
		}
	}

	re.ProcessDissolve();

	// draw downloading progress bar

	// the menu draws next
	_UI_Refresh(cls.realtime);

	// console draws next
	Con_DrawConsole();

	// debug graph can be drawn on top of anything
	if (cl_debuggraph->integer || cl_timegraph->integer)
	{
		SCR_DrawDebugGraph();
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen()
{
	static int recursive;

	if (!scr_initialized)
	{
		return; // not initialized yet
	}

	// load the ref / ui / cgame if needed
	CL_StartHunkUsers();

	if (++recursive > 2)
	{
		Com_Error(ERR_FATAL, "SCR_UpdateScreen: recursively called");
	}
	recursive = qtrue;

	// If there is no VM, there are also no rendering commands issued. Stop the renderer in
	// that case.
	if (cls.uiStarted)
	{
		// if running in stereo, we need to draw the frame twice
		if (cls.glconfig.stereoEnabled)
		{
			SCR_DrawScreenField(STEREO_LEFT);
			SCR_DrawScreenField(STEREO_RIGHT);
		}
		else
		{
			SCR_DrawScreenField(STEREO_CENTER);
		}

		if (com_speeds->integer)
		{
			re.EndFrame(&time_frontend, &time_backend);
		}
		else
		{
			re.EndFrame(nullptr, nullptr);
		}
	}

	recursive = 0;
}

// this stuff is only used by the savegame (SG) code for screenshots...
//

static byte bScreenData[SG_SCR_WIDTH * SG_SCR_HEIGHT * 4];
static qboolean screenDataValid = qfalse;

void SCR_UnprecacheScreenshot()
{
	screenDataValid = qfalse;
}

void SCR_PrecacheScreenshot()
{
	// No screenshots unless connected to single player local server...
	//
	//	char *psInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	//	int iMaxClients = atoi(Info_ValueForKey( psInfo, "sv_maxclients" ));

	// (no need to check single-player status in voyager, this code base is all singleplayer)
	if (cls.state != CA_ACTIVE)
	{
		return;
	}

	if (!Key_GetCatcher())
	{
		// in-game...
		//
		//		SCR_UnprecacheScreenshot();
		//		pbScreenData = (byte *)Z_Malloc(SG_SCR_WIDTH * SG_SCR_HEIGHT * 4);
		S_ClearSoundBuffer(); // clear DMA etc because the following glReadPixels() call can take ages
		re.GetScreenShot(reinterpret_cast<byte*>(&bScreenData), SG_SCR_WIDTH, SG_SCR_HEIGHT);
		screenDataValid = qtrue;
	}
}

byte* SCR_GetScreenshot(qboolean* qValid)
{
	if (!screenDataValid)
	{
		SCR_PrecacheScreenshot();
	}
	if (qValid)
	{
		*qValid = screenDataValid;
	}
	return reinterpret_cast<byte*>(&bScreenData);
}

// called from save-game code to set the lo-res loading screen to be the one from the save file...
//
void SCR_SetScreenshot(const byte* pbData, const int w, const int h)
{
	if (w == SG_SCR_WIDTH && h == SG_SCR_HEIGHT)
	{
		screenDataValid = qtrue;
		memcpy(&bScreenData, pbData, SG_SCR_WIDTH * SG_SCR_HEIGHT * 4);
	}
	else
	{
		screenDataValid = qfalse;
		memset(&bScreenData, 0, SG_SCR_WIDTH * SG_SCR_HEIGHT * 4);
	}
}

#ifdef JK2_MODE
// This is just a client-side wrapper for the function RE_TempRawImage_ReadFromFile() in the renderer code...
//

byte* SCR_TempRawImage_ReadFromFile(const char* psLocalFilename, int* piWidth, int* piHeight, byte* pbReSampleBuffer, qboolean qbVertFlip)
{
	return re.TempRawImage_ReadFromFile(psLocalFilename, piWidth, piHeight, pbReSampleBuffer, qbVertFlip);
}
//
// ditto (sort of)...
//
void  SCR_TempRawImage_CleanUp()
{
	re.TempRawImage_CleanUp();
}
#endif