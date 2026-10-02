/*
===========================================================================
Pazaak - game rules and turn logic (see pazaak_core.h)

Ported from the Jedi Knight Galaxies Lua code by BobaFett. Fixes over the Lua version:
- the AI always stood after playing a card (`if player.NextFieldSlot or ...`)
- the turn/phase check of the player commands never rejected anything (operator precedence)
- a bad side deck called PlayerQuit with an undefined player
- the param of a played card is checked (a client could turn a + card into a - card)
- a full field (9 cards) wins the set for either player, not only the one whose turn it is
- the AI understands every card it can play (+/- with both signs, tie-breaker, +/-1/2)
===========================================================================
*/

#include "pazaak_core.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#define PZK_PHASE_DELAY	250

/*
===========================================================================
Cards
===========================================================================
*/

int Pzk_CardValue(const int id, const int param)
{
	if (id >= PZCARD_NORMAL_1 && id <= PZCARD_NORMAL_10)
	{
		return param == 0 ? id - PZCARD_NORMAL_1 + 1 : -(id - PZCARD_NORMAL_1 + 1);
	}
	if (id >= PZCARD_PLUS_1 && id <= PZCARD_PLUS_6)
	{
		return param == 0 ? id - PZCARD_PLUS_1 + 1 : -(id - PZCARD_PLUS_1 + 1);
	}
	if (id >= PZCARD_MINUS_1 && id <= PZCARD_MINUS_6)
	{
		return param == 0 ? -(id - PZCARD_MINUS_1 + 1) : id - PZCARD_MINUS_1 + 1;
	}
	if (id >= PZCARD_FLIP_1 && id <= PZCARD_FLIP_6)
	{
		return param == 1 || param == 2 ? -(id - PZCARD_FLIP_1 + 1) : id - PZCARD_FLIP_1 + 1;
	}
	switch (id)
	{
	case PZCARD_TIEBREAKER:
		return param == 1 ? -1 : 1;
	case PZCARD_DOUBLE:
		return param;
	case PZCARD_FLIP12:
		switch (param)
		{
		case 0: return 1;
		case 1: return -1;
		case 2: return 2;
		case 3: return -2;
		default: return 0;
		}
	default:
		return 0; // backside, 2&4, 3&6
	}
}

static int Pzk_IsSideCard(const int id)
{
	return id >= PZCARD_PLUS_1 && id <= PZCARD_3N6;
}

// Keeps a param from a client within what the card allows
static int Pzk_SanitizeParam(const int id, const int param)
{
	if (id >= PZCARD_FLIP_1 && id <= PZCARD_FLIP_6)
	{
		return param == 1 ? 1 : 0;
	}
	if (id == PZCARD_TIEBREAKER)
	{
		return param == 1 ? 1 : 0;
	}
	if (id == PZCARD_FLIP12)
	{
		return param >= 0 && param <= 3 ? param : 0;
	}
	return 0;
}

void Pzk_DefaultCards(int* cards)
{
	int i;
	for (i = 0; i < PZK_NUM_SIDECARDS; i++)
	{
		cards[i] = 10;
	}
}

void Pzk_DefaultAISideDeck(int* sideDeck)
{
	static const int deck[PZK_SIDEDECK_SIZE] = {
		PZCARD_FLIP_1, PZCARD_FLIP_2, PZCARD_FLIP_3, PZCARD_FLIP_4, PZCARD_FLIP_5,
		PZCARD_FLIP_6, PZCARD_FLIP_2, PZCARD_FLIP_3, PZCARD_FLIP_4, PZCARD_FLIP_5
	};
	memcpy(sideDeck, deck, sizeof(deck));
}

/*
===========================================================================
Utility
===========================================================================
*/

static int Pzk_Rand(pzkGame_t* g, const int lo, const int hi)
{
	g->seed = g->seed * 1103515245u + 12345u;
	return lo + (int)((g->seed >> 16) % (unsigned int)(hi - lo + 1));
}

static pzkPlayer_t* Pzk_P(pzkGame_t* g, const int pid)
{
	return &g->players[pid - 1];
}

static void Pzk_Send(pzkGame_t* g, const int pid, const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	const pzkPlayer_t* p = Pzk_P(g, pid);

	if (p->isAI || !p->present || !g->send)
	{
		return;
	}
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = 0;
	g->send(g, pid, buf);
}

static void Pzk_GetTurnIDs(const pzkGame_t* g, int* pid, int* oid)
{
	if (g->turn == 1)
	{
		*pid = 1;
		*oid = 2;
	}
	else
	{
		*pid = 2;
		*oid = 1;
	}
}

static void Pzk_QueuePhase(pzkGame_t* g, const int delay)
{
	g->phaseAt = g->now + delay;
	if (g->phaseAt == 0)
	{
		g->phaseAt = 1;
	}
}

static void Pzk_SetTimeout(pzkGame_t* g, const int delay, const int action)
{
	g->timeoutAt = g->now + delay;
	if (g->timeoutAt == 0)
	{
		g->timeoutAt = 1;
	}
	g->timeoutAction = action;
}

static void Pzk_RemoveTimeout(pzkGame_t* g)
{
	g->timeoutAt = 0;
	g->timeoutAction = PZK_TO_NONE;
}

// A shuffled 40-card deck: four of each number 1..10
static void Pzk_GenerateDeck(pzkGame_t* g)
{
	int left[10];
	int numbers[10];
	int numCount = 10;
	int i;

	for (i = 0; i < 10; i++)
	{
		left[i] = 4;
		numbers[i] = i + 1;
	}
	for (i = 0; i < PZK_DECK_SIZE; i++)
	{
		const int pick = Pzk_Rand(g, 0, numCount - 1);
		const int num = numbers[pick];
		g->deck[i] = num;
		left[num - 1]--;
		if (left[num - 1] == 0)
		{
			numbers[pick] = numbers[numCount - 1];
			numCount--;
		}
	}
	g->nextDeckCard = 0;
}

// 4 random cards of the side deck
static void Pzk_GenerateHand(pzkGame_t* g, pzkPlayer_t* p)
{
	int cards[PZK_SIDEDECK_SIZE];
	int count = PZK_SIDEDECK_SIZE;
	int i;

	memcpy(cards, p->sideDeck, sizeof(cards));
	for (i = 0; i < PZK_HAND_SIZE; i++)
	{
		const int pick = Pzk_Rand(g, 0, count - 1);
		p->hand[i] = cards[pick];
		cards[pick] = cards[count - 1];
		count--;
	}
}

static int Pzk_IsValidSideDeck(const int* sideDeck)
{
	int i;
	for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
	{
		if (!Pzk_IsSideCard(sideDeck[i]))
		{
			return 0;
		}
	}
	return 1;
}

static int Pzk_IsValidDeck(const int* cards)
{
	int i, count = 0;
	for (i = 0; i < PZK_NUM_SIDECARDS; i++)
	{
		if (cards[i] < 0)
		{
			return 0;
		}
		count += cards[i];
	}
	return count >= PZK_SIDEDECK_SIZE;
}

// Does the side deck only use cards the player owns?
static int Pzk_ValidateSideDeck(const int* sideDeck, const int* cards)
{
	int count[PZK_NUM_SIDECARDS];
	int i;

	memset(count, 0, sizeof(count));
	for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
	{
		if (!Pzk_IsSideCard(sideDeck[i]))
		{
			return 0;
		}
		count[sideDeck[i] - PZCARD_PLUS_1]++;
	}
	for (i = 0; i < PZK_NUM_SIDECARDS; i++)
	{
		if (cards[i] < count[i])
		{
			return 0;
		}
	}
	return 1;
}

// Fills the empty slots of a side deck with random cards the player still has
static int Pzk_FinishSideDeck(pzkGame_t* g, int* sideDeck, const int* cards)
{
	int remaining[PZK_NUM_SIDECARDS];
	int avail[PZK_NUM_SIDECARDS * 16];
	int numAvail = 0;
	int empty = 0;
	int i, j;

	memcpy(remaining, cards, sizeof(remaining));
	for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
	{
		if (!Pzk_IsSideCard(sideDeck[i]))
		{
			sideDeck[i] = PZCARD_NONE;
			empty++;
		}
		else
		{
			remaining[sideDeck[i] - PZCARD_PLUS_1]--;
		}
	}
	for (i = 0; i < PZK_NUM_SIDECARDS; i++)
	{
		if (remaining[i] < 0)
		{
			return 0;
		}
		for (j = 0; j < remaining[i] && numAvail < (int)(sizeof(avail) / sizeof(avail[0])); j++)
		{
			avail[numAvail++] = PZCARD_PLUS_1 + i;
		}
	}
	if (empty > numAvail)
	{
		return 0;
	}
	for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
	{
		if (sideDeck[i] == PZCARD_NONE)
		{
			const int pick = Pzk_Rand(g, 0, numAvail - 1);
			sideDeck[i] = avail[pick];
			avail[pick] = avail[numAvail - 1];
			numAvail--;
		}
	}
	return 1;
}

static void Pzk_PrepareForNewSet(pzkGame_t* g)
{
	int p, i;
	for (p = 0; p < 2; p++)
	{
		for (i = 0; i < PZK_FIELD_SIZE; i++)
		{
			g->players[p].field[i] = PZCARD_NONE;
			g->players[p].fieldParams[i] = 0;
		}
		g->players[p].nextFieldSlot = 1;
		g->players[p].points = 0;
		g->players[p].standing = 0;
	}
	Pzk_GenerateDeck(g);
}

/*
===========================================================================
Setup
===========================================================================
*/

void Pzk_Init(pzkGame_t* g, const pzkSendFunc_t send, const pzkFinishFunc_t finish, const unsigned int seed)
{
	int p;
	memset(g, 0, sizeof(*g));
	g->inUse = 1;
	g->send = send;
	g->finish = finish;
	g->seed = seed ? seed : 0x5a5a1234u;
	g->showCardSelect = 1;
	g->newSet = 1;
	for (p = 0; p < 2; p++)
	{
		int i;
		g->players[p].present = 1;
		for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
		{
			g->players[p].sideDeck[i] = PZCARD_NONE;
		}
		for (i = 0; i < PZK_HAND_SIZE; i++)
		{
			g->players[p].hand[i] = PZCARD_NONE;
		}
	}
}

void Pzk_SetPlayer(pzkGame_t* g, const int pid, const char* name, const int isAI)
{
	pzkPlayer_t* p = Pzk_P(g, pid);
	int i;

	p->isAI = isAI;
	p->present = 1;
	strncpy(p->name, name ? name : (isAI ? "AI" : "Player"), sizeof(p->name) - 1);
	p->name[sizeof(p->name) - 1] = 0;
	// The name goes between quotes in the "sn" instruction
	for (i = 0; p->name[i]; i++)
	{
		if (p->name[i] == '"' || p->name[i] == ';' || p->name[i] == '\n' || p->name[i] == '\r')
		{
			p->name[i] = '\'';
		}
	}
}

void Pzk_SetCards(pzkGame_t* g, const int pid, const int* cards)
{
	memcpy(Pzk_P(g, pid)->cards, cards, sizeof(int) * PZK_NUM_SIDECARDS);
}

void Pzk_SetSideDeck(pzkGame_t* g, const int pid, const int* sideDeck)
{
	memcpy(Pzk_P(g, pid)->sideDeck, sideDeck, sizeof(int) * PZK_SIDEDECK_SIZE);
}

void Pzk_ShowCardSelection(pzkGame_t* g, const int show)
{
	g->showCardSelect = show;
}

/*
===========================================================================
Game flow
===========================================================================
*/

static void Pzk_PrepareMatch(pzkGame_t* g);
static void Pzk_RunPhase(pzkGame_t* g);
static void Pzk_RunPhase4(pzkGame_t* g);
static void Pzk_RunPhase5(pzkGame_t* g);
static void Pzk_CleanUp(pzkGame_t* g);

static void Pzk_CardSelection(pzkGame_t* g)
{
	int pid;
	g->phase = -1;

	for (pid = 1; pid <= 2; pid++)
	{
		const pzkPlayer_t* p = Pzk_P(g, pid);
		char buf[512];
		int len, i;

		if (p->isAI)
		{
			Pzk_P(g, pid)->ready = 1;
			continue;
		}
		len = snprintf(buf, sizeof(buf), "gtc sdc ");
		for (i = 0; i < PZK_NUM_SIDECARDS; i++)
		{
			len += snprintf(buf + len, sizeof(buf) - len, "%i ", p->cards[i]);
		}
		len += snprintf(buf + len, sizeof(buf) - len, "ssd ");
		for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
		{
			len += snprintf(buf + len, sizeof(buf) - len, "%i ", p->sideDeck[i]);
		}
		if (!g->aiGame)
		{
			snprintf(buf + len, sizeof(buf) - len, "sto 60 "); // PvP: 60 seconds to pick
		}
		Pzk_Send(g, pid, "%s", buf);
	}
	if (!g->aiGame)
	{
		Pzk_SetTimeout(g, 60000, PZK_TO_CARDSEL);
	}
}

const char* Pzk_StartGame(pzkGame_t* g, const int now)
{
	pzkPlayer_t* p1 = Pzk_P(g, 1);
	pzkPlayer_t* p2 = Pzk_P(g, 2);
	static const int emptyDeck[PZK_SIDEDECK_SIZE] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

	g->now = now;
	if (p1->isAI)
	{
		return "Player 1 can't be the AI";
	}

	if (g->showCardSelect)
	{
		if (!Pzk_IsValidDeck(p1->cards))
		{
			return "Invalid deck (Player 1)";
		}
		if (!Pzk_IsValidSideDeck(p1->sideDeck) || !Pzk_ValidateSideDeck(p1->sideDeck, p1->cards))
		{
			Pzk_SetSideDeck(g, 1, emptyDeck); // not mandatory, the player picks one
		}
		if (p2->isAI)
		{
			if (!Pzk_IsValidSideDeck(p2->sideDeck))
			{
				return "Invalid side deck (Player 2)";
			}
		}
		else
		{
			if (!Pzk_IsValidDeck(p2->cards))
			{
				return "Invalid deck (Player 2)";
			}
			if (!Pzk_IsValidSideDeck(p2->sideDeck) || !Pzk_ValidateSideDeck(p2->sideDeck, p2->cards))
			{
				Pzk_SetSideDeck(g, 2, emptyDeck);
			}
		}
	}
	else
	{
		if (!Pzk_IsValidSideDeck(p1->sideDeck))
		{
			return "Invalid side deck (Player 1)";
		}
		if (!Pzk_IsValidSideDeck(p2->sideDeck))
		{
			return "Invalid side deck (Player 2)";
		}
	}

	g->phase = 0;
	g->aiGame = p2->isAI;
	g->finished = 0;
	g->winner = 0;
	g->cleanedUp = 0;

	Pzk_Send(g, 1, "start");
	Pzk_Send(g, 2, "start");

	p1->ready = 0;
	p2->ready = 0;
	if (g->showCardSelect)
	{
		Pzk_CardSelection(g);
	}
	else
	{
		Pzk_PrepareMatch(g);
	}
	return NULL;
}

static void Pzk_CardSelTimeout(pzkGame_t* g)
{
	// Taking too long: make the boards send their (maybe incomplete) side decks
	if (!Pzk_P(g, 1)->ready)
	{
		Pzk_Send(g, 1, "tsdn sto 0");
	}
	if (!Pzk_P(g, 2)->ready)
	{
		Pzk_Send(g, 2, "tsdn sto 0");
	}
}

static void Pzk_PrepareMatch(pzkGame_t* g)
{
	pzkPlayer_t* p1 = Pzk_P(g, 1);
	pzkPlayer_t* p2 = Pzk_P(g, 2);
	int pid;

	Pzk_RemoveTimeout(g);
	p1->ready = 1;
	p2->ready = 1;

	Pzk_GenerateHand(g, p1);
	Pzk_GenerateHand(g, p2);

	Pzk_PrepareForNewSet(g);
	g->newSet = 0;

	p1->score = 0;
	p2->score = 0;
	p1->forfeited = 0;
	p2->forfeited = 0;

	g->firstPlayer = Pzk_Rand(g, 1, 2);
	g->turn = 0; // phase 1 sets it to firstPlayer

	for (pid = 1; pid <= 2; pid++)
	{
		const pzkPlayer_t* me = Pzk_P(g, pid);
		const pzkPlayer_t* other = Pzk_P(g, pid == 1 ? 2 : 1);
		Pzk_Send(g, pid, "gtg db 0 sto 0 sn \"%s\" \"%s\" shcs %i 0 %i 0 %i 0 %i 0 15",
			me->name, other->name, me->hand[0], me->hand[1], me->hand[2], me->hand[3]);
	}

	g->phase = 1;
	Pzk_QueuePhase(g, PZK_PHASE_DELAY);
}

// Phase 1: start a new set if needed, switch the turn
static void Pzk_RunPhase1(pzkGame_t* g)
{
	if (g->newSet)
	{
		Pzk_PrepareForNewSet(g);
		Pzk_Send(g, 1, "cf");
		Pzk_Send(g, 2, "cf");
		g->newSet = 0;
	}

	if (g->turn == 0)
	{
		g->turn = g->firstPlayer;
	}
	else
	{
		g->turn = g->turn == 1 ? 2 : 1;
	}

	Pzk_Send(g, 1, "st %i", g->turn == 1 ? 1 : 2);
	Pzk_Send(g, 2, "st %i", g->turn == 2 ? 1 : 2);

	g->phase = 2;
	Pzk_QueuePhase(g, PZK_PHASE_DELAY);
}

// Phase 2: draw a card from the main deck
static void Pzk_RunPhase2(pzkGame_t* g)
{
	int pid, oid, slot, card;
	pzkPlayer_t* player;

	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);

	if (player->standing)
	{
		g->phase = 4;
		Pzk_RunPhase4(g);
		return;
	}

	card = g->deck[g->nextDeckCard % PZK_DECK_SIZE] - 1; // deck holds 1..10, the IDs are 0..9
	g->nextDeckCard++;

	slot = player->nextFieldSlot;
	player->field[slot - 1] = card;
	player->fieldParams[slot - 1] = 0;
	player->nextFieldSlot = slot + 1;
	player->points += card + 1;

	if (player->nextFieldSlot == 10 || player->points == 20)
	{
		// Field full or 20: stand
		player->standing = 1;
		Pzk_Send(g, pid, "sc %i %i 0 spt 1 %i ss 1 1", slot, card, player->points);
		Pzk_Send(g, oid, "sc %i %i 0 spt 2 %i ss 2 1", slot + 9, card, player->points);
		g->phase = 4;
		Pzk_QueuePhase(g, PZK_PHASE_DELAY);
		return;
	}

	Pzk_Send(g, pid, "sc %i %i 0 spt 1 %i", slot, card, player->points);
	Pzk_Send(g, oid, "sc %i %i 0 spt 2 %i", slot + 9, card, player->points);

	g->phase = 3;
	Pzk_QueuePhase(g, PZK_PHASE_DELAY);
}

// 2&4 and 3&6: flip the sign of every matching card on the field
static int Pzk_FlipField(pzkPlayer_t* player, const int a, const int b, char* bufP, char* bufO, const int bufSize)
{
	int i, pts = 0;
	for (i = 0; i < PZK_FIELD_SIZE; i++)
	{
		const int card = player->field[i];
		int param = player->fieldParams[i];

		if (card == PZCARD_NORMAL_1 + a - 1 || card == PZCARD_NORMAL_1 + b - 1
			|| card == PZCARD_PLUS_1 + a - 1 || card == PZCARD_PLUS_1 + b - 1
			|| card == PZCARD_MINUS_1 + a - 1 || card == PZCARD_MINUS_1 + b - 1)
		{
			param = param == 1 ? 0 : 1;
		}
		else if (card == PZCARD_FLIP_1 + a - 1 || card == PZCARD_FLIP_1 + b - 1)
		{
			param = (param + 2) % 4; // 0 <-> 2, 1 <-> 3
		}
		if (param != player->fieldParams[i])
		{
			const size_t lp = strlen(bufP);
			const size_t lo = strlen(bufO);
			player->fieldParams[i] = param;
			snprintf(bufP + lp, bufSize - lp, "sc %i %i %i ", i + 1, card, param);
			snprintf(bufO + lo, bufSize - lo, "sc %i %i %i ", i + 1 + 9, card, param);
		}
		if (card != PZCARD_NONE)
		{
			pts += Pzk_CardValue(card, param);
		}
	}
	return pts;
}

// Puts a hand card (slot 1..4) of the player whose turn it is on the field
static void Pzk_PlayCard(pzkGame_t* g, const int handSlot, int param)
{
	char bufP[768];
	char bufO[768];
	int pid, oid, cardid, slot;
	size_t len;
	pzkPlayer_t* player;

	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);

	if (handSlot < 1 || handSlot > PZK_HAND_SIZE)
	{
		return;
	}
	cardid = player->hand[handSlot - 1];
	if (!Pzk_IsSideCard(cardid) || player->nextFieldSlot > PZK_FIELD_SIZE)
	{
		return;
	}
	player->hand[handSlot - 1] = PZCARD_NONE;

	bufP[0] = 0;
	bufO[0] = 0;
	param = Pzk_SanitizeParam(cardid, param);

	if (cardid == PZCARD_DOUBLE)
	{
		// Doubles the last card: its value becomes the param
		const int last = player->nextFieldSlot - 2;
		param = last >= 0 ? Pzk_CardValue(player->field[last], player->fieldParams[last]) : 0;
	}
	else if (cardid == PZCARD_TIEBREAKER)
	{
		g->tieBreaker = 1;
	}
	else if (cardid == PZCARD_2N4)
	{
		param = 1;
		player->points = Pzk_FlipField(player, 2, 4, bufP, bufO, sizeof(bufP));
	}
	else if (cardid == PZCARD_3N6)
	{
		param = 1;
		player->points = Pzk_FlipField(player, 3, 6, bufP, bufO, sizeof(bufP));
	}

	slot = player->nextFieldSlot;
	player->field[slot - 1] = cardid;
	player->fieldParams[slot - 1] = param;
	player->points += Pzk_CardValue(cardid, param);

	len = strlen(bufP);
	snprintf(bufP + len, sizeof(bufP) - len, "sc %i %i %i spt 1 %i shc %i -1 0 ", slot, cardid, param, player->points, handSlot);
	len = strlen(bufO);
	snprintf(bufO + len, sizeof(bufO) - len, "sc %i %i %i spt 2 %i shc %i -1 0 ", slot + 9, cardid, param, player->points, handSlot + 4);
	player->nextFieldSlot = slot + 1;

	Pzk_Send(g, pid, "%s", bufP);
	Pzk_Send(g, oid, "%s", bufO);
}

/*
===========================================================================
AI (the KotOR AI as described by its author, see "Pazaak AI.txt")
===========================================================================
*/

typedef struct
{
	int slot;  // hand slot 1..4
	int param;
	int value;
} pzkAIOption_t;

static int Pzk_AIOptions(const pzkPlayer_t* self, pzkAIOption_t* out)
{
	int n = 0, i;
	for (i = 0; i < PZK_HAND_SIZE; i++)
	{
		const int card = self->hand[i];
		if (card >= PZCARD_PLUS_1 && card <= PZCARD_MINUS_6)
		{
			out[n].slot = i + 1;
			out[n].param = 0;
			out[n].value = Pzk_CardValue(card, 0);
			n++;
		}
		else if ((card >= PZCARD_FLIP_1 && card <= PZCARD_FLIP_6) || card == PZCARD_TIEBREAKER)
		{
			int s;
			for (s = 0; s < 2; s++)
			{
				out[n].slot = i + 1;
				out[n].param = s;
				out[n].value = Pzk_CardValue(card, s);
				n++;
			}
		}
		else if (card == PZCARD_FLIP12)
		{
			int s;
			for (s = 0; s < 4; s++)
			{
				out[n].slot = i + 1;
				out[n].param = s;
				out[n].value = Pzk_CardValue(card, s);
				n++;
			}
		}
		// The AI does not use double, 2&4 and 3&6 (as in the Lua code)
	}
	return n;
}

static void Pzk_AINormal(pzkGame_t* g, int* outSlot, int* outParam, int* outStand)
{
	const pzkPlayer_t* self = Pzk_P(g, 2);
	const pzkPlayer_t* opponent = Pzk_P(g, 1);
	pzkAIOption_t opts[PZK_HAND_SIZE * 4];
	const int numOpts = Pzk_AIOptions(self, opts);
	int lowest = self->points;      // lowest total we can reach with a hand card
	int best = self->points;        // closest we can get to 20 with a hand card
	int bestOpt = -1;
	int use = -1;
	int i;

	for (i = 0; i < numOpts; i++)
	{
		const int total = self->points + opts[i].value;
		if (total < lowest)
		{
			lowest = total;
		}
		if (total <= 20 && total > best)
		{
			best = total;
			bestOpt = i;
		}
	}

	if (self->points > 20)
	{
		// Bust: see if a card saves us
		if (lowest < 21)
		{
			int saveTotal = -1000;
			int saveOpt = -1;
			for (i = 0; i < numOpts; i++)
			{
				const int total = self->points + opts[i].value;
				if (total <= 20 && total > saveTotal)
				{
					saveTotal = total;
					saveOpt = i;
				}
			}
			if (saveOpt >= 0)
			{
				if (opponent->points > 17)
				{
					// Only worth a card if it gives us a chance, or if losing the set loses the match
					if (opponent->score == 2 || saveTotal >= opponent->points)
					{
						use = saveOpt;
					}
				}
				else
				{
					use = saveOpt;
				}
			}
			if (use >= 0)
			{
				best = saveTotal;
			}
		}
	}
	else if (best >= 18 && bestOpt >= 0)
	{
		if (best == 20)
		{
			use = bestOpt;
		}
		else if (best == 19)
		{
			if (opponent->points != 20 && lowest <= 11)
			{
				use = bestOpt;
			}
		}
		else if (best == 18)
		{
			if (opponent->points <= 18 && lowest <= 13)
			{
				use = bestOpt;
			}
		}
	}

	if (use < 0)
	{
		best = self->points;
	}

	*outStand = 0;
	if (best == 20)
	{
		*outStand = 1;
	}
	else if (opponent->standing && opponent->points <= 20 && best > opponent->points && best <= 20)
	{
		*outStand = 1; // the player stands below us: stand and win
	}
	else if (opponent->points > best)
	{
		*outStand = 0; // never stand when the player is ahead
	}
	else if (best > 17 && best <= 20 && opponent->points < best)
	{
		*outStand = 1;
	}

	*outSlot = use >= 0 ? opts[use].slot : 0;
	*outParam = use >= 0 ? opts[use].param : 0;
}

// Phase 3: the player may use a hand card, stand or end the turn; the AI decides at once
static void Pzk_RunPhase3(pzkGame_t* g)
{
	int pid, oid;
	pzkPlayer_t* player;

	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);

	if (player->isAI)
	{
		int slot, param, stand;
		Pzk_AINormal(g, &slot, &param, &stand);
		if (slot)
		{
			Pzk_PlayCard(g, slot, param);
			if (player->nextFieldSlot == 10 || player->points == 20)
			{
				stand = 1;
			}
		}
		if (stand)
		{
			player->standing = 1;
			Pzk_Send(g, pid, "ss 1 1 ");
			Pzk_Send(g, oid, "ss 2 1 ");
		}
		g->phase = 4;
		Pzk_QueuePhase(g, PZK_PHASE_DELAY);
		return;
	}

	player->canUseCard = 1;
	if (!g->aiGame)
	{
		// 15 seconds to choose, then it counts as end turn
		Pzk_SetTimeout(g, 15000, PZK_TO_ENDTURN);
		Pzk_Send(g, pid, "db 1 sto 15");
		Pzk_Send(g, oid, "sto 15");
	}
	else
	{
		Pzk_Send(g, pid, "db 1");
	}
}

static void Pzk_P3EndTurn(pzkGame_t* g)
{
	int pid, oid;
	Pzk_RemoveTimeout(g);
	Pzk_GetTurnIDs(g, &pid, &oid);
	Pzk_P(g, pid)->canUseCard = 0;
	Pzk_Send(g, pid, "sto 0 db 0");
	Pzk_Send(g, oid, "sto 0");
	g->phase = 4;
	Pzk_QueuePhase(g, PZK_PHASE_DELAY);
}

static void Pzk_P3Stand(pzkGame_t* g)
{
	int pid, oid;
	pzkPlayer_t* player;

	Pzk_RemoveTimeout(g);
	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);
	player->standing = 1;
	player->canUseCard = 0;
	Pzk_Send(g, pid, "sto 0 db 0 ss 1 1 ");
	Pzk_Send(g, oid, "sto 0 ss 2 1 ");
	g->phase = 4;
	Pzk_QueuePhase(g, PZK_PHASE_DELAY);
}

static void Pzk_P3UseCard(pzkGame_t* g, const int handSlot, const int param)
{
	int pid, oid;
	pzkPlayer_t* player;

	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);
	if (!player->canUseCard || handSlot < 1 || handSlot > PZK_HAND_SIZE || !Pzk_IsSideCard(player->hand[handSlot - 1]))
	{
		return;
	}
	player->canUseCard = 0; // one card per turn

	Pzk_PlayCard(g, handSlot, param);

	if (player->nextFieldSlot == 10 || player->points == 20)
	{
		Pzk_RemoveTimeout(g);
		player->standing = 1;
		Pzk_Send(g, pid, "sto 0 db 0 ss 1 1 ");
		Pzk_Send(g, oid, "ss 2 1 ");
		g->phase = 4;
		Pzk_QueuePhase(g, PZK_PHASE_DELAY);
	}
	else
	{
		Pzk_Send(g, pid, "db 2");
	}
}

// Shows a set dialog on both boards (dlgP for the player whose turn it was, dlgO for the opponent)
static void Pzk_SetDialog(pzkGame_t* g, pzkPlayer_t* player, pzkPlayer_t* opponent, char* bufP, char* bufO, const int size,
	const int dlgP, const int dlgO)
{
	size_t len = strlen(bufP);
	snprintf(bufP + len, size - len, "sd %i ", dlgP);
	len = strlen(bufO);
	snprintf(bufO + len, size - len, "sd %i ", dlgO);
	player->dlgAccepted = 0;
	opponent->dlgAccepted = 0;
	g->dialogPending = 1;
	if (g->aiGame)
	{
		Pzk_P(g, 2)->dlgAccepted = 1;
	}
	else
	{
		len = strlen(bufP);
		snprintf(bufP + len, size - len, "sto 8 ");
		len = strlen(bufO);
		snprintf(bufO + len, size - len, "sto 8 ");
	}
}

// Phase 4: end of turn, check for a won/lost/tied set
static void Pzk_RunPhase4(pzkGame_t* g)
{
	char bufP[256];
	char bufO[256];
	int pid, oid;
	int cont = 1;
	int gotoPhase5 = 0;
	int setWinner = 0; // 0 = none/tie, else pid
	pzkPlayer_t* player;
	pzkPlayer_t* opponent;

	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);
	opponent = Pzk_P(g, oid);
	bufP[0] = 0;
	bufO[0] = 0;

	if (player->points > 20)
	{
		cont = 0;
		setWinner = oid; // busted
	}
	else if (player->standing && opponent->standing)
	{
		const int pFull = player->nextFieldSlot == 10;
		const int oFull = opponent->nextFieldSlot == 10;
		cont = 0;
		if (pFull && !oFull)
		{
			setWinner = pid;
		}
		else if (oFull && !pFull)
		{
			setWinner = oid;
		}
		else if (player->points > opponent->points)
		{
			setWinner = pid;
		}
		else if (player->points < opponent->points)
		{
			setWinner = oid;
		}
		else if (g->tieBreaker)
		{
			setWinner = pid; // the tie-breaker card wins a tied set
		}
	}

	if (!cont)
	{
		if (setWinner)
		{
			pzkPlayer_t* w = Pzk_P(g, setWinner);
			w->score++;
			if (setWinner == pid)
			{
				snprintf(bufP, sizeof(bufP), "ssc 1 %i ", w->score);
				snprintf(bufO, sizeof(bufO), "ssc 2 %i ", w->score);
			}
			else
			{
				snprintf(bufP, sizeof(bufP), "ssc 2 %i ", w->score);
				snprintf(bufO, sizeof(bufO), "ssc 1 %i ", w->score);
			}
			if (w->score == 3)
			{
				gotoPhase5 = 1;
			}
			else if (setWinner == pid)
			{
				Pzk_SetDialog(g, player, opponent, bufP, bufO, sizeof(bufP), PDLGID_WONSET, PDLGID_LOSTSET);
			}
			else
			{
				Pzk_SetDialog(g, player, opponent, bufP, bufO, sizeof(bufP), PDLGID_LOSTSET, PDLGID_WONSET);
			}
		}
		else
		{
			Pzk_SetDialog(g, player, opponent, bufP, bufO, sizeof(bufP), PDLGID_TIESET, PDLGID_TIESET);
		}
		Pzk_Send(g, pid, "%s", bufP);
		Pzk_Send(g, oid, "%s", bufO);
	}

	g->tieBreaker = 0;
	if (cont)
	{
		g->phase = 1;
		Pzk_QueuePhase(g, PZK_PHASE_DELAY);
	}
	else if (gotoPhase5)
	{
		g->phase = 5;
		Pzk_RunPhase5(g);
	}
	else
	{
		// Set finished: wait for the dialogs, then start the next set
		g->newSet = 1;
		g->firstPlayer = g->firstPlayer == 1 ? 2 : 1;
		g->turn = 0;
		if (!g->aiGame)
		{
			Pzk_SetTimeout(g, 8000, PZK_TO_DIALOG);
		}
	}
}

// Phase 5: the match is over
static void Pzk_RunPhase5(pzkGame_t* g)
{
	int pid, oid, pdlg, odlg;
	pzkPlayer_t* player;
	pzkPlayer_t* opponent;

	Pzk_GetTurnIDs(g, &pid, &oid);
	player = Pzk_P(g, pid);
	opponent = Pzk_P(g, oid);

	g->finished = 1;

	if (player->forfeited || opponent->forfeited)
	{
		if (player->forfeited)
		{
			g->winner = oid;
			odlg = PDLGID_WINFORFEIT;
			pdlg = PDLGID_LOSEFORFEIT;
		}
		else
		{
			g->winner = pid;
			odlg = PDLGID_LOSEFORFEIT;
			pdlg = PDLGID_WINFORFEIT;
		}
	}
	else if (player->score == 3)
	{
		g->winner = pid;
		odlg = PDLGID_LOSTMATCH;
		pdlg = PDLGID_WONMATCH;
	}
	else
	{
		g->winner = oid;
		odlg = PDLGID_WONMATCH;
		pdlg = PDLGID_LOSTMATCH;
	}

	player->dlgAccepted = 0;
	opponent->dlgAccepted = 0;
	g->dialogPending = 1;

	if (!player->present && !opponent->present)
	{
		Pzk_CleanUp(g);
		return;
	}
	if (!g->aiGame)
	{
		Pzk_Send(g, pid, "sd %i sto 8", pdlg);
		Pzk_Send(g, oid, "sd %i sto 8", odlg);
		Pzk_SetTimeout(g, 8000, PZK_TO_CLEANUP);
		// A player who left can't confirm
		if (!player->present)
		{
			player->dlgAccepted = 1;
		}
		if (!opponent->present)
		{
			opponent->dlgAccepted = 1;
		}
	}
	else
	{
		Pzk_Send(g, pid, "sd %i", pdlg);
		Pzk_Send(g, oid, "sd %i", odlg);
		Pzk_P(g, 2)->dlgAccepted = 1;
		if (!Pzk_P(g, 1)->present)
		{
			Pzk_CleanUp(g);
		}
	}
}

static void Pzk_CleanUp(pzkGame_t* g)
{
	if (g->cleanedUp)
	{
		return;
	}
	g->cleanedUp = 1;
	g->phaseAt = 0;
	Pzk_RemoveTimeout(g);
	g->dialogPending = 0;

	Pzk_Send(g, 1, "stop");
	Pzk_Send(g, 2, "stop");

	g->inUse = 0;
	if (g->finish)
	{
		g->finish(g, g->winner); // the host may reuse the game after this
	}
}

void Pzk_Abort(pzkGame_t* g)
{
	if (!g->inUse)
	{
		return;
	}
	g->winner = 0;
	Pzk_CleanUp(g);
}

static void Pzk_DialogConfirmed(pzkGame_t* g)
{
	Pzk_RemoveTimeout(g);
	g->dialogPending = 0;
	Pzk_Send(g, 1, "cd sto 0");
	Pzk_Send(g, 2, "cd sto 0");

	if (g->phase == 4)
	{
		g->phase = 1;
		Pzk_QueuePhase(g, PZK_PHASE_DELAY);
	}
	else if (g->phase == 5)
	{
		Pzk_CleanUp(g);
	}
}

static void Pzk_AcceptDialog(pzkGame_t* g, const int pid)
{
	if (!g->dialogPending)
	{
		return;
	}
	Pzk_P(g, pid)->dlgAccepted = 1;
	if (Pzk_P(g, 1)->dlgAccepted && Pzk_P(g, 2)->dlgAccepted)
	{
		Pzk_DialogConfirmed(g);
	}
}

static void Pzk_Forfeit(pzkGame_t* g, const int pid)
{
	if (g->finished)
	{
		return;
	}
	Pzk_P(g, pid)->forfeited = 1;
	g->phaseAt = 0;
	Pzk_RemoveTimeout(g);
	Pzk_Send(g, 1, "db 0 sto 0");
	Pzk_Send(g, 2, "db 0 sto 0");
	if (g->turn == 0)
	{
		g->turn = pid; // RunPhase5 works with the turn IDs
	}
	g->phase = 5;
	Pzk_QueuePhase(g, PZK_PHASE_DELAY);
}

// Quit in the card selection screen
static void Pzk_PlayerQuit(pzkGame_t* g, const int pid)
{
	const int oid = pid == 1 ? 2 : 1;
	pzkPlayer_t* player = Pzk_P(g, pid);
	pzkPlayer_t* opponent = Pzk_P(g, oid);

	if (g->phase != -1)
	{
		return;
	}
	g->winner = oid;
	g->phaseAt = 0;
	Pzk_RemoveTimeout(g);
	g->finished = 1;
	g->phase = 5;
	g->dialogPending = 1;

	player->dlgAccepted = !player->present;
	opponent->dlgAccepted = !opponent->present;
	if (!g->aiGame)
	{
		Pzk_Send(g, pid, "sd %i sto 8", PDLGID_LOSEFORFEIT);
		Pzk_Send(g, oid, "sd %i sto 8", PDLGID_WINFORFEIT);
		Pzk_SetTimeout(g, 8000, PZK_TO_CLEANUP);
	}
	else
	{
		Pzk_Send(g, pid, "sd %i", PDLGID_LOSEQUIT);
		Pzk_P(g, 2)->dlgAccepted = 1;
	}
	if (player->dlgAccepted && opponent->dlgAccepted)
	{
		Pzk_CleanUp(g);
	}
}

static void Pzk_PlayerSetSideDeck(pzkGame_t* g, const int pid, const int argc, const char** argv, const int partial)
{
	const int oid = pid == 1 ? 2 : 1;
	pzkPlayer_t* player = Pzk_P(g, pid);
	int i;

	if (g->phase != -1 || player->ready)
	{
		return;
	}
	for (i = 0; i < PZK_SIDEDECK_SIZE; i++)
	{
		player->sideDeck[i] = i + 1 < argc ? atoi(argv[i + 1]) : PZCARD_NONE;
		if (!Pzk_IsSideCard(player->sideDeck[i]))
		{
			player->sideDeck[i] = PZCARD_NONE;
		}
	}
	if (partial)
	{
		Pzk_FinishSideDeck(g, player->sideDeck, player->cards);
	}

	// The board checks the side deck, so a bad one means a cheater: he loses
	if (!Pzk_IsValidSideDeck(player->sideDeck) || !Pzk_ValidateSideDeck(player->sideDeck, player->cards))
	{
		Pzk_PlayerQuit(g, pid);
		return;
	}
	player->ready = 1;
	if (Pzk_P(g, oid)->ready)
	{
		Pzk_PrepareMatch(g);
	}
}

/*
===========================================================================
Entry points
===========================================================================
*/

static void Pzk_RunPhase(pzkGame_t* g)
{
	Pzk_RemoveTimeout(g);
	switch (g->phase)
	{
	case 1: Pzk_RunPhase1(g); break;
	case 2: Pzk_RunPhase2(g); break;
	case 3: Pzk_RunPhase3(g); break;
	case 4: Pzk_RunPhase4(g); break;
	case 5: Pzk_RunPhase5(g); break;
	default: break;
	}
}

void Pzk_Frame(pzkGame_t* g, const int now)
{
	if (!g->inUse)
	{
		return;
	}
	g->now = now;

	if (g->phaseAt && now >= g->phaseAt)
	{
		g->phaseAt = 0;
		Pzk_RunPhase(g);
		if (!g->inUse)
		{
			return;
		}
	}

	if (g->timeoutAt && now >= g->timeoutAt)
	{
		const int action = g->timeoutAction;
		Pzk_RemoveTimeout(g);
		switch (action)
		{
		case PZK_TO_CARDSEL: Pzk_CardSelTimeout(g); break;
		case PZK_TO_ENDTURN: Pzk_P3EndTurn(g); break;
		case PZK_TO_DIALOG: Pzk_DialogConfirmed(g); break;
		case PZK_TO_CLEANUP: Pzk_CleanUp(g); break;
		default: break;
		}
	}
}

void Pzk_Command(pzkGame_t* g, const int pid, const int argc, const char** argv, const int now)
{
	const char* cmd;

	if (!g->inUse || pid < 1 || pid > 2 || argc < 1 || Pzk_P(g, pid)->isAI || !Pzk_P(g, pid)->present)
	{
		return;
	}
	g->now = now;
	cmd = argv[0];

	if (!strcmp(cmd, "forfeit"))
	{
		Pzk_Forfeit(g, pid);
		return;
	}
	if (!strcmp(cmd, "acceptdlg"))
	{
		Pzk_AcceptDialog(g, pid);
		return;
	}
	if (!strcmp(cmd, "quit"))
	{
		Pzk_PlayerQuit(g, pid);
		return;
	}
	if (!strcmp(cmd, "setsd"))
	{
		Pzk_PlayerSetSideDeck(g, pid, argc, argv, 0);
		return;
	}
	if (!strcmp(cmd, "setsdt"))
	{
		Pzk_PlayerSetSideDeck(g, pid, argc, argv, 1);
		return;
	}

	// The rest only for the player whose turn it is, in phase 3
	if (g->turn != pid || g->phase != 3 || g->finished)
	{
		return;
	}
	if (!strcmp(cmd, "endturn"))
	{
		Pzk_P3EndTurn(g);
	}
	else if (!strcmp(cmd, "stand"))
	{
		Pzk_P3Stand(g);
	}
	else if (!strcmp(cmd, "usecard"))
	{
		const int slot = argc > 1 ? atoi(argv[1]) : 0;
		const int param = argc > 2 ? atoi(argv[2]) : 0;
		Pzk_P3UseCard(g, slot, param);
	}
}

void Pzk_PlayerGone(pzkGame_t* g, const int pid, const int now)
{
	const int oid = pid == 1 ? 2 : 1;
	pzkPlayer_t* player;

	if (!g->inUse || pid < 1 || pid > 2)
	{
		return;
	}
	g->now = now;
	player = Pzk_P(g, pid);
	if (!player->present)
	{
		return;
	}
	player->present = 0;

	if (g->finished)
	{
		// Only the dialogs are left
		player->dlgAccepted = 1;
		if (Pzk_P(g, oid)->dlgAccepted || Pzk_P(g, oid)->isAI || !Pzk_P(g, oid)->present)
		{
			Pzk_CleanUp(g);
		}
		return;
	}
	if (Pzk_P(g, oid)->isAI || !Pzk_P(g, oid)->present)
	{
		// Nobody left to tell
		Pzk_Abort(g);
		return;
	}
	if (g->phase == -1)
	{
		Pzk_PlayerQuit(g, pid);
		return;
	}
	Pzk_Forfeit(g, pid);
}