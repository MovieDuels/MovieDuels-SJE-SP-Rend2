#include "npc_debug.h"
#include "g_navigator.h"

extern cvar_t* g_npc_debugSense;

void NPC_DebugSenseLog(gentity_t* npc, gentity_t* target, const char* context)
{
    if (!g_npc_debugSense || !g_npc_debugSense->integer)
        return;
    if (!npc || !target)
        return;

    float dist = sqrtf((float)DistanceSquared(npc->currentOrigin, target->currentOrigin));
    int inPVS = gi.inPVS(npc->currentOrigin, target->currentOrigin);
    int clearLOS = G_ClearLOS(npc, npc->client ? npc->client->renderInfo.eyePoint : npc->currentOrigin, target);
    int node = -1;
#ifdef NAV_API_AVAILABLE
    node = NAV::GetNearestNode(target->currentOrigin);
#endif

    gi.Printf("NPC_DEBUG [%s] NPC %d -> target %d dist=%.0f inPVS=%d clearLOS=%d node=%d\n",
        context ? context : "", npc->s.number, target->s.number, dist, inPVS, clearLOS, node);
}
