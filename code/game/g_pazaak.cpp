/*
===========================================================================
Pazaak - singleplayer game module side

The match itself runs in the UI (ui_pazaak.cpp) against the AI, the world is paused
meanwhile. This file starts it and settles it. Everyone at the match first sits down to meditate
(BOTH_MEDITATE), facing the opponent, and is invulnerable until it is over, then stands up again.

The Play Pazaak key ("pazaak") does one of two things, depending on what is in the crosshair:
1. nobody: a match against a nameless AI.
2. an NPC: he is challenged. If he is close enough (PZK_CHALLENGE_RANGE, "too far away" otherwise),
   not hostile and not busy fighting, he accepts, sits down with you and is the opponent on the
   board: really the AI plays with his name, he only sits there (his AI rests meanwhile, NPC.cpp).
   An NPC who can't sit (a droid, an animal) still lends his name to the AI.
- "pazaak_result": sent back by the UI when the match is over
No match during a cutscene (camera, a script holding the player, a video): "Pazaak is unavailable".

Scripts (ICARUS, see Q3_Interface.cpp and PAZAAK_SCRIPTING.txt):
- SET_PAZAAK_PLAY "<targetname>": a match between the player and that NPC (he sits down too, no distance
  rule), or with the AI under that name if no entity has it ("" or "AI": a nameless AI)
- SET_PAZAAK_WAGER "<credits>": the wager of the next SET_PAZAAK_PLAY match (taken from PERS_CREDITS)
- SET_PAZAAK_ALLOWED "true" / "false": whether the player's Play Pazaak key works (g_pazaakAllowed)
- SET_PAZAAK_END "<script>": the script the entity that set it runs when the next match is over (once;
  "NULL" clears it); pazaak_result is already set then
- when a match is over: the global float "pazaak_result" (if the script declared it) becomes
  1 = the player won, 2 = the opponent won, 0 = called off, then the signal "pazaak_done" is set
===========================================================================
*/

#include "g_local.h"
#include "g_pazaak.h"
#include "b_local.h"
#include "Q3_Interface.h"
#include "../icarus/IcarusImplementation.h"

#define PZK_LOOK_RANGE		2048	// who is in the crosshair (further away: nobody, the AI)
#define PZK_CHALLENGE_RANGE	256		// an NPC must be this close to be challenged
#define PZK_SIT_TIME	1500	// time to sit down before the board opens, if the anim does not tell
#define PZK_ENEMY_RANGE	1024	// no match while an enemy who is after the player is this close (and in his PVS)

extern bool in_camera;
extern qboolean player_locked;
extern cvar_t* g_skippingcin;
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern qboolean PM_HasAnimation(const gentity_t* ent, int animation);

static int pzkWager = 0;            // credits taken for the running match
static int pzkOpponent = -1;        // NPC entity number
static qboolean pzkRunning = qfalse;
static int pzkStartAt = 0;          // time the board opens (0 = open, or nothing pending)
static char pzkName[64];
static qboolean pzkHadGod = qfalse; // the player had god mode before the match
static qboolean pzkNPCSits = qfalse;     // the opponent NPC sits at the match
static qboolean pzkNPCHadGod = qfalse;   // he had god mode before it
static int pzkScriptWager = 0;      // SET_PAZAAK_WAGER: the wager of the next SET_PAZAAK_PLAY match
static int pzkEndScriptEnt = -1;    // SET_PAZAAK_END: who runs pzkEndScript when the next match is over (-1 = nobody)
static char pzkEndScript[MAX_QPATH];

static cvar_t* g_pazaakAllowed;     // SET_PAZAAK_ALLOWED: 0 = the Play Pazaak key does nothing (saved with the game)

void G_Pazaak_Init()
{
	pzkWager = 0;
	pzkOpponent = -1;
	pzkRunning = qfalse;
	pzkStartAt = 0;
	pzkHadGod = qfalse;
	pzkNPCSits = qfalse;
	pzkNPCHadGod = qfalse;
	pzkScriptWager = 0;
	pzkEndScriptEnt = -1;
	pzkEndScript[0] = 0;
	g_pazaakAllowed = gi.cvar("g_pazaakAllowed", "1", CVAR_SAVEGAME);
}

static void Pzk_Print(const gentity_t* ent, const char* text)
{
	gi.SendServerCommand(ent->s.number, va("print \"%s\n\"", text));
}

static void Pzk_CenterPrint(const gentity_t* ent, const char* text)
{
	gi.SendServerCommand(ent->s.number, va("cp \"%s\"", text));
}

static const char* Pzk_NPCName(const gentity_t* npc)
{
	if (npc->fullName && npc->fullName[0])
	{
		return npc->fullName;
	}
	if (npc->NPC_type && npc->NPC_type[0])
	{
		return npc->NPC_type;
	}
	return "AI";
}

// A cutscene runs (camera, a script holds the player, or it is being skipped)
static qboolean Pzk_InCutscene()
{
	return in_camera || player_locked || g_skippingcin && g_skippingcin->integer ? qtrue : qfalse;
}

// An enemy who is after the player is close by (the match would make him a sitting, invulnerable target)
static qboolean Pzk_EnemyNear(const gentity_t* ent)
{
	gentity_t* list[MAX_GENTITIES];
	vec3_t mins, maxs;

	for (int i = 0; i < 3; i++)
	{
		mins[i] = ent->currentOrigin[i] - PZK_ENEMY_RANGE;
		maxs[i] = ent->currentOrigin[i] + PZK_ENEMY_RANGE;
	}
	const int num = gi.EntitiesInBox(mins, maxs, list, MAX_GENTITIES);
	for (int i = 0; i < num; i++)
	{
		const gentity_t* other = list[i];
		if (other && other != ent && other->client && other->NPC && other->health > 0 && other->enemy == ent
			&& gi.inPVS(other->currentOrigin, ent->currentOrigin))
		{
			return qtrue;
		}
	}
	return qfalse;
}

// Why the player can't start a match now (nullptr: he can)
static const char* Pzk_BusyReason(const gentity_t* ent)
{
	const playerState_t* ps = &ent->client->ps;
	const int flags = ps->eFlags | ent->s.eFlags;

	if (Pzk_InCutscene())
	{
		return "A cutscene is playing.";
	}
	if (ent->s.m_iVehicleNum || ps->m_iVehicleNum)
	{
		return "Not while riding a vehicle.";
	}
	if (flags & EF_LOCKED_TO_WEAPON || ps->viewEntity > 0 && ps->viewEntity < ENTITYNUM_WORLD)
	{
		return "Not while controlling something else.";
	}
	if (flags & (EF_HELD_BY_RANCOR | EF_HELD_BY_WAMPA | EF_HELD_BY_SAND_CREATURE | EF_FORCE_GRIPPED | EF_FORCE_DRAINED)
		|| ps->saberLockTime > level.time || PM_InKnockDown(ps))
	{
		return "Not in the middle of a fight.";
	}
	if (ps->groundEntityNum == ENTITYNUM_NONE || ent->waterlevel >= 2)
	{
		return "You need solid ground to sit down.";
	}
	if (Pzk_EnemyNear(ent))
	{
		return "Enemies are nearby.";
	}
	return nullptr;
}

// On screen, also during a cutscene camera (cgame "pzkcp")
static void Pzk_Unavailable(const gentity_t* ent, const char* reason)
{
	gi.SendServerCommand(ent->s.number, va("pzkcp \"Pazaak is unavailable at this time.\n%s\"", reason ? reason : ""));
}

static qboolean Pzk_CanPlay(const gentity_t* ent)
{
	if (!ent || !ent->client || ent->health <= 0 || ent->client->ps.pm_type == PM_DEAD)
	{
		return qfalse;
	}
	return qtrue;
}

// Turns to face the other one (yaw only), before sitting down: the meditate pose then holds the view
static void Pzk_Face(gentity_t* ent, const gentity_t* other)
{
	vec3_t dir, angles;

	if (!other)
	{
		return;
	}
	VectorSubtract(other->currentOrigin, ent->currentOrigin, dir);
	dir[2] = 0;
	if (VectorLength(dir) < 1.0f)
	{
		return;
	}
	vectoangles(dir, angles);
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	if (ent->NPC)
	{
		ent->NPC->lockedDesiredYaw = ent->NPC->desiredYaw = angles[YAW];
		ent->NPC->lockedDesiredPitch = ent->NPC->desiredPitch = 0;
	}
	SetClientViewAngle(ent, angles);
}

// Saber off, sit down to meditate (g_active.cpp holds the pose while there is no input), invulnerable.
// Returns whether he had god mode before.
static qboolean Pzk_SitDown(gentity_t* ent, const gentity_t* opponent)
{
	Pzk_Face(ent, opponent);
	if (ent->client->ps.SaberActive())
	{
		G_Sound(ent, ent->client->ps.saber[0].soundOff);
		ent->client->ps.SaberDeactivate();
	}
	VectorClear(ent->client->ps.velocity);
	NPC_SetAnim(ent, SETANIM_BOTH, BOTH_MEDITATE, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	const qboolean hadGod = ent->flags & FL_GODMODE ? qtrue : qfalse;
	ent->flags |= FL_GODMODE;
	return hadGod;
}

// Stand up again, no longer invulnerable
static void Pzk_StandUp(gentity_t* ent, const qboolean hadGod)
{
	if (!hadGod)
	{
		ent->flags &= ~FL_GODMODE;
	}
	if (ent->client && ent->client->ps.legsAnim == BOTH_MEDITATE && ent->health > 0)
	{
		NPC_SetAnim(ent, SETANIM_BOTH, BOTH_MEDITATE_END, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}
}

// How long the sitting down takes (the length of BOTH_MEDITATE, which then holds its last frame)
static int Pzk_SitTime(const gentity_t* ent)
{
	const int t = ent->client->ps.legsAnim == BOTH_MEDITATE ? ent->client->ps.legsAnimTimer : 0;
	return t >= 300 && t <= 4000 ? t : PZK_SIT_TIME;
}

// Whether this NPC can sit at the board: a humanoid with the meditate anim
static qboolean Pzk_NPCCanSit(const gentity_t* npc)
{
	return npc && npc->client && npc->NPC && npc->health > 0 && PM_HasAnimation(npc, BOTH_MEDITATE) ? qtrue : qfalse;
}

// The opponent NPC of the match is sitting there (NPC.cpp lets his AI rest)
qboolean G_Pazaak_IsNPCPlaying(const gentity_t* ent)
{
	return pzkRunning && pzkNPCSits && ent && ent->s.number == pzkOpponent ? qtrue : qfalse;
}

// npc: the opponent (his name is used; npcSits: he sits down too), or nullptr: the AI as name
static void Pzk_Start(gentity_t* player, gentity_t* npc, const int wager, const qboolean npcSits, const char* name)
{
	Q_strncpyz(pzkName, npc ? Pzk_NPCName(npc) : name && name[0] ? name : "AI", sizeof(pzkName));
	IIcarusInterface* icarus = IIcarusInterface::GetIcarus();
	if (icarus)
	{
		static_cast<CIcarus*>(icarus)->ClearSignal("pazaak_done"); // an old one nobody waited for
	}
	pzkRunning = qtrue;
	pzkOpponent = npc ? npc->s.number : -1;
	pzkWager = wager;
	pzkHadGod = Pzk_SitDown(player, npc);
	int sit = Pzk_SitTime(player);
	pzkNPCSits = npc && npcSits ? qtrue : qfalse;
	if (pzkNPCSits)
	{
		pzkNPCHadGod = Pzk_SitDown(npc, player);
		if (Pzk_SitTime(npc) > sit)
		{
			sit = Pzk_SitTime(npc);
		}
	}
	// The board opens once they sit (G_Pazaak_RunFrame): BOTH_MEDITATE is the sitting down, then it holds
	pzkStartAt = level.time + sit;
}

// Tells the scripts how it went: the global float "pazaak_result" (if declared) and the signal "pazaak_done"
static void Pzk_TellScripts(const int winner)
{
	Quake3Game()->SetDeclaredFloat("pazaak_result", static_cast<float>(winner));
	IIcarusInterface* icarus = IIcarusInterface::GetIcarus();
	if (icarus)
	{
		static_cast<CIcarus*>(icarus)->Signal("pazaak_done");
	}
}

// SET_PAZAAK_END has a script run once the match is over (after pazaak_result is set)
static void Pzk_RunEndScript()
{
	const int entNum = pzkEndScriptEnt;
	char script[MAX_QPATH];

	if (entNum < 0 || !pzkEndScript[0])
	{
		return;
	}
	Q_strncpyz(script, pzkEndScript, sizeof(script));
	pzkEndScriptEnt = -1; // once: the script may set the next one
	pzkEndScript[0] = 0;
	const gentity_t* ent = &g_entities[entNum];
	if (ent->inuse)
	{
		Quake3Game()->RunScript(ent, script);
	}
}

// The match is over (winner: 1 = player, 2 = opponent, 0 = called off): stand up, settle the wager
static void Pzk_Result(gentity_t* ent, const int winner)
{
	pzkRunning = qfalse;
	pzkStartAt = 0;
	Pzk_StandUp(ent, pzkHadGod);
	pzkHadGod = qfalse;
	if (pzkNPCSits && pzkOpponent >= 0 && pzkOpponent < ENTITYNUM_WORLD)
	{
		gentity_t* npc = &g_entities[pzkOpponent];
		if (npc->inuse && npc->client)
		{
			Pzk_StandUp(npc, pzkNPCHadGod);
		}
	}
	pzkNPCSits = qfalse;
	pzkNPCHadGod = qfalse;
	if (ent->client && pzkWager > 0)
	{
		if (winner == 1)
		{
			ent->client->ps.persistant[PERS_CREDITS] += pzkWager * 2;
			Pzk_Print(ent, va("^5Pazaak:^7 you win %i credits.", pzkWager * 2));
		}
		else if (winner == 0)
		{
			ent->client->ps.persistant[PERS_CREDITS] += pzkWager; // called off: money back
		}
		else
		{
			Pzk_Print(ent, va("^5Pazaak:^7 you lose %i credits.", pzkWager));
		}
	}
	else if (winner == 1)
	{
		Pzk_Print(ent, "^5Pazaak:^7 you win the match.");
	}
	else if (winner == 2)
	{
		Pzk_Print(ent, "^5Pazaak:^7 you lose the match.");
	}
	pzkWager = 0;
	pzkOpponent = -1;
	Pzk_TellScripts(winner);
	Pzk_RunEndScript();
}

// Every frame: open the board once they sit
void G_Pazaak_RunFrame()
{
	if (!pzkStartAt)
	{
		return;
	}
	if (!Pzk_CanPlay(&g_entities[0]))
	{
		// He died while sitting down: no match
		Pzk_Result(&g_entities[0], 0);
		return;
	}
	if (Pzk_InCutscene())
	{
		// A cutscene started while he sat down: no match now
		Pzk_Unavailable(&g_entities[0], "A cutscene is playing.");
		Pzk_Result(&g_entities[0], 0);
		return;
	}
	if (pzkNPCSits && (pzkOpponent < 0 || !Pzk_CanPlay(&g_entities[pzkOpponent])))
	{
		// The opponent died (or went away) while sitting down
		Pzk_Result(&g_entities[0], 0);
		return;
	}
	if (level.time >= pzkStartAt)
	{
		pzkStartAt = 0;
		gi.SendConsoleCommand(va("uipzk_start \"%s\" %i %i\n", pzkName, pzkOpponent, pzkWager));
	}
}

// A wager (credits taken now) and the match; qfalse if he can't pay
static qboolean Pzk_TakeWager(gentity_t* player, const int wager)
{
	if (wager <= 0)
	{
		return qtrue;
	}
	if (player->client->ps.persistant[PERS_CREDITS] < wager)
	{
		Pzk_Print(player, va("^5Pazaak:^7 you need %i credits to play.", wager));
		return qfalse;
	}
	player->client->ps.persistant[PERS_CREDITS] -= wager;
	Pzk_Print(player, va("^5Pazaak:^7 you wager %i credits.", wager));
	return qtrue;
}

qboolean G_Pazaak_StartVsNPC(gentity_t* player, gentity_t* npc, const int wager)
{
	if (!Pzk_CanPlay(player) || pzkRunning)
	{
		return qfalse;
	}
	const char* busy = Pzk_BusyReason(player);
	if (busy)
	{
		Pzk_Unavailable(player, busy);
		return qfalse;
	}
	if (!Pzk_TakeWager(player, wager))
	{
		return qfalse;
	}
	Pzk_Start(player, npc, wager > 0 ? wager : 0, Pzk_NPCCanSit(npc), nullptr);
	return qtrue;
}

// SET_PAZAAK_END (Q3_Interface.cpp): entNum runs this script when the next match is over ("NULL" or "": none)
void G_Pazaak_SetEndScript(const int entNum, const char* script)
{
	if (!script || !script[0] || !Q_stricmp(script, "NULL"))
	{
		pzkEndScriptEnt = -1;
		pzkEndScript[0] = 0;
		return;
	}
	pzkEndScriptEnt = entNum;
	Q_strncpyz(pzkEndScript, script, sizeof(pzkEndScript));
}

// SET_PAZAAK_WAGER (Q3_Interface.cpp)
void G_Pazaak_SetScriptWager(const int wager)
{
	pzkScriptWager = wager > 0 ? wager : 0;
}

// SET_PAZAAK_PLAY (Q3_Interface.cpp): a match the script wants, no distance or "enemies near" rules.
// who: the targetname of the opponent NPC, or the name the AI plays under ("" / "AI": nameless).
void G_Pazaak_StartScripted(const char* who)
{
	gentity_t* player = &g_entities[0];
	gentity_t* npc = nullptr;
	const int wager = pzkScriptWager;

	pzkScriptWager = 0;
	if (who && who[0] && Q_stricmp(who, "AI"))
	{
		npc = G_Find(nullptr, FOFS(targetname), who);
		if (npc && (!npc->client || npc->health <= 0))
		{
			npc = nullptr;
		}
	}
	if (!Pzk_CanPlay(player) || pzkRunning)
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "SET_PAZAAK_PLAY: the player can't play now\n");
		Pzk_TellScripts(0);
		Pzk_RunEndScript();
		return;
	}
	if (Pzk_InCutscene())
	{
		Quake3Game()->DebugPrint(IGameInterface::WL_WARNING, "SET_PAZAAK_PLAY: a cutscene is running (camera or player locked), no match\n");
		Pzk_Unavailable(player, "A cutscene is playing.");
		Pzk_TellScripts(0);
		Pzk_RunEndScript();
		return;
	}
	if (!Pzk_TakeWager(player, wager))
	{
		Pzk_TellScripts(0);
		Pzk_RunEndScript();
		return;
	}
	Pzk_Start(player, npc, wager, Pzk_NPCCanSit(npc), who);
}

// The entity in the player's crosshair (up to PZK_LOOK_RANGE away)
static gentity_t* Pzk_LookTarget(const gentity_t* ent)
{
	vec3_t start, end, fwd;
	trace_t tr;

	VectorCopy(ent->client->ps.origin, start);
	start[2] += ent->client->ps.viewheight;
	AngleVectors(ent->client->ps.viewangles, fwd, nullptr, nullptr);
	VectorMA(start, PZK_LOOK_RANGE, fwd, end);
	gi.trace(&tr, start, nullptr, nullptr, end, ent->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);
	if (tr.entityNum >= ENTITYNUM_WORLD || tr.entityNum == ent->s.number)
	{
		return nullptr;
	}
	return &g_entities[tr.entityNum];
}

// Why this NPC won't sit down with the player now (nullptr: he will)
static const char* Pzk_NPCBusy(const gentity_t* npc, const gentity_t* player)
{
	if (npc->enemy == player || npc->client->playerTeam == TEAM_ENEMY)
	{
		return "refuses to play Pazaak with you.";
	}
	if (npc->enemy && npc->enemy->health > 0)
	{
		return "is busy fighting and cannot respond.";
	}
	if (npc->client->ps.groundEntityNum == ENTITYNUM_NONE || npc->waterlevel >= 2 || npc->client->ps.m_iVehicleNum
		|| npc->svFlags & SVF_ICARUS_FREEZE || player->client->ps.viewEntity == npc->s.number)
	{
		return "is busy and cannot respond.";
	}
	return nullptr;
}

qboolean G_Pazaak_ClientCommand(gentity_t* ent, const char* cmd)
{
	if (!Q_stricmp(cmd, "pazaak"))
	{
		if (!Pzk_CanPlay(ent))
		{
			Pzk_Unavailable(ent, "");
			return qtrue;
		}
		if (pzkRunning)
		{
			return qtrue; // already sitting down for one
		}
		if (g_pazaakAllowed && !g_pazaakAllowed->integer)
		{
			Pzk_Unavailable(ent, ""); // a script switched it off (SET_PAZAAK_ALLOWED)
			return qtrue;
		}
		const char* busy = Pzk_BusyReason(ent);
		if (busy)
		{
			Pzk_Unavailable(ent, busy);
			return qtrue;
		}
		gentity_t* target = Pzk_LookTarget(ent);
		if (target && target->client && target->NPC && target->health > 0)
		{
			// 2. An NPC: challenge him
			const char* name = Pzk_NPCName(target);
			if (Distance(ent->currentOrigin, target->currentOrigin) > PZK_CHALLENGE_RANGE)
			{
				Pzk_CenterPrint(ent, va("%s^7 is too far away.\nYou must be closer to challenge someone to Pazaak.", name));
				return qtrue;
			}
			const char* npcBusy = Pzk_NPCBusy(target, ent);
			if (npcBusy)
			{
				Pzk_CenterPrint(ent, va("You have challenged %s^7 to a Pazaak game.\n%s^7 %s", name, name, npcBusy));
				Pzk_Print(ent, va("^5Pazaak:^7 %s^7 %s", name, npcBusy));
				return qtrue;
			}
			Pzk_CenterPrint(ent, va("You have challenged %s^7 to a Pazaak game.\n%s^7 accepts.", name, name));
			Pzk_Print(ent, va("^5Pazaak:^7 You have challenged %s^7 to a Pazaak game. %s^7 accepts.", name, name));
			Pzk_Start(ent, target, 0, Pzk_NPCCanSit(target), nullptr);
			return qtrue;
		}
		// 1. Nobody: the AI
		Pzk_Start(ent, nullptr, 0, qfalse, nullptr);
		return qtrue;
	}
	if (!Q_stricmp(cmd, "pazaak_result"))
	{
		// pazaak_result <winner: 1 = player, 2 = opponent, 0 = called off> [unavailable]
		const int winner = atoi(gi.argv(1));
		if (!pzkRunning)
		{
			return qtrue;
		}
		if (!Q_stricmp(gi.argv(2), "unavailable"))
		{
			Pzk_Unavailable(ent, "A video is playing."); // it started while he sat down
		}
		Pzk_Result(ent, winner);
		return qtrue;
	}
	return qfalse;
}
