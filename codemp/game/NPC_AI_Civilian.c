// leave this line at the top of all AI_xxxx.cpp files for PCH reasons...
#include "b_local.h"

extern qboolean NPC_CheckSurrender(void);
extern void NPC_BehaviorSet_Default(int b_state);

void NPC_BSCivilian_Default(const int b_state)
{
	if (NPCS.NPC->enemy
		&& NPCS.NPC->s.weapon == WP_NONE
		&& NPC_CheckSurrender())
	{
		//surrendering, do nothing
	}
	else if (NPCS.NPC->enemy
		&& NPCS.NPC->s.weapon == WP_NONE
		&& b_state != BS_HUNT_AND_KILL
		&& !trap->ICARUS_TaskIDPending((sharedEntity_t*)NPCS.NPC, TID_MOVE_NAV))
	{
		//if in battle and have no weapon, run away, fixme: when in BS_HUNT_AND_KILL, they just stand there
		if (!NPCS.NPCInfo->goalEntity
			|| b_state != BS_FLEE //not fleeing
			|| NPCS.NPC->enemy //still have enemy (NPC_BSFlee checks enemy and can clear it)
			&& DistanceSquared(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin) < 16384) //enemy within 128
		{
			//run away!
			NPC_StartFlee(NPCS.NPC->enemy, NPCS.NPC->enemy->r.currentOrigin, AEL_DANGER_GREAT, 5000, 10000);
		}
	}
	else
	{
		//not surrendering
		NPC_BehaviorSet_Default(b_state);
	}
	if (!VectorCompare(NPCS.NPC->client->ps.moveDir, vec3_origin))
	{
		//moving
		if (NPCS.NPC->client->ps.legsAnim == BOTH_COWER1)
		{
			//stop cowering anim on legs
			NPCS.NPC->client->ps.legsTimer = 0;
		}
	}
}