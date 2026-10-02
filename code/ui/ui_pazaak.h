#ifndef PZK_SP
#define PZK_SP
#endif

/*
===========================================================================
Pazaak board (UI) - see ui_pazaak.c
===========================================================================
*/

#ifndef UI_PAZAAK_H
#define UI_PAZAAK_H

// Ownerdraw numbers used by ui/pazaak/sje_pazaak.menu: PZK_OD_BASE + type * 100 + slot
#define PZK_OD_BASE		2000
#define PZK_OD_BCARD	0	// field slots 1..18
#define PZK_OD_BHAND	1	// hand slots 1..8
#define PZK_OD_NAMES	2	// 1 = player, 2 = opponent
#define PZK_OD_POINTS	3	// 1 = player, 2 = opponent
#define PZK_OD_DLGTEXT	4	// dialog lines 1..2
#define PZK_OD_TIMEOUT	5
#define PZK_OD_SELCARD	6	// card selection slots 0..22
#define PZK_OD_SELSD	7	// side deck slots 0..9
#define PZK_OD_WAITING	8
#define PZK_OD_HOWTO	9	// the "how to play" page of the card selection

// Draws the Pazaak ownerdraws; qfalse if the number is not one of ours
qboolean UI_Pazaak_OwnerDraw(int ownerDraw, float x, float y, float w, float h, qhandle_t shader);

// "uiScript pzk_*"; args is the script's argument pointer (char** in MP, const char** in SP)
qboolean UI_Pazaak_Script(const char* name, void* args);

// The board is open (escape asks to forfeit/quit instead of closing the menus)
qboolean UI_Pazaak_Active(void);
void UI_Pazaak_OnEsc(void);

// Instructions of the game ("pzk" words without the "pzk")
void UI_Pazaak_Process(int argc, const char** argv);

// "uipzk" (MP) / "uipzk_start", "uipzk_stop" (SP) console commands
qboolean UI_Pazaak_ConsoleCommand(const char* cmd);

#ifdef PZK_SP
// SP: runs the local match, call every UI frame
void UI_Pazaak_Frame(void);
#endif

#endif // UI_PAZAAK_H
