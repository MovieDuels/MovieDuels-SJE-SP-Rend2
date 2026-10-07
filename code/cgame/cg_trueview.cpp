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

#include "cg_headers.h"

constexpr auto SIEGECHAR_TAB = 9; //perhaps a bit hacky, but I don't think there's any define existing for "tab";

constexpr auto MAX_TRUEVIEW_INFO_SIZE = 65536; // room for every playable model (was 8192: a longer file was ignored)
char true_view_info[MAX_TRUEVIEW_INFO_SIZE];
int true_view_valid;
static char true_view_model[MAX_QPATH]; // the model the eye position was last set for (CG_TrueViewCheckModel)

int BG_SiegeGetPairedValue(const char* buf, char* key, char* outbuf)
{
	int i = 0;
	char check_key[4096]{};

	while (buf[i])
	{
		if (buf[i] != ' ' && buf[i] != '{' && buf[i] != '}' && buf[i] != '\n' && buf[i] != '\r')
		{
			//we're on a valid character
			if (buf[i] == '/' &&
				buf[i + 1] == '/')
			{
				//this is a comment, so skip over it
				while (buf[i] && buf[i] != '\n' && buf[i] != '\r')
				{
					i++;
				}
			}
			else
			{
				//parse to the next space/endline/eos and check this value against our key value.
				int j = 0;

				while (buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\r' && buf[i] != SIEGECHAR_TAB && buf[i])
				{
					if (buf[i] == '/' && buf[i + 1] == '/')
					{
						//hit a comment, break out.
						break;
					}

					check_key[j] = buf[i];
					j++;
					i++;
				}
				check_key[j] = 0;

				int k = i;

				while (buf[k] && (buf[k] == ' ' || buf[k] == '\n' || buf[k] == '\r'))
				{
					k++;
				}

				if (buf[k] == '{')
				{
					//this is not the start of a value but rather of a group. We don't want to look in subgroups so skip over the whole thing.
					int openB = 0;

					while (buf[i] && (buf[i] != '}' || openB))
					{
						if (buf[i] == '{')
						{
							openB++;
						}
						else if (buf[i] == '}')
						{
							openB--;
						}

						if (openB < 0)
						{
							Com_Error(ERR_DROP,
								"Unexpected closing bracket (too many) while parsing to end of group '%s'",
								check_key);
						}

						if (buf[i] == '}' && !openB)
						{
							//this is the end of the group
							break;
						}
						i++;
					}

					if (buf[i] == '}')
					{
						i++;
					}
				}
				else
				{
					//Is this the one we want?
					if (buf[i] != '/' || buf[i + 1] != '/')
					{
						//make sure we didn't stop on a comment, if we did then this is considered an error in the file.
						if (!Q_stricmp(check_key, key))
						{
							//guess so. Parse along to the next valid character, then put that into the output buffer and return 1.
							while ((buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\r' || buf[i] == SIEGECHAR_TAB) && buf
								[i])
							{
								i++;
							}

							if (buf[i])
							{
								//We're at the start of the value now.
								qboolean parse_to_quote = qfalse;

								if (buf[i] == '\"')
								{
									//if the value is in quotes, then stop at the next quote instead of ' '
									i++;
									parse_to_quote = qtrue;
								}

								j = 0;
								while (!parse_to_quote && buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\r' ||
									parse_to_quote && buf[i] != '\"')
								{
									if (buf[i] == '/' &&
										buf[i + 1] == '/')
									{
										//hit a comment after the value? This isn't an ideal way to be writing things, but we'll support it anyway.
										break;
									}
									outbuf[j] = buf[i];
									j++;
									i++;

									if (!buf[i])
									{
										if (parse_to_quote)
										{
											Com_Error(ERR_DROP,
												"Unexpected EOF while looking for endquote, error finding paired value for '%s'",
												key);
										}
										Com_Error(ERR_DROP,
											"Unexpected EOF while looking for space or endline, error finding paired value for '%s'",
											key);
									}
								}
								outbuf[j] = 0;

								return 1; //we got it, so return 1.
							}
							Com_Error(ERR_DROP, "Error parsing file, unexpected EOF while looking for valud '%s'", key);
						}
						//if that wasn't the desired key, then make sure we parse to the end of the line, so we don't mistake a value for a key
						while (buf[i] && buf[i] != '\n')
						{
							i++;
						}
					}
					else
					{
						Com_Error(ERR_DROP, "Error parsing file, found comment, expected value for '%s'", key);
					}
				}
			}
		}

		if (!buf[i])
		{
			break;
		}
		i++;
	}

	return 0; //guess we never found it.
}

//Loads in the True View auto eye positioning data so you don't have to worry about disk access later in the
//game
//Based on CG_InitSagaMode and tck's tck_InitBuffer
void CG_TrueViewInit()
{
	fileHandle_t f;

	const int len = gi.FS_FOpenFile("trueview.cfg", &f, FS_READ);

	if (!f)
	{
		CG_Printf("Error: File Not Found: trueview.cfg\n");
		true_view_valid = 0;
		return;
	}

	if (len < 0 || len >= MAX_TRUEVIEW_INFO_SIZE - 1)
	{
		CG_Printf("Error: trueview.cfg is over the trueview.cfg filesize limit.\n");
		gi.FS_FCloseFile(f);
		true_view_valid = 0;
		return;
	}

	gi.FS_Read(true_view_info, len, f);
	// end with a line break: a value on the last line with none after it was an "Unexpected EOF" error drop
	true_view_info[len] = '\n';
	true_view_info[len + 1] = 0;
	true_view_model[0] = 0; // look the player's model up again

	true_view_valid = 1;

	gi.FS_FCloseFile(f);
}

void CG_AdjustEyePos(const char* model_name)
{
	//eye position

	if (true_view_valid)
	{
		char eyepos[MAX_QPATH];
		if (BG_SiegeGetPairedValue(true_view_info, const_cast<char*>(model_name), eyepos))
		{
			CG_Printf("True View Eye Adjust Loaded for %s.\n", model_name);
			gi.cvar_set("cg_trueeyeposition", eyepos);
		}
		else
		{
			//Couldn't find an entry for the desired model.  Not nessicarily a bad thing.
			gi.cvar_set("cg_trueeyeposition", "0");
		}
	}
	else
	{
		//The model eye position list is messed up.  Default to 0.0 for the eye position
		gi.cvar_set("cg_trueeyeposition", "0");
	}
}

// The player's model changed (or this is the first look at it): its True View eye position from trueview.cfg.
// Called every frame from the True View camera code (cg_players.cpp), so it also catches playermodel changes,
// NPC models and save / load, not only a new client info.
void CG_TrueViewCheckModel(const char* model_name)
{
	if (!model_name)
	{
		model_name = "";
	}
	const char* key = model_name[0] ? model_name : "-"; // "-": no model name (so it is looked up once, not every frame)
	if (!Q_stricmp(true_view_model, key))
	{
		return;
	}
	Q_strncpyz(true_view_model, key, sizeof(true_view_model));
	CG_AdjustEyePos(model_name);
}

// "trueview_save [value]": saves the player's current model with cg_trueeyeposition (or the value given, which is
// also applied) into trueview.cfg in the game's own folder (Documents\...), which is read before the pk3's copy.
// Tuning: go into first person with the model, change cg_trueeyeposition until the view sits just in front of the
// face, then trueview_save. When done, copy that trueview.cfg into the mod's pk3.
void CG_TrueViewSave_f()
{
	static char new_info[MAX_TRUEVIEW_INFO_SIZE];
	char value[32];
	int n = 0;
	qboolean done = qfalse;
	fileHandle_t f;

	if (!true_view_model[0] || !Q_stricmp(true_view_model, "-"))
	{
		CG_Printf("True View: no player model yet (look through his eyes in first person first).\n");
		return;
	}
	if (cgi_Argc() > 1)
	{
		Q_strncpyz(value, CG_Argv(1), sizeof(value));
		gi.cvar_set("cg_trueeyeposition", value);
	}
	else
	{
		Q_strncpyz(value, cg_trueeyeposition.string, sizeof(value));
	}

	// the current list with this model's line replaced (or added at the end)
	const char* p = true_view_valid ? true_view_info : "";
	while (*p)
	{
		const char* eol = strchr(p, '\n');
		const int line_len = eol ? (int)(eol - p) + 1 : (int)strlen(p);
		char key[MAX_QPATH];
		int k = 0;
		const char* q = p;

		while (q < p + line_len && (*q == ' ' || *q == '\t'))
		{
			q++;
		}
		while (q < p + line_len && *q > ' ' && k < MAX_QPATH - 1)
		{
			key[k++] = *q++;
		}
		key[k] = 0;

		if (!done && key[0] && !Q_stricmp(key, true_view_model))
		{
			n += Com_sprintf(new_info + n, sizeof(new_info) - n, "%-24s %s\n", true_view_model, value);
			done = qtrue;
		}
		else if (n + line_len < (int)sizeof(new_info) - 64)
		{
			memcpy(new_info + n, p, line_len);
			n += line_len;
		}
		p += line_len;
	}
	while (n > 1 && new_info[n - 1] == '\n' && new_info[n - 2] == '\n')
	{
		n--; // no growing run of blank lines at the end
	}
	if (!done)
	{
		if (n > 0 && new_info[n - 1] != '\n')
		{
			new_info[n++] = '\n';
		}
		n += Com_sprintf(new_info + n, sizeof(new_info) - n, "%-24s %s\n", true_view_model, value);
	}
	new_info[n] = 0;

	gi.FS_FOpenFile("trueview.cfg", &f, FS_WRITE);
	if (!f)
	{
		CG_Printf("True View: could not write trueview.cfg.\n");
		return;
	}
	gi.FS_Write(new_info, n, f);
	gi.FS_FCloseFile(f);

	// use the new list straight away
	memcpy(true_view_info, new_info, n + 1);
	true_view_valid = 1;
	CG_Printf("True View: saved %s %s to trueview.cfg (in your game folder).\n", true_view_model, value);
}