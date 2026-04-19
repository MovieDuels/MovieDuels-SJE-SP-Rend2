// g_pazaak.cpp
// Basic Pazaak core scaffolding and client command handler
// This file provides an initial C++ representation of the Pazaak game state
// and a small set of commands the client UI/menu can call. The full game
// rules/AI from JKGalaxies' `pazaak.lua` should be ported incrementally.

#include "g_local.h"
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <time.h>

namespace Pazaak {

struct Card {
    int value; // numeric card value (1..10 for now)
    int id;    // optional id
};

class Game {
public:
    Game() { Reset(); }

    void Reset() {
        active = false;
        player1 = nullptr;
        player2 = nullptr;
        deck.clear();
        nextDeckIndex = 0;
        sideDeck1.assign(10, -1);
        sideDeck2.assign(10, -1);
        hand1.assign(4, -1);
        hand2.assign(4, -1);
        for (int p = 0; p < 2; ++p) {
            for (int i = 0; i < 9; ++i) field[p][i] = -1;
            nextFieldSlot[p] = 0;
            points[p] = 0;
            standing[p] = false;
            score[p] = 0;
        }
        // betting state
        pot = 0;
        bets[0] = bets[1] = 0;
    }

    bool Start(gentity_t* p1, gentity_t* p2) {
        if (!p1) return false;
        Reset();
        player1 = p1;
        player2 = p2;
        active = true;

        deck = GenerateDeck();
        std::srand((unsigned)time(nullptr));
        std::random_shuffle(deck.begin(), deck.end());
        nextDeckIndex = 0;

        SendServerMessage(player1, "Pazaak: game started (server)");
        if (player2) SendServerMessage(player2, "Pazaak: game started (server)");

        // send initial state (deck + placeholder side-decks and hands)
        SendInitialStateToClients();

        return true;
    }

    void Stop() {
        if (!active) return;
        active = false;
        SendServerMessage(player1, "Pazaak: game stopped");
        if (player2) SendServerMessage(player2, "Pazaak: game stopped");
        player1 = player2 = nullptr;
        deck.clear();
    }

    void UseCard(gentity_t* who, int handIndex) {
        if (!active || !who) return;
        int pid = GetPlayerIndex(who);
        if (pid < 0) return;
        std::vector<int>& hand = (pid == 0) ? hand1 : hand2;
        if (handIndex < 0 || handIndex >= (int)hand.size()) return;
        int card = hand[handIndex];
        if (card < 0) {
            SendServerMessage(who, "Pazaak: no card at that hand slot");
            return;
        }
        hand[handIndex] = -1; // consume
        int slot = nextFieldSlot[pid]++;
        if (slot >= 9) {
            SendServerMessage(who, "Pazaak: no field space");
            return;
        }
        field[pid][slot] = card;
        points[pid] += card;
        Broadcast(va("pzk sc %d %d %d", pid * 9 + slot, card, points[pid]));
    }

    void DrawCard(gentity_t* who) {
        if (!active || !who) return;
        int pid = GetPlayerIndex(who);
        if (pid < 0) return;
        if (nextDeckIndex >= (int)deck.size()) {
            deck = GenerateDeck();
            std::random_shuffle(deck.begin(), deck.end());
            nextDeckIndex = 0;
        }
        int card = deck[nextDeckIndex++];
        int slot = nextFieldSlot[pid]++;
        if (slot >= 9) {
            standing[pid] = true;
            Broadcast(va("pzk st %d 1", pid + 1));
            return;
        }
        field[pid][slot] = card;
        points[pid] += card;
        Broadcast(va("pzk sc %d %d %d", pid * 9 + slot, card, points[pid]));
    }

    bool PlaceBet(gentity_t* who, int amount) {
        if (!who || !who->client) return false;
        if (amount <= 0) return false;
        int pid = GetPlayerIndex(who);
        if (pid < 0) {
            // If the player isn't one of the current players, reject
            SendServerMessage(who, "Pazaak: you're not in the current game");
            return false;
        }
        int credits = who->client->ps.persistant[PERS_CREDITS];
        if (credits < amount) {
            SendServerMessage(who, "Pazaak: insufficient credits");
            return false;
        }
        who->client->ps.persistant[PERS_CREDITS] = credits - amount;
        int remaining = who->client->ps.persistant[PERS_CREDITS];
        bets[pid] += amount;
        pot += amount;
        // Broadcast: pzk bet <player> <amount> <pot> <remaining_credits>
        Broadcast(va("pzk bet %d %d %d %d", pid, amount, pot, remaining));
        return true;
    }

    void Stand(gentity_t* who) {
        if (!active || !who) return;
        int pid = GetPlayerIndex(who);
        if (pid < 0) return;
        standing[pid] = true;
        Broadcast(va("pzk st %d 1", pid + 1));
    }

    // Per-frame update: handle AI, timeouts, and round/set transitions
    void Update(const int level_time) {
        if (!active) return;

        // start a round if none is active
        if (!roundActive) {
            if (level_time >= scheduledRoundStart) {
                StartRound();
            }
            return;
        }

        // quick end checks
        for (int p = 0; p < 2; ++p) {
            if (points[p] == 20) {
                // instant round win
                EndRound(p);
                return;
            }
            if (points[p] > 20) {
                // bust -> other player wins
                EndRound(p == 0 ? 1 : 0);
                return;
            }
        }

        // if both standing, resolve
        if (standing[0] && standing[1]) {
            int winner = ResolveWinner();
            EndRound(winner);
            return;
        }

        // If it's AI's turn (no real player) invoke simple AI
        if (IsAITurn()) {
            // simple AI: act every 800ms
            if (level_time >= nextAIThink) {
                AI_TakeTurn();
                nextAIThink = level_time + 800;
            }
        }
    }

    void ScheduleNextRound(const int level_time, int delayMs = 1000) {
        scheduledRoundStart = level_time + delayMs;
    }

    bool IsAITurn() const {
        if (!active) return false;
        // if player2 is a real human client (not an NPC), do not treat as AI
        if (player2 && player2->client && player2->NPC == nullptr) return false;
        // it's AI's turn if currentPlayer == 1
        return roundActive && currentPlayer == 1;
    }

    void AI_TakeTurn() {
        // Basic heuristic AI
        const int pid = 1;
        if (standing[pid]) return;

        // prefer to use side cards that get close to 20 without busting
        int bestHandIdx = -1;
        int bestVal = -1000;
        for (int i = 0; i < (int)hand2.size(); ++i) {
            int c = hand2[i];
            if (c < 0) continue;
            int newPoints = points[pid] + c;
            if (newPoints <= 20 && newPoints > bestVal) {
                bestVal = newPoints;
                bestHandIdx = i;
            }
        }
        if (bestHandIdx >= 0 && bestVal >= 17) {
            UseCard(player2, bestHandIdx);
            // after using card, give turn back to player 0
            currentPlayer = 0;
            return;
        }

        // if low points, draw
        if (points[pid] < 16) {
            DrawCard(player2);
            currentPlayer = 0;
            return;
        }

        // otherwise stand
        Stand(player2);
        currentPlayer = 0;
    }

    int ResolveWinner() const {
        // returns winner player index, -1 for tie
        const int p0 = (points[0] > 20) ? -999 : points[0];
        const int p1 = (points[1] > 20) ? -999 : points[1];
        if (p0 == p1) return -1;
        return (p0 > p1) ? 0 : 1;
    }

    void StartRound() {
        roundActive = true;
        nextFieldSlot[0] = nextFieldSlot[1] = 0;
        for (int p = 0; p < 2; ++p) {
            for (int i = 0; i < 9; ++i) field[p][i] = -1;
            points[p] = 0;
            standing[p] = false;
        }

        // ensure side decks exist
        EnsureSideDecks();

        // deal hands (first 4 from side deck)
        for (int i = 0; i < 4; ++i) {
            hand1[i] = (i < (int)sideDeck1.size()) ? sideDeck1[i] : -1;
            hand2[i] = (i < (int)sideDeck2.size()) ? sideDeck2[i] : -1;
        }

        // alternate starter
        currentPlayer = (roundStarter++ % 2);
        Broadcast(va("pzk nr %d", currentPlayer));
    }

    void EndRound(int winner) {
        roundActive = false;
        if (winner >= 0) {
            score[winner]++;
        }
        Broadcast(va("pzk roundend %d %d %d", winner, score[0], score[1]));

        // check for set win (first to 3)
        if (score[0] >= 3 || score[1] >= 3) {
            int setWinner = (score[0] >= 3) ? 0 : 1;
            Broadcast(va("pzk setend %d", setWinner));
            Stop();
            return;
        }

        // schedule next round
        ScheduleNextRound(level.time, 1200);
    }

    bool IsActive() const { return active; }

    void SendInitialStateToClients() {
        // Build simple gtc/sdc message: deck + sidedecks (placeholders) + hands
        if (player1 && player1->client) {
            std::string msg = "pzk gtc sdc ";
            for (size_t i = 0; i < deck.size(); ++i) msg += va("%d ", deck[i]);
            msg += " ssd ";
            for (int i = 0; i < 10; ++i) msg += va("%d ", sideDeck1[i]);
            gi.SendServerCommand(player1 - g_entities, msg.c_str());
        }
        if (player2 && player2->client) {
            std::string msg = "pzk gtc sdc ";
            for (size_t i = 0; i < deck.size(); ++i) msg += va("%d ", deck[i]);
            msg += " ssd ";
            for (int i = 0; i < 10; ++i) msg += va("%d ", sideDeck2[i]);
            gi.SendServerCommand(player2 - g_entities, msg.c_str());
        }
    }

private:
    std::vector<int> GenerateDeck() {
        std::vector<int> d;
        for (int i = 0; i < 4; ++i) d.push_back(1);
        for (int i = 0; i < 4; ++i) d.push_back(2);
        for (int i = 0; i < 4; ++i) d.push_back(3);
        for (int i = 0; i < 3; ++i) d.push_back(4);
        for (int i = 0; i < 2; ++i) d.push_back(5);
        for (int i = 0; i < 2; ++i) d.push_back(6);
        d.push_back(7); d.push_back(8); d.push_back(9); d.push_back(10);
        return d;
    }

    int GetPlayerIndex(gentity_t* who) {
        if (who == player1) return 0;
        if (who == player2) return 1;
        return -1;
    }

    void EnsureSideDecks() {
        bool have1 = false;
        for (int v : sideDeck1) if (v >= 0) { have1 = true; break; }
        bool have2 = false;
        for (int v : sideDeck2) if (v >= 0) { have2 = true; break; }

        if (!have1) {
            sideDeck1.clear();
            for (int i = 0; i < 10; ++i) sideDeck1.push_back((rand() % 10) + 1);
        }
        if (!have2) {
            sideDeck2.clear();
            for (int i = 0; i < 10; ++i) sideDeck2.push_back((rand() % 10) + 1);
        }
    }

    void Broadcast(const char* msg) {
        if (player1 && player1->client) gi.SendServerCommand(player1 - g_entities, msg);
        if (player2 && player2->client) gi.SendServerCommand(player2 - g_entities, msg);
    }

    void SendServerMessage(gentity_t* ent, const char* msg) {
        if (!ent || !ent->client) return;
        gi.SendServerCommand(ent - g_entities, va("print \"%s\n\"", msg));
    }

    bool active;
    gentity_t* player1;
    gentity_t* player2;
    std::vector<int> deck;
    int nextDeckIndex;
    std::vector<int> sideDeck1;
    std::vector<int> sideDeck2;
    std::vector<int> hand1;
    std::vector<int> hand2;
    int field[2][9];
    int nextFieldSlot[2];
    int points[2];
    bool standing[2];
    int score[2];
    int pot; // current betting pot
    int bets[2]; // individual player bets
    // runtime state
    bool roundActive = false;
    int scheduledRoundStart = 0;
    int currentPlayer = 0;
    int roundStarter = 0;
    int nextAIThink = 0;
};

static Game s_Game;

} // namespace Pazaak

// Public hooks for game code and client command processing
extern "C" {
void G_Pazaak_Init(void) {
    Pazaak::s_Game.Reset();
}

bool G_Pazaak_Start(gentity_t* p1, gentity_t* p2) {
    return Pazaak::s_Game.Start(p1, p2);
}

void G_Pazaak_Stop(void) {
    Pazaak::s_Game.Stop();
}

// Handle client-issued pzk commands: called from ClientCommand when a client
// executes `pzk <subcmd> [args...]`.
void G_Pazaak_ProcessClientCommand(gentity_t* ent) {
    if (!ent || !ent->client) return;
    const char* sub = gi.argv(1);
    if (!sub || !sub[0]) {
        gi.SendServerCommand(ent - g_entities, "print \"pzk: missing subcommand\n\"");
        return;
    }

    if (Q_stricmp(sub, "start") == 0) {
        // optional target client id as argv(2)
        gentity_t* opponent = nullptr;
        if (gi.argc() > 2) {
            int target = atoi(gi.argv(2));
            if (target >= 0 && target < MAX_CLIENTS && g_entities[target].client) {
                opponent = &g_entities[target];
            }
        }
        if (!Pazaak::s_Game.Start(ent, opponent)) {
            gi.SendServerCommand(ent - g_entities, "print \"pzk: failed to start game\n\"");
        }
    }
    else if (Q_stricmp(sub, "stop") == 0) {
        Pazaak::s_Game.Stop();
    }
    else if (Q_stricmp(sub, "bet") == 0) {
        int amt = 0;
        if (gi.argc() > 2) amt = atoi(gi.argv(2));
        if (amt <= 0) {
            gi.SendServerCommand(ent - g_entities, "print \"pzk: usage: pzk bet <amount>\n\"");
        }
        else {
            Pazaak::s_Game.PlaceBet(ent, amt);
        }
    }
    else if (Q_stricmp(sub, "usecard") == 0) {
        int idx = -1;
        if (gi.argc() > 2) idx = atoi(gi.argv(2));
        Pazaak::s_Game.UseCard(ent, idx);
    }
    else if (Q_stricmp(sub, "draw") == 0) {
        Pazaak::s_Game.DrawCard(ent);
    }
    else if (Q_stricmp(sub, "stand") == 0) {
        Pazaak::s_Game.Stand(ent);
    }
    else {
        gi.SendServerCommand(ent - g_entities, va("print \"pzk: unknown subcommand '%s'\n\"", sub));
    }
}

} // extern C

extern "C" bool G_Pazaak_PlaceBet(gentity_t* who, int amount) {
    return Pazaak::s_Game.PlaceBet(who, amount);
}

// Per-frame run hook called from the main game loop
extern "C" void G_Pazaak_RunFrame(int level_time) {
    Pazaak::s_Game.Update(level_time);
}

