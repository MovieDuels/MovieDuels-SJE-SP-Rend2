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

#include "g_local.h"
#include "g_functions.h"
#include "g_items.h"
#include "wp_saber.h"
#include "../cgame/cg_local.h"
#include "b_local.h"
#include "ghoul2_shared.h"
#include <qcommon\q_color.h>
#include <cassert>

extern qboolean missionInfo_Updated;
extern cvar_t* com_kotor;
extern void CrystalAmmoSettings(gentity_t* ent);
extern void ChangeWeapon(const gentity_t* ent, int new_weapon);
extern qboolean PM_InKnockDown(const playerState_t* ps);
extern qboolean PM_InGetUp(const playerState_t* ps);
extern void WP_SetSaber(gentity_t* ent, int saber_num, const char* saber_name);
extern void WP_RemoveSaber(gentity_t* ent, int saber_num);
extern void WP_SaberFallSound(const gentity_t* owner, const gentity_t* saber);
extern saber_colors_t TranslateSaberColor(const char* name);
extern qboolean G_ControlledByPlayer(const gentity_t* self);
extern cvar_t* g_SerenityJediEngineMode;
extern void WP_ForcePowerDrain(const gentity_t* self, forcePowers_t force_power, int override_amt);
extern qboolean droideka_npc(const gentity_t* ent);
extern void Weapon_MandoGrappleHook_Fire(gentity_t* ent);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean JET_Flying(const gentity_t* self);

extern cvar_t* g_spskill;
extern cvar_t* g_sex;
extern cvar_t* g_saberPickuppableDroppedSabers;

constexpr auto MAX_BACTA_HEAL_AMOUNT = 25;

/*

  Items are any object that a player can touch to gain some effect.

  Pickup will return the number of seconds until they should respawn.

  all items should pop when dropped in lava or slime

  Respawnable items don't actually go away when picked up, they are
  just made invisible and untouchable.  This allows them to ride
  movers and respawn apropriately.
*/

// Item Spawn flags
constexpr auto ITMSF_SUSPEND = 1;
constexpr auto ITMSF_NOPLAYER = 2;
constexpr auto ITMSF_ALLOWNPC = 4;
constexpr auto ITMSF_NOTSOLID = 8;
constexpr auto ITMSF_VERTICAL = 16;
constexpr auto ITMSF_INVISIBLE = 32;
constexpr auto ITMSF_NOGLOW = 64;
constexpr auto ITMSF_USEPICKUP = 128;
constexpr auto ITMSF_STATIONARY = 2048;

//======================================================================

/*
===============
G_InventorySelectable
===============
*/
qboolean G_InventorySelectable(const int index, const gentity_t* other)
{
	if (other->client->ps.inventory[index])
	{
		return qtrue;
	}

	return qfalse;
}

extern qboolean INV_GoodieKeyGive(const gentity_t* target);
extern qboolean INV_SecurityKeyGive(const gentity_t* target, const char* keyname);

static int Pickup_Holdable(const gentity_t* ent, const gentity_t* other)
{
	other->client->ps.stats[STAT_ITEMS] |= 1 << ent->item->giTag;

	if (ent->item->giTag == INV_SECURITY_KEY)
	{
		//give the key
		//FIXME: temp message
		gi.SendServerCommand(0, "cp @SP_INGAME_YOU_TOOK_SECURITY_KEY");
		INV_SecurityKeyGive(other, ent->message);
	}
	else if (ent->item->giTag == INV_GOODIE_KEY)
	{
		//give the key
		//FIXME: temp message
		gi.SendServerCommand(0, "cp @SP_INGAME_YOU_TOOK_SUPPLY_KEY");
		INV_GoodieKeyGive(other);
	}
	else
	{
		// Picking up a normal item?
		other->client->ps.inventory[ent->item->giTag]++;
	}
	// Got a security key

	// Set the inventory select, just in case it hasn't
	const int original = cg.inventorySelect;
	for (int i = 0; i < INV_MAX; i++)
	{
		if (cg.inventorySelect < INV_ELECTROBINOCULARS || cg.inventorySelect >= INV_MAX)
		{
			cg.inventorySelect = INV_MAX - 1;
		}

		if (G_InventorySelectable(cg.inventorySelect, other))
		{
			return 60;
		}
		cg.inventorySelect++;
	}

	cg.inventorySelect = original;

	return 60;
}

//======================================================================
int Add_Ammo2(const gentity_t* ent, const int ammoType, const int count)
{
	if (ammoType != AMMO_FORCE)
	{
		ent->client->ps.ammo[ammoType] += count;

		// since the ammo is the weapon in this case, picking up ammo should actually give you the weapon
		switch (ammoType)
		{
		case AMMO_THERMAL:
			ent->client->ps.weapons[WP_THERMAL] = 1;
			break;
		case AMMO_DETPACK:
			ent->client->ps.weapons[WP_DET_PACK] = 1;
			break;
		case AMMO_TRIPMINE:
			ent->client->ps.weapons[WP_TRIP_MINE] = 1;
			break;
		default:;
		}

		if (ent->client->ps.ammo[ammoType] > ammoData[ammoType].max)
		{
			ent->client->ps.ammo[ammoType] = ammoData[ammoType].max;
			return qfalse;
		}
	}
	else
	{
		if (ent->client->ps.forcePower >= ammoData[ammoType].max)
		{
			//if have full force, just get 25 extra per crystal
			ent->client->ps.forcePower += 25;
		}
		else
		{
			//else if don't have full charge, give full amount, up to max + 25
			ent->client->ps.forcePower += count;
			if (ent->client->ps.forcePower >= ammoData[ammoType].max + 25)
			{
				//cap at max + 25
				ent->client->ps.forcePower = ammoData[ammoType].max + 25;
			}
		}

		if (ent->client->ps.forcePower >= ammoData[ammoType].max * 2)
		{
			//always cap at twice a full charge
			ent->client->ps.forcePower = ammoData[ammoType].max * 2;
			return qfalse; // can't hold any more
		}
	}
	return qtrue;
}

//-------------------------------------------------------
void Add_Ammo(const gentity_t* ent, const int weapon, const int count)
{
	Add_Ammo2(ent, weaponData[weapon].ammoIndex, count);
}

//-------------------------------------------------------
static int Pickup_Ammo(const gentity_t* ent, const gentity_t* other)
{
	int quantity;

	if (ent->count)
	{
		quantity = ent->count;
	}
	else
	{
		quantity = ent->item->quantity;
	}

	Add_Ammo2(other, ent->item->giTag, quantity);

	return 30;
}

//======================================================================
void Add_Batteries(gentity_t* ent, int* count)
{
	if (ent->client && ent->client->ps.batteryCharge < MAX_BATTERIES && *count)
	{
		if (*count + ent->client->ps.batteryCharge > MAX_BATTERIES)
		{
			// steal what we need, then leave the rest for later
			*count -= MAX_BATTERIES - ent->client->ps.batteryCharge;
			ent->client->ps.batteryCharge = MAX_BATTERIES;
		}
		else
		{
			// just drain all of the batteries
			ent->client->ps.batteryCharge += *count;
			*count = 0;
		}

		G_AddEvent(ent, EV_BATTERIES_CHARGED, 0);
	}
}

//-------------------------------------------------------
static int Pickup_Battery(const gentity_t* ent, gentity_t* other)
{
	int quantity;

	if (ent->count)
	{
		quantity = ent->count;
	}
	else
	{
		quantity = ent->item->quantity;
	}

	// There may be some left over in quantity if the player is close to full, but with pickup items, this amount will just be lost
	Add_Batteries(other, &quantity);

	return 30;
}

//======================================================================

static void G_CopySaberItemValues(const gentity_t* pickUpSaber, gentity_t* oldSaber)
{
	if (oldSaber && pickUpSaber)
	{
		oldSaber->spawnflags = pickUpSaber->spawnflags;
		oldSaber->random = pickUpSaber->random;
		oldSaber->flags = pickUpSaber->flags;
	}
}

gentity_t* G_DropSaberItem(const char* saberType, const saber_colors_t saberColor, vec3_t saberPos, vec3_t saberVel, vec3_t saberAngles, const gentity_t* copySaber)
{
	//turn it into a pick-uppable item!
	gentity_t* newItem = nullptr;
	if (saberType
		&& saberType[0])
	{
		//have a valid string to use for saberType
		newItem = G_Spawn();
		if (newItem)
		{
			newItem->classname = G_NewString("weapon_saber");
			VectorCopy(saberPos, newItem->s.origin);
			G_SetOrigin(newItem, newItem->s.origin);
			VectorCopy(saberAngles, newItem->s.angles);
			G_SetAngles(newItem, newItem->s.angles);
			newItem->spawnflags = 128; /*ITMSF_USEPICKUP*/
			newItem->spawnflags |= 64; /*ITMSF_NOGLOW*/
			newItem->NPC_type = G_NewString(saberType); //saberType
			//FIXME: transfer per-blade color somehow?
			if (saberColor >= SABER_RGB)
			{
				char rgbColor[8];
				Com_sprintf(rgbColor, 8, "x%02x%02x%02x", saberColor & 0xff, saberColor >> 8 & 0xff,saberColor >> 16 & 0xff);
				newItem->NPC_targetname = rgbColor;
			}
			else if (saberColor >= 0 && saberColor < SABER_RGB)
			{
				newItem->NPC_targetname = const_cast<char*>(saberColorStringForColor[saberColor]); //This fixes an unchecked lowerbound for enum sabercolor used as index
			}
			else
			{
				newItem->NPC_targetname = nullptr;
			}
			newItem->count = 1;
			newItem->flags = FL_DROPPED_ITEM;
			G_SpawnItem(newItem, FindItemForWeapon(WP_SABER));
			newItem->s.pos.trType = TR_GRAVITY;
			newItem->s.pos.trTime = level.time;
			VectorCopy(saberVel, newItem->s.pos.trDelta);
			//newItem->s.eFlags |= EF_BOUNCE_HALF;
			//copy some values from another saber, if provided:
			G_CopySaberItemValues(copySaber, newItem);
			//don't *think* about calling FinishSpawningItem, just do it!
			newItem->e_ThinkFunc = thinkF_NULL;
			newItem->nextthink = -1;
			FinishSpawningItem(newItem);
			newItem->delay = level.time + 500; //so you can't pick it back up right away
		}
	}
	return newItem;
}

extern void G_SetSabersFromCVars(gentity_t* ent);

static qboolean Pickup_Saber(gentity_t* self, qboolean hadSaber, gentity_t* pickUpSaber)
{
	//NOTE: loopAnim = saberSolo, alt_fire = saberLeftHand, NPC_type = saberType, NPC_targetname = saberColor
	qboolean foundIt = qfalse;

	if (!pickUpSaber || !self || !self->client)
	{
		return qfalse;
	}

	//G_RemoveWeaponModels( ent );//???
	if (Q_stricmp("player", pickUpSaber->NPC_type) == 0)
	{
		//"player" means use cvar info
		G_SetSabersFromCVars(self);
		foundIt = qtrue;
	}
	else
	{
		saberInfo_t newSaber = { nullptr };
		qboolean swapSabers = qfalse;

		if (self->client->ps.weapon == WP_SABER
			&& self->client->ps.weaponTime > 0)
		{
			//can't pick up a new saber while the old one is busy (also helps to work as a debouncer so you don't swap out sabers rapidly when touching more than one at a time)
			return qfalse;
		}

		if (pickUpSaber->count == 1
			&& g_saberPickuppableDroppedSabers->integer)
		{
			swapSabers = qtrue;
		}

		if (WP_SaberParseParms(pickUpSaber->NPC_type, &newSaber))
		{
			//successfully found a saber .sab entry to use
			int saber_num = 0;
			qboolean removeLeftSaber = qfalse;
			if (pickUpSaber->alt_fire)
			{
				//always go in the left hand
				if (!hadSaber)
				{
					//can't have a saber only in your left hand!
					return qfalse;
				}
				saber_num = 1;
				//just in case...
				removeLeftSaber = qtrue;
			}
			else if (!hadSaber)
			{
				//don't have a saber at all yet, put it in our right hand
				saber_num = 0;
				//just in case...
				removeLeftSaber = qtrue;
			}
			else if (pickUpSaber->loopAnim //only supposed to use this one saber when grab this pickup
				|| newSaber.saberFlags & SFL_TWO_HANDED //new saber is two-handed
				|| hadSaber && self->client->ps.saber[0].saberFlags & SFL_TWO_HANDED) //old saber is two-handed
			{
				//replace the old right-hand saber and remove the left hand one
				saber_num = 0;
				removeLeftSaber = qtrue;
			}
			else
			{
				//have, at least, a saber in our right hand and the new one could go in either left or right hand
				if (self->client->ps.dualSabers)
				{
					//I already have 2 sabers
					vec3_t dir2Saber, rightDir;
					//to determine which one to replace, see which side of me it's on
					VectorSubtract(pickUpSaber->currentOrigin, self->currentOrigin, dir2Saber);
					dir2Saber[2] = 0;
					AngleVectors(self->currentAngles, nullptr, rightDir, nullptr);
					rightDir[2] = 0;
					if (DotProduct(rightDir, dir2Saber) > 0)
					{
						saber_num = 0;
					}
					else
					{
						saber_num = 1;
						//just in case...
						removeLeftSaber = qtrue;
					}
				}
				else
				{
					//just add it as a second saber
					saber_num = 1;
					//just in case...
					removeLeftSaber = qtrue;
				}
			}
			if (saber_num == 0)
			{
				//want to reach out with right hand
				if (self->client->ps.torsoAnim == BOTH_BUTTON_HOLD)
				{
					//but only if already playing the pickup with left hand anim...
					NPC_SetAnim(self, SETANIM_TORSO, BOTH_SABERPULL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
				}
				if (swapSabers)
				{
					//drop first one where the one we're picking up is
					G_DropSaberItem(self->client->ps.saber[saber_num].name,
						self->client->ps.saber[saber_num].blade[0].color, pickUpSaber->currentOrigin,
						vec3_origin, pickUpSaber->currentAngles, pickUpSaber);
					if (removeLeftSaber)
					{
						//drop other one at my origin
						G_DropSaberItem(self->client->ps.saber[1].name, self->client->ps.saber[1].blade[0].color,
							self->currentOrigin, vec3_origin, self->currentAngles, pickUpSaber);
					}
				}
			}
			else
			{
				if (swapSabers)
				{
					G_DropSaberItem(self->client->ps.saber[saber_num].name,
						self->client->ps.saber[saber_num].blade[0].color, pickUpSaber->currentOrigin,
						vec3_origin, pickUpSaber->currentAngles, pickUpSaber);
				}
			}
			if (removeLeftSaber)
			{
				WP_RemoveSaber(self, 1);
			}
			WP_SetSaber(self, saber_num, pickUpSaber->NPC_type);
			WP_SaberInitBladeData(self);
			if (self->client->ps.saber[saber_num].stylesLearned)
			{
				self->client->ps.saberStylesKnown |= self->client->ps.saber[saber_num].stylesLearned;
			}
			if (self->client->ps.saber[saber_num].singleBladeStyle)
			{
				self->client->ps.saberStylesKnown |= self->client->ps.saber[saber_num].singleBladeStyle;
			}
			if (pickUpSaber->NPC_targetname != nullptr)
			{
				//NPC_targetname = saberColor
				saber_colors_t saber_color = TranslateSaberColor(pickUpSaber->NPC_targetname);
				for (auto& blade_num : self->client->ps.saber[saber_num].blade)
				{
					blade_num.color = saber_color;
				}
			}
			if (self->client->ps.torsoAnim == BOTH_BUTTON_HOLD
				|| self->client->ps.torsoAnim == BOTH_SABERPULL)
			{
				//don't let them attack right away, force them to finish the anim
				self->client->ps.weaponTime = self->client->ps.torsoAnimTimer;
			}
			else
			{
				self->client->ps.saber_move = LS_READY;
			}
			foundIt = qtrue;
		}
		WP_SaberFreeStrings(newSaber);
	}
	return foundIt;
}

extern void CG_ChangeWeapon(int num);

static int Pickup_Weapon(gentity_t* ent, gentity_t* other)
{
	int quantity;
	qboolean hadWeapon = qfalse;

	// dropped items are always picked up
	if (ent->flags & FL_DROPPED_ITEM)
	{
		quantity = ent->count;
	}
	else
	{
		//wasn't dropped
		quantity = ent->item->quantity ? ent->item->quantity : 50;
	}

	// add the weapon
	if (other->client->ps.weapons[ent->item->giTag])
	{
		hadWeapon = qtrue;
	}
	other->client->ps.weapons[ent->item->giTag] = 1;

	if (ent->item->giTag == WP_SABER && (!hadWeapon || ent->NPC_type != nullptr))
	{
		//didn't have a saber or it is specifying a certain kind of saber to use
		if (!Pickup_Saber(other, hadWeapon, ent))
		{
			return 0;
		}
	}

	if (other->s.number)
	{
		//NPC
		if (other->s.weapon == WP_NONE
			|| ent->item->giTag == WP_SABER)
		{
			//NPC with no weapon picked up a weapon, change to this weapon
			other->client->ps.weapon = ent->item->giTag;
			other->client->ps.weaponstate = WEAPON_RAISING;
			ChangeWeapon(other, ent->item->giTag);
			if (ent->item->giTag == WP_SABER)
			{
				other->client->ps.SaberActivate();
				WP_SaberAddG2SaberModels(other);
				G_RemoveHolsterModels(ent);
			}
			else
			{
				if (com_kotor->integer == 1) //playing kotor
				{
					G_CreateG2AttachedWeaponModel(other, weaponData[ent->item->giTag].altweaponMdl, other->handRBolt, 0);
				}
				else
				{
					if (other->client->charKOTORWeapons == 1)
					{
						G_CreateG2AttachedWeaponModel(other, weaponData[ent->item->giTag].altweaponMdl, other->handRBolt, 0);
					}
					else
					{
						G_CreateG2AttachedWeaponModel(other, weaponData[ent->item->giTag].weaponMdl, other->handRBolt, 0);
					}
				}
				//holster sabers
				WP_SaberAddHolsteredG2SaberModels(ent);
			}
		}
	}
	if (ent->item->giTag == WP_SABER)
	{
		//picked up a saber
		if (other->s.weapon != WP_SABER)
		{
			//player picking up saber
			other->client->ps.weapon = WP_SABER;
			other->client->ps.weaponstate = WEAPON_RAISING;
			if (other->s.number < MAX_CLIENTS)
			{
				//make sure the cgame-side knows this
				CG_ChangeWeapon(WP_SABER);
			}
			else
			{
				//make sure the cgame-side knows this
				ChangeWeapon(other, WP_SABER);
			}
		}
		if (!other->client->ps.SaberActive())
		{
			//turn it/them on!
			other->client->ps.SaberActivate();
		}
	}

	if (quantity)
	{
		// Give ammo
		Add_Ammo(other, ent->item->giTag, quantity);
	}
	return 5;
}

//======================================================================

int ITM_AddHealth(gentity_t* ent, const int count)
{
	ent->health += count;

	if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH]) // Past max health
	{
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

		return qfalse;
	}

	return qtrue;
}

static int Pickup_Health(const gentity_t* ent, gentity_t* other)
{
	int quantity;

	const int max = other->client->ps.stats[STAT_MAX_HEALTH];

	if (ent->count)
	{
		quantity = ent->count;
	}
	else
	{
		quantity = ent->item->quantity;
	}

	other->health += quantity;

	if (other->health > max)
	{
		other->health = max;
	}

	if (ent->item->giTag == 100)
	{
		// mega health respawns slow
		return 120;
	}

	return 30;
}

//======================================================================

int ITM_AddArmor(const gentity_t* ent, const int count)
{
	ent->client->ps.stats[STAT_ARMOR] += count;

	if (ent->client->ps.stats[STAT_ARMOR] > ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
		return qfalse;
	}

	return qtrue;
}

static int Pickup_Armor(const gentity_t* ent, const gentity_t* other)
{
	// make sure that the shield effect is on
	other->client->ps.powerups[PW_BATTLESUIT] = Q3_INFINITE;

	other->client->ps.stats[STAT_ARMOR] += ent->item->quantity;
	if (other->client->ps.stats[STAT_ARMOR] > other->client->ps.stats[STAT_MAX_HEALTH])
	{
		other->client->ps.stats[STAT_ARMOR] = other->client->ps.stats[STAT_MAX_HEALTH];
	}

	return 30;
}

//======================================================================

static int Pickup_Holocron(const gentity_t* ent, const gentity_t* other)
{
	const int forcePower = ent->item->giTag;
	const int forceLevel = ent->count;
	// check if out of range
	if (forceLevel < 0 || forceLevel >= NUM_FORCE_POWER_LEVELS)
	{
		gi.Printf(" Pickup_Holocron : count %d not in valid range\n", forceLevel);
		return 1;
	}

	// don't pick up if already known AND your level is higher than pickup level
	if (other->client->ps.forcePowersKnown & 1 << forcePower)
	{
		//don't pickup if item is lower than current level
		if (other->client->ps.forcePowerLevel[forcePower] >= forceLevel)
		{
			return 1;
		}
	}

	other->client->ps.forcePowerLevel[forcePower] = forceLevel;
	other->client->ps.forcePowersKnown |= 1 << forcePower;

	missionInfo_Updated = qtrue; // Activate flashing text
	gi.cvar_set("cg_updatedDataPadForcePower1", va("%d", forcePower + 1)); // The +1 is offset in the print routine.
	cg_updatedDataPadForcePower1.integer = forcePower + 1;
	gi.cvar_set("cg_updatedDataPadForcePower2", "0"); // The +1 is offset in the print routine.
	cg_updatedDataPadForcePower2.integer = 0;
	gi.cvar_set("cg_updatedDataPadForcePower3", "0"); // The +1 is offset in the print routine.
	cg_updatedDataPadForcePower3.integer = 0;

	return 1;
}

//======================================================================

/*
===============
RespawnItem
===============
*/
void RespawnItem(gentity_t* ent)
{
}

qboolean CheckItemCanBePickedUpByNPC(const gentity_t* item, const gentity_t* pickerupper)
{
	if (!item->item)
	{
		return qfalse;
	}
	if (item->item->giType == IT_HOLDABLE && item->item->giTag == INV_SECURITY_KEY)
	{
		return qfalse;
	}
	if (item->flags & FL_DROPPED_ITEM
		&& item->activator != &g_entities[0]
		&& pickerupper->s.number
		&& pickerupper->s.weapon == WP_NONE
		&& pickerupper->enemy
		&& pickerupper->painDebounceTime < level.time
		&& pickerupper->NPC && pickerupper->NPC->surrenderTime < level.time //not surrendering
		&& !(pickerupper->NPC->scriptFlags & SCF_FORCED_MARCH)) // not being forced to march
	{
		//non-player, in combat, picking up a dropped item that does NOT belong to the player and it *not* a security key
		if (level.time - item->s.time < 3000) //was 5000
		{
			return qfalse;
		}
		return qtrue;
	}
	return qfalse;
}

qboolean G_CanPickUpWeapons(const gentity_t* other)
{
	if (!other || !other->client)
	{
		return qfalse;
	}
	switch (other->client->NPC_class)
	{
	case CLASS_ATST:
	case CLASS_GONK:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_MOUSE:
	case CLASS_PROBE:
	case CLASS_PROTOCOL:
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_SEEKER:
	case CLASS_REMOTE:
	case CLASS_RANCOR:
	case CLASS_WAMPA:
	case CLASS_JAWA: //FIXME: in some cases it's okay?
	case CLASS_UGNAUGHT: //FIXME: in some cases it's okay?
	case CLASS_SENTRY:
	case CLASS_PROJECTION:
		return qfalse;
	default:
		break;
	}
	return qtrue;
}

/*
===============
Touch_Item
===============
*/
extern cvar_t* g_timescale;
extern qboolean IsWeponPickUpAllowed(const gentity_t* self);
extern cvar_t* g_AdvancedWeaponPickup;

void Touch_Item(gentity_t* ent, gentity_t* other, trace_t* trace)
{
	int respawn;

	if (!other->client)
	{
		return;
	}

	if (other->health < 1)
	{
		return;
	}

	if (other->client->NPC_class == CLASS_DROIDEKA && (ent->item->giType == IT_WEAPON || ent->item->giType == IT_HOLDABLE))
	{
		return;
	}

	if (other->client->ps.pm_time > 0)
	{
		//cant pick up when out of control
		return;
	}

	if (g_AdvancedWeaponPickup->integer && g_SerenityJediEngineMode->integer && (other->s.number < MAX_CLIENTS ||
		G_ControlledByPlayer(other))) //player
	{
		//only if player is holing use button
		if (ent->item->giTag == WP_SABER || ent->item->giTag == WP_BRYAR_PISTOL)
		{
			// cant pick up these
		}
		else
		{
			if (!(other->client->usercmd.buttons & BUTTON_USE) || !IsWeponPickUpAllowed(other))
				// must be pressing the use button
			{
				return;
			}
			NPC_SetAnim(other, SETANIM_TORSO, BOTH_BUTTON2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}

	// NPCs can pick it up
	if (ent->spawnflags & ITMSF_ALLOWNPC && !other->s.number)
	{
		return;
	}

	// Players cannot pick it up
	if (ent->spawnflags & ITMSF_NOPLAYER && other->s.number)
	{
		return;
	}

	if (ent->noDamageTeam != TEAM_FREE && other->client->playerTeam != ent->noDamageTeam)
	{
		//only one team can pick it up
		return;
	}

	if (!G_CanPickUpWeapons(other))
	{
		return;
	}

	if (CheckItemCanBePickedUpByNPC(ent, other))
	{
		if (other->NPC && other->NPC->goalEntity && other->NPC->goalEntity == ent)
		{
			//they were running to pick me up, they did, so clear goal
			other->NPC->goalEntity = nullptr;
			other->NPC->squadState = SQUAD_STAND_AND_SHOOT;
			NPCInfo->tempBehavior = BS_DEFAULT;
			TIMER_Set(other, "flee", -1);
		}
		else
		{
			return;
		}
	}
	else if (!(ent->spawnflags & ITMSF_ALLOWNPC))
	{
		// NPCs cannot pick it up
		if (other->s.number != 0)
		{
			// Not the player?
			return;
		}
	}

	// the same pickup rules are used for client side and server side
	if (!BG_CanItemBeGrabbed(&ent->s, &other->client->ps))
	{
		return;
	}

	if (other->client)
	{
		if (other->client->ps.eFlags & EF_FORCE_GRIPPED || other->client->ps.eFlags & EF_FORCE_DRAINED || other->
			client->ps.eFlags & EF_FORCE_GRASPED)
		{
			//can't pick up anything while being gripped
			return;
		}
		if (PM_InKnockDown(&other->client->ps) && !PM_InGetUp(&other->client->ps))
		{
			//can't pick up while in a knockdown
			return;
		}
	}

	if (!ent->item)
	{
		//not an item!
		gi.Printf("Touch_Item: %s is not an item!\n", ent->classname);
		return;
	}

	if (ent->item->giType == IT_WEAPON && ent->item->giTag == WP_SABER)
	{
		//a saber
		if (ent->delay > level.time)
		{
			//just picked it up, don't pick up again right away
			return;
		}
	}

	if (other->s.number < MAX_CLIENTS && ent->spawnflags & ITMSF_USEPICKUP)
	{
		//only if player is holing use button
		if (!(other->client->usercmd.buttons & BUTTON_USE))
		{
			//not holding use?
			return;
		}
		NPC_SetAnim(other, SETANIM_TORSO, BOTH_BUTTON2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
	}

	qboolean b_had_weapon = qfalse;
	// call the item-specific pickup function
	switch (ent->item->giType)
	{
	case IT_WEAPON:
		if (other->NPC && other->s.weapon == WP_NONE)
		{
			//Make them duck and sit here for a few seconds
			const int pick_up_time = Q_irand(1000, 3000);
			TIMER_Set(other, "duck", pick_up_time);
			TIMER_Set(other, "roamTime", pick_up_time);
			TIMER_Set(other, "stick", pick_up_time);
			TIMER_Set(other, "verifyCP", pick_up_time);
			TIMER_Set(other, "attackDelay", 600);
		}
		if (other->client->ps.weapons[ent->item->giTag])
		{
			b_had_weapon = qtrue;
		}
		respawn = Pickup_Weapon(ent, other);
		break;
	case IT_AMMO:
		respawn = Pickup_Ammo(ent, other);
		break;
	case IT_ARMOR:
		respawn = Pickup_Armor(ent, other);
		break;
	case IT_HEALTH:
		respawn = Pickup_Health(ent, other);
		break;
	case IT_HOLDABLE:
		respawn = Pickup_Holdable(ent, other);
		break;
	case IT_BATTERY:
		respawn = Pickup_Battery(ent, other);
		break;
	case IT_HOLOCRON:
		respawn = Pickup_Holocron(ent, other);
		break;
	default:
		return;
	}

	if (!respawn)
	{
		return;
	}

	// play the normal pickup sound
	if (!other->s.number && g_timescale->value < 1.0f)
	{
		//SIGH... with timescale on, you lose events left and right
		extern void CG_ItemPickup(int itemNum, qboolean bHadItem);
		// but we're SP so we'll cheat
		cgi_S_StartSound(nullptr, other->s.number, CHAN_AUTO, cgi_S_RegisterSound(ent->item->pickup_sound));
		// show icon and name on status bar
		CG_ItemPickup(ent->s.modelindex, b_had_weapon);
	}
	else
	{
		if (b_had_weapon)
		{
			G_AddEvent(other, EV_ITEM_PICKUP, -ent->s.modelindex);
		}
		else
		{
			G_AddEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
		}
	}

	// fire item targets
	G_UseTargets(ent, other);

	if (ent->item->giType == IT_WEAPON && ent->item->giTag == WP_SABER)
	{
		//a saber that was picked up
		if (ent->count < 0)
		{
			//infinite supply
			ent->delay = level.time + 500;
			return;
		}
		ent->count--;
		if (ent->count > 0)
		{
			//still have more to pick up
			ent->delay = level.time + 500;
			return;
		}
	}

	{
		G_FreeEntity(ent);
	}
}

//======================================================================

/*
================
LaunchItem

Spawns an item and tosses it forward
================
*/

gentity_t* LaunchItem(gitem_t* item, const vec3_t origin, const vec3_t velocity, const char* target)
{
	gentity_t* dropped = G_Spawn();

	dropped->s.eType = ET_ITEM;
	dropped->s.modelindex = item - bg_itemlist; // store item number in modelindex
	dropped->s.modelindex2 = 1; // This is non-zero is it's a dropped item

	dropped->classname = G_NewString(item->classname); //copy it so it can be freed safely
	dropped->item = item;

	// try using the "correct" mins/maxs first
	VectorSet(dropped->mins, item->mins[0], item->mins[1], item->mins[2]);
	VectorSet(dropped->maxs, item->maxs[0], item->maxs[1], item->maxs[2]);

	if (!dropped->mins[0] && !dropped->mins[1] && !dropped->mins[2] &&
		(!dropped->maxs[0] && !dropped->maxs[1] && !dropped->maxs[2]))
	{
		VectorSet(dropped->maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
		VectorScale(dropped->maxs, -1, dropped->mins);
	}

	dropped->contents = CONTENTS_TRIGGER | CONTENTS_ITEM;
	//CONTENTS_TRIGGER;//not CONTENTS_BODY for dropped items, don't need to ID them

	if (target && target[0])
	{
		dropped->target = G_NewString(target);
	}
	else
	{
		// if not targeting something, auto-remove after 30 seconds
		// only if it's NOT a security or goodie key
		if (dropped->item->giTag != INV_SECURITY_KEY)
		{
			dropped->e_ThinkFunc = thinkF_G_FreeEntity;
			dropped->nextthink = level.time + 30000;
		}

		if (dropped->item->giType == IT_AMMO && dropped->item->giTag == AMMO_FORCE)
		{
			dropped->nextthink = -1;
			dropped->e_ThinkFunc = thinkF_NULL;
		}
	}

	dropped->e_TouchFunc = touchF_Touch_Item;

	if (item->giType == IT_WEAPON)
	{
		// give weapon items zero pitch, a random yaw, and rolled onto their sides...but would be bad to do this for a bowcaster
		if (item->giTag != WP_BOWCASTER
			&& item->giTag != WP_THERMAL
			&& item->giTag != WP_TRIP_MINE
			&& item->giTag != WP_DET_PACK)
		{
			VectorSet(dropped->s.angles, 0, Q_flrand(-1.0f, 1.0f) * 180, 90.0f);
			G_SetAngles(dropped, dropped->s.angles);
		}
	}

	G_SetOrigin(dropped, origin);
	dropped->s.pos.trType = TR_GRAVITY;
	dropped->s.pos.trTime = level.time;
	VectorCopy(velocity, dropped->s.pos.trDelta);

	dropped->s.eFlags |= EF_BOUNCE_HALF;

	dropped->flags = FL_DROPPED_ITEM;

	gi.linkentity(dropped);

	return dropped;
}

/*
================
Drop_Item

Spawns an item and tosses it forward
================
*/
gentity_t* Drop_Item(gentity_t* ent, gitem_t* item, const float angle, const qboolean copytarget)
{
	gentity_t* dropped;
	vec3_t velocity;
	vec3_t angles;

	VectorCopy(ent->s.apos.trBase, angles);
	angles[YAW] += angle;
	angles[PITCH] = 0; // always forward

	AngleVectors(angles, velocity, nullptr, nullptr);
	VectorScale(velocity, 150, velocity);
	velocity[2] += 200 + Q_flrand(-1.0f, 1.0f) * 50;

	if (copytarget)
	{
		dropped = LaunchItem(item, ent->s.pos.trBase, velocity, ent->opentarget);
	}
	else
	{
		dropped = LaunchItem(item, ent->s.pos.trBase, velocity, nullptr);
	}

	dropped->activator = ent; //so we know who we belonged to so they can pick it back up later
	dropped->s.time = level.time; //mark this time so we aren't picked up instantly by the guy who dropped us
	return dropped;
}

/*
================
Use_Item

Respawn the item
================
*/
void Use_Item(gentity_t* ent, gentity_t* other, gentity_t* activator)
{
	if (ent->svFlags & SVF_PLAYER_USABLE && other && !other->s.number)
	{
		//used directly by the player, pick me up
		if (ent->spawnflags & ITMSF_USEPICKUP)
		{
			//player has to be touching me and hit use to pick it up, so don't allow this
			if (!G_BoundsOverlap(ent->absmin, ent->absmax, other->absmin, other->absmax))
			{
				//not touching
				return;
			}
		}
		GEntity_TouchFunc(ent, other, nullptr);
	}
	else
	{
		//use me
		if (ent->spawnflags & 32) // invisible
		{
			// If it was invisible, first use makes it visible....
			ent->s.eFlags &= ~EF_NODRAW;
			ent->contents = CONTENTS_TRIGGER | CONTENTS_ITEM;

			ent->spawnflags &= ~32;
			return;
		}

		G_ActivateBehavior(ent, BSET_USE);
		RespawnItem(ent);
	}
}

//======================================================================

/*
================
FinishSpawningItem

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
extern int delayedShutDown;
extern cvar_t* g_saber;

void FinishSpawningItem(gentity_t* ent)
{
	trace_t tr;
	gitem_t* item;
	int item_num;

	item_num = 1;
	for (item = bg_itemlist + 1; item->classname; item++, item_num++)
	{
		if (strcmp(item->classname, ent->classname) == 0)
		{
			break;
		}
	}

	// Set bounding box for item
	VectorSet(ent->mins, item->mins[0], item->mins[1], item->mins[2]);
	VectorSet(ent->maxs, item->maxs[0], item->maxs[1], item->maxs[2]);

	if (!ent->mins[0] && !ent->mins[1] && !ent->mins[2] &&
		(!ent->maxs[0] && !ent->maxs[1] && !ent->maxs[2]))
	{
		VectorSet(ent->mins, -ITEM_RADIUS, -ITEM_RADIUS, -2); //to match the comments in the items.dat file!
		VectorSet(ent->maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
	}

	if (item->quantity && item->giType == IT_AMMO)
	{
		ent->count = item->quantity;
	}

	if (item->quantity && item->giType == IT_BATTERY)
	{
		ent->count = item->quantity;
	}

	ent->s.radius = 20;
	VectorSet(ent->s.modelScale, 1.0f, 1.0f, 1.0f);

	if (ent->item->giType == IT_WEAPON
		&& ent->item->giTag == WP_SABER
		&& ent->NPC_type
		&& ent->NPC_type[0])
	{
		saberInfo_t item_saber;
		if (Q_stricmp("player", ent->NPC_type) == 0
			&& g_saber->string
			&& g_saber->string[0]
			&& Q_stricmp("none", g_saber->string)
			&& Q_stricmp("NULL", g_saber->string))
		{
			//player's saber
			WP_SaberParseParms(g_saber->string, &item_saber);
		}
		else
		{
			//specific saber
			WP_SaberParseParms(ent->NPC_type, &item_saber);
		}
		//NOTE:  should I keep this string around for any reason?  Will I ever need it later?
		//ent->??? = G_NewString( itemSaber.model );
		gi.G2API_InitGhoul2Model(ent->ghoul2, item_saber.model, G_ModelIndex(item_saber.model), NULL_HANDLE, NULL_HANDLE,
			0, 0);
		WP_SaberFreeStrings(item_saber);
	}
	else
	{
		gi.G2API_InitGhoul2Model(ent->ghoul2, ent->item->world_model, G_ModelIndex(ent->item->world_model), NULL_HANDLE,
			NULL_HANDLE, 0, 0);
	}

	// Set crystal ammo amount based on skill level
	/*	if ((itemNum == ITM_AMMO_CRYSTAL_BORG) ||
			(itemNum == ITM_AMMO_CRYSTAL_DN) ||
			(itemNum == ITM_AMMO_CRYSTAL_FORGE) ||
			(itemNum == ITM_AMMO_CRYSTAL_SCAVENGER) ||
			(itemNum == ITM_AMMO_CRYSTAL_STASIS))
		{
			CrystalAmmoSettings(ent);
		}
	*/
	ent->s.eType = ET_ITEM;
	ent->s.modelindex = ent->item - bg_itemlist; // store item number in modelindex
	ent->s.modelindex2 = 0; // zero indicates this isn't a dropped item

	ent->contents = CONTENTS_TRIGGER | CONTENTS_ITEM; //CONTENTS_BODY;//CONTENTS_TRIGGER|
	ent->e_TouchFunc = touchF_Touch_Item;
	// useing an item causes it to respawn
	ent->e_UseFunc = useF_Use_Item;
	ent->svFlags |= SVF_PLAYER_USABLE; //so player can pick it up

	// Hang in air?
	ent->s.origin[2] += 1; //just to get it off the damn ground because coplanar = insolid
	if (ent->spawnflags & ITMSF_SUSPEND
		|| ent->flags & FL_DROPPED_ITEM)
	{
		// suspended
		G_SetOrigin(ent, ent->s.origin);
	}
	else
	{
		vec3_t dest;
		// drop to floor
		VectorSet(dest, ent->s.origin[0], ent->s.origin[1], MIN_WORLD_COORD);
		gi.trace(&tr, ent->s.origin, ent->mins, ent->maxs, dest, ent->s.number, MASK_SOLID | CONTENTS_PLAYERCLIP,static_cast<EG2_Collision>(0), 0);
		if (tr.startsolid)
		{ //fixed Items spawning in solid and added required includes and static cast above

			if (g_entities[tr.entityNum].inuse)
			{
				gi.Printf(S_COLOR_RED"FinishSpawningItem: removing %s startsolid at %s (in a %s)\n", ent->classname, vtos(ent->s.origin), g_entities[tr.entityNum].classname);
			}
			else
			{
				gi.Printf(S_COLOR_RED"FinishSpawningItem: removing %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
			}
			assert(0 && "item starting in solid");
			if (!g_entities[ENTITYNUM_WORLD].s.radius) {	//not a region
				delayedShutDown = level.time + 100;
			}
			G_FreeEntity(ent);
			return;
		}

		// allow to ride movers
		ent->s.groundEntityNum = tr.entityNum;

		G_SetOrigin(ent, tr.endpos);
	}

	/* ? don't need this
		// team slaves and targeted items aren't present at start
		if ( ( ent->flags & FL_TEAMSLAVE ) || ent->targetname ) {
			ent->s.eFlags |= EF_NODRAW;
			ent->contents = 0;
			return;
		}
	*/
	if (ent->spawnflags & ITMSF_INVISIBLE) // invisible
	{
		ent->s.eFlags |= EF_NODRAW;
		ent->contents = 0;
	}

	if (ent->spawnflags & ITMSF_NOTSOLID) // not solid
	{
		ent->contents = 0;
	}

	if (ent->spawnflags & ITMSF_STATIONARY)
	{
		//can't be pushed around
		ent->flags |= FL_NO_KNOCKBACK;
	}

	if (ent->flags & FL_DROPPED_ITEM)
	{
		//go away after 30 seconds
		ent->e_ThinkFunc = thinkF_G_FreeEntity;
		ent->nextthink = level.time + 30000;
	}

	gi.linkentity(ent);
}

char itemRegistered[MAX_ITEMS + 1];

/*
==============
ClearRegisteredItems
==============
*/

extern void Player_CacheFromPrevLevel(); //g_client.cpp
void ClearRegisteredItems()
{
	for (int i = 0; i < bg_numItems; i++)
	{
		itemRegistered[i] = '0';
	}
	itemRegistered[bg_numItems] = 0;

	//these are given in g_client, ClientSpawn(), but MUST be registered HERE, BEFORE cgame starts.
	RegisterItem(FindItemForWeapon(WP_MELEE)); //has no item
	RegisterItem(FindItemForWeapon(WP_BRYAR_PISTOL));
	//these are given in g_client, ClientSpawn(), but MUST be registered HERE, BEFORE cgame starts.
	RegisterItem(FindItemForWeapon(WP_STUN_BATON));

	RegisterItem(FindItemForInventory(INV_ELECTROBINOCULARS));
	RegisterItem(FindItemForInventory(INV_BACTA_CANISTER));
	RegisterItem(FindItemForInventory(INV_SEEKER));
	RegisterItem(FindItemForInventory(INV_SENTRY));
	RegisterItem(FindItemForInventory(INV_CLOAK));
	RegisterItem(FindItemForInventory(INV_BARRIER));
	RegisterItem(FindItemForInventory(INV_GRAPPLEHOOK));
	// saber or baton is cached in SP_info_player_deathmatch now.
	Player_CacheFromPrevLevel(); //reads from transition carry-over;
}

/*
===============
RegisterItem

The item will be added to the precache list
===============
*/
void RegisterItem(const gitem_t* item)
{
	if (!item)
	{
		G_Error("RegisterItem: NULL");
	}
	itemRegistered[item - bg_itemlist] = '1';
	gi.SetConfigstring(CS_ITEMS, itemRegistered); //Write the needed items to a config string
}

/*
============
item_spawn_use

 if an item is given a targetname, it will be spawned in when used
============
*/
void item_spawn_use(gentity_t* self, gentity_t* other, gentity_t* activator)
//-----------------------------------------------------------------------------
{
	self->nextthink = level.time + 50;
	self->e_ThinkFunc = thinkF_FinishSpawningItem;
	// I could be fancy and add a count or something like that to be able to spawn the item numerous times...
	self->e_UseFunc = useF_NULL;
}

/*
============
G_SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void G_SpawnItem(gentity_t* ent, gitem_t* item)
{
	G_SpawnFloat("random", "0", &ent->random);
	G_SpawnFloat("wait", "0", &ent->wait);

	RegisterItem(item);
	ent->item = item;

	// targetname indicates they want to spawn it later
	if (ent->targetname)
	{
		ent->e_UseFunc = useF_item_spawn_use;
	}
	else
	{
		// some movers spawn on the second frame, so delay item
		// spawns until the third frame so they can ride trains
		ent->nextthink = level.time + START_TIME_MOVERS_SPAWNED + 50;
		ent->e_ThinkFunc = thinkF_FinishSpawningItem;
	}

	ent->physicsBounce = 0.50; // items are bouncy

	// Set a default infoString text color
	// NOTE: if we want to do cool cross-hair colors for items, we can just modify this, but for now, don't do it
	VectorSet(ent->startRGBA, 1.0f, 1.0f, 1.0f);

	if (ent->team && ent->team[0])
	{
		ent->noDamageTeam = static_cast<team_t>(GetIDForString(TeamTable, ent->team));
		if (ent->noDamageTeam == TEAM_FREE)
		{
			G_Error("team name %s not recognized\n", ent->team);
		}
	}

	if (ent->item
		&& ent->item->giType == IT_WEAPON
		&& ent->item->giTag == WP_SABER)
	{
		//weapon_saber item
		if (!ent->count)
		{
			//can only pick up once
			ent->count = 1;
		}
	}
	ent->team = nullptr;
}

/*
================
G_BounceItem

================
*/
static void G_BounceItem(gentity_t* ent, trace_t* trace)
{
	vec3_t velocity;
	qboolean droppedSaber = qtrue;

	if (ent->item
		&& ent->item->giType == IT_WEAPON
		&& ent->item->giTag == WP_SABER
		&& ent->flags & FL_DROPPED_ITEM)
	{
		droppedSaber = qtrue;
	}

	// reflect the velocity on the trace plane
	const int hitTime = level.previousTime + (level.time - level.previousTime) * trace->fraction;
	EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity);
	const float dot = DotProduct(velocity, trace->plane.normal);
	VectorMA(velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta);

	// cut the velocity to keep from bouncing forever
	VectorScale(ent->s.pos.trDelta, ent->physicsBounce, ent->s.pos.trDelta);

	if (droppedSaber)
	{
		//a dropped saber item
		//FIXME: use NPC_type (as saberType) to get proper bounce sound?
		WP_SaberFallSound(nullptr, ent);
	}

	// check for stop
	if (trace->plane.normal[2] > 0 && ent->s.pos.trDelta[2] < 40)
	{
		//stop
		G_SetOrigin(ent, trace->endpos);
		ent->s.groundEntityNum = trace->entityNum;
		if (droppedSaber)
		{
			//a dropped saber item
			//stop rotation
			VectorClear(ent->s.apos.trDelta);
			ent->currentAngles[PITCH] = SABER_PITCH_HACK;
			ent->currentAngles[ROLL] = 0;
			if (ent->NPC_type
				&& ent->NPC_type[0])
			{
				//we have a valid saber for this
				saberInfo_t saber;
				if (WP_SaberParseParms(ent->NPC_type, &saber))
				{
					if (saber.saberFlags & SFL_BOLT_TO_WRIST)
					{
						ent->currentAngles[PITCH] = 0;
					}
				}
			}
			pitch_roll_for_slope(ent, trace->plane.normal, ent->currentAngles, qtrue);
			G_SetAngles(ent, ent->currentAngles);
		}
		return;
	}
	//bounce
	if (droppedSaber)
	{
		//a dropped saber item
		//change rotation
		VectorCopy(ent->currentAngles, ent->s.apos.trBase);
		ent->s.apos.trType = TR_LINEAR;
		ent->s.apos.trTime = level.time;
		VectorSet(ent->s.apos.trDelta, Q_irand(-300, 300), Q_irand(-300, 300), Q_irand(-300, 300));
	}

	VectorAdd(ent->currentOrigin, trace->plane.normal, ent->currentOrigin);
	VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
	ent->s.pos.trTime = level.time;
}

/*
================
G_RunItem

================
*/

extern cvar_t* g_remove_unused_weapons;

static void G_RemoveWeaponEffect(gentity_t* ent)
{
	if (ent->TimeOfWeaponDrop > level.time)
	{
		return;
	}
	G_PlayEffect("weapon/remove", ent->currentOrigin);

	if (level.time >= 1000)
	{
		G_FreeEntity(ent);
	}
}

extern cvar_t* g_WeaponRemovalTime;
extern cvar_t* com_outcast;
void G_RunItem(gentity_t* ent)
{
	vec3_t origin;
	trace_t tr;
	int mask;

	// if groundentity has been set to -1, it may have been pushed off an edge
	if (ent->s.groundEntityNum == ENTITYNUM_NONE)
	{
		if (ent->s.pos.trType != TR_GRAVITY)
		{
			ent->s.pos.trType = TR_GRAVITY;
			ent->s.pos.trTime = level.time;
		}
	}

	if (ent->s.pos.trType == TR_STATIONARY)
	{
		// check think function
		G_RunThink(ent);

		if (!g_gravity->value)
		{
			ent->s.pos.trType = TR_GRAVITY;
			ent->s.pos.trTime = level.time;
			ent->s.pos.trDelta[0] += Q_flrand(-1.0f, 1.0f) * 40.0f; // I dunno, just do this??
			ent->s.pos.trDelta[1] += Q_flrand(-1.0f, 1.0f) * 40.0f;
			ent->s.pos.trDelta[2] += Q_flrand(0.0f, 1.0f) * 20.0f;
		}
		else if (ent->flags & FL_DROPPED_ITEM
			&& ent->item
			&& ent->item->giType == IT_WEAPON
			&& ent->item->giTag == WP_SABER)
		{
			//a dropped saber item, check below, just in case
			int ignore = ENTITYNUM_NONE;
			if (ent->clipmask)
			{
				mask = ent->clipmask;
			}
			else
			{
				mask = MASK_SOLID | CONTENTS_PLAYERCLIP; //shouldn't be able to get anywhere player can't
			}
			if (ent->owner)
			{
				ignore = ent->owner->s.number;
			}
			else if (ent->activator)
			{
				ignore = ent->activator->s.number;
			}
			VectorSet(origin, ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2] - 1);
			gi.trace(&tr, ent->currentOrigin, ent->mins, ent->maxs, origin, ignore, mask, static_cast<EG2_Collision>(0),
				0);
			if (!tr.allsolid
				&& !tr.startsolid
				&& tr.fraction > 0.001f)
			{
				//wha?  fall....
				ent->s.pos.trType = TR_GRAVITY;
				ent->s.pos.trTime = level.time;
			}
		}
		//else if (ent->flags & FL_DROPPED_ITEM /*&& g_remove_unused_weapons->integer*/ && g_WeaponRemovalTime->integer && com_outcast->integer == 3
		//	&& ent->item
		//	&& ent->item->giType == IT_WEAPON
		//	&& ent->item->giTag != WP_SABER)
		//{
		//	//a dropped weapon item,
		//	G_RemoveWeaponEffect(ent);
		//}
		return;
	}

	// get current position
	EvaluateTrajectory(&ent->s.pos, level.time, origin);
	if (ent->s.apos.trType != TR_STATIONARY)
	{
		EvaluateTrajectory(&ent->s.apos, level.time, ent->currentAngles);
		G_SetAngles(ent, ent->currentAngles);
	}

	// trace a line from the previous position to the current position
	if (ent->clipmask)
	{
		mask = ent->clipmask;
	}
	else
	{
		mask = MASK_SOLID | CONTENTS_PLAYERCLIP; //shouldn't be able to get anywhere player can't
	}

	int ignore = ENTITYNUM_NONE;
	if (ent->owner)
	{
		ignore = ent->owner->s.number;
	}
	else if (ent->activator)
	{
		ignore = ent->activator->s.number;
	}
	gi.trace(&tr, ent->currentOrigin, ent->mins, ent->maxs, origin, ignore, mask, static_cast<EG2_Collision>(0), 0);

	VectorCopy(tr.endpos, ent->currentOrigin);

	if (tr.startsolid)
	{
		tr.fraction = 0;
	}

	gi.linkentity(ent); // FIXME: avoid this for stationary?

	// check think function
	G_RunThink(ent);

	if (tr.fraction == 1)
	{
		if (g_gravity->value <= 0)
		{
			if (ent->s.apos.trType != TR_LINEAR)
			{
				VectorCopy(ent->currentAngles, ent->s.apos.trBase);
				ent->s.apos.trType = TR_LINEAR;
				ent->s.apos.trDelta[1] = Q_flrand(-300, 300);
				ent->s.apos.trDelta[0] = Q_flrand(-10, 10);
				ent->s.apos.trDelta[2] = Q_flrand(-10, 10);
				ent->s.apos.trTime = level.time;
			}
		}
		//friction in zero-G
		if (!g_gravity->value)
		{
			constexpr float friction = 0.975f;
			VectorScale(ent->s.pos.trDelta, friction, ent->s.pos.trDelta);
			VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
			ent->s.pos.trTime = level.time;
		}
		return;
	}

	// if it is in a nodrop volume, remove it
	const int contents = gi.pointcontents(ent->currentOrigin, -1);
	if (contents & CONTENTS_NODROP)
	{
		G_FreeEntity(ent);
		return;
	}

	if (!tr.startsolid)
	{
		G_BounceItem(ent, &tr);
	}
}

/*
================
ItemUse_Bacta

================
*/
void ItemUse_Bacta(gentity_t* ent)
{
	if (!ent || !ent->client)
	{
		return;
	}

	if (ent->health >= ent->client->ps.stats[STAT_MAX_HEALTH] || !ent->client->ps.inventory[INV_BACTA_CANISTER])
	{
		return;
	}

	ent->health += MAX_BACTA_HEAL_AMOUNT;

	if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
	}

	ent->client->ps.inventory[INV_BACTA_CANISTER]--;
	/* Don't play heal sound
	if (!Q_stricmp("kyle", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("kyle2", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("Kyle_boss", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("KyleJK2", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("KyleJK2noforce", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("Kyle_JKA", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("Kyle_MOD", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("KylePlayer", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_lvl1", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_lvl2", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_lvl3", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_lvl4", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_lvl5", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_lvl6", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_alt", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_sj", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_jm", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_old", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2_coat", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2ig", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df2lowpoly", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_mots", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_emp", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_df1", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("md_kyle_officer", ent->client->clientInfo.customBasicSoundDir)
		|| !Q_stricmp("jedi_kyle", ent->client->clientInfo.customBasicSoundDir))
	{
		G_SoundOnEnt(ent, CHAN_VOICE, va("sound/weapons/force/heal%d.mp3", Q_irand(1, 4)));
	}
	else
	{
		G_SoundOnEnt(ent, CHAN_VOICE, va("sound/weapons/force/heal%d_%c.mp3", Q_irand(1, 4), g_sex->string[0]));
	}*/
	NPC_SetAnim(ent, SETANIM_TORSO, BOTH_FORCEHEAL_QUICK, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);

	G_SoundOnEnt(ent, CHAN_ITEM, "sound/items/use_bacta2.wav");
}

constexpr auto CLOAK_TOGGLE_TIME = 1000;
extern void self_check_cloak(gentity_t* self);
extern void player_decloak(gentity_t* self);

void ItemUse_UseCloak(gentity_t* ent)
{
	if (ent->client->cloakToggleTime >= level.time)
	{
		return;
	}

	if (!ent->client->ps.inventory[INV_CLOAK])
	{
		return;
	}

	if (ent->health <= 0 ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 || ent->client->ps.pm_type == PM_DEAD)
	{
		//can't use it when dead under any circumstances.
		return;
	}

	if (ent->client->ps.cloakFuel <= 15)
	{
		//too low on fuel to start it up
		return;
	}

	if (ent->client->ps.powerups[PW_CLOAKED])
	{
		//decloak
		player_decloak(ent);
	}
	else
	{
		//cloak
		if (ent->client->ps.cloakFuel > 15)
		{
			//using cloak, drain force
			self_check_cloak(ent);
			NPC_SetAnim(ent, SETANIM_TORSO, BOTH_FORCE_PROTECT_FAST, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
		}
	}

	ent->client->cloakToggleTime = level.time + CLOAK_TOGGLE_TIME;
}

constexpr auto JETPACK_TOGGLE_TIME = 1000;

void Jetpack_Off(const gentity_t* ent)
{
	//create effects?
	assert(ent && ent->client);

	if (!ent || !ent->client)
		return;

	if (!ent->client->jetPackOn)
	{
		//aready off
		return;
	}

	ent->client->jetPackOn = qfalse;
}

static void Jetpack_On(const gentity_t* ent)
{
	//create effects?
	assert(ent && ent->client);

	if (ent->client->jetPackOn)
	{
		//aready on
		return;
	}

	ent->client->jetPackOn = qtrue;
}

void ItemUse_Jetpack(const gentity_t* ent)
{
	assert(ent && ent->client);

	if (ent->client->jetPackToggleTime >= level.time)
	{
		return;
	}

	if (ent->health <= 0 ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 || ent->client->ps.pm_type == PM_DEAD)
	{
		//can't use it when dead under any circumstances.
		return;
	}

	if (!ent->client->jetPackOn && ent->client->ps.jetpackFuel < 15)
	{
		//too low on fuel to start it up
		return;
	}

	if (ent->client->jetPackOn && ent->client->ps.jetpackFuel < 15)
	{
		//too low on fuel to start it up
		Jetpack_Off(ent);
	}

	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}
	else
	{
		if (ent->client->ps.jetpackFuel > 15)
		{
			Jetpack_On(ent);
		}
	}

	ent->client->jetPackToggleTime = level.time + JETPACK_TOGGLE_TIME;
}

/////////////////////////////////////////////////////////////////////////////////////////
constexpr auto TURN_ON = 0x00000000;
constexpr auto TURN_OFF = 0x00000100;

constexpr auto BARRIER_TOGGLE_TIME = 1000;
constexpr auto SHIELD_HEALTH_DEC = 10;

static qhandle_t shieldLoopSound = 0;
static qhandle_t shieldActivateSound = 0;
static qhandle_t shieldDeactivateSound = 0;
static qhandle_t DekashieldActivateSound = 0;
static qhandle_t DekashieldDeactivateSound = 0;

static qboolean HeHasGun(const gentity_t* ent)
{
	switch (ent->s.weapon)
	{
	case WP_MELEE:
	case WP_BLASTER_PISTOL:
	case WP_BLASTER:
	case WP_DISRUPTOR:
	case WP_BOWCASTER:
	case WP_REPEATER:
	case WP_DEMP2:
	case WP_FLECHETTE:
	case WP_ROCKET_LAUNCHER:
	case WP_THERMAL:
	case WP_TRIP_MINE:
	case WP_DET_PACK:
	case WP_CONCUSSION:
	case WP_STUN_BATON:
	case WP_BRYAR_PISTOL:
	case WP_TUSKEN_RIFLE:
	case WP_BATTLEDROID:
	case WP_THEFIRSTORDER:
	case WP_CLONECARBINE:
	case WP_REBELBLASTER:
	case WP_CLONERIFLE:
	case WP_CLONECOMMANDO:
	case WP_Z6_ROTARY_CANNON:
	case WP_REBELRIFLE:
	case WP_REY:
	case WP_JANGO:
	case WP_BOBA:
	case WP_CLONEPISTOL:
	case WP_SBD_BLASTER:
	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
	case WP_DROIDEKA:
		return qtrue;
	default:;
	}
	return qfalse;
}

void RemoveBarrier(gentity_t* ent)
{
	static qboolean registered = qfalse;

	if (!registered)
	{
		shieldDeactivateSound = G_SoundIndex("sound/barrier/barrier_off.mp3");
		DekashieldDeactivateSound = G_SoundIndex("sound/chars/droideka/shieldoff.mp3");
		registered = qtrue;
	}

	if (droideka_npc(ent))
	{
		return;
	}

	if (ent && ent->client)
	{
		if (ent->client->ps.powerups[PW_GALAK_SHIELD])
		{
			ent->s.loopSound = 0;
			ent->client->ps.powerups[PW_GALAK_SHIELD] = 0; //temp, for effect
			ent->flags &= ~FL_SHIELDED; //no more reflections
			if (ent->client->NPC_class == CLASS_DROIDEKA && ent->s.weapon == WP_DROIDEKA)
			{
				G_AddEvent(ent, EV_GENERAL_SOUND, DekashieldDeactivateSound);
				gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "force_shield", TURN_OFF);
			}
			else
			{
				G_AddEvent(ent, EV_GENERAL_SOUND, shieldDeactivateSound);
				gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "torso_shield_off", TURN_OFF);

				NPC_SetAnim(ent, SETANIM_TORSO, BOTH_FORCE_DRAIN_RELEASE, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
	}
}

void TurnBarrierOff(gentity_t* ent)
{
	if (droideka_npc(ent))
	{
		return;
	}

	ent->client->ps.powerups[PW_GALAK_SHIELD] = 0; //temp, for effect
	ent->flags &= ~FL_SHIELDED; //no more reflections
	if (ent->client->NPC_class == CLASS_DROIDEKA && ent->s.weapon == WP_DROIDEKA)
	{
		gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "force_shield", TURN_OFF);
	}
	else
	{
		gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "torso_shield_off", TURN_OFF);
	}
}

void TurnBarrierON(gentity_t* ent)
{
	static qboolean registered = qfalse;

	if (!registered)
	{
		shieldLoopSound = G_SoundIndex("sound/barrier/barrier_loop.wav");
		registered = qtrue;
	}
	if (droideka_npc(ent))
	{
		return;
	}

	ent->client->ps.powerups[PW_GALAK_SHIELD] = Q3_INFINITE;
	ent->flags |= FL_SHIELDED; //reflect normal shots
	gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "force_shield", TURN_ON);
	ent->s.loopSound = shieldLoopSound;
}

void barrier_update(gentity_t* ent);
static void PlaceBarrier(gentity_t* ent)
{
	static qboolean registered = qfalse;

	if (!registered)
	{
		shieldLoopSound = G_SoundIndex("sound/barrier/barrier_loop.wav");
		shieldActivateSound = G_SoundIndex("sound/barrier/barrier_on.mp3");
		DekashieldActivateSound = G_SoundIndex("sound/chars/droideka/shieldon.mp3");
		registered = qtrue;
	}
	if (droideka_npc(ent))
	{
		return;
	}

	if (ent && ent->client)
	{
		if (!ent->client->ps.powerups[PW_GALAK_SHIELD])
		{
			ent->client->ps.powerups[PW_GALAK_SHIELD] = Q3_INFINITE;
			ent->client->ps.BarrierFuel -= 15;
			ent->flags |= FL_SHIELDED; //reflect normal shots
			ent->fx_time = level.time;
			if (ent->client->NPC_class == CLASS_DROIDEKA && ent->s.weapon == WP_DROIDEKA)
			{
				G_AddEvent(ent, EV_GENERAL_SOUND, DekashieldActivateSound);
				gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "force_shield", TURN_ON);
				barrier_update(ent);
			}
			else
			{
				G_AddEvent(ent, EV_GENERAL_SOUND, shieldActivateSound);
				gi.G2API_SetSurfaceOnOff(&ent->ghoul2[ent->playerModel], "torso_shield_off", TURN_ON);

				NPC_SetAnim(ent, SETANIM_TORSO, BOTH_ATTACK11, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
			}
		}
		ent->s.loopSound = shieldLoopSound;
	}
}

void Ent_CheckBarrierIsAllowed(gentity_t* ent)
{
	if (ent && ent->client)
	{
		if (ent->health <= 0 ||
			!HeHasGun(ent) ||
			ent->client->ps.eFlags & EF_FORCE_GRIPPED ||
			ent->client->ps.eFlags & EF_FORCE_DRAINED ||
			ent->client->ps.eFlags & EF_FORCE_GRASPED ||
			ent->painDebounceTime > level.time)
		{
			RemoveBarrier(ent);
		}
		else if (ent->health > 0
			&& HeHasGun(ent)
			&& !(ent->client->ps.eFlags & EF_FORCE_GRIPPED)
			&& !(ent->client->ps.eFlags & EF_FORCE_DRAINED)
			&& !(ent->client->ps.eFlags & EF_FORCE_GRASPED)
			&& ent->painDebounceTime < level.time)
		{
			PlaceBarrier(ent);
		}
	}
}

void Ent_CheckBarrierIsAllowed_WithSaber(gentity_t* ent)
{
	if (ent && ent->client)
	{
		if (ent->client->ps.SaberActive())
		{
			RemoveBarrier(ent);
		}
		if (ent->health <= 0 ||
			ent->client->ps.eFlags & EF_FORCE_GRIPPED ||
			ent->client->ps.eFlags & EF_FORCE_DRAINED ||
			ent->client->ps.eFlags & EF_FORCE_GRASPED ||
			ent->painDebounceTime > level.time)
		{
			RemoveBarrier(ent);
		}
		else if (ent->health > 0
			&& !(ent->client->ps.eFlags & EF_FORCE_GRIPPED)
			&& !(ent->client->ps.eFlags & EF_FORCE_DRAINED)
			&& !(ent->client->ps.eFlags & EF_FORCE_GRASPED)
			&& ent->painDebounceTime < level.time)
		{
			PlaceBarrier(ent);
		}
	}
}

void ItemUse_Barrier(gentity_t* ent)
{
	if (!HeHasGun(ent))
	{
		return;
	}

	if (ent->client->BarrierToggleTime >= level.time)
	{
		return;
	}

	if (!ent->client->ps.inventory[INV_BARRIER])
	{
		return;
	}

	if (ent->health <= 0 || in_camera ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 || ent->client->ps.pm_type == PM_DEAD)
	{
		//can't use it when dead under any circumstances.
		return;
	}

	if (ent->client->ps.BarrierFuel <= 15)
	{
		//too low on fuel to start it up
		return;
	}

	if (ent->client->ps.powerups[PW_GALAK_SHIELD] || ent->flags & FL_SHIELDED)
	{
		//decloak
		RemoveBarrier(ent);
	}
	else
	{
		//cloak
		if (ent->client->ps.BarrierFuel > 15)
		{
			Ent_CheckBarrierIsAllowed(ent);
		}
	}

	ent->client->BarrierToggleTime = level.time + BARRIER_TOGGLE_TIME;
}

void ItemUse_Barrier_with_saber(gentity_t* ent)
{
	if (ent->client->BarrierToggleTime >= level.time)
	{
		return;
	}

	if (!ent->client->ps.inventory[INV_BARRIER])
	{
		return;
	}

	if (ent->health <= 0 || in_camera ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 || ent->client->ps.pm_type == PM_DEAD)
	{
		//can't use it when dead under any circumstances.
		return;
	}

	if (ent->client->ps.BarrierFuel <= 15)
	{
		//too low on fuel to start it up
		return;
	}
	if (ent->client->ps.BarrierFuel > 15)
	{
		Ent_CheckBarrierIsAllowed_WithSaber(ent);
	}

	ent->client->BarrierToggleTime = level.time + BARRIER_TOGGLE_TIME;
}

void ItemUse_Grapple(gentity_t* ent)
{
	constexpr int anim = BOTH_GRAPPLE_FIRE;

	if (!ent->client->ps.inventory[INV_GRAPPLEHOOK])
	{
		return;
	}

	if (ent->client->ps.weapon == WP_MELEE && ent->client->ps.inventory[INV_GRAPPLEHOOK])
	{
		if (!(ent->client->usercmd.buttons & BUTTON_WALKING) &&
			!PM_RunningAnim(ent->client->ps.legsAnim) &&
			!PM_WalkingAnim(ent->client->ps.legsAnim) &&
			ent->client->ps.pm_type != PM_DEAD &&
			!ent->client->hookhasbeenfired)
		{
			if (ent->client && ent->client->hookDebounceTime > level.time)
			{
				if (ent->client->hook)
				{
					Weapon_HookFree(ent->client->hook);
				}
			}
			else
			{
				if (ent->client->moveType == MT_FLYSWIM || ent->s.groundEntityNum == ENTITYNUM_NONE || JET_Flying(ent)
					|| PM_CrouchAnim(ent->client->ps.legsAnim))
				{
					Boba_FireWristMissile(ent, BOBA_MISSILE_VIBROBLADE);
				}
				else
				{
					if (!(ent->client->ps.communicatingflags & 1 << DASHING))
					{
						Weapon_MandoGrappleHook_Fire(ent);
						ent->client->hookhasbeenfired = qtrue;

						if (ent->s.groundEntityNum != ENTITYNUM_NONE)
						{
							//holding attack
							NPC_SetAnim(ent, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
							//gi.Printf(S_COLOR_YELLOW"item use grapple attack\n");
						}

						G_SoundOnEnt(ent, CHAN_ITEM, "sound/weapons/grapple/hookfire.wav");
						ent->client->hookDebounceTime = level.time + 0;
					}
				}
			}
		}
		else if (ent->client->hook && (ent->client->fireHeld == qfalse ||
			ent->client->usercmd.buttons & BUTTON_WALKING ||
			PM_RunningAnim(ent->client->ps.legsAnim) ||
			PM_WalkingAnim(ent->client->ps.legsAnim) ||
			ent->client->ps.pm_type == PM_DEAD))
		{
			Weapon_HookFree(ent->client->hook);
		}
	}
	else
	{
		if (ent->client->hook && (ent->client->fireHeld == qfalse ||
			ent->client->usercmd.buttons & BUTTON_WALKING ||
			PM_RunningAnim(ent->client->ps.legsAnim) ||
			PM_WalkingAnim(ent->client->ps.legsAnim) ||
			ent->client->ps.pm_type == PM_DEAD))
		{
			Weapon_HookFree(ent->client->hook);
		}

		if (!(ent->client->ps.eFlags & EF_FIRING))
		{
			ent->client->fireHeld = qfalse;
		}
	}
}

extern void G_KnockOver(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength, qboolean breakSaberLock);
static void barrier_push_ent(gentity_t* ent, gentity_t* pushed, vec3_t smack_dir)
{
	G_Damage(pushed, ent, ent, smack_dir, ent->currentOrigin, (g_spskill->integer + 1) * Q_irand(5, 10), DAMAGE_EXTRA_KNOCKBACK, MOD_ELECTROCUTE);
	//G_Throw(pushed, smack_dir, 10);
	G_KnockOver(pushed, ent, smack_dir, 25, qtrue);

	// Make Em Electric
	//------------------
	pushed->s.powerups |= 1 << PW_SHOCKED;
	if (pushed->client)
	{
		pushed->client->ps.powerups[PW_SHOCKED] = level.time + 3000;
	}
}

constexpr auto DROIDEKA_SHIELD_SIZE = 75;
static void barrier_shield_push_radius_ents(gentity_t* ent)
{
	gentity_t* radius_ents[128];
	constexpr float radius = DROIDEKA_SHIELD_SIZE;
	vec3_t mins{}, maxs{};

	for (int i = 0; i < 3; i++)
	{
		mins[i] = ent->currentOrigin[i] - radius;
		maxs[i] = ent->currentOrigin[i] + radius;
	}

	const int num_ents = gi.EntitiesInBox(mins, maxs, radius_ents, 128);
	for (int ent_index = 0; ent_index < num_ents; ent_index++)
	{
		vec3_t smack_dir;
		// Only Clients
		//--------------
		if (!radius_ents[ent_index] || !radius_ents[ent_index]->client)
		{
			continue;
		}

		// Don't Push Away Other Assassin Droids
		//---------------------------------------
		if (radius_ents[ent_index]->client->NPC_class == ent->client->NPC_class)
		{
			continue;
		}

		// Should Have Already Pushed The Enemy If He Touched Us
		//-------------------------------------------------------
		if (ent->enemy && ent->enemy->touchedByPlayer == ent->enemy && radius_ents[ent_index] == ent->enemy)
		{
			continue;
		}

		// Do The Vector Distance Test
		//-----------------------------
		VectorSubtract(radius_ents[ent_index]->currentOrigin, ent->currentOrigin, smack_dir);
		const float smack_dist = VectorNormalize(smack_dir);
		if (smack_dist < radius)
		{
			barrier_push_ent(ent, radius_ents[ent_index], smack_dir);
		}
	}
}

void barrier_update(gentity_t* ent)
{
	// Shields Go When You Die
	//-------------------------
	if (ent->health <= 0)
	{
		if (ent->client->ps.powerups[PW_GALAK_SHIELD])
		{
			//decloak
			RemoveBarrier(ent);
		}
		return;
	}

	if (ent->client->ps.powerups[PW_GALAK_SHIELD])
	{
		barrier_shield_push_radius_ents(ent);
	}
}