/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

/*
===========================================================================
AI fighter pilots for the MP space maps played in singleplayer (mp/siege_destroyer2 and the like)

The map's ship spawners (NPC_Vehicle with an MP "teamowner") make the ships of two sides: team 1, the player's
(Rebels), and team 2, the enemy (Imperials). When such a fighter is made, it can get an NPC pilot
(G_FighterAI_VehicleSpawned): the enemy's ships up to g_spaceEnemies flying at once, the player's up to
g_spaceWingmen (never the X-wings: those are left for the player to fly). The pilot flies it by filling in its
usercmd like a player (NPC_FighterAI, run from NPC_RunBehavior): it picks an enemy ship, leads it and shoots when it
is lined up, breaks away when too close, turns away from the Star Destroyer, asteroids and walls ahead of it, and
uses turbo and its missiles now and then. Without an enemy, a wingman flies with the player's ship, an enemy patrols
(along the map's ship route, if it has one: see "Ship routes" below).

g_spaceBattle 0 switches it off, g_spaceAISkill sets how good they are (0-3, -1: the game's skill, g_spskill).

The AI's state is in fields that are saved: NPC->enemy (the target), NPC->pos3 (where it is heading to without an
enemy, or away to when breaking off), NPC->pos4 (where it started, the centre of a patrol) and its TIMERs.
===========================================================================
*/

#include "b_local.h"
#include "g_functions.h"
#include "g_vehicles.h"

extern gentity_t* NPC_Spawn_Do(gentity_t* ent, qboolean fullSpawnNow);
extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern cvar_t* g_spskill;

static constexpr auto FIGHTER_PILOT_CLASSNAME = "fighter_pilot";
static constexpr float FIGHTER_THINK_SECONDS = 0.05f; // NPCs think every 50 msec
static constexpr float FIGHTER_FIRE_CONE = 6.0f; // degrees off the aim point it still shoots at
static constexpr float FIGHTER_FIRE_RANGE = 7000.0f;
static constexpr float FIGHTER_BREAK_DIST = 450.0f; // closer than this to its target, it breaks away
static constexpr float FIGHTER_MAX_PITCH = 70.0f; // a rider's pitch is clamped to about 75

static cvar_t* g_spaceBattle;
static cvar_t* g_spaceWingmen;
static cvar_t* g_spaceEnemies;
static cvar_t* g_spaceAISkill;

static int route_num_points; // the map's ship route (see "Ship routes" below)

static void Fighter_RegisterCvars()
{
	if (!g_spaceBattle)
	{
		g_spaceBattle = gi.cvar("g_spaceBattle", "1", CVAR_ARCHIVE);
		g_spaceWingmen = gi.cvar("g_spaceWingmen", "3", CVAR_ARCHIVE);
		g_spaceEnemies = gi.cvar("g_spaceEnemies", "5", CVAR_ARCHIVE);
		g_spaceAISkill = gi.cvar("g_spaceAISkill", "-1", CVAR_ARCHIVE);
	}
}

// 0 (easy) .. 3 (hard)
static int Fighter_Skill()
{
	int skill = g_spaceAISkill->integer;
	if (skill < 0)
	{
		skill = g_spskill ? g_spskill->integer : 1;
	}
	return skill < 0 ? 0 : skill > 3 ? 3 : skill;
}

static qboolean Fighter_IsFighter(const gentity_t* ent)
{
	return static_cast<qboolean>(ent && ent->inuse && ent->client && ent->client->NPC_class == CLASS_VEHICLE
		&& ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo
		&& ent->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER);
}

// the side a ship is on is its pilot's (an empty ship is on no side)
static team_t Fighter_ShipTeam(const gentity_t* ship)
{
	const gentity_t* pilot = ship->m_pVehicle->m_pPilot;
	if (!pilot || !pilot->client || pilot->health <= 0)
	{
		return TEAM_FREE;
	}
	return pilot->client->playerTeam;
}

// how many ships of this side NPCs fly now
static int Fighter_CrewCount(const team_t team)
{
	int count = 0;
	for (int i = MAX_CLIENTS; i < globals.num_entities; i++)
	{
		const gentity_t* ent = &g_entities[i];
		if (Fighter_IsFighter(ent) && ent->health > 0 && ent->m_pVehicle->m_pPilot
			&& ent->m_pVehicle->m_pPilot->s.number >= MAX_CLIENTS && Fighter_ShipTeam(ent) == team)
		{
			count++;
		}
	}
	return count;
}

/*
-------------------------
G_FighterAI_VehicleSpawned

A ship an MP map's spawner made (G_VehicleSpawn): it is on the spawner's side, and a fighter can get a pilot.
-------------------------
*/
void G_FighterAI_VehicleSpawned(const gentity_t* spawner, gentity_t* veh)
{
	if (!spawner || !veh || !veh->client || spawner->noDamageTeam == TEAM_FREE)
	{
		return;
	}
	Fighter_RegisterCvars();

	const team_t team = spawner->noDamageTeam;
	veh->client->playerTeam = team;
	veh->client->enemyTeam = team == TEAM_PLAYER ? TEAM_ENEMY : TEAM_PLAYER;

	if (!g_spaceBattle->integer || !Fighter_IsFighter(veh) || !veh->NPC_type)
	{
		return;
	}
	if (Q_stristr(veh->NPC_type, "shuttle") || Q_stristr(veh->NPC_type, "yt-1300"))
	{
		return; // the transports are scenery
	}
	if (team == TEAM_PLAYER)
	{
		if (Q_stristr(veh->NPC_type, "x-wing") || Fighter_CrewCount(team) >= g_spaceWingmen->integer)
		{
			return; // the X-wings are the player's to fly
		}
	}
	else if (Fighter_CrewCount(team) >= g_spaceEnemies->integer)
	{
		return;
	}

	// a pilot, made right in the ship (it is hidden in there) and put in it
	gentity_t* spawner_ent = G_Spawn();
	if (!spawner_ent)
	{
		return;
	}
	spawner_ent->classname = "NPC_spawner";
	spawner_ent->NPC_type = team == TEAM_PLAYER ? "Rebel" : "StormPilot";
	spawner_ent->count = 1;
	spawner_ent->spawnflags = 64 | 128; // NOTSOLID | STARTINSOLID: it starts inside the ship
	VectorCopy(veh->currentOrigin, spawner_ent->s.origin);
	G_SetOrigin(spawner_ent, veh->currentOrigin);

	gentity_t* pilot = NPC_Spawn_Do(spawner_ent, qtrue);
	if (!pilot || !pilot->client || !pilot->NPC)
	{
		return;
	}

	pilot->classname = const_cast<char*>(FIGHTER_PILOT_CLASSNAME);
	pilot->client->playerTeam = team;
	pilot->client->enemyTeam = veh->client->enemyTeam;
	// a ship only fires for a pilot with no weapon in his hands (FireVehicleWeapon)
	G_RemoveWeaponModels(pilot);
	pilot->client->ps.weapon = WP_NONE;
	pilot->s.weapon = WP_NONE;

	Vehicle_t* p_veh = veh->m_pVehicle;
	if (!p_veh->m_pVehicleInfo->Board(p_veh, pilot))
	{
		G_FreeEntity(pilot);
		return;
	}
	p_veh->m_iBoarding = 0; // in it at once, no boarding time

	// a ship docked in its rack (SUSPENDED) is let go, as when the player gets in (Board): it drops clear of the
	// rack first ("dropTime"), then flies out
	int launch_time = 2500;
	if (veh->spawnflags & 2)
	{
		veh->spawnflags &= ~2;
		G_Sound(veh, G_SoundIndex("sound/vehicles/common/release.wav"));
		if (veh->fly_sound_debounce_time)
		{
			p_veh->m_iDropTime = level.time + veh->fly_sound_debounce_time;
			launch_time += veh->fly_sound_debounce_time;
		}
	}

	if (g_developer && g_developer->integer)
	{
		gi.Printf("fighter AI: %s %d flies %s %d for team %d\n", pilot->NPC_type, pilot->s.number, veh->NPC_type,
			veh->s.number, team);
	}

	pilot->painDebounceTime = level.time; // (a hidden pilot is never hurt: the developer messages use it as boarding time)
	VectorCopy(veh->currentOrigin, pilot->pos4); // home, the centre of its patrol
	VectorCopy(veh->currentOrigin, pilot->pos3);
	TIMER_Set(pilot, "fighterLaunch", launch_time); // drops clear and flies straight out of its hangar first
	TIMER_Set(pilot, "fighterRetarget", 0);
}

// ships of the other side with a live pilot: the nearest, liking the ones in front of it, and (a wingman) the ones
// after the player
static gentity_t* Fighter_FindTarget(const gentity_t* ship, const team_t my_team, const vec3_t fwd)
{
	gentity_t* best = nullptr;
	float best_score = 1.0e30f;
	const gentity_t* player_ship = player && player->client && player->client->ps.m_iVehicleNum
		? &g_entities[player->client->ps.m_iVehicleNum]
		: nullptr;
	// on a map with a route they fly it until they meet the enemy; without one they see the whole map
	const float max_dist = route_num_points ? 20000.0f : 40000.0f;

	for (int i = 0; i < globals.num_entities; i++)
	{
		gentity_t* ent = &g_entities[i];
		if (ent == ship || !Fighter_IsFighter(ent) || ent->health <= 0)
		{
			continue;
		}
		const team_t team = Fighter_ShipTeam(ent);
		if (team == TEAM_FREE || team == my_team || team == TEAM_NEUTRAL)
		{
			continue;
		}
		const gentity_t* pilot = ent->m_pVehicle->m_pPilot;
		if (pilot->flags & FL_NOTARGET)
		{
			continue;
		}
		vec3_t dir;
		VectorSubtract(ent->currentOrigin, ship->currentOrigin, dir);
		const float dist = VectorNormalize(dir);
		if (dist > max_dist)
		{
			continue;
		}
		float score = dist;
		if (DotProduct(dir, fwd) > 0.5f)
		{
			score *= 0.6f;
		}
		if (my_team == TEAM_PLAYER && player_ship && pilot->enemy == player_ship)
		{
			score *= 0.4f; // on the player's tail
		}
		if (ent == NPC->enemy)
		{
			score *= 0.8f; // keep at the one it has
		}
		if (score < best_score)
		{
			best_score = score;
			best = ent;
		}
	}
	return best;
}

// turns the pilot's view (which the ship follows) toward dir, at most max_step degrees
static void Fighter_TurnTowards(const vec3_t dir, const float max_step)
{
	vec3_t want;
	vectoangles(dir, want);
	want[PITCH] = AngleNormalize180(want[PITCH]);

	for (int axis = PITCH; axis <= YAW; axis++)
	{
		const float cur = client->ps.viewangles[axis];
		float delta = AngleSubtract(want[axis], cur);
		if (delta > max_step)
		{
			delta = max_step;
		}
		else if (delta < -max_step)
		{
			delta = -max_step;
		}
		float angle = AngleNormalize180(cur + delta);
		if (axis == PITCH)
		{
			angle = Com_Clamp(-FIGHTER_MAX_PITCH, FIGHTER_MAX_PITCH, angle);
			NPCInfo->desiredPitch = angle;
		}
		else
		{
			NPCInfo->desiredYaw = angle;
		}
		ucmd.angles[axis] = ANGLE2SHORT(angle) - client->ps.delta_angles[axis];
	}
}

// is there a ship of its own side between it and its target (don't shoot through friends)
static qboolean Fighter_FriendInLine(const gentity_t* ship, const vec3_t start, const vec3_t end, const team_t my_team)
{
	trace_t tr;
	gi.trace(&tr, start, nullptr, nullptr, end, ship->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);
	if (tr.entityNum >= ENTITYNUM_WORLD)
	{
		return qfalse;
	}
	const gentity_t* hit = &g_entities[tr.entityNum];
	if (Fighter_IsFighter(hit))
	{
		return static_cast<qboolean>(Fighter_ShipTeam(hit) == my_team);
	}
	return static_cast<qboolean>(hit->client && hit->client->playerTeam == my_team);
}

/*
===========================================================================
Ship routes

Points in open space a map's ships patrol along, kept in shiproutes/<map>.route (shiproutes/mp/siege_destroyer2.route).
They are made in the game, flying (or noclipping) around the map:

	ship_wp_add		a point where you are (your ship, if you are in one)
	ship_wp_rem		removes the point nearest to you
	ship_wp_show	shows the points and their links (on/off)
	ship_wp_info	checks the route: points in something solid or not linked, parts not joined to the rest
	ship_wp_save	writes the file
	ship_wp_load	reads it again (undoes changes not saved)
	ship_wp_clear	removes all the points

A point is linked to the nearest ones a ship can fly straight to (nothing in between), so a route never goes through
the Star Destroyer or an asteroid. A ship without an enemy (an enemy patrol, or a wingman when the player is not
flying) flies from point to point along the links, on to the one most ahead of it now and then another; when it
has fought, it goes back to the nearest point it can see. A map without a route file: ships patrol around where they
started.
===========================================================================
*/

static constexpr int ROUTE_MAX_POINTS = 128;
static constexpr int ROUTE_MAX_LINKS = 4; // each point is linked to its nearest few it can see (and they to it)
static constexpr float ROUTE_LINK_DIST = 22000.0f;
static constexpr float ROUTE_LINK_HULL = 64.0f; // the room a ship needs between two points
static constexpr float ROUTE_REACHED = 1500.0f; // they fly too fast to turn tighter than this around a point
static constexpr float ROUTE_SHOW_DIST = 30000.0f;

static vec3_t route_points[ROUTE_MAX_POINTS];
static qboolean route_links[ROUTE_MAX_POINTS][ROUTE_MAX_POINTS];
static qboolean route_links_dirty;
static qboolean route_show;
static int route_show_time;
static int route_cur[MAX_GENTITIES]; // the point a ship flies to (it is its pos3 then), not saved: it finds one again
static int route_prev[MAX_GENTITIES]; // the one it came from

static void Route_ResetShips()
{
	for (int i = 0; i < MAX_GENTITIES; i++)
	{
		route_cur[i] = route_prev[i] = -1;
	}
}

static const char* Route_FileName()
{
	return va("shiproutes/%s.route", level.mapname);
}

static qboolean Route_Clear(const vec3_t a, const vec3_t b)
{
	const vec3_t mins = { -ROUTE_LINK_HULL, -ROUTE_LINK_HULL, -ROUTE_LINK_HULL };
	const vec3_t maxs = { ROUTE_LINK_HULL, ROUTE_LINK_HULL, ROUTE_LINK_HULL };
	trace_t tr;
	gi.trace(&tr, a, mins, maxs, b, ENTITYNUM_NONE, MASK_SOLID, static_cast<EG2_Collision>(0), 0);
	return static_cast<qboolean>(!tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f);
}

// links each point to the nearest few it can see
static void Route_BuildLinks()
{
	if (!route_links_dirty)
	{
		return;
	}
	route_links_dirty = qfalse;
	memset(route_links, 0, sizeof route_links);

	for (int i = 0; i < route_num_points; i++)
	{
		int nearest[ROUTE_MAX_LINKS];
		float nearest_dist[ROUTE_MAX_LINKS];
		int count = 0;

		for (int j = 0; j < route_num_points; j++)
		{
			if (j == i)
			{
				continue;
			}
			const float dist = Distance(route_points[i], route_points[j]);
			if (dist > ROUTE_LINK_DIST)
			{
				continue;
			}
			// would it be one of the nearest (before the trace, the costly part)
			int slot = count;
			while (slot > 0 && nearest_dist[slot - 1] > dist)
			{
				slot--;
			}
			if (slot >= ROUTE_MAX_LINKS || !Route_Clear(route_points[i], route_points[j]))
			{
				continue;
			}
			if (count < ROUTE_MAX_LINKS)
			{
				count++;
			}
			for (int k = count - 1; k > slot; k--)
			{
				nearest[k] = nearest[k - 1];
				nearest_dist[k] = nearest_dist[k - 1];
			}
			nearest[slot] = j;
			nearest_dist[slot] = dist;
		}
		for (int k = 0; k < count; k++)
		{
			route_links[i][nearest[k]] = route_links[nearest[k]][i] = qtrue;
		}
	}
}

static void Route_Load(const qboolean report)
{
	route_num_points = 0;
	route_links_dirty = qtrue;
	Route_ResetShips();

	char* buffer = nullptr;
	const int len = gi.FS_ReadFile(Route_FileName(), reinterpret_cast<void**>(&buffer));
	if (len <= 0 || !buffer)
	{
		if (report)
		{
			gi.Printf(S_COLOR_YELLOW "No ship route file %s\n", Route_FileName());
		}
		return;
	}

	const char* line = buffer;
	while (line && *line && route_num_points < ROUTE_MAX_POINTS)
	{
		vec3_t point;
		if (line[0] != '/' && sscanf(line, "%f %f %f", &point[0], &point[1], &point[2]) == 3)
		{
			VectorCopy(point, route_points[route_num_points]);
			route_num_points++;
		}
		line = strchr(line, '\n');
		if (line)
		{
			line++;
		}
	}
	gi.FS_FreeFile(buffer);

	if (report || (g_developer && g_developer->integer))
	{
		gi.Printf("Ship route: %d points from %s\n", route_num_points, Route_FileName());
	}
}

static void Route_Save()
{
	fileHandle_t f;
	gi.FS_FOpenFile(Route_FileName(), &f, FS_WRITE);
	if (!f)
	{
		gi.Printf(S_COLOR_RED "Could not write %s\n", Route_FileName());
		return;
	}
	const char* header = va("// ship route for %s: x y z of each point (ship_wp_add, ship_wp_save)\n", level.mapname);
	gi.FS_Write(header, strlen(header), f);
	for (int i = 0; i < route_num_points; i++)
	{
		const char* line = va("%.0f %.0f %.0f\n", route_points[i][0], route_points[i][1], route_points[i][2]);
		gi.FS_Write(line, strlen(line), f);
	}
	gi.FS_FCloseFile(f);
	gi.Printf("Ship route: %d points saved to %s\n", route_num_points, Route_FileName());
}

static int Route_Nearest(const vec3_t pos, const qboolean must_see)
{
	int best = -1;
	float best_dist = 1.0e30f;
	for (int i = 0; i < route_num_points; i++)
	{
		const float dist = DistanceSquared(pos, route_points[i]);
		if (dist < best_dist && (!must_see || Route_Clear(pos, route_points[i])))
		{
			best_dist = dist;
			best = i;
		}
	}
	return best;
}

// where the player is (his ship's centre, when he flies one)
static void Route_PlayerPos(const gentity_t* ent, vec3_t pos)
{
	if (ent->client && ent->client->ps.m_iVehicleNum)
	{
		VectorCopy(g_entities[ent->client->ps.m_iVehicleNum].currentOrigin, pos);
	}
	else
	{
		VectorCopy(ent->currentOrigin, pos);
	}
}

/*
-------------------------
G_FighterRoute_ClientCommand

The ship_wp_ commands. qfalse: not one of them.
-------------------------
*/
qboolean G_FighterRoute_ClientCommand(const gentity_t* ent, const char* cmd)
{
	if (Q_stricmpn(cmd, "ship_wp_", 8))
	{
		return qfalse;
	}
	vec3_t pos;
	Route_PlayerPos(ent, pos);

	if (!Q_stricmp(cmd, "ship_wp_add"))
	{
		if (route_num_points >= ROUTE_MAX_POINTS)
		{
			gi.Printf(S_COLOR_RED "Ship route: no more than %d points\n", ROUTE_MAX_POINTS);
			return qtrue;
		}
		const int near_point = Route_Nearest(pos, qfalse);
		if (near_point >= 0 && Distance(pos, route_points[near_point]) < 1000.0f)
		{
			gi.Printf(S_COLOR_YELLOW "Ship route: point %d is only %.0f away, added anyway\n", near_point,
				Distance(pos, route_points[near_point]));
		}
		if (!Route_Clear(pos, pos))
		{
			gi.Printf(S_COLOR_YELLOW "Ship route: this point is in (or too near) something solid, added anyway\n");
		}
		VectorCopy(pos, route_points[route_num_points]);
		route_num_points++;
		route_links_dirty = qtrue;
		route_show = qtrue;
		route_show_time = 0;
		gi.Printf("Ship route: point %d added at (%.0f %.0f %.0f)\n", route_num_points - 1, pos[0], pos[1], pos[2]);
	}
	else if (!Q_stricmp(cmd, "ship_wp_rem"))
	{
		const int point = Route_Nearest(pos, qfalse);
		if (point < 0)
		{
			gi.Printf("Ship route: there are no points\n");
			return qtrue;
		}
		gi.Printf("Ship route: point %d removed, %.0f away\n", point, Distance(pos, route_points[point]));
		for (int i = point; i < route_num_points - 1; i++)
		{
			VectorCopy(route_points[i + 1], route_points[i]);
		}
		route_num_points--;
		route_links_dirty = qtrue;
		Route_ResetShips();
		route_show_time = 0;
	}
	else if (!Q_stricmp(cmd, "ship_wp_show"))
	{
		route_show = static_cast<qboolean>(!route_show);
		route_show_time = 0;
		gi.Printf("Ship route: %d points, %s\n", route_num_points, route_show ? "shown" : "hidden");
	}
	else if (!Q_stricmp(cmd, "ship_wp_save"))
	{
		Route_Save();
	}
	else if (!Q_stricmp(cmd, "ship_wp_load"))
	{
		Route_Load(qtrue);
		route_show_time = 0;
	}
	else if (!Q_stricmp(cmd, "ship_wp_info"))
	{
		// what is wrong with the route: points in something, points not linked, parts not joined to the rest
		Route_BuildLinks();
		int links = 0;
		int problems = 0;
		for (int i = 0; i < route_num_points; i++)
		{
			int count = 0;
			for (int j = 0; j < route_num_points; j++)
			{
				count += route_links[i][j] ? 1 : 0;
			}
			links += count;
			if (!Route_Clear(route_points[i], route_points[i]))
			{
				gi.Printf(S_COLOR_YELLOW "  point %d (%.0f %.0f %.0f) is in something solid\n", i, route_points[i][0],
					route_points[i][1], route_points[i][2]);
				problems++;
			}
			if (!count)
			{
				gi.Printf(S_COLOR_YELLOW "  point %d (%.0f %.0f %.0f) is not linked to any\n", i, route_points[i][0],
					route_points[i][1], route_points[i][2]);
				problems++;
			}
		}
		// the parts of the route (one is right: every point can be reached from every other)
		int group[ROUTE_MAX_POINTS];
		int groups = 0;
		for (int i = 0; i < route_num_points; i++)
		{
			group[i] = -1;
		}
		for (int i = 0; i < route_num_points; i++)
		{
			if (group[i] >= 0)
			{
				continue;
			}
			int stack[ROUTE_MAX_POINTS];
			int top = 0;
			stack[top++] = i;
			group[i] = groups;
			while (top)
			{
				const int p = stack[--top];
				for (int j = 0; j < route_num_points; j++)
				{
					if (route_links[p][j] && group[j] < 0)
					{
						group[j] = groups;
						stack[top++] = j;
					}
				}
			}
			groups++;
		}
		if (groups > 1)
		{
			for (int g = 0; g < groups; g++)
			{
				int count = 0;
				int first = -1;
				for (int i = 0; i < route_num_points; i++)
				{
					if (group[i] == g)
					{
						count++;
						first = first < 0 ? i : first;
					}
				}
				gi.Printf(S_COLOR_YELLOW "  part %d: %d points (point %d...)\n", g, count, first);
			}
		}
		gi.Printf("Ship route: %d points, %d links, %d part%s, %d problem%s\n", route_num_points, links / 2, groups,
			groups == 1 ? "" : "s", problems, problems == 1 ? "" : "s");
	}
	else if (!Q_stricmp(cmd, "ship_wp_clear"))
	{
		route_num_points = 0;
		route_links_dirty = qtrue;
		Route_ResetShips();
		gi.Printf("Ship route: all points removed (ship_wp_load gets the saved ones back)\n");
	}
	else
	{
		gi.Printf("ship_wp_add, ship_wp_rem, ship_wp_show, ship_wp_info, ship_wp_save, ship_wp_load, ship_wp_clear\n");
	}
	return qtrue;
}

// map start (and a save game loaded): the map's route
void G_FighterRoute_Load()
{
	route_show = qfalse;
	Route_Load(qfalse);
}

// draws the route while it is shown: the points as green posts (the one ship_wp_rem would remove yellow), the links
// as blue lines, near the player
extern void CG_TestLine(vec3_t start, vec3_t end, int time, unsigned int color, int radius);

void G_FighterRoute_Frame()
{
	if (!route_show || !player || !player->client || route_show_time > level.time)
	{
		return;
	}
	constexpr int redraw = 500;
	route_show_time = level.time + redraw;
	Route_BuildLinks();

	vec3_t pos;
	Route_PlayerPos(player, pos);
	const int nearest = Route_Nearest(pos, qfalse);

	for (int i = 0; i < route_num_points; i++)
	{
		if (Distance(pos, route_points[i]) > ROUTE_SHOW_DIST)
		{
			continue;
		}
		vec3_t bottom, top;
		VectorCopy(route_points[i], bottom);
		VectorCopy(route_points[i], top);
		bottom[2] -= 200.0f;
		top[2] += 200.0f;
		CG_TestLine(bottom, top, redraw + 100, i == nearest ? 0x00ffff : 0x00ff00, 64);

		for (int j = i + 1; j < route_num_points; j++)
		{
			if (route_links[i][j])
			{
				CG_TestLine(route_points[i], route_points[j], redraw + 100, 0xff8000, 16);
			}
		}
	}
}

// a ship without an enemy on a route: pos3 becomes the point it flies to. qfalse: no route here.
static qboolean Fighter_RouteGoal(const vec3_t my_pos, const vec3_t fwd)
{
	if (route_num_points <= 0)
	{
		return qfalse;
	}
	Route_BuildLinks();

	const int n = NPC->s.number;
	int cur = route_cur[n];
	if (cur < 0 || cur >= route_num_points || !VectorCompare(NPC->pos3, route_points[cur]))
	{
		// (back) onto the route: the nearest point it can see
		cur = Route_Nearest(my_pos, qtrue);
		if (cur < 0)
		{
			cur = Route_Nearest(my_pos, qfalse);
		}
		route_prev[n] = -1;
	}
	else
	{
		vec3_t to_point;
		VectorSubtract(route_points[cur], my_pos, to_point);
		const float dist = VectorNormalize(to_point);
		if (dist < ROUTE_REACHED || (dist < ROUTE_REACHED * 2.5f && DotProduct(to_point, fwd) < 0.0f))
		{
			// there (or flown past it): on to a linked one, liking the ones straight on, not back unless it must
			vec3_t heading;
			if (route_prev[n] >= 0 && route_prev[n] < route_num_points)
			{
				VectorSubtract(route_points[cur], route_points[route_prev[n]], heading);
				VectorNormalize(heading);
			}
			else
			{
				VectorCopy(fwd, heading);
			}
			int next = -1;
			float best_score = -1.0e30f;
			for (int j = 0; j < route_num_points; j++)
			{
				if (!route_links[cur][j])
				{
					continue;
				}
				vec3_t dir;
				VectorSubtract(route_points[j], route_points[cur], dir);
				VectorNormalize(dir);
				float score = DotProduct(dir, heading) + Q_flrand(0.0f, 1.2f);
				if (j == route_prev[n])
				{
					score -= 10.0f; // a dead end only
				}
				if (score > best_score)
				{
					best_score = score;
					next = j;
				}
			}
			if (next >= 0)
			{
				if (g_developer && g_developer->integer > 1)
				{
					gi.Printf("fighter AI: %s %d at route point %d, on to %d\n", NPC->NPC_type, n, cur, next);
				}
				route_prev[n] = cur;
				cur = next;
			}
		}
	}
	route_cur[n] = cur;
	VectorCopy(route_points[cur], NPC->pos3);
	return qtrue;
}

/*
-------------------------
NPC_FighterAI

An NPC flying a fighter: fills in its usercmd. qfalse: not flying one (normal behaviour).
-------------------------
*/
qboolean NPC_FighterAI()
{
	if (!NPC->classname || Q_stricmp(NPC->classname, FIGHTER_PILOT_CLASSNAME))
	{
		return qfalse; // only the pilots G_FighterAI_VehicleSpawned put in (scripted ones are left alone)
	}
	Vehicle_t* p_veh = G_IsRidingVehicle(NPC);
	if (!p_veh || !p_veh->m_pVehicleInfo || p_veh->m_pVehicleInfo->type != VH_FIGHTER)
	{
		if (NPC->health > 0)
		{
			// a pilot without his ship (thrown out of it): he goes
			NPC->s.eFlags |= EF_NODRAW;
			NPC->e_ThinkFunc = thinkF_G_FreeEntity;
			NPC->nextthink = level.time + FRAMETIME;
			return qtrue;
		}
		return qfalse;
	}
	gentity_t* ship = p_veh->m_pParentEntity;
	Fighter_RegisterCvars();

	ucmd.forwardmove = ucmd.rightmove = ucmd.upmove = 0;
	ucmd.buttons = 0;
	if (ship->health <= 0)
	{
		return qtrue;
	}

	const int skill = Fighter_Skill();
	const float turn_rate = 60.0f + 20.0f * static_cast<float>(skill); // degrees per second
	const float max_step = turn_rate * FIGHTER_THINK_SECONDS;
	const float aim_error = 3.0f - 0.8f * static_cast<float>(skill); // degrees
	const team_t my_team = NPC->client->playerTeam;

	vec3_t fwd, my_pos;
	AngleVectors(p_veh->m_vOrientation, fwd, nullptr, nullptr);
	VectorCopy(ship->currentOrigin, my_pos);
	const float speed = VectorLength(ship->client->ps.velocity);

	// out of the hangar first: straight on, full throttle (taking off if it stands on the floor)
	if (!TIMER_Done(NPC, "fighterLaunch"))
	{
		if (p_veh->m_iDropTime < level.time) // not still dropping clear of its rack
		{
			ucmd.forwardmove = 127;
			if (ship->client->ps.groundEntityNum != ENTITYNUM_NONE)
			{
				ucmd.upmove = 127; // standing on the hangar floor: take off (in the air upmove is turbo)
			}
		}
		Fighter_TurnTowards(fwd, max_step);
		VectorCopy(my_pos, NPC->pos4); // home is where it is out of the hangar, not in it
		VectorCopy(my_pos, NPC->pos3);
		return qtrue;
	}

	// a map edge (trigger_shipboundary) is turning the ship around: let it
	if (ship->client->ps.vehTurnaroundTime > level.time)
	{
		ucmd.forwardmove = 127;
		return qtrue;
	}

	// the enemy
	if (NPC->enemy && (!Fighter_IsFighter(NPC->enemy) || NPC->enemy->health <= 0
		|| Fighter_ShipTeam(NPC->enemy) == TEAM_FREE || Fighter_ShipTeam(NPC->enemy) == my_team))
	{
		NPC->enemy = nullptr;
	}
	if (TIMER_Done(NPC, "fighterRetarget"))
	{
		gentity_t* target = Fighter_FindTarget(ship, my_team, fwd);
		if (target != NPC->enemy)
		{
			NPC->enemy = target;
			TIMER_Set(NPC, "fighterReact", 900 - 200 * skill); // a moment to react before shooting
		}
		TIMER_Set(NPC, "fighterRetarget", Q_irand(800, 1500));
	}
	gentity_t* enemy = NPC->enemy;

	vec3_t want_dir;
	VectorCopy(fwd, want_dir);
	qboolean may_fire = qfalse;
	vec3_t aim_point;
	float enemy_dist = 0.0f;

	if (!TIMER_Done(NPC, "fighterEvade"))
	{
		// breaking away
		VectorSubtract(NPC->pos3, my_pos, want_dir);
		ucmd.forwardmove = 127;
	}
	else if (enemy)
	{
		// lead it: where it will be when a shot gets there
		const int weap = p_veh->m_pVehicleInfo->weapon[0].ID;
		const float shot_speed = weap > VEH_WEAPON_NONE && g_vehWeaponInfo[weap].fSpeed > 0.0f
			? g_vehWeaponInfo[weap].fSpeed
			: 6000.0f;
		enemy_dist = Distance(my_pos, enemy->currentOrigin);
		VectorMA(enemy->currentOrigin, enemy_dist / shot_speed, enemy->client->ps.velocity, aim_point);
		VectorSubtract(aim_point, my_pos, want_dir);

		// a little off, more so for a poorer pilot
		vec3_t want_angles;
		vectoangles(want_dir, want_angles);
		want_angles[PITCH] += Q_flrand(-aim_error, aim_error);
		want_angles[YAW] += Q_flrand(-aim_error, aim_error);
		const float len = VectorLength(want_dir);
		AngleVectors(want_angles, want_dir, nullptr, nullptr);
		VectorScale(want_dir, len, want_dir);

		vec3_t to_enemy;
		VectorSubtract(enemy->currentOrigin, my_pos, to_enemy);
		VectorNormalize(to_enemy);
		const float facing = DotProduct(fwd, to_enemy);

		if (enemy_dist < FIGHTER_BREAK_DIST && facing > 0.0f)
		{
			// about to fly into it: break away, up or down and to a side
			vec3_t right, up;
			AngleVectors(p_veh->m_vOrientation, nullptr, right, up);
			VectorMA(my_pos, 2500.0f, fwd, NPC->pos3);
			VectorMA(NPC->pos3, Q_irand(0, 1) ? 2000.0f : -2000.0f, right, NPC->pos3);
			VectorMA(NPC->pos3, Q_irand(0, 1) ? 1500.0f : -1500.0f, up, NPC->pos3);
			TIMER_Set(NPC, "fighterEvade", Q_irand(1500, 2500));
		}

		// throttle: catch up, don't overshoot
		vec3_t rel_vel;
		VectorSubtract(ship->client->ps.velocity, enemy->client->ps.velocity, rel_vel);
		const float closing = DotProduct(rel_vel, to_enemy);
		if (enemy_dist < 1500.0f && closing > 400.0f && facing > 0.7f)
		{
			ucmd.forwardmove = -127;
		}
		else
		{
			ucmd.forwardmove = 127;
		}
		if (enemy_dist > 6000.0f && facing > 0.85f && TIMER_Done(NPC, "fighterTurbo"))
		{
			ucmd.upmove = 127; // turbo
			TIMER_Set(NPC, "fighterTurbo", Q_irand(6000, 12000));
		}
		may_fire = TIMER_Done(NPC, "fighterReact");
	}
	else
	{
		const gentity_t* player_ship = player && player->client && player->client->ps.m_iVehicleNum
			? &g_entities[player->client->ps.m_iVehicleNum]
			: nullptr;
		if (my_team == TEAM_PLAYER && Fighter_IsFighter(player_ship))
		{
			// a wingman flies with the player: behind him and to a side (one side each by number)
			vec3_t p_fwd, p_right;
			AngleVectors(player_ship->m_pVehicle->m_vOrientation, p_fwd, p_right, nullptr);
			const float side = NPC->s.number % 2 ? 500.0f : -500.0f;
			VectorMA(player_ship->currentOrigin, -400.0f - 150.0f * static_cast<float>(NPC->s.number % 3), p_fwd,
				NPC->pos3);
			VectorMA(NPC->pos3, side, p_right, NPC->pos3);
			VectorSubtract(NPC->pos3, my_pos, want_dir);
			ucmd.forwardmove = VectorLength(want_dir) > 700.0f ? 127 : 0;
		}
		else
		{
			// along the map's route, or (none) patrol around where it started
			if (!Fighter_RouteGoal(my_pos, fwd)
				&& (TIMER_Done(NPC, "fighterPatrol") || Distance(my_pos, NPC->pos3) < 800.0f))
			{
				// a point around home it can fly straight to (not inside the Star Destroyer or an asteroid)
				for (int tries = 0; tries < 6; tries++)
				{
					vec3_t point;
					for (int axis = 0; axis < 3; axis++)
					{
						point[axis] = NPC->pos4[axis] + Q_flrand(-5000.0f, 5000.0f);
					}
					trace_t tr;
					gi.trace(&tr, my_pos, ship->mins, ship->maxs, point, ship->s.number, MASK_SOLID,
						static_cast<EG2_Collision>(0), 0);
					if (!tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f)
					{
						VectorCopy(point, NPC->pos3);
						break;
					}
				}
				TIMER_Set(NPC, "fighterPatrol", Q_irand(8000, 15000));
			}
			VectorSubtract(NPC->pos3, my_pos, want_dir);
			ucmd.forwardmove = 100;
		}
	}

	// something in the way ahead (the Star Destroyer, an asteroid, a wall): turn away from it, harder and braking when
	// it is near. It looks about two seconds ahead (they fly at thousands of units a second), with a box near the
	// ship's own size.
	float turn_step = max_step;
	{
		const float half = Com_Clamp(24.0f, 96.0f, ship->maxs[0] * 0.6f);
		const vec3_t look_mins = { -half, -half, -half };
		const vec3_t look_maxs = { half, half, half };
		const float look = Com_Clamp(1200.0f, 9000.0f, speed * 2.0f);
		vec3_t end;
		trace_t tr;
		VectorMA(my_pos, look, fwd, end);
		gi.trace(&tr, my_pos, look_mins, look_maxs, end, ship->s.number, MASK_SOLID | CONTENTS_BODY,
			static_cast<EG2_Collision>(0), 0); // other ships too
		if (!tr.startsolid && tr.fraction < 1.0f && (!enemy || tr.entityNum != enemy->s.number))
		{
			const float dist_ahead = tr.fraction * look;
			vec3_t away;
			VectorScale(fwd, 0.2f, away);
			VectorMA(away, 1.0f, tr.plane.normal, away);
			VectorCopy(away, want_dir);
			may_fire = qfalse;
			ucmd.upmove = 0; // no turbo into it
			if (dist_ahead < speed * 1.0f || dist_ahead < 900.0f)
			{
				ucmd.forwardmove = -127;
				turn_step = max_step * 2.0f; // pull hard
			}
		}
	}

	if (VectorNormalize(want_dir) > 0.0f)
	{
		Fighter_TurnTowards(want_dir, turn_step);
	}

	// shoot when lined up with the aim point
	if (may_fire && enemy && enemy_dist < FIGHTER_FIRE_RANGE)
	{
		vec3_t aim_dir;
		VectorSubtract(aim_point, my_pos, aim_dir);
		VectorNormalize(aim_dir);
		const float cos_cone = cos(DEG2RAD(FIGHTER_FIRE_CONE));
		if (DotProduct(fwd, aim_dir) > cos_cone)
		{
			vec3_t start;
			VectorMA(my_pos, 150.0f, fwd, start);
			if (!Fighter_FriendInLine(ship, start, enemy->currentOrigin, my_team))
			{
				ucmd.buttons |= BUTTON_ATTACK;
				// now and then its other weapon (missiles, torpedoes) from a fair distance
				if (p_veh->m_pVehicleInfo->weapon[1].ID > VEH_WEAPON_NONE && p_veh->weaponStatus[1].ammo > 0
					&& enemy_dist > 1200.0f && enemy_dist < 6000.0f && TIMER_Done(NPC, "fighterAlt"))
				{
					ucmd.buttons |= BUTTON_ALT_ATTACK;
					TIMER_Set(NPC, "fighterAlt", Q_irand(6000, 12000));
				}
			}
		}
	}

	return qtrue;
}
