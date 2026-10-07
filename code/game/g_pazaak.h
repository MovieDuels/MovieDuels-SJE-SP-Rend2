/*
===========================================================================
Pazaak - singleplayer game module side (see g_pazaak.cpp)
===========================================================================
*/

#ifndef G_PAZAAK_H
#define G_PAZAAK_H

void G_Pazaak_Init();
qboolean G_Pazaak_ClientCommand(gentity_t* ent, const char* cmd);
qboolean G_Pazaak_StartVsNPC(gentity_t* player, gentity_t* npc, int wager);
void G_Pazaak_RunFrame();
qboolean G_Pazaak_IsNPCPlaying(const gentity_t* ent);	// the opponent NPC sits at the match (NPC.cpp)
void G_Pazaak_StartScripted(const char* who);			// SET_PAZAAK_PLAY
void G_Pazaak_SetScriptWager(int wager);				// SET_PAZAAK_WAGER
void G_Pazaak_SetEndScript(int entNum, const char* script);	// SET_PAZAAK_END
qboolean G_Pazaak_InterceptScript(const gentity_t* ent, const char* script);	// RunScript: held back for a challenge question
bool G_Pazaak_HoldRun(int entNum, const char* script);	// icarus/Sequencer.cpp: a run( ) waits for a challenge question

#endif // G_PAZAAK_H
