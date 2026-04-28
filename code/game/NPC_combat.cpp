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

//NPC_combat.cpp

#include "b_local.h"
#include "g_nav.h"
#include "g_navigator.h"
#include "wp_saber.h"
#include "g_functions.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <map>
#include "ai.h"
#include "anims.h"
#include "bg_public.h"
#include "bstate.h"
#include "b_public.h"
#include "ghoul2_shared.h"
#include "g_local.h"
#include "g_public.h"
#include "g_shared.h"
#include "teams.h"
#include "weapons.h"
#include <qcommon/q_shared.h>
#include <qcommon/q_color.h>
#include <qcommon/q_math.h>
#include <qcommon/q_platform.h>
#include <qcommon/q_string.h>

extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
extern qboolean NPC_CheckLookTarget(const gentity_t* self);
extern void NPC_ClearLookTarget(const gentity_t* self);
extern void NPC_Jedi_RateNewEnemy(const gentity_t* self, const gentity_t* enemy);
extern qboolean PM_DroidMelee(int npc_class);
extern int delayedShutDown;
extern qboolean G_ValidEnemy(const gentity_t* self, const gentity_t* enemy);
extern qboolean PM_ReloadAnim(int anim);
extern qboolean PM_WeponRestAnim(int anim);
extern cvar_t* com_kotor;
extern cvar_t* g_ffamode;

void ChangeWeapon(const gentity_t* ent, int new_weapon);

void G_ClearEnemy(gentity_t* self)
{
	if (!self)
	{
		return;
	}

	// Update/look target logic first
	NPC_CheckLookTarget(self);

	if (self->enemy)
	{
		// If the enemy is still valid AND this NPC is locked onto them, do not clear
		if (G_ValidEnemy(self, self->enemy) &&
			(self->svFlags & SVF_LOCKEDENEMY))
		{
			return;
		}

		// Clear lookTarget if it was pointing at the enemy
		if (self->client &&
			self->client->renderInfo.lookTarget == self->enemy->s.number)
		{
			NPC_ClearLookTarget(self);
		}

		// If the enemy was also the goalEntity, clear that too
		if (self->NPC && self->enemy == self->NPC->goalEntity)
		{
			self->NPC->goalEntity = nullptr;
		}

		// (FIXME in original: could store lastEnemy here)
	}

	// Finally clear the enemy pointer
	self->enemy = nullptr;
}

constexpr auto ANGER_ALERT_RADIUS = 512;
constexpr auto ANGER_ALERT_SOUND_RADIUS = 256;

void G_AngerAlert(const gentity_t* self)
{
	if (!self)
	{
		return;
	}

	// NPCs flagged as NO_GROUPS never alert others
	if (self->NPC && (self->NPC->scriptFlags & SCF_NO_GROUPS))
	{
		return;
	}

	// Interrogating NPCs do not alert others yet
	if (!TIMER_Done(self, "interrogating"))
	{
		return;
	}

	// Only alert if we actually have an enemy
	if (!self->enemy)
	{
		return;
	}

	// Broadcast alert to nearby allies
	G_AlertTeam(self, self->enemy, ANGER_ALERT_RADIUS, ANGER_ALERT_SOUND_RADIUS);
}

/*
-------------------------
G_TeamEnemy
-------------------------
*/

qboolean G_TeamEnemy(const gentity_t* self)
{
	if (!self || !self->client)
	{
		return qfalse;
	}

	// TEAM_FREE never has teammates
	if (self->client->playerTeam == TEAM_FREE)
	{
		return qfalse;
	}

	// NPCs flagged as NO_GROUPS never consider teammates
	if (self->NPC && (self->NPC->scriptFlags & SCF_NO_GROUPS))
	{
		return qfalse;
	}

	// Scan all entities for teammates who already have an enemy
	for (int i = 1; i < MAX_GENTITIES; i++)
	{
		const gentity_t* ent = &g_entities[i];

		if (!ent || ent == self)
		{
			continue;
		}

		if (!ent->inuse || ent->health <= 0)
		{
			continue;
		}

		if (!ent->client)
		{
			continue;
		}

		// Must be on the same team
		if (ent->client->playerTeam != self->client->playerTeam)
		{
			continue;
		}

		// Teammate has an enemy?
		if (ent->enemy)
		{
			const gentity_t* eEnemy = ent->enemy;

			// If the enemy is not on our team, then our team has an enemy
			if (!eEnemy->client ||
				eEnemy->client->playerTeam != self->client->playerTeam)
			{
				return qtrue;
			}
		}
	}

	return qfalse;
}

static qboolean G_CheckSaberAllyAttackDelay(const gentity_t* self, const gentity_t* enemy)
{
	if (!self || !self->enemy)
	{
		return qfalse;
	}
	if (self->NPC
		&& self->client->leader == player
		&& self->enemy
		&& self->enemy->s.weapon != WP_SABER
		&& self->s.weapon == WP_SABER)
	{
		//assisting the player and I'm using a saber and my enemy is not
		TIMER_Set(self, "allyJediDelay", -level.time);
		//use the distance to the enemy to determine how long to delay
		const float distance = Distance(enemy->currentOrigin, self->currentOrigin);
		if (distance < 256)
		{
			return qtrue;
		}
		int delay;
		if (distance > 2048)
		{
			//the farther they are, the shorter the delay
			delay = 5000 - floor(distance); //(6-g_spskill->integer));
			if (delay < 500)
			{
				delay = 500;
			}
		}
		else
		{
			//the close they are, the shorter the delay
			delay = floor(distance * 4); //(6-g_spskill->integer));
			if (delay > 5000)
			{
				delay = 5000;
			}
		}
		TIMER_Set(self, "allyJediDelay", delay);

		return qtrue;
	}
	return qfalse;
}

static void G_AttackDelay(const gentity_t* self, const gentity_t* enemy)
{
	if (enemy && self->client && self->NPC)
	{
		//delay their attack based on how far away they're facing from enemy
		vec3_t fwd, dir;

		VectorSubtract(self->client->renderInfo.eyePoint, enemy->currentOrigin, dir); //purposely backwards
		VectorNormalize(dir);
		AngleVectors(self->client->renderInfo.eyeAngles, fwd, nullptr, nullptr);
		//dir[2] = fwd[2] = 0;//ignore z diff?

		int att_delay = (4 - g_spskill->integer) * 500; //initial: from 1000ms delay on hard to 2000ms delay on easy
		if (self->client->playerTeam == TEAM_PLAYER)
		{
			//invert
			att_delay = 2000 - att_delay;
		}
		att_delay += floor((DotProduct(fwd, dir) + 1.0f) * 2000.0f); //add up to 4000ms delay if they're facing away

		//FIXME: should distance matter, too?

		//Now modify the delay based on NPC_class, weapon, and team
		//NOTE: attDelay should be somewhere between 1000 to 6000 milliseconds
		switch (self->client->NPC_class)
		{
		case CLASS_IMPERIAL: //they give orders and hang back
			att_delay += Q_irand(500, 1500);
			break;
		case CLASS_CLONETROOPER:
		case CLASS_SBD:
		case CLASS_BATTLEDROID:
		case CLASS_STORMTROOPER: //stormtroopers shoot sooner
			if (self->NPC->rank >= RANK_LT)
			{
				//officers shoot even sooner
				att_delay -= Q_irand(500, 1500);
			}
			else
			{
				//normal stormtroopers don't have as fast reflexes as officers
				att_delay -= Q_irand(0, 1000);
			}
			break;
		case CLASS_SWAMPTROOPER: //shoot very quickly?  What about guys in water?
			att_delay -= Q_irand(1000, 2000);
			break;
		case CLASS_IMPWORKER: //they panic, don't fire right away
			att_delay += Q_irand(1000, 2500);
			break;
		case CLASS_TRANDOSHAN:
			att_delay -= Q_irand(500, 1500);
			break;
		case CLASS_JAN:
		case CLASS_LANDO:
		case CLASS_PRISONER:
		case CLASS_REBEL:
			att_delay -= Q_irand(500, 1500);
			break;
		case CLASS_GALAKMECH:
		case CLASS_ATST:
			att_delay -= Q_irand(1000, 2000);
			break;
		case CLASS_REELO:
		case CLASS_UGNAUGHT:
		case CLASS_JAWA:
			return;
		case CLASS_MINEMONSTER:
		case CLASS_MURJJ:
			return;
		case CLASS_INTERROGATOR:
		case CLASS_PROBE:
		case CLASS_MARK1:
		case CLASS_MARK2:
		case CLASS_SENTRY:
			return;
		case CLASS_REMOTE:
		case CLASS_SEEKER:
			return;
		default:
			break;
		}

		switch (self->s.weapon)
		{
		case WP_NONE:
		case WP_SABER:
			return;
		case WP_BRYAR_PISTOL:
		case WP_SBD_BLASTER:
		case WP_JAWA:
			break;
		case WP_BLASTER:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_BOWCASTER:
			att_delay += Q_irand(0, 500);
			break;
		case WP_REPEATER:
			if (!(self->NPC->scriptFlags & SCF_ALT_FIRE))
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			break;
		case WP_FLECHETTE:
			att_delay += Q_irand(500, 1500);
			break;
		case WP_ROCKET_LAUNCHER:
			att_delay += Q_irand(500, 1500);
			break;
		case WP_CONCUSSION:
			att_delay += Q_irand(500, 1500);
			break;
		case WP_BLASTER_PISTOL: // apparently some enemy only version of the blaster
			att_delay += Q_irand(500, 1500);
			break;
		case WP_DISRUPTOR: //sniper's don't delay?
			return;
		case WP_THERMAL: //grenade-throwing has a built-in delay
			return;
		case WP_MELEE: // Any ol' melee attack
			return;
		case WP_EMPLACED_GUN:
			return;
		case WP_TURRET: // turret guns
			return;
		case WP_BOT_LASER: // Probe droid	- Laser blast
			return;
		case WP_NOGHRI_STICK:
			att_delay += Q_irand(0, 500);
			break;
		case WP_BATTLEDROID:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_THEFIRSTORDER:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_CLONECARBINE:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_REBELBLASTER:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_CLONERIFLE:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_CLONECOMMANDO:
		case WP_WRIST_BLASTER:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;

		case WP_Z6_ROTARY_CANNON:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;

		case WP_REBELRIFLE:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;

		case WP_BOBA:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_CLONEPISTOL:
		case WP_DUAL_CLONEPISTOL:
		case WP_DUAL_PISTOL:
			att_delay -= Q_irand(500, 1500);
			break;

		case WP_REY:
			att_delay -= Q_irand(500, 1500);
			break;

		case WP_JANGO:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(0, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(0, 500);
			}
			break;
		case WP_DROIDEKA:
			if (self->NPC->scriptFlags & SCF_ALT_FIRE)
			{
				//rapid-fire blasters
				att_delay += Q_irand(250, 500);
			}
			else
			{
				//regular blaster
				att_delay -= Q_irand(250, 500);
			}
			break;
		default:;
		}

		if (self->client->playerTeam == TEAM_PLAYER)
		{
			//clamp it
			if (att_delay > 2000)
			{
				att_delay = 2000;
			}
		}

		//don't shoot right away
		if (att_delay > 4000 + (2 - g_spskill->integer) * 3000)
		{
			att_delay = 4000 + (2 - g_spskill->integer) * 3000;
		}
		TIMER_Set(self, "attackDelay", att_delay); //Q_irand( 1500, 4500 ) );
		//don't move right away either
		if (att_delay > 4000)
		{
			att_delay = 4000 - Q_irand(500, 1500);
		}
		else
		{
			att_delay -= Q_irand(500, 1500);
		}

		TIMER_Set(self, "roamTime", att_delay); //was Q_irand( 1000, 3500 );
	}
}

/*
-------------------------
G_SetEnemy
-------------------------
*/
extern gentity_t* G_CheckControlledTurretEnemy(const gentity_t* self, gentity_t* enemy, qboolean validate);

void Saboteur_Cloak(gentity_t* self);
void G_AimSet(const gentity_t* self, int aim);

void G_SetEnemy(gentity_t* self, gentity_t* enemy)
{
	int event = 0;

	if (!self || !enemy)
	{
		return;
	}

	// Must be in use
	if (!enemy->inuse)
	{
		return;
	}

	enemy = G_CheckControlledTurretEnemy(self, enemy, qtrue);
	if (!enemy)
	{
		return;
	}

	// Don't take the enemy if in notarget
	if (enemy->flags & FL_NOTARGET)
	{
		return;
	}

	// Non‑NPCs just store enemy and bail
	if (!self->NPC)
	{
		self->enemy = enemy;
		return;
	}

	// NPC with no client: just store enemy and bail (prevents NULL deref)
	if (!self->client)
	{
		self->enemy = enemy;
		return;
	}

	// Can't pick up enemies if confused
	if (self->NPC->confusionTime > level.time)
	{
		return;
	}

#ifdef _DEBUG
	if (self->s.number && enemy == self)
	{
		gi.Printf("G_SetEnemy: entity %d tried to set itself as enemy\n", self->s.number);
		return;
	}
#endif

	// Prevent friendly targeting unless charmed or special cases
	if (enemy->client &&
		enemy->client->playerTeam == self->client->playerTeam)
	{
		// Probably a script; ignore if charmed
		if (self->NPC->charmedTime > level.time)
		{
			return;
		}

		// Fix TEAM_ENEMY NPCs trying to target the player who is on their team
		if (self->client->playerTeam != TEAM_FREE &&
			self->client->playerTeam != TEAM_SOLO)
		{
			return;
		}
	}

	// FFA mode hook (left as-is, behaviour unchanged)
	if (g_ffamode->integer)
	{
		// Intentionally left as a hook for script/cvar logic
	}

	// Jedi aggression setup when getting a new enemy
	if (self->client->ps.weapon == WP_SABER)
	{
		NPC_Jedi_RateNewEnemy(self, enemy);
	}

	// First time acquiring an enemy
	if (!self->enemy)
	{
		// TEMP HACK: turn on our saber
		if (self->health > 0)
		{
			self->client->ps.SaberActivate();
		}

		// Prevent alert cascading
		G_ClearEnemy(self);
		self->enemy = enemy;

		// Saboteur: cloak and delay decloak
		if (self->client->NPC_class == CLASS_SABOTEUR)
		{
			Saboteur_Cloak(self);
			TIMER_Set(self, "decloakwait", 3000);
		}

		// Special case – player hunted by own people (JKA version)
		if (self->client->playerTeam == TEAM_PLAYER &&
			enemy->s.number == 0 &&
			enemy->client &&
			enemy->client->ps.weapon != WP_TURRET &&
			enemy->client->playerTeam == TEAM_PLAYER)
		{
			// Make the player "evil" so that everyone goes after him
			enemy->client->enemyTeam = TEAM_FREE;
			enemy->client->playerTeam = TEAM_FREE;
		}

		// Anger script or voice events
		if (!G_ActivateBehavior(self, BSET_ANGER))
		{
			if (self->client->NPC_class == CLASS_KYLE &&
				self->client->leader == player &&
				!TIMER_Done(self, "kyleAngerSoundDebounce"))
			{
				// Don't yell more than once every few seconds
			}
			else if (enemy->client &&
				self->client->playerTeam != enemy->client->playerTeam)
			{
				if (self->forcePushTime < level.time) // not currently being pushed
				{
					if (!G_TeamEnemy(self) &&
						self->client->NPC_class != CLASS_BOBAFETT &&
						self->client->NPC_class != CLASS_MANDALORIAN &&
						self->client->NPC_class != CLASS_JANGO &&
						self->client->NPC_class != CLASS_JANGODUAL)
					{
						// Team did not have an enemy previously
						if (self->NPC &&
							self->client->playerTeam == TEAM_PLAYER &&
							enemy->s.number < MAX_CLIENTS &&
							self->client->clientInfo.customBasicSoundDir &&
							self->client->clientInfo.customBasicSoundDir[0] &&
							Q_stricmp("jedi2", self->client->clientInfo.customBasicSoundDir) == 0)
						{
							switch (Q_irand(0, 2))
							{
							case 0:
								G_SoundOnEnt(self, CHAN_VOICE, "sound/chars/jedi2/28je2008.wav");
								break;
							case 1:
								G_SoundOnEnt(self, CHAN_VOICE, "sound/chars/jedi2/28je2009.wav");
								break;
							case 2:
								G_SoundOnEnt(self, CHAN_VOICE, "sound/chars/jedi2/28je2012.wav");
								break;
							default:
								break;
							}
							self->NPC->blockedSpeechDebounceTime = level.time + 2000;
						}
						else
						{
							if (Q_irand(0, 1))
							{
								event = Q_irand(EV_CHASE1, EV_CHASE3);
							}
							else
							{
								event = Q_irand(EV_ANGER1, EV_ANGER3);
							}
						}
					}
				}

				if (event)
				{
					if (self->client->NPC_class == CLASS_KYLE &&
						self->client->leader == player)
					{
						TIMER_Set(self, "kyleAngerSoundDebounce", Q_irand(4000, 8000));
					}
					G_AddVoiceEvent(self, event, Q_irand(10000, 13000));
				}
			}
		}

		// Initial aim error when first getting mad with ranged weapons
		if (self->s.weapon == WP_BLASTER ||
			self->s.weapon == WP_REPEATER ||
			self->s.weapon == WP_THERMAL ||
			self->s.weapon == WP_BLASTER_PISTOL ||
			self->s.weapon == WP_DUAL_PISTOL ||
			self->s.weapon == WP_DUAL_CLONEPISTOL ||
			self->s.weapon == WP_DROIDEKA ||
			self->s.weapon == WP_BOWCASTER ||
			self->s.weapon == WP_BATTLEDROID ||
			self->s.weapon == WP_CLONECARBINE ||
			self->s.weapon == WP_REBELBLASTER ||
			self->s.weapon == WP_CLONERIFLE ||
			self->s.weapon == WP_CLONECOMMANDO ||
			self->s.weapon == WP_Z6_ROTARY_CANNON ||
			self->s.weapon == WP_REBELRIFLE ||
			self->s.weapon == WP_BOBA)
		{
			if (self->client->playerTeam == TEAM_PLAYER)
			{
				G_AimSet(self,
					Q_irand(self->NPC->stats.aim - 5 * g_spskill->integer,
						self->NPC->stats.aim - g_spskill->integer));
			}
			else
			{
				int min_err = 2;
				int max_err = 6;

				if (self->client->NPC_class == CLASS_IMPWORKER)
				{
					min_err = 5;
					max_err = 10;
				}
				else if (self->client->NPC_class == CLASS_STORMTROOPER &&
					self->NPC->rank <= RANK_CREWMAN)
				{
					min_err = 3;
					max_err = 9;
				}

				G_AimSet(self,
					Q_irand(self->NPC->stats.aim - max_err * (3 - g_spskill->integer),
						self->NPC->stats.aim - min_err * (3 - g_spskill->integer)));
			}
		}

		// Alert anyone else in the area (except special holodeck enemies)
		if (Q_stricmp("STCommander", self->NPC_type) != 0 &&
			Q_stricmp("ImpCommander", self->NPC_type) != 0)
		{
			if (self->client &&                      
				!(self->client->ps.eFlags & EF_FORCE_GRIPPED) &&
				!(self->client->ps.eFlags & EF_FORCE_GRASPED))
			{
				G_AngerAlert(self);
			}
		}


		// Saber allies may hold back; others get an attack delay
		if (!G_CheckSaberAllyAttackDelay(self, enemy))
		{
			G_AttackDelay(self, enemy);
		}

		// Imperial holster hack: auto‑draw a blaster‑type weapon
		if (self->client->ps.weapon == WP_NONE &&
			!Q_stricmpn(self->NPC_type, "imp", 3) &&
			!(self->NPC->scriptFlags & SCF_FORCED_MARCH))
		{
			const int weapList[] = {
				WP_BLASTER,
				WP_BLASTER_PISTOL,
				WP_DUAL_PISTOL,
				WP_DUAL_CLONEPISTOL,
				WP_DROIDEKA
			};

			for (int i = 0; i < static_cast<int>(ARRAY_LEN(weapList)); ++i)
			{
				const int w = weapList[i];
				if (!self->client->ps.weapons[w])
				{
					continue;
				}

				ChangeWeapon(self, w);
				self->client->ps.weapon = w;
				self->client->ps.weaponstate = WEAPON_READY;

				const weaponData_t& wd = weaponData[w];
				const char* model = nullptr;

				if (com_kotor->integer == 1 || self->client->charKOTORWeapons == 1)
				{
					model = wd.altweaponMdl;
				}
				else
				{
					model = wd.weaponMdl;
				}

				G_CreateG2AttachedWeaponModel(self, model, self->handRBolt, 0);
				break;
			}
		}

		return;
	}

	// Already had an enemy; just switching
	if (event)
	{
		G_AddVoiceEvent(self, event, 2000);
	}

	G_ClearEnemy(self);
	self->enemy = enemy;
}

void ChangeWeapon(const gentity_t* ent, const int new_weapon)
{
	if (!ent || !ent->client || !ent->NPC)
	{
		return;
	}

	if (PM_ReloadAnim(ent->client->ps.torsoAnim) ||
		PM_WeponRestAnim(ent->client->ps.torsoAnim))
	{
		return;
	}

	ent->client->ps.weapon = new_weapon;
	ent->NPC->shotTime = 0;
	ent->NPC->burstCount = 0;
	ent->NPC->attackHold = 0;
	ent->NPC->currentAmmo = ent->client->ps.ammo[weaponData[new_weapon].ammoIndex];

	switch (new_weapon)
	{
	case WP_BRYAR_PISTOL: //prifle
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 1000;
		break;

	case WP_BLASTER_PISTOL:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;

		if (ent->weaponModel[1] > 0)
		{
			//commando
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 4;
			ent->NPC->burstMean = 6;
			ent->NPC->burstMax = 8;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 600; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 400; //attack debounce
			else
				ent->NPC->burstSpacing = 250; //attack debounce
		}
		else if (ent->client->NPC_class == CLASS_SABOTEUR)
		{
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 900; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 600; //attack debounce
			else
				ent->NPC->burstSpacing = 400; //attack debounce
		}
		else
		{
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_BOT_LASER: //probe attack
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 600; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 400; //attack debounce
		else
			ent->NPC->burstSpacing = 200; //attack debounce
		break;

	case WP_SABER:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 0; //attackdebounce
		break;

	case WP_DISRUPTOR:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			switch (g_spskill->integer)
			{
			case 0:
				ent->NPC->burstSpacing = 2500; //attackdebounce
				break;
			case 1:
				ent->NPC->burstSpacing = 2000; //attackdebounce
				break;
			case 2:
				ent->NPC->burstSpacing = 1500; //attackdebounce
				break;
			default:;
			}
		}
		else
		{
			ent->NPC->burstSpacing = 1000; //attackdebounce
		}
		break;

	case WP_TUSKEN_RIFLE:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			switch (g_spskill->integer)
			{
			case 0:
				ent->NPC->burstSpacing = 2500; //attackdebounce
				break;
			case 1:
				ent->NPC->burstSpacing = 2000; //attackdebounce
				break;
			case 2:
				ent->NPC->burstSpacing = 1500; //attackdebounce
				break;
			default:;
			}
		}
		else
		{
			ent->NPC->burstSpacing = 1000; //attackdebounce
		}
		break;

	case WP_BOWCASTER:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 1000; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 750; //attack debounce
		else
			ent->NPC->burstSpacing = 600; //attack debounce
		break;

	case WP_REPEATER:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			ent->NPC->burstSpacing = 2000; //attackdebounce
		}
		else
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 6;
			ent->NPC->burstMax = 10;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_DEMP2:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 1000; //attackdebounce
		break;

	case WP_FLECHETTE:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->burstSpacing = 2000; //attackdebounce
		}
		else
		{
			ent->NPC->burstSpacing = 1000; //attackdebounce
		}
		break;

	case WP_ROCKET_LAUNCHER:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 2500; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 2000; //attack debounce
		else
			ent->NPC->burstSpacing = 1500; //attack debounce
		break;

	case WP_CONCUSSION:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			//beam
			ent->NPC->burstSpacing = 1200; //attackdebounce
		}
		else
		{
			//rocket
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 2300; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1800; //attack debounce
			else
				ent->NPC->burstSpacing = 1200; //attack debounce
		}
		break;

	case WP_THERMAL:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 4500; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 3000; //attack debounce
		else
			ent->NPC->burstSpacing = 2000; //attack debounce
		break;

	case WP_BLASTER:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_MELEE:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 1000; //attackdebounce
		break;

	case WP_TUSKEN_STAFF:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 2500; //attackdebounce
		break;

	case WP_ATST_MAIN:
	case WP_ATST_SIDE:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 1000; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 750; //attack debounce
		else
			ent->NPC->burstSpacing = 600; //attack debounce
		break;

	case WP_EMPLACED_GUN:
		//FIXME: give some designer-control over this?
		if (ent->client && ent->client->NPC_class == CLASS_REELO)
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			ent->NPC->burstSpacing = 1000;
		}
		else
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 2; // 3 shots, really
			ent->NPC->burstMean = 2;
			ent->NPC->burstMax = 2;

			if (ent->owner)
				// if we have an owner, it should be the chair at this point...so query the chair for its shot debounce times, etc.
			{
				if (g_spskill->integer == 0)
				{
					ent->NPC->burstSpacing = ent->owner->wait + 400; //attack debounce
					ent->NPC->burstMin = ent->NPC->burstMax = 1; // two shots
				}
				else if (g_spskill->integer == 1)
				{
					ent->NPC->burstSpacing = ent->owner->wait + 200; //attack debounce
				}
				else
				{
					ent->NPC->burstSpacing = ent->owner->wait; //attack debounce
				}
			}
			else
			{
				if (g_spskill->integer == 0)
				{
					ent->NPC->burstSpacing = 1200; //attack debounce
					ent->NPC->burstMin = ent->NPC->burstMax = 1; // two shots
				}
				else if (g_spskill->integer == 1)
				{
					ent->NPC->burstSpacing = 1000; //attack debounce
				}
				else
				{
					ent->NPC->burstSpacing = 800; //attack debounce
				}
			}
		}
		break;

	case WP_NOGHRI_STICK:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 2250; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 1500; //attack debounce
		else
			ent->NPC->burstSpacing = 750; //attack debounce
		break;

	case WP_BATTLEDROID:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_THEFIRSTORDER:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_CLONECARBINE:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_REBELBLASTER:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_CLONERIFLE:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_CLONECOMMANDO:
	case WP_WRIST_BLASTER:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_Z6_ROTARY_CANNON:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->rotaryCannonShotsFired = 0;
		ent->NPC->rotaryCannonCooldownTime = 0;
		ent->NPC->burstSpacing = 200; //attack debounce
		break;

	case WP_REBELRIFLE:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_REY:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;

		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 1000; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 750; //attack debounce
		else
			ent->NPC->burstSpacing = 600; //attack debounce
		break;

	case WP_CLONEPISTOL:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;

		if (g_spskill->integer == 0)
			ent->NPC->burstSpacing = 1000; //attack debounce
		else if (g_spskill->integer == 1)
			ent->NPC->burstSpacing = 750; //attack debounce
		else
			ent->NPC->burstSpacing = 600; //attack debounce
		break;

	case WP_DUAL_CLONEPISTOL:
	case WP_DUAL_PISTOL:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 2;
			ent->NPC->burstMean = 2;
			ent->NPC->burstMax = 2;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1900; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1200; //attack debounce
			else
				ent->NPC->burstSpacing = 900; //attack debounce
		}
		else
		{
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;

	case WP_JANGO:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 2;
			ent->NPC->burstMean = 2;
			ent->NPC->burstMax = 2;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1900; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1200; //attack debounce
			else
				ent->NPC->burstSpacing = 900; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1150; //attack debounce
			else
				ent->NPC->burstSpacing = 800; //attack debounce
		}
		break;
	case WP_DROIDEKA:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 2;
			ent->NPC->burstMean = 2;
			ent->NPC->burstMax = 2;

			ent->NPC->burstSpacing = 1000; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;

			if (ent->weaponModel[1] > 0)
			{
				//commando
				ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
				ent->NPC->burstMin = 2;
				ent->NPC->burstMean = 2;
				ent->NPC->burstMax = 2;
				ent->NPC->burstSpacing = 600; //attack debounce
			}
			else
			{
				ent->NPC->burstSpacing = 750; //attack debounce
			}
		}
		break;

	case WP_BOBA:
		if (ent->NPC->scriptFlags & SCF_ALT_FIRE)
		{
			ent->NPC->aiFlags |= NPCAI_BURST_WEAPON;
			ent->NPC->burstMin = 3;
			ent->NPC->burstMean = 3;
			ent->NPC->burstMax = 3;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1500; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		else
		{
			ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
			if (g_spskill->integer == 0)
				ent->NPC->burstSpacing = 1000; //attack debounce
			else if (g_spskill->integer == 1)
				ent->NPC->burstSpacing = 750; //attack debounce
			else
				ent->NPC->burstSpacing = 600; //attack debounce
		}
		break;
	case WP_SBD_BLASTER:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 400;
		break;
	case WP_JAWA: //prifle
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		ent->NPC->burstSpacing = 1000;
		break;

	default:
		ent->NPC->aiFlags &= ~NPCAI_BURST_WEAPON;
		break;
	}
}

extern bool in_camera;

void NPC_ChangeWeapon(const int new_weapon)
{
	qboolean changing = qfalse;

	if (new_weapon != NPC->client->ps.weapon)
	{
		changing = qtrue;
	}
	if (changing)
	{
		G_RemoveWeaponModels(NPC);
	}
	ChangeWeapon(NPC, new_weapon);

	if (changing && NPC->client->ps.weapon != WP_NONE)
	{
		if (NPC->client->ps.weapon == WP_SABER)
		{
			WP_SaberAddG2SaberModels(NPC);
			G_RemoveHolsterModels(NPC);
		}
		else if (NPC->client->ps.weapon == WP_DUAL_PISTOL)
		{
			if (com_kotor->integer == 1) //playing kotor
			{
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handLBolt, 1);
			}
			else
			{
				if (NPC->client->charKOTORWeapons == 1)
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handLBolt, 1);
				}
				else
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handLBolt, 1);
				}
			}
		}
		else if (NPC->client->ps.weapon == WP_DUAL_CLONEPISTOL)
		{
			if (com_kotor->integer == 1) //playing kotor
			{
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handLBolt, 1);
			}
			else
			{
				if (NPC->client->charKOTORWeapons == 1)
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handLBolt, 1);
				}
				else
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handLBolt, 1);
				}
			}
		}
		else if (NPC->client->ps.weapon == WP_DROIDEKA)
		{
			if (com_kotor->integer == 1) //playing kotor
			{
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handLBolt, 1);
			}
			else
			{
				if (NPC->client->charKOTORWeapons == 1)
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handLBolt, 1);
				}
				else
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handRBolt, 0);
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handLBolt, 1);
				}
			}
		}
		else
		{
			if (com_kotor->integer == 1) //playing kotor
			{
				G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
			}
			else
			{
				if (NPC->client->charKOTORWeapons == 1)
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].altweaponMdl, NPC->handRBolt, 0);
				}
				else
				{
					G_CreateG2AttachedWeaponModel(NPC, weaponData[NPC->client->ps.weapon].weaponMdl, NPC->handRBolt, 0);
				}
			}
			WP_SaberAddHolsteredG2SaberModels(NPC);
		}
	}
}

void NPC_ApplyWeaponFireDelay()
{
	if (NPC->attackDebounceTime > level.time)
	{
		//Just fired, if attacking again, must be a burst fire, so don't add delay
		//NOTE: Borg AI uses attackDebounceTime "incorrectly", so this will always return for them!
		return;
	}

	switch (client->ps.weapon)
	{
	case WP_BOT_LASER:
		NPCInfo->burstCount = 0;
		client->fireDelay = 500;
		break;

	case WP_THERMAL:
		if (client->ps.clientNum)
		{
			//NPCs delay...
			//FIXME: player should, too, but would feel weird in 1st person, even though it
			//			would look right in 3rd person.  Really should have a wind-up anim
			//			for player as he holds down the fire button to throw, then play
			//			the actual throw when he lets go...
			client->fireDelay = 700;
		}
		break;

	case WP_MELEE:
	case WP_TUSKEN_STAFF:
		if (!PM_DroidMelee(client->NPC_class))
		{
			//FIXME: should be unique per melee anim
			client->fireDelay = 300;
		}
		break;

	case WP_TUSKEN_RIFLE:
		if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
		{
			//FIXME: should be unique per melee anim
			client->fireDelay = 300;
		}
		break;

	case WP_DROIDEKA:
		if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
		{
			client->fireDelay = Q_irand(500, 1000);
		}
		break;

	default:
		client->fireDelay = 0;
		break;
	}
};

/*
-------------------------
ShootThink
-------------------------
*/
static void ShootThink()
{
	int delay;

	ucmd.buttons |= BUTTON_ATTACK;

	NPCInfo->currentAmmo = client->ps.ammo[weaponData[client->ps.weapon].ammoIndex]; // checkme

	NPC_ApplyWeaponFireDelay();

	if (NPCInfo->aiFlags & NPCAI_BURST_WEAPON)
	{
		if (!NPCInfo->burstCount)
		{
			NPCInfo->burstCount = Q_irand(NPCInfo->burstMin, NPCInfo->burstMax);
			delay = 0;
		}
		else
		{
			NPCInfo->burstCount--;
			if (NPCInfo->burstCount == 0)
			{
				delay = NPCInfo->burstSpacing + Q_irand(-150, 150);
			}
			else
			{
				delay = 0;
			}
		}

		if (!delay)
		{
			// HACK: dirty little emplaced bits, but is done because it would otherwise require some sort of new variable...
			if (client->ps.weapon == WP_EMPLACED_GUN)
			{
				if (NPC->owner) // try and get the debounce values from the chair if we can
				{
					if (g_spskill->integer == 0)
					{
						delay = NPC->owner->random + 150;
					}
					else if (g_spskill->integer == 1)
					{
						delay = NPC->owner->random + 100;
					}
					else
					{
						delay = NPC->owner->random;
					}
				}
				else
				{
					if (g_spskill->integer == 0)
					{
						delay = 350;
					}
					else if (g_spskill->integer == 1)
					{
						delay = 300;
					}
					else
					{
						delay = 200;
					}
				}
			}
		}
	}
	// Custom rotary cannon handling
	else if (client->ps.weapon == WP_Z6_ROTARY_CANNON) {
		// If cooling down, do not fire
		if (level.time < NPCInfo->rotaryCannonCooldownTime)
		{
			ucmd.buttons &= ~BUTTON_ATTACK;
			return;
		}

		// Fire a shot
		//ucmd.buttons |= BUTTON_ATTACK;
		NPCInfo->rotaryCannonShotsFired++;

		// If reached max shots, start cooldown
		if (NPCInfo->rotaryCannonShotsFired >= 35)
		{
			NPCInfo->rotaryCannonCooldownTime = level.time + 2500; // 2.5 seconds cooldown
			NPCInfo->rotaryCannonShotsFired = 0;
		}

		delay = NPCInfo->burstSpacing + Q_irand(-50, 50);
	}
	else
	{
		delay = NPCInfo->burstSpacing + Q_irand(-150, 150);
	}

	NPCInfo->shotTime = level.time + delay;
	NPC->attackDebounceTime = level.time + NPC_AttackDebounceForWeapon();
}

extern qboolean IsSurrendering(const gentity_t* self);
extern qboolean IsRESPECTING(const gentity_t* self);
extern qboolean IsCowering(const gentity_t* self);
extern qboolean is_anim_requires_responce(const gentity_t* self);
extern qboolean InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern void WP_DeactivateSaber(const gentity_t* self, qboolean clear_length = qfalse);
extern qboolean PM_SaberInAttack(int move);
extern qboolean PM_SaberInStart(int move);
extern qboolean pm_saber_in_special_attack(int anim);
extern qboolean PM_SpinningSaberAnim(int anim);
extern qboolean PM_SpinningAnim(int anim);

extern void BubbleShield_TurnOn();
extern qboolean droideka_npc(const gentity_t* ent);

void WeaponThink()
{
	if (!NPC || !NPC->client || !NPCInfo)
	{
		return;
	}

	playerState_t* ps = &NPC->client->ps;
	// ------------------------------------------------------------
	// FIX: Only clear BUTTON_ATTACK for non-saber weapons.
	// Saber NPCs rely on BUTTON_ATTACK being preserved.
	// ------------------------------------------------------------
	if (client->ps.weapon != WP_SABER)
	{
		ucmd.buttons &= ~BUTTON_ATTACK;
	}

	// Weapon state checks
	if (ps->weaponstate == WEAPON_RAISING ||
		ps->weaponstate == WEAPON_DROPPING ||
		ps->weaponstate == WEAPON_RELOADING)
	{
		ucmd.weapon = ps->weapon;
		return;
	}

	// Assassin droid shield prevents firing
	if ((NPC->flags & FL_SHIELDED) &&
		NPC->client->NPC_class == CLASS_ASSASSIN_DROID)
	{
		return;
	}

	// Cloak / stun / shock prevents firing
	if (ps->powerups[PW_CLOAKED] ||
		level.time < ps->powerups[PW_UNCLOAKING] ||
		ps->powerups[PW_STUNNED] ||
		ps->powerups[PW_SHOCKED])
	{
		return;
	}

	// No weapon equipped
	if (ps->weapon == WP_NONE)
	{
		return;
	}

	// Only fire when ready
	if (ps->weaponstate != WEAPON_READY &&
		ps->weaponstate != WEAPON_FIRING &&
		ps->weaponstate != WEAPON_IDLE)
	{
		return;
	}

	// Fire rate limiter
	if (level.time < NPCInfo->shotTime)
	{
		return;
	}

	// DROIDEKA SHIELD HANDLING
	if (droideka_npc(NPC))
	{
		// Turn shield on if needed
		if (in_camera ||
			!(NPC->flags & FL_SHIELDED) ||
			!ps->powerups[PW_GALAK_SHIELD])
		{
			BubbleShield_TurnOn();
		}

		if (NPC->client->ps.powerups[PW_GALAK_SHIELD] == 0)
		{
			BubbleShield_TurnOn();
		}

		if (NPC->client->ps.powerups[PW_STUNNED] != 0)
		{
			return;
		}
	}

	// Ammo checks
	const int ammoIndex = weaponData[ps->weapon].ammoIndex;
	const int primaryCost = weaponData[ps->weapon].energyPerShot;
	const int altCost = weaponData[ps->weapon].altEnergyPerShot;

	if (ps->ammo[ammoIndex] < primaryCost)
	{
		Add_Ammo(NPC, ps->weapon, primaryCost * 10);
		NPC_SetAnim(NPC, SETANIM_TORSO, BOTH_RELOAD,
			SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		ps->weaponstate = WEAPON_RELOADING;
		return;
	}
	else if (ps->ammo[ammoIndex] < altCost)
	{
		Add_Ammo(NPC, ps->weapon, altCost * 5);
		NPC_SetAnim(NPC, SETANIM_TORSO, BOTH_RELOAD,
			SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		ps->weaponstate = WEAPON_RELOADING;
		return;
	}

	// Ready to fire
	ucmd.weapon = ps->weapon;
	ShootThink();
}

/*
HaveWeapon
*/

qboolean HaveWeapon(const int weapon)
{
	return static_cast<qboolean>(client->ps.weapons[weapon] != 0);
}

qboolean EntIsGlass(const gentity_t* check)
{
	if (check->classname &&
		!Q_stricmp("func_breakable", check->classname) &&
		check->count == 1 && check->health <= 100)
	{
		return qtrue;
	}

	return qfalse;
}

qboolean ShotThroughGlass(trace_t* tr, const gentity_t* target, vec3_t spot, const int mask)
{
	const gentity_t* hit = &g_entities[tr->entityNum];
	if (hit != target && EntIsGlass(hit))
	{
		//ok to shoot through breakable glass
		const int skip = hit->s.number;
		vec3_t muzzle;

		VectorCopy(tr->endpos, muzzle);
		gi.trace(tr, muzzle, nullptr, nullptr, spot, skip, mask, static_cast<EG2_Collision>(0), 0);
		return qtrue;
	}

	return qfalse;
}

/*
CanShoot
determine if NPC can directly target enemy

this function does not check teams, invulnerability, notarget, etc....

Added: If can't shoot center, try head, if not, see if it's close enough to try anyway.
*/
extern qboolean NPC_EntityIsBreakable(const gentity_t* ent);

qboolean CanShoot(const gentity_t* ent, const gentity_t* shooter)
{
	trace_t tr;
	vec3_t muzzle;
	vec3_t spot, diff;
	const qboolean is_breakable = NPC_EntityIsBreakable(ent);

	CalcEntitySpot(shooter, SPOT_WEAPON, muzzle);
	CalcEntitySpot(ent, SPOT_ORIGIN, spot); //FIXME preferred target locations for some weapons (feet for R/L)

	gi.trace(&tr, muzzle, nullptr, nullptr, spot, shooter->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);
	const gentity_t* traceEnt = &g_entities[tr.entityNum];

	// point blank, baby!
	if (!is_breakable && tr.startsolid && shooter->NPC && shooter->NPC->touchedByPlayer)
	{
		traceEnt = shooter->NPC->touchedByPlayer;
	}

	if (!is_breakable && ShotThroughGlass(&tr, ent, spot, MASK_SHOT))
	{
		traceEnt = &g_entities[tr.entityNum];
	}

	if (is_breakable && tr.fraction > 0.8)
	{
		// Close enough...
		return qtrue;
	}

	// shot is dead on
	if (traceEnt == ent)
	{
		return qtrue;
	}
	//MCG - Begin
	//ok, can't hit them in center, try their head
	CalcEntitySpot(ent, SPOT_HEAD, spot);
	gi.trace(&tr, muzzle, nullptr, nullptr, spot, shooter->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);
	traceEnt = &g_entities[tr.entityNum];
	if (traceEnt == ent)
	{
		return qtrue;
	}

	//Actually, we should just check to fire in dir we're facing and if it's close enough,
	//and we didn't hit someone on our own team, shoot
	VectorSubtract(spot, tr.endpos, diff);
	if (VectorLength(diff) < Q_flrand(0.0f, 1.0f) * 32)
	{
		return qtrue;
	}
	//MCG - End
	// shot would hit a non-client
	if (!traceEnt->client)
	{
		return qfalse;
	}

	// shot is blocked by another player

	// he's already dead, so go ahead
	if (traceEnt->health <= 0)
	{
		return qtrue;
	}

	// don't deliberately shoot a teammate
	if (traceEnt->client && traceEnt->client->playerTeam == shooter->client->playerTeam)
	{
		return qfalse;
	}

	// he's just in the wrong place, go ahead
	return qtrue;
}

/*
void NPC_CheckPossibleEnemy( gentity_t *other, visibility_t vis )

Added: hacks for scripted NPCs
*/
void NPC_CheckPossibleEnemy(gentity_t* other, const visibility_t vis)
{
	// is he is already our enemy?
	if (other == NPC->enemy)
		return;

	if (other->flags & FL_NOTARGET)
		return;

	// we already have an enemy and this guy is in our FOV, see if this guy would be better
	if (NPC->enemy && vis == VIS_FOV)
	{
		if (NPCInfo->enemyLastSeenTime - level.time < 2000)
		{
			return;
		}
		if (enemyVisibility == VIS_UNKNOWN)
		{
			enemyVisibility = NPC_CheckVisibility(NPC->enemy, CHECK_360 | CHECK_FOV);
		}
		if (enemyVisibility == VIS_FOV)
		{
			return;
		}
	}

	if (!NPC->enemy)
	{
		//only take an enemy if you don't have one yet
		G_SetEnemy(NPC, other);
	}

	if (vis == VIS_FOV)
	{
		NPCInfo->enemyLastSeenTime = level.time;
		VectorCopy(other->currentOrigin, NPCInfo->enemyLastSeenLocation);
		NPCInfo->enemyLastHeardTime = 0;
		VectorClear(NPCInfo->enemyLastHeardLocation);
	}
	else
	{
		NPCInfo->enemyLastSeenTime = 0;
		VectorClear(NPCInfo->enemyLastSeenLocation);
		NPCInfo->enemyLastHeardTime = level.time;
		VectorCopy(other->currentOrigin, NPCInfo->enemyLastHeardLocation);
	}
}

//==========================================
//MCG Added functions:
//==========================================
int NPC_AttackDebounceForWeapon()
{
	switch (NPC->client->ps.weapon)
	{
	case WP_SABER:
	{
		if (NPC->client->NPC_class == CLASS_KYLE
			&& NPC->spawnflags & 1)
		{
			return Q_irand(1500, 5000);
		}
		return 0;
	}

	case WP_BOT_LASER:

		if (g_spskill->integer == 0)
			return 2000;

		if (g_spskill->integer == 1)
			return 1500;

		return 1000;

	default:
		return NPCInfo->burstSpacing + Q_irand(800, 1500); //was 100 by default
	}
}

//FIXME: need a mindist for explosive weapons
float NPC_MaxDistSquaredForWeapon()
{
	if (NPCInfo->stats.shootDistance > 0)
	{
		//overrides default weapon dist
		return NPCInfo->stats.shootDistance * NPCInfo->stats.shootDistance;
	}

	switch (NPC->s.weapon)
	{
	case WP_BLASTER: //scav rifle
		return 1024 * 1024; //should be shorter?

	case WP_BRYAR_PISTOL: //prifle
	case WP_SBD_BLASTER:
	case WP_JAWA:
		return 1024 * 1024;

	case WP_REY:
		return 1024 * 1024;

	case WP_JANGO:
		return 1024 * 1024;

	case WP_CLONEPISTOL:
	case WP_DUAL_CLONEPISTOL:
		return 1024 * 1024;

	case WP_BLASTER_PISTOL: //prifle
	case WP_DUAL_PISTOL:
	case WP_DROIDEKA:
		return 1024 * 1024;

	case WP_DISRUPTOR: //disruptor
	case WP_TUSKEN_RIFLE:
	{
		if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
		{
			return 4096 * 4096;
		}
		return 1024 * 1024;
	}
	case WP_SABER:
	{
		if (NPC->client && NPC->client->ps.SaberLength())
		{
			//FIXME: account for whether enemy and I are heading towards each other!
			return (NPC->client->ps.SaberLength() + NPC->maxs[0] * 1.5) * (NPC->client->ps.SaberLength() + NPC->maxs
				[0] * 1.5);
		}
		return 48 * 48;
	}

	default:
		return 1024 * 1024; //was 0
	}
}

qboolean NPC_EnemyTooFar(const gentity_t* enemy, float dist, const qboolean to_shoot)
{
	if (!to_shoot)
	{
		//Not trying to actually press fire button with this check
		if (NPC->client->ps.weapon == WP_SABER)
		{
			//Just have to get to him
			return qfalse;
		}
	}

	if (!dist)
	{
		vec3_t vec;
		VectorSubtract(NPC->currentOrigin, enemy->currentOrigin, vec);
		dist = VectorLengthSquared(vec);
	}

	if (dist > NPC_MaxDistSquaredForWeapon())
		return qtrue;

	return qfalse;
}

/*
NPC_PickEnemy

Randomly picks a living enemy from the specified team and returns it

FIXME: For now, you MUST specify an enemy team

If you specify choose closest, it will find only the closest enemy

If you specify checkVis, it will return and enemy that is visible

If you specify findPlayersFirst, it will try to find players first

You can mix and match any of those options (example: find closest visible players first)

FIXME: this should go through the snapshot and find the closest enemy
*/
gentity_t* NPC_PickEnemy(const gentity_t* closest_to,
	const int enemy_team,
	const qboolean check_vis,
	const qboolean find_players_first,
	const qboolean find_closest)
{
	if (!NPC || !NPCInfo || !closest_to)
	{
		return nullptr;
	}

	if (enemy_team == TEAM_NEUTRAL)
	{
		return nullptr;
	}

	int         num_choices = 0;
	int         choice[128] = { 0 }; // NOTE: hard cap on random choices
	gentity_t* newenemy;
	gentity_t* closest_enemy = nullptr;
	vec3_t      diff;
	float       rel_dist;
	float       best_dist = Q3_INFINITE;
	qboolean    failed = qfalse;
	int         vis_checks = CHECK_360 | CHECK_FOV | CHECK_VISRANGE;
	int         min_vis = VIS_FOV;

	// Active battle states ignore FOV
	if (NPCInfo->behaviorState == BS_STAND_AND_SHOOT ||
		NPCInfo->behaviorState == BS_HUNT_AND_KILL)
	{
		vis_checks &= ~CHECK_FOV;
		min_vis = VIS_360;
	}

	// Optional: try to find the player first (entity 0)
	if (find_players_first)
	{
		newenemy = &g_entities[0];

		if (newenemy &&
			newenemy->inuse &&
			newenemy->client &&
			!(newenemy->flags & FL_NOTARGET) &&
			!(newenemy->s.eFlags & EF_NODRAW) &&
			newenemy->health > 0 &&
			NPC_ValidEnemy(newenemy))
		{
			if (newenemy != NPC->lastEnemy &&
				gi.inPVS(newenemy->currentOrigin, NPC->currentOrigin))
			{
				if (NPCInfo->behaviorState == BS_INVESTIGATE ||
					NPCInfo->behaviorState == BS_PATROL)
				{
					if (!NPC->enemy)
					{
						if (!InVisrange(newenemy))
						{
							failed = qtrue;
						}
						else if (NPC_CheckVisibility(newenemy,
							CHECK_360 | CHECK_FOV | CHECK_VISRANGE) != VIS_FOV)
						{
							failed = qtrue;
						}
					}
				}

				if (!failed)
				{
					VectorSubtract(closest_to->currentOrigin, newenemy->currentOrigin, diff);
					rel_dist = VectorLengthSquared(diff);

					// Hidden logic
					if (newenemy->client->hiddenDist > 0.0f)
					{
						const float hiddenDistSq = newenemy->client->hiddenDist * newenemy->client->hiddenDist;

						if (rel_dist > hiddenDistSq)
						{
							if (VectorLengthSquared(newenemy->client->hiddenDir))
							{
								VectorNormalize(diff);
								const float dot = DotProduct(newenemy->client->hiddenDir, diff);

								if (dot > 0.5f)
								{
									failed = qtrue;
								}
								else
								{
									Debug_Printf(debugNPCAI, DEBUG_LEVEL_INFO,
										"%s saw %s trying to hide - hiddenDir %s targetDir %s dot %f\n",
										NPC->targetname, newenemy->targetname,
										vtos(newenemy->client->hiddenDir), vtos(diff), dot);
								}
							}
							else
							{
								failed = qtrue;
							}
						}
						else
						{
							Debug_Printf(debugNPCAI, DEBUG_LEVEL_INFO,
								"%s saw %s trying to hide - hiddenDist %f\n",
								NPC->targetname, newenemy->targetname,
								newenemy->client->hiddenDist);
						}
					}

					if (!failed)
					{
						if (find_closest)
						{
							if (rel_dist < best_dist &&
								!NPC_EnemyTooFar(newenemy, rel_dist, qfalse))
							{
								if (check_vis)
								{
									if (NPC_CheckVisibility(newenemy, vis_checks) == min_vis)
									{
										closest_enemy = newenemy;
										best_dist = rel_dist;
									}
								}
								else
								{
									closest_enemy = newenemy;
									best_dist = rel_dist;
								}
							}
						}
						else if (!NPC_EnemyTooFar(newenemy, 0, qfalse))
						{
							if (check_vis)
							{
								if (NPC_CheckVisibility(newenemy,
									CHECK_360 | CHECK_FOV | CHECK_VISRANGE) == VIS_FOV)
								{
									if (num_choices < static_cast<int>(ARRAY_LEN(choice)))
									{
										choice[num_choices++] = newenemy->s.number;
									}
								}
							}
							else
							{
								if (num_choices < static_cast<int>(ARRAY_LEN(choice)))
								{
									choice[num_choices++] = newenemy->s.number;
								}
							}
						}
					}
				}
			}
		}
	}

	if (find_closest && closest_enemy)
	{
		return closest_enemy;
	}

	if (num_choices > 0)
	{
		const int idx = choice[rand() % num_choices];
		return &g_entities[idx];
	}

	// Reset for general search
	num_choices = 0;
	best_dist = Q3_INFINITE;
	closest_enemy = nullptr;

	for (int entNum = 0; entNum < globals.num_entities; entNum++)
	{
		newenemy = &g_entities[entNum];

		if (!newenemy || !newenemy->inuse || newenemy == NPC)
		{
			continue;
		}

		if (!(newenemy->client || (newenemy->svFlags & SVF_NONNPC_ENEMY)))
		{
			continue;
		}

		if ((newenemy->flags & FL_NOTARGET) ||
			(newenemy->s.eFlags & EF_NODRAW) ||
			newenemy->health <= 0)
		{
			continue;
		}

		// Team / enemy validation
		if (newenemy->client)
		{
			if (!NPC_ValidEnemy(newenemy))
			{
				continue;
			}
		}
		else
		{
			if (newenemy->noDamageTeam != enemy_team)
			{
				continue;
			}
		}

		// Player allies turning on themselves?
		if (NPC->client->playerTeam == TEAM_PLAYER && enemy_team == TEAM_PLAYER)
		{
			if (newenemy->s.number != 0)
			{
				continue;
			}
		}

		if (newenemy == NPC->lastEnemy)
		{
			continue;
		}

		if (!gi.inPVS(newenemy->currentOrigin, NPC->currentOrigin))
		{
			continue;
		}

		if (NPCInfo->behaviorState == BS_INVESTIGATE ||
			NPCInfo->behaviorState == BS_PATROL)
		{
			if (!NPC->enemy)
			{
				if (!InVisrange(newenemy))
				{
					continue;
				}
				if (NPC_CheckVisibility(newenemy,
					CHECK_360 | CHECK_FOV | CHECK_VISRANGE) != VIS_FOV)
				{
					continue;
				}
			}
		}

		VectorSubtract(closest_to->currentOrigin, newenemy->currentOrigin, diff);
		rel_dist = VectorLengthSquared(diff);

		// Hidden logic
		if (newenemy->client && newenemy->client->hiddenDist > 0.0f)
		{
			const float hiddenDistSq = newenemy->client->hiddenDist * newenemy->client->hiddenDist;

			if (rel_dist > hiddenDistSq)
			{
				if (VectorLengthSquared(newenemy->client->hiddenDir))
				{
					VectorNormalize(diff);
					const float dot = DotProduct(newenemy->client->hiddenDir, diff);

					if (dot > 0.5f)
					{
						continue;
					}

					Debug_Printf(debugNPCAI, DEBUG_LEVEL_INFO,
						"%s saw %s trying to hide - hiddenDir %s targetDir %s dot %f\n",
						NPC->targetname, newenemy->targetname,
						vtos(newenemy->client->hiddenDir), vtos(diff), dot);
				}
				else
				{
					continue;
				}
			}
			else
			{
				Debug_Printf(debugNPCAI, DEBUG_LEVEL_INFO,
					"%s saw %s trying to hide - hiddenDist %f\n",
					NPC->targetname, newenemy->targetname,
					newenemy->client->hiddenDist);
			}
		}

		if (find_closest)
		{
			if (rel_dist < best_dist &&
				!NPC_EnemyTooFar(newenemy, rel_dist, qfalse))
			{
				if (check_vis)
				{
					if (NPC_CheckVisibility(newenemy, vis_checks) == min_vis)
					{
						best_dist = rel_dist;
						closest_enemy = newenemy;
					}
				}
				else
				{
					best_dist = rel_dist;
					closest_enemy = newenemy;
				}
			}
		}
		else if (!NPC_EnemyTooFar(newenemy, 0, qfalse))
		{
			if (check_vis)
			{
				if (NPC_CheckVisibility(newenemy, CHECK_360 | CHECK_VISRANGE) >= VIS_360)
				{
					if (num_choices < static_cast<int>(ARRAY_LEN(choice)))
					{
						choice[num_choices++] = newenemy->s.number;
					}
				}
			}
			else
			{
				if (num_choices < static_cast<int>(ARRAY_LEN(choice)))
				{
					choice[num_choices++] = newenemy->s.number;
				}
			}
		}
	}

	if (find_closest)
	{
		// NOTE: can pick an enemy around a corner this way (original behaviour)
		return closest_enemy;
	}

	if (!num_choices)
	{
		return nullptr;
	}

	const int idx = choice[rand() % num_choices];
	return &g_entities[idx];
}

/*
gentity_t *NPC_PickAlly ( void )

  Simply returns closest visible ally
*/

gentity_t* NPC_PickAlly(const qboolean facing_each_other,
	const float range,
	const qboolean ignore_group,
	const qboolean moving_only)
{
	if (!NPC || !NPC->client)
	{
		return nullptr;
	}

	gentity_t* closest_ally = nullptr;
	float best_dist = range;

	for (int entNum = 0; entNum < globals.num_entities; entNum++)
	{
		gentity_t* ally = &g_entities[entNum];

		// Basic validity checks
		if (!ally || !ally->inuse || ally == NPC)
		{
			continue;
		}

		if (!ally->client || ally->health <= 0)
		{
			continue;
		}

		// Must be on same team (or disguised)
		if (!(ally->client->playerTeam == NPC->client->playerTeam ||
			NPC->client->playerTeam == TEAM_ENEMY))
		{
			continue;
		}

		// Group ignore logic
		if (ignore_group)
		{
			if (ally == NPC->client->leader)
			{
				continue;
			}

			if (ally->client->leader && ally->client->leader == NPC)
			{
				continue;
			}
		}

		// Must be in PVS
		if (!gi.inPVS(ally->currentOrigin, NPC->currentOrigin))
		{
			continue;
		}

		// Moving-only filter
		if (moving_only)
		{
			if (!VectorLengthSquared(ally->client->ps.velocity) &&
				!VectorLengthSquared(NPC->client->ps.velocity))
			{
				continue;
			}
		}

		// Distance check
		vec3_t diff;
		VectorSubtract(NPC->currentOrigin, ally->currentOrigin, diff);
		float rel_dist = VectorLength(diff);

		if (rel_dist >= best_dist)
		{
			continue;
		}

		// Facing-each-other logic
		if (facing_each_other)
		{
			vec3_t vf;

			// Ally must face NPC
			AngleVectors(ally->client->ps.viewangles, vf, nullptr, nullptr);
			VectorNormalize(vf);

			float dot = DotProduct(diff, vf);
			if (dot < 0.5f)
			{
				continue;
			}

			// NPC must face ally
			AngleVectors(NPC->client->ps.viewangles, vf, nullptr, nullptr);
			VectorNormalize(vf);

			dot = DotProduct(diff, vf);
			if (dot > -0.5f)
			{
				continue;
			}
		}

		// Visibility check
		if (NPC_CheckVisibility(ally, CHECK_360 | CHECK_VISRANGE) < VIS_360)
		{
			continue;
		}

		// This ally is the best so far
		best_dist = rel_dist;
		closest_ally = ally;
	}

	return closest_ally;
}

gentity_t* NPC_CheckEnemy(const qboolean find_new, const qboolean too_far_ok, const qboolean set_enemy)
{
	qboolean forcefind_new = qfalse;
	gentity_t* new_enemy = nullptr;

	if (NPC->enemy)
	{
		if (!NPC->enemy->inuse) //|| NPC->enemy == NPC )//wtf?  NPCs should never get mad at themselves!
		{
			if (set_enemy)
			{
				G_ClearEnemy(NPC);
			}
		}
	}

	if (NPC->svFlags & SVF_IGNORE_ENEMIES)
	{
		//We're ignoring all enemies for now
		if (set_enemy)
		{
			G_ClearEnemy(NPC);
		}
		return nullptr;
	}

	// Kyle does not get new enemies if not close to his leader
	if ((NPC->client->NPC_class == CLASS_KYLE || NPC->client->NPC_class == CLASS_GROGU) &&
		NPC->client->leader &&
		Distance(NPC->client->leader->currentOrigin, NPC->currentOrigin) > 3000)
	{
		if (NPC->enemy)
		{
			G_ClearEnemy(NPC);
		}
		return nullptr;
	}

	if (NPC->svFlags & SVF_LOCKEDENEMY)
	{
		//keep this enemy until dead
		if (NPC->enemy)
		{
			if (!NPC->NPC && !(NPC->svFlags & SVF_NONNPC_ENEMY) || NPC->enemy->health > 0)
			{
				//Enemy never had health (a train or info_not_null, etc) or did and is now dead (NPCs, turrets, etc)
				return nullptr;
			}
		}
		NPC->svFlags &= ~SVF_LOCKEDENEMY;
	}

	if (NPC->enemy)
	{
		if (NPC_EnemyTooFar(NPC->enemy, 0, qfalse))
		{
			if (find_new)
			{
				//See if there is a close one and take it if so, else keep this one
				forcefind_new = qtrue;
			}
			else if (!too_far_ok) //FIXME: don't need this extra bool any more
			{
				if (set_enemy)
				{
					G_ClearEnemy(NPC);
				}
			}
		}
		else if (!gi.inPVS(NPC->currentOrigin, NPC->enemy->currentOrigin))
		{
			//FIXME: should this be a line-of site check?
			//FIXME: a lot of things check PVS AGAIN when deciding whether
			//or not to shoot, redundant!
			//Should we lose the enemy?
			//FIXME: if lose enemy, run lostenemyscript
			if (NPC->enemy->client && NPC->enemy->client->hiddenDist)
			{
				//He ducked into shadow while we weren't looking
				//Drop enemy and see if we should search for him
				NPC_LostEnemyDecideChase();
			}
			else
			{
				//If we're not chasing him, we need to lose him
				//NOTE: since we no longer have bStates, really, this logic doesn't work, so never give him up

				/*
				switch( NPCInfo->behaviorState )
				{
				case BS_HUNT_AND_KILL:
					//Okay to lose PVS, we're chasing them
					break;
				case BS_RUN_AND_SHOOT:
				//FIXME: only do this if !(NPCInfo->scriptFlags&SCF_CHASE_ENEMY)
					//If he's not our goalEntity, we're running somewhere else, so lose him
					if ( NPC->enemy != NPCInfo->goalEntity )
					{
						G_ClearEnemy( NPC );
					}
					break;
				default:
					//We're not chasing him, so lose him as an enemy
					G_ClearEnemy( NPC );
					break;
				}
				*/
			}
		}
	}

	if (NPC->enemy)
	{
		if (NPC->enemy->health <= 0 || NPC->enemy->flags & FL_NOTARGET)
		{
			if (set_enemy)
			{
				G_ClearEnemy(NPC);
			}
		}
	}

	const gentity_t* closest_to = NPC;
	//FIXME: check your defendEnt, if you have one, see if their enemy is different
	//than yours, or, if they don't have one, pick the closest enemy to THEM?
	if (NPCInfo->defendEnt)
	{
		//Trying to protect someone
		if (NPCInfo->defendEnt->health > 0)
		{
			//Still alive, We presume we're close to them, navigation should handle this?
			if (NPCInfo->defendEnt->enemy)
			{
				//They were shot or acquired an enemy
				if (NPC->enemy != NPCInfo->defendEnt->enemy)
				{
					//They have a different enemy, take it!
					new_enemy = NPCInfo->defendEnt->enemy;
					if (set_enemy)
					{
						G_SetEnemy(NPC, NPCInfo->defendEnt->enemy);
					}
				}
			}
			else if (NPC->enemy == nullptr)
			{
				//We don't have an enemy, so find closest to defendEnt
				closest_to = NPCInfo->defendEnt;
			}
		}
	}

	if (!NPC->enemy || NPC->enemy && NPC->enemy->health <= 0 || forcefind_new)
	{
		//FIXME: NPCs that are moving after an enemy should ignore the can't hit enemy counter- that should only be for NPCs that are standing still
		//NOTE: cantHitEnemyCounter >= 100 means we couldn't hit enemy for a full
		//	10 seconds, so give up.  This means even if we're chasing him, we would
		//	try to find another enemy after 10 seconds (assuming the cantHitEnemyCounter
		//	is allowed to increment in a chasing b_state)
		qboolean foundenemy = qfalse;

		if (!find_new)
		{
			if (set_enemy)
			{
				NPC->lastEnemy = NPC->enemy;
				G_ClearEnemy(NPC);
			}
			return nullptr;
		}

		//If enemy dead or unshootable, look for others on out enemy's team
		if (NPC->client->enemyTeam != TEAM_NEUTRAL)
		{
			//NOTE:  this only checks vis if can't hit enemy for 10 tries, which I suppose
			//			means they need to find one that in more than just PVS
			//newenemy = NPC_PickEnemy( closestTo, NPC->client->enemyTeam, (NPC->cantHitEnemyCounter > 10), qfalse, qtrue );//3rd parm was (NPC->enemyTeam == TEAM_STARFLEET)
			//For now, made it so you ALWAYS have to check VIS
			new_enemy = NPC_PickEnemy(closest_to, NPC->client->enemyTeam, qtrue, qfalse, qtrue);
			//3rd parm was (NPC->enemyTeam == TEAM_STARFLEET)
			if (new_enemy)
			{
				foundenemy = qtrue;
				if (set_enemy)
				{
					G_SetEnemy(NPC, new_enemy);
				}
			}
		}

		//if ( !forcefindNew )
		{
			if (!foundenemy)
			{
				if (set_enemy)
				{
					NPC->lastEnemy = NPC->enemy;
					G_ClearEnemy(NPC);
				}
			}

			NPC->cantHitEnemyCounter = 0;
		}
		//FIXME: if we can't find any at all, go into INdependant NPC AI, pursue and kill
	}

	if (NPC->enemy && NPC->enemy->client)
	{
		if (NPC->enemy->client->playerTeam
			&& NPC->enemy->client->playerTeam != TEAM_FREE)
		{
			//			assert( NPC->client->playerTeam != NPC->enemy->client->playerTeam);
			if (NPC->client->playerTeam != NPC->enemy->client->playerTeam
				&& NPC->client->enemyTeam != TEAM_FREE
				&& NPC->client->enemyTeam != NPC->enemy->client->playerTeam)
			{
				NPC->client->enemyTeam = NPC->enemy->client->playerTeam;
			}
		}
	}

	npc_check_speak(NPC);

	return new_enemy;
}

/*
-------------------------
NPC_ClearShot
-------------------------
*/

qboolean NPC_ClearShot(const gentity_t* ent)
{
	if (NPC == nullptr || ent == nullptr)
		return qfalse;

	vec3_t muzzle;
	trace_t tr;

	CalcEntitySpot(NPC, SPOT_WEAPON, muzzle);

	// add aim error
	// use weapon instead of specific npc types, although you could add certain npc classes if you wanted
	if (NPC->s.weapon == WP_BLASTER || NPC->s.weapon == WP_BLASTER_PISTOL || NPC->s.weapon == WP_DUAL_PISTOL || NPC->s.weapon == WP_DUAL_CLONEPISTOL || NPC->s.weapon == WP_DROIDEKA)
		// any other guns to check for?
	{
		constexpr vec3_t mins = { -2, -2, -2 };
		constexpr vec3_t maxs = { 2, 2, 2 };

		gi.trace(&tr, muzzle, mins, maxs, ent->currentOrigin, NPC->s.number, MASK_SHOT, static_cast<EG2_Collision>(0),
			0);
	}
	else
	{
		gi.trace(&tr, muzzle, nullptr, nullptr, ent->currentOrigin, NPC->s.number, MASK_SHOT,
			static_cast<EG2_Collision>(0), 0);
	}

	if (tr.startsolid || tr.allsolid)
	{
		return qfalse;
	}

	if (tr.entityNum == ent->s.number)
		return qtrue;

	return qfalse;
}

/*
-------------------------
NPC_ShotEntity
-------------------------
*/

int NPC_ShotEntity(const gentity_t* ent, vec3_t impact_pos)
{
	if (!NPC || !NPC->client || !ent)
	{
		return ENTITYNUM_NONE;
	}

	vec3_t muzzle;
	vec3_t targ;
	trace_t tr;

	// Determine muzzle origin
	if (NPC->s.weapon == WP_THERMAL)
	{
		// Thermal detonators aim from slightly above the head
		vec3_t angles, forward, end;

		CalcEntitySpot(NPC, SPOT_HEAD, muzzle);

		VectorSet(angles, 0.0f, NPC->client->ps.viewangles[YAW], 0.0f);
		AngleVectors(angles, forward, nullptr, nullptr);

		VectorMA(muzzle, 8.0f, forward, end);
		end[2] += 24.0f;

		gi.trace(&tr, muzzle, vec3_origin, vec3_origin, end,
			NPC->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);

		VectorCopy(tr.endpos, muzzle);
	}
	else
	{
		CalcEntitySpot(NPC, SPOT_WEAPON, muzzle);
	}

	// Target point
	CalcEntitySpot(ent, SPOT_CHEST, targ);

	// Trace bounding box for blaster-type weapons
	int useSmallBox =
		(NPC->s.weapon == WP_BLASTER ||
			NPC->s.weapon == WP_BLASTER_PISTOL ||
			NPC->s.weapon == WP_DUAL_PISTOL ||
			NPC->s.weapon == WP_DUAL_CLONEPISTOL ||
			NPC->s.weapon == WP_DROIDEKA);

	if (useSmallBox)
	{
		static const vec3_t mins = { -2, -2, -2 };
		static const vec3_t maxs = { 2,  2,  2 };

		gi.trace(&tr, muzzle, mins, maxs, targ,
			NPC->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);
	}
	else
	{
		gi.trace(&tr, muzzle, nullptr, nullptr, targ,
			NPC->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);
	}

	// Return impact position if requested
	if (impact_pos)
	{
		VectorCopy(tr.endpos, impact_pos);
	}

	// Validate hit index
	if (tr.entityNum < 0 || tr.entityNum >= ENTITYNUM_WORLD)
	{
		return ENTITYNUM_NONE;
	}

	return tr.entityNum;
}

qboolean NPC_EvaluateShot(const int hit)
{
	if (!NPC || !NPC->enemy)
	{
		return qfalse;
	}

	// Validate hit index
	if (hit < 0 || hit >= ENTITYNUM_WORLD)
	{
		return qfalse;
	}

	gentity_t* hitEnt = &g_entities[hit];

	// If we hit the enemy directly, always shoot
	if (hit == NPC->enemy->s.number)
	{
		return qtrue;
	}

	// If we hit glass or a breakable brush, allow the shot
	if (hitEnt &&
		hitEnt->inuse &&
		(hitEnt->svFlags & SVF_GLASS_BRUSH))
	{
		return qtrue;
	}

	return qfalse;
}

/*
NPC_CheckAttack

Simply checks aggression and returns true or false
*/

qboolean NPC_CheckAttack(float scale)
{
	if (!scale)
		scale = 1.0;

	if (static_cast<float>(NPCInfo->stats.aggression) * scale < Q_flrand(0, 4))
	{
		return qfalse;
	}

	if (NPCInfo->shotTime > level.time)
		return qfalse;

	return qtrue;
}

/*
NPC_CheckDefend

Simply checks evasion and returns true or false
*/

qboolean NPC_CheckDefend(float scale)
{
	if (!scale)
		scale = 1.0;

	if (static_cast<float>(NPCInfo->stats.evasion) > Q_flrand(0.0f, 1.0f) * 4 * scale)
		return qtrue;

	return qfalse;
}

//NOTE: BE SURE TO CHECK PVS BEFORE THIS!
qboolean NPC_CheckCanAttack(float attack_scale)
{
	if (!NPC || !NPC->enemy || !NPC->enemy->inuse)
		return qfalse;

	if (NPC->enemy->flags & FL_NOTARGET)
		return qfalse;

	if (NPCInfo->scriptFlags & SCF_DONT_FIRE)
		return qfalse;

	if (attack_scale <= 0.0f)
		attack_scale = 1.0f;

	vec3_t muzzle, enemy_org, delta, angle_to_enemy;
	trace_t tr;

	// Get target position (head for accuracy)
	CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemy_org);
	NPC_AimWiggle(enemy_org);

	// Get muzzle position
	CalcEntitySpot(NPC, SPOT_WEAPON, muzzle);

	// Direction and distance
	VectorSubtract(enemy_org, muzzle, delta);
	vectoangles(delta, angle_to_enemy);
	float distance = VectorNormalize(delta);

	// Update desired yaw/pitch
	NPC->NPC->desiredYaw = angle_to_enemy[YAW];
	NPC_UpdateFiringAngles(qfalse, qtrue);

	// Too far?
	if (NPC_EnemyTooFar(NPC->enemy, distance * distance, qtrue))
		return qfalse;

	// Fire delay?
	if (client->fireDelay > 0)
	{
		NPC->NPC->desiredPitch = angle_to_enemy[PITCH];
		NPC_UpdateFiringAngles(qtrue, qfalse);
		return qfalse;
	}

	// Visibility check
	enemyVisibility = NPC_CheckVisibility(NPC->enemy, CHECK_360 | CHECK_FOV);
	NPCInfo->enemyLastVisibility = enemyVisibility;

	if (enemyVisibility < VIS_FOV)
	{
		// Update pitch even if we can't shoot
		NPC->NPC->desiredPitch = angle_to_enemy[PITCH];
		NPC_UpdateFiringAngles(qtrue, qfalse);
		return qfalse;
	}

	// In FOV — now check if shot is clean
	vec3_t forward, hitspot;
	AngleVectors(client->ps.viewangles, forward, nullptr, nullptr);
	VectorMA(muzzle, distance, forward, hitspot);

	gi.trace(&tr, muzzle, nullptr, nullptr, hitspot,
		NPC->s.number, MASK_SHOT, (EG2_Collision)0, 0);

	ShotThroughGlass(&tr, NPC->enemy, hitspot, MASK_SHOT);

	gentity_t* hitEnt = &g_entities[tr.entityNum];
	int dead_on = (hitEnt == NPC->enemy);

	// Friendly fire check
	if (!dead_on && hitEnt->client &&
		NPC->client->playerTeam &&
		hitEnt->client->playerTeam == NPC->client->playerTeam)
	{
		return qfalse;
	}

	// Explosive safety
	if (!dead_on && hitEnt->health <= 30 && hitEnt->e_DieFunc == dieF_ExplodeDeath_Wait)
	{
		vec3_t diff;
		VectorSubtract(NPC->currentOrigin, hitEnt->currentOrigin, diff);
		if (VectorLengthSquared(diff) < hitEnt->splashRadius * hitEnt->splashRadius)
			return qfalse;

		attack_scale *= 2.0f;
	}

	// Aim error modelling
	float max_aim_off = 128.0f - 16.0f * (float)NPCInfo->stats.aim;
	vec3_t diff;
	VectorSubtract(hitspot, enemy_org, diff);
	float aim_off = VectorLength(diff);

	if (!dead_on)
	{
		if (aim_off > Q_flrand(0.0f, 1.0f) * max_aim_off)
		{
			attack_scale *= 0.75f;
			if (aim_off > Q_flrand(0.0f, 1.0f) * max_aim_off)
				return qfalse;
		}
		attack_scale *= (max_aim_off - aim_off + 1.0f) / max_aim_off;
	}

	// Update pitch for final shot
	VectorSubtract(hitspot, muzzle, delta);
	vectoangles(delta, angle_to_enemy);
	NPC->NPC->desiredPitch = angle_to_enemy[PITCH];
	NPC_UpdateFiringAngles(qtrue, qfalse);

	// Final aggression check
	if (!NPC_CheckAttack(attack_scale))
		return qfalse;

	enemyVisibility = VIS_SHOOT;
	WeaponThink();
	return qtrue;
}

//========================================================================================
//OLD id-style hunt and kill
//========================================================================================
/*
IdealDistance

determines what the NPC's ideal distance from it's enemy should
be in the current situation
*/
float IdealDistance()
{
	float ideal = 225 - 20 * NPCInfo->stats.aggression;
	switch (NPC->s.weapon)
	{
	case WP_ROCKET_LAUNCHER:
		ideal += 200;
		break;

	case WP_CONCUSSION:
		ideal += 200;
		break;

	case WP_THERMAL:
		ideal += 50;
		break;

	case WP_SABER:
	case WP_BRYAR_PISTOL:
	case WP_SBD_BLASTER:
	case WP_BLASTER_PISTOL:
	case WP_BLASTER:
	case WP_REY:
	case WP_JANGO:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
	case WP_DROIDEKA:
	case WP_JAWA:
	default:
		break;
	}

	return ideal;
}

/*QUAKED point_combat (0.7 0 0.7) (-20 -20 -24) (20 20 45) DUCK FLEE INVESTIGATE SQUAD LEAN SNIPE
NPCs in b_state BS_COMBAT_POINT will find their closest empty combat_point

DUCK - NPC will duck and fire from this point, NOT IMPLEMENTED?
FLEE - Will choose this point when running
INVESTIGATE - Will look here if a sound is heard near it
SQUAD - NOT IMPLEMENTED
LEAN - Lean-type cover, NOT IMPLEMENTED
SNIPE - Snipers look for these first, NOT IMPLEMENTED
*/

void SP_point_combat(gentity_t* self)
{
	if (level.numCombatPoints >= MAX_COMBAT_POINTS)
	{
#ifndef FINAL_BUILD
		gi.Printf(S_COLOR_RED"ERROR:  Too many combat points, limit is %d\n", MAX_COMBAT_POINTS);
#endif
		G_FreeEntity(self);
		return;
	}

	self->s.origin[2] += 0.125;
	G_SetOrigin(self, self->s.origin);
	gi.linkentity(self);

	if (G_CheckInSolid(self, qtrue))
	{
#ifndef FINAL_BUILD
		gi.Printf(S_COLOR_RED"ERROR: combat point at %s in solid!\n", vtos(self->currentOrigin));
#endif
	}

	VectorCopy(self->currentOrigin, level.combatPoints[level.numCombatPoints].origin);

	level.combatPoints[level.numCombatPoints].flags = self->spawnflags;
	level.combatPoints[level.numCombatPoints].occupied = qfalse;

	level.numCombatPoints++;

	SpawnedPoint(self, NAV::PT_COMBATNODE);

	G_FreeEntity(self);
};

void CP_FindCombatPointWaypoints()
{
	for (int i = 0; i < level.numCombatPoints; i++)
	{
		level.combatPoints[i].waypoint = NAV::GetNearestNode(level.combatPoints[i].origin);
		if (level.combatPoints[i].waypoint == WAYPOINT_NONE)
		{
			//assert(0);
			level.combatPoints[i].waypoint = NAV::GetNearestNode(level.combatPoints[i].origin);
			gi.Printf(S_COLOR_RED"ERROR: Combat Point at %s has no waypoint!\n", vtos(level.combatPoints[i].origin));
			//`delayedShutDown = level.time + 100;
		}
	}
}

/*
-------------------------
NPC_CollectCombatPoints
-------------------------
*/

using combatPoint_m = std::map<float, int>;

static int NPC_CollectCombatPoints(const vec3_t origin, const float radius, combatPoint_m& points, const int flags)
{
	const float radius_sqr = radius * radius;
	float distance;

	//Collect all nearest
	for (int i = 0; i < level.numCombatPoints; i++)
	{
		//Must be vacant
		if (level.combatPoints[i].occupied == static_cast<int>(qtrue))
			continue;

		//If we want a duck space, make sure this is one
		if (flags & CP_DUCK && !(level.combatPoints[i].flags & CPF_DUCK))
			continue;

		//If we want a flee point, make sure this is one
		if (flags & CP_FLEE && !(level.combatPoints[i].flags & CPF_FLEE))
			continue;

		//If we want a snipe point, make sure this is one
		if (flags & CP_SNIPE && !(level.combatPoints[i].flags & CPF_SNIPE))
			continue;

		///Make sure this is an investigate combat point
		if (flags & CP_INVESTIGATE && level.combatPoints[i].flags & CPF_INVESTIGATE)
			continue;

		//Squad points are only valid if we're looking for them
		if (level.combatPoints[i].flags & CPF_SQUAD && (flags & CP_SQUAD) == qfalse)
			continue;

		if (flags & CP_NO_PVS)
		{
			//must not be within PVS of mu current origin
			if (gi.inPVS(origin, level.combatPoints[i].origin))
			{
				continue;
			}
		}

		if (flags & CP_HORZ_DIST_COLL)
		{
			distance = DistanceHorizontalSquared(origin, level.combatPoints[i].origin);
		}
		else
		{
			distance = DistanceSquared(origin, level.combatPoints[i].origin);
		}

		if (distance < radius_sqr)
		{
			//Using a map will sort nearest automatically
			points[distance] = i;
		}
	}

	return points.size();
}

/*
-------------------------
NPC_FindCombatPoint
-------------------------
*/

constexpr auto MIN_AVOID_DOT = 0.7f;
constexpr auto MIN_AVOID_DISTANCE = 128;
#define MIN_AVOID_DISTANCE_SQUARED	( MIN_AVOID_DISTANCE * MIN_AVOID_DISTANCE )
constexpr auto CP_COLLECT_RADIUS = 512.0f;

int NPC_FindCombatPoint(const vec3_t position, const vec3_t avoid_position, vec3_t dest_position, const int flags,
	float avoid_dist, const int ignore_point)
{
	combatPoint_m points;

	constexpr int best = -1; //, cost, bestCost = Q3_INFINITE, waypoint = WAYPOINT_NONE, destWaypoint = WAYPOINT_NONE;
	trace_t tr;
	float coll_rad = CP_COLLECT_RADIUS;
	vec3_t enemyPosition;
	const float vis_range_sq = NPCInfo->stats.visrange * NPCInfo->stats.visrange;
	const bool use_horiz_dist = NPC->s.weapon == WP_THERMAL || flags & CP_HORZ_DIST_COLL;

	if (NPC->enemy)
	{
		VectorCopy(NPC->enemy->currentOrigin, enemyPosition);
	}
	else if (avoid_position)
	{
		VectorCopy(avoid_position, enemyPosition);
	}
	else if (dest_position)
	{
		VectorCopy(dest_position, enemyPosition);
	}
	else
	{
		VectorCopy(NPC->currentOrigin, enemyPosition);
	}

	if (avoid_dist <= 0)
	{
		avoid_dist = MIN_AVOID_DISTANCE_SQUARED;
	}
	else
	{
		avoid_dist *= avoid_dist;
	}

	//Collect our nearest points
	if (flags & CP_NO_PVS || flags & CP_TRYFAR)
	{
		//much larger radius since most will be dropped?
		coll_rad = CP_COLLECT_RADIUS * 4;
	}
	NPC_CollectCombatPoints(dest_position, coll_rad, points, flags); //position

	for (const auto& point : points)
	{
		const int i = point.second;

		//Must not be one we want to ignore
		if (i == ignore_point)
		{
			continue;
		}

		//Get some distances for reasoning
		//distSqPointToNPC		= (*cpi).first;

		float dist_sq_point_to_enemy = DistanceSquared(level.combatPoints[i].origin, enemyPosition);
		float dist_sq_point_to_enemy_horiz = DistanceHorizontalSquared(level.combatPoints[i].origin, enemyPosition);
		const float dist_sq_point_to_enemy_check = use_horiz_dist
			? dist_sq_point_to_enemy_horiz
			: dist_sq_point_to_enemy;

		float dist_sq_npc_to_enemy = DistanceSquared(NPC->currentOrigin, enemyPosition);
		float dist_sq_npc_to_enemy_horiz = DistanceHorizontalSquared(NPC->currentOrigin, enemyPosition);
		const float dist_sq_npc_to_enemy_check = use_horiz_dist ? dist_sq_npc_to_enemy_horiz : dist_sq_npc_to_enemy;

		//Ignore points that are farther than currently located
		if (flags & CP_APPROACH_ENEMY && dist_sq_point_to_enemy_check > dist_sq_npc_to_enemy_check)
		{
			continue;
		}

		//Ignore points that are closer than currently located
		if (flags & CP_RETREAT && dist_sq_point_to_enemy_check < dist_sq_npc_to_enemy_check)
		{
			continue;
		}

		//Ignore points that are out of vis range
		if (flags & CP_CLEAR && dist_sq_point_to_enemy_check > vis_range_sq)
		{
			continue;
		}

		//Avoid this position?
		if (avoid_position && !(flags & CP_AVOID_ENEMY) && flags & CP_AVOID && DistanceSquared(
			level.combatPoints[i].origin, avoid_position) < avoid_dist)
		{
			continue;
		}

		//We want a point on other side of the enemy from current pos
		if (flags & CP_FLANK)
		{
			vec3_t e_dir2_cp;
			vec3_t e_dir2_me;
			VectorSubtract(position, enemyPosition, e_dir2_me);
			VectorNormalize(e_dir2_me);

			VectorSubtract(level.combatPoints[i].origin, enemyPosition, e_dir2_cp);
			VectorNormalize(e_dir2_cp);

			const float dot_to_cp = DotProduct(e_dir2_me, e_dir2_cp);

			//Not far enough behind enemy from current pos
			if (dot_to_cp >= 0.4)
			{
				continue;
			}
		}

		//we must have a route to the combat point
		if (flags & CP_HAS_ROUTE && !NAV::InSameRegion(NPC, level.combatPoints[i].origin))
		{
			continue;
		}

		//See if we're trying to avoid our enemy
		if (flags & CP_AVOID_ENEMY)
		{
			//Can't be too close to the enemy
			if (dist_sq_point_to_enemy < avoid_dist)
			{
				continue;
			}

			// otherwise, if currently safe and the path is not safe, ignore this point
			if (dist_sq_npc_to_enemy > avoid_dist &&
				!NAV::SafePathExists(position, level.combatPoints[i].origin, enemyPosition, avoid_dist))
			{
				continue;
			}
		}

		//Okay, now make sure it's not blocked
		gi.trace(&tr, level.combatPoints[i].origin, NPC->mins, NPC->maxs, level.combatPoints[i].origin, NPC->s.number,
			NPC->clipmask, static_cast<EG2_Collision>(0), 0);
		if (tr.allsolid || tr.startsolid)
		{
			continue;
		}

		if (NPC->enemy)
		{
			// Ignore Points That Do Not Have A Clear LOS To The Player
			if (flags & CP_CLEAR)
			{
				vec3_t weapon_offset;
				CalcEntitySpot(NPC, SPOT_WEAPON, weapon_offset);
				VectorSubtract(weapon_offset, NPC->currentOrigin, weapon_offset);
				VectorAdd(weapon_offset, level.combatPoints[i].origin, weapon_offset);

				if (NPC_ClearLOS(weapon_offset, NPC->enemy) == qfalse)
				{
					continue;
				}
			}

			// Ignore points that are not behind cover
			if (flags & CP_COVER && NPC_ClearLOS(level.combatPoints[i].origin, NPC->enemy) == qtrue)
			{
				continue;
			}
		}

		//they are sorted by this distance, so the first one to get this far is the closest
		return i;
	}

	return best;
}

int NPC_FindCombatPointRetry(const vec3_t position,
	const vec3_t avoid_position,
	vec3_t dest_position,
	int* cp_flags,
	const float avoid_dist,
	const int ignore_point)
{
	int cp = NPC_FindCombatPoint(position,
		avoid_position,
		dest_position,
		*cp_flags,
		avoid_dist,
		ignore_point);
	while (cp == -1 && (*cp_flags & ~CP_HAS_ROUTE) != CP_ANY)
	{
		//start "OR"ing out certain flags to see if we can find *any* point
		if (*cp_flags & CP_INVESTIGATE)
		{
			//don't need to investigate
			*cp_flags &= ~CP_INVESTIGATE;
		}
		else if (*cp_flags & CP_SQUAD)
		{
			//don't need to stick to squads
			*cp_flags &= ~CP_SQUAD;
		}
		else if (*cp_flags & CP_DUCK)
		{
			//don't need to duck
			*cp_flags &= ~CP_DUCK;
		}
		else if (*cp_flags & CP_NEAREST)
		{
			//don't need closest one to me
			*cp_flags &= ~CP_NEAREST;
		}
		else if (*cp_flags & CP_FLANK)
		{
			//don't need to flank enemy
			*cp_flags &= ~CP_FLANK;
		}
		else if (*cp_flags & CP_SAFE)
		{
			//don't need one that hasn't been shot at recently
			*cp_flags &= ~CP_SAFE;
		}
		else if (*cp_flags & CP_CLOSEST)
		{
			//don't need to get closest to enemy
			*cp_flags &= ~CP_CLOSEST;
			//but let's try to approach at least
			*cp_flags |= CP_APPROACH_ENEMY;
		}
		else if (*cp_flags & CP_APPROACH_ENEMY)
		{
			//don't need to approach enemy
			*cp_flags &= ~CP_APPROACH_ENEMY;
		}
		else if (*cp_flags & CP_COVER)
		{
			//don't need cover
			*cp_flags &= ~CP_COVER;
			//but let's pick one that makes us duck
			//*cpFlags |= CP_DUCK;
		}
		//	else if ( *cpFlags & CP_CLEAR )
		//	{//don't need a clear shot to enemy
		//		*cpFlags &= ~CP_CLEAR;
		//	}
		//	Never Give Up On Avoiding The Enemy
		//	else if ( *cpFlags & CP_AVOID_ENEMY )
		//	{//don't need to avoid enemy
		//		*cpFlags &= ~CP_AVOID_ENEMY;
		//	}
		else if (*cp_flags & CP_RETREAT)
		{
			//don't need to retreat
			*cp_flags &= ~CP_RETREAT;
		}
		else if (*cp_flags & CP_FLEE)
		{
			//don't need to flee
			*cp_flags &= ~CP_FLEE;
			//but at least avoid enemy and pick one that gives cover
			*cp_flags |= CP_COVER | CP_AVOID_ENEMY;
		}
		else if (*cp_flags & CP_AVOID)
		{
			//okay, even pick one right by me
			*cp_flags &= ~CP_AVOID;
		}
		else if (*cp_flags & CP_SHORTEST_PATH)
		{
			//okay, don't need the one with the shortest path
			*cp_flags &= ~CP_SHORTEST_PATH;
		}
		else
		{
			//screw it, we give up!
			return -1;
			/*
			if ( *cpFlags & CP_HAS_ROUTE )
			{//NOTE: this is really an absolute worst case scenario - will go to the first cp on the map!
				*cpFlags &= ~CP_HAS_ROUTE;
			}
			else
			{//NOTE: this is really an absolute worst case scenario - will go to the first cp on the map!
				*cpFlags = CP_ANY;
			}
			*/
		}
		//now try again
		cp = NPC_FindCombatPoint(position,
			avoid_position,
			dest_position,
			*cp_flags,
			avoid_dist,
			ignore_point);
	}
	return cp;
}

/*
-------------------------
NPC_FindSquadPoint
-------------------------
*/

int NPC_FindSquadPoint(vec3_t position)
{
	float nearest_dist = static_cast<float>(WORLD_SIZE) * static_cast<float>(WORLD_SIZE);
	int nearest_point = -1;

	for (int i = 0; i < level.numCombatPoints; i++)
	{
		//Squad points are only valid if we're looking for them
		if ((level.combatPoints[i].flags & CPF_SQUAD) == qfalse)
			continue;

		//Must be vacant
		if (level.combatPoints[i].occupied == qtrue)
			continue;

		const float dist = DistanceSquared(position, level.combatPoints[i].origin);

		//See if this is closer than the others
		if (dist < nearest_dist)
		{
			nearest_point = i;
			nearest_dist = dist;
		}
	}

	return nearest_point;
}

/*
-------------------------
NPC_ReserveCombatPoint
-------------------------
*/

qboolean NPC_ReserveCombatPoint(const int combat_point_id)
{
	//Make sure it's valid
	if (combat_point_id > level.numCombatPoints)
		return qfalse;

	//Make sure it's not already occupied
	if (level.combatPoints[combat_point_id].occupied)
		return qfalse;

	//Reserve it
	level.combatPoints[combat_point_id].occupied = qtrue;

	return qtrue;
}

/*
-------------------------
NPC_FreeCombatPoint
-------------------------
*/

qboolean NPC_FreeCombatPoint(const int combat_point_id, const qboolean failed)
{
	if (failed)
	{
		//remember that this one failed for us
		NPCInfo->lastFailedCombatPoint = combat_point_id;
	}
	//Make sure it's valid
	if (combat_point_id > level.numCombatPoints)
		return qfalse;

	//Make sure it's currently occupied
	if (level.combatPoints[combat_point_id].occupied == qfalse)
		return qfalse;

	//Free it
	level.combatPoints[combat_point_id].occupied = qfalse;

	return qtrue;
}

/*
-------------------------
NPC_SetCombatPoint
-------------------------
*/

qboolean NPC_SetCombatPoint(const int combat_point_id)
{
	if (combat_point_id == NPCInfo->combatPoint)
	{
		return qtrue;
	}

	//Free a combat point if we already have one
	if (NPCInfo->combatPoint != -1)
	{
		NPC_FreeCombatPoint(NPCInfo->combatPoint);
	}

	if (NPC_ReserveCombatPoint(combat_point_id) == qfalse)
		return qfalse;

	NPCInfo->combatPoint = combat_point_id;

	return qtrue;
}

extern qboolean CheckItemCanBePickedUpByNPC(const gentity_t* item, const gentity_t* pickerupper);

gentity_t* NPC_SearchForWeapons()
{
	gentity_t* best_found = nullptr;
	float best_dist = Q3_INFINITE;
	//	for ( found = g_entities; found < &g_entities[globals.num_entities] ; found++)
	for (int i = 0; i < globals.num_entities; i++)
	{
		//		if ( !found->inuse )
		//		{
		//			continue;
		//		}
		if (!PInUse(i))
			continue;

		gentity_t* found = &g_entities[i];

		//FIXME: Also look for ammo_racks that have weapons on them?
		if (found->s.eType != ET_ITEM)
		{
			continue;
		}
		if (found->item->giType != IT_WEAPON)
		{
			continue;
		}
		if (found->s.eFlags & EF_NODRAW)
		{
			continue;
		}
		if (CheckItemCanBePickedUpByNPC(found, NPC))
		{
			if (gi.inPVS(found->currentOrigin, NPC->currentOrigin))
			{
				const float dist = DistanceSquared(found->currentOrigin, NPC->currentOrigin);
				if (dist < best_dist)
				{
					if (NAV::InSameRegion(NPC, found))
					{
						//can nav to it
						best_dist = dist;
						best_found = found;
					}
				}
			}
		}
	}

	return best_found;
}

static void NPC_SetPickUpGoal(gentity_t* found_weap)
{
	vec3_t org;

	//NPCInfo->goalEntity = foundWeap;
	VectorCopy(found_weap->currentOrigin, org);
	org[2] += 24 - found_weap->mins[2] * -1; //adjust the origin so that I am on the ground
	NPC_SetMoveGoal(NPC, org, found_weap->maxs[0] * 0.75, qfalse, -1, found_weap);
	NPCInfo->tempGoal->waypoint = found_weap->waypoint;
	NPCInfo->tempBehavior = BS_DEFAULT;
	NPCInfo->squadState = SQUAD_TRANSITION;
}

extern void Q3_TaskIDComplete(gentity_t* ent, taskID_t taskType);
extern qboolean G_CanPickUpWeapons(const gentity_t* other);

void NPC_CheckGetNewWeapon()
{
	if (NPC->client
		&& !G_CanPickUpWeapons(NPC))
	{
		//this NPC can't pick up weapons...
		return;
	}
	if (NPC->s.weapon == WP_NONE && NPC->enemy)
	{
		//if running away because dropped weapon...
		if (NPCInfo->goalEntity
			&& NPCInfo->goalEntity == NPCInfo->tempGoal
			&& NPCInfo->goalEntity->enemy
			&& !NPCInfo->goalEntity->enemy->inuse)
		{
			//maybe was running at a weapon that was picked up
			NPC_ClearGoal();
			Q3_TaskIDComplete(NPC, TID_MOVE_NAV);
			//NPCInfo->goalEntity = NULL;
		}
		if (TIMER_Done(NPC, "panic") && NPCInfo->goalEntity == nullptr)
		{
			//need a weapon, any lying around?
			gentity_t* found_weap = NPC_SearchForWeapons();
			if (found_weap)
			{
				NPC_SetPickUpGoal(found_weap);
			}
		}
	}
}

void NPC_AimAdjust(const int change)
{
	if (!TIMER_Exists(NPC, "aimDebounce"))
	{
		const int debounce = 500 + (3 - g_spskill->integer) * 100;
		TIMER_Set(NPC, "aimDebounce", Q_irand(debounce, debounce + 1000));
		return;
	}
	if (TIMER_Done(NPC, "aimDebounce"))
	{
		NPCInfo->currentAim += change;
		if (NPCInfo->currentAim > NPCInfo->stats.aim)
		{
			//can never be better than max aim
			NPCInfo->currentAim = NPCInfo->stats.aim;
		}
		else if (NPCInfo->currentAim < -30)
		{
			//can never be worse than this
			NPCInfo->currentAim = -30;
		}

		const int debounce = 500 + (3 - g_spskill->integer) * 100;
		TIMER_Set(NPC, "aimDebounce", Q_irand(debounce, debounce + 1000));
	}
}

void G_AimSet(const gentity_t* self, const int aim)
{
	if (self->NPC)
	{
		self->NPC->currentAim = aim;

		const int debounce = 500 + (3 - g_spskill->integer) * 100;
		TIMER_Set(self, "aimDebounce", Q_irand(debounce, debounce + 1000));
	}
}