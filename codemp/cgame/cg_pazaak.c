// cg_pazaak.c
// Minimal client-side Pazaak UI state + server command handler

#include "cg_local.h"
#include "cg_pazaak.h"
#include "ui/jamp/menudef.h"

static int pzk_field[2][9];
static int pzk_points[2];
static int pzk_score[2];
static int pzk_hand[2][4];
static int pzk_side[2][10];
static int pzk_currentStarter = 0;
static qboolean pzk_active = qfalse;
static int pzk_pot = 0;
static int pzk_credits[2];

void CG_Pazaak_Init(void) {
    memset(pzk_field, -1, sizeof(pzk_field));
    memset(pzk_points, 0, sizeof(pzk_points));
    memset(pzk_score, 0, sizeof(pzk_score));
    for (int p = 0; p < 2; ++p) for (int i = 0; i < 4; ++i) pzk_hand[p][i] = -1;
    for (int p = 0; p < 2; ++p) for (int i = 0; i < 10; ++i) pzk_side[p][i] = -1;
    pzk_active = qfalse;
    pzk_credits[0] = pzk_credits[1] = 0;
}

// Helper: parse int safely
static int PZK_ParseInt(int idx) {
    const char* s = CG_Argv(idx);
    if (!s) return 0;
    return atoi(s);
}

// Server command: pzk <sub> [args...]
void CG_Pazaak_ServerCmd_f(void) {
    const char* sub = CG_Argv(1);
    if (!sub) return;

    if (Q_stricmp(sub, "sc") == 0) {
        // pzk sc <slot> <card> <points>
        int slot = PZK_ParseInt(2);
        int card = PZK_ParseInt(3);
        int pts = PZK_ParseInt(4);
        int pid = slot / 9;
        int pos = slot % 9;
        if (pid >= 0 && pid < 2 && pos >= 0 && pos < 9) {
            pzk_field[pid][pos] = card;
            pzk_points[pid] = pts;
            pzk_active = qtrue;
        }
        return;
    }
    if (Q_stricmp(sub, "st") == 0) {
        // pzk st <player> <flag>
        int who = PZK_ParseInt(2) - 1; // server sends 1-based
        int flag = PZK_ParseInt(3);
        if (who >= 0 && who < 2) {
            // use points array or flags as needed
            if (flag) pzk_points[who] = pzk_points[who];
        }
        return;
    }
    if (Q_stricmp(sub, "nr") == 0) {
        pzk_currentStarter = PZK_ParseInt(2);
        return;
    }
    if (Q_stricmp(sub, "bet") == 0) {
        // pzk bet <player> <amount> <pot> <remaining_credits>
        int who = PZK_ParseInt(2);
        int amount = PZK_ParseInt(3);
        int pot = PZK_ParseInt(4);
        int credits = PZK_ParseInt(5);
        pzk_pot = pot;
        if (who >= 0 && who < 2) pzk_credits[who] = credits;
        pzk_active = qtrue;
        return;
    }
    if (Q_stricmp(sub, "roundend") == 0) {
        int winner = PZK_ParseInt(2);
        pzk_score[0] = PZK_ParseInt(3);
        pzk_score[1] = PZK_ParseInt(4);
        return;
    }
    if (Q_stricmp(sub, "setend") == 0) {
        int w = PZK_ParseInt(2);
        // clear client state
        CG_Pazaak_Init();
        return;
    }
    if (Q_stricmp(sub, "gtc") == 0) {
        // pzk gtc sdc <deck...> ssd <sidedeck...>
        // parse tokens starting at 2
        int argc = CG_Argc();
        int i = 2;
        // expect token 'sdc' or deck numbers directly
        // collect deck until token 'ssd'
        int deckIdx = 0;
        while (i < argc) {
            const char* tok = CG_Argv(i);
            if (!tok) break;
            if (Q_stricmp(tok, "ssd") == 0) { i++; break; }
            // parse as deck card
            if (deckIdx < 100) { /* ignore deck storage for now */ }
            i++;
        }
        // parse sidedeck into player 0 for now
        int sd = 0;
        while (i < argc && sd < 10) {
            pzk_side[0][sd++] = atoi(CG_Argv(i));
            i++;
        }
        return;
    }
}

// Minimal ownerdraw renderer for Pazaak: draws simple boxes and numbers
void CG_Pazaak_OwnerDraw(int ownerDraw, float x, float y, float w, float h, vec4_t color, qhandle_t shader, float scale) {
    if (!pzk_active) return;

    // draw scores
    char buf[64];
    Com_sprintf(buf, sizeof(buf), "%d - %d", pzk_score[0], pzk_score[1]);
    vec4_t white = {1,1,1,1};
    CG_Text_Paint(x + w/2 - 10, y, 0.9f * scale, white, buf, 0, 0, ITEM_TEXTSTYLE_NORMAL, FONT_MEDIUM);

    // draw pot
    Com_sprintf(buf, sizeof(buf), "Pot: %d", pzk_pot);
    CG_Text_Paint(x + w/2 - 10, y + 14, 0.75f * scale, white, buf, 0, 0, ITEM_TEXTSTYLE_NORMAL, FONT_SMALL);

    // draw two rows of field slots
    const float slotW = w / 9.0f;
    for (int p = 0; p < 2; ++p) {
        float rowY = y + p * (h * 0.4f);
        for (int i = 0; i < 9; ++i) {
            float sx = x + i * slotW;
            qhandle_t sh = cgs.media.whiteShader;
            CG_DrawPic(sx, rowY, slotW - 2, 18, sh);
            if (pzk_field[p][i] >= 0) {
                char cbuf[8];
                Com_sprintf(cbuf, sizeof(cbuf), "%d", pzk_field[p][i]);
                CG_Text_Paint(sx + 2, rowY + 2, 0.6f * scale, white, cbuf, 0, 0, ITEM_TEXTSTYLE_NORMAL, FONT_SMALL);
            }
        }
        // draw points
        Com_sprintf(buf, sizeof(buf), "P%d: %d", p, pzk_points[p]);
        CG_Text_Paint(x, rowY + 20, 0.8f * scale, white, buf, 0, 0, ITEM_TEXTSTYLE_NORMAL, FONT_SMALL);
        // draw credits for this player
        Com_sprintf(buf, sizeof(buf), "Credits: %d", pzk_credits[p]);
        CG_Text_Paint(x + w - 70, rowY + 20, 0.75f * scale, white, buf, 0, 0, ITEM_TEXTSTYLE_NORMAL, FONT_SMALL);
    }
}
