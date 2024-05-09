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

// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"

cvar_t* cvar_vars = nullptr;
cvar_t* cvar_cheats;
int cvar_modifiedFlags;

#define	MAX_CVARS	8192
cvar_t cvar_indexes[MAX_CVARS];
int cvar_numIndexes;

#define FILE_HASH_SIZE		512
static cvar_t* hashTable[FILE_HASH_SIZE];
static qboolean cvar_sort = qfalse;

static char* lastMemPool = nullptr;
static int memPoolSize;

//If the string came from the memory pool, don't really free it.  The entire
//memory pool will be wiped during the next level load.
static void Cvar_FreeString(char* string)
{
	if (!lastMemPool || string < lastMemPool ||
		string >= lastMemPool + memPoolSize)
	{
		Z_Free(string);
	}
}

/*
================
return a hash value for the filename
================
*/
static long generateHashValue(const char* fname)
{
	long hash = 0;
	int i = 0;
	while (fname[i] != '\0')
	{
		const char letter = tolower(static_cast<unsigned char>(fname[i]));
		hash += static_cast<long>(letter) * (i + 119);
		i++;
	}
	hash &= (FILE_HASH_SIZE - 1);
	return hash;
}

/*
============
Cvar_ValidateString
============
*/
static qboolean Cvar_ValidateString(const char* s)
{
	if (!s)
	{
		return qfalse;
	}
	if (strchr(s, '\\'))
	{
		return qfalse;
	}
	if (strchr(s, '\"'))
	{
		return qfalse;
	}
	if (strchr(s, ';'))
	{
		return qfalse;
	}
	return qtrue;
}

/*
============
Cvar_FindVar
============
*/
static cvar_t* Cvar_FindVar(const char* var_name)
{
	const long hash = generateHashValue(var_name);

	for (cvar_t* var = hashTable[hash]; var; var = var->hashNext)
	{
		if (!Q_stricmp(var_name, var->name))
		{
			return var;
		}
	}

	return nullptr;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(const char* var_name)
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
		return 0;
	return var->value;
}

/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableIntegerValue(const char* var_name)
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
		return 0;
	return var->integer;
}

/*
============
Cvar_VariableString
============
*/
char* Cvar_VariableString(const char* var_name)
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
		return "";
	return var->string;
}

/*
============
Cvar_VariableStringBuffer
============
*/
void Cvar_VariableStringBuffer(const char* var_name, char* buffer, const int bufsize)
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
	{
		*buffer = 0;
	}
	else
	{
		Q_strncpyz(buffer, var->string, bufsize);
	}
}

/*
============
Cvar_Flags
============
*/
int Cvar_Flags(const char* var_name)
{
	cvar_t* var;

	if (!(var = Cvar_FindVar(var_name)))
		return CVAR_NONEXISTENT;
	if (var->modified)
		return var->flags | CVAR_MODIFIED;
	return var->flags;
}

/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion(const callbackFunc_t callback)
{
	for (const cvar_t* cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if ((cvar->flags & CVAR_CHEAT) && !cvar_cheats->integer)
		{
			continue;
		}
		callback(cvar->name);
	}
}

/*
============
Cvar_Validate
============
*/
static const char* Cvar_Validate(const cvar_t* var, const char* value, const qboolean warn)
{
	float valuef;
	qboolean changed = qfalse;

	if (!var->validate)
		return value;

	if (!value)
		return value;

	if (Q_isanumber(value))
	{
		valuef = atof(value);

		if (var->integral)
		{
			if (!Q_isintegral(valuef))
			{
				if (warn)
					Com_Printf("WARNING: cvar '%s' must be integral", var->name);

				valuef = static_cast<int>(valuef);
				changed = qtrue;
			}
		}
	}
	else
	{
		if (warn)
			Com_Printf("WARNING: cvar '%s' must be numeric", var->name);

		valuef = atof(var->resetString);
		changed = qtrue;
	}

	if (valuef < var->min)
	{
		if (warn)
		{
			if (changed)
				Com_Printf(" and is");
			else
				Com_Printf("WARNING: cvar '%s'", var->name);

			if (Q_isintegral(var->min))
				Com_Printf(" out of range (min %d)", static_cast<int>(var->min));
			else
				Com_Printf(" out of range (min %f)", var->min);
		}

		valuef = var->min;
		changed = qtrue;
	}
	else if (valuef > var->max)
	{
		if (warn)
		{
			if (changed)
				Com_Printf(" and is");
			else
				Com_Printf("WARNING: cvar '%s'", var->name);

			if (Q_isintegral(var->max))
				Com_Printf(" out of range (max %d)", static_cast<int>(var->max));
			else
				Com_Printf(" out of range (max %f)", var->max);
		}

		valuef = var->max;
		changed = qtrue;
	}

	if (changed)
	{
		static char s[MAX_CVAR_VALUE_STRING];
		if (Q_isintegral(valuef))
		{
			Com_sprintf(s, sizeof(s), "%d", static_cast<int>(valuef));

			if (warn)
				Com_Printf(", setting to %d\n", static_cast<int>(valuef));
		}
		else
		{
			Com_sprintf(s, sizeof(s), "%f", valuef);

			if (warn)
				Com_Printf(", setting to %f\n", valuef);
		}

		return s;
	}
	return value;
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t* Cvar_Get(const char* var_name, const char* var_value, int flags)
{
	cvar_t* var;
	long hash;
	int index;

	if (!var_name || !var_value)
	{
		Com_Error(ERR_FATAL, "Cvar_Get: NULL parameter");
	}

	if (!Cvar_ValidateString(var_name))
	{
		Com_Printf("invalid cvar name string: %s\n", var_name);
		var_name = "BADNAME";
	}

#if 0		// FIXME: values with backslash happen
	if (!Cvar_ValidateString(var_value)) {
		Com_Printf("invalid cvar value string: %s\n", var_value);
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar(var_name);
	if (var)
	{
		var_value = Cvar_Validate(var, var_value, qfalse);

		// Make sure the game code cannot mark engine-added variables as gamecode vars
		if (var->flags & CVAR_VM_CREATED)
		{
			if (!(flags & CVAR_VM_CREATED))
				var->flags &= ~CVAR_VM_CREATED;
		}
		else if (!(var->flags & CVAR_USER_CREATED))
		{
			if (flags & CVAR_VM_CREATED)
				flags &= ~CVAR_VM_CREATED;
		}

		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value
		if (var->flags & CVAR_USER_CREATED)
		{
			var->flags &= ~CVAR_USER_CREATED;
			Cvar_FreeString(var->resetString);
			var->resetString = CopyString(var_value);

			if (flags & CVAR_ROM)
			{
				// this variable was set by the user,
				// so force it to value given by the engine.

				if (var->latchedString)
					Cvar_FreeString(var->latchedString);

				var->latchedString = CopyString(var_value);
			}
		}

		// Make sure servers cannot mark engine-added variables as SERVER_CREATED
		if (var->flags & CVAR_SERVER_CREATED)
		{
			if (!(flags & CVAR_SERVER_CREATED))
				var->flags &= ~CVAR_SERVER_CREATED;
		}
		else
		{
			if (flags & CVAR_SERVER_CREATED)
				flags &= ~CVAR_SERVER_CREATED;
		}

		var->flags |= flags;

		// only allow one non-empty reset string without a warning
		if (!var->resetString[0])
		{
			// we don't have a reset string yet
			Cvar_FreeString(var->resetString);
			var->resetString = CopyString(var_value);
		}
		else if (var_value[0] && strcmp(var->resetString, var_value) != 0)
		{
			Com_DPrintf(S_COLOR_YELLOW "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
				var_name, var->resetString, var_value);
		}
		// if we have a latched string, take that value now
		if (var->latchedString)
		{
			char* s = var->latchedString;
			var->latchedString = nullptr; // otherwise cvar_set2 would free it
			Cvar_Set2(var_name, s, qtrue);
			Cvar_FreeString(s);
		}

		// ZOID--needs to be set so that cvars the game sets as
		// SERVERINFO get sent to clients
		cvar_modifiedFlags |= flags;

		return var;
	}

	//
	// allocate a new cvar
	//

	// find a free cvar
	for (index = 0; index < MAX_CVARS; index++)
	{
		if (!cvar_indexes[index].name)
			break;
	}

	if (index >= MAX_CVARS)
	{
		if (!com_errorEntered)
			Com_Error(ERR_FATAL, "Error: Too many cvars, cannot create a new one!");

		return nullptr;
	}

	var = &cvar_indexes[index];

	if (index >= cvar_numIndexes)
		cvar_numIndexes = index + 1;

	var->name = CopyString(var_name);
	var->string = CopyString(var_value);
	var->modified = qtrue;
	var->modificationCount = 1;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->resetString = CopyString(var_value);
	var->validate = qfalse;

	// link the variable in
	var->next = cvar_vars;
	if (cvar_vars)
		cvar_vars->prev = var;

	var->prev = nullptr;
	cvar_vars = var;

	var->flags = flags;
	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	hash = generateHashValue(var_name);
	var->hashIndex = hash;

	var->hashNext = hashTable[hash];
	if (hashTable[hash])
		hashTable[hash]->hashPrev = var;

	var->hashPrev = nullptr;
	hashTable[hash] = var;

	// sort on write
	cvar_sort = qtrue;

	return var;
}

static void Cvar_QSortByName(cvar_t** a, const int n)
{
	int i = 0;
	int j = n;
	const cvar_t* m = a[n >> 1];

	do
	{
		// sort in descending order
		while (strcmp(a[i]->name, m->name) > 0) i++;
		while (strcmp(a[j]->name, m->name) < 0) j--;

		if (i <= j)
		{
			cvar_t* temp = a[i];
			a[i] = a[j];
			a[j] = temp;
			i++;
			j--;
		}
	} while (i <= j);

	if (j > 0) Cvar_QSortByName(a, j);
	if (n > i) Cvar_QSortByName(a + i, n - i);
}

static void Cvar_Sort()
{
	cvar_t* list[MAX_CVARS]{}, * var;
	int count;

	for (count = 0, var = cvar_vars; var; var = var->next)
	{
		if (var->name)
		{
			list[count++] = var;
		}
		else
		{
			Com_Error(ERR_FATAL, "Cvar_Sort: NULL cvar name");
		}
	}

	if (count < 2)
	{
		return; // nothing to sort
	}

	Cvar_QSortByName(&list[0], count - 1);

	cvar_vars = nullptr;

	// relink cvars
	for (int i = 0; i < count; i++)
	{
		var = list[i];
		// link the variable in
		var->next = cvar_vars;
		if (cvar_vars)
			cvar_vars->prev = var;
		var->prev = nullptr;
		cvar_vars = var;
	}
}

/*
============
Cvar_Print

Prints the value, default, and latched string of the given variable
============
*/
static void Cvar_Print(const cvar_t* v)
{
	Com_Printf(
		S_COLOR_GREY "Cvar " S_COLOR_WHITE "%s = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE,
		v->name, v->string);

	if (!(v->flags & CVAR_ROM))
	{
		if (!Q_stricmp(v->string, v->resetString))
			Com_Printf(", " S_COLOR_WHITE "the default");
		else
			Com_Printf(
				", " S_COLOR_WHITE "default = " S_COLOR_GREY"\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE,
				v->resetString);
	}

	Com_Printf("\n");

	if (v->latchedString)
		Com_Printf("     latched = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"\n", v->latchedString);
}

/*
============
Cvar_Set2
============
*/
cvar_t* Cvar_Set2(const char* var_name, const char* value, const qboolean force)
{
	cvar_t* var;

	//Com_DPrintf( "Cvar_Set2: %s %s\n", var_name, value );

	if (!Cvar_ValidateString(var_name))
	{
		Com_Printf("invalid cvar name string: %s\n", var_name);
		var_name = "BADNAME";
	}

#if 0	// FIXME
	if (value && !Cvar_ValidateString(value)) {
		Com_Printf("invalid cvar value string: %s\n", value);
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar(var_name);
	if (!var)
	{
		if (!value)
		{
			return nullptr;
		}
		// create it
		if (!force)
		{
			return Cvar_Get(var_name, value, CVAR_USER_CREATED);
		}
		return Cvar_Get(var_name, value, 0);
	}

	if (!value)
	{
		value = var->resetString;
	}

	value = Cvar_Validate(var, value, qtrue);

	if ((var->flags & CVAR_LATCH) && var->latchedString)
	{
		if (strcmp(value, var->string) == 0)
		{
			Cvar_FreeString(var->latchedString);
			var->latchedString = nullptr;
			return var;
		}

		if (strcmp(value, var->latchedString) == 0)
			return var;
	}
	else if (strcmp(value, var->string) == 0)
		return var;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	if (!force)
	{
		if (var->flags & CVAR_ROM)
		{
			Com_Printf("%s is read only.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_INIT)
		{
			Com_Printf("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latchedString)
			{
				if (strcmp(value, var->latchedString) == 0)
					return var;
				Cvar_FreeString(var->latchedString);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			Com_Printf("%s will be changed upon restarting.\n", var_name);
			var->latchedString = CopyString(value);
			var->modified = qtrue;
			var->modificationCount++;
			return var;
		}

		if ((var->flags & CVAR_CHEAT) && !cvar_cheats->integer)
		{
			Com_Printf("%s is cheat protected.\n", var_name);
			return var;
		}
	}
	else
	{
		if (var->latchedString)
		{
			Cvar_FreeString(var->latchedString);
			var->latchedString = nullptr;
		}
	}

	if (strcmp(value, var->string) == 0)
		return var; // not changed

	var->modified = qtrue;
	var->modificationCount++;

	Cvar_FreeString(var->string); // free the old value string

	var->string = CopyString(value);
	var->value = atof(var->string);
	var->integer = atoi(var->string);

	return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set(const char* var_name, const char* value)
{
	Cvar_Set2(var_name, value, qtrue);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue(const char* var_name, const float value)
{
	char val[32];

	if (value == static_cast<int>(value))
	{
		Com_sprintf(val, sizeof(val), "%i", static_cast<int>(value));
	}
	else
	{
		Com_sprintf(val, sizeof(val), "%f", value);
	}
	Cvar_Set(var_name, val);
}

/*
============
Cvar_SetValue2
============
*/
static void Cvar_SetValue2(const char* var_name, const float value, const qboolean force)
{
	char val[32];

	if (Q_isintegral(value))
		Com_sprintf(val, sizeof(val), "%i", static_cast<int>(value));
	else
		Com_sprintf(val, sizeof(val), "%f", value);
	Cvar_Set2(var_name, val, force);
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset(const char* var_name)
{
	Cvar_Set2(var_name, nullptr, qfalse);
}

/*
============
Cvar_ForceReset
============
*/
void Cvar_ForceReset(const char* var_name)
{
	Cvar_Set2(var_name, nullptr, qtrue);
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState()
{
	// set all default vars to the safe value
	for (cvar_t* var = cvar_vars; var; var = var->next)
	{
		if (var->flags & CVAR_CHEAT)
		{
			// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here
			// because of a different var->latchedString
			if (var->latchedString)
			{
				Cvar_FreeString(var->latchedString);
				var->latchedString = nullptr;
			}
			if (strcmp(var->resetString, var->string) != 0)
			{
				Cvar_Set(var->name, var->resetString);
			}
		}
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command()
{
	// check variables
	const cvar_t* v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
	{
		return qfalse;
	}

	// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Cvar_Print(v);
		return qtrue;
	}

	// toggle
	if (strcmp(Cmd_Argv(1), "!") == 0)
	{
		// Swap the value if our command has ! in it (bind p "cg_thirdPeson !")
		Cvar_SetValue2(v->name, !v->value, qfalse);
		return qtrue;
	}

	// set the value if forcing isn't required
	Cvar_Set2(v->name, Cmd_Args(), qfalse);

	return qtrue;
}

/*
============
Cvar_Print_f

Prints the contents of a cvar
(preferred over Cvar_Command where cvar names and commands conflict)
============
*/
static void Cvar_Print_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("usage: print <variable>\n");
		return;
	}

	char* name = Cmd_Argv(1);

	const cvar_t* cv = Cvar_FindVar(name);

	if (cv)
		Cvar_Print(cv);
	else
		Com_Printf("Cvar %s does not exist.\n", name);
}

/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding, optionally through a list of
given values
============
*/
static void Cvar_Toggle_f()
{
	const int c = Cmd_Argc();

	if (c < 2)
	{
		Com_Printf("usage: toggle <variable> [value1, value2, ...]\n");
		return;
	}

	if (c == 2)
	{
		Cvar_Set2(Cmd_Argv(1), va("%d",
			!Cvar_VariableValue(Cmd_Argv(1))),
			qfalse);
		return;
	}

	if (c == 3)
	{
		Com_Printf("toggle: nothing to toggle to\n");
		return;
	}

	const char* curval = Cvar_VariableString(Cmd_Argv(1));

	// don't bother checking the last arg for a match since the desired
	// behaviour is the same as no match (set to the first argument)
	for (int i = 2; i + 1 < c; i++)
	{
		if (strcmp(curval, Cmd_Argv(i)) == 0)
		{
			Cvar_Set2(Cmd_Argv(1), Cmd_Argv(i + 1), qfalse);
			return;
		}
	}

	// fallback
	Cvar_Set2(Cmd_Argv(1), Cmd_Argv(2), qfalse);
}

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
static void Cvar_Set_f()
{
	const int c = Cmd_Argc();
	char* cmd = Cmd_Argv(0);

	if (c < 2)
	{
		Com_Printf("usage: %s <variable> <value>\n", cmd);
		return;
	}
	if (c == 2)
	{
		Cvar_Print_f();
		return;
	}

	cvar_t* v = Cvar_Set2(Cmd_Argv(1), Cmd_ArgsFrom(2), qfalse);
	if (!v)
	{
		return;
	}
	switch (cmd[3])
	{
	case 'a':
		if (!(v->flags & CVAR_ARCHIVE))
		{
			v->flags |= CVAR_ARCHIVE;
			cvar_modifiedFlags |= CVAR_ARCHIVE;
		}
		break;
	case 'u':
		if (!(v->flags & CVAR_USERINFO))
		{
			v->flags |= CVAR_USERINFO;
			cvar_modifiedFlags |= CVAR_USERINFO;
		}
		break;
	case 's':
		if (!(v->flags & CVAR_SERVERINFO))
		{
			v->flags |= CVAR_SERVERINFO;
			cvar_modifiedFlags |= CVAR_SERVERINFO;
		}
		break;
	default:;
	}
}

/*
============
Cvar_Reset_f
============
*/
static void Cvar_Reset_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset(Cmd_Argv(1));
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to qtrue.
============
*/
void Cvar_WriteVariables(const fileHandle_t f)
{
	if (cvar_sort)
	{
		Com_DPrintf("Cvar_Sort: sort cvars\n");
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	for (const cvar_t* var = cvar_vars; var; var = var->next)
	{
		if (!var->name || Q_stricmp(var->name, "cl_cdkey") == 0)
			continue;

		if (var->flags & CVAR_ARCHIVE)
		{
			char buffer[1024];
			// write the latched value, even if it hasn't taken effect yet
			if (var->latchedString)
			{
				if (strlen(var->name) + strlen(var->latchedString) + 10 > sizeof(buffer))
				{
					Com_Printf(S_COLOR_YELLOW "WARNING: value of variable "
						"\"%s\" too long to write to file\n", var->name);
					continue;
				}
				if ((var->flags & CVAR_NODEFAULT) && strcmp(var->latchedString, var->resetString) == 0)
				{
					continue;
				}
				Com_sprintf(buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->latchedString);
			}
			else
			{
				if (strlen(var->name) + strlen(var->string) + 10 > sizeof(buffer))
				{
					Com_Printf(S_COLOR_YELLOW "WARNING: value of variable "
						"\"%s\" too long to write to file\n", var->name);
					continue;
				}
				if ((var->flags & CVAR_NODEFAULT) && strcmp(var->string, var->resetString) == 0)
				{
					continue;
				}
				Com_sprintf(buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->string);
			}
			FS_Write(buffer, strlen(buffer), f);
		}
	}
}

/*
============
Cvar_List_f
============
*/
static void Cvar_List_f()
{
	const cvar_t* var;
	int i;
	const char* match = nullptr;

	if (Cmd_Argc() > 1)
		match = Cmd_Argv(1);

	for (var = cvar_vars, i = 0;
		var;
		var = var->next, i++)
	{
		if (!var->name || (match && !Com_Filter(match, var->name, qfalse)))
			continue;

		if (var->flags & CVAR_SERVERINFO) Com_Printf("S");
		else Com_Printf(" ");
		if (var->flags & CVAR_SYSTEMINFO) Com_Printf("s");
		else Com_Printf(" ");
		if (var->flags & CVAR_USERINFO) Com_Printf("U");
		else Com_Printf(" ");
		if (var->flags & CVAR_ROM) Com_Printf("R");
		else Com_Printf(" ");
		if (var->flags & CVAR_INIT) Com_Printf("I");
		else Com_Printf(" ");
		if (var->flags & CVAR_ARCHIVE) Com_Printf("A");
		else Com_Printf(" ");
		if (var->flags & CVAR_LATCH) Com_Printf("L");
		else Com_Printf(" ");
		if (var->flags & CVAR_CHEAT) Com_Printf("C");
		else Com_Printf(" ");
		if (var->flags & CVAR_USER_CREATED) Com_Printf("?");
		else Com_Printf(" ");

		Com_Printf(S_COLOR_WHITE " %s = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE,
			var->name, var->string);
		if (var->latchedString)
			Com_Printf(", latched = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE,
				var->latchedString);
		Com_Printf("\n");
	}

	Com_Printf("\n%i total cvars\n", i);
	if (i != cvar_numIndexes)
		Com_Printf("%i cvar indexes\n", cvar_numIndexes);
}

static void Cvar_SerenityJediEngine_f()
{
	Com_Printf("-----A basic guide to player debugging----------\n");
	Com_Printf("-----------------------------------------------\n");
	Com_Printf("open console and type seta\n");
	Com_Printf("--------------------------\n");
	Com_Printf("Use the following commands\n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("(d_blockinfo 1)= This will give information on saber blocking input \n");
	Com_Printf("(d_attackinfo 1)= This will give information on saber attacking input \n");
	Com_Printf("(d_saberinfo 1)= This will give information on saber blade input \n");
	Com_Printf("(d_SaberactionInfo 1)= This will give information on saber action input \n");
	Com_Printf("(d_saberCombat 1)= This will give information on saber Combat input \n");
	Com_Printf("(d_combatinfo 1)= This will give information on combatinfo input \n");
	Com_Printf("(d_JediAI 1)= This will give information on NPC input \n");
	Com_Printf("(g_DebugSaberCombat 1)= This will give  all information available \n");
	Com_Printf("-----------------------------------------------------------------\n");
	Com_Printf("-Use these commands to help locate bugs or specific moments in code-\n");
}

static void Cvar_ListModified_f()
{
	// build a list of cvars that are modified
	for (const cvar_t* var = cvar_vars;
		var;
		var = var->next)
	{
		char* value = var->latchedString ? var->latchedString : var->string;
		if (!var->name || !var->modificationCount || strcmp(value, var->resetString) == 0)
			continue;

		Com_Printf(S_COLOR_GREY "Cvar "
			S_COLOR_WHITE "%s = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE ", "
			S_COLOR_WHITE "default = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE "\n",
			var->name, value, var->resetString);
	}
}

static void Cvar_ListUserCreated_f()
{
	uint32_t count = 0;

	// build a list of cvars that are modified
	for (const cvar_t* var = cvar_vars;
		var;
		var = var->next)
	{
		char* value = var->latchedString ? var->latchedString : var->string;
		if (!(var->flags & CVAR_USER_CREATED))
			continue;

		Com_Printf(S_COLOR_GREY "Cvar "
			S_COLOR_WHITE "%s = " S_COLOR_GREY "\"" S_COLOR_WHITE "%s" S_COLOR_GREY "\"" S_COLOR_WHITE "\n",
			var->name, value);
		count++;
	}

	if (count > 0)
		Com_Printf(S_COLOR_GREY "Showing " S_COLOR_WHITE "%u" S_COLOR_GREY " user created cvars" S_COLOR_WHITE "\n",
			count);
	else
		Com_Printf(S_COLOR_GREY "No user created cvars" S_COLOR_WHITE "\n");
}

/*
============
Cvar_Unset

Unsets a cvar
============
*/

static cvar_t* Cvar_Unset(cvar_t* cv)
{
	cvar_t* next = cv->next;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= cv->flags;

	if (cv->name)
		Cvar_FreeString(cv->name);
	if (cv->string)
		Cvar_FreeString(cv->string);
	if (cv->latchedString)
		Cvar_FreeString(cv->latchedString);
	if (cv->resetString)
		Cvar_FreeString(cv->resetString);

	if (cv->prev)
		cv->prev->next = cv->next;
	else
		cvar_vars = cv->next;
	if (cv->next)
		cv->next->prev = cv->prev;

	if (cv->hashPrev)
		cv->hashPrev->hashNext = cv->hashNext;
	else
		hashTable[cv->hashIndex] = cv->hashNext;
	if (cv->hashNext)
		cv->hashNext->hashPrev = cv->hashPrev;

	memset(cv, 0, sizeof(*cv));

	return next;
}

/*
============
Cvar_Unset_f

Unsets a userdefined cvar
============
*/

static void Cvar_Unset_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <varname>\n", Cmd_Argv(0));
		return;
	}

	cvar_t* cv = Cvar_FindVar(Cmd_Argv(1));

	if (!cv)
		return;

	if (cv->flags & CVAR_USER_CREATED)
		Cvar_Unset(cv);
	else
		Com_Printf("Error: %s: Variable %s is not user created.\n", Cmd_Argv(0), cv->name);
}

static void Cvar_UnsetUserCreated_f()
{
	cvar_t* curvar = cvar_vars;
	uint32_t count = 0;

	while (curvar)
	{
		if ((curvar->flags & CVAR_USER_CREATED))
		{
			// throw out any variables the user created
			curvar = Cvar_Unset(curvar);
			count++;
			continue;
		}
		curvar = curvar->next;
	}

	if (count > 0)
		Com_Printf(S_COLOR_GREY "Removed " S_COLOR_WHITE "%u" S_COLOR_GREY " user created cvars" S_COLOR_WHITE "\n",
			count);
	else
		Com_Printf(S_COLOR_GREY "No user created cvars to remove" S_COLOR_WHITE "\n");
}

/*
============
Cvar_Restart

Resets all cvars to their hardcoded values and removes userdefined variables
and variables added via the VMs if requested.
============
*/

void Cvar_Restart(const qboolean unsetVM)
{
	cvar_t* curvar = cvar_vars;

	while (curvar)
	{
		if ((curvar->flags & CVAR_USER_CREATED) ||
			(unsetVM && (curvar->flags & CVAR_VM_CREATED)))
		{
			// throw out any variables the user/vm created
			curvar = Cvar_Unset(curvar);
			continue;
		}

		if (!(curvar->flags & (CVAR_ROM | CVAR_INIT | CVAR_NORESTART)))
		{
			// Just reset the rest to their default values.
			Cvar_Set2(curvar->name, curvar->resetString, qfalse);
		}

		curvar = curvar->next;
	}
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f()
{
	Cvar_Restart(qfalse);
}

/*
=====================
Cvar_InfoString
=====================
*/
char* Cvar_InfoString(const int bit)
{
	static char info[MAX_INFO_STRING];

	info[0] = 0;

	for (const cvar_t* var = cvar_vars; var; var = var->next)
	{
		if (var->name && (var->flags & bit))
		{
			Info_SetValueForKey(info, var->name, var->string);
		}
	}
	return info;
}

/*
=====================
Cvar_InfoStringBuffer
=====================
*/
void Cvar_InfoStringBuffer(const int bit, char* buff, const int buffsize)
{
	Q_strncpyz(buff, Cvar_InfoString(bit), buffsize);
}

/*
=====================
Cvar_CheckRange
=====================
*/
void Cvar_CheckRange(cvar_t* var, const float min_val, const float max, const qboolean integral)
{
	var->validate = qtrue;
	var->min = min_val;
	var->max = max;
	var->integral = integral;

	// Force an initial range check
	Cvar_Set(var->name, var->string);
}

/*
=====================
Cvar_Register

basically a slightly modified Cvar_Get for the interpreted modules
=====================
*/
void Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags)
{
	// There is code in Cvar_Get to prevent CVAR_ROM cvars being changed by the
	// user. In other words CVAR_ARCHIVE and CVAR_ROM are mutually exclusive
	// flags. Unfortunately some historical game code (including single player
	// baseq3) sets both flags. We unset CVAR_ROM for such cvars.
	if ((flags & (CVAR_ARCHIVE | CVAR_ROM)) == (CVAR_ARCHIVE | CVAR_ROM))
	{
		Com_DPrintf(S_COLOR_YELLOW "WARNING: Unsetting CVAR_ROM cvar '%s', "
			"since it is also CVAR_ARCHIVE\n", varName);
		flags &= ~CVAR_ROM;
	}

	const cvar_t* cv = Cvar_Get(varName, defaultValue, flags | CVAR_VM_CREATED);
	if (!vmCvar)
	{
		return;
	}
	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;
	Cvar_Update(vmCvar);
}

/*
=====================
Cvar_Update

updates an interpreted modules' version of a cvar
=====================
*/
void Cvar_Update(vmCvar_t* vmCvar)
{
	assert(vmCvar);

	if (static_cast<unsigned>(vmCvar->handle) >= static_cast<unsigned>(cvar_numIndexes))
	{
		Com_Error(ERR_DROP, "Cvar_Update: handle %u out of range", static_cast<unsigned>(vmCvar->handle));
	}

	const cvar_t* cv = cvar_indexes + vmCvar->handle;

	if (cv->modificationCount == vmCvar->modificationCount)
	{
		return;
	}
	if (!cv->string)
	{
		return; // variable might have been cleared by a cvar_restart
	}
	vmCvar->modificationCount = cv->modificationCount;
	if (strlen(cv->string) + 1 > MAX_CVAR_VALUE_STRING)
		Com_Error(ERR_DROP, "Cvar_Update: src %s length %u exceeds MAX_CVAR_VALUE_STRING",
			cv->string,
			strlen(cv->string));
	Q_strncpyz(vmCvar->string, cv->string, MAX_CVAR_VALUE_STRING);
	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}

/*
==================
Cvar_CompleteCvarName
==================
*/
void Cvar_CompleteCvarName(char* args, const int argNum)
{
	if (argNum == 2)
	{
		// Skip "<cmd> "
		char* p = Com_SkipTokens(args, 1, " ");

		if (p > args)
			Field_CompleteCommand(p, qfalse, qtrue);
	}
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init()
{
	memset(cvar_indexes, 0, sizeof(cvar_indexes));
	memset(hashTable, 0, sizeof(hashTable));

	cvar_cheats = Cvar_Get("helpUsObi", "0", CVAR_SYSTEMINFO);

	com_outcast = Cvar_Get("com_outcast", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	com_kotor = Cvar_Get("com_kotor", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	g_trueguns = Cvar_Get("cg_trueguns", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	g_Weather = Cvar_Get("r_weather", "0", CVAR_ARCHIVE);

	g_update6firststartup = Cvar_Get("g_update6firststartup", "1", 0);

	g_totgfirststartup = Cvar_Get("g_totgfirststartup", "1", 0);

	com_rend2 = Cvar_Get("com_rend2", "0", CVAR_ARCHIVE | CVAR_SAVEGAME | CVAR_NORESTART);

	Cmd_AddCommand("print", Cvar_Print_f);
	Cmd_SetCommandCompletionFunc("print", Cvar_CompleteCvarName);
	Cmd_AddCommand("toggle", Cvar_Toggle_f);
	Cmd_SetCommandCompletionFunc("toggle", Cvar_CompleteCvarName);
	Cmd_AddCommand("set", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc("set", Cvar_CompleteCvarName);
	Cmd_AddCommand("sets", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc("sets", Cvar_CompleteCvarName);
	Cmd_AddCommand("setu", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc("setu", Cvar_CompleteCvarName);
	Cmd_AddCommand("seta", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc("seta", Cvar_CompleteCvarName);
	Cmd_AddCommand("reset", Cvar_Reset_f);
	Cmd_SetCommandCompletionFunc("reset", Cvar_CompleteCvarName);
	Cmd_AddCommand("unset", Cvar_Unset_f);
	Cmd_SetCommandCompletionFunc("unset", Cvar_CompleteCvarName);
	Cmd_AddCommand("unset_usercreated", Cvar_UnsetUserCreated_f);
	Cmd_AddCommand("cvarlist", Cvar_List_f);
	Cmd_AddCommand("cvar_usercreated", Cvar_ListUserCreated_f);
	Cmd_AddCommand("cvar_modified", Cvar_ListModified_f);
	Cmd_AddCommand("cvar_restart", Cvar_Restart_f);
	Cmd_AddCommand("Debuginfo", Cvar_SerenityJediEngine_f);
}

static void Cvar_Realloc(char** string, char* memPool, int& memPoolUsed)
{
	if (string && *string)
	{
		char* temp = memPool + memPoolUsed;
		strcpy(temp, *string);
		memPoolUsed += strlen(*string) + 1;
		Cvar_FreeString(*string);
		*string = temp;
	}
}

//Turns many small allocation blocks into one big one.
void Cvar_Defrag()
{
	cvar_t* var;
	int totalMem = 0;

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->name)
		{
			totalMem += strlen(var->name) + 1;
		}
		if (var->string)
		{
			totalMem += strlen(var->string) + 1;
		}
		if (var->resetString)
		{
			totalMem += strlen(var->resetString) + 1;
		}
		if (var->latchedString)
		{
			totalMem += strlen(var->latchedString) + 1;
		}
	}

	const auto mem = static_cast<char*>(Z_Malloc(totalMem, TAG_SMALL, qfalse));
	const int nextMemPoolSize = totalMem;
	totalMem = 0;

	for (var = cvar_vars; var; var = var->next)
	{
		Cvar_Realloc(&var->name, mem, totalMem);
		Cvar_Realloc(&var->string, mem, totalMem);
		Cvar_Realloc(&var->resetString, mem, totalMem);
		Cvar_Realloc(&var->latchedString, mem, totalMem);
	}

	if (lastMemPool)
	{
		Z_Free(lastMemPool);
	}
	lastMemPool = mem;
	memPoolSize = nextMemPoolSize;
}