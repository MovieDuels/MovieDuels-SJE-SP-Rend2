#include "g_local.h"
#include "cgame/cg_local.h"
#include "cgame/cg_media.h"

constexpr auto DRAWBOX_REFRESH = 500; // How long it takes to redraw the boxes;
constexpr auto BOX_VERTICES = 8;

qboolean inAIWorkshop = qfalse;
qboolean showDisplay = qfalse;

int drawBoxesTime = 0;
int selectedAI = ENTITYNUM_NONE;

extern stringID_table_t BSTable[];
extern stringID_table_t BSETTable[];
extern stringID_table_t teamTable[];
extern stringID_table_t NPCClassTable[];
extern stringID_table_t RankTable[];
extern stringID_table_t MoveTypeTable[];
extern stringID_table_t FPTable[];
extern cvar_t* com_kotor;

constexpr auto OL_S = 0.5f;
constexpr auto OL_Y = 30;
constexpr auto OL_H = 6;

static void WorkshopDrawEntityInformation(const gentity_t* ent, const int x, const char* title)
{
	int add = OL_H;
	constexpr vec4_t textcolor = { 0.4f, 0.4f, 0.8f, 1.0f };

	cgi_R_Font_DrawString(x, OL_Y, title, textcolor, cgs.media.qhFontSmall, -1, OL_S);

	cgi_R_Font_DrawString(x, OL_Y + add, va("NPC_type: %s", ent->NPC_type), textcolor, cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;

	cgi_R_Font_DrawString(x, OL_Y + add, va("health: %i/%i", ent->health, ent->max_health), textcolor,
		cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;

	if (ent->script_targetname)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, va("script_targetname: %s", ent->script_targetname), textcolor,
			cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->NPC->goalEntity)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, va("goalEnt = %i", ent->NPC->goalEntity->s.number), textcolor,
			cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}
	cgi_R_Font_DrawString(x, OL_Y + add, va("bs = %s", BSTable[ent->NPC->behaviorState].name), textcolor,
		cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;
	if (ent->NPC->combatMove)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "-- in combat move --", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	cgi_R_Font_DrawString(x, OL_Y + add, va("class = %s", NPCClassTable[ent->client->NPC_class].name), textcolor,
		cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;
	cgi_R_Font_DrawString(x, OL_Y + add, va("rank = %s (%i)", RankTable[ent->NPC->rank].name, ent->NPC->rank),
		textcolor, cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;

	if (ent->NPC->scriptFlags)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, va("scriptFlags: %i", ent->NPC->scriptFlags), textcolor,
			cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}
	if (ent->NPC->aiFlags)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, va("aiFlags: %i", ent->NPC->aiFlags), textcolor, cgs.media.qhFontSmall, -1,
			OL_S);
		add += OL_H;
	}

	if (ent->client->noclip)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: NOCLIP", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->flags & FL_GODMODE)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: GODMODE", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->flags & FL_FORCEMODE)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: FORCEMODE", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->flags & FL_BLOCKPOINTMODE)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: BLOCKMODE", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->flags & FL_STAMINA_MODE)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: STAMINAMODE", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->flags & FL_NOTARGET)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: NOTARGET", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	if (ent->flags & FL_UNDYING)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "cheat: UNDEAD", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
	}

	cgi_R_Font_DrawString(x, OL_Y + add, va("playerTeam: %s", teamTable[ent->client->playerTeam].name), textcolor,
		cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;

	cgi_R_Font_DrawString(x, OL_Y + add, va("enemyTeam: %s", teamTable[ent->client->enemyTeam].name), textcolor,
		cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;

	cgi_R_Font_DrawString(x, OL_Y + add, "-- assigned scripts --", textcolor, cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;

	for (int i = BSET_SPAWN; i < NUM_BSETS; i++)
	{
		if (ent->behaviorSet[i])
		{
			cgi_R_Font_DrawString(x, OL_Y + add, va("%s: %s", BSETTable[i].name, ent->behaviorSet[i]), textcolor,
				cgs.media.qhFontSmall, -1, OL_S);
			add += OL_H;
		}
	}

	if (ent->parms)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "-- parms --", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
		for (int i = 0; i < MAX_PARMS; i++)
		{
			if (ent->parms->parm[i][0])
			{
				cgi_R_Font_DrawString(x, OL_Y + add, va("parm%i : %s", i + 1, ent->parms->parm[i]), textcolor,
					cgs.media.qhFontSmall, -1, OL_S);
				add += OL_H;
			}
		}
	}

	if (ent->NPC->group && ent->NPC->group->numGroup > 1)
	{
		cgi_R_Font_DrawString(x, OL_Y + add, "-- squad data --", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
		cgi_R_Font_DrawString(x, OL_Y + add, va("morale: %i", ent->NPC->group->morale), textcolor,
			cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
		cgi_R_Font_DrawString(x, OL_Y + add, va("morale debounce: %i", ent->NPC->group->moraleDebounce), textcolor,
			cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
		cgi_R_Font_DrawString(x, OL_Y + add,
			va("last seen enemy: %i milliseconds", level.time - ent->NPC->group->lastSeenEnemyTime),
			textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
		if (ent->NPC->group->commander)
		{
			cgi_R_Font_DrawString(x, OL_Y + add, va("commander: %i", ent->NPC->group->commander->s.number), textcolor,
				cgs.media.qhFontSmall, -1, OL_S);
			add += OL_H;
		}

		cgi_R_Font_DrawString(x, OL_Y + add, "-- squad members --", textcolor, cgs.media.qhFontSmall, -1, OL_S);
		add += OL_H;
		for (int i = 0; i < ent->NPC->group->numGroup; i++)
		{
			const AIGroupMember_t* member_ai = &ent->NPC->group->member[i];
			const int member_num = member_ai->number;
			const gentity_t* member = &g_entities[member_num];
			const char* member_text = va("* entity %i, closestBuddy: %i, class: %s, rank: %s (%i), health: %i/%i",
				member_num, member_ai->closestBuddy,
				NPCClassTable[member->client->NPC_class].name,
				RankTable[member->NPC->rank].name, member->NPC->rank, member->health,
				member->max_health);
			cgi_R_Font_DrawString(x, OL_Y + add, member_text, textcolor, cgs.media.qhFontSmall, -1, OL_S);
			add += OL_H;
		}
	}

	cgi_R_Font_DrawString(x, OL_Y + add, "-- currently active timers --", textcolor, cgs.media.qhFontSmall, -1, OL_S);
	add += OL_H;
	const auto timers = TIMER_List(ent);
	for (auto& timer : timers)
	{
		if (timer.second >= 0)
		{
			cgi_R_Font_DrawString(x, OL_Y + add, va("%s : %i", timer.first.c_str(), timer.second), textcolor,
				cgs.media.qhFontSmall, -1, OL_S);
			add += OL_H;
		}
	}
}

//
// Draw textual information about the NPC in our crosshairs and also the currently selected one
//
void workshop_draw_clientside_information()
{
	if (inAIWorkshop == qfalse)
	{
		return;
	}
	if (showDisplay == qfalse)
	{
		return;
	}
	// Draw the information for the NPC that is in our crosshairs
	if (cg.crosshairclient_num != ENTITYNUM_NONE && cg.crosshairclient_num != 0 && g_entities[cg.crosshairclient_num].
		client)
	{
		const gentity_t* crossEnt = &g_entities[cg.crosshairclient_num];
		WorkshopDrawEntityInformation(crossEnt, 10, "Crosshair AI");
	}

	// Draw the information for the AI that we have selected
	if (selectedAI != ENTITYNUM_NONE)
	{
		const gentity_t* selectedEnt = &g_entities[selectedAI];
		WorkshopDrawEntityInformation(selectedEnt, 500, "Selected AI");
	}
}

//
//	Draws a box around the bounding box of the entity
//
static void WorkshopDrawEntBox(const gentity_t* ent, const int color_override = -1)
{
	unsigned color = 0x666666; // G_SoundOnEnt(ent, "satan.mp3");

	if (color_override == -1 && selectedAI == ent->s.number)
	{
		// Draw a box around our goal ent
		if (ent->NPC->goalEntity != nullptr)
		{
			WorkshopDrawEntBox(ent->NPC->goalEntity, color);
		}
		color = 0x0000FFFF; // Yellow = selected AI
	}
	else if (color_override == -1 && ent->client->playerTeam == TEAM_ENEMY)
	{
		color = 0x000000FF; // Red = enemy
	}
	else if (color_override == -1 && ent->client->playerTeam == TEAM_PLAYER)
	{
		color = 0x0000FF00; // Green = ally
	}
	else if (color_override == -1 && ent->client->playerTeam == TEAM_NEUTRAL)
	{
		color = 0x00FF0000; // Blue = neutral
	}
	else if (color_override != -1)
	{
		color = color_override;
	}

	// This is going to look like a mess and I can't really comment on this, but I drew this out manually and it works.
	vec3_t vertices[BOX_VERTICES] = { 0 };
	for (auto& vertice : vertices)
	{
		VectorCopy(ent->currentOrigin, vertice);
	}

	for (int i = 4; i < BOX_VERTICES; i++)
	{
		vertices[i][2] += ent->mins[2];
	}
	for (int i = 0; i < 4; i++)
	{
		vertices[i][2] += ent->maxs[2];
	}
	vertices[0][0] += ent->mins[0];
	vertices[1][0] += ent->mins[0];
	vertices[4][0] += ent->mins[0];
	vertices[5][0] += ent->mins[0];
	vertices[2][0] += ent->maxs[0];
	vertices[3][0] += ent->maxs[0];
	vertices[6][0] += ent->maxs[0];
	vertices[7][0] += ent->maxs[0];
	vertices[0][1] += ent->mins[1];
	vertices[2][1] += ent->mins[1];
	vertices[4][1] += ent->mins[1];
	vertices[6][1] += ent->mins[1];
	vertices[1][1] += ent->maxs[1];
	vertices[3][1] += ent->maxs[1];
	vertices[5][1] += ent->maxs[1];
	vertices[7][1] += ent->maxs[1];

	G_DebugLine(vertices[0], vertices[1], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[2], vertices[3], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[0], vertices[2], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[1], vertices[3], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[0], vertices[4], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[1], vertices[5], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[2], vertices[6], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[3], vertices[7], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[4], vertices[5], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[6], vertices[7], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[4], vertices[6], DRAWBOX_REFRESH, color);
	G_DebugLine(vertices[5], vertices[7], DRAWBOX_REFRESH, color);
}

//
//	Occurs every frame
//
void WorkshopThink()
{
	if (!inAIWorkshop)
	{
		return;
	}

	// If the selected AI is dead, deselect it
	if (selectedAI != ENTITYNUM_NONE)
	{
		const gentity_t* ent = &g_entities[selectedAI];
		if (ent->health <= 0)
		{
			selectedAI = ENTITYNUM_NONE;
		}
	}
	if (drawBoxesTime < level.time && showDisplay)
	{
		for (int i = 1; i < globals.num_entities; i++)
		{
			if (!PInUse(i))
			{
				continue;
			}

			const gentity_t* found = &g_entities[i];
			if (!found->client)
			{
				continue;
			}

			WorkshopDrawEntBox(found);
			drawBoxesTime = level.time + DRAWBOX_REFRESH - 50;
			// -50 so we get an extra frame to draw, so the boxes don't blink
		}
	}
}

//
//	Workshop mode just got toggled by console command
//
void WorkshopToggle(gentity_t* ent)
{
	inAIWorkshop = static_cast<qboolean>(!inAIWorkshop);

	if (inAIWorkshop)
	{
		// Workshop mode just got enabled
		//gi.Printf("AI Workshop enabled\n");
	}
	else
	{
		// Workshop mode just got disabled
		//gi.Printf("AI Workshop disabled\n");
	}
}

//////////////////////
//
// Below are various commands that we can do
//
//////////////////////

static void WorkshopSelect_f(gentity_t* ent)
{
	vec3_t src, dest, vf;
	trace_t trace;
	VectorCopy(ent->client->renderInfo.eyePoint, src);
	AngleVectors(ent->client->ps.viewangles, vf, nullptr, nullptr);
	//ent->client->renderInfo.eyeAngles was cg.refdef.viewangles, basically
	//extend to find end of use trace
	VectorMA(src, 32967.0f, vf, dest);

	//Trace ahead to find a valid target
	gi.trace(&trace, src, vec3_origin, vec3_origin, dest, ent->s.number,
		MASK_OPAQUE | CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE, G2_NOCOLLIDE, 0);
	if (trace.fraction == 1.0f || trace.entityNum < 1)
	{
		return;
	}

	const gentity_t* target = &g_entities[trace.entityNum];
	if (target->client)
	{
		selectedAI = target->s.number;
	}
	else
	{
		gi.Printf("No NPC in crosshair\n");
	}
}

// Deselect currently selected NPC
static void WorkshopDeselect_f(gentity_t* ent)
{
	selectedAI = ENTITYNUM_NONE;
}

// View all behavior states
static void Workshop_List_BehaviorState_f(gentity_t* ent)
{
	for (int i = 0; i < NUM_BSTATES; i++)
	{
		gi.Printf(va("%s\n", BSTable[i].name));
	}
}

// View all teams
static void Workshop_List_Team_f(gentity_t* ent)
{
	for (int i = 0; i < TEAM_NUM_TEAMS; i++)
	{
		gi.Printf(va("%s\n", teamTable[i].name));
	}
}

// View all ranks
static void Workshop_List_Ranks_f(gentity_t* ent)
{
	for (int i = 0; i < RANK_MAX; i++)
	{
		gi.Printf(va("%s\n", RankTable[i].name));
	}
}

// View all classes
static void Workshop_List_Classes_f(gentity_t* ent)
{
	for (int i = 0; i < CLASS_NUM_CLASSES; i++)
	{
		gi.Printf(va("%s\n", NPCClassTable[i].name));
	}
}

// View all movetypes
static void Workshop_List_Movetypes_f(gentity_t* ent)
{
	for (int i = 0; i <= MT_FLYSWIM; i++)
	{
		gi.Printf(va("%s\n", MoveTypeTable[i].name));
	}
}

// View all forcepowers
static void Workshop_List_ForcePowers_f(gentity_t* ent)
{
	for (int i = 0; i <= FP_SABER_OFFENSE; i++)
	{
		gi.Printf(va("%s\n", FPTable[i].name));
	}
}

// View all behaviorsets
static void Workshop_List_BehaviorSets_f(gentity_t* ent)
{
	for (int i = BSET_SPAWN; i < NUM_BSETS; i++)
	{
		gi.Printf(va("%s\n", BSETTable[i].name));
	}
}

// View all animations
static void Workshop_List_Anims_f(gentity_t* ent)
{
	gi.Printf("^5Listing animations....");
	for (int i = 0; i < MAX_ANIMATIONS; i++)
	{
		// Print three animations per line
		if (i % 3 == 0)
		{
			gi.Printf("\n");
		}
		gi.Printf("%-.32s\t", anim_table[i].name);
	}
	gi.Printf("\n");
	gi.Printf("%i animations listed.\n", MAX_ANIMATIONS);
}

// List all scriptflags
#define PRINTFLAG(x) gi.Printf("" #x " = %i\n", x)

static void Workshop_List_Scriptflags_f(gentity_t* ent)
{
	PRINTFLAG(SCF_CROUCHED);
	PRINTFLAG(SCF_WALKING);
	PRINTFLAG(SCF_MORELIGHT);
	PRINTFLAG(SCF_LEAN_RIGHT);
	PRINTFLAG(SCF_LEAN_LEFT);
	PRINTFLAG(SCF_RUNNING);
	PRINTFLAG(SCF_ALT_FIRE);
	PRINTFLAG(SCF_NO_RESPONSE);
	PRINTFLAG(SCF_FFDEATH);
	PRINTFLAG(SCF_NO_COMBAT_TALK);
	PRINTFLAG(SCF_CHASE_ENEMIES);
	PRINTFLAG(SCF_LOOK_FOR_ENEMIES);
	PRINTFLAG(SCF_FACE_MOVE_DIR);
	PRINTFLAG(SCF_IGNORE_ALERTS);
	PRINTFLAG(SCF_DONT_FIRE);
	PRINTFLAG(SCF_DONT_FLEE);
	PRINTFLAG(SCF_FORCED_MARCH);
	PRINTFLAG(SCF_NO_GROUPS);
	PRINTFLAG(SCF_FIRE_WEAPON);
	PRINTFLAG(SCF_NO_MIND_TRICK);
	PRINTFLAG(SCF_USE_CP_NEAREST);
	PRINTFLAG(SCF_NO_FORCE);
	PRINTFLAG(SCF_NO_FALLTODEATH);
	PRINTFLAG(SCF_NO_ACROBATICS);
	PRINTFLAG(SCF_USE_SUBTITLES);
	PRINTFLAG(SCF_NO_ALERT_TALK);
}

// List all aiflags
static void Workshop_List_AIFlags_f(gentity_t* ent)
{
	PRINTFLAG(NPCAI_CHECK_WEAPON);
	PRINTFLAG(NPCAI_BURST_WEAPON);
	PRINTFLAG(NPCAI_MOVING);
	PRINTFLAG(NPCAI_TOUCHED_GOAL);
	PRINTFLAG(NPCAI_PUSHED);
	PRINTFLAG(NPCAI_NO_COLL_AVOID);
	PRINTFLAG(NPCAI_BLOCKED);
	PRINTFLAG(NPCAI_OFF_PATH);
	PRINTFLAG(NPCAI_IN_SQUADPOINT);
	PRINTFLAG(NPCAI_STRAIGHT_TO_DESTPOS);
	PRINTFLAG(NPCAI_NO_SLOWDOWN);
	PRINTFLAG(NPCAI_LOST);
	PRINTFLAG(NPCAI_SHIELDS);
	PRINTFLAG(NPCAI_GREET_ALLIES);
	PRINTFLAG(NPCAI_FORM_TELE_NAV);
	PRINTFLAG(NPCAI_ENROUTE_TO_HOMEWP);
	PRINTFLAG(NPCAI_MATCHPLAYERWEAPON);
	PRINTFLAG(NPCAI_DIE_ON_IMPACT);
}

// Set a raw timer
static void Workshop_Set_Timer_f(gentity_t* ent)
{
	if (gi.argc() != 3)
	{
		gi.Printf("usage: workshop_set_timer <timer name> <milliseconds>\n");
		return;
	}
	const char* timerName = gi.argv(1);
	const int ms = atoi(gi.argv(2));
	TIMER_Set(&g_entities[selectedAI], timerName, ms);
}

// View all timers, including ones which are dead
static void Workshop_View_Timers_f(gentity_t* ent)
{
	const auto timers = TIMER_List(&g_entities[selectedAI]);
	for (auto& timer : timers)
	{
		gi.Printf("%s\t\t:\t\t%i\n", timer.first.c_str(), timer.second);
	}
}

// Set Behavior State
static void Workshop_Set_BehaviorState_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_bstate <bstate>\n");
		return;
	}
	int b_state = GetIDForString(BSTable, gi.argv(1));
	if (b_state == -1)
	{
		gi.Printf("Invalid bstate\n");
		return;
	}
	g_entities[selectedAI].NPC->behaviorState = static_cast<bState_t>(b_state);
}

// Set goal entity
static void Workshop_Set_GoalEntity_f(gentity_t* ent)
{
	if (gi.argc() == 2)
	{
		// There's a second argument. Use that.
		if (!Q_stricmp(gi.argv(1), "me"))
		{
			g_entities[selectedAI].NPC->goalEntity = g_entities;
			return;
		}
		g_entities[selectedAI].NPC->goalEntity = &g_entities[atoi(gi.argv(1))];
		if (g_entities[selectedAI].NPC->lastGoalEntity != nullptr)
		{
			gi.Printf("New goal entity: %i (last goal entity was %i)\n",
				g_entities[selectedAI].NPC->goalEntity->s.number,
				g_entities[selectedAI].NPC->lastGoalEntity->s.number);
		}
		else
		{
			gi.Printf("New goal entity: %i\n", g_entities[selectedAI].NPC->goalEntity->s.number);
		}
	}

	vec3_t src, dest, vf;
	trace_t trace;
	VectorCopy(ent->client->renderInfo.eyePoint, src);
	AngleVectors(ent->client->ps.viewangles, vf, nullptr, nullptr);
	//ent->client->renderInfo.eyeAngles was cg.refdef.viewangles, basically
	//extend to find end of use trace
	VectorMA(src, 32967.0f, vf, dest);

	//Trace ahead to find a valid target
	gi.trace(&trace, src, vec3_origin, vec3_origin, dest, ent->s.number,
		MASK_OPAQUE | CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_ITEM | CONTENTS_CORPSE, G2_NOCOLLIDE, 0);
	if (trace.fraction == 1.0f || trace.entityNum < 1 || trace.entityNum == ENTITYNUM_NONE || trace.entityNum ==
		ENTITYNUM_WORLD)
	{
		gi.Printf("Invalid entity\n");
		return;
	}

	gentity_t* target = &g_entities[trace.entityNum];
	const gentity_t* selected = &g_entities[selectedAI];
	selected->NPC->lastGoalEntity = selected->NPC->goalEntity;
	selected->NPC->goalEntity = target;
	if (selected->NPC->lastGoalEntity != nullptr)
	{
		gi.Printf("New goal entity: %i (last goal entity was %i)\n", selected->NPC->goalEntity->s.number,
			selected->NPC->lastGoalEntity->s.number);
	}
	else
	{
		gi.Printf("New goal entity: %i\n", selected->NPC->goalEntity->s.number);
	}
}

// Set enemy
static void Workshop_Set_Enemy_f(gentity_t* ent)
{
	if (gi.argc() == 2)
	{
		// There's a second argument. Use that.
		if (!Q_stricmp(gi.argv(1), "me"))
		{
			g_entities[selectedAI].lastEnemy = g_entities[selectedAI].enemy;
			g_entities[selectedAI].enemy = g_entities;
		}
		else
		{
			g_entities[selectedAI].enemy = &g_entities[atoi(gi.argv(1))];
		}

		if (g_entities[selectedAI].lastEnemy != nullptr)
		{
			gi.Printf("New enemy: %i (last enemy was %i)\n", g_entities[selectedAI].enemy->s.number,
				g_entities[selectedAI].lastEnemy->s.number);
		}
		else
		{
			gi.Printf("New enemy: %i\n", g_entities[selectedAI].enemy->s.number);
		}
	}

	vec3_t src, dest, vf;
	trace_t trace;
	VectorCopy(ent->client->renderInfo.eyePoint, src);
	AngleVectors(ent->client->ps.viewangles, vf, nullptr, nullptr);
	//extend to find end of use trace
	VectorMA(src, 32967.0f, vf, dest);

	//Trace ahead to find a valid target
	gi.trace(&trace, src, vec3_origin, vec3_origin, dest, ent->s.number,
		MASK_OPAQUE | CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_ITEM | CONTENTS_CORPSE, G2_NOCOLLIDE, 0);

	if (trace.fraction == 1.0f || trace.entityNum < 1 || trace.entityNum == ENTITYNUM_NONE || trace.entityNum ==
		ENTITYNUM_WORLD)
	{
		gi.Printf("Invalid entity\n");
		return;
	}

	gentity_t* target = &g_entities[trace.entityNum];
	gentity_t* selected = &g_entities[selectedAI];
	selected->lastEnemy = selected->enemy;
	selected->enemy = target;

	if (selected->lastEnemy != nullptr)
	{
		gi.Printf("New enemy: %i (last enemy was %i)\n", selected->enemy->s.number, selected->lastEnemy->s.number);
	}
	else
	{
		gi.Printf("New enemy: %i\n", selected->enemy->s.number);
	}
}

// Set scriptflags
static void Workshop_Set_Scriptflags_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_scriptflags <script flags>\n");
		return;
	}
	const int scriptFlags = atoi(gi.argv(1));
	if (scriptFlags < 0)
	{
		gi.Printf("Invalid scriptflags.\n");
		return;
	}
	g_entities[selectedAI].NPC->scriptFlags = scriptFlags;
}

// Set aiflags
static void Workshop_Set_Aiflags_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_aiflags <ai flags>\n");
		return;
	}
	const int scriptFlags = atoi(gi.argv(1));
	if (scriptFlags < 0)
	{
		gi.Printf("Invalid scriptflags.\n");
		return;
	}
	g_entities[selectedAI].NPC->aiFlags = scriptFlags;
}

// Set weapon
extern stringID_table_t WPTable[];
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern int WP_SaberInitBladeData(gentity_t* ent);
extern void G_CreateG2AttachedWeaponModel(gentity_t* ent, const char* ps_weapon_model, int bolt_num, int weapon_num);
extern void WP_SaberAddG2SaberModels(gentity_t* ent, int specific_saber_num = -1);
extern void WP_SaberAddHolsteredG2SaberModels(gentity_t* ent, int specific_saber_num = -1);

static void Workshop_Set_Weapon_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_weapon <weapon name or 'me'>\n");
		return;
	}
	int weaponNum;
	if (!Q_stricmp(gi.argv(1), "me"))
	{
		weaponNum = g_entities[0].s.weapon;
	}
	else
	{
		weaponNum = GetIDForString(WPTable, gi.argv(1));

		if (weaponNum == -1)
		{
			gi.Printf("Invalid weapon number. Maybe try WP_NONE, or WP_BLASTER for instance?\n");
			return;
		}
	}

	gentity_t* selected = &g_entities[selectedAI];
	selected->client->ps.weapon = weaponNum;
	selected->client->ps.weaponstate = WEAPON_RAISING;

	G_RemoveWeaponModels(selected);

	ChangeWeapon(selected, weaponNum);

	if (weaponNum == WP_SABER)
	{
		WP_SaberInitBladeData(selected);
		WP_SaberAddG2SaberModels(selected);
		G_RemoveHolsterModels(selected);
	}
	else
	{
		if (com_kotor->integer == 1) //playing kotor
		{
			G_CreateG2AttachedWeaponModel(selected, weaponData[weaponNum].altweaponMdl, selected->handRBolt, 0);
		}
		else
		{
			if (selected->client->friendlyfaction == FACTION_KOTOR)
			{
				G_CreateG2AttachedWeaponModel(selected, weaponData[weaponNum].altweaponMdl, selected->handRBolt, 0);
			}
			else
			{
				G_CreateG2AttachedWeaponModel(selected, weaponData[weaponNum].weaponMdl, selected->handRBolt, 0);
			}
		}

		WP_SaberAddHolsteredG2SaberModels(selected);
	}
}

// Set team
static void Workshop_Set_Team_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_team <team name>\n");
		return;
	}
	int team = GetIDForString(teamTable, gi.argv(1));
	if (team == -1)
	{
		gi.Printf("invalid team\n");
		return;
	}
	g_entities[selectedAI].client->playerTeam = static_cast<team_t>(team);
}

// Set enemyteam
static void Workshop_Set_Enemyteam_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_enemyteam <team name>\n");
		return;
	}
	int team = GetIDForString(teamTable, gi.argv(1));
	if (team == -1)
	{
		gi.Printf("invalid team\n");
		return;
	}
	g_entities[selectedAI].client->enemyTeam = static_cast<team_t>(team);
}

// Set class
static void Workshop_Set_Class_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_class <class name>\n");
		return;
	}
	int team = GetIDForString(NPCClassTable, gi.argv(1));
	if (team == -1)
	{
		gi.Printf("invalid class\n");
		return;
	}
	g_entities[selectedAI].client->NPC_class = static_cast<class_t>(team);
}

// Set rank
static void Workshop_Set_Rank_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_rank <rank name>\n");
		return;
	}
	int team = GetIDForString(RankTable, gi.argv(1));
	if (team == -1)
	{
		gi.Printf("invalid rank\n");
		return;
	}
	g_entities[selectedAI].NPC->rank = static_cast<rank_t>(team);
}

// Set leader
static void Workshop_Set_Leader_f(gentity_t* ent)
{
	if (gi.argc() == 2)
	{
		// There's a second argument. Use that.
		if (!Q_stricmp(gi.argv(1), "me"))
		{
			g_entities[selectedAI].client->leader = g_entities;
			//g_entities[selectedAI].client->team_leader = g_entities;
			return;
		}
		g_entities[selectedAI].client->leader = &g_entities[atoi(gi.argv(1))];
		//g_entities[selectedAI].client->team_leader = &g_entities[atoi(gi.argv(1))];
	}

	vec3_t src, dest, vf;
	trace_t trace;
	VectorCopy(ent->client->renderInfo.eyePoint, src);
	AngleVectors(ent->client->ps.viewangles, vf, nullptr, nullptr);
	//ent->client->renderInfo.eyeAngles was cg.refdef.viewangles, basically
	//extend to find end of use trace
	VectorMA(src, 32967.0f, vf, dest);

	//Trace ahead to find a valid target
	gi.trace(&trace, src, vec3_origin, vec3_origin, dest, ent->s.number,
		MASK_OPAQUE | CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_ITEM | CONTENTS_CORPSE, G2_NOCOLLIDE, 0);
	if (trace.fraction == 1.0f || trace.entityNum < 1 || trace.entityNum == ENTITYNUM_NONE || trace.entityNum ==
		ENTITYNUM_WORLD)
	{
		gi.Printf("Invalid entity\n");
		return;
	}

	gentity_t* target = &g_entities[trace.entityNum];
	const gentity_t* selected = &g_entities[selectedAI];
	selected->client->leader = target;
}

// Set movetype
static void Workshop_Set_Movetype_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: workshop_set_movetype <move type>\n");
		return;
	}
	int team = GetIDForString(MoveTypeTable, gi.argv(1));
	if (team == -1)
	{
		gi.Printf("invalid movetype\n");
		return;
	}
	g_entities[selectedAI].client->moveType = static_cast<movetype_t>(team);
}

// Set forcepower
static void Workshop_Set_Forcepower_f(gentity_t* ent)
{
	if (gi.argc() != 3)
	{
		gi.Printf("usage: workshop_set_forcepower <force power> <rank>\n");
		return;
	}
	const int power = GetIDForString(FPTable, gi.argv(1));
	if (power == -1)
	{
		gi.Printf("invalid force power\n");
		return;
	}
	const int rank = atoi(gi.argv(2));
	if (rank < 0)
	{
		gi.Printf("invalid rank\n");
		return;
	}

	const gentity_t* selected = &g_entities[selectedAI];
	if (rank == 0)
	{
		selected->client->ps.forcePowersKnown ^= 1 << power;
	}
	else
	{
		selected->client->ps.forcePowersKnown |= 1 << power;
	}
	selected->client->ps.forcePowerLevel[power] = rank;
}

// Activates a behavior set
static void Workshop_Activate_BSet_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: %s <bset>\n", gi.argv(0));
		return;
	}

	const int bset = GetIDForString(BSETTable, gi.argv(1));
	if (bset == -1)
	{
		gi.Printf("Invalid Behavior Set.\n");
		return;
	}

	gentity_t* selected = &g_entities[selectedAI];
	G_ActivateBehavior(selected, bset);
}

// Set a spawnscript, angerscript, etc
static void Workshop_Set_BSetScript_f(gentity_t* ent)
{
	if (gi.argc() != 3)
	{
		gi.Printf("usage: %s <bset> <script path>\n", gi.argv(0));
		return;
	}

	const int bset = GetIDForString(BSETTable, gi.argv(1));
	if (bset == -1)
	{
		gi.Printf("Invalid Behavior Set.\n");
		return;
	}

	const char* path = gi.argv(2);
	gentity_t* selected = &g_entities[selectedAI];
	selected->behaviorSet[bset] = G_NewString(path);
}

extern void Q3_SetParm(int entID, int parmNum, const char* parmValue);

static void Workshop_Set_Parm_f(gentity_t* ent)
{
	if (gi.argc() != 3)
	{
		gi.Printf("usage: %s <parm num> <value>\n", gi.argv(0));
		return;
	}

	const int parmNum = atoi(gi.argv(1)) - 1;
	if (parmNum < 0 || parmNum >= MAX_PARMS)
	{
		gi.Printf("Invalid parm. Must be between 1 and 16 (inclusive)\n");
		return;
	}

	char* text = gi.argv(2);
	if (text[0] == ' ' && text[1] == '\0')
	{
		gi.Printf("This parm will be blanked.\n");
		text[0] = '\0';
	}

	Q3_SetParm(selectedAI, parmNum, text);
}

static void Workshop_Play_Dialogue_f(gentity_t* ent)
{
	const gentity_t* selected = &g_entities[selectedAI];
	if (gi.argc() != 2)
	{
		gi.Printf("usage: %s <sound to play>\n", gi.argv(0));
		return;
	}

	const char* sound = gi.argv(1);
	G_SoundOnEnt(selected, CHAN_VOICE, sound);
}

extern qboolean Q3_SetAnimUpper(int entID, const char* anim_name);
extern qboolean Q3_SetAnimLower(int entID, const char* anim_name);
extern void Q3_SetAnimHoldTime(int entID, int int_data, qboolean lower);

static void Workshop_Set_Animation_f(gentity_t* ent)
{
	if (gi.argc() != 4)
	{
		gi.Printf("usage: %s <animation name> <\"lower\", \"upper\" or \"both\"> <hold time, milliseconds>\n",
			gi.argv(0));
		return;
	}

	const char* animName = gi.argv(1);
	const char* section = gi.argv(2);
	const int holdTime = atoi(gi.argv(3));
	if (Q_stricmp(section, "lower") && Q_stricmp(section, "upper") && Q_stricmp(section, "both"))
	{
		gi.Printf("Only \"lower\", \"upper\", or \"both\" are valid sections for %s\n", gi.argv(0));
		return;
	}

	if (!Q_stricmp(section, "lower") || !Q_stricmp(section, "both"))
	{
		Q3_SetAnimLower(selectedAI, animName);
		Q3_SetAnimHoldTime(selectedAI, holdTime, qtrue);
	}
	if (!Q_stricmp(section, "upper") || !Q_stricmp(section, "both"))
	{
		Q3_SetAnimUpper(selectedAI, animName);
		Q3_SetAnimHoldTime(selectedAI, holdTime, qfalse);
	}
}

extern void Q3_SetHealth(int entID, int data);

static void Workshop_Set_Health_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: %s <health>\n", gi.argv(0));
		return;
	}

	const int health = atoi(gi.argv(1));
	Q3_SetHealth(selectedAI, health);
}

extern void Q3_SetArmor(int entID, int data);

static void Workshop_Set_Armor_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: %s <armor>\n", gi.argv(0));
		return;
	}

	const int armor = atoi(gi.argv(1));
	Q3_SetArmor(selectedAI, armor);
}

// Some more...exotic options

// Set NPC gravity
extern void Q3_SetGravity(int entID, float float_data);

static void Workshop_Set_Gravity_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: %s <gravity - 800 is default>\n", gi.argv(0));
		return;
	}

	const float gravity = atof(gi.argv(1));
	Q3_SetGravity(selectedAI, gravity);
}

// Set NPC scale
extern void Q3_SetScale(int entID, float float_data);

static void Workshop_Set_Scale_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		gi.Printf("usage: %s <scale - 1.0 is the default>\n", gi.argv(0));
		return;
	}

	gentity_t* selected = &g_entities[selectedAI];
	const float scale = atof(gi.argv(1));

	Q3_SetScale(selectedAI, scale);

	// Adjust the model scale and the collision (mins/maxs) in one pass
	const vec_t prevMins = selected->mins[2];
	for (int i = 0; i < 3; i++)
	{
		const float correctScale = selected->s.modelScale[i] < 0.001f ? 1.0f : selected->s.modelScale[i];
		const float maxNormalizedValue = selected->maxs[i] / correctScale;
		const float minNormalizedValue = selected->mins[i] / correctScale;
		selected->maxs[i] = maxNormalizedValue * scale;
		selected->mins[i] = minNormalizedValue * scale;
		selected->s.modelScale[i] = scale;
	}
	const vec_t afterMins = selected->mins[2];
	const vec_t diff = afterMins - prevMins;

	// Do the below so the NPC doesn't get stuck in the floor
	selected->client->ps.origin[2] += fabs(diff * 1.1f);
	selected->currentOrigin[2] += fabs(diff);
	selected->s.radius = sqrt(selected->maxs[0] * selected->maxs[0] + selected->maxs[1] * selected->maxs[1]);
}

// Mind control an NPC
extern qboolean G_ClearViewEntity(gentity_t* ent);
extern void G_SetViewEntity(gentity_t* self, gentity_t* view_entity);

static void Workshop_MindControl_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	G_ClearViewEntity(ent);
	G_SetViewEntity(ent, selected);
}

static void Workshop_EndMindControl_f(gentity_t* ent)
{
	G_ClearViewEntity(ent);
}

// Toggles freeze on the current NPC
static void Workshop_Freeze_f(gentity_t* ent)
{
	gentity_t* ai = &g_entities[selectedAI];
	if (ai->svFlags & SVF_ICARUS_FREEZE)
	{
		// Entity is frozen, unfreeze it!
		ai->svFlags &= ~SVF_ICARUS_FREEZE;
	}
	else
	{
		ai->svFlags |= SVF_ICARUS_FREEZE;
	}
}

// Kills the NPC
static void Workshop_Kill_f(gentity_t* ent)
{
	gentity_t* ai = &g_entities[selectedAI];
	G_Damage(ai, ent, ent, vec3_origin, vec3_origin, 9000, DAMAGE_NO_PROTECTION | DAMAGE_NO_KNOCKBACK, MOD_UNKNOWN);
	selectedAI = ENTITYNUM_NONE;
}

// Godmode for the NPC
static void Workshop_God_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	if (selected->flags & FL_GODMODE)
	{
		gi.Printf("godmode OFF\n");
		selected->flags ^= FL_GODMODE;
	}
	else
	{
		gi.Printf("godmode ON\n");
		selected->flags |= FL_GODMODE;
	}
}

static void Workshop_Force_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	if (selected->flags & FL_FORCEMODE)
	{
		gi.Printf("unlimitedpower OFF\n");
		selected->flags ^= FL_FORCEMODE;
	}
	else
	{
		gi.Printf("unlimitedpower ON\n");
		selected->flags |= FL_FORCEMODE;
	}
}

static void Workshop_Block_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	if (selected->flags & FL_BLOCKPOINTMODE)
	{
		gi.Printf("thisweaponisyourlife OFF\n");
		selected->flags ^= FL_BLOCKPOINTMODE;
	}
	else
	{
		gi.Printf("thisweaponisyourlife ON\n");
		selected->flags |= FL_BLOCKPOINTMODE;
	}
}

static void Workshop_Stamina_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	if (selected->flags & FL_STAMINA_MODE)
	{
		gi.Printf("unlimitedstamina OFF\n");
		selected->flags ^= FL_STAMINA_MODE;
	}
	else
	{
		gi.Printf("unlimitedstamina ON\n");
		selected->flags |= FL_STAMINA_MODE;
	}
}

extern void WP_RemoveSaber(gentity_t* ent, int saber_num);
extern qboolean WP_SaberParseParms(const char* SaberName, saberInfo_t* saber, qboolean setColors = qtrue);
extern qboolean WP_UseFirstValidSaberStyle(const gentity_t* ent, int* saber_anim_level);
extern void WP_SaberSetDefaults(saberInfo_t* saber, qboolean set_colors = qtrue);

static int WP_SetSaberNPCModel(gclient_t* client, const class_t npcClass)
{
	if (client)
	{
		switch (npcClass)
		{
		case CLASS_DESANN: //Desann
			client->ps.saber[0].model = "models/weapons2/saber_desann/saber_w.glm";
			break;
		case CLASS_VADER: //Desann
			client->ps.saber[0].model = "models/weapons2/saber_VaderEp5/saber_w.glm";
			break;
		case CLASS_LUKE: //Luke
			client->ps.saber[0].model = "models/weapons2/saber_luke/saber_w.glm";
			break;
		case CLASS_YODA: //YODA
			client->ps.saber[0].model = "models/weapons2/saber_plasma/saber_yoda.glm";
			break;
		case CLASS_PLAYER: //Kyle NPC and player
		case CLASS_PROJECTION:
		case CLASS_KYLE: //Kyle NPC and player
			client->ps.saber[0].model = "models/weapons2/saber/saber_w.glm";
			break;
		default: //reborn and tavion and everyone
			client->ps.saber[0].model = DEFAULT_SABER_MODEL;
			break;
		}
		return G_ModelIndex(client->ps.saber[0].model);
	}
	switch (npcClass)
	{
	case CLASS_DESANN: //Desann
		return G_ModelIndex("models/weapons2/saber_desann/saber_w.glm");
	case CLASS_VADER: //Desann
		return G_ModelIndex("models/weapons2/saber_VaderEp5/saber_w.glm");
	case CLASS_LUKE: //Luke
		return G_ModelIndex("models/weapons2/saber_luke/saber_w.glm");
	case CLASS_YODA: //Luke
		return G_ModelIndex("models/weapons2/saber_plasma/saber_yoda.glm");
	case CLASS_PLAYER: //Kyle NPC and player
	case CLASS_PROJECTION:
	case CLASS_KYLE: //Kyle NPC and player
		return G_ModelIndex("models/weapons2/saber/saber_w.glm");
	default: //reborn and tavion and everyone else
		return G_ModelIndex(DEFAULT_SABER_MODEL);
	}
}

static void WP_RemovenpcSaber(gentity_t* ent, const int saber_num)
{
	gentity_t* selected = &g_entities[selectedAI];

	if (!selected || !selected->client)
	{
		return;
	}
	//reset everything for this saber just in case
	WP_SaberSetDefaults(&selected->client->ps.saber[saber_num]);

	selected->client->ps.dualSabers = qfalse;
	selected->client->ps.saber[saber_num].Deactivate();
	selected->client->ps.saber[saber_num].SetLength(0.0f);
	if (selected->weaponModel[saber_num] > 0)
	{
		gi.G2API_SetSkin(&selected->ghoul2[selected->weaponModel[saber_num]], -1, 0);
		gi.G2API_RemoveGhoul2Model(selected->ghoul2, selected->weaponModel[saber_num]);
		selected->weaponModel[saber_num] = -1;
	}
	if (selected->client->ps.saber_anim_level == SS_DUAL || selected->client->ps.saber_anim_level == SS_STAFF)
	{
		//change to the style to the default
		for (int i = SS_NONE + 1; i < SS_NUM_SABER_STYLES; i++)
		{
			if (selected->client->ps.saberStylesKnown & 1 << i)
			{
				selected->client->ps.saber_anim_level = i;
				if (selected->s.number < MAX_CLIENTS)
				{
					cg.saberAnimLevelPending = selected->client->ps.saber_anim_level;
				}
				break;
			}
		}
	}
}

static void WP_SetSaberNPCEntModelSkin(gentity_t* ent, gentity_t* saberent)
{
	const gentity_t* selected = &g_entities[selectedAI];
	int saberModel;
	qboolean newModel = qfalse;

	if (!selected->client->ps.saber[0].model)
	{
		saberModel = WP_SetSaberNPCModel(selected->client, selected->client->NPC_class);
	}
	else
	{
		//got saberModel from NPCs.cfg
		saberModel = G_ModelIndex(selected->client->ps.saber[0].model);
	}
	if (saberModel && saberent->s.modelindex != saberModel)
	{
		if (saberent->playerModel >= 0)
		{
			//remove the old one, if there is one
			gi.G2API_RemoveGhoul2Model(saberent->ghoul2, saberent->playerModel);
		}
		//add the new one
		saberent->playerModel = gi.G2API_InitGhoul2Model(saberent->ghoul2, selected->client->ps.saber[0].model,
			saberModel, NULL_HANDLE, NULL_HANDLE, 0, 0);
		saberent->s.modelindex = saberModel;
		newModel = qtrue;
	}
	//set skin, too
	if (selected->client->ps.saber[0].skin == nullptr)
	{
		gi.G2API_SetSkin(&saberent->ghoul2[0], -1, 0);
	}
	else
	{
		//if this saber has a customSkin, use it
		// lets see if it's out there
		const int saberSkin = gi.RE_RegisterSkin(selected->client->ps.saber[0].skin);
		if (saberSkin && (newModel || saberent->s.modelindex2 != saberSkin))
		{
			// put it in the config strings
			// and set the ghoul2 model to use it
			gi.G2API_SetSkin(&saberent->ghoul2[0], G_SkinIndex(selected->client->ps.saber[0].skin), saberSkin);
			saberent->s.modelindex2 = saberSkin;
		}
	}
}

static void WP_SetSecondSaberNPCEntModelSkin(gentity_t* ent, gentity_t* saberent)
{
	const gentity_t* selected = &g_entities[selectedAI];
	int saberModel;
	qboolean newModel = qfalse;

	if (!selected->client->ps.saber[1].model)
	{
		saberModel = WP_SetSaberNPCModel(selected->client, selected->client->NPC_class);
	}
	else
	{
		//got saberModel from NPCs.cfg
		saberModel = G_ModelIndex(selected->client->ps.saber[1].model);
	}
	if (saberModel && saberent->s.modelindex != saberModel)
	{
		if (saberent->playerModel >= 0)
		{
			//remove the old one, if there is one
			gi.G2API_RemoveGhoul2Model(saberent->ghoul2, saberent->playerModel);
		}
		//add the new one
		saberent->playerModel = gi.G2API_InitGhoul2Model(saberent->ghoul2, selected->client->ps.saber[1].model,
			saberModel, NULL_HANDLE, NULL_HANDLE, 0, 0);
		saberent->s.modelindex = saberModel;
		newModel = qtrue;
	}
	//set skin, too
	if (selected->client->ps.saber[1].skin == nullptr)
	{
		gi.G2API_SetSkin(&saberent->ghoul2[0], -1, 0);
	}
	else
	{
		//if this saber has a customSkin, use it
		// lets see if it's out there
		const int saberSkin = gi.RE_RegisterSkin(selected->client->ps.saber[1].skin);
		if (saberSkin && (newModel || saberent->s.modelindex2 != saberSkin))
		{
			// put it in the config strings
			// and set the ghoul2 model to use it
			gi.G2API_SetSkin(&saberent->ghoul2[0], G_SkinIndex(selected->client->ps.saber[1].skin), saberSkin);
			saberent->s.modelindex2 = saberSkin;
		}
	}
}

void WP_SetNPCSaber(gentity_t* ent, const int saber_num, const char* saberName)
{
	gentity_t* selected = &g_entities[selectedAI];

	if (!selected || !selected->client)
	{
		return;
	}
	if (Q_stricmp("none", saberName) == 0 || Q_stricmp("remove", saberName) == 0)
	{
		WP_RemoveSaber(selected, saber_num);
		return;
	}
	if (selected->weaponModel[saber_num] > 0)
	{
		gi.G2API_RemoveGhoul2Model(selected->ghoul2, selected->weaponModel[saber_num]);
		selected->weaponModel[saber_num] = -1;
	}
	WP_SaberParseParms(saberName, &selected->client->ps.saber[saber_num]); //get saber info
	if (selected->client->ps.saber[saber_num].stylesLearned)
	{
		selected->client->ps.saberStylesKnown |= selected->client->ps.saber[saber_num].stylesLearned;
	}
	if (selected->client->ps.saber[saber_num].singleBladeStyle)
	{
		selected->client->ps.saberStylesKnown |= selected->client->ps.saber[saber_num].singleBladeStyle;
	}
	if (saber_num == 1 && selected->client->ps.saber[1].saberFlags & SFL_TWO_HANDED)
	{
		//not allowed to use a 2-handed saber as second saber
		WP_RemoveSaber(selected, saber_num);
		return;
	}
	G_ModelIndex(selected->client->ps.saber[saber_num].model);
	WP_SaberInitBladeData(selected);
	if (saber_num == 1)
	{
		//now have 2 sabers
		selected->client->ps.dualSabers = qtrue;
	}
	if (selected->client->ps.weapon == WP_SABER)
	{
		WP_SaberAddG2SaberModels(selected, saber_num);
		selected->client->ps.saber[saber_num].SetLength(0.0f);
		selected->client->ps.saber[saber_num].Activate();
	}
	else
	{
		G_RemoveHolsterModels(selected);
		WP_SaberAddHolsteredG2SaberModels(selected);
	}

	if (selected->client->ps.saber[saber_num].stylesLearned)
	{
		//change to the style we're supposed to be using
		selected->client->ps.saberStylesKnown |= selected->client->ps.saber[saber_num].stylesLearned;
	}
	if (selected->client->ps.saber[saber_num].singleBladeStyle)
	{
		selected->client->ps.saberStylesKnown |= selected->client->ps.saber[saber_num].singleBladeStyle;
	}
	WP_UseFirstValidSaberStyle(selected, &selected->client->ps.saber_anim_level);
}

static void Workshop_Set_Saber_f(gentity_t* ent)
{
	const char* saber = gi.argv(1);
	const char* saber2 = gi.argv(2);
	char name[MAX_CVAR_VALUE_STRING] = { 0 };
	gentity_t* selected = &g_entities[selectedAI];

	if (gi.argc() < 2)
	{
		gi.Printf("Usage: saber <saber1> <saber2>\n");
		gi.Cvar_VariableStringBuffer("g_NPCsaber", name, sizeof name);
		gi.Printf("g_NPCsaber is set to %s\n", name);
		gi.Cvar_VariableStringBuffer("g_NPCsabertwo", name, sizeof name);
		if (name[0])
			gi.Printf("g_NPCsabertwo is set to %s\n", name);
		return;
	}

	if (!selected || !saber || !saber[0])
	{
		return;
	}

	gi.cvar_set("g_NPCsaber", saber);
	WP_SetNPCSaber(selected, 0, saber);
	WP_SetSaberNPCEntModelSkin(selected, &g_entities[selected->client->ps.saberEntityNum]);

	if (saber2 && saber2[0] && !(selected->client->ps.saber[0].saberFlags & SFL_TWO_HANDED))
	{
		//want to use a second saber and first one is not twoHanded
		gi.cvar_set("g_NPCsabertwo", saber2);
		WP_SetNPCSaber(selected, 1, saber2);
		WP_SetSaberNPCEntModelSkin(selected, &g_entities[selected->client->ps.saberEntityNum]);
	}
	else
	{
		gi.cvar_set("g_NPCsabertwo", "");
		WP_RemoveSaber(selected, 1);
	}
	WP_UseFirstValidSaberStyle(selected, &selected->client->ps.saber_anim_level);
}

static void Workshop_Set_SaberSecond_f(gentity_t* ent)
{
	const char* saber2 = gi.argv(2);
	char name[MAX_CVAR_VALUE_STRING] = { 0 };
	gentity_t* selected = &g_entities[selectedAI];

	if (gi.argc() < 2)
	{
		gi.Cvar_VariableStringBuffer("g_NPCsabertwo", name, sizeof name);
		if (name[0])
			gi.Printf("g_NPCsabertwo is set to %s\n", name);
		return;
	}

	if (!selected || !saber2 || !saber2[1])
	{
		return;
	}

	if (saber2 && !(selected->client->ps.saber[0].saberFlags & SFL_TWO_HANDED))
	{
		//want to use a second saber and first one is not twoHanded
		gi.cvar_set("g_NPCsabertwo", saber2);
		WP_SetNPCSaber(selected, 1, saber2);
		WP_SetSaberNPCEntModelSkin(selected, &g_entities[selected->client->ps.saberEntityNum]);
	}
	else
	{
		gi.cvar_set("g_NPCsabertwo", "");
		WP_RemoveSaber(selected, 1);
	}
	WP_UseFirstValidSaberStyle(selected, &selected->client->ps.saber_anim_level);
}

static void Workshop_Set_SaberSingle_f(gentity_t* ent)
{
	const char* saber = gi.argv(1);
	char name[MAX_CVAR_VALUE_STRING] = { 0 };
	gentity_t* selected = &g_entities[selectedAI];

	if (gi.argc() < 2)
	{
		gi.Printf("Usage: saber <saber1> <saber2>\n");
		gi.Cvar_VariableStringBuffer("g_NPCsaber", name, sizeof name);
		gi.Printf("g_NPCsaber is set to %s\n", name);
		return;
	}

	if (!selected || !saber || !saber[0])
	{
		return;
	}

	gi.cvar_set("g_NPCsaber", saber);
	WP_SetNPCSaber(selected, 0, saber);
	WP_SetSaberNPCEntModelSkin(selected, &g_entities[selected->client->ps.saberEntityNum]);

	gi.cvar_set("g_NPCsabertwo", "");
	WP_RemoveSaber(selected, 1);
	WP_UseFirstValidSaberStyle(selected, &selected->client->ps.saber_anim_level);
}

extern saber_colors_t TranslateSaberColor(const char* name);
extern cvar_t* g_NPCsabercolor;
extern cvar_t* g_NPCsabertwocolor;

static void Workshop_Set_SaberColor_f(gentity_t* ent)
{
	//FIXME: just list the colors, each additional listing sets that blade
	int saber_num = atoi(gi.argv(1));
	const char* color[MAX_BLADES]{};
	int blade_num;

	for (blade_num = 0; blade_num < MAX_BLADES; blade_num++)
	{
		color[blade_num] = gi.argv(2 + blade_num);
	}

	if (saber_num < 1 || saber_num > 2 || gi.argc() < 3)
	{
		gi.Printf("Usage:  saberColor <saber_num> <blade1 color> <blade2 color> ... <blade8 color>\n");
		gi.Printf("valid saber_nums:  1 or 2\n");
		gi.Printf("valid colors:  red, orange, yellow, green, blue, and purple\n");

		return;
	}
	saber_num--;

	const gentity_t* selected = &g_entities[selectedAI];

	for (blade_num = 0; blade_num < MAX_BLADES; blade_num++)
	{
		if (!color[blade_num] || !color[blade_num][0])
		{
			break;
		}
		selected->client->ps.saber[saber_num].blade[blade_num].color = TranslateSaberColor(color[blade_num]);
	}

	if (saber_num == 0)
	{
		gi.cvar_set("g_NPCsabercolor", color[0]);
	}
	else if (saber_num == 1)
	{
		gi.cvar_set("g_NPCsabertwocolor", color[0]);
	}
}

// Notarget mode for the NPC
static void Workshop_Notarget_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	if (selected->flags & FL_NOTARGET)
	{
		gi.Printf("notarget OFF\n");
		selected->flags ^= FL_NOTARGET;
	}
	else
	{
		gi.Printf("notarget ON\n");
		selected->flags |= FL_NOTARGET;
	}
}

// Undying mode for the NPC
static void Workshop_Undying_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	const char* msg;
	if (selected->flags & FL_UNDYING)
	{
		msg = "undead mode OFF\n";
	}
	else
	{
		msg = "undead mode ON\n";
	}
	selected->flags ^= FL_UNDYING;
	gi.Printf(msg);
}

// Shielding for the NPC
static void Workshop_Shielding_f(gentity_t* ent)
{
	gentity_t* selected = &g_entities[selectedAI];
	const char* msg;
	if (selected->flags & FL_SHIELDED)
	{
		msg = "shielded mode OFF\n";
	}
	else
	{
		msg = "shielded mode ON\n";
	}
	selected->flags ^= FL_SHIELDED;
	gi.Printf(msg);
}

// Toggle the detailed display of the workshop
static void Workshop_ToggleDisplay_f(gentity_t* ent)
{
	showDisplay = static_cast<qboolean>(!showDisplay);
}

// Toggle following
static void Workshop_Toggle_Follow_f(gentity_t* ent)
{
	const gentity_t* ai = &g_entities[selectedAI];

	if (ai->client->leader)
	{
		ai->client->leader = nullptr;
		ai->NPC->defaultBehavior = BS_STAND_GUARD;
		ai->NPC->behaviorState = BS_DEFAULT;
	}
	else
	{
		ai->client->leader = &g_entities[0];
		ai->NPC->defaultBehavior = BS_FOLLOW_LEADER;
		ai->NPC->behaviorState = BS_FOLLOW_LEADER;
	}
}

// Stubs
void Workshop_Commands_f(gentity_t* ent);
void Workshop_CmdHelp_f(gentity_t* ent);

constexpr auto WSFLAG_NEEDSELECTED = 1;
constexpr auto WSFLAG_ONLYINWS = 2;

struct workshopCmd_t
{
	char* command;
	char* help;
	int flags;
	void (*Command)(gentity_t*);
};

workshopCmd_t workshopCommands[] =
{
	{"aiworkshop", "Toggles the AI workshop", 0, WorkshopToggle},
	{"workshop_commands", "Lists all AI workshop commands", 0, Workshop_Commands_f},
	{"workshop_cmdhelp", "Provides detailed information about an AI workshop command", 0, Workshop_CmdHelp_f},
	{
		"workshop_toggle_display", "Toggles the detailed display of the AI workshop",
		WSFLAG_NEEDSELECTED | WSFLAG_ONLYINWS, Workshop_ToggleDisplay_f
	},
	{"workshop_select", "Selects an NPC that you are looking at.", WSFLAG_ONLYINWS, WorkshopSelect_f},
	{
		"workshop_deselect", "Deselects your currently selected NPC.", WSFLAG_NEEDSELECTED | WSFLAG_ONLYINWS,
		WorkshopDeselect_f
	},
	{
		"workshop_list_bstates", "Lists all of the Behavior States that an NPC can be in.", WSFLAG_ONLYINWS,
		Workshop_List_BehaviorState_f
	},
	{
		"workshop_list_scriptflags", "Lists all of the Script Flags that an NPC can have.", WSFLAG_ONLYINWS,
		Workshop_List_Scriptflags_f
	},
	{
		"workshop_list_teams", "Lists all of the Teams that an NPC can belong to or fight.", WSFLAG_ONLYINWS,
		Workshop_List_Team_f
	},
	{
		"workshop_list_aiflags", "Lists all of the AI Flags that an NPC can posses.", WSFLAG_ONLYINWS,
		Workshop_List_AIFlags_f
	},
	{
		"workshop_list_classes", "Lists all of the Classes that an AI can be in.", WSFLAG_ONLYINWS,
		Workshop_List_Classes_f
	},
	{"workshop_list_ranks", "Lists all of the Ranks that an AI can be.", WSFLAG_ONLYINWS, Workshop_List_Ranks_f},
	{
		"workshop_list_movetypes", "Lists all of the Move Types that an AI can use.", WSFLAG_ONLYINWS,
		Workshop_List_Movetypes_f
	},
	{
		"workshop_list_forcepowers", "Lists all of the Force Powers that an AI can have.", WSFLAG_ONLYINWS,
		Workshop_List_ForcePowers_f
	},
	{
		"workshop_list_bsets", "Lists all of the Behavior Sets that an NPC can attain.", WSFLAG_ONLYINWS,
		Workshop_List_BehaviorSets_f
	},
	{
		"workshop_list_anims", "Lists all of the animations that can be played by an NPC.", WSFLAG_ONLYINWS,
		Workshop_List_Anims_f
	},
	{
		"workshop_view_timers",
		"Lists all of the timers (alive or dead) that are active on the currently selected NPC.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_View_Timers_f
	},
	{"workshop_set_timer", "Sets a timer on an NPC", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Timer_f},
	{
		"workshop_set_bstate", "Changes the Behavior State of an NPC", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,
		Workshop_Set_BehaviorState_f
	},
	{
		"workshop_set_goalent",
		"Sets the NPC's navgoal to be the thing that you are looking at.\nYou can use \"me\" or an entity number as an optional argument.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_GoalEntity_f
	},
	{
		"workshop_set_leader",
		"Gives the NPC a leader - the thing you are looking at.\nYou can use \"me\" or an entity number as an optional argument.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Leader_f
	},
	{
		"workshop_set_enemy",
		"Gives the NPC an enemy - the thing you are looking at.\nYou can use \"me\" or an entity number as an optional argument.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Enemy_f
	},
	{
		"workshop_set_scriptflags", "Sets the NPC's scriptflags. Use workshop_list_scriptflags to get a list of these.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Scriptflags_f
	},
	{
		"workshop_set_weapon",
		"Sets the NPC's weapon. You can use \"me\" instead of a weapon name to have them change to your weapon.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Weapon_f
	},
	{
		"workshop_set_team",
		"Sets the NPC's team. This does not affect who they shoot at, only who shoots at them. Change their enemyteam for that.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Team_f
	},
	{
		"workshop_set_enemyteam", "Sets who the NPC will shoot at. Stormtroopers shoot at TEAM_PLAYER for instance.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Enemyteam_f
	},
	{
		"workshop_set_aiflags", "Sets the NPC's AI Flags. These are a second set of temporary flags.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Aiflags_f
	},
	{
		"workshop_set_class",
		"Sets the NPC's class. Note that this is sometimes ignored, like if they are using a lightsaber.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Class_f
	},
	{
		"workshop_set_rank", "Sets the NPC's rank. Rank has some minor effects on behavior.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Rank_f
	},
	{
		"workshop_set_movetype", "Sets the NPC's movetype. Rather self-explanatory.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Movetype_f
	},
	{
		"workshop_set_forcepower",
		"Gives the NPC a force power. Setting their force power to 0 takes the power away from them.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Forcepower_f
	},
	{
		"workshop_set_bsetscript",
		"Makes the NPC use an ICARUS script when a certain behaviorset is activated. These are things like angerscript, spawnscript, etc.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_BSetScript_f
	},
	{
		"workshop_set_parm", "Sets a parm that can be read with ICARUS.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,
		Workshop_Set_Parm_f
	},
	{"workshop_set_health", "Sets the NPC's health.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Health_f},
	{"workshop_set_armor", "Sets the NPC's armor.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Armor_f},
	{"workshop_set_gravity", "Sets an NPC's gravity.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Gravity_f},
	{"workshop_set_scale", "Sets an NPC's scale.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Scale_f},
	{
		"workshop_play_dialogue", "Plays a line of dialogue from the NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,
		Workshop_Play_Dialogue_f
	},
	{
		"workshop_toggle_follow", "Toggles the NPC following you or not.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,
		Workshop_Toggle_Follow_f
	},
	{
		"workshop_set_anim", "Sets the current animation of the NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,
		Workshop_Set_Animation_f
	},
	{
		"workshop_activate_bset", "Activates a Behavior Set on an NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,
		Workshop_Activate_BSet_f
	},
	{
		"workshop_control", "Allows you to control an NPC, like with Mind Trick level 4.",
		WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_MindControl_f
	},
	{"workshop_end_control", "Stops controlling an NPC", WSFLAG_ONLYINWS, Workshop_EndMindControl_f},
	{"workshop_freeze", "Freezes an NPC", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Freeze_f},
	{"workshop_kill", "Kills the NPC", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Kill_f},
	{
		"workshop_god", "Uses the god cheat (invincibility) on an NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,	Workshop_God_f
	},
	{
		"workshop_notarget","Uses the notarget cheat (can't be seen by enemies) on an NPC. Some NPCs have notarget on by default.",	WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Notarget_f
	},
	{
		"workshop_undying", "Uses the undead mode cheat (cannot die) on an NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,Workshop_Undying_f
	},
	{
		"workshop_shielding", "Toggles a blaster-protecting shield on an NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,Workshop_Shielding_f
	},
	//
	{
		"workshop_force", "Uses the force cheat (unlimitedpower) on an NPC.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,Workshop_Force_f
	},
	{
		"workshop_block", "Uses the block cheat (thisweaponisyourlife) on an NPC.",WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Block_f
	},
	{
		"workshop_stamina", "Uses the stamina cheat (unlimitedstamina) on an NPC.",WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Stamina_f
	},
	//
	{"Workshop_Set_Saber", "Sets the NPC's saber.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED, Workshop_Set_Saber_f},
	{
		"Workshop_Set_SaberSingle", "Sets the NPC's Single saber.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,	Workshop_Set_SaberSingle_f
	},
	{
		"Workshop_Set_SaberSecond", "Sets the NPC's Second saber.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,Workshop_Set_SaberSecond_f
	},
	{
		"Workshop_Set_SaberColor", "Sets the NPC's sabercolor.", WSFLAG_ONLYINWS | WSFLAG_NEEDSELECTED,	Workshop_Set_SaberColor_f
	},
	//
	{"", "", 0, nullptr},
};

void Workshop_Commands_f(gentity_t* ent)
{
	workshopCmd_t* cmd = workshopCommands;
	while (cmd->Command)
	{
		Com_Printf("%s\n", cmd->command);
		cmd++;
	}
	Com_Printf("You can receive help with any of these commands using the workshop_cmdhelp command.\n");
}

void Workshop_CmdHelp_f(gentity_t* ent)
{
	if (gi.argc() != 2)
	{
		Com_Printf("usage: workshop_cmdhelp <command name>\n");
		return;
	}

	workshopCmd_t* cmd = workshopCommands;
	const char* text = gi.argv(1);
	while (cmd->Command)
	{
		if (!Q_stricmp(cmd->command, text))
		{
			Com_Printf("%s\n", cmd->help);
			return;
		}
		cmd++;
	}
	Com_Printf("Command not found.\n");
}

qboolean TryWorkshopCommand(gentity_t* ent)
{
	if (gi.argc() == 0)
	{
		// how in the fuck..
		return qfalse;
	}
	const char* text = gi.argv(0);
	workshopCmd_t* cmd = workshopCommands;
	while (cmd->Command)
	{
		if (!Q_stricmp(cmd->command, text))
		{
			if (!inAIWorkshop && cmd->flags & WSFLAG_ONLYINWS)
			{
				Com_Printf("You need to be in the AI workshop to use that command.\n");
			}
			else if (selectedAI == ENTITYNUM_NONE && cmd->flags & WSFLAG_NEEDSELECTED)
			{
				Com_Printf("You need to have selected an NPC first in order to use that command.\n");
			}
			else
			{
				cmd->Command(ent);
			}
			return qtrue;
		}
		cmd++;
	}
	return qfalse;
}