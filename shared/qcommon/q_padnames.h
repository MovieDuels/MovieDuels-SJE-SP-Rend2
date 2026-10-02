/*
===========================================================================
Controller button names, as printed on the controller in use

The input code (shared/sdl/sdl_input.cpp) works out which kind of controller is in use and puts it in the
read only cvar in_controllerType:
	"xbox"        Xbox One / Series (and other pads that are Xbox ones to SDL)
	"xbox360"     Xbox 360
	"playstation" PS4 (and PS3)
	"ps5"         PS5 (DualSense)
	"switch"      Nintendo Switch Pro controller / Joy-Cons
	"generic"     another gamepad SDL knows how to read
	"joystick"    a stick SDL has no gamepad layout for (its buttons are the JOY keys)
	""            none
Pad_ButtonName gives a key's name for the menus (the controls menu): the config files keep the normal key
names (PAD-A, PAD_START, ...), so binds work with any controller.

Include after the keycodes (A_PAD0_*, A_JOY*): client.h / keys.h.
===========================================================================
*/

#pragma once

// the A_PAD0_* keys in order: A_PAD0_A .. A_PAD0_RIGHTTRIGGER
constexpr int PAD_NAME_COUNT = 26;
static_assert(A_PAD0_RIGHTTRIGGER - A_PAD0_A + 1 == PAD_NAME_COUNT, "the pad keys changed: update the name tables");

// SDL's buttons by position, as the Xbox pad has them (A bottom, B right, X left, Y top)
static const char* const pad_names_xbox[PAD_NAME_COUNT] = {
	"A", "B", "X", "Y", "View", "Xbox", "Menu", "L3", "R3", "LB", "RB",
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Share",
	"L Left", "L Right", "L Up", "L Down", "R Left", "R Right", "R Up", "R Down", "LT", "RT"
};

static const char* const pad_names_xbox360[PAD_NAME_COUNT] = {
	"A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3", "LB", "RB",
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Misc",
	"L Left", "L Right", "L Up", "L Down", "R Left", "R Right", "R Up", "R Down", "LT", "RT"
};

static const char* const pad_names_ps4[PAD_NAME_COUNT] = {
	"Cross", "Circle", "Square", "Triangle", "Share", "PS", "Options", "L3", "R3", "L1", "R1",
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Misc",
	"L Left", "L Right", "L Up", "L Down", "R Left", "R Right", "R Up", "R Down", "L2", "R2"
};

static const char* const pad_names_ps5[PAD_NAME_COUNT] = {
	"Cross", "Circle", "Square", "Triangle", "Create", "PS", "Options", "L3", "R3", "L1", "R1",
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Mic",
	"L Left", "L Right", "L Up", "L Down", "R Left", "R Right", "R Up", "R Down", "L2", "R2"
};

// SDL reports the Nintendo buttons by their labels (A right, B bottom, X top, Y left)
static const char* const pad_names_switch[PAD_NAME_COUNT] = {
	"A", "B", "X", "Y", "-", "Home", "+", "L Stick", "R Stick", "L", "R",
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Capture",
	"L Left", "L Right", "L Up", "L Down", "R Left", "R Right", "R Up", "R Down", "ZL", "ZR"
};

static const char* const pad_names_generic[PAD_NAME_COUNT] = {
	"A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3", "LB", "RB",
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Misc",
	"L Left", "L Right", "L Up", "L Down", "R Left", "R Right", "R Up", "R Down", "LT", "RT"
};

// A PlayStation pad SDL has no gamepad layout for (a PS5 pad with SDL 2.0.12) comes in as a plain joystick:
// its buttons are JOY1.. (button 0 is JOY1) in the DirectInput order, its D-pad is the hat (JOY28..31) and its
// right stick axes 2 and 5 (JOY16/17, JOY22/23; the left stick is the cursor keys).
constexpr int PAD_RAW_SONY_COUNT = 15;
static const char* const pad_names_raw_sony[PAD_RAW_SONY_COUNT] = {
	"Square", "Cross", "Circle", "Triangle", "L1", "R1", "L2", "R2", "Share", "Options", "L3", "R3", "PS", "Touchpad", "Mic"
};

// keynum's name on a controller of the given in_controllerType; nullptr when there is none (not a controller
// button, or no names for this controller): then the key's normal name is shown
static inline const char* Pad_ButtonName(const int keynum, const char* type)
{
	if (!type || !type[0])
	{
		return nullptr;
	}
	const bool sony = !Q_stricmp(type, "ps5") || !Q_stricmp(type, "playstation");

	if (keynum >= A_PAD0_A && keynum < A_PAD0_A + PAD_NAME_COUNT)
	{
		const char* const* names;
		if (!Q_stricmp(type, "xbox"))
			names = pad_names_xbox;
		else if (!Q_stricmp(type, "xbox360"))
			names = pad_names_xbox360;
		else if (!Q_stricmp(type, "ps5"))
			names = pad_names_ps5;
		else if (sony)
			names = pad_names_ps4;
		else if (!Q_stricmp(type, "switch"))
			names = pad_names_switch;
		else if (!Q_stricmp(type, "generic"))
			names = pad_names_generic;
		else
			return nullptr;
		return names[keynum - A_PAD0_A];
	}

	if (!sony)
	{
		return nullptr;
	}
	if (keynum >= A_JOY1 && keynum < A_JOY1 + PAD_RAW_SONY_COUNT)
	{
		const char* name = pad_names_raw_sony[keynum - A_JOY1];
		if (keynum == A_JOY9 && !Q_stricmp(type, "ps5"))
		{
			name = "Create";
		}
		return name;
	}
	switch (keynum)
	{
	case A_JOY16: return "R Left";
	case A_JOY17: return "R Right";
	case A_JOY22: return "R Up";
	case A_JOY23: return "R Down";
	case A_JOY28: return "D-Pad Up";
	case A_JOY29: return "D-Pad Right";
	case A_JOY30: return "D-Pad Down";
	case A_JOY31: return "D-Pad Left";
	default: return nullptr;
	}
}
