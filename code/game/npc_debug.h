#pragma once

#include "g_local.h"

// Logs sensing/engagement diagnostic info. Controlled via cvar 'g_npc_debugSense'.
void NPC_DebugSenseLog(gentity_t* npc, gentity_t* target, const char* context);
