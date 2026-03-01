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

#include "g_local.h"
#include "g_functions.h"
#include "bg_public.h"
#include "../cgame/cg_local.h"
#include "g_public.h"
#include <qcommon\q_platform.h>
#include <qcommon\q_shared.h>
#include "anims.h"
#include <string.h>
#include <qcommon\q_math.h>
#include "g_shared.h"
#include "ghoul2_shared.h"
#include "weapons.h"
#include "surfaceflags.h"
#include <qcommon\q_color.h>

extern cvar_t* g_spskill;

//
// Helper functions
//
//------------------------------------------------------------
static void SetMiscModelModels(const char* modelNameString, gentity_t* ent, const qboolean damage_model)
{
	//Main model
	ent->s.modelindex = G_ModelIndex(modelNameString);

	if (damage_model)
	{
		char chunkModel[MAX_QPATH];
		char damageModel[MAX_QPATH];
		const int len = strlen(modelNameString) - 4; // extract the extension

		//Dead/damaged model
		strncpy(damageModel, modelNameString, len);
		damageModel[len] = 0;
		strncpy(chunkModel, damageModel, sizeof chunkModel);
		strcat(damageModel, "_d1.md3");
		ent->s.modelindex2 = G_ModelIndex(damageModel);

		ent->spawnflags |= 4; // deadsolid

		//Chunk model
		strcat(chunkModel, "_c1.md3");
		ent->s.modelindex3 = G_ModelIndex(chunkModel);
	}
}

//------------------------------------------------------------
void SetMiscModelDefaults(gentity_t* ent, const useFunc_t use_func, const char* material, const int solid_mask,
	const int animFlag,
	const qboolean take_damage, const qboolean damage_model = qfalse)
{
	// Apply damage and chunk models if they exist
	SetMiscModelModels(ent->model, ent, damage_model);

	ent->s.eFlags = animFlag;
	ent->svFlags |= SVF_PLAYER_USABLE;
	ent->contents = solid_mask;

	G_SetOrigin(ent, ent->s.origin);
	VectorCopy(ent->s.angles, ent->s.apos.trBase);
	gi.linkentity(ent);

	// Set a generic use function

	ent->e_UseFunc = use_func;
	G_SpawnInt("material", material, reinterpret_cast<int*>(&ent->material));

	if (ent->health)
	{
		ent->max_health = ent->health;
		ent->takedamage = take_damage;
		ent->e_PainFunc = painF_misc_model_breakable_pain;
		ent->e_DieFunc = dieF_misc_model_breakable_die;
	}
}

static void HealthStationSettings(gentity_t* ent)
{
	G_SpawnInt("count", "0", &ent->count);

	if (!ent->count)
	{
		switch (g_spskill->integer)
		{
		case 0: //	EASY
			ent->count = 100;
			break;
		case 1: //	MEDIUM
			ent->count = 75;
			break;
		default:
		case 2: //	HARD
			ent->count = 50;
			break;
		}
	}
}

void CrystalAmmoSettings(gentity_t* ent)
{
	G_SpawnInt("count", "0", &ent->count);

	if (!ent->count)
	{
		switch (g_spskill->integer)
		{
		case 0: //	EASY
			ent->count = 75;
			break;
		case 1: //	MEDIUM
			ent->count = 75;
			break;
		default:
		case 2: //	HARD
			ent->count = 75;
			break;
		}
	}
}

//------------------------------------------------------------

//------------------------------------------------------------
/*QUAKED misc_model_ghoul (1 0 0) (-16 -16 -37) (16 16 32)
"model"		arbitrary .glm file to display
"health" - how much health the model has - default 60 (zero makes non-breakable)
*/
//------------------------------------------------------------

extern int G_ParseAnimFileSet(const char* skeletonName, const char* model_name = nullptr);
int temp_animFileIndex;

void set_MiscAnim(gentity_t* ent)
{
	const animation_t* animations = level.knownAnimFileSets[temp_animFileIndex].animations;
	if (ent->playerModel & 1)
	{
		constexpr int anim = BOTH_STAND3;
		const float animSpeed = 50.0f / animations[anim].frameLerp;

		// yes, its the same animation, so work out where we are in the leg anim, and blend us
		gi.G2API_SetBoneAnim(&ent->ghoul2[0], "model_root", animations[anim].firstFrame,
			animations[anim].numFrames - 1 + animations[anim].firstFrame,
			BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, animSpeed, cg.time ? cg.time : level.time,
			-1, 350);
	}
	else
	{
		constexpr int anim = BOTH_PAIN3;
		const float animSpeed = 50.0f / animations[anim].frameLerp;
		gi.G2API_SetBoneAnim(&ent->ghoul2[0], "model_root", animations[anim].firstFrame,
			animations[anim].numFrames - 1 + animations[anim].firstFrame,
			BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, animSpeed, cg.time ? cg.time : level.time,
			-1, 350);
	}
	ent->nextthink = level.time + 900;
	ent->playerModel++;
}

void SP_misc_model_ghoul(gentity_t* ent)
{
#if 1
	if (!ent || !ent->model || !ent->model[0])
	{
		Com_Printf(S_COLOR_RED "SP_misc_model_ghoul: missing model key\n");
		return;
	}

	// Register model and init Ghoul2
	ent->s.modelindex = G_ModelIndex(ent->model);
	gi.G2API_InitGhoul2Model(ent->ghoul2, ent->model, ent->s.modelindex, NULL_HANDLE, NULL_HANDLE, 0, 0);
	ent->s.radius = 50;

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);

	qboolean bHasScale = G_SpawnVector("modelscale_vec", "1 1 1", ent->s.modelScale);
	if (!bHasScale)
	{
		float temp;
		G_SpawnFloat("modelscale", "0", &temp);
		if (temp != 0.0f)
		{
			ent->s.modelScale[0] = ent->s.modelScale[1] = ent->s.modelScale[2] = temp;
			bHasScale = qtrue;
		}
	}

	if (bHasScale)
	{
		// scale X
		ent->maxs[0] *= ent->s.modelScale[0];
		ent->mins[0] *= ent->s.modelScale[0];

		// scale Y
		ent->maxs[1] *= ent->s.modelScale[1];
		ent->mins[1] *= ent->s.modelScale[1];

		// scale Z and adjust origin
		const float oldMins2 = ent->mins[2];
		ent->maxs[2] *= ent->s.modelScale[2];
		ent->mins[2] *= ent->s.modelScale[2];
		ent->s.origin[2] += oldMins2 - ent->mins[2];
	}

	gi.linkentity(ent);
#else
	// old test block...
#endif
}

constexpr auto RACK_BLASTER = 1;
constexpr auto RACK_REPEATER = 2;
constexpr auto RACK_ROCKET = 4;

/*QUAKED misc_model_gun_rack (1 0 0.25) (-14 -14 -4) (14 14 30) BLASTER REPEATER ROCKET
model="models/map_objects/kejim/weaponsrack.md3"

NOTE: can mix and match these spawnflags to get multi-weapon racks.  If only one type is checked the rack will be full of those weapons
BLASTER - Puts one or more blaster guns on the rack.
REPEATER - Puts one or more repeater guns on the rack.
ROCKET - Puts one or more rocket launchers on the rack.
*/

static void GunRackAddItem(gitem_t* gun, vec3_t org, vec3_t angs, const float ffwd, const float fright, const float fup)
{
	vec3_t fwd, right;
	gentity_t* it_ent = G_Spawn();
	qboolean rotate = qtrue;

	AngleVectors(angs, fwd, right, nullptr);

	if (it_ent && gun)
	{
		// FIXME: scaling the ammo will probably need to be tweaked to a reasonable amount...adjust as needed
		// Set base ammo per type
		if (gun->giType == IT_WEAPON)
		{
			it_ent->spawnflags |= 16; // VERTICAL

			switch (gun->giTag)
			{
			case WP_BLASTER:
				it_ent->count = 15;
				break;
			case WP_REPEATER:
				it_ent->count = 100;
				break;
			case WP_ROCKET_LAUNCHER:
				it_ent->count = 4;
				break;
			default:;
			}
		}
		else
		{
			rotate = qfalse;

			// must deliberately make it small, or else the objects will spawn inside of each other.
			VectorSet(it_ent->maxs, 6.75f, 6.75f, 6.75f);
			VectorScale(it_ent->maxs, -1, it_ent->mins);
		}

		it_ent->spawnflags |= 1; // ITMSF_SUSPEND
		it_ent->classname = G_NewString(gun->classname); //copy it so it can be freed safely
		G_SpawnItem(it_ent, gun);

		// FinishSpawningItem handles everything, so clear the thinkFunc that was set in G_SpawnItem
		FinishSpawningItem(it_ent);

		if (gun->giType == IT_AMMO)
		{
			if (gun->giTag == AMMO_BLASTER) // I guess this just has to use different logic??
			{
				if (g_spskill->integer >= 2)
				{
					it_ent->count += 10; // give more on higher difficulty because there will be more/harder enemies?
				}
			}
			else
			{
				// scale ammo based on skill
				switch (g_spskill->integer)
				{
				case 0: // do default
					break;
				case 1:
					it_ent->count *= 0.75f;
					break;
				case 2:
					it_ent->count *= 0.5f;
					break;
				default:;
				}
			}
		}

		it_ent->nextthink = 0;

		VectorCopy(org, it_ent->s.origin);
		VectorMA(it_ent->s.origin, fright, right, it_ent->s.origin);
		VectorMA(it_ent->s.origin, ffwd, fwd, it_ent->s.origin);
		it_ent->s.origin[2] += fup;

		VectorCopy(angs, it_ent->s.angles);

		// by doing this, we can force the amount of ammo we desire onto the weapon for when it gets picked-up
		it_ent->flags |= FL_DROPPED_ITEM | FL_FORCE_PULLABLE_ONLY;
		it_ent->physicsBounce = 0.1f;

		for (int t = 0; t < 3; t++)
		{
			if (rotate)
			{
				if (t == YAW)
				{
					it_ent->s.angles[t] = AngleNormalize180(it_ent->s.angles[t] + 180 + Q_flrand(-1.0f, 1.0f) * 14);
				}
				else
				{
					it_ent->s.angles[t] = AngleNormalize180(it_ent->s.angles[t] + Q_flrand(-1.0f, 1.0f) * 4);
				}
			}
			else
			{
				if (t == YAW)
				{
					it_ent->s.angles[t] = AngleNormalize180(it_ent->s.angles[t] + 90 + Q_flrand(-1.0f, 1.0f) * 4);
				}
			}
		}

		G_SetAngles(it_ent, it_ent->s.angles);
		G_SetOrigin(it_ent, it_ent->s.origin);
		gi.linkentity(it_ent);
	}
}

//---------------------------------------------
void SP_misc_model_gun_rack(gentity_t* ent)
{
	gitem_t* blaster = nullptr, * repeater = nullptr, * rocket = nullptr;
	int ct = 0;
	float ofz[3]{};
	gitem_t* itemList[3]{};
	char* s;

	G_SpawnString("icon", "", &s);

	if (s && s[0])
	{
		// We have an icon, so index it now.  We are reusing the genericenemyindex
		// variable rather than adding a new one to the entity state.
		ent->s.genericenemyindex = G_IconIndex(s);
	}

	// If BLASTER is checked...or nothing is checked then we'll do blasters
	if (ent->spawnflags & RACK_BLASTER || !(ent->spawnflags & (RACK_BLASTER | RACK_REPEATER | RACK_ROCKET)))
	{
		blaster = FindItemForWeapon(WP_BLASTER);
	}

	if (ent->spawnflags & RACK_REPEATER)
	{
		repeater = FindItemForWeapon(WP_REPEATER);
	}

	if (ent->spawnflags & RACK_ROCKET)
	{
		rocket = FindItemForWeapon(WP_ROCKET_LAUNCHER);
	}

	//---------weapon types
	if (blaster)
	{
		ofz[ct] = 23.0f;
		itemList[ct++] = blaster;
	}

	if (repeater)
	{
		ofz[ct] = 24.5f;
		itemList[ct++] = repeater;
	}

	if (rocket)
	{
		ofz[ct] = 25.5f;
		itemList[ct++] = rocket;
	}

	if (ct) //..should always have at least one item on their, but just being safe
	{
		for (; ct < 3; ct++)
		{
			ofz[ct] = ofz[0];
			itemList[ct] = itemList[0]; // first weapon ALWAYS propagates to fill up the shelf
		}
	}

	// now actually add the items to the shelf...validate that we have a list to add
	if (ct)
	{
		for (int i = 0; i < ct; i++)
		{
			GunRackAddItem(itemList[i], ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 2,
				(i - 1) * 9 + Q_flrand(-1.0f, 1.0f) * 2, ofz[i]);
		}
	}

	ent->s.modelindex = G_ModelIndex("models/map_objects/kejim/weaponsrack.md3");

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);

	ent->contents = CONTENTS_SOLID;
	ent->svFlags |= SVF_BROADCAST;
	ent->s.eFlags2 |= EF2_RADAROBJECT;
	ent->s.genericenemyindex = G_IconIndex("gfx/mp/siegeicons/desert/weapon_recharge");

	gi.linkentity(ent);
}

constexpr auto RACK_METAL_BOLTS = 2;
constexpr auto RACK_ROCKETS = 4;
constexpr auto RACK_WEAPONS = 8;
constexpr auto RACK_HEALTH = 16;
constexpr auto RACK_PWR_CELL = 32;
constexpr auto RACK_NO_FILL = 64;

/*QUAKED misc_model_ammo_rack (1 0 0.25) (-14 -14 -4) (14 14 30) BLASTER METAL_BOLTS ROCKETS WEAPON HEALTH PWR_CELL NO_FILL
model="models/map_objects/kejim/weaponsrung.md3"

NOTE: can mix and match these spawnflags to get multi-ammo racks.  If only one type is checked the rack will be full of that ammo.  Only three ammo packs max can be displayed.

BLASTER - Adds one or more ammo packs that are compatible with Blasters and the Bryar pistol.
METAL_BOLTS - Adds one or more metal bolt ammo packs that are compatible with the heavy repeater and the flechette gun
ROCKETS - Puts one or more rocket packs on a rack.
WEAPON - adds a weapon matching a selected ammo type to the rack.
HEALTH - adds a health pack to the top shelf of the ammo rack
PWR_CELL - Adds one or more power cell packs that are compatible with the Disuptor, bowcaster, and demp2
NO_FILL - Only puts selected ammo on the rack, it never fills up all three slots if only one or two items were checked
*/

extern gitem_t* FindItemForAmmo(ammo_t ammo);

//---------------------------------------------
void SP_misc_model_ammo_rack(gentity_t* ent)
{
	char* s;

	G_SpawnString("icon", "", &s);

	if (s && s[0])
	{
		// We have an icon, so index it now.  We are reusing the genericenemyindex
		// variable rather than adding a new one to the entity state.
		ent->s.genericenemyindex = G_IconIndex(s);
	}
	// If BLASTER is checked...or nothing is checked then we'll do blasters
	if (ent->spawnflags & RACK_BLASTER || !(ent->spawnflags & (RACK_BLASTER | RACK_METAL_BOLTS | RACK_ROCKETS |
		RACK_PWR_CELL)))
	{
		if (ent->spawnflags & RACK_WEAPONS)
		{
			RegisterItem(FindItemForWeapon(WP_BLASTER));
		}
		RegisterItem(FindItemForAmmo(AMMO_BLASTER));
	}

	if (ent->spawnflags & RACK_METAL_BOLTS)
	{
		if (ent->spawnflags & RACK_WEAPONS)
		{
			RegisterItem(FindItemForWeapon(WP_REPEATER));
		}
		RegisterItem(FindItemForAmmo(AMMO_METAL_BOLTS));
	}

	if (ent->spawnflags & RACK_ROCKETS)
	{
		if (ent->spawnflags & RACK_WEAPONS)
		{
			RegisterItem(FindItemForWeapon(WP_ROCKET_LAUNCHER));
		}
		RegisterItem(FindItemForAmmo(AMMO_ROCKETS));
	}

	if (ent->spawnflags & RACK_PWR_CELL)
	{
		RegisterItem(FindItemForAmmo(AMMO_POWERCELL));
	}

	if (ent->spawnflags & RACK_HEALTH)
	{
		RegisterItem(FindItem("item_medpak_instant"));
	}

	ent->e_ThinkFunc = thinkF_spawn_rack_goods;
	ent->nextthink = level.time + 100;

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);

	ent->contents = CONTENTS_SHOTCLIP | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP;
	//CONTENTS_SOLID;//so use traces can go through them
	ent->svFlags |= SVF_BROADCAST;
	ent->s.eFlags2 |= EF2_RADAROBJECT;
	ent->s.genericenemyindex = G_IconIndex("gfx/mp/siegeicons/desert/weapon_recharge");

	gi.linkentity(ent);
}

// AMMO RACK!!
void spawn_rack_goods(gentity_t* ent)
{
	gitem_t* blaster = nullptr, * metal_bolts = nullptr, * rockets = nullptr;
	gitem_t* am_blaster = nullptr, * am_metal_bolts = nullptr, * am_rockets = nullptr, * am_pwr_cell = nullptr;
	gitem_t* health = nullptr;
	int pos = 0, ct = 0;
	gitem_t* itemList[4]{};
	// allocating 4, but we only use 3.  done so I don't have to validate that the array isn't full before I add another

	gi.unlinkentity(ent);

	// If BLASTER is checked...or nothing is checked then we'll do blasters
	if (ent->spawnflags & RACK_BLASTER || !(ent->spawnflags & (RACK_BLASTER | RACK_METAL_BOLTS | RACK_ROCKETS |
		RACK_PWR_CELL)))
	{
		if (ent->spawnflags & RACK_WEAPONS)
		{
			blaster = FindItemForWeapon(WP_BLASTER);
		}
		am_blaster = FindItemForAmmo(AMMO_BLASTER);
	}

	if (ent->spawnflags & RACK_METAL_BOLTS)
	{
		if (ent->spawnflags & RACK_WEAPONS)
		{
			metal_bolts = FindItemForWeapon(WP_REPEATER);
		}
		am_metal_bolts = FindItemForAmmo(AMMO_METAL_BOLTS);
	}

	if (ent->spawnflags & RACK_ROCKETS)
	{
		if (ent->spawnflags & RACK_WEAPONS)
		{
			rockets = FindItemForWeapon(WP_ROCKET_LAUNCHER);
		}
		am_rockets = FindItemForAmmo(AMMO_ROCKETS);
	}

	if (ent->spawnflags & RACK_PWR_CELL)
	{
		am_pwr_cell = FindItemForAmmo(AMMO_POWERCELL);
	}

	if (ent->spawnflags & RACK_HEALTH)
	{
		health = FindItem("item_medpak_instant");
		RegisterItem(health);
	}

	//---------Ammo types
	if (am_blaster)
	{
		itemList[ct++] = am_blaster;
	}

	if (am_metal_bolts)
	{
		itemList[ct++] = am_metal_bolts;
	}

	if (am_pwr_cell)
	{
		itemList[ct++] = am_pwr_cell;
	}

	if (am_rockets)
	{
		itemList[ct++] = am_rockets;
	}

	if (!(ent->spawnflags & RACK_NO_FILL) && ct)
		//double negative..should always have at least one item on there, but just being safe
	{
		for (; ct < 3; ct++)
		{
			itemList[ct] = itemList[0]; // first item ALWAYS propagates to fill up the shelf
		}
	}

	// now actually add the items to the shelf...validate that we have a list to add
	if (ct)
	{
		for (int i = 0; i < ct; i++)
		{
			GunRackAddItem(itemList[i], ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 0.5f, (i - 1) * 8, 7.0f);
		}
	}

	// -----Weapon option
	if (ent->spawnflags & RACK_WEAPONS)
	{
		gitem_t* it = nullptr;
		float v_off = 0;
		if (!(ent->spawnflags & (RACK_BLASTER | RACK_METAL_BOLTS | RACK_ROCKETS | RACK_PWR_CELL)))
		{
			// nothing was selected, so we assume blaster pack
			it = blaster;
		}
		else
		{
			// if weapon is checked...and so are one or more ammo types, then pick a random weapon to display..always give weaker weapons first
			if (blaster)
			{
				it = blaster;
				v_off = 25.5f;
			}
			else if (metal_bolts)
			{
				it = metal_bolts;
				v_off = 27.0f;
			}
			else if (rockets)
			{
				it = rockets;
				v_off = 28.0f;
			}
		}

		if (it)
		{
			// since we may have to put up a health pack on the shelf, we should know where we randomly put
			//	the gun so we don't put the pack on the same spot..so pick either the left or right side
			pos = Q_flrand(0.0f, 1.0f) > .5 ? -1 : 1;

			GunRackAddItem(it, ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 2,
				(Q_flrand(0.0f, 1.0f) * 6 + 4) * pos, v_off);
		}
	}

	// ------Medpack
	if (ent->spawnflags & RACK_HEALTH && health)
	{
		if (!pos)
		{
			// we haven't picked a side already...
			pos = Q_flrand(0.0f, 1.0f) > .5 ? -1 : 1;
		}
		else
		{
			// switch to the opposite side
			pos *= -1;
		}

		GunRackAddItem(health, ent->s.origin, ent->s.angles, Q_flrand(-1.0f, 1.0f) * 0.5f,
			(Q_flrand(0.0f, 1.0f) * 4 + 4) * pos, 24);
	}

	ent->s.modelindex = G_ModelIndex("models/map_objects/kejim/weaponsrung.md3");

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);

	gi.linkentity(ent);
}

constexpr auto DROP_MEDPACK = 1;
constexpr auto DROP_SHIELDS = 2;
constexpr auto DROP_BACTA = 4;
constexpr auto DROP_BATTERIES = 8;
constexpr auto DROP_CLOAK = 16;
constexpr auto DROP_BARRIER = 32;
constexpr auto DROP_GRAPPLEHOOK = 64;

/*QUAKED misc_model_cargo_small (1 0 0.25) (-14 -14 -4) (14 14 30) MEDPACK SHIELDS BACTA BATTERIES
model="models/map_objects/kejim/cargo_small.md3"

  Cargo crate that can only be destroyed by heavy class weapons ( turrets, emplaced guns, at-st )  Can spawn useful things when it breaks

MEDPACK - instant use medpacks
SHIELDS - instant shields
BACTA - bacta tanks
BATTERIES -

"health" - how much damage to take before blowing up ( default 25 )
"splashRadius" - damage range when it explodes ( default 96 )
"splashDamage" - damage to do within explode range ( default 1 )

*/
extern gentity_t* LaunchItem(gitem_t* item, const vec3_t origin, const vec3_t velocity, const char* target);

void misc_model_cargo_die(gentity_t* self, const gentity_t* inflictor, gentity_t* attacker, const int damage,
	const int mod, int d_flags,
	int hit_loc)
{
	vec3_t org, temp{};

	// copy these for later
	const int flags = self->spawnflags;
	VectorCopy(self->currentOrigin, org);

	// we already had spawn flags, but we don't care what they were...we just need to set up the flags we want for misc_model_breakable_die
	self->spawnflags = 8; // NO_DMODEL

	// pass through to get the effects and such
	misc_model_breakable_die(self, inflictor, attacker, damage, mod);

	// now that the model is broken, we can safely spawn these in it's place without them being in solid
	temp[2] = org[2] + 16;

	// annoying, but spawn each thing in its own little quadrant so that they don't end up on top of each other
	if (flags & DROP_MEDPACK)
	{
		gitem_t* health = FindItem("item_medpak_instant");

		if (health)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 + 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 + 16;

			LaunchItem(health, temp, vec3_origin, nullptr);
		}
	}
	if (flags & DROP_SHIELDS)
	{
		gitem_t* shields = FindItem("item_shield_sm_instant");

		if (shields)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 - 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 + 16;

			LaunchItem(shields, temp, vec3_origin, nullptr);
		}
	}

	if (flags & DROP_BACTA)
	{
		gitem_t* bacta = FindItem("item_bacta");

		if (bacta)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 - 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 - 16;

			LaunchItem(bacta, temp, vec3_origin, nullptr);
		}
	}

	if (flags & DROP_BATTERIES)
	{
		gitem_t* batteries = FindItem("item_battery");

		if (batteries)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 + 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 - 16;

			LaunchItem(batteries, temp, vec3_origin, nullptr);
		}
	}

	if (flags & DROP_CLOAK)
	{
		gitem_t* cloak = FindItem("item_cloak");

		if (cloak)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 - 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 - 16;

			LaunchItem(cloak, temp, vec3_origin, nullptr);
		}
	}

	if (flags & DROP_BARRIER)
	{
		gitem_t* barrier = FindItem("item_Barrier");

		if (barrier)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 - 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 - 16;

			LaunchItem(barrier, temp, vec3_origin, nullptr);
		}
	}

	if (flags & DROP_GRAPPLEHOOK)
	{
		gitem_t* grapple = FindItem("item_GrappleHook");

		if (grapple)
		{
			temp[0] = org[0] + Q_flrand(-1.0f, 1.0f) * 8 - 16;
			temp[1] = org[1] + Q_flrand(-1.0f, 1.0f) * 8 - 16;

			LaunchItem(grapple, temp, vec3_origin, nullptr);
		}
	}
}

//---------------------------------------------
void SP_misc_model_cargo_small(gentity_t* ent)
{
	G_SpawnInt("splashRadius", "96", &ent->splashRadius);
	G_SpawnInt("splashDamage", "1", &ent->splashDamage);

	if (ent->spawnflags & DROP_MEDPACK)
	{
		RegisterItem(FindItem("item_medpak_instant"));
	}

	if (ent->spawnflags & DROP_SHIELDS)
	{
		RegisterItem(FindItem("item_shield_sm_instant"));
	}

	if (ent->spawnflags & DROP_BACTA)
	{
		RegisterItem(FindItem("item_bacta"));
	}

	if (ent->spawnflags & DROP_BATTERIES)
	{
		RegisterItem(FindItem("item_battery"));
	}

	if (ent->spawnflags & DROP_CLOAK)
	{
		RegisterItem(FindItem("item_cloak"));
	}

	if (ent->spawnflags & DROP_BARRIER)
	{
		RegisterItem(FindItem("item_Barrier"));
	}

	if (ent->spawnflags & DROP_GRAPPLEHOOK)
	{
		RegisterItem(FindItem("item_GrappleHook"));
	}

	G_SpawnInt("health", "25", &ent->health);

	SetMiscModelDefaults(ent, useF_NULL, "11",
		CONTENTS_SOLID | CONTENTS_OPAQUE | CONTENTS_BODY | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP, 0,
		qtrue, qfalse);
	ent->s.modelindex2 = G_ModelIndex("/models/map_objects/kejim/cargo_small.md3"); // Precache model

	// we only take damage from a heavy weapon class missile
	ent->flags |= FL_DMG_BY_HEAVY_WEAP_ONLY;

	ent->e_DieFunc = dieF_misc_model_cargo_die;

	ent->radius = 1.5f; // scale number of chunks spawned
}