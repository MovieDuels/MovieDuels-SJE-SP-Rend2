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

// cmd.c -- Quake script command processing module

#include "q_shared.h"
#include "qcommon.h"

constexpr auto MAX_CMD_BUFFER = 128 * 1024;
constexpr auto MAX_CMD_LINE = 1024;

using cmd_t = struct cmd_s
{
	byte* data;
	int maxsize;
	int cursize;
};

int cmd_wait;
cmd_t cmd_text;
byte cmd_text_buf[MAX_CMD_BUFFER];

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
============
*/
static void Cmd_Wait_f()
{
	if (Cmd_Argc() == 2)
	{
		cmd_wait = atoi(Cmd_Argv(1));
		if (cmd_wait < 0)
			cmd_wait = 1; // ignore the argument
	}
	else
	{
		cmd_wait = 1;
	}
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void Cbuf_Init()
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = MAX_CMD_BUFFER;
	cmd_text.cursize = 0;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer, does NOT add a final \n
============
*/
void Cbuf_AddText(const char* text)
{
	const int l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}
	Com_Memcpy(&cmd_text.data[cmd_text.cursize], text, l);
	cmd_text.cursize += l;
}

/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
static void Cbuf_InsertText(const char* text)
{
	const int len = strlen(text) + 1;
	if (len + cmd_text.cursize > cmd_text.maxsize)
	{
		Com_Printf("Cbuf_InsertText overflowed\n");
		return;
	}

	// move the existing command text
	for (int i = cmd_text.cursize - 1; i >= 0; i--)
	{
		cmd_text.data[i + len] = cmd_text.data[i];
	}

	// copy the new text in
	Com_Memcpy(cmd_text.data, text, len - 1);

	// add a \n
	cmd_text.data[len - 1] = '\n';

	cmd_text.cursize += len;
}

/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText(const int exec_when, const char* text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		if (text && strlen(text) > 0)
		{
			Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", text);
			Cmd_ExecuteString(text);
		}
		else
		{
			Cbuf_Execute();
			Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", cmd_text.data);
		}
		break;
	case EXEC_INSERT:
		Cbuf_InsertText(text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText(text);
		break;
	default:
		Com_Error(ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute()
{
	int i;
	char line[MAX_CMD_LINE];

	// This will keep // style comments all on one line by not breaking on
	// a semicolon.  It will keep /* ... */ style comments all on one line by not
	// breaking it for semicolon or newline.
	qboolean in_star_comment = qfalse;
	qboolean in_slash_comment = qfalse;

	while (cmd_text.cursize)
	{
		if (cmd_wait > 0)
		{
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait--;
			break;
		}

		// find a \n or ; line break or comment: // or /* */
		const auto text = reinterpret_cast<char*>(cmd_text.data);

		int quotes = 0;
		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"')
				quotes++;

			if (!(quotes & 1))
			{
				if (i < cmd_text.cursize - 1)
				{
					if (!in_star_comment && text[i] == '/' && text[i + 1] == '/')
						in_slash_comment = qtrue;
					else if (!in_slash_comment && text[i] == '/' && text[i + 1] == '*')
						in_star_comment = qtrue;
					else if (in_star_comment && text[i] == '*' && text[i + 1] == '/')
					{
						in_star_comment = qfalse;
						// If we are in a star comment, then the part after it is valid
						// Note: This will cause it to NUL out the terminating '/'
						// but ExecuteString doesn't require it anyway.
						i++;
						break;
					}
				}
				if (!in_slash_comment && !in_star_comment && text[i] == ';')
					break;
			}
			if (!in_star_comment && (text[i] == '\n' || text[i] == '\r'))
			{
				in_slash_comment = qfalse;
				break;
			}
		}

		if (i >= (MAX_CMD_LINE - 1))
		{
			i = MAX_CMD_LINE - 1;
		}

		Com_Memcpy(line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec) can insert data at the
		// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		// execute the command line

		Cmd_ExecuteString(line);
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f(void)
{
	union
	{
		char* c;
		void* v;
	} f{};
	char filename[MAX_QPATH];

	const bool quiet = !Q_stricmp(Cmd_Argv(0), "execq");

	if (Cmd_Argc() != 2)
	{
		//Com_Printf("exec%s <filename> : execute a script file%s\n", quiet ? "q" : "", quiet ? " without notification" : "");
		return;
	}

	Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
	COM_DefaultExtension(filename, sizeof(filename), ".cfg");
	FS_ReadFile(filename, &f.v);
	if (!f.c)
	{
		Com_Printf("couldn't exec %s\n", filename);
		return;
	}
	if (!quiet)
		Com_Printf("execing %s\n", filename);

	Cbuf_InsertText(f.c);

	FS_FreeFile(f.v);
}

/*
===============
Cmd_Vstr_f

Inserts the current value of a variable as command text
===============
*/
static void Cmd_Vstr_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("vstr <variablename> : execute a variable command\n");
		return;
	}

	const char* v = Cvar_VariableString(Cmd_Argv(1));
	Cbuf_InsertText(va("%s\n", v));
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
static void Cmd_Echo_f()
{
	Com_Printf("%s\n", Cmd_Args());
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

using cmd_function_t = struct cmd_function_s
{
	cmd_function_s* next;
	char* name;
	xcommand_t function;
	completionFunc_t complete;
};

static int cmd_argc;
static char* cmd_argv[MAX_STRING_TOKENS]; // points into cmd_tokenized
static char cmd_tokenized[BIG_INFO_STRING + MAX_STRING_TOKENS]; // will have 0 bytes inserted
static char cmd_cmd[BIG_INFO_STRING]; // the original command we received (no token processing)

static cmd_function_t* cmd_functions; // possible commands to execute

/*
============
Cmd_FindCommand
============
*/
static cmd_function_t* Cmd_FindCommand(const char* cmd_name)
{
	for (cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_stricmp(cmd_name, cmd->name))
			return cmd;
	return nullptr;
}

/*
============
Cmd_Argc
============
*/
int Cmd_Argc()
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char* Cmd_Argv(const int arg)
{
	if (static_cast<unsigned>(arg) >= static_cast<unsigned>(cmd_argc))
		return "";

	return cmd_argv[arg];
}

/*
============
Cmd_ArgvBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void Cmd_ArgvBuffer(const int arg, char* buffer, const int buffer_length)
{
	Q_strncpyz(buffer, Cmd_Argv(arg), buffer_length);
}

/*
============
Cmd_ArgsFrom

Returns a single string containing argv(arg) to argv(argc()-1)
============
*/
char* Cmd_ArgsFrom(int arg)
{
	static char cmd_args[BIG_INFO_STRING];

	cmd_args[0] = '\0';
	if (arg < 0)
		arg = 0;
	for (int i = arg; i < cmd_argc; i++)
	{
		Q_strcat(cmd_args, sizeof(cmd_args), cmd_argv[i]);
		if (i != cmd_argc - 1)
		{
			Q_strcat(cmd_args, sizeof(cmd_args), " ");
		}
	}

	return cmd_args;
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char* Cmd_Args()
{
	return Cmd_ArgsFrom(1);
}

/*
============
Cmd_ArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void Cmd_ArgsBuffer(char* buffer, const int buffer_length)
{
	Q_strncpyz(buffer, Cmd_ArgsFrom(1), buffer_length);
}

/*
============
Cmd_ArgsFromBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
static void Cmd_ArgsFromBuffer(char* buffer, const int buffer_length)
{
	Q_strncpyz(buffer, Cmd_Args(), buffer_length);
}

/*
============
Cmd_Cmd

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
============
*/
static char* Cmd_Cmd()
{
	return cmd_cmd;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
The text is copied to a seperate buffer and 0 characters
are inserted in the apropriate place, The argv array
will point into this temporary buffer.
============
*/
static void Cmd_TokenizeString2(const char* text_in, const qboolean ignore_quotes)
{
	// clear previous args
	cmd_argc = 0;

	if (!text_in)
	{
		return;
	}

	Q_strncpyz(cmd_cmd, text_in, sizeof(cmd_cmd));

	const char* text = text_in;
	char* text_out = cmd_tokenized;

	while (true)
	{
		if (cmd_argc == MAX_STRING_TOKENS)
		{
			return; // this is usually something malicious
		}

		while (true)
		{
			// skip whitespace
			while (*text && *reinterpret_cast<const unsigned char*>(text) <= ' ')
			{
				text++;
			}
			if (!*text)
			{
				return; // all tokens parsed
			}

			// skip // comments
			if (text[0] == '/' && text[1] == '/')
			{
				return; // all tokens parsed
			}

			// skip /* */ comments
			if (text[0] == '/' && text[1] == '*')
			{
				while (*text && (text[0] != '*' || text[1] != '/'))
				{
					text++;
				}
				if (!*text)
				{
					return; // all tokens parsed
				}
				text += 2;
			}
			else
			{
				break; // we are ready to parse a token
			}
		}

		// handle quoted strings
		// NOTE TTimo this doesn't handle \" escaping
		if (!ignore_quotes && *text == '"')
		{
			cmd_argv[cmd_argc] = text_out;
			cmd_argc++;
			text++;
			while (*text && *text != '"')
			{
				*text_out++ = *text++;
			}
			*text_out++ = 0;
			if (!*text)
			{
				return; // all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = text_out;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while (*reinterpret_cast<const unsigned char*>(text) > ' ')
		{
			if (!ignore_quotes && text[0] == '"')
			{
				break;
			}

			if (text[0] == '/' && text[1] == '/')
			{
				break;
			}

			// skip /* */ comments
			if (text[0] == '/' && text[1] == '*')
			{
				break;
			}

			*text_out++ = *text++;
		}

		*text_out++ = 0;

		if (!*text)
		{
			return; // all tokens parsed
		}
	}
}

/*
============
Cmd_TokenizeString
============
*/
void Cmd_TokenizeString(const char* text_in)
{
	Cmd_TokenizeString2(text_in, qfalse);
}

/*
============
Cmd_TokenizeStringIgnoreQuotes
============
*/
void Cmd_TokenizeStringIgnoreQuotes(const char* text_in)
{
	Cmd_TokenizeString2(text_in, qtrue);
}

/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand(const char* cmd_name, const xcommand_t function)
{
	// fail if the command already exists
	if (Cmd_FindCommand(cmd_name))
	{
		// allow completion-only commands to be silently doubled
		if (function != nullptr)
		{
			Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
		}
		return;
	}

	// use a small malloc to avoid zone fragmentation
	const auto cmd = static_cast<cmd_function_s*>(S_Malloc(sizeof(cmd_function_t)));
	cmd->name = CopyString(cmd_name);
	cmd->function = function;
	cmd->complete = nullptr;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_SetCommandCompletionFunc
============
*/
void Cmd_SetCommandCompletionFunc(const char* command, const completionFunc_t complete)
{
	for (cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!Q_stricmp(command, cmd->name))
			cmd->complete = complete;
	}
}

/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand(const char* cmd_name)
{
	cmd_function_t** back = &cmd_functions;
	while (true)
	{
		cmd_function_t* cmd = *back;
		if (!cmd)
		{
			// command wasn't active
			return;
		}
		if (strcmp(cmd_name, cmd->name) == 0)
		{
			*back = cmd->next;
			if (cmd->name)
			{
				Z_Free(cmd->name);
			}
			Z_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}

/*
============
Cmd_CommandCompletion
============
*/
void Cmd_CommandCompletion(const callbackFunc_t callback)
{
	for (const cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		callback(cmd->name);
	}
}

/*
============
Cmd_CompleteArgument
============
*/
void Cmd_CompleteArgument(const char* command, char* args, const int arg_num)
{
	for (const cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!Q_stricmp(command, cmd->name) && cmd->complete)
			cmd->complete(args, arg_num);
	}
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
============
*/
void Cmd_ExecuteString(const char* text)
{
	cmd_function_t* cmd = nullptr;

	// execute the command line
	Cmd_TokenizeString(text);
	if (!Cmd_Argc())
	{
		return; // no tokens
	}

	// check registered command functions
	for (cmd_function_t** prev = &cmd_functions; *prev; prev = &cmd->next)
	{
		cmd = *prev;
		if (!Q_stricmp(Cmd_Argv(0), cmd->name))
		{
			// rearrange the links so that the command will be
			// near the head of the list next time it is used
			*prev = cmd->next;
			cmd->next = cmd_functions;
			cmd_functions = cmd;

			// perform the action
			if (!cmd->function)
			{
				// let the cgame or game handle it
				break;
			}
			cmd->function();
			return;
		}
	}

	// check cvars
	if (Cvar_Command())
	{
		return;
	}

	// check client game commands
	if (com_cl_running && com_cl_running->integer && CL_GameCommand())
	{
		return;
	}

	// check server game commands
	if (com_sv_running && com_sv_running->integer && SV_GameCommand())
	{
		return;
	}

	// check ui commands
	if (com_cl_running && com_cl_running->integer && UI_GameCommand())
	{
		return;
	}

	// send it as a server command if we are connected
	// this will usually result in a chat message
	CL_ForwardCommandToServer();
}

/*
============
Cmd_List_f
============
*/
static void Cmd_List_f()
{
	const cmd_function_t* cmd;
	int i, j;
	const char* match = nullptr;

	if (Cmd_Argc() > 1)
	{
		match = Cmd_Argv(1);
	}

	for (cmd = cmd_functions, i = 0, j = 0;
		cmd;
		cmd = cmd->next, i++)
	{
		if (!cmd->name || (match && !Com_Filter(match, cmd->name, qfalse)))
			continue;

		Com_Printf(" %s\n", cmd->name);
		j++;
	}

	Com_Printf("\n%i total commands\n", i);
	if (i != j)
		Com_Printf("%i matching commands\n", j);
}

/*
==================
Cmd_CompleteCfgName
==================
*/
void Cmd_CompleteCfgName(char* args, const int arg_num)
{
	if (arg_num == 2)
	{
		Field_CompleteFilename("", "cfg", qfalse, qtrue);
	}
}

/*
============
Cmd_Init
============
*/
void Cmd_Init(void)
{
	Cmd_AddCommand("cmdlist", Cmd_List_f);
	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("execq", Cmd_Exec_f);
	Cmd_SetCommandCompletionFunc("exec", Cmd_CompleteCfgName);
	Cmd_SetCommandCompletionFunc("execq", Cmd_CompleteCfgName);
	Cmd_AddCommand("vstr", Cmd_Vstr_f);
	Cmd_SetCommandCompletionFunc("vstr", Cvar_CompleteCvarName);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("wait", Cmd_Wait_f);
}