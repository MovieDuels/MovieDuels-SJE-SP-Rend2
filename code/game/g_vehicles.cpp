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

#include "../qcommon/q_shared.h"
#include "g_local.h"

#ifdef _JK2 //SP does not have this preprocessor for game like MP does
#ifndef _JK2MP
#define _JK2MP
#endif
#endif

#ifndef _JK2MP
#include "g_functions.h"
#include "g_vehicles.h"
#include "../cgame/cg_local.h"
#else
#include "bg_vehicles.h"
#endif
#include <qcommon\q_math.h>

#ifdef _JK2MP
//this is really horrible, but it works! just be sure not to use any locals or anything
//with these names (exluding bool, false, true). -rww
#define currentAngles r.currentAngles
#define currentOrigin r.currentOrigin
#define mins r.mins
#define maxs r.maxs
#define legsAnimTimer legsTimer
#define torsoAnimTimer torsoTimer
#define bool qboolean
#define false qfalse
#define true qtrue

#define sqrtf sqrt

#define MOD_EXPLOSIVE MOD_SUICIDE
#endif

#ifndef _JK2MP
#define bgEntity_t gentity_t
#endif

extern gentity_t* NPC_Spawn_Do(gentity_t* pEnt, qboolean fullSpawnNow);
extern qboolean G_ClearLineOfSight(const vec3_t point1, const vec3_t point2, int ignore, int clipmask);
extern qboolean g_set_g2_player_model_info(gentity_t* pEnt, const char* model_name, const char* surf_off, const char* surf_on);
extern void G_RemovePlayerModel(gentity_t* pEnt);
extern void G_ChangePlayerModel(gentity_t* pEnt, const char* new_model);
extern void G_RemoveWeaponModels(gentity_t* ent);
extern void CG_ChangeWeapon(int num);
extern qboolean Q3_TaskIDPending(const gentity_t* ent, taskID_t taskType);
extern void SetClientViewAngle(gentity_t* ent, vec3_t angle);
extern vmCvar_t cg_thirdPersonAlpha;
extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern cvar_t* g_speederControlScheme;
extern cvar_t* in_joystick;
extern void PM_SetAnim(const pmove_t* pm, int set_anim_parts, int anim, int set_anim_flags, int blend_time);
extern int PM_AnimLength(const int index, const animNumber_t anim);
extern void NPC_SetAnim(gentity_t* ent, int set_anim_parts, int anim, int set_anim_flags, int i_blend);
extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t push_dir, float strength, const qboolean breakSaberLock);
extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torso_anim_timer, int time);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legs_anim_timer, int time);
extern qboolean BG_UnrestrainedPitchRoll(const playerState_t* ps, const Vehicle_t* p_veh);

void Vehicle_SetAnim(gentity_t* ent, int setAnimParts, int anim, int setAnimFlags, int iBlend)
{
	NPC_SetAnim(ent, setAnimParts, anim, setAnimFlags, iBlend);
}

void G_VehicleTrace(trace_t* results, const vec3_t start, const vec3_t tMins, const vec3_t tMaxs, const vec3_t end, int passEntityNum, int contentmask)
{
	gi.trace(results, start, tMins, tMaxs, end, passEntityNum, contentmask, static_cast<EG2_Collision>(0), 0);
}

Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt)
{
	const gentity_t* ent = pEnt;

	if (ent && ent->client && ent->client->NPC_class != CLASS_VEHICLE && ent->s.m_iVehicleNum != 0)
	{
		return g_entities[ent->s.m_iVehicleNum].m_pVehicle;
	}
	return nullptr;
}

bool G_IsRidingTurboVehicle(const gentity_t* pEnt)
{
	const gentity_t* ent = pEnt;

	if (ent && ent->client && ent->client->NPC_class != CLASS_VEHICLE && ent->s.m_iVehicleNum != 0)
	{
		return level.time < g_entities[ent->s.m_iVehicleNum].m_pVehicle->m_iTurboTime;
	}
	return false;
}

float G_CanJumpToEnemyVeh(Vehicle_t* p_veh, const usercmd_t* pUcmd)
{
#ifndef _JK2MP
	const gentity_t* rider = p_veh->m_pPilot;

	// If There Is An Enemy And We Are At The Same Z Height
	//------------------------------------------------------
	if (rider &&
		rider->enemy &&
		pUcmd->rightmove &&
		fabsf(rider->enemy->currentOrigin[2] - rider->currentOrigin[2]) < 50.0f)
	{
		if (level.time < p_veh->m_safeJumpMountTime)
		{
			return p_veh->m_safeJumpMountRightDot;
		}

		// If The Enemy Is Riding Another Vehicle
		//----------------------------------------
		const Vehicle_t* enemyVeh = G_IsRidingVehicle(rider->enemy);
		if (enemyVeh)
		{
			vec3_t toEnemy;

			// If He Is Close Enough And Going The Same Speed
			//------------------------------------------------
			VectorSubtract(rider->enemy->currentOrigin, rider->currentOrigin, toEnemy);
			const float toEnemyDistance = VectorNormalize(toEnemy);
			if (toEnemyDistance < 70.0f &&
				p_veh->m_pParentEntity->resultspeed > 100.0f &&
				fabsf(p_veh->m_pParentEntity->resultspeed - enemyVeh->m_pParentEntity->resultspeed) < 100.0f)
			{
				vec3_t riderRight;
				vec3_t riderFwd;
				// If He Is Either To The Left Or Right Of Me
				//--------------------------------------------
				AngleVectors(rider->currentAngles, riderFwd, riderRight, nullptr);
				const float riderRightDot = DotProduct(riderRight, toEnemy);
				if (pUcmd->rightmove > 0 && riderRightDot > 0.2 || pUcmd->rightmove < 0 && riderRightDot < -0.2)
				{
					vec3_t enemyFwd;
					// If We Are Both Going About The Same Direction
					//-----------------------------------------------
					AngleVectors(rider->enemy->currentAngles, enemyFwd, nullptr, nullptr);
					if (DotProduct(enemyFwd, riderFwd) > 0.2f)
					{
						p_veh->m_safeJumpMountTime = level.time + Q_irand(3000, 4000); // Ok, now you get a 3 sec window
						p_veh->m_safeJumpMountRightDot = riderRightDot;
						return riderRightDot;
					} // Same Direction?
				} // To Left Or Right?
			} // Close Enough & Same Speed?
		} // Enemy Riding A Vehicle?
	} // Has Enemy And On Same Z-Height
#endif
	return 0.0f;
}

// Spawn this vehicle into the world.
void G_VehicleSpawn(gentity_t* self)
{
	float yaw;
	gentity_t* vehEnt;

	VectorCopy(self->currentOrigin, self->s.origin);

	gi.linkentity(self);

	if (!self->count)
	{
		self->count = 1;
	}

	//save this because self gets removed in next func
	yaw = self->s.angles[YAW];

	vehEnt = NPC_Spawn_Do(self, qtrue);

	if (!vehEnt)
	{
		return; //return NULL;
	}

	vehEnt->s.angles[YAW] = yaw;
	if (vehEnt->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL)
	{
		vehEnt->NPC->behaviorState = BS_CINEMATIC;
	}

#ifdef _JK2MP //special check in case someone disconnects/dies while boarding
	if (vehEnt->spawnflags & 1)
	{ //die without pilot
		if (!vehEnt->damage)
		{ //default 10 sec
			vehEnt->damage = 10000;
		}
		if (!vehEnt->speed)
		{ //default 512 units
			vehEnt->speed = 512.0f;
		}
		vehEnt->m_pVehicle->m_iPilotTime = level.time + vehEnt->damage;
	}
#else
	if (vehEnt->spawnflags & 1)
	{
		//die without pilot
		vehEnt->m_pVehicle->m_iPilotTime = level.time + vehEnt->endFrame;
	}
#endif
	//return vehEnt;
}

// Attachs an entity to the vehicle it's riding (it's owner).
void G_AttachToVehicle(gentity_t* pEnt, usercmd_t** ucmd)
{
	gentity_t* vehEnt;
	mdxaBone_t bolt_matrix;
	gentity_t* ent;

	if (!pEnt || !ucmd)
		return;

	ent = pEnt;
	vehEnt = ent->owner;

	ent->waypoint = vehEnt->waypoint; // take the veh's waypoint as your own

	if (!vehEnt->m_pVehicle)
		return;
	// Get the driver tag.
	gi.G2API_GetBoltMatrix(vehEnt->ghoul2, vehEnt->playerModel, vehEnt->crotchBolt, &bolt_matrix,
		vehEnt->m_pVehicle->m_vOrientation, vehEnt->currentOrigin,
		cg.time ? cg.time : level.time, nullptr, vehEnt->s.modelScale);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent->client->ps.origin);
	gi.linkentity(ent);
}

void G_KnockOffVehicle(gentity_t* pRider, const gentity_t* self, const qboolean bPull)
{
	vec3_t riderAngles, fDir, rDir, dir2Me;

	if (!pRider || !pRider->client)
	{
		return;
	}

	Vehicle_t* p_veh = G_IsRidingVehicle(pRider);

	if (!p_veh || !p_veh->m_pVehicleInfo)
	{
		return;
	}

	VectorCopy(pRider->currentAngles, riderAngles);
	riderAngles[0] = 0;
	AngleVectors(riderAngles, fDir, rDir, nullptr);
	VectorSubtract(self->currentOrigin, pRider->currentOrigin, dir2Me);
	dir2Me[2] = 0;
	VectorNormalize(dir2Me);
	const float fDot = DotProduct(fDir, dir2Me);
	if (fDot >= 0.5f)
	{
		//I'm in front of them
		if (bPull)
		{
			//pull them foward
			p_veh->m_EjectDir = VEH_EJECT_FRONT;
		}
		else
		{
			//push them back
			p_veh->m_EjectDir = VEH_EJECT_REAR;
		}
	}
	else if (fDot <= -0.5f)
	{
		//I'm behind them
		if (bPull)
		{
			//pull them back
			p_veh->m_EjectDir = VEH_EJECT_REAR;
		}
		else
		{
			//push them forward
			p_veh->m_EjectDir = VEH_EJECT_FRONT;
		}
	}
	else
	{
		//to the side of them
		const float rDot = DotProduct(fDir, dir2Me);
		if (rDot >= 0.0f)
		{
			//to the right
			if (bPull)
			{
				//pull them right
				p_veh->m_EjectDir = VEH_EJECT_RIGHT;
			}
			else
			{
				//push them left
				p_veh->m_EjectDir = VEH_EJECT_LEFT;
			}
		}
		else
		{
			//to the left
			if (bPull)
			{
				//pull them left
				p_veh->m_EjectDir = VEH_EJECT_LEFT;
			}
			else
			{
				//push them right
				p_veh->m_EjectDir = VEH_EJECT_RIGHT;
			}
		}
	}
	//now forcibly eject them
	p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qtrue);
}

void G_PilotXWing(gentity_t* ent)
{
	if (ent->client->ps.vehicleModel != 0)
	{
		CG_ChangeWeapon(WP_SABER);
		ent->client->ps.vehicleModel = 0;
		ent->svFlags &= ~SVF_CUSTOM_GRAVITY;
		ent->client->ps.stats[STAT_ARMOR] = 0; //HACK
		//ent->mass = 10;
		//gi.cvar_set( "m_pitchOverride", "0" );
		//gi.cvar_set( "m_yawOverride", "0" );
		if (ent->client->ps.weapon != WP_SABER)
		{
			gi.cvar_set("cg_thirdperson", "0");
		}
		cg.overrides.active |= CG_OVERRIDE_3RD_PERSON_RNG;
		cg.overrides.thirdPersonRange = 240;
		cg.overrides.active &= ~CG_OVERRIDE_FOV;
		cg.overrides.fov = 0;
	}
	else
	{
		ent->client->ps.vehicleModel = G_ModelIndex("models/map_objects/ships/x_wing.md3");

		ent->client->ps.stats[STAT_WEAPONS] |= 1 << WP_ATST_SIDE;
		ent->client->ps.ammo[weaponData[WP_ATST_SIDE].ammoIndex] = ammoData[weaponData[WP_ATST_SIDE].ammoIndex].max;
		const gitem_t* item = FindItemForWeapon(WP_ATST_SIDE);
		RegisterItem(item); //make sure the weapon is cached in case this runs at startup
		G_AddEvent(ent, EV_ITEM_PICKUP, item - bg_itemlist);
		CG_ChangeWeapon(WP_ATST_SIDE);

		ent->client->ps.gravity = 0;
		ent->svFlags |= SVF_CUSTOM_GRAVITY;
		ent->client->ps.stats[STAT_ARMOR] = 200; //FIXME: define?
		//ent->mass = 300;
		ent->client->ps.speed = 0;
		gi.cvar_set("cg_thirdperson", "1");
		cg.overrides.active |= CG_OVERRIDE_3RD_PERSON_RNG | CG_OVERRIDE_FOV;
		cg.overrides.thirdPersonRange = 240;
		cg.overrides.fov = 100;
	}
}

void G_DrivableATSTDie(gentity_t* self)
{
}

void G_DriveATST(gentity_t* pEnt, gentity_t* atst)
{
	if (pEnt->NPC_type && pEnt->client && pEnt->client->NPC_class == CLASS_ATST)
	{
		//already an atst, switch back
		//open hatch
		G_RemovePlayerModel(pEnt);
		pEnt->NPC_type = "player";
		pEnt->client->NPC_class = CLASS_PLAYER;
		pEnt->flags &= ~FL_SHIELDED;
		pEnt->client->ps.eFlags &= ~EF_IN_ATST;
		//size
		VectorCopy(playerMins, pEnt->mins);
		VectorCopy(playerMaxs, pEnt->maxs);
		pEnt->client->crouchheight = CROUCH_MAXS_2;
		pEnt->client->standheight = DEFAULT_MAXS_2;
		pEnt->s.radius = 0;
		G_ChangePlayerModel(pEnt, pEnt->NPC_type);

		pEnt->client->ps.weapons[WP_ATST_MAIN] = 0;
		pEnt->client->ps.weapons[WP_ATST_SIDE] = 0;
		pEnt->client->ps.ammo[weaponData[WP_ATST_MAIN].ammoIndex] = 0;
		pEnt->client->ps.ammo[weaponData[WP_ATST_SIDE].ammoIndex] = 0;
		if (pEnt->client->ps.weapons[WP_BLASTER])
		{
			CG_ChangeWeapon(WP_BLASTER);
			//camera
			if (cg_gunAutoFirst.integer)
			{
				//go back to first person
				gi.cvar_set("cg_thirdperson", "0");
			}
		}
		else
		{
			CG_ChangeWeapon(WP_NONE);
		}
		cg.overrides.active &= ~(CG_OVERRIDE_3RD_PERSON_RNG | CG_OVERRIDE_3RD_PERSON_VOF | CG_OVERRIDE_3RD_PERSON_POF |
			CG_OVERRIDE_3RD_PERSON_APH);
		cg.overrides.thirdPersonRange = cg.overrides.thirdPersonVertOffset = cg.overrides.thirdPersonPitchOffset = 0;
		cg.overrides.thirdPersonAlpha = cg_thirdPersonAlpha.value;
		pEnt->client->ps.viewheight = pEnt->maxs[2] + STANDARD_VIEWHEIGHT_OFFSET;
		//pEnt->mass = 10;
	}
	else
	{
		//become an atst
		pEnt->NPC_type = "atst";
		pEnt->client->NPC_class = CLASS_ATST;
		pEnt->client->ps.eFlags |= EF_IN_ATST;
		pEnt->flags |= FL_SHIELDED;
		//size
		VectorSet(pEnt->mins, ATST_MINS0, ATST_MINS1, ATST_MINS2);
		VectorSet(pEnt->maxs, ATST_MAXS0, ATST_MAXS1, ATST_MAXS2);
		pEnt->client->crouchheight = ATST_MAXS2;
		pEnt->client->standheight = ATST_MAXS2;
		if (!atst)
		{
			//no pEnt to copy from
			G_ChangePlayerModel(pEnt, "atst");
			//G_SetG2PlayerModel( pEnt, "atst", NULL, NULL, NULL );
			NPC_SetAnim(pEnt, SETANIM_BOTH, BOTH_STAND1, SETANIM_FLAG_OVERRIDE, 200);
		}
		else
		{
			G_RemovePlayerModel(pEnt);
			G_RemoveWeaponModels(pEnt);
			gi.G2API_CopyGhoul2Instance(atst->ghoul2, pEnt->ghoul2, -1);
			pEnt->playerModel = 0;
			g_set_g2_player_model_info(pEnt, "atst", nullptr, nullptr);
			//turn off hatch underside
			gi.G2API_SetSurfaceOnOff(&pEnt->ghoul2[pEnt->playerModel], "head_hatchcover",
				0x00000002/*G2SURFACEFLAG_OFF*/);
			G_Sound(pEnt, G_SoundIndex("sound/chars/atst/atst_hatch_close"));
		}
		pEnt->s.radius = 320;
		//weapon
		const gitem_t* item = FindItemForWeapon(WP_ATST_MAIN); //precache the weapon
		CG_RegisterItemSounds(item - bg_itemlist);
		CG_RegisterItemVisuals(item - bg_itemlist);
		item = FindItemForWeapon(WP_ATST_SIDE); //precache the weapon
		CG_RegisterItemSounds(item - bg_itemlist);
		CG_RegisterItemVisuals(item - bg_itemlist);
		pEnt->client->ps.weapons[WP_ATST_MAIN] = 1;
		pEnt->client->ps.weapons[WP_ATST_SIDE] = 1;
		pEnt->client->ps.ammo[weaponData[WP_ATST_MAIN].ammoIndex] = ammoData[weaponData[WP_ATST_MAIN].ammoIndex].max;
		pEnt->client->ps.ammo[weaponData[WP_ATST_SIDE].ammoIndex] = ammoData[weaponData[WP_ATST_SIDE].ammoIndex].max;
		CG_ChangeWeapon(WP_ATST_MAIN);
		//HACKHACKHACKTEMP
		item = FindItemForWeapon(WP_EMPLACED_GUN);
		CG_RegisterItemSounds(item - bg_itemlist);
		CG_RegisterItemVisuals(item - bg_itemlist);
		item = FindItemForWeapon(WP_ROCKET_LAUNCHER);
		CG_RegisterItemSounds(item - bg_itemlist);
		CG_RegisterItemVisuals(item - bg_itemlist);
		item = FindItemForWeapon(WP_BOWCASTER);
		CG_RegisterItemSounds(item - bg_itemlist);
		CG_RegisterItemVisuals(item - bg_itemlist);
		gi.cvar_set("cg_thirdperson", "1");
		cg.overrides.active |= CG_OVERRIDE_3RD_PERSON_RNG;
		cg.overrides.thirdPersonRange = 240;
		pEnt->client->ps.viewheight = 120;
	}
}

// Animate the vehicle and it's riders.
static void Animate(Vehicle_t* p_veh)
{
	// Validate a pilot rider.
	if (p_veh->m_pPilot)
	{
		if (p_veh->m_pVehicleInfo->AnimateRiders)
		{
			p_veh->m_pVehicleInfo->AnimateRiders(p_veh);
		}
	}

	p_veh->m_pVehicleInfo->AnimateVehicle(p_veh);
}

// Determine whether this entity is able to board this vehicle or not.
static bool ValidateBoard(Vehicle_t* p_veh, bgEntity_t* pEnt)
{
	// Determine where the entity is entering the vehicle from (left, right, or back).
	vec3_t vVehToEnt;
	vec3_t vVehDir;
	const gentity_t* parent = p_veh->m_pParentEntity;
	const gentity_t* ent = pEnt;
	vec3_t vVehAngles;

	if (p_veh->m_iDieTime > 0)
	{
		return false;
	}

	if (ent->health <= 0)
	{
		//dead men can't ride vehicles
		return false;
	}

	if (p_veh->m_pPilot != nullptr)
	{
		//already have a driver!
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			//I know, I know, this should by in the fighters's validateboard()
			//can never steal a fighter from it's pilot
			return false;
		}
		if (p_veh->m_pVehicleInfo->type == VH_WALKER)
		{
			//I know, I know, this should by in the walker's validateboard()
			if (!ent->client || ent->client->ps.groundEntityNum != parent->s.number)
			{
				//can only steal an occupied AT-ST if you're on top (by the hatch)
				return false;
			}
		}
		else if (p_veh->m_pVehicleInfo->type == VH_SPEEDER)
		{
			//you can only steal the bike from the driver if you landed on the driver or bike
			return p_veh->m_iBoarding == VEH_MOUNT_THROW_LEFT || p_veh->m_iBoarding == VEH_MOUNT_THROW_RIGHT;
		}
	}
	// Yes, you shouldn't have put this here (you 'should' have made an 'overriden' ValidateBoard func), but in this
	// instance it's more than adequate (which is why I do it too :-). Making a whole other function for this is silly.
	else if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
	{
		// If you're a fighter, you allow everyone to enter you from all directions.
		return true;
	}

	// Clear out all orientation axis except for the yaw.
	VectorSet(vVehAngles, 0, parent->currentAngles[YAW], 0);

	// Vector from Entity to Vehicle.
	VectorSubtract(ent->currentOrigin, parent->currentOrigin, vVehToEnt);
	vVehToEnt[2] = 0;
	VectorNormalize(vVehToEnt);

	// Get the right vector.
	AngleVectors(vVehAngles, nullptr, vVehDir, nullptr);
	VectorNormalize(vVehDir);

	// Find the angle between the vehicle right vector and the vehicle to entity vector.
	const float fDot = DotProduct(vVehToEnt, vVehDir);

	// If the entity is within a certain angle to the left of the vehicle...
	if (fDot >= 0.5f)
	{
		// Right board.
		p_veh->m_iBoarding = -2;
	}
	else if (fDot <= -0.5f)
	{
		// Left board.
		p_veh->m_iBoarding = -1;
	}
	// Maybe they're trying to board from the back...
	else
	{
		// If the entity is within a certain angle behind the vehicle...
		//if ( fDot <= -0.85f )
		{
			// Jump board.
			p_veh->m_iBoarding = -3;
		}
	}

	// If for some reason we couldn't board, leave...
	if (p_veh->m_iBoarding > -1)
		return false;

	return true;
}

// Board this Vehicle (get on). The first entity to board an empty vehicle becomes the Pilot.
static bool Board(Vehicle_t* p_veh, bgEntity_t* pEnt)
{
	vec3_t v_player_dir;
	const auto ent = pEnt;
	auto parent = p_veh->m_pParentEntity;

	// If it's not a valid entity, OR if the vehicle is blowing up (it's dead), OR it's not
	// empty, OR we're already being boarded, OR the person trying to get on us is already
	// in a vehicle (that was a fun bug :-), leave!
	if (!ent || parent->health <= 0 || p_veh->m_iBoarding > 0 || ent->s.m_iVehicleNum != 0)
	{
		return false;
	}

	// Bucking so we can't do anything (NOTE: Should probably be a better name since fighters don't buck...).
	if (p_veh->m_ulFlags & VEH_BUCKING)
	{
		return false;
	}

	// Validate the entity's ability to board this vehicle.
	if (!p_veh->m_pVehicleInfo->ValidateBoard(p_veh, pEnt))
	{
		return false;
	}

	// Tell everybody their status.
	// ALWAYS let the player be the pilot.
	if (ent->s.number < MAX_CLIENTS)
	{
		p_veh->m_pOldPilot = p_veh->m_pPilot;
		p_veh->m_pVehicleInfo->SetPilot(p_veh, ent);
		ent->s.m_iVehicleNum = parent->s.number;
		parent->owner = ent;

#ifdef QAGAME
		gentity_t* gParent = parent;

		if ((gParent->spawnflags & 2))
		{//was being suspended
			gParent->spawnflags &= ~2;
			G_Sound(gParent, CHAN_AUTO, G_SoundIndex("sound/vehicles/common/release.wav"));
			if (gParent->fly_sound_debounce_time)
			{//we should drop like a rock for a few seconds
				p_veh->m_iDropTime = level.time + gParent->fly_sound_debounce_time;
			}
		}
#endif
	}
	else
	{
		// If there's no pilot, try to drive this vehicle.
		if (p_veh->m_pPilot == nullptr)
		{
			p_veh->m_pVehicleInfo->SetPilot(p_veh, ent);
			parent->owner = ent;

			parent->client->ps.speed = 0;
			memset(&p_veh->m_ucmd, 0, sizeof(usercmd_t));
		}
		// We're full, sorry...
		else
		{
			return false;
		}
	}

	// Make sure the entity knows it's in a vehicle.
	ent->s.m_iVehicleNum = parent->s.number;
	ent->owner = parent;
	parent->s.m_iVehicleNum = ent->s.number + 1;

	if (p_veh->m_pVehicleInfo->numHands == 2)
	{
		//switch to vehicle weapon
		if (ent->s.number < MAX_CLIENTS)
		{
			// Riding means you get WP_NONE
			ent->client->ps.stats[STAT_WEAPONS] |= 1 << WP_NONE;
		}
		//PM_WeaponOkOnVehicle
		if (ent->client->ps.weapon != WP_SABER
			&& ent->client->ps.weapon != WP_BLASTER_PISTOL
			&& ent->client->ps.weapon != WP_BLASTER
			&& ent->client->ps.weapon != WP_BRYAR_PISTOL
			&& ent->client->ps.weapon != WP_BOWCASTER
			&& ent->client->ps.weapon != WP_REPEATER
			&& ent->client->ps.weapon != WP_DEMP2
			&& ent->client->ps.weapon != WP_FLECHETTE
			|| !(p_veh->m_pVehicleInfo->type == VH_ANIMAL || p_veh->m_pVehicleInfo->type == VH_SPEEDER))
		{
			//switch to weapon none?
			if (ent->s.number < MAX_CLIENTS)
			{
				CG_ChangeWeapon(WP_NONE);
			}
			ent->client->ps.weapon = WP_NONE;
			G_RemoveWeaponModels(ent);
		}
	}

	if (p_veh->m_pVehicleInfo->hideRider)
	{
		//hide the rider
		p_veh->m_pVehicleInfo->Ghost(p_veh, ent);
	}
	// Play the start sounds
	if (p_veh->m_pVehicleInfo->soundOn)
	{
		G_SoundIndexOnEnt(parent, CHAN_AUTO, p_veh->m_pVehicleInfo->soundOn);
	}

	VectorCopy(p_veh->m_vOrientation, v_player_dir);
	v_player_dir[ROLL] = 0;
	SetClientViewAngle(ent, v_player_dir);

	return true;
}

static bool VEH_TryEject(const Vehicle_t* p_veh,
	gentity_t* parent,
	gentity_t* ent,
	const int ejectDir,
	vec3_t vExitPos)
{
	float fEntDiag;
	vec3_t vEntMins, vEntMaxs, vVehLeaveDir, vVehAngles;
	trace_t m_ExitTrace;

	// Make sure that the entity is not 'stuck' inside the vehicle (since their bboxes will now intersect).
	// This makes the entity leave the vehicle from the right side.
	VectorSet(vVehAngles, 0, parent->currentAngles[YAW], 0);
	switch (ejectDir)
	{
		// Left.
	case VEH_EJECT_LEFT:
		AngleVectors(vVehAngles, nullptr, vVehLeaveDir, nullptr);
		vVehLeaveDir[0] = -vVehLeaveDir[0];
		vVehLeaveDir[1] = -vVehLeaveDir[1];
		vVehLeaveDir[2] = -vVehLeaveDir[2];
		break;
		// Right.
	case VEH_EJECT_RIGHT:
		AngleVectors(vVehAngles, nullptr, vVehLeaveDir, nullptr);
		break;
		// Front.
	case VEH_EJECT_FRONT:
		AngleVectors(vVehAngles, vVehLeaveDir, nullptr, nullptr);
		break;
		// Rear.
	case VEH_EJECT_REAR:
		AngleVectors(vVehAngles, vVehLeaveDir, nullptr, nullptr);
		vVehLeaveDir[0] = -vVehLeaveDir[0];
		vVehLeaveDir[1] = -vVehLeaveDir[1];
		vVehLeaveDir[2] = -vVehLeaveDir[2];
		break;
		// Top.
	case VEH_EJECT_TOP:
		AngleVectors(vVehAngles, nullptr, nullptr, vVehLeaveDir);
		break;
		// Bottom?.
	case VEH_EJECT_BOTTOM:
		break;
	default:;
	}
	VectorNormalize(vVehLeaveDir);

	float fBias = 1.0f;
	if (p_veh->m_pVehicleInfo->type == VH_WALKER)
	{
		//hacktastic!
		fBias += 0.2f;
	}
	VectorCopy(ent->currentOrigin, vExitPos);
	const float fVehDiag = sqrtf(parent->maxs[0] * parent->maxs[0] + parent->maxs[1] * parent->maxs[1]);
	VectorCopy(ent->maxs, vEntMaxs);
#ifdef _JK2MP
	if (ent->s.number < MAX_CLIENTS)
	{//for some reason, in MP, player client mins and maxs are never stored permanently, just set to these hardcoded numbers in PMove
		vEntMaxs[0] = 15;
		vEntMaxs[1] = 15;
	}
#endif
	fEntDiag = sqrtf(vEntMaxs[0] * vEntMaxs[0] + vEntMaxs[1] * vEntMaxs[1]);
	vVehLeaveDir[0] *= (fVehDiag + fEntDiag) * fBias; // x
	vVehLeaveDir[1] *= (fVehDiag + fEntDiag) * fBias; // y
	vVehLeaveDir[2] *= (fVehDiag + fEntDiag) * fBias;
	VectorAdd(vExitPos, vVehLeaveDir, vExitPos);

	//we actually could end up *not* getting off if the trace fails...
	// Check to see if this new position is a valid place for our entity to go.
#ifdef _JK2MP
	VectorSet(vEntMins, -15.0f, -15.0f, DEFAULT_MINS_2);
	VectorSet(vEntMaxs, 15.0f, 15.0f, DEFAULT_MAXS_2);
#else
	VectorCopy(ent->mins, vEntMins);
	VectorCopy(ent->maxs, vEntMaxs);
#endif
	G_VehicleTrace(&m_ExitTrace, ent->currentOrigin, vEntMins, vEntMaxs, vExitPos, ent->s.number, ent->clipmask);

	if (m_ExitTrace.allsolid //in solid
		|| m_ExitTrace.startsolid)
	{
		return false;
	}
	// If the trace hit something, we can't go there!
	if (m_ExitTrace.fraction < 1.0f)
	{
		//not totally clear
#ifdef _JK2MP
		if ((parent->clipmask & ent->r.contents))//vehicle could actually get stuck on body
#else
		if (parent->clipmask & ent->contents) //vehicle could actually get stuck on body
#endif
		{
			//the trace hit the vehicle, don't let them get out, just in case
			return false;
		}
		//otherwise, use the trace.endpos
		VectorCopy(m_ExitTrace.endpos, vExitPos);
	}
	return true;
}

static void G_EjectDroidUnit(Vehicle_t* p_veh, qboolean kill)
{
	p_veh->m_pDroidUnit->s.m_iVehicleNum = ENTITYNUM_NONE;
#ifdef _JK2MP
	p_veh->m_pDroidUnit->s.owner = ENTITYNUM_NONE;
#else
	p_veh->m_pDroidUnit->owner = nullptr;
#endif
	//	p_veh->m_pDroidUnit->s.otherentity_num2 = ENTITYNUM_NONE;
#ifdef QAGAME
	{
		gentity_t* droidEnt = (gentity_t*)p_veh->m_pDroidUnit;
		droidEnt->flags &= ~FL_UNDYING;
		droidEnt->r.ownerNum = ENTITYNUM_NONE;
		if (droidEnt->client)
		{
			droidEnt->client->ps.m_iVehicleNum = ENTITYNUM_NONE;
		}
		if (kill)
		{//Kill them, too
			//FIXME: proper origin, MOD and attacker (for credit/death message)?  Get from vehicle?
			G_MuteSound(droidEnt->s.number, CHAN_VOICE);
			G_Damage(droidEnt, nullptr, nullptr, nullptr, droidEnt->s.origin, 10000, 0, MOD_SUICIDE);//FIXME: proper MOD?  Get from vehicle?
		}
	}
#endif
	p_veh->m_pDroidUnit = nullptr;
}

// Eject the pilot from the vehicle.
static bool Eject(Vehicle_t* p_veh, bgEntity_t* pEnt, const qboolean force_eject)
{
	gentity_t* parent;
	vec3_t vExitPos;
#ifndef _JK2MP
	vec3_t vPlayerDir;
#endif
	auto ent = pEnt;
	int firstEjectDir;

#ifdef _JK2MP
	qboolean	taintedRider = qfalse;
	qboolean	deadRider = qfalse;

	if (pEnt == p_veh->m_pDroidUnit)
	{
		G_EjectDroidUnit(p_veh, qfalse);
		return true;
	}

	if (ent)
	{
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
		{
			taintedRider = qtrue;
			parent = (gentity_t*)p_veh->m_pParentEntity;
			goto getItOutOfMe;
		}
		else if (ent->health < 1)
		{
			deadRider = qtrue;
		}
	}
#endif

	// Validate.
	if (!ent)
	{
		return false;
	}
	if (!force_eject)
	{
		if (!(p_veh->m_iBoarding == 0 || p_veh->m_iBoarding == -999 || p_veh->m_iBoarding < -3 && p_veh->m_iBoarding >= -
			9))
		{
#ifdef _JK2MP //I don't care, if he's dead get him off even if he died while boarding
			deadRider = qtrue;
			p_veh->m_iBoarding = 0;
			p_veh->m_bWasBoarding = false;
#else
			return false;
#endif
		}
	}

	parent = p_veh->m_pParentEntity;

	//Try ejecting in every direction
	if (p_veh->m_EjectDir < VEH_EJECT_LEFT)
	{
		p_veh->m_EjectDir = VEH_EJECT_LEFT;
	}
	else if (p_veh->m_EjectDir > VEH_EJECT_BOTTOM)
	{
		p_veh->m_EjectDir = VEH_EJECT_BOTTOM;
	}
	firstEjectDir = p_veh->m_EjectDir;
	while (!VEH_TryEject(p_veh, parent, ent, p_veh->m_EjectDir, vExitPos))
	{
		p_veh->m_EjectDir++;
		if (p_veh->m_EjectDir > VEH_EJECT_BOTTOM)
		{
			p_veh->m_EjectDir = VEH_EJECT_LEFT;
		}
		if (p_veh->m_EjectDir == firstEjectDir)
		{
			//they all failed
#ifdef _JK2MP
			if (!deadRider)
			{ //if he's dead.. just shove him in solid, who cares.
				return false;
			}
#endif
			if (force_eject)
			{
				//we want to always get out, just eject him here
				VectorCopy(ent->currentOrigin, vExitPos);
				break;
			}
			//can't eject
			return false;
		}
	}

	if (p_veh->m_pVehicleInfo->soundOff)
	{
		G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, p_veh->m_pVehicleInfo->soundOff);
	}

	// Move them to the exit position.
	G_SetOrigin(ent, vExitPos);
#ifdef _JK2MP
	VectorCopy(ent->currentOrigin, ent->client->ps.origin);
	trap_LinkEntity(ent);
#else
	gi.linkentity(ent);
#endif

	// If it's the player, stop overrides.
	if (ent->s.number < MAX_CLIENTS)
	{
#ifndef _JK2MP
		cg.overrides.active = 0;
#else

#endif
	}

#ifdef _JK2MP //in MP if someone disconnects on us, we still have to clear our owner
	getItOutOfMe :
#endif

	// If he's the pilot...
	if (p_veh->m_pPilot == ent)
	{
#ifdef _JK2MP
		int j = 0;
#endif

		p_veh->m_pPilot = nullptr;
#ifdef _JK2MP
		parent->r.ownerNum = ENTITYNUM_NONE;
		parent->s.owner = parent->r.ownerNum; //for prediction

		//keep these current angles
		//SetClientViewAngle( parent, p_veh->m_vOrientation );
		memset(&parent->client->pers.cmd, 0, sizeof(usercmd_t));
#else
		parent->owner = nullptr;

		//keep these current angles
		//SetClientViewAngle( parent, p_veh->m_vOrientation );
		memset(&parent->client->usercmd, 0, sizeof(usercmd_t));
#endif
		memset(&p_veh->m_ucmd, 0, sizeof(usercmd_t));

#ifdef _JK2MP //if there are some passengers, promote the first passenger to pilot
		while (j < p_veh->m_iNumPassengers)
		{
			if (p_veh->m_ppPassengers[j])
			{
				int k = 1;
				p_veh->m_pVehicleInfo->SetPilot(p_veh, p_veh->m_ppPassengers[j]);
				parent->r.ownerNum = p_veh->m_ppPassengers[j]->s.number;
				parent->s.owner = parent->r.ownerNum; //for prediction
				parent->client->ps.m_iVehicleNum = p_veh->m_ppPassengers[j]->s.number + 1;

				//rearrange the passenger slots now..
#ifdef QAGAME
				//Server just needs to tell client he's not a passenger anymore
				if (((gentity_t*)p_veh->m_ppPassengers[j])->client)
				{
					((gentity_t*)p_veh->m_ppPassengers[j])->client->ps.generic1 = 0;
				}
#endif
				p_veh->m_ppPassengers[j] = nullptr;
				while (k < p_veh->m_iNumPassengers)
				{
					if (!p_veh->m_ppPassengers[k - 1])
					{ //move down
						p_veh->m_ppPassengers[k - 1] = p_veh->m_ppPassengers[k];
						p_veh->m_ppPassengers[k] = nullptr;
#ifdef QAGAME
						//Server just needs to tell client which passenger he is
						if (((gentity_t*)p_veh->m_ppPassengers[k - 1])->client)
						{
							((gentity_t*)p_veh->m_ppPassengers[k - 1])->client->ps.generic1 = k;
						}
#endif
					}
					k++;
				}
				p_veh->m_iNumPassengers--;

				break;
			}
			j++;
		}
#endif
	}
	else if (ent == p_veh->m_pOldPilot)
	{
		p_veh->m_pOldPilot = nullptr;
	}

#ifdef _JK2MP //I hate adding these!
	if (!taintedRider)
	{
#endif
		if (p_veh->m_pVehicleInfo->hideRider)
		{
			p_veh->m_pVehicleInfo->UnGhost(p_veh, ent);
		}
#ifdef _JK2MP
	}
#endif

	// If the vehicle now has no pilot...
	if (p_veh->m_pPilot == nullptr)
	{
#ifdef _JK2MP
		parent->client->ps.loopSound = parent->s.loopSound = 0;
#else
		parent->s.loopSound = 0;
#endif
		// Completely empty vehicle...?
#ifdef _JK2MP
		parent->client->ps.m_iVehicleNum = 0;
#else
		parent->s.m_iVehicleNum = 0;
#endif
	}

#ifdef _JK2MP
	if (taintedRider)
	{ //you can go now
		p_veh->m_iBoarding = level.time + 1000;
		return true;
	}
#endif

	// Client not in a vehicle.
#ifdef _JK2MP
	ent->client->ps.m_iVehicleNum = 0;
	ent->r.ownerNum = ENTITYNUM_NONE;
	ent->s.owner = ent->r.ownerNum; //for prediction

	ent->client->ps.viewangles[PITCH] = 0.0f;
	ent->client->ps.viewangles[ROLL] = 0.0f;
	ent->client->ps.viewangles[YAW] = p_veh->m_vOrientation[YAW];
	SetClientViewAngle(ent, ent->client->ps.viewangles);

	if (ent->client->solidHack)
	{
		ent->client->solidHack = 0;
		ent->r.contents = CONTENTS_BODY;
	}
#else
	ent->owner = nullptr;
#endif
	ent->s.m_iVehicleNum = 0;

	// Jump out.
	/*	if ( ent->client->ps.velocity[2] < JUMP_VELOCITY )
		{
			ent->client->ps.velocity[2] = JUMP_VELOCITY;
		}
		else
		{
			ent->client->ps.velocity[2] += JUMP_VELOCITY;
		}*/

		// Make sure entity is facing the direction it got off at.
#ifndef _JK2MP
	VectorCopy(p_veh->m_vOrientation, vPlayerDir);
	vPlayerDir[ROLL] = 0;
	SetClientViewAngle(ent, vPlayerDir);
#endif

	//if was using vehicle weapon, remove it and switch to normal weapon when hop out...
	if (ent->client->ps.weapon == WP_NONE)
	{
		//FIXME: check against this vehicle's gun from the g_vehicleInfo table
		//remove the vehicle's weapon from me
		//ent->client->ps.stats[STAT_WEAPONS] &= ~( 1 << WP_EMPLACED_GUN );
		//ent->client->ps.ammo[weaponData[WP_EMPLACED_GUN].ammoIndex] = 0;//maybe store this ammo on the vehicle before clearing it?
		//switch back to a normal weapon we're carrying

		//FIXME: store the weapon we were using when we got on and restore that when hop off
		/*		if ( (ent->client->ps.stats[STAT_WEAPONS]&(1<<WP_SABER)) )
				{
					CG_ChangeWeapon( WP_SABER );
				}
				else
				{//go through all weapons and switch to highest held
					for ( int checkWp = WP_NUM_WEAPONS-1; checkWp > WP_NONE; checkWp-- )
					{
						if ( (ent->client->ps.stats[STAT_WEAPONS]&(1<<checkWp)) )
						{
							CG_ChangeWeapon( checkWp );
						}
					}
					if ( checkWp == WP_NONE )
					{
						CG_ChangeWeapon( WP_NONE );
					}
				}*/
	}
	else
	{
		//FIXME: if they have their saber out:
		//if dualSabers, add the second saber into the left hand
		//saber[0] has more than one blade, turn them all on
		//NOTE: this is because you're only allowed to use your first saber's first blade on a vehicle
	}
	PM_SetLegsAnimTimer(ent, &ent->client->ps.legsAnimTimer, 0);
	PM_SetTorsoAnimTimer(ent, &ent->client->ps.torsoAnimTimer, 0);

	// Set how long until this vehicle can be boarded again.
	p_veh->m_iBoarding = level.time + 1000;

	return true;
}

// Eject all the inhabitants of this vehicle.
static bool EjectAll(Vehicle_t* p_veh)
{
	// TODO: Setup a default escape for ever vehicle type.

	p_veh->m_EjectDir = VEH_EJECT_TOP;
	// Make sure no other boarding calls exist. We MUST exit.
	p_veh->m_iBoarding = 0;
	p_veh->m_bWasBoarding = false;

	// Throw them off.
	if (p_veh->m_pPilot)
	{
#ifdef QAGAME
		gentity_t* pilot = (gentity_t*)p_veh->m_pPilot;
#endif
		p_veh->m_pVehicleInfo->Eject(p_veh, p_veh->m_pPilot, qtrue);
#ifdef QAGAME
		if (p_veh->m_pVehicleInfo->killRiderOnDeath && pilot)
		{//Kill them, too
			//FIXME: proper origin, MOD and attacker (for credit/death message)?  Get from vehicle?
			G_MuteSound(pilot->s.number, CHAN_VOICE);
			G_Damage(pilot, player, player, nullptr, pilot->s.origin, 10000, 0, MOD_SUICIDE);
		}
#endif
	}
	if (p_veh->m_pOldPilot)
	{
#ifdef QAGAME
		gentity_t* pilot = (gentity_t*)p_veh->m_pOldPilot;
#endif
		p_veh->m_pVehicleInfo->Eject(p_veh, p_veh->m_pOldPilot, qtrue);
#ifdef QAGAME
		if (p_veh->m_pVehicleInfo->killRiderOnDeath && pilot)
		{//Kill them, too
			//FIXME: proper origin, MOD and attacker (for credit/death message)?  Get from vehicle?
			G_MuteSound(pilot->s.number, CHAN_VOICE);
			G_Damage(pilot, player, player, nullptr, pilot->s.origin, 10000, 0, MOD_SUICIDE);
		}
#endif
	}

	if (p_veh->m_pDroidUnit)
	{
		G_EjectDroidUnit(p_veh, p_veh->m_pVehicleInfo->killRiderOnDeath);
	}

	return true;
}

// Start a delay until the vehicle explodes.
static void StartDeathDelay(Vehicle_t* p_veh, const int iDelayTimeOverride)
{
	auto parent = p_veh->m_pParentEntity;

	if (iDelayTimeOverride)
	{
		p_veh->m_iDieTime = level.time + iDelayTimeOverride;
	}
	else
	{
		p_veh->m_iDieTime = level.time + p_veh->m_pVehicleInfo->explosionDelay;
	}

#ifdef _JK2MP
	if (p_veh->m_pVehicleInfo->flammable)
	{
		parent->client->ps.loopSound = parent->s.loopSound = G_SoundIndex("sound/vehicles/common/fire_lp.wav");
	}
#else
	// Armor Gone Effects (Fire)
	//---------------------------
	if (p_veh->m_pVehicleInfo->iArmorGoneFX)
	{
		if (!(p_veh->m_ulFlags & VEH_ARMORGONE) && p_veh->m_iArmor <= 0)
		{
			p_veh->m_ulFlags |= VEH_ARMORGONE;
			G_PlayEffect(p_veh->m_pVehicleInfo->iArmorGoneFX, parent->playerModel, parent->crotchBolt, parent->s.number,
				parent->currentOrigin, 1, qtrue);
			parent->s.loopSound = G_SoundIndex("sound/vehicles/common/fire_lp.wav");
		}
	}
#endif
}

// Decide whether to explode the vehicle or not.
static void DeathUpdate(Vehicle_t* p_veh)
{
	auto parent = p_veh->m_pParentEntity;

	if (level.time >= p_veh->m_iDieTime)
	{
		// If the vehicle is not empty.
		if (p_veh->m_pVehicleInfo->Inhabited(p_veh))
		{
#ifndef _JK2MP
			if (p_veh->m_pPilot)
			{
				p_veh->m_pPilot->client->noRagTime = -1; // no ragdoll for you
			}
#endif

			p_veh->m_pVehicleInfo->EjectAll(p_veh);
#ifdef _JK2MP
			if (p_veh->m_pVehicleInfo->Inhabited(p_veh))
			{ //if we've still got people in us, just kill the bastards
				if (p_veh->m_pPilot)
				{
					//FIXME: does this give proper credit to the enemy who shot you down?
					G_Damage((gentity_t*)p_veh->m_pPilot, (gentity_t*)p_veh->m_pParentEntity, (gentity_t*)p_veh->m_pParentEntity,
						nullptr, p_veh->m_pParentEntity->playerState->origin, 999, DAMAGE_NO_PROTECTION, MOD_EXPLOSIVE);
				}
				if (p_veh->m_iNumPassengers)
				{
					int i;

					for (i = 0; i < p_veh->m_pVehicleInfo->maxPassengers; i++)
					{
						if (p_veh->m_ppPassengers[i])
						{
							//FIXME: does this give proper credit to the enemy who shot you down?
							G_Damage((gentity_t*)p_veh->m_ppPassengers[i], (gentity_t*)p_veh->m_pParentEntity, (gentity_t*)p_veh->m_pParentEntity,
								nullptr, p_veh->m_pParentEntity->playerState->origin, 999, DAMAGE_NO_PROTECTION, MOD_EXPLOSIVE);
						}
					}
				}
			}
#endif
		}

		if (!p_veh->m_pVehicleInfo->Inhabited(p_veh))
		{
			//explode now as long as we managed to kick everyone out
			vec3_t bottom;
			trace_t trace;

#ifndef _JK2MP
			// Kill All Client Side Looping Effects
			//--------------------------------------
			if (p_veh->m_pVehicleInfo->iExhaustFX)
			{
				for (int i = 0; i < MAX_VEHICLE_EXHAUSTS && p_veh->m_iExhaustTag[i] != -1; i++)
				{
					G_StopEffect(p_veh->m_pVehicleInfo->iExhaustFX, parent->playerModel, p_veh->m_iExhaustTag[i],
						parent->s.number);
				}
			}
			if (p_veh->m_pVehicleInfo->iArmorLowFX)
			{
				G_StopEffect(p_veh->m_pVehicleInfo->iArmorLowFX, parent->playerModel, parent->crotchBolt,
					parent->s.number);
			}
			if (p_veh->m_pVehicleInfo->iArmorGoneFX)
			{
				G_StopEffect(p_veh->m_pVehicleInfo->iArmorGoneFX, parent->playerModel, parent->crotchBolt,
					parent->s.number);
			}
			//--------------------------------------
#endif
			if (p_veh->m_pVehicleInfo->iExplodeFX)
			{
				vec3_t fxAng = { 0.0f, -1.0f, 0.0f };
				G_PlayEffect(p_veh->m_pVehicleInfo->iExplodeFX, parent->currentOrigin, fxAng);

				//trace down and place mark
				VectorCopy(parent->currentOrigin, bottom);
				bottom[2] -= 80;
				G_VehicleTrace(&trace, parent->currentOrigin, vec3_origin, vec3_origin, bottom, parent->s.number,
					CONTENTS_SOLID);
				if (trace.fraction < 1.0f)
				{
					VectorCopy(trace.endpos, bottom);
					bottom[2] += 2;
#ifdef _JK2MP
					VectorSet(fxAng, -90.0f, 0.0f, 0.0f);
					G_PlayEffectID(G_EffectIndex("ships/ship_explosion_mark"), trace.endpos, fxAng);
#else
					G_PlayEffect("ships/ship_explosion_mark", trace.endpos);
#endif
				}
			}

			parent->takedamage = qfalse; //so we don't recursively damage ourselves
			if (p_veh->m_pVehicleInfo->explosionRadius > 0 && p_veh->m_pVehicleInfo->explosionDamage > 0)
			{
				vec3_t lMaxs;
				vec3_t lMins;
				VectorCopy(parent->mins, lMins);
				lMins[2] = -4; //to keep it off the ground a *little*
				VectorCopy(parent->maxs, lMaxs);
				VectorCopy(parent->currentOrigin, bottom);
				bottom[2] += parent->mins[2] - 32;
				G_VehicleTrace(&trace, parent->currentOrigin, lMins, lMaxs, bottom, parent->s.number, CONTENTS_SOLID);
#ifdef _JK2MP
				G_RadiusDamage(trace.endpos, nullptr, p_veh->m_pVehicleInfo->explosionDamage, p_veh->m_pVehicleInfo->explosionRadius, nullptr, nullptr, MOD_EXPLOSIVE);//FIXME: extern damage and radius or base on fuel
#else
				G_RadiusDamage(trace.endpos, player, p_veh->m_pVehicleInfo->explosionDamage,
					p_veh->m_pVehicleInfo->explosionRadius, nullptr, MOD_EXPLOSIVE);
				//FIXME: extern damage and radius or base on fuel
#endif
			}

#ifdef _JK2MP
			parent->think = G_FreeEntity;
#else
			parent->e_ThinkFunc = thinkF_G_FreeEntity;
#endif
			parent->nextthink = level.time + FRAMETIME;
		}
	}
#ifndef _JK2MP
	else
	{
		//let everyone around me know I'm gonna blow!
		if (!Q_irand(0, 10))
		{
			//not so often...
			AddSoundEvent(parent, parent->currentOrigin, 512, AEL_DANGER);
			AddSightEvent(parent, parent->currentOrigin, 512, AEL_DANGER, 100);
		}
	}
#endif
}

// Register all the assets used by this vehicle.
static void RegisterAssets(Vehicle_t* p_veh)
{
}

extern void ChangeWeapon(const gentity_t* ent, int new_weapon);

// Initialize the vehicle.
static bool Initialize(Vehicle_t* p_veh)
{
	auto parent = p_veh->m_pParentEntity;
	int i;

	if (!parent || !parent->client)
		return false;

#ifdef _JK2MP
	parent->client->ps.m_iVehicleNum = 0;
#endif
	parent->s.m_iVehicleNum = 0;
	{
		p_veh->m_iArmor = p_veh->m_pVehicleInfo->armor;
		parent->client->pers.maxHealth = parent->client->ps.stats[STAT_MAX_HEALTH] = parent->NPC->stats.health = parent
			->health = parent->client->ps.stats[STAT_HEALTH] = p_veh->m_iArmor;
		p_veh->m_iShields = p_veh->m_pVehicleInfo->shields;
		parent->client->ps.stats[STAT_ARMOR] = p_veh->m_iShields;
	}
	parent->mass = p_veh->m_pVehicleInfo->mass;
	//initialize the ammo to max
	for (i = 0; i < MAX_VEHICLE_WEAPONS; i++)
	{
		parent->client->ps.ammo[i] = p_veh->weaponStatus[i].ammo = p_veh->m_pVehicleInfo->weapon[i].ammoMax;
	}
	for (i = 0; i < MAX_VEHICLE_TURRETS; i++)
	{
		p_veh->turretStatus[i].nextMuzzle = p_veh->m_pVehicleInfo->turret[i].iMuzzle[i] - 1;
		parent->client->ps.ammo[MAX_VEHICLE_WEAPONS + i] = p_veh->turretStatus[i].ammo = p_veh->m_pVehicleInfo->turret[i].
			iAmmoMax;
		if (p_veh->m_pVehicleInfo->turret[i].bAI)
		{
			//they're going to be finding enemies, init this to NONE
			p_veh->turretStatus[i].enemyEntNum = ENTITYNUM_NONE;
		}
	}
	//begin stopped...?
	parent->client->ps.speed = 0;

	VectorClear(p_veh->m_vOrientation);
	p_veh->m_vOrientation[YAW] = parent->s.angles[YAW];

#ifdef _JK2MP
	if (p_veh->m_pVehicleInfo->gravity &&
		p_veh->m_pVehicleInfo->gravity != g_gravity.value)
	{//not normal gravity
		if (parent->NPC)
		{
			parent->NPC->aiFlags |= NPCAI_CUSTOM_GRAVITY;
		}
		parent->client->ps.gravity = p_veh->m_pVehicleInfo->gravity;
	}
#else
	if (p_veh->m_pVehicleInfo->gravity &&
		p_veh->m_pVehicleInfo->gravity != g_gravity->value)
	{
		//not normal gravity
		parent->svFlags |= SVF_CUSTOM_GRAVITY;
		parent->client->ps.gravity = p_veh->m_pVehicleInfo->gravity;
	}
#endif
	{
		p_veh->m_ulFlags = 0;
	}
	p_veh->m_fTimeModifier = 1.0f;
	p_veh->m_iBoarding = 0;
	p_veh->m_bWasBoarding = false;
	p_veh->m_pOldPilot = nullptr;
	VectorClear(p_veh->m_vBoardingVelocity);
	p_veh->m_pPilot = nullptr;
	memset(&p_veh->m_ucmd, 0, sizeof(usercmd_t));
	p_veh->m_iDieTime = 0;
	p_veh->m_EjectDir = VEH_EJECT_LEFT;
	memset(p_veh->m_iExhaustTag, -1, sizeof(int) * MAX_VEHICLE_EXHAUSTS);
	memset(p_veh->m_iMuzzleTag, -1, sizeof(int) * MAX_VEHICLE_MUZZLES);
	// FIXME! Use external values read from the vehicle data file!
#ifndef _JK2MP //blargh, fixme
	memset(p_veh->m_Muzzles, 0, sizeof(Muzzle) * MAX_VEHICLE_MUZZLES);
#endif
	p_veh->m_iDroidUnitTag = -1;

	//initialize to blaster, just since it's a basic weapon and there's no lightsaber crap...?
	parent->client->ps.weapon = WP_BLASTER;
	parent->client->ps.weaponstate = WEAPON_READY;
	parent->client->ps.weapons[WP_BLASTER] = 1;

	//Initialize to landed (wings closed, gears down) animation
	int iFlags = SETANIM_FLAG_NORMAL, iBlend = 300;

	NPC_SetAnim(p_veh->m_pParentEntity, SETANIM_BOTH, BOTH_VS_IDLE, iFlags, iBlend);

	return true;
}

// Like a think or move command, this updates various vehicle properties.
#ifdef _JK2MP
void G_VehicleDamageBoxSizing(Vehicle_t* p_veh); //declared below
#endif
static bool Update(Vehicle_t* p_veh, const usercmd_t* pUmcd)
{
	auto parent = p_veh->m_pParentEntity;
	//static float fMod = 1000.0f / 60.0f;
	vec3_t vVehAngles;
	int i;
	int prevSpeed;
	int nextSpeed;
	int curTime;
	int halfMaxSpeed;
	playerState_t* parent_ps;
	constexpr qboolean link_held = qfalse;

#ifdef _JK2MP
	parent_ps = p_veh->m_pParentEntity->playerState;
#else
	parent_ps = &p_veh->m_pParentEntity->client->ps;
#endif

#ifndef _JK2MP//SP
	curTime = level.time;
#elif QAGAME//MP GAME
	curTime = level.time;
#elif CGAME//MP CGAME
	//FIXME: pass in ucmd?  Not sure if this is reliable...
	curTime = pm->cmd.serverTime;
#endif

	//increment the ammo for all rechargeable weapons
	for (i = 0; i < MAX_VEHICLE_WEAPONS; i++)
	{
		if (p_veh->m_pVehicleInfo->weapon[i].ID > VEH_WEAPON_BASE //have a weapon in this slot
			&& p_veh->m_pVehicleInfo->weapon[i].ammoRechargeMS //its ammo is rechargable
			&& p_veh->weaponStatus[i].ammo < p_veh->m_pVehicleInfo->weapon[i].ammoMax //its ammo is below max
			&& pUmcd->serverTime - p_veh->weaponStatus[i].lastAmmoInc >= p_veh->m_pVehicleInfo->weapon[i].ammoRechargeMS)
			//enough time has passed
		{
			//add 1 to the ammo
			p_veh->weaponStatus[i].lastAmmoInc = pUmcd->serverTime;
			p_veh->weaponStatus[i].ammo++;
			//NOTE: in order to send the vehicle's ammo info to the client, we copy the ammo into the first 2 ammo slots on the vehicle NPC's client->ps.ammo array
			if (parent && parent->client)
			{
				parent->client->ps.ammo[i] = p_veh->weaponStatus[i].ammo;
			}
		}
	}
	for (i = 0; i < MAX_VEHICLE_TURRETS; i++)
	{
		if (p_veh->m_pVehicleInfo->turret[i].iWeapon > VEH_WEAPON_BASE //have a weapon in this slot
			&& p_veh->m_pVehicleInfo->turret[i].iAmmoRechargeMS //its ammo is rechargable
			&& p_veh->turretStatus[i].ammo < p_veh->m_pVehicleInfo->turret[i].iAmmoMax //its ammo is below max
			&& pUmcd->serverTime - p_veh->turretStatus[i].lastAmmoInc >= p_veh->m_pVehicleInfo->turret[i].iAmmoRechargeMS)
			//enough time has passed
		{
			//add 1 to the ammo
			p_veh->turretStatus[i].lastAmmoInc = pUmcd->serverTime;
			p_veh->turretStatus[i].ammo++;
			//NOTE: in order to send the vehicle's ammo info to the client, we copy the ammo into the first 2 ammo slots on the vehicle NPC's client->ps.ammo array
			if (parent && parent->client)
			{
				parent->client->ps.ammo[MAX_VEHICLE_WEAPONS + i] = p_veh->turretStatus[i].ammo;
			}
		}
	}

	//increment shields for rechargable shields
	if (p_veh->m_pVehicleInfo->shieldRechargeMS
		&& parent_ps->stats[STAT_ARMOR] > 0 //still have some shields left
		&& parent_ps->stats[STAT_ARMOR] < p_veh->m_pVehicleInfo->shields //its below max
		&& pUmcd->serverTime - p_veh->lastShieldInc >= p_veh->m_pVehicleInfo->shieldRechargeMS) //enough time has passed
	{
		parent_ps->stats[STAT_ARMOR]++;
		if (parent_ps->stats[STAT_ARMOR] > p_veh->m_pVehicleInfo->shields)
		{
			parent_ps->stats[STAT_ARMOR] = p_veh->m_pVehicleInfo->shields;
		}
		p_veh->m_iShields = parent_ps->stats[STAT_ARMOR];
	}

#ifdef _JK2MP //sometimes this gets out of whack, probably init'ing
	if (parent && parent->r.ownerNum != parent->s.owner)
	{
		parent->s.owner = parent->r.ownerNum;
	}

	//keep the PS value in sync. set it up here in case we return below at some point.
	if (p_veh->m_iBoarding)
	{
		parent->client->ps.vehBoarding = qtrue;
	}
	else
	{
		parent->client->ps.vehBoarding = qfalse;
	}
#endif

	// See whether this vehicle should be dieing or dead.
	if (p_veh->m_iDieTime != 0
#ifndef _JK2MP //sometimes this gets out of whack, probably init'ing
		|| parent->health <= 0
#endif
		)
	{
		//NOTE!!!: This HAS to be consistent with cgame!!!
		// Keep track of the old orientation.
		VectorCopy(p_veh->m_vOrientation, p_veh->m_vPrevOrientation);

		// Process the orient commands.
		p_veh->m_pVehicleInfo->ProcessOrientCommands(p_veh);
		// Need to copy orientation to our entity's viewangles so that it renders at the proper angle and currentAngles is correct.
		SetClientViewAngle(parent, p_veh->m_vOrientation);
		if (p_veh->m_pPilot)
		{
			SetClientViewAngle(p_veh->m_pPilot, p_veh->m_vOrientation);
		}

		// Process the move commands.
		p_veh->m_pVehicleInfo->ProcessMoveCommands(p_veh);

		// Setup the move direction.
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			AngleVectors(p_veh->m_vOrientation, parent->client->ps.moveDir, nullptr, nullptr);
		}
		else
		{
			VectorSet(vVehAngles, 0, p_veh->m_vOrientation[YAW], 0);
			AngleVectors(vVehAngles, parent->client->ps.moveDir, nullptr, nullptr);
		}
		p_veh->m_pVehicleInfo->DeathUpdate(p_veh);
		return false;
	}
	// Vehicle dead!

#ifdef _JK2MP
	else if (parent->health <= 0)
	{
		// Instant kill.
		if (p_veh->m_pVehicleInfo->type == VH_FIGHTER &&
			p_veh->m_iLastImpactDmg > 500)
		{ //explode instantly in inferno-y death
			p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, -1/* -1 causes instant death */);
		}
		else
		{
			p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, 0);
		}
		p_veh->m_pVehicleInfo->DeathUpdate(p_veh);
		return false;
	}
#endif

#ifdef _JK2MP //special check in case someone disconnects/dies while boarding
#ifdef QAGAME
	if (parent->spawnflags & 1)
	{
		if (p_veh->m_pPilot || !p_veh->m_bHasHadPilot)
		{
			if (p_veh->m_pPilot && !p_veh->m_bHasHadPilot)
			{
				p_veh->m_bHasHadPilot = qtrue;
				p_veh->m_iPilotLastIndex = p_veh->m_pPilot->s.number;
			}
			p_veh->m_iPilotTime = level.time + parent->damage;
		}
		else if (p_veh->m_iPilotTime)
		{ //die
			gentity_t* oldPilot = &g_entities[p_veh->m_iPilotLastIndex];

			if (!oldPilot->inuse || !oldPilot->client ||
				oldPilot->client->pers.connected != CON_CONNECTED)
			{ //no longer in the game?
				G_Damage(parent, parent, parent, nullptr, parent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
			}
			else
			{
				vec3_t v;
				VectorSubtract(parent->client->ps.origin, oldPilot->client->ps.origin, v);

				if (VectorLength(v) < parent->speed)
				{ //they are still within the minimum distance to their vehicle
					p_veh->m_iPilotTime = level.time + parent->damage;
				}
				else if (p_veh->m_iPilotTime < level.time)
				{ //dying time
					G_Damage(parent, parent, parent, nullptr, parent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
				}
			}
		}
	}
#endif
#else
	if (parent->spawnflags & 1)
	{
		//NOTE: in SP, this actually just checks LOS to the Player
		if (p_veh->m_iPilotTime < level.time)
		{
			//do another check?
			if (!player || G_ClearLineOfSight(p_veh->m_pParentEntity->currentOrigin, player->currentOrigin,
				p_veh->m_pParentEntity->s.number, MASK_OPAQUE))
			{
				p_veh->m_iPilotTime = level.time + p_veh->m_pParentEntity->endFrame;
			}
		}
		if (p_veh->m_iPilotTime && p_veh->m_iPilotTime < level.time)
		{
			//die
			//FIXME: does this give proper credit to the enemy who shot you down?
			G_Damage(parent, player, player, nullptr, parent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION,
				MOD_SUICIDE);
		}
	}
#endif

#ifndef _JK2MP
	//	if (level.time<p_veh->m_iTurboTime || p_veh->m_pVehicleInfo->type==VH_ANIMAL)
	// always knock guys around now...
	{
		vec3_t dir;
		vec3_t projectedPosition;
		VectorCopy(parent->client->ps.velocity, dir);
		VectorMA(parent->currentOrigin, 0.1f, dir, projectedPosition);

		float force = VectorNormalize(dir);
		force /= 10.0f;
		if (force > 30.0f)
		{
			trace_t tr;
			G_VehicleTrace(&tr, parent->currentOrigin, parent->mins, parent->maxs, projectedPosition, parent->s.number,
				CONTENTS_BODY);
			if (tr.fraction < 1.0f &&
				!tr.allsolid &&
				!tr.startsolid &&
				tr.entityNum != ENTITYNUM_NONE &&
				tr.entityNum != ENTITYNUM_WORLD &&
				(level.time < p_veh->m_iTurboTime || Q_irand(0, 3) == 0))
			{
				gentity_t* other = &g_entities[tr.entityNum];
				if (other && other->client && !other->s.m_iVehicleNum)
				{
					G_Throw(other, dir, force / 10.0f);
					G_Knockdown(other, parent, dir, force, qtrue);
					G_Damage(other, player, player, parent->client->ps.velocity, parent->currentOrigin, force,
						DAMAGE_NO_ARMOR | DAMAGE_EXTRA_KNOCKBACK, MOD_IMPACT);
				}
			}
		}
	}
#endif

#ifdef _JK2MP //special check in case someone disconnects/dies while boarding
	if (p_veh->m_iBoarding != 0)
	{
		pilotEnt = (gentity_t*)p_veh->m_pPilot;
		if (pilotEnt)
		{
			if (!pilotEnt->inuse || !pilotEnt->client || pilotEnt->health <= 0 ||
				pilotEnt->client->pers.connected != CON_CONNECTED)
			{
				p_veh->m_pVehicleInfo->Eject(p_veh, p_veh->m_pPilot, qtrue);
				return false;
			}
		}
	}
#endif

	// If we're not done mounting, can't do anything.
	if (p_veh->m_iBoarding != 0)
	{
		if (!p_veh->m_bWasBoarding)
		{
			VectorCopy(parent_ps->velocity, p_veh->m_vBoardingVelocity);
			p_veh->m_bWasBoarding = true;
		}

		// See if we're done boarding.
		if (p_veh->m_iBoarding > -1 && p_veh->m_iBoarding <= level.time)
		{
			p_veh->m_bWasBoarding = false;
			p_veh->m_iBoarding = 0;
		}
		else
		{
#ifdef _JK2MP
			goto maintainSelfDuringBoarding;
#else
			return false;
#endif
		}
	}

	parent = p_veh->m_pParentEntity;

	// Validate vehicle.
	if (!parent || !parent->client || parent->health <= 0)
		return false;

	// See if any of the riders are dead and if so kick em off.
	if (p_veh->m_pPilot)
	{
		gentity_t* pilotEnt = p_veh->m_pPilot;

#ifdef _JK2MP
		if (!pilotEnt->inuse || !pilotEnt->client || pilotEnt->health <= 0 ||
			pilotEnt->client->pers.connected != CON_CONNECTED)
#else
		if (pilotEnt->health <= 0)
#endif
		{
			p_veh->m_pVehicleInfo->Eject(p_veh, p_veh->m_pPilot, qtrue);
		}
	}

#ifdef _JK2MP
	// Copy over the commands for local storage.
	memcpy(&parent->client->pers.cmd, &p_veh->m_ucmd, sizeof(usercmd_t));
	p_veh->m_ucmd.buttons &= ~(BUTTON_TALK);//|BUTTON_GESTURE); //don't want some of these buttons
#else
	// Copy over the commands for local storage.
	memcpy(&p_veh->m_ucmd, pUmcd, sizeof(usercmd_t));
	memcpy(&parent->client->pers.lastCommand, pUmcd, sizeof(usercmd_t));
#endif

	/*
	// Update time modifier.
	p_veh->m_fTimeModifier = p_veh->m_ucmd.serverTime - parent->client->ps.commandTime;
	//sanity check
	if ( p_veh->m_fTimeModifier < 1 )
	{
		p_veh->m_fTimeModifier = 1;
	}
	else if ( p_veh->m_fTimeModifier > 200 )
	{
		p_veh->m_fTimeModifier = 200;
	}
	//normalize to 1.0f at 20fps
	p_veh->m_fTimeModifier = p_veh->m_fTimeModifier / fMod;
	*/

	//check for weapon linking/unlinking command
	for (i = 0; i < MAX_VEHICLE_WEAPONS; i++)
	{
		//HMM... can't get a seperate command for each weapon, so do them all...?
		if (p_veh->m_pVehicleInfo->weapon[i].linkable == 2)
		{
			//always linked
			//FIXME: just set this once, on Initialize...?
			if (!p_veh->weaponStatus[i].linked)
			{
				p_veh->weaponStatus[i].linked = qtrue;
			}
		}
	}
	{
		//so we don't hold it down and toggle it back and forth
		p_veh->linkWeaponToggleHeld = qfalse;
	}
#ifdef _JK2MP
	//now pass it over the network so cgame knows about it
	//NOTE: SP can just cheat and check directly
	parent_ps->vehWeaponsLinked = qfalse;
	for (i = 0; i < MAX_VEHICLE_WEAPONS; i++)
	{//HMM... can't get a seperate command for each weapon, so do them all...?
		if (p_veh->weaponStatus[i].linked)
		{
			parent_ps->vehWeaponsLinked = qtrue;
		}
	}
#endif

#ifdef _JK2MP
	maintainSelfDuringBoarding :

	if (p_veh->m_pPilot && p_veh->m_pPilot->playerState && p_veh->m_iBoarding != 0)
	{
		VectorCopy(p_veh->m_vOrientation, p_veh->m_pPilot->playerState->viewangles);
		p_veh->m_ucmd.buttons = 0;
		p_veh->m_ucmd.forwardmove = 0;
		p_veh->m_ucmd.rightmove = 0;
		p_veh->m_ucmd.upmove = 0;
	}
#endif

	// Keep track of the old orientation.
	VectorCopy(p_veh->m_vOrientation, p_veh->m_vPrevOrientation);

	// Process the orient commands.
	p_veh->m_pVehicleInfo->ProcessOrientCommands(p_veh);
	// Need to copy orientation to our entity's viewangles so that it renders at the proper angle and currentAngles is correct.
	SetClientViewAngle(parent, p_veh->m_vOrientation);
	if (p_veh->m_pPilot)
	{
#ifdef _JK2MP
		if (!BG_UnrestrainedPitchRoll(p_veh->m_pPilot->playerState, p_veh))
		{
			vec3_t newVAngle;
			newVAngle[PITCH] = p_veh->m_pPilot->playerState->viewangles[PITCH];
			newVAngle[YAW] = p_veh->m_pPilot->playerState->viewangles[YAW];
			newVAngle[ROLL] = p_veh->m_vOrientation[ROLL];
			SetClientViewAngle((gentity_t*)p_veh->m_pPilot, newVAngle);
		}
#else
		if (!BG_UnrestrainedPitchRoll(&p_veh->m_pPilot->client->ps, p_veh))
		{
			SetClientViewAngle(p_veh->m_pPilot, p_veh->m_vOrientation);
		}
#endif
	}

	// Process the move commands.
	prevSpeed = parent_ps->speed;
	p_veh->m_pVehicleInfo->ProcessMoveCommands(p_veh);
	nextSpeed = parent_ps->speed;
	halfMaxSpeed = p_veh->m_pVehicleInfo->speedMax * 0.5f;

	// Shifting Sounds
	//=====================================================================
	if (p_veh->m_iTurboTime < curTime &&
		p_veh->m_iSoundDebounceTimer < curTime &&
		(nextSpeed > prevSpeed && nextSpeed > halfMaxSpeed && prevSpeed < halfMaxSpeed || nextSpeed > halfMaxSpeed &&
			!Q_irand(0, 1000))
		)
	{
		int shift_sound = Q_irand2(1, 10);
		switch (shift_sound)
		{
		case 1:
			shift_sound = p_veh->m_pVehicleInfo->soundShift1;
			break;
		case 2:
			shift_sound = p_veh->m_pVehicleInfo->soundShift2;
			break;
		case 3:
			shift_sound = p_veh->m_pVehicleInfo->soundShift3;
			break;
		case 4:
			shift_sound = p_veh->m_pVehicleInfo->soundShift4;
			break;
		case 5:
			shift_sound = p_veh->m_pVehicleInfo->soundShift5;
			break;
		case 6:
			shift_sound = p_veh->m_pVehicleInfo->soundShift6;
			break;
		case 7:
			shift_sound = p_veh->m_pVehicleInfo->soundShift7;
			break;
		case 8:
			shift_sound = p_veh->m_pVehicleInfo->soundShift8;
			break;
		case 9:
			shift_sound = p_veh->m_pVehicleInfo->soundShift9;
			break;
		case 10:
			shift_sound = p_veh->m_pVehicleInfo->soundShift10;
			break;
		default:;
		}
		if (shift_sound)
		{
			p_veh->m_iSoundDebounceTimer = curTime + Q_irand(1000, 4000);
			G_SoundIndexOnEnt(p_veh->m_pParentEntity, CHAN_AUTO, shift_sound);
		}
	}
	//=====================================================================

	// Setup the move direction.
	if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
	{
		AngleVectors(p_veh->m_vOrientation, parent->client->ps.moveDir, nullptr, nullptr);
	}
	else
	{
		VectorSet(vVehAngles, 0, p_veh->m_vOrientation[YAW], 0);
		AngleVectors(vVehAngles, parent->client->ps.moveDir, nullptr, nullptr);
	}

#ifdef _JK2MP
	if (p_veh->m_pVehicleInfo->surfDestruction)
	{
		if (p_veh->m_iRemovedSurfaces)
		{
			gentity_t* killer = parent;
			G_VehicleDamageBoxSizing(p_veh);

			//damage him constantly if any chunks are currently taken off
			if (parent->client->ps.otherKiller < ENTITYNUM_WORLD &&
				parent->client->ps.otherKillerTime > level.time)
			{
				gentity_t* potentialKiller = &g_entities[parent->client->ps.otherKiller];

				if (potentialKiller->inuse && potentialKiller->client)
				{ //he's valid I guess
					killer = potentialKiller;
				}
			}
			//FIXME: aside from bypassing shields, maybe set m_iShields to 0, too... ?
			G_Damage(parent, killer, killer, nullptr, parent->client->ps.origin, Q_irand(2, 5), DAMAGE_NO_PROTECTION | DAMAGE_NO_ARMOR, MOD_SUICIDE);
		}

		//make sure playerstate value stays in sync
		parent->client->ps.vehSurfaces = p_veh->m_iRemovedSurfaces;
	}
#endif

#ifdef _JK2MP
	//keep the PS value in sync
	if (p_veh->m_iBoarding)
	{
		parent->client->ps.vehBoarding = qtrue;
	}
	else
	{
		parent->client->ps.vehBoarding = qfalse;
	}
#endif

#ifndef _JK2MP
	// Make sure the vehicle takes on the enemy of it's rider (for homing missles for instance).
	if (p_veh->m_pPilot)
	{
		parent->enemy = p_veh->m_pPilot->enemy;
	}
#endif

	if (p_veh->m_pPilot)
	{
		if (p_veh->m_pVehicleInfo->type == VH_WALKER)
		{
			parent->s.loopSound = G_SoundIndex("sound/vehicles/shuttle/loop.wav");
		}
		else if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
		{
			parent->s.loopSound = p_veh->m_pVehicleInfo->soundLoop;
		}
	}

	return true;
}

// Update the properties of a Rider (that may reflect what happens to the vehicle).
static bool UpdateRider(Vehicle_t* p_veh, bgEntity_t* pRider, usercmd_t* pUmcd)
{
	if (p_veh->m_iBoarding != 0 && p_veh->m_iDieTime == 0)
		return true;

	auto parent = p_veh->m_pParentEntity;
	auto rider = pRider;

#ifdef _JK2MP
	//MG FIXME !! Single player needs update!
	if (rider && rider->client
		&& parent && parent->client)
	{//so they know who we're locking onto with our rockets, if anyone
		rider->client->ps.rocketLockIndex = parent->client->ps.rocketLockIndex;
		rider->client->ps.rocketLockTime = parent->client->ps.rocketLockTime;
		rider->client->ps.rocketTargetTime = parent->client->ps.rocketTargetTime;
	}
#endif
	// Regular exit.
	if (pUmcd->buttons & BUTTON_USE && p_veh->m_pVehicleInfo->type != VH_SPEEDER)
	{
		if (p_veh->m_pVehicleInfo->type == VH_WALKER)
		{
			//just get the fuck out
			p_veh->m_EjectDir = VEH_EJECT_REAR;
			if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
				return false;
		}
		else if (!(p_veh->m_ulFlags & VEH_FLYING))
		{
			animNumber_t anim = {};
			// If going too fast, roll off.
			if (parent->client->ps.speed <= 600 && pUmcd->rightmove != 0)
			{
				if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
				{
					constexpr int i_flags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS, i_blend =
						300;
					if (pUmcd->rightmove > 0)
					{
						anim = BOTH_ROLL_R;
						p_veh->m_EjectDir = VEH_EJECT_RIGHT;
					}
					else
					{
						anim = BOTH_ROLL_L;
						p_veh->m_EjectDir = VEH_EJECT_LEFT;
					}
					VectorScale(parent->client->ps.velocity, 0.25f, rider->client->ps.velocity);
					Vehicle_SetAnim(rider, SETANIM_BOTH, anim, i_flags, i_blend);
					rider->client->ps.weaponTime = rider->client->ps.torsoAnimTimer - 200;
					//just to make sure it's cleared when roll is done
					G_AddEvent(rider, EV_ROLL, 0);
					return false;
				}
			}
			else
			{
				// FIXME: Check trace to see if we should start playing the animation.
				constexpr int i_flags = SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, i_blend = 500;

				if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
				{
					//just get the fuck out
					p_veh->m_EjectDir = VEH_EJECT_REAR;
					if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
						return false;
				}
				else
				{
					if (pUmcd->rightmove > 0)
					{
						anim = BOTH_VS_DISMOUNT_R;
						p_veh->m_EjectDir = VEH_EJECT_RIGHT;
					}
					else
					{
						anim = BOTH_VS_DISMOUNT_L;
						p_veh->m_EjectDir = VEH_EJECT_LEFT;
					}
				}

				if (p_veh->m_iBoarding <= 1)
				{
					// NOTE: I know I shouldn't reuse p_veh->m_iBoarding so many times for so many different
					// purposes, but it's not used anywhere else right here so why waste memory???
					const int iAnimLen = PM_AnimLength(pRider->client->clientInfo.animFileIndex, anim);
					p_veh->m_iBoarding = level.time + iAnimLen;
					// Weird huh? Well I wanted to reuse flags and this should never be set in an
					// entity, so what the heck.
					rider->client->ps.eFlags |= EF_VEH_BOARDING;

					// Make sure they can't fire when leaving.
					rider->client->ps.weaponTime = iAnimLen;
				}

				VectorScale(parent->client->ps.velocity, 0.25f, rider->client->ps.velocity);

				Vehicle_SetAnim(rider, SETANIM_BOTH, anim, i_flags, i_blend);
			}
		}
		// Flying, so just fall off.
		else
		{
			if (p_veh->m_pVehicleInfo->type == VH_FIGHTER)
			{
				//just get the fuck out
				p_veh->m_EjectDir = VEH_EJECT_REAR;
				if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
					return false;
			}
			else
			{
				p_veh->m_EjectDir = VEH_EJECT_LEFT;
				if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
					return false;
			}
		}
	}

	if (p_veh->m_pPilot)
	{
		if (p_veh->m_pVehicleInfo->type == VH_SPEEDER)
		{
			rider->s.loopSound = p_veh->m_pVehicleInfo->soundLoop;
		}
		else
		{
			rider->s.loopSound = p_veh->m_pVehicleInfo->soundLoop; // play it if its got one
		}
	}

	// Getting off animation complete (if we had one going)?
	if (p_veh->m_iBoarding < level.time && rider->client->ps.eFlags & EF_VEH_BOARDING)
	{
		rider->client->ps.eFlags &= ~EF_VEH_BOARDING;
		// Eject this guy now.
		if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
		{
			return false;
		}
	}

	if (p_veh->m_pVehicleInfo->type != VH_FIGHTER
		&& p_veh->m_pVehicleInfo->type != VH_WALKER)
	{
		// Jump off.
		if (pUmcd->upmove > 0)
		{
			// NOT IN MULTI PLAYER!
			//===================================================================
			const float riderRightDot = G_CanJumpToEnemyVeh(p_veh, pUmcd);
			if (riderRightDot != 0.0f)
			{
				// Eject Player From Current Vehicle
				//-----------------------------------
				p_veh->m_EjectDir = VEH_EJECT_TOP;
				p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qtrue);

				// Send Current Vehicle Spinning Out Of Control
				//----------------------------------------------
				p_veh->m_pVehicleInfo->StartDeathDelay(p_veh, 10000);
				p_veh->m_ulFlags |= VEH_OUTOFCONTROL;
				VectorScale(p_veh->m_pParentEntity->client->ps.velocity, 1.0f, p_veh->m_pParentEntity->pos3);

				// Todo: Throw Old Vehicle Away From The New Vehicle Some
				//-------------------------------------------------------
				vec3_t toEnemy;
				VectorSubtract(p_veh->m_pParentEntity->currentOrigin, rider->enemy->currentOrigin, toEnemy);
				VectorNormalize(toEnemy);
				G_Throw(p_veh->m_pParentEntity, toEnemy, 50);

				// Start Boarding On Enemy's Vehicle
				//-----------------------------------
				Vehicle_t* enemyVeh = G_IsRidingVehicle(rider->enemy);
				enemyVeh->m_iBoarding = riderRightDot > 0 ? VEH_MOUNT_THROW_RIGHT : VEH_MOUNT_THROW_LEFT;
				enemyVeh->m_pVehicleInfo->Board(enemyVeh, rider);
			}

			// Don't Jump Off If Holding Strafe Key and Moving Fast
			else if (pUmcd->rightmove && parent->client->ps.speed >= 10)
			{
				return true;
			}
			//===================================================================

			if (p_veh->m_pVehicleInfo->Eject(p_veh, pRider, qfalse))
			{
				// Allow them to force jump off.
				VectorScale(parent->client->ps.velocity, 0.5f, rider->client->ps.velocity);
				rider->client->ps.velocity[2] += JUMP_VELOCITY;
#ifdef _JK2MP
				rider->client->ps.fd.forceJumpZStart = rider->client->ps.origin[2];

				if (!trap_ICARUS_TaskIDPending(rider, TID_CHAN_VOICE))
#else
				rider->client->ps.pm_flags |= PMF_JUMPING | PMF_JUMP_HELD;
				rider->client->ps.forceJumpZStart = rider->client->ps.origin[2];

				if (!Q3_TaskIDPending(rider, TID_CHAN_VOICE))
#endif
				{
					G_AddEvent(rider, EV_JUMP, 0);
				}
#if 1
				Vehicle_SetAnim(rider, SETANIM_BOTH, BOTH_JUMP1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 300);
#else

#endif
				return false;
			}
		}
	}

	return true;
}

// Attachs all the riders of this vehicle to their appropriate tag (*driver, *pass1, *pass2, whatever...).
static void AttachRiders(Vehicle_t* p_veh)
{
	// If we have a pilot, attach him to the driver tag.
	if (p_veh->m_pPilot)
	{
		gentity_t* const parent = p_veh->m_pParentEntity;
		gentity_t* const pilot = p_veh->m_pPilot;
		mdxaBone_t bolt_matrix;

		pilot->waypoint = parent->waypoint; // take the veh's waypoint as your own

		// Get the driver tag.
		gi.G2API_GetBoltMatrix(parent->ghoul2, parent->playerModel, parent->crotchBolt, &bolt_matrix,
			p_veh->m_vOrientation, parent->currentOrigin,
			cg.time ? cg.time : level.time, nullptr, parent->s.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, pilot->client->ps.origin);
		G_SetOrigin(pilot, pilot->client->ps.origin);
		gi.linkentity(pilot);
	}
	// If we have a pilot, attach him to the driver tag.
	if (p_veh->m_pOldPilot)
	{
		gentity_t* const parent = p_veh->m_pParentEntity;
		gentity_t* const pilot = p_veh->m_pOldPilot;
		mdxaBone_t bolt_matrix;

		pilot->waypoint = parent->waypoint; // take the veh's waypoint as your own

		// Get the driver tag.
		gi.G2API_GetBoltMatrix(parent->ghoul2, parent->playerModel, parent->crotchBolt, &bolt_matrix,
			p_veh->m_vOrientation, parent->currentOrigin,
			cg.time ? cg.time : level.time, nullptr, parent->s.modelScale);
		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, pilot->client->ps.origin);
		G_SetOrigin(pilot, pilot->client->ps.origin);
		gi.linkentity(pilot);
	}
}

// Make someone invisible and un-collidable.
static void Ghost(Vehicle_t* p_veh, bgEntity_t* pEnt)
{
	if (!pEnt)
		return;

	auto ent = pEnt;

	ent->s.eFlags |= EF_NODRAW;
	if (ent->client)
	{
		ent->client->ps.eFlags |= EF_NODRAW;
	}
#ifdef _JK2MP
	ent->r.contents = 0;
#else
	ent->contents = 0;
#endif
}

// Make someone visible and collidable.
static void UnGhost(Vehicle_t* p_veh, bgEntity_t* pEnt)
{
	if (!pEnt)
		return;

	auto ent = pEnt;

	ent->s.eFlags &= ~EF_NODRAW;
	if (ent->client)
	{
		ent->client->ps.eFlags &= ~EF_NODRAW;
	}
#ifdef _JK2MP
	ent->r.contents = CONTENTS_BODY;
#else
	ent->contents = CONTENTS_BODY;
#endif
}

#ifdef _JK2MP
//try to resize the bounding box around a torn apart ship
void G_VehicleDamageBoxSizing(Vehicle_t* p_veh)
{
	vec3_t fwd, right, up;
	vec3_t nose; //maxs
	vec3_t back; //mins
	trace_t trace;
	const float fDist = 256.0f; //estimated distance to nose from origin
	const float bDist = 256.0f; //estimated distance to back from origin
	const float wDist = 32.0f; //width on each side from origin
	const float hDist = 32.0f; //height on each side from origin
	gentity_t* parent = (gentity_t*)p_veh->m_pParentEntity;

	if (!parent->ghoul2 || !parent->m_pVehicle || !parent->client)
	{ //shouldn't have gotten in here then
		return;
	}

	//for now, let's only do anything if all wings are stripped off.
	//this is because I want to be able to tear my wings off and fling
	//myself down narrow hallways to my death. Because it's fun! -rww
	if (!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) ||
		!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D) ||
		!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) ||
		!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F))
	{
		return;
	}

	//get directions based on orientation
	AngleVectors(p_veh->m_vOrientation, fwd, right, up);

	//get the nose and back positions (relative to 0, they're gonna be mins/maxs)
	VectorMA(vec3_origin, fDist, fwd, nose);
	VectorMA(vec3_origin, -bDist, fwd, back);

	//move the nose and back to opposite right/left, they will end up as our relative mins and maxs
	VectorMA(nose, wDist, right, nose);
	VectorMA(nose, -wDist, right, back);

	//use the same concept for up/down now
	VectorMA(nose, hDist, up, nose);
	VectorMA(nose, -hDist, up, back);

	//and now, let's trace and see if our new mins/maxs are safe..
	trap_Trace(&trace, parent->client->ps.origin, back, nose, parent->client->ps.origin, parent->s.number, parent->clipmask);
	if (!trace.allsolid && !trace.startsolid && trace.fraction == 1.0f)
	{ //all clear!
		VectorCopy(nose, parent->maxs);
		VectorCopy(back, parent->mins);
	}
	else
	{ //oh well, DIE!
		//FIXME: does this give proper credit to the enemy who shot you down?
		G_Damage(parent, parent, parent, nullptr, parent->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
	}
}

//get one of 4 possible impact locations based on the trace direction
static int G_FlyVehicleImpactDir(gentity_t* veh, trace_t* trace)
{
	float impactAngle;
	float relativeAngle;
	trace_t localTrace;
	vec3_t testMins, testMaxs;
	vec3_t rWing, lWing;
	vec3_t fwd, right;
	vec3_t fPos;
	Vehicle_t* p_veh = veh->m_pVehicle;
	qboolean noseClear = qfalse;

	if (!trace || !p_veh || !veh->client)
	{
		return -1;
	}

	AngleVectors(veh->client->ps.viewangles, fwd, right, 0);
	VectorSet(testMins, -24.0f, -24.0f, -24.0f);
	VectorSet(testMaxs, 24.0f, 24.0f, 24.0f);

	//do a trace to determine if the nose is clear
	VectorMA(veh->client->ps.origin, 256.0f, fwd, fPos);
	trap_Trace(&localTrace, veh->client->ps.origin, testMins, testMaxs, fPos, veh->s.number, veh->clipmask);
	if (!localTrace.startsolid && !localTrace.allsolid && localTrace.fraction == 1.0f)
	{ //otherwise I guess it's not clear..
		noseClear = qtrue;
	}

	if (noseClear)
	{ //if nose is clear check for tearing the wings off
		//sadly, the trace endpos given always matches the vehicle origin, so we
		//can't get a real impact direction. First we'll trace forward and see if the wings are colliding
		//with anything, and if not, we'll fall back to checking the trace plane normal.
		VectorMA(veh->client->ps.origin, 128.0f, right, rWing);
		VectorMA(veh->client->ps.origin, -128.0f, right, lWing);

		//test the right wing - unless it's already removed
		if (!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_E) ||
			!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_F))
		{
			VectorMA(rWing, 256.0f, fwd, fPos);
			trap_Trace(&localTrace, rWing, testMins, testMaxs, fPos, veh->s.number, veh->clipmask);
			if (localTrace.startsolid || localTrace.allsolid || localTrace.fraction != 1.0f)
			{ //impact
				return SHIPSURF_RIGHT;
			}
		}

		//test the left wing - unless it's already removed
		if (!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_C) ||
			!(p_veh->m_iRemovedSurfaces & SHIPSURF_BROKEN_D))
		{
			VectorMA(lWing, 256.0f, fwd, fPos);
			trap_Trace(&localTrace, lWing, testMins, testMaxs, fPos, veh->s.number, veh->clipmask);
			if (localTrace.startsolid || localTrace.allsolid || localTrace.fraction != 1.0f)
			{ //impact
				return SHIPSURF_LEFT;
			}
		}
	}

	//try to use the trace plane normal
	impactAngle = vectoyaw(trace->plane.normal);
	relativeAngle = AngleSubtract(impactAngle, veh->client->ps.viewangles[YAW]);

	if (relativeAngle > 130 ||
		relativeAngle < -130)
	{ //consider this front
		return SHIPSURF_FRONT;
	}
	else if (relativeAngle > 0)
	{
		return SHIPSURF_RIGHT;
	}
	else if (relativeAngle < 0)
	{
		return SHIPSURF_LEFT;
	}

	return SHIPSURF_BACK;
}

//try to break surfaces off the ship on impact
#define TURN_ON				0x00000000
#define TURN_OFF			0x00000100
extern void NPC_SetSurfaceOnOff(gentity_t* ent, const char* surfaceName, int surfaceFlags); //NPC_utils.c

void G_SetVehDamageFlags(gentity_t* veh, int shipSurf, int damageLevel)
{
	int dmgFlag;
	switch (damageLevel)
	{
	case 3://destroyed
		//add both flags so cgame side knows this surf is GONE
		//add heavy
		dmgFlag = SHIPSURF_DAMAGE_FRONT_HEAVY + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs |= (1 << dmgFlag);
		//add light
		dmgFlag = SHIPSURF_DAMAGE_FRONT_LIGHT + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs |= (1 << dmgFlag);
		//copy down
		veh->s.brokenLimbs = veh->client->ps.brokenLimbs;
		//check droid
		if (shipSurf == SHIPSURF_BACK)
		{//destroy the droid if we have one
			if (veh->m_pVehicle
				&& veh->m_pVehicle->m_pDroidUnit)
			{//we have one
				gentity_t* droidEnt = (gentity_t*)veh->m_pVehicle->m_pDroidUnit;
				if (droidEnt
					&& ((droidEnt->flags & FL_UNDYING) || droidEnt->health > 0))
				{//boom
					//make it vulnerable
					droidEnt->flags &= ~FL_UNDYING;
					//blow it up
					G_Damage(droidEnt, veh->enemy, veh->enemy, nullptr, nullptr, 99999, 0, MOD_UNKNOWN);
				}
			}
		}
		break;
	case 2://heavy only
		dmgFlag = SHIPSURF_DAMAGE_FRONT_HEAVY + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs |= (1 << dmgFlag);
		//remove light
		dmgFlag = SHIPSURF_DAMAGE_FRONT_LIGHT + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs &= ~(1 << dmgFlag);
		//copy down
		veh->s.brokenLimbs = veh->client->ps.brokenLimbs;
		//check droid
		if (shipSurf == SHIPSURF_BACK)
		{//make the droid vulnerable if we have one
			if (veh->m_pVehicle
				&& veh->m_pVehicle->m_pDroidUnit)
			{//we have one
				gentity_t* droidEnt = (gentity_t*)veh->m_pVehicle->m_pDroidUnit;
				if (droidEnt
					&& (droidEnt->flags & FL_UNDYING))
				{//make it vulnerab;e
					droidEnt->flags &= ~FL_UNDYING;
				}
			}
		}
		break;
	case 1://light only
		//add light
		dmgFlag = SHIPSURF_DAMAGE_FRONT_LIGHT + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs |= (1 << dmgFlag);
		//remove heavy (shouldn't have to do this, but...
		dmgFlag = SHIPSURF_DAMAGE_FRONT_HEAVY + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs &= ~(1 << dmgFlag);
		//copy down
		veh->s.brokenLimbs = veh->client->ps.brokenLimbs;
		break;
	case 0://no damage
	default:
		//remove heavy
		dmgFlag = SHIPSURF_DAMAGE_FRONT_HEAVY + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs &= ~(1 << dmgFlag);
		//remove light
		dmgFlag = SHIPSURF_DAMAGE_FRONT_LIGHT + (shipSurf - SHIPSURF_FRONT);
		veh->client->ps.brokenLimbs &= ~(1 << dmgFlag);
		//copy down
		veh->s.brokenLimbs = veh->client->ps.brokenLimbs;
		break;
	}
}

void G_VehicleSetDamageLocFlags(gentity_t* veh, int impactDir, int deathPoint)
{
	if (!veh->client)
	{
		return;
	}
	else
	{
		int	deathPoint, heavyDamagePoint, lightDamagePoint;
		switch (impactDir)
		{
		case SHIPSURF_FRONT:
			deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_front;
			break;
		case SHIPSURF_BACK:
			deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_back;
			break;
		case SHIPSURF_RIGHT:
			deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_right;
			break;
		case SHIPSURF_LEFT:
			deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_left;
			break;
		default:
			return;
			break;
		}
		if (veh->m_pVehicle
			&& veh->m_pVehicle->m_pVehicleInfo
			&& veh->m_pVehicle->m_pVehicleInfo->malfunctionArmorLevel
			&& veh->m_pVehicle->m_pVehicleInfo->armor)
		{
			float perc = ((float)veh->m_pVehicle->m_pVehicleInfo->malfunctionArmorLevel / (float)veh->m_pVehicle->m_pVehicleInfo->armor);
			if (perc > 0.99f)
			{
				perc = 0.99f;
			}
			heavyDamagePoint = ceil(deathPoint * perc * 0.25f);
			lightDamagePoint = ceil(deathPoint * perc);
		}
		else
		{
			heavyDamagePoint = ceil(deathPoint * 0.66f);
			lightDamagePoint = ceil(deathPoint * 0.14f);
		}

		if (veh->locationDamage[impactDir] >= deathPoint)
		{//destroyed
			G_SetVehDamageFlags(veh, impactDir, 3);
		}
		else if (veh->locationDamage[impactDir] <= heavyDamagePoint)
		{//heavy only
			G_SetVehDamageFlags(veh, impactDir, 2);
		}
		else if (veh->locationDamage[impactDir] <= lightDamagePoint)
		{//light only
			G_SetVehDamageFlags(veh, impactDir, 1);
		}
	}
}

qboolean G_FlyVehicleDestroySurface(gentity_t* veh, int surface)
{
	char* surfName[4]; //up to 4 surfs at once
	int numSurfs = 0;
	int smashedBits = 0;

	if (surface == -1)
	{ //not valid?
		return qfalse;
	}

	switch (surface)
	{
	case SHIPSURF_FRONT: //break the nose off
		surfName[0] = "nose";

		smashedBits = (SHIPSURF_BROKEN_G);

		numSurfs = 1;
		break;
	case SHIPSURF_BACK: //break both the bottom wings off for a backward impact I guess
		surfName[0] = "r_wing2";
		surfName[1] = "l_wing2";

		//get rid of the landing gear
		surfName[2] = "r_gear";
		surfName[3] = "l_gear";

		smashedBits = (SHIPSURF_BROKEN_A | SHIPSURF_BROKEN_B | SHIPSURF_BROKEN_D | SHIPSURF_BROKEN_F);

		numSurfs = 4;
		break;
	case SHIPSURF_RIGHT: //break both right wings off
		surfName[0] = "r_wing1";
		surfName[1] = "r_wing2";

		//get rid of the landing gear
		surfName[2] = "r_gear";

		smashedBits = (SHIPSURF_BROKEN_B | SHIPSURF_BROKEN_E | SHIPSURF_BROKEN_F);

		numSurfs = 3;
		break;
	case SHIPSURF_LEFT: //break both left wings off
		surfName[0] = "l_wing1";
		surfName[1] = "l_wing2";

		//get rid of the landing gear
		surfName[2] = "l_gear";

		smashedBits = (SHIPSURF_BROKEN_A | SHIPSURF_BROKEN_C | SHIPSURF_BROKEN_D);

		numSurfs = 3;
		break;
	default:
		break;
	}

	if (numSurfs < 1)
	{ //didn't get any valid surfs..
		return qfalse;
	}

	while (numSurfs > 0)
	{ //use my silly system of automatically managing surf status on both client and server
		numSurfs--;
		NPC_SetSurfaceOnOff(veh, surfName[numSurfs], TURN_OFF);
	}

	if (!veh->m_pVehicle->m_iRemovedSurfaces)
	{//first time something got blown off
		//if (veh->m_pVehicle->m_pPilot)
		//{//make the pilot scream to his death
		//	G_EntitySound((gentity_t*)veh->m_pVehicle->m_pPilot, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
		//}
	}
	//so we can check what's broken
	veh->m_pVehicle->m_iRemovedSurfaces |= smashedBits;

	//do some explosive damage, but don't damage this ship with it
	G_RadiusDamage(veh->client->ps.origin, veh, 100, 500, veh, nullptr, MOD_SUICIDE);

	//when spiraling to your death, do the electical shader
	veh->client->ps.electrifyTime = level.time + 10000;

	return qtrue;
}

static void G_FlyVehicleSurfaceDestruction(gentity_t* veh, trace_t* trace, int magnitude, qboolean force)
{
	int impactDir;
	int secondImpact;
	int deathPoint = -1;
	qboolean alreadyRebroken = qfalse;

	if (!veh->ghoul2 || !veh->m_pVehicle)
	{ //no g2 instance.. or no vehicle instance
		return;
	}

	impactDir = G_FlyVehicleImpactDir(veh, trace);

anotherImpact:
	if (impactDir == -1)
	{ //not valid?
		return;
	}

	veh->locationDamage[impactDir] += magnitude * 7;

	switch (impactDir)
	{
	case SHIPSURF_FRONT:
		deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_front;
		break;
	case SHIPSURF_BACK:
		deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_back;
		break;
	case SHIPSURF_RIGHT:
		deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_right;
		break;
	case SHIPSURF_LEFT:
		deathPoint = veh->m_pVehicle->m_pVehicleInfo->health_left;
		break;
	default:
		break;
	}

	if (deathPoint != -1)
	{//got a valid health value
		if (force && veh->locationDamage[impactDir] < deathPoint)
		{//force that surf to be destroyed
			veh->locationDamage[impactDir] = deathPoint;
		}
		if (veh->locationDamage[impactDir] >= deathPoint)
		{ //do it
			if (G_FlyVehicleDestroySurface(veh, impactDir))
			{//actually took off a surface
				G_VehicleSetDamageLocFlags(veh, impactDir, deathPoint);
			}
		}
		else
		{
			G_VehicleSetDamageLocFlags(veh, impactDir, deathPoint);
		}
	}

	if (!alreadyRebroken)
	{
		secondImpact = G_FlyVehicleImpactDir(veh, trace);
		if (impactDir != secondImpact)
		{ //can break off another piece in this same impact.. but only break off up to 2 at once
			alreadyRebroken = qtrue;
			impactDir = secondImpact;
			goto anotherImpact;
		}
	}
}
#endif

int G_ShipSurfaceForSurfName(const char* surfaceName)
{
	if (!surfaceName)
	{
		return -1;
	}
	if (!Q_strncmp("nose", surfaceName, 4)
		|| !Q_strncmp("f_gear", surfaceName, 6)
		|| !Q_strncmp("glass", surfaceName, 5))
	{
		return SHIPSURF_FRONT;
	}
	if (!Q_strncmp("body", surfaceName, 4))
	{
		return SHIPSURF_BACK;
	}
	if (!Q_strncmp("r_wing1", surfaceName, 7)
		|| !Q_strncmp("r_wing2", surfaceName, 7)
		|| !Q_strncmp("r_gear", surfaceName, 6))
	{
		return SHIPSURF_RIGHT;
	}
	if (!Q_strncmp("l_wing1", surfaceName, 7)
		|| !Q_strncmp("l_wing2", surfaceName, 7)
		|| !Q_strncmp("l_gear", surfaceName, 6))
	{
		return SHIPSURF_LEFT;
	}
	return -1;
}

// Set the parent entity of this Vehicle NPC.
static void SetParent(Vehicle_t* p_veh, bgEntity_t* pParentEntity) { p_veh->m_pParentEntity = pParentEntity; }

// Add a pilot to the vehicle.
static void SetPilot(Vehicle_t* p_veh, bgEntity_t* pPilot) { p_veh->m_pPilot = pPilot; }

// Add a passenger to the vehicle (false if we're full).
static bool AddPassenger(Vehicle_t* p_veh) { return false; }

// Whether this vehicle is currently inhabited (by anyone) or not.
static bool Inhabited(Vehicle_t* p_veh) { return p_veh->m_pPilot ? true : false; }

// Setup the shared functions (one's that all vehicles would generally use).
void G_SetSharedVehicleFunctions(vehicleInfo_t* pVehInfo)
{
	//	pVehInfo->AnimateVehicle				=		AnimateVehicle;
	//	pVehInfo->AnimateRiders					=		AnimateRiders;
	pVehInfo->ValidateBoard = ValidateBoard;
	pVehInfo->SetParent = SetParent;
	pVehInfo->SetPilot = SetPilot;
	pVehInfo->AddPassenger = AddPassenger;
	pVehInfo->Animate = Animate;
	pVehInfo->Board = Board;
	pVehInfo->Eject = Eject;
	pVehInfo->EjectAll = EjectAll;
	pVehInfo->StartDeathDelay = StartDeathDelay;
	pVehInfo->DeathUpdate = DeathUpdate;
	pVehInfo->RegisterAssets = RegisterAssets;
	pVehInfo->Initialize = Initialize;
	pVehInfo->Update = Update;
	pVehInfo->UpdateRider = UpdateRider;
	pVehInfo->AttachRiders = AttachRiders;
	pVehInfo->Ghost = Ghost;
	pVehInfo->UnGhost = UnGhost;
	pVehInfo->Inhabited = Inhabited;
}

#ifdef _JK2MP
//get rid of all the crazy defs we added for this file
#undef currentAngles
#undef currentOrigin
#undef mins
#undef maxs
#undef legsAnimTimer
#undef torsoAnimTimer
#undef bool
#undef false
#undef true

#undef sqrtf

#undef MOD_EXPLOSIVE
#endif