/* g_pazaak.h
 * Declarations for Pazaak server hooks. Use C linkage for cross-file calls.
 */
#ifndef G_PAZAAK_H
#define G_PAZAAK_H

#include "g_local.h"

#ifdef __cplusplus
extern "C" {
#endif

void G_Pazaak_Init(void);
bool G_Pazaak_Start(gentity_t* p1, gentity_t* p2);
void G_Pazaak_Stop(void);
void G_Pazaak_ProcessClientCommand(gentity_t* ent);
bool G_Pazaak_PlaceBet(gentity_t* who, int amount);
void G_Pazaak_RunFrame(int level_time);

#ifdef __cplusplus
}
#endif

#endif // G_PAZAAK_H
