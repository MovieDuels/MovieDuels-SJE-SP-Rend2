/*
===========================================================================
Pazaak - game rules and turn logic

Ported from the Jedi Knight Galaxies Lua code (pazaak.lua, cards.lua, util.lua, AI.lua,
written by BobaFett). Plain C so the same file builds in the MP game module (C) and the
SP UI (C++). It knows nothing about the engine: the host gives it a send function (text
for one player's board, in the "pzk" instruction set the board understands) and calls
Pzk_Frame with the current time.

Players are numbered 1 and 2 like in the Lua code; player 2 can be the AI.
===========================================================================
*/

#ifndef PAZAAK_CORE_H
#define PAZAAK_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

	// Card IDs (shared with the board UI)
	enum
	{
		PZCARD_BACK = -2, // face-down card (opponent's hand)
		PZCARD_NONE = -1,

		PZCARD_NORMAL_1 = 0, // main deck cards 1..10 (param 1 = flipped by 2&4 / 3&6)
		PZCARD_NORMAL_2,
		PZCARD_NORMAL_3,
		PZCARD_NORMAL_4,
		PZCARD_NORMAL_5,
		PZCARD_NORMAL_6,
		PZCARD_NORMAL_7,
		PZCARD_NORMAL_8,
		PZCARD_NORMAL_9,
		PZCARD_NORMAL_10,

		PZCARD_PLUS_1, // + cards (param 1 = flipped)
		PZCARD_PLUS_2,
		PZCARD_PLUS_3,
		PZCARD_PLUS_4,
		PZCARD_PLUS_5,
		PZCARD_PLUS_6,

		PZCARD_MINUS_1, // - cards (param 1 = flipped)
		PZCARD_MINUS_2,
		PZCARD_MINUS_3,
		PZCARD_MINUS_4,
		PZCARD_MINUS_5,
		PZCARD_MINUS_6,

		PZCARD_FLIP_1, // +/- cards: param 0 = +, 1 = -, 2 = + flipped, 3 = - flipped
		PZCARD_FLIP_2,
		PZCARD_FLIP_3,
		PZCARD_FLIP_4,
		PZCARD_FLIP_5,
		PZCARD_FLIP_6,

		PZCARD_TIEBREAKER, // param 0 = +1T, 1 = -1T
		PZCARD_DOUBLE,     // param = value of the card it doubles (0 in the hand)
		PZCARD_FLIP12,     // param 0 = +1, 1 = -1, 2 = +2, 3 = -2
		PZCARD_2N4,        // param 1 = on the field
		PZCARD_3N6         // param 1 = on the field
	};

#define PZK_NUM_SIDECARDS	23	// card types a player can own (PZCARD_PLUS_1 .. PZCARD_3N6)
#define PZK_SIDEDECK_SIZE	10
#define PZK_HAND_SIZE		4
#define PZK_FIELD_SIZE		9
#define PZK_DECK_SIZE		40

	// Dialog IDs for the "sd" instruction
	enum
	{
		PDLGID_ANY,
		PDLGID_WONSET,
		PDLGID_LOSTSET,
		PDLGID_TIESET,
		PDLGID_WONMATCH,
		PDLGID_LOSTMATCH,
		PDLGID_WINFORFEIT,
		PDLGID_LOSEFORFEIT,
		PDLGID_LOSEQUIT
	};

	typedef struct pzkPlayer_s
	{
		char name[64];
		int isAI;
		int present;    // 0 once the player left (disconnect, death), nothing is sent to him then
		int cards[PZK_NUM_SIDECARDS];      // how many of each side card the player owns
		int sideDeck[PZK_SIDEDECK_SIZE];   // the 10 cards picked for this match
		int hand[PZK_HAND_SIZE];           // 4 cards drawn from the side deck (PZCARD_NONE when used)
		int field[PZK_FIELD_SIZE];
		int fieldParams[PZK_FIELD_SIZE];
		int nextFieldSlot;                 // 1..10 like the Lua code (10 = field full)
		int points;                        // card total
		int score;                         // sets won
		int standing;
		int canUseCard;
		int dlgAccepted;
		int forfeited;
		int ready;
	} pzkPlayer_t;

	typedef struct pzkGame_s pzkGame_t;

	typedef void (*pzkSendFunc_t)(pzkGame_t* game, int pid, const char* text);
	typedef void (*pzkFinishFunc_t)(pzkGame_t* game, int winnerPid); // winnerPid 0 = aborted

	// Timeout actions (the Lua "TimeoutName" timer)
	enum
	{
		PZK_TO_NONE,
		PZK_TO_CARDSEL,
		PZK_TO_ENDTURN,
		PZK_TO_DIALOG,
		PZK_TO_CLEANUP
	};

	struct pzkGame_s
	{
		int inUse;
		int deck[PZK_DECK_SIZE];
		int nextDeckCard;
		int phase;          // -1 card selection, 0 init, 1 turn start, 2 draw, 3 hand, 4 turn end, 5 finished
		int aiGame;
		int firstPlayer;
		int newSet;
		int finished;
		int winner;         // pid of the winner once finished (0 = none)
		int tieBreaker;
		int turn;           // 0 = not started, else 1 or 2
		int showCardSelect;
		int dialogPending;
		int cleanedUp;

		pzkPlayer_t players[2];

		int phaseAt;        // time to run the queued phase (0 = none)
		int timeoutAt;      // time the timeout fires (0 = none)
		int timeoutAction;  // PZK_TO_*
		int now;            // time of the current call

		unsigned int seed;

		pzkSendFunc_t send;
		pzkFinishFunc_t finish;
		void* host;         // free for the host
		int hostData[4];    // free for the host
	};

	void Pzk_Init(pzkGame_t* g, pzkSendFunc_t send, pzkFinishFunc_t finish, unsigned int seed);
	void Pzk_SetPlayer(pzkGame_t* g, int pid, const char* name, int isAI);
	void Pzk_SetCards(pzkGame_t* g, int pid, const int* cards);      // PZK_NUM_SIDECARDS amounts
	void Pzk_SetSideDeck(pzkGame_t* g, int pid, const int* sideDeck); // PZK_SIDEDECK_SIZE card IDs
	void Pzk_ShowCardSelection(pzkGame_t* g, int show);

	// Returns NULL when the game started, else the reason it could not.
	const char* Pzk_StartGame(pzkGame_t* g, int now);

	// Runs the queued phase and the timeouts.
	void Pzk_Frame(pzkGame_t* g, int now);

	// A player's command (the words after "~pzk"): forfeit, acceptdlg, quit, setsd, setsdt,
	// endturn, stand, usecard <slot> <param>.
	void Pzk_Command(pzkGame_t* g, int pid, int argc, const char** argv, int now);

	// The player is gone (disconnect, death...): the opponent wins, or the game ends if both are gone.
	void Pzk_PlayerGone(pzkGame_t* g, int pid, int now);

	// Ends the game at once without a winner (map change, shutdown). Sends "stop" to the players.
	void Pzk_Abort(pzkGame_t* g);

	int Pzk_CardValue(int id, int param);

	// The default side-card collection (the Jedi Knight Galaxies test command: 10 of each card) and the AI's side deck.
	void Pzk_DefaultCards(int* cards);
	void Pzk_DefaultAISideDeck(int* sideDeck);

#ifdef __cplusplus
}
#endif

#endif // PAZAAK_CORE_H
