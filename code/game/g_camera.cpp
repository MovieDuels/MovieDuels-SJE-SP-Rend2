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

//g_camera.cpp
#include "g_local.h"

void SP_misc_camera_focus(gentity_t* self)
{
	if (!self->targetname)
	{
		gi.Printf(S_COLOR_RED"ERROR: misc_camera_focus with no targetname\n");
		G_FreeEntity(self);
		return;
	}
	self->speed = 0;
	self->script_targetname = G_NewString(self->targetname);
}

void SP_misc_camera_track(gentity_t* self)
{
	if (!self->targetname || !self->targetname[0])
	{
		gi.Printf(S_COLOR_RED"ERROR: misc_camera_track with no targetname\n");
		G_FreeEntity(self);
		return;
	}

	self->script_targetname = G_NewString(self->targetname);
}