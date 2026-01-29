/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

// g_combat.c

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "anims.h"
#include "objectives.h"
#include "../cgame/cg_local.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "Q3_Interface.h"
#include "g_navigator.h"

constexpr auto TURN_OFF = 0x00000100;

extern qboolean rosh_twin_present();
extern void G_CheckCharmed(gentity_t* self);
extern qboolean Wampa_CheckDropVictim(gentity_t* self, qboolean exclude_me);
extern qboolean rocket_trooper_player(const gentity_t* self);

extern int G_ShipSurfaceForSurfName(const char* surfaceName);
extern qboolean G_FlyVehicleDestroySurface(gentity_t* veh, int surface);
extern void G_VehicleSetDamageLocFlags(gentity_t* veh, int impactDir, int deathPoint);
extern void WP_DeactivateSaber(const gentity_t* self, qboolean clear_length = qfalse);

extern cvar_t* g_debugDamage;
extern qboolean stop_icarus;
extern cvar_t* g_dismemberment;
extern cvar_t* g_gundismemberment;
extern cvar_t* g_saberRealisticCombat;
extern cvar_t* g_saberPickuppableDroppedSabers;
extern cvar_t* g_timescale;
extern cvar_t* d_slowmodeath;
extern gentity_t* player;
extern cvar_t* debug_subdivision;
extern cvar_t* g_dismemberProbabilities;
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern cvar_t* g_broadsword;
gentity_t* g_lastClientDamaged;
extern cvar_t* g_lightningdamage;
extern qboolean jedi_win_po(const gentity_t* self);
extern cvar_t* com_kotor;

extern void NPC_SetPainEvent(gentity_t* self);
extern qboolean PM_FaceProtectAnim(int anim);
extern qboolean Boba_StopKnockdown(gentity_t* self, const gentity_t* pusher, const vec3_t push_dir,
	qboolean force_knockdown = qfalse);
extern qboolean jedi_stop_knockdown(gentity_t* self, const vec3_t push_dir);
extern void jedi_play_blocked_push_sound(const gentity_t* self);

extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern void G_StartRoll(gentity_t* ent, int anim);
extern void WP_ForcePowerStart(gentity_t* self, forcePowers_t force_power, int override_amt);
extern int killPlayerTimer;
extern qboolean jedi_is_kick_resistant(gentity_t* self);
extern void G_MawStagger(gentity_t* hit_ent);
extern void NPC_TempLookTarget(const gentity_t* self, int lookEntNum, int minLookTime, int maxLookTime);
extern void G_AddVoiceEvent(const gentity_t* self, int event, int speak_debounce_time);
extern qboolean PM_HasAnimation(const gentity_t* ent, int animation);
extern qboolean G_TeamEnemy(const gentity_t* self);
extern void CG_ChangeWeapon(int num);
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
qboolean NPC_IsNotDismemberable(const gentity_t* self);
extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legs_anim_timer, int time);
extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torso_anim_timer, int time);
extern int PM_PickAnim(const gentity_t* self, int min_anim, int max_anim);
extern qboolean PM_InOnGroundAnim(playerState_t* ps);
extern void G_ATSTCheckPain(gentity_t* self, gentity_t* other, const vec3_t point, int damage, int mod, int hit_loc);
extern qboolean jedi_waiting_ambush(const gentity_t* self);
extern qboolean G_ClearViewEntity(gentity_t* ent);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern qboolean PM_InRoll(const playerState_t* ps);
extern qboolean PM_SpinningAnim(int anim);
extern qboolean PM_RunningAnim(int anim);
extern int PM_PowerLevelForSaberAnim(const playerState_t* ps, int saber_num = 0);
extern qboolean pm_saber_in_special_attack(int anim);
extern qboolean PM_SpinningSaberAnim(int anim);
extern qboolean PM_FlippingAnim(int anim);
extern qboolean PM_InSpecialJump(int anim);
extern qboolean PM_RollingAnim(int anim);
extern qboolean PM_InAnimForSaberMove(int anim, int saber_move);
extern qboolean PM_SaberInStart(int move);
extern qboolean PM_SaberInReturn(int move);
extern int PM_AnimLength(const int index, const animNumber_t anim);
extern qboolean PM_LockedAnim(int anim);
extern qboolean PM_KnockDownAnim(int anim);
extern void G_SpeechEvent(const gentity_t* self, int event);
extern qboolean rosh_being_healed(const gentity_t* self);
void AddFatigueKillBonus(const gentity_t* attacker, const gentity_t* victim, int means_of_death);
void AddFatigueHurtBonus(const gentity_t* attacker, const gentity_t* victim, int mod);
void AddFatigueHurtBonusMax(const gentity_t* attacker, const gentity_t* victim, int mod);

static int G_CheckForLedge(const gentity_t* self, vec3_t fall_check_dir, float check_dist);
static void G_TrackWeaponUsage(const gentity_t* self, const gentity_t* inflictor, int add, int mod);
static qboolean G_Dismemberable(const gentity_t* self, int hit_loc);
extern gitem_t* FindItemForAmmo(ammo_t ammo);
extern void WP_RemoveSaber(gentity_t* ent, int saber_num);
extern cvar_t* g_SerenityJediEngineMode;
extern cvar_t* g_ffamode;
extern cvar_t* g_Bloodmist;
extern void Jetpack_Off(const gentity_t* ent);
extern void Boba_FlyStop(gentity_t* self);
extern void WP_ForcePowerRegenerate(const gentity_t* self, int override_amt);
extern void WP_BlockPointsRegenerate(const gentity_t* self, int override_amt);
extern void TurnBarrierOff(gentity_t* ent);

qboolean G_GetRootSurfNameWithVariant(gentity_t* ent, const char* root_surf_name, char* return_surf_name,
	int return_size);
/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore(const gentity_t* ent, const int score)
{
	if (!ent->client)
	{
		return;
	}
	// no scoring during pre-match warmup
	ent->client->ps.persistant[PERS_SCORE] += score;
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
extern cvar_t* g_WeaponRemovalTime;

static int WeaponRemovalTime()
{
	int time;

	if (g_WeaponRemovalTime->integer <= 0)
	{
		time = Q3_INFINITE;
	}
	else
	{
		time = g_WeaponRemovalTime->integer * 1000;
	}

	return time;
}

extern cvar_t* g_AllowWeaponDropping;
extern gentity_t* WP_DropThermal(gentity_t* ent);
extern qboolean WP_SaberLose(gentity_t* self, vec3_t throw_dir);

gentity_t* TossClientItems(gentity_t* self)
{
	gentity_t* dropped = nullptr;
	gitem_t* item = nullptr;
	gclient_t* client = self->client;
	const char* info = CG_ConfigString(CS_SERVERINFO);
	const char* s = Info_ValueForKey(info, "mapname");

	if (!g_AllowWeaponDropping->integer)
	{
		return nullptr; // reduce memory use to increase fps
	}

	if (com_kotor->integer == 1)//playing kotor
	{
		return nullptr; // reduce memory use to increase fps
	}

	if (strcmp(s, "md_ga_jedi") == 0
		|| strcmp(s, "md_ga_sith") == 0
		|| strcmp(s, "md_gb_sith") == 0
		|| strcmp(s, "md_gb_jedi") == 0
		|| strcmp(s, "md_arena1") == 0
		|| strcmp(s, "md_arena2") == 0
		|| strcmp(s, "md_arena3") == 0)
	{
		return nullptr; // reduce memory use to increase fps
	}

	if (self->client->charKOTORWeapons == 1)//playing kotor
	{
		return nullptr; // reduce memory use to increase fps
	}

	if (client &&
		(client->NPC_class == CLASS_SEEKER
			|| client->NPC_class == CLASS_REMOTE
			|| client->NPC_class == CLASS_SABER_DROID
			|| client->NPC_class == CLASS_VEHICLE
			|| client->NPC_class == CLASS_SBD
			|| client->NPC_class == CLASS_DROIDEKA
			|| client->NPC_class == CLASS_OBJECT
			|| client->NPC_class == CLASS_ATST))
	{
		// these things are so small that they shouldn't bother throwing anything
		return nullptr;
	}

	// drop the weapon if not a saber or enemy-only weapon
	int weapon = self->s.weapon;

	if (weapon == WP_SABER)
	{
		if (self->weaponModel[0] < 0)
		{
			//don't have one in right hand
			self->s.weapon = WP_NONE;
		}
		else if (g_saberPickuppableDroppedSabers->integer || (client && !(client->ps.saber[0].saberFlags & SFL_NOT_DISARMABLE)))
		{
			//okay to drop it
			if (WP_SaberLose(self, nullptr))
			{
				self->s.weapon = WP_NONE;
			}
		}
		if (g_saberPickuppableDroppedSabers->integer)
		{
			//drop your left one, too
			if (self->weaponModel[1] >= 0)
			{
				//have one in left
				if (client && (!(client->ps.saber[0].saberFlags & SFL_NOT_DISARMABLE) || g_saberPickuppableDroppedSabers->integer))
				{
					//okay to drop it
					//just drop an item
					if (client->ps.saber[1].name
						&& client->ps.saber[1].name[0])
					{
						//have a valid string to use for saberType
						//turn it into a pick-uppable item!
						if (G_DropSaberItem(client->ps.saber[1].name, client->ps.saber[1].blade[0].color,
							client->renderInfo.handLPoint, client->ps.velocity,
							self->currentAngles) != nullptr)
						{
							//dropped it
							WP_RemoveSaber(self, 1);
						}
					}
				}
			}
		}
	}
	else if (weapon == WP_BLASTER_PISTOL)
	{
		//FIXME: either drop the pistol and make the pickup only give ammo or drop ammo
	}
	else if (weapon == WP_STUN_BATON || weapon == WP_MELEE || weapon == WP_DUAL_PISTOL || weapon == WP_DUAL_CLONEPISTOL || weapon == WP_DROIDEKA)
	{
		//never drop these
	}
	else if (weapon > WP_SABER && weapon < WP_NUM_WEAPONS && playerUsableWeapons[weapon])
	{
		self->s.weapon = WP_NONE;

		if (weapon == WP_THERMAL && client && client->ps.torsoAnim == BOTH_ATTACK10)
		{
			//we were getting ready to throw the thermal, drop it!
			client->ps.weaponChargeTime = level.time - FRAMETIME; //so it just kind of drops it
			dropped = WP_DropThermal(self);
		}
		else
		{
			// find the item type for this weapon
			item = FindItemForWeapon(static_cast<weapon_t>(weapon));
		}

		if (item && !dropped)
		{
			dropped = Drop_Item(self, item, 0, qtrue);
			// spawn the item
			dropped->e_ThinkFunc = thinkF_NULL;
			dropped->nextthink = -1;

			if (!self->s.number)
			{
				//player's dropped items never go away
				dropped->count = 0; //no ammo
			}
			else
			{
				//FIXME: base this on the NPC's actual amount of ammo he's used up...
				switch (weapon)
				{
				case WP_BRYAR_PISTOL:
				case WP_BLASTER_PISTOL:
				case WP_SBD_BLASTER:
				case WP_JAWA:
					dropped->count = 20;
					break;
				case WP_BLASTER:
					dropped->count = 15;
					break;
				case WP_DISRUPTOR:
					dropped->count = 20;
					break;
				case WP_BOWCASTER:
					dropped->count = 5;
					break;
				case WP_REPEATER:
					dropped->count = 20;
					break;
				case WP_DEMP2:
					dropped->count = 10;
					break;
				case WP_FLECHETTE:
					dropped->count = 30;
					break;
				case WP_ROCKET_LAUNCHER:
					dropped->count = 3;
					break;
				case WP_CONCUSSION:
					dropped->count = 200;
					break;
				case WP_THERMAL:
					dropped->count = 4;
					break;
				case WP_TRIP_MINE:
					dropped->count = 3;
					break;
				case WP_DET_PACK:
					dropped->count = 1;
					break;
				case WP_STUN_BATON:
					dropped->count = 20;
					break;
				case WP_TUSKEN_RIFLE:
					dropped->count = 20;
					break;
				case WP_NOGHRI_STICK:
					dropped->count = 15;
					break;
				case WP_BATTLEDROID:
					dropped->count = 15;
					break;
				case WP_THEFIRSTORDER:
					dropped->count = 15;
					break;
				case WP_CLONECARBINE:
					dropped->count = 15;
					break;
				case WP_REBELBLASTER:
					dropped->count = 15;
					break;
				case WP_CLONERIFLE:
					dropped->count = 15;
					break;
				case WP_CLONECOMMANDO:
				case WP_WRIST_BLASTER:
					dropped->count = 15;
					break;
				case WP_Z6_ROTARY_CANNON:
					dropped->count = 100;
					break;
				case WP_REBELRIFLE:
					dropped->count = 15;
					break;
				case WP_REY:
					dropped->count = 15;
					break;
				case WP_JANGO:
					dropped->count = 15;
					break;
				case WP_BOBA:
					dropped->count = 15;
					break;
				case WP_CLONEPISTOL:
					dropped->count = 15;
					break;
				default:
					dropped->count = 0;
					break;
				}
			}
			// well, dropped weapons are G2 models, so they have to be initialised if they want to draw..give us a radius so we don't get prematurely culled
			if (weapon != WP_THERMAL
				&& weapon != WP_TRIP_MINE
				&& weapon != WP_DET_PACK)
			{
				gi.G2API_InitGhoul2Model(dropped->ghoul2, item->world_model, G_ModelIndex(item->world_model),
					NULL_HANDLE, NULL_HANDLE, 0, 0);
				dropped->s.radius = 10;
			}
		}
	}
	else if (client && client->NPC_class == CLASS_MARK1)
	{
		if (Q_irand(1, 2) > 1)
		{
			item = FindItemForAmmo(AMMO_METAL_BOLTS);
		}
		else
		{
			item = FindItemForAmmo(AMMO_BLASTER);
		}
		Drop_Item(self, item, 0, qtrue);
	}
	else if (client && client->NPC_class == CLASS_MARK2)
	{
		if (Q_irand(1, 2) > 1)
		{
			item = FindItemForAmmo(AMMO_METAL_BOLTS);
		}
		else
		{
			item = FindItemForAmmo(AMMO_POWERCELL);
		}
		Drop_Item(self, item, 0, qtrue);
	}

	return dropped; //NOTE: presumes only drop one thing
}

static void G_DropKey(gentity_t* self)
{
	//drop whatever security key I was holding
	gitem_t* item;
	if (!Q_stricmp("goodie", self->message))
	{
		item = FindItemForInventory(INV_GOODIE_KEY);
	}
	else
	{
		item = FindItemForInventory(INV_SECURITY_KEY);
	}
	gentity_t* dropped = Drop_Item(self, item, 0, qtrue);
	//Don't throw the key
	VectorClear(dropped->s.pos.trDelta);
	dropped->message = self->message;
	self->message = nullptr;
}

void ObjectDie(gentity_t* self, gentity_t* attacker)
{
	if (self->target)
		G_UseTargets(self, attacker);

	//remove my script_targetname
	G_FreeEntity(self);
}

/*
==================
ExplodeDeath
==================
*/

//FIXME: all hacked up...

void ExplodeDeath(gentity_t* self)
{
	//	gentity_t	*tent;
	vec3_t forward;

	self->takedamage = qfalse; //stop chain reaction runaway loops

	self->s.loopSound = 0;

	VectorCopy(self->currentOrigin, self->s.pos.trBase);

	//	tent = G_TempEntity( self->s.origin, EV_FX_EXPLOSION );
	AngleVectors(self->s.angles, forward, nullptr, nullptr); // FIXME: letting effect always shoot up?  Might be ok.

	if (self->fxID > 0)
	{
		G_PlayEffect(self->fxID, self->currentOrigin, forward);
	}

	if (self->splashDamage > 0 && self->splashRadius > 0)
	{
		gentity_t* attacker = self;
		if (self->owner)
		{
			attacker = self->owner;
		}
		G_RadiusDamage(self->currentOrigin, attacker, self->splashDamage, self->splashRadius,
			attacker, MOD_UNKNOWN);
	}

	ObjectDie(self, self);
}

void ExplodeDeath_Wait(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod,
	int d_flags, int hit_loc)
{
	self->e_DieFunc = dieF_NULL;
	self->nextthink = level.time + Q_irand(100, 500);
	self->e_ThinkFunc = thinkF_ExplodeDeath;
}

void ExplodeDeath(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int meansOfDeath, int d_flags,
	int hit_loc)
{
	self->currentOrigin[2] += 16;
	// me bad for hacking this.  should either do it in the effect file or make a custom explode death??
	ExplodeDeath(self);
}

void GoExplodeDeath(gentity_t* self, gentity_t* other, gentity_t* activator)
{
	G_ActivateBehavior(self, BSET_USE);

	self->targetname = nullptr; //Make sure this entity cannot be told to explode again (recursive death fix)

	ExplodeDeath(self);
}

qboolean G_ActivateBehavior(gentity_t* self, int bset);

static void G_CheckVictoryScript(gentity_t* self)
{
	if (!G_ActivateBehavior(self, BSET_VICTORY))
	{
		if (self->NPC && self->s.weapon == WP_SABER)
		{
			//Jedi taunt from within their AI
			self->NPC->blockedSpeechDebounceTime = 0; //get them ready to taunt
			return;
		}
		if (self->client && self->client->NPC_class == CLASS_GALAKMECH)
		{
			self->wait = 1;
			TIMER_Set(self, "gloatTime", Q_irand(5000, 8000));
			self->NPC->blockedSpeechDebounceTime = 0; //get him ready to taunt
			return;
		}
		//FIXME: any way to not say this *right away*?  Wait for victim's death anim/scream to finish?
		if (self->NPC && self->NPC->group && self->NPC->group->commander && self->NPC->group->commander->NPC && self->
			NPC->group->commander->NPC->rank > self->NPC->rank && !Q_irand(0, 2))
		{
			//sometimes have the group commander speak instead
			self->NPC->group->commander->NPC->greetingDebounceTime = level.time + Q_irand(2000, 5000);
		}
		else if (self->NPC)
		{
			self->NPC->greetingDebounceTime = level.time + Q_irand(2000, 5000);
		}
	}
}

void G_CheckAlmostVictoryScript(gentity_t* self)
{
	if (!G_ActivateBehavior(self, UNDYINGPLAYERVICTORY))
	{
		if (self->NPC && self->s.weapon == WP_SABER)
		{
			//Jedi taunt from within their AI
			self->NPC->blockedSpeechDebounceTime = 0; //get them ready to taunt
			return;
		}
		if (self->client && self->client->NPC_class == CLASS_GALAKMECH)
		{
			self->wait = 1;
			TIMER_Set(self, "gloatTime", Q_irand(5000, 8000));
			self->NPC->blockedSpeechDebounceTime = 0; //get him ready to taunt
			return;
		}
		//FIXME: any way to not say this *right away*?  Wait for victim's death anim/scream to finish?
		if (self->NPC && self->NPC->group && self->NPC->group->commander && self->NPC->group->commander->NPC && self->
			NPC->group->commander->NPC->rank > self->NPC->rank && !Q_irand(0, 2))
		{
			//sometimes have the group commander speak instead
			self->NPC->group->commander->NPC->greetingDebounceTime = level.time + Q_irand(2000, 5000);
		}
		else if (self->NPC)
		{
			self->NPC->greetingDebounceTime = level.time + Q_irand(2000, 5000);
		}
	}
}

//ME             //ENEMY
qboolean OnSameTeam(const gentity_t* ent1, const gentity_t* ent2)
{
	if (ent1->s.number < MAX_CLIENTS
		&& ent1->client
		&& ent1->client->playerTeam == TEAM_FREE)
	{
		//evil player *has* no allies
		return qfalse;
	}
	if (ent2->s.number < MAX_CLIENTS
		&& ent2->client
		&& ent2->client->playerTeam == TEAM_FREE)
	{
		//evil player *has* no allies
		return qfalse;
	}

	if (!ent1->client || !ent2->client)
	{
		if (ent1->noDamageTeam)
		{
			if (ent2->client && ent2->client->playerTeam == ent1->noDamageTeam)
			{
				return qtrue;
			}
			if (ent2->noDamageTeam == ent1->noDamageTeam)
			{
				if (ent1->splashDamage && ent2->splashDamage && Q_stricmp("ambient_etherian_fliers", ent1->classname) !=
					0)
				{
					//Barrels, exploding breakables and mines will blow each other up
					return qfalse;
				}
				return qtrue;
			}
		}
		return qfalse;
	}

	return static_cast<qboolean>(ent1->client->playerTeam == ent2->client->playerTeam);
}

/*
-------------------------
G_AlertTeam
-------------------------
*/

void G_AlertTeam(const gentity_t* victim, gentity_t* attacker, const float radius, const float sound_dist)
{
	gentity_t* radius_ents[128];
	vec3_t mins{}, maxs{};
	int i;
	const float snd_dist_sq = sound_dist * sound_dist;

	if (attacker == nullptr || attacker->client == nullptr)
		return;

	//Setup the bbox to search in
	for (i = 0; i < 3; i++)
	{
		mins[i] = victim->currentOrigin[i] - radius;
		maxs[i] = victim->currentOrigin[i] + radius;
	}

	//Get the number of entities in a given space
	const int num_ents = gi.EntitiesInBox(mins, maxs, radius_ents, 128);

	//Cull this list
	for (i = 0; i < num_ents; i++)
	{
		//Validate clients
		if (radius_ents[i]->client == nullptr)
			continue;

		//only want NPCs
		if (radius_ents[i]->NPC == nullptr)
			continue;

		//Don't bother if they're ignoring enemies
		if (radius_ents[i]->svFlags & SVF_IGNORE_ENEMIES)
			continue;

		//This NPC specifically flagged to ignore alerts
		if (radius_ents[i]->NPC->scriptFlags & SCF_IGNORE_ALERTS)
			continue;

		//This NPC specifically flagged to ignore alerts
		if (!(radius_ents[i]->NPC->scriptFlags & SCF_LOOK_FOR_ENEMIES))
			continue;

		//this ent does not participate in group AI
		if (radius_ents[i]->NPC->scriptFlags & SCF_NO_GROUPS)
			continue;

		//Skip the requested avoid radius_ents[i] if present
		if (radius_ents[i] == victim)
			continue;

		//Skip the attacker
		if (radius_ents[i] == attacker)
			continue;

		//Must be on the same team
		if (radius_ents[i]->client->playerTeam != victim->client->playerTeam)
			continue;

		//Must be alive
		if (radius_ents[i]->health <= 0)
			continue;

		if (radius_ents[i]->enemy == nullptr)
		{
			//only do this if they're not already mad at someone
			const float dist_sq = DistanceSquared(radius_ents[i]->currentOrigin, victim->currentOrigin);
			if (dist_sq > 16384 /*128 squared*/ && !gi.inPVS(victim->currentOrigin, radius_ents[i]->currentOrigin))
			{
				//not even potentially visible/hearable
				continue;
			}
			//NOTE: this allows sound alerts to still go through doors/PVS if the teammate is within 128 of the victim...
			if (sound_dist <= 0 || dist_sq > snd_dist_sq)
			{
				//out of sound range
				if (!InFOV(victim, radius_ents[i], radius_ents[i]->NPC->stats.hfov, radius_ents[i]->NPC->stats.vfov)
					|| !NPC_ClearLOS(radius_ents[i], victim->currentOrigin))
				{
					//out of FOV or no LOS
					continue;
				}
			}

			//FIXME: This can have a nasty cascading effect if setup wrong...
			G_SetEnemy(radius_ents[i], attacker);
		}
	}
}

/*
-------------------------
G_DeathAlert
-------------------------
*/

constexpr auto DEATH_ALERT_RADIUS = 512;
constexpr auto DEATH_ALERT_SOUND_RADIUS = 512;

static void G_DeathAlert(const gentity_t* victim, gentity_t* attacker)
{
	//FIXME: with all the other alert stuff, do we really need this?
	G_AlertTeam(victim, attacker, DEATH_ALERT_RADIUS, DEATH_ALERT_SOUND_RADIUS);
}

/*
----------------------------------------
DeathFX

Applies appropriate special effects that occur while the entity is dying
Not to be confused with NPC_RemoveBodyEffects (NPC.cpp), which only applies effect when removing the body
----------------------------------------
*/

extern qboolean jedi_cultist_destroyer(const gentity_t* self);

void DeathFX(const gentity_t* ent)
{
	if (!ent || !ent->client)
	{
		return;
	}

	vec3_t effect_pos, right;

	if (ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER &&
		ent->client && ent->client->ps.m_iVehicleNum)
	{
		//go into "die in ship" mode with flag
	}

	if (jedi_cultist_destroyer(NPC))
	{
		//destroyer
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 10, right, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/cultist1/misc/anger1");
	}

	if (ent && !Q_stricmp("cultist_destroyer", ent->NPC_type))
	{
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 10, right, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/cultist1/misc/anger1");
	}

	switch (ent->client->NPC_class)
	{
	case CLASS_MOUSE:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] -= 20;
		G_PlayEffect("env/small_explode", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/mouse/misc/death1");
		break;

	case CLASS_PROBE:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] += 50;
		G_PlayEffect("explosions/probeexplosion1", effect_pos);
		break;

	case CLASS_ATST:
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 20, right, effect_pos);
		effect_pos[2] += 180;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -40, right, effect_pos);
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		break;

	case CLASS_SEEKER:
	case CLASS_REMOTE:
		G_PlayEffect("env/small_explode", ent->currentOrigin);
		break;

	case CLASS_GONK:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] -= 5;
		G_SoundOnEnt(ent, CHAN_AUTO, va("sound/chars/gonk/misc/death%d.wav", Q_irand(1, 3)));
		G_PlayEffect("env/med_explode", effect_pos);
		break;

		// should list all remaining droids here, hope I didn't miss any
	case CLASS_R2D2:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] -= 10;
		G_PlayEffect("env/med_explode", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/mark2/misc/mark2_explo");
		break;

	case CLASS_PROTOCOL: //??
	case CLASS_R5D2:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] -= 10;
		G_PlayEffect("env/med_explode", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/mark2/misc/mark2_explo");
		break;

	case CLASS_MARK2:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/mark2/misc/mark2_explo");
		break;

	case CLASS_INTERROGATOR:
		VectorCopy(ent->currentOrigin, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/interrogator/misc/int_droid_explo");
		break;

	case CLASS_MARK1:
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 10, right, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/mark1/misc/mark1_explo");
		break;

	case CLASS_SENTRY:
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/sentry/misc/sentry_explo");
		VectorCopy(ent->currentOrigin, effect_pos);
		G_PlayEffect("env/med_explode", effect_pos);
		break;

	case CLASS_BOBAFETT:
	case CLASS_MANDALORIAN:
	case CLASS_JANGO:
	case CLASS_JANGODUAL:
	case CLASS_ROCKETTROOPER:
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 10, right, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/mark1/misc/mark1_explo");
		break;

	case CLASS_SBD:
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/SBD/misc/death2");
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 10, right, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("env/small_explode", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		break;

	case CLASS_ASSASSIN_DROID:
	case CLASS_HAZARD_TROOPER:
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/battledroid/misc/death3");
		AngleVectors(ent->currentAngles, nullptr, right, nullptr);
		VectorMA(ent->currentOrigin, 10, right, effect_pos);
		effect_pos[2] -= 15;
		G_PlayEffect("explosions/droidexplosion1", effect_pos);
		VectorMA(effect_pos, -20, right, effect_pos);
		break;

	case CLASS_BATTLEDROID:
		VectorCopy(ent->currentOrigin, effect_pos);
		G_PlayEffect("env/small_explode", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/battledroid/misc/death2");
		break;

	case CLASS_DROIDEKA:
		VectorCopy(ent->currentOrigin, effect_pos);
		G_PlayEffect("env/small_explode", effect_pos);
		G_SoundOnEnt(ent, CHAN_AUTO, "sound/chars/droideka/foldin");
		break;

	default:
		break;
	}
}

static void G_SetMissionStatusText(const gentity_t* attacker, const int mod)
{
	if (statusTextIndex >= 0)
	{
		return;
	}

	if (mod == MOD_FALLING)
	{
		//fell to your death
		statusTextIndex = STAT_WATCHYOURSTEP;
	}
	else if (mod == MOD_CRUSH)
	{
		//crushed
		statusTextIndex = STAT_JUDGEMENTMUCHDESIRED;
	}
	else if (attacker && Q_stricmp("trigger_hurt", attacker->classname) == 0)
	{
		//Killed by something that should have been clearly dangerous
		//		statusTextIndex = Q_irand( IGT_JUDGEMENTDESIRED, IGT_JUDGEMENTMUCHDESIRED );
		statusTextIndex = STAT_JUDGEMENTMUCHDESIRED;
	}
	else if (attacker && attacker->s.number != 0 && attacker->client && attacker->client->playerTeam == TEAM_PLAYER)
	{
		//killed by a teammate
		statusTextIndex = STAT_INSUBORDINATION;
	}
}

void G_MakeTeamVulnerable()
{
	const gentity_t* self = &g_entities[0];
	if (!self->client)
	{
		return;
	}
	for (int i = 0; i < globals.num_entities; i++)
	{
		if (!PInUse(i))
			continue;
		gentity_t* ent = &g_entities[i];
		if (!ent->client)
		{
			continue;
		}
		if (ent->client->playerTeam != TEAM_PLAYER)
		{
			continue;
		}
		if (!(ent->flags & FL_UNDYING))
		{
			continue;
		}
		ent->flags &= ~FL_UNDYING;
		const int newhealth = Q_irand(5, 40);
		if (ent->health > newhealth)
		{
			ent->health = newhealth;
		}
	}
}

void G_StartMatrixEffect(const gentity_t* ent, const int me_flags = 0, const int length = 1000,
	const float time_scale = 0.0f, const int spin_time = 0)
{
	//FIXME: allow them to specify a different focal entity or point?
	if (g_timescale->value != 1.0 || in_camera)
	{
		//already in some slow-mo mode or in_camera
		return;
	}

	gentity_t* matrix = G_Spawn();
	if (matrix)
	{
		G_SetOrigin(matrix, ent->currentOrigin);
		gi.linkentity(matrix);
		matrix->s.otherentity_num = ent->s.number;
		matrix->e_clThinkFunc = clThinkF_CG_MatrixEffect;
		matrix->s.eType = ET_THINKER;
		matrix->svFlags |= SVF_BROADCAST; // Broadcast to all clients
		matrix->s.time = level.time;
		matrix->s.eventParm = length;
		matrix->s.boltInfo = me_flags;
		matrix->s.time2 = spin_time;
		matrix->s.angles2[0] = time_scale;
	}
}

void G_StartStasisEffect_FORCE_LEVEL_1(const gentity_t* ent, const int me_flags = 0, const int length = 1000,
	const float time_scale = 0.0f,
	const int spin_time = 0)
{
	if (g_timescale->value != 1.0 || in_camera)
	{
		//already in some slow-mo mode or in_camera
		return;
	}

	gentity_t* stasis = G_Spawn();
	if (stasis)
	{
		G_SetOrigin(stasis, ent->currentOrigin);
		gi.linkentity(stasis);
		stasis->s.otherentity_num = ent->s.number;
		stasis->e_clThinkFunc = clThinkF_CG_StasisEffect;
		stasis->s.eType = ET_THINKER;
		stasis->svFlags |= SVF_BROADCAST; // Broadcast to all clients
		stasis->s.time = level.time;
		stasis->s.eventParm = length;
		stasis->s.boltInfo = me_flags;
		stasis->s.time2 = spin_time;
		stasis->s.angles2[0] = time_scale;
	}
}

void G_StartStasisEffect_FORCE_LEVEL_2(const gentity_t* ent, const int me_flags = 0, const int length = 1000,
	const float time_scale = 0.0f,
	const int spin_time = 0)
{
	if (g_timescale->value != 1.0 || in_camera)
	{
		//already in some slow-mo mode or in_camera
		return;
	}

	gentity_t* stasis = G_Spawn();
	if (stasis)
	{
		G_SetOrigin(stasis, ent->currentOrigin);
		gi.linkentity(stasis);
		stasis->s.otherentity_num = ent->s.number;
		stasis->e_clThinkFunc = clThinkF_CG_StasisEffectLong;
		stasis->s.eType = ET_THINKER;
		stasis->svFlags |= SVF_BROADCAST; // Broadcast to all clients
		stasis->s.time = level.time;
		stasis->s.eventParm = length;
		stasis->s.boltInfo = me_flags;
		stasis->s.time2 = spin_time;
		stasis->s.angles2[0] = time_scale;
	}
}

static qhandle_t ItemActivateSound = 0;
extern cvar_t* g_IconBackgroundSlow;

void G_StartNextItemEffect(gentity_t* ent, const int me_flags = 0, const int length = 1000, const float time_scale = 0.0f, const int spin_time = 0)
{
	static qboolean registered = qfalse;

	if (!registered)
	{
		ItemActivateSound = G_SoundIndex("sound/effects/Iconbackground.mp3");
		registered = qtrue;
	}
	if (g_timescale->value != 1.0 || in_camera)
	{
		//already in some slow-mo mode or in_camera
		return;
	}

	if (!g_IconBackgroundSlow->integer)
	{
		//already in some slow-mo mode or in_camera
		return;
	}

	gentity_t* stasis = G_Spawn();
	if (stasis)
	{
		G_SetOrigin(stasis, ent->currentOrigin);
		gi.linkentity(stasis);
		stasis->s.otherentity_num = ent->s.number;
		stasis->e_clThinkFunc = clThinkF_CG_StasisEffect;
		stasis->s.eType = ET_THINKER;
		stasis->svFlags |= SVF_BROADCAST; // Broadcast to all clients
		stasis->s.time = level.time;
		stasis->s.eventParm = length;
		stasis->s.boltInfo = me_flags;
		stasis->s.time2 = spin_time;
		stasis->s.angles2[0] = time_scale;
		G_AddEvent(ent, EV_GENERAL_SOUND, ItemActivateSound);
	}
}

static qboolean G_JediInRoom(vec3_t from)
{
	for (int i = 1; i < globals.num_entities; i++)
	{
		if (!PInUse(i))
			continue;

		const gentity_t* ent = &g_entities[i];

		if (!ent->NPC)
		{
			continue;
		}
		if (ent->health <= 0)
		{
			continue;
		}
		if (ent->s.eFlags & EF_NODRAW)
		{
			continue;
		}
		if (ent->s.weapon != WP_SABER)
		{
			continue;
		}
		if (!gi.inPVS(ent->currentOrigin, from))
		{
			continue;
		}
		return qtrue;
	}
	return qfalse;
}

qboolean G_GetHitLocFromSurfName(gentity_t* ent, const char* surf_name, int* hit_loc, vec3_t point, vec3_t dir,
	vec3_t blade_dir, const int mod, const saberType_t saber_type)
{
	qboolean dismember = qfalse;

	*hit_loc = HL_NONE;

	if (!surf_name || !surf_name[0])
	{
		return qfalse;
	}

	if (!ent->client)
	{
		return qfalse;
	}

	if (ent->client
		&& (ent->client->NPC_class == CLASS_R2D2
			|| ent->client->NPC_class == CLASS_R5D2
			|| ent->client->NPC_class == CLASS_GONK
			|| ent->client->NPC_class == CLASS_MOUSE
			|| ent->client->NPC_class == CLASS_SEEKER
			|| ent->client->NPC_class == CLASS_INTERROGATOR
			|| ent->client->NPC_class == CLASS_SENTRY
			|| ent->client->NPC_class == CLASS_PROBE
			|| ent->client->NPC_class == CLASS_SBD
			|| ent->client->NPC_class == CLASS_DROIDEKA
			|| ent->client->NPC_class == CLASS_PROJECTION))
	{
		//we don't care about per-surface hit-locations or dismemberment for these guys
		return qfalse;
	}

	if (!Q_stricmp("md_grie_egg", ent->NPC_type)
		|| !Q_stricmp("md_grie2_egg", ent->NPC_type)
		|| !Q_stricmp("md_grie3_egg", ent->NPC_type)
		|| !Q_stricmp("md_grie4_egg", ent->NPC_type)
		|| !Q_stricmp("md_grievous", ent->NPC_type)
		|| !Q_stricmp("md_grievous4", ent->NPC_type)
		|| !Q_stricmp("md_grievous_robed", ent->NPC_type)
		|| !Q_stricmp("md_dro_am", ent->NPC_type))
	{
		//we don't care about per-surface hit-locations or dismemberment for these guys
		return qfalse;
	}

	if (ent->client && ent->client->NPC_class == CLASS_ATST)
	{
		//FIXME: almost impossible to hit these... perhaps we should
		//		check for splashDamage and do radius damage to these parts?
		//		Or, if we ever get bbox G2 traces, that may fix it, too
		if (!Q_stricmp("head_light_blaster_cann", surf_name))
		{
			*hit_loc = HL_ARM_LT;
		}
		else if (!Q_stricmp("head_concussion_charger", surf_name))
		{
			*hit_loc = HL_ARM_RT;
		}
		return qfalse;
	}
	if (ent->client && ent->client->NPC_class == CLASS_MARK1)
	{
		if (!Q_stricmp("l_arm", surf_name))
		{
			*hit_loc = HL_ARM_LT;
		}
		else if (!Q_stricmp("r_arm", surf_name))
		{
			*hit_loc = HL_ARM_RT;
		}
		else if (!Q_stricmp("torso_front", surf_name))
		{
			*hit_loc = HL_CHEST;
		}
		else if (!Q_stricmp("torso_tube1", surf_name))
		{
			*hit_loc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_tube2", surf_name))
		{
			*hit_loc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_tube3", surf_name))
		{
			*hit_loc = HL_GENERIC3;
		}
		else if (!Q_stricmp("torso_tube4", surf_name))
		{
			*hit_loc = HL_GENERIC4;
		}
		else if (!Q_stricmp("torso_tube5", surf_name))
		{
			*hit_loc = HL_GENERIC5;
		}
		else if (!Q_stricmp("torso_tube6", surf_name))
		{
			*hit_loc = HL_GENERIC6;
		}
		return qfalse;
	}
	if (ent->client && ent->client->NPC_class == CLASS_MARK2)
	{
		if (!Q_stricmp("torso_canister1", surf_name))
		{
			*hit_loc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_canister2", surf_name))
		{
			*hit_loc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_canister3", surf_name))
		{
			*hit_loc = HL_GENERIC3;
		}
		return qfalse;
	}
	if (ent->client && ent->client->NPC_class == CLASS_GALAKMECH)
	{
		if (!Q_stricmp("torso_antenna", surf_name) || !Q_stricmp("torso_antenna_base", surf_name))
		{
			*hit_loc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_shield", surf_name))
		{
			*hit_loc = HL_GENERIC2;
		}
		else
		{
			*hit_loc = HL_CHEST;
		}
		return qfalse;
	}

	//FIXME: check the hit_loc and hitDir against the cap tag for the place
	//where the split will be- if the hit dir is roughly perpendicular to
	//the direction of the cap, then the split is allowed, otherwise we
	//hit it at the wrong angle and should not dismember...
	const int actual_time = cg.time ? cg.time : level.time;
	if (!Q_stricmpn("hips", surf_name, 4))
	{
		//FIXME: test properly for legs
		*hit_loc = HL_WAIST;
		if (ent->client != nullptr && ent->ghoul2.size())
		{
			mdxaBone_t bolt_matrix;
			vec3_t tag_org, angles;

			VectorSet(angles, 0, ent->currentAngles[YAW], 0);
			if (ent->kneeLBolt >= 0)
			{
				gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->kneeLBolt,
					&bolt_matrix, angles, ent->currentOrigin,
					actual_time, nullptr, ent->s.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
				if (DistanceSquared(point, tag_org) < 100)
				{
					//actually hit the knee
					*hit_loc = HL_LEG_LT;
				}
			}
			if (*hit_loc == HL_WAIST)
			{
				if (ent->kneeRBolt >= 0)
				{
					gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->kneeRBolt,
						&bolt_matrix, angles, ent->currentOrigin,
						actual_time, nullptr, ent->s.modelScale);
					gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
					if (DistanceSquared(point, tag_org) < 100)
					{
						//actually hit the knee
						*hit_loc = HL_LEG_RT;
					}
				}
			}
		}
	}
	else if (!Q_stricmpn("torso", surf_name, 5))
	{
		if (!ent->client)
		{
			*hit_loc = HL_CHEST;
		}
		else
		{
			vec3_t t_fwd, t_rt, t_up, dir_to_impact;
			AngleVectors(ent->client->renderInfo.torsoAngles, t_fwd, t_rt, t_up);
			VectorSubtract(point, ent->client->renderInfo.torsoPoint, dir_to_impact);
			const float front_side = DotProduct(t_fwd, dir_to_impact);
			const float right_side = DotProduct(t_rt, dir_to_impact);
			const float up_side = DotProduct(t_up, dir_to_impact);
			if (up_side < -10)
			{
				//hit at waist
				*hit_loc = HL_WAIST;
			}
			else
			{
				//hit on upper torso
				if (right_side > 4)
				{
					*hit_loc = HL_ARM_RT;
				}
				else if (right_side < -4)
				{
					*hit_loc = HL_ARM_LT;
				}
				else if (right_side > 2)
				{
					if (front_side > 0)
					{
						*hit_loc = HL_CHEST_RT;
					}
					else
					{
						*hit_loc = HL_BACK_RT;
					}
				}
				else if (right_side < -2)
				{
					if (front_side > 0)
					{
						*hit_loc = HL_CHEST_LT;
					}
					else
					{
						*hit_loc = HL_BACK_LT;
					}
				}
				else if (up_side > -3 && mod == MOD_SABER)
				{
					*hit_loc = HL_HEAD;
				}
				else if (front_side > 0)
				{
					*hit_loc = HL_CHEST;
				}
				else
				{
					*hit_loc = HL_BACK;
				}
			}
		}
	}
	else if (!Q_stricmpn("head", surf_name, 4))
	{
		*hit_loc = HL_HEAD;
	}
	else if (!Q_stricmpn("r_arm", surf_name, 5))
	{
		*hit_loc = HL_ARM_RT;
		if (ent->client != nullptr && ent->ghoul2.size())
		{
			mdxaBone_t bolt_matrix;
			vec3_t angles;

			VectorSet(angles, 0, ent->currentAngles[YAW], 0);
			if (ent->handRBolt >= 0)
			{
				vec3_t tag_org;
				gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->handRBolt,
					&bolt_matrix, angles, ent->currentOrigin,
					actual_time, nullptr, ent->s.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
				if (DistanceSquared(point, tag_org) < 256)
				{
					//actually hit the hand
					*hit_loc = HL_HAND_RT;
				}
			}
		}
	}
	else if (!Q_stricmpn("l_arm", surf_name, 5))
	{
		*hit_loc = HL_ARM_LT;
		if (ent->client != nullptr && ent->ghoul2.size())
		{
			mdxaBone_t bolt_matrix;
			vec3_t angles;

			VectorSet(angles, 0, ent->currentAngles[YAW], 0);
			if (ent->handLBolt >= 0)
			{
				vec3_t tag_org;
				gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->handLBolt,
					&bolt_matrix, angles, ent->currentOrigin,
					actual_time, nullptr, ent->s.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
				if (DistanceSquared(point, tag_org) < 256)
				{
					//actually hit the hand
					*hit_loc = HL_HAND_LT;
				}
			}
		}
	}
	else if (!Q_stricmpn("r_leg", surf_name, 5))
	{
		*hit_loc = HL_LEG_RT;
		if (ent->client != nullptr && ent->ghoul2.size())
		{
			mdxaBone_t bolt_matrix;
			vec3_t angles;

			VectorSet(angles, 0, ent->currentAngles[YAW], 0);
			if (ent->footRBolt >= 0)
			{
				vec3_t tag_org;
				gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->footRBolt,
					&bolt_matrix, angles, ent->currentOrigin,
					actual_time, nullptr, ent->s.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
				if (DistanceSquared(point, tag_org) < 100)
				{
					//actually hit the foot
					*hit_loc = HL_FOOT_RT;
				}
			}
		}
	}
	else if (!Q_stricmpn("l_leg", surf_name, 5))
	{
		*hit_loc = HL_LEG_LT;
		if (ent->client != nullptr && ent->ghoul2.size())
		{
			mdxaBone_t bolt_matrix;
			vec3_t angles;

			VectorSet(angles, 0, ent->currentAngles[YAW], 0);
			if (ent->footLBolt >= 0)
			{
				vec3_t tag_org;
				gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->footLBolt,
					&bolt_matrix, angles, ent->currentOrigin,
					actual_time, nullptr, ent->s.modelScale);
				gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
				if (DistanceSquared(point, tag_org) < 100)
				{
					//actually hit the foot
					*hit_loc = HL_FOOT_LT;
				}
			}
		}
	}
	else if (mod == MOD_SABER && WP_BreakSaber(ent, surf_name, saber_type))
	{
		//saber hit and broken
		*hit_loc = HL_HAND_RT;
	}
	else if (!Q_stricmpn("r_hand", surf_name, 6) || !Q_stricmpn("w_", surf_name, 2))
	{
		//right hand or weapon
		//FIXME: if hit weapon, chance of breaking saber (if sabers.cfg entry shows it as breakable)
		//			if breaks, remove saber and replace with the 2 replacement sabers (preserve color, length, etc.)
		*hit_loc = HL_HAND_RT;
	}
	else if (!Q_stricmpn("l_hand", surf_name, 6))
	{
		*hit_loc = HL_HAND_LT;
	}
	else if (ent->client && ent->client->ps.powerups[PW_GALAK_SHIELD] && !Q_stricmp("force_shield", surf_name))
	{
		*hit_loc = HL_GENERIC2;
	}
#ifdef _DEBUG
	else
	{
		//Com_Printf( "ERROR: surface %s does not belong to any hitLocation!!!\n", surfName );
	}
#endif //_DEBUG

	if (g_saberRealisticCombat->integer > 1
		|| debug_subdivision->integer)
	{
		dismember = qtrue;
	}
	else if (ent->client &&
		(ent->client->NPC_class == CLASS_PROTOCOL ||
			ent->client->NPC_class == CLASS_ASSASSIN_DROID ||
			ent->client->NPC_class == CLASS_DROIDEKA ||
			ent->client->NPC_class == CLASS_SABER_DROID))
	{
		dismember = qtrue;
	}
	else if (debug_subdivision->integer || !ent->client->dismembered)
	{
		if (dir && (dir[0] || dir[1] || dir[2]) &&
			blade_dir && (blade_dir[0] || blade_dir[1] || blade_dir[2]))
		{
			//we care about direction (presumably for dismemberment)
			if (g_dismemberProbabilities->value <= 0.0f || G_Dismemberable(ent, *hit_loc))
			{
				//the probability let us continue
				const char* tag_name = nullptr;
				float aoa = 0.5f;
				//dir must be roughly perpendicular to the hit_loc's cap bolt
				switch (*hit_loc)
				{
				case HL_LEG_RT:
					tag_name = "*hips_cap_r_leg";
					break;
				case HL_LEG_LT:
					tag_name = "*hips_cap_l_leg";
					break;
				case HL_WAIST:
					tag_name = "*hips_cap_torso";
					aoa = 0.25f;
					break;
				case HL_CHEST_RT:
				case HL_ARM_RT:
				case HL_BACK_LT:
					tag_name = "*torso_cap_r_arm";
					break;
				case HL_CHEST_LT:
				case HL_ARM_LT:
				case HL_BACK_RT:
					tag_name = "*torso_cap_l_arm";
					break;
				case HL_HAND_RT:
					tag_name = "*r_arm_cap_r_hand";
					break;
				case HL_HAND_LT:
					tag_name = "*l_arm_cap_l_hand";
					break;
				case HL_HEAD:
					tag_name = "*torso_cap_head";
					aoa = 0.25f;
					break;
				case HL_CHEST:
				case HL_BACK:
				case HL_FOOT_RT:
				case HL_FOOT_LT:
				default:
					//no dismemberment possible with these, so no checks needed
					break;
				}
				if (tag_name)
				{
					const int tag_bolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], tag_name);
					if (tag_bolt != -1)
					{
						mdxaBone_t bolt_matrix;
						vec3_t tag_org, tag_dir, angles;
						VectorSet(angles, 0, ent->currentAngles[YAW], 0);
						gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, tag_bolt,
							&bolt_matrix, angles, ent->currentOrigin,
							actual_time, nullptr, ent->s.modelScale);
						gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, tag_org);
						gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, tag_dir);
						if (DistanceSquared(point, tag_org) < 256)
						{
							//hit close
							float dot = DotProduct(dir, tag_dir);
							if (dot < aoa && dot > -aoa)
							{
								//hit roughly perpendicular
								dot = DotProduct(blade_dir, tag_dir);
								if (dot < aoa && dot > -aoa)
								{
									//blade was roughly perpendicular
									dismember = qtrue;
								}
							}
						}
					}
				}
			}
		}
	}
	return dismember;
}

static int G_GetHitLocation(const gentity_t* target, const vec3_t ppoint)
{
	vec3_t point, point_dir;
	vec3_t forward, right, up;
	vec3_t tangles, tcenter;
	int vertical;
	int Forward;
	int lateral;

	//get target forward, right and up
	if (target->client)
	{
		//ignore player's pitch and roll
		VectorSet(tangles, 0, target->currentAngles[YAW], 0);
	}

	AngleVectors(tangles, forward, right, up);

	//get center of target
	VectorAdd(target->absmin, target->absmax, tcenter);
	VectorScale(tcenter, 0.5, tcenter);

	//get impact point
	if (ppoint && !VectorCompare(ppoint, vec3_origin))
	{
		VectorCopy(ppoint, point);
	}
	else
	{
		return HL_NONE;
	}

	/*
	//get impact dir
		if(pdir && !VectorCompare(pdir, vec3_origin))
		{
			VectorCopy(pdir, dir);
		}
		else
		{
			return;
		}

	//put point at controlled distance from center
		VectorSubtract(point, tcenter, tempvec);
		tempvec[2] = 0;
		hdist = VectorLength(tempvec);

		VectorMA(point, hdist - tradius, dir, point);
		//now a point on the surface of a cylinder with a radius of tradius
	*/
	VectorSubtract(point, tcenter, point_dir);
	VectorNormalize(point_dir);

	//Get bottom to top (Vertical) position index
	const float udot = DotProduct(up, point_dir);
	if (udot > .800)
		vertical = 4;
	else if (udot > .400)
		vertical = 3;
	else if (udot > -.333)
		vertical = 2;
	else if (udot > -.666)
		vertical = 1;
	else
		vertical = 0;

	//Get back to front (Forward) position index
	const float fdot = DotProduct(forward, point_dir);
	if (fdot > .666)
		Forward = 4;
	else if (fdot > .333)
		Forward = 3;
	else if (fdot > -.333)
		Forward = 2;
	else if (fdot > -.666)
		Forward = 1;
	else
		Forward = 0;

	//Get left to right (Lateral) position index
	const float rdot = DotProduct(right, point_dir);
	if (rdot > .666)
		lateral = 4;
	else if (rdot > .333)
		lateral = 3;
	else if (rdot > -.333)
		lateral = 2;
	else if (rdot > -.666)
		lateral = 1;
	else
		lateral = 0;

	const int hit_loc = vertical * 25 + Forward * 5 + lateral;

	if (hit_loc <= 10)
	{
		//feet
		if (rdot > 0)
		{
			return HL_FOOT_RT;
		}
		return HL_FOOT_LT;
	}
	if (hit_loc <= 50)
	{
		//legs
		if (rdot > 0)
		{
			return HL_LEG_RT;
		}
		return HL_LEG_LT;
	}
	if (hit_loc == 56 || hit_loc == 60 || hit_loc == 61 || hit_loc == 65 || hit_loc == 66 || hit_loc == 70)
	{
		//hands
		if (rdot > 0)
		{
			return HL_HAND_RT;
		}
		return HL_HAND_LT;
	}
	if (hit_loc == 83 || hit_loc == 87 || hit_loc == 88 || hit_loc == 92 || hit_loc == 93 || hit_loc == 97)
	{
		//arms
		if (rdot > 0)
		{
			return HL_ARM_RT;
		}
		return HL_ARM_LT;
	}
	if (hit_loc >= 107 && hit_loc <= 109 ||
		hit_loc >= 112 && hit_loc <= 114 ||
		hit_loc >= 117 && hit_loc <= 119)
	{
		//head
		return HL_HEAD;
	}
	if (udot < 0.3)
	{
		return HL_WAIST;
	}
	if (fdot < 0)
	{
		if (rdot > 0.4)
		{
			return HL_BACK_RT;
		}
		if (rdot < -0.4)
		{
			return HL_BACK_LT;
		}
		return HL_BACK;
	}
	if (rdot > 0.3)
	{
		return HL_CHEST_RT;
	}
	if (rdot < -0.3)
	{
		return HL_CHEST_LT;
	}
	return HL_CHEST;
	//return HL_NONE;
}

int G_PickPainAnim(const gentity_t* self, const vec3_t point, int hit_loc = HL_NONE)
{
	if (hit_loc == HL_NONE)
	{
		hit_loc = G_GetHitLocation(self, point);
	}
	switch (hit_loc)
	{
	case HL_FOOT_RT:
		return BOTH_PAIN12;
	case HL_FOOT_LT:
		return -1;
	case HL_LEG_RT:
	{
		if (!Q_irand(0, 1))
		{
			return BOTH_PAIN11;
		}
		return BOTH_PAIN13;
	}
	case HL_LEG_LT:
		return BOTH_PAIN14;
	case HL_BACK_RT:
		return BOTH_PAIN7;
	case HL_BACK_LT:
		return Q_irand(BOTH_PAIN15, BOTH_PAIN16);
	case HL_BACK:
	{
		if (!Q_irand(0, 1))
		{
			return BOTH_PAIN1;
		}
		return BOTH_PAIN5;
	}
	case HL_CHEST_RT:
		return BOTH_PAIN3;
	case HL_CHEST_LT:
		return BOTH_PAIN2;
	case HL_WAIST:
	case HL_CHEST:
	{
		if (!Q_irand(0, 3))
		{
			return BOTH_PAIN6;
		}
		if (!Q_irand(0, 2))
		{
			return BOTH_PAIN8;
		}
		if (!Q_irand(0, 1))
		{
			return BOTH_PAIN17;
		}
		return BOTH_PAIN18;
	}
	case HL_ARM_RT:
	case HL_HAND_RT:
		return BOTH_PAIN9;
	case HL_ARM_LT:
	case HL_HAND_LT:
		return BOTH_PAIN10;
	case HL_HEAD:
		return BOTH_PAIN4;
	default:
		return -1;
	}
}

extern void G_BounceMissile(gentity_t* ent, trace_t* trace);

void LimbThink(gentity_t* ent)
{
	//FIXME: just use object thinking?
	vec3_t origin;
	trace_t tr;

	ent->nextthink = level.time + FRAMETIME;
	if (ent->owner
		&& ent->owner->client
		&& ent->owner->client->ps.eFlags & EF_HELD_BY_RANCOR)
	{
		ent->e_ThinkFunc = thinkF_G_FreeEntity;
		return;
	}

	if (ent->enemy)
	{
		//alert people that I am a piece of one of their friends
		AddSightEvent(ent->enemy, ent->currentOrigin, 384, AEL_DISCOVERED);
	}

	if (ent->s.pos.trType == TR_STATIONARY)
	{
		//stopped
		if (level.time > ent->s.apos.trTime + ent->s.apos.trDuration)
		{
			if (ent->owner && ent->owner->m_pVehicle)
			{
				ent->nextthink = level.time + Q_irand(10000, 15000);
			}
			else
			{
				ent->nextthink = level.time + Q_irand(5000, 15000);
			}

			ent->e_ThinkFunc = thinkF_G_FreeEntity;
			//FIXME: these keep drawing for a frame or so after being freed?!  See them lerp to origin of world...
		}
		else
		{
			EvaluateTrajectory(&ent->s.apos, level.time, ent->currentAngles);
		}
		return;
	}

	// get current position
	EvaluateTrajectory(&ent->s.pos, level.time, origin);
	// get current angles
	EvaluateTrajectory(&ent->s.apos, level.time, ent->currentAngles);

	// trace a line from the previous position to the current position,
	// ignoring interactions with the missile owner
	gi.trace(&tr, ent->currentOrigin, ent->mins, ent->maxs, origin,
		ent->owner ? ent->owner->s.number : ENTITYNUM_NONE, ent->clipmask, static_cast<EG2_Collision>(0), 0);

	VectorCopy(tr.endpos, ent->currentOrigin);
	if (tr.startsolid)
	{
		tr.fraction = 0;
	}

	gi.linkentity(ent);

	if (tr.fraction != 1)
	{
		G_BounceMissile(ent, &tr);
		if (ent->s.pos.trType == TR_STATIONARY)
		{
			//stopped, stop spinning
			//lay flat
			//pitch
			VectorCopy(ent->currentAngles, ent->s.apos.trBase);
			vec3_t flat_angles{};
			if (ent->s.angles2[0] == -1)
			{
				//any pitch is okay
				flat_angles[0] = ent->currentAngles[0];
			}
			else
			{
				//lay flat
				if (ent->owner
					&& ent->owner->client
					&& ent->owner->client->NPC_class == CLASS_PROTOCOL
					&& ent->count == BOTH_DISMEMBER_TORSO1)
				{
					if (ent->currentAngles[0] > 0 || ent->currentAngles[0] < -180)
					{
						flat_angles[0] = -90;
					}
					else
					{
						flat_angles[0] = 90;
					}
				}
				else
				{
					if (ent->currentAngles[0] > 90 || ent->currentAngles[0] < -90)
					{
						flat_angles[0] = 180;
					}
					else
					{
						flat_angles[0] = 0;
					}
				}
			}
			//yaw
			flat_angles[1] = ent->currentAngles[1];
			//roll
			if (ent->s.angles2[2] == -1)
			{
				//any roll is okay
				flat_angles[2] = ent->currentAngles[2];
			}
			else
			{
				if (ent->currentAngles[2] > 90 || ent->currentAngles[2] < -90)
				{
					flat_angles[2] = 180;
				}
				else
				{
					flat_angles[2] = 0;
				}
			}
			VectorSubtract(flat_angles, ent->s.apos.trBase, ent->s.apos.trDelta);
			for (float& i : ent->s.apos.trDelta)
			{
				i = AngleNormalize180(i);
			}
			ent->s.apos.trTime = level.time;
			ent->s.apos.trDuration = 1000;
			ent->s.apos.trType = TR_LINEAR_STOP;
			//VectorClear( ent->s.apos.trDelta );
		}
	}
}

float hitLocHealthPercentage[HL_MAX] =
{
	0.0f, //HL_NONE = 0,
	0.05f, //HL_FOOT_RT,
	0.05f, //HL_FOOT_LT,
	0.20f, //HL_LEG_RT,
	0.20f, //HL_LEG_LT,
	0.30f, //HL_WAIST,
	0.15f, //HL_BACK_RT,
	0.15f, //HL_BACK_LT,
	0.30f, //HL_BACK,
	0.15f, //HL_CHEST_RT,
	0.15f, //HL_CHEST_LT,
	0.30f, //HL_CHEST,
	0.05f, //HL_ARM_RT,
	0.05f, //HL_ARM_LT,
	0.01f, //HL_HAND_RT,
	0.01f, //HL_HAND_LT,
	0.45f, //HL_HEAD
	0.0f, //HL_GENERIC1,
	0.0f, //HL_GENERIC2,
	0.0f, //HL_GENERIC3,
	0.0f, //HL_GENERIC4,
	0.0f, //HL_GENERIC5,
	0.0f //HL_GENERIC6
};

const char* hitLocName[HL_MAX] =
{
	"none", //HL_NONE = 0,
	"right foot", //HL_FOOT_RT,
	"left foot", //HL_FOOT_LT,
	"right leg", //HL_LEG_RT,
	"left leg", //HL_LEG_LT,
	"waist", //HL_WAIST,
	"back right shoulder", //HL_BACK_RT,
	"back left shoulder", //HL_BACK_LT,
	"back", //HL_BACK,
	"front right shouler", //HL_CHEST_RT,
	"front left shoulder", //HL_CHEST_LT,
	"chest", //HL_CHEST,
	"right arm", //HL_ARM_RT,
	"left arm", //HL_ARM_LT,
	"right hand", //HL_HAND_RT,
	"left hand", //HL_HAND_LT,
	"head", //HL_HEAD
	"generic1", //HL_GENERIC1,
	"generic2", //HL_GENERIC2,
	"generic3", //HL_GENERIC3,
	"generic4", //HL_GENERIC4,
	"generic5", //HL_GENERIC5,
	"generic6" //HL_GENERIC6
};

static qboolean G_LimbLost(const gentity_t* ent, const int hit_loc)
{
	switch (hit_loc)
	{
	case HL_FOOT_RT:
		if (ent->locationDamage[HL_FOOT_RT] >= Q3_INFINITE)
		{
			return qtrue;
		}
		//NOTE: falls through
	case HL_LEG_RT:
		//NOTE: feet fall through
		if (ent->locationDamage[HL_LEG_RT] >= Q3_INFINITE)
		{
			return qtrue;
		}
		return qfalse;

	case HL_FOOT_LT:
		if (ent->locationDamage[HL_FOOT_LT] >= Q3_INFINITE)
		{
			return qtrue;
		}
		//NOTE: falls through
	case HL_LEG_LT:
		//NOTE: feet fall through
		if (ent->locationDamage[HL_LEG_LT] >= Q3_INFINITE)
		{
			return qtrue;
		}
		return qfalse;

	case HL_HAND_LT:
		if (ent->locationDamage[HL_HAND_LT] >= Q3_INFINITE)
		{
			return qtrue;
		}
		//NOTE: falls through
	case HL_ARM_LT:
	case HL_CHEST_LT:
	case HL_BACK_RT:
		//NOTE: hand falls through
		if (ent->locationDamage[HL_ARM_LT] >= Q3_INFINITE
			|| ent->locationDamage[HL_CHEST_LT] >= Q3_INFINITE
			|| ent->locationDamage[HL_BACK_RT] >= Q3_INFINITE
			|| ent->locationDamage[HL_WAIST] >= Q3_INFINITE)
		{
			return qtrue;
		}
		return qfalse;

	case HL_HAND_RT:
		if (ent->locationDamage[HL_HAND_RT] >= Q3_INFINITE)
		{
			return qtrue;
		}
		//NOTE: falls through
	case HL_ARM_RT:
	case HL_CHEST_RT:
	case HL_BACK_LT:
		//NOTE: hand falls through
		if (ent->locationDamage[HL_ARM_RT] >= Q3_INFINITE
			|| ent->locationDamage[HL_CHEST_RT] >= Q3_INFINITE
			|| ent->locationDamage[HL_BACK_LT] >= Q3_INFINITE
			|| ent->locationDamage[HL_WAIST] >= Q3_INFINITE)
		{
			return qtrue;
		}
		return qfalse;

	case HL_HEAD:
		if (ent->locationDamage[HL_HEAD] >= Q3_INFINITE)
		{
			return qtrue;
		}
		//NOTE: falls through
	case HL_WAIST:
		//NOTE: head falls through
		if (ent->locationDamage[HL_WAIST] >= Q3_INFINITE)
		{
			return qtrue;
		}
		return qfalse;
	default:
		return static_cast<qboolean>(ent->locationDamage[hit_loc] >= Q3_INFINITE);
	}
}

extern qboolean G_GetRootSurfNameWithVariant(gentity_t* ent, const char* root_surf_name, char* return_surf_name,
	int return_size);

static void G_RemoveWeaponsWithLimbs(gentity_t* ent, gentity_t* limb, const int limb_anim)
{
	int check_anim;

	for (int weapon_model_num = 0; weapon_model_num < MAX_INHAND_WEAPONS; weapon_model_num++)
	{
		if (ent->weaponModel[weapon_model_num] >= 0)
		{
			char hand_name[MAX_QPATH];
			//have a weapon in this hand
			if (weapon_model_num == 0 && ent->client->ps.saberInFlight)
			{
				//this is the right-hand weapon and it's a saber in-flight (i.e.: not in-hand, so no need to worry about it)
				continue;
			}
			//otherwise, the corpse hasn't dropped their weapon
			switch (weapon_model_num)
			{
			case 0: //right hand
				check_anim = BOTH_DISMEMBER_RARM;
				G_GetRootSurfNameWithVariant(ent, "r_hand", hand_name, sizeof hand_name);
				break;
			case 1: //left hand
				check_anim = BOTH_DISMEMBER_LARM;
				G_GetRootSurfNameWithVariant(ent, "l_hand", hand_name, sizeof hand_name);
				break;
			default: //not handled/valid
				continue;
			}

			if (limb_anim == check_anim || limb_anim == BOTH_DISMEMBER_TORSO1) //either/both hands
			{
				//FIXME: is this first check needed with this lower one?
				if (!gi.G2API_GetSurfaceRenderStatus(&limb->ghoul2[0], hand_name))
				{
					//only copy the weapon over if the hand is actually on this limb...
					//copy it to limb
					if (ent->s.weapon != WP_NONE)
					{
						//only if they actually still have a weapon
						limb->s.weapon = ent->s.weapon;
						limb->weaponModel[weapon_model_num] = ent->weaponModel[weapon_model_num];
					} //else - weaponModel is not -1 but don't have a weapon?  Oops, somehow G2 model wasn't removed?
					//remove it on owner
					if (ent->weaponModel[weapon_model_num] > 0)
					{
						gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[weapon_model_num]);
						ent->weaponModel[weapon_model_num] = -1;
					}
					if (!ent->client->ps.saberInFlight)
					{
						//saberent isn't flying through the air, it's in-hand and attached to player
						if (ent->client->ps.saberEntityNum != ENTITYNUM_NONE && ent->client->ps.saberEntityNum > 0)
						{
							//remove the owner ent's saber model and entity
							if (g_entities[ent->client->ps.saberEntityNum].inuse)
							{
								G_FreeEntity(&g_entities[ent->client->ps.saberEntityNum]);
							}
							ent->client->ps.saberEntityNum = ENTITYNUM_NONE;
						}
					}
				}
				else
				{
					//the hand had already been removed
					if (ent->weaponModel[weapon_model_num] > 0)
					{
						//still a weapon associated with it, remove it from the limb
						gi.G2API_RemoveGhoul2Model(limb->ghoul2, ent->weaponModel[weapon_model_num]);
						limb->weaponModel[weapon_model_num] = -1;
					}
				}
			}
			else
			{
				//this weapon isn't affected by this dismemberment
				if (ent->weaponModel[weapon_model_num] > 0)
				{
					//but a weapon was copied over to the limb, so remove it
					gi.G2API_RemoveGhoul2Model(limb->ghoul2, ent->weaponModel[weapon_model_num]);
					limb->weaponModel[weapon_model_num] = -1;
				}
			}
		}
	}
}

static qboolean G_Dismember(gentity_t* ent, vec3_t point,
	const char* limb_bone, const char* rotate_bone, const char* limb_name,
	const char* limb_cap_name, const char* stub_cap_name, const char* limb_tag_name,
	const char* stub_tag_name,
	const int limb_anim, const float limb_roll_base, const float limb_pitch_base,
	const int hit_loc)
{
	vec3_t dir, new_point;
	const vec3_t limb_angles = { 0, ent->client->ps.legsYaw, 0 };
	trace_t trace;

	//make sure this limb hasn't been lopped off already!
	if (gi.G2API_GetSurfaceRenderStatus(&ent->ghoul2[ent->playerModel], limb_name) == 0x00000100
		/*G2SURFACEFLAG_NODESCENDANTS*/)
	{
		//already lost this limb
		return qfalse;
	}

	//NOTE: only reason I have this next part is because G2API_GetSurfaceRenderStatus is *not* working
	if (G_LimbLost(ent, hit_loc))
	{
		//already lost this limb
		return qfalse;
	}

	//FIXME: when timescale is high, can sometimes cut off a surf that includes a surf that was already cut off
	//0) create a limb ent
	VectorCopy(point, new_point);
	new_point[2] += 6;
	gentity_t* limb = G_Spawn();
	G_SetOrigin(limb, new_point);
	VectorCopy(new_point, limb->s.pos.trBase);
	gi.G2API_CopyGhoul2Instance(ent->ghoul2, limb->ghoul2, 0);
	limb->playerModel = 0; //assumption!
	limb->craniumBone = ent->craniumBone;
	limb->cervicalBone = ent->cervicalBone;
	limb->thoracicBone = ent->thoracicBone;
	limb->upperLumbarBone = ent->upperLumbarBone;
	limb->lowerLumbarBone = ent->lowerLumbarBone;
	limb->hipsBone = ent->hipsBone;
	limb->rootBone = ent->rootBone;

	if (limb_tag_name && g_Bloodmist->integer == 1)
	{
		//add smoke to cap tag
		const class_t npc_class = ent->client->NPC_class;

		if (npc_class != CLASS_ATST && npc_class != CLASS_GONK &&
			npc_class != CLASS_INTERROGATOR && npc_class != CLASS_MARK1 &&
			npc_class != CLASS_MARK2 && npc_class != CLASS_MOUSE &&
			npc_class != CLASS_PROBE && npc_class != CLASS_PROTOCOL &&
			npc_class != CLASS_R2D2 && npc_class != CLASS_R5D2 &&
			npc_class != CLASS_SEEKER && npc_class != CLASS_SENTRY &&
			npc_class != CLASS_SBD && npc_class != CLASS_BATTLEDROID &&
			npc_class != CLASS_DROIDEKA && npc_class != CLASS_OBJECT &&
			npc_class != CLASS_ASSASSIN_DROID && npc_class != CLASS_SABER_DROID)
		{
			//Only the humanoids bleed
			const int new_bolt = gi.G2API_AddBolt(&limb->ghoul2[limb->playerModel], limb_tag_name);
			if (new_bolt != -1)
			{
				G_PlayEffect(G_EffectIndex("saber/limb_bolton"), limb->playerModel, new_bolt, limb->s.number, new_point);
			}
		}
		else
		{//Only the humanoids bleed
			const int new_bolt = gi.G2API_AddBolt(&limb->ghoul2[limb->playerModel], limb_tag_name);

			if (new_bolt != -1)
			{
				G_PlayEffect(G_EffectIndex("env/small_fire"), limb->playerModel, new_bolt, limb->s.number, new_point);
			}
		}
	}

	gi.G2API_StopBoneAnimIndex(&limb->ghoul2[limb->playerModel], limb->hipsBone);

	gi.G2API_SetRootSurface(limb->ghoul2, limb->playerModel, limb_name);

	if (limb_bone && hit_loc == HL_WAIST && ent->client->NPC_class == CLASS_PROTOCOL)
	{
		//play the dismember anim on the limb?
		gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "model_root");
		gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "motion");
		gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "pelvis");
		gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "upper_lumbar");
		//FIXME: screws up origin
		const animation_t* animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
		//play the proper dismember anim on the limb
		gi.G2API_SetBoneAnim(&limb->ghoul2[limb->playerModel], nullptr, animations[limb_anim].firstFrame,
			animations[limb_anim].numFrames + animations[limb_anim].firstFrame,
			BONE_ANIM_OVERRIDE_FREEZE, 1, cg.time, -1, -1);
	}
	if (rotate_bone)
	{
		gi.G2API_SetNewOrigin(&limb->ghoul2[0], gi.G2API_AddBolt(&limb->ghoul2[0], rotate_bone));

		//now let's try to position the limb at the *exact* right spot
		const int new_bolt = gi.G2API_AddBolt(&ent->ghoul2[0], rotate_bone);
		if (new_bolt != -1)
		{
			const int actual_time = cg.time ? cg.time : level.time;
			mdxaBone_t bolt_matrix;
			vec3_t angles;

			VectorSet(angles, 0, ent->currentAngles[YAW], 0);
			gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, new_bolt,
				&bolt_matrix, angles, ent->currentOrigin,
				actual_time, nullptr, ent->s.modelScale);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, limb->s.origin);
			G_SetOrigin(limb, limb->s.origin);
			VectorCopy(limb->s.origin, limb->s.pos.trBase);
		}
	}
	if (limb_cap_name)
	{
		//turn on caps
		gi.G2API_SetSurfaceOnOff(&limb->ghoul2[limb->playerModel], limb_cap_name, 0);
	}

	if (stub_tag_name)
	{
		//add smoke to cap surf, spawn effect
		//do it later
		limb->target = G_NewString(stub_tag_name);
	}
	if (limb_name)
	{
		limb->target2 = G_NewString(limb_name);
		//gi.G2API_SetSurfaceOnOff( &ent->ghoul2[ent->playerModel], limbName, 0x00000100 );//G2SURFACEFLAG_NODESCENDANTS
	}
	if (stub_cap_name)
	{
		//turn on caps
		limb->target3 = G_NewString(stub_cap_name);
		//gi.G2API_SetSurfaceOnOff( &ent->ghoul2[ent->playerModel], stubCapName, 0 );
	}
	limb->count = limb_anim;
	//
	limb->s.radius = 60;
	//4) toss the limb away
	limb->classname = "limb";
	limb->owner = ent;
	limb->enemy = ent->enemy;

	//remove weapons/move them to limb (if applicable in this dismemberment)
	G_RemoveWeaponsWithLimbs(ent, limb, limb_anim);

	limb->e_clThinkFunc = clThinkF_CG_Limb;
	limb->e_ThinkFunc = thinkF_LimbThink;
	limb->nextthink = level.time + FRAMETIME;
	gi.linkentity(limb);
	//need size, contents, clipmask
	limb->svFlags = SVF_USE_CURRENT_ORIGIN;
	limb->clipmask = MASK_SOLID;
	limb->contents = CONTENTS_CORPSE;
	VectorSet(limb->mins, -3.0f, -3.0f, -6.0f);
	VectorSet(limb->maxs, 3.0f, 3.0f, 6.0f);

	//make sure it doesn't start in solid
	gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number, limb->clipmask,
		static_cast<EG2_Collision>(0), 0);
	if (trace.startsolid)
	{
		limb->s.pos.trBase[2] -= limb->mins[2];
		gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number, limb->clipmask,
			static_cast<EG2_Collision>(0), 0);
		if (trace.startsolid)
		{
			limb->s.pos.trBase[2] += limb->mins[2];
			gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number,
				limb->clipmask, static_cast<EG2_Collision>(0), 0);
			if (trace.startsolid)
			{
				//stuck?  don't remove
				G_FreeEntity(limb);
				return qfalse;
			}
		}
	}

	//move it
	VectorCopy(limb->s.pos.trBase, limb->currentOrigin);
	gi.linkentity(limb);

	limb->s.eType = ET_THINKER; //ET_GENERAL;
	limb->physicsBounce = 0.2f;
	limb->s.pos.trType = TR_GRAVITY;
	limb->s.pos.trTime = level.time; // move a bit on the very first frame
	VectorSubtract(point, ent->currentOrigin, dir);
	VectorNormalize(dir);
	VectorSet(limb->s.angles2, limb_pitch_base, 0, limb_roll_base);
	VectorCopy(limb_angles, limb->s.apos.trBase);
	limb->s.apos.trTime = level.time;
	limb->s.apos.trType = TR_LINEAR;
	VectorClear(limb->s.apos.trDelta);

	if (hit_loc == HL_HAND_RT || hit_loc == HL_HAND_LT)
	{
		//hands fly farther
		VectorMA(ent->client->ps.velocity, 200, dir, limb->s.pos.trDelta);
		//make it bounce some
		limb->s.eFlags |= EF_BOUNCE_HALF;
		limb->s.apos.trDelta[0] = Q_irand(-300, 300);
		limb->s.apos.trDelta[1] = Q_irand(-800, 800);
	}
	else if (limb_anim == BOTH_DISMEMBER_HEAD1
		|| limb_anim == BOTH_DISMEMBER_LARM
		|| limb_anim == BOTH_DISMEMBER_RARM)
	{
		//head and arms don't fly as far
		limb->s.eFlags |= EF_BOUNCE_SHRAPNEL;
		VectorMA(ent->client->ps.velocity, 150, dir, limb->s.pos.trDelta);
		limb->s.apos.trDelta[0] = Q_irand(-200, 200);
		limb->s.apos.trDelta[1] = Q_irand(-400, 400);
	}
	else
		// if ( limbAnim == BOTH_DISMEMBER_TORSO1 || limbAnim == BOTH_DISMEMBER_LLEG || limbAnim == BOTH_DISMEMBER_RLEG )
	{
		//everything else just kinda falls off
		limb->s.eFlags |= EF_BOUNCE_SHRAPNEL;
		VectorMA(ent->client->ps.velocity, 100, dir, limb->s.pos.trDelta);
		limb->s.apos.trDelta[0] = Q_irand(-100, 100);
		limb->s.apos.trDelta[1] = Q_irand(-200, 200);
	}
	//roll? No, doesn't work...
	//limb->s.apos.trDelta[2] = Q_irand( -300, 300 );//FIXME: this scales it down @ 80% and does weird stuff in timescale != 1.0
	//limb->s.apos.trDelta[2] = limbRoll;

	//preserve scale so giants don't have tiny limbs
	VectorCopy(ent->s.modelScale, limb->s.modelScale);

	//mark ent as dismembered
	ent->locationDamage[hit_loc] = Q3_INFINITE; //mark this limb as gone
	ent->client->dismembered = true;

	//copy the custom RGB to the limb (for skin coloring)
	limb->startRGBA[0] = ent->client->renderInfo.customRGBA[0];
	limb->startRGBA[1] = ent->client->renderInfo.customRGBA[1];
	limb->startRGBA[2] = ent->client->renderInfo.customRGBA[2];

	return qtrue;
}

static qboolean G_Dismemberable(const gentity_t* self, const int hit_loc)
{
	if (self->client->dismembered)
	{
		//cannot dismember me right now
		return qfalse;
	}
	if (!debug_subdivision->integer && g_saberRealisticCombat->integer < 2)
	{
		if (g_dismemberProbabilities->value > 0.0f)
		{
			//use the ent-specific dismemberProbabilities
			float dismember_prob;
			// check which part of the body it is. Then check the npc's probability
			// of that body part coming off, if it doesn't pass, return out.
			switch (hit_loc)
			{
			case HL_LEG_RT:
			case HL_LEG_LT:
				dismember_prob = self->client->dismemberProbLegs;
				break;
			case HL_WAIST:
				dismember_prob = self->client->dismemberProbWaist;
				break;
			case HL_BACK_RT:
			case HL_BACK_LT:
			case HL_CHEST_RT:
			case HL_CHEST_LT:
			case HL_ARM_RT:
			case HL_ARM_LT:
				dismember_prob = self->client->dismemberProbArms;
				break;
			case HL_HAND_RT:
			case HL_HAND_LT:
				dismember_prob = self->client->dismemberProbHands;
				break;
			case HL_HEAD:
				dismember_prob = self->client->dismemberProbHead;
				break;
			default:
				return qfalse;
			}

			//check probability of this happening on this npc
			if (floor(Q_flrand(1, 100) * g_dismemberProbabilities->value) > dismember_prob * 2.0f)
				//probabilities seemed really really low, had to crank them up
			{
				return qfalse;
			}
		}
	}
	return qtrue;
}

static qboolean G_Dismemberable2(const gentity_t* self, const int hit_loc)
{
	if (self->client->dismembered)
	{
		//cannot dismember me right now
		return qfalse;
	}
	if (!debug_subdivision->integer && g_saberRealisticCombat->integer < 2)
	{
		if (g_dismemberProbabilities->value <= 0.0f)
		{
			//add the passed-in damage to the locationDamage array, check to see if it's taken enough damage to actually dismember
			if (self->locationDamage[hit_loc] < self->client->ps.stats[STAT_MAX_HEALTH] * hitLocHealthPercentage[
				hit_loc])
			{
				//this location has not taken enough damage to dismember
				return qfalse;
			}
		}
	}
	return qtrue;
}

constexpr auto MAX_VARIANTS = 8;

qboolean G_GetRootSurfNameWithVariant(gentity_t* ent, const char* root_surf_name, char* return_surf_name,
	const int return_size)
{
	if (!gi.G2API_GetSurfaceRenderStatus(&ent->ghoul2[ent->playerModel], root_surf_name))
	{
		//see if the basic name without variants is on
		Q_strncpyz(return_surf_name, root_surf_name, return_size);
		return qtrue;
	}
	//check variants
	for (int i = 0; i < MAX_VARIANTS; i++)
	{
		Com_sprintf(return_surf_name, return_size, "%s%c", root_surf_name, 'a' + i);
		if (!gi.G2API_GetSurfaceRenderStatus(&ent->ghoul2[ent->playerModel], return_surf_name))
		{
			return qtrue;
		}
	}
	Q_strncpyz(return_surf_name, root_surf_name, return_size);
	return qfalse;
}

extern qboolean g_standard_humanoid(gentity_t* self);

qboolean G_DoDismemberment(gentity_t* self, vec3_t point, const int mod, const int hit_loc,
	const qboolean force = qfalse)
{
	if ((g_dismemberment->integer || g_saberRealisticCombat->integer > 1) && mod == MOD_SABER) //only lightsaber
	{
		if (g_standard_humanoid(self)
			&& (force || g_dismemberProbabilities->value > 0.0f || G_Dismemberable2(self, hit_loc)))
		{
			const char* limb_bone = nullptr, * rotate_bone = nullptr, * limb_tag_name = nullptr, * stub_tag_name = nullptr;
			int anim = -1;
			float limb_roll_base = 0, limb_pitch_base = 0;
			qboolean do_dismemberment = qfalse;
			char limb_name[MAX_QPATH];
			char stub_name[MAX_QPATH];
			char limb_cap_name[MAX_QPATH];
			char stub_cap_name[MAX_QPATH];

			switch (hit_loc)
			{
			case HL_LEG_RT:
				if (g_dismemberment->integer > 1)
				{
					do_dismemberment = qtrue;
					limb_bone = "rtibia";
					rotate_bone = "rtalus";
					G_GetRootSurfNameWithVariant(self, "r_leg", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "hips", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_hips", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_leg", stub_name);
					limb_tag_name = "*r_leg_cap_hips";
					stub_tag_name = "*hips_cap_r_leg";
					anim = BOTH_DISMEMBER_RLEG;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_LEG_LT:
				if (g_dismemberment->integer > 1)
				{
					do_dismemberment = qtrue;
					limb_bone = "ltibia";
					rotate_bone = "ltalus";
					G_GetRootSurfNameWithVariant(self, "l_leg", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "hips", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_hips", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_leg", stub_name);
					limb_tag_name = "*l_leg_cap_hips";
					stub_tag_name = "*hips_cap_l_leg";
					anim = BOTH_DISMEMBER_LLEG;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_WAIST:
				if (g_dismemberment->integer > 2 &&
					(!self->s.number || !self->message))
				{
					do_dismemberment = qtrue;
					limb_bone = "pelvis";
					rotate_bone = "thoracic";
					Q_strncpyz(limb_name, "torso", sizeof limb_name);
					Q_strncpyz(limb_cap_name, "torso_cap_hips", sizeof limb_cap_name);
					Q_strncpyz(stub_cap_name, "hips_cap_torso", sizeof stub_cap_name);
					limb_tag_name = "*torso_cap_hips";
					stub_tag_name = "*hips_cap_torso";
					anim = BOTH_DISMEMBER_TORSO1;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_BACK_RT:
				if (g_dismemberment->integer)
				{
					do_dismemberment = qtrue;
					limb_bone = "rhumerus";
					rotate_bone = "rradius";
					G_GetRootSurfNameWithVariant(self, "r_arm", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "torso", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_torso", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_arm", stub_name);
					limb_tag_name = "*r_arm_cap_torso";
					stub_tag_name = "*torso_cap_r_arm";
					anim = BOTH_DISMEMBER_RARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_BACK_LT:
				if (g_dismemberment->integer &&
					(!self->s.number || !self->message))
				{
					//either the player or not carrying a key on my arm
					do_dismemberment = qtrue;
					limb_bone = "lhumerus";
					rotate_bone = "lradius";
					G_GetRootSurfNameWithVariant(self, "l_arm", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "torso", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_torso", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_arm", stub_name);
					limb_tag_name = "*l_arm_cap_torso";
					stub_tag_name = "*torso_cap_l_arm";
					anim = BOTH_DISMEMBER_LARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_HAND_RT:
				if (g_dismemberment->integer)
				{
					do_dismemberment = qtrue;
					limb_bone = "rradiusX";
					rotate_bone = "rhand";
					G_GetRootSurfNameWithVariant(self, "r_hand", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "r_arm", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_r_arm", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_hand", stub_name);
					limb_tag_name = "*r_hand_cap_r_arm";
					stub_tag_name = "*r_arm_cap_r_hand";
					anim = BOTH_DISMEMBER_RARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_HAND_LT:
				if (g_dismemberment->integer)
				{
					do_dismemberment = qtrue;
					limb_bone = "lradiusX";
					rotate_bone = "lhand";
					G_GetRootSurfNameWithVariant(self, "l_hand", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "l_arm", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_l_arm", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_hand", stub_name);
					limb_tag_name = "*l_hand_cap_l_arm";
					stub_tag_name = "*l_arm_cap_l_hand";
					anim = BOTH_DISMEMBER_LARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
				}
				break;
			case HL_HEAD:
				if (g_dismemberment->integer > 1)
				{
					do_dismemberment = qtrue;
					limb_bone = "cervical";
					rotate_bone = "cranium";
					Q_strncpyz(limb_name, "head", sizeof limb_name);
					Q_strncpyz(limb_cap_name, "head_cap_torso", sizeof limb_cap_name);
					Q_strncpyz(stub_cap_name, "torso_cap_head", sizeof stub_cap_name);
					limb_tag_name = "*head_cap_torso";
					stub_tag_name = "*torso_cap_head";
					anim = BOTH_DISMEMBER_HEAD1;
					limb_roll_base = -1;
					limb_pitch_base = -1;
				}
				break;
			case HL_FOOT_RT:
			case HL_FOOT_LT:
			case HL_CHEST:
			case HL_BACK:
			default:
				break;
			}
			if (do_dismemberment)
			{
				return G_Dismember(self, point, limb_bone, rotate_bone, limb_name, limb_cap_name, stub_cap_name,
					limb_tag_name,
					stub_tag_name, anim, limb_roll_base, limb_pitch_base, hit_loc);
			}
		}
	}
	return qfalse;
}

qboolean G_DoDismembermentcin(gentity_t* self, vec3_t point, const int mod, const int hit_loc,
	const qboolean force = qfalse)
{
	if (mod == MOD_SABER) //only lightsaber
	{
		if (g_standard_humanoid(self)
			&& force)
		{
			const char* limb_bone = nullptr, * rotate_bone = nullptr, * limb_tag_name = nullptr, * stub_tag_name = nullptr;
			int anim = -1;
			float limb_roll_base = 0, limbPitchBase = 0;
			qboolean do_dismemberment = qfalse;
			char limb_name[MAX_QPATH];
			char stub_name[MAX_QPATH];
			char limb_cap_name[MAX_QPATH];
			char stub_cap_name[MAX_QPATH];

			switch (hit_loc)
			{
			case HL_LEG_RT:
				do_dismemberment = qtrue;
				limb_bone = "rtibia";
				rotate_bone = "rtalus";
				G_GetRootSurfNameWithVariant(self, "r_leg", limb_name, sizeof limb_name);
				G_GetRootSurfNameWithVariant(self, "hips", stub_name, sizeof stub_name);
				Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_hips", limb_name);
				Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_leg", stub_name);
				limb_tag_name = "*r_leg_cap_hips";
				stub_tag_name = "*hips_cap_r_leg";
				anim = BOTH_DISMEMBER_RLEG;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_LEG_LT:
				do_dismemberment = qtrue;
				limb_bone = "ltibia";
				rotate_bone = "ltalus";
				G_GetRootSurfNameWithVariant(self, "l_leg", limb_name, sizeof limb_name);
				G_GetRootSurfNameWithVariant(self, "hips", stub_name, sizeof stub_name);
				Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_hips", limb_name);
				Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_leg", stub_name);
				limb_tag_name = "*l_leg_cap_hips";
				stub_tag_name = "*hips_cap_l_leg";
				anim = BOTH_DISMEMBER_LLEG;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_WAIST:
				do_dismemberment = qtrue;
				limb_bone = "pelvis";
				rotate_bone = "thoracic";
				Q_strncpyz(limb_name, "torso", sizeof limb_name);
				Q_strncpyz(limb_cap_name, "torso_cap_hips", sizeof limb_cap_name);
				Q_strncpyz(stub_cap_name, "hips_cap_torso", sizeof stub_cap_name);
				limb_tag_name = "*torso_cap_hips";
				stub_tag_name = "*hips_cap_torso";
				anim = BOTH_DISMEMBER_TORSO1;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_BACK_RT:
				do_dismemberment = qtrue;
				limb_bone = "rhumerus";
				rotate_bone = "rradius";
				G_GetRootSurfNameWithVariant(self, "r_arm", limb_name, sizeof limb_name);
				G_GetRootSurfNameWithVariant(self, "torso", stub_name, sizeof stub_name);
				Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_torso", limb_name);
				Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_arm", stub_name);
				limb_tag_name = "*r_arm_cap_torso";
				stub_tag_name = "*torso_cap_r_arm";
				anim = BOTH_DISMEMBER_RARM;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_BACK_LT:
				do_dismemberment = qtrue;
				limb_bone = "lhumerus";
				rotate_bone = "lradius";
				G_GetRootSurfNameWithVariant(self, "l_arm", limb_name, sizeof limb_name);
				G_GetRootSurfNameWithVariant(self, "torso", stub_name, sizeof stub_name);
				Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_torso", limb_name);
				Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_arm", stub_name);
				limb_tag_name = "*l_arm_cap_torso";
				stub_tag_name = "*torso_cap_l_arm";
				anim = BOTH_DISMEMBER_LARM;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_HAND_RT:
				do_dismemberment = qtrue;
				limb_bone = "rradiusX";
				rotate_bone = "rhand";
				G_GetRootSurfNameWithVariant(self, "r_hand", limb_name, sizeof limb_name);
				G_GetRootSurfNameWithVariant(self, "r_arm", stub_name, sizeof stub_name);
				Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_r_arm", limb_name);
				Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_hand", stub_name);
				limb_tag_name = "*r_hand_cap_r_arm";
				stub_tag_name = "*r_arm_cap_r_hand";
				anim = BOTH_DISMEMBER_RARM;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_HAND_LT:
				do_dismemberment = qtrue;
				limb_bone = "lradiusX";
				rotate_bone = "lhand";
				G_GetRootSurfNameWithVariant(self, "l_hand", limb_name, sizeof limb_name);
				G_GetRootSurfNameWithVariant(self, "l_arm", stub_name, sizeof stub_name);
				Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_l_arm", limb_name);
				Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_hand", stub_name);
				limb_tag_name = "*l_hand_cap_l_arm";
				stub_tag_name = "*l_arm_cap_l_hand";
				anim = BOTH_DISMEMBER_LARM;
				limb_roll_base = 0;
				limbPitchBase = 0;
				break;
			case HL_HEAD:
				do_dismemberment = qtrue;
				limb_bone = "cervical";
				rotate_bone = "cranium";
				Q_strncpyz(limb_name, "head", sizeof limb_name);
				Q_strncpyz(limb_cap_name, "head_cap_torso", sizeof limb_cap_name);
				Q_strncpyz(stub_cap_name, "torso_cap_head", sizeof stub_cap_name);
				limb_tag_name = "*head_cap_torso";
				stub_tag_name = "*torso_cap_head";
				anim = BOTH_DISMEMBER_HEAD1;
				limb_roll_base = -1;
				limbPitchBase = -1;
				break;
			case HL_FOOT_RT:
			case HL_FOOT_LT:
			case HL_CHEST:
			case HL_BACK:
			default:
				break;
			}
			if (do_dismemberment)
			{
				return G_Dismember(self, point, limb_bone, rotate_bone, limb_name, limb_cap_name, stub_cap_name,
					limb_tag_name,
					stub_tag_name, anim, limb_roll_base, limbPitchBase, hit_loc);
			}
		}
	}
	return qfalse;
}

static qboolean G_DoGunDismemberment(gentity_t* self, vec3_t point, const int mod, const int hit_loc)
{
	if (mod == MOD_BLASTER
		|| mod == MOD_BLASTER_ALT
		|| mod == MOD_REBELBLASTER
		|| mod == MOD_REBELBLASTER_ALT
		|| mod == MOD_CLONECOMMANDO
		|| mod == MOD_CLONECOMMANDO_ALT
		|| mod == MOD_REBELRIFLE
		|| mod == MOD_REBELRIFLE_ALT
		|| mod == MOD_REY
		|| mod == MOD_REY_ALT
		|| mod == MOD_CLONEPISTOL
		|| mod == MOD_CLONEPISTOL_ALT
		|| mod == MOD_JANGO
		|| mod == MOD_JANGO_ALT
		|| mod == MOD_BOBA
		|| mod == MOD_BOBA_ALT
		|| mod == MOD_BOWCASTER
		|| mod == MOD_BOWCASTER_ALT
		|| mod == MOD_REPEATER
		|| mod == MOD_FLECHETTE
		|| mod == MOD_BRYAR_ALT
		|| mod == MOD_BRYAR
		|| mod == MOD_EMPLACED
		|| mod == MOD_HEADSHOT
		|| mod == MOD_BODYSHOT)
	{
		if (g_gundismemberment->integer > 0)
		{
			if (g_standard_humanoid(self) && !NPC_IsNotDismemberable(self)
				&& g_dismemberProbabilities->value > 0.0f)
			{
				const char* limb_bone = nullptr, * rotate_bone = nullptr, * limb_tag_name = nullptr, * stub_tag_name =
					nullptr;
				int anim = -1;
				float limb_roll_base = 0, limb_pitch_base = 0;
				qboolean do_dismemberment = qfalse;
				char limb_name[MAX_QPATH];
				char stub_name[MAX_QPATH];
				char limb_cap_name[MAX_QPATH];
				char stub_cap_name[MAX_QPATH];

				switch (hit_loc)
				{
				case HL_LEG_RT:
					do_dismemberment = qtrue;
					limb_bone = "rtibia";
					rotate_bone = "rtalus";
					G_GetRootSurfNameWithVariant(self, "r_leg", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "hips", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_hips", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_leg", stub_name);
					limb_tag_name = "*r_leg_cap_hips";
					stub_tag_name = "*hips_cap_r_leg";
					anim = BOTH_DISMEMBER_RLEG;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_LEG_LT:
					do_dismemberment = qtrue;
					limb_bone = "ltibia";
					rotate_bone = "ltalus";
					G_GetRootSurfNameWithVariant(self, "l_leg", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "hips", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_hips", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_leg", stub_name);
					limb_tag_name = "*l_leg_cap_hips";
					stub_tag_name = "*hips_cap_l_leg";
					anim = BOTH_DISMEMBER_LLEG;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_WAIST:
					do_dismemberment = qtrue;
					limb_bone = "pelvis";
					rotate_bone = "thoracic";
					Q_strncpyz(limb_name, "torso", sizeof limb_name);
					Q_strncpyz(limb_cap_name, "torso_cap_hips", sizeof limb_cap_name);
					Q_strncpyz(stub_cap_name, "hips_cap_torso", sizeof stub_cap_name);
					limb_tag_name = "*torso_cap_hips";
					stub_tag_name = "*hips_cap_torso";
					anim = BOTH_DISMEMBER_TORSO1;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_CHEST_RT:
				case HL_ARM_RT:
				case HL_BACK_RT:
					do_dismemberment = qtrue;
					limb_bone = "rhumerus";
					rotate_bone = "rradius";
					G_GetRootSurfNameWithVariant(self, "r_arm", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "torso", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_torso", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_arm", stub_name);
					limb_tag_name = "*r_arm_cap_torso";
					stub_tag_name = "*torso_cap_r_arm";
					anim = BOTH_DISMEMBER_RARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_CHEST_LT:
				case HL_ARM_LT:
				case HL_BACK_LT:
					do_dismemberment = qtrue;
					limb_bone = "lhumerus";
					rotate_bone = "lradius";
					G_GetRootSurfNameWithVariant(self, "l_arm", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "torso", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_torso", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_arm", stub_name);
					limb_tag_name = "*l_arm_cap_torso";
					stub_tag_name = "*torso_cap_l_arm";
					anim = BOTH_DISMEMBER_LARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_HAND_RT:
					do_dismemberment = qtrue;
					limb_bone = "rradiusX";
					rotate_bone = "rhand";
					G_GetRootSurfNameWithVariant(self, "r_hand", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "r_arm", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_r_arm", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_r_hand", stub_name);
					limb_tag_name = "*r_hand_cap_r_arm";
					stub_tag_name = "*r_arm_cap_r_hand";
					anim = BOTH_DISMEMBER_RARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_HAND_LT:
					do_dismemberment = qtrue;
					limb_bone = "lradiusX";
					rotate_bone = "lhand";
					G_GetRootSurfNameWithVariant(self, "l_hand", limb_name, sizeof limb_name);
					G_GetRootSurfNameWithVariant(self, "l_arm", stub_name, sizeof stub_name);
					Com_sprintf(limb_cap_name, sizeof limb_cap_name, "%s_cap_l_arm", limb_name);
					Com_sprintf(stub_cap_name, sizeof stub_cap_name, "%s_cap_l_hand", stub_name);
					limb_tag_name = "*l_hand_cap_l_arm";
					stub_tag_name = "*l_arm_cap_l_hand";
					anim = BOTH_DISMEMBER_LARM;
					limb_roll_base = 0;
					limb_pitch_base = 0;
					break;
				case HL_HEAD:
					do_dismemberment = qtrue;
					limb_bone = "cervical";
					rotate_bone = "cranium";
					Q_strncpyz(limb_name, "head", sizeof limb_name);
					Q_strncpyz(limb_cap_name, "head_cap_torso", sizeof limb_cap_name);
					Q_strncpyz(stub_cap_name, "torso_cap_head", sizeof stub_cap_name);
					limb_tag_name = "*head_cap_torso";
					stub_tag_name = "*torso_cap_head";
					anim = BOTH_DISMEMBER_HEAD1;
					limb_roll_base = -1;
					limb_pitch_base = -1;
					break;
				case HL_FOOT_RT:
				case HL_FOOT_LT:
				case HL_CHEST:
				case HL_BACK:
				default:
					break;
				}
				if (do_dismemberment)
				{
					return G_Dismember(self, point, limb_bone, rotate_bone, limb_name, limb_cap_name, stub_cap_name,
						limb_tag_name, stub_tag_name, anim, limb_roll_base, limb_pitch_base, hit_loc);
				}
			}
		}
	}
	return qfalse;
}

static int G_CheckSpecialDeathAnim(gentity_t* self)
{
	int death_anim = -1;

	if (self->client->ps.legsAnim == BOTH_GETUP_BROLL_L
		|| self->client->ps.legsAnim == BOTH_GETUP_BROLL_R)
	{
		//rolling away to the side on our back
		death_anim = BOTH_DEATH_LYING_UP;
	}
	else if (self->client->ps.legsAnim == BOTH_GETUP_FROLL_L
		|| self->client->ps.legsAnim == BOTH_GETUP_FROLL_R)
	{
		//rolling away to the side on our front
		death_anim = BOTH_DEATH_LYING_DN;
	}
	else if (self->client->ps.legsAnim == BOTH_GETUP_BROLL_F
		&& self->client->ps.legsAnimTimer > 350)
	{
		//kicking up
		death_anim = BOTH_DEATH_FALLING_UP;
	}
	else if (self->client->ps.legsAnim == BOTH_GETUP_BROLL_B
		&& self->client->ps.legsAnimTimer > 950)
	{
		//on back, rolling back to get up
		death_anim = BOTH_DEATH_LYING_UP;
	}
	else if (self->client->ps.legsAnim == BOTH_GETUP_BROLL_B
		&& self->client->ps.legsAnimTimer <= 950
		&& self->client->ps.legsAnimTimer > 250)
	{
		//flipping over backwards
		death_anim = BOTH_FALLDEATH1LAND;
	}
	else if (self->client->ps.legsAnim == BOTH_GETUP_FROLL_B
		&& self->client->ps.legsAnimTimer <= 1100
		&& self->client->ps.legsAnimTimer > 250)
	{
		//flipping over backwards
		death_anim = BOTH_FALLDEATH1LAND;
	}
	else if (PM_InRoll(&self->client->ps))
	{
		death_anim = BOTH_DEATH_ROLL; //# Death anim from a roll
	}
	else if (PM_FlippingAnim(self->client->ps.legsAnim))
	{
		death_anim = BOTH_DEATH_FLIP; //# Death anim from a flip
	}
	else if (PM_SpinningAnim(self->client->ps.legsAnim))
	{
		float yaw_diff = AngleNormalize180(
			AngleNormalize180(self->client->renderInfo.torsoAngles[YAW]) - AngleNormalize180(
				self->client->ps.viewangles[YAW]));
		if (yaw_diff > 135 || yaw_diff < -135)
		{
			death_anim = BOTH_DEATH_SPIN_180; //# Death anim when facing backwards
		}
		else if (yaw_diff < -60)
		{
			death_anim = BOTH_DEATH_SPIN_90_R; //# Death anim when facing 90 degrees right
		}
		else if (yaw_diff > 60)
		{
			death_anim = BOTH_DEATH_SPIN_90_L; //# Death anim when facing 90 degrees left
		}
	}
	else if (PM_InKnockDown(&self->client->ps))
	{
		//since these happen a lot, let's handle them case by case
		int anim_length = PM_AnimLength(self->client->clientInfo.animFileIndex,
			static_cast<animNumber_t>(self->client->ps.legsAnim));
		if (self->s.number < MAX_CLIENTS)
		{
			switch (self->client->ps.legsAnim)
			{
			case BOTH_KNOCKDOWN1:
			case BOTH_KNOCKDOWN2:
			case BOTH_KNOCKDOWN3:
			case BOTH_KNOCKDOWN4:
			case BOTH_KNOCKDOWN5:
			case BOTH_SLAPDOWNRIGHT:
			case BOTH_SLAPDOWNLEFT:
				anim_length += PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME;
				break;
			default:;
			}
		}
		switch (self->client->ps.legsAnim)
		{
		case BOTH_KNOCKDOWN1:
			if (anim_length - self->client->ps.legsAnimTimer > 100)
			{
				//on our way down
				if (self->client->ps.legsAnimTimer > 600)
				{
					//still partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_PLAYER_PA_3_FLY:
		case BOTH_KNOCKDOWN2:
			if (anim_length - self->client->ps.legsAnimTimer > 700)
			{
				//on our way down
				if (self->client->ps.legsAnimTimer > 600)
				{
					//still partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_KNOCKDOWN3:
			if (anim_length - self->client->ps.legsAnimTimer > 100)
			{
				//on our way down
				if (self->client->ps.legsAnimTimer > 1300)
				{
					//still partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_KNOCKDOWN4:
		case BOTH_RELEASED:
			if (anim_length - self->client->ps.legsAnimTimer > 300)
			{
				//on our way down
				if (self->client->ps.legsAnimTimer > 350)
				{
					//still partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			else
			{
				//crouch death
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			break;
		case BOTH_KNOCKDOWN5:
		case BOTH_SLAPDOWNRIGHT:
		case BOTH_SLAPDOWNLEFT:
		case BOTH_LK_DL_ST_T_SB_1_L:
			if (self->client->ps.legsAnimTimer < 750)
			{
				//flat
				death_anim = BOTH_DEATH_LYING_DN;
			}
			break;
		case BOTH_GETUP1:
			if (self->client->ps.legsAnimTimer < 350)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 800)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 450)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP2:
			if (self->client->ps.legsAnimTimer < 150)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 850)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 500)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP3:
			if (self->client->ps.legsAnimTimer < 250)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 600)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 150)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_GETUP4:
			if (self->client->ps.legsAnimTimer < 250)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 600)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 850)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP5:
			if (self->client->ps.legsAnimTimer > 850)
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 1500)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_GETUP_CROUCH_B1:
			if (self->client->ps.legsAnimTimer < 800)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 400)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP_CROUCH_F1:
			if (self->client->ps.legsAnimTimer < 800)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 150)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B1:
			if (self->client->ps.legsAnimTimer < 325)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 725)
			{
				//spinning up
				death_anim = BOTH_DEATH_SPIN_180; //# Death anim when facing backwards
			}
			else if (self->client->ps.legsAnimTimer < 900)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 50)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B2:
			if (self->client->ps.legsAnimTimer < 575)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 875)
			{
				//spinning up
				death_anim = BOTH_DEATH_SPIN_180; //# Death anim when facing backwards
			}
			else if (self->client->ps.legsAnimTimer < 900)
			{
				//crouching
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else
			{
				//lying down
				//partially up
				death_anim = BOTH_DEATH_FALLING_UP;
			}
			break;
		case BOTH_FORCE_GETUP_B3:
			if (self->client->ps.legsAnimTimer < 150)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 775)
			{
				//flipping
				death_anim = BOTH_DEATHBACKWARD2; //backflip
			}
			else
			{
				//lying down
				//partially up
				death_anim = BOTH_DEATH_FALLING_UP;
			}
			break;
		case BOTH_FORCE_GETUP_B4:
			if (self->client->ps.legsAnimTimer < 325)
			{
				//standing up
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 150)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B5:
			if (self->client->ps.legsAnimTimer < 550)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 1025)
			{
				//kicking up
				death_anim = BOTH_DEATHBACKWARD2; //backflip
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 50)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B6:
			if (self->client->ps.legsAnimTimer < 225)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 425)
			{
				//crouching up
				vec3_t fwd;
				AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
				float thrown = DotProduct(fwd, self->client->ps.velocity);
				if (thrown < -150)
				{
					death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
				}
				else
				{
					death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
				}
			}
			else if (self->client->ps.legsAnimTimer < 825)
			{
				//flipping up
				death_anim = BOTH_DEATHFORWARD3; //backflip
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 225)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_UP;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_F1:
			if (self->client->ps.legsAnimTimer < 275)
			{
				//standing up
			}
			else if (self->client->ps.legsAnimTimer < 750)
			{
				//flipping
				death_anim = BOTH_DEATH14;
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 100)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_FORCE_GETUP_F2:
			if (self->client->ps.legsAnimTimer < 1200)
			{
				//standing
			}
			else
			{
				//lying down
				if (anim_length - self->client->ps.legsAnimTimer > 225)
				{
					//partially up
					death_anim = BOTH_DEATH_FALLING_DN;
				}
				else
				{
					//down
					death_anim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		default:;
		}
	}
	else if (PM_InOnGroundAnim(&self->client->ps))
	{
		if (AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0)
		{
			death_anim = BOTH_DEATH_LYING_UP; //# Death anim when lying on back
		}
		else
		{
			death_anim = BOTH_DEATH_LYING_DN; //# Death anim when lying on front
		}
	}
	else if (PM_CrouchAnim(self->client->ps.legsAnim))
	{
		vec3_t fwd;
		AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
		float thrown = DotProduct(fwd, self->client->ps.velocity);
		if (thrown < -200)
		{
			death_anim = BOTH_DEATHBACKWARD1; //# Death anim when crouched and thrown back
			if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100)
			{
				self->client->ps.velocity[2] = 100;
			}
		}
		else
		{
			death_anim = BOTH_DEATH_CROUCHED; //# Death anim when crouched
		}
	}

	return death_anim;
}

extern qboolean PM_FinishedCurrentLegsAnim(gentity_t* self);

static int G_PickDeathAnim(gentity_t* self, vec3_t point, const int damage, int hit_loc)
{
	int death_anim = -1;

	if (hit_loc == HL_NONE)
	{
		hit_loc = G_GetHitLocation(self, point); //self->hit_loc
	}
	//dead flops...if you are already playing a death animation, I guess it can just return directly
	switch (self->client->ps.legsAnim)
	{
	case BOTH_DEATH1: //# First Death anim
	case BOTH_DEAD1:
	case BOTH_DEATH2: //# Second Death anim
	case BOTH_DEAD2:
	case BOTH_DEATH8: //#
	case BOTH_DEAD8:
	case BOTH_DEATH13: //#
	case BOTH_DEAD13:
	case BOTH_DEATH14: //#
	case BOTH_DEAD14:
	case BOTH_DEATH16: //#
	case BOTH_DEAD16:
	case BOTH_DEADBACKWARD1: //# First thrown backward death finished pose
	case BOTH_DEADBACKWARD2: //# Second thrown backward death finished pose
		//return -2;
		//break;
		if (PM_FinishedCurrentLegsAnim(self))
		{
			//done with the anim
			death_anim = BOTH_DEADFLOP2;
		}
		else
		{
			death_anim = -2;
		}
		return death_anim;
	case BOTH_DEADFLOP2:
		//return -2;
		return BOTH_DEADFLOP2;
	case BOTH_DEATH10: //#
	case BOTH_DEAD10:
	case BOTH_DEATH15: //#
	case BOTH_DEAD15:
	case BOTH_DEADFORWARD1: //# First thrown forward death finished pose
	case BOTH_DEADFORWARD2: //# Second thrown forward death finished pose
		//return -2;
		//break;
		if (PM_FinishedCurrentLegsAnim(self))
		{
			//done with the anim
			death_anim = BOTH_DEADFLOP1;
		}
		else
		{
			death_anim = -2;
		}
		return death_anim;
	case BOTH_DEADFLOP1:
		//return -2;
		return BOTH_DEADFLOP1;
	case BOTH_DEAD3: //# Third Death finished pose
	case BOTH_DEAD4: //# Fourth Death finished pose
	case BOTH_DEAD5: //# Fifth Death finished pose
	case BOTH_DEAD6: //# Sixth Death finished pose
	case BOTH_DEAD7: //# Seventh Death finished pose
	case BOTH_DEAD9: //#
	case BOTH_DEAD11: //#
	case BOTH_DEAD12: //#
	case BOTH_DEAD17: //#
	case BOTH_DEAD18: //#
	case BOTH_DEAD19: //#
	case BOTH_DEAD20: //#
	case BOTH_DEAD21: //#
	case BOTH_DEAD22: //#
	case BOTH_DEAD23: //#
	case BOTH_DEAD24: //#
	case BOTH_DEAD25: //#
	case BOTH_LYINGDEAD1: //# Killed lying down death finished pose
	case BOTH_STUMBLEDEAD1: //# Stumble forward death finished pose
	case BOTH_FALLDEAD1LAND: //# Fall forward and splat death finished pose
	case BOTH_DEATH3: //# Third Death anim
	case BOTH_DEATH4: //# Fourth Death anim
	case BOTH_DEATH5: //# Fifth Death anim
	case BOTH_DEATH6: //# Sixth Death anim
	case BOTH_DEATH7: //# Seventh Death anim
	case BOTH_DEATH9: //#
	case BOTH_DEATH11: //#
	case BOTH_DEATH12: //#
	case BOTH_DEATH17: //#
	case BOTH_DEATH18: //#
	case BOTH_DEATH19: //#
	case BOTH_DEATH20: //#
	case BOTH_DEATH21: //#
	case BOTH_DEATH22: //#
	case BOTH_DEATH23: //#
	case BOTH_DEATH24: //#
	case BOTH_DEATH25: //#
	case BOTH_DEATHFORWARD1: //# First Death in which they get thrown forward
	case BOTH_DEATHFORWARD2: //# Second Death in which they get thrown forward
	case BOTH_DEATHFORWARD3: //# Second Death in which they get thrown forward
	case BOTH_DEATHBACKWARD1: //# First Death in which they get thrown backward
	case BOTH_DEATHBACKWARD2: //# Second Death in which they get thrown backward
	case BOTH_DEATH1IDLE: //# Idle while close to death
	case BOTH_LYINGDEATH1: //# Death to play when killed lying down
	case BOTH_STUMBLEDEATH1: //# Stumble forward and fall face first death
	case BOTH_FALLDEATH1: //# Fall forward off a high cliff and splat death - start
	case BOTH_FALLDEATH1INAIR: //# Fall forward off a high cliff and splat death - loop
	case BOTH_FALLDEATH1LAND: //# Fall forward off a high cliff and splat death - hit bottom
		return -2;
	case BOTH_DEATH_ROLL: //# Death anim from a roll
	case BOTH_DEATH_FLIP: //# Death anim from a flip
	case BOTH_DEATH_SPIN_90_R: //# Death anim when facing 90 degrees right
	case BOTH_DEATH_SPIN_90_L: //# Death anim when facing 90 degrees left
	case BOTH_DEATH_SPIN_180: //# Death anim when facing backwards
	case BOTH_DEATH_LYING_UP: //# Death anim when lying on back
	case BOTH_DEATH_LYING_DN: //# Death anim when lying on front
	case BOTH_DEATH_FALLING_DN: //# Death anim when falling on face
	case BOTH_DEATH_FALLING_UP: //# Death anim when falling on back
	case BOTH_DEATH_CROUCHED: //# Death anim when crouched
	case BOTH_RIGHTHANDCHOPPEDOFF:
		return -2;
	default:;
	}
	// Not currently playing a death animation, so try and get an appropriate one now.
	if (death_anim == -1)
	{
		death_anim = G_CheckSpecialDeathAnim(self);

		if (death_anim == -1)
		{
			//base on hit_loc
			vec3_t fwd;
			AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
			const float thrown = DotProduct(fwd, self->client->ps.velocity);
			//death anims
			switch (hit_loc)
			{
			case HL_FOOT_RT:
				if (!Q_irand(0, 2) && thrown < 250)
				{
					death_anim = BOTH_DEATH24; //right foot trips up, spin
				}
				else if (!Q_irand(0, 1))
				{
					if (!Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH4; //back: forward
					}
					else
					{
						death_anim = BOTH_DEATH16; //same as 1
					}
				}
				else
				{
					death_anim = BOTH_DEATH5; //same as 4
				}
				break;
			case HL_FOOT_LT:
				if (!Q_irand(0, 2) && thrown < 250)
				{
					death_anim = BOTH_DEATH25; //left foot trips up, spin
				}
				else if (!Q_irand(0, 1))
				{
					if (!Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH4; //back: forward
					}
					else
					{
						death_anim = BOTH_DEATH16; //same as 1
					}
				}
				else
				{
					death_anim = BOTH_DEATH5; //same as 4
				}
				break;
			case HL_LEG_RT:
				if (!Q_irand(0, 2) && thrown < 250)
				{
					death_anim = BOTH_DEATH3; //right leg collapse
				}
				else if (!Q_irand(0, 1))
				{
					death_anim = BOTH_DEATH5; //same as 4
				}
				else
				{
					if (!Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH4; //back: forward
					}
					else
					{
						death_anim = BOTH_DEATH16; //same as 1
					}
				}
				break;
			case HL_LEG_LT:
				if (!Q_irand(0, 2) && thrown < 250)
				{
					death_anim = BOTH_DEATH7; //left leg collapse
				}
				else if (!Q_irand(0, 1))
				{
					death_anim = BOTH_DEATH5; //same as 4
				}
				else
				{
					if (!Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH4; //back: forward
					}
					else
					{
						death_anim = BOTH_DEATH16; //same as 1
					}
				}
				break;
			case HL_BACK:
				if (fabs(thrown) < 50 || fabs(thrown) < 200 && !Q_irand(0, 3))
				{
					if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH17; //head/back: croak
					}
					else
					{
						death_anim = BOTH_DEATH10; //back: bend back, fall forward
					}
				}
				else
				{
					if (!Q_irand(0, 2))
					{
						death_anim = BOTH_DEATH4; //back: forward
					}
					else if (!Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH5; //back: forward
					}
					else
					{
						death_anim = BOTH_DEATH16; //same as 1
					}
				}
				break;
			case HL_HAND_RT:
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_BACK_LT:
				if (damage <= self->max_health * 0.25 && Q_irand(0, 1) || fabs(thrown) < 200 && !Q_irand(0, 2) || !
					Q_irand(0, 10))
				{
					if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH9; //chest right: snap, fall forward
					}
					else
					{
						death_anim = BOTH_DEATH20; //chest right: snap, fall forward
					}
				}
				else if (damage <= self->max_health * 0.5 && Q_irand(0, 1) || !Q_irand(0, 10))
				{
					death_anim = BOTH_DEATH3; //chest right: back
				}
				else if (damage <= self->max_health * 0.75 && Q_irand(0, 1) || !Q_irand(0, 10))
				{
					death_anim = BOTH_DEATH6; //chest right: spin
				}
				else
				{
					//TEMP HACK: play spinny deaths less often
					if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH8; //chest right: spin high
					}
					else
					{
						switch (Q_irand(0, 3))
						{
						default:
						case 0:
							death_anim = BOTH_DEATH9; //chest right: snap, fall forward
							break;
						case 1:
							death_anim = BOTH_DEATH3; //chest right: back
							break;
						case 2:
							death_anim = BOTH_DEATH6; //chest right: spin
							break;
						case 3:
							death_anim = BOTH_DEATH20; //chest right: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_HAND_LT:
			case HL_BACK_RT:
				if (damage <= self->max_health * 0.25 && Q_irand(0, 1) || fabs(thrown) < 200 && !Q_irand(0, 2) || !
					Q_irand(0, 10))
				{
					if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH11; //chest left: snap, fall forward
					}
					else
					{
						death_anim = BOTH_DEATH21; //chest left: snap, fall forward
					}
				}
				else if (damage <= self->max_health * 0.5 && Q_irand(0, 1) || !Q_irand(0, 10))
				{
					death_anim = BOTH_DEATH7; //chest left: back
				}
				else if (damage <= self->max_health * 0.75 && Q_irand(0, 1) || !Q_irand(0, 10))
				{
					death_anim = BOTH_DEATH12; //chest left: spin
				}
				else
				{
					//TEMP HACK: play spinny deaths less often
					if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH14; //chest left: spin high
					}
					else
					{
						switch (Q_irand(0, 3))
						{
						default:
						case 0:
							death_anim = BOTH_DEATH11; //chest left: snap, fall forward
							break;
						case 1:
							death_anim = BOTH_DEATH7; //chest left: back
							break;
						case 2:
							death_anim = BOTH_DEATH12; //chest left: spin
							break;
						case 3:
							death_anim = BOTH_DEATH21; //chest left: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST:
			case HL_WAIST:
				if (damage <= self->max_health * 0.25 && Q_irand(0, 1) || thrown > -50)
				{
					if (!Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH18; //gut: fall right
					}
					else
					{
						death_anim = BOTH_DEATH19; //gut: fall left
					}
				}
				else if (damage <= self->max_health * 0.5 && !Q_irand(0, 1) || fabs(thrown) < 200 && !Q_irand(0, 3))
				{
					if (Q_irand(0, 2))
					{
						death_anim = BOTH_DEATH2; //chest: backward short
					}
					else if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATH22; //chest: backward short
					}
					else
					{
						death_anim = BOTH_DEATH23; //chest: backward short
					}
				}
				else if (thrown < -300 && Q_irand(0, 1))
				{
					if (Q_irand(0, 1))
					{
						death_anim = BOTH_DEATHBACKWARD1; //chest: fly back
					}
					else
					{
						death_anim = BOTH_DEATHBACKWARD2; //chest: flip back
					}
				}
				else if (thrown < -200 && Q_irand(0, 1))
				{
					death_anim = BOTH_DEATH15; //chest: roll backward
				}
				else
				{
					death_anim = BOTH_DEATH1; //chest: backward med
				}
				break;
			case HL_HEAD:
				if (damage <= self->max_health * 0.5 && Q_irand(0, 2))
				{
					death_anim = BOTH_DEATH17; //head/back: croak
				}
				else
				{
					if (Q_irand(0, 2))
					{
						death_anim = BOTH_DEATH13; //head: stumble, fall back
					}
					else
					{
						death_anim = BOTH_DEATH10; //head: stumble, fall back
					}
				}
				break;
			default:
				break;
			}
		}
	}

	// Validate.....
	if (death_anim == -1 || !PM_HasAnimation(self, death_anim))
	{
		if (death_anim == BOTH_DEADFLOP1
			|| death_anim == BOTH_DEADFLOP2)
		{
			//if don't have deadflop, don't do anything
			death_anim = -1;
		}
		else
		{
			// I guess we'll take what we can get.....
			death_anim = PM_PickAnim(self, BOTH_DEATH1, BOTH_DEATH25);
		}
	}

	return death_anim;
}

int G_CheckLedgeDive(gentity_t* self, const float check_dist, const vec3_t check_vel, const qboolean try_opposite,
	const qboolean try_perp)
{
	//		Intelligent Ledge-Diving Deaths:
	//		If I'm an NPC, check for nearby ledges and fall off it if possible
	//		How should/would/could this interact with knockback if we already have some?
	//		Ideally - apply knockback if there are no ledges or a ledge in that dir
	//		But if there is a ledge and it's not in the dir of my knockback, fall off the ledge instead
	if (!self || !self->client)
	{
		return 0;
	}

	vec3_t fall_forward_dir, fall_right_dir;
	vec3_t angles = { 0 };
	int cliff_fall = 0;

	if (check_vel && !VectorCompare(check_vel, vec3_origin))
	{
		//already moving in a dir
		angles[1] = vectoyaw(self->client->ps.velocity);
		AngleVectors(angles, fall_forward_dir, fall_right_dir, nullptr);
	}
	else
	{
		//try forward first
		angles[1] = self->client->ps.viewangles[1];
		AngleVectors(angles, fall_forward_dir, fall_right_dir, nullptr);
	}
	VectorNormalize(fall_forward_dir);
	float fall_dist = G_CheckForLedge(self, fall_forward_dir, check_dist);
	if (fall_dist >= 128)
	{
		VectorClear(self->client->ps.velocity);
		G_Throw(self, fall_forward_dir, 85);
		self->client->ps.velocity[2] = 100;
		self->client->ps.groundEntityNum = ENTITYNUM_NONE;
	}
	else if (try_opposite)
	{
		VectorScale(fall_forward_dir, -1, fall_forward_dir);
		fall_dist = G_CheckForLedge(self, fall_forward_dir, check_dist);
		if (fall_dist >= 128)
		{
			VectorClear(self->client->ps.velocity);
			G_Throw(self, fall_forward_dir, 85);
			self->client->ps.velocity[2] = 100;
			self->client->ps.groundEntityNum = ENTITYNUM_NONE;
		}
	}
	if (!cliff_fall && try_perp)
	{
		//try sides
		VectorNormalize(fall_right_dir);
		fall_dist = G_CheckForLedge(self, fall_right_dir, check_dist);
		if (fall_dist >= 128)
		{
			VectorClear(self->client->ps.velocity);
			G_Throw(self, fall_right_dir, 85);
			self->client->ps.velocity[2] = 100;
		}
		else
		{
			VectorScale(fall_right_dir, -1, fall_right_dir);
			fall_dist = G_CheckForLedge(self, fall_right_dir, check_dist);
			if (fall_dist >= 128)
			{
				VectorClear(self->client->ps.velocity);
				G_Throw(self, fall_right_dir, 85);
				self->client->ps.velocity[2] = 100;
			}
		}
	}
	if (fall_dist >= 256)
	{
		cliff_fall = 2;
	}
	else if (fall_dist >= 128)
	{
		cliff_fall = 1;
	}
	return cliff_fall;
}

/*
==================
GibEntity
==================
*/
static void GibEntity(gentity_t* self)
{
	G_AddEvent(self, EV_GIB_PLAYER, 0);
	self->takedamage = qfalse;
	self->s.eType = ET_INVISIBLE;
	self->contents = 0;
}

static void GibEntity_Headshot(gentity_t* self)
{
	G_AddEvent(self, EV_GIB_PLAYER_HEADSHOT, 0);
	self->client->noHead = qtrue;
}

/*
==================
player_die
==================
*/
extern qboolean NPC_IsJetpacking(const gentity_t* self);
void NPC_SetAnim(gentity_t* ent, int set_anim_parts, int anim, int set_anim_flags, int i_blend);
extern void AI_DeleteSelfFromGroup(const gentity_t* self);
extern void AI_GroupMemberKilled(const gentity_t* self);
extern qboolean FlyingCreature(const gentity_t* ent);
extern void G_DrivableATSTDie(gentity_t* self);
extern void jet_fly_stop(gentity_t* self);
extern void NPC_LeaveTroop(const gentity_t* actor);
extern void Rancor_DropVictim(gentity_t* self);
extern void Wampa_DropVictim(gentity_t* self);
extern void WP_StopForceHealEffects(const gentity_t* self);
extern void Boba_NoDeadFlameThrower(const gentity_t* self);

void player_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, const int damage, const int means_of_death,
	const int d_flags,
	const int hit_loc)
{
	int anim;
	int contents;
	qboolean death_script = qfalse;
	qboolean last_in_group = qfalse;
	qboolean special_anim = qfalse;
	qboolean holding_saber = qfalse;
	int cliff_fall = 0;

	if (self->client->hook)
	{
		// free it!
		Weapon_HookFree(self->client->hook);
	}

	if (self->client->stun)
	{
		// free it!
		Weapon_StunFree(self->client->stun);
	}

	//FIXME: somehow people are sometimes not completely dying???
	if (self->client->ps.pm_type == PM_DEAD && (means_of_death != MOD_SNIPER && means_of_death != MOD_PROJECTION_END ||
		self
		->flags & FL_DISINTEGRATED))
	{
		//do dismemberment/twitching
		if (self->client->NPC_class == CLASS_MARK1)
		{
			DeathFX(self);
			self->takedamage = qfalse;
			self->client->ps.eFlags |= EF_NODRAW;
			self->contents = 0;
			self->e_ThinkFunc = thinkF_G_FreeEntity;
			self->nextthink = level.time + FRAMETIME;
		}
		else
		{
			anim = G_PickDeathAnim(self, self->pos1, damage, hit_loc);

			if (d_flags & DAMAGE_DISMEMBER || means_of_death == MOD_SABER)
			{
				if (self->client->NPC_class == CLASS_PROJECTION)
				{
					//gentity_t* tent;
					//vec3_t		spot;

					//VectorCopy(self->currentOrigin, spot);

					//self->flags |= FL_DISINTEGRATED;
					//self->svFlags |= SVF_BROADCAST;
					//tent = G_TempEntity(spot, EV_DISINTEGRATION);
					//tent->s.eventParm = PW_DISRUPTION;
					//tent->svFlags |= SVF_BROADCAST;
					//tent->owner = self;

					//G_AlertTeam(self, attacker, 512, 88);

					//if (self->playerModel >= 0)
					//{
					//	// don't let 'em animate
					//	gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->rootBone, cg.time);
					//	gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->motionBone, cg.time);
					//	gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->lowerLumbarBone, cg.time);
					//	anim = -1;
					//}

					////not solid anymore
					//self->contents = 0;
					//self->maxs[2] = -8;

					//if (self->NPC)
					//{
					//	//need to pad deathtime some to stick around long enough for death effect to play
					//	self->NPC->timeOfDeath = level.time + 2000;
					//}
					G_DoDismemberment(self, self->pos1, means_of_death, hit_loc);
				}
				else
				{
					G_DoDismemberment(self, self->pos1, means_of_death, hit_loc);
				}
			}
			else if (means_of_death == MOD_BLASTER
				|| means_of_death == MOD_BLASTER_ALT
				|| means_of_death == MOD_REBELBLASTER
				|| means_of_death == MOD_REBELBLASTER_ALT
				|| means_of_death == MOD_CLONECOMMANDO
				|| means_of_death == MOD_CLONECOMMANDO_ALT
				|| means_of_death == MOD_REBELRIFLE
				|| means_of_death == MOD_REBELRIFLE_ALT
				|| means_of_death == MOD_REY
				|| means_of_death == MOD_REY_ALT
				|| means_of_death == MOD_CLONEPISTOL
				|| means_of_death == MOD_CLONEPISTOL_ALT
				|| means_of_death == MOD_JANGO
				|| means_of_death == MOD_JANGO_ALT
				|| means_of_death == MOD_BOBA
				|| means_of_death == MOD_BOBA_ALT
				|| means_of_death == MOD_BOWCASTER
				|| means_of_death == MOD_BOWCASTER_ALT
				|| means_of_death == MOD_REPEATER
				|| means_of_death == MOD_FLECHETTE
				|| means_of_death == MOD_BRYAR_ALT
				|| means_of_death == MOD_BRYAR
				|| means_of_death == MOD_EMPLACED && !NPC_IsNotDismemberable(self))
			{
				if (self->client->NPC_class == CLASS_PROJECTION)
				{
					//gentity_t* tent;
					//vec3_t		spot;

					//VectorCopy(self->currentOrigin, spot);

					//self->flags |= FL_DISINTEGRATED;
					//self->svFlags |= SVF_BROADCAST;
					//tent = G_TempEntity(spot, EV_DISINTEGRATION);
					//tent->s.eventParm = PW_DISRUPTION;
					//tent->svFlags |= SVF_BROADCAST;
					//tent->owner = self;

					//G_AlertTeam(self, attacker, 512, 88);

					//if (self->playerModel >= 0)
					//{
					//	// don't let 'em animate
					//	gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->rootBone, cg.time);
					//	gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->motionBone, cg.time);
					//	gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->lowerLumbarBone, cg.time);
					//	anim = -1;
					//}

					////not solid anymore
					//self->contents = 0;
					//self->maxs[2] = -8;

					//if (self->NPC)
					//{
					//	//need to pad deathtime some to stick around long enough for death effect to play
					//	self->NPC->timeOfDeath = level.time + 2000;
					//}
					GibEntity(self);
					G_DoGunDismemberment(self, self->pos1, means_of_death, hit_loc);
				}
				else
				{
					GibEntity(self);
					G_DoGunDismemberment(self, self->pos1, means_of_death, hit_loc);
				}
			}
			if (anim >= 0)
			{
				NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_RESTART | SETANIM_FLAG_HOLD);
			}
		}
		return;
	}

	// If the entity is in a vehicle.
	if (self->client && self->client->NPC_class != CLASS_VEHICLE && self->s.m_iVehicleNum != 0)
	{
		Vehicle_t* p_veh = g_entities[self->s.m_iVehicleNum].m_pVehicle;
		if (p_veh)
		{
			if (p_veh->m_pOldPilot != self
				&& p_veh->m_pPilot != self)
			{
				//whaaa?  I'm not on this bike?  er....
				assert(!!"How did we get to this point?");
			}
			else
			{
				// Get thrown out.
				p_veh->m_pVehicleInfo->Eject(p_veh, self, qtrue);

				// Now Send The Vehicle Flying To It's Death
				if (p_veh->m_pVehicleInfo->type == VH_SPEEDER && p_veh->m_pParentEntity && p_veh->m_pParentEntity->
					client)
				{
					gentity_t* parent = p_veh->m_pParentEntity;
					float cur_speed = VectorLength(parent->client->ps.velocity);

					// If Moving
					//-----------
					if (cur_speed > p_veh->m_pVehicleInfo->speedMax * 0.5f)
					{
						// Send The Bike Out Of Control
						//------------------------------
						p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, 10000);
						p_veh->m_ulFlags |= VEH_OUTOFCONTROL;
						VectorScale(parent->client->ps.velocity, 1.25f, parent->pos3);

						// Try To Accelerate A Slowing Moving Vehicle To Full Speed
						//----------------------------------------------------------
						if (cur_speed < p_veh->m_pVehicleInfo->speedMax * 0.9f)
						{
							VectorNormalize(parent->pos3);
							if (fabsf(parent->pos3[2]) < 0.3f)
							{
								VectorScale(parent->pos3, p_veh->m_pVehicleInfo->speedMax * 1.25f, parent->pos3);
							}
							else
							{
								VectorClear(parent->pos3);
							}
						}

						// Throw The Pilot
						//----------------
						if (parent->pos3[0] || parent->pos3[1])
						{
							vec3_t throw_dir;

							VectorCopy(parent->client->ps.velocity, throw_dir);
							VectorNormalize(throw_dir);
							throw_dir[2] += 0.3f; // up a little

							self->client->noRagTime = -1; // no ragdoll for you
							cur_speed /= 10.0f;
							if (cur_speed < 50.0)
							{
								cur_speed = 50.0f;
							}
							if (throw_dir[2] < 0.0f)
							{
								throw_dir[2] = fabsf(throw_dir[2]);
							}
							if (fabsf(throw_dir[0]) < 0.2f)
							{
								throw_dir[0] = Q_flrand(-0.5f, 0.5f);
							}
							if (fabsf(throw_dir[1]) < 0.2f)
							{
								throw_dir[1] = Q_flrand(-0.5f, 0.5f);
							}
							G_Throw(self, throw_dir, cur_speed);
						}
					}
				}
			}
		}
		else
		{
			assert(!!"How did we get to this point?");
		}
	}

#ifndef FINAL_BUILD
	if (d_saberCombat->integer && attacker && attacker->client)
	{
		gi.Printf(S_COLOR_YELLOW"combatant %s died, killer anim = %s\n", self->targetname, anim_table[attacker->client->ps.torsoAnim].name);
	}
#endif//FINAL_BUILD

	// Remove The Bubble Shield
	if (self->client && (self->flags & FL_SHIELDED || self->client->ps.powerups[PW_GALAK_SHIELD]))
	{
		TurnBarrierOff(self);
	}

	if (self->client &&
		(self->client->NPC_class == CLASS_BOBAFETT
			|| self->client->NPC_class == CLASS_MANDALORIAN
			|| self->client->NPC_class == CLASS_JANGO
			|| self->client->NPC_class == CLASS_JANGODUAL
			|| self->client->NPC_class == CLASS_ROCKETTROOPER || rocket_trooper_player(self)))
	{
		Boba_NoDeadFlameThrower(self);
	}

	if (self->NPC)
	{
		if (NAV::HasPath(self))
		{
			NAV::ClearPath(self);
		}
		if (self->NPC->troop)
		{
			NPC_LeaveTroop(self);
		}

		//clear charmed
		G_CheckCharmed(self);

		// Remove The Bubble Shield From The Assassin Droid
		if (self->client && self->client->NPC_class == CLASS_ASSASSIN_DROID && self->flags & FL_SHIELDED)
		{
			self->flags &= ~FL_SHIELDED;
			self->client->ps.stats[STAT_ARMOR] = 0;
			self->client->ps.powerups[PW_GALAK_SHIELD] = 0;
			gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], "force_shield", TURN_OFF);
		}
		if (self->client && self->client->NPC_class == CLASS_DROIDEKA && self->flags & FL_SHIELDED)
		{
			self->flags &= ~FL_SHIELDED;
			self->client->ps.stats[STAT_ARMOR] = 0;
			self->client->ps.powerups[PW_GALAK_SHIELD] = 0;
			gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], "force_shield", TURN_OFF);
		}

		if (self->client && self->client->NPC_class == CLASS_HOWLER)
		{
			G_StopEffect(G_EffectIndex("howler/sonic"), self->playerModel, self->genericBolt1, self->s.number);
		}

		if (self->client && jedi_waiting_ambush(self))
		{
			//ambushing trooper
			self->client->noclip = false;
		}
		NPC_FreeCombatPoint(self->NPC->combatPoint);
		if (self->NPC->group)
		{
			last_in_group = static_cast<qboolean>(self->NPC->group->numGroup < 2);
			AI_GroupMemberKilled(self);
			AI_DeleteSelfFromGroup(self);
		}

		if (self->NPC->tempGoal)
		{
			G_FreeEntity(self->NPC->tempGoal);
			self->NPC->tempGoal = nullptr;
		}
		if (self->s.eFlags & EF_LOCKED_TO_WEAPON)
		{
			// dumb, just get the NPC out of the chair
			extern void RunEmplacedWeapon(gentity_t * ent, usercmd_t * *ucmd);

			usercmd_t cmd = {}, * ad_cmd;

			//gentity_t *old = self->owner;

			if (self->owner)
			{
				self->owner->s.frame = self->owner->startFrame = self->owner->endFrame = 0;
				self->owner->svFlags &= ~SVF_ANIMATING;
			}

			cmd.buttons |= BUTTON_USE;
			ad_cmd = &cmd;
			RunEmplacedWeapon(self, &ad_cmd);
		}

		if (self->client->NPC_class == CLASS_BOBAFETT || self->client->NPC_class == CLASS_MANDALORIAN || self->client->
			NPC_class == CLASS_JANGO || self->client->NPC_class == CLASS_JANGODUAL
			|| self->client->NPC_class == CLASS_ROCKETTROOPER || rocket_trooper_player(self))
		{
			if (self->client->moveType == MT_FLYSWIM || self->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				jet_fly_stop(self);
			}
			if (self->client->jetPackOn)
			{
				//disable jetpack temporarily
				Jetpack_Off(self);
				client->jetPackToggleTime = level.time + Q_irand(3000, 10000);
			}
		}
		if (self->client->NPC_class == CLASS_ROCKETTROOPER)
		{
			self->client->ps.eFlags &= ~EF_SPOTLIGHT;
		}
		if (self->client->NPC_class == CLASS_SAND_CREATURE)
		{
			self->client->ps.eFlags &= ~EF_NODRAW;
			self->s.eFlags &= ~EF_NODRAW;
		}
		if (self->client->NPC_class == CLASS_RANCOR)
		{
			if (self->count)
			{
				Rancor_DropVictim(self);
			}
		}
		if (self->client->NPC_class == CLASS_WAMPA)
		{
			if (self->count)
			{
				if (self->activator && attacker == self->activator && means_of_death == MOD_SABER)
				{
					self->client->dismembered = false;
					//FIXME: the limb should just disappear, cuz I ate it
					G_DoDismemberment(self, self->currentOrigin, MOD_SABER, HL_ARM_RT, qtrue);
				}
				Wampa_DropVictim(self);
			}
		}
		if (self->NPC->aiFlags & NPCAI_HEAL_ROSH)
		{
			if (self->client->leader)
			{
				self->client->leader->flags &= ~FL_UNDYING;
				if (self->client->leader->client)
				{
					self->client->leader->client->ps.forcePowersKnown &= ~FORCE_POWERS_ROSH_FROM_TWINS;
				}
			}
		}
		if (self->client->ps.weapons[WP_SCEPTER])
		{
			G_StopEffect(G_EffectIndex("scepter/beam_warmup.efx"), self->weaponModel[1], self->genericBolt1,
				self->s.number);
			G_StopEffect(G_EffectIndex("scepter/beam.efx"), self->weaponModel[1], self->genericBolt1, self->s.number);
			G_StopEffect(G_EffectIndex("scepter/slam_warmup.efx"), self->weaponModel[1], self->genericBolt1,
				self->s.number);
			self->s.loopSound = 0;
		}
	}
	if (attacker && attacker->NPC && attacker->NPC->group && attacker->NPC->group->enemy == self)
	{
		attacker->NPC->group->enemy = nullptr;
	}
	if (self->s.weapon == WP_SABER)
	{
		holding_saber = qtrue;
	}
	if (self->client && self->client->ps.saberEntityNum != ENTITYNUM_NONE && self->client->ps.saberEntityNum > 0)
	{
		if (self->client->ps.saberInFlight)
		{
			//just drop it
			self->client->ps.saber[0].Deactivate();
		}
		else
		{
			if (g_saberPickuppableDroppedSabers->integer)
			{
				//always drop your sabers
				TossClientItems(self);
				self->client->ps.weapon = self->s.weapon = WP_NONE;
			}
			else if ((hit_loc != HL_HAND_RT && hit_loc != HL_CHEST_RT && hit_loc != HL_ARM_RT && hit_loc != HL_BACK_LT
				|| self->client->dismembered
				|| means_of_death != MOD_SABER) //if might get hand cut off, leave saber in hand
				&& holding_saber
				&& (Q_irand(0, 1)
					|| means_of_death == MOD_EXPLOSIVE
					|| means_of_death == MOD_REPEATER_ALT
					|| means_of_death == MOD_FLECHETTE_ALT
					|| means_of_death == MOD_ROCKET
					|| means_of_death == MOD_ROCKET_ALT
					|| means_of_death == MOD_CONC
					|| means_of_death == MOD_CONC_ALT
					|| means_of_death == MOD_THERMAL
					|| means_of_death == MOD_THERMAL_ALT
					|| means_of_death == MOD_DETPACK
					|| means_of_death == MOD_LASERTRIP
					|| means_of_death == MOD_LASERTRIP_ALT
					|| means_of_death == MOD_MELEE
					|| means_of_death == MOD_FORCE_GRIP
					|| means_of_death == MOD_KNOCKOUT
					|| means_of_death == MOD_CRUSH
					|| means_of_death == MOD_IMPACT
					|| means_of_death == MOD_FALLING
					|| means_of_death == MOD_EXPLOSIVE_SPLASH
					|| means_of_death == MOD_DESTRUCTION))
			{
				//drop it
				TossClientItems(self);
				self->client->ps.weapon = self->s.weapon = WP_NONE;
			}
			else
			{
				//just free it
				if (g_entities[self->client->ps.saberEntityNum].inuse)
				{
					G_FreeEntity(&g_entities[self->client->ps.saberEntityNum]);
				}
				self->client->ps.saberEntityNum = ENTITYNUM_NONE;
			}
		}
	}
	if (self->client && self->client->NPC_class == CLASS_SHADOWTROOPER)
	{
		//drop a force crystal
		if (Q_stricmpn("shadowtrooper", self->NPC_type, 13) == 0)
		{
			gitem_t* item = FindItemForAmmo(AMMO_FORCE);
			Drop_Item(self, item, 0, qtrue);
		}
	}
	//Use any target we had
	if (means_of_death != MOD_KNOCKOUT)
	{
		G_UseTargets(self, self);
	}

	if (attacker)
	{
		if (g_ffamode->integer)
		{
			// MESSAGE TO MIKE - We need some sort of CVAR to be activated when playing this FFA Mode
			// If we dont have a cvar it will effect other SP game stuff possibly ???
			// Can you activate g_ffamode in scripts somehow and then put everything in the cvar bracket to avoid any
			// conflictions

			// Change this to be how you need it

			if (self->client->ps.stats[STAT_HEALTH] <= 3)
			{
				if (attacker->client && !attacker->s.number)
				{
					if (self->client)
					{
						//killed a client
						if (self->client->playerTeam == TEAM_ENEMY
							|| self->client->playerTeam == TEAM_FREE
							|| self->NPC && self->NPC->charmedTime > level.time
							|| (self->client->playerTeam == TEAM_PLAYER && attacker->client->playerTeam == TEAM_ENEMY))
						{
							//killed an enemy
							attacker->client->sess.missionStats.enemiesKilled++;
						}
					}
					if (attacker != self)
					{
						G_TrackWeaponUsage(attacker, inflictor, 30, means_of_death);
					}
				}
				G_CheckAlmostVictoryScript(attacker);
			}
		}
		else
		{
			if (attacker->client && !attacker->s.number)
			{
				if (self->client)
				{
					//killed a client
					if (self->client->playerTeam == TEAM_ENEMY
						|| self->client->playerTeam == TEAM_FREE
						|| self->NPC && self->NPC->charmedTime > level.time
						|| (self->client->playerTeam == TEAM_PLAYER && attacker->client->playerTeam == TEAM_ENEMY))
					{
						//killed an enemy
						attacker->client->sess.missionStats.enemiesKilled++;
					}
				}
				if (attacker != self)
				{
					G_TrackWeaponUsage(attacker, inflictor, 30, means_of_death);
				}
			}
			G_CheckVictoryScript(attacker);
		}

		//player killing a jedi with a lightsaber spawns a matrix-effect entity
		if (d_slowmodeath->integer)
		{
			if (!self->s.number)
			{
				//what the hell, always do slow-mo when player dies
				//FIXME: don't do this when crushed to death?
				if (means_of_death == MOD_FALLING && self->client->ps.groundEntityNum == ENTITYNUM_NONE)
				{
					//falling to death, have not hit yet
					G_StartMatrixEffect(self, MEF_NO_VERTBOB | MEF_HIT_GROUND_STOP | MEF_MULTI_SPIN, 10000, 0.25f);
				}
				else if (means_of_death != MOD_CRUSH)
				{
					//for all deaths except being crushed
					G_StartMatrixEffect(self);
				}
			}
			else if (d_slowmodeath->integer < 4)
			{
				//any jedi killed by player-saber
				if (d_slowmodeath->integer < 3)
				{
					//must be the last jedi in the room
					if (!G_JediInRoom(attacker->currentOrigin))
					{
						last_in_group = qtrue;
					}
					else
					{
						last_in_group = qfalse;
					}
				}
				if (!attacker->s.number
					&& (holding_saber || self->client->NPC_class == CLASS_WAMPA)
					&& means_of_death == MOD_SABER
					&& attacker->client
					&& attacker->client->ps.weapon == WP_SABER
					&& !attacker->client->ps.saberInFlight
					//FIXME: if dualSabers, should still do slowmo if this killing blow was struck with the left-hand saber...
					&& (d_slowmodeath->integer > 2 || last_in_group))
					//either slow mo death level 3 (any jedi) or 2 and I was the last jedi in the room
				{
					//Matrix!
					if (attacker->client->ps.torsoAnim == BOTH_A6_SABERPROTECT)
					{
						//don't override the range and vertbob
						G_StartMatrixEffect(self, MEF_NO_RANGEVAR | MEF_NO_VERTBOB);
					}
					else
					{
						G_StartMatrixEffect(self);
					}
				}
			}
			else
			{
				//all player-saber kills
				if (!attacker->s.number
					&& means_of_death == MOD_SABER
					&& attacker->client
					&& attacker->client->ps.weapon == WP_SABER
					&& !attacker->client->ps.saberInFlight
					&& (d_slowmodeath->integer > 4 || last_in_group || holding_saber || self->client->NPC_class ==
						CLASS_WAMPA))
					//either slow mo death level 5 (any enemy) or 4 and I was the last in my group or I'm a saber user
				{
					//Matrix!
					if (attacker->client->ps.torsoAnim == BOTH_A6_SABERPROTECT)
					{
						//don't override the range and vertbob
						G_StartMatrixEffect(self, MEF_NO_RANGEVAR | MEF_NO_VERTBOB);
					}
					else
					{
						G_StartMatrixEffect(self);
					}
				}
			}
		}
	}

	self->enemy = attacker;
	self->client->renderInfo.lookTarget = ENTITYNUM_NONE;

	self->client->ps.persistant[PERS_KILLED]++;
	if (self->client->playerTeam == TEAM_PLAYER)
	{
		// Don't count player
		if ((g_entities[0].inuse && g_entities[0].client) && (self->s.number != 0))
		{
			//add to the number of teammates lost
			g_entities[0].client->ps.persistant[PERS_TEAMMATES_KILLED]++;
		}
		else // Player died, fire off scoreboard soon
		{
			cg.missionStatusDeadTime = level.time + 1000; // Too long?? Too short??
			cg.zoomMode = 0; // turn off zooming when we die
		}
	}

	if (self->s.number == 0 && attacker)
	{
		G_MakeTeamVulnerable();
	}

	if (attacker && attacker->client)
	{
		if (attacker == self || OnSameTeam(self, attacker))
		{
			AddScore(attacker, -1);
		}
		else
		{
			AddScore(attacker, 1);
			AddFatigueKillBonus(attacker, self, means_of_death);
		}
	}
	else
	{
		AddScore(self, -1);
	}

	// if client is in a nodrop area, don't drop anything
	contents = gi.pointcontents(self->currentOrigin, -1);

	if (!holding_saber
		&& !(contents & CONTENTS_NODROP)
		&& means_of_death != MOD_SNIPER
		&& (!self->client || self->client->NPC_class != CLASS_GALAKMECH))
	{
		TossClientItems(self);
	}

	if (means_of_death == MOD_SNIPER)
	{
		//I was disintegrated
		if (self->message)
		{
			//I was holding a key
			//drop the key
			G_DropKey(self);
		}
	}

	if (holding_saber)
	{
		//never drop a lightsaber!
		if (self->client->ps.SaberActive())
		{
			self->client->ps.SaberDeactivate();
			G_SoundIndexOnEnt(self, CHAN_AUTO, self->client->ps.saber[0].soundOff);
		}
	}
	else if (self->s.weapon != WP_BRYAR_PISTOL && self->s.weapon != WP_SBD_BLASTER && self->s.weapon != WP_JAWA
		&& self->s.weapon != WP_DUAL_PISTOL && self->s.weapon != WP_DUAL_CLONEPISTOL && self->s.weapon != WP_WRIST_BLASTER && self->s.weapon != WP_DROIDEKA)
	{
		//since player can't pick up bryar pistols, never drop those
		self->s.weapon = WP_NONE;
		G_RemoveWeaponModels(self);
	}

	self->s.powerups &= ~PW_REMOVE_AT_DEATH; //removes everything but electricity and force push

	if (!self->s.number)
	{
		//player
		self->contents = CONTENTS_CORPSE;
		self->maxs[2] = -8;
	}
	self->clipmask &= ~(CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP); //so dead NPC can fly off ledges

	//FACING==========================================================
	if (attacker && self->s.number == 0)
	{
		self->client->ps.stats[STAT_DEAD_YAW] = AngleNormalize180(self->client->ps.viewangles[YAW]);
	}
	self->currentAngles[PITCH] = 0;
	self->currentAngles[ROLL] = 0;
	if (self->NPC)
	{
		self->NPC->desiredYaw = 0;
		self->NPC->desiredPitch = 0;
		self->NPC->confusionTime = 0;
		self->NPC->charmedTime = 0;
		if (self->ghoul2.size())
		{
			if (self->chestBolt != -1)
			{
				G_StopEffect("force/rage2", self->playerModel, self->chestBolt, self->s.number);
			}
			if (self->headBolt != -1)
			{
				G_StopEffect("force/confusion", self->playerModel, self->headBolt, self->s.number);
			}
			WP_StopForceHealEffects(self);
		}
	}
	self->client->ps.stasisTime = 0;
	self->client->ps.stasisJediTime = 0;
	VectorCopy(self->currentAngles, self->client->ps.viewangles);
	//Make sure the jetpack is turned off.
	Jetpack_Off(self);
	self->client->isHacking = 0;
	self->client->ps.hackingTime = 0;
	//FACING==========================================================
	if (player && player->client && player->client->ps.viewEntity == self->s.number)
	{
		//I was the player's viewentity and I died, kick him back to his normal view
		G_ClearViewEntity(player);
	}
	else if (!self->s.number && self->client->ps.viewEntity > 0 && self->client->ps.viewEntity < ENTITYNUM_NONE)
	{
		G_ClearViewEntity(self);
	}
	else if (!self->s.number && self->client->ps.viewEntity > 0 && self->client->ps.viewEntity < ENTITYNUM_NONE)
	{
		G_ClearViewEntity(self);
	}

	self->s.loopSound = 0;

	// remove powerups
	memset(self->client->ps.powerups, 0, sizeof self->client->ps.powerups);

	if (self->client->ps.eFlags & EF_HELD_BY_RANCOR
		|| self->client->ps.eFlags & EF_HELD_BY_SAND_CREATURE
		|| self->client->ps.eFlags & EF_HELD_BY_WAMPA)
	{
		//do nothing special here
	}
	else if (self->client->NPC_class == CLASS_MARK1)
	{
		Mark1_die(self, inflictor, attacker, damage, means_of_death, d_flags, hit_loc);
	}
	else if (self->client->NPC_class == CLASS_INTERROGATOR)
	{
		Interrogator_die(self, inflictor, attacker, damage, means_of_death, d_flags, hit_loc);
	}
	else if (self->client->NPC_class == CLASS_GALAKMECH)
	{
		//FIXME: need keyframed explosions?
		NPC_SetAnim(self, SETANIM_BOTH, BOTH_DEATH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		G_AddEvent(self, Q_irand(EV_DEATH1, EV_DEATH3), self->health);
	}
	else if (self->client->NPC_class == CLASS_ATST)
	{
		//FIXME: need keyframed explosions
		if (!self->s.number)
		{
			G_DrivableATSTDie(self);
		}
		anim = PM_PickAnim(self, BOTH_DEATH1, BOTH_DEATH25); //initialize to good data
		if (anim != -1)
		{
			NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}
	else if (self->client->NPC_class == CLASS_DROIDEKA)
	{
		anim = PM_PickAnim(self, BOTH_DEATH1, BOTH_DEATH1); //initialize to good data
		if (anim != -1)
		{
			NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}
	else if (self->s.number && self->message && means_of_death != MOD_SNIPER && means_of_death != MOD_PROJECTION_END)
	{
		//imp with a key on his arm
		//pick a death anim that leaves key visible
		switch (Q_irand(0, 3))
		{
		case 0:
			anim = BOTH_DEATH4;
			break;
		case 1:
			anim = BOTH_DEATH21;
			break;
		case 2:
			anim = BOTH_DEATH17;
			break;
		case 3:
		default:
			anim = BOTH_DEATH18;
			break;
		}
		//FIXME: verify we have this anim?
		NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		if (means_of_death == MOD_KNOCKOUT || means_of_death == MOD_MELEE)
		{
			G_AddEvent(self, EV_JUMP, 0);
		}
		else if (means_of_death == MOD_FORCE_DRAIN)
		{
			G_AddEvent(self, EV_WATER_DROWN, 0);
		}
		else if (means_of_death == MOD_GAS)
		{
			G_AddEvent(self, EV_WATER_DROWN, 0);
		}
		else
		{
			G_AddEvent(self, Q_irand(EV_DEATH1, EV_DEATH3), self->health);
		}
	}
	else if (means_of_death == MOD_FALLING || self->client->ps.legsAnim == BOTH_FALLDEATH1INAIR && self->client->ps.
		torsoAnim == BOTH_FALLDEATH1INAIR || self->client->ps.legsAnim == BOTH_FALLDEATH1 && self->client->ps.
		torsoAnim == BOTH_FALLDEATH1)
	{
		//FIXME: no good way to predict you're going to fall to your death... need falling bushes/triggers?
		if (self->client->ps.groundEntityNum == ENTITYNUM_NONE //in the air
			&& self->client->ps.velocity[2] < 0 //falling
			&& self->client->ps.legsAnim != BOTH_FALLDEATH1INAIR //not already in falling loop
			&& self->client->ps.torsoAnim != BOTH_FALLDEATH1INAIR) //not already in falling loop
		{
			NPC_SetAnim(self, SETANIM_BOTH, BOTH_FALLDEATH1INAIR, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			if (!self->NPC)
			{
				G_SoundOnEnt(self, CHAN_VOICE, "*falling1.wav"); //CHAN_VOICE_ATTEN
			}
			else if (!(self->NPC->aiFlags & NPCAI_DIE_ON_IMPACT))
			{
				G_SoundOnEnt(self, CHAN_VOICE, "*falling1.wav"); //CHAN_VOICE_ATTEN
				//so we don't do this again
				self->NPC->aiFlags |= NPCAI_DIE_ON_IMPACT;
				//self->client->ps.gravity *= 0.5;//Fall a bit slower
				self->client->ps.friction = 1;
			}
		}
		else
		{
			int death_anim = BOTH_FALLDEATH1LAND;
			if (PM_InOnGroundAnim(&self->client->ps))
			{
				if (AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0)
				{
					death_anim = BOTH_DEATH_LYING_UP; //# Death anim when lying on back
				}
				else
				{
					death_anim = BOTH_DEATH_LYING_DN; //# Death anim when lying on front
				}
			}
			else if (PM_InKnockDown(&self->client->ps))
			{
				if (AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0)
				{
					death_anim = BOTH_DEATH_FALLING_UP; //# Death anim when falling on back
				}
				else
				{
					death_anim = BOTH_DEATH_FALLING_DN; //# Death anim when falling on face
				}
			}
			NPC_SetAnim(self, SETANIM_BOTH, death_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			//HMM: check for nodrop?
			G_SoundOnEnt(self, CHAN_BODY, "sound/player/fallsplat.wav");
			if (gi.VoiceVolume[self->s.number]
				&& self->NPC && self->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
			{
				//I was talking, so cut it off... with a jump sound?
				G_SoundOnEnt(self, CHAN_VOICE_ATTEN, "*pain100.wav");
			}
		}
	}
	else
	{
		// normal death
		anim = G_CheckSpecialDeathAnim(self);
		if (anim == -1)
		{
			if (PM_InOnGroundAnim(&self->client->ps) && PM_HasAnimation(self, BOTH_LYINGDEATH1))
			{
				//on ground, need different death anim
				anim = BOTH_LYINGDEATH1;
			}
			else if (self->client->AmputateTime > 0)
			{
				//anim = BOTH_DEATH_LYING_UP;
				//do nothing special here
			}
			else if (means_of_death == MOD_TRIGGER_HURT && self->s.powerups & 1 << PW_SHOCKED)
			{
				//electrocuted
				anim = BOTH_DEATH17;
			}
			else if (means_of_death == MOD_WATER || means_of_death == MOD_GAS || means_of_death == MOD_FORCE_DRAIN)
			{
				//drowned
				anim = BOTH_DEATH17;
			}
			else if (means_of_death != MOD_SNIPER && means_of_death != MOD_PROJECTION_END //disintegrates
				&& means_of_death != MOD_CONC_ALT) //does its own death throw
			{
				cliff_fall = G_CheckLedgeDive(self, 128, self->client->ps.velocity, qtrue, qfalse);
				if (cliff_fall == 2)
				{
					if (!FlyingCreature(self) && g_gravity->value > 0)
					{
						if (!self->NPC)
						{
							G_SoundOnEnt(self, CHAN_VOICE, "*falling1.wav"); //CHAN_VOICE_ATTEN
						}
						else if (!(self->NPC->aiFlags & NPCAI_DIE_ON_IMPACT))
						{
							G_SoundOnEnt(self, CHAN_VOICE, "*falling1.wav"); //CHAN_VOICE_ATTEN
							self->NPC->aiFlags |= NPCAI_DIE_ON_IMPACT;
							self->client->ps.friction = 0;
						}
					}
				}
				if (self->client->ps.pm_time > 0 && self->client->ps.pm_flags & PMF_TIME_KNOCKBACK && self->client->ps.
					velocity[2] > 0)
				{
					vec3_t throwdir, forward;

					AngleVectors(self->currentAngles, forward, nullptr, nullptr);
					const float thrown = VectorNormalize2(self->client->ps.velocity, throwdir);
					const float dot = DotProduct(forward, throwdir);
					if (thrown > 100)
					{
						if (dot > 0.3)
						{
							//falling forward
							if (cliff_fall == 2 && PM_HasAnimation(self, BOTH_FALLDEATH1))
							{
								anim = BOTH_FALLDEATH1;
							}
							else
							{
								switch (Q_irand(0, 7))
								{
								case 0:
								case 1:
									anim = BOTH_DEATH4;
									break;
								case 2:
									anim = BOTH_DEATH16;
									break;
								case 3:
								case 4:
								case 5:
									anim = BOTH_DEATH5;
									break;
								case 6:
									anim = BOTH_DEATH8;
									break;
								case 7:
									anim = BOTH_DEATH14;
									break;
								default:;
								}
								if (PM_HasAnimation(self, anim))
								{
									self->client->ps.gravity *= 0.8;
									self->client->ps.friction = 0;
									if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100)
									{
										self->client->ps.velocity[2] = 100;
									}
								}
								else
								{
									anim = -1;
								}
							}
						}
						else if (dot < -0.3)
						{
							if (thrown >= 250 && !Q_irand(0, 3))
							{
								if (Q_irand(0, 1))
								{
									anim = BOTH_DEATHBACKWARD1;
								}
								else
								{
									anim = BOTH_DEATHBACKWARD2;
								}
							}
							else
							{
								switch (Q_irand(0, 7))
								{
								case 0:
								case 1:
									anim = BOTH_DEATH1;
									break;
								case 2:
								case 3:
									anim = BOTH_DEATH2;
									break;
								case 4:
								case 5:
									anim = BOTH_DEATH22;
									break;
								case 6:
								case 7:
									anim = BOTH_DEATH23;
									break;
								default:;
								}
							}
							if (PM_HasAnimation(self, anim))
							{
								self->client->ps.gravity *= 0.8;
								self->client->ps.friction = 0;
								if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100)
								{
									self->client->ps.velocity[2] = 100;
								}
							}
							else
							{
								anim = -1;
							}
						}
						else
						{
							//falling to one of the sides
							if (cliff_fall == 2 && PM_HasAnimation(self, BOTH_FALLDEATH1))
							{
								anim = BOTH_FALLDEATH1;
								if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100)
								{
									self->client->ps.velocity[2] = 100;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			special_anim = qtrue;
		}

		if (anim == -1)
		{
			if (means_of_death == MOD_ELECTROCUTE
				|| means_of_death == MOD_FORCE_DRAIN && self->s.eFlags & EF_FORCE_DRAINED)
			{
				//electrocuted
				anim = BOTH_DEATH17;
			}
			else if (means_of_death == MOD_CRUSH && self->s.eFlags & EF_FORCE_GRIPPED)
			{
				//choked to death
				if (self->client->ps.groundEntityNum == ENTITYNUM_NONE)
				{
					anim = BOTH_DEATH10;
				}
				else
				{
					anim = BOTH_DEATH17;
				}
			}
			else if (means_of_death == MOD_CRUSH && self->s.eFlags & EF_FORCE_GRASPED)
			{
				//choked to death
				if (self->client->ps.groundEntityNum == ENTITYNUM_NONE)
				{
					anim = BOTH_DEATH10;
				}
				else
				{
					anim = BOTH_DEATH17;
				}
			}
		}
		if (anim == -1)
		{
			anim = PM_PickAnim(self, BOTH_DEATH1, BOTH_DEATH25); //initialize to good data
			//TEMP HACK: these spinny deaths should happen less often
			if ((anim == BOTH_DEATH8 || anim == BOTH_DEATH14) && Q_irand(0, 1))
			{
				anim = PM_PickAnim(self, BOTH_DEATH1, BOTH_DEATH25); //initialize to good data
			}
		}
		else if (!PM_HasAnimation(self, anim))
		{
			//crap, still missing an anim, so pick one that we do have
			anim = PM_PickAnim(self, BOTH_DEATH1, BOTH_DEATH25); //initialize to good data
		}

		if (means_of_death == MOD_KNOCKOUT)
		{
			//FIXME: knock-out sound, and don't remove me
			G_AddEvent(self, EV_JUMP, 0);
			G_UseTargets2(self, self, self->target2);
			G_AlertTeam(self, attacker, 512, 32);
			if (self->NPC)
			{
				//stick around for a while
				self->NPC->timeOfDeath = level.time + 10000;
			}
		}
		else if (means_of_death == MOD_GAS || means_of_death == MOD_FORCE_DRAIN)
		{
			G_AddEvent(self, EV_WATER_DROWN, 0);
			G_AlertTeam(self, attacker, 512, 32);
			if (self->NPC)
			{
				//stick around for a while
				self->NPC->timeOfDeath = level.time + 10000;
			}
		}
		else if (means_of_death == MOD_SNIPER || means_of_death == MOD_PROJECTION_END)
		{
			vec3_t spot;

			VectorCopy(self->currentOrigin, spot);

			self->flags |= FL_DISINTEGRATED;
			self->svFlags |= SVF_BROADCAST;
			gentity_t* tent = G_TempEntity(spot, EV_DISINTEGRATION);
			tent->s.eventParm = PW_DISRUPTION;
			tent->svFlags |= SVF_BROADCAST;
			tent->owner = self;

			G_AlertTeam(self, attacker, 512, 88);

			if (self->playerModel >= 0)
			{
				// don't let 'em animate
				gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->rootBone, cg.time);
				gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->motionBone, cg.time);
				gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->lowerLumbarBone, cg.time);
				anim = -1;
			}

			//not solid anymore
			self->contents = 0;
			self->maxs[2] = -8;

			if (self->NPC)
			{
				//need to pad deathtime some to stick around long enough for death effect to play
				self->NPC->timeOfDeath = level.time + 2000;
			}
		}
		else
		{
			if (hit_loc == HL_HEAD
				&& !(d_flags & DAMAGE_RADIUS)
				&& means_of_death != MOD_REPEATER_ALT
				&& means_of_death != MOD_FLECHETTE_ALT
				&& means_of_death != MOD_ROCKET
				&& means_of_death != MOD_ROCKET_ALT
				&& means_of_death != MOD_CONC
				&& means_of_death != MOD_THERMAL
				&& means_of_death != MOD_THERMAL_ALT
				&& means_of_death != MOD_DETPACK
				&& means_of_death != MOD_LASERTRIP
				&& means_of_death != MOD_LASERTRIP_ALT
				&& means_of_death != MOD_EXPLOSIVE
				&& means_of_death != MOD_EXPLOSIVE_SPLASH
				&& means_of_death != MOD_DESTRUCTION)
			{
				//no sound when killed by headshot (explosions don't count)
				G_AlertTeam(self, attacker, 512, 0);
				if (gi.VoiceVolume[self->s.number])
				{
					//I was talking, so cut it off... with a jump sound?
					if (G_IsRidingVehicle(self))
					{
					}
					else
					{
						G_SoundOnEnt(self, CHAN_VOICE, "*jump1.wav");
					}
				}
			}
			else
			{
				if (self->client->ps.eFlags & EF_FORCE_GRIPPED || self->client->ps.eFlags & EF_FORCE_DRAINED || self->
					client->ps.eFlags & EF_FORCE_GRASPED)
				{
					//killed while gripped - no loud scream
					G_AlertTeam(self, attacker, 512, 32);
				}
				else if (cliff_fall != 2)
				{
					if (means_of_death == MOD_KNOCKOUT || means_of_death == MOD_MELEE)
					{
						G_AddEvent(self, EV_JUMP, 0);
					}
					else if (means_of_death == MOD_GAS || means_of_death == MOD_FORCE_DRAIN)
					{
						G_AddEvent(self, EV_WATER_DROWN, 0);
					}
					else
					{
						G_AddEvent(self, Q_irand(EV_DEATH1, EV_DEATH3), self->health);
					}
					G_DeathAlert(self, attacker);
				}
				else
				{
					//screaming death is louder
					G_AlertTeam(self, attacker, 512, 1024);
				}
			}
		}

		if (attacker && attacker->s.number == 0)
		{
			//killed by player
			//FIXME: this should really be wherever my body comes to rest...
			AddSightEvent(attacker, self->currentOrigin, 384, AEL_DISCOVERED, 10);
			//FIXME: danger event so that others will run away from this area since it's obviously dangerous
		}

		if (anim >= 0) //can be -1 if it fails, -2 if it's already in a death anim
		{
			NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}

	//do any dismemberment if there's any to do...
	if (d_flags & DAMAGE_DISMEMBER
		&& G_DoDismemberment(self, self->pos1, means_of_death, hit_loc)
		&& !special_anim)
	{
		//we did dismemberment and our death anim is okay to override
		if (hit_loc == HL_HAND_RT && self->locationDamage[hit_loc] >= Q3_INFINITE && cliff_fall != 2 && self->client->ps
			.
			groundEntityNum != ENTITYNUM_NONE)
		{
			//just lost our right hand and we're on the ground, use the special anim
			NPC_SetAnim(self, SETANIM_BOTH, BOTH_RIGHTHANDCHOPPEDOFF, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}

	if (means_of_death == MOD_BODYSHOT && !NPC_IsNotDismemberable(self))
	{
		GibEntity(self);
		G_DoGunDismemberment(self, self->pos1, means_of_death, hit_loc);
	}
	else if (means_of_death == MOD_HEADSHOT && !NPC_IsNotDismemberable(self))
	{
		GibEntity_Headshot(self);
		G_DoGunDismemberment(self, self->pos1, means_of_death, hit_loc);
	}
	else
	{
		self->client->noHead = qfalse;
	}

	// don't allow player to respawn for a few seconds
	self->client->respawnTime = level.time + 2000; //self->client->ps.legsAnimTimer;

	//rww - RAGDOLL_BEGIN
	if (g_broadsword->integer && self->client->NPC_class != CLASS_SBD && self->client->NPC_class != CLASS_DROIDEKA)
	{
		if (self->client && (!self->NPC || !g_standard_humanoid(self)))
		{
			PM_SetLegsAnimTimer(self, &self->client->ps.legsAnimTimer, -1);
			PM_SetTorsoAnimTimer(self, &self->client->ps.torsoAnimTimer, -1);
		}
	}
	else
	{
		if (self->client)
		{
			PM_SetLegsAnimTimer(self, &self->client->ps.legsAnimTimer, -1);
			PM_SetTorsoAnimTimer(self, &self->client->ps.torsoAnimTimer, -1);
		}
	}
	//rww - RAGDOLL_END

	//Flying creatures should drop when killed
	//FIXME: This may screw up certain things that expect to float even while dead <?>
	self->svFlags &= ~SVF_CUSTOM_GRAVITY;

	self->client->ps.pm_type = PM_DEAD;
	self->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
	//need to update STAT_HEALTH here because ClientThink_real for self may happen before STAT_HEALTH is updated from self->health and pmove will stomp death anim with a move anim
	self->client->ps.stats[STAT_HEALTH] = self->health;

	if (self->NPC)
	{
		//If an NPC, make sure we start running our scripts again- this gets set to infinite while we fall to our deaths
		self->NPC->nextBStateThink = level.time;
	}

	if (G_ActivateBehavior(self, BSET_DEATH))
	{
		death_script = qtrue;
	}

	if (self->NPC && self->NPC->scriptFlags & SCF_FFDEATH)
	{
		if (G_ActivateBehavior(self, BSET_FFDEATH))
		{
			//FIXME: should running this preclude running the normal deathscript?
			death_script = qtrue;
		}
		G_UseTargets2(self, self, self->target4);
	}

	if (!death_script && !(self->svFlags & SVF_KILLED_SELF))
	{
		//Should no longer run scripts
		//WARNING!!! DO NOT DO THIS WHILE RUNNING A SCRIPT, ICARUS WILL HANDLE IT, but it's bad
		Quake3Game()->FreeEntity(self);
	}

	// Free up any timers we may have on us.
	TIMER_Clear(self->s.number);

	// Set pending objectives to failed
	OBJ_SetPendingObjectives(self);

	gi.linkentity(self);

	self->bounceCount = -1; // This is a cheap hack for optimizing the pointcontents check in deadthink
	if (self->NPC)
	{
		self->NPC->timeOfDeath = level.time; //this will change - used for debouncing post-death events
		self->s.time = level.time; //this will not chage- this is actual time of death
	}

	// Start any necessary death fx for this entity
	DeathFX(self);
}

qboolean G_CheckForStrongAttackMomentum(const gentity_t* self)
{
	//see if our saber attack has too much momentum to be interrupted
	if (PM_PowerLevelForSaberAnim(&self->client->ps) > FORCE_LEVEL_2)
	{
		//strong attacks can't be interrupted
		if (PM_InAnimForSaberMove(self->client->ps.torsoAnim, self->client->ps.saber_move))
		{
			//our saber_move was not already interupted by some other anim (like pain)
			if (PM_SaberInStart(self->client->ps.saber_move))
			{
				const float animLength = PM_AnimLength(self->client->clientInfo.animFileIndex,
					static_cast<animNumber_t>(self->client->ps.torsoAnim));
				if (animLength - self->client->ps.torsoAnimTimer > 750)
				{
					//start anim is already 3/4 of a second into it, can't interrupt it now
					return qtrue;
				}
			}
			else if (PM_SaberInReturn(self->client->ps.saber_move))
			{
				if (self->client->ps.torsoAnimTimer > 750)
				{
					//still have a good amount of time left in the return anim, can't interrupt it
					return qtrue;
				}
			}
			else
			{
				//cannot interrupt actual transitions and attacks
				return qtrue;
			}
		}
	}
	return qfalse;
}

void PlayerPain(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, const vec3_t point, const int damage,
	const int mod, const int hit_loc)
{
	if (self->client->NPC_class == CLASS_ATST)
	{
		//different kind of pain checking altogether
		G_ATSTCheckPain(self, attacker, point, damage, mod, hit_loc);
		const int blaster_test = gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel],
			"head_light_blaster_cann");
		const int chargerTest = gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel],
			"head_concussion_charger");
		if (blaster_test && chargerTest)
		{
			//lost both side guns
			//take away that weapon
			self->client->ps.weapons[WP_ATST_SIDE] = 0;
			//switch to primary guns
			if (self->client->ps.weapon == WP_ATST_SIDE)
			{
				CG_ChangeWeapon(WP_ATST_MAIN);
			}
		}
	}
	else
	{
		// play an apropriate pain sound
		if (level.time > self->painDebounceTime && !(self->flags & FL_GODMODE))
		{
			//first time hit this frame and not in godmode
			self->client->ps.damageEvent++;
			if (!Q3_TaskIDPending(self, TID_CHAN_VOICE))
			{
				if (self->client->damage_blood)
				{
					//took damage myself, not just armor
					if (mod == MOD_GAS)
					{
						//SIGH... because our choke sounds are inappropriately long, I have to debounce them in code!
						if (TIMER_Done(self, "gasChokeSound"))
						{
							TIMER_Set(self, "gasChokeSound", Q_irand(1000, 2000));
							G_SpeechEvent(self, Q_irand(EV_CHOKE1, EV_CHOKE3));
						}
						if (self->painDebounceTime <= level.time)
						{
							self->painDebounceTime = level.time + 50;
						}
					}
					else
					{
						G_AddEvent(self, EV_PAIN, self->health);
					}
				}
			}
		}
		if (damage != -1 && (mod == MOD_MELEE || damage == 0 || Q_irand(0, 10) <= damage && self->client
			->damage_blood))
		{
			//-1 == don't play pain anim
			if (((mod == MOD_SABER || mod == MOD_MELEE) && self->client->damage_blood || mod == MOD_CRUSH) && (self->s
				.weapon == WP_SABER || self->s.weapon == WP_MELEE || cg.renderingThirdPerson))
				//FIXME: not only if using saber, but if in third person at all?  But then 1st/third person functionality is different...
			{
				//FIXME: only strong-level saber attacks should make me play pain anim?
				if (!G_CheckForStrongAttackMomentum(self) && !PM_SpinningSaberAnim(self->client->ps.legsAnim)
					&& !pm_saber_in_special_attack(self->client->ps.torsoAnim)
					&& !PM_InKnockDown(&self->client->ps))
				{
					//strong attacks and spins cannot be interrupted by pain, no pain when in knockdown
					int parts;
					if (self->client->ps.groundEntityNum != ENTITYNUM_NONE &&
						!PM_SpinningSaberAnim(self->client->ps.legsAnim) &&
						!PM_FlippingAnim(self->client->ps.legsAnim) &&
						!PM_InSpecialJump(self->client->ps.legsAnim) &&
						!PM_RollingAnim(self->client->ps.legsAnim) &&
						!PM_CrouchAnim(self->client->ps.legsAnim) &&
						!PM_RunningAnim(self->client->ps.legsAnim))
					{
						//if on a surface and not in a spin or flip, play full body pain
						parts = SETANIM_BOTH;
					}
					else
					{
						//play pain just in torso
						parts = SETANIM_TORSO;
					}
					if (self->painDebounceTime < level.time)
					{
						//temp HACK: these are the only 2 pain anims that look good when holding a saber
						NPC_SetAnim(self, parts, PM_PickAnim(self, BOTH_PAIN2, BOTH_PAIN3),
							SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						self->client->ps.saber_move = LS_READY; //don't finish whatever saber move you may have been in
						//WTF - insn't working
						if (self->health < 10 && d_slowmodeath->integer > 5)
						{
							G_StartMatrixEffect(self);
						}
					}
					if (parts == SETANIM_BOTH && damage > 30 || self->painDebounceTime > level.time && damage > 10)
					{
						//took a lot of damage in 1 hit //or took 2 hits in quick succession
						self->aimDebounceTime = level.time + self->client->ps.torsoAnimTimer;
						self->client->ps.pm_time = self->client->ps.torsoAnimTimer;
						self->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
					}
					self->client->ps.weaponTime = self->client->ps.torsoAnimTimer;
					self->attackDebounceTime = level.time + self->client->ps.torsoAnimTimer;
				}
				self->painDebounceTime = level.time + self->client->ps.torsoAnimTimer;
			}
		}
	}
	if (mod != MOD_GAS && self->painDebounceTime <= level.time)
	{
		self->painDebounceTime = level.time + 700;
	}
}

/*
================
CheckArmor
================
*/
static int CheckArmor(const gentity_t* ent, const int damage, const int dflags, const int mod)
{
	int save;

	if (!damage)
		return 0;

	gclient_t* client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
	{
		// If this isn't a vehicle, leave.
		if (client->NPC_class != CLASS_VEHICLE)
		{
			return 0;
		}
	}

	if (client->NPC_class == CLASS_ASSASSIN_DROID || client->NPC_class == CLASS_DROIDEKA)
	{
		// The Assassin Always Completely Ignores These Damage Types
		//-----------------------------------------------------------
		if (mod == MOD_GAS || mod == MOD_IMPACT || mod == MOD_LAVA || mod == MOD_SLIME || mod == MOD_WATER ||
			mod == MOD_FORCE_GRIP || mod == MOD_FORCE_DRAIN || mod == MOD_SEEKER || mod == MOD_MELEE ||
			mod == MOD_BOWCASTER || mod == MOD_BRYAR || mod == MOD_BRYAR_ALT || mod == MOD_BLASTER || mod ==
			MOD_BLASTER_ALT ||
			mod == MOD_SNIPER || mod == MOD_BOWCASTER || mod == MOD_BOWCASTER_ALT || mod == MOD_REPEATER || mod ==
			MOD_REPEATER_ALT)
		{
			return damage;
		}

		// The Assassin Always Takes Half Of These Damage Types
		//------------------------------------------------------
		if (mod == MOD_GAS || mod == MOD_IMPACT || mod == MOD_LAVA || mod == MOD_SLIME || mod == MOD_WATER)
		{
			return damage / 2;
		}

		// If The Shield Is Not On, No Additional Protection
		//---------------------------------------------------
		if (!(ent->flags & FL_SHIELDED))
		{
			// He Does Ignore Half Saber Damage, Even Shield Down
			//----------------------------------------------------
			if (mod == MOD_SABER)
			{
				return static_cast<int>(static_cast<float>(damage) * 0.75f);
			}
			return 0;
		}

		// If The Shield Is Up, He Ignores These Damage Types
		//----------------------------------------------------
		if (mod == MOD_SABER || mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT || mod == MOD_DISRUPTOR)
		{
			return damage;
		}

		// The Demp Completely Destroys The Shield
		//-----------------------------------------
		if (mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT || mod == MOD_LIGHTNING_STRIKE)
		{
			client->ps.stats[STAT_ARMOR] = 0;
			return 0;
		}

		// Otherwise, The Shield Absorbs As Much Damage As Possible
		//----------------------------------------------------------
		const int previous_armor = client->ps.stats[STAT_ARMOR];
		client->ps.stats[STAT_ARMOR] -= damage;
		if (client->ps.stats[STAT_ARMOR] < 0)
		{
			client->ps.stats[STAT_ARMOR] = 0;
		}
		return previous_armor - client->ps.stats[STAT_ARMOR];
	}

	if (client->NPC_class == CLASS_GALAKMECH)
	{
		//special case
		if (client->ps.stats[STAT_ARMOR] <= 0)
		{
			//no shields
			client->ps.powerups[PW_GALAK_SHIELD] = 0;
			return 0;
		}
		//shields take all the damage
		client->ps.stats[STAT_ARMOR] -= damage;
		if (client->ps.stats[STAT_ARMOR] <= 0)
		{
			client->ps.powerups[PW_GALAK_SHIELD] = 0;
			client->ps.stats[STAT_ARMOR] = 0;
		}
		return damage;
	}

	//if (client->NPC_class == CLASS_VADER) // Vader takes half fire damage
	//{
	//	if (mod == MOD_LAVA)
	//	{
	//		return damage / 2;
	//	}
	//}

	if (G_ControlledByPlayer(ent) && ent->client->ps.powerups[PW_GALAK_SHIELD])
	{
		//special case
		if (client->ps.stats[STAT_ARMOR] <= 0)
		{
			//no shields
			client->ps.powerups[PW_GALAK_SHIELD] = 0;
			return 0;
		}
		//shields take all the damage
		client->ps.stats[STAT_ARMOR] -= damage;
		if (client->ps.stats[STAT_ARMOR] <= 0)
		{
			client->ps.powerups[PW_GALAK_SHIELD] = 0;
			client->ps.stats[STAT_ARMOR] = 0;
		}
		return damage;
	}
	// armor
	const int count = client->ps.stats[STAT_ARMOR];

	// No damage to entity until armor is at less than 50% strength
	if (count > client->ps.stats[STAT_MAX_HEALTH] / 2) // MAX_HEALTH is considered max armor. Or so I'm told.
	{
		save = damage;
	}
	else
	{
		if (!ent->s.number && client->NPC_class == CLASS_ATST)
		{
			//player in ATST... armor takes *all* the damage
			save = damage;
		}
		else
		{
			save = ceil(static_cast<float>(damage) * ARMOR_PROTECTION);
		}
	}

	//Always round up
	if (damage == 1)
	{
		if (client->ps.stats[STAT_ARMOR] > 0)
			client->ps.stats[STAT_ARMOR] -= save;
		//WTF? returns false 0 if armor absorbs only 1 point of damage
		return 0;
	}

	if (save >= count)
		save = count;

	if (!save)
		return 0;

	client->ps.stats[STAT_ARMOR] -= save;

	return save;
}

void G_Slapdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength,
	const qboolean breakSaberLock)
{
	if (!self || !self->client)
	{
		return;
	}

	if (self->client->NPC_class == CLASS_ROCKETTROOPER)
	{
		return;
	}

	if (self->client->moveType == MT_FLYSWIM)
	{
		//can't knock me down when I'm flying
		return;
	}

	if (Boba_StopKnockdown(self, attacker, push_dir))
	{
		return;
	}
	if (jedi_stop_knockdown(self, push_dir))
	{
		//They can sometimes backflip instead of be knocked down
		return;
	}
	if (PM_LockedAnim(self->client->ps.legsAnim))
	{
		//stuck doing something else
		return;
	}
	if (rosh_being_healed(self))
	{
		return;
	}
	if (PM_FaceProtectAnim(self->client->ps.torsoAnim))
	{
		//Protect face against lightning and fire
		return;
	}

	if (jedi_is_kick_resistant(self))
	{
		if (strength > 10)
		{
			strength = 10;
		}
	}

	//break out of a saberLock?
	if (self->client->ps.saberLockTime > level.time)
	{
		if (breakSaberLock)
		{
			self->client->ps.saberLockTime = 0;
			self->client->ps.saberLockEnemy = ENTITYNUM_NONE;
		}
		else
		{
			return;
		}
	}

	if (self->health > 0)
	{
		if (!self->s.number)
		{
			NPC_SetPainEvent(self);
		}
		else
		{
			GEntity_PainFunc(self, attacker, attacker, self->currentOrigin, 0, MOD_MELEE);
		}

		G_CheckLedgeDive(self, 72, push_dir, qfalse, qfalse);

		if (self->client->ps.SaberActive())
		{
			if (self->client->ps.saber[1].Active())
			{
				//turn off second saber
				G_Sound(self, self->client->ps.saber[1].soundOff);
			}
			else if (self->client->ps.saber[0].Active())
			{
				//turn off first
				G_Sound(self, self->client->ps.saber[0].soundOff);
			}
			self->client->ps.SaberDeactivate();
		}

		if (!PM_RollingAnim(self->client->ps.legsAnim)
			&& !PM_InKnockDown(&self->client->ps))
		{
			int knock_anim = BOTH_KNOCKDOWN1;
			if (!self->s.number && strength < 300)
			{
				//player only knocked down if pushed *hard*
				return;
			}
			if (PM_CrouchAnim(self->client->ps.legsAnim))
			{
				//crouched knockdown
				if (jedi_is_kick_resistant(self))
				{
					G_MawStagger(self);
					WP_DeactivateSaber(self, qtrue);
					if (jedi_is_kick_resistant(self))
					{
						jedi_play_blocked_push_sound(self);
					}
				}
				else
				{
					knock_anim = BOTH_KNOCKDOWN4;
				}
			}
			else
			{
				//plain old knockdown
				vec3_t p_l_fwd;
				const vec3_t p_l_angles = { 0, self->client->ps.viewangles[YAW], 0 };
				AngleVectors(p_l_angles, p_l_fwd, nullptr, nullptr);
				if (DotProduct(p_l_fwd, push_dir) > 0.2f)
				{
					//pushing him from behind
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN3;
					}
				}
				else
				{
					//pushing him from front
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN1;
					}
				}
			}
			if (knock_anim == BOTH_KNOCKDOWN1 && strength > 100)
			{
				//push *hard*
				knock_anim = BOTH_KNOCKDOWN2;
			}
			NPC_SetAnim(self, SETANIM_BOTH, knock_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

			if (self->s.number >= MAX_CLIENTS)
			{
				//randomize getup times
				int add_time;
				if (jedi_is_kick_resistant(self))
				{
					add_time = 0;
				}
				else if (self->client->NPC_class == CLASS_BOBAFETT || self->client->NPC_class == CLASS_MANDALORIAN ||
					self->client->NPC_class == CLASS_JANGO || self->client->NPC_class == CLASS_JANGODUAL)
				{
					add_time = Q_irand(-500, 0);
				}
				else
				{
					add_time = Q_irand(-200, 200);
				}
				self->client->ps.legsAnimTimer += add_time;
				self->client->ps.torsoAnimTimer += add_time;
			}
			else
			{
				//player holds extra long so you have more time to decide to do the quick getup
				if (PM_KnockDownAnim(self->client->ps.legsAnim))
				{
					if (!self->NPC)
					{
						self->client->ps.legsAnimTimer += PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME;
					}
					else
					{
						self->client->ps.legsAnimTimer += NPC_KNOCKDOWN_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += NPC_KNOCKDOWN_HOLD_EXTRA_TIME;
					}
				}
			}
			if (attacker->NPC && attacker->enemy == self)
			{
				//pushed pushed down his enemy
				G_AddVoiceEvent(attacker, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
				attacker->NPC->blockedSpeechDebounceTime = level.time + 10000;
			}
		}
	}
}

void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength, const qboolean breakSaberLock)
{
	if (!self || !self->client || !attacker || !attacker->client)
	{
		return;
	}

	if (self->client->NPC_class == CLASS_ROCKETTROOPER)
	{
		return;
	}

	if (self->client->moveType == MT_FLYSWIM)
	{
		//can't knock me down when I'm flying
		return;
	}

	if (Boba_StopKnockdown(self, attacker, push_dir))
	{
		return;
	}
	if (jedi_stop_knockdown(self, push_dir))
	{
		//They can sometimes backflip instead of be knocked down
		return;
	}
	if (PM_LockedAnim(self->client->ps.legsAnim))
	{
		//stuck doing something else
		return;
	}
	if (rosh_being_healed(self))
	{
		return;
	}
	if (PM_FaceProtectAnim(self->client->ps.torsoAnim))
	{
		//Protect face against lightning and fire
		return;
	}

	if (jedi_is_kick_resistant(self))
	{
		if (strength > 10)
		{
			strength = 10;
		}
	}

	//break out of a saberLock?
	if (self->client->ps.saberLockTime > level.time)
	{
		if (breakSaberLock)
		{
			self->client->ps.saberLockTime = 0;
			self->client->ps.saberLockEnemy = ENTITYNUM_NONE;
		}
		else
		{
			return;
		}
	}

	if (self->health > 0)
	{
		if (!self->s.number)
		{
			NPC_SetPainEvent(self);
		}
		else
		{
			GEntity_PainFunc(self, attacker, attacker, self->currentOrigin, 0, MOD_MELEE);
		}

		G_CheckLedgeDive(self, 72, push_dir, qfalse, qfalse);

		if (self->client->ps.SaberActive())
		{
			if (self->client->ps.saber[1].Active())
			{
				//turn off second saber
				G_Sound(self, self->client->ps.saber[1].soundOff);
			}
			else if (self->client->ps.saber[0].Active())
			{
				//turn off first
				G_Sound(self, self->client->ps.saber[0].soundOff);
			}
			self->client->ps.SaberDeactivate();
		}

		if (!PM_RollingAnim(self->client->ps.legsAnim)
			&& !PM_InKnockDown(&self->client->ps))
		{
			int knock_anim = BOTH_KNOCKDOWN1;
			if (!self->s.number && strength < 300)
			{
				//player only knocked down if pushed *hard*
				return;
			}
			if (PM_CrouchAnim(self->client->ps.legsAnim))
			{
				//crouched knockdown
				if (jedi_is_kick_resistant(self))
				{
					G_MawStagger(self);
					WP_DeactivateSaber(self, qtrue);
					if (jedi_is_kick_resistant(self))
					{
						jedi_play_blocked_push_sound(self);
					}
				}
				else
				{
					knock_anim = BOTH_KNOCKDOWN4;
				}
			}
			else
			{
				//plain old knockdown
				vec3_t p_l_fwd;
				const vec3_t p_l_angles = { 0, self->client->ps.viewangles[YAW], 0 };
				AngleVectors(p_l_angles, p_l_fwd, nullptr, nullptr);
				if (DotProduct(p_l_fwd, push_dir) > 0.2f)
				{
					//pushing him from behind
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN3;
					}
				}
				else
				{
					//pushing him from front
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN1;
					}
				}
			}
			if (knock_anim == BOTH_KNOCKDOWN1 && strength > 100)
			{
				//push *hard*
				knock_anim = BOTH_KNOCKDOWN2;
			}
			NPC_SetAnim(self, SETANIM_BOTH, knock_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

			if (self->s.number >= MAX_CLIENTS)
			{
				//randomize getup times
				int add_time;
				if (jedi_is_kick_resistant(self))
				{
					add_time = 0;
				}
				else if (self->client->NPC_class == CLASS_BOBAFETT || self->client->NPC_class == CLASS_MANDALORIAN ||
					self->client->NPC_class == CLASS_JANGO || self->client->NPC_class == CLASS_JANGODUAL)
				{
					add_time = Q_irand(-500, 0);
				}
				else
				{
					add_time = Q_irand(-200, 200);
				}
				self->client->ps.legsAnimTimer += add_time;
				self->client->ps.torsoAnimTimer += add_time;
			}
			else
			{
				//player holds extra long so you have more time to decide to do the quick getup
				if (PM_KnockDownAnim(self->client->ps.legsAnim))
				{
					if (!self->NPC)
					{
						self->client->ps.legsAnimTimer += PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME;
					}
					else
					{
						self->client->ps.legsAnimTimer += NPC_KNOCKDOWN_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += NPC_KNOCKDOWN_HOLD_EXTRA_TIME;
					}
				}
			}
			if (attacker->NPC && attacker->enemy == self)
			{
				//pushed pushed down his enemy
				G_AddVoiceEvent(attacker, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
				attacker->NPC->blockedSpeechDebounceTime = level.time + 10000;
			}
		}
	}
}

void G_KnockOver(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, const float strength,
	const qboolean breakSaberLock)
{
	if (!self || !self->client || !attacker || !attacker->client)
	{
		return;
	}

	if (self->client->NPC_class == CLASS_ROCKETTROOPER)
	{
		return;
	}

	if (self->client->moveType == MT_FLYSWIM)
	{
		//can't knock me down when I'm flying
		return;
	}

	if (Boba_StopKnockdown(self, attacker, push_dir, qfalse))
	{
		return;
	}
	if (jedi_stop_knockdown(self, push_dir))
	{
		//They can sometimes backflip instead of be knocked down
		return;
	}
	if (PM_LockedAnim(self->client->ps.legsAnim))
	{
		//stuck doing something else
		return;
	}
	if (rosh_being_healed(self))
	{
		return;
	}
	if (PM_FaceProtectAnim(self->client->ps.torsoAnim))
	{
		//Protect face against lightning and fire
		return;
	}

	//break out of a saberLock?
	if (self->client->ps.saberLockTime > level.time)
	{
		if (breakSaberLock)
		{
			self->client->ps.saberLockTime = 0;
			self->client->ps.saberLockEnemy = ENTITYNUM_NONE;
		}
		else
		{
			return;
		}
	}

	if (self->health > 0)
	{
		if (!self->s.number)
		{
			NPC_SetPainEvent(self);
		}
		else
		{
			GEntity_PainFunc(self, attacker, attacker, self->currentOrigin, 0, MOD_MELEE);
		}

		G_CheckLedgeDive(self, 72, push_dir, qfalse, qfalse);

		if (self->client->ps.SaberActive())
		{
			if (self->client->ps.saber[1].Active())
			{
				//turn off second saber
				G_Sound(self, self->client->ps.saber[1].soundOff);
			}
			else if (self->client->ps.saber[0].Active())
			{
				//turn off first
				G_Sound(self, self->client->ps.saber[0].soundOff);
			}
			self->client->ps.SaberDeactivate();
		}

		if (!PM_RollingAnim(self->client->ps.legsAnim)
			&& !PM_InKnockDown(&self->client->ps))
		{
			int knock_anim = BOTH_KNOCKDOWN1;
			if (!self->s.number && strength < 300)
			{
				//player only knocked down if pushed *hard*
				return;
			}
			if (PM_CrouchAnim(self->client->ps.legsAnim))
			{
				//crouched knockdown
				if (jedi_is_kick_resistant(self))
				{
					G_MawStagger(self);
					WP_DeactivateSaber(self, qtrue);
					if (jedi_is_kick_resistant(self))
					{
						jedi_play_blocked_push_sound(self);
					}
				}
				else
				{
					knock_anim = BOTH_KNOCKDOWN4;
				}
			}
			else
			{
				//plain old knockdown
				vec3_t p_l_fwd;
				const vec3_t p_l_angles = { 0, self->client->ps.viewangles[YAW], 0 };
				AngleVectors(p_l_angles, p_l_fwd, nullptr, nullptr);
				if (DotProduct(p_l_fwd, push_dir) > 0.2f)
				{
					//pushing him from behind
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN3;
					}
				}
				else
				{
					//pushing him from front
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN1;
					}
				}
			}
			if (knock_anim == BOTH_KNOCKDOWN1 && strength > 100)
			{
				//push *hard*
				knock_anim = BOTH_KNOCKDOWN2;
			}
			NPC_SetAnim(self, SETANIM_BOTH, knock_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

			if (self->s.number >= MAX_CLIENTS)
			{
				//randomize getup times
				int add_time;
				if (jedi_is_kick_resistant(self))
				{
					add_time = 0;
				}
				else if (self->client->NPC_class == CLASS_BOBAFETT || self->client->NPC_class == CLASS_MANDALORIAN ||
					self->client->NPC_class == CLASS_JANGO || self->client->NPC_class == CLASS_JANGODUAL)
				{
					add_time = Q_irand(-500, 0);
				}
				else
				{
					add_time = Q_irand(-200, 200);
				}
				self->client->ps.legsAnimTimer += add_time;
				self->client->ps.torsoAnimTimer += add_time;
			}
			else
			{
				//player holds extra long so you have more time to decide to do the quick getup
				if (PM_KnockDownAnim(self->client->ps.legsAnim))
				{
					if (!self->NPC)
					{
						self->client->ps.legsAnimTimer += PLAYER_KNOCKOVER_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += PLAYER_KNOCKOVER_HOLD_EXTRA_TIME;
					}
					else
					{
						self->client->ps.legsAnimTimer += NPC_KNOCKOVER_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += NPC_KNOCKOVER_HOLD_EXTRA_TIME;
					}
				}
			}
			if (attacker->NPC && attacker->enemy == self)
			{
				//pushed pushed down his enemy
				G_AddVoiceEvent(attacker, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
				attacker->NPC->blockedSpeechDebounceTime = level.time + 10000;
			}
		}
	}
}

void G_BlastDown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength)
{
	if (!self || !self->client)
	{
		return;
	}

	if (self->client->NPC_class == CLASS_ROCKETTROOPER)
	{
		return;
	}

	if (self->client->moveType == MT_FLYSWIM)
	{
		//can't knock me down when I'm flying
		return;
	}

	if (Boba_StopKnockdown(self, attacker, push_dir, qfalse))
	{
		return;
	}
	if (jedi_stop_knockdown(self, push_dir))
	{
		//They can sometimes backflip instead of be knocked down
		return;
	}
	if (PM_LockedAnim(self->client->ps.legsAnim))
	{
		//stuck doing something else
		return;
	}
	if (rosh_being_healed(self))
	{
		return;
	}
	if (PM_FaceProtectAnim(self->client->ps.torsoAnim))
	{
		//Protect face against lightning and fire
		return;
	}

	strength *= 3;

	//break out of a saberLock?
	if (self->client->ps.saberLockTime > level.time)
	{
		self->client->ps.saberLockTime = 0;
		self->client->ps.saberLockEnemy = ENTITYNUM_NONE;
	}

	if (self->health > 0)
	{
		if (!self->s.number)
		{
			NPC_SetPainEvent(self);
		}
		else
		{
			GEntity_PainFunc(self, attacker, attacker, self->currentOrigin, 0, MOD_MELEE);
		}

		G_CheckLedgeDive(self, 72, push_dir, qfalse, qfalse);

		if (self->client->ps.SaberActive())
		{
			if (self->client->ps.saber[1].Active())
			{
				//turn off second saber
				G_Sound(self, self->client->ps.saber[1].soundOff);
			}
			else if (self->client->ps.saber[0].Active())
			{
				//turn off first
				G_Sound(self, self->client->ps.saber[0].soundOff);
			}
			self->client->ps.SaberDeactivate();
		}

		if (!PM_RollingAnim(self->client->ps.legsAnim)
			&& !PM_InKnockDown(&self->client->ps))
		{
			int knock_anim = BOTH_KNOCKDOWN1;
			if (!self->s.number && strength < 300)
			{
				//player only knocked down if pushed *hard*
				return;
			}
			if (PM_CrouchAnim(self->client->ps.legsAnim))
			{
				//crouched knockdown
				if (jedi_is_kick_resistant(self))
				{
					G_MawStagger(self);
					WP_DeactivateSaber(self, qtrue);
					if (jedi_is_kick_resistant(self))
					{
						jedi_play_blocked_push_sound(self);
					}
				}
				else
				{
					knock_anim = BOTH_KNOCKDOWN4;
				}
			}
			else
			{
				//plain old knockdown
				vec3_t p_l_fwd;
				const vec3_t p_l_angles = { 0, self->client->ps.viewangles[YAW], 0 };
				AngleVectors(p_l_angles, p_l_fwd, nullptr, nullptr);
				if (DotProduct(p_l_fwd, push_dir) > 0.2f)
				{
					//pushing him from behind
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN3;
					}
				}
				else
				{
					//pushing him from front
					if (jedi_is_kick_resistant(self))
					{
						G_MawStagger(self);
						WP_DeactivateSaber(self, qtrue);
						if (jedi_is_kick_resistant(self))
						{
							jedi_play_blocked_push_sound(self);
						}
					}
					else
					{
						knock_anim = BOTH_KNOCKDOWN1;
					}
				}
			}
			if (knock_anim == BOTH_KNOCKDOWN1 && strength > 100)
			{
				//push *hard*
				knock_anim = BOTH_KNOCKDOWN2;
			}
			NPC_SetAnim(self, SETANIM_BOTH, knock_anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

			if (self->s.number >= MAX_CLIENTS)
			{
				//randomize getup times
				int add_time;
				if (jedi_is_kick_resistant(self))
				{
					add_time = 0;
				}
				else if (self->client->NPC_class == CLASS_BOBAFETT || self->client->NPC_class == CLASS_MANDALORIAN ||
					self->client->NPC_class == CLASS_JANGO || self->client->NPC_class == CLASS_JANGODUAL)
				{
					add_time = Q_irand(-500, 0);
				}
				else
				{
					add_time = Q_irand(-200, 200);
				}
				self->client->ps.legsAnimTimer += add_time;
				self->client->ps.torsoAnimTimer += add_time;
			}
			else
			{
				//player holds extra long so you have more time to decide to do the quick getup
				if (PM_KnockDownAnim(self->client->ps.legsAnim))
				{
					if (!self->NPC)
					{
						self->client->ps.legsAnimTimer += PLAYER_KNOCKOVER_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += PLAYER_KNOCKOVER_HOLD_EXTRA_TIME;
					}
					else
					{
						self->client->ps.legsAnimTimer += NPC_KNOCKOVER_HOLD_EXTRA_TIME;
						self->client->ps.torsoAnimTimer += NPC_KNOCKOVER_HOLD_EXTRA_TIME;
					}
				}
			}
			if (attacker->NPC && attacker->enemy == self)
			{
				//pushed pushed down his enemy
				G_AddVoiceEvent(attacker, Q_irand(EV_GLOAT1, EV_GLOAT3), 10000);
				attacker->NPC->blockedSpeechDebounceTime = level.time + 10000;
			}
		}
	}
}

/*
============
G_LocationDamage
============
*/
static int G_LocationDamage(const vec3_t point, const gentity_t* targ, int take)
{
	vec3_t bullet_path;
	vec3_t bullet_angle;

	// First things first.  If we're not damaging them, why are we here?
	if (!take)
	{
		return 0;
	}

	// Point[2] is the REAL world Z. We want Z relative to the clients feet

	// Where the feet are at [real Z]
	const int client_feet_z = targ->currentOrigin[2] + targ->mins[2];
	// How tall the client is [Relative Z]
	const int client_height = targ->maxs[2] - targ->mins[2];
	// Where the bullet struck [Relative Z]
	const int bullet_height = point[2] - client_feet_z;
	// Get a vector aiming from the client to the bullet hit
	VectorSubtract(targ->currentOrigin, point, bullet_path);
	// Convert it into PITCH, ROLL, YAW
	vectoangles(bullet_path, bullet_angle);
	const int client_rotation = targ->client->ps.viewangles[YAW];
	const int bullet_rotation = bullet_angle[YAW];
	int impact_rotation = abs(client_rotation - bullet_rotation);
	impact_rotation += 45; // just to make it easier to work with
	impact_rotation = impact_rotation % 360; // Keep it in the 0-359 range

	if (impact_rotation < 90)
	{
		targ->client->lasthurt_location = LOCATION_BACK;
	}
	else if (impact_rotation < 180)
	{
		targ->client->lasthurt_location = LOCATION_RIGHT;
	}
	else if (impact_rotation < 270)
	{
		targ->client->lasthurt_location = LOCATION_FRONT;
	}
	else if (impact_rotation < 360)
	{
		targ->client->lasthurt_location = LOCATION_LEFT;
	}
	else
	{
		targ->client->lasthurt_location = LOCATION_NONE;
	}
	// The upper body never changes height, just distance from the feet
	if (bullet_height > client_height - 2)
	{
		targ->client->lasthurt_location |= LOCATION_HEAD;
	}
	else if (bullet_height > client_height - 8)
	{
		targ->client->lasthurt_location |= LOCATION_FACE;
	}
	else if (bullet_height > client_height - 10)
	{
		targ->client->lasthurt_location |= LOCATION_SHOULDER;
	}
	else if (bullet_height > client_height - 16)
	{
		targ->client->lasthurt_location |= LOCATION_CHEST;
	}
	else if (bullet_height > client_height - 26)
	{
		targ->client->lasthurt_location |= LOCATION_STOMACH;
	}
	else if (bullet_height > client_height - 29)
	{
		targ->client->lasthurt_location |= LOCATION_GROIN;
	}
	else if (bullet_height < 4)
	{
		targ->client->lasthurt_location |= LOCATION_FOOT;
	}
	else
	{
		// The leg is the only thing that changes size when you duck,
		// so we check for every other parts RELATIVE location, and
		// whats left over must be the leg.
		targ->client->lasthurt_location |= LOCATION_LEG;
	}

	// Check the location ignoring the rotation info
	switch (targ->client->lasthurt_location & ~(LOCATION_BACK | LOCATION_LEFT | LOCATION_RIGHT | LOCATION_FRONT))
	{
	case LOCATION_HEAD:
		take *= 2.8;
		break;
	case LOCATION_FACE:
		if (targ->client->lasthurt_location & LOCATION_FRONT)
		{
			take *= 3.0; // Faceshots REALLY suck
		}
		else
		{
			take *= 2.2;
		}
		break;
	case LOCATION_SHOULDER:
		if (targ->client->lasthurt_location & (LOCATION_FRONT | LOCATION_BACK))
		{
			take *= 1.2; // Throat or nape of neck
		}
		else
		{
			take *= 1; // Shoulders
		}
		break;
	case LOCATION_CHEST:
		if (targ->client->lasthurt_location & (LOCATION_FRONT | LOCATION_BACK))
		{
			take *= 1.5; // Belly or back
		}
		else
		{
			take *= 1.4; // Arms
		}
		break;
	case LOCATION_STOMACH:
		take *= 1.3;
		break;
	case LOCATION_GROIN:
		if (targ->client->lasthurt_location & LOCATION_FRONT)
		{
			take *= 1.2; // Groin shot
		}
		break;
	case LOCATION_LEG:
		take *= 0.7;
		break;
	case LOCATION_FOOT:
		take *= 0.5;
		break;
	default:;
	}
	return take;
}

static void G_CheckKnockdown(gentity_t* targ, gentity_t* attacker, vec3_t new_dir, const int dflags, const int mod)
{
	if (!targ || !attacker)
	{
		return;
	}
	if (!(dflags & DAMAGE_RADIUS))
	{
		//not inherently explosive damage, check mod
		if (mod != MOD_REPEATER_ALT
			&& mod != MOD_FLECHETTE_ALT
			&& mod != MOD_ROCKET
			&& mod != MOD_ROCKET_ALT
			&& mod != MOD_CONC
			&& mod != MOD_CONC_ALT
			&& mod != MOD_THERMAL
			&& mod != MOD_THERMAL_ALT
			&& mod != MOD_DETPACK
			&& mod != MOD_LASERTRIP
			&& mod != MOD_LASERTRIP_ALT
			&& mod != MOD_EXPLOSIVE
			&& mod != MOD_EXPLOSIVE_SPLASH
			&& mod != MOD_DESTRUCTION)
		{
			return;
		}
	}

	if (!targ->client || targ->client->NPC_class == CLASS_PROTOCOL || !g_standard_humanoid(targ))
	{
		return;
	}

	if (targ->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		//already in air
		return;
	}

	if (!targ->s.number)
	{
		//player less likely to be knocked down
		if (!g_spskill->integer)
		{
			//never in easy
			return;
		}
		if (!cg.renderingThirdPerson || cg.zoomMode)
		{
			//never if not in chase camera view (so more likely with saber out)
			return;
		}
		if (g_spskill->integer == 1)
		{
			//33% chance on medium
			if (Q_irand(0, 2))
			{
				return;
			}
		}
		else
		{
			//50% chance on hard
			if (Q_irand(0, 1))
			{
				return;
			}
		}
	}

	const float strength = VectorLength(targ->client->ps.velocity);
	if (targ->client->ps.velocity[2] > 100 && strength > Q_irand(150, 350)) //600 ) )
	{
		//explosive concussion possibly do a knockdown?
		G_Knockdown(targ, attacker, new_dir, strength, qtrue);
	}
}

static void G_CheckLightningKnockdown(gentity_t* targ, gentity_t* attacker, vec3_t new_dir, const int dflags, const int mod)
{
	if (!targ || !attacker)
	{
		return;
	}
	if (!(dflags & DAMAGE_RADIUS))
	{
		//not inherently explosive damage, check mod
		if (mod != MOD_REPEATER_ALT
			&& mod != MOD_FLECHETTE_ALT
			&& mod != MOD_ROCKET
			&& mod != MOD_ROCKET_ALT
			&& mod != MOD_CONC
			&& mod != MOD_CONC_ALT
			&& mod != MOD_THERMAL
			&& mod != MOD_THERMAL_ALT
			&& mod != MOD_DETPACK
			&& mod != MOD_LASERTRIP
			&& mod != MOD_LASERTRIP_ALT
			&& mod != MOD_EXPLOSIVE
			&& mod != MOD_EXPLOSIVE_SPLASH
			&& mod != MOD_DESTRUCTION)
		{
			return;
		}
	}

	if (!targ->client || targ->client->NPC_class == CLASS_PROTOCOL || !g_standard_humanoid(targ))
	{
		return;
	}

	if (targ->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		//already in air
		return;
	}

	if (!targ->s.number)
	{
		//player less likely to be knocked down
		if (!g_spskill->integer)
		{
			//never in easy
			return;
		}
		if (!cg.renderingThirdPerson || cg.zoomMode)
		{
			//never if not in chase camera view (so more likely with saber out)
			return;
		}
		if (g_spskill->integer == 1)
		{
			//33% chance on medium
			if (Q_irand(0, 2))
			{
				return;
			}
		}
		else
		{
			//50% chance on hard
			if (Q_irand(0, 1))
			{
				return;
			}
		}
	}

	const float strength = VectorLength(targ->client->ps.velocity);
	if (targ->client->ps.velocity[2] > 50 && strength > Q_irand(10, 50))
	{
		//explosive concussion possibly do a knockdown?
		G_Knockdown(targ, attacker, new_dir, strength, qtrue);
	}
}

void G_ApplyKnockback(gentity_t* targ, vec3_t new_dir, float knockback)
{
	vec3_t kvel;
	float mass;

	if (targ
		&& targ->client
		&& (targ->client->NPC_class == CLASS_ATST
			|| targ->client->NPC_class == CLASS_RANCOR
			|| targ->client->NPC_class == CLASS_SAND_CREATURE
			|| targ->client->NPC_class == CLASS_WAMPA))
	{
		//much to large to *ever* throw
		return;
	}

	//--- TEMP TEST
	if (new_dir[2] <= 0.0f)
	{
		new_dir[2] += (0.0f - new_dir[2]) * 1.2f;
	}

	knockback *= 2.0f;

	if (knockback > 120)
	{
		knockback = 120;
	}
	//--- TEMP TEST

	if (targ && targ->physicsBounce > 0) //override the mass
		mass = targ->physicsBounce;
	else
		mass = 200;

	if (g_gravity->value > 0)
	{
		VectorScale(new_dir, g_knockback->value * knockback / mass * 0.8, kvel);
		kvel[2] = new_dir[2] * (g_knockback->value * knockback) / (mass * 1.5) + 20;
	}
	else
	{
		VectorScale(new_dir, g_knockback->value * knockback / mass, kvel);
	}

	if (targ && targ->client)
	{
		VectorAdd(targ->client->ps.velocity, kvel, targ->client->ps.velocity);
	}
	else if (targ && targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType
		!= TR_NONLINEAR_STOP)
	{
		VectorAdd(targ->s.pos.trDelta, kvel, targ->s.pos.trDelta);
		VectorCopy(targ->currentOrigin, targ->s.pos.trBase);
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if (targ->client && !targ->client->ps.pm_time)
	{
		int t = knockback * 2;
		if (t < 50)
		{
			t = 50;
		}
		if (t > 200)
		{
			t = 200;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

static void G_ApplyLightningKnockback(gentity_t* targ, vec3_t new_dir, float knockback)
{
	if (g_lightningdamage->integer)
	{
		Com_Printf(S_COLOR_CYAN"WINDU LIGHTNING KNOCKBACK CORRECT\n");
	}
	vec3_t kvel;
	float mass;

	if (targ
		&& targ->client
		&& (targ->client->NPC_class == CLASS_ATST
			|| targ->client->NPC_class == CLASS_RANCOR
			|| targ->client->NPC_class == CLASS_SAND_CREATURE
			|| targ->client->NPC_class == CLASS_WAMPA))
	{
		//much to large to *ever* throw
		return;
	}

	//--- TEMP TEST
	if (new_dir[2] <= 0.0f)
	{
		new_dir[2] += 0.0f - new_dir[2];
	}

	knockback = 0.2f;

	if (knockback > 0.3)
	{
		knockback = 0.3f;
	}
	//--- TEMP TEST

	if (targ && targ->physicsBounce > 0) //override the mass
		mass = targ->physicsBounce;
	else
		mass = 250;

	if (g_gravity->value > 0)
	{
		VectorScale(new_dir, g_knockback->value * knockback / mass * 10.9, kvel);
		kvel[2] = new_dir[2] * (g_knockback->value * knockback) / mass;
	}
	else
	{
		VectorScale(new_dir, g_knockback->value * knockback / mass, kvel);
	}

	if (targ && targ->client)
	{
		VectorAdd(targ->client->ps.velocity, kvel, targ->client->ps.velocity);
	}
	else if (targ && targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType
		!= TR_NONLINEAR_STOP)
	{
		VectorAdd(targ->s.pos.trDelta, kvel, targ->s.pos.trDelta);
		VectorCopy(targ->currentOrigin, targ->s.pos.trBase);
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if (targ && targ->client && !targ->client->ps.pm_time)
	{
		int t = knockback;
		/*if (t < 1)
		{
			t = 0.5f;
		}*/
		if (t > 0.3)
		{
			t = 0.3f;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

static int G_CheckForLedge(const gentity_t* self, vec3_t fall_check_dir, const float check_dist)
{
	vec3_t start, end;
	trace_t tr;

	VectorMA(self->currentOrigin, check_dist, fall_check_dir, end);
	//Should have clip brushes masked out by now and have bbox resized to death size
	gi.trace(&tr, self->currentOrigin, self->mins, self->maxs, end, self->s.number, self->clipmask,
		static_cast<EG2_Collision>(0), 0);
	if (tr.allsolid || tr.startsolid)
	{
		return 0;
	}
	VectorCopy(tr.endpos, start);
	VectorCopy(start, end);
	end[2] -= 256;

	gi.trace(&tr, start, self->mins, self->maxs, end, self->s.number, self->clipmask, static_cast<EG2_Collision>(0), 0);
	if (tr.allsolid || tr.startsolid)
	{
		return 0;
	}
	if (tr.fraction >= 1.0)
	{
		return start[2] - tr.endpos[2];
	}
	return 0;
}

static void G_FriendlyFireReaction(const gentity_t* self, const gentity_t* other, const int dflags)
{
	if (!player->client->ps.viewEntity || other->s.number != player->client->ps.viewEntity)
	{
		//hit by a teammate
		if (other != self->enemy && self != other->enemy)
		{
			//we weren't already enemies
			if (self->enemy || other->enemy || other->s.number && other->s.number != player->client->ps.viewEntity)
			{
				//if one of us actually has an enemy already, it's okay, just an accident OR wasn't hit by player or someone controlled by player OR player hit ally and didn't get 25% chance of getting mad (FIXME:accumulate anger+base on diff?)
				return;
			}
			if (self->NPC && !other->s.number) //should be assumed, but...
			{
				//dammit, stop that!
				if (!(dflags & DAMAGE_RADIUS))
				{
					//if it's radius damage, ignore it
					if (self->NPC->ffireDebounce < level.time)
					{
						//FIXME: way something?  NEED DIALOGUE
						self->NPC->ffireCount++;
						//Com_Printf( "incr: %d < %d\n", self->NPC->ffireCount, 3+((2-g_spskill->integer)*2) );
						self->NPC->ffireDebounce = level.time + 500;
					}
				}
			}
		}
	}
}

float damageModifier[HL_MAX] =
{
	1.0f, //HL_NONE,
	0.25f, //HL_FOOT_RT,
	0.25f, //HL_FOOT_LT,
	0.75f, //HL_LEG_RT,
	0.75f, //HL_LEG_LT,
	1.0f, //HL_WAIST,
	1.0f, //HL_BACK_RT,
	1.0f, //HL_BACK_LT,
	1.0f, //HL_BACK,
	1.0f, //HL_CHEST_RT,
	1.0f, //HL_CHEST_LT,
	1.0f, //HL_CHEST,
	0.5f, //HL_ARM_RT,
	0.5f, //HL_ARM_LT,
	0.25f, //HL_HAND_RT,
	0.25f, //HL_HAND_LT,
	1.0f, //HL_HEAD,
	1.0f, //HL_GENERIC1,
	1.0f, //HL_GENERIC2,
	1.0f, //HL_GENERIC3,
	1.0f, //HL_GENERIC4,
	1.0f, //HL_GENERIC5,
	1.0f, //HL_GENERIC6,
};

void G_TrackWeaponUsage(const gentity_t* self, const gentity_t* inflictor, const int add, const int mod)
{
	if (!self || !self->client || self->s.number)
	{
		//player only
		return;
	}
	int weapon = WP_NONE;
	//FIXME: need to check the MOD to find out what weapon (if *any*) actually did the killing
	if (inflictor && !inflictor->client && mod != MOD_SABER && inflictor->lastEnemy && inflictor->lastEnemy != self)
	{
		//a missile that was reflected, ie: not owned by me originally
		if (inflictor->owner == self && self->s.weapon == WP_SABER)
		{
			//we reflected it
			weapon = WP_SABER;
		}
	}
	if (weapon == WP_NONE)
	{
		switch (mod)
		{
		case MOD_SABER:
			weapon = WP_SABER;
			break;
		case MOD_BRYAR:
		case MOD_BRYAR_ALT:
			weapon = WP_JAWA;
			weapon = WP_SBD_BLASTER;
			weapon = WP_BRYAR_PISTOL;
			break;
		case MOD_BLASTER:
		case MOD_BLASTER_ALT:
			weapon = WP_BLASTER;
			weapon = WP_THEFIRSTORDER;
			weapon = WP_CLONECARBINE;
			break;
		case MOD_CLONERIFLE:
		case MOD_CLONERIFLE_ALT:
			weapon = WP_CLONERIFLE;
			break;
		case MOD_REBELBLASTER:
		case MOD_REBELBLASTER_ALT:
			weapon = WP_REBELBLASTER;
			break;
		case MOD_CLONECOMMANDO:
		case MOD_CLONECOMMANDO_ALT:
			weapon = WP_CLONECOMMANDO;
			break;
		case MOD_Z6_ROTARY_CANNON:
			weapon = WP_Z6_ROTARY_CANNON;
			break;
		case MOD_REBELRIFLE:
		case MOD_REBELRIFLE_ALT:
			weapon = WP_REBELRIFLE;
			break;
		case MOD_REY:
		case MOD_REY_ALT:
			weapon = WP_REY;
			break;
		case MOD_CLONEPISTOL:
		case MOD_CLONEPISTOL_ALT:
			weapon = WP_CLONEPISTOL;
			break;
		case MOD_JANGO:
		case MOD_JANGO_ALT:
			weapon = WP_JANGO;
			break;
		case MOD_BOBA:
		case MOD_BOBA_ALT:
			weapon = WP_BOBA;
			break;
		case MOD_DISRUPTOR:
		case MOD_SNIPER:
			weapon = WP_DISRUPTOR;
			break;
		case MOD_BOWCASTER:
		case MOD_BOWCASTER_ALT:
			weapon = WP_BOWCASTER;
			break;
		case MOD_REPEATER:
		case MOD_REPEATER_ALT:
			weapon = WP_REPEATER;
			break;
		case MOD_DEMP2:
		case MOD_DEMP2_ALT:
			weapon = WP_DEMP2;
			break;
		case MOD_FLECHETTE:
		case MOD_FLECHETTE_ALT:
			weapon = WP_FLECHETTE;
			break;
		case MOD_ROCKET:
		case MOD_ROCKET_ALT:
			weapon = WP_ROCKET_LAUNCHER;
			break;
		case MOD_CONC:
		case MOD_CONC_ALT:
			weapon = WP_CONCUSSION;
			break;
		case MOD_THERMAL:
		case MOD_THERMAL_ALT:
			weapon = WP_THERMAL;
			break;
		case MOD_DETPACK:
			weapon = WP_DET_PACK;
			break;
		case MOD_LASERTRIP:
		case MOD_LASERTRIP_ALT:
			weapon = WP_TRIP_MINE;
			break;
		case MOD_MELEE:
			if (self->s.weapon == WP_STUN_BATON)
			{
				weapon = WP_STUN_BATON;
			}
			else if (self->s.weapon == WP_MELEE)
			{
				weapon = WP_MELEE;
			}
			break;
		default:;
		}
	}
	if (weapon != WP_NONE)
	{
		self->client->sess.missionStats.weaponUsed[weapon] += add;
	}
}

static qboolean G_NonLocationSpecificDamage(const int means_of_death)
{
	if (means_of_death == MOD_EXPLOSIVE
		|| means_of_death == MOD_REPEATER_ALT
		|| means_of_death == MOD_FLECHETTE_ALT
		|| means_of_death == MOD_ROCKET
		|| means_of_death == MOD_ROCKET_ALT
		|| means_of_death == MOD_CONC
		|| means_of_death == MOD_THERMAL
		|| means_of_death == MOD_THERMAL_ALT
		|| means_of_death == MOD_DETPACK
		|| means_of_death == MOD_LASERTRIP
		|| means_of_death == MOD_LASERTRIP_ALT
		|| means_of_death == MOD_MELEE
		|| means_of_death == MOD_FORCE_GRIP
		|| means_of_death == MOD_KNOCKOUT
		|| means_of_death == MOD_CRUSH
		|| means_of_death == MOD_EXPLOSIVE_SPLASH
		|| means_of_death == MOD_DESTRUCTION)
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean G_ImmuneToGas(const gentity_t* ent)
{
	if (!ent || !ent->client)
	{
		//only effects living clients
		return qtrue;
	}
	if (ent->s.weapon == WP_NOGHRI_STICK //assumes user is immune
		|| ent->client->NPC_class == CLASS_HAZARD_TROOPER
		|| ent->client->NPC_class == CLASS_ATST
		|| ent->client->NPC_class == CLASS_GONK
		|| ent->client->NPC_class == CLASS_SAND_CREATURE
		|| ent->client->NPC_class == CLASS_INTERROGATOR
		|| ent->client->NPC_class == CLASS_MARK1
		|| ent->client->NPC_class == CLASS_MARK2
		|| ent->client->NPC_class == CLASS_GALAKMECH
		|| ent->client->NPC_class == CLASS_MOUSE
		|| ent->client->NPC_class == CLASS_PROBE // droid
		|| ent->client->NPC_class == CLASS_PROTOCOL // droid
		|| ent->client->NPC_class == CLASS_R2D2 // droid
		|| ent->client->NPC_class == CLASS_R5D2 // droid
		|| ent->client->NPC_class == CLASS_REMOTE
		|| ent->client->NPC_class == CLASS_SEEKER // droid
		|| ent->client->NPC_class == CLASS_SENTRY
		|| ent->client->NPC_class == CLASS_SWAMPTROOPER
		|| ent->client->NPC_class == CLASS_TUSKEN
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO
		|| ent->client->NPC_class == CLASS_JANGODUAL
		|| ent->client->NPC_class == CLASS_ROCKETTROOPER
		|| ent->client->NPC_class == CLASS_SABER_DROID
		|| ent->client->NPC_class == CLASS_ASSASSIN_DROID
		|| ent->client->NPC_class == CLASS_DROIDEKA
		|| ent->client->NPC_class == CLASS_HAZARD_TROOPER
		|| ent->client->NPC_class == CLASS_VEHICLE)
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean G_IsJediClass(const gclient_t* client)
{
	if (!client)
	{
		return qfalse;
	}
	switch (client->NPC_class)
	{
	case CLASS_ALORA:
	case CLASS_DESANN:
	case CLASS_JEDI:
	case CLASS_KYLE:
	case CLASS_LUKE:
	case CLASS_REBORN:
	case CLASS_BOC:
	case CLASS_SHADOWTROOPER:
	case CLASS_TAVION:
	case CLASS_VADER:
	case CLASS_SITHLORD:
	case CLASS_GALEN:
	case CLASS_YODA:
	case CLASS_AHSOKA:
	case CLASS_PROJECTION:
	case CLASS_JEDIMASTER:
	case CLASS_GROGU:
		return qtrue;
	default:
		return qfalse;
	}
}

qboolean NPC_IsNotDismemberable(const gentity_t* self)
{
	switch (self->client->NPC_class)
	{
	case CLASS_ATST:
	case CLASS_HOWLER:
	case CLASS_RANCOR:
	case CLASS_SAND_CREATURE:
	case CLASS_WAMPA:
	case CLASS_INTERROGATOR:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_GALAKMECH:
	case CLASS_MINEMONSTER:
	case CLASS_MOUSE:
	case CLASS_PROBE:
		//case CLASS_PROTOCOL:
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_REMOTE:
	case CLASS_SEEKER:
	case CLASS_SENTRY:
	case CLASS_ROCKETTROOPER:
	case CLASS_ASSASSIN_DROID:
	case CLASS_HAZARD_TROOPER:
	case CLASS_VEHICLE:
	case CLASS_DROIDEKA:
	case CLASS_SBD:
	case CLASS_PROJECTION:
		return qtrue;
	default:
		break;
	}

	return qfalse;
}

/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
	DAMAGE_NO_HIT_LOC		Damage not based on hit location
============
*/
int G_PickPainAnim(const gentity_t* self, const vec3_t point, int hit_loc);
void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, const vec3_t dir, const vec3_t point,
	int damage, int dflags, int mod, int hit_loc)
{
	gclient_t* client;
	int take;
	int asave = 0;
	int knockback;
	vec3_t new_dir;
	qboolean already_dead = qfalse;
	float shield_absorbed = 0;

	if (!targ->takedamage)
	{
		if (targ->client //client
			&& targ->client->NPC_class == CLASS_SAND_CREATURE //sand creature
			&& targ->activator //something in our mouth
			&& targ->activator == inflictor) //inflictor of damage is the thing in our mouth
		{
			//being damaged by the thing in our mouth, allow the damage
		}
		else
		{
			return;
		}
	}

	if (g_SerenityJediEngineMode->integer == 2)
	{
		//Advanced MD Mode
		if ((mod == MOD_BRYAR_ALT || mod == MOD_SEEKER || mod == MOD_DEMP2) && targ && targ->inuse && targ->client)
		{
			//it acts like a stun hit
			int stun_damage = static_cast<float>(inflictor->s.generic1) / BRYAR_MAX_CHARGE * 7;

			WP_ForcePowerDrain(targ, FP_ABSORB, 20);

			targ->client->ps.saberFatigueChainCount += stun_damage;

			// I guess always do 10 points of damage...feel free to tweak as needed
			if (damage < 10)
			{
				//ignore piddly little damage
				damage = 0;
			}
			else if (damage >= 10)
			{
				damage = 10;
			}

			if (targ->client->ps.saberFatigueChainCount >= MISHAPLEVEL_HEAVY || targ->client->ps.forcePower < 50)
			{
				//knockdown
				G_Knockdown(targ, attacker, dir, 50, qtrue);
			}
			else if (targ->client->ps.saberFatigueChainCount >= MISHAPLEVEL_LIGHT && !PM_InKnockDown(&targ->client->ps))
			{
				//stumble
				AnimateStun(targ, attacker);
			}
			else
			{
				if (!PM_InKnockDown(&targ->client->ps))
				{
					NPC_SetAnim(targ, SETANIM_TORSO, BOTH_SONICPAIN_HOLD, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
					targ->client->ps.torsoAnimTimer += 100;
					targ->client->ps.weaponTime = targ->client->ps.torsoAnimTimer;
				}
			}

			if (targ->client->ps.powerups[PW_SHOCKED] < level.time + 100)
			{
				// ... do the effect for a split second for some more feedback
				targ->s.powerups |= 1 << PW_SHOCKED;
				targ->client->ps.powerups[PW_SHOCKED] = level.time + 3000;
			}

			if (targ->client->ps.powerups[PW_SHOCKED] > level.time + 100)
			{
				targ->client->stunDamage = 9;
				targ->client->stunTime = level.time + 1000;

				gentity_t* tent = G_TempEntity(targ->currentOrigin, EV_STUNNED);
				tent->owner = targ;
			}
			return;
		}
	}

	if (targ->health <= 0 && !targ->client)
	{
		// allow corpses to be disintegrated
		if (mod != MOD_SNIPER && mod != MOD_PROJECTION_END || targ->flags & FL_DISINTEGRATED)
			return;
	}

	// if we are the player and we are locked to an emplaced gun, we have to reroute damage to the gun....sigh.
	if (targ->s.eFlags & EF_LOCKED_TO_WEAPON
		&& targ->s.number == 0
		&& targ->owner
		&& !targ->owner->bounceCount //not an EWeb
		&& !(targ->owner->flags & FL_GODMODE))
	{
		// swapping the gun into our place to absorb our damage
		targ = targ->owner;
	}

	if (targ->flags & FL_DINDJARIN
		&& mod != MOD_SABER
		&& mod != MOD_REPEATER_ALT
		&& mod != MOD_FLECHETTE_ALT
		&& mod != MOD_ROCKET
		&& mod != MOD_ROCKET_ALT
		&& mod != WP_NOGHRI_STICK
		&& mod != MOD_CONC_ALT
		&& mod != MOD_THERMAL
		&& mod != MOD_THERMAL_ALT
		&& mod != MOD_DEMP2
		&& mod != MOD_DEMP2_ALT
		&& mod != MOD_EXPLOSIVE
		&& mod != MOD_DETPACK
		&& mod != MOD_LASERTRIP
		&& mod != MOD_LASERTRIP_ALT
		&& mod != MOD_FORCE_GRIP
		&& mod != MOD_FORCE_LIGHTNING
		&& mod != MOD_FORCE_DRAIN
		&& mod != MOD_SEEKER
		&& mod != MOD_CONC)
	{
		if (damage < 10)
		{//ignore piddly little damage
			damage = 5;
		}
		else if (damage >= 10)
		{
			const int choice = Q_irand(0, 3);

			switch (choice)
			{
			case 0:
				damage = ceil(static_cast<float>(damage) * 0.60f);
				break;
			case 1:
				damage = ceil(static_cast<float>(damage) * 0.70f);
				break;
			case 2:
				damage = ceil(static_cast<float>(damage) * 0.80f);
				break;
			case 3:
				damage = ceil(static_cast<float>(damage));
				break;
			default:
				damage = ceil(static_cast<float>(damage));
				break;
			}
		}
	}

	if (targ->flags & FL_SHIELDED && mod != MOD_SABER && !targ->client)
	{
		//magnetically protected, this thing can only be damaged by lightsabers
		return;
	}

	if (targ->flags & FL_DMG_BY_SABER_ONLY && mod != MOD_SABER)
	{
		//can only be damaged by lightsabers (but no shield... yeah, it's redundant, but whattayagonnado?)
		return;
	}

	if (targ->flags & FL_DMG_BY_HEAVY_WEAP_ONLY && !(dflags & DAMAGE_HEAVY_WEAP_CLASS))
	{
		// can only be damaged by an heavy type weapon...but impacting missile was in the heavy weap class...so we just aren't taking damage from this missile
		return;
	}

	if (targ->svFlags & SVF_BBRUSH
		|| !targ->client && Q_stricmp("misc_model_breakable", targ->classname) == 0)
		//FIXME: flag misc_model_breakables?
	{
		//breakable brush or misc_model_breakable
		if (targ->NPC_targetname)
		{
			//only a certain attacker can destroy this
			if (!attacker
				|| !attacker->targetname
				|| Q_stricmp(targ->NPC_targetname, attacker->targetname) != 0)
			{
				//and it's not this one, do nothing
				return;
			}
		}
	}

	if (targ->client && targ->client->NPC_class == CLASS_ATST)
	{
		// extra checks can be done here
		if (mod == MOD_SNIPER
			|| mod == MOD_DISRUPTOR
			|| mod == MOD_CONC_ALT)
		{
			// disruptor does not hurt an atst
			return;
		}
	}
	if (targ->client
		&& targ->client->NPC_class == CLASS_RANCOR
		&& (!attacker || !attacker->client || attacker->client->NPC_class != CLASS_RANCOR))
	{
		// I guess always do 10 points of damage...feel free to tweak as needed
		if (damage < 10)
		{
			//ignore piddly little damage
			damage = 0;
		}
		else if (damage >= 10)
		{
			damage = 10;
		}
	}
	else if (mod == MOD_SABER)
	{
		//sabers do less damage to mark1's and atst's, and to hazard troopers and assassin droids
		if (targ->client)
		{
			if (targ->client->NPC_class == CLASS_ATST || targ->client->NPC_class == CLASS_MARK1)
			{
				// I guess always do 5 points of damage...feel free to tweak as needed
				if (damage > 5)
				{
					damage = 5;
				}
			}
		}
	}

	//if (targ && targ->client && targ->client->NPC_class == CLASS_VADER) // Vader takes half fire damage
	//{
	//	if (mod == MOD_LAVA)
	//	{
	//		damage = ceil(damage * 0.50f);
	//	}
	//}

	if (!inflictor)
	{
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if (!attacker)
	{
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	client = targ->client;

	if (client)
	{
		if (client->noclip && !targ->s.number)
		{
			return;
		}
	}

	if (mod == MOD_GAS)
	{
		//certain NPCs are immune
		if (G_ImmuneToGas(targ))
		{
			//immune
			return;
		}
		dflags |= DAMAGE_NO_ARMOR;
	}
	if (dflags & DAMAGE_NO_DAMAGE)
	{
		damage = 0;
	}

	if (dir == nullptr)
	{
		dflags |= DAMAGE_NO_KNOCKBACK;
	}
	else
	{
		VectorNormalize2(dir, new_dir);
	}

	if (targ->s.number != 0)
	{
		//not the player
		if (targ->flags & FL_GODMODE || targ->flags & FL_UNDYING)
		{
			//have god or undying on, so ignore no protection flag
			dflags &= ~DAMAGE_NO_PROTECTION;
		}
	}

	if (client && PM_InOnGroundAnim(&client->ps))
	{
		dflags |= DAMAGE_NO_KNOCKBACK;
	}
	if (!attacker->s.number && targ->client && attacker->client && targ->client->playerTeam == attacker->client->
		playerTeam)
	{
		//player doesn't do knockback against allies unless he kills them
		dflags |= DAMAGE_DEATH_KNOCKBACK;
	}

	if (targ->client && (mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT))
	{
		TIMER_Set(targ, "DEMP2_StunTime", Q_irand(1000, 2000));
	}

	if (client &&
		(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) &&
		(
			client->NPC_class == CLASS_SABER_DROID ||
			client->NPC_class == CLASS_ASSASSIN_DROID ||
			client->NPC_class == CLASS_DROIDEKA ||
			client->NPC_class == CLASS_GONK ||
			client->NPC_class == CLASS_MOUSE ||
			client->NPC_class == CLASS_PROBE ||
			client->NPC_class == CLASS_PROTOCOL ||
			client->NPC_class == CLASS_R2D2 ||
			client->NPC_class == CLASS_R5D2 ||
			client->NPC_class == CLASS_SEEKER ||
			client->NPC_class == CLASS_INTERROGATOR))
	{
		damage *= 7;
	}

	if (client && client->NPC_class == CLASS_HAZARD_TROOPER)
	{
		if (mod == MOD_SABER
			&& damage > 0
			&& !(dflags & DAMAGE_NO_PROTECTION))
		{
			damage /= 10;
		}
	}

	if (client
		&& client->NPC_class == CLASS_GALAKMECH
		&& !(dflags & DAMAGE_NO_PROTECTION))
	{
		//hit Galak
		if (client->ps.stats[STAT_ARMOR] > 0)
		{
			//shields are up
			dflags &= ~DAMAGE_NO_ARMOR; //always affect armor
			if (mod == MOD_ELECTROCUTE
				|| mod == MOD_DEMP2
				|| mod == MOD_DEMP2_ALT)
			{
				//shield protects us from this
				damage = 0;
			}
		}
		else
		{
			//shields down
			if (mod == MOD_MELEE
				|| mod == MOD_CRUSH && attacker && attacker->client)
			{
				//Galak takes no impact damage
				return;
			}
			if (dflags & DAMAGE_RADIUS
				|| mod == MOD_REPEATER_ALT
				|| mod == MOD_FLECHETTE_ALT
				|| mod == MOD_ROCKET
				|| mod == MOD_ROCKET_ALT
				|| mod == MOD_CONC
				|| mod == MOD_THERMAL
				|| mod == MOD_THERMAL_ALT
				|| mod == MOD_DETPACK
				|| mod == MOD_LASERTRIP
				|| mod == MOD_LASERTRIP_ALT
				|| mod == MOD_EXPLOSIVE_SPLASH
				|| mod == MOD_ENERGY_SPLASH
				|| mod == MOD_SABER
				|| mod == MOD_DESTRUCTION)
			{
				//galak without shields takes quarter damage from explosives and lightsaber
				damage = ceil(static_cast<float>(damage) / 4.0f);
			}
		}
	}

	if (mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT)
	{
		if (client)
		{
			if (client->NPC_class == CLASS_BOBAFETT || client->NPC_class == CLASS_MANDALORIAN || client->NPC_class ==
				CLASS_JANGO || client->NPC_class == CLASS_JANGODUAL || rocket_trooper_player(targ))
			{
				// DEMP2 also disables npc jetpack
				Boba_FlyStop(targ);
				if (client->jetPackOn)
				{
					//disable jetpack temporarily
					Jetpack_Off(targ);
					client->jetPackToggleTime = level.time + Q_irand(3000, 10000);
				}
			}
			if (client->NPC_class == CLASS_PROTOCOL || client->NPC_class == CLASS_SEEKER ||
				client->NPC_class == CLASS_R2D2 || client->NPC_class == CLASS_R5D2 ||
				client->NPC_class == CLASS_MOUSE || client->NPC_class == CLASS_GONK)
			{
				// DEMP2 does more damage to these guys.
				damage *= 2;
			}
			else if (client->NPC_class == CLASS_PROBE || client->NPC_class == CLASS_INTERROGATOR ||
				client->NPC_class == CLASS_MARK1 || client->NPC_class == CLASS_MARK2 || client->NPC_class ==
				CLASS_SENTRY ||
				client->NPC_class == CLASS_ATST)
			{
				// DEMP2 does way more damage to these guys.
				damage *= 5;
			}
		}
		else if (targ->s.weapon == WP_TURRET)
		{
			damage *= 6; // more damage to turret things
		}
	}

	if (targ->s.number >= MAX_CLIENTS && mod == MOD_DESTRUCTION) //Destruction should do less damage to enemies
	{
		if (targ->s.weapon == WP_SABER || G_IsJediClass(client))
		{
			if (client)
			{
				switch (client->ps.forcePowerLevel[FP_SABER_DEFENSE])
				{
				case FORCE_LEVEL_3:
					damage *= 0.4;
					break;
				case FORCE_LEVEL_2:
					damage *= 0.6;
					break;
				case FORCE_LEVEL_1:
					damage *= 0.8;
					break;
				default:
					break;
				}
			}
		}
	}

	if (targ
		&& targ->client
		&& !(dflags & DAMAGE_NO_PROTECTION)
		&& !(dflags & DAMAGE_DIE_ON_IMPACT))
		//falling to you death ignores force protect and force rage (but obeys godmode and undying flags)
	{
		//force protections
		//rage
		if (targ->client->ps.forcePowersActive & 1 << FP_RAGE)
		{
			damage = floor(
				static_cast<float>(damage) / static_cast<float>(targ->client->ps.forcePowerLevel[FP_RAGE] * 2));
		}
		//protect
		if (targ->client->ps.forcePowersActive & 1 << FP_PROTECT)
		{
			//New way: just cut all physical damage by force level
			if (mod == MOD_FALLING
				&& targ->NPC
				&& targ->NPC->aiFlags & NPCAI_DIE_ON_IMPACT)
			{
				//if falling to your death, protect can't save you
			}
			else
			{
				qboolean doSound = qfalse;
				switch (mod)
				{
				case MOD_CRUSH:
					if (attacker && attacker->client)
					{
						//need to still be crushed by AT-ST
						break;
					}
				case MOD_REPEATER_ALT:
				case MOD_FLECHETTE_ALT:
				case MOD_ROCKET:
				case MOD_ROCKET_ALT:
				case MOD_CONC:
				case MOD_THERMAL:
				case MOD_THERMAL_ALT:
				case MOD_DETPACK:
				case MOD_LASERTRIP:
				case MOD_LASERTRIP_ALT:
				case MOD_EMPLACED:
				case MOD_EXPLOSIVE:
				case MOD_EXPLOSIVE_SPLASH:
				case MOD_SABER:
				case MOD_DISRUPTOR:
				case MOD_SNIPER:
				case MOD_CONC_ALT:
				case MOD_BOWCASTER:
				case MOD_BOWCASTER_ALT:
				case MOD_DEMP2:
				case MOD_DEMP2_ALT:
				case MOD_ENERGY:
				case MOD_ENERGY_SPLASH:
				case MOD_ELECTROCUTE:
				case MOD_BRYAR:
				case MOD_BRYAR_ALT:
				case MOD_BLASTER:
				case MOD_BLASTER_ALT:
				case MOD_REBELBLASTER:
				case MOD_REBELBLASTER_ALT:
				case MOD_CLONERIFLE:
				case MOD_CLONERIFLE_ALT:
				case MOD_CLONECOMMANDO:
				case MOD_CLONECOMMANDO_ALT:
				case MOD_Z6_ROTARY_CANNON:
				case MOD_REBELRIFLE:
				case MOD_REBELRIFLE_ALT:
				case MOD_REY:
				case MOD_REY_ALT:
				case MOD_CLONEPISTOL:
				case MOD_CLONEPISTOL_ALT:
				case MOD_JANGO:
				case MOD_JANGO_ALT:
				case MOD_BOBA:
				case MOD_BOBA_ALT:
				case MOD_REPEATER:
				case MOD_FLECHETTE:
				case MOD_WATER:
				case MOD_SLIME:
				case MOD_LAVA:
				case MOD_FALLING:
				case MOD_MELEE:
				case MOD_PROJECTION_END:
					doSound = static_cast<qboolean>(Q_irand(0, 4) == 0);
					switch (targ->client->ps.forcePowerLevel[FP_PROTECT])
					{
					case FORCE_LEVEL_4:
						//je suis invincible!!!
						if (targ->client
							&& attacker->client
							&& targ->client->playerTeam == attacker->client->playerTeam
							&& (!targ->NPC || !targ->NPC->charmedTime))
						{
							//complain, but don't turn on them
							G_FriendlyFireReaction(targ, attacker, dflags);
						}
						return;
					case FORCE_LEVEL_3:
						//one-tenth damage
						if (damage <= 1)
						{
							damage = 0;
						}
						else
						{
							damage = ceil(static_cast<float>(damage) * 0.25f); //was 0.1f);
						}
						break;
					case FORCE_LEVEL_2:
						//half damage
						if (damage <= 1)
						{
							damage = 0;
						}
						else
						{
							damage = ceil(static_cast<float>(damage) * 0.5f);
						}
						break;
					case FORCE_LEVEL_1:
						//three-quarters damage
						if (damage <= 1)
						{
							damage = 0;
						}
						else
						{
							damage = ceil(static_cast<float>(damage) * 0.75f);
						}
						break;
					default:;
					}
					break;
				default:;
				}
				if (doSound)
				{
					//make protect sound
					G_SoundOnEnt(targ, CHAN_ITEM, "sound/weapons/force/protecthit.wav");
				}
			}
		}
		//absorb

		if (targ->client->ps.forcePowersActive & 1 << FP_ABSORB)
		{
			if (mod == MOD_DESTRUCTION)
			{
				switch (targ->client->ps.forcePowerLevel[FP_ABSORB])
				{
				case FORCE_LEVEL_1:
					damage *= 0.5f;
					break;
				case FORCE_LEVEL_2:
					damage *= 0.25f;
					break;
				case FORCE_LEVEL_3:
					damage *= 0.1f;
					break;
				default:
					damage = 0;
					break;
				}
			}
		}
	}

	knockback = damage;

	//Attempt to apply extra knockback
	if (dflags & DAMAGE_EXTRA_KNOCKBACK)
	{
		knockback *= 2;
	}

	if (knockback > 200)
	{
		knockback = 200;
	}

	if (targ->client
		&& targ->client->ps.forcePowersActive & 1 << FP_PROTECT && targ->client->ps.forcePowerLevel[FP_PROTECT] ==
		FORCE_LEVEL_3)
	{
		//pretend there was no damage?
		knockback = 0;
	}
	else if (mod == MOD_CRUSH)
	{
		knockback = 0;
	}
	else if (targ->flags & FL_NO_KNOCKBACK)
	{
		knockback = 0;
	}
	else if (targ->NPC
		&& targ->NPC->jumpState == JS_JUMPING)
	{
		knockback = 0;
	}
	else if (attacker->s.number >= MAX_CLIENTS //an NPC fired
		&& targ->client //hit a client
		&& attacker->client //attacker is a client
		&& targ->client->playerTeam == attacker->client->playerTeam) //on same team
	{
		//crap, ignore knockback
		knockback = 0;
	}
	else if (dflags & DAMAGE_NO_KNOCKBACK)
	{
		knockback = 0;
	}
	else if (dflags & DAMAGE_LIGHNING_KNOCKBACK)
	{
		//knockback *= 2;
		G_ApplyLightningKnockback(targ, new_dir, knockback);
	}
	else if (mod == MOD_ROCKET || mod == MOD_ROCKET_ALT)
	{
		knockback *= 3.5;
	}

	if (dflags & DAMAGE_SABER_KNOCKBACK1)
	{
		if (attacker && attacker->client)
		{
			knockback *= attacker->client->ps.saber[0].knockbackScale;
		}
	}
	if (dflags & DAMAGE_SABER_KNOCKBACK1_B2)
	{
		if (attacker && attacker->client)
		{
			knockback *= attacker->client->ps.saber[0].knockbackScale2;
		}
	}
	if (dflags & DAMAGE_SABER_KNOCKBACK2)
	{
		if (attacker && attacker->client)
		{
			knockback *= attacker->client->ps.saber[1].knockbackScale;
		}
	}
	if (dflags & DAMAGE_SABER_KNOCKBACK2_B2)
	{
		if (attacker && attacker->client)
		{
			knockback *= attacker->client->ps.saber[1].knockbackScale2;
		}
	}
	// figure momentum add, even if the damage won't be taken
	if (knockback && !(dflags & DAMAGE_DEATH_KNOCKBACK)) //&& targ->client
	{
		if (jedi_win_po(targ))
		{
			G_ApplyLightningKnockback(targ, new_dir, knockback);
			G_CheckLightningKnockdown(targ, attacker, new_dir, dflags, mod);
		}
		else
		{
			G_ApplyKnockback(targ, new_dir, knockback);
			G_CheckKnockdown(targ, attacker, new_dir, dflags, mod);
		}
	}

	// check for godmode, completely getting out of the damage
	if ((targ->flags & FL_GODMODE || targ->client && targ->client->ps.powerups[PW_INVINCIBLE] > level.time)
		&& !(dflags & DAMAGE_NO_PROTECTION))
	{
		if (targ->client
			&& attacker->client
			&& targ->client->playerTeam == attacker->client->playerTeam
			&& (!targ->NPC || !targ->NPC->charmedTime))
		{
			//complain, but don't turn on them
			G_FriendlyFireReaction(targ, attacker, dflags);
		}
		return;
	}

	// Check for team damage
	/*
	if ( targ != attacker && !(dflags&DAMAGE_IGNORE_TEAM) && OnSameTeam (targ, attacker)  )
	{//on same team
		if ( !targ->client )
		{//a non-player object should never take damage from an ent on the same team
			return;
		}

		if ( attacker->client && attacker->client->playerTeam == targ->noDamageTeam )
		{//NPC or player shot an object on his own team
			return;
		}

		if ( attacker->s.number != 0 && targ->s.number != 0 &&//player not involved in any way in this exchange
			attacker->client && targ->client &&//two NPCs
			attacker->client->playerTeam == targ->client->playerTeam ) //on the same team
		{//NPCs on same team don't hurt each other
			return;
		}

		if ( targ->s.number == 0 &&//player was hit
			attacker->client && targ->client &&//by an NPC
			attacker->client->playerTeam == TEAM_PLAYER ) //on the same team
		{
			if ( attacker->enemy != targ )//by accident
			{//do no damage, no armor loss, no reaction, run no scripts
				return;
			}
		}
	}
	*/

	// add to the attacker's hit counter
	if (attacker->client && targ != attacker && targ->health > 0)
	{
		if (OnSameTeam(targ, attacker))
		{
			//			attacker->client->ps.persistant[PERS_HITS] -= damage;
		}
		else
		{
			//			attacker->client->ps.persistant[PERS_HITS] += damage;
		}
	}

	take = damage;

	//FIXME: Do not use this method of difficulty changing
	// Scale the amount of damage given to the player based on the skill setting
	/*
	if ( targ->s.number == 0 && targ != attacker )
	{
		take *= ( g_spskill->integer + 1) * 0.75;
	}

	if ( take < 1 ) {
		take = 1;
	}
	*/
	if (client)
	{
		//don't lose armor if on same team
		// save some from armor
		asave = CheckArmor(targ, take, dflags, mod);

		if (asave)
		{
			shield_absorbed = asave;
		}

		if (targ->client->NPC_class != CLASS_VEHICLE)
		{
			//vehicles don't have personal shields
			if (mod == MOD_DEMP2
				|| mod == MOD_DEMP2_ALT
				|| mod == MOD_CONC
				|| mod == MOD_CONC_ALT
				|| mod == MOD_ENERGY
				|| mod == MOD_ENERGY_SPLASH)
			{
				targ->client->ps.powerups[PW_BATTLESUIT] = level.time + ARMOR_EFFECT_TIME;
			}
			if (targ->client->ps.stats[STAT_ARMOR] <= 0)
			{
				//all out of armor
				//remove Galak's shield
				targ->client->ps.powerups[PW_BATTLESUIT] = 0;
			}
		}

		if (mod == MOD_SLIME || mod == MOD_LAVA)
		{
			// Hazard Troopers Don't Mind Acid Rain
			if (targ->client->NPC_class == CLASS_HAZARD_TROOPER
				&& !(dflags & DAMAGE_NO_PROTECTION))
			{
				take = 0;
			}

			if (mod == MOD_SLIME)
			{
				trace_t test_trace;

				{
					vec3_t test_end_pos;
					vec3_t test_start_pos;
					vec3_t test_direction{};
					test_direction[0] = Q_flrand(0.0f, 1.0f) * 0.5f - 0.25f;
					test_direction[1] = Q_flrand(0.0f, 1.0f) * 0.5f - 0.25f;
					test_direction[2] = 1.0f;
					VectorMA(targ->currentOrigin, 60.0f, test_direction, test_start_pos);
					VectorCopy(targ->currentOrigin, test_end_pos);
					test_end_pos[0] += Q_flrand(0.0f, 1.0f) * 8.0f - 4.0f;
					test_end_pos[1] += Q_flrand(0.0f, 1.0f) * 8.0f - 4.0f;
					test_end_pos[2] += Q_flrand(0.0f, 1.0f) * 8.0f;

					gi.trace(&test_trace, test_start_pos, nullptr, nullptr, test_end_pos, ENTITYNUM_NONE, MASK_SHOT,
						G2_COLLIDE, 0);

					if (!test_trace.startsolid &&
						!test_trace.allsolid &&
						test_trace.entityNum == targ->s.number &&
						test_trace.G2CollisionMap[0].mEntityNum != -1)
					{
						G_PlayEffect("world/acid_fizz", test_trace.G2CollisionMap[0].mCollisionPosition);
					}
					//					CG_DrawEdge(testStartPos,	testEndPos,	EDGE_IMPACT_POSSIBLE);
					float chance_of_fizz = gi.WE_GetChanceOfSaberFizz();
					TIMER_Set(targ, "AcidPainDebounce", 200 + 10000.0f * Q_flrand(0.0f, 1.0f) * chance_of_fizz);
					hit_loc = HL_CHEST;
				}
			}
		}

		take -= asave;

		if (targ->client->NPC_class == CLASS_VEHICLE)
		{
			if (targ->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
			{
				//((CVehicleNPC *)targ->NPC)->m_ulFlags |= CVehicleNPC::VEH_BUCKING;
			}

			if (damage > 0 && // Actually took some damage
				mod != MOD_SABER && // and damage didn't come from a saber
				(targ->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER || targ->m_pVehicle->m_pVehicleInfo->type ==
					VH_FIGHTER) && // and is a speeder
				attacker && // and there is an attacker
				attacker->client && // who is a client
				attacker->s.number < MAX_CLIENTS && // who is the player
				G_IsRidingVehicle(attacker)) // who is riding a bike
			{
				vec3_t veh_fwd;
				vec3_t actor_fwd;
				AngleVectors(targ->currentAngles, actor_fwd, nullptr, nullptr);

				Vehicle_t* p_veh = G_IsRidingVehicle(attacker);
				VectorCopy(p_veh->m_pParentEntity->client->ps.velocity, veh_fwd);
				VectorNormalize(veh_fwd);

				if (DotProduct(veh_fwd, actor_fwd) > 0.5)
				{
					damage *= 10.0f;
				}
			}

			if (damage > 0 && // Actually took some damage
				mod == MOD_SABER && // If Attacked By A Saber
				(targ->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER || targ->m_pVehicle->m_pVehicleInfo->type ==
					VH_FIGHTER) && // and is a speeder
				!(targ->m_pVehicle->m_ulFlags & VEH_OUTOFCONTROL) && // and is not already spinning
				targ->client->ps.speed > 30.0f && // and is moving
				(attacker == inflictor || Q_irand(0, 30) == 0)
				// and EITHER saber is held, or 1 in 30 chance of hitting when thrown
				)
			{
				Vehicle_t* p_veh = targ->m_pVehicle;
				gentity_t* parent = p_veh->m_pParentEntity;
				float cur_speed = VectorLength(parent->client->ps.velocity);
				p_veh->m_iArmor = 0; // Remove all remaining Armor
				p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, 10000);
				p_veh->m_ulFlags |= VEH_OUTOFCONTROL | VEH_SPINNING;
				VectorScale(parent->client->ps.velocity, 1.25f, parent->pos3);
				if (cur_speed < p_veh->m_pVehicleInfo->speedMax)
				{
					VectorNormalize(parent->pos3);
					if (cur_speed < p_veh->m_pVehicleInfo->speedMax)
					{
						VectorNormalize(parent->pos3);
						if (fabsf(parent->pos3[2]) < 0.25f)
						{
							VectorScale(parent->pos3, p_veh->m_pVehicleInfo->speedMax * 1.25f, parent->pos3);
						}
						else
						{
							VectorScale(parent->client->ps.velocity, 1.25f, parent->pos3);
						}
					}
				}
				if (attacker == inflictor && (!G_IsRidingVehicle(attacker) || Q_irand(0, 3) == 0))
				{
					attacker->lastEnemy = targ;
					G_StartMatrixEffect(attacker, MEF_LOOK_AT_ENEMY | MEF_NO_RANGEVAR | MEF_NO_VERTBOB | MEF_NO_SPIN,
						1000);
					if (!G_IsRidingVehicle(attacker))
					{
						G_StartRoll(attacker, Q_irand(0, 1) == 0 ? BOTH_ROLL_L : BOTH_ROLL_R);
					}
				}

				if (targ->m_pVehicle->m_pPilot && targ->m_pVehicle->m_pPilot->s.number >= MAX_CLIENTS)
				{
					G_SoundOnEnt(targ->m_pVehicle->m_pPilot, CHAN_VOICE, "*falling1.wav");
				}

				// DISMEMBER THE FRONT PART OF THE MODEL
				{
					trace_t trace;

					gentity_t* limb = G_Spawn();

					// Setup Basic Limb Entity Properties
					//------------------------------------
					limb->s.radius = 60;
					limb->s.eType = ET_THINKER;
					limb->s.eFlags |= EF_BOUNCE_HALF;
					limb->classname = "limb";
					limb->owner = targ;
					limb->enemy = targ->enemy;
					limb->svFlags = SVF_USE_CURRENT_ORIGIN;
					limb->playerModel = 0;
					limb->clipmask = MASK_SOLID;
					limb->contents = CONTENTS_CORPSE;
					limb->e_clThinkFunc = clThinkF_CG_Limb;
					limb->e_ThinkFunc = thinkF_LimbThink;
					limb->nextthink = level.time + FRAMETIME;
					limb->physicsBounce = 0.2f;
					limb->craniumBone = targ->craniumBone;
					limb->cervicalBone = targ->cervicalBone;
					limb->thoracicBone = targ->thoracicBone;
					limb->upperLumbarBone = targ->upperLumbarBone;
					limb->lowerLumbarBone = targ->lowerLumbarBone;
					limb->hipsBone = targ->hipsBone;
					limb->rootBone = targ->rootBone;

					// Calculate The Location Of The New Limb
					//----------------------------------------
					G_SetOrigin(limb, targ->currentOrigin);

					VectorCopy(targ->currentOrigin, limb->s.pos.trBase);
					VectorSet(limb->mins, -3.0f, -3.0f, -6.0f);
					VectorSet(limb->maxs, 3.0f, 3.0f, 6.0f);
					VectorCopy(targ->s.modelScale, limb->s.modelScale);

					//copy the g2 instance of the victim into the limb
					//-------------------------------------------------
					gi.G2API_CopyGhoul2Instance(targ->ghoul2, limb->ghoul2, -1);
					gi.G2API_SetRootSurface(limb->ghoul2, limb->playerModel, "lfront");
					gi.G2API_SetSurfaceOnOff(&targ->ghoul2[targ->playerModel], "lfront", TURN_OFF);
					animation_t* animations = level.knownAnimFileSets[targ->client->clientInfo.animFileIndex].
						animations;

					//play the proper dismember anim on the limb
					gi.G2API_SetBoneAnim(&limb->ghoul2[limb->playerModel], nullptr,
						animations[BOTH_A1_BL_TR].firstFrame,
						animations[BOTH_A1_BL_TR].numFrames + animations[BOTH_A1_BL_TR].firstFrame,
						BONE_ANIM_OVERRIDE_FREEZE, 1, level.time, -1, -1);

					// Check For Start In Solid
					//--------------------------
					gi.linkentity(limb);
					gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number,
						limb->clipmask, static_cast<EG2_Collision>(0), 0);
					if (trace.startsolid)
					{
						limb->s.pos.trBase[2] -= limb->mins[2];
						gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number,
							limb->clipmask, static_cast<EG2_Collision>(0), 0);
						if (trace.startsolid)
						{
							limb->s.pos.trBase[2] += limb->mins[2];
							gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase,
								limb->s.number, limb->clipmask, static_cast<EG2_Collision>(0), 0);
						}
					}

					// If Started In Solid, Remove
					//-----------------------------
					if (trace.startsolid)
					{
						G_FreeEntity(limb);
					}

					// Otherwise, Send It Flying
					//---------------------------
					else
					{
						VectorCopy(limb->s.pos.trBase, limb->currentOrigin);
						VectorScale(targ->client->ps.velocity, 1.0f, limb->s.pos.trDelta);
						limb->s.pos.trType = TR_GRAVITY;
						limb->s.pos.trTime = level.time;

						VectorCopy(targ->currentAngles, limb->s.apos.trBase);
						VectorClear(limb->s.apos.trDelta);
						limb->s.apos.trTime = level.time;
						limb->s.apos.trType = TR_LINEAR;
						limb->s.apos.trDelta[0] = Q_irand(-300, 300);
						limb->s.apos.trDelta[1] = Q_irand(-800, 800);

						gi.linkentity(limb);
					}
				}
			}

			targ->m_pVehicle->m_iShields = targ->client->ps.stats[STAT_ARMOR];
			targ->m_pVehicle->m_iArmor -= take;
			if (targ->m_pVehicle->m_iArmor < 0)
			{
				targ->m_pVehicle->m_iArmor = 0;
			}

			//if (targ->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
			//{//get the last surf that was hit
			//	//if (targ->client && targ->client->g2LastSurfaceTime == level.time)
			//	if (targ->client && targ->painDebounceTime == level.time)
			//	{
			//		char hitSurface[MAX_QPATH];
			//		int surface = -1;

			//		gi.G2API_GetSurfaceName(targ->ghoul2.size(), targ->client->g2LastSurfaceHit, hitSurface);

			//		if (hitSurface[0])
			//		{
			//			surface = G_ShipSurfaceForSurfName(&hitSurface[0]);

			//			if (take && surface > 0)
			//			{//hit a certain part of the ship
			//				int deathPoint = 0;

			//				targ->locationDamage[surface] += take;

			//				switch (surface)
			//				{
			//				case SHIPSURF_FRONT:
			//					deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_front;
			//					break;
			//				case SHIPSURF_BACK:
			//					deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_back;
			//					break;
			//				case SHIPSURF_RIGHT:
			//					deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_right;
			//					break;
			//				case SHIPSURF_LEFT:
			//					deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_left;
			//					break;
			//				default:
			//					break;
			//				}

			//				//presume 0 means it wasn't set and so it should never die.
			//				if (deathPoint)
			//				{
			//					if (targ->locationDamage[surface] >= deathPoint)
			//					{ //this area of the ship is now dead
			//						if (G_FlyVehicleDestroySurface(targ, surface))
			//						{//actually took off a surface
			//							G_VehicleSetDamageLocFlags(targ, surface, deathPoint);
			//						}
			//					}
			//					else
			//					{
			//						G_VehicleSetDamageLocFlags(targ, surface, deathPoint);
			//					}
			//				}
			//			}
			//		}
			//	}
			//}
			if (targ->m_pVehicle->m_iArmor <= 0
				&& targ->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL)
			{
				//vehicle all out of armor
				Vehicle_t* p_veh = targ->m_pVehicle;
				if (dflags & DAMAGE_IMPACT_DIE)
				{
					// kill it now
					p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, -1/* -1 causes instant death */);
				}
				else
				{
					if (p_veh->m_iDieTime == 0)
					{
						//just start the flaming effect and explosion delay, if it's not going already...
						p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, Q_irand(4000, 5500));
					}
				}
			}
			else if (targ->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL)
			{
				take = 0;
			}
		}
	}
	if (!(dflags & DAMAGE_NO_HIT_LOC) || !(dflags & DAMAGE_RADIUS))
	{
		if (!G_NonLocationSpecificDamage(mod))
		{
			//certain kinds of damage don't care about hit location
			take = ceil(static_cast<float>(take) * damageModifier[hit_loc]);
		}
	}

	if (g_debugDamage->integer)
	{
		gi.Printf("[%d]client:%i health:%i damage:%i armor:%i hitloc:%s\n", level.time, targ->s.number, targ->health,
			take, asave, hitLocName[hit_loc]);
	}

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (client)
	{
		client->ps.persistant[PERS_ATTACKER] = attacker->s.number; //attack can be the world ent
		client->damage_armor += asave;
		client->damage_blood += take;
		if (dir)
		{
			//can't check newdir since it's local, newdir is dir normalized
			VectorCopy(new_dir, client->damage_from);
			client->damage_fromWorld = false;
		}
		else
		{
			VectorCopy(targ->currentOrigin, client->damage_from);
			client->damage_fromWorld = true;
		}
	}

	if (targ->client && g_standard_humanoid(targ) && !NPC_IsNotDismemberable(targ))
	{
		// set the last client who damaged the target
		targ->client->lasthurt_client = attacker->s.number;
		targ->client->lasthurt_mod = mod;
		// Modify the damage for location damage
		if (point && targ && targ->health > 0 && attacker && take)
		{
			take = G_LocationDamage(point, targ, take);
		}
		else
		{
			targ->client->lasthurt_location = LOCATION_NONE;
		}
	}

	if (targ->client && attacker->client && targ->health > 0 && g_standard_humanoid(targ) && !
		NPC_IsNotDismemberable(targ))
	{
		//do head shots
		if (inflictor->s.weapon == WP_BLASTER
			|| inflictor->s.weapon == WP_TUSKEN_RIFLE
			|| inflictor->s.weapon == WP_FLECHETTE
			|| inflictor->s.weapon == WP_BRYAR_PISTOL
			|| inflictor->s.weapon == WP_SBD_BLASTER
			|| inflictor->s.weapon == WP_TURRET
			|| inflictor->s.weapon == WP_BLASTER_PISTOL
			|| inflictor->s.weapon == WP_EMPLACED_GUN
			|| inflictor->s.weapon == WP_TUSKEN_RIFLE
			|| inflictor->s.weapon == WP_BATTLEDROID
			|| inflictor->s.weapon == WP_THEFIRSTORDER
			|| inflictor->s.weapon == WP_REBELBLASTER
			|| inflictor->s.weapon == WP_CLONECARBINE
			|| inflictor->s.weapon == WP_CLONECOMMANDO
			|| inflictor->s.weapon == WP_WRIST_BLASTER
			|| inflictor->s.weapon == WP_BOWCASTER
			|| inflictor->s.weapon == WP_JANGO
			|| inflictor->s.weapon == WP_DUAL_PISTOL
			|| inflictor->s.weapon == WP_DUAL_CLONEPISTOL
			|| inflictor->s.weapon == WP_BOBA
			|| inflictor->s.weapon == WP_CLONEPISTOL
			|| inflictor->s.weapon == WP_DROIDEKA
			|| inflictor->s.weapon == WP_JAWA)
		{
			float targ_maxs2;
			int height;
			float z_rel;
			float z_ratio;
			targ_maxs2 = targ->maxs[2];
			// handling crouching
			if (targ->client->ps.pm_flags & PMF_DUCKED)
			{
				height = (abs(targ->mins[2]) + targ_maxs2) * 0.75;
			}
			else
			{
				height = abs(targ->mins[2]) + targ_maxs2;
			}
			// project the z component of point
			// onto the z component of the model's origin
			// this results in the z component from the origin at 0
			z_rel = point[2] - targ->currentOrigin[2] + abs(targ->mins[2]);
			z_ratio = z_rel / height;

			if (z_ratio > 0.90)
			{
				take = G_LocationDamage(point, targ, take);
				mod = MOD_HEADSHOT;
			}
			else
			{
				take = G_LocationDamage(point, targ, take);
				mod = MOD_BODYSHOT;
			}
		}
	}

	if ((mod == MOD_DEMP2
		|| mod == MOD_DEMP2_ALT
		|| mod == MOD_CONC
		|| mod == MOD_CONC_ALT
		|| mod == MOD_ENERGY
		|| mod == MOD_ENERGY_SPLASH) && shield_absorbed)
	{
		gentity_t* ev_ent;
		vec3_t vec3;
		// Send off an event to show a shield shell on the player, pointing in the right direction.
		ev_ent = G_TempEntity(targ->currentOrigin, EV_SHIELD_HIT);
		ev_ent->s.otherentity_num = targ->s.number;
		ev_ent->s.eventParm = DirToByte(vec3);
		ev_ent->s.time2 = shield_absorbed;
	}

	// do the damage
	if (targ->health <= 0)
	{
		already_dead = qtrue;
	}

	// Undying If:
	//--------------------------------------------------------------------------
	auto targ_undying = static_cast<qboolean>(!already_dead &&
		!(dflags & DAMAGE_NO_PROTECTION) &&
		(targ->flags & FL_UNDYING ||
			dflags & DAMAGE_NO_KILL ||
			targ->client &&
			targ->client->ps.forcePowersActive & 1 << FP_RAGE &
			!(dflags & DAMAGE_NO_PROTECTION) &&
			!(dflags & DAMAGE_DIE_ON_IMPACT)));

	if (targ->client
		&& targ->client->NPC_class == CLASS_WAMPA
		&& targ->count
		&& take >= targ->health)
	{
		//wampa holding someone, don't die unless you can release them!
		qboolean remove_arm = qfalse;
		if (targ->activator
			&& attacker == targ->activator
			&& mod == MOD_SABER)
		{
			remove_arm = qtrue;
		}
		if (Wampa_CheckDropVictim(targ, qtrue))
		{
			//released our victim
			if (remove_arm)
			{
				targ->client->dismembered = false;
				//FIXME: the limb should just disappear, cuz I ate it
				G_DoDismemberment(targ, targ->currentOrigin, MOD_SABER, HL_ARM_RT, qtrue);
			}
		}
		else
		{
			//couldn't release him
			targ_undying = qtrue;
		}
	}

	if (attacker && attacker->client && !attacker->s.number)
	{
		if (!already_dead)
		{
			int add;
			if (take > targ->health)
			{
				add = targ->health;
			}
			else
			{
				add = take;
			}
			add += asave;
			add = ceil(add / 10.0f);
			if (attacker != targ)
			{
				G_TrackWeaponUsage(attacker, inflictor, add, mod);
			}
		}
	}

	if (take || dflags & DAMAGE_NO_DAMAGE)
	{
		if (!targ->client || !attacker->client)
		{
			targ->health = targ->health - take;
			if (targ->health < 0)
			{
				targ->health = 0;
			}
			if (targ_undying)
			{
				if (targ->health < 1)
				{
					G_ActivateBehavior(targ, BSET_DEATH);
					targ->health = 1;
				}
			}
		}
		else
		{
			//two clients
			team_t targ_team = TEAM_FREE;
			team_t attacker_team = TEAM_FREE;

			if (player->client->ps.viewEntity && targ->s.number == player->client->ps.viewEntity)
			{
				targ_team = player->client->playerTeam;
			}
			else if (targ->client)
			{
				targ_team = targ->client->playerTeam;
			}
			else
			{
				targ_team = targ->noDamageTeam;
			}
			if (player->client->ps.viewEntity && attacker->s.number == player->client->ps.viewEntity)
			{
				attacker_team = player->client->playerTeam;
			}
			else if (attacker->client)
			{
				attacker_team = attacker->client->playerTeam;
			}
			else
			{
				attacker_team = attacker->noDamageTeam;
			}

			if (targ_team != attacker_team || targ_team == TEAM_SOLO
				|| targ->s.number < MAX_CLIENTS && targ_team == TEAM_FREE //evil player hit
				|| attacker && attacker->s.number < MAX_CLIENTS && attacker_team == TEAM_FREE) //evil player attacked
			{
				//on opposite team
				if (targ->client && targ->health > 0 && attacker && (attacker->client && mod != MOD_FORCE_LIGHTNING) &&
					(attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker) && attacker->client && mod !=
						MOD_MELEE))
				{
					//targ is creature and attacker is creature
					if (take > targ->health)
					{
						//damage is greater than target's health, only give BP/FP reward for damage used to kill victim
						AddFatigueHurtBonusMax(attacker, targ, mod);
					}
					else
					{
						AddFatigueHurtBonus(attacker, targ, mod);
					}
				}

				targ->health = targ->health - take;

				//MCG - Falling should never kill player- only if a trigger_hurt does so.
				if (mod == MOD_FALLING && targ->s.number == 0 && targ->health < 1)
				{
					targ->health = 1;
				}
				else if (targ->health < 0)
				{
					targ->health = 0;
				}

				if (targ_undying)
				{
					if (targ->health < 1)
					{
						if (targ->NPC == nullptr || !(targ->NPC->aiFlags & NPCAI_ROSH) || !rosh_twin_present())
						{
							//NOTE: Rosh won't run his death script until he doesn't have the twins to heal him
							targ->client->dismembered = qtrue;
							G_ActivateBehavior(targ, BSET_DEATH);
						}
						targ->health = 1;
					}
				}
				else if (targ->health < 1 && attacker->client)
				{
					// The player or NPC just killed an enemy so increment the kills counter
					attacker->client->ps.persistant[PERS_ENEMIES_KILLED]++;
				}
			}
			else if (targ_team == TEAM_PLAYER && targ->s.number == 0 || targ_team == TEAM_PLAYER && attacker ==
				player || targ_team == TEAM_SOLO)
			{
				//on the same team, and target is an ally
				qboolean take_damage = qtrue;
				qboolean yell_at_attacker = qtrue;

				//1) player doesn't take damage from teammates unless they're angry at him
				if (targ->s.number == 0)
				{
					//the player
					if (attacker->enemy != targ && attacker != targ)
					{
						//an NPC shot the player by accident
						take_damage = qfalse;
					}
				}
				//2) NPCs don't take any damage from player during combat
				else
				{
					//an NPC
					if (dflags & DAMAGE_RADIUS && !(dflags & DAMAGE_IGNORE_TEAM))
					{
						//An NPC got hit by player and this is during combat or it was slash damage
						//NOTE: though it's not realistic to have teammates not take splash damage,
						//		even when not in combat, it feels really bad to have them able to
						//		actually be killed by the player's splash damage
						take_damage = qfalse;
					}

					if (dflags & DAMAGE_RADIUS)
					{
						//you're fighting and it's just radius damage, so don't even mention it
						yell_at_attacker = qfalse;
					}
				}

				if (take_damage)
				{
					if (targ->client && targ->health > 0 && attacker && (attacker->client && mod != MOD_FORCE_LIGHTNING)
						&& (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker) && attacker->client &&
							mod != MOD_MELEE))
					{
						//targ is creature and attacker is creature
						if (take > targ->health)
						{
							//damage is greater than target's health, only give BP/FP reward for damage used to kill victim
							AddFatigueHurtBonusMax(attacker, targ, mod);
						}
						else
						{
							AddFatigueHurtBonus(attacker, targ, mod);
						}
					}

					targ->health = targ->health - take;

					if (!already_dead && ((targ->flags & FL_UNDYING || targ->client->ps.forcePowersActive & 1 <<
						FP_RAGE) && !(dflags & DAMAGE_NO_PROTECTION) && attacker->s.number != 0 || dflags &
						DAMAGE_NO_KILL))
					{
						//guy is marked undying and we're not the player or we're in combat
						if (targ->health < 1)
						{
							G_ActivateBehavior(targ, BSET_DEATH);

							targ->health = 1;
						}
					}
					else if (!already_dead && ((targ->flags & FL_UNDYING || targ->client->ps.forcePowersActive & 1 <<
						FP_RAGE) && !(dflags & DAMAGE_NO_PROTECTION) && !attacker->s.number && !targ->s.number ||
						dflags
						& DAMAGE_NO_KILL))
					{
						// player is undying and he's attacking himself, don't let him die
						if (targ->health < 1)
						{
							G_ActivateBehavior(targ, BSET_DEATH);

							targ->health = 1;
						}
					}
					else if (targ->health < 0)
					{
						targ->health = 0;
						if (attacker->s.number == 0 && targ->NPC)
						{
							targ->NPC->scriptFlags |= SCF_FFDEATH;
						}
					}
				}

				if (yell_at_attacker)
				{
					if (!targ->NPC || !targ->NPC->charmedTime)
					{
						G_FriendlyFireReaction(targ, attacker, dflags);
					}
				}
			}
			else
			{
			}
		}

		if (targ->client)
		{
			targ->client->ps.stats[STAT_HEALTH] = targ->health;
			g_lastClientDamaged = targ;
		}

		//TEMP HACK FOR PLAYER LOOK AT ENEMY CODE
		//FIXME: move this to a player pain func?
		if (targ->s.number == 0)
		{
			if (!targ->enemy //player does not have an enemy yet
				|| targ->enemy->s.weapon != WP_SABER //or player's enemy is not a jedi
				|| attacker->s.weapon == WP_SABER) //and attacker is a jedi
				//keep enemy jedi over shooters
			{
				if (attacker->enemy == targ || !OnSameTeam(targ, attacker))
				{
					//don't set player's enemy to teammates that hit him by accident
					targ->enemy = attacker;
				}
				NPC_SetLookTarget(targ, attacker->s.number, level.time + 1000);
			}
		}
		else if (attacker->s.number == 0 && (!targ->NPC || !targ->NPC->timeOfDeath) && (mod == MOD_SABER || attacker->s.
			weapon != WP_SABER || !attacker->enemy || attacker->enemy->s.weapon != WP_SABER))
			//keep enemy jedi over shooters
		{
			//this looks dumb when they're on the ground and you keep hitting them, so only do this when first kill them
			if (!OnSameTeam(targ, attacker))
			{
				//don't set player's enemy to teammates that he hits by accident
				attacker->enemy = targ;
			}
			NPC_SetLookTarget(attacker, targ->s.number, level.time + 1000);
		}
		//TEMP HACK FOR PLAYER LOOK AT ENEMY CODE

		//add up the damage to the location
		if (targ->client)
		{
			if (targ->locationDamage[hit_loc] < Q3_INFINITE)
			{
				targ->locationDamage[hit_loc] += take;
			}
		}

		if (targ->health > 0 && targ->NPC && targ->NPC->surrenderTime > level.time)
		{
			//he was surrendering, goes down with one hit
			if (!targ->client || targ->client->NPC_class != CLASS_BOBAFETT && targ->client->NPC_class !=
				CLASS_MANDALORIAN && targ->client->NPC_class != CLASS_JANGO && targ->client->NPC_class !=
				CLASS_JANGODUAL)
			{
				targ->health = 0;
			}
		}

		if (targ->health <= 0)
		{
			if (knockback && dflags & DAMAGE_DEATH_KNOCKBACK)
			{
				if (mod == MOD_FLECHETTE || mod == MOD_BOWCASTER || mod == MOD_BOWCASTER_ALT
					|| mod == MOD_CONC || mod == MOD_CONC_ALT)
				{
					//special case because this is shotgun-ish damage, we need to multiply the knockback
					knockback *= 4;
				}
				else if (mod == MOD_FORCE_LIGHTNING)
				{
					//special case because this is shotgun-ish damage, we need to multiply the knockback
					knockback *= 1.5;
				}
				G_ApplyKnockback(targ, new_dir, knockback);
			}

			if (targ->health < -999)
				targ->health = -999;

			// If we are a breaking glass brush, store the damage point so we can do cool things with it.
			if (targ->svFlags & SVF_GLASS_BRUSH)
			{
				VectorCopy(point, targ->pos1);
				VectorCopy(dir, targ->pos2);
			}
			if (targ->client)
			{
				//HACK
				if (point)
				{
					VectorCopy(point, targ->pos1);
				}
				else
				{
					VectorCopy(targ->currentOrigin, targ->pos1);
				}
			}
			if (!already_dead && !targ->enemy)
			{
				//just killed and didn't have an enemy before
				targ->enemy = attacker;
			}

			GEntity_DieFunc(targ, inflictor, attacker, take, mod, dflags, hit_loc);
		}
		else
		{
			GEntity_PainFunc(targ, inflictor, attacker, point, take, mod, hit_loc);
			if (targ->s.number == 0)
			{
				//player run pain script
				G_ActivateBehavior(targ, BSET_PAIN);
				if (targ->health <= 25)
				{
					G_ActivateBehavior(targ, BSET_FLEE);
				}
			}
		}
	}
}

/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage(const gentity_t* targ, const vec3_t origin)
{
	vec3_t dest;
	trace_t tr;
	vec3_t midpoint;
	qboolean cant_hit_ent = qtrue;

	if (targ->contents & MASK_SOLID)
	{
		//can hit it
		if (targ->s.solid == SOLID_BMODEL)
		{
			//but only if it's a brush model
			cant_hit_ent = qfalse;
		}
	}

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin at 0,0,0
	VectorAdd(targ->absmin, targ->absmax, midpoint);
	VectorScale(midpoint, 0.5, midpoint);

	VectorCopy(midpoint, dest);
	/*
	vec3_t blah;
	VectorCopy( origin, blah);
	G_DebugLine(blah, dest, 5000, 0x0000ff, qtrue );
	*/
	gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, static_cast<EG2_Collision>(0), 0);
	if (tr.fraction == 1.0 && cant_hit_ent || tr.entityNum == targ->s.number)
		// if we also test the entitynum's we can bust up bbrushes better!
		return qtrue;

	// this should probably check in the plane of projection,
	// rather than in world coordinate, and also include Z
	VectorCopy(midpoint, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, static_cast<EG2_Collision>(0), 0);
	if (tr.fraction == 1.0 && cant_hit_ent || tr.entityNum == targ->s.number)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, static_cast<EG2_Collision>(0), 0);
	if (tr.fraction == 1.0 && cant_hit_ent || tr.entityNum == targ->s.number)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, static_cast<EG2_Collision>(0), 0);
	if (tr.fraction == 1.0 && cant_hit_ent || tr.entityNum == targ->s.number)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, static_cast<EG2_Collision>(0), 0);
	if (tr.fraction == 1.0 && cant_hit_ent || tr.entityNum == targ->s.number)
		return qtrue;

	return qfalse;
}

extern void Boba_DustFallNear(const vec3_t origin, int dustcount);
extern void G_GetMassAndVelocityForEnt(const gentity_t* ent, float* mass, vec3_t velocity);
/*
============
G_RadiusDamage
============
*/
void G_RadiusDamage(const vec3_t origin, gentity_t* attacker, const float damage, float radius,
	const gentity_t* ignore, const int mod)
{
	gentity_t* entity_list[MAX_GENTITIES];
	vec3_t mins{}, maxs{};
	vec3_t v{};
	vec3_t dir;
	int i;
	int d_flags = DAMAGE_RADIUS;

	if (radius < 1)
	{
		radius = 1;
	}

	for (i = 0; i < 3; i++)
	{
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	if (mod == MOD_ROCKET)
	{
		Boba_DustFallNear(origin, 10);
	}

	if (mod == MOD_GAS)
	{
		d_flags |= DAMAGE_NO_KNOCKBACK;
	}

	const int num_listed_entities = gi.EntitiesInBox(mins, maxs, entity_list, MAX_GENTITIES);

	for (int e = 0; e < num_listed_entities; e++)
	{
		gentity_t* ent = entity_list[e];

		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;
		if (!ent->contents)
			continue;

		// find the distance from the edge of the bounding box
		for (i = 0; i < 3; i++)
		{
			if (origin[i] < ent->absmin[i])
			{
				v[i] = ent->absmin[i] - origin[i];
			}
			else if (origin[i] > ent->absmax[i])
			{
				v[i] = origin[i] - ent->absmax[i];
			}
			else
			{
				v[i] = 0;
			}
		}

		const float dist = VectorLength(v);
		if (dist >= radius)
		{
			continue;
		}

		float points = damage * (1.0 - dist / radius);

		// Lessen damage to vehicles that are moving away from the explosion
		if (ent->client && (ent->client->NPC_class == CLASS_VEHICLE || G_IsRidingVehicle(ent)))
		{
			const gentity_t* bike = ent;

			if (G_IsRidingVehicle(ent) && ent->owner)
			{
				bike = ent->owner;
			}

			vec3_t veh_move_direction;

			float mass;
			G_GetMassAndVelocityForEnt(bike, &mass, veh_move_direction);
			const float veh_move_speed = VectorNormalize(veh_move_direction);
			if (veh_move_speed > 300.0f)
			{
				vec3_t explosion_direction;
				VectorSubtract(bike->currentOrigin, origin, explosion_direction);
				VectorNormalize(explosion_direction);

				const float explosionDirectionSimilarity = DotProduct(veh_move_direction, explosion_direction);
				if (explosionDirectionSimilarity > 0.0f)
				{
					points *= 1.0f - explosionDirectionSimilarity;
				}
			}
		}

		if (CanDamage(ent, origin))
		{
			//FIXME: still do a little damage in in PVS and close?
			if (ent->svFlags & (SVF_GLASS_BRUSH | SVF_BBRUSH))
			{
				VectorAdd(ent->absmin, ent->absmax, v);
				VectorScale(v, 0.5f, v);
			}
			else
			{
				VectorCopy(ent->currentOrigin, v);
			}

			VectorSubtract(v, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;

			if (ent->svFlags & SVF_GLASS_BRUSH)
			{
				if (points > 1.0f)
				{
					// we want to cap this at some point, otherwise it just gets crazy
					if (points > 6.0f)
					{
						VectorScale(dir, 6.0f, dir);
					}
					else
					{
						VectorScale(dir, points, dir);
					}
				}

				ent->splashRadius = radius; // * ( 1.0 - dist / radius );
			}

			G_Damage(ent, nullptr, attacker, dir, origin, static_cast<int>(points), d_flags, mod);
		}
	}
}

//Combat Reward Code
void AddFatigueKillBonus(const gentity_t* attacker, const gentity_t* victim, const int means_of_death)
{
	const qboolean is_holding_block_button = attacker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;

	//get a bonus for killing an enemy
	if (!attacker || !attacker->client || !victim || !victim->client)
	{
		return;
	}

	if (victim->s.weapon == WP_TURRET ||
		victim->s.weapon == WP_ATST_MAIN ||
		victim->s.weapon == WP_ATST_SIDE ||
		victim->s.weapon == WP_EMPLACED_GUN ||
		victim->s.weapon == WP_BOT_LASER ||
		victim->s.weapon == WP_TIE_FIGHTER ||
		victim->s.weapon == WP_RAPID_FIRE_CONC)
	{
		return;
	}

	if (means_of_death == MOD_CRUSH || means_of_death == MOD_FORCE_GRIP || means_of_death == MOD_LIGHTNING_STRIKE ||
		means_of_death == MOD_FORCE_LIGHTNING || means_of_death == MOD_FORCE_DRAIN)
	{
		return;
	}

	if (!g_SerenityJediEngineMode->integer)
	{
		return;
	}

	if (is_holding_block_button && (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker)))
		//DONT GET THIS BONUS IF YOUR A BLOCK SPAMMER
	{
		//add bonus
		if (g_SerenityJediEngineMode->integer == 2)
		{
			WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUSMAX);
		}
		else
		{
			WP_ForcePowerRegenerate(attacker, FATIGUE_HURTBONUSMAX);
		}
	}
	else
	{
		//add bonus
		if (g_SerenityJediEngineMode->integer == 2)
		{
			WP_BlockPointsRegenerate(attacker, FATIGUE_KILLBONUS);
		}
		else
		{
			WP_ForcePowerRegenerate(attacker, FATIGUE_KILLBONUS);
		}
		if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_TEN)
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
		}
	}
}

void AddFatigueHurtBonus(const gentity_t* attacker, const gentity_t* victim, const int mod)
{
	const qboolean is_holding_block_button = attacker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;

	//get a small bonus for killing an enemy
	if (!attacker || !attacker->client || !victim || !victim->client)
	{
		return;
	}

	if (victim->s.weapon == WP_TURRET ||
		victim->s.weapon == WP_ATST_MAIN ||
		victim->s.weapon == WP_ATST_SIDE ||
		victim->s.weapon == WP_EMPLACED_GUN ||
		victim->s.weapon == WP_BOT_LASER ||
		victim->s.weapon == WP_TIE_FIGHTER ||
		victim->s.weapon == WP_RAPID_FIRE_CONC)
	{
		return;
	}

	if (mod == MOD_CRUSH || mod == MOD_FORCE_GRIP || mod == MOD_LIGHTNING_STRIKE || mod == MOD_FORCE_LIGHTNING || mod ==
		MOD_FORCE_DRAIN)
	{
		return;
	}

	if (!g_SerenityJediEngineMode->integer)
	{
		return;
	}

	if (is_holding_block_button && (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker)))
		//DONT GET THIS BONUS IF YOUR A BLOCK SPAMMER
	{
		//add bonus
		if (g_SerenityJediEngineMode->integer == 2)
		{
			WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUSMIN);
		}
		else
		{
			WP_ForcePowerRegenerate(attacker, FATIGUE_HURTBONUSMIN);
		}
	}
	else
	{
		//add bonus
		if (g_SerenityJediEngineMode->integer == 2)
		{
			if (attacker->s.client_num >= MAX_CLIENTS && !G_ControlledByPlayer(attacker))
			{
				WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUSMAX);
			}
			else
			{
				WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUS);
			}
		}
		else
		{
			WP_ForcePowerRegenerate(attacker, FATIGUE_HURTBONUS);
		}
		if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_TEN)
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
		}
	}
}

void AddFatigueHurtBonusMax(const gentity_t* attacker, const gentity_t* victim, const int mod)
{
	const qboolean is_holding_block_button = attacker->client->ps.ManualBlockingFlags & 1 << HOLDINGBLOCK ? qtrue : qfalse;

	//get a small bonus for killing an enemy
	if (!attacker || !attacker->client || !victim || !victim->client)
	{
		return;
	}

	if (victim->s.weapon == WP_TURRET ||
		victim->s.weapon == WP_ATST_MAIN ||
		victim->s.weapon == WP_ATST_SIDE ||
		victim->s.weapon == WP_EMPLACED_GUN ||
		victim->s.weapon == WP_BOT_LASER ||
		victim->s.weapon == WP_TIE_FIGHTER ||
		victim->s.weapon == WP_RAPID_FIRE_CONC)
	{
		return;
	}

	if (mod == MOD_CRUSH || mod == MOD_FORCE_GRIP || mod == MOD_LIGHTNING_STRIKE || mod == MOD_FORCE_LIGHTNING || mod ==
		MOD_FORCE_DRAIN)
	{
		return;
	}

	if (!g_SerenityJediEngineMode->integer)
	{
		return;
	}

	if (is_holding_block_button && (attacker->s.number < MAX_CLIENTS || G_ControlledByPlayer(attacker)))
		//DONT GET THIS BONUS IF YOUR A BLOCK SPAMMER
	{
		//add bonus
		if (g_SerenityJediEngineMode->integer == 2)
		{
			WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUSMIN);
		}
		else
		{
			WP_ForcePowerRegenerate(attacker, FATIGUE_HURTBONUSMIN);
		}
	}
	else
	{
		//add bonus
		if (g_SerenityJediEngineMode->integer == 2)
		{
			WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUSMAX);
		}
		else
		{
			WP_ForcePowerRegenerate(attacker, FATIGUE_HURTBONUSMAX);
		}
		if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_TEN)
		{
			attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
		}
	}
}

void AddFatigueMeleeBonus(const gentity_t* attacker, const gentity_t* victim)
{
	//get a small bonus for slapping an enemy
	if (!attacker || !attacker->client || !victim || !victim->client)
	{
		return;
	}

	if (victim->s.weapon == WP_TURRET ||
		victim->s.weapon == WP_ATST_MAIN ||
		victim->s.weapon == WP_ATST_SIDE ||
		victim->s.weapon == WP_EMPLACED_GUN ||
		victim->s.weapon == WP_BOT_LASER ||
		victim->s.weapon == WP_TIE_FIGHTER ||
		victim->s.weapon == WP_RAPID_FIRE_CONC)
	{
		return;
	}

	//add bonus
	if (g_SerenityJediEngineMode->integer == 2)
	{
		WP_BlockPointsRegenerate(attacker, FATIGUE_HURTBONUS);
	}
	else
	{
		WP_ForcePowerRegenerate(attacker, FATIGUE_HURTBONUS);
	}
	if (attacker->client->ps.saberFatigueChainCount >= MISHAPLEVEL_TEN)
	{
		attacker->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
	}
}

void AddNPCBlockPointBonus(const gentity_t* self)
{
	//get a small bonus
	//add bonus
	WP_BlockPointsRegenerate(self, FATIGUE_DAMAGEBONUS);

	if (self->client->ps.saberFatigueChainCount >= MISHAPLEVEL_TEN)
	{
		self->client->ps.saberFatigueChainCount = MISHAPLEVEL_LIGHT;
	}
}